inline CPostingSurface::TPostingCapab::TPostingCapab()
    : iMaxSourceSize(0)
    , iMaxPixelSize( 0, 0 )
    , iSupportedRotations(0)
    , iSupportsMirroring(EFalse)
    , iSupportsScaling(EFalse)
    , iSupportsBrightnessControl(EFalse)
    , iSupportsContrastControl(EFalse)
    , iSupportedPostingBuffering(EBufferingInvalid)
    , iSupportedBufferTypes(EBufferTypeInvalid) {
}

inline CPostingSurface::TPostingSourceParams::TPostingSourceParams()
    : iPostingBuffering(EBufferingInvalid)
    , iBufferType(EBufferTypeInvalid)
    , iPostingUsage(EUsageInvalid)
    , iSourceImageSize( 0, 0 )
    , iPostingFormat(EFormatInvalidValue)
    , iPixelAspectRatioNum(0)
    , iPixelAspectRatioDenom(0)
    , iContentCopyRestricted(EFalse) {
}

inline CPostingSurface::TPostingParams::TPostingParams()
    : iDisplayedRect( 0, 0, 0, 0 )
    , iScaleToRect( 0, 0, 0, 0 )
    , iInputCrop( 0, 0, 0, 0 )
    , iMirror(EFalse)
    , iRotation(ENoRotation)
    , iBrightness(0)
    , iContrast(0)
    , iBackGround(0) {
}