/// ambitap.doppler~ (Pd) — variable propagation delay for a HOA bus (modulate
/// 'distance' for the Doppler effect). Multichannel in and out; order is a
/// creation argument. The delay buffers are sized from the host sample rate.
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include "ambitap/dsp/doppler.h"
#include "ambitap_pd.h"

using io_t = ambitap_pd::mc_io<ambitap::dsp::doppler>;

static t_class* ambitap_doppler_tilde_class;

struct t_ambitap_doppler_tilde {
    t_object x_obj;
    t_float  x_f;
    io_t*    io;
};

static t_int* doppler_perform(t_int* w) {
    auto* x = reinterpret_cast<t_ambitap_doppler_tilde*>(w[1]);
    x->io->run(reinterpret_cast<t_sample*>(w[2]), static_cast<int>(w[3]), reinterpret_cast<t_sample*>(w[4]),
               static_cast<int>(w[5]));
    return w + 6;
}

static void doppler_dsp(t_ambitap_doppler_tilde* x, t_signal** sp) {
    x->io->proc.prepare(static_cast<float>(sp[0]->s_sr));
    signal_setmultiout(&sp[1], x->io->nch);
    x->io->ensure_zero(sp[0]->s_length);
    dsp_add(doppler_perform, 5, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans), sp[1]->s_vec,
            static_cast<t_int>(sp[0]->s_length));
}

static void doppler_distance(t_ambitap_doppler_tilde* x, t_floatarg f) {
    x->io->proc.set_distance(static_cast<float>(f));
}
static void doppler_speed(t_ambitap_doppler_tilde* x, t_floatarg f) {
    x->io->proc.set_speed_of_sound(static_cast<float>(f));
}
static void doppler_maxdist(t_ambitap_doppler_tilde* x, t_floatarg f) {
    x->io->proc.set_max_distance(static_cast<float>(f));
}

static void* doppler_new(t_symbol*, int argc, t_atom* argv) {
    auto* x   = reinterpret_cast<t_ambitap_doppler_tilde*>(pd_new(ambitap_doppler_tilde_class));
    int   ord = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord       = std::clamp(ord, 1, ambitap::k_max_order);
    x->io     = new io_t(ord);
    x->x_f    = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void doppler_free(t_ambitap_doppler_tilde* x) {
    delete x->io;
}

void ambitap_doppler_tilde_setup(void) {
    t_class* c                  = class_new(gensym("ambitap.doppler~"), reinterpret_cast<t_newmethod>(doppler_new),
                                            reinterpret_cast<t_method>(doppler_free), sizeof(t_ambitap_doppler_tilde),
                                            CLASS_MULTICHANNEL, A_GIMME, 0);
    ambitap_doppler_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_doppler_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(doppler_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(doppler_distance), gensym("distance"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(doppler_speed), gensym("speed_of_sound"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(doppler_maxdist), gensym("max_distance"), A_FLOAT, 0);
}
