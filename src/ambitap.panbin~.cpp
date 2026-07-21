/// ambitap.panbin~ (Pd) — direct binaural panner: pan a mono source to stereo
/// with a per-source HRTF at (azimuth, elevation), WITHOUT an ambisonic bus.
/// Unlike encode~ → binaural~ there is no order-limited spatial blur: the
/// source is convolved with the HRIR reconstructed exactly at its direction.
///
/// Reuse path: reconstruct the per-ear HRIRs the way
/// binaural_renderer::probe_response does (evaluate the SH basis at the
/// direction and sum the built-in order-5 KEMAR SH-domain FIRs weighted by
/// those coefficients), resample to the host rate with tap::ambi::resample_fir,
/// and feed one tap::ambi::partitioned_convolver per ear.
///
/// Direction changes are click-free. The azimuth/elevation handlers run on the
/// control thread, where they rebuild the two-convolver pair (allocation is
/// fine there) and publish it through a lock-free single-slot handoff. The
/// perform routine adopts the new pair at a block boundary, runs old and new in
/// parallel for that one block while crossfading, then parks the old pair in a
/// trash slot that the control thread reaps on its next rebuild — the audio
/// thread never allocates or frees. It is silent until the dsp method has
/// prepared the convolvers.
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include <atomic>
#include <memory>
#include <mutex>

#include "ambitap/math/binaural/convolution.h"
#include "ambitap/math/binaural/hrtf_data.h"
#include "ambitap/math/binaural/resample.h"
#include "ambitap/math/core/spherical_harmonics.h"
#include "ambitap_pd.h"

static t_class* ambitap_panbin_tilde_class;

struct panbin_impl {
    /// One HRIR-loaded convolver per ear. Built on the control thread; used
    /// (and only used) on the audio thread after ownership is handed over.
    struct convolver_pair {
        tap::ambi::partitioned_convolver left;
        tap::ambi::partitioned_convolver right;

        convolver_pair(size_t block_size, const std::vector<float>& l, const std::vector<float>& r)
            : left(block_size, l.data(), l.size())
            , right(block_size, r.data(), r.size()) {}
    };

    std::mutex control_mutex;
    double     azimuth_value{0.0};
    double     elevation_value{0.0};
    long       block_size{0};
    double     sample_rate{0.0};

    std::atomic<convolver_pair*> pending{nullptr};
    std::atomic<convolver_pair*> trash{nullptr};
    std::atomic<float>           gain{1.0f};

    convolver_pair*    active{nullptr};
    float              gain_current{1.0f};
    std::vector<float> in_buf, left_buf, right_buf, fade_l, fade_r;

    ~panbin_impl() {
        delete pending.exchange(nullptr);
        delete trash.exchange(nullptr);
        delete active;
    }

    /// Reconstruct the per-ear HRIRs at the current direction (the same sum
    /// binaural_renderer::probe_response performs), resampled to the host rate,
    /// wrapped in a fresh convolver pair. Control thread only.
    std::unique_ptr<convolver_pair> build_pair() {
        float sh[tap::ambi::k_max_channel_count];
        tap::ambi::evaluate_sh(tap::ambi::builtin_hrtf_order, static_cast<float>(azimuth_value),
                               static_cast<float>(elevation_value), sh);

        std::vector<float> l(tap::ambi::builtin_hrtf_length, 0.0f);
        std::vector<float> r(tap::ambi::builtin_hrtf_length, 0.0f);
        for (size_t ch = 0; ch < tap::ambi::builtin_hrtf_channels; ++ch) {
            for (size_t t = 0; t < tap::ambi::builtin_hrtf_length; ++t) {
                l[t] += sh[ch] * tap::ambi::builtin_hrtf_left[ch][t];
                r[t] += sh[ch] * tap::ambi::builtin_hrtf_right[ch][t];
            }
        }

        const auto host_rate = static_cast<float>(sample_rate);
        if (host_rate != tap::ambi::builtin_hrtf_sample_rate) {
            l = tap::ambi::resample_fir(l.data(), l.size(), tap::ambi::builtin_hrtf_sample_rate, host_rate);
            r = tap::ambi::resample_fir(r.data(), r.size(), tap::ambi::builtin_hrtf_sample_rate, host_rate);
        }
        return std::make_unique<convolver_pair>(static_cast<size_t>(block_size), l, r);
    }

    void set_direction(double az, double el) {
        std::lock_guard<std::mutex> lock(control_mutex);
        azimuth_value   = az;
        elevation_value = el;
        if (block_size == 0) {
            return; // not prepared yet; the dsp method builds the first pair
        }
        delete trash.exchange(nullptr, std::memory_order_acq_rel); // reap
        delete pending.exchange(build_pair().release(), std::memory_order_acq_rel);
    }

    void prepare(long vector_size, double sr) {
        std::lock_guard<std::mutex> lock(control_mutex);
        delete pending.exchange(nullptr);
        delete trash.exchange(nullptr);
        delete active;
        active = nullptr;

        const bool valid = (vector_size >= 4) && ((vector_size & (vector_size - 1)) == 0);
        if (!valid) {
            block_size = 0; // unsupported vector size -> stay silent
            return;
        }
        block_size  = vector_size;
        sample_rate = sr;

        const auto v = static_cast<size_t>(vector_size);
        in_buf.assign(v, 0.0f);
        left_buf.assign(v, 0.0f);
        right_buf.assign(v, 0.0f);
        fade_l.assign(v, 0.0f);
        fade_r.assign(v, 0.0f);
        active = build_pair().release();
    }
};

struct t_ambitap_panbin_tilde {
    t_object     x_obj;
    t_float      x_f;
    panbin_impl* p;
};

static t_int* panbin_perform(t_int* w) {
    auto*        x     = reinterpret_cast<t_ambitap_panbin_tilde*>(w[1]);
    t_sample*    in    = reinterpret_cast<t_sample*>(w[2]);
    t_sample*    out_l = reinterpret_cast<t_sample*>(w[3]);
    t_sample*    out_r = reinterpret_cast<t_sample*>(w[4]);
    const int    n     = static_cast<int>(w[5]);
    panbin_impl* p     = x->p;
    using pair         = panbin_impl::convolver_pair;

    if (p->block_size == 0 || n != p->block_size) {
        for (int i = 0; i < n; ++i) {
            out_l[i] = 0.0f;
            out_r[i] = 0.0f;
        }
        return w + 6;
    }

    for (int i = 0; i < n; ++i) {
        p->in_buf[static_cast<size_t>(i)] = in[i];
    }

    // Adopt a newly published direction, but only when the trash slot is free
    // to receive the pair we would retire (the control thread reaps it).
    pair* incoming = nullptr;
    if (p->trash.load(std::memory_order_relaxed) == nullptr) {
        incoming = p->pending.exchange(nullptr, std::memory_order_acq_rel);
    }

    if (incoming && p->active) {
        p->active->left.process(p->in_buf.data(), p->fade_l.data());
        p->active->right.process(p->in_buf.data(), p->fade_r.data());
        incoming->left.process(p->in_buf.data(), p->left_buf.data());
        incoming->right.process(p->in_buf.data(), p->right_buf.data());
        const float step = 1.0f / static_cast<float>(n);
        float       fw   = 0.0f;
        for (int i = 0; i < n; ++i) {
            fw += step;
            p->left_buf[static_cast<size_t>(i)] =
                p->fade_l[static_cast<size_t>(i)]
                + fw * (p->left_buf[static_cast<size_t>(i)] - p->fade_l[static_cast<size_t>(i)]);
            p->right_buf[static_cast<size_t>(i)] =
                p->fade_r[static_cast<size_t>(i)]
                + fw * (p->right_buf[static_cast<size_t>(i)] - p->fade_r[static_cast<size_t>(i)]);
        }
        p->trash.store(p->active, std::memory_order_release);
        p->active = incoming;
    }
    else {
        if (incoming) {
            p->active = incoming; // first-ever pair: nothing to fade from
        }
        if (!p->active) {
            for (int i = 0; i < n; ++i) {
                out_l[i] = 0.0f;
                out_r[i] = 0.0f;
            }
            return w + 6;
        }
        p->active->left.process(p->in_buf.data(), p->left_buf.data());
        p->active->right.process(p->in_buf.data(), p->right_buf.data());
    }

    // Gain: linear ramp from the previous value to the target across this block.
    const float target = p->gain.load(std::memory_order_relaxed);
    float       g      = p->gain_current;
    const float g_step = (target - g) / static_cast<float>(n);
    for (int i = 0; i < n; ++i) {
        g += g_step;
        out_l[i] = p->left_buf[static_cast<size_t>(i)] * g;
        out_r[i] = p->right_buf[static_cast<size_t>(i)] * g;
    }
    p->gain_current = target;
    return w + 6;
}

static void panbin_dsp(t_ambitap_panbin_tilde* x, t_signal** sp) {
    x->p->prepare(sp[0]->s_length, sp[0]->s_sr);
    signal_setmultiout(&sp[1], 1);
    signal_setmultiout(&sp[2], 1);
    dsp_add(panbin_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, static_cast<t_int>(sp[0]->s_length));
}

static void panbin_azimuth(t_ambitap_panbin_tilde* x, t_floatarg f) {
    x->p->set_direction(static_cast<double>(f), x->p->elevation_value);
}
static void panbin_elevation(t_ambitap_panbin_tilde* x, t_floatarg f) {
    x->p->set_direction(x->p->azimuth_value, static_cast<double>(f));
}
static void panbin_gain(t_ambitap_panbin_tilde* x, t_floatarg f) {
    x->p->gain.store(static_cast<float>(f), std::memory_order_relaxed);
}

static void* panbin_new(t_symbol*, int, t_atom*) {
    auto* x = reinterpret_cast<t_ambitap_panbin_tilde*>(pd_new(ambitap_panbin_tilde_class));
    x->p    = new panbin_impl();
    x->x_f  = 0;
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void panbin_free(t_ambitap_panbin_tilde* x) {
    delete x->p;
}

void ambitap_panbin_tilde_setup(void) {
    t_class* c                 = class_new(gensym("ambitap.panbin~"), reinterpret_cast<t_newmethod>(panbin_new),
                                           reinterpret_cast<t_method>(panbin_free), sizeof(t_ambitap_panbin_tilde), CLASS_MULTICHANNEL,
                                           A_GIMME, 0);
    ambitap_panbin_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_panbin_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(panbin_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(panbin_azimuth), gensym("azimuth"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(panbin_elevation), gensym("elevation"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(panbin_gain), gensym("gain"), A_FLOAT, 0);
}
