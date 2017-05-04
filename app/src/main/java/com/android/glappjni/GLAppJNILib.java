// Copyright (C) 2017 Chris Liebert

package com.android.glappjni;

import android.content.res.AssetManager;

// Wrapper for native library
public class GLAppJNILib {
     static {
         System.loadLibrary("glappjni");
     }

    public static native void init(AssetManager asset_manager);
    public static native void moveCamera(float x, float y, float z);
    public static native void render();
    public static native void resize(int width, int height);
    public static native void update();
    public static native void destroy();
}
