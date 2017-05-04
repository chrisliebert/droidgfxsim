// Copyright (C) 2017 Chris Liebert

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
