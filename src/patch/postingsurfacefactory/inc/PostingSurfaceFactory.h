#ifndef POSTING_SURFACE_FACTORY_H
#define POSTING_SURFACE_FACTORY_H

#include <e32base.h>

class CPostingSurface;

class CPostingSurfaceFactory : public CBase {
public:
    virtual ~CPostingSurfaceFactory();
    virtual CPostingSurface* NewPostingSurfaceL(TInt aDisplayIndex);
};

#endif
