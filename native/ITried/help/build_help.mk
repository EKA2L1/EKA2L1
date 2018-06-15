# ============================================================================
#  Name	 : build_help.mk
#  Part of  : ITried
# ============================================================================
#  Name	 : build_help.mk
#  Part of  : ITried
#
#  Description: This make file will build the application help file (.hlp)
# 
# ============================================================================

do_nothing :
	@rem do_nothing

# build the help from the MAKMAKE step so the header file generated
# will be found by cpp.exe when calculating the dependency information
# in the mmp makefiles.

MAKMAKE : ITried_0xed3e09d5.hlp
ITried_0xed3e09d5.hlp : ITried.xml ITried.cshlp Custom.xml
	cshlpcmp.bat ITried.cshlp
ifeq (WINSCW,$(findstring WINSCW, $(PLATFORM)))
	md $(EPOCROOT)epoc32\$(PLATFORM)\c\resource\help
	copy ITried_0xed3e09d5.hlp $(EPOCROOT)epoc32\$(PLATFORM)\c\resource\help
endif

BLD : do_nothing

CLEAN :
	del ITried_0xed3e09d5.hlp
	del ITried_0xed3e09d5.hlp.hrh

LIB : do_nothing

CLEANLIB : do_nothing

RESOURCE : do_nothing
		
FREEZE : do_nothing

SAVESPACE : do_nothing

RELEASABLES :
	@echo ITried_0xed3e09d5.hlp

FINAL : do_nothing
