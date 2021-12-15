#ifndef POSTING_SURFACE_H__
#define POSTING_SURFACE_H__

#include <e32base.h>
#include <e32cmn.h>
#include <gdi.h>

///Little endian bit mask
const TUint KPostFormatLE    = 0x01000000;
///Big endian bit mask
const TUint KPostFormatBE    = 0x02000000;
///Color range 0-255 bit mask
const TUint KPostColorRange0 = 0x00000010;
///Color range 16-235 bit mask
const TUint KPostColorRange1 = 0x00000020;
///ITU-R BT.709-5 standard bit mask
const TUint KPost709         = 0x00000100;
///ITU-R BT.601-5 standard bit mask
const TUint KPost601         = 0x00000200;
///Posting RGB format RGB565 bit mask
const TUint KPostRGB565      = 0x00000001;
///Posting RGB format RGB888 (24bit) bit mask
const TUint KPostRGB888      = 0x00000002;
///Posting RGB format RGBx888 (32bit) bit mask
const TUint KPostRGBx888     = 0x00000004;
///Posting YUV 420 Planar format
const TUint KPostYUV420Planar= 0x00001000;
///Platform specific
const TUint KPostPlatformDependent = 0x10000000;

class CPostingSurface;
class TPostingBuff;

class CPostingSurface: public CBase {
public:
    enum TPostingFormat {
        EFormatInvalidValue   = 0x00000000,
        ERgb16bit565Le        = KPostRGB565  | KPostFormatLE,
        ERgb24bit888Le        = KPostRGB888  | KPostFormatLE,
        ERgb32bit888Le        = KPostRGBx888 | KPostFormatLE,
        EYuv422LeBt709Range0  = KPost709     | KPostFormatLE | KPostColorRange0,
        EYuv422LeBt709Range1  = KPost709     | KPostFormatLE | KPostColorRange1,
        EYuv422BeBt709Range0  = KPost709     | KPostFormatBE | KPostColorRange0,
        EYuv422BeBt709Range1  = KPost709     | KPostFormatBE | KPostColorRange1,
        EYuv422LeBt601Range0  = KPost601     | KPostFormatLE | KPostColorRange0,
        EYuv422LeBt601Range1  = KPost601     | KPostFormatLE | KPostColorRange1,
        EYuv422BeBt601Range0  = KPost601     | KPostFormatBE | KPostColorRange0,
        EYuv422BeBt601Range1  = KPost601     | KPostFormatBE | KPostColorRange1,
        EYuv420PlanarBt709Range0 = KPost709     | KPostYUV420Planar | KPostColorRange0,
        EYuv420PlanarBt709Range1 = KPost709     | KPostYUV420Planar | KPostColorRange1,
        EYuv420PlanarBt601Range0 = KPost601     | KPostYUV420Planar | KPostColorRange0,
        EYuv420PlanarBt601Range1 = KPost601     | KPostYUV420Planar | KPostColorRange1,
        EYuvPlatformDependent    = KPostPlatformDependent,
    };

    enum TPostingBuffering {
        EBufferingInvalid  = 0,
        ESingleBuffering   = 0x00000004,
        EDoubleBuffering   = 0x00000001,
        ETripleBuffering   = 0x00000002,
        EProgressiveFrames = 0x00000010,
        EInterlacedFrames  = 0x00000020,
        EDisAllowFrameSkip = 0x00000100,
        EAllowFrameSkip    = 0x00000200,
        EInternalBuffers   = 0x00001000,
        EExternalBuffers   = 0x00002000,
    };

    enum TRotationType {
        ENoRotation            = 0,
        ERotate90ClockWise     = 1 << 0,
        ERotate180             = 1 << 1,
        ERotate90AntiClockWise = 1 << 2,
    };

    enum TPostingUsageHint {
        EUsageInvalid   = 0,
        EGeneric        = 1 << 0,
        E3D             = 1 << 1,
        EVideoPlayback  = 1 << 2,
        EVideoCapture   = 1 << 3,
        EViewfinder     = 1 << 4,
        EStillImage,
        EVendorSpecific = 0x80000000,
    };

    class TPostingCapab {
    public:
        TUint iMaxSourceSize;
        TSize iMaxPixelSize;
        TUint iSupportedRotations;
        TBool iSupportsMirroring;
        TBool iSupportsScaling;
        TBool iSupportsBrightnessControl;
        TBool iSupportsContrastControl;
        TUint iSupportedPostingBuffering;
        TUint iSupportedBufferTypes;
        
        inline TPostingCapab();
    };

    enum TBufferType {
        EBufferTypeInvalid = 0x00000000,
        EStandardBuffer    = 0x00000001,
        EChunkBuffer       = 0x00000002,
        EExternalBuffer    = 0x00000004,
    };

    class TPostingSourceParams {
    public:
        TUint iPostingBuffering;
        TBufferType iBufferType;
        TUint iPostingUsage;

        TSize iSourceImageSize;
        TPostingFormat iPostingFormat;

        TUint16 iPixelAspectRatioNum;
        TUint16 iPixelAspectRatioDenom;
        TBool iContentCopyRestricted;

        inline TPostingSourceParams();
    };

    class TPostingParams {
    public:
        TRect iDisplayedRect;
        TRect iScaleToRect;
        TRect iInputCrop;
        TBool iMirror;

        TRotationType iRotation;

        TInt16 iBrightness;
        TInt16 iContrast;
        TRgb iBackGround;

        inline TPostingParams();
    };


    class TPostingBuff {
    protected:
        TBufferType iType;
        TAny*  iBuffer;
        TSize  iSize;
        TUint iStride;

    public:
        IMPORT_C virtual TBufferType GetType();
        IMPORT_C virtual TAny* GetBuffer();
        IMPORT_C virtual TSize GetSize();
        IMPORT_C virtual TUint GetStride();

    protected:
        IMPORT_C TPostingBuff();
    };

    class TPostingBuffExt : public TPostingBuff {
    protected:
        IMPORT_C TPostingBuffExt();

    public:
        IMPORT_C virtual void SetBuffer(TAny* aBuffer);
        IMPORT_C virtual void SetSize(TSize aSize);
        IMPORT_C virtual void SetStride(TUint aStride);
    };

    class TPostingBufferChunk : public TPostingBuff {
    protected:
        RChunk iChunk;
        TUint iOffsetInChunk;

        IMPORT_C TPostingBufferChunk();

    public:
        IMPORT_C virtual RChunk GetChunk();
        IMPORT_C virtual TUint GetOffsetInChunk();
    };

    virtual ~CPostingSurface() {};

    virtual void InitializeL(const TPostingSourceParams& aSource, const TPostingParams& aDest) = 0;

    virtual void GetCapabilities(TPostingCapab& aCaps) = 0;
    virtual TBool FormatSupported(TPostingFormat aFormat) = 0;

    virtual TInt SetPostingParameters(const TPostingParams& aParams) = 0;
    virtual TInt SetClipRegion(const TRegion& aClipRegion) = 0;

    virtual TInt WaitForNextBuffer(TRequestStatus& aComplete) = 0;
    virtual TPostingBuff* NextBuffer() = 0;
    virtual TInt CancelBuffer() = 0;
    virtual TInt PostBuffer(TPostingBuff* aBuffer) = 0;

    virtual void Destroy(TRequestStatus& aComplete)= 0;
    virtual void Destroy() = 0;
};

#include "PostingSurface.inl"

#endif
