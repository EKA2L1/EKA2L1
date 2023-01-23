/*
 * Copyright (c) 2022 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <HostBridgedImeFEP.h>
#include <aknappui.h>
#include <aknedsts.h>
#include <Dispatch.h>
#include <Log.h>

CHostDialogIme::CHostDialogIme(CHostBridgedImeFEP *aHost)
    : CActive(CActive::EPriorityHigh)
    , iHost(aHost) {
    CActiveScheduler::Add(this);
}

CHostDialogIme::~CHostDialogIme() {
    Deque();
}

TInt CHostDialogIme::Open(const TDesC *aInitialText, const TInt aMaxLength) {
    iStatus = KRequestPending;
    TInt result = ::EHUIOpenGlobalTextView(0, aInitialText, aMaxLength, &iStatus);
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
    , iHasFep(EFalse)
    , iInFEPWork(EFalse)
    , iImeDialog(this) {
    
}

CHostBridgedImeFEP::~CHostBridgedImeFEP() {
    if (iDialogPending) {
        TRAP_IGNORE(CancelDialogL());
    }

    if (EHUIIsKeypadBased(0)) {
        UnregisterObserver();
        if (EditorState()) {
            EditorState()->SetObserver(NULL);
        }
    }
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

void CHostBridgedImeFEP::OpenDialogInputL(const TBool aTranstractRestart) {
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();
    
    HBufC *initialText = NULL;
    TPtr initialTextPtr(0, 0);

    iInFEPWork = ETrue;

    if (editor->DocumentLengthForFep()) {
        // Spawn text dialog from host and listen for completion
        initialText = HBufC::NewLC(editor->DocumentLengthForFep());
        initialTextPtr.Set(initialText->Des());
        initialTextPtr.SetLength(editor->DocumentLengthForFep());

        editor->GetEditorContentForFep(initialTextPtr, 0, editor->DocumentLengthForFep());
    }

    TCursorSelection selection(0, editor->DocumentLengthForFep());
    editor->SetCursorSelectionForFepL(selection);
    editor->StartFepInlineEditL(initialTextPtr, initialTextPtr.Length(), ETrue, NULL, *this, *this);

    // Start the request
    if (iImeDialog.Open(&initialTextPtr, editor->DocumentMaximumLengthForFep()) != KErrNone) {
        LogOut(KAvkonFepCat, _L("Text view is already owned (this should already been canceled in losing foreground)!"));
        User::Leave(KErrInUse);
    }

    iInFEPWork = EFalse;
    
    if (initialText) {
        CleanupStack::PopAndDestroy();
    }
}

CAknEdwinState* CHostBridgedImeFEP::EditorState() const {
    MCoeFepAwareTextEditor *fepAvareTextEditor = iInputCapabilities.FepAwareTextEditor();

    if (fepAvareTextEditor && fepAvareTextEditor->Extension1()) {
        return static_cast<CAknEdwinState*>(fepAvareTextEditor->Extension1()->State(KNullUid));
    }

    return NULL;
}

void CHostBridgedImeFEP::HandleInputCapabilitiesEventL( TInt aEvent, TAny* aParams ) {
    switch (aEvent) {
    case CAknExtendedInputCapabilities::MAknEventObserver::EActivatePenInputRequest:
        if (!iDialogPending) {
            OpenDialogInputL();
            iDialogPending = ETrue;
        }

        break;

    default:
        break;
    }
}

void CHostBridgedImeFEP::HandleAknEdwinStateEventL(CAknEdwinState* aAknEdwinState, EAknEdwinStateEvent aEventType) {
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();
    if (editor) {
        switch (aEventType) {
        case EAknActivatePenInputRequest:
            if (!iDialogPending) {
                OpenDialogInputL();
                iDialogPending = ETrue;
            }

            break;

        // Close one, for S^3
        case EAknActivatePenInputRequest + 3:
            CancelDialogL();
            break;

        default:
            break;
        }
    }
}

void CHostBridgedImeFEP::RegisterObserver() {
    MObjectProvider* mop = iInputCapabilities.ObjectProvider();

    if (mop) {
        CAknExtendedInputCapabilities* extendedInputCapabilities = mop->MopGetObject( extendedInputCapabilities );

        if (extendedInputCapabilities) {
            extendedInputCapabilities->RegisterObserver( this );
        }
    }
}

void CHostBridgedImeFEP::UnregisterObserver() {
    MObjectProvider* mop = iInputCapabilities.ObjectProvider();

    if (mop) {
        CAknExtendedInputCapabilities* extendedInputCapabilities = mop->MopGetObject( extendedInputCapabilities );

        if (extendedInputCapabilities) {
            extendedInputCapabilities->UnregisterObserver( this );
        }
    }
}

void CHostBridgedImeFEP::HandleChangeInFocusL() {
    CCoeEnv* coeEnv = CCoeEnv::Static();
    iInputCapabilities = static_cast<const CCoeAppUi*>(coeEnv->AppUi())->InputCapabilities();
 
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();

    if (editor && !iHasFep) {
        if (EHUIIsKeypadBased(0)) {
            if (!iDialogPending) {
                OpenDialogInputL();
                iDialogPending = ETrue;
            }
        } else {
            RegisterObserver();

            if (EditorState()) {
                EditorState()->SetObserver(this);
            } else {
                EditorState()->SetObserver(NULL);
            }
        }

        iHasFep = ETrue;
    } else {
        if (!EHUIIsKeypadBased(0)) {
            UnregisterObserver();
        }

        CancelDialogL();
        iHasFep = EFalse;
    }
}

void CHostBridgedImeFEP::CommitChangedTextL() {
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();
    if (!editor) {
        LogOut(KAvkonFepCat, _L("Trying to commit while FEP Aware editor is not available (impossible)!"));
        return;
    }

    if (!iDialogPending) {
        return;
    }

    iDialogPending = EFalse;
    
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
        CancelDialogL();
        iDialogPending = ETrue;
    }
}

void CHostBridgedImeFEP::HandleDestructionOfFocusedItem() {
    MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();
    if (!editor) {
        TRAP_IGNORE(CancelDialogL());
    }
}

void CHostBridgedImeFEP::CancelDialogL() {
    if (iDialogPending) {
        MCoeFepAwareTextEditor *editor = iInputCapabilities.FepAwareTextEditor();

        if (editor) {
            editor->CancelFepInlineEdit();
        }

        iImeDialog.Cancel();
        iDialogPending = EFalse;
    }
}

void CHostBridgedImeFEP::CancelTransaction() {
    if (!iInFEPWork) {
        CancelDialogL();
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

