/// @file
/// Shared helpers for the AmbiTap Pure Data externals.
///
/// pd_new() allocates raw, zero-filled memory and does NOT run C++ constructors,
/// so a Pd object struct must not contain C++ members with non-trivial ctors
/// (std::vector, the AmbiTap processors, ...). The pattern here: the Pd struct
/// holds a POINTER to a C++ "impl" that is `new`-ed in the object's new method
/// and `delete`-d in its free method. mc_io is such an impl for the common case
/// (multichannel in and out, equal channel counts, a planar process()).

#pragma once

#include "m_pd.h"

#include <algorithm>
#include <type_traits>
#include <vector>

static_assert(std::is_same<t_sample, float>::value,
              "AmbiTap-Pd assumes a 32-bit-sample Pd (t_sample == float). For a "
              "64-bit-sample Pd, add float<->double conversion in the perform routines.");

namespace ambitap_pd {

/// Holds a planar-process AmbiTap processor plus reusable channel-pointer scratch
/// for an object whose input and output are one multichannel signal of the same
/// channel count (== proc.channels()). Pd signal vectors are channel-major
/// (channel c starts at s_vec + c*blocksize), so we hand the processor an array
/// of per-channel pointers into the signal buffers — no per-sample copying.
template <typename Proc>
struct mc_io {
    Proc                      proc;
    int                       nch;
    std::vector<const float*> in_ptrs;
    std::vector<float*>       out_ptrs;
    std::vector<float>        zero;  // for input channels the upstream didn't supply

    explicit mc_io(int order)
        : proc(order)
        , nch(static_cast<int>(proc.channels()))
        , in_ptrs(static_cast<size_t>(nch))
        , out_ptrs(static_cast<size_t>(nch)) {}

    void ensure_zero(int n) {
        if (static_cast<int>(zero.size()) < n)
            zero.assign(static_cast<size_t>(n), 0.0f);
    }

    void run(t_sample* in, int in_nch, t_sample* out, int n) {
        for (int c = 0; c < nch; ++c) {
            in_ptrs[static_cast<size_t>(c)] =
                (c < in_nch) ? reinterpret_cast<const float*>(in + c * n) : zero.data();
            out_ptrs[static_cast<size_t>(c)] = reinterpret_cast<float*>(out + c * n);
        }
        proc.process(in_ptrs.data(), out_ptrs.data(), static_cast<size_t>(n));
    }
};

}  // namespace ambitap_pd
