## Required programs

### For all targets

- Git: Required to clone the source code of EKA2L1
- CMake: Used to generate Makefiles/solution for work/compliation process.
- Qt5/Qt6: This is used to build the UI of the emulator on PC platforms.
- Python: The emulator calls some additional Python scripts for extra special build process.

### Specific target

- Android:
    * Android Studio: Develop and compile to Android platfrom through this IDE.
    * NDK: The Android SDK.

- Windows:
    * Visual Studio 2019 or higher, coming with support for compilation of C++17 code or higher.

- MacOS X:
    * XCode 12.4 or higher.

- Linux:
    * clang/g++: Clang 10.0/GCC 10.1 or higher is recommended. Note that Clang is not tested or built on the CI, but it should compile fine if compliation with GCC goes OK. If not, please open an issue.

### Optional

- Symbian SDKs (only on Windows!): EKA2L1 contains code that can compile to native Symbian DLL, with the intention of replacing the original DLL files shipped with phones with another DLL versions that interact directly with the emulator. SDKs that should be installed are: **S60v5, S60v2fp2 and S^3** SDKs. **Make sure** the EKA2L1 source code and the Symbian SDKs are *on the same drive*.

## Retrieve the source code

- Use a preferred Git client, or the command line tool with the command ```git clone --recurse-submodules https://github.com/EKA2L1/EKA2L1``` to clone the [EKA2L1](https://github.com/EKA2L1/EKA2L1) Github source code.

- EKA2L1 makes use of many code repositories as dependencies, so initializing and cloning all submodules are required. If the Git client has not done the job yet (you can go check the folder ```src/emu/external``` and see if content of child folders are empty or not), or you forgot to clone the repository with the ```--recurse-submodules``` flag, use this command to update and clone the submodules: ```git submodule update --init --recursive```.

## Build the emulator

- It's recommended to build the emulator with **RelWithDebInfo** configuration for debugging purposes.

### On Windows, MacOS and Linux distros

- During the installation of the Qt framework, if you use the official install tool, you might already have Qt Creator installed. The choice of using another IDE or using Qt Creator is up to you:

    * If you are using Qt Creator: there should be no additional setup needed, open the CMakeLists.txt in the root folder of the EKA2L1 source code, which will then automatically setup the project in the IDE for you. Choose the preferred build configuration and build the eka2l1_qt target to generate the UI executable.

    * If you are not using Qt Creator: run CMake with CMAKE_PREFIX_PATH set to your Qt installation. For example, if you want to generate solution to open in Visual Studio, you can set your CMAKE_PREFIX_PATH to *D:\Programs\Qt\6.1.1\msvc2019_64*. After you got the makefile/VS solution, proceed to call *make*/open them for compilation.

### On Android

- With Android Studio opened, navigate to File/Open and choose the ```source code root/src/android/``` folder. The android project for EKA2L1 should setup and ready. 