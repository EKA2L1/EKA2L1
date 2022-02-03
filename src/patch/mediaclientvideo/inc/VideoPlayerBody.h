#ifndef MEDIACLIENTVIDEO_VIDEO_PLAYER_BODY_H
#define MEDIACLIENTVIDEO_VIDEO_PLAYER_BODY_H

#include <VideoPlayer.h>

enum TVideoPlayerState {
    EVideoPlayerStateIdle = 0,
    EVideoPlayerStateOpened = 1,
    EVideoPlayerStatePrepared = 2,
    EVideoPlayerStatePlaying = 3,
    EVideoPlayerStatePaused = 4
};

class CVideoPlayerFeedbackHandler {
private:
    MVideoPlayerUtilityObserver &iObserver;
    TVideoPlayerState iCurrentState;

public:
    CVideoPlayerFeedbackHandler(MVideoPlayerUtilityObserver &aObserver);
    
    void OpenComplete(const TInt aError);
    void PrepareComplete(const TInt aError);
    void PlayComplete(const TInt aError);
    
    void Pause();
    
    const TVideoPlayerState CurrentState() const {
        return iCurrentState;
    }
};

class CVideoPlayerUtility::CBody : public CActive {
private:
    CVideoPlayerFeedbackHandler iFeedbackHandler;
    
    RWindowBase *iWindow;
    TRect iDisplayRect;
    
    TAny *iDispatchInstance;
    TReal32 iVideoFps;
    TInt iVideoBitRate;
    TInt iAudioBitRate;
    TInt iCurrentVolume;
    TVideoRotation iCurrentRotation;
    
    CIdle *iCompleteIdle;

    CBody(MVideoPlayerUtilityObserver &aObserver);

    void ConstructL(RWsSession &aWsSession, RWindowBase &aWindow, const TRect &aClipRect);
    
public:
    static CBody *NewL(MVideoPlayerUtilityObserver &aObserver, RWsSession &aWsSession, RWindowBase &aWindow, const TRect &aClipRect);
    ~CBody();

    void SetOwnedWindowL(RWsSession &aSession, RWindowBase &aWindow);
    void SetDisplayRectL(const TRect &aClipRect);
    
    void OpenFileL(const TDesC &aPath);
    void OpenDesL(const TDesC8 &aContent);
    void Close();

    void Prepare();
    
    void Play(const TTimeIntervalMicroSeconds *aInterval);
    
    void Pause();
    void Stop();
    
    void SetPositionL(const TTimeIntervalMicroSeconds &aWhere);
    TTimeIntervalMicroSeconds PositionL() const;
    TTimeIntervalMicroSeconds DurationL() const;
    
    void SetFpsL(const TReal32 aFps);
    TReal32 Fps();
    
    TInt VideoBitRate();
    TInt AudioBitRate();
    
    void SetVolumeL(TInt aVolume);
    TInt Volume();
    TInt MaxVolume() const;

    void SetRotationL(TVideoRotation aRotation);
    TVideoRotation Rotation() const;
 
    virtual void RunL();
    virtual void DoCancel();

    void GetDisplayRect(TRect &aClipRect) {
        aClipRect = iDisplayRect;
    }
};

#endif
