#ifndef MEDIACLIENTVIDEO_VIDEO_PLAYER_BODY_H
#define MEDIACLIENTVIDEO_VIDEO_PLAYER_BODY_H

#ifdef MCV_EKA1
#include <videoplayer.h>
#else
#include "VideoPlayer1P2.h"
#endif
#include "dispatch.h"

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

    void Play();
    void Pause();
    void Stop();
    void Close();
    
    const TVideoPlayerState CurrentState() const {
        return iCurrentState;
    }
};

class CVideoPlayerUtility::CBody : public CActive {
private:
    CVideoPlayerFeedbackHandler iFeedbackHandler;

    struct TDisplayWindowInfo {
        const RWindowBase *iWindow;
        TInt iManagedHandle;
        TVideoWindowGeometry iGeometry;
        TVideoRotation iRotation;
        TReal32 iScaleWidth;
        TReal32 iScaleHeight;
        TBool iHasScale;
    };

    RArray<TDisplayWindowInfo> iWindowInfos;
#ifndef MCV_EKA1
    struct TDisplayObserver {
        TInt iDisplay;
        MMMFSurfaceEventHandler *iHandler;
    };
    RArray<TDisplayObserver> iDisplayObservers;
    TSurfaceId iSurfaceId;
#endif
    RWindowBase *iActiveWindow;
    TInt iActiveWindowHandle;
    TRect iActiveClipRect;
    TRect iCropRegion;

    TAny *iDispatchInstance;
    TReal32 iVideoFps;
    TInt iVideoBitRate;
    TInt iAudioBitRate;
    TInt iCurrentVolume;
    TVideoRotation iCurrentRotation;
    
    CIdle *iCompleteIdle;
    TInt iVersion;

    CBody(MVideoPlayerUtilityObserver &aObserver, TInt aVersion);

    void ConstructL(RWsSession &aWsSession, RWindowBase &aWindow, const TRect &aWindowRect, const TRect &aClipRect);
    void Construct2L();
    TDisplayWindowInfo &DisplayWindowL(const RWindow &aWindow);
    void UpdateDisplayWindowL(const TDisplayWindowInfo &aInfo);
    
public:
    static CBody *NewL(MVideoPlayerUtilityObserver &aObserver, RWsSession &aWsSession, RWindowBase &aWindow, const TRect &aWindowRect, const TRect &aClipRect);
    static CBody *New2L(MVideoPlayerUtilityObserver &aObserver);

    ~CBody();

#ifndef MCV_EKA1
    void AddDisplayL(TInt aDisplay, MMMFSurfaceEventHandler &aHandler);
    void RemoveDisplay(TInt aDisplay);
#endif
    void SetWindowRotationL(const RWindow &aWindow, TVideoRotation aRotation);
    TVideoRotation WindowRotationL(const RWindow &aWindow);
    void SetWindowScaleFactorL(const RWindow &aWindow, TReal32 aWidth, TReal32 aHeight);
    void GetWindowScaleFactorL(const RWindow &aWindow, TReal32 &aWidth, TReal32 &aHeight);
    void SetOwnedWindowL(RWsSession &aSession, RWindowBase &aWindow);
    void SetDisplayRectL(const TRect &aWindowRect, const TRect &aClipRect);
    void AddDisplayWindowL(RWsSession &aSession, RWindowBase &aWindow, const TRect &aExtent, const TRect &aClipRect);
    void SetDisplayRectForWindowL(const RWindow &aWindow, const TRect &aClipRect);
    void SetVideoExtentL(const RWindow &aWindow, const TRect &aExtent);
    void SetCropRegionL(const TRect &aCrop);
    void RemoveDisplayWindow(const RWindow &aWindow);

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
    
    void FrameSizeL(TSize &aSize) const;
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

    void GetCropRegion(TRect &aCrop) const {
        aCrop = iCropRegion;
    }
};

#endif
