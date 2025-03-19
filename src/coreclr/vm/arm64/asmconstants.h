// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// asmconstants.h -
//
// This header defines field offsets and constants used by assembly code
// Be sure to rebuild clr/src/vm/ceemain.cpp after changing this file, to
// ensure that the constants match the expected C/C++ values

// #ifndef HOST_ARM64
// #error this file should only be used on an ARM platform
// #endif // HOST_ARM64

#include "../../inc/switches.h"

//-----------------------------------------------------------------------------

#ifndef ASMCONSTANTS_C_ASSERT
#define ASMCONSTANTS_C_ASSERT(cond)
#endif

#ifndef ASMCONSTANTS_RUNTIME_ASSERT
#define ASMCONSTANTS_RUNTIME_ASSERT(cond)
#endif

// Some constants are different in _DEBUG builds.  This macro factors out ifdefs from below.
#ifdef _DEBUG
#define DBG_FRE(dbg,fre) dbg
#else
#define DBG_FRE(dbg,fre) fre
#endif

#define FRAMETYPE_InlinedCallFrame 1
ASMCONSTANTS_C_ASSERT(FRAMETYPE_InlinedCallFrame == (int)FrameIdentifier::InlinedCallFrame)

#define DynamicHelperFrameFlags_Default     0
#define DynamicHelperFrameFlags_ObjectArg   1
#define DynamicHelperFrameFlags_ObjectArg2  2

#define ThisPtrRetBufPrecodeData__Target      0x00
ASMCONSTANTS_C_ASSERT(ThisPtrRetBufPrecodeData__Target == offsetof(ThisPtrRetBufPrecodeData, Target));

#define               Thread__m_fPreemptiveGCDisabled   0x04
#define               Thread__m_pFrame                  0x08

ASMCONSTANTS_C_ASSERT(Thread__m_fPreemptiveGCDisabled == offsetof(Thread, m_fPreemptiveGCDisabled));
ASMCONSTANTS_C_ASSERT(Thread__m_pFrame == offsetof(Thread, m_pFrame));

#define Thread_m_pFrame Thread__m_pFrame
#define Thread_m_fPreemptiveGCDisabled Thread__m_fPreemptiveGCDisabled

#define METHODDESC_REGISTER            x12

#define SIZEOF__ArgumentRegisters 0x40
ASMCONSTANTS_C_ASSERT(SIZEOF__ArgumentRegisters == sizeof(ArgumentRegisters))

// There are 8 128-bit registers in FloatArgumentRegisters
#define SIZEOF__FloatArgumentRegisters 0x80
ASMCONSTANTS_C_ASSERT(SIZEOF__FloatArgumentRegisters == sizeof(FloatArgumentRegisters))

#define ASM_ENREGISTERED_RETURNTYPE_MAXSIZE 0x40
ASMCONSTANTS_C_ASSERT(ASM_ENREGISTERED_RETURNTYPE_MAXSIZE == ENREGISTERED_RETURNTYPE_MAXSIZE)

#define CallDescrData__pSrc                     0x00
#define CallDescrData__numStackSlots            0x08
#define CallDescrData__pArgumentRegisters       0x10
#define CallDescrData__pFloatArgumentRegisters  0x18
#define CallDescrData__fpReturnSize             0x20
#define CallDescrData__pTarget                  0x28
#define CallDescrData__pRetBuffArg              0x30
#define CallDescrData__returnValue              0x40

ASMCONSTANTS_C_ASSERT(CallDescrData__pSrc                 == offsetof(CallDescrData, pSrc))
ASMCONSTANTS_C_ASSERT(CallDescrData__numStackSlots        == offsetof(CallDescrData, numStackSlots))
ASMCONSTANTS_C_ASSERT(CallDescrData__pArgumentRegisters   == offsetof(CallDescrData, pArgumentRegisters))
ASMCONSTANTS_C_ASSERT(CallDescrData__pFloatArgumentRegisters == offsetof(CallDescrData, pFloatArgumentRegisters))
ASMCONSTANTS_C_ASSERT(CallDescrData__fpReturnSize         == offsetof(CallDescrData, fpReturnSize))
ASMCONSTANTS_C_ASSERT(CallDescrData__pTarget              == offsetof(CallDescrData, pTarget))
ASMCONSTANTS_C_ASSERT(CallDescrData__pRetBuffArg          == offsetof(CallDescrData, pRetBuffArg))
ASMCONSTANTS_C_ASSERT(CallDescrData__returnValue          == offsetof(CallDescrData, returnValue))


// Offset of the array containing the address of captured registers in MachState
#define MachState__captureX19_X29 0x0
ASMCONSTANTS_C_ASSERT(MachState__captureX19_X29 == offsetof(MachState, captureX19_X29))

// Offset of the array containing the address of preserved registers in MachState
#define MachState__ptrX19_X29 0x58
ASMCONSTANTS_C_ASSERT(MachState__ptrX19_X29 == offsetof(MachState, ptrX19_X29))

#define MachState__isValid 0xc0
ASMCONSTANTS_C_ASSERT(MachState__isValid == offsetof(MachState, _isValid))

#define LazyMachState_captureX19_X29 MachState__captureX19_X29
ASMCONSTANTS_C_ASSERT(LazyMachState_captureX19_X29 == offsetof(LazyMachState, captureX19_X29))

#ifdef __APPLE__
#define LazyMachState_captureSp     (MachState__isValid+8+88) // padding for alignment
#else // __APPLE__
#define LazyMachState_captureSp     (MachState__isValid+8) // padding for alignment
#endif // __APPLE
ASMCONSTANTS_C_ASSERT(LazyMachState_captureSp == offsetof(LazyMachState, captureSp))

#define LazyMachState_captureIp     (LazyMachState_captureSp+8)
ASMCONSTANTS_C_ASSERT(LazyMachState_captureIp == offsetof(LazyMachState, captureIp))

#define VASigCookie__pNDirectILStub 0x8
ASMCONSTANTS_C_ASSERT(VASigCookie__pNDirectILStub == offsetof(VASigCookie, pNDirectILStub))

#define SIZEOF__Frame                 0x10
ASMCONSTANTS_C_ASSERT(SIZEOF__Frame == sizeof(Frame));

#if !defined(HOST_WINDOWS)
#define SIZEOF__CONTEXT               0x3e0
#else
#define SIZEOF__CONTEXT               0x390
#endif
ASMCONSTANTS_C_ASSERT(SIZEOF__CONTEXT == sizeof(T_CONTEXT));

#define               OFFSETOF__DynamicHelperStubArgs__Constant1    0x0
ASMCONSTANTS_C_ASSERT(OFFSETOF__DynamicHelperStubArgs__Constant1
                    == offsetof(DynamicHelperStubArgs, Constant1));

#define               OFFSETOF__DynamicHelperStubArgs__Constant2    0x8
ASMCONSTANTS_C_ASSERT(OFFSETOF__DynamicHelperStubArgs__Constant2
                    == offsetof(DynamicHelperStubArgs, Constant2));

#define               OFFSETOF__DynamicHelperStubArgs__Helper    0x10
ASMCONSTANTS_C_ASSERT(OFFSETOF__DynamicHelperStubArgs__Helper
                    == offsetof(DynamicHelperStubArgs, Helper));

#define               OFFSETOF__GenericDictionaryDynamicHelperStubData__SecondIndir    0x0
ASMCONSTANTS_C_ASSERT(OFFSETOF__GenericDictionaryDynamicHelperStubData__SecondIndir
                    == offsetof(GenericDictionaryDynamicHelperStubData, SecondIndir));

#define               OFFSETOF__GenericDictionaryDynamicHelperStubData__LastIndir    0x4
ASMCONSTANTS_C_ASSERT(OFFSETOF__GenericDictionaryDynamicHelperStubData__LastIndir
                    == offsetof(GenericDictionaryDynamicHelperStubData, LastIndir));

#define               OFFSETOF__GenericDictionaryDynamicHelperStubData__SizeOffset    0x8
ASMCONSTANTS_C_ASSERT(OFFSETOF__GenericDictionaryDynamicHelperStubData__SizeOffset
                    == offsetof(GenericDictionaryDynamicHelperStubData, SizeOffset));

#define               OFFSETOF__GenericDictionaryDynamicHelperStubData__SlotOffset    0xc
ASMCONSTANTS_C_ASSERT(OFFSETOF__GenericDictionaryDynamicHelperStubData__SlotOffset
                    == offsetof(GenericDictionaryDynamicHelperStubData, SlotOffset));

#define               OFFSETOF__GenericDictionaryDynamicHelperStubData__HandleArgs    0x10
ASMCONSTANTS_C_ASSERT(OFFSETOF__GenericDictionaryDynamicHelperStubData__HandleArgs
                    == offsetof(GenericDictionaryDynamicHelperStubData, HandleArgs));

#define               OFFSETOF__InstantiatedMethodDesc__m_pPerInstInfo    DBG_FRE(0x40, 0x18)
ASMCONSTANTS_C_ASSERT(OFFSETOF__InstantiatedMethodDesc__m_pPerInstInfo
                    == offsetof(InstantiatedMethodDesc, m_pPerInstInfo));

//=========================================
#define MethodTable__m_dwFlags         0x0
ASMCONSTANTS_C_ASSERT(MethodTable__m_dwFlags == offsetof(MethodTable, m_dwFlags));

#define MethodTable__m_BaseSize         0x04
ASMCONSTANTS_C_ASSERT(MethodTable__m_BaseSize == offsetof(MethodTable, m_BaseSize));

#define               OFFSETOF__MethodTable__m_pPerInstInfo    DBG_FRE(0x38, 0x30)
ASMCONSTANTS_C_ASSERT(OFFSETOF__MethodTable__m_pPerInstInfo
                    == offsetof(MethodTable, m_pPerInstInfo));

#define MethodTable__m_ElementType     DBG_FRE(0x38, 0x30)
ASMCONSTANTS_C_ASSERT(MethodTable__m_ElementType == offsetof(MethodTable, m_ElementTypeHnd));

#define ArrayBase__m_NumComponents     0x8
ASMCONSTANTS_C_ASSERT(ArrayBase__m_NumComponents == offsetof(ArrayBase, m_NumComponents));

#define PtrArray__m_Array              0x10
ASMCONSTANTS_C_ASSERT(PtrArray__m_Array == offsetof(PtrArray, m_Array));

#define TypeHandle_CanCast 0x1 // TypeHandle::CanCast

//=========================================



#ifdef FEATURE_COMINTEROP

#define SIZEOF__ComMethodFrame 0x70
ASMCONSTANTS_C_ASSERT(SIZEOF__ComMethodFrame == sizeof(ComMethodFrame));

#define UnmanagedToManagedFrame__m_pvDatum 0x10
ASMCONSTANTS_C_ASSERT(UnmanagedToManagedFrame__m_pvDatum == offsetof(UnmanagedToManagedFrame, m_pvDatum));

#endif // FEATURE_COMINTEROP

#ifdef FEATURE_SPECIAL_USER_MODE_APC
#define OFFSETOF__APC_CALLBACK_DATA__ContextRecord 0x8
#endif

#define REDIRECTSTUB_SP_OFFSET_CONTEXT 0

#define CONTEXT_Pc 0x108
ASMCONSTANTS_C_ASSERT(CONTEXT_Pc == offsetof(T_CONTEXT,Pc))

#define SIZEOF__FaultingExceptionFrame                  (SIZEOF__Frame + 0x10 + SIZEOF__CONTEXT)
#define FaultingExceptionFrame__m_fFilterExecuted       SIZEOF__Frame
ASMCONSTANTS_C_ASSERT(SIZEOF__FaultingExceptionFrame        == sizeof(FaultingExceptionFrame));
ASMCONSTANTS_C_ASSERT(FaultingExceptionFrame__m_fFilterExecuted == offsetof(FaultingExceptionFrame, m_fFilterExecuted));

#define SIZEOF__FixupPrecode                 24
//#define Offset_PrecodeChunkIndex             15
//#define Offset_MethodDescChunkIndex          14
#define MethodDesc_ALIGNMENT_SHIFT           3
//#define FixupPrecode_ALIGNMENT_SHIFT_1       3
//#define FixupPrecode_ALIGNMENT_SHIFT_2       4

ASMCONSTANTS_C_ASSERT(SIZEOF__FixupPrecode == sizeof(FixupPrecode));
//ASMCONSTANTS_C_ASSERT(Offset_PrecodeChunkIndex == offsetof(FixupPrecode, m_PrecodeChunkIndex));
//ASMCONSTANTS_C_ASSERT(Offset_MethodDescChunkIndex == offsetof(FixupPrecode, m_MethodDescChunkIndex));
ASMCONSTANTS_C_ASSERT(MethodDesc_ALIGNMENT_SHIFT == MethodDesc::ALIGNMENT_SHIFT);
//ASMCONSTANTS_C_ASSERT((1<<FixupPrecode_ALIGNMENT_SHIFT_1) + (1<<FixupPrecode_ALIGNMENT_SHIFT_2)  == sizeof(FixupPrecode));

#ifdef FEATURE_VIRTUAL_STUB_DISPATCH
#define ResolveCacheElem__target      0x10
#define ResolveCacheElem__pNext       0x18
ASMCONSTANTS_C_ASSERT(ResolveCacheElem__target == offsetof(ResolveCacheElem, target));
ASMCONSTANTS_C_ASSERT(ResolveCacheElem__pNext == offsetof(ResolveCacheElem, pNext));
#endif // FEATURE_VIRTUAL_STUB_DISPATCH

#define                OFFSETOF__DynamicStaticsInfo__m_pMethodTable 0x10
ASMCONSTANTS_C_ASSERT(OFFSETOF__DynamicStaticsInfo__m_pMethodTable
                    == offsetof(DynamicStaticsInfo, m_pMethodTable));

#define                OFFSETOF__DynamicStaticsInfo__m_pNonGCStatics 0x8
ASMCONSTANTS_C_ASSERT(OFFSETOF__DynamicStaticsInfo__m_pNonGCStatics
                    == offsetof(DynamicStaticsInfo, m_pNonGCStatics));

#define                OFFSETOF__DynamicStaticsInfo__m_pGCStatics 0
ASMCONSTANTS_C_ASSERT(OFFSETOF__DynamicStaticsInfo__m_pGCStatics
                    == offsetof(DynamicStaticsInfo, m_pGCStatics));

// For JIT_PInvokeBegin and JIT_PInvokeEnd helpers
#define               Frame__m_Next 0x08
ASMCONSTANTS_C_ASSERT(Frame__m_Next == offsetof(Frame, m_Next))

#define               InlinedCallFrame__m_Datum 0x10
ASMCONSTANTS_C_ASSERT(InlinedCallFrame__m_Datum == offsetof(InlinedCallFrame, m_Datum))

#define               InlinedCallFrame__m_pCallSiteSP 0x18
ASMCONSTANTS_C_ASSERT(InlinedCallFrame__m_pCallSiteSP == offsetof(InlinedCallFrame, m_pCallSiteSP))

#define               InlinedCallFrame__m_pCallerReturnAddress 0x20
ASMCONSTANTS_C_ASSERT(InlinedCallFrame__m_pCallerReturnAddress == offsetof(InlinedCallFrame, m_pCallerReturnAddress))

#define               InlinedCallFrame__m_pCalleeSavedFP 0x28
ASMCONSTANTS_C_ASSERT(InlinedCallFrame__m_pCalleeSavedFP == offsetof(InlinedCallFrame, m_pCalleeSavedFP))

#define               InlinedCallFrame__m_pThread 0x30
ASMCONSTANTS_C_ASSERT(InlinedCallFrame__m_pThread == offsetof(InlinedCallFrame, m_pThread))

#define FixupPrecodeData__Target 0x00
ASMCONSTANTS_C_ASSERT(FixupPrecodeData__Target            == offsetof(FixupPrecodeData, Target))

#define FixupPrecodeData__MethodDesc 0x08
ASMCONSTANTS_C_ASSERT(FixupPrecodeData__MethodDesc        == offsetof(FixupPrecodeData, MethodDesc))

#define FixupPrecodeData__PrecodeFixupThunk 0x10
ASMCONSTANTS_C_ASSERT(FixupPrecodeData__PrecodeFixupThunk == offsetof(FixupPrecodeData, PrecodeFixupThunk))

#define StubPrecodeData__Target 0x08
ASMCONSTANTS_C_ASSERT(StubPrecodeData__Target            == offsetof(StubPrecodeData, Target))

#define StubPrecodeData__SecretParam 0x00
ASMCONSTANTS_C_ASSERT(StubPrecodeData__SecretParam        == offsetof(StubPrecodeData, SecretParam))

#define CallCountingStubData__RemainingCallCountCell 0x00
ASMCONSTANTS_C_ASSERT(CallCountingStubData__RemainingCallCountCell == offsetof(CallCountingStubData, RemainingCallCountCell))

#define CallCountingStubData__TargetForMethod 0x08
ASMCONSTANTS_C_ASSERT(CallCountingStubData__TargetForMethod == offsetof(CallCountingStubData, TargetForMethod))

#define CallCountingStubData__TargetForThresholdReached 0x10
ASMCONSTANTS_C_ASSERT(CallCountingStubData__TargetForThresholdReached == offsetof(CallCountingStubData, TargetForThresholdReached))

#ifdef FEATURE_CACHED_INTERFACE_DISPATCH
#define OFFSETOF__InterfaceDispatchCache__m_rgEntries 0x20
ASMCONSTANTS_C_ASSERT(OFFSETOF__InterfaceDispatchCache__m_rgEntries == offsetof(InterfaceDispatchCache, m_rgEntries))

#define OFFSETOF__InterfaceDispatchCell__m_pCache 0x08
ASMCONSTANTS_C_ASSERT(OFFSETOF__InterfaceDispatchCell__m_pCache == offsetof(InterfaceDispatchCell, m_pCache))
#endif // FEATURE_CACHED_INTERFACE_DISPATCH

#ifdef PROFILING_SUPPORTED
#define PROFILE_ENTER        0x1
#define PROFILE_LEAVE        0x2
#define PROFILE_TAILCALL     0x4

#define SIZEOF__PROFILE_PLATFORM_SPECIFIC_DATA 320
ASMCONSTANTS_C_ASSERT(SIZEOF__PROFILE_PLATFORM_SPECIFIC_DATA == sizeof(PROFILE_PLATFORM_SPECIFIC_DATA))
ASMCONSTANTS_C_ASSERT((SIZEOF__PROFILE_PLATFORM_SPECIFIC_DATA & 0xf) == 0)

#define ASMCONSTANTS_C_ASSERT_OFFSET(type, field) \
    ASMCONSTANTS_C_ASSERT(type##__##field == offsetof(type, field))

#define PROFILE_PLATFORM_SPECIFIC_DATA__Fp 0
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, Fp)
#define PROFILE_PLATFORM_SPECIFIC_DATA__Pc 8
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, Pc)
#define PROFILE_PLATFORM_SPECIFIC_DATA__x8 16
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, x8)
#define PROFILE_PLATFORM_SPECIFIC_DATA__argumentRegisters 24
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, argumentRegisters)
#define PROFILE_PLATFORM_SPECIFIC_DATA__functionId 88
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, functionId)
#define PROFILE_PLATFORM_SPECIFIC_DATA__floatArgumentRegisters 96
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, floatArgumentRegisters)
#define PROFILE_PLATFORM_SPECIFIC_DATA__probeSp 224
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, probeSp)
#define PROFILE_PLATFORM_SPECIFIC_DATA__profiledSp 232
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, profiledSp)
#define PROFILE_PLATFORM_SPECIFIC_DATA__hiddenArg 240
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, hiddenArg)
#define PROFILE_PLATFORM_SPECIFIC_DATA__flags 248
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, flags)
#define PROFILE_PLATFORM_SPECIFIC_DATA__unused 252
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, unused)
#define PROFILE_PLATFORM_SPECIFIC_DATA__buffer 256
ASMCONSTANTS_C_ASSERT_OFFSET(PROFILE_PLATFORM_SPECIFIC_DATA, buffer)

#undef ASMCONSTANTS_C_ASSERT_OFFSET
#endif  // PROFILING_SUPPORTED

#undef ASMCONSTANTS_RUNTIME_ASSERT
#undef ASMCONSTANTS_C_ASSERT
