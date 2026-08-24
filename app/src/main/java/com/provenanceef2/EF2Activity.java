package com.provenanceef2;

import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * <obb>/base are owned by whoever pushed the retail tree and a new subdirectory
 * The retail paks are NOT bundled (not redistributable) -- ImportActivity (the
 */
public class EF2Activity extends SDLActivity {
    private static final String TAG = "ProvenanceEF2";
    private static final String VERSION_MARKER = ".assets_version";
    private static final String GAME_DIR = "base";

    /**
     * touched -- never the live unified config, never retail data in the paks.
     */
    private static final String[] STALE_BASE_FILES = {
            "config.cfg",            // 6,271 B: an older build's home==base config
            "android_defaults.cfg",  // the retired defaults asset (now in the binary)
            "autoexec.cfg",          // the retired empty autoexec hook (effects removed)
            // baseline retail's FS_InitFilesystem requires, and defaultcontrols.cfg /
            "global/default.cfg",
            "global/defaultcontrols.cfg",
            "global/defaultoptions.cfg",
    };

    private String mFatalError;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File baseDir = null;
        try {
            File obbRoot = getObbDir();   // Android/obb/com.provenanceef2
            if (obbRoot == null) {
                mFatalError = "Cannot access the app's OBB storage directory "
                        + "(shared storage unavailable).";
            } else {
                obbRoot.mkdirs();
                baseDir = new File(obbRoot, GAME_DIR);
                try {
                    extractAssetsIfNeeded(baseDir);
                } catch (Exception e) {
                    // andport-androidport: EXTRACTION FAILURE MUST DEGRADE WHEN THE ASSETS ARE ALREADY
                    // global/default.cfg, which retail's FS_InitFilesystem hard-requires.
                    Log.w(TAG, "asset extraction failed; checking for an existing install", e);
                    String missing = missingBundledAsset(baseDir);
                    if (missing == null) {
                        Log.w(TAG, "continuing with the bundled assets already present in " + baseDir);
                    } else {
                        mFatalError = "Extracting bundled game files failed (" + e
                                + ") and " + missing + " is missing from " + baseDir + ".";
                    }
                }
                migrateStaleConfigs(baseDir);
            }
        } catch (Exception e) {
            Log.e(TAG, "OBB setup failed", e);
            mFatalError = "Preparing the game data directory failed: " + e;
        }

        super.onCreate(savedInstanceState);

        // bar-inset size (e.g. 1794x1017) while the GLES surface ends up
        // full-screen (1920x1080) -- the frame does not fill the screen.
        applyImmersiveMode();

        if (mFatalError != null) {
            AlertDialog.Builder b = new AlertDialog.Builder(this);
            b.setTitle("ProvenanceEF2 error");
            b.setMessage(mFatalError + "\n\nThe game cannot start.");
            b.setCancelable(false);
            b.setPositiveButton("Quit", new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    finish();
                }
            });
            b.show();
        }
    }

    private void applyImmersiveMode() {
        int flags = View.SYSTEM_UI_FLAG_FULLSCREEN |
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
        getWindow().getDecorView().setSystemUiVisibility(flags);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        SDLActivity.mFullscreenModeActive = true;
    }

    @Override
    public void loadLibraries() {
        if (mFatalError != null) {
            throw new RuntimeException(mFatalError);
        }
        super.loadLibraries();
    }

    private int apkVersionCode() {
        try {
            return getPackageManager()
                    .getPackageInfo(getPackageName(), 0).versionCode;
        } catch (Exception e) {
            Log.w(TAG, "could not read versionCode", e);
            return -1;
        }
    }

    private void extractAssetsIfNeeded(File baseDir) throws IOException {
        int versionCode = apkVersionCode();
        File marker = new File(baseDir, VERSION_MARKER);

        if (versionCode > 0 && marker.isFile()
                && String.valueOf(versionCode).equals(readSmallFile(marker))) {
            Log.i(TAG, "assets already extracted for versionCode " + versionCode);
            return;
        }

        copyAssetDir(getAssets(), GAME_DIR, baseDir);

        if (versionCode > 0) {
            try {
                writeSmallFile(marker, String.valueOf(versionCode));
            } catch (IOException e) {
                Log.w(TAG, "could not write the extraction marker "
                        + marker + " (assets are extracted; the next launch will re-extract)", e);
            }
        }
    }

    /**
     * {@code global/default.cfg}, which retail's {@code FS_InitFilesystem} hard-requires (a missing
     * one is a {@code Com_Error: FATAL: Couldn't load default.cfg} at boot), plus the on-screen
     */
    private static String missingBundledAsset(File baseDir) {
        File pak = new File(baseDir, "zpak-android.pk3");
        if (!pak.isFile() || pak.length() == 0) {
            return "zpak-android.pk3";
        }
        return null;
    }

    private static String readSmallFile(File f) {
        try (InputStream in = new FileInputStream(f)) {
            byte[] buf = new byte[64];
            int n = in.read(buf);
            return n > 0 ? new String(buf, 0, n, "UTF-8").trim() : "";
        } catch (IOException e) {
            return "";
        }
    }

    private static void writeSmallFile(File f, String content) throws IOException {
        try (OutputStream out = new FileOutputStream(f)) {
            out.write(content.getBytes("UTF-8"));
        }
    }

    private void copyAssetDir(AssetManager am, String assetPath, File dest) throws IOException {
        String[] children = am.list(assetPath);
        if (children == null || children.length == 0) {
            copyAssetFile(am, assetPath, dest);   // leaf = file
            return;
        }
        dest.mkdirs();
        for (String child : children) {
            copyAssetDir(am, assetPath + "/" + child, new File(dest, child));
        }
    }

    /**
     * {@code global/*.cfg} (retail data inside the paks).
     */
    private void migrateStaleConfigs(File baseDir) {
        for (String name : STALE_BASE_FILES) {
            File stale = new File(baseDir, name);
            if (!stale.exists()) {
                continue;   // already gone: the common case after the first run
            }
            if (stale.isDirectory()) {
                Log.w(TAG, "stale config " + stale + " is a directory; leaving it alone");
                continue;
            }
            if (stale.delete()) {
                Log.i(TAG, "migration: removed stale config " + stale);
            } else {
                Log.w(TAG, "migration: could not remove stale config " + stale);
            }
        }
    }

    private void copyAssetFile(AssetManager am, String assetPath, File dest) throws IOException {
        File tmp = new File(dest.getParentFile(), dest.getName() + ".tmp");
        boolean ok = false;
        try (InputStream in = am.open(assetPath);
             OutputStream out = new FileOutputStream(tmp)) {
            byte[] buf = new byte[65536];
            int n;
            while ((n = in.read(buf)) != -1) {
                out.write(buf, 0, n);
            }
            ok = true;
        } finally {
            if (!ok) {
                tmp.delete();
            }
        }
        if (dest.exists() && !dest.delete()) {
            tmp.delete();
            throw new IOException("could not replace " + dest);
        }
        if (!tmp.renameTo(dest)) {
            tmp.delete();
            throw new IOException("could not rename " + tmp + " -> " + dest);
        }
        Log.i(TAG, "extracted " + assetPath + " -> " + dest);
    }
}
