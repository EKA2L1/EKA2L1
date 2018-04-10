# EKA2L1HWD


## Brief 
- This is based on the Hello World app example, which emits info about where pointer relies in the memory region. This is made because the
description in Symbian header files are not really helpful. 
- I could just made a memory mapped with page and alloc memory from them without caring about the address but that will breaks the 
compability of many apps and games because of dynamic allocation and also relocations. For e.g, in 2010, Opera Mobile on Symbian gets a
JIT, which relies heavily on dynamic allocation (RChunk::CreateLocalCode). gpSP emulator relies heavily on relocation, since the distance
between local data and code is too far. And many more hacks.
- The only solution I could think of is to make a memory management unit which acts pretty smiliar to Symbian's.
- Current memory code I am writing is inspired by Citra's

## Result after testing
- Static data stays in the RAM code as I expected
- Memory used by user are only allocated in local. To allocate in global, one must use another like RChunk.
- The position to put code is near the region's end. Kernel puts the code in 0x79000000 and upper, I guess. It's pretty near to the
end. I will try to emulate this behavior by caculate the code and data size and substract it from 0x79000000 (a constant I think at the
time writing this).
