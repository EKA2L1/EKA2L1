#include <VideoPlayerBody.h>
#include <dispatch.h>
#include <Log.h>

CVideoPlayerFeedbackHandler::CVideoPlayerFeedbackHandler(MVideoPlayerUtilityObserver &aObserver)
    : iObserver(aObserver)
    , iCurrentState(EVideoPlayerStateIdle) {
    
}

void CVideoPlayerFeedbackHandler::OpenComplete(const TInt aError) {
    if (aError == KErrNone) {
        iCurrentState = EVideoPlayerStateOpened;
    }

    iObserver.MvpuoOpenComplete(aError);
}

void CVideoPlayerFeedbackHandler::PrepareComplete(const TInt aError) {
    if (aError == KErrNone) {
        iCurrentState = EVideoPlayerStatePrepared;
    }

    iObserver.MvpuoPrepareComplete(aError);
}

void CVideoPlayerFeedbackHandler::PlayComplete(const TInt aError) {
    iCurrentState = EVideoPlayerStatePrepared;      // Whatever error it's, gonna stop anyway.
    iObserver.MvpuoPlayComplete(aError);
}

void CVideoPlayerFeedbackHandler::Pause() {
    iCurrentState = EVideoPlayerStatePaused;
}

CVideoPlayerUtility::CBody::CBody(MVideoPlayerUtilityObserver &aObserver)
        : CActive(CActive::EPriorityStandard)
        , iFeedbackHandler(aObserver)
        , iWindow(NULL)
        , iVideoFps(-1.0f)
        , iVideoBitRate(-1)
        , iAudioBitRate(-1)
        , iCurrentVolume(-1)
        , iCurrentRotation(EVideoRotationNone)
        , iCompleteIdle(NULL) {
}

CVideoPlayerUtility::CBody::~CBody() {
    iCompleteIdle->Deque();
    Deque();
    
    delete iCompleteIdle;
    
    EVideoPlayerDestroy(0, iDispatchInstance);
}

void CVideoPlayerUtility::CBody::ConstructL(RWsSession &aWsSession, RWindowBase &aWindow, const TRect &aClipRect) {
    iDispatchInstance = EVideoPlayerCreate(0);

    User::LeaveIfNull(iDispatchInstance);

    SetOwnedWindowL(aWsSession, aWindow);
    SetDisplayRectL(aClipRect);
    
    iCompleteIdle = CIdle::NewL(CActive::EPriorityHigh);
    CActiveScheduler::Add(this);
}

void CVideoPlayerUtility::CBody::SetOwnedWindowL(RWsSession &aSession, RWindowBase &aWindow) {
    iWindow = &aWindow;
    User::LeaveIfError(EVideoPlayerSetOwnedWindow(0, iDispatchInstance, aSession.Handle(), aWindow.WsHandle()));
}

void CVideoPlayerUtility::CBody::SetDisplayRectL(const TRect &aClipRect) {
    iDisplayRect = aClipRect;
    User::LeaveIfError(EVideoPlayerSetClipRect(0, iDispatchInstance, &iDisplayRect));
}

TInt McvPlayerOpenCompleteIdleCallback(TAny *aData) {
    CVideoPlayerFeedbackHandler *util = reinterpret_cast<CVideoPlayerFeedbackHandler*>(aData);
    util->OpenComplete(KErrNone);

    return KErrNone;
}

TInt McvPlayerPrepareCompleteIdleCallback(TAny *aData) {
    CVideoPlayerFeedbackHandler *util = reinterpret_cast<CVideoPlayerFeedbackHandler*>(aData);
    util->PrepareComplete(KErrNone);

    return KErrNone;
}

void CVideoPlayerUtility::CBody::OpenFileL(const TDesC &aPath) {
    if ((iFeedbackHandler.CurrentState() == EVideoPlayerStatePlaying) || (iFeedbackHandler.CurrentState() == EVideoPlayerStatePaused)) {
        LogOut(KMcvCat, _L("Open a new video player when the video player is not stopped yet is not valid!"));
        User::Leave(KErrInUse);
        
        return;
    }

    if (iFeedbackHandler.CurrentState() != EVideoPlayerStateIdle) {
        Close();
    }

    TInt result = EVideoPlayerOpenUrl(0, iDispatchInstance, &aPath);
    if ((result == KErrNotSupported) || (result == KErrNotFound)) {
        iFeedbackHandler.OpenComplete(result);
        return;
    }
    
    if (result == KErrNone) {
        iVideoBitRate = -1;
        iAudioBitRate = -1;
        iVideoFps = -1.0f;

        iCompleteIdle->Start(TCallBack(McvPlayerOpenCompleteIdleCallback, &iFeedbackHandler));
        return;
    }
    
    // Who are you? I don't know you!!!!
    User::Leave(result);
}

void CVideoPlayerUtility::CBody::OpenDesL(const TDesC8 &aContent) {
    if ((iFeedbackHandler.CurrentState() == EVideoPlayerStatePlaying) || (iFeedbackHandler.CurrentState() == EVideoPlayerStatePaused)) {
        LogOut(KMcvCat, _L("Open a new video player when the video player is not stopped yet is not valid!"));
        User::Leave(KErrInUse);
        
        return;
    }

    TInt result = EVideoPlayerOpenDes(0, iDispatchInstance, &aContent);
    if ((result == KErrNotSupported) || (result == KErrNotFound)) {
        iFeedbackHandler.OpenComplete(result);
        return;
    }
    
    if (result == KErrNone) {
        iCompleteIdle->Start(TCallBack(McvPlayerOpenCompleteIdleCallback, &iFeedbackHandler));
        return;
    }
    
    // Who are you? I don't know you!!!!
    User::Leave(result);
}

void CVideoPlayerUtility::CBody::Prepare() {
    if (iFeedbackHandler.CurrentState() >= EVideoPlayerStatePrepared) {
        // No need ;)
        return;
    }

    if (iFeedbackHandler.CurrentState() == EVideoPlayerStateIdle) {
        LogOut(KMcvCat, _L("Prepare a new video player with no content opening (warning)!"));
    }

    iCompleteIdle->Start(TCallBack(McvPlayerPrepareCompleteIdleCallback, &iFeedbackHandler));
}

void CVideoPlayerUtility::CBody::Play(const TTimeIntervalMicroSeconds *aInterval) {
    const TVideoPlayerState state = iFeedbackHandler.CurrentState();
    if (state >= EVideoPlayerStatePrepared) {
        if (state == EVideoPlayerStatePlaying) {
            return;
        }

        iStatus = KRequestPending;
        
        EVideoPlayerSetPlayDoneNotification(0, iDispatchInstance, &iStatus);
        SetActive();

        EVideoPlayerPlay(0, iDispatchInstance, aInterval);
    } else {
        LogOut(KMcvCat, _L("Requesting to play when no video has been opened to the video player yet!"));
        return;
    }
}

void CVideoPlayerUtility::CBody::Pause() {
    if (iFeedbackHandler.CurrentState() != EVideoPlayerStatePlaying) {
        if (iFeedbackHandler.CurrentState() <= EVideoPlayerStatePrepared) {
            LogOut(KMcvCat, _L("Requesting to pause when video player is not playing anything!"));
        }
        return;
    }

    EVideoPlayerPause(0, iDispatchInstance);
    iFeedbackHandler.Pause();
}

void CVideoPlayerUtility::CBody::Stop() {
    if (iFeedbackHandler.CurrentState() <= EVideoPlayerStatePrepared) {
        return;
    }

    EVideoPlayerStop(0, iDispatchInstance);
}

void CVideoPlayerUtility::CBody::Close() {
    if (iFeedbackHandler.CurrentState() == EVideoPlayerStateIdle) {
        return;
    }
    
    EVideoPlayerClose(0, iDispatchInstance);
}

void CVideoPlayerUtility::CBody::SetPositionL(const TTimeIntervalMicroSeconds &aWhere) {
    if (iFeedbackHandler.CurrentState() == EVideoPlayerStatePlaying) {
        LogOut(KMcvCat, _L("Attempt to set video's time position when the video is still playing!"));
        User::Leave(KErrInUse);
    }
    
    User::LeaveIfError(EVideoPlayerSetPosition(0, iDispatchInstance, &aWhere));
}

TTimeIntervalMicroSeconds CVideoPlayerUtility::CBody::PositionL() const {
    TTimeIntervalMicroSeconds position;
    User::LeaveIfError(EVideoPlayerGetPosition(0, iDispatchInstance, &position));
    
    return position;
}

void CVideoPlayerUtility::CBody::SetVolumeL(TInt aVolume) {
    User::LeaveIfError(EVideoPlayerSetVolume(0, iDispatchInstance, aVolume));
    iCurrentVolume = aVolume;
}

TInt CVideoPlayerUtility::CBody::Volume() {
    if (iCurrentVolume == -1) {
        iCurrentVolume = EVideoPlayerCurrentVolume(0, iDispatchInstance);
    }
    
    return iCurrentVolume;
}

TInt CVideoPlayerUtility::CBody::MaxVolume() const {
    return EVideoPlayerMaxVolume(0, iDispatchInstance);
}

void CVideoPlayerUtility::CBody::SetFpsL(const TReal32 aFps) {
    User::LeaveIfError(EVideoPlayerSetVideoFps(0, iDispatchInstance, &aFps));
    iVideoFps = aFps;
}

TReal32 CVideoPlayerUtility::CBody::Fps() {
    if (iVideoFps < 0) {
        EVideoPlayerGetVideoFps(0, iDispatchInstance, &iVideoFps);
    }
    
    return iVideoFps;
}

TTimeIntervalMicroSeconds CVideoPlayerUtility::CBody::DurationL() const {
    TTimeIntervalMicroSeconds duration;
    User::LeaveIfError(EVideoPlayerGetDuration(0, iDispatchInstance, &duration));
    
    return duration;
}

TInt CVideoPlayerUtility::CBody::VideoBitRate() {
    if (iVideoBitRate < 0) {
        iVideoBitRate = EVideoPlayerGetBitrate(0, iDispatchInstance, EFalse);
    }
    
    return iVideoBitRate;
}

TInt CVideoPlayerUtility::CBody::AudioBitRate() {
    if (iAudioBitRate < 0) {
        iAudioBitRate = EVideoPlayerGetBitrate(0, iDispatchInstance, ETrue);
    }
    
    return iAudioBitRate;
}

void CVideoPlayerUtility::CBody::SetRotationL(TVideoRotation aRotation) {
    User::LeaveIfError(EVideoPlayerSetRotation(0, iDispatchInstance, (TInt)aRotation));
    iCurrentRotation = aRotation;
}

TVideoRotation CVideoPlayerUtility::CBody::Rotation() const {
    return iCurrentRotation;
}

void CVideoPlayerUtility::CBody::RunL() {
    iFeedbackHandler.PlayComplete(iStatus.Int());
}

void CVideoPlayerUtility::CBody::DoCancel() {
    EVideoPlayerCancelPlayDoneNotification(0, iDispatchInstance);
}

CVideoPlayerUtility::CBody *CVideoPlayerUtility::CBody::NewL(MVideoPlayerUtilityObserver &aObserver, RWsSession &aWsSession, RWindowBase &aWindow, const TRect &aClipRect) {
    CVideoPlayerUtility::CBody *self = new (ELeave) CVideoPlayerUtility::CBody(aObserver);

    CleanupStack::PushL(self);
    self->ConstructL(aWsSession, aWindow, aClipRect);
    CleanupStack::Pop();
    
    return self;
}

EXPORT_C CVideoPlayerUtility* CVideoPlayerUtility::NewL(MVideoPlayerUtilityObserver& aObserver,
                                          TInt aPriority,
                                          TMdaPriorityPreference aPref,
                                          RWsSession& aWs,
                                          CWsScreenDevice& aScreenDevice,
                                          RWindowBase& aWindow,
                                          const TRect& aScreenRect,
                                          const TRect& aClipRect) {
    CVideoPlayerUtility *self = new (ELeave) CVideoPlayerUtility;
    CleanupStack::PushL(self);

    self->iBody = CVideoPlayerUtility::CBody::NewL(aObserver, aWs, aWindow, aClipRect);
    CleanupStack::Pop();
    
    return self;
}

EXPORT_C void CVideoPlayerUtility::OpenFileL(const TDesC& aFileName,TUid aControllerUid) {
    iBody->OpenFileL(aFileName);
}

EXPORT_C void CVideoPlayerUtility::OpenFileL(const RFile& aFileName, TUid aControllerUid) {
    TBufC<512> nameFull;
    TDes nameFullDesc = nameFull.Des();
    aFileName.FullName(nameFullDesc);
    
    iBody->OpenFileL(nameFull);
}

EXPORT_C void CVideoPlayerUtility::OpenFileL(const TMMSource& aSource, TUid aControllerUid) {
    LogOut(KMcvCat, _L("Video Player's open file through MMSource is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::OpenDesL(const TDesC8& aDescriptor,TUid aControllerUid) {
    iBody->OpenDesL(aDescriptor);
}

EXPORT_C void CVideoPlayerUtility::OpenUrlL(const TDesC& aUrl, TInt aIapId, const TDesC8& aMimeType, TUid aControllerUid) {
    LogOut(KMcvCat, _L("Video Player's open URL is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::Prepare() {
    iBody->Prepare();
}

EXPORT_C void CVideoPlayerUtility::Close() {
    iBody->Close();
}

EXPORT_C void CVideoPlayerUtility::Play() {
    iBody->Play(NULL);
}

EXPORT_C void CVideoPlayerUtility::Play(const TTimeIntervalMicroSeconds& aStartPoint, const TTimeIntervalMicroSeconds& aEndPoint) {
    TTimeIntervalMicroSeconds range[2];
    range[0] = aStartPoint;
    range[1] = aEndPoint;
    
    iBody->Play(range);
}

EXPORT_C TInt CVideoPlayerUtility::Stop() {
    iBody->Stop();
}

EXPORT_C void CVideoPlayerUtility::PauseL() {
    iBody->Pause();
}

EXPORT_C void CVideoPlayerUtility::SetPriorityL(TInt aPriority, TMdaPriorityPreference aPref) {
    LogOut(KMcvCat, _L("Video Player's set priority is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::PriorityL(TInt& aPriority, TMdaPriorityPreference& aPref) const {
    LogOut(KMcvCat, _L("Video Player's get priority is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::SetDisplayWindowL(RWsSession& aWs,CWsScreenDevice& aScreenDevice,RWindowBase& aWindow,const TRect& aWindowRect,const TRect& aClipRect) {
    iBody->SetOwnedWindowL(aWs, aWindow);
}

EXPORT_C void CVideoPlayerUtility::RegisterForVideoLoadingNotification(MVideoLoadingObserver& aCallback) {
    LogOut(KMcvCat, _L("Video Player's register video loading notification is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::GetVideoLoadingProgressL(TInt& aPercentageComplete) {
    LogOut(KMcvCat, _L("Video Player's get vido loading progress is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::GetFrameL(TDisplayMode aDisplayMode) {
    LogOut(KMcvCat, _L("Video Player's get frame is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::GetFrameL(TDisplayMode aDisplayMode, ContentAccess::TIntent aIntent) {
    LogOut(KMcvCat, _L("Video Player's get frame with access intent is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::RefreshFrameL() {
    LogOut(KMcvCat, _L("Video Player's refresh frame is not yet implemented!"));
}

EXPORT_C TReal32 CVideoPlayerUtility::VideoFrameRateL() const {
    return iBody->Fps();
}

EXPORT_C void CVideoPlayerUtility::SetVideoFrameRateL(TReal32 aFramesPerSecond) {
    iBody->SetFpsL(aFramesPerSecond);
}

EXPORT_C void CVideoPlayerUtility::VideoFrameSizeL(TSize& aSize) const {
    LogOut(KMcvCat, _L("Video Player's video frame size is not yet implemented!"));
}

EXPORT_C const TDesC8& CVideoPlayerUtility::VideoFormatMimeType() const {
    LogOut(KMcvCat, _L("Video Player's video format mime type is not yet implemented!"));
}

EXPORT_C TInt CVideoPlayerUtility::VideoBitRateL() const {
    return iBody->VideoBitRate();
}

EXPORT_C TInt CVideoPlayerUtility::AudioBitRateL() const {
    return iBody->AudioBitRate();
}

EXPORT_C TFourCC CVideoPlayerUtility::AudioTypeL() const {
    LogOut(KMcvCat, _L("Video Player's auto type is not yet implemented!"));
}

EXPORT_C TBool CVideoPlayerUtility::AudioEnabledL() const {
    LogOut(KMcvCat, _L("Video Player's audio enabled is stubbed!"));
    return ETrue;
}

EXPORT_C void CVideoPlayerUtility::SetPositionL(const TTimeIntervalMicroSeconds& aPosition) {
    iBody->SetPositionL(aPosition);
}

EXPORT_C TTimeIntervalMicroSeconds CVideoPlayerUtility::PositionL() const {
    return iBody->PositionL();
}

EXPORT_C TTimeIntervalMicroSeconds CVideoPlayerUtility::DurationL() const {
    return iBody->DurationL();
}

EXPORT_C void CVideoPlayerUtility::SetVolumeL(TInt aVolume) {
    iBody->SetVolumeL(aVolume);
}

EXPORT_C TInt CVideoPlayerUtility::Volume() const {
    return iBody->Volume();
}

EXPORT_C TInt CVideoPlayerUtility::MaxVolume() const {
    return iBody->MaxVolume();
}

EXPORT_C void CVideoPlayerUtility::SetBalanceL(TInt aBalance) {
    LogOut(KMcvCat, _L("Video Player's set balance is not yet implemented!"));
}

EXPORT_C TInt CVideoPlayerUtility::Balance()const {
    LogOut(KMcvCat, _L("Video Player's get balance is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::SetRotationL(TVideoRotation aRotation) {
    iBody->SetRotationL(aRotation);
}

EXPORT_C TVideoRotation CVideoPlayerUtility::RotationL() const {
    return iBody->Rotation();
}

EXPORT_C void CVideoPlayerUtility::SetScaleFactorL(TReal32 aWidthPercentage, TReal32 aHeightPercentage, TBool aAntiAliasFiltering) {
    LogOut(KMcvCat, _L("Video Player's set scale factor is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::GetScaleFactorL(TReal32& aWidthPercentage, TReal32& aHeightPercentage, TBool& aAntiAliasFiltering) const {
    LogOut(KMcvCat, _L("Video Player's get scale factor is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::SetCropRegionL(const TRect& aCropRegion) {
    iBody->SetDisplayRectL(aCropRegion);
}

EXPORT_C void CVideoPlayerUtility::GetCropRegionL(TRect& aCropRegion) const {
    iBody->GetDisplayRect(aCropRegion);
}

EXPORT_C TInt CVideoPlayerUtility::NumberOfMetaDataEntriesL() const {
    LogOut(KMcvCat, _L("Video Player's number of metadata entries is not yet implemented!"));
    return 0;
}

EXPORT_C CMMFMetaDataEntry* CVideoPlayerUtility::MetaDataEntryL(TInt aIndex) const {
    LogOut(KMcvCat, _L("Video Player's get metadata entry is not yet implemented!"));
    return NULL;
}

EXPORT_C const CMMFControllerImplementationInformation& CVideoPlayerUtility::ControllerImplementationInformationL() {
    LogOut(KMcvCat, _L("Video Player's controller implementation information is not yet implemented!"));
}

EXPORT_C TInt CVideoPlayerUtility::CustomCommandSync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TDes8& aDataFrom) {
    LogOut(KMcvCat, _L("Video Player's custom command sync v1 is not yet implemented!"));
    return 0;
}

EXPORT_C TInt CVideoPlayerUtility::CustomCommandSync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2) {
    LogOut(KMcvCat, _L("Video Player's custom command sync v2 is not yet implemented!"));
    return 0;
}

EXPORT_C void CVideoPlayerUtility::CustomCommandAsync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TDes8& aDataFrom, TRequestStatus& aStatus) {
    LogOut(KMcvCat, _L("Video Player's custom command async v1 is not yet implemented!"));

    TRequestStatus *statusPtr = &aStatus;    
    User::RequestComplete(statusPtr, KErrNone);
}

EXPORT_C void CVideoPlayerUtility::CustomCommandAsync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TRequestStatus& aStatus) {
    LogOut(KMcvCat, _L("Video Player's custom command async v2 is not yet implemented!"));

    TRequestStatus *statusPtr = &aStatus;    
    User::RequestComplete(statusPtr, KErrNone);
}

EXPORT_C MMMFDRMCustomCommand* CVideoPlayerUtility::GetDRMCustomCommand() {
    LogOut(KMcvCat, _L("Video Player's get drm custom command is not yet implemented!"));
    return NULL;
}

EXPORT_C void CVideoPlayerUtility::StopDirectScreenAccessL() {
    // Does nothing ;)
}

EXPORT_C void CVideoPlayerUtility::StartDirectScreenAccessL() {
    // Does nothing ;)
}

EXPORT_C TInt CVideoPlayerUtility::RegisterAudioResourceNotification(MMMFAudioResourceNotificationCallback& aCallback, TUid aNotificationEventUid, const TDesC8& aNotificationRegistrationData) {
    LogOut(KMcvCat, _L("Video Player's register audio resource notification is not yet implemented!"));
}

EXPORT_C TInt CVideoPlayerUtility::CancelRegisterAudioResourceNotification(TUid aNotificationEventId) {
    LogOut(KMcvCat, _L("Video Player's cancel register audio resource notification is not yet implemented!"));
}

EXPORT_C TInt CVideoPlayerUtility::WillResumePlay() {
    LogOut(KMcvCat, _L("Video Player's will resume play is not yet implemented!"));
    return ETrue;
}

EXPORT_C TInt CVideoPlayerUtility::SetInitScreenNumber(TInt aScreenNumber) {
    LogOut(KMcvCat, _L("Video Player's set init screen number is not yet implemented!"));
    return KErrNone;
}

EXPORT_C void CVideoPlayerUtility::SetPlayVelocityL(TInt aVelocity) {
    LogOut(KMcvCat, _L("Video Player's set play velocity is not yet implemented!"));
}

EXPORT_C TInt CVideoPlayerUtility::PlayVelocityL() const {
    LogOut(KMcvCat, _L("Video Player's get play velocity is not yet implemented!"));
    return 0;
}

EXPORT_C void CVideoPlayerUtility::StepFrameL(TInt aStep) {
    LogOut(KMcvCat, _L("Video Player's Step frame is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::GetPlayRateCapabilitiesL(TVideoPlayRateCapabilities& aCapabilities) const {
    LogOut(KMcvCat, _L("Video Player's get play rate capabilities is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::SetVideoEnabledL(TBool aVideoEnabled) {
    LogOut(KMcvCat, _L("Video Player's set video enabled is not yet implemented!"));
}

EXPORT_C TBool CVideoPlayerUtility::VideoEnabledL() const {
    LogOut(KMcvCat, _L("Video Player's get video enabled is not yet implemented!"));
    return ETrue;
}

EXPORT_C void CVideoPlayerUtility::SetAudioEnabledL(TBool aAudioEnabled) {
    LogOut(KMcvCat, _L("Video Player's set audio enabled is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::SetAutoScaleL(TAutoScaleType aScaleType) {
    LogOut(KMcvCat, _L("Video Player's set auto scale is not yet implemented!"));
}

EXPORT_C void CVideoPlayerUtility::SetAutoScaleL(TAutoScaleType aScaleType, TInt aHorizPos, TInt aVertPos) {
    LogOut(KMcvCat, _L("Video Player's set video scale is not yet implemented!"));    
}

CVideoPlayerUtility::~CVideoPlayerUtility() {
    delete iBody;
}

EXPORT_C void Reserved1() {
    
}

EXPORT_C void Reserved2() {
    
}
