/*
 ============================================================================
 Name		: ITriedApplication.h
 Author	  : Your friendly mama
 Copyright   : I'm your mama
 Description : Declares main application class.
 ============================================================================
 */

#ifndef __ITRIEDAPPLICATION_H__
#define __ITRIEDAPPLICATION_H__

// INCLUDES
#include <aknapp.h>
#include "ITried.hrh"

// UID for the application;
// this should correspond to the uid defined in the mmp file
const TUid KUidITriedApp =
	{
	_UID3
	};

// CLASS DECLARATION

/**
 * CITriedApplication application class.
 * Provides factory to create concrete document object.
 * An instance of CITriedApplication is the application part of the
 * AVKON application framework for the ITried example application.
 */
class CITriedApplication : public CAknApplication
	{
public:
	// Functions from base classes

	/**
	 * From CApaApplication, AppDllUid.
	 * @return Application's UID (KUidITriedApp).
	 */
	TUid AppDllUid() const;

protected:
	// Functions from base classes

	/**
	 * From CApaApplication, CreateDocumentL.
	 * Creates CITriedDocument document object. The returned
	 * pointer in not owned by the CITriedApplication object.
	 * @return A pointer to the created document object.
	 */
	CApaDocument* CreateDocumentL();
	};

#endif // __ITRIEDAPPLICATION_H__
// End of File
