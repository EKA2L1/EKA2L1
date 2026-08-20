This part of the project is the replacement for the screen driver comes with every phone (scdv.dll).

This project is neccessary to replace any references to communicate with the hardware through logical device driver,
and replace them with calls to emulator.

Because the implementation on Symbian OSS is licensed under EPL, this project reimplements all stuffs, hoping
to be faster.

Newer S60/Belle BitGDI clients ask the draw device for the premultiplied-alpha
`EColor16MAP` mode and for the screen `MSurfaceId` interface, and give up when
either is missing. Both are implemented here in C++, with the two partner-only
declarations they need (`CDirectScreenBitmap`, `TSurfaceId`/`MSurfaceId`)
restated locally, since the public Belle SDK does not ship those headers.

The checked-in `group/scdv_general.dll` is a GCCE build of this source from the
Nokia Symbian Belle SDK; its frozen DEF keeps the 31-entry export ABI that ROM
patch maps rely on. Rebuilding it needs that SDK and its bundled CSL toolchain,
so the binary is committed alongside the source the way the other patch DLLs in
this tree are.
