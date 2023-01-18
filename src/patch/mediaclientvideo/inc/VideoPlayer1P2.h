#ifndef __VIDEOPLAYER_H__
#define __VIDEOPLAYER_H__

#include <w32std.h>
#include <fbs.h>
#include <f32file.h>
#include <mmf/common/mmfbase.h>
#include <mmf/common/mmfcontroller.h>
#include <mmf/common/mmfaudio.h>
#include <mmf/common/mmfstandardcustomcommands.h>
#include <mmf/common/mmfdrmcustomcommands.h>
#include <mda/common/base.h>
#include <mmf/common/mmfutilities.h>
#include <mmf/common/mmfcontrollerframeworkbase.h>
#include "mmf/common/mmcaf.h"
#include <mmfclntutility.h>

class MVideoPlayerUtilityObserver {
public:
	virtual void MvpuoOpenComplete(TInt aError) = 0;
	virtual void MvpuoPrepareComplete(TInt aError) = 0;
	virtual void MvpuoFrameReady(CFbsBitmap& aFrame,TInt aError) = 0;
	virtual void MvpuoPlayComplete(TInt aError) = 0;
	virtual void MvpuoEvent(const TMMFEvent& aEvent) = 0;
	};

class MVideoLoadingObserver {
public:
	virtual void MvloLoadingStarted() = 0;
	virtual void MvloLoadingComplete() = 0;
};

class CMMFVideoPlayerCallback;
class CVideoPlayerUtility2;
class TVideoPlayRateCapabilities;

class CVideoPlayerUtility : public CBase, public MMMFClientUtility {
public:
	~CVideoPlayerUtility();

	EXPORT_C static CVideoPlayerUtility* NewL(MVideoPlayerUtilityObserver& aObserver,
											  TInt aPriority,
											  TMdaPriorityPreference aPref,
											  RWsSession& aWs,
											  CWsScreenDevice& aScreenDevice,
											  RWindowBase& aWindow,
											  const TRect& aScreenRect,
											  const TRect& aClipRect);

	EXPORT_C void OpenFileL(const TDesC& aFileName,TUid aControllerUid = KNullUid);
	EXPORT_C void OpenFileL(const RFile& aFileName, TUid aControllerUid = KNullUid);

	EXPORT_C void OpenFileL(const TMMSource& aSource, TUid aControllerUid = KNullUid);

	EXPORT_C void OpenDesL(const TDesC8& aDescriptor,TUid aControllerUid = KNullUid);

	EXPORT_C void OpenUrlL(const TDesC& aUrl, TInt aIapId = KUseDefaultIap, const TDesC8& aMimeType=KNullDesC8, TUid aControllerUid = KNullUid);

	EXPORT_C void Prepare();

	EXPORT_C void Close();

	EXPORT_C void Play();

	EXPORT_C void Play(const TTimeIntervalMicroSeconds& aStartPoint, const TTimeIntervalMicroSeconds& aEndPoint);

	EXPORT_C TInt Stop();

	EXPORT_C void PauseL();

	EXPORT_C void SetPriorityL(TInt aPriority, TMdaPriorityPreference aPref);

	EXPORT_C void PriorityL(TInt& aPriority, TMdaPriorityPreference& aPref) const;

	EXPORT_C void SetDisplayWindowL(RWsSession& aWs,CWsScreenDevice& aScreenDevice,RWindowBase& aWindow,const TRect& aWindowRect,const TRect& aClipRect);

	EXPORT_C void RegisterForVideoLoadingNotification(MVideoLoadingObserver& aCallback);

	EXPORT_C void GetVideoLoadingProgressL(TInt& aPercentageComplete);

	EXPORT_C void GetFrameL(TDisplayMode aDisplayMode);

	EXPORT_C void GetFrameL(TDisplayMode aDisplayMode, ContentAccess::TIntent aIntent);

	EXPORT_C void RefreshFrameL();

	EXPORT_C TReal32 VideoFrameRateL() const;

	EXPORT_C void SetVideoFrameRateL(TReal32 aFramesPerSecond);

	EXPORT_C void VideoFrameSizeL(TSize& aSize) const;

	EXPORT_C const TDesC8& VideoFormatMimeType() const;

	EXPORT_C TInt VideoBitRateL() const;

	EXPORT_C TInt AudioBitRateL() const;

	EXPORT_C TFourCC AudioTypeL() const;

	EXPORT_C TBool AudioEnabledL() const;

	EXPORT_C void SetPositionL(const TTimeIntervalMicroSeconds& aPosition);

	EXPORT_C TTimeIntervalMicroSeconds PositionL() const;

	EXPORT_C TTimeIntervalMicroSeconds DurationL() const;

	EXPORT_C void SetVolumeL(TInt aVolume);

	EXPORT_C TInt Volume() const;

	EXPORT_C TInt MaxVolume() const;

	EXPORT_C void SetBalanceL(TInt aBalance);

	EXPORT_C TInt Balance()const;

	EXPORT_C void SetRotationL(TVideoRotation aRotation);

	EXPORT_C TVideoRotation RotationL() const;

	EXPORT_C void SetScaleFactorL(TReal32 aWidthPercentage, TReal32 aHeightPercentage, TBool aAntiAliasFiltering);

	EXPORT_C void GetScaleFactorL(TReal32& aWidthPercentage, TReal32& aHeightPercentage, TBool& aAntiAliasFiltering) const;

	EXPORT_C void SetCropRegionL(const TRect& aCropRegion);

	EXPORT_C void GetCropRegionL(TRect& aCropRegion) const;

	EXPORT_C TInt NumberOfMetaDataEntriesL() const;

	EXPORT_C CMMFMetaDataEntry* MetaDataEntryL(TInt aIndex) const;

	EXPORT_C const CMMFControllerImplementationInformation& ControllerImplementationInformationL();

	EXPORT_C TInt CustomCommandSync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TDes8& aDataFrom);

	EXPORT_C TInt CustomCommandSync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2);

	EXPORT_C void CustomCommandAsync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TDes8& aDataFrom, TRequestStatus& aStatus);

	EXPORT_C void CustomCommandAsync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TRequestStatus& aStatus);

	EXPORT_C MMMFDRMCustomCommand* GetDRMCustomCommand();

	EXPORT_C void StopDirectScreenAccessL();
	
	EXPORT_C void StartDirectScreenAccessL();

	EXPORT_C TInt RegisterAudioResourceNotification(MMMFAudioResourceNotificationCallback& aCallback, TUid aNotificationEventUid, const TDesC8& aNotificationRegistrationData = KNullDesC8);

	EXPORT_C TInt CancelRegisterAudioResourceNotification(TUid aNotificationEventId);
    
	EXPORT_C TInt WillResumePlay();
	
	EXPORT_C TInt SetInitScreenNumber(TInt aScreenNumber);

	EXPORT_C void SetPlayVelocityL(TInt aVelocity);

	EXPORT_C TInt PlayVelocityL() const;

	EXPORT_C void StepFrameL(TInt aStep);
	
	EXPORT_C void GetPlayRateCapabilitiesL(TVideoPlayRateCapabilities& aCapabilities) const;
	
	EXPORT_C void SetVideoEnabledL(TBool aVideoEnabled);

	EXPORT_C TBool VideoEnabledL() const;

	EXPORT_C void SetAudioEnabledL(TBool aAudioEnabled);

	EXPORT_C void SetAutoScaleL(TAutoScaleType aScaleType);

	EXPORT_C void SetAutoScaleL(TAutoScaleType aScaleType, TInt aHorizPos, TInt aVertPos);
	
	EXPORT_C void SetExternalDisplaySwitchingL(TInt aDisplay, TBool aControl);
	
private:
	class CBody;
	
	CBody* iBody;
	
	friend class CBody;
	friend class CVideoPlayerUtility2;	
private:
	enum TMMFVideoPlayerState
		{
		EStopped,
		EOpening,
		EPaused,
		EPlaying		
		};
	};

class CVideoPlayerUtility2 : public CVideoPlayerUtility {
public:
	~CVideoPlayerUtility2();

	EXPORT_C static CVideoPlayerUtility2* NewL(MVideoPlayerUtilityObserver& aObserver,
											  TInt aPriority,
											  TInt aPref);
							
	EXPORT_C void AddDisplayWindowL(RWsSession& aWs, CWsScreenDevice& aScreenDevice, 
									RWindow& aWindow, const TRect& aVideoExtent, 
									const TRect& aWindowClipRect);

	EXPORT_C void AddDisplayWindowL(RWsSession& aWs, CWsScreenDevice& aScreenDevice, RWindow& aWindow);

 	EXPORT_C void RemoveDisplayWindow(RWindow& aWindow);
 	
 	EXPORT_C void SetVideoExtentL(const RWindow& aWindow, const TRect& aVideoExtent);
	
	EXPORT_C void SetWindowClipRectL(const RWindow& aWindow, const TRect& aWindowClipRect);
	
	EXPORT_C void SetRotationL(const RWindow& aWindow, TVideoRotation aRotation);
	
	EXPORT_C TVideoRotation RotationL(const RWindow& aWindow);
	
	EXPORT_C void SetScaleFactorL(const RWindow& aWindow, TReal32 aWidthPercentage, TReal32 aHeightPercentage);
	
	EXPORT_C void GetScaleFactorL(const RWindow& aWindow, TReal32& aWidthPercentage, TReal32& aHeightPercentage);
	
	EXPORT_C void SetAutoScaleL(const RWindow& aWindow, TAutoScaleType aScaleType);

    EXPORT_C void SetAutoScaleL(const RWindow& aWindow, TAutoScaleType aScaleType, TInt aHorizPos, TInt aVertPos);
};

#endif
