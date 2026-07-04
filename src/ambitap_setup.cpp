/// @file
/// Library setup for the AmbiTap Pure Data externals. Pd calls ambitap_setup()
/// when the library loads (e.g. via [declare -lib ambitap]); it registers every
/// ambitap.*~ class. Each object's own <name>_setup() lives in its source file.

#include "m_pd.h"

void ambitap_encode_tilde_setup(void);
void ambitap_rotate_tilde_setup(void);
void ambitap_decode_tilde_setup(void);
void ambitap_binaural_tilde_setup(void);
void ambitap_mirror_tilde_setup(void);
void ambitap_format_tilde_setup(void);
void ambitap_vmic_tilde_setup(void);
void ambitap_directional_tilde_setup(void);
void ambitap_doppler_tilde_setup(void);
void ambitap_compress_tilde_setup(void);
void ambitap_energyvec_tilde_setup(void);
void ambitap_bed2hoa_tilde_setup(void);
void ambitap_distance_tilde_setup(void);
void ambitap_panbin_tilde_setup(void);
void ambitap_xtc_tilde_setup(void);
void ambitap_room_tilde_setup(void);

extern "C" void ambitap_setup(void) {
    post("AmbiTap: higher-order ambisonics externals (AmbiX: ACN/SN3D)");
    ambitap_encode_tilde_setup();
    ambitap_rotate_tilde_setup();
    ambitap_decode_tilde_setup();
    ambitap_binaural_tilde_setup();
    ambitap_mirror_tilde_setup();
    ambitap_format_tilde_setup();
    ambitap_vmic_tilde_setup();
    ambitap_directional_tilde_setup();
    ambitap_doppler_tilde_setup();
    ambitap_compress_tilde_setup();
    ambitap_energyvec_tilde_setup();
    ambitap_bed2hoa_tilde_setup();
    ambitap_distance_tilde_setup();
    ambitap_panbin_tilde_setup();
    ambitap_xtc_tilde_setup();
    ambitap_room_tilde_setup();
}
