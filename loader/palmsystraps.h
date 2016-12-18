/**
 * \file
 * \brief Enumerated types for system trap names.
 *
 * Based on code from the Metrowerks Compiler, PalmOS 2.0 support.
 *
 * \authors
 * Copyright (C) 2000, Sun Microsystems, Inc
 * \authors
 * Copyright (C) 2000, The University of Queensland
 *
 * \copyright
 * See the file "LICENSE.TERMS" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef SYSTRAPS_H
#define SYSTRAPS_H

#define sysTrapBase         0xA000
#define numTrapStrings      (sizeof trapNames / sizeof *trapNames)

//  "sysTrapMemInit",       // = sysTrapBase


const char *trapNames[] = {
	"MemInit",
	"MemInitHeapTable",
	"MemStoreInit",
	"MemCardFormat",
	"MemCardInfo",
	"MemStoreInfo",
	"MemStoreSetInfo",
	"MemNumHeaps",
	"MemNumRAMHeaps",
	"MemHeapID",
	"MemHeapPtr",
	"MemHeapFreeBytes",
	"MemHeapSize",
	"MemHeapFlags",
	"MemHeapCompact",
	"MemHeapInit",
	"MemHeapFreeByOwnerID",
	"MemChunkNew",
	"MemPtrFree",        // Formerly sysTrapMemChunkFree
	"MemPtrNew",
	"MemPtrRecoverHandle",
	"MemPtrFlags",
	"MemPtrSize",
	"MemPtrOwner",
	"MemPtrHeapID",
	"MemPtrCardNo",
	"MemPtrToLocalID",
	"MemPtrSetOwner",
	"MemPtrResize",
	"MemPtrResetLock",
	"MemHandleNew",
	"MemHandleLockCount",
	"MemHandleToLocalID",
	"MemHandleLock",
	"MemHandleUnlock",
	"MemLocalIDToGlobal",
	"MemLocalIDKind",
	"MemLocalIDToPtr",
	"MemMove",
	"MemSet",
	"MemStoreSearch",
	"MemPtrDataStorage",
	"MemKernelInit",
	"MemHandleFree",
	"MemHandleFlags",
	"MemHandleSize",
	"MemHandleOwner",
	"MemHandleHeapID",
	"MemHandleDataStorage",
	"MemHandleCardNo",
	"MemHandleSetOwner",
	"MemHandleResize",
	"MemHandleResetLock",
	"MemPtrUnlock",
	"MemLocalIDToLockedPtr",
	"MemSetDebugMode",
	"MemHeapScramble",
	"MemHeapCheck",
	"MemNumCards",
	"MemDebugMode",
	"MemSemaphoreReserve",
	"MemSemaphoreRelease",
	"MemHeapDynamic",
	"MemNVParams",


	"DmInit",
	"DmCreateDatabase",
	"DmDeleteDatabase",
	"DmNumDatabases",
	"DmGetDatabase",
	"DmFindDatabase",
	"DmDatabaseInfo",
	"DmSetDatabaseInfo",
	"DmDatabaseSize",
	"DmOpenDatabase",
	"DmCloseDatabase",
	"DmNextOpenDatabase",
	"DmOpenDatabaseInfo",
	"DmResetRecordStates",
	"DmGetLastErr",
	"DmNumRecords",
	"DmRecordInfo",
	"DmSetRecordInfo",
	"DmAttachRecord",
	"DmDetachRecord",
	"DmMoveRecord",
	"DmNewRecord",
	"DmRemoveRecord",
	"DmDeleteRecord",
	"DmArchiveRecord",
	"DmNewHandle",
	"DmRemoveSecretRecords",
	"DmQueryRecord",
	"DmGetRecord",
	"DmResizeRecord",
	"DmReleaseRecord",
	"DmGetResource",
	"DmGet1Resource",
	"DmReleaseResource",
	"DmResizeResource",
	"DmNextOpenResDatabase",
	"DmFindResourceType",
	"DmFindResource",
	"DmSearchResource",
	"DmNumResources",
	"DmResourceInfo",
	"DmSetResourceInfo",
	"DmAttachResource",
	"DmDetachResource",
	"DmNewResource",
	"DmRemoveResource",
	"DmGetResourceIndex",
	"DmQuickSort",
	"DmQueryNextInCategory",
	"DmNumRecordsInCategory",
	"DmPositionInCategory",
	"DmSeekRecordInCategory",
	"DmMoveCategory",
	"DmOpenDatabaseByTypeCreator",
	"DmWrite",
	"DmStrCopy",
	"DmGetNextDatabaseByTypeCreator",
	"DmWriteCheck",
	"DmMoveOpenDBContext",
	"DmFindRecordByID",
	"DmGetAppInfoID",
	"DmFindSortPositionV10",
	"DmSet",
	"DmCreateDatabaseFromImage",


	"DbgSrcMessage",
	"DbgMessage",
	"DbgGetMessage",
	"DbgCommSettings",

	"ErrDisplayFileLineMsg",
	"ErrSetJump",
	"ErrLongJump",
	"ErrThrow",
	"ErrExceptionList",

	"SysBroadcastActionCode",
	"SysUnimplemented",
	"SysColdBoot",
	"SysReset",
	"SysDoze",
	"SysAppLaunch",
	"SysAppStartup",
	"SysAppExit",
	"SysSetA5",
	"SysSetTrapAddress",
	"SysGetTrapAddress",
	"SysTranslateKernelErr",
	"SysSemaphoreCreate",
	"SysSemaphoreDelete",
	"SysSemaphoreWait",
	"SysSemaphoreSignal",
	"SysTimerCreate",
	"SysTimerWrite",
	"SysTaskCreate",
	"SysTaskDelete",
	"SysTaskTrigger",
	"SysTaskID",
	"SysTaskUserInfoPtr",
	"SysTaskDelay",
	"SysTaskSetTermProc",
	"SysUILaunch",
	"SysNewOwnerID",
	"SysSemaphoreSet",
	"SysDisableInts",
	"SysRestoreStatus",
	"SysUIAppSwitch",
	"SysCurAppInfoP",
	"SysHandleEvent",
	"SysInit",
	"SysQSort",
	"SysCurAppDatabase",
	"SysFatalAlert",
	"SysResSemaphoreCreate",
	"SysResSemaphoreDelete",
	"SysResSemaphoreReserve",
	"SysResSemaphoreRelease",
	"SysSleep",
	"SysKeyboardDialogV10",
	"SysAppLauncherDialog",
	"SysSetPerformance",
	"SysBatteryInfo",
	"SysLibInstall",
	"SysLibRemove",
	"SysLibTblEntry",
	"SysLibFind",
	"SysBatteryDialog",
	"SysCopyStringResource",
	"SysKernelInfo",
	"SysLaunchConsole",
	"SysTimerDelete",
	"SysSetAutoOffTime",
	"SysFormPointerArrayToStrings",
	"SysRandom",
	"SysTaskSwitching",
	"SysTimerRead",


	"StrCopy",
	"StrCat",
	"StrLen",
	"StrCompare",
	"StrIToA",
	"StrCaselessCompare",
	"StrIToH",
	"StrChr",
	"StrStr",
	"StrAToI",
	"StrToLower",

	"SerReceiveISP",

	"SlkOpen",
	"SlkClose",
	"SlkOpenSocket",
	"SlkCloseSocket",
	"SlkSocketRefNum",
	"SlkSocketSetTimeout",
	"SlkFlushSocket",
	"SlkSetSocketListener",
	"SlkSendPacket",
	"SlkReceivePacket",
	"SlkSysPktDefaultResponse",
	"SlkProcessRPC",


	"ConPutS",
	"ConGetS",

	"FplInit",
	"FplFree",
	"FplFToA",
	"FplAToF",
	"FplBase10Info",
	"FplLongToFloat",
	"FplFloatToLong",
	"FplFloatToULong",
	"FplMul",
	"FplAdd",
	"FplSub",
	"FplDiv",

	"ScrInit",
	"ScrCopyRectangle",
	"ScrDrawChars",
	"ScrLineRoutine",
	"ScrRectangleRoutine",
	"ScrScreenInfo",
	"ScrDrawNotify",
	"ScrSendUpdateArea",
	"ScrCompressScanLine",
	"ScrDeCompressScanLine",


	"TimGetSeconds",
	"TimSetSeconds",
	"TimGetTicks",
	"TimInit",
	"TimSetAlarm",
	"TimGetAlarm",
	"TimHandleInterrupt",
	"TimSecondsToDateTime",
	"TimDateTimeToSeconds",
	"TimAdjust",
	"TimSleep",
	"TimWake",

	"CategoryCreateListV10",
	"CategoryFreeListV10",
	"CategoryFind",
	"CategoryGetName",
	"CategoryEditV10",
	"CategorySelectV10",
	"CategoryGetNext",
	"CategorySetTriggerLabel",
	"CategoryTruncateName",

	"ClipboardAddItem",
	"ClipboardCheckIfItemExist",
	"ClipboardGetItem",

	"CtlDrawControl",
	"CtlEraseControl",
	"CtlHideControl",
	"CtlShowControl",
	"CtlGetValue",
	"CtlSetValue",
	"CtlGetLabel",
	"CtlSetLabel",
	"CtlHandleEvent",
	"CtlHitControl",
	"CtlSetEnabled",
	"CtlSetUsable",
	"CtlEnabled",


	"EvtInitialize",
	"EvtAddEventToQueue",
	"EvtCopyEvent",
	"EvtGetEvent",
	"EvtGetPen",
	"EvtSysInit",
	"EvtGetSysEvent",
	"EvtProcessSoftKeyStroke",
	"EvtGetPenBtnList",
	"EvtSetPenQueuePtr",
	"EvtPenQueueSize",
	"EvtFlushPenQueue",
	"EvtEnqueuePenPoint",
	"EvtDequeuePenStrokeInfo",
	"EvtDequeuePenPoint",
	"EvtFlushNextPenStroke",
	"EvtSetKeyQueuePtr",
	"EvtKeyQueueSize",
	"EvtFlushKeyQueue",
	"EvtEnqueueKey",
	"EvtDequeueKeyEvent",
	"EvtWakeup",
	"EvtResetAutoOffTimer",
	"EvtKeyQueueEmpty",
	"EvtEnableGraffiti",


	"FldCopy",
	"FldCut",
	"FldDrawField",
	"FldEraseField",
	"FldFreeMemory",
	"FldGetBounds",
	"FldGetTextPtr",
	"FldGetSelection",
	"FldHandleEvent",
	"FldPaste",
	"FldRecalculateField",
	"FldSetBounds",
	"FldSetText",
	"FldGetFont",
	"FldSetFont",
	"FldSetSelection",
	"FldGrabFocus",
	"FldReleaseFocus",
	"FldGetInsPtPosition",
	"FldSetInsPtPosition",
	"FldSetScrollPosition",
	"FldGetScrollPosition",
	"FldGetTextHeight",
	"FldGetTextAllocatedSize",
	"FldGetTextLength",
	"FldScrollField",
	"FldScrollable",
	"FldGetVisibleLines",
	"FldGetAttributes",
	"FldSetAttributes",
	"FldSendChangeNotification",
	"FldCalcFieldHeight",
	"FldGetTextHandle",
	"FldCompactText",
	"FldDirty",
	"FldWordWrap",
	"FldSetTextAllocatedSize",
	"FldSetTextHandle",
	"FldSetTextPtr",
	"FldGetMaxChars",
	"FldSetMaxChars",
	"FldSetUsable",
	"FldInsert",
	"FldDelete",
	"FldUndo",
	"FldSetDirty",
	"FldSendHeightChangeNotification",
	"FldMakeFullyVisible",


	"FntGetFont",
	"FntSetFont",
	"FntGetFontPtr",
	"FntBaseLine",
	"FntCharHeight",
	"FntLineHeight",
	"FntAverageCharWidth",
	"FntCharWidth",
	"FntCharsWidth",
	"FntDescenderHeight",
	"FntCharsInWidth",
	"FntLineWidth",



	"FrmInitForm",
	"FrmDeleteForm",
	"FrmDrawForm",
	"FrmEraseForm",
	"FrmGetActiveForm",
	"FrmSetActiveForm",
	"FrmGetActiveFormID",
	"FrmGetUserModifiedState",
	"FrmSetNotUserModified",
	"FrmGetFocus",
	"FrmSetFocus",
	"FrmHandleEvent",
	"FrmGetFormBounds",
	"FrmGetWindowHandle",
	"FrmGetFormId",
	"FrmGetFormPtr",
	"FrmGetNumberOfObjects",
	"FrmGetObjectIndex",
	"FrmGetObjectId",
	"FrmGetObjectType",
	"FrmGetObjectPtr",
	"FrmHideObject",
	"FrmShowObject",
	"FrmGetObjectPosition",
	"FrmSetObjectPosition",
	"FrmGetControlValue",
	"FrmSetControlValue",
	"FrmGetControlGroupSelection",
	"FrmSetControlGroupSelection",
	"FrmCopyLabel",
	"FrmSetLabel",
	"FrmGetLabel",
	"FrmSetCategoryLabel",
	"FrmGetTitle",
	"FrmSetTitle",
	"FrmAlert",
	"FrmDoDialog",
	"FrmCustomAlert",
	"FrmHelp",
	"FrmUpdateScrollers",
	"FrmGetFirstForm",
	"FrmVisible",
	"FrmGetObjectBounds",
	"FrmCopyTitle",
	"FrmGotoForm",
	"FrmPopupForm",
	"FrmUpdateForm",
	"FrmReturnToForm",
	"FrmSetEventHandler",
	"FrmDispatchEvent",
	"FrmCloseAllForms",
	"FrmSaveAllForms",
	"FrmGetGadgetData",
	"FrmSetGadgetData",
	"FrmSetCategoryTrigger",


	"UIInitialize",
	"UIReset",

	"InsPtInitialize",
	"InsPtSetLocation",
	"InsPtGetLocation",
	"InsPtEnable",
	"InsPtEnabled",
	"InsPtSetHeight",
	"InsPtGetHeight",
	"InsPtCheckBlink",

	"LstSetDrawFunction",
	"LstDrawList",
	"LstEraseList",
	"LstGetSelection",
	"LstGetSelectionText",
	"LstHandleEvent",
	"LstSetHeight",
	"LstSetSelection",
	"LstSetListChoices",
	"LstMakeItemVisible",
	"LstGetNumberOfItems",
	"LstPopupList",
	"LstSetPosition",

	"MenuInit",
	"MenuDispose",
	"MenuHandleEvent",
	"MenuDrawMenu",
	"MenuEraseStatus",
	"MenuGetActiveMenu",
	"MenuSetActiveMenu",


	"RctSetRectangle",
	"RctCopyRectangle",
	"RctInsetRectangle",
	"RctOffsetRectangle",
	"RctPtInRectangle",
	"RctGetIntersection",


	"TblDrawTable",
	"TblEraseTable",
	"TblHandleEvent",
	"TblGetItemBounds",
	"TblSelectItem",
	"TblGetItemInt",
	"TblSetItemInt",
	"TblSetItemStyle",
	"TblUnhighlightSelection",
	"TblSetRowUsable",
	"TblGetNumberOfRows",
	"TblSetCustomDrawProcedure",
	"TblSetRowSelectable",
	"TblRowSelectable",
	"TblSetLoadDataProcedure",
	"TblSetSaveDataProcedure",
	"TblGetBounds",
	"TblSetRowHeight",
	"TblGetColumnWidth",
	"TblGetRowID",
	"TblSetRowID",
	"TblMarkRowInvalid",
	"TblMarkTableInvalid",
	"TblGetSelection",
	"TblInsertRow",
	"TblRemoveRow",
	"TblRowInvalid",
	"TblRedrawTable",
	"TblRowUsable",
	"TblReleaseFocus",
	"TblEditing",
	"TblGetCurrentField",
	"TblSetColumnUsable",
	"TblGetRowHeight",
	"TblSetColumnWidth",
	"TblGrabFocus",
	"TblSetItemPtr",
	"TblFindRowID",
	"TblGetLastUsableRow",
	"TblGetColumnSpacing",
	"TblFindRowData",
	"TblGetRowData",
	"TblSetRowData",
	"TblSetColumnSpacing",



	"WinCreateWindow",
	"WinCreateOffscreenWindow",
	"WinDeleteWindow",
	"WinInitializeWindow",
	"WinAddWindow",
	"WinRemoveWindow",
	"WinSetActiveWindow",
	"WinSetDrawWindow",
	"WinGetDrawWindow",
	"WinGetActiveWindow",
	"WinGetDisplayWindow",
	"WinGetFirstWindow",
	"WinEnableWindow",
	"WinDisableWindow",
	"WinGetWindowFrameRect",
	"WinDrawWindowFrame",
	"WinEraseWindow",
	"WinSaveBits",
	"WinRestoreBits",
	"WinCopyRectangle",
	"WinScrollRectangle",
	"WinGetDisplayExtent",
	"WinGetWindowExtent",
	"WinDisplayToWindowPt",
	"WinWindowToDisplayPt",
	"WinGetClip",
	"WinSetClip",
	"WinResetClip",
	"WinClipRectangle",
	"WinDrawLine",
	"WinDrawGrayLine",
	"WinEraseLine",
	"WinInvertLine",
	"WinFillLine",
	"WinDrawRectangle",
	"WinEraseRectangle",
	"WinInvertRectangle",
	"WinDrawRectangleFrame",
	"WinDrawGrayRectangleFrame",
	"WinEraseRectangleFrame",
	"WinInvertRectangleFrame",
	"WinGetFramesRectangle",
	"WinDrawChars",
	"WinEraseChars",
	"WinInvertChars",
	"WinGetPattern",
	"WinSetPattern",
	"WinSetUnderlineMode",
	"WinDrawBitmap",
	"WinModal",
	"WinGetWindowBounds",
	"WinFillRectangle",
	"WinDrawInvertedChars",



	"PrefOpenPreferenceDBV10",
	"PrefGetPreferences",
	"PrefSetPreferences",
	"PrefGetAppPreferencesV10",
	"PrefSetAppPreferencesV10",


	"SndInit",
	"SndSetDefaultVolume",
	"SndGetDefaultVolume",
	"SndDoCmd",
	"SndPlaySystemSound",


	"AlmInit",
	"AlmCancelAll",
	"AlmAlarmCallback",
	"AlmSetAlarm",
	"AlmGetAlarm",
	"AlmDisplayAlarm",
	"AlmEnableNotification",


	"HwrGetRAMMapping",
	"HwrMemWritable",
	"HwrMemReadable",
	"HwrDoze",
	"HwrSleep",
	"HwrWake",
	"HwrSetSystemClock",
	"HwrSetCPUDutyCycle",
	"HwrLCDInit",
	"HwrLCDSleep",
	"HwrTimerInit",
	"HwrCursor",
	"HwrBatteryLevel",
	"HwrDelay",
	"HwrEnableDataWrites",
	"HwrDisableDataWrites",
	"HwrLCDBaseAddr",
	"HwrLCDDrawBitmap",
	"HwrTimerSleep",
	"HwrTimerWake",
	"HwrLCDWake",
	"HwrIRQ1Handler",
	"HwrIRQ2Handler",
	"HwrIRQ3Handler",
	"HwrIRQ4Handler",
	"HwrIRQ5Handler",
	"HwrIRQ6Handler",
	"HwrDockSignals",
	"HwrPluggedIn",


	"Crc16CalcBlock",


	"SelectDayV10",
	"SelectTime",

	"DayDrawDaySelector",
	"DayHandleEvent",
	"DayDrawDays",
	"DayOfWeek",
	"DaysInMonth",
	"DayOfMonth",

	"DateDaysToDate",
	"DateToDays",
	"DateAdjust",
	"DateSecondsToDate",
	"DateToAscii",
	"DateToDOWDMFormat",
	"TimeToAscii",


	"Find",
	"FindStrInStr",
	"FindSaveMatch",
	"FindGetLineBounds",
	"FindDrawHeader",

	"PenOpen",
	"PenClose",
	"PenGetRawPen",
	"PenCalibrate",
	"PenRawToScreen",
	"PenScreenToRaw",
	"PenResetCalibration",
	"PenSleep",
	"PenWake",


	"ResLoadForm",
	"ResLoadMenu",

	"FtrInit",
	"FtrUnregister",
	"FtrGet",
	"FtrSet",
	"FtrGetByIndex",



	"GrfInit",
	"GrfFree",
	"GrfGetState",
	"GrfSetState",
	"GrfFlushPoints",
	"GrfAddPoint",
	"GrfInitState",
	"GrfCleanState",
	"GrfMatch",
	"GrfGetMacro",
	"GrfFilterPoints",
	"GrfGetNumPoints",
	"GrfGetPoint",
	"GrfFindBranch",
	"GrfMatchGlyph",
	"GrfGetGlyphMapping",
	"GrfGetMacroName",
	"GrfDeleteMacro",
	"GrfAddMacro",
	"GrfGetAndExpandMacro",
	"GrfProcessStroke",
	"GrfFieldChange",


	"GetCharSortValue",
	"GetCharAttr",
	"GetCharCaselessValue",


	"PwdExists",
	"PwdVerify",
	"PwdSet",
	"PwdRemove",

	"GsiInitialize",
	"GsiSetLocation",
	"GsiEnable",
	"GsiEnabled",
	"GsiSetShiftState",

	"KeyInit",
	"KeyHandleInterrupt",
	"KeyCurrentState",
	"KeyResetDoubleTap",
	"KeyRates",
	"KeySleep",
	"KeyWake",


	"DlkControl",               // was sysTrapCmBroadcast

	"DlkStartServer",
	"DlkGetSyncInfo",
	"DlkSetLogEntry",

	"Unused2",              // *DO NOT RE-USE UNTIL 3.0* was sysTrapPsrInit (vmk 7/16/96)
	"SysLibLoad",           // was sysTrapPsrClose",
	"Unused4",              // was sysTrapPsrGetCommand,
	"Unused5",              // was sysTrapPsrSendReply,

	"AbtShowAbout",

	"MdmDial",
	"MdmHangUp",

	"DmSearchRecord",

	"SysInsertionSort",
	"DmInsertionSort",

	"LstSetTopItem",

	"SclSetScrollBar",
	"SclDrawScrollBar",
	"SclHandleEvent",

	"SysMailboxCreate",
	"SysMailboxDelete",
	"SysMailboxFlush",
	"SysMailboxSend",
	"SysMailboxWait",

	"SysTaskWait",
	"SysTaskWake",
	"SysTaskWaitClr",
	"SysTaskSuspend",
	"SysTaskResume",

	"CategoryCreateList",
	"CategoryFreeList",
	"CategoryEdit",
	"CategorySelect",

	"DmDeleteCategory",

	"SysEvGroupCreate",
	"SysEvGroupSignal",
	"SysEvGroupRead",
	"SysEvGroupWait",

	"EvtEventAvail",
	"EvtSysEventAvail",
	"StrNCopy",

	"KeySetMask",

	"SelectDay",

	"PrefGetPreference",
	"PrefSetPreference",
	"PrefGetAppPreferences",
	"PrefSetAppPreferences",

	"FrmPointInTitle",

	"StrNCat",

	"MemCmp",

	"TblSetColumnEditIndicator",

	"FntWordWrap",

	"FldGetScrollValues",

	"SysCreateDataBaseList",
	"SysCreatePanelList",

	"DlkDispatchRequest",

	"StrPrintF",
	"StrVPrintF",

	"PrefOpenPreferenceDB",

	"SysGraffitiReferenceDialog",

	"SysKeyboardDialog",

	"FntWordWrapReverseNLines",
	"FntGetScrollValues",

	"TblSetRowStaticHeight",
	"TblHasScrollBar",

	"SclGetScrollBar",

	"FldGetNumberOfBlankLines",

	"SysTicksPerSecond",
	"HwrBacklight",
	"DmDatabaseProtect",

	"TblSetBounds",

	"StrNCompare",
	"StrNCaselessCompare",

	"PhoneNumberLookup",

	"FrmSetMenu",

	"EncDigestMD5",

	"DmFindSortPosition",

	"SysBinarySearch",
	"SysErrString",
	"SysStringByIndex",

	"EvtAddUniqueEventToQueue",

	"StrLocalizeNumber",
	"StrDelocalizeNumber",
	"LocGetNumberSeparators",

	"MenuSetActiveMenuRscID",

	"LstScrollList",

	"CategoryInitialize",

	"EncDigestMD4",
	"EncDES",

	"LstGetVisibleItems",

	"WinSetWindowBounds",

	"CategorySetName",

	"FldSetInsertionPoint",

	"FrmSetObjectBounds",

	"WinSetColors",

	"FlpDispatch",
	"FlpEmDispatch"
};

#endif
