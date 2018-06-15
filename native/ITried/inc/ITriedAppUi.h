/*
 ============================================================================
 Name		: ITriedAppUi.h
 Author	  : Your friendly mama
 Copyright   : I'm your mama
 Description : Declares UI class for application.
 ============================================================================
 */

#ifndef __ITRIEDAPPUI_h__
#define __ITRIEDAPPUI_h__

// INCLUDES
#include <aknappui.h>

// FORWARD DECLARATIONS
class CITriedAppView;

// CLASS DECLARATION
/**
 * CITriedAppUi application UI class.
 * Interacts with the user through the UI and request message processing
 * from the handler class
 */
class CITriedAppUi : public CAknAppUi
	{
public:
	// Constructors and destructor

	/**
	 * ConstructL.
	 * 2nd phase constructor.
	 */
	void ConstructL();

	/**
	 * CITriedAppUi.
	 * C++ default constructor. This needs to be public due to
	 * the way the framework constructs the AppUi
	 */
	CITriedAppUi();

	/**
	 * ~CITriedAppUi.
	 * Virtual Destructor.
	 */
	virtual ~CITriedAppUi();

private:
	// Functions from base classes

	/**
	 * From CEikAppUi, HandleCommandL.
	 * Takes care of command handling.
	 * @param aCommand Command to be handled.
	 */
	void HandleCommandL(TInt aCommand);

	/**
	 *  HandleStatusPaneSizeChange.
	 *  Called by the framework when the application status pane
	 *  size is changed.
	 */
	void HandleStatusPaneSizeChange();

	/**
	 *  From CCoeAppUi, HelpContextL.
	 *  Provides help context for the application.
	 *  size is changed.
	 */
	CArrayFix<TCoeHelpContext>* HelpContextL() const;

private:
	// Data

	/**
	 * The application view
	 * Owned by CITriedAppUi
	 */
	CITriedAppView* iAppView;

	};

#endif // __ITRIEDAPPUI_h__
// End of File
