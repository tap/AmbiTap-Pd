/// ambitap.compress~ (Pd) — spatial-image-preserving compressor for a HOA bus:
/// the detector keys off W and the same gain is applied to every channel.
/// Multichannel in and out; order is a creation argument. Envelope coefficients
/// depend on the host sample rate.

#include "ambitap_pd.h"

#include "ambitap/dsp/spatial_compressor.h"

using io_t = ambitap_pd::mc_io<ambitap::dsp::spatial_compressor>;

static t_class* ambitap_compress_tilde_class;

struct t_ambitap_compress_tilde {
    t_object x_obj;
    t_float  x_f;
    io_t*    io;
};

static t_int* compress_perform(t_int* w) {
    auto* x = reinterpret_cast<t_ambitap_compress_tilde*>(w[1]);
    x->io->run(reinterpret_cast<t_sample*>(w[2]), static_cast<int>(w[3]),
               reinterpret_cast<t_sample*>(w[4]), static_cast<int>(w[5]));
    return w + 6;
}

static void compress_dsp(t_ambitap_compress_tilde* x, t_signal** sp) {
    x->io->proc.prepare(static_cast<float>(sp[0]->s_sr));
    signal_setmultiout(&sp[1], x->io->nch);
    x->io->ensure_zero(sp[0]->s_length);
    dsp_add(compress_perform, 5, x, sp[0]->s_vec, static_cast<t_int>(sp[0]->s_nchans),
            sp[1]->s_vec, static_cast<t_int>(sp[0]->s_length));
}

static void compress_threshold(t_ambitap_compress_tilde* x, t_floatarg f) { x->io->proc.set_threshold_db(static_cast<float>(f)); }
static void compress_ratio(t_ambitap_compress_tilde* x, t_floatarg f) { x->io->proc.set_ratio(static_cast<float>(f)); }
static void compress_attack(t_ambitap_compress_tilde* x, t_floatarg f) { x->io->proc.set_attack(static_cast<float>(f)); }
static void compress_release(t_ambitap_compress_tilde* x, t_floatarg f) { x->io->proc.set_release(static_cast<float>(f)); }
static void compress_makeup(t_ambitap_compress_tilde* x, t_floatarg f) { x->io->proc.set_makeup_gain_db(static_cast<float>(f)); }

static void* compress_new(t_symbol*, int argc, t_atom* argv) {
    auto* x   = reinterpret_cast<t_ambitap_compress_tilde*>(pd_new(ambitap_compress_tilde_class));
    int   ord = (argc >= 1) ? static_cast<int>(atom_getfloat(argv)) : 1;
    ord       = std::clamp(ord, 1, ambitap::max_order);
    x->io     = new io_t(ord);
    x->x_f    = 0;
    outlet_new(&x->x_obj, &s_signal);
    return x;
}

static void compress_free(t_ambitap_compress_tilde* x) { delete x->io; }

void ambitap_compress_tilde_setup(void) {
    t_class* c = class_new(gensym("ambitap.compress~"),
                           reinterpret_cast<t_newmethod>(compress_new),
                           reinterpret_cast<t_method>(compress_free),
                           sizeof(t_ambitap_compress_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    ambitap_compress_tilde_class = c;
    CLASS_MAINSIGNALIN(c, t_ambitap_compress_tilde, x_f);
    class_addmethod(c, reinterpret_cast<t_method>(compress_dsp), gensym("dsp"), A_CANT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(compress_threshold), gensym("threshold"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(compress_ratio), gensym("ratio"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(compress_attack), gensym("attack"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(compress_release), gensym("release"), A_FLOAT, 0);
    class_addmethod(c, reinterpret_cast<t_method>(compress_makeup), gensym("makeup_gain"), A_FLOAT, 0);
}
