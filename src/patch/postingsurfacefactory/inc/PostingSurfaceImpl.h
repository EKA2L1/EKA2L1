#ifndef POSTING_SURFACE_IMPL_H_
#define POSTING_SURFACE_IMPL_H_

#include <PostingSurface.h>

const TInt KMaxSourceDimXPixels = 4096;
const TInt KMaxSourceDimYPixels = 4096;

class T12Z1PostingBufferChunk: public CPostingSurface::TPostingBufferChunk {
public:
    T12Z1PostingBufferChunk();

    virtual TUint GetOffsetInChunk() {
        return 0;
    }

    void ConstructL(TSize aSize, CPostingSurface::TPostingFormat aFormat);
    void Free();
};

class CPostingSurfaceImpl: public CPostingSurface {
private:
    TInt iScreenIndex;
    TPostingCapab iCapabilities;
    
    TPostingFormat iSourceFormat;
    TSize iSourceDimension;

    TPostingParams iPostParams;
    T12Z1PostingBufferChunk iBufferToPass;
    
    TRequestStatus *iWaiter;

public:
    CPostingSurfaceImpl();
    void ConstructL(TInt aScreenIndex);
    
    virtual ~CPostingSurfaceImpl();

    virtual void InitializeL(const TPostingSourceParams& aSource, const TPostingParams& aDest);

    virtual void GetCapabilities(TPostingCapab& aCaps);
    virtual TBool FormatSupported(TPostingFormat aFormat);

    virtual TInt SetPostingParameters(const TPostingParams& aParams);
    virtual TInt SetClipRegion(const TRegion& aClipRegion);

    virtual TInt WaitForNextBuffer(TRequestStatus& aComplete);
    virtual TPostingBuff* NextBuffer();
    virtual TInt CancelBuffer();
    virtual TInt PostBuffer(TPostingBuff* aBuffer);

    virtual void Destroy(TRequestStatus& aComplete);
    virtual void Destroy();
};

#endif
