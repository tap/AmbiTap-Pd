#!/usr/bin/env python3
"""Generate Pure Data help patches (<name>-help.pd) for the AmbiTap externals.

A .pd file is a flat list of records. Object indices for `#X connect` count
every placed item (obj/msg/text/floatatom) in file order, 0-based. This builds
patches via a Patch class that returns an index handle for each item, so
connections never reference a hand-counted number.
"""
import os

OUT = os.path.dirname(os.path.abspath(__file__))


class Patch:
    def __init__(self, w=660, h=540, font=12):
        self.lines = [f"#N canvas 40 40 {w} {h} {font};"]
        self.n = 0
        self.conns = []

    def _add(self, rec):
        self.lines.append(rec)
        idx = self.n
        self.n += 1
        return idx

    def obj(self, x, y, text):
        return self._add(f"#X obj {x} {y} {text};")

    def msg(self, x, y, text):
        return self._add(f"#X msg {x} {y} {text};")

    def text(self, x, y, text):
        return self._add(f"#X text {x} {y} {text};")

    def fatom(self, x, y, w=5):
        return self._add(f"#X floatatom {x} {y} {w} 0 0 0 - - - 0;")

    def connect(self, a, ao, b, bi):
        self.conns.append(f"#X connect {a} {ao} {b} {bi};")

    def render(self):
        return "\n".join(self.lines + self.conns) + "\n"


def header(p, name, desc):
    p.text(20, 14, f"{name}")
    p.text(20, 32, desc)
    p.text(20, 50, "AmbiX convention: ACN channel ordering + SN3D normalization.")
    p.obj(20, h_decl_y, "declare -lib ambitap")


h_decl_y = 506  # bottom-left declare object


def live_ctrl(p, x, y, selector, init_label):
    """A number box -> [selector $1] message -> returns the message-box index."""
    fa = p.fatom(x, y)
    m = p.msg(x, y + 24, f"{selector} $1")
    p.connect(fa, 0, m, 0)
    p.text(x + 60, y + 2, init_label)
    return m


def chain_source(p):
    """osc~ 220 at a fixed spot; returns its index."""
    return p.obj(40, 110, "osc~ 220")


def write(name, p):
    path = os.path.join(OUT, f"{name}-help.pd")
    with open(path, "w") as f:
        f.write(p.render())
    return path


# ---------------------------------------------------------------- encode~
def gen_encode():
    p = Patch()
    header(p, "ambitap.encode~ <order>",
           "Encode a mono source to higher-order ambisonics (point source).")
    src = chain_source(p)
    enc = p.obj(40, 200, "ambitap.encode~ 3")
    bin = p.obj(40, 300, "ambitap.binaural~ 3")
    dac = p.obj(40, 360, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    az = live_ctrl(p, 340, 120, "azimuth", "azimuth (deg)")
    el = p.msg(340, 200, "elevation 30")
    gn = p.msg(340, 240, "gain 1")
    p.connect(az, 0, enc, 0)
    p.connect(el, 0, enc, 0)
    p.connect(gn, 0, enc, 0)
    p.text(200, 203, "outlet: (order+1)^2 HOA channels (multichannel)")
    p.text(200, 303, "headphone monitor (binaural)")
    p.text(340, 300, "messages: azimuth / elevation / gain")
    write("ambitap.encode~", p)


# ---------------------------------------------------------------- rotate~
def gen_rotate():
    p = Patch()
    header(p, "ambitap.rotate~ <order>",
           "Rotate a HOA bus by yaw / pitch / roll (Euler Z-Y-X degrees).")
    src = chain_source(p)
    enc = p.obj(40, 180, "ambitap.encode~ 3")
    rot = p.obj(40, 250, "ambitap.rotate~ 3")
    bin = p.obj(40, 330, "ambitap.binaural~ 3")
    dac = p.obj(40, 390, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, rot, 0)
    p.connect(rot, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    yaw = live_ctrl(p, 340, 120, "yaw", "yaw (deg)")
    pit = p.msg(340, 200, "pitch 0")
    rol = p.msg(340, 240, "roll 0")
    p.connect(yaw, 0, rot, 0)
    p.connect(pit, 0, rot, 0)
    p.connect(rol, 0, rot, 0)
    p.text(200, 253, "HOA in -> HOA out (same order)")
    p.text(340, 300, "messages: yaw / pitch / roll")
    write("ambitap.rotate~", p)


# ---------------------------------------------------------------- decode~
def gen_decode():
    p = Patch()
    header(p, "ambitap.decode~ <order> <layout>",
           "Decode a HOA bus to a loudspeaker layout.")
    src = chain_source(p)
    enc = p.obj(40, 180, "ambitap.encode~ 3")
    dec = p.obj(40, 250, "ambitap.decode~ 3 quad")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, dec, 0)
    mm = p.msg(340, 130, "decoder_type mode_match")
    ar = p.msg(340, 165, "decoder_type allrad")
    ep = p.msg(340, 200, "decoder_type epad")
    mr1 = p.msg(340, 245, "max_re 1")
    mr0 = p.msg(420, 245, "max_re 0")
    for m in (mm, ar, ep, mr1, mr0):
        p.connect(m, 0, dec, 0)
    p.text(200, 253, "outlet: speaker feeds (multichannel)")
    p.text(40, 300, "for N>2 speakers monitor with [dac~ 1 2 3 4 ...]")
    p.text(40, 330, "layouts: stereo quad surround_5_1 surround_7_1 surround_7_1_4 cube hexagon octagon")
    p.text(40, 350, "5.1 / 7.1 / 7.1.4 are also accepted as layout names")
    p.text(340, 100, "decoder_type:")
    p.text(340, 285, "max_re weighting on / off")
    write("ambitap.decode~", p)


# ---------------------------------------------------------------- binaural~
def gen_binaural():
    p = Patch()
    header(p, "ambitap.binaural~ <order>",
           "Decode a HOA bus to binaural stereo (built-in MIT KEMAR, orders 1-5).")
    src = chain_source(p)
    enc = p.obj(40, 180, "ambitap.encode~ 3")
    bin = p.obj(40, 260, "ambitap.binaural~ 3")
    dac = p.obj(40, 330, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    yaw = live_ctrl(p, 340, 120, "yaw", "head yaw (deg)")
    pit = p.msg(340, 200, "pitch 0")
    rol = p.msg(340, 240, "roll 0")
    vol = p.msg(340, 280, "volume 1")
    ls = p.msg(340, 320, "hrtf_dataset ls")
    ml = p.msg(460, 320, "hrtf_dataset magls")
    for m in (yaw, pit, rol, vol, ls, ml):
        p.connect(m, 0, bin, 0)
    p.text(200, 263, "left / right headphone outs")
    p.text(340, 300, "HRTF projection:")
    p.text(340, 360, "head-tracking: yaw / pitch / roll")
    write("ambitap.binaural~", p)


# ---------------------------------------------------------------- mirror~
def gen_mirror():
    p = Patch()
    header(p, "ambitap.mirror~ <order>",
           "Mirror a HOA bus across cardinal planes (left-right / front-back / up-down).")
    src = chain_source(p)
    enc = p.obj(40, 180, "ambitap.encode~ 3")
    mir = p.obj(40, 250, "ambitap.mirror~ 3")
    bin = p.obj(40, 330, "ambitap.binaural~ 3")
    dac = p.obj(40, 390, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, mir, 0)
    p.connect(mir, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    lr1 = p.msg(340, 130, "flip_lr 1")
    lr0 = p.msg(420, 130, "flip_lr 0")
    fb1 = p.msg(340, 170, "flip_fb 1")
    fb0 = p.msg(420, 170, "flip_fb 0")
    ud1 = p.msg(340, 210, "flip_ud 1")
    ud0 = p.msg(420, 210, "flip_ud 0")
    for m in (lr1, lr0, fb1, fb0, ud1, ud0):
        p.connect(m, 0, mir, 0)
    p.text(200, 253, "HOA in -> mirrored HOA out")
    p.text(340, 100, "sign-flip toggles (1 = on, 0 = off):")
    write("ambitap.mirror~", p)


# ---------------------------------------------------------------- format~
def gen_format():
    p = Patch()
    header(p, "ambitap.format~ <order>",
           "Convert an ambisonics bus between FuMa and AmbiX (orders 0-3).")
    src = chain_source(p)
    enc = p.obj(40, 170, "ambitap.encode~ 3")
    f1 = p.obj(40, 230, "ambitap.format~ 3")
    f2 = p.obj(40, 290, "ambitap.format~ 3")
    bin = p.obj(40, 360, "ambitap.binaural~ 3")
    dac = p.obj(40, 420, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, f1, 0)
    p.connect(f1, 0, f2, 0)
    p.connect(f2, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    toF = p.msg(340, 130, "direction ambix_to_fuma")
    toA = p.msg(340, 170, "direction fuma_to_ambix")
    p.connect(toF, 0, f1, 0)
    p.connect(toA, 0, f2, 0)
    p.text(230, 233, "AmbiX -> FuMa")
    p.text(230, 293, "FuMa -> AmbiX (round-trip back to AmbiX for monitoring)")
    p.text(340, 100, "direction:")
    write("ambitap.format~", p)


# ---------------------------------------------------------------- vmic~
def gen_vmic():
    p = Patch()
    header(p, "ambitap.vmic~ <order>",
           "Virtual mic: extract a mono directional signal from a HOA bus.")
    src = chain_source(p)
    enc = p.obj(40, 180, "ambitap.encode~ 3")
    vm = p.obj(40, 250, "ambitap.vmic~ 3")
    dac = p.obj(40, 330, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, vm, 0)
    p.connect(vm, 0, dac, 0)
    p.connect(vm, 0, dac, 1)
    az = live_ctrl(p, 340, 120, "azimuth", "look azimuth (deg)")
    el = p.msg(340, 200, "elevation 0")
    mr1 = p.msg(340, 240, "max_re 1")
    mr0 = p.msg(420, 240, "max_re 0")
    for m in (az, el, mr1, mr0):
        p.connect(m, 0, vm, 0)
    p.text(200, 253, "mono directional output")
    p.text(340, 280, "max_re weighting on / off")
    write("ambitap.vmic~", p)


# ---------------------------------------------------------------- directional~
def gen_directional():
    p = Patch()
    header(p, "ambitap.directional~ <order>",
           "Per-direction gain on a HOA bus (1 = bypass, >1 boost, <1 attenuate).")
    src = chain_source(p)
    enc = p.obj(40, 180, "ambitap.encode~ 3")
    dl = p.obj(40, 250, "ambitap.directional~ 3")
    bin = p.obj(40, 330, "ambitap.binaural~ 3")
    dac = p.obj(40, 390, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, dl, 0)
    p.connect(dl, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    gn = live_ctrl(p, 340, 120, "gain", "gain (linear)")
    az = p.msg(340, 200, "azimuth 90")
    el = p.msg(340, 240, "elevation 0")
    for m in (gn, az, el):
        p.connect(m, 0, dl, 0)
    p.text(200, 253, "HOA in -> shaped HOA out")
    p.text(340, 290, "messages: gain / azimuth / elevation")
    write("ambitap.directional~", p)


# ---------------------------------------------------------------- doppler~
def gen_doppler():
    p = Patch()
    header(p, "ambitap.doppler~ <order>",
           "Variable propagation delay on a HOA bus (modulate distance for Doppler).")
    src = chain_source(p)
    enc = p.obj(40, 180, "ambitap.encode~ 3")
    dp = p.obj(40, 250, "ambitap.doppler~ 3")
    bin = p.obj(40, 330, "ambitap.binaural~ 3")
    dac = p.obj(40, 390, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, dp, 0)
    p.connect(dp, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    dist = live_ctrl(p, 340, 120, "distance", "distance (m)")
    sos = p.msg(340, 200, "speed_of_sound 343")
    md = p.msg(340, 240, "max_distance 100")
    for m in (dist, sos, md):
        p.connect(m, 0, dp, 0)
    p.text(200, 253, "HOA in -> delayed HOA out")
    p.text(340, 290, "sweep distance for a moving-source Doppler shift")
    write("ambitap.doppler~", p)


# ---------------------------------------------------------------- compress~
def gen_compress():
    p = Patch()
    header(p, "ambitap.compress~ <order>",
           "Spatial compressor: detector keys off W, one gain applied to all channels.")
    src = p.obj(40, 110, "noise~")
    enc = p.obj(40, 180, "ambitap.encode~ 3")
    cp = p.obj(40, 250, "ambitap.compress~ 3")
    bin = p.obj(40, 330, "ambitap.binaural~ 3")
    dac = p.obj(40, 390, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, cp, 0)
    p.connect(cp, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    th = live_ctrl(p, 340, 120, "threshold", "threshold (dB)")
    ra = p.msg(340, 200, "ratio 4")
    at = p.msg(340, 240, "attack 10")
    re = p.msg(340, 280, "release 100")
    mk = p.msg(340, 320, "makeup_gain 6")
    for m in (th, ra, at, re, mk):
        p.connect(m, 0, cp, 0)
    p.text(200, 253, "image-preserving HOA compression")
    p.text(340, 360, "messages: threshold ratio attack release makeup_gain")
    write("ambitap.compress~", p)


# ---------------------------------------------------------------- energyvec~
def gen_energyvec():
    p = Patch()
    header(p, "ambitap.energyvec~",
           "Active-intensity DOA: x / y / z energy vector from the first 4 HOA channels.")
    src = chain_source(p)
    enc = p.obj(40, 180, "ambitap.encode~ 1")
    ev = p.obj(40, 250, "ambitap.energyvec~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, ev, 0)
    # metronome-driven snapshots of the three signal outputs
    start = p.msg(330, 110, "1")
    stop = p.msg(360, 110, "0")
    metro = p.obj(330, 150, "metro 50")
    p.connect(start, 0, metro, 0)
    p.connect(stop, 0, metro, 0)
    labels = ["x", "y", "z"]
    for i, lab in enumerate(labels):
        sx = 330 + i * 90
        snap = p.obj(sx, 210, "snapshot~")
        fa = p.fatom(sx, 250)
        p.connect(ev, i, snap, 0)
        p.connect(metro, 0, snap, 0)
        p.connect(snap, 0, fa, 0)
        p.text(sx, 285, lab)
    sm = p.msg(40, 320, "smoothing_time 50")
    p.connect(sm, 0, ev, 0)
    p.text(200, 253, "3 signal outlets: x / y / z")
    p.text(330, 90, "click 1 to poll the vector:")
    p.text(40, 350, "smoothing_time sets the averaging window (ms)")
    write("ambitap.energyvec~", p)


# ---------------------------------------------------------------- bed2hoa~
def gen_bed2hoa():
    p = Patch()
    header(p, "ambitap.bed2hoa~ <order> <layout>",
           "Encode a channel-based surround bed to HOA (each speaker feed at its "
           "canonical direction).")
    src = chain_source(p)
    enc = p.obj(40, 170, "ambitap.encode~ 3")
    dec = p.obj(40, 230, "ambitap.decode~ 3 quad")
    b2h = p.obj(40, 290, "ambitap.bed2hoa~ 3 quad")
    bin = p.obj(40, 360, "ambitap.binaural~ 3")
    dac = p.obj(40, 420, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, dec, 0)
    p.connect(dec, 0, b2h, 0)
    p.connect(b2h, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    gn = p.msg(340, 130, "gain 1")
    p.connect(gn, 0, b2h, 0)
    p.text(230, 233, "quad speaker feeds (a demo bed)")
    p.text(230, 293, "bed -> (order+1)^2 HOA channels")
    p.text(40, 460, "feed real speaker channels in layout order (no LFE); "
                    "layouts match ambitap.decode~")
    p.text(340, 100, "message: gain")
    write("ambitap.bed2hoa~", p)


# ---------------------------------------------------------------- distance~
def gen_distance():
    p = Patch()
    header(p, "ambitap.distance~ <order>",
           "Distance cues on a HOA bus: Doppler delay, 1/r gain, air absorption, NFC.")
    src = chain_source(p)
    enc = p.obj(40, 180, "ambitap.encode~ 3")
    ds = p.obj(40, 250, "ambitap.distance~ 3")
    bin = p.obj(40, 330, "ambitap.binaural~ 3")
    dac = p.obj(40, 390, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, ds, 0)
    p.connect(ds, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    dist = live_ctrl(p, 340, 120, "distance", "distance (m)")
    ref = p.msg(340, 200, "reference_distance 1")
    att = p.msg(340, 236, "attenuation 1")
    air = p.msg(340, 272, "air_absorption 0.5")
    sos = p.msg(340, 308, "speed_of_sound 343")
    md = p.msg(340, 344, "max_distance 50")
    dop1 = p.msg(340, 384, "doppler 1")
    dop0 = p.msg(420, 384, "doppler 0")
    nfc1 = p.msg(340, 420, "nfc 1")
    nfc0 = p.msg(420, 420, "nfc 0")
    for m in (dist, ref, att, air, sos, md, dop1, dop0, nfc1, nfc0):
        p.connect(m, 0, ds, 0)
    p.text(200, 253, "HOA in -> HOA out with distance cues")
    p.text(40, 430, "sweep distance for a moving-source Doppler glide")
    write("ambitap.distance~", p)


# ---------------------------------------------------------------- panbin~
def gen_panbin():
    p = Patch()
    header(p, "ambitap.panbin~",
           "Direct per-source binaural panner (per-source HRTF, no ambisonic bus).")
    src = chain_source(p)
    pb = p.obj(40, 250, "ambitap.panbin~")
    dac = p.obj(40, 330, "dac~")
    p.connect(src, 0, pb, 0)
    p.connect(pb, 0, dac, 0)
    p.connect(pb, 1, dac, 1)
    az = live_ctrl(p, 340, 120, "azimuth", "azimuth (deg)")
    el = p.msg(340, 200, "elevation 0")
    gn = p.msg(340, 240, "gain 1")
    for m in (az, el, gn):
        p.connect(m, 0, pb, 0)
    p.text(200, 253, "mono in -> left / right (direction crossfades click-free)")
    p.text(340, 290, "messages: azimuth / elevation / gain")
    write("ambitap.panbin~", p)


# ---------------------------------------------------------------- xtc~
def gen_xtc():
    p = Patch()
    header(p, "ambitap.xtc~",
           "Transaural crosstalk cancellation: binaural/stereo over two loudspeakers.")
    src = chain_source(p)
    enc = p.obj(40, 170, "ambitap.encode~ 3")
    bin = p.obj(40, 240, "ambitap.binaural~ 3")
    xtc = p.obj(40, 320, "ambitap.xtc~")
    dac = p.obj(40, 390, "dac~")
    p.connect(src, 0, enc, 0)
    p.connect(enc, 0, bin, 0)
    p.connect(bin, 0, xtc, 0)
    p.connect(bin, 1, xtc, 1)
    p.connect(xtc, 0, dac, 0)
    p.connect(xtc, 1, dac, 1)
    span = live_ctrl(p, 340, 120, "span", "speaker span (deg)")
    dist = p.msg(340, 200, "distance 1")
    reg = p.msg(340, 240, "regularization 0.5")
    by1 = p.msg(340, 284, "bypass 1")
    by0 = p.msg(420, 284, "bypass 0")
    for m in (span, dist, reg, by1, by0):
        p.connect(m, 0, xtc, 0)
    p.text(200, 323, "left / right LOUDSPEAKER feeds (not headphones)")
    p.text(340, 300, "bypass = A/B reference (loudness-match upstream)")
    p.text(40, 430, "512-sample filter latency; ~-12 dB makeup on the cancelled path")
    write("ambitap.xtc~", p)


# ---------------------------------------------------------------- room~
def gen_room():
    p = Patch()
    header(p, "ambitap.room~ <order>",
           "Shoebox room on a HOA bus: image-source early reflections + FDN tail.")
    src = chain_source(p)
    rm = p.obj(40, 250, "ambitap.room~ 3")
    bin = p.obj(40, 330, "ambitap.binaural~ 3")
    dac = p.obj(40, 390, "dac~")
    p.connect(src, 0, rm, 0)
    p.connect(rm, 0, bin, 0)
    p.connect(bin, 0, dac, 0)
    p.connect(bin, 1, dac, 1)
    rt = live_ctrl(p, 340, 120, "rt60", "RT60 (s)")
    dx = p.msg(340, 200, "dim_x 7")
    dy = p.msg(340, 236, "dim_y 5")
    dz = p.msg(340, 272, "dim_z 3")
    dir1 = p.msg(340, 312, "direct 1")
    dir0 = p.msg(420, 312, "direct 0")
    er1 = p.msg(340, 348, "er 1")
    er0 = p.msg(420, 348, "er 0")
    tl1 = p.msg(340, 384, "tail 1")
    tl0 = p.msg(420, 384, "tail 0")
    for m in (rt, dx, dy, dz, dir1, dir0, er1, er0, tl1, tl0):
        p.connect(m, 0, rm, 0)
    p.text(200, 253, "mono in -> (order+1)^2 HOA channels (wet room)")
    p.text(40, 430, "also: source_x/y/z, listener_x/y/z, gain, "
                    "rt60band <hz> <s>, reflections <x0 x1 y0 y1 z0 z1>")
    p.text(40, 450, "note: ~53 ms fixed latency at 48 kHz on every path")
    write("ambitap.room~", p)


def main():
    os.makedirs(OUT, exist_ok=True)
    gens = [gen_encode, gen_rotate, gen_decode, gen_binaural, gen_mirror,
            gen_format, gen_vmic, gen_directional, gen_doppler, gen_compress,
            gen_energyvec, gen_bed2hoa, gen_distance, gen_panbin, gen_xtc,
            gen_room]
    for g in gens:
        g()
    files = sorted(os.listdir(OUT))
    print(f"wrote {len([f for f in files if f.endswith('-help.pd')])} help patches to {OUT}")
    for f in files:
        if f.endswith("-help.pd"):
            print("  ", f)


if __name__ == "__main__":
    main()
