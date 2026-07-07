/// ambitap.bed2hoa~ (Pd) — encode a channel-based surround bed (5.1, 7.1,
/// 7.1.4, …) into higher-order ambisonics (AmbiX: ACN/SN3D) by encoding each
/// speaker feed at its canonical direction. Creation args: <order> <layout>
/// (the same layout set as ambitap.decode~, so bed2hoa~ → decode~ round-trips a
/// bed through the HOA domain). Multichannel in (speaker feeds in layout order,
/// no LFE); multichannel out ((order+1)^2 channels). The encoding matrix is
/// static (canonical directions), so the perform routine is a plain matrix
/// multiply — wait-free, no allocation.
// SPDX-License-Identifier: MIT
// Copyright 2025-2026 Timothy Place.

#include <cstring>

#include "ambitap/math/core/spherical_harmonics.h"
#include "ambitap/math/geometry/layouts.h"
#include "ambitap_pd.h"

static t_class* ambitap_bed2hoa_tilde_class;

struct bed2hoa_impl {
    int                              hoa_ch; // (order+1)^2
    int                              spk;    // layout speaker count
    float                            gain{1.0f};
    std::vector<std::vector<double>> gains; // [speaker][hoa channel]
    std::vector<float>               zero;

    bed2hoa_impl(int order, const std::vector<ambitap::spherical_coord>& speakers)
        : hoa_ch(static_cast<int>(ambitap::channel_count(order)))
        , spk(static_cast<int>(speakers.size())) {
        gains.assign(static_cast<size_t>(spk), std::vector<double>(static_cast<size_t>(hoa_ch), 0.0));
        float sh[ambitap::k_max_channel_count];
        for (size_t s = 0; s < static_cast<size_t>(spk); ++s) {
            ambitap::evaluate_sh(order, speakers[s].azimuth, speakers[s].elevation, sh);
            for (size_t ch = 0; ch < static_cast<size_t>(hoa_ch); ++ch) {
                gains[s][ch] = static_cast<double>(sh[ch]);
            }
        }
    }
};

struct t_ambitap_bed2hoa_tilde {
    t_object      x_obj;
    t_float       x_f;
    bed2hoa_impl* p;
};

static std::vector<ambitap::spherical_coord> layout_from_name(const char* name) {
    using namespace ambitap::layouts;
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

// out[ch] = gain * sum over speakers of in[s] * G[s][ch]. Input channels beyond
// the layout's speaker count are ignored; HOA channels are channel-major.
static t_int* bed2hoa_perform(t_int* w) {
    auto*         x      = reinterpret_cast<t_ambitap_bed2hoa_tilde*>(w[1]);
    t_sample*     in     = reinterpret_cast<t_sample*>(w[2]);
    const int     in_nch = static_cast<int>(w[3]);
    t_sample*     out    = reinterpret_cast<t_sample*>(w[4]);
    const int     n      = static_cast<int>(w[5]);
    bed2hoa_impl* p      = x->p;
    const int     in_ch  = std::min(in_nch, p->spk);
    const float   g      = p->gain;

    for (int ch = 0; ch < p->hoa_ch; ++ch) {
        t_sample* oc = out + ch * n;
        for (int i = 0; i < n; ++i) {
            oc[i] = 0.0f;
        }
        for (int s = 0; s < in_ch; ++s) {
            const float     gs = static_cast<float>(p->gains[static_cast<size_t>(s)][static_cast<size_t>(ch)]) * g;
            const t_sample* ic = in + s * n;
            for (int i = 0; i < n; ++i) {
                oc[i] += ic[i] * gs;
            }
        }
    }
    return w + 6;
}

static void bed2hoa_dsp(t_ambitap_bed2hoa_tilde* x, t_signal** sp) {
    signal_setmultiout(&sp[1], x->p->hoa_ch);
    dsp_add(bed2hoa_perform, 5, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans), sp[1]->s_vec,
            static_cast<t_int>(sp[0]->s_length));
}

static void bed2hoa_gain(t_ambitap_bed2hoa_tilde* x, t_floatarg f) {
    x->p->gain = static_cast<float>(f);
}

static void* bed2hoa_new(t_symbol*, int argc, t_atom* argv) {
    auto* x              = reinterpret_cast<t_ambitap_bed2hoa_tilde*>(pd_new(ambitap_bed2hoa_tilde_class));
    int   ord            = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord                  = std::clamp(ord, 0, ambitap::k_max_order);
    const char* layout   = (argc >= 2) ? atom_getsymbol(argv + 1)->s_name : "surround_5_1";
    auto        speakers = layout_from_name(layout);
    if (speakers.empty()) {
        speakers = ambitap::layouts::surround_5_1();
    }
    x->p   = new bed2hoa_impl(ord, speakers);
    x->x_f = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void bed2hoa_free(t_ambitap_bed2hoa_tilde* x) {
    delete x->p;
}

void ambitap_bed2hoa_tilde_setup(void) {
    t_class* c                  = class_new(gensym("ambitap.bed2hoa~"), reinterpret_cast<t_newmethod>(bed2hoa_new),
                                            reinterpret_cast<t_method>(bed2hoa_free), sizeof(t_ambitap_bed2hoa_tilde),
                                            CLASS_MULTICHANNEL, A_GIMME, 0);
    ambitap_bed2hoa_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_bed2hoa_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(bed2hoa_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(bed2hoa_gain), gensym("gain"), A_FLOAT, 0);
}
