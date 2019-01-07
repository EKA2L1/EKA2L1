/*
 ============================================================================
 Name		: ITriedApplication.cpp
 Author	  : Your friendly mama
 Copyright   : I'm your mama
 Description : Main application class
 ============================================================================
 */

// INCLUDE FILES
#include "ITriedApplication.h"
#include "ITried.hrh"
#include "ITriedDocument.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CITriedApplication::CreateDocumentL()
// Creates CApaDocument object
// -----------------------------------------------------------------------------
//
CApaDocument *CITriedApplication::CreateDocumentL() {
    // Create an ITried document, and return a pointer to it
    return CITriedDocument::NewL(*this);
}

// -----------------------------------------------------------------------------
// CITriedApplication::AppDllUid()
// Returns application UID
// -----------------------------------------------------------------------------
//
TUid CITriedApplication::AppDllUid() const {
    // Return the UID for the ITried application
    return KUidITriedApp;
}

// End of File
