## On Windows, MacOS and Linux distros

Building on computer platforms require you to have at least Qt 5.15 installed. EKA2L1 is being developed with Qt 6, but it should also be compatible with older Qt versions.

### Generating makefile/solution

- If you are using Qt Creator: there should be no additional setups, run CMake inside Qt and it will be automatically setup for you.
- If you are not using Qt Creator: run CMake with CMAKE_PREFIX_PATH set to your Qt installation. For example, if you want to generate solution to open in Visual Studio, you can set your CMAKE_PREFIX_PATH to *D:\Programs\Qt\6.1.1\msvc2019_64*.

### The build target

- There is no standalone command line build target. EKA2L1 is made with UI integrated heavily. The Qt UI target is named **eka2l1_qt**

## On Android

There's no special setup either for the Android backend. Open the *src/android*'s CMakeLists in Android Studio. And you are good to go.