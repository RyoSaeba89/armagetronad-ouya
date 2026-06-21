cmake_minimum_required(VERSION 3.18)
project(armagetronad CXX C)

# =============================================================================
# Armagetron Advanced -> OUYA (Tegra 3 / Android API 16 / armeabi-v7a / GLES2)
# Rendering: gl4es translates the engine's immediate-mode OpenGL to GLES2.
# This CMakeLists assembles the native shared lib `libmain.so` (SDL2 entry).
# =============================================================================

set(CMAKE_CXX_STANDARD 17)            # current protobuf requires C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ARMA      "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../armagetronad/src")
set(GEN       "${CMAKE_CURRENT_SOURCE_DIR}/gen/protobuf")
set(LIBDIR    "${CMAKE_CURRENT_SOURCE_DIR}/lib/${ANDROID_ABI}")
set(LIB       "${CMAKE_CURRENT_SOURCE_DIR}/lib")
set(INC       "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(BOOST_INC "${CMAKE_CURRENT_SOURCE_DIR}/../../../../../deps-staging/boost_1_82_0")

# --- prebuilt / staged dependencies (IMPORTED) -------------------------------
add_library(sdl2        SHARED IMPORTED)
set_target_properties(sdl2        PROPERTIES IMPORTED_LOCATION ${LIB}/SDL2/${ANDROID_ABI}/libSDL2.so)
add_library(hidapi      SHARED IMPORTED)
set_target_properties(hidapi      PROPERTIES IMPORTED_LOCATION ${LIB}/SDL2/${ANDROID_ABI}/libhidapi.so)
add_library(sdl2_image  SHARED IMPORTED)
set_target_properties(sdl2_image  PROPERTIES IMPORTED_LOCATION ${LIB}/sdl_image/${ANDROID_ABI}/libSDL2_image.so)
add_library(sdl2_mixer  SHARED IMPORTED)
set_target_properties(sdl2_mixer  PROPERTIES IMPORTED_LOCATION ${LIB}/sdl_mixer/${ANDROID_ABI}/libSDL2_mixer.so)
add_library(gl4es       STATIC IMPORTED)
set_target_properties(gl4es       PROPERTIES IMPORTED_LOCATION ${LIB}/gl4es/${ANDROID_ABI}/libGL.a)
add_library(freetype    STATIC IMPORTED)
set_target_properties(freetype    PROPERTIES IMPORTED_LOCATION ${LIB}/freetype/${ANDROID_ABI}/libfreetype.a)
add_library(ftgl        STATIC IMPORTED)
set_target_properties(ftgl        PROPERTIES IMPORTED_LOCATION ${LIB}/ftgl/${ANDROID_ABI}/libftgl.a)
add_library(xml2        STATIC IMPORTED)
set_target_properties(xml2        PROPERTIES IMPORTED_LOCATION ${LIB}/libxml2/${ANDROID_ABI}/libxml2.a)
add_library(protobuf    STATIC IMPORTED)
set_target_properties(protobuf    PROPERTIES IMPORTED_LOCATION ${LIB}/protobuf/${ANDROID_ABI}/libprotobuf.a)
add_library(glu_shim    STATIC IMPORTED)
set_target_properties(glu_shim    PROPERTIES IMPORTED_LOCATION ${LIB}/glu_shim/${ANDROID_ABI}/libglu_shim.a)
add_library(png         STATIC IMPORTED)
set_target_properties(png         PROPERTIES IMPORTED_LOCATION ${LIB}/libpng/${ANDROID_ABI}/libpng.a)
add_library(z           STATIC IMPORTED)
set_target_properties(z           PROPERTIES IMPORTED_LOCATION ${LIB}/zlib/${ANDROID_ABI}/libz.a)

# --- Armagetron engine sources (mirrors src/Makefile.am lib*_a_SOURCES) -------
# Glob per module; non-Android sources are excluded below.
file(GLOB_RECURSE ARMA_SRC
    ${ARMA}/tools/*.cpp
    ${ARMA}/resource/*.cpp
    ${ARMA}/network/*.cpp
    ${ARMA}/engine/*.cpp
    ${ARMA}/render/*.cpp
    ${ARMA}/tron/*.cpp
    ${ARMA}/ui/*.cpp
    ${ARMA}/thirdparty/mathexpr/*.cpp
    ${ARMA}/thirdparty/particles/*.cpp
    ${ARMA}/thirdparty/scrap/*.cpp
)
set(THIRDPARTY_C ${ARMA}/thirdparty/binreloc/prefix.c)

# exclude non-client / non-Android / dedicated-only translation units
# Exclude non-library translation units (test harnesses/demos with their own main(),
# OSX/win32 platform files, and EXTRA_DIST leftovers) — verified against src/Makefile.am.
# gWinZone = zones-v1 (BUILDZONESV1, default OFF); we build zones-v2 (zone/*.cpp + zFortress).
list(FILTER ARMA_SRC EXCLUDE REGEX "rConsoleCout|gOSXURLHandler|uOSXPaste|/win32|/dummy/|/test/|/test\\.cpp|/unitTest/|example|mathexpr_c|transfab|gWinZone|master\\.cpp|serverquery|l2_demo|l3_demo|nettest|astat|memtest|testgl|/eSound\\.cpp|tSafePTRBase")

# protobuf-generated sources (25 .pb.cc produced by host protoc)
file(GLOB PB_SRC ${GEN}/*.pb.cc)

add_library(main SHARED
    ${ARMA_SRC}
    ${THIRDPARTY_C}
    ${PB_SRC}
    ${CMAKE_CURRENT_SOURCE_DIR}/glu_shim/glu_shim.c   # built in-tree too
    ${CMAKE_CURRENT_SOURCE_DIR}/android_init.c        # gl4es env (constructor 100)
)

# --- include paths (from the per-lib _CXXFLAGS -iquote lists) -----------------
target_include_directories(main PRIVATE
    ${ARMA}
    ${ARMA}/tools ${ARMA}/tools/values ${ARMA}/resource
    ${ARMA}/thirdparty/mathexpr ${ARMA}/thirdparty/utf8
    ${ARMA}/thirdparty/particles ${ARMA}/thirdparty/scrap ${ARMA}/thirdparty/shttpd
    ${ARMA}/network ${ARMA}/render ${ARMA}/ui
    ${ARMA}/engine ${ARMA}/engine/sound ${ARMA}/engine/sound/sdl_mixer
    ${ARMA}/tron ${ARMA}/tron/cockpit ${ARMA}/tron/zone
    ${ARMA}/protobuf ${GEN}
    ${INC}                       # SDL2, GL (gl4es), freetype, FTGL, libxml2, google/protobuf
    ${INC}/SDL_mixer
    ${BOOST_INC}
    ${CMAKE_CURRENT_SOURCE_DIR}  # config.h (hand-authored for the OUYA build)
)

# config.h is provided in-tree (cpp/config.h); the autotools HAVE_* macros.
target_compile_definitions(main PRIVATE HAVE_CONFIG_H)

# --- link --------------------------------------------------------------------
find_library(log-lib log)
find_library(android-lib android)
target_link_libraries(main
    ftgl freetype            # FTGL before freetype (FTGL needs freetype symbols)
    glu_shim gl4es           # GLU shim resolves gluX into gl4es' GL
    xml2 protobuf
    png z
    sdl2_image sdl2_mixer sdl2 hidapi
    EGL GLESv2
    ${log-lib} ${android-lib} m dl
)
