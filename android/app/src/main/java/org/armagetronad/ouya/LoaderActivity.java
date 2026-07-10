package org.armagetronad.ouya;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.TextView;

/**
 * OUYA launcher entry point.
 *
 * The bundled game data (~tens of MB) is unpacked from the APK assets to external
 * storage on first launch, which takes ~1 minute on the OUYA's slow flash. Doing
 * that work synchronously inside SDLActivity.onCreate blocks the UI thread long
 * enough to trip Android's ANR watchdog (~5s) and the system force-finishes the
 * game. So we export on a background thread behind a tiny loading screen, and only
 * start the real (SDL) ArmagetronActivity once the data is on disk.
 *
 * This activity also carries the launcher / OUYA "GAME" categories so it is what
 * shows up (with assets/ouya_icon.png) in the OUYA games menu.
 */
public class LoaderActivity extends Activity {

    private static final String TAG = "LoaderActivity";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final TextView tv = new TextView(this);
        tv.setText("Armagetron Advanced\n\nLoading…");
        tv.setTextColor(Color.WHITE);
        tv.setTextSize(24);
        tv.setGravity(Gravity.CENTER);
        tv.setBackgroundColor(Color.BLACK);
        tv.setLayoutParams(new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        setContentView(tv);

        int versionCode = 1;
        try {
            versionCode = getPackageManager()
                .getPackageInfo(getPackageName(), 0).versionCode;
        } catch (PackageManager.NameNotFoundException e) {
            Log.w(TAG, "versionCode lookup failed", e);
        }
        final int finalVersionCode = versionCode;

        new Thread(new Runnable() {
            @Override
            public void run() {
                AssetExporter.exportIfNeeded(LoaderActivity.this, finalVersionCode,
                        new AssetExporter.ProgressListener() {
                    @Override
                    public void onProgress(final int copied, final int total) {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                int pct = total > 0 ? (copied * 100) / total : 0;
                                tv.setText("Armagetron Advanced\n\nInstalling game data… " + pct
                                        + "%\n(first launch only)");
                            }
                        });
                    }
                });
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        startActivity(new Intent(LoaderActivity.this, ArmagetronActivity.class));
                        finish();
                    }
                });
            }
        }, "asset-export").start();
    }
}
