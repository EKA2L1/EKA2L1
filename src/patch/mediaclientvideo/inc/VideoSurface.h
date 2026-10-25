#ifndef MEDIACLIENTVIDEO_VIDEO_SURFACE_H
#define MEDIACLIENTVIDEO_VIDEO_SURFACE_H

#include <e32std.h>
#include <mmf/common/mmfvideo.h>

// The S60 5th public SDK omits the graphics-surface observer headers.
#ifndef __SURFACE_H__
#define __SURFACE_H__
class TSurfaceId {
public:
    TUint32 iInternal[4];
    static TSurfaceId CreateNullId() {
        TSurfaceId id = {{ 0, 0, 0, 0 }};
        return id;
    }
    TBool IsNull() const {
        return !(iInternal[0] | iInternal[1] | iInternal[2] | iInternal[3]);
    }
};
#endif

#ifndef SURFACEEVENTHANDLER_H
#define SURFACEEVENTHANDLER_H
class MMMFSurfaceEventHandler {
public:
    virtual void MmsehSurfaceCreated(TInt aDisplayId, const TSurfaceId &aId,
        const TRect &aCropRect, TVideoAspectRatio aAspectRatio) = 0;
    virtual void MmsehSurfaceParametersChanged(const TSurfaceId &aId,
        const TRect &aCropRect, TVideoAspectRatio aAspectRatio) = 0;
    virtual void MmsehRemoveSurface(const TSurfaceId &aId) = 0;
};
#endif
#endif
