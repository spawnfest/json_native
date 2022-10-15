# Makefile for building the NIF
#
# Makefile targets:
#
# all/install   build and install the NIF
# clean         clean build products and intermediates
#
# Variables to override:
#
# MIX_APP_PATH  path to the build directory
# MIX_ENV       Mix build environment - "test" forces use of the stub
#
# CXX                C++ compiler
# CROSSCOMPILE       crosscompiler prefix, if any
# CXXFLAGS           compiler flags for compiling all C++ files
# ERL_CXXFLAGS       additional compiler flags for files using Erlang header files
# ERL_EI_INCLUDE_DIR include path to ei.h (Required for crosscompile)
# ERL_EI_LIBDIR      path to libei.a (Required for crosscompile)
# LDFLAGS            linker flags for linking all binaries
# ERL_LDFLAGS        additional linker flags for projects referencing Erlang libraries

PREFIX = $(MIX_APP_PATH)/priv
BUILD  = $(MIX_APP_PATH)/obj

NIF = $(PREFIX)/libjason.so

CXXFLAGS ?= -O2 -Wall -Wextra -pedantic -std=c++20 -fno-exceptions

# Check that we're on a supported build platform
ifeq ($(CROSSCOMPILE),)
    # Not crosscompiling, so check that we're on Linux.
    ifneq ($(shell uname -s),Linux)
        LDFLAGS += -undefined dynamic_lookup -dynamiclib
    else
        LDFLAGS += -fPIC -shared
        CXXFLAGS += -fPIC
    endif
else
# Crosscompiled build
LDFLAGS += -fPIC -shared
CXXFLAGS += -fPIC
endif

ifeq ($(MIX_ENV),test)
    CXXFLAGS += -DMIX_TEST
endif

# Set Erlang-specific compile and linker flags
ERL_CXXFLAGS ?= -I$(ERL_EI_INCLUDE_DIR)
ERL_LDFLAGS ?= -L$(ERL_EI_LIBDIR) -lei

SRC = c_src/jason.cxx
HEADERS =$(wildcard c_src/*.h)
OBJ = $(SRC:c_src/%.cxx=$(BUILD)/%.o)

all: install

install: $(PREFIX) $(BUILD) $(NIF)

$(OBJ): $(HEADERS) Makefile

$(BUILD)/%.o: c_src/%.cxx
	@echo " CXX $(notdir $@)"
	$(CXX) -c $(ERL_CXXFLAGS) $(CXXFLAGS) -o $@ $<

$(NIF): $(OBJ)
	@echo " LD  $(notdir $@)"
	$(CXX) -o $@ $(ERL_LDFLAGS) $(LDFLAGS) $^

$(PREFIX) $(BUILD):
	mkdir -p $@

clean:
	$(RM) $(NIF) $(OBJ)

.PHONY: all clean install

# Don't echo commands unless the caller exports "V=1"
${V}.SILENT:
