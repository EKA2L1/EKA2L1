/*
 * Copyright (c) 2026 EKA2L1 Team.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <coeaui.h>
#include <coecntrl.h>
#include <coeinput.h>
#include <fepbase.h>
#include <fepitfr.h>
#include <txtfrmat.h>
#include <frmtlay.h>
#include "Dispatch.h"

class CHostFep;

class CHostFepControl : public CCoeControl {
public:
    CHostFepControl(CHostFep &aFep) : iFep(aFep) {}
    TKeyResponse OfferKeyEventL(const TKeyEvent &aKey, TEventCode aType);
private:
    CHostFep &iFep;
};

class CHostFepDialog : public CActive {
public:
    CHostFepDialog(CHostFep &aFep);
    ~CHostFepDialog() { Cancel(); }
    void OpenL(const TDesC &aText, TInt aMaximumLength) {
        iStatus = KRequestPending;
        const TInt error = EHUIOpenGlobalTextView(0, &aText, aMaximumLength, &iStatus);
        if (error != KErrNone && iStatus != KRequestPending) {
            // A host-open failure may already have completed the request before SetActive.
            User::WaitForRequest(iStatus);
        }
        User::LeaveIfError(error);
        SetActive();
    }
private:
    void RunL();
    TInt RunError(TInt aError);
    void DoCancel() { EHUICancelGlobalTextView(0); }
    CHostFep &iFep;
};

class CHostFep : public CCoeFep, public MFepInlineTextFormatRetriever,
    public MFepPointerEventHandlerDuringInlineEdit {
public:
    CHostFep(CCoeEnv &aEnv)
        : CCoeFep(aEnv), iCapabilities(TCoeInputCapabilities::ENone), iDialog(*this),
          iControl(NULL), iForeground(ETrue), iInlineEdit(EFalse),
          iChangingText(EFalse) {}

    ~CHostFep() {
        CancelTransaction();
        EHUISetInputAvailable(0, EFalse);
        if (iControl) {
            static_cast<CCoeAppUi *>(CCoeEnv::Static()->AppUi())->RemoveFromStack(iControl);
            delete iControl;
        }
    }

    void ConstructL(const CCoeFepParameters &aParameters) {
        BaseConstructL(aParameters);
        if (EHUIIsManualInput(0)) {
            iControl = new(ELeave) CHostFepControl(*this);
            static_cast<CCoeAppUi *>(CCoeEnv::Static()->AppUi())->AddToStackL(iControl,
                ECoeStackPriorityFep, ECoeStackFlagRefusesFocus | ECoeStackFlagSharable);
        }
    }

    void CommitL() {
        MCoeFepAwareTextEditor *editor = iCapabilities.FepAwareTextEditor();
        if (!editor || !iInlineEdit) {
            return;
        }
        TInt length = 0;
        EHUIGetStoredText(0, &length, NULL);
        HBufC *text = HBufC::NewLC(length);
        TPtr value(text->Des());
        value.SetLength(length);
        EHUIGetStoredText(0, &length, value.Ptr());
        iChangingText = ETrue;
        TRAPD(error, editor->UpdateFepInlineTextL(value, value.Length());
            editor->CommitFepInlineEditL(*CCoeEnv::Static()));
        iChangingText = EFalse;
        CleanupStack::PopAndDestroy(text);
        if (error != KErrNone) {
            CancelTransaction();
            User::Leave(error);
        }
        iInlineEdit = EFalse;
        const TInt end = editor->DocumentLengthForFep();
        editor->SetCursorSelectionForFepL(TCursorSelection(end, end));
        UpdateAvailability();
    }

private:
    friend class CHostFepControl;
    friend class CHostFepDialog;
    void OpenL() {
        MCoeFepAwareTextEditor *editor = iCapabilities.FepAwareTextEditor();
        if (!iForeground || !editor || iDialog.IsActive() || iInlineEdit) {
            return;
        }
        HBufC *text = HBufC::NewLC(editor->DocumentLengthForFep());
        TPtr value(text->Des());
        editor->GetEditorContentForFep(value, 0, editor->DocumentLengthForFep());
        iChangingText = ETrue;
        TRAPD(error,
            editor->SetCursorSelectionForFepL(TCursorSelection(0, value.Length()));
            editor->StartFepInlineEditL(value, value.Length(), ETrue, NULL, *this, *this));
        iChangingText = EFalse;
        User::LeaveIfError(error);
        iInlineEdit = ETrue;
        TRAP(error, iDialog.OpenL(value, editor->DocumentMaximumLengthForFep()));
        if (error != KErrNone) {
            CancelTransaction();
        }
        CleanupStack::PopAndDestroy(text);
        User::LeaveIfError(error);
    }

    void UpdateAvailability() {
        MCoeFepAwareTextEditor *editor = iCapabilities.FepAwareTextEditor();
        EHUISetInputAvailable(0, iForeground && editor && !iCapabilities.IsNone()
            && editor->DocumentMaximumLengthForFep() > 0);
    }

    void HandleChangeInFocus() {
        const TCoeInputCapabilities capabilities =
            static_cast<CCoeAppUi *>(CCoeEnv::Static()->AppUi())->InputCapabilities();
        if (capabilities != iCapabilities) {
            CancelTransaction();
            iCapabilities = capabilities;
        }
        UpdateAvailability();
        if (!EHUIIsManualInput(0) && iForeground && iCapabilities.FepAwareTextEditor()) {
            TRAP_IGNORE(OpenL());
        }
    }

    void HandleDestructionOfFocusedItem() {
        const TCoeInputCapabilities capabilities =
            static_cast<CCoeAppUi *>(CCoeEnv::Static()->AppUi())->InputCapabilities();
        if (capabilities != iCapabilities) {
            // The old editor can already be destroyed when CONE sends this notification.
            iDialog.Cancel();
            iInlineEdit = EFalse;
            iCapabilities = capabilities;
        }
        UpdateAvailability();
    }

    void HandleGainingForeground() {
        iForeground = ETrue;
        HandleChangeInFocus();
    }

    void HandleLosingForeground() {
        iForeground = EFalse;
        CancelTransaction();
        EHUISetInputAvailable(0, EFalse);
    }

    void CancelTransaction() {
        if (iChangingText) {
            return;
        }
        iDialog.Cancel();
        if (iInlineEdit) {
            iInlineEdit = EFalse;
            MCoeFepAwareTextEditor *editor = iCapabilities.FepAwareTextEditor();
            if (editor) {
                editor->CancelFepInlineEdit();
            }
        }
    }

    void OfferKeyEventL(TEventResponse &aResponse, const TKeyEvent &, TEventCode) {
        aResponse = EEventWasNotConsumed;
    }

    void OfferPointerEventL(TEventResponse &aResponse, const TPointerEvent &, const CCoeControl *) {
        aResponse = EEventWasNotConsumed;
    }
    void OfferPointerBufferReadyEventL(TEventResponse &aResponse, const CCoeControl *) {
        aResponse = EEventWasNotConsumed;
    }
    void IsOnHasChangedState() {}
    TInt NumberOfAttributes() const { return 0; }
    TUid AttributeAtIndex(TInt) const { return KNullUid; }
    void WriteAttributeDataToStreamL(TUid, RWriteStream &) const {}
    void ReadAttributeDataFromStreamL(TUid, RReadStream &) {}
    void GetFormatOfFepInlineText(TCharFormat &aFormat, TInt &aLength, TInt) const {
        aFormat = TCharFormat();
        aLength = KMaxTInt;
    }
    void HandlePointerEventInInlineTextL(TPointerEvent::TType, TUint, TInt) {}

    TCoeInputCapabilities iCapabilities;
    CHostFepDialog iDialog;
    CHostFepControl *iControl;
    TBool iForeground;
    TBool iInlineEdit;
    TBool iChangingText;
};

TKeyResponse CHostFepControl::OfferKeyEventL(const TKeyEvent &aKey, TEventCode aType) {
    if (aKey.iScanCode != EStdKeyF20) {
        return EKeyWasNotConsumed;
    }
    if (aType == EEventKey) {
        iFep.OpenL();
    }
    return EKeyWasConsumed;
}

CHostFepDialog::CHostFepDialog(CHostFep &aFep) : CActive(EPriorityHigh), iFep(aFep) {
    CActiveScheduler::Add(this);
}

void CHostFepDialog::RunL() {
    if (iStatus == KErrNone) {
        iFep.CommitL();
    } else {
        iFep.CancelTransaction();
    }
}

TInt CHostFepDialog::RunError(TInt aError) {
    iFep.CancelTransaction();
    return aError;
}

EXPORT_C CCoeFep *NewFepL(CCoeEnv &aEnv, const TDesC &, const CCoeFepParameters &aParameters) {
    CHostFep *fep = new(ELeave) CHostFep(aEnv);
    CleanupStack::PushL(fep);
    fep->ConstructL(aParameters);
    CleanupStack::Pop(fep);
    return fep;
}

EXPORT_C void SynchronouslyExecuteSettingsDialogL(CCoeEnv &, const TDesC &) {}

GLDEF_C TInt E32Dll(TDllReason) { return KErrNone; }
