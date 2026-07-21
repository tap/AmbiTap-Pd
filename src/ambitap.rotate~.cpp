/// ambitap.rotate~ (Pd) — rotate a HOA bus by yaw/pitch/roll (Euler, Z-Y-X).
/// Multichannel in and out; order is a creation argument.
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include "ambitap/dsp/rotator.h"
#include "ambitap_pd.h"

using io_t = ambitap_pd::mc_io<tap::ambi::dsp::rotator>;

static t_class* ambitap_rotate_tilde_class;

struct t_ambitap_rotate_tilde {
    t_object x_obj;
    t_float  x_f;
    io_t*    io;
};

static t_int* rotate_perform(t_int* w) {
    auto* x = reinterpret_cast<t_ambitap_rotate_tilde*>(w[1]);
    x->io->run(reinterpret_cast<t_sample*>(w[2]), static_cast<int>(w[3]), reinterpret_cast<t_sample*>(w[4]),
               static_cast<int>(w[5]));
    return w + 6;
}

static void rotate_dsp(t_ambitap_rotate_tilde* x, t_signal** sp) {
    signal_setmultiout(&sp[1], x->io->nch);
    x->io->ensure_zero(sp[0]->s_length);
    dsp_add(rotate_perform, 5, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans), sp[1]->s_vec,
            static_cast<t_int>(sp[0]->s_length));
}

static void rotate_yaw(t_ambitap_rotate_tilde* x, t_floatarg f) {
    x->io->proc.set_yaw(static_cast<float>(f));
}
static void rotate_pitch(t_ambitap_rotate_tilde* x, t_floatarg f) {
    x->io->proc.set_pitch(static_cast<float>(f));
}
static void rotate_roll(t_ambitap_rotate_tilde* x, t_floatarg f) {
    x->io->proc.set_roll(static_cast<float>(f));
}

static void* rotate_new(t_symbol*, int argc, t_atom* argv) {
    auto* x   = reinterpret_cast<t_ambitap_rotate_tilde*>(pd_new(ambitap_rotate_tilde_class));
    int   ord = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord       = std::clamp(ord, 0, tap::ambi::k_max_order);
    x->io     = new io_t(ord);
    x->x_f    = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void rotate_free(t_ambitap_rotate_tilde* x) {
    delete x->io;
}

void ambitap_rotate_tilde_setup(void) {
    t_class* c                 = class_new(gensym("ambitap.rotate~"), reinterpret_cast<t_newmethod>(rotate_new),
                                           reinterpret_cast<t_method>(rotate_free), sizeof(t_ambitap_rotate_tilde), CLASS_MULTICHANNEL,
                                           A_GIMME, 0);
    ambitap_rotate_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_rotate_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(rotate_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(rotate_yaw), gensym("yaw"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(rotate_pitch), gensym("pitch"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(rotate_roll), gensym("roll"), A_FLOAT, 0);
}
