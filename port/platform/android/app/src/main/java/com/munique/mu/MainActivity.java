package com.munique.mu;

// SDL3's official `SDLActivity` is provided by the SDL3 Android source
// tree. Once the SDL3 Gradle AAR is wired in (Milestone 2), this class
// will extend `org.libsdl.app.SDLActivity` and dlopen "main" (the .so
// produced by our CMake build).
//
// For Milestone 1 the class is a placeholder so the Android manifest is
// valid for static analysis tooling.

import android.app.Activity;
import android.os.Bundle;

public class MainActivity extends Activity {

    static {
        // Match the SDL3 default library load order. On Milestone 2 we
        // will replace this with:   System.loadLibrary("SDL3");
        try {
            System.loadLibrary("main");
        } catch (UnsatisfiedLinkError ignored) {
            // Acceptable in M1 — Gradle hasn't been wired to actually
            // build the native lib yet; this class only needs to compile.
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        finish();  // M1: just bail out; M2 will hand control to SDLActivity.
    }
}
