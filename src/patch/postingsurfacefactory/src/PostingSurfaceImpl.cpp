#include <PostingSurfaceImpl.h>
#include <PostingSurfaceFactory.h>
#include <Dispatch.h>
#include <hal.h>
#include <Log.h>

// Based on references in ngi3runtime_0_9.dll shipped with The Big Roll in Paradise
// Entry 1 is called, which then call a vtable method with argument 1 equals to 0. It's assumed
// to be the screen index.
EXPORT_C CPostingSurfaceFactory* NewSurfaceManagerFactoryL() {
    return new (ELeave) CPostingSurfaceFactory;
}

EXPORT_C CPostingSurface::TBufferType CPostingSurface::TPostingBuff::GetType() {
    return iType;
}

EXPORT_C TAny * CPostingSurface::TPostingBuff::GetBuffer() {
    return iBuffer;
}

EXPORT_C TSize CPostingSurface::TPostingBuff::GetSize() {
    return iSize;
}

EXPORT_C TUint CPostingSurface::TPostingBuff::GetStride() {
    return iStride;
}

EXPORT_C CPostingSurface::TPostingBuff::TPostingBuff()
    : iType(EBufferTypeInvalid)
    , iBuffer(NULL)
    , iStride(0) {
    
}

EXPORT_C void CPostingSurface::TPostingBuffExt::SetBuffer(TAny* aBuffer) {
    iBuffer = aBuffer;
}

EXPORT_C void CPostingSurface::TPostingBuffExt::SetSize(TSize aSize) {
    iSize = aSize;
}

EXPORT_C void CPostingSurface::TPostingBuffExt::SetStride(TUint aStride) {
    iStride = aStride;
}

EXPORT_C CPostingSurface::TPostingBuffExt::TPostingBuffExt()
    : CPostingSurface::TPostingBuff::TPostingBuff() {
    iType = EStandardBuffer;
}

EXPORT_C CPostingSurface::TPostingBufferChunk::TPostingBufferChunk()
    : CPostingSurface::TPostingBuff::TPostingBuff()
    , iOffsetInChunk(0) {
    iType = EChunkBuffer;
}

EXPORT_C RChunk CPostingSurface::TPostingBufferChunk::GetChunk() {
    return iChunk;
}

EXPORT_C TUint CPostingSurface::TPostingBufferChunk::GetOffsetInChunk() {
    return iOffsetInChunk;
}

CPostingSurfaceFactory::~CPostingSurfaceFactory() {
    
}

CPostingSurface* CPostingSurfaceFactory::NewPostingSurfaceL(TInt aDisplayIndex) {
    CPostingSurfaceImpl *self = new (ELeave) CPostingSurfaceImpl();
    CleanupStack::PushL(self);

    self->ConstructL(aDisplayIndex);
    CleanupStack::Pop(self);

    return self;
}

T12Z1PostingBufferChunk::T12Z1PostingBufferChunk()
    : CPostingSurface::TPostingBufferChunk() {

}

void T12Z1PostingBufferChunk::ConstructL(TSize aSize, CPostingSurface::TPostingFormat aFormat) {
    TUint32 bufferSizeInBytes = 0;
    if ((aFormat == CPostingSurface::EYuv420PlanarBt709Range0) || (aFormat == CPostingSurface::EYuv420PlanarBt709Range1)
        || (aFormat == CPostingSurface::EYuv420PlanarBt601Range0) || (aFormat == CPostingSurface::EYuv420PlanarBt601Range1)) {
        bufferSizeInBytes = aSize.iHeight * aSize.iWidth + aSize.iWidth * aSize.iHeight * 2 / 4;
        iStride = 0;
    } else {
        switch (aFormat) {
            case CPostingSurface::ERgb16bit565Le:
            case CPostingSurface::EYuv422LeBt709Range0:
            case CPostingSurface::EYuv422LeBt709Range1:
            case CPostingSurface::EYuv422LeBt601Range0:
            case CPostingSurface::EYuv422LeBt601Range1:
                iStride = (((aSize.iWidth * 2) + 3) / 4 * 4);
                break;
                
            case CPostingSurface::ERgb24bit888Le:
                iStride = (((aSize.iWidth * 3) + 3) / 4 * 4);
                break;
                
            case CPostingSurface::ERgb32bit888Le:
                iStride = aSize.iWidth * 4;
                break;
                
            default:
                User::Leave(KErrNotSupported);
                break;
        }
        
        bufferSizeInBytes = iStride * aSize.iHeight;
    }
    
    iSize = aSize;
    iBuffer = NULL;

    User::LeaveIfError(iChunk.CreateLocal(bufferSizeInBytes, bufferSizeInBytes, EOwnerProcess));

    iBuffer = iChunk.Base();
}

void T12Z1PostingBufferChunk::Free() {
    iChunk.Close();
}

CPostingSurfaceImpl::CPostingSurfaceImpl()
    : iScreenIndex(0)
    , iWaiter(NULL) {
}

CPostingSurfaceImpl::~CPostingSurfaceImpl() {
    iBufferToPass.Free();
}

void CPostingSurfaceImpl::ConstructL(TInt aScreenIndex) {
    // Mean to test if the screen exists
    TInt testXDim = 0;
    User::LeaveIfError(HAL::Get(aScreenIndex, HAL::EDisplayXPixels, testXDim));
 
    iScreenIndex = aScreenIndex;
    
    // Set size to max of our screen size, and max source size to RGBA8888 buffer size.
    iCapabilities.iMaxPixelSize.SetSize(KMaxSourceDimXPixels, KMaxSourceDimYPixels);
    iCapabilities.iMaxSourceSize = KMaxSourceDimXPixels * KMaxSourceDimYPixels * sizeof(TUint32);
    iCapabilities.iSupportedBufferTypes = EChunkBuffer;
    iCapabilities.iSupportsScaling = ETrue;
    iCapabilities.iSupportsMirroring = ETrue;
    iCapabilities.iSupportsContrastControl = ETrue;
    iCapabilities.iSupportsBrightnessControl = ETrue;
    iCapabilities.iSupportedRotations = ERotate90ClockWise | ERotate180 | ERotate90AntiClockWise;
    iCapabilities.iSupportedPostingBuffering = ESingleBuffering | EDoubleBuffering;
}

void CPostingSurfaceImpl::GetCapabilities(TPostingCapab& aCaps) {
    aCaps = iCapabilities;
}

TBool CPostingSurfaceImpl::FormatSupported(TPostingFormat aFormat) {
    // Most formats except BE ones
    return (aFormat == ERgb16bit565Le) || (aFormat == ERgb24bit888Le) || (aFormat == ERgb32bit888Le)
            || (aFormat == EYuv422LeBt709Range0) || (aFormat == EYuv422LeBt709Range1)
            || (aFormat == EYuv422LeBt601Range0) || (aFormat == EYuv422LeBt601Range1)
            || (aFormat == EYuv420PlanarBt709Range0) || (aFormat == EYuv420PlanarBt709Range1)
            || (aFormat == EYuv420PlanarBt601Range0) || (aFormat == EYuv420PlanarBt601Range1);
}

void CPostingSurfaceImpl::InitializeL(const TPostingSourceParams& aSource, const TPostingParams& aDest) {
    if ((aSource.iBufferType != EChunkBuffer) && (aSource.iBufferType != EStandardBuffer)) {
        LogOut(KPostingSurfaceCat, _L("Buffer type requested in initialization is not chunk or standard! (type=%d)"), (TInt)aSource.iBufferType);
        User::Leave(KErrArgument);
    }
    
    if ((aSource.iSourceImageSize.iWidth > iCapabilities.iMaxPixelSize.iWidth)
        || (aSource.iSourceImageSize.iHeight > iCapabilities.iMaxPixelSize.iHeight)) {
        LogOut(KPostingSurfaceCat, _L("Posting size dimension requested in initialization exceed the maximum!"));
        User::Leave(KErrArgument);
    }
    
    if (aSource.iPostingBuffering & ETripleBuffering) {
        LogOut(KPostingSurfaceCat, _L("Tripple buffering is not supported!"));
        User::Leave(KErrArgument);
    }
    
    if (!FormatSupported(aSource.iPostingFormat)) {
        LogOut(KPostingSurfaceCat, _L("Format 0x%08X is not supported!"), (TUint32)aSource.iPostingFormat);
        User::Leave(KErrNotSupported);
    } else {
        LogOut(KPostingSurfaceCat, _L("Posting texture (%d, %d, 0x%08X) initialized!"), aSource.iSourceImageSize.iWidth,
            aSource.iSourceImageSize.iHeight, static_cast<TInt>(aSource.iPostingFormat));
    }
    
    // TV ratio ignored at the moment (norm and denorm)
    iSourceDimension = aSource.iSourceImageSize;
    iSourceFormat = aSource.iPostingFormat;
    
    iPostParams = aDest;
    
    iBufferToPass.ConstructL(iSourceDimension, iSourceFormat);
}

TInt CPostingSurfaceImpl::SetPostingParameters(const TPostingParams& aParams) {
    iPostParams = aParams;
    return KErrNone;
}

void CPostingSurfaceImpl::Destroy(TRequestStatus& aComplete) {
    Destroy();

    TRequestStatus *temp = &aComplete;
    User::RequestComplete(temp, KErrNone);
}

void CPostingSurfaceImpl::Destroy() {
    iBufferToPass.Free();
}

TInt CPostingSurfaceImpl::WaitForNextBuffer(TRequestStatus& aComplete) {
    iWaiter = &aComplete;
    aComplete = KRequestPending;

    return EScreenAddWaitVSync(0, iScreenIndex, &aComplete);
}

TInt CPostingSurfaceImpl::CancelBuffer() {
    if (!iWaiter) {
        return KErrNotReady;
    }

    EScreenCancelWaitVSync(0, iScreenIndex, iWaiter);
    iWaiter = NULL;
    
    return KErrNone;
}

CPostingSurfaceImpl::TPostingBuff* CPostingSurfaceImpl::NextBuffer() {
    return &iBufferToPass;
}

TInt CPostingSurfaceImpl::PostBuffer(TPostingBuff* aBuffer) {
    iWaiter = NULL;

    EScreenFlexiblePost(0, iScreenIndex, iBufferToPass.GetChunk().Base(), iSourceDimension,
            iSourceFormat, &iPostParams);

    return KErrNone;
}

TInt CPostingSurfaceImpl::SetClipRegion(const TRegion& aClipRegion) {
    LogOut(KPostingSurfaceCat, _L("Set clip region is not supported!"));
    return KErrNone;
}
