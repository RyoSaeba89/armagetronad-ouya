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
#include <stdio.h>
#include <string.h>

/* Fallback if /proc/self/cmdline can't be read (must match applicationId in
 * build.gradle). The real value is derived at runtime below so a package
 * rename cannot silently break the writable paths again: v0.4.5 shipped with
 * the pre-rename id hardcoded here, which sent every config/cache write into
 * the data dir of a no-longer-installed package — user settings, first-setup
 * state and tutorial progress were silently lost on every restart. */
#define ARMA_FALLBACK_PKG "com.ryo.armagetronadouya"

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

    /* The app's writable external files dir. tDirectories computes its XDG
     * write paths (config/cache/var) at C++ static-init time, BEFORE main(),
     * so HOME/XDG_* must be set here (constructor priority 100 runs before
     * the unprioritized C++ global initializers). Default HOME=/data is not
     * writable. The package name == the Android process name, readable from
     * /proc/self/cmdline by the time libmain.so is loaded. */
    char pkg[128] = ARMA_FALLBACK_PKG;
    {
        FILE *f = fopen("/proc/self/cmdline", "r");
        if (f) {
            char buf[128] = {0};
            size_t n = fread(buf, 1, sizeof(buf) - 1, f);
            fclose(f);
            /* cmdline is NUL-terminated; some devices append ":remote" style
             * suffixes to secondary processes — keep the base name only. */
            if (n > 0 && buf[0] != '\0') {
                char *colon = strchr(buf, ':');
                if (colon) *colon = '\0';
                strncpy(pkg, buf, sizeof(pkg) - 1);
                pkg[sizeof(pkg) - 1] = '\0';
            }
        }
    }

    char base[256];
    snprintf(base, sizeof(base), "/sdcard/Android/data/%s/files", pkg);

    char path[300];
    setenv("HOME", base, 1);
    snprintf(path, sizeof(path), "%s/.config", base);
    setenv("XDG_CONFIG_HOME", path, 1);
    snprintf(path, sizeof(path), "%s/.cache", base);
    setenv("XDG_CACHE_HOME", path, 1);
    snprintf(path, sizeof(path), "%s/.data", base);
    setenv("XDG_DATA_HOME", path, 1);

    // libxml2 has no system XML catalog on Android; an empty list stops it from
    // probing (and silences "File ... is not an XML Catalog" when loading cockpit XML).
    setenv("XML_CATALOG_FILES", "", 1);
}
