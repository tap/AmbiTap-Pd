# AmbiTap-Pd — Pure Data externals over the AmbiTap library.
#
# Builds a single library `ambitap.pd_darwin` (universal x86_64+arm64) containing
# every ambitap.*~ class; load it in Pd with [declare -lib ambitap].
#
# Pd's symbols resolve at load time (-undefined dynamic_lookup), so this builds
# against the vendored pd/m_pd.h header alone — no Pd binary required.
#
# Dependencies:
#   AMBITAP_ROOT  the AmbiTap library (bundled as the submodules/AmbiTap
#                 submodule) — headers, plus its vendored Ooura fftsg.c (the FFT
#                 for binaural). Override to build against a checkout elsewhere.
#   EIGEN_ROOT    Eigen headers (rotate/decode/binaural matrix math). Defaults to
#                 the Homebrew location; override if elsewhere.

AMBITAP_ROOT ?= submodules/AmbiTap
PD_INCLUDE   ?= pd
EIGEN_ROOT   ?= /usr/local/include/eigen3

ARCHS    := -arch x86_64 -arch arm64
CXX      := c++
CC       := cc
CPPFLAGS := -DPD -I$(PD_INCLUDE) -I$(AMBITAP_ROOT)/include -I$(EIGEN_ROOT)
# C++20: the AmbiTap core uses std::bit_cast (math/core/fast_math.h).
CXXFLAGS := -std=c++20 -O2 -fPIC $(ARCHS) -Wall
CFLAGS   := -O2 -fPIC $(ARCHS)
# -undefined dynamic_lookup leaves Pd's symbols to be resolved at load; -lc++
# links the C++ runtime (std::thread etc. used by the async processors).
LDFLAGS  := -bundle -undefined dynamic_lookup -lc++ $(ARCHS)

SRC  := $(wildcard src/*.cpp)
OBJ  := $(SRC:.cpp=.o)
FFT  := $(AMBITAP_ROOT)/third_party/ooura/fftsg.c
FFTO := build/fftsg.o
LIB  := externals/ambitap.pd_darwin

all: $(LIB)

$(LIB): $(OBJ) $(FFTO)
	@mkdir -p externals
	$(CXX) $(LDFLAGS) -o $@ $(OBJ) $(FFTO)

src/%.o: src/%.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(FFTO): $(FFT)
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(FFTO) $(LIB)

.PHONY: all clean
