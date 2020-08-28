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

#ifndef MDA_AUDIO_OUTPUT_STREAM_MAIN_H_
#define MDA_AUDIO_OUTPUT_STREAM_MAIN_H_

// This file is based on newest declaration of CMdaAudioOutputStream.
// To provide extra neccessary functions.

#include <e32std.h>
#include <mda/client/utility.h>
#include <mda/common/base.h>
#include <mmf/common/mmfaudio.h>
#include <mmf/common/mmfbase.h>
#include <mmf/common/mmfstandardcustomcommands.h>

#if !defined(__SERIES80__) && !defined(__SERIES60_1X__)
#define MMF_BASE_CLIENT_UTILITY
#endif

#ifdef MMF_BASE_CLIENT_UTILITY
#include <mmfclntutility.h>
#endif

class MMdaAudioOutputStreamCallback {
public:
    virtual void MaoscOpenComplete(TInt aError) = 0;
    virtual void MaoscBufferCopied(TInt aError, const TDesC8 &aBuffer) = 0;
    virtual void MaoscPlayComplete(TInt aError) = 0;
};

class CMMFMdaAudioOutputStream;

class CMdaAudioOutputStream : public CBase
#ifdef MMF_BASE_CLIENT_UTILITY
                            , public MMMFClientUtility
#endif
{
public:
    EXPORT_C static CMdaAudioOutputStream *NewL(MMdaAudioOutputStreamCallback &aCallBack,
        CMdaServer *aServer = NULL);

    EXPORT_C static CMdaAudioOutputStream *NewL(MMdaAudioOutputStreamCallback &aCallBack,
        TInt aPriority,
        TMdaPriorityPreference aPref = EMdaPriorityPreferenceTimeAndQuality);

    ~CMdaAudioOutputStream();

    virtual void SetAudioPropertiesL(TInt aSampleRate, TInt aChannels);
    virtual void Open(TMdaPackage *aSettings);
    virtual TInt MaxVolume();
    virtual TInt Volume();
    virtual void SetVolume(const TInt aNewVolume);
    virtual void SetPriority(TInt aPriority, TMdaPriorityPreference aPref);
    virtual void WriteL(const TDesC8 &aData);
    virtual void Stop();

    EXPORT_C TInt Pause();
    EXPORT_C TInt Resume();

    virtual const TTimeIntervalMicroSeconds &Position();

    EXPORT_C void SetBalanceL(TInt aBalance = KMMFBalanceCenter);
    EXPORT_C TInt GetBalanceL() const;
    EXPORT_C TInt GetBytes();
    EXPORT_C void SetDataTypeL(TFourCC aAudioType);
    EXPORT_C TFourCC DataType() const;
    
#ifdef MMF_BASE_CLIENT_UTILITY
    EXPORT_C TInt RegisterAudioResourceNotification(MMMFAudioResourceNotificationCallback &aCallback, TUid aNotificationEventUid, const TDesC8 &aNotificationRegistrationData = KNullDesC8);
    EXPORT_C TInt CancelRegisterAudioResourceNotification(TUid aNotificationEventId);
#endif

    EXPORT_C TInt WillResumePlay();

    EXPORT_C TAny *CustomInterface(TUid aInterfaceId);
    EXPORT_C TInt KeepOpenAtEnd();
    EXPORT_C TInt RequestStop();

private:
    CMdaAudioOutputStream();

private:
    CMMFMdaAudioOutputStream *iProperties;
};

#endif
