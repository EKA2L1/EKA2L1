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

#ifndef AVKONFEP_IME_FEP_H_
#define AVKONFEP_IME_FEP_H_

#include <coeinput.h>
#include <fepbase.h>
#include <fepplugin.h>
#include <fepitfr.h>
#include <ecom/ecom.h>
#include <ecom/ImplementationProxy.h>
#include <aknedstsobs.h>
#include <aknextendedinputcapabilities.h>

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

class CHostBridgedImeFEP :
    public CCoeFep,
    public MFepInlineTextFormatRetriever,
    public MFepPointerEventHandlerDuringInlineEdit,
    public CAknExtendedInputCapabilities::MAknEventObserver,
    private MAknEdStateObserver {
private:
    virtual void HandleGainingForeground();
    virtual void HandleLosingForeground();
    virtual void HandleChangeInFocus();
    virtual void HandleDestructionOfFocusedItem();
    
    void HandleChangeInFocusL();
    void OpenDialogInputL(const TBool aTranstractRestart = EFalse);

    void RegisterObserver();
    void UnregisterObserver();

    void HandleInputCapabilitiesEventL( TInt aEvent, TAny* aParams );
    void HandleAknEdwinStateEventL(CAknEdwinState* aAknEdwinState, EAknEdwinStateEvent aEventType);
    void CancelAndCommitDialogL();

    CAknEdwinState* EditorState() const;

    TCoeInputCapabilities iInputCapabilities;
    TBool iDialogPending;
    TBool iInRestart;
    TBool iHasFep;

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
