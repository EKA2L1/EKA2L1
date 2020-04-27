#ifndef MEDIA_CLIENT_AUDIO_STREAM_DISPATCH_H_
#define MEDIA_CLIENT_AUDIO_STREAM_DISPATCH_H_

#include <e32std.h>

#define CALL_HLE_DISPATCH(FUNCID)      \
    asm volatile("mov r0, #" #FUNCID); \
    asm volatile("swi #0xFE");

#define HLE_DISPATCH_FUNC(ret, name, ...) \
    ret name(const TUint32 func_id, __VA_ARGS__)

// DSP functions
HLE_DISPATCH_FUNC(TAny *, EAudioDspOutStreamCreate, void *aNot);
HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamWrite, TAny *aInstance, const TUint8 *aData, const TUint32 aSize);

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamSetProperties, TAny *aInstance, TInt aFrequency, TInt aChannels);
HLE_DISPATCH_FUNC(TInt, EAudioDspStreamStart, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, EAudioDspStreamStop, TAny *aInstance);

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamGetSupportedFormats, TUint32 *aFourCCs, TUint32 &aListCount);
HLE_DISPATCH_FUNC(TInt, EAudioDspStreamSetFormat, TAny *aInstance, const TUint32 aFourCC);
HLE_DISPATCH_FUNC(TInt, EAudioDspStreamGetFormat, TAny *aInstance, TUint32 &aFourCC);

HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamSetVolume, TAny *aInstance, TInt aVolume);
HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamMaxVolume, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, EAudioDspOutStreamGetVolume, TAny *aInstance);

// Notify that a frame has been wrote to the audio driver's buffer on HLE side.
HLE_DISPATCH_FUNC(TInt, EAudioDspStreamNotifyBufferSentToDriver, TAny *aInstance, TRequestStatus &aStatus);
HLE_DISPATCH_FUNC(TInt, EAudioDspStreamNotifyEnd, TAny *aInstance, TRequestStatus &aStatus);
HLE_DISPATCH_FUNC(TInt, EAudioDspStreamCancelNotifyBufferSentToDriver, TAny *aInstance);

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamDestroy, TAny *aInstance);

HLE_DISPATCH_FUNC(TInt, EAudioDspStreamSetBalance, TAny *aInstance, TInt aBalance);
HLE_DISPATCH_FUNC(TInt, EAudioDspStreamGetBalance, TAny *aInstance);
HLE_DISPATCH_FUNC(TInt, EAudioDspStreamBytesRendered, TAny *aInstance, TUint64 &aBytesRendered);
HLE_DISPATCH_FUNC(TInt, EAudioDspStreamPosition, TAny *aInstance, TUint64 &aPosition);

#endif
