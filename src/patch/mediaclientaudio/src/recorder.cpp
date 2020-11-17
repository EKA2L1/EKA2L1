/*
 * Copyright (c) 2020 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <mdaaudiosampleeditor.h>
#include <mda/common/resource.h>

#include <AudCommon.h>
#include <Log.h>

#include "common.h"
#include "dispatch.h"
#include "impl.h"

#include <e32std.h>

///============ RECORDER MAIN =================
EXPORT_C CMdaAudioRecorderUtility *CMdaAudioRecorderUtility::NewL(MMdaObjectStateChangeObserver &aObserver, CMdaServer* aServer, TInt aPriority, TMdaPriorityPreference aPref) {
	CMdaAudioRecorderUtility *util = new (ELeave) CMdaAudioRecorderUtility();
    CleanupStack::PushL(util);
    util->iProperties = CMMFMdaAudioRecorderUtility::NewL(aObserver, aPriority, aPref);
    CleanupStack::Pop(util);

    return util;
}

CMdaAudioRecorderUtility::~CMdaAudioRecorderUtility() {
	delete iProperties;
}

void CMdaAudioRecorderUtility::OpenFileL(const TDesC& aFileName) {
	iProperties->SupplyUrl(aFileName);
}

void CMdaAudioRecorderUtility::OpenDesL(const TDesC8& aDescriptor) {
    LogOut(KMcaCat, _L("Unimplemented function to open recorder utility with descriptor to play!"));
}

void CMdaAudioRecorderUtility::OpenL(TMdaClipLocation* aLocation, TMdaClipFormat* aFormat, TMdaPackage* aArg1,	
	TMdaPackage* aArg2) {
	if (!aLocation) {
		LogOut(KMcaCat, _L("Clip location is null!"));
		User::Leave(KErrArgument);

		return;
	}

	if (aLocation->Type() != KUidMdaClipLocation) {
		LogOut(KMcaCat, _L("Clip location package is corrupted!"));
		User::Leave(KErrArgument);

		return;
	}

	TBool supplyUrl = EFalse;

	// Check the UID to locate if it's a file location or descriptor location
	if (aLocation->Uid() == KUidMdaFileResLoc) {
		supplyUrl = ETrue;
	} else if (aLocation->Uid() == KUidMdaDesResLoc) {
		supplyUrl = EFalse;
	} else {
		LogOut(KMcaCat, _L("Unknown MDA clip location type 0x%08x"), aLocation->Uid().iUid);
		User::Leave(KErrArgument);

		return;
	}

	if (supplyUrl) {
		TMdaFileClipLocation *urlLocation = reinterpret_cast<TMdaFileClipLocation*>(aLocation);
		iProperties->SupplyUrl(urlLocation->iName);			// TODO mimetype tap
	} else {
		TMdaDesClipLocation *desLocation = reinterpret_cast<TMdaDesClipLocation*>(aLocation);
		iProperties->SupplyData(*desLocation->iDes);
	}

	if (aFormat->Type() != KUidMdaClipFormat) {
		LogOut(KMcaCat, _L("Audio format package is corrupted!"));
		User::Leave(KErrArgument);

		return;
	}

	User::LeaveIfError(iProperties->SetDestContainerFormat(aFormat->Uid().iUid));

	// Check out the encoding package now
	if (aArg1) {
		TFourCC auEncodingCC;

		switch (aArg1->Uid().iUid) {
		case KUidMdaAu16PcmCodecDefine:
			auEncodingCC = KMMFFourCCCodePCM16;
			break;

		case KUidMdaAu8PcmCodecDefine:
			auEncodingCC = KMMFFourCCCodePCM8;
			break;

		case KUidMdaAuMulawCodecDefine:
		case KUidMdaWavMulawCodecDefine:
			auEncodingCC = KMMFFourCCCodeMuLAW;
			break;

		case KUidMdaWavImaAdpcmCodecDefine:
			auEncodingCC = KMMFFourCCCodeIMAD;
			break;

		case KUidMdaWavPcmCodecDefine: {
			TMdaPcmWavCodec *pcmCodec = reinterpret_cast<TMdaPcmWavCodec*>(aArg1);

			switch (pcmCodec->iBits) {
			case TMdaPcmWavCodec::E8BitPcm:
				auEncodingCC = KMMFFourCCCodePCM8;
				break;

			case TMdaPcmWavCodec::E16BitPcm:
				auEncodingCC = KMMFFourCCCodePCM16;
				break;

			default:
				LogOut(KMcaCat, _L("Invalid pcm wav codec bits!"));

				User::Leave(KErrNotSupported);
				return;
			}

			break;
		}
		
		default:
			LogOut(KMcaCat, _L("Unsupported or unkown encoding UID 0x%08x"), aArg1->Uid().iUid);
			User::Leave(KErrNotSupported);

			return;
		}

		if (iProperties->SetDestCodec(auEncodingCC) != KErrNone) {
			LogOut(KMcaCat, _L("Error setting output codec in recorder utility. Ignored."));
		}
	}

	if (aArg2) {
		// Check record audio outut properties...
		TMdaAudioDataSettings *settings = reinterpret_cast<TMdaAudioDataSettings*>(aArg2);

		if ((settings->Type() != KUidMdaDataTypeSettings) || (settings->Uid() != KUidMdaMediaTypeAudio)) {
			LogOut(KMcaCat, _L("Settings package is corrupted!"));
			User::Leave(KErrArgument);

			return;
		}

		const TInt realSampleRate = ConvertFreqEnumToNumber(settings->iSampleRate);
		const TInt realChannelCount = ConvertChannelEnumToNum(settings->iChannels);

		if ((realSampleRate == -1) || (realChannelCount == -1)) {
			LogOut(KMcaCat, _L("Invalid destination sample rate or channel count enum in setting package, ignored"));
		} else {
			iProperties->SetDestSampleRate(realSampleRate);
			iProperties->SetDestChannelCount(realChannelCount);
		}
	}
}

void CMdaAudioRecorderUtility::SetAudioDeviceMode(CMdaAudioRecorderUtility::TDeviceMode aMode) {
	LogOut(KMcaCat, _L("Audio device mode set to %d"), static_cast<TInt>(aMode));
}

TInt CMdaAudioRecorderUtility::MaxVolume() {
	return iProperties->MaxVolume();
}

TInt CMdaAudioRecorderUtility::MaxGain() {
	LogOut(KMcaCat, _L("Unimplemented function getting recorder's max gain"));
	return 0;
}

void CMdaAudioRecorderUtility::SetVolume(TInt aVolume) {
	iProperties->SetVolume(aVolume);
}

void CMdaAudioRecorderUtility::SetGain(TInt aGain) {
	LogOut(KMcaCat, _L("Unimplemented function setting recorder's gain"));
}

void CMdaAudioRecorderUtility::SetVolumeRamp(const TTimeIntervalMicroSeconds& aRampDuration) {
	LogOut(KMcaCat, _L("Unimplemented function setting recorder's volume ramp"));
}

CMdaAudioClipUtility::TState CMdaAudioRecorderUtility::State() {
	CMdaAudioClipUtility::TState destState = CMdaAudioClipUtility::ENotReady;
	if (!TranslateInternalStateToReleasedState(iProperties->State(), destState)) {
		return CMdaAudioClipUtility::ENotReady;
	}
	
	return destState;
}

void CMdaAudioRecorderUtility::Close() {
	Stop();
}

void CMdaAudioRecorderUtility::PlayL() {
	LogOut(KMcaCat, _L("Try to play bzzz bzzz"));
	iProperties->Play();
}

void CMdaAudioRecorderUtility::RecordL() {
	LogOut(KMcaCat, _L("Unimplemented feature recording!"));
}

void CMdaAudioRecorderUtility::Stop() {
	iProperties->Stop();
}

void CMdaAudioRecorderUtility::CropL() {
	LogOut(KMcaCat, _L("Unimplement feature cropping!"));
}

void CMdaAudioRecorderUtility::SetPosition(const TTimeIntervalMicroSeconds& aPosition) {
	iProperties->SetCurrentPosition(aPosition);
}

const TTimeIntervalMicroSeconds& CMdaAudioRecorderUtility::Position() {
	return iProperties->CurrentPosition();
}

const TTimeIntervalMicroSeconds& CMdaAudioRecorderUtility::RecordTimeAvailable() {
	LogOut(KMcaCat, _L("Unimplemented feature get available record time"));

#ifdef EKA2
	return 0;
#else
	TInt64 returnVal(0);
	return TTimeIntervalMicroSeconds(returnVal);
#endif
}

const TTimeIntervalMicroSeconds& CMdaAudioRecorderUtility::Duration() {
	return iProperties->Duration();
}

void CMdaAudioRecorderUtility::SetPlayWindow(const TTimeIntervalMicroSeconds& aStart, const TTimeIntervalMicroSeconds& aEnd) {
	LogOut(KMcaCat, _L("Unimplemented function setting play window"));
}

void CMdaAudioRecorderUtility::ClearPlayWindow() {
	LogOut(KMcaCat, _L("Unimplemented function clearing play window!"));
}

void CMdaAudioRecorderUtility::SetRepeats(TInt aRepeatNumberOfTimes, const TTimeIntervalMicroSeconds& aTrailingSilence) {
	if ((aRepeatNumberOfTimes < 0) && (aRepeatNumberOfTimes != KMdaRepeatForever)) {
		LogOut(KMcaCat, _L("Invalid repeat numbers %d, set repeats does not do anything"), aRepeatNumberOfTimes);
		return;
	}

	iProperties->SetRepeats(aRepeatNumberOfTimes, aTrailingSilence);
}

void CMdaAudioRecorderUtility::SetMaxWriteLength(TInt aMaxWriteLength) {
	LogOut(KMcaCat, _L("Unimplemented function setting max write length"));
}

void CMdaAudioRecorderUtility::CropFromBeginningL() {
	LogOut(KMcaCat, _L("Unimplemented function cropping from beginning!"));
}

#if (MCA_NEW >= 3)
EXPORT_C void CMdaAudioRecorderUtility::OpenFileL(const RFile& aFile) {
	TFullName filePath;
	aFile.FullName(filePath);

	iProperties->SupplyUrl(filePath);
}

EXPORT_C void CMdaAudioRecorderUtility::OpenFileL(const TMMSource& aSource) {
	LogOut(KMcaCat, _L("Unimplemented open file for recorder utility with MMSource!"));
}

EXPORT_C void CMdaAudioRecorderUtility::OpenFileL(const RFile& aFile, TUid aRecordControllerUid, TUid aPlaybackControllerUid, TUid aDestinationFormatUid,
    TFourCC aDestinationDataType) {
	LogOut(KMcaCat, _L("Unimplemented function open file using RFile with custom controller UID"));
}

EXPORT_C void CMdaAudioRecorderUtility::OpenFileL(const TMMSource& aSource, TUid aRecordControllerUid, TUid aPlaybackControllerUid, TUid aDestinationFormatUid,
    TFourCC aDestinationDataType) {
	LogOut(KMcaCat, _L("Unimplemented function open file using MMSource with custom controller UID"));
}

EXPORT_C TMMFDurationInfo CMdaAudioRecorderUtility::Duration(TTimeIntervalMicroSeconds& aDuration) {
	LogOut(KMcaCat, _L("Unimplemented function getting duration with returned detail!"));
	return EMMFDurationInfoUnknown;
}

EXPORT_C MMMFDRMCustomCommand* CMdaAudioRecorderUtility::GetDRMCustomCommand() {
	LogOut(KMcaCat, _L("Get DRM custom command unimplemented!"));
	return NULL;
}

EXPORT_C TInt CMdaAudioRecorderUtility::RegisterAudioResourceNotification(MMMFAudioResourceNotificationCallback& aCallback,TUid aNotificationEventUid,const TDesC8& aNotificationRegistrationData) {
	LogOut(KMcaCat, _L("Register audio resource notification unimplemented!"));
	return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioRecorderUtility::CancelRegisterAudioResourceNotification(TUid aNotificationEventId) {
	LogOut(KMcaCat, _L("Cancel register audio resource notification unimplemented"));
	return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioRecorderUtility::WillResumePlay() {
	LogOut(KMcaCat, _L("Will resume play unimplemented"));
	return KErrNone;
}
	
EXPORT_C TInt CMdaAudioRecorderUtility::SetThreadPriorityPlayback(const TThreadPriority& aThreadPriority) const {
	LogOut(KMcaCat, _L("Set thread priority playback ignored"));
	return KErrNone;
}

EXPORT_C TInt CMdaAudioRecorderUtility::SetThreadPriorityRecord(const TThreadPriority& aThreadPriority) const {
	LogOut(KMcaCat, _L("Set thread priority record ignored"));
	return KErrNone;
}

EXPORT_C void CMdaAudioRecorderUtility::UseSharedHeap() {
	LogOut(KMcaCat, _L("Using shared heap for recorder utility"));
}

#endif

#if (MCA_NEW >= 2)
EXPORT_C void CMdaAudioRecorderUtility::OpenFileL(const TDesC& aFileName, TUid aRecordControllerUid,
	TUid aPlaybackControllerUid, TUid aDestinationFormatUid, TFourCC aDestinationDataType) {
	LogOut(KMcaCat, _L("Unimplemented function to open recorder utility with file name to play/record!"));
}

EXPORT_C void CMdaAudioRecorderUtility::OpenDesL(TDes8& aDescriptor) {
    LogOut(KMcaCat, _L("Unimplemented function to open recorder utility with descriptor to play/record!"));
}

EXPORT_C void CMdaAudioRecorderUtility::OpenUrlL(const TDesC& aUrl, TInt aIapId, TUid aRecordControllerUid, TUid aPlaybackControllerUid,
    TUid aDestinationFormatUid, TFourCC aDestinationDataType) {
	LogOut(KMcaCat, _L("Unimplemented function open URL with custom controller UID"));
}

EXPORT_C void CMdaAudioRecorderUtility::OpenUrlL(const TDesC& aUrl, TInt aIapId, const TDesC8& aMimeType) {
	LogOut(KMcaCat, _L("Unimplemented function open URL with custom MIME type!"));
}

EXPORT_C void CMdaAudioRecorderUtility::OpenDesL(TDes8& aDescriptor, TUid aRecordControllerUid, TUid aPlaybackControllerUid,
	TUid aDestinationFormatUid, TFourCC aDestinationDataType) {
	LogOut(KMcaCat, _L("Unimplemented function open descriptor with custom controller ID"));
}

EXPORT_C TInt CMdaAudioRecorderUtility::GetGain(TInt& aGain) {
	LogOut(KMcaCat, _L("Get gain for recorder unimplemented!"));
	return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioRecorderUtility::GetVolume(TInt& aVolume) {
	aVolume = iProperties->GetVolume();
	return (aVolume < KErrNone) ? aVolume : KErrNone;
}

EXPORT_C TInt CMdaAudioRecorderUtility::SetPlaybackBalance(TInt aBalance) {
	return iProperties->SetBalance(aBalance);
}

EXPORT_C TInt CMdaAudioRecorderUtility::GetPlaybackBalance(TInt& aBalance) {
	aBalance = iProperties->GetBalance();
	return (aBalance < KErrNone) ? aBalance : KErrNone;
}

EXPORT_C TInt CMdaAudioRecorderUtility::SetRecordBalance(TInt aBalance) {
	LogOut(KMcaCat, _L("Set record balance unimplemented!"));
	return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioRecorderUtility::GetRecordBalance(TInt& aBalance) {
	LogOut(KMcaCat, _L("Get record balance unimplemented!"));
	return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioRecorderUtility::GetNumberOfMetaDataEntries(TInt& aNumEntries) {
	LogOut(KMcaCat, _L("Get number of metadata entries unimplemented!"));
	return KErrNotSupported;
}

EXPORT_C CMMFMetaDataEntry* CMdaAudioRecorderUtility::GetMetaDataEntryL(TInt aMetaDataIndex) {
	LogOut(KMcaCat, _L("Get metadata entry unimplemented!"));
	return NULL;
}

EXPORT_C void CMdaAudioRecorderUtility::AddMetaDataEntryL(CMMFMetaDataEntry& aMetaDataEntry) {
	LogOut(KMcaCat, _L("Add metadata entry unimplemented!"));
}

EXPORT_C TInt CMdaAudioRecorderUtility::RemoveMetaDataEntry(TInt aMetaDataIndex) {
	LogOut(KMcaCat, _L("Remove metadata entry unimplemented!"));
	return KErrNotSupported;
}

EXPORT_C void CMdaAudioRecorderUtility::ReplaceMetaDataEntryL(TInt aMetaDataIndex, CMMFMetaDataEntry& aMetaDataEntry) {
	LogOut(KMcaCat, _L("Replace metadata entry unimplemented"));
}

EXPORT_C void CMdaAudioRecorderUtility::SetPriority(TInt aPriority, TMdaPriorityPreference aPref) {
	LogOut(KMcaCat, _L("Set priority unimplemented!"));
}

EXPORT_C void CMdaAudioRecorderUtility::GetSupportedDestinationDataTypesL(RArray<TFourCC>& aSupportedDataTypes) {
	LogOut(KMcaCat, _L("Get supported destination data types unimplemented!"));
}

EXPORT_C void CMdaAudioRecorderUtility::SetDestinationDataTypeL(TFourCC aDataType) {
	LogOut(KMcaCat, _L("Set destination data type unimplemented!"));
}

EXPORT_C TFourCC CMdaAudioRecorderUtility::DestinationDataTypeL() {
	LogOut(KMcaCat, _L("Get destination data type unimplemented!"));
	return TFourCC(0, 0, 0, 0);
}

EXPORT_C void CMdaAudioRecorderUtility::SetDestinationBitRateL(TUint aBitRate) {
	LogOut(KMcaCat, _L("Set destination bitrate unimplemented!"));
}

EXPORT_C TUint CMdaAudioRecorderUtility::DestinationBitRateL() {
	LogOut(KMcaCat, _L("Destination bitrate unimplemented!"));
	return 0;
}

#if (MCA_NEW >= 3)
EXPORT_C TUint CMdaAudioRecorderUtility::SourceBitRateL() {
	TUint bitRate = 0;
	TInt err = iProperties->BitRate(bitRate);
	
	if (err < KErrNone) {
		LogOut(KMcaCat, _L("Error while getting source bitrate!"));
		return 0;
	}

	return bitRate;
}
#endif

EXPORT_C void CMdaAudioRecorderUtility::GetSupportedBitRatesL(RArray<TUint>& aSupportedBitRates) {
	LogOut(KMcaCat, _L("Get supported bitrate unimplemented!"));
}

EXPORT_C void CMdaAudioRecorderUtility::SetDestinationSampleRateL(TUint aSampleRate) {
	User::LeaveIfError(iProperties->SetDestSampleRate(aSampleRate));
}

EXPORT_C TUint CMdaAudioRecorderUtility::DestinationSampleRateL() {
	const TInt result = iProperties->GetDestSampleRate();

	if (result < KErrNone) {
		User::Leave(result);
	}

	return static_cast<TUint>(result);
}

EXPORT_C void CMdaAudioRecorderUtility::GetSupportedSampleRatesL(RArray<TUint>& aSupportedSampleRates) {
	LogOut(KMcaCat, _L("Get supported sample rate unimplemented!"));
}

EXPORT_C void CMdaAudioRecorderUtility::SetDestinationFormatL(TUid aFormatUid) {
	User::LeaveIfError(iProperties->SetDestContainerFormat(aFormatUid.iUid));
}

EXPORT_C TUid CMdaAudioRecorderUtility::DestinationFormatL() {
	TUid returnUid;
	TUint32 returnUidValueU = 0;

	User::LeaveIfError(iProperties->GetDestContainerFormat(returnUidValueU));

	returnUid.iUid = static_cast<TInt32>(returnUidValueU);
	return returnUid;
}

EXPORT_C void CMdaAudioRecorderUtility::SetDestinationNumberOfChannelsL(TUint aNumberOfChannels) {
	LogOut(KMcaCat, _L("Set destination number of channels unimplemented!"));
}

EXPORT_C TUint CMdaAudioRecorderUtility::DestinationNumberOfChannelsL(){ 
	LogOut(KMcaCat, _L("Get destination number of channels unimplemented!"));
	return 0;
}

EXPORT_C void CMdaAudioRecorderUtility::GetSupportedNumberOfChannelsL(RArray<TUint>& aSupportedNumChannels) {
	LogOut(KMcaCat, _L("Get supported number of channels unimplemented!"));
}

EXPORT_C void CMdaAudioRecorderUtility::RegisterForAudioLoadingNotification(MAudioLoadingObserver& aCallback) {
	LogOut(KMcaCat, _L("Register for audio loading notification unimplemented!"));
}

EXPORT_C void CMdaAudioRecorderUtility::GetAudioLoadingProgressL(TInt& aPercentageComplete) {
	LogOut(KMcaCat, _L("Get audio loading progress stubbed"));
	aPercentageComplete = 100;
}

EXPORT_C const CMMFControllerImplementationInformation& CMdaAudioRecorderUtility::AudioPlayControllerImplementationInformationL() {
	LogOut(KMcaCat, _L("Unimplemented function get audio play impl info!"));
	CMMFControllerImplementationInformation *info = NULL;
	return *info;
}

EXPORT_C const CMMFControllerImplementationInformation& CMdaAudioRecorderUtility::AudioRecorderControllerImplementationInformationL() {
	LogOut(KMcaCat, _L("Unimplemented function get record play impl info!"));
	CMMFControllerImplementationInformation *info = NULL;
	return *info;
}

EXPORT_C TInt CMdaAudioRecorderUtility::RecordControllerCustomCommandSync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TDes8& aDataFrom) {
	LogOut(KMcaCat, _L("Record controller custom command sync unimplemented (has data from)!"));
	return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioRecorderUtility::RecordControllerCustomCommandSync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2) {
	LogOut(KMcaCat, _L("Record controller custom command sync unimplemented (no data from)!"));
	return KErrNotSupported;
}

EXPORT_C void CMdaAudioRecorderUtility::RecordControllerCustomCommandAsync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TDes8& aDataFrom, TRequestStatus& aStatus) {
	LogOut(KMcaCat, _L("Record controller custom command async unimplemented (has data from)!"));
}

EXPORT_C void CMdaAudioRecorderUtility::RecordControllerCustomCommandAsync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TRequestStatus& aStatus) {
	LogOut(KMcaCat, _L("Record controller custom command async unimplemented (no data from)!"));
}

EXPORT_C TInt CMdaAudioRecorderUtility::PlayControllerCustomCommandSync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TDes8& aDataFrom) {
	LogOut(KMcaCat, _L("Play controller custom command sync unimplemented (has data from)!"));
	return KErrNotSupported;
}

EXPORT_C TInt CMdaAudioRecorderUtility::PlayControllerCustomCommandSync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2) {
	LogOut(KMcaCat, _L("Play controller custom command sync unimplemented (no data from)!"));
	return KErrNotSupported;
}

EXPORT_C void CMdaAudioRecorderUtility::PlayControllerCustomCommandAsync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TDes8& aDataFrom, TRequestStatus& aStatus) {
	LogOut(KMcaCat, _L("Play controller custom command async unimplemented (has data from)!"));
}

EXPORT_C void CMdaAudioRecorderUtility::PlayControllerCustomCommandAsync(const TMMFMessageDestinationPckg& aDestination, TInt aFunction, const TDesC8& aDataTo1, const TDesC8& aDataTo2, TRequestStatus& aStatus) {
	LogOut(KMcaCat, _L("Play controller custom command async unimplemented! (no data from)"));
}
#endif

/// == END REPROXY ==
