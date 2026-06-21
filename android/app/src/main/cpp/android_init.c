/*
 * OUYA / Android early init for the Armagetron Advanced port.
 *
 * gl4es reads its LIBGL_* configuration in a constructor(101) that runs at
 * libmain.so load (before main()). On the OUYA's Tegra 3, gl4es' hardware-probe
 * shaders make the ancient Cg compiler (libcgdrv.so) SIGSEGV, and its EGL pbuffer
 * probe is a second EGL_BAD_DISPLAY source. Setting LIBGL_NOTEST=1 skips both.
 * These env vars must be set BEFORE gl4es' constructor, so this runs at
 * constructor priority 100 (lower = earlier).
 */
#include <stdlib.h>

/* The OUYA app's writable external files dir. tDirectories computes its XDG
 * write paths (config/cache/var) at C++ static-init time, BEFORE main(), so the
 * HOME/XDG_* env must be set here (constructor priority 100 runs before the
 * unprioritized C++ global initializers). Default HOME=/data is not writable. */
#define ARMA_FILES_DIR "/sdcard/Android/data/org.armagetronad.ouya/files"

__attribute__((constructor(100)))
static void arma_gl4es_env(void)
{
    setenv("LIBGL_NOTEST",   "1", 1);  // skip Cg-crashing probe shaders + pbuffer probe
    setenv("LIBGL_ES",       "2", 1);  // force the GLES2 backend
    setenv("LIBGL_NOHIGHP",  "1", 1);  // no highp in fragment shaders (Tegra)
    setenv("LIBGL_NOPSA",    "1", 1);  // disable PSA (problematic on Tegra)
    setenv("LIBGL_NOBANNER", "1", 1);
    setenv("LIBGL_MIPMAP",   "3", 1);  // auto-generate + use mipmaps (fewer cache misses)
    setenv("LIBGL_NOERROR",  "1", 1);  // skip gl4es internal glGetError checks

    // Redirect writable config/cache/var paths to the (writable) external files dir.
    setenv("HOME",            ARMA_FILES_DIR, 1);
    setenv("XDG_CONFIG_HOME", ARMA_FILES_DIR "/.config", 1);
    setenv("XDG_CACHE_HOME",  ARMA_FILES_DIR "/.cache",  1);
    setenv("XDG_DATA_HOME",   ARMA_FILES_DIR "/.data",   1);

    // libxml2 has no system XML catalog on Android; an empty list stops it from
    // probing (and silences "File ... is not an XML Catalog" when loading cockpit XML).
    setenv("XML_CATALOG_FILES", "", 1);
}
