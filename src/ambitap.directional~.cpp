/// ambitap.directional~ (Pd) — per-direction gain on a HOA bus (1 = bypass,
/// >1 boost, <1 attenuate). Multichannel in and out; order is a creation arg.
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include "ambitap/dsp/directional_loudness.h"
#include "ambitap_pd.h"

using io_t = ambitap_pd::mc_io<tap::ambi::dsp::directional_loudness>;

static t_class* ambitap_directional_tilde_class;

struct t_ambitap_directional_tilde {
    t_object x_obj;
    t_float  x_f;
    io_t*    io;
};

static t_int* directional_perform(t_int* w) {
    auto* x = reinterpret_cast<t_ambitap_directional_tilde*>(w[1]);
    x->io->run(reinterpret_cast<t_sample*>(w[2]), static_cast<int>(w[3]), reinterpret_cast<t_sample*>(w[4]),
               static_cast<int>(w[5]));
    return w + 6;
}

static void directional_dsp(t_ambitap_directional_tilde* x, t_signal** sp) {
    signal_setmultiout(&sp[1], x->io->nch);
    x->io->ensure_zero(sp[0]->s_length);
    dsp_add(directional_perform, 5, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans), sp[1]->s_vec,
            static_cast<t_int>(sp[0]->s_length));
}

static void directional_azimuth(t_ambitap_directional_tilde* x, t_floatarg f) {
    x->io->proc.set_azimuth(static_cast<float>(f));
}
static void directional_elevation(t_ambitap_directional_tilde* x, t_floatarg f) {
    x->io->proc.set_elevation(static_cast<float>(f));
}
static void directional_gain(t_ambitap_directional_tilde* x, t_floatarg f) {
    x->io->proc.set_gain(static_cast<float>(f));
}

static void* directional_new(t_symbol*, int argc, t_atom* argv) {
    auto* x   = reinterpret_cast<t_ambitap_directional_tilde*>(pd_new(ambitap_directional_tilde_class));
    int   ord = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord       = std::clamp(ord, 1, tap::ambi::k_max_order);
    x->io     = new io_t(ord);
    x->x_f    = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void directional_free(t_ambitap_directional_tilde* x) {
    delete x->io;
}

void ambitap_directional_tilde_setup(void) {
    t_class* c = class_new(gensym("ambitap.directional~"), reinterpret_cast<t_newmethod>(directional_new),
                           reinterpret_cast<t_method>(directional_free), sizeof(t_ambitap_directional_tilde),
                           CLASS_MULTICHANNEL, A_GIMME, 0);
    ambitap_directional_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_directional_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(directional_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(directional_azimuth), gensym("azimuth"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(directional_elevation), gensym("elevation"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(directional_gain), gensym("gain"), A_FLOAT, 0);
}
