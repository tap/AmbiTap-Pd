/// ambitap.vmic~ (Pd) — virtual mic: extract a mono directional signal from a
/// HOA bus. Multichannel in; mono out. Order is a creation argument.
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include "ambitap/dsp/virtual_mic.h"
#include "ambitap_pd.h"

static t_class* ambitap_vmic_tilde_class;

struct vmic_impl {
    ambitap::dsp::virtual_mic mic;
    int                       ch;
    std::vector<const float*> in_ptrs;
    std::vector<float>        zero;

    explicit vmic_impl(int order)
        : mic(order)
        , ch(static_cast<int>(mic.channels()))
        , in_ptrs(static_cast<size_t>(ch)) {}
};

struct t_ambitap_vmic_tilde {
    t_object   x_obj;
    t_float    x_f;
    vmic_impl* p;
};

static t_int* vmic_perform(t_int* w) {
    auto*      x      = reinterpret_cast<t_ambitap_vmic_tilde*>(w[1]);
    t_sample*  in     = reinterpret_cast<t_sample*>(w[2]);
    const int  in_nch = static_cast<int>(w[3]);
    t_sample*  out    = reinterpret_cast<t_sample*>(w[4]);
    const int  n      = static_cast<int>(w[5]);
    vmic_impl* p      = x->p;
    for (int c = 0; c < p->ch; ++c) {
        p->in_ptrs[static_cast<size_t>(c)] = (c < in_nch) ? reinterpret_cast<const float*>(in + c * n) : p->zero.data();
    }
    p->mic.process(p->in_ptrs.data(), reinterpret_cast<float*>(out), static_cast<size_t>(n));
    return w + 6;
}

static void vmic_dsp(t_ambitap_vmic_tilde* x, t_signal** sp) {
    signal_setmultiout(&sp[1], 1);
    if (static_cast<int>(x->p->zero.size()) < sp[0]->s_length) {
        x->p->zero.assign(static_cast<size_t>(sp[0]->s_length), 0.0f);
    }
    dsp_add(vmic_perform, 5, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans), sp[1]->s_vec,
            static_cast<t_int>(sp[0]->s_length));
}

static void vmic_azimuth(t_ambitap_vmic_tilde* x, t_floatarg f) {
    x->p->mic.set_azimuth(static_cast<float>(f));
}
static void vmic_elevation(t_ambitap_vmic_tilde* x, t_floatarg f) {
    x->p->mic.set_elevation(static_cast<float>(f));
}
static void vmic_max_re(t_ambitap_vmic_tilde* x, t_floatarg f) {
    x->p->mic.set_max_re(f != 0);
}

static void* vmic_new(t_symbol*, int argc, t_atom* argv) {
    auto* x   = reinterpret_cast<t_ambitap_vmic_tilde*>(pd_new(ambitap_vmic_tilde_class));
    int   ord = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord       = std::clamp(ord, 1, ambitap::k_max_order);
    x->p      = new vmic_impl(ord);
    x->x_f    = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void vmic_free(t_ambitap_vmic_tilde* x) {
    delete x->p;
}

void ambitap_vmic_tilde_setup(void) {
    t_class* c =
        class_new(gensym("ambitap.vmic~"), reinterpret_cast<t_newmethod>(vmic_new),
                  reinterpret_cast<t_method>(vmic_free), sizeof(t_ambitap_vmic_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    ambitap_vmic_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_vmic_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(vmic_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(vmic_azimuth), gensym("azimuth"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(vmic_elevation), gensym("elevation"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(vmic_max_re), gensym("max_re"), A_FLOAT, 0);
}
