# AmbiTap-Pd — Pure Data externals over the AmbiTap library.
#
# Builds a single Pd library, "ambitap", containing every ambitap.*~ class;
# load it in Pd with [declare -lib ambitap]. Cross-platform: produces
# ambitap.pd_darwin (macOS), ambitap.pd_linux (Linux), or ambitap.dll (Windows)
# in externals/.
#
# The AmbiTap core is consumed as a subproject (submodules/AmbiTap): its
# AmbiTap::ambitap INTERFACE target brings the headers, Eigen, the Ooura FFT
# (used by binaural/panbin/xtc/room), and C++20 — so nothing here recompiles
# third-party sources by hand. Point -DAMBITAP_CORE_DIR=/path elsewhere to
# build against a core checkout outside the submodule.
#
# Configure/build (single-config generators):
#   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --parallel
# macOS universal:
#   cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"

cmake_minimum_required(VERSION 3.24)

project(AmbiTap-Pd LANGUAGES C CXX)

# ------------------------------------------------------------------------------
# AmbiTap core (headers + Eigen + Ooura FFT + C++20, via AmbiTap::ambitap).
# Consumed as a subproject, so its own tests/examples/install auto-disable.
# ------------------------------------------------------------------------------
set(AMBITAP_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/submodules/AmbiTap"
    CACHE PATH "AmbiTap core source directory (the git submodule by default)")

if(NOT EXISTS "${AMBITAP_CORE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "AmbiTap core not found at ${AMBITAP_CORE_DIR}. Initialize the submodule "
        "(git submodule update --init --recursive) or pass -DAMBITAP_CORE_DIR=/path.")
endif()

add_subdirectory("${AMBITAP_CORE_DIR}" "${CMAKE_BINARY_DIR}/ambitap-core")

# ------------------------------------------------------------------------------
# The single Pd library. One .cpp per class under src/ (+ ambitap_setup.cpp,
# which registers them all). The core already defines a target named "ambitap",
# so this target is ambitap_pd with OUTPUT_NAME ambitap.
# ------------------------------------------------------------------------------
file(GLOB AMBITAP_PD_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(ambitap_pd MODULE ${AMBITAP_PD_SOURCES})

target_include_directories(ambitap_pd PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/pd")
target_compile_features(ambitap_pd PRIVATE cxx_std_20)
target_compile_definitions(ambitap_pd PRIVATE PD)
target_link_libraries(ambitap_pd PRIVATE AmbiTap::ambitap)

set_target_properties(ambitap_pd PROPERTIES
    OUTPUT_NAME ambitap
    PREFIX ""
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/externals"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/externals")

# ------------------------------------------------------------------------------
# Per-platform Pd external extension + how Pd's own symbols get resolved.
#   macOS  — .pd_darwin; Pd symbols resolved at load (-undefined dynamic_lookup).
#   Linux  — .pd_linux;  undefined Pd symbols resolve against the host Pd at
#            load (the default for a shared object), so no extra link flags.
#   Windows— .dll;       Pd symbols must be linked against Pd's import library
#            (pd.lib) — set -DPD_LIB=/path/to/pd.lib (see README).
# ------------------------------------------------------------------------------
if(APPLE)
    set_target_properties(ambitap_pd PROPERTIES SUFFIX ".pd_darwin")
    target_link_options(ambitap_pd PRIVATE LINKER:-undefined,dynamic_lookup)
elseif(WIN32)
    set_target_properties(ambitap_pd PROPERTIES SUFFIX ".dll")
    if(NOT DEFINED PD_LIB)
        message(FATAL_ERROR
            "Windows build needs Pd's import library: pass -DPD_LIB=/path/to/pd.lib "
            "(from a Pd Windows distribution's bin/ — see .github/workflows/ci.yml).")
    endif()
    target_compile_definitions(ambitap_pd PRIVATE MSW)   # Pd's Windows platform macro
    target_link_libraries(ambitap_pd PRIVATE "${PD_LIB}")
    if(MSVC)
        # Pd finds the library's entry point by name; MSVC exports nothing from
        # a DLL unless told to, so export the (extern "C") setup function.
        target_link_options(ambitap_pd PRIVATE "/EXPORT:ambitap_setup")
    endif()
else()
    set_target_properties(ambitap_pd PROPERTIES SUFFIX ".pd_linux")
endif()
