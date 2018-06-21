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