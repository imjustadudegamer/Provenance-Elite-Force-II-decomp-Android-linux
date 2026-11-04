package com.provenanceef2;

import android.app.Activity;
import android.app.Dialog;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.Gravity;
import android.view.Window;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

/**
 * Error windows built from literal colours only: an OEM alert skin showed the
 * themed dialogs white-on-white, so the port's own panels resolve nothing
 * through a theme.
 */
final class UiPanel {
    private static final int PANEL = 0xF01B1B22;
    private static final int TITLE = 0xFFFFFFFF;
    private static final int BODY = 0xFFD8D8DE;
    private static final int BUTTON = 0xFF3A3F48;

    private UiPanel() {}

    /** A dark panel with title, body and a Quit button that ends the activity. */
    static Dialog error(Activity a, String title, String body) {
        ScrollView scroll = new ScrollView(a);
        scroll.setBackgroundColor(PANEL);

        LinearLayout box = new LinearLayout(a);
        box.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(a, 18);
        box.setPadding(pad, pad, pad, pad);
        scroll.addView(box);

        TextView t = new TextView(a);
        t.setTextColor(TITLE);
        t.setTextSize(18);
        t.setText(title);
        box.addView(t);

        TextView m = new TextView(a);
        m.setTextColor(BODY);
        m.setTextSize(14);
        m.setText(body);
        box.addView(m);

        Button quit = new Button(a);
        quit.setText("Quit");
        quit.setTextColor(TITLE);
        quit.setBackgroundColor(BUTTON);
        quit.setOnClickListener(v -> a.finish());
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT,
                LinearLayout.LayoutParams.WRAP_CONTENT);
        lp.topMargin = dp(a, 14);
        lp.gravity = Gravity.END;
        box.addView(quit, lp);

        Dialog d = new Dialog(a);
        d.requestWindowFeature(Window.FEATURE_NO_TITLE);
        d.setContentView(scroll);
        d.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        d.setCancelable(false);
        d.show();
        return d;
    }

    private static int dp(Activity a, int v) {
        return Math.round(v * a.getResources().getDisplayMetrics().density);
    }
}
