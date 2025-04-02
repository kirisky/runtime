// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
//
// File: stubs.cpp
//
// This file contains stub functions for unimplemented features need to
// run on the ARM64 platform.

#include "common.h"
#include "dllimportcallback.h"
#include "comdelegate.h"
#include "asmconstants.h"
#include "virtualcallstub.h"
#include "jitinterface.h"
#include "ecall.h"

#ifdef FEATURE_PERFMAP
#include "perfmap.h"
#endif

#ifndef DACCESS_COMPILE
//-----------------------------------------------------------------------
// InstructionFormat for B.cond
//-----------------------------------------------------------------------
class ConditionalBranchInstructionFormat : public InstructionFormat
{

    public:
        ConditionalBranchInstructionFormat() : InstructionFormat(InstructionFormat::k32)
        {
            LIMITED_METHOD_CONTRACT;
        }

        virtual UINT GetSizeOfInstruction(UINT refsize, UINT variationCode)
        {
            LIMITED_METHOD_CONTRACT;

            _ASSERTE(refsize == InstructionFormat::k32);

            return 4;
        }

        virtual UINT GetHotSpotOffset(UINT refsize, UINT variationCode)
        {
            WRAPPER_NO_CONTRACT;
            return 0;
        }


        virtual BOOL CanReach(UINT refSize, UINT variationCode, BOOL fExternal, INT_PTR offset)
        {
            _ASSERTE(!fExternal || "ARM64:NYI - CompareAndBranchInstructionFormat::CanReach external");
            if (fExternal)
                return false;

            if (offset < -1048576 || offset > 1048572)
                return false;
            return true;
        }
        // B.<cond> <label>
        // Encoding 0|1|0|1|0|1|0|0|imm19|0|cond
        // cond = Bits3-0(variation)
        // imm19 = bits19-0(fixedUpReference/4), will be SignExtended
        virtual VOID EmitInstruction(UINT refSize, int64_t fixedUpReference, BYTE *pOutBufferRX, BYTE *pOutBufferRW, UINT variationCode, BYTE *pDataBuffer)
        {
            LIMITED_METHOD_CONTRACT;

            _ASSERTE(refSize == InstructionFormat::k32);

            if (fixedUpReference < -1048576 || fixedUpReference > 1048572)
                COMPlusThrow(kNotSupportedException);

            _ASSERTE((fixedUpReference & 0x3) == 0);
            DWORD imm19 = (DWORD)(0x7FFFF & (fixedUpReference >> 2));

            pOutBufferRW[0] = static_cast<BYTE>((0x7 & imm19 /* Bits2-0(imm19) */) << 5  | (0xF & variationCode /* cond */));
            pOutBufferRW[1] = static_cast<BYTE>((0x7F8 & imm19 /* Bits10-3(imm19) */) >> 3);
            pOutBufferRW[2] = static_cast<BYTE>((0x7F800 & imm19 /* Bits19-11(imm19) */) >> 11);
            pOutBufferRW[3] = static_cast<BYTE>(0x54);
        }
};

//-----------------------------------------------------------------------
// InstructionFormat for B(L)(R) (unconditional branch)
//-----------------------------------------------------------------------
class BranchInstructionFormat : public InstructionFormat
{
    // Encoding of the VariationCode:
    // bit(0) indicates whether this is a direct or an indirect jump.
    // bit(1) indicates whether this is a branch with link -a.k.a call- (BL(R)) or not (B(R))

    public:
        enum VariationCodes
        {
            BIF_VAR_INDIRECT           = 0x00000001,
            BIF_VAR_CALL               = 0x00000002,

            BIF_VAR_JUMP               = 0x00000000,
            BIF_VAR_INDIRECT_CALL      = 0x00000003
        };
    private:
        BOOL IsIndirect(UINT variationCode)
        {
            return (variationCode & BIF_VAR_INDIRECT) != 0;
        }
        BOOL IsCall(UINT variationCode)
        {
            return (variationCode & BIF_VAR_CALL) != 0;
        }


    public:
        BranchInstructionFormat() : InstructionFormat(InstructionFormat::k64)
        {
            LIMITED_METHOD_CONTRACT;
        }

        virtual UINT GetSizeOfInstruction(UINT refSize, UINT variationCode)
        {
            LIMITED_METHOD_CONTRACT;
            _ASSERTE(refSize == InstructionFormat::k64);

            if (IsIndirect(variationCode))
                return 12;
            else
                return 8;
        }

        virtual UINT GetSizeOfData(UINT refSize, UINT variationCode)
        {
            WRAPPER_NO_CONTRACT;
            return 8;
        }

        virtual UINT GetHotSpotOffset(UINT refsize, UINT variationCode)
        {
            WRAPPER_NO_CONTRACT;
            return 0;
        }

        virtual BOOL CanReach(UINT refSize, UINT variationCode, BOOL fExternal, INT_PTR offset)
        {
            if (fExternal)
            {
                // Note that the parameter 'offset' is not an offset but the target address itself (when fExternal is true)
                return (refSize == InstructionFormat::k64);
            }
            else
            {
                return ((offset >= -134217728 && offset <= 134217724) || (refSize == InstructionFormat::k64));
            }
        }

        virtual VOID EmitInstruction(UINT refSize, int64_t fixedUpReference, BYTE *pOutBufferRX, BYTE *pOutBufferRW, UINT variationCode, BYTE *pDataBuffer)
        {
            LIMITED_METHOD_CONTRACT;

            if (IsIndirect(variationCode))
            {
                _ASSERTE(((UINT_PTR)pDataBuffer & 7) == 0);
                int64_t dataOffset = pDataBuffer - pOutBufferRW;

                if (dataOffset < -1048576 || dataOffset > 1048572)
                    COMPlusThrow(kNotSupportedException);

                DWORD imm19 = (DWORD)(0x7FFFF & (dataOffset >> 2));

                // +0: ldr x16, [pc, #dataOffset]
                // +4: ldr x16, [x16]
                // +8: b(l)r x16
                *((DWORD*)pOutBufferRW) = (0x58000010 | (imm19 << 5));
                *((DWORD*)(pOutBufferRW+4)) = 0xF9400210;
                if (IsCall(variationCode))
                {
                    *((DWORD*)(pOutBufferRW+8)) = 0xD63F0200; // blr x16
                }
                else
                {
                    *((DWORD*)(pOutBufferRW+8)) = 0xD61F0200; // br x16
                }


                *((int64_t*)pDataBuffer) = fixedUpReference + (int64_t)pOutBufferRX;
            }
            else
            {

                _ASSERTE(((UINT_PTR)pDataBuffer & 7) == 0);
                int64_t dataOffset = pDataBuffer - pOutBufferRW;

                if (dataOffset < -1048576 || dataOffset > 1048572)
                    COMPlusThrow(kNotSupportedException);

                DWORD imm19 = (DWORD)(0x7FFFF & (dataOffset >> 2));

                // +0: ldr x16, [pc, #dataOffset]
                // +4: b(l)r x16
                *((DWORD*)pOutBufferRW) = (0x58000010 | (imm19 << 5));
                if (IsCall(variationCode))
                {
                    *((DWORD*)(pOutBufferRW+4)) = 0xD63F0200; // blr x16
                }
                else
                {
                    *((DWORD*)(pOutBufferRW+4)) = 0xD61F0200; // br x16
                }

                if (!ClrSafeInt<int64_t>::addition(fixedUpReference, (int64_t)pOutBufferRX, fixedUpReference))
                    COMPlusThrowArithmetic();
                *((int64_t*)pDataBuffer) = fixedUpReference;
            }
        }

};

//-----------------------------------------------------------------------
// InstructionFormat for loading a label to the register (ADRP/ADR)
//-----------------------------------------------------------------------
class LoadFromLabelInstructionFormat : public InstructionFormat
{
    public:
        LoadFromLabelInstructionFormat() : InstructionFormat( InstructionFormat::k32)
        {
            LIMITED_METHOD_CONTRACT;
        }

        virtual UINT GetSizeOfInstruction(UINT refSize, UINT variationCode)
        {
            WRAPPER_NO_CONTRACT;
            return 8;

        }

        virtual UINT GetHotSpotOffset(UINT refsize, UINT variationCode)
        {
            WRAPPER_NO_CONTRACT;
            return 0;
        }

        virtual BOOL CanReach(UINT refSize, UINT variationCode, BOOL fExternal, INT_PTR offset)
        {
            return fExternal;
        }

        virtual VOID EmitInstruction(UINT refSize, int64_t fixedUpReference, BYTE *pOutBufferRX, BYTE *pOutBufferRW, UINT variationCode, BYTE *pDataBuffer)
        {
            LIMITED_METHOD_CONTRACT;
            // VariationCode is used to indicate the register the label is going to be loaded

            DWORD imm =(DWORD)(fixedUpReference>>12);
            if (imm>>21)
                COMPlusThrow(kNotSupportedException);

            // Can't use SP or XZR
            _ASSERTE((variationCode & 0x1F) != 31);

            // adrp Xt, #Page_of_fixedUpReference
            *((DWORD*)pOutBufferRW) = ((9<<28) | ((imm & 3)<<29) | (imm>>2)<<5 | (variationCode&0x1F));

            // ldr Xt, [Xt, #offset_of_fixedUpReference_to_its_page]
            UINT64 target = (UINT64)(fixedUpReference + pOutBufferRX)>>3;
            *((DWORD*)(pOutBufferRW+4)) = ( 0xF9400000 | ((target & 0x1FF)<<10) | (variationCode & 0x1F)<<5 | (variationCode & 0x1F));
        }
};



static BYTE gConditionalBranchIF[sizeof(ConditionalBranchInstructionFormat)];
static BYTE gBranchIF[sizeof(BranchInstructionFormat)];
static BYTE gLoadFromLabelIF[sizeof(LoadFromLabelInstructionFormat)];

#endif

void ClearRegDisplayArgumentAndScratchRegisters(REGDISPLAY * pRD)
{
    for (int i=0; i < 18; i++)
        pRD->volatileCurrContextPointers.X[i] = NULL;
}

void LazyMachState::unwindLazyState(LazyMachState* baseState,
                                    MachState* unwoundstate,
                                    DWORD threadId,
                                    int funCallDepth)
{
    T_CONTEXT context;
    T_KNONVOLATILE_CONTEXT_POINTERS nonVolContextPtrs;

    context.ContextFlags = 0; // Read by PAL_VirtualUnwind.

    context.X19 = unwoundstate->captureX19_X29[0] = baseState->captureX19_X29[0];
    context.X20 = unwoundstate->captureX19_X29[1] = baseState->captureX19_X29[1];
    context.X21 = unwoundstate->captureX19_X29[2] = baseState->captureX19_X29[2];
    context.X22 = unwoundstate->captureX19_X29[3] = baseState->captureX19_X29[3];
    context.X23 = unwoundstate->captureX19_X29[4] = baseState->captureX19_X29[4];
    context.X24 = unwoundstate->captureX19_X29[5] = baseState->captureX19_X29[5];
    context.X25 = unwoundstate->captureX19_X29[6] = baseState->captureX19_X29[6];
    context.X26 = unwoundstate->captureX19_X29[7] = baseState->captureX19_X29[7];
    context.X27 = unwoundstate->captureX19_X29[8] = baseState->captureX19_X29[8];
    context.X28 = unwoundstate->captureX19_X29[9] = baseState->captureX19_X29[9];
    context.Fp  = unwoundstate->captureX19_X29[10] = baseState->captureX19_X29[10];
    context.Lr = 0; // Filled by the unwinder

    context.Sp = baseState->captureSp;
    context.Pc = baseState->captureIp;

#if !defined(DACCESS_COMPILE)
    // For DAC, if we get here, it means that the LazyMachState is uninitialized and we have to unwind it.
    // The API we use to unwind in DAC is StackWalk64(), which does not support the context pointers.
    //
    // Restore the integer registers to KNONVOLATILE_CONTEXT_POINTERS to be used for unwinding.
    nonVolContextPtrs.X19 = &unwoundstate->captureX19_X29[0];
    nonVolContextPtrs.X20 = &unwoundstate->captureX19_X29[1];
    nonVolContextPtrs.X21 = &unwoundstate->captureX19_X29[2];
    nonVolContextPtrs.X22 = &unwoundstate->captureX19_X29[3];
    nonVolContextPtrs.X23 = &unwoundstate->captureX19_X29[4];
    nonVolContextPtrs.X24 = &unwoundstate->captureX19_X29[5];
    nonVolContextPtrs.X25 = &unwoundstate->captureX19_X29[6];
    nonVolContextPtrs.X26 = &unwoundstate->captureX19_X29[7];
    nonVolContextPtrs.X27 = &unwoundstate->captureX19_X29[8];
    nonVolContextPtrs.X28 = &unwoundstate->captureX19_X29[9];
    nonVolContextPtrs.Fp  = &unwoundstate->captureX19_X29[10];
    nonVolContextPtrs.Lr = 0; // Filled by the unwinder

#endif // DACCESS_COMPILE

    LOG((LF_GCROOTS, LL_INFO100000, "STACKWALK    LazyMachState::unwindLazyState(ip:%p,sp:%p)\n", baseState->captureIp, baseState->captureSp));

    PCODE pvControlPc;

    do {

#ifndef TARGET_UNIX
        pvControlPc = Thread::VirtualUnwindCallFrame(&context, &nonVolContextPtrs);
#else // !TARGET_UNIX
#ifdef DACCESS_COMPILE
        HRESULT hr = DacVirtualUnwind(threadId, &context, &nonVolContextPtrs);
        if (FAILED(hr))
        {
            DacError(hr);
        }
#else // DACCESS_COMPILE
        BOOL success = PAL_VirtualUnwind(&context, &nonVolContextPtrs);
        if (!success)
        {
            _ASSERTE(!"unwindLazyState: Unwinding failed");
            EEPOLICY_HANDLE_FATAL_ERROR(COR_E_EXECUTIONENGINE);
        }
#endif // DACCESS_COMPILE
        pvControlPc = GetIP(&context);
#endif // !TARGET_UNIX

        if (funCallDepth > 0)
        {
            funCallDepth--;
            if (funCallDepth == 0)
                break;
        }
        else
        {
            // Determine  whether given IP resides in JITted code. (It returns nonzero in that case.)
            // Use it now to see if we've unwound to managed code yet.
            BOOL fIsManagedCode = ExecutionManager::IsManagedCode(pvControlPc);

            if (fIsManagedCode)
                break;

        }
    } while (true);

#ifdef __APPLE__
    unwoundstate->unwoundX19_X29[0] = context.X19;
    unwoundstate->unwoundX19_X29[1] = context.X20;
    unwoundstate->unwoundX19_X29[2] = context.X21;
    unwoundstate->unwoundX19_X29[3] = context.X22;
    unwoundstate->unwoundX19_X29[4] = context.X23;
    unwoundstate->unwoundX19_X29[5] = context.X24;
    unwoundstate->unwoundX19_X29[6] = context.X25;
    unwoundstate->unwoundX19_X29[7] = context.X26;
    unwoundstate->unwoundX19_X29[8] = context.X27;
    unwoundstate->unwoundX19_X29[9] = context.X28;
    unwoundstate->unwoundX19_X29[10] = context.Fp;
#endif // __APPLE__

#ifdef DACCESS_COMPILE
    // For DAC builds, we update the registers directly since we dont have context pointers
    unwoundstate->captureX19_X29[0] = context.X19;
    unwoundstate->captureX19_X29[1] = context.X20;
    unwoundstate->captureX19_X29[2] = context.X21;
    unwoundstate->captureX19_X29[3] = context.X22;
    unwoundstate->captureX19_X29[4] = context.X23;
    unwoundstate->captureX19_X29[5] = context.X24;
    unwoundstate->captureX19_X29[6] = context.X25;
    unwoundstate->captureX19_X29[7] = context.X26;
    unwoundstate->captureX19_X29[8] = context.X27;
    unwoundstate->captureX19_X29[9] = context.X28;
    unwoundstate->captureX19_X29[10] = context.Fp;
#else // !DACCESS_COMPILE
    // For non-DAC builds, update the register state from context pointers
    unwoundstate->ptrX19_X29[0] = nonVolContextPtrs.X19;
    unwoundstate->ptrX19_X29[1] = nonVolContextPtrs.X20;
    unwoundstate->ptrX19_X29[2] = nonVolContextPtrs.X21;
    unwoundstate->ptrX19_X29[3] = nonVolContextPtrs.X22;
    unwoundstate->ptrX19_X29[4] = nonVolContextPtrs.X23;
    unwoundstate->ptrX19_X29[5] = nonVolContextPtrs.X24;
    unwoundstate->ptrX19_X29[6] = nonVolContextPtrs.X25;
    unwoundstate->ptrX19_X29[7] = nonVolContextPtrs.X26;
    unwoundstate->ptrX19_X29[8] = nonVolContextPtrs.X27;
    unwoundstate->ptrX19_X29[9] = nonVolContextPtrs.X28;
    unwoundstate->ptrX19_X29[10] = nonVolContextPtrs.Fp;
#endif // DACCESS_COMPILE

    unwoundstate->_pc = context.Pc;
    unwoundstate->_sp = context.Sp;

    unwoundstate->_isValid = TRUE;
}

void HelperMethodFrame::UpdateRegDisplay_Impl(const PREGDISPLAY pRD, bool updateFloats)
{
    CONTRACTL
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
        SUPPORTS_DAC;
    }
    CONTRACTL_END;

#ifndef DACCESS_COMPILE
    if (updateFloats)
    {
        UpdateFloatingPointRegisters(pRD);
        _ASSERTE(pRD->pCurrentContext->Pc == GetReturnAddress());
    }
#endif // DACCESS_COMPILE

    pRD->IsCallerContextValid = FALSE;
    pRD->IsCallerSPValid      = FALSE;        // Don't add usage of this field.  This is only temporary.

    //
    // Copy the saved state from the frame to the current context.
    //

    LOG((LF_GCROOTS, LL_INFO100000, "STACKWALK    HelperMethodFrame::UpdateRegDisplay cached ip:%p, sp:%p\n", m_MachState._pc, m_MachState._sp));

 #if defined(DACCESS_COMPILE)
    // For DAC, we may get here when the HMF is still uninitialized.
    // So we may need to unwind here.
    if (!m_MachState.isValid())
    {
        // This allocation throws on OOM.
        MachState* pUnwoundState = (MachState*)DacAllocHostOnlyInstance(sizeof(*pUnwoundState), true);

        EnsureInit(pUnwoundState);

        pRD->pCurrentContext->Pc = pRD->ControlPC = pUnwoundState->_pc;
        pRD->pCurrentContext->Sp = pRD->SP        = pUnwoundState->_sp;

        pRD->pCurrentContext->X19 = (DWORD64)(pUnwoundState->captureX19_X29[0]);
        pRD->pCurrentContext->X20 = (DWORD64)(pUnwoundState->captureX19_X29[1]);
        pRD->pCurrentContext->X21 = (DWORD64)(pUnwoundState->captureX19_X29[2]);
        pRD->pCurrentContext->X22 = (DWORD64)(pUnwoundState->captureX19_X29[3]);
        pRD->pCurrentContext->X23 = (DWORD64)(pUnwoundState->captureX19_X29[4]);
        pRD->pCurrentContext->X24 = (DWORD64)(pUnwoundState->captureX19_X29[5]);
        pRD->pCurrentContext->X25 = (DWORD64)(pUnwoundState->captureX19_X29[6]);
        pRD->pCurrentContext->X26 = (DWORD64)(pUnwoundState->captureX19_X29[7]);
        pRD->pCurrentContext->X27 = (DWORD64)(pUnwoundState->captureX19_X29[8]);
        pRD->pCurrentContext->X28 = (DWORD64)(pUnwoundState->captureX19_X29[9]);
        pRD->pCurrentContext->Fp = (DWORD64)(pUnwoundState->captureX19_X29[10]);
        pRD->pCurrentContext->Lr = 0; // Unwind again to get Caller's PC

        pRD->pCurrentContextPointers->X19 = &pRD->pCurrentContext->X19;
        pRD->pCurrentContextPointers->X20 = &pRD->pCurrentContext->X20;
        pRD->pCurrentContextPointers->X21 = &pRD->pCurrentContext->X21;
        pRD->pCurrentContextPointers->X22 = &pRD->pCurrentContext->X22;
        pRD->pCurrentContextPointers->X23 = &pRD->pCurrentContext->X23;
        pRD->pCurrentContextPointers->X24 = &pRD->pCurrentContext->X24;
        pRD->pCurrentContextPointers->X25 = &pRD->pCurrentContext->X25;
        pRD->pCurrentContextPointers->X26 = &pRD->pCurrentContext->X26;
        pRD->pCurrentContextPointers->X27 = &pRD->pCurrentContext->X27;
        pRD->pCurrentContextPointers->X28 = &pRD->pCurrentContext->X28;
        pRD->pCurrentContextPointers->Fp = &pRD->pCurrentContext->Fp;
        pRD->pCurrentContextPointers->Lr = &pRD->pCurrentContext->Lr;

        return;
    }
#endif // DACCESS_COMPILE

    // reset pContext; it's only valid for active (top-most) frame
    pRD->pContext = NULL;
    pRD->ControlPC = GetReturnAddress(); // m_MachState._pc;
    pRD->SP = (DWORD64)(size_t)m_MachState._sp;

    pRD->pCurrentContext->Pc = pRD->ControlPC;
    pRD->pCurrentContext->Sp = pRD->SP;

#ifdef __APPLE__
    pRD->pCurrentContext->X19 = (DWORD64)(m_MachState.unwoundX19_X29[0]);
    pRD->pCurrentContext->X20 = (DWORD64)(m_MachState.unwoundX19_X29[1]);
    pRD->pCurrentContext->X21 = (DWORD64)(m_MachState.unwoundX19_X29[2]);
    pRD->pCurrentContext->X22 = (DWORD64)(m_MachState.unwoundX19_X29[3]);
    pRD->pCurrentContext->X23 = (DWORD64)(m_MachState.unwoundX19_X29[4]);
    pRD->pCurrentContext->X24 = (DWORD64)(m_MachState.unwoundX19_X29[5]);
    pRD->pCurrentContext->X25 = (DWORD64)(m_MachState.unwoundX19_X29[6]);
    pRD->pCurrentContext->X26 = (DWORD64)(m_MachState.unwoundX19_X29[7]);
    pRD->pCurrentContext->X27 = (DWORD64)(m_MachState.unwoundX19_X29[8]);
    pRD->pCurrentContext->X28 = (DWORD64)(m_MachState.unwoundX19_X29[9]);
    pRD->pCurrentContext->Fp = (DWORD64)(m_MachState.unwoundX19_X29[10]);
    pRD->pCurrentContext->Lr = 0; // Unwind again to get Caller's PC
#else // __APPLE__
    pRD->pCurrentContext->X19 = *m_MachState.ptrX19_X29[0];
    pRD->pCurrentContext->X20 = *m_MachState.ptrX19_X29[1];
    pRD->pCurrentContext->X21 = *m_MachState.ptrX19_X29[2];
    pRD->pCurrentContext->X22 = *m_MachState.ptrX19_X29[3];
    pRD->pCurrentContext->X23 = *m_MachState.ptrX19_X29[4];
    pRD->pCurrentContext->X24 = *m_MachState.ptrX19_X29[5];
    pRD->pCurrentContext->X25 = *m_MachState.ptrX19_X29[6];
    pRD->pCurrentContext->X26 = *m_MachState.ptrX19_X29[7];
    pRD->pCurrentContext->X27 = *m_MachState.ptrX19_X29[8];
    pRD->pCurrentContext->X28 = *m_MachState.ptrX19_X29[9];
    pRD->pCurrentContext->Fp  = *m_MachState.ptrX19_X29[10];
    pRD->pCurrentContext->Lr = 0; // Unwind again to get Caller's PC
#endif // __APPLE__

#if !defined(DACCESS_COMPILE)
    pRD->pCurrentContextPointers->X19 = m_MachState.ptrX19_X29[0];
    pRD->pCurrentContextPointers->X20 = m_MachState.ptrX19_X29[1];
    pRD->pCurrentContextPointers->X21 = m_MachState.ptrX19_X29[2];
    pRD->pCurrentContextPointers->X22 = m_MachState.ptrX19_X29[3];
    pRD->pCurrentContextPointers->X23 = m_MachState.ptrX19_X29[4];
    pRD->pCurrentContextPointers->X24 = m_MachState.ptrX19_X29[5];
    pRD->pCurrentContextPointers->X25 = m_MachState.ptrX19_X29[6];
    pRD->pCurrentContextPointers->X26 = m_MachState.ptrX19_X29[7];
    pRD->pCurrentContextPointers->X27 = m_MachState.ptrX19_X29[8];
    pRD->pCurrentContextPointers->X28 = m_MachState.ptrX19_X29[9];
    pRD->pCurrentContextPointers->Fp = m_MachState.ptrX19_X29[10];
    pRD->pCurrentContextPointers->Lr = 0; // Unwind again to get Caller's PC
#endif

    ClearRegDisplayArgumentAndScratchRegisters(pRD);
}

void UpdateRegDisplayFromCalleeSavedRegisters(REGDISPLAY * pRD, CalleeSavedRegisters * pCalleeSaved)
{
    LIMITED_METHOD_CONTRACT;

    pRD->pCurrentContext->X19 = pCalleeSaved->x19;
    pRD->pCurrentContext->X20 = pCalleeSaved->x20;
    pRD->pCurrentContext->X21 = pCalleeSaved->x21;
    pRD->pCurrentContext->X22 = pCalleeSaved->x22;
    pRD->pCurrentContext->X23 = pCalleeSaved->x23;
    pRD->pCurrentContext->X24 = pCalleeSaved->x24;
    pRD->pCurrentContext->X25 = pCalleeSaved->x25;
    pRD->pCurrentContext->X26 = pCalleeSaved->x26;
    pRD->pCurrentContext->X27 = pCalleeSaved->x27;
    pRD->pCurrentContext->X28 = pCalleeSaved->x28;
    pRD->pCurrentContext->Fp  = pCalleeSaved->x29;
    pRD->pCurrentContext->Lr  = pCalleeSaved->x30;

    T_KNONVOLATILE_CONTEXT_POINTERS * pContextPointers = pRD->pCurrentContextPointers;
    pContextPointers->X19 = (PDWORD64)&pCalleeSaved->x19;
    pContextPointers->X20 = (PDWORD64)&pCalleeSaved->x20;
    pContextPointers->X21 = (PDWORD64)&pCalleeSaved->x21;
    pContextPointers->X22 = (PDWORD64)&pCalleeSaved->x22;
    pContextPointers->X23 = (PDWORD64)&pCalleeSaved->x23;
    pContextPointers->X24 = (PDWORD64)&pCalleeSaved->x24;
    pContextPointers->X25 = (PDWORD64)&pCalleeSaved->x25;
    pContextPointers->X26 = (PDWORD64)&pCalleeSaved->x26;
    pContextPointers->X27 = (PDWORD64)&pCalleeSaved->x27;
    pContextPointers->X28 = (PDWORD64)&pCalleeSaved->x28;
    pContextPointers->Fp  = (PDWORD64)&pCalleeSaved->x29;
    pContextPointers->Lr  = (PDWORD64)&pCalleeSaved->x30;
}


void TransitionFrame::UpdateRegDisplay_Impl(const PREGDISPLAY pRD, bool updateFloats)
{
#ifndef DACCESS_COMPILE
    if (updateFloats)
    {
        UpdateFloatingPointRegisters(pRD);
        _ASSERTE(pRD->pCurrentContext->Pc == GetReturnAddress());
    }
#endif // DACCESS_COMPILE

    pRD->IsCallerContextValid = FALSE;
    pRD->IsCallerSPValid      = FALSE;        // Don't add usage of this field.  This is only temporary.

    // copy the callee saved regs
    CalleeSavedRegisters *pCalleeSaved = GetCalleeSavedRegisters();
    UpdateRegDisplayFromCalleeSavedRegisters(pRD, pCalleeSaved);

    ClearRegDisplayArgumentAndScratchRegisters(pRD);

    // copy the control registers
    pRD->pCurrentContext->Fp = pCalleeSaved->x29;
    pRD->pCurrentContext->Lr = pCalleeSaved->x30;
    pRD->pCurrentContext->Pc = GetReturnAddress();
    pRD->pCurrentContext->Sp = this->GetSP();

    // Finally, syncup the regdisplay with the context
    SyncRegDisplayToCurrentContext(pRD);

    LOG((LF_GCROOTS, LL_INFO100000, "STACKWALK    TransitionFrame::UpdateRegDisplay_Impl(pc:%p, sp:%p)\n", pRD->ControlPC, pRD->SP));
}



void FaultingExceptionFrame::UpdateRegDisplay_Impl(const PREGDISPLAY pRD, bool updateFloats)
{
    LIMITED_METHOD_DAC_CONTRACT;

    // Copy the context to regdisplay
    memcpy(pRD->pCurrentContext, &m_ctx, sizeof(T_CONTEXT));

    // Clear the CONTEXT_XSTATE, since the REGDISPLAY contains just plain CONTEXT structure
    // that cannot contain any extended state.
    pRD->pCurrentContext->ContextFlags &= ~(CONTEXT_XSTATE & CONTEXT_AREA_MASK);

    pRD->ControlPC = ::GetIP(&m_ctx);
    pRD->SP = ::GetSP(&m_ctx);

    // Update the integer registers in KNONVOLATILE_CONTEXT_POINTERS from
    // the exception context we have.
    pRD->pCurrentContextPointers->X19 = (PDWORD64)&m_ctx.X19;
    pRD->pCurrentContextPointers->X20 = (PDWORD64)&m_ctx.X20;
    pRD->pCurrentContextPointers->X21 = (PDWORD64)&m_ctx.X21;
    pRD->pCurrentContextPointers->X22 = (PDWORD64)&m_ctx.X22;
    pRD->pCurrentContextPointers->X23 = (PDWORD64)&m_ctx.X23;
    pRD->pCurrentContextPointers->X24 = (PDWORD64)&m_ctx.X24;
    pRD->pCurrentContextPointers->X25 = (PDWORD64)&m_ctx.X25;
    pRD->pCurrentContextPointers->X26 = (PDWORD64)&m_ctx.X26;
    pRD->pCurrentContextPointers->X27 = (PDWORD64)&m_ctx.X27;
    pRD->pCurrentContextPointers->X28 = (PDWORD64)&m_ctx.X28;
    pRD->pCurrentContextPointers->Fp = (PDWORD64)&m_ctx.Fp;
    pRD->pCurrentContextPointers->Lr = (PDWORD64)&m_ctx.Lr;

    ClearRegDisplayArgumentAndScratchRegisters(pRD);

    pRD->IsCallerContextValid = FALSE;
    pRD->IsCallerSPValid      = FALSE;        // Don't add usage of this field.  This is only temporary.

    LOG((LF_GCROOTS, LL_INFO100000, "STACKWALK    FaultingExceptionFrame::UpdateRegDisplay_Impl(pc:%p, sp:%p)\n", pRD->ControlPC, pRD->SP));
}

void InlinedCallFrame::UpdateRegDisplay_Impl(const PREGDISPLAY pRD, bool updateFloats)
{
    CONTRACT_VOID
    {
        NOTHROW;
        GC_NOTRIGGER;
#ifdef PROFILING_SUPPORTED
        PRECONDITION(CORProfilerStackSnapshotEnabled() || InlinedCallFrame::FrameHasActiveCall(this));
#endif
        MODE_ANY;
        SUPPORTS_DAC;
    }
    CONTRACT_END;

    if (!InlinedCallFrame::FrameHasActiveCall(this))
    {
        LOG((LF_CORDB, LL_ERROR, "WARNING: InlinedCallFrame::UpdateRegDisplay called on inactive frame %p\n", this));
        return;
    }

#ifndef DACCESS_COMPILE
    if (updateFloats)
    {
        UpdateFloatingPointRegisters(pRD);
    }
#endif // DACCESS_COMPILE

    pRD->IsCallerContextValid = FALSE;
    pRD->IsCallerSPValid      = FALSE;

    pRD->pCurrentContext->Pc = *(DWORD64 *)&m_pCallerReturnAddress;
    pRD->pCurrentContext->Sp = *(DWORD64 *)&m_pCallSiteSP;
    pRD->pCurrentContext->Fp = *(DWORD64 *)&m_pCalleeSavedFP;

    pRD->pCurrentContextPointers->X19 = NULL;
    pRD->pCurrentContextPointers->X20 = NULL;
    pRD->pCurrentContextPointers->X21 = NULL;
    pRD->pCurrentContextPointers->X22 = NULL;
    pRD->pCurrentContextPointers->X23 = NULL;
    pRD->pCurrentContextPointers->X24 = NULL;
    pRD->pCurrentContextPointers->X25 = NULL;
    pRD->pCurrentContextPointers->X26 = NULL;
    pRD->pCurrentContextPointers->X27 = NULL;
    pRD->pCurrentContextPointers->X28 = NULL;

    pRD->ControlPC = m_pCallerReturnAddress;
    pRD->SP = (DWORD64) dac_cast<TADDR>(m_pCallSiteSP);

    // reset pContext; it's only valid for active (top-most) frame
    pRD->pContext = NULL;

    ClearRegDisplayArgumentAndScratchRegisters(pRD);


    // Update the frame pointer in the current context.
    pRD->pCurrentContextPointers->Fp = (DWORD64 *)&m_pCalleeSavedFP;

    LOG((LF_GCROOTS, LL_INFO100000, "STACKWALK    InlinedCallFrame::UpdateRegDisplay_Impl(pc:%p, sp:%p)\n", pRD->ControlPC, pRD->SP));

    RETURN;
}

#ifdef FEATURE_HIJACK
TADDR ResumableFrame::GetReturnAddressPtr_Impl(void)
{
    LIMITED_METHOD_DAC_CONTRACT;
    return dac_cast<TADDR>(m_Regs) + offsetof(T_CONTEXT, Pc);
}

void ResumableFrame::UpdateRegDisplay_Impl(const PREGDISPLAY pRD, bool updateFloats)
{
    CONTRACT_VOID
    {
        NOTHROW;
        GC_NOTRIGGER;
        MODE_ANY;
        SUPPORTS_DAC;
    }
    CONTRACT_END;

    CopyMemory(pRD->pCurrentContext, m_Regs, sizeof(T_CONTEXT));

    pRD->ControlPC = m_Regs->Pc;
    pRD->SP = m_Regs->Sp;

    pRD->pCurrentContextPointers->X19 = &m_Regs->X19;
    pRD->pCurrentContextPointers->X20 = &m_Regs->X20;
    pRD->pCurrentContextPointers->X21 = &m_Regs->X21;
    pRD->pCurrentContextPointers->X22 = &m_Regs->X22;
    pRD->pCurrentContextPointers->X23 = &m_Regs->X23;
    pRD->pCurrentContextPointers->X24 = &m_Regs->X24;
    pRD->pCurrentContextPointers->X25 = &m_Regs->X25;
    pRD->pCurrentContextPointers->X26 = &m_Regs->X26;
    pRD->pCurrentContextPointers->X27 = &m_Regs->X27;
    pRD->pCurrentContextPointers->X28 = &m_Regs->X28;
    pRD->pCurrentContextPointers->Fp  = &m_Regs->Fp;
    pRD->pCurrentContextPointers->Lr  = &m_Regs->Lr;

    for (int i=0; i < 18; i++)
        pRD->volatileCurrContextPointers.X[i] = &m_Regs->X[i];

    pRD->IsCallerContextValid = FALSE;
    pRD->IsCallerSPValid      = FALSE;        // Don't add usage of this field.  This is only temporary.

    LOG((LF_GCROOTS, LL_INFO100000, "STACKWALK    ResumableFrame::UpdateRegDisplay_Impl(pc:%p, sp:%p)\n", pRD->ControlPC, pRD->SP));

    RETURN;
}

void HijackFrame::UpdateRegDisplay_Impl(const PREGDISPLAY pRD, bool updateFloats)
{
    LIMITED_METHOD_CONTRACT;

     pRD->IsCallerContextValid = FALSE;
     pRD->IsCallerSPValid      = FALSE;

     pRD->pCurrentContext->Pc = m_ReturnAddress;
     size_t s = sizeof(struct HijackArgs);
     _ASSERTE(s%8 == 0); // HijackArgs contains register values and hence will be a multiple of 8
     // stack must be multiple of 16. So if s is not multiple of 16 then there must be padding of 8 bytes
     s = s + s%16;
     pRD->pCurrentContext->Sp = PTR_TO_TADDR(m_Args) + s ;

     pRD->pCurrentContext->X0 = m_Args->X0;
     pRD->pCurrentContext->X1 = m_Args->X1;

     pRD->volatileCurrContextPointers.X0 = &m_Args->X0;
     pRD->volatileCurrContextPointers.X1 = &m_Args->X1;

     pRD->pCurrentContext->X19 = m_Args->X19;
     pRD->pCurrentContext->X20 = m_Args->X20;
     pRD->pCurrentContext->X21 = m_Args->X21;
     pRD->pCurrentContext->X22 = m_Args->X22;
     pRD->pCurrentContext->X23 = m_Args->X23;
     pRD->pCurrentContext->X24 = m_Args->X24;
     pRD->pCurrentContext->X25 = m_Args->X25;
     pRD->pCurrentContext->X26 = m_Args->X26;
     pRD->pCurrentContext->X27 = m_Args->X27;
     pRD->pCurrentContext->X28 = m_Args->X28;
     pRD->pCurrentContext->Fp = m_Args->X29;
     pRD->pCurrentContext->Lr = m_Args->Lr;

     pRD->pCurrentContextPointers->X19 = &m_Args->X19;
     pRD->pCurrentContextPointers->X20 = &m_Args->X20;
     pRD->pCurrentContextPointers->X21 = &m_Args->X21;
     pRD->pCurrentContextPointers->X22 = &m_Args->X22;
     pRD->pCurrentContextPointers->X23 = &m_Args->X23;
     pRD->pCurrentContextPointers->X24 = &m_Args->X24;
     pRD->pCurrentContextPointers->X25 = &m_Args->X25;
     pRD->pCurrentContextPointers->X26 = &m_Args->X26;
     pRD->pCurrentContextPointers->X27 = &m_Args->X27;
     pRD->pCurrentContextPointers->X28 = &m_Args->X28;
     pRD->pCurrentContextPointers->Fp = &m_Args->X29;
     pRD->pCurrentContextPointers->Lr = NULL;

     SyncRegDisplayToCurrentContext(pRD);

    LOG((LF_GCROOTS, LL_INFO100000, "STACKWALK    HijackFrame::UpdateRegDisplay_Impl(pc:%p, sp:%p)\n", pRD->ControlPC, pRD->SP));
}
#endif // FEATURE_HIJACK

#ifdef FEATURE_COMINTEROP

void emitCOMStubCall (ComCallMethodDesc *pCOMMethodRX, ComCallMethodDesc *pCOMMethodRW, PCODE target)
{
    WRAPPER_NO_CONTRACT;

	// adr x12, label_comCallMethodDesc
	// ldr x10, label_target
	// br x10
	// 4 byte padding for alignment
	// label_target:
    // target address (8 bytes)
    // label_comCallMethodDesc:
    DWORD rgCode[] = {
        0x100000cc,
        0x5800006a,
        0xd61f0140
    };

    BYTE *pBufferRX = (BYTE*)pCOMMethodRX - COMMETHOD_CALL_PRESTUB_SIZE;
    BYTE *pBufferRW = (BYTE*)pCOMMethodRW - COMMETHOD_CALL_PRESTUB_SIZE;

    memcpy(pBufferRW, rgCode, sizeof(rgCode));
    *((PCODE*)(pBufferRW + sizeof(rgCode) + 4)) = target;

    // Ensure that the updated instructions get actually written
    ClrFlushInstructionCache(pBufferRX, COMMETHOD_CALL_PRESTUB_SIZE);

    _ASSERTE(IS_ALIGNED(pBufferRX + COMMETHOD_CALL_PRESTUB_ADDRESS_OFFSET, sizeof(void*)) &&
             *((PCODE*)(pBufferRX + COMMETHOD_CALL_PRESTUB_ADDRESS_OFFSET)) == target);
}
#endif // FEATURE_COMINTEROP

#if !defined(DACCESS_COMPILE)
EXTERN_C void JIT_UpdateWriteBarrierState(bool skipEphemeralCheck, size_t writeableOffset);

extern "C" void STDCALL JIT_PatchedCodeStart();
extern "C" void STDCALL JIT_PatchedCodeLast();

static void UpdateWriteBarrierState(bool skipEphemeralCheck)
{
    BYTE *writeBarrierCodeStart = GetWriteBarrierCodeLocation((void*)JIT_PatchedCodeStart);
    BYTE *writeBarrierCodeStartRW = writeBarrierCodeStart;
    ExecutableWriterHolderNoLog<BYTE> writeBarrierWriterHolder;
    if (IsWriteBarrierCopyEnabled())
    {
        writeBarrierWriterHolder.AssignExecutableWriterHolder(writeBarrierCodeStart, (BYTE*)JIT_PatchedCodeLast - (BYTE*)JIT_PatchedCodeStart);
        writeBarrierCodeStartRW = writeBarrierWriterHolder.GetRW();
    }
    JIT_UpdateWriteBarrierState(GCHeapUtilities::IsServerHeap(), writeBarrierCodeStartRW - writeBarrierCodeStart);
}

void InitJITHelpers1()
{
    STANDARD_VM_CONTRACT;

    _ASSERTE(g_SystemInfo.dwNumberOfProcessors != 0);

    // Allocation helpers, faster but non-logging
    if (!((TrackAllocationsEnabled()) ||
        (LoggingOn(LF_GCALLOC, LL_INFO10))
#ifdef _DEBUG
        || (g_pConfig->ShouldInjectFault(INJECTFAULT_GCHEAP) != 0)
#endif // _DEBUG
        ))
    {
        if (GCHeapUtilities::UseThreadAllocationContexts())
        {
            SetJitHelperFunction(CORINFO_HELP_NEWSFAST, JIT_NewS_MP_FastPortable);
            SetJitHelperFunction(CORINFO_HELP_NEWSFAST_ALIGN8, JIT_NewS_MP_FastPortable);
            SetJitHelperFunction(CORINFO_HELP_NEWARR_1_VC, JIT_NewArr1VC_MP_FastPortable);
            SetJitHelperFunction(CORINFO_HELP_NEWARR_1_OBJ, JIT_NewArr1OBJ_MP_FastPortable);
            SetJitHelperFunction(CORINFO_HELP_BOX, JIT_Box_MP_FastPortable);

            ECall::DynamicallyAssignFCallImpl(GetEEFuncEntryPoint(AllocateString_MP_FastPortable), ECall::FastAllocateString);
        }
    }

    UpdateWriteBarrierState(GCHeapUtilities::IsServerHeap());
}


#else
void UpdateWriteBarrierState(bool) {}
#endif // !defined(DACCESS_COMPILE)

#ifdef TARGET_WINDOWS
PTR_CONTEXT GetCONTEXTFromRedirectedStubStackFrame(T_DISPATCHER_CONTEXT * pDispatcherContext)
{
    LIMITED_METHOD_DAC_CONTRACT;

    DWORD64 stackSlot = pDispatcherContext->EstablisherFrame + REDIRECTSTUB_SP_OFFSET_CONTEXT;
    PTR_PTR_CONTEXT ppContext = dac_cast<PTR_PTR_CONTEXT>((TADDR)stackSlot);
    return *ppContext;
}
#endif // TARGET_WINDOWS

PTR_CONTEXT GetCONTEXTFromRedirectedStubStackFrame(T_CONTEXT * pContext)
{
    LIMITED_METHOD_DAC_CONTRACT;

    DWORD64 stackSlot = pContext->Sp + REDIRECTSTUB_SP_OFFSET_CONTEXT;
    PTR_PTR_CONTEXT ppContext = dac_cast<PTR_PTR_CONTEXT>((TADDR)stackSlot);
    return *ppContext;
}

#if !defined(DACCESS_COMPILE)
#ifdef TARGET_WINDOWS
FaultingExceptionFrame *GetFrameFromRedirectedStubStackFrame (DISPATCHER_CONTEXT *pDispatcherContext)
{
    LIMITED_METHOD_CONTRACT;

    return (FaultingExceptionFrame*)((TADDR)pDispatcherContext->ContextRecord->X19);
}
#endif // TARGET_WINDOWS

BOOL
AdjustContextForVirtualStub(
        EXCEPTION_RECORD *pExceptionRecord,
        CONTEXT *pContext)
{
    LIMITED_METHOD_CONTRACT;

    Thread * pThread = GetThreadNULLOk();

    // We may not have a managed thread object. Example is an AV on the helper thread.
    // (perhaps during StubManager::IsStub)
    if (pThread == NULL)
    {
        return FALSE;
    }

    PCODE f_IP = GetIP(pContext);

    bool isVirtualStubNullCheck = false;
#ifdef FEATURE_CACHED_INTERFACE_DISPATCH
    if (VirtualCallStubManager::isCachedInterfaceDispatchStubAVLocation(f_IP))
    {
        isVirtualStubNullCheck = true; 
    }
#endif // FEATURE_CACHED_INTERFACE_DISPATCH
#ifdef FEATURE_VIRTUAL_STUB_DISPATCH
    if (!isVirtualStubNullCheck)
    {
        StubCodeBlockKind sk = RangeSectionStubManager::GetStubKind(f_IP);

        if (sk == STUB_CODE_BLOCK_VSD_DISPATCH_STUB)
        {
            if (*PTR_DWORD(f_IP) != DISPATCH_STUB_FIRST_DWORD)
            {
                _ASSERTE(!"AV in DispatchStub at unknown instruction");
            }
            else
            {
                isVirtualStubNullCheck = true;
            }
        }
        else
        if (sk == STUB_CODE_BLOCK_VSD_RESOLVE_STUB)
        {
            if (*PTR_DWORD(f_IP) != RESOLVE_STUB_FIRST_DWORD)
            {
                _ASSERTE(!"AV in ResolveStub at unknown instruction");
            }
            else
            {
                isVirtualStubNullCheck = true;
            }
        }
    }
#endif // FEATURE_VIRTUAL_STUB_DISPATCH

    if (!isVirtualStubNullCheck)
    {
        return FALSE;
    }

    PCODE callsite = GetAdjustedCallAddress(GetLR(pContext));

    // Lr must already have been saved before calling so it should not be necessary to restore Lr

    if (pExceptionRecord != NULL)
    {
        pExceptionRecord->ExceptionAddress = (PVOID)callsite;
    }

    SetIP(pContext, callsite);

    return TRUE;
}
#endif // !DACCESS_COMPILE

#if !defined(DACCESS_COMPILE)
VOID ResetCurrentContext()
{
    LIMITED_METHOD_CONTRACT;
}
#endif

LONG CLRNoCatchHandler(EXCEPTION_POINTERS* pExceptionInfo, PVOID pv)
{
    return EXCEPTION_CONTINUE_SEARCH;
}

void FlushWriteBarrierInstructionCache()
{
    // this wouldn't be called in arm64, just to comply with gchelpers.h
}

int StompWriteBarrierEphemeral(bool isRuntimeSuspended)
{
    UpdateWriteBarrierState(GCHeapUtilities::IsServerHeap());
    return SWB_PASS;
}

int StompWriteBarrierResize(bool isRuntimeSuspended, bool bReqUpperBoundsCheck)
{
    UpdateWriteBarrierState(GCHeapUtilities::IsServerHeap());
    return SWB_PASS;
}

#ifdef FEATURE_USE_SOFTWARE_WRITE_WATCH_FOR_GC_HEAP
int SwitchToWriteWatchBarrier(bool isRuntimeSuspended)
{
    UpdateWriteBarrierState(GCHeapUtilities::IsServerHeap());
    return SWB_PASS;
}

int SwitchToNonWriteWatchBarrier(bool isRuntimeSuspended)
{
    UpdateWriteBarrierState(GCHeapUtilities::IsServerHeap());
    return SWB_PASS;
}
#endif // FEATURE_USE_SOFTWARE_WRITE_WATCH_FOR_GC_HEAP

#ifdef DACCESS_COMPILE
BOOL GetAnyThunkTarget (T_CONTEXT *pctx, TADDR *pTarget, TADDR *pTargetMethodDesc)
{
    _ASSERTE(!"ARM64:NYI");
    return FALSE;
}
#endif // DACCESS_COMPILE

#ifndef DACCESS_COMPILE
// ----------------------------------------------------------------
// StubLinkerCPU methods
// ----------------------------------------------------------------

void StubLinkerCPU::EmitMovConstant(IntReg target, UINT64 constant)
{
#define WORD_MASK 0xFFFF

        // Move the 64bit constant in 4 chunks (of 16 bits).
        // MOVZ Rd, <1st word>, LSL 0
        // MOVK Rd, <2nd word>, LSL 1
        // MOVK Rd, <3nd word>, LSL 2
        // MOVK Rd, <4nd word>, LSL 3

        DWORD movInstr = 0xD2; // MOVZ
        int shift = 0;
        do
        {
            WORD word = (WORD) (constant & WORD_MASK);
            Emit32((DWORD)(movInstr<<24 | (1 << 23) | (shift)<<21 | word<<5 | target));
            shift++;
            movInstr = 0xF2; // MOVK
        }
        while ((constant >>= 16) != 0);


#undef WORD_MASK
}

void StubLinkerCPU::EmitCmpImm(IntReg reg, int imm)
{

    if (0 <= imm && imm < 4096)
    {
        // CMP <Xn|SP>, #<imm>{, <shift>}
        // Encoding: 1|1|1|1|0|0|0|0|shift(2)|imm(12)|Rn|Rt
        // Where I encode shift as 0 and Rt has to be 1F
        Emit32((DWORD) ((0xF1<<24) | ((0xFFF & imm)<<10) | (reg<<5) | (0x1F)) );

    }
    else
        _ASSERTE(!"ARM64: NYI");
}

void StubLinkerCPU::EmitCmpReg(IntReg Xn, IntReg Xm)
{

    // Encoding for CMP (shifted register)
    // sf|1|1|0|1|0|1|1|shift(2)|0|Xm(5)|imm(6)|Xn(5)|XZR(5)
    // where
    //    sf = 1 for 64-bit variant,
    //    shift will be set to 00 (LSL)
    //    imm(6), which is the shift amount, will be set to 0

    Emit32((DWORD) (0xEB<<24) | (Xm<<16) | (Xn<<5) | 0x1F);
}

void StubLinkerCPU::EmitCondFlagJump(CodeLabel * target, UINT cond)
{
    WRAPPER_NO_CONTRACT;
    EmitLabelRef(target, reinterpret_cast<ConditionalBranchInstructionFormat&>(gConditionalBranchIF), cond);
}

void StubLinkerCPU::EmitJumpRegister(IntReg regTarget)
{
    // br regTarget
    Emit32((DWORD) (0x3587C0<<10 | regTarget<<5));
}

void StubLinkerCPU::EmitRet(IntReg Xn)
{
    // Encoding: 1101011001011111000000| Rn |00000
    Emit32((DWORD)(0xD65F0000 | (Xn << 5)));
}

void StubLinkerCPU::EmitLoadStoreRegPairImm(DWORD flags, IntReg Xt1, IntReg Xt2, IntReg Xn, int offset)
{
    EmitLoadStoreRegPairImm(flags, (int)Xt1, (int)Xt2, Xn, offset, FALSE);
}

void StubLinkerCPU::EmitLoadStoreRegPairImm(DWORD flags, VecReg Vt1, VecReg Vt2, IntReg Xn, int offset)
{
    EmitLoadStoreRegPairImm(flags, (int)Vt1, (int)Vt2, Xn, offset, TRUE);
}

void StubLinkerCPU::EmitLoadStoreRegPairImm(DWORD flags, int regNum1, int regNum2, IntReg Xn, int offset, BOOL isVec)
{
    // Encoding:
    // [opc(2)] | 1 | 0 | 1 | [IsVec(1)] | 0 | [!postIndex(1)] | [writeBack(1)] | [isLoad(1)] | [imm(7)] | [Xt2(5)] | [Xn(5)] | [Xt1(5)]
    // where opc=01 and if isVec==1, opc=10 otherwise

    BOOL isLoad    = flags & 1;
    BOOL writeBack = flags & 2;
    BOOL postIndex = flags & 4;
    _ASSERTE((-512 <= offset) && (offset <= 504));
    _ASSERTE((offset & 7) == 0);
    int opc = isVec ? 1 : 2;
    Emit32((DWORD) ( (opc<<30) | // opc
                     (0x5<<27) |
                     (!!isVec<<26) |
                     (!postIndex<<24) |
                     (!!writeBack<<23) |
                     (!!isLoad<<22) |
                     ((0x7F & (offset >> 3)) << 15) |
                     (regNum2 << 10) |
                     (Xn << 5) |
                     (regNum1)
                   ));

}


void StubLinkerCPU::EmitLoadStoreRegImm(DWORD flags, IntReg Xt, IntReg Xn, int offset, int log2Size)
{
    EmitLoadStoreRegImm(flags, (int)Xt, Xn, offset, FALSE, log2Size);
}
void StubLinkerCPU::EmitLoadStoreRegImm(DWORD flags, VecReg Vt, IntReg Xn, int offset)
{
    EmitLoadStoreRegImm(flags, (int)Vt, Xn, offset, TRUE);
}

void StubLinkerCPU::EmitLoadStoreRegImm(DWORD flags, int regNum, IntReg Xn, int offset, BOOL isVec, int log2Size)
{
    // Encoding:
    // wb=1 : [size(2)=11] | 1 | 1 | 1 | [IsVec(1)] | 0 | [!writeBack(1)] | 0 | [isLoad(1)] | 0 | [imm(7)] | [!postIndex(1)] | [Xn(5)] | [Xt(5)]
    // wb=0 : [size(2)=11] | 1 | 1 | 1 | [IsVec(1)] | 0 | [!writeBack(1)] | 0 | [isLoad(1)] | [          imm(12)           ] | [Xn(5)] | [Xt(5)]
    // where IsVec=0 for IntReg, 1 for VecReg

    _ASSERTE((log2Size & ~0x3ULL) == 0);

    BOOL isLoad    = flags & 1;
    BOOL writeBack = flags & 2;
    BOOL postIndex = flags & 4;
    if (writeBack)
    {
        _ASSERTE(-256 <= offset && offset <= 255);
        Emit32((DWORD) ( (log2Size << 30) |
                         (0x7<<27) |
                         (!!isVec<<26) |
                         (!writeBack<<24) |
                         (!!isLoad<<22) |
                         ((0x1FF & offset) << 12) |
                         (!postIndex<<11) |
                         (0x1<<10) |
                         (Xn<<5) |
                         (regNum))
              );
    }
    else
    {
        int scaledOffset = 0xFFF & (offset >> log2Size);

        _ASSERTE(offset == (scaledOffset << log2Size));

        Emit32((DWORD) ( (log2Size << 30) |
                         (0x7<<27) |
                         (!!isVec<<26) |
                         (!writeBack<<24) |
                         (!!isLoad<<22) |
                         (scaledOffset << 10) |
                         (Xn<<5) |
                         (regNum))
              );
    }



}

// Load Register (Register Offset)
void StubLinkerCPU::EmitLoadRegReg(IntReg Xt, IntReg Xn, IntReg Xm, DWORD option)
{
    Emit32((DWORD) ( (0xF8600800) |
                     (option << 12) |
                     (Xm << 16) |
                     (Xn << 5) |
                     (Xt)
                ));

}

void StubLinkerCPU::EmitMovReg(IntReg Xd, IntReg Xm)
{
    if (Xd == RegSp || Xm == RegSp)
    {
        // This is a different encoding than the regular MOV (register) below.
        // Note that RegSp and RegZero share the same encoding.
        // TODO: check that the intention is not mov Xd, XZR
        //  MOV <Xd|SP>, <Xn|SP>
        // which is equivalent to
        //  ADD <Xd|SP>, <Xn|SP>, #0
        // Encoding: sf|0|0|1|0|0|0|1|shift(2)|imm(12)|Xn|Xd
        // where
        //  sf = 1 -> 64-bit variant
        //  shift and imm12 are both 0
        Emit32((DWORD) (0x91000000 | (Xm << 5) | Xd));
    }
    else
    {
        //  MOV <Xd>, <Xm>
        // which is equivalent to
        //  ORR <Xd>. XZR, <Xm>
        // Encoding: sf|0|1|0|1|0|1|0|shift(2)|0|Xm|imm(6)|Xn|Xd
        // where
        //  sf = 1 -> 64-bit variant
        //  shift and imm6 are both 0
        //  Xn = XZR
        Emit32((DWORD) ( (0xAA << 24) | (Xm << 16) | (0x1F << 5) | Xd));
    }
}

void StubLinkerCPU::EmitAddImm(IntReg Xd, IntReg Xn, unsigned int value)
{
    // add SP, SP, #imm{, <shift>}
    // Encoding: sf|0|0|1|0|0|0|1|shift(2)|imm(12)|Rn|Rd
    // where <shift> is encoded as LSL #0 (no shift) when shift=00 and LSL #12 when shift=01. (No shift in this impl)
    // imm(12) is an unsigned immediate in the range of 0 to 4095
    // Rn and Rd are both encoded as SP=31
    // sf = 1 for 64-bit variant
    _ASSERTE((0 <= value) && (value <= 4095));
    Emit32((DWORD) ((0x91 << 24) | (value << 10) | (Xn << 5) | Xd));
}

void StubLinkerCPU::EmitCallRegister(IntReg reg)
{
    // blr Xn
    // Encoding: 1|1|0|1|0|1|1|0|0|0|1|1|1|1|1|1|0|0|0|0|0|Rn|0|0|0|0|0
    Emit32((DWORD) (0xD63F0000 | (reg << 5)));
}

void StubLinkerCPU::Init()
{
    new (gConditionalBranchIF) ConditionalBranchInstructionFormat();
    new (gBranchIF) BranchInstructionFormat();
    new (gLoadFromLabelIF) LoadFromLabelInstructionFormat();
}

// Emits code to adjust arguments for static delegate target.
VOID StubLinkerCPU::EmitShuffleThunk(ShuffleEntry *pShuffleEntryArray)
{
    // On entry x0 holds the delegate instance. Look up the real target address stored in the MethodPtrAux
    // field and save it in x16(ip). Tailcall to the target method after re-arranging the arguments
    // ldr x16, [x0, #offsetof(DelegateObject, _methodPtrAux)]
    EmitLoadStoreRegImm(eLOAD, IntReg(16), IntReg(0), DelegateObject::GetOffsetOfMethodPtrAux());
    //add x11, x0, DelegateObject::GetOffsetOfMethodPtrAux() - load the indirection cell into x11 used by ResolveWorkerAsmStub
    EmitAddImm(IntReg(11), IntReg(0), DelegateObject::GetOffsetOfMethodPtrAux());

    for (ShuffleEntry* pEntry = pShuffleEntryArray; pEntry->srcofs != ShuffleEntry::SENTINEL; pEntry++)
    {
        if (pEntry->srcofs & ShuffleEntry::REGMASK)
        {
            // If source is present in register then destination must also be a register
            _ASSERTE(pEntry->dstofs & ShuffleEntry::REGMASK);
            _ASSERTE(!(pEntry->dstofs & ShuffleEntry::FPREGMASK));
            _ASSERTE(!(pEntry->srcofs & ShuffleEntry::FPREGMASK));

            EmitMovReg(IntReg(pEntry->dstofs & ShuffleEntry::OFSREGMASK), IntReg(pEntry->srcofs & ShuffleEntry::OFSREGMASK));
        }
        else if (pEntry->dstofs & ShuffleEntry::REGMASK)
        {
            // source must be on the stack
            _ASSERTE(!(pEntry->srcofs & ShuffleEntry::REGMASK));
            _ASSERTE(!(pEntry->dstofs & ShuffleEntry::FPREGMASK));


#if !defined(TARGET_APPLE)
            EmitLoadStoreRegImm(eLOAD, IntReg(pEntry->dstofs & ShuffleEntry::OFSREGMASK), RegSp, pEntry->srcofs * sizeof(void*));
#else
            int log2Size = (pEntry->srcofs >> 12);
            int srcOffset = int(pEntry->srcofs & 0xfff) << log2Size;
            EmitLoadStoreRegImm(eLOAD, IntReg(pEntry->dstofs & ShuffleEntry::OFSREGMASK), RegSp, srcOffset, log2Size);
#endif
        }
        else
        {
            // source must be on the stack
            _ASSERTE(!(pEntry->srcofs & ShuffleEntry::REGMASK));

            // dest must be on the stack
            _ASSERTE(!(pEntry->dstofs & ShuffleEntry::REGMASK));

#if !defined(TARGET_APPLE)
            EmitLoadStoreRegImm(eLOAD, IntReg(9), RegSp, pEntry->srcofs * sizeof(void*));
            EmitLoadStoreRegImm(eSTORE, IntReg(9), RegSp, pEntry->dstofs * sizeof(void*));
#else
            // Decode ShuffleIterator::GetNextOfs() encoded entries
            // See comments in that function

            // We expect src and dst stack size to be the same.
            _ASSERTE((pEntry->srcofs >> 12) == (pEntry->dstofs >> 12));
            int log2Size = (pEntry->srcofs >> 12);
            int srcOffset = int(pEntry->srcofs & 0xfff) << log2Size;
            int dstOffset = int(pEntry->dstofs & 0xfff) << log2Size;

            EmitLoadStoreRegImm(eLOAD, IntReg(9), RegSp, srcOffset, log2Size);
            EmitLoadStoreRegImm(eSTORE, IntReg(9), RegSp, dstOffset, log2Size);
#endif
        }
    }

    // Tailcall to target
    // br x16
    EmitJumpRegister(IntReg(16));
}

// Emits code to adjust arguments for static delegate target.
VOID StubLinkerCPU::EmitComputedInstantiatingMethodStub(MethodDesc* pSharedMD, struct ShuffleEntry *pShuffleEntryArray, void* extraArg)
{
    STANDARD_VM_CONTRACT;

    for (ShuffleEntry* pEntry = pShuffleEntryArray; pEntry->srcofs != ShuffleEntry::SENTINEL; pEntry++)
    {
        _ASSERTE(pEntry->dstofs & ShuffleEntry::REGMASK);
        _ASSERTE(pEntry->srcofs & ShuffleEntry::REGMASK);
        _ASSERTE(!(pEntry->dstofs & ShuffleEntry::FPREGMASK));
        _ASSERTE(!(pEntry->srcofs & ShuffleEntry::FPREGMASK));
        _ASSERTE(pEntry->dstofs != ShuffleEntry::HELPERREG);
        _ASSERTE(pEntry->srcofs != ShuffleEntry::HELPERREG);

        EmitMovReg(IntReg(pEntry->dstofs & ShuffleEntry::OFSREGMASK), IntReg(pEntry->srcofs & ShuffleEntry::OFSREGMASK));
    }

    MetaSig msig(pSharedMD);
    ArgIterator argit(&msig);

    if (argit.HasParamType())
    {
        ArgLocDesc sInstArgLoc;
        argit.GetParamTypeLoc(&sInstArgLoc);
        int regHidden = sInstArgLoc.m_idxGenReg;
        _ASSERTE(regHidden != -1);

        if (extraArg == NULL)
        {
            if (pSharedMD->RequiresInstMethodTableArg())
            {
                // Unboxing stub case
                // Fill param arg with methodtable of this pointer
                // ldr regHidden, [x0, #0]
                EmitLoadStoreRegImm(eLOAD, IntReg(regHidden), IntReg(0), 0);
            }
        }
        else
        {
            EmitMovConstant(IntReg(regHidden), (UINT64)extraArg);
        }
    }

    if (extraArg == NULL)
    {
        // Unboxing stub case
        // Address of the value type is address of the boxed instance plus sizeof(MethodDesc*).
        //  add x0, #sizeof(MethodDesc*)
        EmitAddImm(IntReg(0), IntReg(0), sizeof(MethodDesc*));
    }

    // Tail call the real target.
    EmitCallManagedMethod(pSharedMD, TRUE /* tail call */);
    SetTargetMethod(pSharedMD);
}

void StubLinkerCPU::EmitCallLabel(CodeLabel *target, BOOL fTailCall, BOOL fIndirect)
{
    STANDARD_VM_CONTRACT;

    BranchInstructionFormat::VariationCodes variationCode = BranchInstructionFormat::VariationCodes::BIF_VAR_JUMP;
    if (!fTailCall)
        variationCode = static_cast<BranchInstructionFormat::VariationCodes>(variationCode | BranchInstructionFormat::VariationCodes::BIF_VAR_CALL);
    if (fIndirect)
        variationCode = static_cast<BranchInstructionFormat::VariationCodes>(variationCode | BranchInstructionFormat::VariationCodes::BIF_VAR_INDIRECT);

    EmitLabelRef(target, reinterpret_cast<BranchInstructionFormat&>(gBranchIF), (UINT)variationCode);

}

void StubLinkerCPU::EmitCallManagedMethod(MethodDesc *pMD, BOOL fTailCall)
{
    STANDARD_VM_CONTRACT;

    PCODE multiCallableAddr = pMD->TryGetMultiCallableAddrOfCode(CORINFO_ACCESS_PREFER_SLOT_OVER_TEMPORARY_ENTRYPOINT);

    // Use direct call if possible.
    if (multiCallableAddr != (PCODE)NULL)
    {
        EmitCallLabel(NewExternalCodeLabel((LPVOID)multiCallableAddr), fTailCall, FALSE);
    }
    else
    {
        EmitCallLabel(NewExternalCodeLabel((LPVOID)pMD->GetAddrOfSlot()), fTailCall, TRUE);
    }
}


#ifdef FEATURE_READYTORUN

//
// Allocation of dynamic helpers
//
#ifndef FEATURE_STUBPRECODE_DYNAMIC_HELPERS

#define DYNAMIC_HELPER_ALIGNMENT sizeof(TADDR)

#define BEGIN_DYNAMIC_HELPER_EMIT_WORKER(size) \
    SIZE_T cb = size; \
    SIZE_T cbAligned = ALIGN_UP(cb, DYNAMIC_HELPER_ALIGNMENT); \
    BYTE * pStartRX = (BYTE *)(void*)pAllocator->GetDynamicHelpersHeap()->AllocAlignedMem(cbAligned, DYNAMIC_HELPER_ALIGNMENT); \
    ExecutableWriterHolder<BYTE> startWriterHolder(pStartRX, cbAligned); \
    BYTE * pStart = startWriterHolder.GetRW(); \
    size_t rxOffset = pStartRX - pStart; \
    BYTE * p = pStart;

#ifdef FEATURE_PERFMAP
#define BEGIN_DYNAMIC_HELPER_EMIT(size) \
    BEGIN_DYNAMIC_HELPER_EMIT_WORKER(size) \
    PerfMap::LogStubs(__FUNCTION__, "DynamicHelper", (PCODE)p, size, PerfMapStubType::Individual);
#else
#define BEGIN_DYNAMIC_HELPER_EMIT(size) BEGIN_DYNAMIC_HELPER_EMIT_WORKER(size)
#endif


#define END_DYNAMIC_HELPER_EMIT() \
    _ASSERTE(pStart + cb == p); \
    while (p < pStart + cbAligned) { *(DWORD*)p = 0xBADC0DF0; p += 4; }\
    ClrFlushInstructionCache(pStartRX, cbAligned); \
    return (PCODE)pStartRX

// Uses x8 as scratch register to store address of data label
// After load x8 is increment to point to next data
// only accepts positive offsets
static void LoadRegPair(BYTE* p, int reg1, int reg2, UINT32 offset)
{
    LIMITED_METHOD_CONTRACT;

    // adr x8, <label>
    *(DWORD*)(p + 0) = 0x10000008 | ((offset >> 2) << 5);
    // ldp reg1, reg2, [x8], #16 ; postindex & wback
    *(DWORD*)(p + 4) = 0xa8c10100 | (reg2 << 10) | reg1;
}

PCODE DynamicHelpers::CreateHelper(LoaderAllocator * pAllocator, TADDR arg, PCODE target)
{
    STANDARD_VM_CONTRACT;

    BEGIN_DYNAMIC_HELPER_EMIT(32);

    // adr x8, <label>
    // ldp x0, x12, [x8]
    LoadRegPair(p, 0, 12, 16);
    p += 8;
    // br x12
    *(DWORD*)p = 0xd61f0180;
    p += 4;

    // padding to make 8 byte aligned
    *(DWORD*)p = 0xBADC0DF0; p += 4;

    // label:
    // arg
    *(TADDR*)p = arg;
    p += 8;
    // target
    *(PCODE*)p = target;
    p += 8;

    END_DYNAMIC_HELPER_EMIT();
}

// Caller must ensure sufficient byte are allocated including padding (if applicable)
void DynamicHelpers::EmitHelperWithArg(BYTE*& p, size_t rxOffset, LoaderAllocator * pAllocator, TADDR arg, PCODE target)
{
    STANDARD_VM_CONTRACT;

    // if p is already aligned at 8-byte then padding is required for data alignment
    bool padding = (((uintptr_t)p & 0x7) == 0);

    // adr x8, <label>
    // ldp x1, x12, [x8]
    LoadRegPair(p, 1, 12, padding?16:12);
    p += 8;

    // br x12
    *(DWORD*)p = 0xd61f0180;
    p += 4;

    if(padding)
    {
        // padding to make 8 byte aligned
        *(DWORD*)p = 0xBADC0DF0;
        p += 4;
    }

    // label:
    // arg
    *(TADDR*)p = arg;
    p += 8;
    // target
    *(PCODE*)p = target;
    p += 8;
}

PCODE DynamicHelpers::CreateHelperWithArg(LoaderAllocator * pAllocator, TADDR arg, PCODE target)
{
    STANDARD_VM_CONTRACT;

    BEGIN_DYNAMIC_HELPER_EMIT(32);

    EmitHelperWithArg(p, rxOffset, pAllocator, arg, target);

    END_DYNAMIC_HELPER_EMIT();
}

PCODE DynamicHelpers::CreateHelper(LoaderAllocator * pAllocator, TADDR arg, TADDR arg2, PCODE target)
{
    STANDARD_VM_CONTRACT;

    BEGIN_DYNAMIC_HELPER_EMIT(40);

    // adr x8, <label>
    // ldp x0, x1, [x8] ; wback
    LoadRegPair(p, 0, 1, 16);
    p += 8;

    // ldr x12, [x8]
    *(DWORD*)p = 0xf940010c;
    p += 4;
    // br x12
    *(DWORD*)p = 0xd61f0180;
    p += 4;
    // label:
    // arg
    *(TADDR*)p = arg;
    p += 8;
    // arg2
    *(TADDR*)p = arg2;
    p += 8;
    // target
    *(TADDR*)p = target;
    p += 8;

    END_DYNAMIC_HELPER_EMIT();
}

PCODE DynamicHelpers::CreateHelperArgMove(LoaderAllocator * pAllocator, TADDR arg, PCODE target)
{
    STANDARD_VM_CONTRACT;

    BEGIN_DYNAMIC_HELPER_EMIT(32);

    // mov x1, x0
    *(DWORD*)p = 0x91000001;
    p += 4;

    // adr x8, <label>
    // ldp x0, x12, [x8]
    LoadRegPair(p, 0, 12, 12);
    p += 8;

    // br x12
    *(DWORD*)p = 0xd61f0180;
    p += 4;

    // label:
    // arg
    *(TADDR*)p = arg;
    p += 8;
    // target
    *(TADDR*)p = target;
    p += 8;

    END_DYNAMIC_HELPER_EMIT();
}

PCODE DynamicHelpers::CreateReturn(LoaderAllocator * pAllocator)
{
    STANDARD_VM_CONTRACT;

    BEGIN_DYNAMIC_HELPER_EMIT(4);

    // br lr
    *(DWORD*)p = 0xd61f03c0;
    p += 4;
    END_DYNAMIC_HELPER_EMIT();
}

PCODE DynamicHelpers::CreateReturnConst(LoaderAllocator * pAllocator, TADDR arg)
{
    STANDARD_VM_CONTRACT;

    BEGIN_DYNAMIC_HELPER_EMIT(16);

    // ldr x0, <label>
    *(DWORD*)p = 0x58000040;
    p += 4;

    // br lr
    *(DWORD*)p = 0xd61f03c0;
    p += 4;

    // label:
    // arg
    *(TADDR*)p = arg;
    p += 8;

    END_DYNAMIC_HELPER_EMIT();
}

PCODE DynamicHelpers::CreateReturnIndirConst(LoaderAllocator * pAllocator, TADDR arg, INT8 offset)
{
    STANDARD_VM_CONTRACT;

    BEGIN_DYNAMIC_HELPER_EMIT(24);

    // ldr x0, <label>
    *(DWORD*)p = 0x58000080;
    p += 4;

    // ldr x0, [x0]
    *(DWORD*)p = 0xf9400000;
    p += 4;

    // add x0, x0, offset
    *(DWORD*)p = 0x91000000 | (offset << 10);
    p += 4;

    // br lr
    *(DWORD*)p = 0xd61f03c0;
    p += 4;

    // label:
    // arg
    *(TADDR*)p = arg;
    p += 8;

    END_DYNAMIC_HELPER_EMIT();
}

PCODE DynamicHelpers::CreateHelperWithTwoArgs(LoaderAllocator * pAllocator, TADDR arg, PCODE target)
{
    STANDARD_VM_CONTRACT;

    BEGIN_DYNAMIC_HELPER_EMIT(32);

    // adr x8, <label>
    // ldp x2, x12, [x8]
    LoadRegPair(p, 2, 12, 16);
    p += 8;

    // br x12
    *(DWORD*)p = 0xd61f0180;
    p += 4;

    // padding to make 8 byte aligned
    *(DWORD*)p = 0xBADC0DF0; p += 4;

    // label:
    // arg
    *(TADDR*)p = arg;
    p += 8;

    // target
    *(TADDR*)p = target;
    p += 8;
    END_DYNAMIC_HELPER_EMIT();
}

PCODE DynamicHelpers::CreateHelperWithTwoArgs(LoaderAllocator * pAllocator, TADDR arg, TADDR arg2, PCODE target)
{
    STANDARD_VM_CONTRACT;

    BEGIN_DYNAMIC_HELPER_EMIT(40);

    // adr x8, <label>
    // ldp x2, x3, [x8]; wback
    LoadRegPair(p, 2, 3, 16);
    p += 8;

    // ldr x12, [x8]
    *(DWORD*)p = 0xf940010c;
    p += 4;

    // br x12
    *(DWORD*)p = 0xd61f0180;
    p += 4;

    // label:
    // arg
    *(TADDR*)p = arg;
    p += 8;
    // arg2
    *(TADDR*)p = arg2;
    p += 8;
    // target
    *(TADDR*)p = target;
    p += 8;
    END_DYNAMIC_HELPER_EMIT();
}

PCODE DynamicHelpers::CreateDictionaryLookupHelper(LoaderAllocator * pAllocator, CORINFO_RUNTIME_LOOKUP * pLookup, DWORD dictionaryIndexAndSlot, Module * pModule)
{
    STANDARD_VM_CONTRACT;

    PCODE helperAddress = GetDictionaryLookupHelper(pLookup->helper);

    GenericHandleArgs * pArgs = (GenericHandleArgs *)(void *)pAllocator->GetDynamicHelpersHeap()->AllocAlignedMem(sizeof(GenericHandleArgs), DYNAMIC_HELPER_ALIGNMENT);
    ExecutableWriterHolder<GenericHandleArgs> argsWriterHolder(pArgs, sizeof(GenericHandleArgs));
    argsWriterHolder.GetRW()->dictionaryIndexAndSlot = dictionaryIndexAndSlot;
    argsWriterHolder.GetRW()->signature = pLookup->signature;
    argsWriterHolder.GetRW()->module = (CORINFO_MODULE_HANDLE)pModule;

    WORD slotOffset = (WORD)(dictionaryIndexAndSlot & 0xFFFF) * sizeof(Dictionary*);

    // It's available only via the run-time helper function
    if (pLookup->indirections == CORINFO_USEHELPER)
    {
        BEGIN_DYNAMIC_HELPER_EMIT(32);

        // X0 already contains generic context parameter
        // reuse EmitHelperWithArg for below two operations
        // X1 <- pArgs
        // branch to helperAddress
        EmitHelperWithArg(p, rxOffset, pAllocator, (TADDR)pArgs, helperAddress);

        END_DYNAMIC_HELPER_EMIT();
    }
    else
    {
        int indirectionsCodeSize = 0;
        int indirectionsDataSize = 0;
        if (pLookup->sizeOffset != CORINFO_NO_SIZE_CHECK)
        {
            indirectionsCodeSize += (pLookup->sizeOffset > 32760 ? 8 : 4);
            indirectionsDataSize += (pLookup->sizeOffset > 32760 ? 4 : 0);
            indirectionsCodeSize += 12;
        }

        for (WORD i = 0; i < pLookup->indirections; i++)
        {
            indirectionsCodeSize += (pLookup->offsets[i] > 32760 ? 8 : 4); // if( > 32760) (8 code bytes) else 4 bytes for instruction with offset encoded in instruction
            indirectionsDataSize += (pLookup->offsets[i] > 32760 ? 4 : 0); // 4 bytes for storing indirection offset values
        }

        int codeSize = indirectionsCodeSize;
        if(pLookup->testForNull)
        {
            codeSize += 16; // mov-cbz-ret-mov
            //padding for 8-byte align (required by EmitHelperWithArg)
            if((codeSize & 0x7) == 0)
                codeSize += 4;
            codeSize += 28; // size of EmitHelperWithArg
        }
        else
        {
            codeSize += 4 ; /* ret */
        }

        codeSize += indirectionsDataSize;

        BEGIN_DYNAMIC_HELPER_EMIT(codeSize);

        if (pLookup->testForNull || pLookup->sizeOffset != CORINFO_NO_SIZE_CHECK)
        {
            // mov x9, x0
            *(DWORD*)p = 0x91000009;
            p += 4;
        }

        BYTE* pBLECall = NULL;

        // moving offset value wrt PC. Currently points to first indirection offset data.
        uint dataOffset = codeSize - indirectionsDataSize - (pLookup->testForNull ? 4 : 0);
        for (WORD i = 0; i < pLookup->indirections; i++)
        {
            if (i == pLookup->indirections - 1 && pLookup->sizeOffset != CORINFO_NO_SIZE_CHECK)
            {
                _ASSERTE(pLookup->testForNull && i > 0);

                if (pLookup->sizeOffset > 32760)
                {
                    // ldr w10, [PC, #dataOffset]
                    *(DWORD*)p = 0x1800000a | ((dataOffset >> 2) << 5); p += 4;
                    // ldr x11, [x0, x10]
                    *(DWORD*)p = 0xf86a680b; p += 4;

                    // move to next indirection offset data
                    dataOffset = dataOffset - 8 + 4; // subtract 8 as we have moved PC by 8 and add 4 as next data is at 4 bytes from previous data
                }
                else
                {
                    // ldr x11, [x0, #(pLookup->sizeOffset)]
                    *(DWORD*)p = 0xf940000b | (((UINT32)pLookup->sizeOffset >> 3) << 10); p += 4;
                    dataOffset -= 4; // subtract 4 as we have moved PC by 4
                }

                // mov x10,slotOffset
                *(DWORD*)p = 0xd280000a | ((UINT32)slotOffset << 5); p += 4;
                dataOffset -= 4;

                // cmp x11,x10
                *(DWORD*)p = 0xeb0a017f; p += 4;
                dataOffset -= 4;

                // ble 'CALL HELPER'
                pBLECall = p;       // Offset filled later
                *(DWORD*)p = 0x5400000d; p += 4;
                dataOffset -= 4;
            }
            if(pLookup->offsets[i] > 32760)
            {
                // ldr w10, [PC, #dataOffset]
                *(DWORD*)p = 0x1800000a | ((dataOffset>>2)<<5);
                p += 4;
                // ldr x0, [x0, x10]
                *(DWORD*)p = 0xf86a6800;
                p += 4;

                // move to next indirection offset data
                dataOffset = dataOffset - 8 + 4; // subtract 8 as we have moved PC by 8 and add 4 as next data is at 4 bytes from previous data
            }
            else
            {
                // offset must be 8 byte aligned
                _ASSERTE((pLookup->offsets[i] & 0x7) == 0);

                // ldr x0, [x0, #(pLookup->offsets[i])]
                *(DWORD*)p = 0xf9400000 | ( ((UINT32)pLookup->offsets[i]>>3) <<10 );
                p += 4;
                dataOffset -= 4; // subtract 4 as we have moved PC by 4
            }
        }

        // No null test required
        if (!pLookup->testForNull)
        {
            _ASSERTE(pLookup->sizeOffset == CORINFO_NO_SIZE_CHECK);

            // ret lr
            *(DWORD*)p = 0xd65f03c0;
            p += 4;
        }
        else
        {
            // cbz x0, 'CALL HELPER'
            *(DWORD*)p = 0xb4000040;
            p += 4;
            // ret lr
            *(DWORD*)p = 0xd65f03c0;
            p += 4;

            // CALL HELPER:
            if(pBLECall != NULL)
                *(DWORD*)pBLECall |= (((UINT32)(p - pBLECall) >> 2) << 5);

            // mov x0, x9
            *(DWORD*)p = 0x91000120;
            p += 4;
            // reuse EmitHelperWithArg for below two operations
            // X1 <- pArgs
            // branch to helperAddress
            EmitHelperWithArg(p, rxOffset, pAllocator, (TADDR)pArgs, helperAddress);
        }

        // datalabel:
        for (WORD i = 0; i < pLookup->indirections; i++)
        {
            if (i == pLookup->indirections - 1 && pLookup->sizeOffset != CORINFO_NO_SIZE_CHECK && pLookup->sizeOffset > 32760)
            {
                *(UINT32*)p = (UINT32)pLookup->sizeOffset;
                p += 4;
            }
            if (pLookup->offsets[i] > 32760)
            {
                *(UINT32*)p = (UINT32)pLookup->offsets[i];
                p += 4;
            }
        }

        END_DYNAMIC_HELPER_EMIT();
    }
}
#endif // FEATURE_STUBPRECODE_DYNAMIC_HELPERS
#endif // FEATURE_READYTORUN


#endif // #ifndef DACCESS_COMPILE
