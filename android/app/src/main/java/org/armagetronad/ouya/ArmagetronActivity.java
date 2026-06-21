package org.armagetronad.ouya;

import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;

import org.libsdl.app.SDLActivity;

/**
 * Armagetron Advanced OUYA entry point.
 *
 * The native libraries must be loaded explicitly, in dependency order: the
 * API-16 dynamic linker does NOT resolve transitive DT_NEEDED from the app lib
 * dir, so each .so is named here with `main` last (SDL uses it as the main
 * object). Static libs (gl4es, freetype, ftgl, libxml2, protobuf, glu_shim)
 * are linked into libmain.so and need no entry.
 */
public class ArmagetronActivity extends SDLActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Unpack bundled game data to the external files dir BEFORE the native
        // SDL thread starts (super.onCreate launches it). main() then points
        // tDirectories at SDL_AndroidGetExternalStoragePath() = the same dir.
        int versionCode = 1;
        try {
            versionCode = getPackageManager()
                .getPackageInfo(getPackageName(), 0).versionCode;
        } catch (PackageManager.NameNotFoundException e) {
            Log.w("ArmagetronActivity", "versionCode lookup failed", e);
        }
        AssetExporter.exportIfNeeded(this, versionCode);
        super.onCreate(savedInstanceState);
    }

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "c++_shared",
            "hidapi",
            "SDL2",
            "SDL2_image",
            "SDL2_mixer",
            "main"          // must be last
        };
    }
}
