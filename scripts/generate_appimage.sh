#!/bin/sh -ex

# A part of this script is based on DuckStation! Thanks all the contributors
# from there very much!i

cd build

# Download all resources
wget --timestamping https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
chmod a+x linuxdeploy-x86_64.AppImage


wget --timestamping https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod a+x linuxdeploy-plugin-qt-x86_64.AppImage

wget --timestamping  https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage
chmod a+x linuxdeploy-plugin-appimage-x86_64.AppImage

# Copy files in bin to AppDir
# We mixed many files in here, might as well as copy all
mkdir -p eka2l1.AppDir/usr/bin/
cp -a bin/. eka2l1.AppDir/usr/bin/

./linuxdeploy-x86_64.AppImage --appimage-extract
OUTPUT="eka2l1-qt-x64.AppImage" ./squashfs-root/AppRun --appdir=eka2l1.AppDir --executable=bin/eka2l1_qt --desktop-file=bin/eka2l1.desktop --icon-file=bin/duck_tank.png --plugin=qt --output appimage
