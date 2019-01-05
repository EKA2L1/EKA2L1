# Building
EKA2L1 almost has any dependency provided in the repo, so build the emulator should be trivial. There are still some external dependency, including:
- curl

To build the emulator, follow these steps:
- Windows:
   - Bootstrap VCPKG and install missing dependencies. In case of EKA2L1, run these commands on Powershell with Adminstrator permissions
   ```
   ./vcpkg --triplet x64-windows curl 
   ```
   - Run CMake targets your favorite generator with *-DCMAKE_TOOLCHAIN_FILE='<vcpkg_path>\scripts\buildsystems\vcpkg.cmake'* to specify the VCPKG toolchain
   include. If you don't pass the toolchain argument, than CURL will not be found, unless in some way you have installed it.
   - Make all or make these critical targets:
      * eka2l1 *(Console interface of the emulator)*

- Linux, MacOS
    - Install curl package through your prefer package manager.
    - Run CMake to generate make file.
    - Build with make -j 4
