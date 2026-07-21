/// @file
/// ambitap.encode~ (Pure Data) — encode a mono source into higher-order
/// ambisonics (AmbiX: ACN ordering, SN3D normalization).
///
/// Order is a creation argument (default 1). The output is a single
/// multichannel signal of (order+1)^2 channels (Pd >= 0.54). DSP lives in
/// tap::ambi::dsp::encoder; this file is Pd C-API glue.
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include <algorithm>

#include "ambitap/dsp/encoder.h"
#include "m_pd.h"

static t_class* ambitap_encode_tilde_class;

struct t_ambitap_encode_tilde {
    t_object               x_obj;
    t_float                x_f; // main-signal-inlet float fallback (CLASS_MAINSIGNALIN)
    tap::ambi::dsp::encoder* x_enc;
    int                    x_channels; // (order+1)^2
};

// args: x, in (channel 0), out (nchans * n, channel-major), n
static t_int* ambitap_encode_tilde_perform(t_int* w) {
    t_ambitap_encode_tilde* x   = reinterpret_cast<t_ambitap_encode_tilde*>(w[1]);
    const t_sample*         in  = reinterpret_cast<const t_sample*>(w[2]);
    t_sample*               out = reinterpret_cast<t_sample*>(w[3]);
    const int               n   = static_cast<int>(w[4]);
    const int               nch = x->x_channels;

    for (int c = 0; c < nch; ++c) {
        t_sample*      oc = out + c * n; // channel-major output
        const t_sample g  = static_cast<t_sample>(x->x_enc->channel_gain(c));
        for (int i = 0; i < n; ++i) {
            oc[i] = in[i] * g;
        }
    }
    return w + 5;
}

static void ambitap_encode_tilde_dsp(t_ambitap_encode_tilde* x, t_signal** sp) {
    // Output gets (order+1)^2 channels; input is treated as mono (channel 0).
    signal_setmultiout(&sp[1], x->x_channels);
    dsp_add(ambitap_encode_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, static_cast<t_int>(sp[0]->s_length));
}

static void ambitap_encode_tilde_azimuth(t_ambitap_encode_tilde* x, t_floatarg f) {
    x->x_enc->set_azimuth(static_cast<float>(f));
}
static void ambitap_encode_tilde_elevation(t_ambitap_encode_tilde* x, t_floatarg f) {
    x->x_enc->set_elevation(static_cast<float>(f));
}
static void ambitap_encode_tilde_gain(t_ambitap_encode_tilde* x, t_floatarg f) {
    x->x_enc->set_gain(static_cast<float>(f));
}

static void* ambitap_encode_tilde_new(t_symbol* s, int argc, t_atom* argv) {
    (void)s;
    t_ambitap_encode_tilde* x     = reinterpret_cast<t_ambitap_encode_tilde*>(pd_new(ambitap_encode_tilde_class));
    int                     order = 1;
    if (argc >= 1) {
        order = static_cast<int>(atom_getfloat(argv));
    }
    order         = std::clamp(order, 0, tap::ambi::k_max_order);
    x->x_enc      = new tap::ambi::dsp::encoder(order);
    x->x_channels = static_cast<int>(x->x_enc->channels());
    x->x_f        = 0;
    outlet_new(&x->x_obj, &s_signal); // one (multichannel) signal outlet
    return x;
}

static void ambitap_encode_tilde_free(t_ambitap_encode_tilde* x) {
    delete x->x_enc;
}

void ambitap_encode_tilde_setup(void) {
    ambitap_encode_tilde_class =
        class_new(gensym("ambitap.encode~"), reinterpret_cast<t_newmethod>(ambitap_encode_tilde_new),
                  reinterpret_cast<t_method>(ambitap_encode_tilde_free), sizeof(t_ambitap_encode_tilde),
                  CLASS_MULTICHANNEL, A_GIMME, 0);

    CLASS_MAINSIGNALIN(ambitap_encode_tilde_class, t_ambitap_encode_tilde, x_f);
    class_addmethod(ambitap_encode_tilde_class, reinterpret_cast<t_method>(ambitap_encode_tilde_dsp), gensym("dsp"),
                    A_CANT, 0);
    class_addmethod(ambitap_encode_tilde_class, reinterpret_cast<t_method>(ambitap_encode_tilde_azimuth),
                    gensym("azimuth"), A_FLOAT, 0);
    class_addmethod(ambitap_encode_tilde_class, reinterpret_cast<t_method>(ambitap_encode_tilde_elevation),
                    gensym("elevation"), A_FLOAT, 0);
    class_addmethod(ambitap_encode_tilde_class, reinterpret_cast<t_method>(ambitap_encode_tilde_gain), gensym("gain"),
                    A_FLOAT, 0);
}
