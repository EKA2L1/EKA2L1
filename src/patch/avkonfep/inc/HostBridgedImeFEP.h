#ifndef AVKONFEP_IME_FEP_H_
#define AVKONFEP_IME_FEP_H_

#include <coeinput.h>
#include <fepbase.h>
#include <fepplugin.h>
#include <fepitfr.h>
#include <ecom/ecom.h>
#include <ecom/ImplementationProxy.h>

class CHostBridgedImeFEP;

class CHostDialogIme : public CActive {
private:
    CHostBridgedImeFEP *iHost;
public:
    CHostDialogIme(CHostBridgedImeFEP *aHost);
    virtual ~CHostDialogIme();
    
    TInt Open(const TDesC *aInitialText, const TInt aMaxLength);

    virtual void RunL();
    virtual void DoCancel();
};

class CHostBridgedImeFEP : public CCoeFep, public MFepInlineTextFormatRetriever, public MFepPointerEventHandlerDuringInlineEdit {
private:
    virtual void HandleGainingForeground();
    virtual void HandleLosingForeground();
    virtual void HandleChangeInFocus();
    virtual void HandleDestructionOfFocusedItem();
    
    void HandleChangeInFocusL();
    void OpenDialogInputL();

    TCoeInputCapabilities iInputCapabilities;
    TBool iDialogPending;

    CHostDialogIme iImeDialog;

public:
    CHostBridgedImeFEP(CCoeEnv &aConeEnvironment);
    virtual ~CHostBridgedImeFEP();

    void ConstructL(const CCoeFepParameters &aParameters);
    
    virtual TInt NumberOfAttributes() const;
    virtual TUid AttributeAtIndex(TInt aIndex) const;
    virtual void WriteAttributeDataToStreamL(TUid aAttributeUid, RWriteStream& aStream) const;
    virtual void ReadAttributeDataFromStreamL(TUid aAttributeUid, RReadStream& aStream);

    virtual void CancelTransaction();
    virtual void IsOnHasChangedState();
    virtual void OfferKeyEventL(TEventResponse& aEventResponse, const TKeyEvent& aKeyEvent, TEventCode aEventCode);
    virtual void OfferPointerEventL(TEventResponse& aEventResponse, const TPointerEvent& aPointerEvent, const CCoeControl* aWindowOwningControl);
    virtual void OfferPointerBufferReadyEventL(TEventResponse& aEventResponse, const CCoeControl* aWindowOwningControl);
    
    virtual void GetFormatOfFepInlineText(TCharFormat& aFormat, TInt& aNumberOfCharactersWithSameFormat, 
        TInt aPositionOfCharacter) const;
    virtual void HandlePointerEventInInlineTextL(TPointerEvent::TType aType, TUint aModifiers, 
        TInt aPositionInInlineText);

    void CommitChangedTextL();
};

class CHostBridgedImeFEPPlugin : public CCoeFepPlugIn {
public:
    static CHostBridgedImeFEPPlugin *NewL();

    virtual CCoeFep* NewFepL(CCoeEnv& aConeEnvironment, const CCoeFepParameters& aFepParameters);
    virtual void SynchronouslyExecuteSettingsDialogL(CCoeEnv& aConeEnvironment);
};
  
EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount);

#endif
