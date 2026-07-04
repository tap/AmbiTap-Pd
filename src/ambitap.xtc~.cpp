/// ambitap.xtc~ (Pd) — transaural / crosstalk cancellation: play stereo or
/// binaural program over TWO loudspeakers so each ear hears (mostly) only its
/// own channel. The 2x2 filter matrix is a regularized frequency-domain inverse
/// of the KEMAR speaker→ear plant for a stated symmetric geometry (span,
/// distance), designed entirely by dsp::xtc in the AmbiTap library — v1 is
/// computed-plant presets only. The design meets the numeric gates X1–X6 of the
/// library's docs/PERCEPTUAL-VERIFICATION.md; the `bypass` message is the A/B
/// leg the listening protocol requires.
///
/// Threading follows the ambitap.panbin~ pattern, widened from 2 filters to 4
/// (2-in/2-out): control-thread handlers redesign the FIRs and publish a freshly
/// built quad of partitioned convolvers through a lock-free single-slot handoff;
/// the perform routine adopts, crossfades over one block, and parks the retiring
/// quad for the control thread to reap. Wait-free and allocation-free; silent
/// until the dsp method has prepared the convolvers.
///
/// Latency: dsp::xtc realizes the (non-causal) inverse with a modeling delay of
/// half the FIR length (512 samples) on top of the host vector; bypass is plain
/// passthrough (not delay-compensated). The shipped filters carry the X5 makeup
/// attenuation (~-12 dB), so the cancelled path is noticeably quieter than
/// bypass — match levels upstream when running the protocol.

#include "ambitap_pd.h"

#include "ambitap/dsp/xtc.h"

#include <atomic>
#include <memory>
#include <mutex>

static t_class* ambitap_xtc_tilde_class;

struct xtc_impl {
    /// The four FIR convolvers of one design: speaker feed = row, program
    /// input = column. Built on the control thread; used only on the audio
    /// thread after ownership is handed over.
    struct convolver_quad {
        ambitap::partitioned_convolver ll;    // left speaker  <- left input
        ambitap::partitioned_convolver lr;    // left speaker  <- right input
        ambitap::partitioned_convolver rl;    // right speaker <- left input
        ambitap::partitioned_convolver rr;    // right speaker <- right input

        convolver_quad(size_t block_size, const ambitap::dsp::xtc& d)
            : ll(block_size, d.fir(0, 0).data(), d.fir(0, 0).size())
            , lr(block_size, d.fir(0, 1).data(), d.fir(0, 1).size())
            , rl(block_size, d.fir(1, 0).data(), d.fir(1, 0).size())
            , rr(block_size, d.fir(1, 1).data(), d.fir(1, 1).size()) {}

        /// out_l/out_r = quad applied to (in_l, in_r); tmp is caller scratch.
        void process(const float* in_l, const float* in_r, float* out_l, float* out_r,
                     float* tmp, int frames) {
            ll.process(in_l, out_l);
            lr.process(in_r, tmp);
            for (int i = 0; i < frames; ++i) out_l[i] += tmp[i];
            rl.process(in_l, out_r);
            rr.process(in_r, tmp);
            for (int i = 0; i < frames; ++i) out_r[i] += tmp[i];
        }
    };

    std::mutex        control_mutex;
    ambitap::dsp::xtc design;
    long              block_size {0};
    double            sample_rate {0.0};

    std::atomic<convolver_quad*> pending {nullptr};
    std::atomic<convolver_quad*> trash {nullptr};
    std::atomic<float>           bypass_target {0.0f};

    convolver_quad*    active {nullptr};
    float              bypass_current {0.0f};
    std::vector<float> in_l, in_r, wet_l, wet_r, fade_l, fade_r, tmp;

    ~xtc_impl() {
        delete pending.exchange(nullptr);
        delete trash.exchange(nullptr);
        delete active;
    }

    /// Publish a freshly built quad for the audio thread to crossfade to.
    /// Caller holds control_mutex; no-op until prepared.
    void publish() {
        if (block_size == 0)
            return;
        delete trash.exchange(nullptr, std::memory_order_acq_rel);    // reap
        auto quad = std::make_unique<convolver_quad>(static_cast<size_t>(block_size), design);
        delete pending.exchange(quad.release(), std::memory_order_acq_rel);
    }

    void prepare(long vector_size, double sr) {
        std::lock_guard<std::mutex> lock(control_mutex);
        delete pending.exchange(nullptr);
        delete trash.exchange(nullptr);
        delete active;
        active = nullptr;

        const bool valid = (vector_size >= 4) && ((vector_size & (vector_size - 1)) == 0);
        if (!valid) {
            block_size = 0;    // unsupported vector size -> stay silent
            return;
        }
        block_size = vector_size;
        if (sr != sample_rate) {
            sample_rate = sr;
            design.set_sample_rate(static_cast<float>(sr));    // redesigns the FIRs
        }
        const auto v = static_cast<size_t>(vector_size);
        in_l.assign(v, 0.0f);
        in_r.assign(v, 0.0f);
        wet_l.assign(v, 0.0f);
        wet_r.assign(v, 0.0f);
        fade_l.assign(v, 0.0f);
        fade_r.assign(v, 0.0f);
        tmp.assign(v, 0.0f);
        active = new convolver_quad(v, design);
    }
};

struct t_ambitap_xtc_tilde {
    t_object  x_obj;
    t_float   x_f;
    xtc_impl* p;
};

static t_int* xtc_perform(t_int* w) {
    auto*     x     = reinterpret_cast<t_ambitap_xtc_tilde*>(w[1]);
    t_sample* inl   = reinterpret_cast<t_sample*>(w[2]);
    t_sample* inr   = reinterpret_cast<t_sample*>(w[3]);
    t_sample* out_l = reinterpret_cast<t_sample*>(w[4]);
    t_sample* out_r = reinterpret_cast<t_sample*>(w[5]);
    const int n     = static_cast<int>(w[6]);
    xtc_impl* p     = x->p;
    using quad      = xtc_impl::convolver_quad;

    if (p->block_size == 0 || n != p->block_size) {
        for (int i = 0; i < n; ++i) { out_l[i] = 0.0f; out_r[i] = 0.0f; }
        return w + 7;
    }

    // Copy the inputs up front: outputs may alias inputs, and the dry (bypass)
    // mix needs them after the wet render.
    for (int i = 0; i < n; ++i) {
        p->in_l[static_cast<size_t>(i)] = inl[i];
        p->in_r[static_cast<size_t>(i)] = inr[i];
    }

    quad* incoming = nullptr;
    if (p->trash.load(std::memory_order_relaxed) == nullptr)
        incoming = p->pending.exchange(nullptr, std::memory_order_acq_rel);

    if (incoming && p->active) {
        p->active->process(p->in_l.data(), p->in_r.data(), p->fade_l.data(), p->fade_r.data(),
                           p->tmp.data(), n);
        incoming->process(p->in_l.data(), p->in_r.data(), p->wet_l.data(), p->wet_r.data(),
                          p->tmp.data(), n);
        const float step = 1.0f / static_cast<float>(n);
        float       fw   = 0.0f;
        for (int i = 0; i < n; ++i) {
            fw += step;
            p->wet_l[static_cast<size_t>(i)] = p->fade_l[static_cast<size_t>(i)] + fw * (p->wet_l[static_cast<size_t>(i)] - p->fade_l[static_cast<size_t>(i)]);
            p->wet_r[static_cast<size_t>(i)] = p->fade_r[static_cast<size_t>(i)] + fw * (p->wet_r[static_cast<size_t>(i)] - p->fade_r[static_cast<size_t>(i)]);
        }
        p->trash.store(p->active, std::memory_order_release);
        p->active = incoming;
    }
    else {
        if (incoming)
            p->active = incoming;    // first-ever quad: nothing to fade from
        if (!p->active) {
            for (int i = 0; i < n; ++i) { out_l[i] = 0.0f; out_r[i] = 0.0f; }
            return w + 7;
        }
        p->active->process(p->in_l.data(), p->in_r.data(), p->wet_l.data(), p->wet_r.data(),
                           p->tmp.data(), n);
    }

    // Bypass: linear ramp from the previous mix to the target across this block.
    const float target = p->bypass_target.load(std::memory_order_relaxed);
    float       b      = p->bypass_current;
    const float b_step = (target - b) / static_cast<float>(n);
    for (int i = 0; i < n; ++i) {
        b += b_step;
        out_l[i] = p->wet_l[static_cast<size_t>(i)] + b * (p->in_l[static_cast<size_t>(i)] - p->wet_l[static_cast<size_t>(i)]);
        out_r[i] = p->wet_r[static_cast<size_t>(i)] + b * (p->in_r[static_cast<size_t>(i)] - p->wet_r[static_cast<size_t>(i)]);
    }
    p->bypass_current = target;
    return w + 7;
}

static void xtc_dsp(t_ambitap_xtc_tilde* x, t_signal** sp) {
    x->p->prepare(sp[0]->s_length, sp[0]->s_sr);
    signal_setmultiout(&sp[2], 1);
    signal_setmultiout(&sp[3], 1);
    // sp[0] = left in, sp[1] = right in, sp[2] = left out, sp[3] = right out.
    dsp_add(xtc_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec,
            static_cast<t_int>(sp[0]->s_length));
}

static void xtc_span(t_ambitap_xtc_tilde* x, t_floatarg f) {
    std::lock_guard<std::mutex> lock(x->p->control_mutex);
    x->p->design.set_span(static_cast<float>(f));
    x->p->publish();
}
static void xtc_distance(t_ambitap_xtc_tilde* x, t_floatarg f) {
    std::lock_guard<std::mutex> lock(x->p->control_mutex);
    x->p->design.set_distance(static_cast<float>(f));
    x->p->publish();
}
static void xtc_regularization(t_ambitap_xtc_tilde* x, t_floatarg f) {
    std::lock_guard<std::mutex> lock(x->p->control_mutex);
    x->p->design.set_regularization(static_cast<float>(f));
    x->p->publish();
}
static void xtc_bypass(t_ambitap_xtc_tilde* x, t_floatarg f) {
    x->p->bypass_target.store(f != 0 ? 1.0f : 0.0f, std::memory_order_relaxed);
}

static void* xtc_new(t_symbol*, int, t_atom*) {
    auto* x = reinterpret_cast<t_ambitap_xtc_tilde*>(pd_new(ambitap_xtc_tilde_class));
    x->p    = new xtc_impl();
    x->x_f  = 0;
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);   // second signal inlet (right)
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void xtc_free(t_ambitap_xtc_tilde* x) { delete x->p; }

void ambitap_xtc_tilde_setup(void) {
    t_class* c = class_new(gensym("ambitap.xtc~"),
                           reinterpret_cast<t_newmethod>(xtc_new),
                           reinterpret_cast<t_method>(xtc_free),
                           sizeof(t_ambitap_xtc_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    ambitap_xtc_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_xtc_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(xtc_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(xtc_span), gensym("span"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(xtc_distance), gensym("distance"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(xtc_regularization), gensym("regularization"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(xtc_bypass), gensym("bypass"), A_FLOAT, 0);
}
