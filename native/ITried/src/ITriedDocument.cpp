/*
 ============================================================================
 Name		: ITriedDocument.cpp
 Author	  : Your friendly mama
 Copyright   : I'm your mama
 Description : CITriedDocument implementation
 ============================================================================
 */

// INCLUDE FILES
#include "ITriedDocument.h"
#include "ITriedAppUi.h"

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CITriedDocument::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CITriedDocument *CITriedDocument::NewL(CEikApplication &aApp) {
    CITriedDocument *self = NewLC(aApp);
    CleanupStack::Pop(self);
    return self;
}

// -----------------------------------------------------------------------------
// CITriedDocument::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CITriedDocument *CITriedDocument::NewLC(CEikApplication &aApp) {
    CITriedDocument *self = new (ELeave) CITriedDocument(aApp);

    CleanupStack::PushL(self);
    self->ConstructL();
    return self;
}

// -----------------------------------------------------------------------------
// CITriedDocument::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CITriedDocument::ConstructL() {
    // No implementation required
}

// -----------------------------------------------------------------------------
// CITriedDocument::CITriedDocument()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CITriedDocument::CITriedDocument(CEikApplication &aApp)
    : CAknDocument(aApp) {
    // No implementation required
}

// ---------------------------------------------------------------------------
// CITriedDocument::~CITriedDocument()
// Destructor.
// ---------------------------------------------------------------------------
//
CITriedDocument::~CITriedDocument() {
    // No implementation required
}

// ---------------------------------------------------------------------------
// CITriedDocument::CreateAppUiL()
// Constructs CreateAppUi.
// ---------------------------------------------------------------------------
//
CEikAppUi *CITriedDocument::CreateAppUiL() {
    // Create the application user interface, and return a pointer to it;
    // the framework takes ownership of this object
    return new (ELeave) CITriedAppUi;
}

// End of File
