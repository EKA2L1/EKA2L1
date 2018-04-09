#include "EKA2L1Mem.h"

// Translate the memory region address
TPtrC TranslateMemoryRegion(TUint32 aAddress) {
	if ((UserLocalData < aAddress) &&
	   (aAddress < DLLStaticData)) {
		return _L("Local data");
	} else {
		if ((UserSharedData < aAddress) &&
			   (aAddress < LoadedCode)) {
				return _L("Shared data");
		} else {
			if ((GlobalData < aAddress) &&
				(aAddress < RAMDrive)) {
				return _L("Global data");
			} else {
				if ((LoadedCode < aAddress) &&
					(aAddress < ROM)) {
				    return _L("RAM code");	
				}
			}
		}
	}
	
	return _L("Unknown region");
}
