#ifndef AUDIOOUTPUT_H
#define AUDIOOUTPUT_H

#include <e32base.h>
#include <mdaaudiotoneplayer.h>
#include <mdaaudiooutputstream.h>
#include <midiclientutility.h>
#include <MCustomInterface.h>

class MAudioOutputObserver;
class MCustomCommand;
class CMdaAudioPlayerUtility;
class CMdaAudioRecorderUtility;
class CMMFDevSound;
class CDrmPlayerUtility;
class CVideoPlayerUtility;

class CAudioOutput : public CBase{
public:
    IMPORT_C static CAudioOutput* NewL(CMdaAudioPlayerUtility& aUtility);
    IMPORT_C static CAudioOutput* NewL(CMdaAudioRecorderUtility& aUtility, TBool aRecordStream);
    IMPORT_C static CAudioOutput* NewL(CMdaAudioOutputStream& aUtility);
    IMPORT_C static CAudioOutput* NewL(CMdaAudioToneUtility& aUtility);
    IMPORT_C static CAudioOutput* NewL(CMMFDevSound& aDevSound);
    IMPORT_C static CAudioOutput* NewL(MCustomInterface& aUtility);
    IMPORT_C static CAudioOutput* NewL(MCustomCommand& aUtility);
    IMPORT_C static CAudioOutput* NewL(CMidiClientUtility& aUtility);
    IMPORT_C static CAudioOutput* NewL(CDrmPlayerUtility& aUtility);
    IMPORT_C static CAudioOutput* NewL(CVideoPlayerUtility& aUtility);

    enum TAudioOutputPreference {
        ENoPreference,
        EAll,
        ENoOutput,
        EPrivate,
        EPublic
    };

    virtual TAudioOutputPreference AudioOutput() = 0;
    virtual TAudioOutputPreference DefaultAudioOutput() = 0;

    virtual void RegisterObserverL( MAudioOutputObserver& aObserver ) = 0;
    virtual TBool SecureOutput() = 0;
    virtual void SetAudioOutputL( TAudioOutputPreference aAudioOutput = ENoPreference ) = 0;
    virtual void SetSecureOutputL( TBool aSecure = EFalse ) = 0;
    virtual void UnregisterObserver( MAudioOutputObserver& aObserver ) = 0;
};

#endif