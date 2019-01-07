/*
 ============================================================================
 Name		: ITried.cpp
 Author	  : Your friendly mama
 Copyright   : I'm your mama
 Description : Main application class
 ============================================================================
 */

// INCLUDE FILES
#include "ITriedApplication.h"
#include <eikstart.h>

LOCAL_C CApaApplication *NewApplication() {
    return new CITriedApplication;
}

GLDEF_C TInt E32Main() {
    return EikStart::RunApplication(NewApplication);
}
