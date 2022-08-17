#include "audiooutput.h"
#include <e32std.h>

class CAudioOutputStub : public CAudioOutput {
    virtual TAudioOutputPreference AudioOutput() {
        return ENoPreference;
    }

    virtual TAudioOutputPreference DefaultAudioOutput() {
        return EPublic;
    }

    virtual void RegisterObserverL( MAudioOutputObserver& aObserver ) {

    }

    virtual TBool SecureOutput() {
        return ETrue;
    }

    virtual void SetAudioOutputL( TAudioOutputPreference aAudioOutput = ENoPreference ) {

    }

    virtual void SetSecureOutputL( TBool aSecure = EFalse ) {

    }

    virtual void UnregisterObserver( MAudioOutputObserver& aObserver ) {

    }
};


EXPORT_C CAudioOutput* CAudioOutput::NewL(CMdaAudioPlayerUtility& aUtility) {
    return new (ELeave) CAudioOutputStub;
}

EXPORT_C CAudioOutput* CAudioOutput::NewL(CMdaAudioRecorderUtility& aUtility, TBool aRecordStream) {
    return new (ELeave) CAudioOutputStub;
}

EXPORT_C CAudioOutput* CAudioOutput::NewL(CMdaAudioOutputStream& aUtility) {
    return new (ELeave) CAudioOutputStub;
}

EXPORT_C CAudioOutput* CAudioOutput::NewL(CMdaAudioToneUtility& aUtility) {
    return new (ELeave) CAudioOutputStub;
}

EXPORT_C CAudioOutput* CAudioOutput::NewL(CMMFDevSound& aDevSound) {
    return new (ELeave) CAudioOutputStub;
}

EXPORT_C CAudioOutput* CAudioOutput::NewL(MCustomInterface& aUtility) {
    return new (ELeave) CAudioOutputStub;
}

EXPORT_C CAudioOutput* CAudioOutput::NewL(MCustomCommand& aUtility) {
    return new (ELeave) CAudioOutputStub;
}

EXPORT_C CAudioOutput* CAudioOutput::NewL(CMidiClientUtility& aUtility) {
    return new (ELeave) CAudioOutputStub;
}

EXPORT_C CAudioOutput* CAudioOutput::NewL(CDrmPlayerUtility& aUtility) {
    return new (ELeave) CAudioOutputStub;
}

EXPORT_C CAudioOutput* CAudioOutput::NewL(CVideoPlayerUtility& aUtility) {
    return new (ELeave) CAudioOutputStub;
}