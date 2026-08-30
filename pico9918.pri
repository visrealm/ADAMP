# pico9918-core: the PICO9918 VDP engine, built by its own CMake as a static library.
# The generated pico9918_build_config.h records the ABI-affecting build choices and
# the public headers assert against it, so the build tree is on the include path too.

PICO9918_SRC   = $$PWD/3rdparty/pico9918-core
PICO9918_BUILD = $$OUT_PWD/pico9918-core

!exists($$PICO9918_SRC/CMakeLists.txt) {
    error("pico9918-core is missing - run: git submodule update --init --recursive")
}

INCLUDEPATH += $$PICO9918_SRC/src \
               $$PICO9918_BUILD/src

# Static archive. Without this the headers fall through to __declspec(dllimport)
# on WIN32 and every entry point comes out as __imp_*.
DEFINES += PICO9918_STATIC

# The library's version, packed major(4) | minor(4) | patch(8) as the config stamp
# wants it. Read from the library's CMakeLists rather than written here, so an upgrade
# cannot leave ADAMP stamping a stale version and skipping the field migration.
PICO9918_VERSION = $$cat($$PICO9918_SRC/CMakeLists.txt, lines)
PICO9918_VERSION = $$find(PICO9918_VERSION, "pico9918_core VERSION [0-9]")
PICO9918_VERSION = $$split(PICO9918_VERSION, " ")
PICO9918_VERSION = $$member(PICO9918_VERSION, 2)
PICO9918_VERSION_PARTS = $$split(PICO9918_VERSION, ".")
isEmpty(PICO9918_VERSION_PARTS) {
    error("pico9918-core: could not read the project version from CMakeLists.txt")
}
PICO9918_VER_MAJOR = $$member(PICO9918_VERSION_PARTS, 0)
PICO9918_VER_MINOR = $$member(PICO9918_VERSION_PARTS, 1)
PICO9918_VER_PATCH = $$member(PICO9918_VERSION_PARTS, 2)
DEFINES += PICO9918_CORE_VER_MAJOR=$$PICO9918_VER_MAJOR            PICO9918_CORE_VER_MINOR=$$PICO9918_VER_MINOR            PICO9918_CORE_VER_PATCH=$$PICO9918_VER_PATCH

# 80-column text at 8bpp: ECM, palette select and the bitmap layer in T80, as real
# F18A hardware does. One variable so the CMake option and the define cannot drift.
PICO9918_TEXT80_8BPP = ON
equals(PICO9918_TEXT80_8BPP, ON): DEFINES += PICO9918_TEXT80_8BPP=1

win32 {
    PICO9918_GENERATOR = MinGW Makefiles
} else {
    PICO9918_GENERATOR = Unix Makefiles
}

CONFIG(debug, debug|release) {
    PICO9918_BUILD_TYPE = Debug
} else {
    PICO9918_BUILD_TYPE = Release
}

# PICO9918_MODE=1 selects the F18A: the enhanced renderer and the TMS9900 GPU.
# Configure eagerly, while qmake parses: the CMake configure step writes
# pico9918_build_config.h, which every TU including pico9918.h needs before the first
# compile. PRE_TARGETDEPS only orders the link, far too late under a parallel make.
# ADAMP's pixel policy, force-included ahead of the library's headers. The desktop
# defaults are #ifndef-guarded, so this pins a correct one without editing
# pico9918-core. See bridge/pico9918_pixel_policy.h.
PICO9918_POLICY = $$PWD/bridge/pico9918_pixel_policy.h

PICO9918_CONFIGURE = cmake -S \"$$PICO9918_SRC\" -B \"$$PICO9918_BUILD\" -G \"$$PICO9918_GENERATOR\" -DCMAKE_BUILD_TYPE=$$PICO9918_BUILD_TYPE -DPICO9918_MODE=1 -DPICO9918_TEXT80_8BPP=$$PICO9918_TEXT80_8BPP -DCMAKE_C_FLAGS=\"-include $$PICO9918_POLICY\"
!system($$PICO9918_CONFIGURE) {
    error("pico9918-core: CMake configure failed - is cmake on PATH?")
}

pico9918core.target   = pico9918-core-lib
pico9918core.commands = cmake --build \"$$PICO9918_BUILD\" --config $$PICO9918_BUILD_TYPE --target pico9918_core
QMAKE_EXTRA_TARGETS += pico9918core
PRE_TARGETDEPS      += pico9918-core-lib

LIBS += -L$$PICO9918_BUILD/src -lpico9918_core
