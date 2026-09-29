# qjpeg

A replacement for Qt's JPEG image format plugin that decodes on the host instead
of running libjpeg under emulation. Qt applications reach it through the normal
`QImageReader` plugin lookup.

`qjpeg.dll.map` makes the loader load this plugin in place of the ROM's
`qjpeg.dll`, but only when the ROM's Qt would accept it (see the `qt-plugin`
requirement in `lib_manager`). It carries the ROM plugin's UID3 and
capabilities.

## Building

Belle SDK (Qt 4.7.4), which produces a plugin the Qt 4.8.0 ROM accepts - the
build key is `Symbian full-config` in both, and Qt accepts a plugin whose minor
version is not newer than its own.

```
moc.exe -o src\qjpeghandler.moc src\qjpeghandler.cpp
cd group\general
sbs -b bld.inf -c armv5_urel_gcce4_4_1
```

`moc` output is generated next to the source and included by it, so it does not
appear in the mmp. The built DLL is committed as `group/qjpeg_general.dll`.
