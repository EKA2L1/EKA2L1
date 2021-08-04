#include <e32base.h>
#include <e32std.h>
#include <f32file.h>
#include <mmf/server/sounddevice.h>

class CAudioFileFeeder : public MDevSoundObserver {
private:
	RFile iFile;
	RFile iLogFile;

	RFs iFs;
	
	CMMFDevSound *iDevSound;

public:
	CAudioFileFeeder(CMMFDevSound *aDevSound)
		: iDevSound(aDevSound) {
	}
	
	void ConstructL() {
		User::LeaveIfError(iFs.Connect(-1));
		iFs.SetSessionToPrivate(EDriveC);

		User::LeaveIfError(iFile.Open(iFs, _L("sample.mp3"), EFileRead | EFileShareAny));
		User::LeaveIfError(iLogFile.Replace(iFs, _L("E:\\testlog.log"), EFileWrite | EFileShareAny));
	}
	
	static CAudioFileFeeder *NewLC(CMMFDevSound *aDevSound) {
		CAudioFileFeeder *observer = new CAudioFileFeeder(aDevSound);
		CleanupStack::PushL(observer);

		observer->ConstructL();		
		return observer;
	}
	
	static CAudioFileFeeder *NewL(CMMFDevSound *aDevSound) {
		CAudioFileFeeder *observer = NewLC(aDevSound);
		CleanupStack::Pop(observer);
		
		return observer;
	}
	
	virtual void BufferToBeFilled(CMMFBuffer *aBuffer) {
        CMMFDataBuffer* dataBuffer = static_cast <CMMFDataBuffer*>(aBuffer);
        aBuffer->SetLastBuffer(EFalse);

		TDes8 &dataDes = dataBuffer->Data();
		TUint32 originalSize = dataDes.MaxLength();

        iFile.Read(dataDes);
		
        TBuf8<256> lineToPrint;
        lineToPrint.Format(_L8("Requested %d bytes, got %d bytes"), originalSize, dataDes.Length());
        
        RDebug::Printf((const char*)lineToPrint.Ptr());
        iLogFile.Write(lineToPrint);
        
        if (dataDes.Length() != originalSize) {
        	aBuffer->SetLastBuffer(ETrue);
        }
        
        iDevSound->PlayData();
	}
	
	virtual void PlayError(TInt error) {
		CActiveScheduler::Stop();
	}

	virtual void BufferToBeEmptied(CMMFBuffer *aBuffer) {
	}

	virtual void ConvertError(TInt aError) {
		
	}

	virtual void DeviceMessage(TUid aMessageType, const TDesC8 &aMsg) {
		
	}
	
	virtual void InitializeComplete(TInt aError) {
		
	}

	virtual void RecordError(TInt aError) {
		
	}
	
	virtual void ToneFinished(TInt aError) {
		
	}
};

LOCAL_C void MainL() {
	CMMFDevSound *devsound = CMMFDevSound::NewL();
	User::LeaveIfNull(devsound);

	CleanupStack::PushL(devsound);
	
	CAudioFileFeeder *observer = CAudioFileFeeder::NewL(devsound);
	CleanupStack::PushL(observer);
	
	devsound->InitializeL(*observer, ::KMMFFourCCCodeMP3, EMMFStatePlaying);
	devsound->SetVolume(6);

	TMMFCapabilities caps = devsound->Capabilities();
	caps.iChannels = EMMFStereo;
	caps.iRate = EMMFSampleRate44100Hz;
	caps.iEncoding = EMMFSoundEncoding16BitPCM;
	caps.iBufferSize = 8192;

	devsound->SetConfigL(caps);
	devsound->PlayInitL();
	devsound->PlayData();
	
	CActiveScheduler::Start();
	CleanupStack::Pop(2);
}

LOCAL_C void DoStartL() {
	// Create active scheduler (to run active objects)
	CActiveScheduler* scheduler = new (ELeave) CActiveScheduler();
	CleanupStack::PushL(scheduler);
	CActiveScheduler::Install(scheduler);

	MainL();

	// Delete active scheduler
	CleanupStack::PopAndDestroy(scheduler);
}

GLDEF_C TInt E32Main() {
	__UHEAP_MARK;
	CTrapCleanup* cleanup = CTrapCleanup::New();

	// Run application code inside TRAP harness, wait keypress when terminated
	TRAPD(mainError, DoStartL());

	delete cleanup;
	__UHEAP_MARKEND;
	return KErrNone;
}

