/*
 * Copyright (c) 2018 EKA2L1 Team / 2009 Nokia
 * 
 * This file is part of EKA2L1 project / Symbian Open Source Project
 * (see bentokun.github.com/EKA2L1).
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

#pragma once

enum TAppListServRanges {
    EFirstUnrestrictedOpcodeInAppListServ,
    EFirstOpcodeNeedingWriteDeviceDataInAppListServ = 100,
};

enum TAppListRequest {
    EAppListServFirst = EFirstUnrestrictedOpcodeInAppListServ, // = 0
    EAppListServInitFullList,
    EAppListServInitEmbedList,
    EAppListServGetNextApp,
    EAppListServEmbedCount,
    EAppListServAppCount,
    EAppListServGetAppInfo,
    EAppListServGetAppCapability,
    EAppListServStartAppWithoutReturningThreadId, // deprecated
    EAppListServStartAppReturningThreadId, // deprecated
    EAppListServRecognizeData, // = 10
    EAppListServRecognizeDataPassedByFileHandle,
    EAppListServRecognizeSpecificData,
    EAppListServRecognizeSpecificDataPassedByFileHandle,
    EAppListServAppForDataType,
    EAppListServStartDocument, // deprecated
    EAppListServStartDocumentByDataType, // deprecated
    EAppListServStartDocumentByUid, // deprecated
    EAppListServCreateDocumentByUid, // deprecated
    EAppListServGetExecutableNameGivenDocument,
    EAppListServGetExecutableNameGivenDocumentPassedByFileHandle, // = 20
    EAppListServGetExecutableNameGivenDataType,
    EAppListServGetExecutableNameGivenAppUid,
    EAppListServGetOpaqueData,
    EAppListServGetNativeExecutableNameIfNonNative,
    EAppListServAppIconByUid,
    EAppListServAppForDocument,
    EAppListServAppForDocumentPassedByFileHandle,
    EAppListServGetConfidence,
    EAppListServGetBufSize,
    EAppListServSetBufSize, // = 30
    EAppListServGetDataTypesPhase1,
    EAppListServGetDataTypesPhase2,
    ESetNotify,
    ECancelNotify,
    EDCloseServer,
    EAppListServAppIconByUidAndSize,
    EAppListServGetAppIconSizes,
    EAppListServGetAppViews,
    EAppListServViewIconByUidAndSize,
    EAppListServGetFileOwnershipInfo, // = 40
    EAppListServNumberOfOwnDefinedIcons,
    EAppListServInitFilteredEmbedList,
    EAppListServInitAttrFilteredList,
    EAppListServAppIconFileName,
    EAppListServAppViewIconFileName,
    EAppListServInitServerAppList,
    EAppListServGetAppServices,
    EAppListServGetServiceImplementations,
    EAppListServGetServiceImplementationsDataType,
    EAppListServGetAppServiceUids, // = 50
    EAppListServGetAppServiceOpaqueData,
    EAppListServAppForDataTypeAndService,
    EAppListServAppForDocumentAndService,
    EAppListServAppForDocumentAndServicePassedByFileHandle,
    EAppListServApplicationLanguage,
    ERegisterListPopulationCompleteObserver,
    ECancelListPopulationCompleteObserver,
    EAppListServPreferredBufSize,
    EAppListServRecognizeFiles,
    EAppListServRecognizeFilesAsync, // = 60
    ECancelRecognizeFiles,
    EAppListServTransferRecognitionResult,
    EAppListServGetAppByDataType,
    EAppListServGetDefaultScreenNumber,
    EAppListServRuleBasedLaunching,
    EAppListServRegisterNonNativeApplicationType,
    EAppListServDeregisterNonNativeApplicationType,
    EAppListServPrepareNonNativeApplicationsUpdates,
    EAppListServRegisterNonNativeApplication,
    EAppListServDeregisterNonNativeApplication, // = 70
    EAppListServCommitNonNativeApplications,
    EAppListServRollbackNonNativeApplications,
    EAppListServGetAppType,
    ENotifyOnDataMappingChange,
    ECancelNotifyOnDataMappingChange,
    EMatchesSecurityPolicy,
    EAppListServSetAppShortCaption,
    EAppListServForceRegistration,
    EDebugHeapMark,
    EDebugHeapMarkEnd, // = 80
    EDebugHeapFailNext,
    EDebugClearAppInfoArray,
    EDebugFlushRecognitionCache,
    EDebugSetLoadRecognizersOnDemand,
    EDebugPerformOutstandingRecognizerUnloading,
    EAppListServAppIconFileHandle,
    EDebugAddFailingNonNativeApplicationsUpdate,
    EDebugAddPanicingNonNativeApplicationsUpdate,
    EDebugAddRollbackPanicingNonNativeApplicationsUpdate, // = 89
    EAppListServUpdateAppList,
    EAppListUpdatedAppsInfo,
    EAppListServAppInfoProvidedByRegistrationFile = 99, // = 99
    //WriteDeviceData Capability requirement
    // ER5
    EAppListServSetConfidence = EFirstOpcodeNeedingWriteDeviceDataInAppListServ, // = 100
    // 8.1
    EAppListInsertDataMapping,
    EAppListInsertDataMappingIfHigher,
    EAppListDeleteDataMapping,
    // End Marker no Capability
    EAppListFirstUnusedOpcode,
};