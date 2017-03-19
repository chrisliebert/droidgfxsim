/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.glappjni;
/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


import android.content.Context;
import android.graphics.PixelFormat;
import android.opengl.GLSurfaceView;
import android.util.Log;
import android.content.res.AssetManager;
import android.view.KeyEvent;
import android.view.MotionEvent;

import javax.microedition.khronos.egl.EGL10;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.egl.EGLContext;
import javax.microedition.khronos.egl.EGLDisplay;
import javax.microedition.khronos.opengles.GL10;

/**
 * A simple GLSurfaceView sub-class that demonstrate how to perform
 * OpenGL ES 2.0 rendering into a GL Surface. Note the following important
 * details:
 *
 * - The class must use a custom context factory to enable 2.0 rendering.
 *   See ContextFactory class definition below.
 *
 * - The class must use a custom EGLConfigChooser to be able to select
 *   an EGLConfig that supports 2.0. This is done by providing a config
 *   specification to eglChooseConfig() that has the attribute
 *   EGL10.ELG_RENDERABLE_TYPE containing the EGL_OPENGL_ES2_BIT flag
 *   set. See ConfigChooser class definition below.
 *
 * - The class must select the surface's format, then choose an EGLConfig
 *   that matches it exactly (with regards to red/green/blue/alpha channels
 *   bit depths). Failure to do so would result in an EGL_BAD_MATCH error.
 */
class GLAppJNIView extends GLSurfaceView {
    private static String TAG = "GLJNIView";
    private static final boolean DEBUG = false;

    private float last_x = -1.f, last_y =-1.f, dx = 0.f, dy = 0.f;
    private float last_secondary_x = -1.f, last_secondary_y = -1.f, secondary_dx = 0.f, secondary_dy = 0.f;
    private boolean primary_down = false, secondary_down = false;
    private int pointer_count = 0;

    public GLAppJNIView(Context context) {
        super(context);
        // Pick an EGLConfig with RGB8 color, 16-bit depth, no stencil,
        // supporting OpenGL ES 2.0 or later backwards-compatible versions.
        init(false, 0, 0);
    }

    public GLAppJNIView(Context context, boolean translucent, int depth, int stencil) {
        super(context);
        init(translucent, depth, stencil);
    }

    private void init(boolean translucent, int depth, int stencil) {

        /* By default, GLSurfaceView() creates a RGB_565 opaque surface.
         * If we want a translucent one, we should change the surface's
         * format here, using PixelFormat.TRANSLUCENT for GL Surfaces
         * is interpreted as any 32-bit surface with alpha by SurfaceFlinger.
         */
        if (translucent) {
            this.getHolder().setFormat(PixelFormat.TRANSLUCENT);
        }

        /* Setup the context factory for 2.0 rendering.
         * See ContextFactory class definition below
         */
        //setEGLContextFactory(new ContextFactory());

        /* We need to choose an EGLConfig that matches the format of
         * our surface exactly. This is going to be done in our
         * custom config chooser. See ConfigChooser class definition
         * below.
         */
        //setEGLConfigChooser( translucent ?
        //                     new ConfigChooser(8, 8, 8, 8, depth, stencil) :
        //                     new ConfigChooser(5, 6, 5, 0, depth, stencil) );

        // Pick an EGLConfig with RGB8 color, 16-bit depth, no stencil,
        // supporting OpenGL ES 2.0 or later backwards-compatible versions.
        setEGLConfigChooser(8, 8, 8, 0, 16, 0);
        setEGLContextClientVersion(2);

        /* Set the renderer responsible for frame rendering */
        setRenderer(new Renderer());
    }

    private static void checkEglError(String prompt, EGL10 egl) {
        int error;
        while ((error = egl.eglGetError()) != EGL10.EGL_SUCCESS) {
            Log.e(TAG, String.format("%s: EGL error: 0x%x", prompt, error));
        }
    }

    /*
    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent keyEvent) {
        Log.i(TAG, "Touch Event: " + keyCode + ", " + keyEvent);
        return super.onKeyLongPress(keyCode, keyEvent);
    } */

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        final int action = ev.getActionMasked();

        /*
        if(action == (MotionEvent.ACTION_POINTER_DOWN & MotionEvent.ACTION_POINTER_INDEX_MASK & 2)) {
            Log.i(TAG, "Recieved 2 multi down: " + ev);
        } else if(action == (MotionEvent.ACTION_POINTER_UP & MotionEvent.ACTION_POINTER_INDEX_MASK & 2)) {
            Log.i(TAG, "Recieved 2 multi up: " + ev);
        } else
        */
        if(action == MotionEvent.ACTION_POINTER_DOWN) {
            pointer_count++;
            secondary_down = true;

            if(pointer_count > 1) {

                Log.i(TAG, "Recieved additional multi down: " + ev);
            }
        } else if(action == MotionEvent.ACTION_POINTER_UP) {
            pointer_count--;
            secondary_down = false;

            if(pointer_count > 1) {
                Log.i(TAG, "Recieved additional multi up: " + ev);
            }
        } else if(action == MotionEvent.ACTION_DOWN) {
            primary_down = true;
            pointer_count++;
            last_x = ev.getX();
            last_y = ev.getY();
        } else if(action == MotionEvent.ACTION_UP) {
            primary_down = false;
            pointer_count--;
            last_x = ev.getX();
            last_y = ev.getY();
        } else if(action == MotionEvent.ACTION_MOVE) {
            dx = (last_x - ev.getX());
            dy = (ev.getY() - last_y);
            last_x = ev.getX();
            last_y = ev.getY();
            final float move_factor = 0.005f;
            GLAppJNILib.moveCamera(dx * move_factor, dy * move_factor, 0.0f);
            //Log.i(TAG, "moved camera, pointer count = " + pointer_count);
        } else {
            Log.e(TAG, "Recieved unhandled touch event: " + ev);
        }
        return true;
    }

    public void onDestroy() {
        this.queueEvent(new Runnable() {
            @Override
            public void run() {
                GLAppJNILib.destroy();
            }
        });
    }

    private class Renderer implements GLSurfaceView.Renderer {
        public Renderer() {}
        public void onDrawFrame(GL10 gl) {
            GLAppJNILib.update();
            GLAppJNILib.render();
        }

        public void onSurfaceChanged(GL10 gl, int width, int height) {
            GLAppJNILib.resize(width, height);
        }

        public void onSurfaceCreated(GL10 gl, EGLConfig config)
        {
            AssetManager mgr = getResources().getAssets();
            GLAppJNILib.init(mgr);
        }
    }
}
