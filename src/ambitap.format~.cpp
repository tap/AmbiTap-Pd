/// ambitap.format~ (Pd) — convert an ambisonics bus between FuMa and AmbiX
/// (orders 0-3). Multichannel in and out; order is a creation argument.
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include <cstring>

#include "ambitap/dsp/format_converter.h"
#include "ambitap_pd.h"

using io_t = ambitap_pd::mc_io<tap::ambi::dsp::format_converter>;

static t_class* ambitap_format_tilde_class;

struct t_ambitap_format_tilde {
    t_object x_obj;
    t_float  x_f;
    io_t*    io;
};

static t_int* format_perform(t_int* w) {
    auto* x = reinterpret_cast<t_ambitap_format_tilde*>(w[1]);
    x->io->run(reinterpret_cast<t_sample*>(w[2]), static_cast<int>(w[3]), reinterpret_cast<t_sample*>(w[4]),
               static_cast<int>(w[5]));
    return w + 6;
}

static void format_dsp(t_ambitap_format_tilde* x, t_signal** sp) {
    signal_setmultiout(&sp[1], x->io->nch);
    x->io->ensure_zero(sp[0]->s_length);
    dsp_add(format_perform, 5, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans), sp[1]->s_vec,
            static_cast<t_int>(sp[0]->s_length));
}

static void format_direction(t_ambitap_format_tilde* x, t_symbol* s) {
    using dir = tap::ambi::dsp::format_direction;
    x->io->proc.set_direction(std::strcmp(s->s_name, "fuma_to_ambix") == 0 ? dir::fuma_to_ambix : dir::ambix_to_fuma);
}

static void* format_new(t_symbol*, int argc, t_atom* argv) {
    auto* x   = reinterpret_cast<t_ambitap_format_tilde*>(pd_new(ambitap_format_tilde_class));
    int   ord = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord       = std::clamp(ord, 0, 3);
    x->io     = new io_t(ord);
    x->x_f    = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void format_free(t_ambitap_format_tilde* x) {
    delete x->io;
}

void ambitap_format_tilde_setup(void) {
    t_class* c                 = class_new(gensym("ambitap.format~"), reinterpret_cast<t_newmethod>(format_new),
                                           reinterpret_cast<t_method>(format_free), sizeof(t_ambitap_format_tilde), CLASS_MULTICHANNEL,
                                           A_GIMME, 0);
    ambitap_format_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_format_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(format_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(format_direction), gensym("direction"), A_SYMBOL, 0);
}
