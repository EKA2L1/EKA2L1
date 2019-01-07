// Copyright (c) 1999-2010 Nokia Corporation and/or its subsidiary(-ies).
// All rights reserved.
// This component and the accompanying materials are made available
// under the terms of "Eclipse Public License v1.0"
// which accompanies this distribution, and is available
// at the URL "http://www.eclipse.org/legal/epl-v10.html".
//
// Initial Contributors:
// Nokia Corporation - initial contribution.
//
// Contributors:
//
// Description:
// Command numbers and structures for client to window server communications.
//
//

#pragma once

enum TWservMessages {
    EWservMessCommandBuffer,
    EWservMessShutdown,
    EWservMessInit,
    EWservMessFinish,
    EWservMessSyncMsgBuf,
    EWservMessAsynchronousService = 0x010000,
    EWservMessAnimDllAsyncCommand = 0x100000,
};

enum TWsClientOpcodes {
    EWsClOpDisconnect, //Keep this as the first enum value - used by test code
    EWsClOpClearHotKeys,
    EWsClOpSetHotKey,
    EWsClOpRestoreDefaultHotKey,
    EWsClOpComputeMode,
    EWsClOpEventReady,
    EWsClOpEventReadyCancel,
    EWsClOpGetEvent,
    EWsClOpPurgePointerEvents,
    EWsClOpRedrawReady,
    EWsClOpRedrawReadyCancel,
    EWsClOpGetRedraw,
    EWsClOpPriorityKeyReady,
    EWsClOpPriorityKeyReadyCancel,
    EWsClOpGetPriorityKey,
    EWsClOpSetShadowVector,
    EWsClOpShadowVector,
    EWsClOpSetKeyboardRepeatRate,
    EWsClOpGetKeyboardRepeatRate,
    EWsClOpSetDoubleClick,
    EWsClOpGetDoubleClickSettings,
    EWsClOpCreateWindowGroup,
    EWsClOpCreateWindow,
    EWsClOpCreateGc,
    EWsClOpCreateAnimDll,
    EWsClOpCreateScreenDevice,
    EWsClOpCreateSprite,
    EWsClOpCreatePointerCursor,
    EWsClOpStartSetCustomTextCursor,
    EWsClOpCompleteSetCustomTextCursor,
    EWsClOpCreateBitmap,
    EWsClOpNumWindowGroups,
    EWsClOpNumWindowGroupsAllPriorities,
    EWsClOpWindowGroupList,
    EWsClOpWindowGroupListAllPriorities,
    EWsClOpSetWindowGroupOrdinalPosition,
    EWsClOpGetWindowGroupHandle,
    EWsClOpGetWindowGroupOrdinalPriority,
    EWsClOpGetWindowGroupClientThreadId,
    EWsClOpGetWindowGroupNameFromIdentifier,
    EWsClOpGetFocusWindowGroup,
    EWsClOpGetDefaultOwningWindow,
    EWsClOpSendEventToWindowGroup,
    EWsClOpFindWindowGroupIdentifier,
    EWsClOpFindWindowGroupIdentifierThread,
    EWsClOpSetBackgroundColor,
    EWsClOpGetBackgroundColor,
    EWsClOpSetSystemPointerCursor,
    EWsClOpClearSystemPointerCursor,
    EWsClOpClaimSystemPointerCursorList,
    EWsClOpFreeSystemPointerCursorList,
    EWsClOpGetModifierState,
    EWsClOpSetModifierState,
    EWsClOpHeapCount,
    EWsClOpResourceCount,
    EWsClOpTestInvariant,
    EWsClOpHeapSetFail,
    EWsClOpRawEvent,
    EWsClOpSystemInfo,
    EWsClOpLogMessage,
    EWsClOpPasswordEntered,
    EWsClOpSendMessageToWindowGroup,
    EWsClOpSendOffEventsToShell,
    EWsClOpGetDefModeMaxNumColors,
    EWsClOpGetColorModeList,
    EWsClOpKeyEvent,
    EWsClOpSendEventToAllWindowGroup,
    EWsClOpSendEventToAllWindowGroupPriority,
    EWsClOpSetPointerCursorArea,
    EWsClOpPointerCursorArea,
    EWsClOpSetPointerCursorMode,
    EWsClOpPointerCursorMode,
    EWsClOpSetDefaultSystemPointerCursor,
    EWsClOpClearDefaultSystemPointerCursor,
    EWsClOpSetPointerCursorPosition,
    EWsClOpPointerCursorPosition,
    EWsClOpSetDefaultFadingParams,
    EWsClOpSendMessageToAllWindowGroups,
    EWsClOpSendMessageToAllWindowGroupsPriority,
    EWsClOpFetchMessage,
    EWsClOpPrepareForSwitchOff,
    EWsClOpCreateDirectScreenAccess,
    EWsClOpSetFaded,
    EWsClOpLogCommand,
    EWsClOpCreateClick,
    EWsClOpSendEventToOneWindowGroupPerClient,
    EWsClOpNoFlickerFree,
    EWsClOpSetFocusScreen,
    EWsClOpGetFocusScreen,
    EWsClOpWindowGroupListAndChain,
    EWsClOpWindowGroupListAndChainAllPriorities,
    EWsClOpSetClientCursorMode,
    EWsClOpClearAllRedrawStores,
    EWsClOpCreateGraphic,
    EWsClOpGraphicMessageReady,
    EWsClOpGetGraphicMessage,
    EWsClOpGraphicMessageCancel,
    EWsClOpNumWindowGroupsOnScreen,
    EWsClOpGetNumberOfScreens,
    EWsClOpGraphicAbortMessage,
    EWsClOpGraphicFetchHeaderMessage,
    EWsClOpDebugInfo,
    EWsClOpDebugInfoReplyBuf,
    EWsClOpRegisterSurface,
    EWsClOpUnregisterSurface,
    EWsClOpSetCloseProximityThresholds,
    EWsClOpSetHighPressureThresholds,
    EWsClOpGetEnterCloseProximityThreshold,
    EWsClOpGetExitCloseProximityThreshold,
    EWsClOpGetEnterHighPressureThreshold,
    EWsClOpGetExitHighPressureThreshold,
    EWsClOpCreateDrawableSource,
    EWsClOpCreateDirectScreenAccessRegionTrackingOnly,
    EWsClOpSendEffectCommand,
    EWsClOpRegisterTFXEffectBuf,
    EWsClOpRegisterTFXEffectIPC,
    EWsClOpUnregisterTFXEffect,
    EWsClOpUnregisterAllTFXEffect,
    EWsClOpOverrideEffectBuf,
    EWsClOpOverrideEffectIPC,
    EWsClOpIndicateAppOrientation,
    EWsClOpLastEnumValue //Keep this at the end - used by test code
};

enum TWsScreenDeviceOpcodes {
    EWsSdSetableBitFlag = 0x80000000,
    EWsSdOpGetScanLine = 0x0000,
    EWsSdOpPixel,
    EWsSdOpTwipsSize,
    EWsSdOpPixelSize,
    EWsSdOpFree,
    EWsSdOpHorizontalTwipsToPixels,
    EWsSdOpVerticalTwipsToPixels,
    EWsSdOpHorizontalPixelsToTwips,
    EWsSdOpVerticalPixelsToTwips,
    EWsSdOpDisplayMode,
    EWsSdOpRectCompare,
    EWsSdOpPointerRect,
    EWsSdOpCopyScreenToBitmap,
    EWsSdOpCopyScreenToBitmap2,
    EWsSdOpSetScreenSizeAndRotation,
    EWsSdOpSetScreenSizeAndRotation2,
    EWsSdOpGetDefaultScreenSizeAndRotation,
    EWsSdOpGetDefaultScreenSizeAndRotation2,
    EWsSdOpGetNumScreenModes,
    EWsSdOpSetScreenMode,
    EWsSdOpGetScreenModeSizeAndRotation,
    EWsSdOpGetScreenModeSizeAndRotation2,
    EWsSdOpSetScreenModeEnforcement,
    EWsSdOpScreenModeEnforcement,
    EWsSdOpSetModeRotation,
    EWsSdOpGetRotationList,
    EWsSdOpPaletteAttributes,
    EWsSdOpSetPalette,
    EWsSdOpGetPalette,
    EWsSdOpGetScreenMode,
    EWsSdOpGetDefaultScreenModeOrigin,
    EWsSdOpGetScreenModeOrigin,
    EWsSdOpGetScreenModeScale,
    EWsSdOpGetCurrentScreenModeScale,
    EWsSdOpSetAppScreenMode,
    EWsSdOpGetScreenModeScaledOrigin,
    EWsSdOpGetCurrentScreenModeScaledOrigin,
    EWsSdOpSetCurrentScreenModeAttributes,
    EWsSdOpGetCurrentScreenModeAttributes,
    EWsSdOpGetScreenNumber,
    EWsSdOpGetScreenSizeModeList,
    EWsSdOpGetScreenModeDisplayMode,
    EWsClOpSetBackLight,
    EWsSdOpExtensionsSupported,
    EWsSdOpXDcGetNumberOfResolutions,
    EWsSdOpXDcGetResolutionsList,
    EWsSdOpXDcGetConfiguration,
    EWsSdOpXDcSetConfiguration,
    EWsSdOpXDcGetPreferredDisplayVersion,
    EWsSdOpXDcNotifyOnDisplayChange,
    EWsSdOpXDcNotifyOnDisplayChangeCancel,
    EWsSdOpXDcDisplayChangeEventEnabled,
    EWsSdOpXDmMapExtent,
    EWsSdOpXTestScreenCapture,
    EWsSdOpIsScreenModeDynamic,
    EWsSdOpXTestScreenCaptureSize,
};

enum TWsGcOpcodes {
    EWsGcOpFree = 0x0000,
    EWsGcOpActivate,
    EWsGcOpDeactivate,
    EWsGcOpSetClippingRegion,
    EWsGcOpSetClippingRect,
    EWsGcOpCancelClippingRegion,
    EWsGcOpCancelClippingRect,
    EWsGcOpSetWordJustification,
    EWsGcOpSetCharJustification,
    EWsGcOpSetPenColor,
    EWsGcOpSetPenStyle,
    EWsGcOpSetPenSize,
    EWsGcOpSetBrushColor,
    EWsGcOpSetBrushStyle,
    EWsGcOpSetBrushOrigin,
    EWsGcOpUseBrushPattern,
    EWsGcOpDiscardBrushPattern,
    EWsGcOpDrawArc,
    EWsGcOpDrawLine,
    EWsGcOpPlot,
    EWsGcOpDrawTo,
    EWsGcOpDrawBy,
    EWsGcOpDrawPolyLine,
    EWsGcOpDrawPolyLineContinued, //Quater Way

    EWsGcOpDrawPie,
    EWsGcOpDrawRoundRect,
    EWsGcOpDrawPolygon,
    EWsGcOpStartSegmentedDrawPolygon,
    EWsGcOpSegmentedDrawPolygonData,
    EWsGcOpDrawSegmentedPolygon,
    EWsGcOpDrawBitmap,
    EWsGcOpDrawBitmap2,
    EWsGcOpDrawBitmap3,
    EWsGcOpDrawBitmapMasked,
    EWsGcOpWsDrawBitmapMasked,
    EWsGcOpDrawText,
    EWsGcOpDrawTextPtr,
    EWsGcOpDrawTextInContextPtr,
    EWsGcOpDrawTextInContext,
    EWsGcOpDrawTextVertical,
    EWsGcOpDrawTextInContextVertical,
    EWsGcOpDrawTextInContextVerticalPtr,
    EWsGcOpDrawTextVerticalPtr,
    EWsGcOpDrawBoxTextOptimised1,
    EWsGcOpDrawBoxTextOptimised2,
    EWsGcOpDrawBoxTextInContextOptimised1,
    EWsGcOpDrawBoxTextInContextOptimised2,
    EWsGcOpDrawBoxText,
    EWsGcOpDrawBoxTextInContext,
    EWsGcOpDrawBoxTextPtr,
    EWsGcOpDrawBoxTextInContextPtr,
    EWsGcOpDrawBoxTextVertical,
    EWsGcOpDrawBoxTextInContextVertical,
    EWsGcOpDrawBoxTextVerticalPtr,
    EWsGcOpDrawBoxTextInContextVerticalPtr,
    EWsGcOpMoveBy,
    EWsGcOpMoveTo,
    EWsGcOpSetOrigin,
    EWsGcOpCopyRect,
    EWsGcOpDrawRect,
    EWsGcOpDrawEllipse, //Half Way

    EWsGcOpGdiBlt2,
    EWsGcOpGdiBlt3,
    EWsGcOpGdiBltMasked,
    EWsGcOpGdiWsBlt2,
    EWsGcOpGdiWsBlt3,
    EWsGcOpGdiWsBltMasked,
    EWsGcOpSize,
    EWsGcOpUseFont,
    //Two unused codes deleted
    EWsGcOpDiscardFont = EWsGcOpUseFont + 3,
    EWsGcOpSetUnderlineStyle,
    EWsGcOpSetStrikethroughStyle,
    EWsGcOpSetDrawMode,
    EWsGcOpTestInvariant,
    EWsGcOpClearRect,
    EWsGcOpClear,
    EWsGcOpReset,
    EWsGcOpSetDitherOrigin,
    EWsGcOpMapColors,
    EWsGcOpDrawWsGraphic, //PREQ1246 Bravo
    EWsGcOpDrawWsGraphicPtr,
    //
    // Local opcodes used internally access GC drawing code
    //
    EWsGcOpDrawPolyLineLocal,
    EWsGcOpDrawPolyLineLocalBufLen,
    EWsGcOpDrawPolygonLocal,
    EWsGcOpDrawPolygonLocalBufLen,
    EWsGcOpDrawTextLocal,
    EWsGcOpDrawTextInContextLocal,
    EWsGcOpDrawBoxTextLocal,
    EWsGcOpDrawBoxTextInContextLocal,
    EWsGcOpDrawBitmapLocal,
    EWsGcOpDrawBitmap2Local,
    EWsGcOpDrawBitmap3Local,
    EWsGcOpDrawBitmapMaskedLocal,
    EWsGcOpGdiBlt2Local,
    EWsGcOpGdiBlt3Local,
    EWsGcOpGdiBltMaskedLocal,

    //
    // Local opcodes used when reading long strings
    //
    EWsGcOpDrawTextPtr1,
    EWsGcOpDrawTextInContextPtr1,
    EWsGcOpDrawTextVerticalPtr1,
    EWsGcOpDrawTextInContextVerticalPtr1,
    EWsGcOpDrawBoxTextPtr1,
    EWsGcOpDrawBoxTextInContextPtr1,
    EWsGcOpDrawBoxTextVerticalPtr1,
    EWsGcOpDrawBoxTextInContextVerticalPtr1,

    //
    // New functions added leaving a space just in case it may be useful
    //
    EWsGcOpSetFaded = 200,
    EWsGcOpSetFadeParams,
    EWsGcOpGdiAlphaBlendBitmaps,
    EWsGcOpGdiWsAlphaBlendBitmaps,
    EWsGcOpSetOpaque,
    EWsGcOpMapColorsLocal,
    EWsGcOpAlphaBlendBitmapsLocal,
    EWsGcOpSetShadowColor,
    EWsGcOpSetDrawTextInContext,
    EWsGcOpDrawResourceToPos,
    EWsGcOpDrawResourceToRect,
    EWsGcOpDrawResourceFromRectToRect,
    EWsGcOpDrawResourceWithData,
    //
    // Codes for internal use only
    //
    EWsStoreAllGcAttributes = 1000,
    EWsStoreClippingRegion,
    EWsGcOpFlagDrawOp = 0x8000
};

enum TWsAnimDllOpcode {
    EWsAnimDllOpCreateInstance = 0x0000,
    EWsAnimDllOpCommand,
    EWsAnimDllOpCommandReply,
    EWsAnimDllOpDestroyInstance,
    EWsAnimDllOpFree,
    EWsAnimDllOpCreateInstanceSprite,
};

enum TWsWindowOpcodes {
    EWsWinOpFree = 0x0000,
    EWsWinOpSetExtent,
    EWsWinOpSetExtentErr,
    EWsWinOpOrdinalPosition,
    EWsWinOpFullOrdinalPosition,
    EWsWinOpSetOrdinalPosition,
    EWsWinOpSetOrdinalPositionPri,
    EWsWinOpSetOrdinalPriorityAdjust,
    EWsWinOpSetPos,
    EWsWinOpSetSize,
    EWsWinOpSetSizeErr,
    EWsWinOpPosition,
    EWsWinOpAbsPosition,
    EWsWinOpSize,
    EWsWinOpActivate,
    EWsWinOpInvalidate,
    EWsWinOpInvalidateFull,
    EWsWinOpBeginRedraw,
    EWsWinOpBeginRedrawFull,
    EWsWinOpEndRedraw,
    EWsWinOpTestInvariant,
    EWsWinOpParent,
    EWsWinOpPrevSibling,
    EWsWinOpNextSibling,
    EWsWinOpChild,
    EWsWinOpInquireOffset,
    EWsWinOpPointerFilter,
    EWsWinOpSetPointerCapture,
    EWsWinOpSetPointerGrab,
    EWsWinOpClaimPointerGrab,
    EWsWinOpSetBackgroundColor,
    EWsWinOpSetOrdinalPositionErr,
    EWsWinOpSetVisible,
    EWsWinOpScroll,
    EWsWinOpScrollClip,
    EWsWinOpScrollRect,
    EWsWinOpScrollClipRect,
    EWsWinOpReceiveFocus,
    EWsWinOpAutoForeground,
    EWsWinOpSetNoBackgroundColor,
    EWsWinOpCaptureKey,
    EWsWinOpCancelCaptureKey,
    EWsWinOpCaptureKeyUpsAndDowns,
    EWsWinOpCancelCaptureKeyUpsAndDowns,
    EWsWinOpAddPriorityKey,
    EWsWinOpRemovePriorityKey,
    EWsWinOpSetTextCursor,
    EWsWinOpSetTextCursorClipped,
    EWsWinOpCancelTextCursor,
    EWsWinOpSetShadowHeight,
    EWsWinOpShadowDisabled,
    EWsWinOpGetInvalidRegion,
    EWsWinOpGetInvalidRegionCount,
    EWsWinOpSetColor,
    EWsWinOpSetCornerType,
    EWsWinOpSetShape,
    EWsWinOpBitmapHandle,
    EWsWinOpUpdateScreen,
    EWsWinOpUpdateScreenRegion,
    EWsWinOpUpdateBackupBitmap,
    EWsWinOpMaintainBackup,
    EWsWinOpName,
    EWsWinOpSetName,
    EWsWinOpSetOwningWindowGroup,
    EWsWinOpDefaultOwningWindow,
    EWsWinOpRequiredDisplayMode,
    EWsWinOpEnableOnEvents,
    EWsWinOpDisableOnEvents,
    EWsWinOpEnableGroupChangeEvents,
    EWsWinOpDisableGroupChangeEvents,
    EWsWinOpSetPointerCursor,
    EWsWinOpSetCustomPointerCursor,
    EWsWinOpRequestPointerRepeatEvent,
    EWsWinOpCancelPointerRepeatEventRequest,
    EWsWinOpAllocPointerMoveBuffer,
    EWsWinOpFreePointerMoveBuffer,
    EWsWinOpEnablePointerMoveBuffer,
    EWsWinOpDisablePointerMoveBuffer,
    EWsWinOpRetrievePointerMoveBuffer,
    EWsWinOpDiscardPointerMoveBuffer, //Tested to here %%%
    EWsWinOpEnableModifierChangedEvents,
    EWsWinOpDisableModifierChangedEvents,
    EWsWinOpEnableErrorMessages,
    EWsWinOpDisableErrorMessages,
    EWsWinOpAddKeyRect,
    EWsWinOpRemoveAllKeyRects,
    EWsWinOpPasswordWindow,
    EWsWinOpEnableBackup,
    EWsWinOpIdentifier,
    EWsWinOpDisableKeyClick,
    EWsWinOpSetFade = EWsWinOpDisableKeyClick + 3, //Two messages removed
    EWsWinOpSetNonFading,
    EWsWinOpFadeBehind,
    EWsWinOpEnableScreenChangeEvents,
    EWsWinOpDisableScreenChangeEvents,
    EWsWinOpSendPointerEvent,
    EWsWinOpSendAdvancedPointerEvent,
    EWsWinOpGetDisplayMode,
    EWsWinOpGetIsFaded,
    EWsWinOpGetIsNonFading,
    EWsWinOpOrdinalPriority,
    EWsWinOpClearPointerCursor,
    EWsWinOpMoveToGroup,
    EWsWinOpEnableFocusChangeEvents,
    EWsWinOpDisableFocusChangeEvents,
    EWsWinOpEnableGroupListChangeEvents,
    EWsWinOpDisableGroupListChangeEvents,
    EWsWinOpCaptureLongKey,
    EWsWinOpCancelCaptureLongKey,
    EWsWinOpStoreDrawCommands,
    EWsWinOpHandleTransparencyUpdate,
    EWsWinOpSetTransparencyFactor,
    EWsWinOpSetTransparencyBitmap,
    EWsWinOpAllowChildWindowGroup,
    EWsWinOpSetTransparencyBitmapCWs,
    EWsWinOpEnableVisibilityChangeEvents,
    EWsWinOpDisableVisibilityChangeEvents,
    EWsWinOpSetTransparencyAlphaChannel,
    EWsWinOpWindowGroupId,
    EWsWinOpSetPointerCapturePriority,
    EWsWinOpGetPointerCapturePriority,
    EWsWinOpSetTransparentRegion,
    EWsWinOpSetTransparencyPolicy,
    EWsWinOpIsRedrawStoreEnabled,
    EWsWinOpEnableOSB,
    EWsWinOpDisableOSB,
    EWsWinOpClearChildGroup,
    EWsWinOpSetChildGroup,
    EWsWinOpClientHandle,
    EWsWinOpSetBackgroundSurface,
    EWsWinOpKeyColor = EWsWinOpSetBackgroundSurface + 2, //One message removed
    EWsWinOpSetBackgroundSurfaceConfig = EWsWinOpKeyColor + 5, //Four messages removed
    EWsWinOpRemoveBackgroundSurface = EWsWinOpSetBackgroundSurfaceConfig + 2, //One message removed
    EWsWinOpGetBackgroundSurfaceConfig = EWsWinOpRemoveBackgroundSurface + 2, //One message removed
    EWsWinOpClearRedrawStore = EWsWinOpGetBackgroundSurfaceConfig + 2, //One message removed
    EWsWinOpScreenNumber,
    EWsWinOpEnableAdvancedPointers,
#ifdef SYMBIAN_GRAPHICS_WSERV_QT_EFFECTS
    EWsWinOpSetSurfaceTransparency,
#endif
    EWsWinOpSetPurpose,
    EWsWinOpSendEffectCommand,
    EWsWinOpOverrideEffectBuf,
    EWsWinOpOverrideEffectIPC,
    EWsWinOpTestLowPriorityRedraw = 0x2000, //Specific opcode for testing redraw queue priorities
};

enum TWsClickOpcodes {
    EWsClickOpFree = 0x0000,
    EWsClickOpIsLoaded,
    EWsClickOpUnLoad,
    EWsClickOpLoad,
    EWsClickOpCommandReply,
    EWsClickOpSetKeyClick,
    EWsClickOpSetPenClick,
    EWsClickOpKeyClickEnabled,
    EWsClickOpPenClickEnabled,
};

enum THotKey {
    /** Enables logging of all messages to and from the window server.
	Note that the required type of logging must have been specified in the wsini.ini
	file (using the LOG keyword), and the appropriate logging DLL must be available.
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\>E */
    EHotKeyEnableLogging,
    /** Always disables window server logging, if active. Does nothing otherwise.
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\>D */
    EHotKeyDisableLogging,
    /** Dumps a list of all windows to the log. (If logging is disabled, it is temporarily
	enabled in order to do this.)
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\>W */
    EHotKeyStateDump,
    /** Kills the foreground application.
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\>K */
    EHotKeyOfDeath,
    /** Shuts down the window server.
	Be cautious! This may mean resetting the machine to re-start the window server.
	Default key mapping: release (not available), debug (\<Ctrl\>\<Alt\>\<Shift\>X). */
    EHotKeyShutDown,
    /** Dumps a list of cells allocated on the window server's heap to the log. (If
	logging is disabled, it is temporarily enabled in order to do this.)
	Note that logging requires that the type of logging has been set up in the
	wsini.ini file, and that the appropriate logging DLL is available.
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\>H */
    EHotKeyHeapDump,
    /** Increases the LCD contrast.
	Default key mapping: EKeyIncContrast. Note that this value is from an enum
	in e32keys.h. The contrast wraps around when it reaches the maximum. */
    EHotKeyIncContrast,
    /** Decreases the LCD contrast.
	Default key mapping: EKeyDecContrast. Note that this value is from an enum
	in e32keys.h. The contrast wraps around when it reaches the minimum. */
    EHotKeyDecContrast,
    /** Switches the machine off.
	Default key mapping: EKeyOff. Note that this value is from an enum in e32keys.h. */
    EHotKeyOff,
    /** Switches the backlight on.
	Default key mapping: EKeyBacklightOn. Note that this value is from an enum
	in e32keys.h. */
    EHotKeyBacklightOn,
    /** Switches the backlight off.
	Default key mapping: EKeyBacklightOff. Note that this value is from an enum
	in e32keys.h. */
    EHotKeyBacklightOff,
    /** Toggles the backlight.
	Default key mapping: EKeyBacklightToggle. Note that this value is from an
	enum in e32keys.h. */
    EHotKeyBacklightToggle,
    /** Switches to screen size 0.
	This, and the following 3 keys are used to switch screen sizes on real hardware,
	for instance when the cover is closed on a phone that supports screen flipping. */
    EHotKeyScreenDimension0,
    /** Switches to screen size 1.
	This might be generated when the cover is opened on a phone that supports screen
	flipping. */
    EHotKeyScreenDimension1,
    /** Switches to screen size 2. */
    EHotKeyScreenDimension2,
    /** Switches to screen size 3. */
    EHotKeyScreenDimension3,
    /** Cycles the display though its possible sizes.
	This is used only for debugging.
	A device may have several screen sizes, each with a default orientation. For
	example a phone that supports screen flipping will have different display
	sizes when the cover is opened and closed.
	Default key mapping: debug : \<Ctrl\>\<Alt\>\<Shift\> U. Release : none. */
    EHotKeyCycleDisplaySize,
    /** Cycles the screen orientation through the specified rotations for the particular
	size mode you are in.
	For example, phones that support screen flipping may
	use this feature for changing between right and left handed use.
	For rectangular display modes you can only specify 2 orientations, 180 degrees
	apart. For square modes you can specify 4 rotations (90 degrees) or however
	many you want.
	Specification of the list of rotations takes place in the WSINI.INI file.
	Default key mapping: debug : \<Ctrl\>\<Alt\>\<Shift\> O. Release : none. */
    EHotKeyCycleOrientation,
    /** Increases the screen's brightness.
	The brightness wraps around to the minimum
	value after it has reached the maximum. */
    EHotKeyIncBrightness,
    /** Decreases the screen's brightness.
	The brightness wraps around to the maximum value after it has reached the minimum. */
    EHotKeyDecBrightness,

    /** Cycle focus screen from one to another in multiple screen environment. Start
	from current focused screen, switch to next the screen, and wraps around when it
	reaches the last screen.
	Default key mapping: \<Ctrl\>\<Alt\>\<Shift\> I. */
    EHotKeyCycleFocusScreen,

    /** Value for first hot key.
	Used with EHotKeyLastKeyType to make it easy to write a for loop that steps
	through all the different key values. */
    EHotKeyFirstKeyType = EHotKeyEnableLogging, //Must always be set to the first one
    /** Value for last hot key.
	Used with EHotKeyFirstKeyType to make it easy to write a for loop that steps
	through all the different key values. */
    EHotKeyLastKeyType = EHotKeyCycleFocusScreen, //Must always be set to the last one
};