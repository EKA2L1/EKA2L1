/*
 ============================================================================
 Name		: ITriedDocument.h
 Author	  : Your friendly mama
 Copyright   : I'm your mama
 Description : Declares document class for application.
 ============================================================================
 */

#ifndef __ITRIEDDOCUMENT_h__
#define __ITRIEDDOCUMENT_h__

// INCLUDES
#include <akndoc.h>

// FORWARD DECLARATIONS
class CITriedAppUi;
class CEikApplication;

// CLASS DECLARATION

/**
 * CITriedDocument application class.
 * An instance of class CITriedDocument is the Document part of the
 * AVKON application framework for the ITried example application.
 */
class CITriedDocument : public CAknDocument {
public:
    // Constructors and destructor

    /**
	 * NewL.
	 * Two-phased constructor.
	 * Construct a CITriedDocument for the AVKON application aApp
	 * using two phase construction, and return a pointer
	 * to the created object.
	 * @param aApp Application creating this document.
	 * @return A pointer to the created instance of CITriedDocument.
	 */
    static CITriedDocument *NewL(CEikApplication &aApp);

    /**
	 * NewLC.
	 * Two-phased constructor.
	 * Construct a CITriedDocument for the AVKON application aApp
	 * using two phase construction, and return a pointer
	 * to the created object.
	 * @param aApp Application creating this document.
	 * @return A pointer to the created instance of CITriedDocument.
	 */
    static CITriedDocument *NewLC(CEikApplication &aApp);

    /**
	 * ~CITriedDocument
	 * Virtual Destructor.
	 */
    virtual ~CITriedDocument();

public:
    // Functions from base classes

    /**
	 * CreateAppUiL
	 * From CEikDocument, CreateAppUiL.
	 * Create a CITriedAppUi object and return a pointer to it.
	 * The object returned is owned by the Uikon framework.
	 * @return Pointer to created instance of AppUi.
	 */
    CEikAppUi *CreateAppUiL();

private:
    // Constructors

    /**
	 * ConstructL
	 * 2nd phase constructor.
	 */
    void ConstructL();

    /**
	 * CITriedDocument.
	 * C++ default constructor.
	 * @param aApp Application creating this document.
	 */
    CITriedDocument(CEikApplication &aApp);
};

#endif // __ITRIEDDOCUMENT_h__
// End of File
