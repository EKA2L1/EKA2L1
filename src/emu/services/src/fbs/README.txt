Something to note for this server:

1. EFbsMessLoadBitmap always decompress the bitmap. Only external calls when compress bitmap.
2. Font size of bitmap font (gdr) will stay the same in every situation.
3. On s60v1 only two in-memory compression supported (8bpp and 12bpp, base compression number=50).
On s60v3 clean bitmap mechanism make this more ease.