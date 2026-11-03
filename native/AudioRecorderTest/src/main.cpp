// Copyright (c) 2026 EKA2L1 Team.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <e32base.h>
#include <f32file.h>
#include <mdaaudiosampleeditor.h>
#include <mmf/common/mmfcontrollerpluginresolver.h>

_LIT(KClip, "C:\\private\\e0002a21\\clip.wav");
_LIT(KMissingClip, "C:\\private\\e0002a21\\missing.wav");
_LIT(KResults, "C:\\private\\e0002a21\\results.txt");

class CTimeout : public CTimer {
public:
    CActiveSchedulerWait iWait;

    CTimeout() : CTimer(EPriorityStandard) {}
    void ConstructL() {
        CTimer::ConstructL();
        CActiveScheduler::Add(this);
    }
    void RunL() { iWait.AsyncStop(); }
    void Wait(TInt aMicroseconds) {
        After(aMicroseconds);
        iWait.Start();
        Cancel();
    }
};

class CRecorderTest : public CBase, public MMdaObjectStateChangeObserver {
public:
    CMdaAudioRecorderUtility *iRecorder;
    CTimeout *iTimeout;
    RFile iLog;
    TInt iCalls;
    CBase *iObject;
    TInt iPrevious;
    TInt iCurrent;
    TInt iError;
    TInt iChecks;
    TPtrC8 iCase;

    ~CRecorderTest() {
        delete iRecorder;
        delete iTimeout;
        iLog.Close();
    }

    void ConstructL(RFs &aFs) {
        User::LeaveIfError(iLog.Replace(aFs, KResults, EFileWrite));
        iTimeout = new (ELeave) CTimeout;
        iTimeout->ConstructL();
        iRecorder = CMdaAudioRecorderUtility::NewL(*this);
    }

    void CheckL(TBool aCondition, const TDesC8 &aName) {
        TBuf8<192> line;
        line.Append(aCondition ? _L8("PASS ") : _L8("FAIL "));
        line.Append(iCase);
        line.Append(_L8(": "));
        line.Append(aName);
        line.Append(_L8("\r\n"));
        User::LeaveIfError(iLog.Write(line));
        User::LeaveIfError(iLog.Flush());
        if (!aCondition) {
            User::Leave(KErrGeneral);
        }
        ++iChecks;
    }

    void MoscoStateChangeEvent(CBase *aObject, TInt aPrevious, TInt aCurrent, TInt aError) {
        ++iCalls;
        iObject = aObject;
        iPrevious = aPrevious;
        iCurrent = aCurrent;
        iError = aError;
        if (iTimeout->iWait.IsStarted()) {
            iTimeout->iWait.AsyncStop();
        }
    }

    void OpenL(const TDesC8 &aName, const TDesC &aFile, TUid aController, TInt aExpectedError, TUid aFormat = KNullUid) {
        iCase.Set(aName);
        iCalls = 0;
        TRAPD(err, iRecorder->OpenFileL(aFile, KNullUid, aController, aFormat));
        CheckL((err == KErrNone) && (iCalls == 0), _L8("open completes asynchronously"));
        iTimeout->Wait(5000000);
        CheckL((iCalls == 1) && (iObject == iRecorder), _L8("callback identifies the public utility"));
        TBuf8<160> detail;
        detail.Format(_L8("controller=%08x format=%08x previous=%d current=%d error=%d\r\n"),
            aController.iUid, aFormat.iUid, iPrevious, iCurrent, iError);
        User::LeaveIfError(iLog.Write(detail));
        CheckL((iPrevious == CMdaAudioClipUtility::ENotReady) &&
            (iCurrent == ((aExpectedError == KErrNone) ? CMdaAudioClipUtility::EOpen : CMdaAudioClipUtility::ENotReady)) &&
            (iError == aExpectedError), _L8("open state and error"));
    }

    void RunL(TUid aController, TUid aFormat) {
        OpenL(_L8("valid file"), KClip, aController, KErrNone);
        CheckL(iRecorder->State() == CMdaAudioClipUtility::EOpen, _L8("State reports an opened clip"));
        OpenL(_L8("reopen"), KClip, aController, KErrNone);
        iRecorder->Close();
        OpenL(_L8("explicit format"), KClip, aController, KErrNone, aFormat);
        iRecorder->Close();
        OpenL(_L8("missing file"), KMissingClip, aController, KErrNotFound);
        iRecorder->Close();
        OpenL(_L8("unknown controller"), KClip, TUid::Uid(0xeffffffe), KErrNotFound);
        iRecorder->Close();
        OpenL(_L8("null controllers"), KClip, KNullUid, KErrNotSupported);
        iRecorder->Close();

        iCase.Set(_L8("cancel open"));
        iCalls = 0;
        iRecorder->OpenFileL(KClip, KNullUid, aController);
        iRecorder->Close();
        iTimeout->Wait(200000);
        CheckL(iCalls == 0, _L8("Close cancels a pending open"));

        iRecorder->OpenFileL(KClip, KNullUid, aController);
        delete iRecorder;
        iRecorder = NULL;
        iTimeout->Wait(200000);
        CheckL(iCalls == 0, _L8("destruction cancels a pending open"));
        TBuf8<64> result;
        result.Format(_L8("RESULT PASS checks=%d\r\n"), iChecks);
        User::LeaveIfError(iLog.Write(result));
        User::LeaveIfError(iLog.Flush());
    }
};

static TUid PlaybackControllerL(TUid &aFormat) {
    CMMFControllerPluginSelectionParameters *selection = CMMFControllerPluginSelectionParameters::NewLC();
    CMMFFormatSelectionParameters *format = CMMFFormatSelectionParameters::NewLC();
    format->SetMatchToFileNameL(KClip);
    selection->SetRequiredPlayFormatSupportL(*format);
    RMMFControllerImplInfoArray controllers;
    CleanupResetAndDestroyPushL(controllers);
    selection->ListImplementationsL(controllers);
    if (controllers.Count() == 0) {
        User::Leave(KErrNotFound);
    }
    const TUid uid = controllers[0]->Uid();
    aFormat = KNullUid;
    const RMMFFormatImplInfoArray &formats = controllers[0]->PlayFormats();
    for (TInt i = 0; i < formats.Count(); ++i) {
        if (formats[i]->SupportsFileExtension(_L8(".wav"))) {
            aFormat = formats[i]->Uid();
            break;
        }
    }
    if (aFormat == KNullUid) {
        User::Leave(KErrNotFound);
    }
    CleanupStack::PopAndDestroy(3, selection);
    return uid;
}

static void RunTestsL() {
    CActiveScheduler *scheduler = new (ELeave) CActiveScheduler;
    CleanupStack::PushL(scheduler);
    CActiveScheduler::Install(scheduler);
    RFs fs;
    User::LeaveIfError(fs.Connect());
    CleanupClosePushL(fs);
    const TInt pathError = fs.CreatePrivatePath(EDriveC);
    if (pathError != KErrAlreadyExists) {
        User::LeaveIfError(pathError);
    }
    fs.Delete(KMissingClip);

    RFile clip;
    User::LeaveIfError(clip.Replace(fs, KClip, EFileWrite));
    CleanupClosePushL(clip);
    const TUint8 header[] = {
        'R','I','F','F', 0x64,6,0,0, 'W','A','V','E', 'f','m','t',' ',
        16,0,0,0, 1,0,1,0, 0x40,0x1f,0,0, 0x80,0x3e,0,0, 2,0,16,0,
        'd','a','t','a', 0x40,6,0,0
    };
    TBuf8<1644> data;
    data.FillZ(data.MaxLength());
    Mem::Copy(const_cast<TUint8 *>(data.Ptr()), header, sizeof(header));
    User::LeaveIfError(clip.Write(data));
    CleanupStack::PopAndDestroy(&clip);

    CRecorderTest *test = new (ELeave) CRecorderTest;
    CleanupStack::PushL(test);
    test->ConstructL(fs);
    TUid format;
    const TUid controller = PlaybackControllerL(format);
    test->RunL(controller, format);
    CleanupStack::PopAndDestroy(3, scheduler);
}

GLDEF_C TInt E32Main() {
    CTrapCleanup *cleanup = CTrapCleanup::New();
    if (!cleanup) {
        return KErrNoMemory;
    }
    TRAPD(err, RunTestsL());
    delete cleanup;
    return err;
}
