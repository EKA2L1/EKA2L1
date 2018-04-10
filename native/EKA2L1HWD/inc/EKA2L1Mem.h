#ifndef __EKA2L1MEM_H__
#define __EKA2L1MEM_H__

#include <e32base.h>
#include <e32std.h>

enum EMemoryRegion {
	UserNullTrap   = 0x00000000,
	UserLocalData  = 0x00400000,
	DLLStaticData  = 0x38000000,
	UserSharedData = 0x40000000,
	LoadedCode     = 0x70000000,
	ROM            = 0x80000000,
	GlobalData     = 0x90000000,
	RAMDrive       = 0xA0000000,
	KernelHeap     = 0xC8000000
};

TPtrC TranslateMemoryRegion(TUint32 aAddress);

#endif
