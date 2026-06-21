// =============================================================================
// Armagetron Advanced - hand-authored config.h for the OUYA / Android build.
// Replaces the autotools-generated config.h. Modeled on src/win32/aa_config.h +
// config_common.h but for a Linux/Android (Bionic) client using gl4es + SDL2.
// NOTE: starter set — extend HAVE_* macros as the source compile surfaces them.
// =============================================================================
#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED

// --- package identity --------------------------------------------------------
#define PACKAGE        "armagetronad"
#define PACKAGE_NAME   "Armagetron Advanced"
#define VERSION        "0.4-ouya"
#define PACKAGE_VERSION VERSION

// --- our dependency set on OUYA ----------------------------------------------
#define HAVE_LIBSDL_MIXER 1        // audio via SDL2_mixer (harvested from SRB2Kart)
#define HAVE_FTGL_FTGL_H  1        // we include <FTGL/ftgl.h>
#define HAVE_FTGL         1
#define HAVE_LIBXML2      1        // libxml2 (static, nanohttp HTTP enabled)
#define HAVE_LIBXML       1
#define LIBXML_HTTP_ENABLED 1
#define ENABLE_ZONESV2    1        // zones v2 support (as in the win32 build)

// gl4es replaces desktop GL/GLEW; do NOT define HAVE_GLEW.
// boost::thread lib is not cross-compiled -> leave HAVE_BOOST_THREAD undefined
// so the engine falls back to pthread (see configure.ac AX_BOOST_THREAD branch).

// --- POSIX / Bionic platform features (Android NDK provides these) -----------
#define HAVE_UNISTD_H     1
#define HAVE_SYS_STAT_H   1
#define HAVE_SYS_TIME_H   1
#define HAVE_FCNTL_H      1
#define HAVE_STDLIB_H     1
#define HAVE_STRING_H     1
#define HAVE_STRINGS_H    1
#define HAVE_POW          1
#define HAVE_SQRT         1
#define HAVE_GETTIMEOFDAY 1
#define HAVE_NANOSLEEP    1
#define HAVE_PTHREAD      1

// SDL2 image header location (<SDL_image.h>)
#define HAVE_SDL_IMG_H    1

// --- data directories --------------------------------------------------------
// On Android the real base dir is resolved at runtime via
// SDL_AndroidGetExternalStoragePath() (see the runtime patch in main entry);
// these are fallbacks only.
#define USER_DATA_DIR  "."
#define LEGACY_USER_DATA_DIR "."

// include the platform-independent non-autoconf common config
#include "config_ide.h"

#endif // CONFIG_H_INCLUDED
