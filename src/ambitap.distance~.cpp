/// ambitap.distance~ (Pd) — distance cues for a higher-order ambisonics bus:
/// propagation delay / Doppler, 1/r^n distance gain, air-absorption low-pass,
/// and near-field compensation (NFC-HOA, per-order shelving). Order is a
/// creation argument; multichannel in and out.
///
/// Processing chain (per frame): doppler delay -> distance gain -> air
/// absorption -> NFC. The delay comes first (propagation of the dry bus, so
/// modulating 'distance' yields the physical pitch glide); gain and the
/// air-absorption one-pole are applied uniformly across channels (spatial
/// encoding preserved); the per-order NFC shelf runs last. Air-absorption
/// cutoff model: fc(d) = 20 kHz / (1 + amount * 0.1/m * max(d - d_ref, 0)).

#include "ambitap_pd.h"

#include "ambitap/dsp/doppler.h"
#include "ambitap/dsp/nfc.h"

#include <cmath>

static t_class* ambitap_distance_tilde_class;

struct distance_impl {
    ambitap::dsp::doppler dop;
    ambitap::dsp::nfc     nfc;
    int                   nch;
    float                 fs {48000.0f};

    // Control-thread parameters (smoothed on the audio thread).
    float distance {1.0f};
    float reference_distance {1.0f};
    float attenuation {1.0f};
    float air_absorption {0.0f};
    bool  doppler_on {true};
    bool  nfc_on {true};

    // Audio-thread state.
    float              distance_smooth {1.0f};
    std::vector<float> frame;
    std::vector<float> lp_state;

    // One-pole coefficient of the per-sample distance slew for the gain and
    // air-absorption cues (matches dsp::doppler's internal delay slew).
    static constexpr float k_distance_slew = 1.0f / 1024.0f;

    explicit distance_impl(int order)
        : dop(order)
        , nfc(order)
        , nch(static_cast<int>(nfc.channels()))
        , frame(static_cast<size_t>(nch), 0.0f)
        , lp_state(static_cast<size_t>(nch), 0.0f) {}
};

struct t_ambitap_distance_tilde {
    t_object       x_obj;
    t_float        x_f;
    distance_impl* p;
};

static t_int* distance_perform(t_int* w) {
    auto*          x      = reinterpret_cast<t_ambitap_distance_tilde*>(w[1]);
    t_sample*      in     = reinterpret_cast<t_sample*>(w[2]);
    const int      in_nch = static_cast<int>(w[3]);
    t_sample*      out    = reinterpret_cast<t_sample*>(w[4]);
    const int      n      = static_cast<int>(w[5]);
    distance_impl* p      = x->p;
    const int      nch    = p->nch;
    const float    d_min  = ambitap::dsp::nfc::k_min_distance;

    const float d_target = std::max(p->distance, d_min);
    const float d_ref    = std::max(p->reference_distance, d_min);
    const float exponent = p->attenuation;
    const float air      = p->air_absorption;
    const bool  dop_on   = p->doppler_on;
    const bool  nfc_on   = p->nfc_on;
    float*      frame    = p->frame.data();

    for (int i = 0; i < n; ++i) {
        for (int c = 0; c < nch; ++c)
            frame[c] = (c < in_nch) ? in[c * n + i] : 0.0f;

        // 1. Propagation delay (Doppler): the library slews the delay
        //    internally, producing the pitch glide on distance jumps.
        if (dop_on)
            p->dop.process_frame(frame, frame);

        // Smooth the distance for the gain/absorption cues.
        p->distance_smooth += (d_target - p->distance_smooth) * distance_impl::k_distance_slew;
        const float d = std::max(p->distance_smooth, d_min);

        // 2. Distance gain: (d_ref / d)^attenuation, uniform across channels.
        const float gain = (exponent > 0.0f) ? std::pow(d_ref / d, exponent) : 1.0f;

        // 3. Air absorption: shared one-pole low-pass, cutoff falling with
        //    excess distance beyond the reference.
        float lp_coeff = 0.0f;
        if (air > 0.0f) {
            const float excess = std::max(d - d_ref, 0.0f);
            const float fc     = 20000.0f / (1.0f + air * 0.1f * excess);
            lp_coeff           = std::min(1.0f - std::exp(-6.2831853f * fc / p->fs), 1.0f);
        }
        for (int c = 0; c < nch; ++c) {
            float v = frame[c] * gain;
            if (air > 0.0f) {
                float& s = p->lp_state[static_cast<size_t>(c)];
                s += lp_coeff * (v - s);
                v = s;
            }
            frame[c] = v;
        }

        // 4. Near-field compensation: per-order bass shelf referencing the
        //    decoder radius (identity when d == d_ref).
        if (nfc_on)
            p->nfc.process_frame(frame, frame);

        for (int c = 0; c < nch; ++c)
            out[c * n + i] = frame[c];
    }
    return w + 6;
}

static void distance_dsp(t_ambitap_distance_tilde* x, t_signal** sp) {
    distance_impl* p = x->p;
    p->fs            = static_cast<float>(sp[0]->s_sr);
    p->dop.prepare(p->fs);
    p->nfc.prepare(p->fs);
    std::fill(p->lp_state.begin(), p->lp_state.end(), 0.0f);
    p->distance_smooth = std::max(p->distance, ambitap::dsp::nfc::k_min_distance);
    signal_setmultiout(&sp[1], p->nch);
    dsp_add(distance_perform, 5, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans),
            sp[1]->s_vec, static_cast<t_int>(sp[0]->s_length));
}

static void distance_distance(t_ambitap_distance_tilde* x, t_floatarg f) {
    x->p->distance = static_cast<float>(f);
    x->p->dop.set_distance(static_cast<float>(f));
    x->p->nfc.set_source_distance(static_cast<float>(f));
}
static void distance_reference(t_ambitap_distance_tilde* x, t_floatarg f) {
    x->p->reference_distance = static_cast<float>(f);
    x->p->nfc.set_reference_distance(static_cast<float>(f));
}
static void distance_attenuation(t_ambitap_distance_tilde* x, t_floatarg f) {
    x->p->attenuation = std::max(static_cast<float>(f), 0.0f);
}
static void distance_air(t_ambitap_distance_tilde* x, t_floatarg f) {
    x->p->air_absorption = std::clamp(static_cast<float>(f), 0.0f, 1.0f);
}
static void distance_speed(t_ambitap_distance_tilde* x, t_floatarg f) {
    x->p->dop.set_speed_of_sound(static_cast<float>(f));
    x->p->nfc.set_speed_of_sound(static_cast<float>(f));
}
static void distance_maxdist(t_ambitap_distance_tilde* x, t_floatarg f) {
    x->p->dop.set_max_distance(static_cast<float>(f));
}
static void distance_doppler(t_ambitap_distance_tilde* x, t_floatarg f) {
    x->p->doppler_on = (f != 0);
}
static void distance_nfc(t_ambitap_distance_tilde* x, t_floatarg f) {
    x->p->nfc_on = (f != 0);
}

static void* distance_new(t_symbol*, int argc, t_atom* argv) {
    auto* x   = reinterpret_cast<t_ambitap_distance_tilde*>(pd_new(ambitap_distance_tilde_class));
    int   ord = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord       = std::clamp(ord, 1, ambitap::max_order);
    x->p      = new distance_impl(ord);
    x->x_f    = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void distance_free(t_ambitap_distance_tilde* x) { delete x->p; }

void ambitap_distance_tilde_setup(void) {
    t_class* c = class_new(gensym("ambitap.distance~"),
                           reinterpret_cast<t_newmethod>(distance_new),
                           reinterpret_cast<t_method>(distance_free),
                           sizeof(t_ambitap_distance_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    ambitap_distance_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_distance_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(distance_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(distance_distance), gensym("distance"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(distance_reference), gensym("reference_distance"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(distance_attenuation), gensym("attenuation"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(distance_air), gensym("air_absorption"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(distance_speed), gensym("speed_of_sound"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(distance_maxdist), gensym("max_distance"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(distance_doppler), gensym("doppler"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(distance_nfc), gensym("nfc"), A_FLOAT, 0);
}
