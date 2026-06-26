/// ambitap.energyvec~ (Pd) — active-intensity (energy vector) DOA estimate.
/// Uses the first four HOA channels (W, Y, Z, X); outputs x / y / z as three
/// signals. Multichannel in.

#include "ambitap_pd.h"

#include "ambitap/analysis/energy_vector.h"

static t_class* ambitap_energyvec_tilde_class;

struct energyvec_impl {
    ambitap::analysis::energy_vector ev;
    std::vector<const float*>        in4;
    std::vector<float>               zero;

    energyvec_impl() : in4(4) {}
};

struct t_ambitap_energyvec_tilde {
    t_object        x_obj;
    t_float         x_f;
    energyvec_impl* p;
};

static t_int* energyvec_perform(t_int* w) {
    auto*           x      = reinterpret_cast<t_ambitap_energyvec_tilde*>(w[1]);
    t_sample*       in     = reinterpret_cast<t_sample*>(w[2]);
    const int       in_nch = static_cast<int>(w[3]);
    t_sample*       ox     = reinterpret_cast<t_sample*>(w[4]);
    t_sample*       oy     = reinterpret_cast<t_sample*>(w[5]);
    t_sample*       oz     = reinterpret_cast<t_sample*>(w[6]);
    const int       n      = static_cast<int>(w[7]);
    energyvec_impl* p      = x->p;
    for (int c = 0; c < 4; ++c)
        p->in4[static_cast<size_t>(c)] =
            (c < in_nch) ? reinterpret_cast<const float*>(in + c * n) : p->zero.data();
    float* out3[3] = {reinterpret_cast<float*>(ox), reinterpret_cast<float*>(oy),
                      reinterpret_cast<float*>(oz)};
    p->ev.process(p->in4.data(), out3, static_cast<size_t>(n));
    return w + 8;
}

static void energyvec_dsp(t_ambitap_energyvec_tilde* x, t_signal** sp) {
    x->p->ev.prepare(static_cast<float>(sp[0]->s_sr));
    signal_setmultiout(&sp[1], 1);
    signal_setmultiout(&sp[2], 1);
    signal_setmultiout(&sp[3], 1);
    if (static_cast<int>(x->p->zero.size()) < sp[0]->s_length)
        x->p->zero.assign(static_cast<size_t>(sp[0]->s_length), 0.0f);
    dsp_add(energyvec_perform, 7, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans),
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, static_cast<t_int>(sp[0]->s_length));
}

static void energyvec_smoothing(t_ambitap_energyvec_tilde* x, t_floatarg f) {
    x->p->ev.set_smoothing_time(static_cast<float>(f));
}

static void* energyvec_new(t_symbol*, int, t_atom*) {
    auto* x = reinterpret_cast<t_ambitap_energyvec_tilde*>(pd_new(ambitap_energyvec_tilde_class));
    x->p    = new energyvec_impl();
    x->x_f  = 0;
    outlet_new(&x->x_obj, &s_signal);  // x
    outlet_new(&x->x_obj, &s_signal);  // y
    outlet_new(&x->x_obj, &s_signal);  // z
    return x;
}

static void energyvec_free(t_ambitap_energyvec_tilde* x) { delete x->p; }

void ambitap_energyvec_tilde_setup(void) {
    t_class* c = class_new(gensym("ambitap.energyvec~"),
                           reinterpret_cast<t_newmethod>(energyvec_new),
                           reinterpret_cast<t_method>(energyvec_free),
                           sizeof(t_ambitap_energyvec_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    ambitap_energyvec_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_energyvec_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(energyvec_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(energyvec_smoothing), gensym("smoothing_time"), A_FLOAT, 0);
}
