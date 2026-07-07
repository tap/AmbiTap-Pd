/// ambitap.room~ (Pd) — shoebox room simulation on a higher-order ambisonics
/// bus: mono source in, (order+1)^2 SH channels out (AmbiX: ACN/SN3D), as direct
/// sound + image-source early reflections + a 16-line SH-domain FDN late tail.
/// Order is a creation argument (default 1, max 3).
///
/// DSP lives in ambitap::dsp::room — the real-time realization verified against
/// the R1-R10 gates in the AmbiTap library's docs/PERCEPTUAL-VERIFICATION.md.
/// Geometry / RT60 changes are rebuilt on the library's worker thread and
/// crossfaded in; the audio path is wait-free. The room state is (re)allocated
/// for the host's signal vector size and sample rate in the dsp method
/// (power-of-two vectors only); the object is silent until then.
///
/// Note the fixed latency on every path (direct sound included):
/// room.latency_samples() = max(3989 - round(0.030 * samplerate), 0) samples
/// (~53 ms at 48 kHz) — the causality cost of the FDN's injection alignment.
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include <array>
#include <cmath>
#include <memory>

#include "ambitap/dsp/room.h"
#include "ambitap_pd.h"

static t_class* ambitap_room_tilde_class;

struct room_impl {
    std::unique_ptr<ambitap::dsp::room> room;
    int                                 nch;
    long                                block_size{0};
    float                               gain{1.0f};
    float                               gain_smooth{1.0f};
    std::vector<float>                  in_buf;
    std::vector<float*>                 out_ptrs;

    // Tracked geometry (the room composes x/y/z together; Pd delivers one
    // component per message). Defaults mirror dsp::room's verified seed-11 set.
    float dim[3]{7.10f, 5.30f, 3.10f};
    float src[3]{3.674f, 1.137f, 1.977f};
    float lis[3]{1.746f, 1.711f, 0.668f};

    // One-pole coefficient of the per-sample output-gain slew (~5 ms at 48 kHz).
    static constexpr float k_gain_slew = 1.0f / 256.0f;

    explicit room_impl(int order)
        : room(std::make_unique<ambitap::dsp::room>(order))
        , nch(static_cast<int>(room->channels()))
        , out_ptrs(static_cast<size_t>(nch), nullptr) {}
};

struct t_ambitap_room_tilde {
    t_object   x_obj;
    t_float    x_f;
    room_impl* p;
};

static t_int* room_perform(t_int* w) {
    auto*      x   = reinterpret_cast<t_ambitap_room_tilde*>(w[1]);
    t_sample*  in  = reinterpret_cast<t_sample*>(w[2]);
    t_sample*  out = reinterpret_cast<t_sample*>(w[3]);
    const int  n   = static_cast<int>(w[4]);
    room_impl* p   = x->p;
    const int  nch = p->nch;

    if (!p->room->is_prepared() || n != p->block_size) {
        for (int c = 0; c < nch; ++c) {
            for (int i = 0; i < n; ++i) {
                out[c * n + i] = 0.0f;
            }
        }
        return w + 5;
    }

    for (int i = 0; i < n; ++i) {
        p->in_buf[static_cast<size_t>(i)] = in[i];
    }

    // Each output channel is a contiguous planar block (out + c*n), exactly the
    // planar layout room::process writes — point the room straight at it.
    for (int c = 0; c < nch; ++c) {
        p->out_ptrs[static_cast<size_t>(c)] = reinterpret_cast<float*>(out + c * n);
    }
    p->room->process(p->in_buf.data(), p->out_ptrs.data(), static_cast<size_t>(n));

    // Smoothed output gain, applied in place (same ramp on every channel).
    const float g_target = p->gain;
    float       g_end    = p->gain_smooth;
    for (int c = 0; c < nch; ++c) {
        float* oc = reinterpret_cast<float*>(out + c * n);
        float  g  = p->gain_smooth;
        for (int i = 0; i < n; ++i) {
            g += (g_target - g) * room_impl::k_gain_slew;
            oc[i] *= g;
        }
        g_end = g;
    }
    p->gain_smooth = g_end;
    return w + 5;
}

static void room_dsp(t_ambitap_room_tilde* x, t_signal** sp) {
    room_impl* p = x->p;
    const int  n = sp[0]->s_length;
    signal_setmultiout(&sp[1], p->nch);

    const bool valid = (n >= 4) && ((n & (n - 1)) == 0);
    if (!valid) {
        p->block_size = 0; // unsupported vector size -> stay silent
    }
    else {
        p->block_size = n;
        p->room->prepare(static_cast<size_t>(n), static_cast<float>(sp[0]->s_sr));
        p->in_buf.assign(static_cast<size_t>(n), 0.0f);
        p->gain_smooth = p->gain;
    }
    dsp_add(room_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, static_cast<t_int>(n));
}

static void room_set_dim(t_ambitap_room_tilde* x, int idx, t_floatarg f) {
    x->p->dim[idx] = static_cast<float>(f);
    x->p->room->set_room_dimensions(x->p->dim[0], x->p->dim[1], x->p->dim[2]);
}
static void room_dim_x(t_ambitap_room_tilde* x, t_floatarg f) {
    room_set_dim(x, 0, f);
}
static void room_dim_y(t_ambitap_room_tilde* x, t_floatarg f) {
    room_set_dim(x, 1, f);
}
static void room_dim_z(t_ambitap_room_tilde* x, t_floatarg f) {
    room_set_dim(x, 2, f);
}

static void room_set_src(t_ambitap_room_tilde* x, int idx, t_floatarg f) {
    x->p->src[idx] = static_cast<float>(f);
    x->p->room->set_source_position(x->p->src[0], x->p->src[1], x->p->src[2]);
}
static void room_source_x(t_ambitap_room_tilde* x, t_floatarg f) {
    room_set_src(x, 0, f);
}
static void room_source_y(t_ambitap_room_tilde* x, t_floatarg f) {
    room_set_src(x, 1, f);
}
static void room_source_z(t_ambitap_room_tilde* x, t_floatarg f) {
    room_set_src(x, 2, f);
}

static void room_set_lis(t_ambitap_room_tilde* x, int idx, t_floatarg f) {
    x->p->lis[idx] = static_cast<float>(f);
    x->p->room->set_listener_position(x->p->lis[0], x->p->lis[1], x->p->lis[2]);
}
static void room_listener_x(t_ambitap_room_tilde* x, t_floatarg f) {
    room_set_lis(x, 0, f);
}
static void room_listener_y(t_ambitap_room_tilde* x, t_floatarg f) {
    room_set_lis(x, 1, f);
}
static void room_listener_z(t_ambitap_room_tilde* x, t_floatarg f) {
    room_set_lis(x, 2, f);
}

static void room_rt60(t_ambitap_room_tilde* x, t_floatarg f) {
    x->p->room->set_rt60(static_cast<float>(f));
}
static void room_direct(t_ambitap_room_tilde* x, t_floatarg f) {
    x->p->room->set_direct_enabled(f != 0);
}
static void room_er(t_ambitap_room_tilde* x, t_floatarg f) {
    x->p->room->set_early_enabled(f != 0);
}
static void room_tail(t_ambitap_room_tilde* x, t_floatarg f) {
    x->p->room->set_tail_enabled(f != 0);
}
static void room_gain(t_ambitap_room_tilde* x, t_floatarg f) {
    x->p->gain = std::max(static_cast<float>(f), 0.0f);
}

// absorption fir|iir — per-line loop filter. fir (default): verified 255-tap
// linear-phase FIRs. iir: one cheap first-order low-pass per line (much lower
// CPU, approximate mid-band RT60, slightly different late texture; the tail
// stays level-calibrated).
static void room_absorption(t_ambitap_room_tilde* x, t_symbol* s) {
    using kind = ambitap::dsp::room::absorption_kind;
    x->p->room->set_absorption_kind(s == gensym("iir") ? kind::iir : kind::fir);
}

// rt60band <center_hz> <seconds>.
static void room_rt60band(t_ambitap_room_tilde* x, t_symbol*, int argc, t_atom* argv) {
    if (argc < 2) {
        return;
    }
    const double hz  = atom_getfloat(argv);
    const float  sec = static_cast<float>(atom_getfloat(argv + 1));
    for (size_t b = 0; b < ambitap::dsp::room::k_rt60_bands; ++b) {
        if (std::abs(ambitap::dsp::room::k_rt60_centers_hz[b] - hz) < 1.0) {
            x->p->room->set_rt60_band(b, sec);
            return;
        }
    }
    pd_error(x, "ambitap.room~: rt60band: no band at %g Hz (250/500/1000/2000/4000)", hz);
}

// reflections <x0 x1 y0 y1 z0 z1>: six wall amplitude coefficients (0..1).
static void room_reflections(t_ambitap_room_tilde* x, t_symbol*, int argc, t_atom* argv) {
    if (argc < static_cast<int>(ambitap::dsp::room::k_walls)) {
        return;
    }
    std::array<float, ambitap::dsp::room::k_walls> c{};
    for (size_t w = 0; w < c.size(); ++w) {
        c[w] = static_cast<float>(atom_getfloat(argv + w));
    }
    x->p->room->set_wall_reflections(c);
}

static void* room_new(t_symbol*, int argc, t_atom* argv) {
    auto* x   = reinterpret_cast<t_ambitap_room_tilde*>(pd_new(ambitap_room_tilde_class));
    int   ord = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord       = std::clamp(ord, 0, ambitap::dsp::room::k_max_room_order);
    x->p      = new room_impl(ord);
    x->x_f    = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void room_free(t_ambitap_room_tilde* x) {
    delete x->p;
}

void ambitap_room_tilde_setup(void) {
    t_class* c =
        class_new(gensym("ambitap.room~"), reinterpret_cast<t_newmethod>(room_new),
                  reinterpret_cast<t_method>(room_free), sizeof(t_ambitap_room_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    ambitap_room_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_room_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(room_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_dim_x), gensym("dim_x"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_dim_y), gensym("dim_y"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_dim_z), gensym("dim_z"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_source_x), gensym("source_x"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_source_y), gensym("source_y"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_source_z), gensym("source_z"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_listener_x), gensym("listener_x"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_listener_y), gensym("listener_y"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_listener_z), gensym("listener_z"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_rt60), gensym("rt60"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_direct), gensym("direct"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_er), gensym("er"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_tail), gensym("tail"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_gain), gensym("gain"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_absorption), gensym("absorption"), A_SYMBOL, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_rt60band), gensym("rt60band"), A_GIMME, 0);
    class_addmethod(c, reinterpret_cast<t_method>(room_reflections), gensym("reflections"), A_GIMME, 0);
}
