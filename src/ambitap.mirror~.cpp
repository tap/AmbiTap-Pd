/// ambitap.mirror~ (Pd) — mirror a HOA bus across cardinal planes (LR/FB/UD).
/// Multichannel in and out; order is a creation argument.
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include "ambitap/dsp/mirror.h"
#include "ambitap_pd.h"

using io_t = ambitap_pd::mc_io<tap::ambi::dsp::mirror>;

static t_class* ambitap_mirror_tilde_class;

struct t_ambitap_mirror_tilde {
    t_object x_obj;
    t_float  x_f;
    io_t*    io;
};

static t_int* mirror_perform(t_int* w) {
    auto* x = reinterpret_cast<t_ambitap_mirror_tilde*>(w[1]);
    x->io->run(reinterpret_cast<t_sample*>(w[2]), static_cast<int>(w[3]), reinterpret_cast<t_sample*>(w[4]),
               static_cast<int>(w[5]));
    return w + 6;
}

static void mirror_dsp(t_ambitap_mirror_tilde* x, t_signal** sp) {
    signal_setmultiout(&sp[1], x->io->nch);
    x->io->ensure_zero(sp[0]->s_length);
    dsp_add(mirror_perform, 5, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans), sp[1]->s_vec,
            static_cast<t_int>(sp[0]->s_length));
}

static void mirror_flip_lr(t_ambitap_mirror_tilde* x, t_floatarg f) {
    x->io->proc.set_flip_lr(f != 0);
}
static void mirror_flip_fb(t_ambitap_mirror_tilde* x, t_floatarg f) {
    x->io->proc.set_flip_fb(f != 0);
}
static void mirror_flip_ud(t_ambitap_mirror_tilde* x, t_floatarg f) {
    x->io->proc.set_flip_ud(f != 0);
}

static void* mirror_new(t_symbol*, int argc, t_atom* argv) {
    auto* x   = reinterpret_cast<t_ambitap_mirror_tilde*>(pd_new(ambitap_mirror_tilde_class));
    int   ord = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord       = std::clamp(ord, 0, tap::ambi::k_max_order);
    x->io     = new io_t(ord);
    x->x_f    = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void mirror_free(t_ambitap_mirror_tilde* x) {
    delete x->io;
}

void ambitap_mirror_tilde_setup(void) {
    t_class* c                 = class_new(gensym("ambitap.mirror~"), reinterpret_cast<t_newmethod>(mirror_new),
                                           reinterpret_cast<t_method>(mirror_free), sizeof(t_ambitap_mirror_tilde), CLASS_MULTICHANNEL,
                                           A_GIMME, 0);
    ambitap_mirror_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_mirror_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(mirror_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(mirror_flip_lr), gensym("flip_lr"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(mirror_flip_fb), gensym("flip_fb"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(mirror_flip_ud), gensym("flip_ud"), A_FLOAT, 0);
}
