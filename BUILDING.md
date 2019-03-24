# Building
EKA2L1 codebase has many dependencies prebuilt or build with source. Some dependency requires:
- Boost

Otherwise the build process are pretty trivial. EKA2L1 try to avoid bringing dependency without CMakeLists in.

To build the emulator, install these dependencies, and follow these steps:

- Windows:
   - Run CMake targets your favorite generator.
   - Make all or make these critical targets:
      * eka2l1 *(Console interface of the emulator)*

- Linux, MacOS
    - Run CMake to generate make file.
    - Build with make -j 4
