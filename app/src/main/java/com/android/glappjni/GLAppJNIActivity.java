/*
 * Copyright (C) 2007 The Android Open Source Project
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

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.content.res.AssetManager;
import android.util.Log;


public class GLAppJNIActivity extends Activity {

    boolean stopped = false;
    GLAppJNIView mView;
    Bundle bundle = null;

    @Override protected void onCreate(Bundle icicle) {
        super.onCreate(icicle);
        bundle = icicle;
        mView = new GLAppJNIView(getApplication());
        setContentView(mView);
    }

    @Override protected void onDestroy() {
        super.onDestroy();
        mView.onDestroy();
    }

    @Override protected void onStop() {
        super.onStop();
        if(!stopped) {
            onDestroy();
            Log.i("Activity", "Stopping activity");
            stopped = true;
        }
    }

    @Override protected void onResume() {
        if(stopped) {
            mView = null;
            onCreate(bundle);
            stopped = false;
        }
        super.onResume();
        mView.onResume();
    }
}
