/*
 ============================================================================
 Name		: ITriedAppView.h
 Author	  : Your friendly mama
 Copyright   : I'm your mama
 Description : Declares view class for application.
 ============================================================================
 */

#ifndef __ITRIEDAPPVIEW_h__
#define __ITRIEDAPPVIEW_h__

// INCLUDES
#include <coecntrl.h>

// CLASS DECLARATION
class CITriedAppView : public CCoeControl {
public:
    // New methods

    /**
	 * NewL.
	 * Two-phased constructor.
	 * Create a CITriedAppView object, which will draw itself to aRect.
	 * @param aRect The rectangle this view will be drawn to.
	 * @return a pointer to the created instance of CITriedAppView.
	 */
    static CITriedAppView *NewL(const TRect &aRect);

    /**
	 * NewLC.
	 * Two-phased constructor.
	 * Create a CITriedAppView object, which will draw itself
	 * to aRect.
	 * @param aRect Rectangle this view will be drawn to.
	 * @return A pointer to the created instance of CITriedAppView.
	 */
    static CITriedAppView *NewLC(const TRect &aRect);

    /**
	 * ~CITriedAppView
	 * Virtual Destructor.
	 */
    virtual ~CITriedAppView();

public:
    // Functions from base classes

    /**
	 * From CCoeControl, Draw
	 * Draw this CITriedAppView to the screen.
	 * @param aRect the rectangle of this view that needs updating
	 */
    void Draw(const TRect &aRect) const;

    /**
	 * From CoeControl, SizeChanged.
	 * Called by framework when the view size is changed.
	 */
    virtual void SizeChanged();

    /**
	 * From CoeControl, HandlePointerEventL.
	 * Called by framework when a pointer touch event occurs.
	 * Note: although this method is compatible with earlier SDKs, 
	 * it will not be called in SDKs without Touch support.
	 * @param aPointerEvent the information about this event
	 */
    virtual void HandlePointerEventL(const TPointerEvent &aPointerEvent);

private:
    // Constructors

    /**
	 * ConstructL
	 * 2nd phase constructor.
	 * Perform the second phase construction of a
	 * CITriedAppView object.
	 * @param aRect The rectangle this view will be drawn to.
	 */
    void ConstructL(const TRect &aRect);

    /**
	 * CITriedAppView.
	 * C++ default constructor.
	 */
    CITriedAppView();
};

#endif // __ITRIEDAPPVIEW_h__
// End of File
