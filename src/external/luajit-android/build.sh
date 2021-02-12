NDK=/e/Dev/AndroidSDK/ndk/22.0.6917172
NDKABI=21
NDKVER=$NDK/toolchains/llvm
NDKP=$NDKVER/prebuilt/windows-x86_64/bin/aarch64-linux-android-
NDKCC=$NDKVER/prebuilt/windows-x86_64/bin/aarch64-linux-android$NDKABI-clang
NDKF="--sysroot=$NDKVER/prebuilt/windows-x86_64/sysroot"
NDKARCH=""
# For 32 bit, use gcc -m32, ndkcc = armv7a-linux-androideabi$NDKABI, ndkp = arm-linux-androideabi-, softfloat
make DEFAULT_CC=clang HOST_CC="gcc" CROSS=$NDKP STATIC_CC=$NDKCC DYNAMIC_CC="$NDKCC -fPIC" TARGET_LD=$NDKCC TARGET_SYS=Linux TARGET_FLAGS="$NDKF $NDKARCH" -j4