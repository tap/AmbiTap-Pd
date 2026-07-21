/// ambitap.decode~ (Pd) — decode a HOA bus to a loudspeaker layout. Creation
/// args: <order> <layout>. Multichannel in; multichannel out (speaker feeds).
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include <cstring>
#include <string>

#include "ambitap/dsp/decoder.h"
#include "ambitap/math/geometry/layouts.h"
#include "ambitap_pd.h"

static t_class* ambitap_decode_tilde_class;

struct decode_impl {
    tap::ambi::dsp::decoder   dec;
    int                       in_ch;
    int                       spk;
    std::vector<const float*> in_ptrs;
    std::vector<float*>       out_ptrs;
    std::vector<float>        zero;

    decode_impl(int order, std::vector<tap::ambi::spherical_coord> speakers)
        : dec(order)
        , in_ch(static_cast<int>(dec.input_channels()))
        , spk(static_cast<int>(speakers.size()))
        , in_ptrs(static_cast<size_t>(in_ch))
        , out_ptrs(static_cast<size_t>(spk)) {
        dec.set_speakers(std::move(speakers));
    }
};

struct t_ambitap_decode_tilde {
    t_object     x_obj;
    t_float      x_f;
    decode_impl* p;
};

static std::vector<tap::ambi::spherical_coord> layout_from_name(const char* name) {
    using namespace tap::ambi::layouts;
    if (!std::strcmp(name, "stereo")) {
        return stereo();
    }
    if (!std::strcmp(name, "quad")) {
        return quad();
    }
    if (!std::strcmp(name, "surround_5_1") || !std::strcmp(name, "5.1")) {
        return surround_5_1();
    }
    if (!std::strcmp(name, "surround_7_1") || !std::strcmp(name, "7.1")) {
        return surround_7_1();
    }
    if (!std::strcmp(name, "surround_7_1_4") || !std::strcmp(name, "7.1.4")) {
        return surround_7_1_4();
    }
    if (!std::strcmp(name, "cube")) {
        return cube();
    }
    if (!std::strcmp(name, "hexagon")) {
        return hexagon();
    }
    if (!std::strcmp(name, "octagon")) {
        return octagon();
    }
    return {};
}

static t_int* decode_perform(t_int* w) {
    auto*        x      = reinterpret_cast<t_ambitap_decode_tilde*>(w[1]);
    t_sample*    in     = reinterpret_cast<t_sample*>(w[2]);
    const int    in_nch = static_cast<int>(w[3]);
    t_sample*    out    = reinterpret_cast<t_sample*>(w[4]);
    const int    n      = static_cast<int>(w[5]);
    decode_impl* p      = x->p;
    for (int c = 0; c < p->in_ch; ++c) {
        p->in_ptrs[static_cast<size_t>(c)] = (c < in_nch) ? reinterpret_cast<const float*>(in + c * n) : p->zero.data();
    }
    for (int s = 0; s < p->spk; ++s) {
        p->out_ptrs[static_cast<size_t>(s)] = reinterpret_cast<float*>(out + s * n);
    }
    p->dec.process(p->in_ptrs.data(), p->out_ptrs.data(), static_cast<size_t>(p->spk), static_cast<size_t>(n));
    return w + 6;
}

static void decode_dsp(t_ambitap_decode_tilde* x, t_signal** sp) {
    signal_setmultiout(&sp[1], x->p->spk);
    if (static_cast<int>(x->p->zero.size()) < sp[0]->s_length) {
        x->p->zero.assign(static_cast<size_t>(sp[0]->s_length), 0.0f);
    }
    dsp_add(decode_perform, 5, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans), sp[1]->s_vec,
            static_cast<t_int>(sp[0]->s_length));
}

static void decode_decoder_type(t_ambitap_decode_tilde* x, t_symbol* s) {
    using alg = tap::ambi::dsp::decoder_algorithm;
    if (!std::strcmp(s->s_name, "allrad")) {
        x->p->dec.set_algorithm(alg::allrad);
    }
    else if (!std::strcmp(s->s_name, "epad")) {
        x->p->dec.set_algorithm(alg::epad);
    }
    else {
        x->p->dec.set_algorithm(alg::mode_match);
    }
}
static void decode_max_re(t_ambitap_decode_tilde* x, t_floatarg f) {
    x->p->dec.set_max_re(f != 0);
}

static void* decode_new(t_symbol*, int argc, t_atom* argv) {
    auto* x              = reinterpret_cast<t_ambitap_decode_tilde*>(pd_new(ambitap_decode_tilde_class));
    int   ord            = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord                  = std::clamp(ord, 1, tap::ambi::k_max_order);
    const char* layout   = (argc >= 2) ? atom_getsymbol(argv + 1)->s_name : "stereo";
    auto        speakers = layout_from_name(layout);
    if (speakers.empty()) {
        speakers = tap::ambi::layouts::stereo();
    }
    x->p   = new decode_impl(ord, std::move(speakers));
    x->x_f = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void decode_free(t_ambitap_decode_tilde* x) {
    delete x->p;
}

void ambitap_decode_tilde_setup(void) {
    t_class* c                 = class_new(gensym("ambitap.decode~"), reinterpret_cast<t_newmethod>(decode_new),
                                           reinterpret_cast<t_method>(decode_free), sizeof(t_ambitap_decode_tilde), CLASS_MULTICHANNEL,
                                           A_GIMME, 0);
    ambitap_decode_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_decode_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(decode_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(decode_decoder_type), gensym("decoder_type"), A_SYMBOL, 0);
    class_addmethod(c, reinterpret_cast<t_method>(decode_max_re), gensym("max_re"), A_FLOAT, 0);
}
