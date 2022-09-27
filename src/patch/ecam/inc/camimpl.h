/*
 * Copyright (c) 2021 EKA2L1 Team.
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

#ifndef ECAM_CAMIMPL_H_
#define ECAM_CAMIMPL_H_

#include <ecam.h>
#include <e32base.h>
#include <e32std.h>

enum TCameraParameter {
    EOpticalZoom,       // Morden API on phone like Android doesn't seems to include thing like this...
    EDigitalZoom,
    EContrast,
    EBrightness,
    EFlash,
    EExposure,
    EWhiteBalance
};

class CCameraImageBufferImpl: public MCameraBuffer {
private:
	HBufC8 *iDataBuffer;
	CFbsBitmap *iDataBitmap;

public:
	CCameraImageBufferImpl();

	virtual ~CCameraImageBufferImpl() {
		Release();
	}

	void ConstructL(TInt aDataSize);
	void ConstructBitmapL(TSize aSize, TInt aFormat);

	TUint8 *DataPtr();
	void EndDataAccess();

	TBool IsFree() {
		return (!iDataBuffer && !iDataBitmap);
	}
	
	void Clear() {
		iDataBuffer = NULL;
		iDataBitmap = NULL;
	}

	virtual TInt NumFrames() {
		return 1;
	}

	virtual TDesC8 *DataL(TInt aFrameIndex);
	virtual CFbsBitmap& BitmapL(TInt aFrameIndex);
	virtual RChunk& ChunkL();
	virtual TInt ChunkOffsetL(TInt aFrameIndex);
	virtual TInt FrameSize(TInt aFrameIndex);
	virtual void Release();
};

class CCameraVideoChunkBufferBase {
protected:
    RChunk iChunk;
    RArray<TInt> iOffsets;
    TUint32 iTotalSize;
    
public:
    CCameraVideoChunkBufferBase();
    virtual ~CCameraVideoChunkBufferBase();

    void Construct();    
    void PrepareRecieveL(TInt aTotalOfFrames, TUint32 aTotalSize);
    TInt *GetOffsetArrays() const;
    
    TUint8 *ChunkDataPtr() {
        return iChunk.Base();
    }
    
    TBool IsFree() {
        return iTotalSize == 0;
    }
};

class CCameraVideoFramebuffer: public MFrameBuffer, public CCameraVideoChunkBufferBase {
public:
    virtual TInt NumFrames() {
        return iOffsets.Count();
    }

    virtual TDesC8 *DataL(TInt aFrameIndex);
    virtual CFbsBitmap *FrameL(TInt aFrameIndex);
    virtual void Release();
};

class CCameraVideoBuffer2Impl: public MCameraBuffer, public CCameraVideoChunkBufferBase  {
public:
    virtual TInt NumFrames() {
        return iOffsets.Count();
    }

    virtual TDesC8 *DataL(TInt aFrameIndex);
    virtual CFbsBitmap& BitmapL(TInt aFrameIndex);
    virtual RChunk& ChunkL();
    virtual TInt ChunkOffsetL(TInt aFrameIndex);
    virtual TInt FrameSize(TInt aFrameIndex);
    virtual void Release();
};

class CCameraImageCaptureObject : public CActive {
private:
	TAny *iDispatch;
    TAny *iObserver;

    HBufC8 *iDataBuffer;
    CFbsBitmap *iDataBitmap;

	TInt iObserverVersion;
	TInt iBitmapDisplayMode;

	TSize iBitmapSize;

	RPointerArray<CCameraImageBufferImpl> iImageBuffers;

public:
    enum TCameraBufferProperties {
        CAMERA_MAX_IMAGE_BUFFER = 10
    };
    
    CCameraImageCaptureObject();
    virtual ~CCameraImageCaptureObject();
    
    void ConstructL(TAny *aDispatch, void *aObserver, TInt aObserverVersion);

    void PrepareImageCaptureL(TInt aIndex, CCamera::TFormat aFormat, TInt aVersion);
    TBool StartCapture(TInt aIndex, CCamera::TFormat aFormat, TRect *aClipRect);
 
    void HandleCompleteV1();
    void HandleCompleteV2();

    virtual void RunL();
    virtual void DoCancel();
};

struct CCameraVideoCaptureObject: public CActive {
private:
    RPointerArray<CCameraVideoChunkBufferBase> iBuffers;
    
    TAny *iDispatch;
    TAny *iObserver;
    TInt iVersion;
    TInt iFramesPerBuffer;

    void ResetBufferArray();
    void HandleCompleteV1();
    void HandleCompleteV2();
    
    CCameraVideoChunkBufferBase *PickFreeBuffer();
    
public:
    CCameraVideoCaptureObject();
    virtual ~CCameraVideoCaptureObject();

    void ConstructL(TAny *aDispatch, void *aObserver, TInt aObserverVersion);
    void PrepareBuffersL(TInt aBuffers);

    TInt BuffersInUse() const;
    
    void StartCaptureL(TInt aSizeIndex, TInt aFrameRateIndex, CCamera::TFormat aFormat, TInt aFramePerBuffer, TRect *aClipRect);

    TInt FramesPerBuffer() const {
        return iFramesPerBuffer;
    }
 
    virtual void RunL();
    virtual void DoCancel();
};

class CCameraViewfinderBitmapObject: public CActive {
private:
	TAny *iDispatch;
	TAny *iObserver;
	TInt iObserverVersion;

	RPointerArray<CCameraImageBufferImpl> iBitmapBuffers;
	CFbsBitmap *iDataBitmap;

	enum TCameraBufferProperties {
		CAMERA_MAX_BITMAP_BUFFER = 10
	};

	TInt iScreenDisplayMode;
	TSize iBitmapSize;

private:
	void CleanBuffers();
	void HandleCompleteV1();
	void HandleCompleteV2();

public:
	CCameraViewfinderBitmapObject();
	virtual ~CCameraViewfinderBitmapObject();

	void ConstructL(TAny *aDispatch, void *aObserver, TInt aObserverVersion);
	void StartL(TSize &aSize);

	virtual void RunL();
	virtual void DoCancel();
};

class CCameraPlugin : public CCamera {
private:
    TAny *iDispatchInstance;
    void *iObserver;
	TInt iVersion;

	CCameraImageCaptureObject iImageCapture;
	CCameraVideoCaptureObject iVideoCapture;
	CCameraViewfinderBitmapObject iViewfinderBitmapCapture;

	CIdle *iEventCompleteIdle;

    TInt iOpticalZoomFactor;
    TInt iDigitalZoomFactor;
    TInt iContrast;
    TInt iBrightness;
    TFlash iFlash;
    TExposure iExposure;
    TWhiteBalance iWhiteBalance;

	TCameraInfo iInfo;

	TInt iImageCaptureSizeIndex;
	TInt iImageCaptureFormat;
	TRect iImageCaptureClipRect;
	TBool iImageCaptureNeedClip;
	
	TInt iVideoCaptureSizeIndex;
	TInt iVideoCaptureFormat;
	TRect iVideoCaptureClipRect;
	TBool iVideoCaptureNeedClip;
	TInt iVideoCaptureFrameRateIndex;
	TInt iVideoFramesPerBuffer;
	
	TSize iVideoFrameDimCache;
	TReal32 iVideoFrameRateCache;
	
	TInt iJpegQuality;

	void HandleReserveCompleteV1(TInt aError);
	void HandleReserveCompleteV2(TInt aError);
	void HandlePowerOnCompleteV1(TInt aError);
	void HandlePowerOnCompleteV2(TInt aError);

public:
	CCameraPlugin(MCameraObserver *observer);
	CCameraPlugin(MCameraObserver2 *observer);
	~CCameraPlugin();

	void ConstructL(TInt aCameraIndex);
	void ConstructSharedL(TAny *aSharedHandle);

	void HandleReserveComplete(TInt aError);
	void HandlePowerOnComplete(TInt aError);

    virtual void CameraInfo(TCameraInfo& aInfo) const;

	virtual void Reserve();
	virtual void Release();
	virtual void PowerOn();
	virtual void PowerOff();
	virtual TInt Handle();

	virtual void SetZoomFactorL(TInt aZoomFactor = 0);
	virtual TInt ZoomFactor() const;
	virtual void SetDigitalZoomFactorL(TInt aDigitalZoomFactor = 0);
	virtual TInt DigitalZoomFactor() const;
	virtual void SetContrastL(TInt aContrast);
	virtual TInt Contrast() const;
	virtual void SetBrightnessL(TInt aBrightness);
	virtual TInt Brightness() const;
	virtual void SetFlashL(TFlash aFlash = EFlashNone);
	virtual TFlash Flash() const;
	virtual void SetExposureL(TExposure aExposure = EExposureAuto);
	virtual TExposure Exposure() const;
	virtual void SetWhiteBalanceL(TWhiteBalance aWhiteBalance = EWBAuto);
	virtual TWhiteBalance WhiteBalance() const;

	virtual void StartViewFinderDirectL(RWsSession& aWs,CWsScreenDevice& aScreenDevice,RWindowBase& aWindow,TRect& aScreenRect);
	virtual void StartViewFinderDirectL(RWsSession& aWs,CWsScreenDevice& aScreenDevice,RWindowBase& aWindow,TRect& aScreenRect,TRect& aClipRect);
	virtual void StartViewFinderBitmapsL(TSize& aSize);	
	virtual void StartViewFinderBitmapsL(TSize& aSize,TRect& aClipRect);
	virtual void StartViewFinderL(TFormat aImageFormat,TSize& aSize);
	virtual void StartViewFinderL(TFormat aImageFormat,TSize& aSize,TRect& aClipRect);
	virtual void StopViewFinder();
	virtual TBool ViewFinderActive() const;
	virtual void SetViewFinderMirrorL(TBool aMirror);
	virtual TBool ViewFinderMirror() const;

	virtual void PrepareImageCaptureL(TFormat aImageFormat,TInt aSizeIndex);
	virtual void PrepareImageCaptureL(TFormat aImageFormat,TInt aSizeIndex,const TRect& aClipRect);
	virtual void CaptureImage();
	virtual void CancelCaptureImage();
	virtual void EnumerateCaptureSizes(TSize& aSize,TInt aSizeIndex,TFormat aFormat) const;

	virtual void PrepareVideoCaptureL(TFormat aFormat,TInt aSizeIndex,TInt aRateIndex,TInt aBuffersToUse,TInt aFramesPerBuffer);
	virtual void PrepareVideoCaptureL(TFormat aFormat,TInt aSizeIndex,TInt aRateIndex,TInt aBuffersToUse,TInt aFramesPerBuffer,const TRect& aClipRect);
	virtual void StartVideoCapture();
	virtual void StopVideoCapture();
	virtual TBool VideoCaptureActive() const;
	virtual void EnumerateVideoFrameSizes(TSize& aSize,TInt aSizeIndex,TFormat aFormat) const;
	virtual void EnumerateVideoFrameRates(TReal32& aRate,TInt aRateIndex,TFormat aFormat,TInt aSizeIndex,TExposure aExposure = EExposureAuto) const;
	virtual void GetFrameSize(TSize& aSize) const;
	virtual TReal32 FrameRate() const;
	virtual TInt BuffersInUse() const;
	virtual TInt FramesPerBuffer() const;
	virtual void SetJpegQuality(TInt aQuality);
	virtual TInt JpegQuality() const;
    virtual TAny* CustomInterface(TUid aInterface);
};

#endif
