#include <jni.h>
#include <android/log.h>
#include <sched.h>

#if DYNAMIC_ES3
#include "gl3stub.h"
#else

#include <GLES3/gl3.h>

#endif

#define    LOG_TAG        "libglappjni"
#define    LOGI(...)    __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define    LOGE(...)    __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

#include "application/application.h"
#include "graphics/gl_code.h"
#include "graphics/gl2_renderer.h"
#include "graphics/gl3_renderer.h"

static Application* app = 0;
static GL2SceneGraphRenderer* gl2 = 0;
static GL3SceneGraphRenderer* gl3 = 0;

extern "C" {

JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_init(JNIEnv *env, jobject obj, jobject asset_mgr);
JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_moveCamera(JNIEnv *env, jobject obj, jfloat x, jfloat y, jfloat z);
JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_render(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_resize(JNIEnv *env, jobject obj, jint width, jint height);
JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_update(JNIEnv *env, jobject obj);
JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_destroy(JNIEnv *env, jobject obj);

};

//#if !defined(DYNAMIC_ES3)
//static GLboolean gl3stubInit() {
//    return GL_TRUE;
//}
//#endif

JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_init(JNIEnv *env, jobject obj, jobject asset_mgr) {
	AAssetManager* asset_manager = AAssetManager_fromJava(env, asset_mgr);
	assert(asset_manager);
	if(gl2) {
        delete gl2;
        gl2 = 0;
    }
    if(gl3) {
        delete gl3;
        gl3 = 0;
    }
    if(app != 0) {
        delete app;
        app = 0;
    }
    app = new Application(asset_manager);
    assert(app);
    const char *versionStr = (const char *) glGetString(GL_VERSION);
	if (strstr(versionStr, "OpenGL ES 3.")) {
#if DYNAMIC_ES3
		if(!gl3stubInit()) {
			LOGE("Unable to initialize dynamic GL ES3 stub");
			exit(-1);
		}
#endif //DYNAMIC_ES3
		LOGI("Creating OpenGL ES 3 Renderer");
		gl3 = new GL3SceneGraphRenderer(app->images);
		assert(gl3);
	} else if (strstr(versionStr, "OpenGL ES 2.")) {
		LOGI("Creating OpenGL 2 Renderer");
		gl2 = new GL2SceneGraphRenderer(app->images);
		assert(gl2);
	} else {
		LOGE("Unsupported OpenGL ES version");
		exit(-2);
	}
}

JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_moveCamera(JNIEnv *env, jobject obj, jfloat x, jfloat y, jfloat z) {
    if(app) {
        app->camera->position.x += x;
        app->camera->position.y += y;
        app->camera->position.z += z;
        app->camera->update();
    }
}

JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_render(JNIEnv *env, jobject obj) {
    if(app) {
        if (gl2) app->render(gl2);
        else if (gl3) app->render(gl3);
        else LOGE("No renderer present");
    }
}

JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_resize(JNIEnv *env, jobject obj, jint width, jint height) {
	if(app) app->resize(width, height);
}

JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_update(JNIEnv *env, jobject obj) {
    if(app) app->step();
}

JNIEXPORT void JNICALL Java_com_android_glappjni_GLAppJNILib_destroy(JNIEnv *env, jobject obj) {
    if(gl2) {
		delete gl2;
		gl2 = 0;
	}
    if(gl3) {
		delete gl3;
		gl3 = 0;
	}

    if(app) {
        if(app->scenegraph_root) {
            scenegraph::destroy(app->scenegraph_root);
            app->scenegraph_root = 0;
        }
        LOGI("Destroyed scenegraph");
        delete app;
        app = 0;
    }
}
