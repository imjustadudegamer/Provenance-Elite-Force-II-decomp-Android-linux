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
 * ProvenanceEF2 engine activity (the EF1 port's EFActivity, adapted to EF2's data
 * layout: the engine's game directory is "base", not "baseEF").
 *
 * Before the SDL/native engine starts, extract the port-supplied files bundled
 * in the APK -- the on-screen touch art (zpak-android.pk3 + gfx/touch/*) -- into
 * <obb>/base/, the directory the engine uses as fs_basepath (ANDROID_DATA_ROOT,
 * app/src/main/cpp/cmake/platforms/android.cmake,
 * /storage/emulated/0/Android/obb/com.provenanceef2).  The engine's WRITE root
 * (fs_homepath and the other fs_home* paths) is deliberately NOT this directory:
 * it is <obb>/provenanceef2, created by the engine, because the directories under
 * <obb>/base are owned by whoever pushed the retail tree and a new subdirectory
 * there cannot be created by the app (the 0.2.16 New Game crash).
 *
 * Extraction runs once per APK version: a marker file base/.assets_version
 * holding the versionCode is written after a successful extraction and checked
 * on later launches.  All files are written via a .tmp + rename so a crash
 * mid-write never leaves a truncated file in place.
 *
 * A one-time MIGRATION removes the stale config files older builds left in
 * <obb>/base (effects-effects; see migrateStaleConfigs).  The engine writes exactly
 * ONE user-state file now -- <obb>/provenanceef2/base/config.cfg -- and nothing is
 * exec'd from <obb>/base any more, so those leftovers are dead weight.  There is
 * no autoexec asset: a operator's own overrides belong in the one unified config.cfg.
 *
 * The retail paks are NOT bundled (not redistributable) -- ImportActivity (the
 * launcher) walks the operator through importing them.
 *
 * The native side is libmain.so (the whole engine) plus libgame.so /
 * libcgame.so / librenderer_gles3.so / libopenal.so / libSDL2.so, all packaged
 * by the same gradle build from app/src/main/cpp's own CMakeLists.txt.  SDLActivity's
 * default getLibraries() {"SDL2","main"} + getMainSharedObject() resolves both to
 * this app's nativeLibraryDir, and SDL_main() is the engine's main().
 */
public class EF2Activity extends SDLActivity {
    private static final String TAG = "ProvenanceEF2";
    private static final String VERSION_MARKER = ".assets_version";
    /** The engine's game directory (q_shared.h BASEGAME). */
    private static final String GAME_DIR = "base";

    /**
     * Stale files older builds wrote into <obb>/base (effects-effects migration).
     *
     * <p>History: an older build's home path WAS <obb>/base, so it wrote its
     * user config there as {@code config.cfg} / the OS-user config {@code legacy.cfg};
     * a still older one exec'd the APK asset {@code android_defaults.cfg}; and a
     * build before 0.2.24 extracted an empty {@code autoexec.cfg} hook.  The
     * engine now writes exactly one file, {@code <obb>/provenanceef2/base/config.cfg},
     * and execs none of these, so they are dead.  Only these exact names are
     * touched -- never the live unified config, never retail data in the paks.
     */
    private static final String[] STALE_BASE_FILES = {
            "config.cfg",            // 6,271 B: an older build's home==base config
            "legacy.cfg",              // 3,782 B: the pre-unification OS-user config
            "android_defaults.cfg",  // the retired defaults asset (now in the binary)
            "autoexec.cfg",          // the retired empty autoexec hook (effects removed)
            // retailcfg: the loose RETAIL cfgs.  The operator's rule is ONE loose cfg, so the
            // port supplies all three from its own pak (global/default.cfg is the boot
            // baseline retail's FS_InitFilesystem requires, and defaultcontrols.cfg /
            // defaultoptions.cfg are what the menus' "Restore Controls" and "Restore
            // Defaults" buttons exec).  Deleting the loose copies therefore loses
            // nothing; the pak's ones take over.
            "global/default.cfg",
            "global/defaultcontrols.cfg",
            "global/defaultoptions.cfg",
    };

    /** Non-null when asset extraction failed; blocks engine startup. */
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
                    // THERE.  The human installs fresh builds and pushes paks by hand, so a first
                    // boot that aborts because one bundled file could not be (re)written -- while the
                    // working copy from the previous install is still on disk -- is a defect, not a
                    // safety check.  Only a MISSING essential asset is fatal, and then the message
                    // names the file.  The essential bundled asset is the port's own pak: it carries
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
                // The stale-config migration is independent of extraction and stays best-effort.
                migrateStaleConfigs(baseDir);
            }
        } catch (Exception e) {
            Log.e(TAG, "OBB setup failed", e);
            mFatalError = "Preparing the game data directory failed: " + e;
        }

        // Note: super.onCreate() must always run (Activity contract). When
        // mFatalError is set, our loadLibraries() override below throws, so
        // SDLActivity takes its "broken libraries" path: it shows a blocking
        // error dialog containing the message and never starts the engine.
        super.onCreate(savedInstanceState);

        // Go immersive-fullscreen NOW, before the first layout pass.
        //
        // SDLActivity.onCreate() calls setWindowStyle(false) (system bars
        // visible) and only switches to immersive later, asynchronously, when
        // the native engine creates its fullscreen window.  By then GLimp_Init
        // has already measured the drawable size, so glConfig gets the
        // bar-inset size (e.g. 1794x1017) while the GLES surface ends up
        // full-screen (1920x1080) -- the frame does not fill the screen.
        // Applying the flags here (and setting SDLActivity.mFullscreenModeActive
        // so SDLActivity's onSystemUiVisibilityChange handler keeps re-asserting
        // them) makes the surface full-size before the engine ever measures it.
        // (EF1's EFActivity does exactly this for the same reason.)
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

    /** Same flag set SDLActivity uses for COMMAND_CHANGE_WINDOW_STYLE(1). */
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
        // SDLActivity re-asserts immersive on visibility changes when this
        // flag is set, so the bars stay hidden across focus loss/regain.
        SDLActivity.mFullscreenModeActive = true;
    }

    @Override
    public void loadLibraries() {
        if (mFatalError != null) {
            // Aborts SDLActivity.onCreate() before any engine setup.
            throw new RuntimeException(mFatalError);
        }
        super.loadLibraries();
    }

    /** Current APK versionCode (used for the extraction marker). */
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
            // andport-androidport: the marker is an OPTIMISATION (skip re-extraction next launch), not
            // a game asset.  A read-only or full filesystem can fail this write after every real
            // asset has been copied; that must not block startup, it only costs one re-extract.
            try {
                writeSmallFile(marker, String.valueOf(versionCode));
            } catch (IOException e) {
                Log.w(TAG, "could not write the extraction marker "
                        + marker + " (assets are extracted; the next launch will re-extract)", e);
            }
        }
    }

    /**
     * The bundled asset the engine cannot start without, or {@code null} when it is present.
     *
     * <p>andport-androidport: {@code zpak-android.pk3} is the port's own pak. It carries
     * {@code global/default.cfg}, which retail's {@code FS_InitFilesystem} hard-requires (a missing
     * one is a {@code Com_Error: FATAL: Couldn't load default.cfg} at boot), plus the on-screen
     * touch art. Everything else the activity extracts is replaceable, so only this file gates
     * startup. Called after an extraction failure so the app can continue on an existing install
     * instead of aborting on an optional write.
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
     * One-time cleanup of the stale config files older builds left in
     * {@code <obb>/base} (effects-effects; the operator's "delete the old cfgs" order).
     *
     * <p>The engine writes exactly ONE user-state file today,
     * {@code <obb>/provenanceef2/base/config.cfg}, and it lives OUTSIDE this directory,
     * so every name in {@link #STALE_BASE_FILES} is dead.  Strictly additive and
     * harmless when a file is absent; it never recurses and never touches
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
        // Write to .tmp and rename so a crash never leaves a truncated file.
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
