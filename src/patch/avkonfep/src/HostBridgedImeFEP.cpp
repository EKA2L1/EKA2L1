#include <HostBridgedImeFEP.h>
#include <aknappui.h>
#include <Dispatch.h>
#include <Log.h>

CHostDialogIme::CHostDialogIme(CHostBridgedImeFEP *aHost)
    : CActive(EPriorityHigh)
    , iHost(aHost) {
    CActiveScheduler::Add(this);
}

CHostDialogIme::~CHostDialogIme() {
    Deque();
}

TInt CHostDialogIme::Open(const TDesC *aInitialText, const TInt aMaxLength) {
    iStatus = KRequestPending;
    TInt result = EHUIOpenGlobalTextView(0, aInitialText, aMaxLength, &iStatus);
    if (result != KErrNone) {
        return result;
    }
    
    SetActive();
    return KErrNone;
}

void CHostDialogIme::RunL() {
    if (iStatus.Int() == KErrNone) {
        iHost->CommitChangedTextL();
    }
}

void CHostDialogIme::DoCancel() {
    ::EHUICancelGlobalTextView(0);
}

CHostBridgedImeFEP::CHostBridgedImeFEP(CCoeEnv &aConeEnvironment)
    : CCoeFep(aConeEnvironment)
    , iDialogPending(EFalse)
    , iImeDialog(this) {
    
}

CHostBridgedImeFEP::~CHostBridgedImeFEP() {
}

void CHostBridgedImeFEP::ConstructL(const CCoeFepParameters &aParameters) {
    BaseConstructL(aParameters);
}

void CHostBridgedImeFEP::HandleChangeInFocus() {
    TRAPD(err, HandleChangeInFocusL());
    if (err != KErrNone) {
        LogOut(KAvkonFepCat, _L("Handle change in focus leaved with error code %d"), err);
    }
}

void CHostBridgedImeFEP::OpenDialogInputL() {
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();
    
    HBufC *initialText = NULL;
    TPtr initialTextPtr(0, 0);

    if (editor->DocumentLengthForFep()) {
        // Spawn text dialog from host and listen for completion
        initialText = HBufC::NewLC(editor->DocumentLengthForFep());
        initialTextPtr.Set(initialText->Des());
        initialTextPtr.SetLength(editor->DocumentLengthForFep());

        editor->GetEditorContentForFep(initialTextPtr, 0, editor->DocumentLengthForFep());
    }

    TCursorSelection selection(0, editor->DocumentLengthForFep());

    editor->SetCursorSelectionForFepL(selection);
    editor->StartFepInlineEditL(_L(""), 0, ETrue, NULL, *this, *this);
    //LogOut(KAvkonFepCat, _L("Maximum length of document is %d"), editor->DocumentMaximumLengthForFep());

    // Start the request
    if (iImeDialog.Open(&initialTextPtr, editor->DocumentMaximumLengthForFep()) != KErrNone) {
       LogOut(KAvkonFepCat, _L("Text view is already owned (this should already been canceled in losing foreground)!"));
       User::Leave(KErrInUse);
    }
    
    if (initialText) {
        CleanupStack::PopAndDestroy();
    }
}

void CHostBridgedImeFEP::HandleChangeInFocusL() {
    CCoeEnv* coeEnv = CCoeEnv::Static();
    iInputCapabilities = static_cast<const CCoeAppUi*>(coeEnv->AppUi())->InputCapabilities();
 
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();

    if (editor && !iImeDialog.IsActive() && !iDialogPending) {
        OpenDialogInputL();
        iDialogPending = ETrue;
    } else if (iDialogPending && !editor) {
        // Cancel the thing
        iImeDialog.Cancel();
        iDialogPending = EFalse;
    }
}

void CHostBridgedImeFEP::CommitChangedTextL() {
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();
    if (!editor) {
        LogOut(KAvkonFepCat, _L("Trying to commit while FEP Aware editor is not available (impossible)!"));
        return;
    }
    
    TInt length = 0;
    ::EHUIGetStoredText(0, &length, NULL);

    TPtr textChanged(0, 0);
    HBufC *textChangedBuf = NULL;
    
    if (length != 0) {
        textChangedBuf = HBufC::NewLC(length);
        textChanged.Set(textChangedBuf->Des());
        textChanged.SetLength(length);
        
        ::EHUIGetStoredText(0, &length, textChangedBuf->Ptr());
    }

    editor->UpdateFepInlineTextL(textChanged, 0);
    editor->CommitFepInlineEditL(*CCoeEnv::Static());

    TInt endPosition = editor->DocumentLengthForFep();
    TCursorSelection selection(endPosition, endPosition);
    editor->SetCursorSelectionForFepL(selection);

    if (textChangedBuf) {
        CleanupStack::PopAndDestroy();
    }

    // Dialog pending state should not be changed
}

void CHostBridgedImeFEP::HandleGainingForeground() {
    // Reclaim dialog if still pending while exiting foreground
    if (iDialogPending) {
        TRAPD(err, OpenDialogInputL());
        if (err != KErrNone) {
            LogOut(KAvkonFepCat, _L("Text view is already owned (reclaiming foreground error)!"));
        }
    }
}

void CHostBridgedImeFEP::HandleLosingForeground() {
    if (iDialogPending) {
        iImeDialog.Cancel();
        TRAP_IGNORE(CommitChangedTextL());
        
        iDialogPending = ETrue;
    }
}

void CHostBridgedImeFEP::HandleDestructionOfFocusedItem() {
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();
    if (!editor) {
        if (iDialogPending) {
            iImeDialog.Cancel();
        }

        iDialogPending = EFalse;
    }
}

void CHostBridgedImeFEP::CancelTransaction() {
    if (iDialogPending) {
        iImeDialog.Cancel();
        iDialogPending = EFalse;
    }
}

void CHostBridgedImeFEP::IsOnHasChangedState() {
    
}

void CHostBridgedImeFEP::OfferKeyEventL(TEventResponse& aEventResponse, const TKeyEvent& aKeyEvent, TEventCode aEventCode) {
    // Ignore
}

void CHostBridgedImeFEP::OfferPointerEventL(TEventResponse& aEventResponse, const TPointerEvent& aPointerEvent, const CCoeControl* aWindowOwningControl) {
    // Ignore
}

void CHostBridgedImeFEP::OfferPointerBufferReadyEventL(TEventResponse& aEventResponse, const CCoeControl* aWindowOwningControl) {
    // Ignore
}

TInt CHostBridgedImeFEP::NumberOfAttributes() const {
    return 0;
}

TUid CHostBridgedImeFEP::AttributeAtIndex(TInt aIndex) const {
    return TUid::Null();
}

void CHostBridgedImeFEP::WriteAttributeDataToStreamL(TUid aAttributeUid, RWriteStream& aStream) const {
    // Ignore
}

void CHostBridgedImeFEP::ReadAttributeDataFromStreamL(TUid aAttributeUid, RReadStream& aStream) {
    // Ignore
}

void CHostBridgedImeFEP::GetFormatOfFepInlineText(TCharFormat& aFormat, TInt& aNumberOfCharactersWithSameFormat, 
    TInt aPositionOfCharacter) const {
    // Ignore
}

void CHostBridgedImeFEP::HandlePointerEventInInlineTextL(TPointerEvent::TType aType, TUint aModifiers, 
    TInt aPositionInInlineText) {
    // Ignore
}

CHostBridgedImeFEPPlugin *CHostBridgedImeFEPPlugin::NewL() {
    return new (ELeave) CHostBridgedImeFEPPlugin();
}

CCoeFep* CHostBridgedImeFEPPlugin::NewFepL(CCoeEnv& aConeEnvironment, const CCoeFepParameters& aFepParameters) {
    CHostBridgedImeFEP *fep = new (ELeave) CHostBridgedImeFEP(aConeEnvironment);
    CleanupStack::PushL(fep);
    fep->ConstructL(aFepParameters);
    CleanupStack::Pop(fep);
    
    return fep;
}

void CHostBridgedImeFEPPlugin::SynchronouslyExecuteSettingsDialogL(CCoeEnv& aConeEnvironment) {
    
}

const TImplementationProxy ImplementationTable[] = {
    IMPLEMENTATION_PROXY_ENTRY(0x101FD65A, CHostBridgedImeFEPPlugin::NewL)
};

EXPORT_C const TImplementationProxy* ImplementationGroupProxy(TInt& aTableCount) {
    aTableCount = sizeof(ImplementationTable) / sizeof(TImplementationProxy);
    return ImplementationTable;
}

