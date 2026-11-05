package com.provenanceef2;

import android.app.Activity;
import android.app.Dialog;
import android.content.ContentResolver;
import android.content.Intent;
import android.database.Cursor;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.View;
import android.view.Window;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * layout): makes sure the retail game data is present in the app's OBB
 * EF2's game directory is "base" (q_shared.h BASEGAME) and retail ships the
 *
 * The same screen offers the renderer: Vulkan or OpenGL ES 3.2.  The choice is
 * written as the `seta cl_renderer "<value>"` row of the engine's own
 * <obb>/base/config.cfg and nowhere else, so the in-game graphics row (which
 * writes that cvar) and this picker can never disagree.
 */
public class ImportActivity extends Activity {
    private static final String TAG = "ProvenanceEF2";
    private static final int REQUEST_PICK_PAKS = 42;

    /**
     * The two renderer modules the APK ships (lib/librenderer_*.so); the value is the
     * literal CL_InitRef formats into "renderer_%s".  The label is the row's own.
     */
    private static final String RENDERER_VULKAN = "vulkan";
    private static final String RENDERER_GLES3 = "gles3";
    private static final String[] RENDERER_VALUES = {RENDERER_VULKAN, RENDERER_GLES3};

    /** The engine's one archive: <obb>/base/config.cfg (qcommon.h EF2_CONFIG_FILE). */
    private static final String CONFIG_FILE = "config.cfg";
    private static final String RENDERER_CVAR = "cl_renderer";

    /**
     * The paks the single-player campaign needs.  The multiplayer map paks
     * (ctf_tartirus, ctf_vor2, dm_ctf_voy1, hm_ctf_exigent) and the 1.1 patch
     * paks some installs carry (pak50, locpak50) are OPTIONAL: several retail
     * distributions do not ship them at all, the campaign references no file
     * that lives only in them, and the engine tolerates a missing pak with a
     * per-file warning at use time.  They are copied when picked, never demanded.
     */
    private static final String[] REQUIRED_PAKS = {
            "pak0.pk3", "pak10.pk3", "pak15.pk3", "pak16.pk3",
            "pak20.pk3", "pak30.pk3", "pak31.pk3", "pak32.pk3",
            "locpak1.pk3"
    };
    /** Multiplayer map paks and patch paks: optional, copied when the player picks them. */
    private static final String[] OPTIONAL_PAKS = {
            "pak50.pk3", "locpak50.pk3",
            "ctf_tartirus.pk3", "ctf_vor2.pk3", "dm_ctf_voy1.pk3", "hm_ctf_exigent.pk3"
    };
    /** A genuine retail DLL must accompany the paks as proof of ownership. */
    private static final String REQUIRED_DLL = "gamex86.dll";

    private File mBaseDir;

    /** The picker's current value; null until the import screen has been shown. */
    private String mRendererChoice;
    private Dialog mImportDialog;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        File obbRoot = getObbDir();   // Android/obb/com.provenanceef2
        if (obbRoot == null) {
            showFatal("Cannot access the app's OBB storage directory "
                    + "(shared storage unavailable). The game cannot run.");
            return;
        }
        mBaseDir = new File(obbRoot, "base");
        mBaseDir.mkdirs();

        if (allRequiredPresent()) {
            launchGame();
        } else {
            showImportDialog(null);
        }
    }

    private boolean allRequiredPresent() {
        return allPaksPresent() && dllPresent();
    }

    private boolean allPaksPresent() {
        for (String pak : REQUIRED_PAKS) {
            File f = new File(mBaseDir, pak);
            if (!f.isFile() || f.length() == 0) {
                return false;
            }
        }
        return true;
    }

    /**
     * Verify a genuine retail DLL sits alongside the paks. Lenient on version:
     */
    private boolean dllPresent() {
        File f = new File(mBaseDir, REQUIRED_DLL);
        if (!f.isFile() || f.length() < 4096) {
            return false;
        }
        InputStream in = null;
        try {
            in = new FileInputStream(f);
            int m = in.read();
            int z = in.read();
            return m == 'M' && z == 'Z';
        } catch (Exception e) {
            return false;
        } finally {
            if (in != null) try { in.close(); } catch (Exception ignored) {}
        }
    }

    private String missingList() {
        StringBuilder sb = new StringBuilder();
        for (String pak : REQUIRED_PAKS) {
            File f = new File(mBaseDir, pak);
            if (!f.isFile() || f.length() == 0) {
                if (sb.length() > 0) sb.append(", ");
                sb.append(pak);
            }
        }
        if (!dllPresent()) {
            if (sb.length() > 0) sb.append(", ");
            sb.append(REQUIRED_DLL);
        }
        return sb.toString();
    }

    /** The optional paks (multiplayer maps, patch content) that are not in place yet. */
    private String optionalMissingList() {
        StringBuilder sb = new StringBuilder();
        for (String pak : OPTIONAL_PAKS) {
            File f = new File(mBaseDir, pak);
            if (!f.isFile() || f.length() == 0) {
                if (sb.length() > 0) sb.append(", ");
                sb.append(pak);
            }
        }
        return sb.toString();
    }

    private void launchGame() {
        startActivity(new Intent(this, EF2Activity.class));
        finish();
    }

    /* ---- the renderer row -------------------------------------------------------
     *
     * <obb>/base/config.cfg is the engine's one archive and the in-game graphics row
     * writes the same cvar, so this is the single store: the picker reads the row to
     * pre-select and rewrites the row to set it, and every other byte of the file is
     * copied through untouched.  The engine execs the file in Com_Init before
     * CL_InitRef reads cl_renderer and dlopens renderer_<value>, so a row written
     * here is what the next process start loads.
     */

    /** The row as the engine would write it; `seta` is what Cvar_WriteVariables emits. */
    private static String rendererRow(String value) {
        return "seta " + RENDERER_CVAR + " \"" + value + "\"";
    }

    /**
     * The renderer the archive selects.  A config with no row is not an error: the
     * engine's registration default (Cvar_Get("cl_renderer", EF2_DEFAULT_RENDERER))
     * seeds the compiled value, which is vulkan.
     */
    private String readRendererChoice() {
        File config = new File(mBaseDir, CONFIG_FILE);
        if (!config.isFile()) {
            return RENDERER_VULKAN;
        }

        String value = null;
        try {
            for (String line : configLines(new String(readAll(config), StandardCharsets.ISO_8859_1))) {
                String row = rendererRowValue(line);
                if (row != null) {
                    value = row;   // the last row wins, exactly as the config exec runs
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "could not read " + config, e);
        }

        return value != null ? value : RENDERER_VULKAN;
    }

    /**
     * Rewrite (or create) the cl_renderer row, atomically: a temp file in the same
     * directory, fsynced and renamed over config.cfg.  Every line that is not that
     * row keeps its exact bytes; the row itself becomes the engine's own form, so a
     * hand-edited `set cl_renderer gles3` is normalised rather than duplicated.
     */
    private void writeRendererChoice(String value) throws IOException {
        File config = new File(mBaseDir, CONFIG_FILE);
        byte[] before = config.isFile() ? readAll(config) : new byte[0];
        String text = new String(before, StandardCharsets.ISO_8859_1);

        StringBuilder out = new StringBuilder(text.length() + 64);
        boolean found = false;
        for (String line : configLines(text)) {
            String eol = lineTerminator(line);
            String body = line.substring(0, line.length() - eol.length());
            if (rendererRowValue(body) != null) {
                out.append(rendererRow(value)).append(eol);
                found = true;
            } else {
                out.append(line);
            }
        }
        if (!found) {
            if (out.length() > 0 && out.charAt(out.length() - 1) != '\n') {
                out.append('\n');
            }
            out.append(rendererRow(value)).append('\n');
        }

        File tmp = new File(mBaseDir, CONFIG_FILE + ".tmp");
        FileOutputStream fos = new FileOutputStream(tmp);
        try {
            fos.write(out.toString().getBytes(StandardCharsets.ISO_8859_1));
            fos.flush();
            fos.getFD().sync();
        } finally {
            fos.close();
        }
        if (!tmp.renameTo(config)) {
            tmp.delete();
            throw new IOException("could not replace " + config);
        }
        if (LogGate.on()) {
            Log.i(TAG, "renderer row -> " + value + " in " + config);
        }
    }

    /** Every line, terminator included, so a rewrite cannot change any other byte. */
    private static List<String> configLines(String text) {
        List<String> lines = new ArrayList<>();
        int start = 0;
        for (int i = 0; i < text.length(); i++) {
            if (text.charAt(i) == '\n') {
                lines.add(text.substring(start, i + 1));
                start = i + 1;
            }
        }
        if (start < text.length()) {
            lines.add(text.substring(start));
        }
        return lines;
    }

    private static String lineTerminator(String line) {
        if (line.endsWith("\r\n")) {
            return "\r\n";
        }
        return line.endsWith("\n") ? "\n" : "";
    }

    /**
     * The value of a `set* cl_renderer <value>` line, or null when the line is not that
     * row.  Only the first three tokens are read; the quote handling matches the engine
     * command parser closely enough for the rows it writes.
     */
    private static String rendererRowValue(String line) {
        List<String> tok = configTokens(line);
        if (tok.size() < 3 || !tok.get(1).equalsIgnoreCase(RENDERER_CVAR)) {
            return null;
        }
        if (!tok.get(0).toLowerCase(Locale.US).startsWith("set")) {
            return null;
        }
        return tok.get(2);
    }

    private static List<String> configTokens(String line) {
        List<String> tok = new ArrayList<>();
        int i = 0;
        int n = line.length();
        while (i < n && tok.size() < 3) {
            char c = line.charAt(i);
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ';') {
                i++;
                continue;
            }
            if (c == '/' && i + 1 < n && line.charAt(i + 1) == '/') {
                break;
            }
            StringBuilder sb = new StringBuilder();
            if (c == '"') {
                i++;
                while (i < n && line.charAt(i) != '"') {
                    sb.append(line.charAt(i++));
                }
                if (i < n) {
                    i++;
                }
            } else {
                while (i < n) {
                    char d = line.charAt(i);
                    if (d == ' ' || d == '\t' || d == '\r' || d == '\n' || d == ';'
                            || d == '"') {
                        break;
                    }
                    sb.append(d);
                    i++;
                }
            }
            tok.add(sb.toString());
        }
        return tok;
    }

    private static int rendererIndex(String value) {
        for (int i = 0; i < RENDERER_VALUES.length; i++) {
            if (RENDERER_VALUES[i].equalsIgnoreCase(value)) {
                return i;
            }
        }
        return 0;   // an archived name this build does not ship: show the default
    }

    private static byte[] readAll(File f) throws IOException {
        FileInputStream in = new FileInputStream(f);
        try {
            ByteArrayOutputStream buf = new ByteArrayOutputStream((int) Math.max(1, f.length()));
            byte[] chunk = new byte[64 * 1024];
            int n;
            while ((n = in.read(chunk)) != -1) {
                buf.write(chunk, 0, n);
            }
            return buf.toByteArray();
        } finally {
            in.close();
        }
    }

    private void chooseRenderer(String value) {
        mRendererChoice = value;
        // Written now, not at launch: the picker dialog can be re-shown or the activity
        // recreated before the import finishes, and the row is the only store.
        try {
            writeRendererChoice(value);
        } catch (Exception e) {
            Log.e(TAG, "could not write the renderer row", e);
            showImportDialog("Could not save the renderer choice (" + e + ").");
        }
    }

    /** The picker screen: our own panel, every colour literal (no theme). */
    private void showImportDialog(String extraLine) {
        if (mImportDialog != null) {
            mImportDialog.dismiss();
            mImportDialog = null;
        }

        if (mRendererChoice == null) {
            mRendererChoice = readRendererChoice();
        }

        View panel = getLayoutInflater().inflate(R.layout.import_dialog, null);
        String optional = optionalMissingList();
        String optionalLine = optional.isEmpty() ? ""
                : "\nThe multiplayer map paks and patch paks below are OPTIONAL -- "
                + "the campaign does not need them. Copy them too only if you want "
                + "the multiplayer maps:\n\n    " + optional + "\n\n";
        String msg = (extraLine != null ? extraLine + "\n\n" : "")
                + "Star Trek: Elite Force II needs the game data from your "
                + "retail copy of the game.\n\n"
                + "Copy these ten files out of the retail \"base\" folder to "
                + "somewhere this picker can reach (e.g. Downloads) -- together "
                + "they are about 1 GB -- then tap \"Select game files\" "
                + "and pick all of them.\n\n"
                + "    pak0.pk3  pak10.pk3  pak15.pk3  pak16.pk3\n"
                + "    pak20.pk3  pak30.pk3  pak31.pk3  pak32.pk3\n"
                + "    locpak1.pk3  gamex86.dll"
                + optionalLine;
        ((TextView) panel.findViewById(R.id.import_body)).setText(msg);

        RadioButton vulkan = panel.findViewById(R.id.import_renderer_vulkan);
        RadioButton gles3 = panel.findViewById(R.id.import_renderer_gles3);
        (rendererIndex(mRendererChoice) == 1 ? gles3 : vulkan).setChecked(true);
        RadioGroup group = panel.findViewById(R.id.import_renderer_group);
        group.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup g, int id) {
                chooseRenderer(id == R.id.import_renderer_gles3
                        ? RENDERER_GLES3 : RENDERER_VULKAN);
            }
        });
        panel.findViewById(R.id.import_select).setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        openPicker();
                    }
                });
        panel.findViewById(R.id.import_quit).setOnClickListener(
                new View.OnClickListener() {
                    @Override
                    public void onClick(View v) {
                        finish();
                    }
                });

        Dialog d = new Dialog(this);
        d.requestWindowFeature(Window.FEATURE_NO_TITLE);
        d.setContentView(panel);
        d.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        d.setCancelable(false);
        mImportDialog = d;
        d.show();
    }

    private void showFatal(String msg) {
        UiPanel.error(this, "Error", msg);
    }

    private void openPicker() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        try {
            startActivityForResult(intent, REQUEST_PICK_PAKS);
        } catch (Exception e) {
            showFatal("No document picker available on this device: " + e);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST_PICK_PAKS) {
            return;
        }
        if (resultCode != RESULT_OK || data == null) {
            showImportDialog("No files were selected.");
            return;
        }

        final Uri[] uris;
        if (data.getClipData() != null) {
            int count = data.getClipData().getItemCount();
            uris = new Uri[count];
            for (int i = 0; i < count; i++) {
                uris[i] = data.getClipData().getItemAt(i).getUri();
            }
        } else if (data.getData() != null) {
            uris = new Uri[]{data.getData()};
        } else {
            showImportDialog("No files were selected.");
            return;
        }

        copyPickedFiles(uris);
    }

    private void copyPickedFiles(final Uri[] uris) {
        View panel = getLayoutInflater().inflate(R.layout.import_progress, null);
        final TextView progressText = panel.findViewById(R.id.import_progress_message);
        progressText.setText("Starting…");
        final Dialog progress = new Dialog(this);
        progress.requestWindowFeature(Window.FEATURE_NO_TITLE);
        progress.setContentView(panel);
        progress.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        progress.setCancelable(false);
        progress.show();

        new Thread(new Runnable() {
            @Override
            public void run() {
                String error = null;
                int copied = 0;
                for (Uri uri : uris) {
                    String name = displayName(uri);
                    if (name == null) {
                        Log.w(TAG, "skipping document with unknown name: " + uri);
                        continue;
                    }
                    setProgressText(progress, progressText, "Copying " + name
                            + " (" + (copied + 1) + "/" + uris.length + ")…\n"
                            + "The two largest are ~185 MB each; this can take a while.");
                    try {
                        copyDocument(uri, new File(mBaseDir, name), progress, progressText, name);
                        copied++;
                    } catch (Exception e) {
                        Log.e(TAG, "failed to import " + name, e);
                        error = "Failed to copy " + name + ": " + e;
                        break;
                    }
                }

                final String fError = error;
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        try {
                            progress.dismiss();
                        } catch (Exception ignored) {
                        }
                        if (isFinishing()) {
                            return;
                        }
                        if (fError != null) {
                            showImportDialog(fError);
                        } else if (allRequiredPresent()) {
                            launchGame();
                        } else {
                            /* the body no longer lists what is absent: the only place that
                               names it after a clean copy run is this retry line */
                            showImportDialog("Still missing: " + missingList() + ".");
                        }
                    }
                });
            }
        }, "ef2-pak-import").start();
    }

    private void setProgressText(final Dialog d, final TextView tv, final String text) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (d.isShowing()) {
                    tv.setText(text);
                    // a dialog window keeps its show-time frame; re-wrap after each text change
                    d.getWindow().setLayout(
                            android.view.ViewGroup.LayoutParams.WRAP_CONTENT,
                            android.view.ViewGroup.LayoutParams.WRAP_CONTENT);
                }
            }
        });
    }

    private String displayName(Uri uri) {
        Cursor c = null;
        try {
            c = getContentResolver().query(uri, null, null, null, null);
            if (c != null && c.moveToFirst()) {
                int idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (idx >= 0) {
                    String name = c.getString(idx);
                    if (name != null) {
                        return new File(name).getName();
                    }
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "could not query display name for " + uri, e);
        } finally {
            if (c != null) c.close();
        }
        return null;
    }

    private void copyDocument(Uri uri, File dest, Dialog progress, TextView progressText,
                              String name) throws Exception {
        ContentResolver resolver = getContentResolver();
        File tmp = new File(dest.getParentFile(), dest.getName() + ".tmp");
        InputStream in = null;
        OutputStream out = null;
        boolean ok = false;
        try {
            in = resolver.openInputStream(uri);
            if (in == null) {
                throw new java.io.IOException("could not open " + uri);
            }
            out = new FileOutputStream(tmp);
            byte[] buf = new byte[1024 * 1024];   // 1 MB chunks (pak32 ~185 MB)
            long total = 0;
            long lastReported = 0;
            int n;
            while ((n = in.read(buf)) != -1) {
                out.write(buf, 0, n);
                total += n;
                if (total - lastReported >= 32L * 1024 * 1024) {
                    lastReported = total;
                    setProgressText(progress, progressText, "Copying " + name + "…\n"
                            + (total / (1024 * 1024)) + " MB written");
                }
            }
            out.flush();
            ok = true;
        } finally {
            if (in != null) try { in.close(); } catch (Exception ignored) {}
            if (out != null) try { out.close(); } catch (Exception ignored) {}
            if (!ok) {
                tmp.delete();
            }
        }
        if (dest.exists() && !dest.delete()) {
            tmp.delete();
            throw new java.io.IOException("could not replace " + dest);
        }
        if (!tmp.renameTo(dest)) {
            tmp.delete();
            throw new java.io.IOException("could not rename " + tmp + " -> " + dest);
        }
        if (LogGate.on()) {
            Log.i(TAG, "imported " + dest + " (" + dest.length() + " bytes)");
        }
    }
}
