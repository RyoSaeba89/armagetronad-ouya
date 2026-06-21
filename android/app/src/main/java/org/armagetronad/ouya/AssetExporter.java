package org.armagetronad.ouya;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Unpacks the bundled Armagetron game data (config/, language/, textures/,
 * models/, sound/, music/, resource/, scripts/) from the APK's assets into the
 * app's external files dir — the same directory the native side reads via
 * SDL_AndroidGetExternalStoragePath(). The engine uses std file I/O, so the data
 * must exist on the real filesystem.
 *
 * Runs once per versionCode (a marker file gates re-export).
 */
public class AssetExporter {
    private static final String TAG = "AssetExporter";
    private static final String[] DATA_DIRS = {
        "config", "language", "textures", "models", "sound", "music", "resource", "scripts"
    };

    // A file that MUST exist after a good export. Used to validate that the data
    // dir is really populated (not just that a marker was left behind).
    private static final String SENTINEL = "config/settings.cfg";

    public static void exportIfNeeded(Context ctx, int versionCode) {
        File base = resolveBase(ctx);
        if (base == null) {
            // Should never happen now, but never proceed to a broken launch silently.
            Log.e(TAG, "could not obtain a usable external files dir; assets NOT exported");
            return;
        }

        File marker = new File(base, "exported_version.txt");
        File sentinel = new File(base, SENTINEL);
        // Re-export unless BOTH the version marker matches AND the data is actually
        // on disk. Deleting Android/data/<pkg> wipes both, so we fall through and
        // rebuild the whole tree instead of crashing on missing config later.
        if (marker.exists()
                && String.valueOf(versionCode).equals(readFile(marker))
                && sentinel.exists()) {
            Log.d(TAG, "assets already exported for version " + versionCode);
            return;
        }

        // A stale/partial marker from an interrupted export would block a re-copy;
        // drop it so the export below is the single source of truth.
        if (marker.exists()) {
            //noinspection ResultOfMethodCallIgnored
            marker.delete();
        }

        Log.d(TAG, "exporting game assets to " + base.getAbsolutePath());
        AssetManager am = ctx.getAssets();
        try {
            for (String dir : DATA_DIRS) {
                copyAsset(am, dir, base);
            }
            if (!sentinel.exists()) {
                throw new IOException("export finished but sentinel missing: " + sentinel);
            }
            writeFile(marker, String.valueOf(versionCode));
            Log.d(TAG, "asset export complete");
        } catch (IOException e) {
            // Leave NO marker so the next launch retries from scratch.
            Log.e(TAG, "asset export failed: " + e.getMessage(), e);
        }
    }

    /**
     * Resolve the external files dir robustly. After the user manually deletes
     * Android/data/&lt;pkg&gt; (file manager / adb), some Android 4.x devices —
     * the OUYA included — transiently return null or a not-yet-recreated dir from
     * getExternalFilesDir() because the framework has cached a negative lookup.
     * Retry a few times, forcing the tree into existence and verifying it is a
     * writable directory before we hand it back.
     */
    private static File resolveBase(Context ctx) {
        for (int attempt = 0; attempt < 5; attempt++) {
            File base = ctx.getExternalFilesDir(null);
            if (base != null) {
                if (!base.exists()) {
                    //noinspection ResultOfMethodCallIgnored
                    base.mkdirs();
                }
                if (base.isDirectory() && base.canWrite()) {
                    return base;
                }
                Log.w(TAG, "external files dir not writable yet (attempt " + attempt
                        + "): " + base.getAbsolutePath());
            } else {
                Log.w(TAG, "getExternalFilesDir returned null (attempt " + attempt + ")");
            }
            try {
                Thread.sleep(120);
            } catch (InterruptedException ie) {
                Thread.currentThread().interrupt();
                break;
            }
        }
        // Last resort: internal storage always exists. The native side reads
        // SDL_AndroidGetExternalStoragePath(), so this only helps if external came
        // back at all — but returning the best non-null candidate beats giving up.
        return ctx.getExternalFilesDir(null);
    }

    private static void copyAsset(AssetManager am, String path, File outRoot) throws IOException {
        String[] children = am.list(path);
        if (children != null && children.length > 0) {
            // directory
            File dir = new File(outRoot, path);
            if (!dir.exists() && !dir.mkdirs()) {
                throw new IOException("mkdirs failed: " + dir);
            }
            for (String child : children) {
                copyAsset(am, path + "/" + child, outRoot);
            }
        } else {
            // file
            File outFile = new File(outRoot, path);
            File parent = outFile.getParentFile();
            if (parent != null && !parent.exists()) parent.mkdirs();
            byte[] buf = new byte[65536];   // 64KB: minutes -> seconds on OUYA flash
            try (InputStream in = am.open(path);
                 OutputStream out = new BufferedOutputStream(new FileOutputStream(outFile), 65536)) {
                int n;
                while ((n = in.read(buf)) != -1) out.write(buf, 0, n);
            }
        }
    }

    private static String readFile(File f) {
        try (InputStream in = new java.io.FileInputStream(f)) {
            byte[] b = new byte[64];
            int n = in.read(b);
            return n > 0 ? new String(b, 0, n).trim() : "";
        } catch (IOException e) {
            return "";
        }
    }

    private static void writeFile(File f, String s) throws IOException {
        try (OutputStream out = new FileOutputStream(f)) {
            out.write(s.getBytes());
        }
    }
}
