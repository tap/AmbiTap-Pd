/// ambitap.binaural~ (Pd) — decode a HOA bus to binaural stereo via SH-domain
/// HRTF convolution (built-in MIT KEMAR, orders 1-5), with head-tracking.
/// Multichannel in; two signal outs (left, right).
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include <cstring>
#include <string>

#include "ambitap/dsp/binaural_renderer.h"
#include "ambitap_pd.h"

static t_class* ambitap_binaural_tilde_class;

struct binaural_impl {
    tap::ambi::dsp::binaural_renderer rend;
    int                             ch;
    std::vector<const float*>       in_ptrs;
    std::vector<float>              zero;

    explicit binaural_impl(int order)
        : rend(order)
        , ch(static_cast<int>(rend.channels()))
        , in_ptrs(static_cast<size_t>(ch)) {}
};

struct t_ambitap_binaural_tilde {
    t_object       x_obj;
    t_float        x_f;
    binaural_impl* p;
};

static t_int* binaural_perform(t_int* w) {
    auto*          x      = reinterpret_cast<t_ambitap_binaural_tilde*>(w[1]);
    t_sample*      in     = reinterpret_cast<t_sample*>(w[2]);
    const int      in_nch = static_cast<int>(w[3]);
    t_sample*      left   = reinterpret_cast<t_sample*>(w[4]);
    t_sample*      right  = reinterpret_cast<t_sample*>(w[5]);
    const int      n      = static_cast<int>(w[6]);
    binaural_impl* p      = x->p;
    for (int c = 0; c < p->ch; ++c) {
        p->in_ptrs[static_cast<size_t>(c)] = (c < in_nch) ? reinterpret_cast<const float*>(in + c * n) : p->zero.data();
    }
    p->rend.process(p->in_ptrs.data(), reinterpret_cast<float*>(left), reinterpret_cast<float*>(right),
                    static_cast<size_t>(n));
    return w + 7;
}

static void binaural_dsp(t_ambitap_binaural_tilde* x, t_signal** sp) {
    const int n = sp[0]->s_length;
    if (n >= 4 && (n & (n - 1)) == 0) {
        x->p->rend.prepare(static_cast<size_t>(n));
    }
    signal_setmultiout(&sp[1], 1);
    signal_setmultiout(&sp[2], 1);
    if (static_cast<int>(x->p->zero.size()) < n) {
        x->p->zero.assign(static_cast<size_t>(n), 0.0f);
    }
    dsp_add(binaural_perform, 6, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans), sp[1]->s_vec, sp[2]->s_vec,
            static_cast<t_int>(n));
}

static void binaural_volume(t_ambitap_binaural_tilde* x, t_floatarg f) {
    x->p->rend.set_volume(static_cast<float>(f));
}
static void binaural_yaw(t_ambitap_binaural_tilde* x, t_floatarg f) {
    x->p->rend.set_yaw(static_cast<float>(f));
}
static void binaural_pitch(t_ambitap_binaural_tilde* x, t_floatarg f) {
    x->p->rend.set_pitch(static_cast<float>(f));
}
static void binaural_roll(t_ambitap_binaural_tilde* x, t_floatarg f) {
    x->p->rend.set_roll(static_cast<float>(f));
}
static void binaural_hrtf(t_ambitap_binaural_tilde* x, t_symbol* s) {
    using proj = tap::ambi::dsp::binaural_renderer::hrtf_projection;
    x->p->rend.set_projection(!std::strcmp(s->s_name, "magls") ? proj::magls : proj::ls);
}

static void* binaural_new(t_symbol*, int argc, t_atom* argv) {
    auto* x   = reinterpret_cast<t_ambitap_binaural_tilde*>(pd_new(ambitap_binaural_tilde_class));
    int   ord = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord       = std::clamp(ord, 1, tap::ambi::builtin_hrtf_order);
    x->p      = new binaural_impl(ord);
    x->x_f    = 0;
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void binaural_free(t_ambitap_binaural_tilde* x) {
    delete x->p;
}

void ambitap_binaural_tilde_setup(void) {
    t_class* c                   = class_new(gensym("ambitap.binaural~"), reinterpret_cast<t_newmethod>(binaural_new),
                                             reinterpret_cast<t_method>(binaural_free), sizeof(t_ambitap_binaural_tilde),
                                             CLASS_MULTICHANNEL, A_GIMME, 0);
    ambitap_binaural_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_binaural_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(binaural_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(binaural_volume), gensym("volume"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(binaural_yaw), gensym("yaw"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(binaural_pitch), gensym("pitch"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(binaural_roll), gensym("roll"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(binaural_hrtf), gensym("hrtf_dataset"), A_SYMBOL, 0);
}
