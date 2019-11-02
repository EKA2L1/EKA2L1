/*
 ============================================================================
 Name		: ITriedAppView.cpp
 Author	  : Your friendly mama
 Copyright   : I'm your mama
 Description : Application view implementation
 ============================================================================
 */

// INCLUDE FILES
#include "ITriedAppView.h"
#include <coemain.h>
#include <e32debug.h>

// ============================ MEMBER FUNCTIONS ===============================

// -----------------------------------------------------------------------------
// CITriedAppView::NewL()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CITriedAppView *CITriedAppView::NewL(const TRect &aRect) {
    CITriedAppView *self = CITriedAppView::NewLC(aRect);
    CleanupStack::Pop(self);
    return self;
}

// -----------------------------------------------------------------------------
// CITriedAppView::NewLC()
// Two-phased constructor.
// -----------------------------------------------------------------------------
//
CITriedAppView *CITriedAppView::NewLC(const TRect &aRect) {
    CITriedAppView *self = new (ELeave) CITriedAppView;
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    return self;
}

// -----------------------------------------------------------------------------
// CITriedAppView::ConstructL()
// Symbian 2nd phase constructor can leave.
// -----------------------------------------------------------------------------
//
void CITriedAppView::ConstructL(const TRect &aRect) {
    RDebug::Printf("Starting construct app view!");
    
    // Create a window for this application view
    CreateWindowL();

    // Set the windows size
    SetRect(aRect);

    // Activate the window, which makes it ready to be drawn
    ActivateL();
    
    RDebug::Printf("Successfully construct app view!");
}

// -----------------------------------------------------------------------------
// CITriedAppView::CITriedAppView()
// C++ default constructor can NOT contain any code, that might leave.
// -----------------------------------------------------------------------------
//
CITriedAppView::CITriedAppView() {
    // No implementation required
}

// -----------------------------------------------------------------------------
// CITriedAppView::~CITriedAppView()
// Destructor.
// -----------------------------------------------------------------------------
//
CITriedAppView::~CITriedAppView() {
    // No implementation required
}

// -----------------------------------------------------------------------------
// CITriedAppView::Draw()
// Draws the display.
// -----------------------------------------------------------------------------
//
void CITriedAppView::Draw(const TRect & /*aRect*/) const {
	RDebug::Printf("Drawing view");
	
    // Get the standard graphics context
    CWindowGc &gc = SystemGc();

    // Gets the control's extent
    TRect drawRect(Rect());

    // Clears the screen
    gc.Clear(drawRect);
    
    RDebug::Printf("Done drawing view");	
}

// -----------------------------------------------------------------------------
// CITriedAppView::SizeChanged()
// Called by framework when the view size is changed.
// -----------------------------------------------------------------------------
//
void CITriedAppView::SizeChanged() {
	RDebug::Printf("App view size changed");
    DrawNow();
}

// -----------------------------------------------------------------------------
// CITriedAppView::HandlePointerEventL()
// Called by framework to handle pointer touch events.
// Note: although this method is compatible with earlier SDKs,
// it will not be called in SDKs without Touch support.
// -----------------------------------------------------------------------------
//
void CITriedAppView::HandlePointerEventL(const TPointerEvent &aPointerEvent) {
    // Call base class HandlePointerEventL()
    CCoeControl::HandlePointerEventL(aPointerEvent);
}

// End of File
