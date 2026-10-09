package com.provenanceef2;

import android.util.Log;

/**
 * The one diag arm for every routine log row, native and Java alike:
 *
 * <pre>adb shell setprop log.tag.ProvenanceEF2 1</pre>
 *
 * Any value other than 0/false arms it, and it is read once.
 *
 * Policy: routine rows -- verbose, debug and info, vendor and app alike --
 * answer the arm and are silent without it.  Warning and error rows do not
 * consult it and stay live in every state, which is the same floor the native
 * half holds SDL's own output to (SDL_LogSetAllPriority(WARN) when quiet).
 * The crash channel is likewise ungated.
 *
 * The property is read raw rather than through {@link Log#isLoggable}: the
 * platform parser accepts only level letters (liblog's properties.cpp has no
 * digit cases), so the shipped arm value would leave isLoggable at its INFO
 * default and an INFO row would print on every boot.
 */
public final class LogGate {
    private static Boolean sArmed;

    private LogGate() {
    }

    public static boolean on() {
        Boolean armed = sArmed;
        if (armed == null) {
            armed = readArm();
            sArmed = armed;
        }
        return armed.booleanValue();
    }

    private static boolean readArm() {
        try {
            Class<?> sysprops = Class.forName("android.os.SystemProperties");
            java.lang.reflect.Method get = sysprops.getMethod("get", String.class, String.class);
            String value = (String) get.invoke(null, "log.tag.ProvenanceEF2", "");
            if (value != null && value.length() > 0
                    && !value.equalsIgnoreCase("0") && !value.equalsIgnoreCase("false")) {
                return true;
            }
        } catch (Throwable t) {
            // The property read is unavailable: the level-letter form of the
            // same arm still works, and it arms the native half too.
            if (Log.isLoggable("ProvenanceEF2", Log.VERBOSE)) {
                return true;
            }
        }
        return false;
    }
}
