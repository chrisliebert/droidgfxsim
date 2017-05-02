Copyright (C) 2017 Chris Liebert

This Android app uses a native OpenGL ES compatable render system along with
the Bullet Physics SDK to provide an XML-defined graphical 3D simulation that can
also be built as a native desktop application using GLFW3 for window
management. The Android portion of application is derived from the gles3jni
Android SDK sample
(for more information, see
https://github.com/googlesamples/android-ndk/tree/master/gles3jni ).

Building the Desktop Application:
All third-party dependencies are either included in the repository or installed
via the ExternalProject_Add function provided by the CMake build system and Git. It
can be built for Windows with MSVC, MinGW or Linux. The build script will attempts 
to download and build all dependencies and link them statically. An advantage of 
this approach is the ability to easily install the debug symbols across different
platforms.

Running the Desktop Application:
There are various assets found in app/src/main/assets that need to be in the
current directory for the application to work correctly.

Building the Android Application:
This application requires the Android NDK and relies on a slightly different CMake
build script than the desktop application and will be used to produce shared libraries
for multiple architectures. The CMake build is triggered by the top-level Gradle build
script.

License Information:
There are files that are part of the Android Open Source
Project. There are a number of dependencies that use various permissive open source
licences.
