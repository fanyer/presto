/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#ifdef ES_NATIVE_SUPPORT
#ifdef ARCHITECTURE_ARM

#include "modules/ecmascript/carakan/src/util/es_codegenerator_arm.h"
#include "modules/ecmascript/carakan/src/util/es_codegenerator_base_functions.h"

#ifdef NATIVE_DISASSEMBLER_SUPPORT
#include "modules/ecmascript/carakan/src/util/es_native_disass.h"
#endif // NATIVE_DISASSEMBLER_SUPPORT

static unsigned
DataProcessingInstruction(unsigned opcode, ES_CodeGenerator::Register op1, const ES_CodeGenerator::Operand &op2, ES_CodeGenerator::Register dst, ES_CodeGenerator::SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    ES_NativeInstruction codeword = dst << 12 | op1 << 16 | (set_condition_codes == ES_CodeGenerator::SET_CONDITION_CODES ? 1u : 0u) << 20 | opcode << 21 | condition << 28;

    if (op2.is_register)
    {
        codeword |= op2.u.reg.base | op2.u.reg.shift_type << 5;

        if (op2.u.reg.is_register_shift)
            codeword |= 1 << 4 | op2.u.reg.u.shift_register << 8;
        else
            codeword |= (op2.u.reg.u.shift_amount & 0x1f) << 7;
    }
    else
        codeword |= (op2.u.imm.reduced_immediate & 0xfff) | 1 << 25;

    return codeword;
}

ES_CodeGenerator::ES_CodeGenerator(OpMemGroup *arena)
    : ES_CodeGenerator_Base(arena)
#ifdef NATIVE_DISASSEMBLER_SUPPORT
    , disassemble(FALSE)
    , dump_cv(FALSE)
#endif // NATIVE_DISASSEMBLER_SUPPORT
    , maximum_constant_range(4096)
{
}

ES_CodeGenerator::Operand::Operand(Register base)
    : is_register(TRUE)
{
    u.reg.base = base;
    u.reg.shift_type = ES_CodeGenerator::SHIFT_LOGICAL_LEFT;
    u.reg.is_register_shift = FALSE;
    u.reg.u.shift_amount = 0;
}

ES_CodeGenerator::Operand::Operand(Register base, Register shift_register, ShiftType shift_type)
    : is_register(TRUE)
{
    u.reg.base = base;
    u.reg.shift_type = shift_type;
    u.reg.is_register_shift = TRUE;
    u.reg.u.shift_register = shift_register;
}

ES_CodeGenerator::Operand::Operand(Register base, unsigned shift_amount, ShiftType shift_type)
    : is_register(TRUE)
{
    u.reg.base = base;
    u.reg.shift_type = shift_type;
    u.reg.is_register_shift = FALSE;
    u.reg.u.shift_amount = shift_amount;
}

ES_CodeGenerator::Operand::Operand(int immediate)
    : is_register(FALSE)
{
#ifdef DEBUG_ENABLE_OPASSERT
    BOOL is_possible = EncodeImmediate(immediate, &u.imm.reduced_immediate);
    OP_ASSERT(is_possible);
#else // DEBUG_ENABLE_OPASSERT
    EncodeImmediate(immediate, &u.imm.reduced_immediate);
#endif // DEBUG_ENABLE_OPASSERT
}

/* static */ BOOL
ES_CodeGenerator::Operand::EncodeImmediate(int immediate, unsigned *reduced_immediate, BOOL *negated, BOOL *inverted)
{
    unsigned uimmediate = immediate;
    unsigned rotate_amount = 0;

    while (rotate_amount < 16 && (uimmediate & ~0xffu) != 0)
    {
        ++rotate_amount;
        uimmediate = uimmediate << 2 | (uimmediate & 0xc0000000u) >> 30;
    }

    if (rotate_amount != 16)
    {
        if (reduced_immediate)
        {
            *reduced_immediate = uimmediate | rotate_amount << 8;

            if (negated) *negated = FALSE;
            if (inverted) *inverted = FALSE;
        }

        return TRUE;
    }

    if (negated)
    {
        unsigned nimmediate = -immediate;
        rotate_amount = 0;

        while (rotate_amount < 16 && (nimmediate & ~0xffu) != 0)
        {
            ++rotate_amount;
            nimmediate = nimmediate << 2 | (nimmediate & 0xc0000000u) >> 30;
        }

        if (rotate_amount != 16)
        {
            if (reduced_immediate)
            {
                *reduced_immediate = nimmediate | rotate_amount << 8;
                *negated = TRUE;
            }

            return TRUE;
        }
    }

    if (inverted)
    {
        unsigned nimmediate = ~static_cast<unsigned>(immediate);
        rotate_amount = 0;

        while (rotate_amount < 16 && (nimmediate & ~0xffu) != 0)
        {
            ++rotate_amount;
            nimmediate = nimmediate << 2 | (nimmediate & 0xc0000000u) >> 30;
        }

        if (rotate_amount != 16)
        {
            if (reduced_immediate)
            {
                *reduced_immediate = nimmediate | rotate_amount << 8;
                *inverted = TRUE;
            }

            return TRUE;
        }
    }

    return FALSE;
}

ES_CodeGenerator::NegOperand::NegOperand(int immediate)
    : negated(FALSE)
{
#ifdef DEBUG_ENABLE_OPASSERT
    BOOL is_possible = Operand::EncodeImmediate(immediate, &u.imm.reduced_immediate, &negated, NULL);
    OP_ASSERT(is_possible);
#else // DEBUG_ENABLE_OPASSERT
    Operand::EncodeImmediate(immediate, &u.imm.reduced_immediate, &negated, NULL);
#endif // DEBUG_ENABLE_OPASSERT
}

/* static */ BOOL
ES_CodeGenerator::NegOperand::EncodeImmediate(int immediate)
{
    BOOL negated;
    return Operand::EncodeImmediate(immediate, NULL, &negated, NULL);
}

ES_CodeGenerator::NotOperand::NotOperand(int immediate)
    : inverted(FALSE)
{
#ifdef DEBUG_ENABLE_OPASSERT
    BOOL is_possible = Operand::EncodeImmediate(immediate, &u.imm.reduced_immediate, NULL, &inverted);
    OP_ASSERT(is_possible);
#else // DEBUG_ENABLE_OPASSERT
    Operand::EncodeImmediate(immediate, &u.imm.reduced_immediate, NULL, &inverted);
#endif // DEBUG_ENABLE_OPASSERT
}

/* static */ BOOL
ES_CodeGenerator::NotOperand::EncodeImmediate(int immediate)
{
    BOOL inverted;
    return Operand::EncodeImmediate(immediate, NULL, NULL, &inverted);
}

#ifdef DEBUG_ENABLE_OPASSERT
void
ES_CodeGenerator::BKPT(unsigned short imm)
{
    Reserve();

    ES_NativeInstruction codeword = 0x01200070 | (imm & 0xfff0) << 4 | (imm & 0xf) | ES_NATIVE_CONDITION_ALWAYS << 28;

    *buffer++ = codeword;
}
#endif

void
ES_CodeGenerator::BX(Register src, ES_NativeJumpCondition condition)
{
    Reserve();

    ES_NativeInstruction codeword = src | 0x012fff10 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::BLX(Register src, ES_NativeJumpCondition condition)
{
    Reserve();

    ES_NativeInstruction codeword = src | 0x012fff30 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::SMULL(Register op1, Register op2, Register dsthigh, Register dstlow, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    Reserve();

    ES_NativeInstruction codeword = 0x00c00090 | op1 | op2 << 8 | dstlow << 12 | dsthigh << 16 | (set_condition_codes == SET_CONDITION_CODES ? 1 : 0) << 20 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::UMULL(Register op1, Register op2, Register dsthigh, Register dstlow, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    Reserve();

    ES_NativeInstruction codeword = 0x00800090 | op1 | op2 << 8 | dstlow << 12 | dsthigh << 16 | (set_condition_codes == SET_CONDITION_CODES ? 1 : 0) << 20 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::LDR(Constant *constant, Register dst, ES_NativeJumpCondition condition)
{
    Reserve();

    constant->AddUse(current_block, GetBufferUsed() - current_block->start, arena);

    LDR(REG_PC, 0, dst, condition);
}

void
ES_CodeGenerator::LDRH(Register base, int offset, Register dst, ES_NativeJumpCondition condition)
{
    Reserve();

    BOOL negative;

    if (offset < 0)
    {
        negative = TRUE;
        offset = -offset;
    }
    else
        negative = FALSE;

    OP_ASSERT(offset < 256);

    ES_NativeInstruction codeword = 0x015000b0 | (!negative ? 1 : 0) << 23 | base << 16 | dst << 12 | (offset & 0xf0) << 4 | (offset & 0x0f) | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::LDRH(Register base, Register offset, BOOL up, Register dst, ES_NativeJumpCondition condition)
{
    Reserve();

    ES_NativeInstruction codeword = 0x011000b0 | (up ? 1 : 0) << 23 | base << 16 | dst << 12 | offset | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::LDRSB(Register base, int offset, Register dst, ES_NativeJumpCondition condition)
{
    Reserve();

    BOOL negative;

    if (offset < 0)
    {
        negative = TRUE;
        offset = -offset;
    }
    else
        negative = FALSE;

    OP_ASSERT(offset < 256);

    ES_NativeInstruction codeword = 0x015000d0 | (!negative ? 1 : 0) << 23 | base << 16 | dst << 12 | (offset & 0xf0) << 4 | (offset & 0x0f) | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::LDRSB(Register base, Register offset, BOOL up, Register dst, ES_NativeJumpCondition condition)
{
    Reserve();

    ES_NativeInstruction codeword = 0x011000d0 | (up ? 1 : 0) << 23 | base << 16 | dst << 12 | offset | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::LDRSH(Register base, int offset, Register dst, ES_NativeJumpCondition condition)
{
    Reserve();

    BOOL negative;

    if (offset < 0)
    {
        negative = TRUE;
        offset = -offset;
    }
    else
        negative = FALSE;

    OP_ASSERT(offset < 256);

    ES_NativeInstruction codeword = 0x015000f0 | (!negative ? 1 : 0) << 23 | base << 16 | dst << 12 | (offset & 0xf0) << 4 | (offset & 0x0f) | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::LDRSH(Register base, Register offset, BOOL up, Register dst, ES_NativeJumpCondition condition)
{
    Reserve();

    ES_NativeInstruction codeword = 0x011000f0 | (up ? 1 : 0) << 23 | base << 16 | dst << 12 | offset | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::LDRD(Register base, int offset, Register dst, ES_NativeJumpCondition condition)
{
    Reserve();

    BOOL negative;

    if (offset < 0)
    {
        negative = TRUE;
        offset = -offset;
    }
    else
        negative = FALSE;

    OP_ASSERT(offset < 256);
    OP_ASSERT((dst & 1) == 0);

    ES_NativeInstruction codeword = 0x014000d0 | (!negative ? 1 : 0) << 23 | base << 16 | dst << 12 | (offset & 0xf0) << 4 | (offset & 0x0f) | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::STRD(Register src, Register base, int offset, ES_NativeJumpCondition condition)
{
    Reserve();

    BOOL negative;

    if (offset < 0)
    {
        negative = TRUE;
        offset = -offset;
    }
    else
        negative = FALSE;

    OP_ASSERT(offset < 256);
    OP_ASSERT((src & 1) == 0);

    ES_NativeInstruction codeword = 0x014000f0 | (!negative ? 1 : 0) << 23 | base << 16 | src << 12 | (offset & 0xf0) << 4 | (offset & 0x0f) | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::MRS(Register dst, ES_NativeJumpCondition condition)
{
    Reserve();

    ES_NativeInstruction codeword = 0x010f0000 | dst << 12 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::MSR(Register src, BOOL control, BOOL extensions, BOOL status, BOOL flags, ES_NativeJumpCondition condition)
{
    Reserve();

    ES_NativeInstruction codeword = 0x0120f000 | src | condition << 28;

    if (control)
        codeword |= 1 << 16;
    if (extensions)
        codeword |= 1 << 17;
    if (status)
        codeword |= 1 << 18;
    if (flags)
        codeword |= 1 << 19;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::MCRR(unsigned coproc, unsigned opcode, Register Rd, Register Rn, unsigned CRm, ES_NativeJumpCondition condition)
{
    Reserve();

    ES_NativeInstruction codeword = 0x0c400000 | Rn << 16 | Rd << 12 | coproc << 8 | opcode << 4 | CRm | condition << 28;

    *buffer++ = codeword;
}

#ifdef ARCHITECTURE_ARM_VFP

#ifdef _DEBUG
# define CHECK_VFP_SUPPORT() \
    if (!SupportsVFP()) \
        OP_ASSERT(!"Invalid FPU operation emitted!");
#else // _DEBUG
# define CHECK_VFP_SUPPORT()
#endif // _DEBUG

/* static */ BOOL
ES_CodeGenerator::SupportsVFP()
{
    return (g_op_system_info->GetCPUFeatures() & (OpSystemInfo::CPU_FEATURES_ARM_VFPV2 | OpSystemInfo::CPU_FEATURES_ARM_VFPV3)) != 0;
}

static unsigned
FLD_FST_ProcessOffset(int offset)
{
    /* Offset must be multiple of 4. */
    OP_ASSERT(offset % 4 == 0);

    offset /= 4;

    /* Offset/4 must fit in 8 bits + sign bit. */
    OP_ASSERT(offset > -256 && offset < 256);

    if (offset < 0)
        return -offset;
    else
        return offset | 1u << 23;
}

void
ES_CodeGenerator::FLDS(Register base, int offset, VFPSingleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    unsigned processed_offset = FLD_FST_ProcessOffset(offset);

    ES_NativeInstruction codeword = 0x0d100a00 | processed_offset | (dst >> 1) << 12 | base << 16 | (dst & 1u) << 22 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FSTS(VFPSingleRegister src, Register base, int offset, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    unsigned processed_offset = FLD_FST_ProcessOffset(offset);

    ES_NativeInstruction codeword = 0x0d000a00 | processed_offset | (src >> 1) << 12 | base << 16 | (src & 1u) << 22 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FLDD(Register base, int offset, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    unsigned processed_offset = FLD_FST_ProcessOffset(offset);

    ES_NativeInstruction codeword = 0x0d100b00 | processed_offset | dst << 12 | base << 16 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FLDD(Constant *constant, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    constant->AddUse(current_block, 0x80000000 | (GetBufferUsed() - current_block->start), arena);
    maximum_constant_range = 1024;

    FLDD(REG_PC, 0, dst, condition);
}

void
ES_CodeGenerator::FSTD(VFPDoubleRegister dst, Register base, int offset, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    unsigned processed_offset = FLD_FST_ProcessOffset(offset);

    ES_NativeInstruction codeword = 0x0d000b00 | processed_offset | dst << 12 | base << 16 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FMSR(Register src, VFPSingleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0e000a10 | (dst >> 1) << 16 | src << 12 | (dst & 1u) << 3 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FMRS(VFPSingleRegister src, Register dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0e100a10 | (src >> 1) << 16 | dst << 12 | (src & 1u) << 3 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FMRX(VFPSystemRegister src, Register dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0ef00a10 | src << 16 | dst << 12 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FMXR(Register src, VFPSystemRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0ee00a10 | src << 16 | dst << 12 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FMDRR(Register high, Register low, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0c400b10 | dst | low << 12 | high << 16 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FMRRD(VFPDoubleRegister src, Register high, Register low, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0c500b10 | src | low << 12 | high << 16 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FCPYD(VFPDoubleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb00b40 | src | dst << 12 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FNEGD(VFPDoubleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb10b40 | src | dst << 12 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FABSD(VFPDoubleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb00bc0 | src | dst << 12 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FADDD(VFPDoubleRegister lhs, VFPDoubleRegister rhs, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0e300b00 | rhs | dst << 12 | lhs << 16 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FSUBD(VFPDoubleRegister lhs, VFPDoubleRegister rhs, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0e300b40 | rhs | dst << 12 | lhs << 16 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FMULD(VFPDoubleRegister lhs, VFPDoubleRegister rhs, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0e200b00 | rhs | dst << 12 | lhs << 16 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FDIVD(VFPDoubleRegister lhs, VFPDoubleRegister rhs, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0e800b00 | rhs | dst << 12 | lhs << 16 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FCVTDS(VFPSingleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb70ac0 | dst << 12 | src | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FCVTSD(VFPDoubleRegister src, VFPSingleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb70bc0 | dst << 12 | src | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FTOSID(VFPDoubleRegister src, VFPSingleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0ebd0b40 | (dst >> 1) << 12 | (dst & 1u) << 22 | src | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FTOSIZD(VFPDoubleRegister src, VFPSingleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0ebd0bc0 | (dst >> 1) << 12 | (dst & 1u) << 22 | src | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FSITOD(VFPSingleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb80bc0 | dst << 12 | src >> 1 | (src & 1u) << 5 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FSITOS(VFPSingleRegister src, VFPSingleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb80ac0 | dst << 12 | src >> 1 | (src & 1u) << 5 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FUITOD(VFPSingleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb80b40 | dst << 12 | src >> 1 | (src & 1u) << 5 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FSQRTD(VFPDoubleRegister src, VFPDoubleRegister dst, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb10bc0 | dst << 12 | src | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FCMPD(VFPDoubleRegister lhs, VFPDoubleRegister rhs, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb40b40 | lhs << 12 | rhs | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FCMPZD(VFPDoubleRegister lhs, ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0eb50b40 | lhs << 12 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::FMSTAT(ES_NativeJumpCondition condition)
{
    CHECK_VFP_SUPPORT();

    Reserve();

    ES_NativeInstruction codeword = 0x0ef1fa10 | condition << 28;

    *buffer++ = codeword;
}

#else // ARCHITECTURE_ARM_VFP

/* static */ BOOL
ES_CodeGenerator::SupportsVFP()
{
    return FALSE;
}

#endif // ARCHITECTURE_ARM_VFP

/* virtual */ const OpExecMemory *
ES_CodeGenerator::GetCode(OpExecMemoryManager *execmem, void* /* constant_reg */)
{
    FinalizeBlockList();

    Block *block;
    Constant *constant;

    unsigned size = 0;

    block = first_block;
    while (block)
    {
        size += block->end - block->start;

        if (block->jump_condition != ES_NATIVE_NOT_A_JUMP)
        {
            ++size;

            Block *target = block->jump_target;

            while (target->start == target->end)
                if (target->jump_condition == ES_NATIVE_CONDITION_ALWAYS)
                    target = target->jump_target;
                else if (target->jump_condition == ES_NATIVE_NOT_A_JUMP)
                    target = target->next;
                else
                    break;

            block->jump_target = target;
        }

        block = block->next;
    }

    unsigned constant_size = 0;

    constant = first_constant;
    while (constant)
    {
        constant_size += (constant->alignment + constant->size + sizeof(ES_NativeInstruction) + 1) / sizeof(ES_NativeInstruction);

        if ((size * sizeof(ES_NativeInstruction)) % constant->alignment)
            size += (constant->alignment - size * sizeof(ES_NativeInstruction) % constant->alignment) / sizeof(ES_NativeInstruction);

        size += (constant->size + sizeof(ES_NativeInstruction) - 1) / sizeof(ES_NativeInstruction);

        Constant::Use *use = constant->first_use;
        while (use)
        {
            use->offset |= 0x40000000u;
            use = use->next_per_constant;
        }

        constant = constant->next;
    }

    maximum_constant_range /= sizeof(ES_NativeInstruction);

    BOOL is_simple = size < maximum_constant_range;

    const OpExecMemory *result;
    unsigned *write_base;

    if (is_simple)
    {
        result = execmem->AllocateL(size * sizeof(ES_NativeInstruction));
        write_base = reinterpret_cast<unsigned *>(result->address);
    }
    else
        write_base = OP_NEWGROA_L(unsigned, size + constant_size * (size / maximum_constant_range), arena);

    unsigned *write = write_base;

#ifdef NATIVE_DISASSEMBLER_SUPPORT
#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
    Block::Annotation *all_annotations = NULL, **annotations_pointer = &all_annotations;
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

    struct ES_NativeDisassembleData nd_data;
    struct ES_NativeDisassembleRange *nd_range = NULL;

    if (disassemble)
    {
        nd_data.ranges = nd_range = OP_NEWGRO_L(struct ES_NativeDisassembleRange, (), arena);
        nd_data.annotations = NULL;

        nd_range->type = ES_NDRT_CODE;
        nd_range->start = 0;
        nd_range->next = NULL;
    }
#endif // NATIVE_DISASSEMBLER_SUPPORT

    Block **block_ptr = &first_block, *previous_block = NULL;
    unsigned *first_constant_use = NULL, next_data_block_size = 0;

    block = first_block;
    while (block)
    {
        unsigned count = block->end - block->start, *read = buffer_base + block->start, *skip_constants_jump = NULL;
        unsigned length = count + (block->jump_condition == ES_NATIVE_NOT_A_JUMP ? 0 : 1);

        if (!is_simple && next_data_block_size != 0 && (write - first_constant_use) + length + next_data_block_size >= maximum_constant_range)
        {
            constant = first_constant;
            while (constant)
                if (OutputConstant(constant, NULL, write, FALSE))
                {
                    if (previous_block->jump_condition != ES_NATIVE_CONDITION_ALWAYS)
                        skip_constants_jump = write++;

                    constant = first_constant;
                    while (constant)
                    {
#ifdef NATIVE_DISASSEMBLER_SUPPORT
                        if (disassemble && nd_range->type == ES_NDRT_CODE)
                            nd_range->end = (write - write_base) * sizeof(ES_NativeInstruction);
#endif // NATIVE_DISASSEMBLER_SUPPORT

                        if (OutputConstant(constant, write_base, write, FALSE))
                        {
#ifdef NATIVE_DISASSEMBLER_SUPPORT
                            if (disassemble && nd_range->type == ES_NDRT_CODE)
                            {
                                nd_range = nd_range->next = OP_NEWGRO_L(struct ES_NativeDisassembleRange, (), arena);

                                nd_range->type = ES_NDRT_DATA;
                                nd_range->start = constant->first_instance->offset;
                                nd_range->next = NULL;
                            }
#endif // NATIVE_DISASSEMBLER_SUPPORT
                        }

                        constant = constant->next;
                    }

#ifdef NATIVE_DISASSEMBLER_SUPPORT
                    if (disassemble)
                    {
                        nd_range->end = (write - write_base) * sizeof(ES_NativeInstruction);

                        nd_range = nd_range->next = OP_NEWGRO_L(struct ES_NativeDisassembleRange, (), arena);

                        nd_range->type = ES_NDRT_CODE;
                        nd_range->start = (write - write_base) * sizeof(ES_NativeInstruction);
                        nd_range->next = NULL;
                    }
#endif // NATIVE_DISASSEMBLER_SUPPORT

                    if (skip_constants_jump)
                    {
                        unsigned offset = write - skip_constants_jump - 2;
                        *skip_constants_jump = 0xea000000u | offset;
                    }

                    first_constant_use = NULL;
                    next_data_block_size = 0;
                }
                else
                    constant = constant->next;
        }

        block->actual_start = write - write_base;

        for (unsigned index = 0; index < count; ++index)
            *write++ = *read++;

        if (block->jump_condition != ES_NATIVE_NOT_A_JUMP)
        {
            if (block->jump_target->actual_start != UINT_MAX)
            {
                int offset = -static_cast<int>(write - write_base + 2 - block->jump_target->actual_start);
                *write++ = 0x0a000000u | (block->jump_is_call ? 1 : 0) << 24 | static_cast<unsigned>(offset) & 0x00ffffffu | block->jump_condition << 28;
            }
            else
            {
                Block *source = block, *next = source->next;
                Block *target = block->jump_target;

                while (next != target && next->start == next->end && next->jump_condition == ES_NATIVE_NOT_A_JUMP)
                {
                    source = next;
                    next = source->next;
                }

                if (next != target)
                {
                    *block_ptr = block;
                    block_ptr = &block->next;
                    ++write;
                }
                else
                    block->jump_condition = ES_NATIVE_NOT_A_JUMP;
            }
        }

        block->actual_end = write - write_base;

        if (Constant::Use *use = block->first_constant_use)
        {
            if (!first_constant_use)
                first_constant_use = write_base + block->actual_start + (use->offset & 0xffffffu);

            do
            {
                Constant::Use *other = use->constant->first_use;

                do
                {
                    if (other->block->actual_start != UINT_MAX && (other->offset & 0x40000000u) != 0 &&
                        (other->block != block || other->offset < use->offset))
                        break;
                }
                while ((other = other->next_per_constant) != NULL);

                if (!other)
                    next_data_block_size += use->constant->alignment + use->constant->size;
            }
            while ((use = use->next_per_block) != NULL);
        }

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
        if (block->actual_start != block->actual_end)
        {
            Block::Annotation *annotation = block->annotations;

            while (annotation)
            {
                *annotations_pointer = annotation;

                annotation->offset += block->actual_start;

                annotation = *(annotations_pointer = &annotation->next);
            }
        }
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

        previous_block = block;
        block = block->next;
    }

    *block_ptr = NULL;

    block = first_block;
    while (block)
    {
        if (block->jump_target->actual_start == block->actual_end)
            write_base[block->actual_end - 1] = DataProcessingInstruction(OPCODE_MOV, REG_R0, REG_R0, REG_R0, UNSET_CONDITION_CODES, ES_NATIVE_CONDITION_ALWAYS);
        else
        {
            int offset = block->jump_target->actual_start - (block->actual_end + 1);

            OP_ASSERT(offset >= 0);

            write_base[block->actual_end - 1] = 0x0a000000u | (block->jump_is_call ? 1 : 0) << 24 | offset | block->jump_condition << 28;
        }

        block = block->next;
    }

    constant = first_constant;
    while (constant)
    {
#ifdef NATIVE_DISASSEMBLER_SUPPORT
        if (disassemble && nd_range->type == ES_NDRT_CODE && OutputConstant(constant, NULL, write, TRUE))
        {
            nd_range->end = (write - write_base) * sizeof(ES_NativeInstruction);

            OutputConstant(constant, write_base, write, TRUE);

            nd_range = nd_range->next = OP_NEWGRO_L(struct ES_NativeDisassembleRange, (), arena);

            nd_range->type = ES_NDRT_DATA;
            nd_range->start = constant->first_instance->offset;
            nd_range->next = NULL;
        }
        else
#endif // NATIVE_DISASSEMBLER_SUPPORT
            OutputConstant(constant, write_base, write, TRUE);

        constant = constant->next;
    }

#ifdef NATIVE_DISASSEMBLER_SUPPORT
    if (disassemble)
        nd_range->end = (write - write_base) * sizeof(ES_NativeInstruction);
#endif // NATIVE_DISASSEMBLER_SUPPORT

#if 0
    /* In case we need to reverse byte-order. */
    for (unsigned index = 0, used = GetBufferUsed(); index < used; ++index)
    {
        unsigned char *dst = reinterpret_cast<unsigned char *>(block->address) + index * sizeof(ES_NativeInstruction);
        unsigned char *src = reinterpret_cast<unsigned char *>(buffer_base) + index * sizeof(ES_NativeInstruction);

        dst[0] = src[3];
        dst[1] = src[2];
        dst[2] = src[1];
        dst[3] = src[0];
    }
#endif // 0

    if (!is_simple)
    {
        size = write - write_base;

        result = execmem->AllocateL(size * sizeof(ES_NativeInstruction));

        op_memcpy(result->address, write_base, size * sizeof(ES_NativeInstruction));
    }

    constant = first_constant;
    while (constant)
    {
        if (Constant *address_of = constant->address_of)
        {
            void *address = static_cast<char *>(result->address) + constant->first_instance->offset;

            Constant::Instance *instance = address_of->first_instance;
            while (instance)
            {
                static_cast<void **>(result->address)[instance->offset / sizeof(ES_NativeInstruction)] = address;
                instance = instance->next;
            }
        }

        constant = constant->next;
    }

#ifdef NATIVE_DISASSEMBLER_SUPPORT
    if (disassemble)
    {
#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
        struct ES_NativeDisassembleAnnotation **nd_annotations = &nd_data.annotations;

        Block::Annotation *annotation = all_annotations;
        while (annotation)
        {
            struct ES_NativeDisassembleAnnotation *nd_annotation = *nd_annotations = OP_NEWGRO_L(struct ES_NativeDisassembleAnnotation, (), arena);
            nd_annotations = &nd_annotation->next;

            nd_annotation->offset = annotation->offset * 4;
            nd_annotation->value = annotation->value;
            nd_annotation->next = NULL;

            annotation = annotation->next;
        }
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

        ES_NativeDisassemble(result->address, result->size, ES_NDM_DEBUGGING, &nd_data);
    }

    if (dump_cv)
    {
#if 0
        ES_NativeDisassemble(reinterpret_cast<unsigned char *>(code_begin), reinterpret_cast<unsigned char *>(code_end),
                             reinterpret_cast<unsigned char *>(data_begin), reinterpret_cast<unsigned char *>(data_end),
                             NULL, NULL, 1);
#endif // 0
    }
#endif // NATIVE_DISASSEMBLER_SUPPORT

    return result;
}

#ifdef NATIVE_DISASSEMBLER_SUPPORT

BOOL
ES_CodeGenerator::DisassembleEnabled()
{
    extern FILE *g_native_disassembler_file;
    return g_native_disassembler_file && disassemble;
}

#endif // NATIVE_DISASSEMBLER_SUPPORT

void
ES_CodeGenerator::DataProcessing(DataProcessingOpCode opcode, Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    OP_ASSERT(condition != ES_NATIVE_CONDITION_NEVER);

    Reserve();

    *buffer++ = DataProcessingInstruction(opcode, op1, op2, dst, set_condition_codes, condition);
}

void
ES_CodeGenerator::Multiply(BOOL accumulate, Register op1, Register op2, Register acc, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    OP_ASSERT(condition != ES_NATIVE_CONDITION_NEVER);

    Reserve();

    ES_NativeInstruction codeword = op1 | 1 << 4 | 1 << 7 | op2 << 8 | acc << 12 | dst << 16 | (set_condition_codes ? 1 : 0) << 20 | (accumulate ? 1 : 0) << 21 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::SingleDataTransfer(BOOL load, BOOL write_back, OperandSize opsize, BOOL up, BOOL pre_indexing, Register base, BOOL register_offset, unsigned offset_imm, Register offset_reg, ShiftType offset_shift_type, unsigned offset_shift_amount, Register src_dst, ES_NativeJumpCondition condition)
{
    OP_ASSERT(condition != ES_NATIVE_CONDITION_NEVER);
    OP_ASSERT(!register_offset || (offset_shift_amount & ~0x1fu) == 0);
    OP_ASSERT(register_offset || (offset_imm & ~0xfffu) == 0);

    Reserve();

    if (!pre_indexing)
        write_back = FALSE;

    ES_NativeInstruction codeword = src_dst << 12 | base << 16 | (load ? 1 : 0) << 20 | (write_back ? 1 : 0) << 21 | (opsize == OPSIZE_8 ? 1 : 0) << 22 | (up ? 1 : 0) << 23 | (pre_indexing ? 1 : 0) << 24 | (register_offset ? 1 : 0) << 25 | 1 << 26 | condition << 28;

    if (register_offset)
        codeword |= offset_reg | offset_shift_type << 5 | (offset_shift_amount & 0x1fu) << 7;
    else
        codeword |= offset_imm & 0xfffu;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::BlockDataTransfer(BOOL load, BOOL write_back, BOOL up, BOOL pre_indexing, Register base, unsigned registers, ES_NativeJumpCondition condition)
{
    OP_ASSERT(registers != 0);
    OP_ASSERT((registers & ~0xffffu) == 0);

    Reserve();

    ES_NativeInstruction codeword = registers | base << 16 | (load ? 1 : 0) << 20 | (write_back ? 1 : 0) << 21 | (up ? 1 : 0) << 23 | (pre_indexing ? 1 : 0) << 24 | 1 << 27 | condition << 28;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::HalfWordDataTransfer(BOOL load, BOOL write_back, BOOL up, BOOL pre_indexing, Register base, BOOL register_offset, unsigned offset_imm, Register offset_reg, Register src_dst, ES_NativeJumpCondition condition)
{
    OP_ASSERT(condition != ES_NATIVE_CONDITION_NEVER);
    OP_ASSERT(register_offset || (offset_imm & ~0xffu) == 0);

    Reserve();

    if (!pre_indexing)
        write_back = FALSE;

    ES_NativeInstruction codeword = src_dst << 12 | base << 16 | (load ? 1 : 0) << 20 | (write_back ? 1 : 0) << 21 | (up ? 1 : 0) << 23 | (pre_indexing ? 1 : 0) << 24 | condition << 28 | 0xb0u;

/*
  LDRH imm
  cond[4] 0 0 0 P U 1 W 1 Rn[4] Rt[4] imm4H 1 0 1 1 imm4L

  LDRH reg
  cond[4] 0 0 0 P U 0 W 1 Rn[4] Rt[4] (0 0 0 0)  1 0 1 1 Rm[4]

  STRH imm
  cond[4] 0 0 0 P U 1 W 0 Rn[4] Rt[4] imm4H 1 0 1 1 imm4L

  STRH reg
  cond[4] 0 0 0 P U 0 W 0 Rn[4] Rt[4] (0 0 0 0)  1 0 1 1 Rm[4]
*/
    if (register_offset)
        codeword |= offset_reg;
    else
        codeword |= 1 << 22 | (offset_imm & 0xf0u) << 4 | (offset_imm & 0xfu);

    *buffer++ = codeword;
}

BOOL
ES_CodeGenerator::OutputConstant(Constant *constant, unsigned *base, unsigned *&current, BOOL include_addressed)
{
    /* First make sure this constant has any relevant uses: if it doesn't,
       there's no point in outputting it. */
    Constant::Use *use = constant->first_use;
    while (use)
    {
        /* If 'use->block->actual_start' is UINT_MAX, that block hasn't been
           output yet, and neither has the reference to this constant.

           If 'use->offset & 0x40000000u' is zero, this reference to the
           constant has already been processed. */
        if (use->block->actual_start != UINT_MAX && (use->offset & 0x40000000u) != 0)
            break;

        use = use->next_per_constant;
    }
    if (!use && !(include_addressed && constant->address_of))
        return FALSE;
    else if (base)
    {
        unsigned offset = (current - base) * sizeof(ES_NativeInstruction);

        if (offset % constant->alignment)
            offset += constant->alignment - offset % constant->alignment;

        constant->AddInstance(offset, arena);

        unsigned *write = base + offset / sizeof(ES_NativeInstruction);

        switch (constant->type)
        {
        case Constant::TYPE_INT:
            reinterpret_cast<int *>(write)[0] = constant->value.i;
            break;

        case Constant::TYPE_DOUBLE:
            reinterpret_cast<double *>(write)[0] = constant->value.d;
            break;

        case Constant::TYPE_2DOUBLE:
            reinterpret_cast<double *>(write)[0] = constant->value.d2.d1;
            reinterpret_cast<double *>(write)[1] = constant->value.d2.d2;
            break;

        case Constant::TYPE_POINTER:
            reinterpret_cast<const void **>(write)[0] = constant->value.p;
        }

        current = write + constant->size / sizeof(ES_NativeInstruction);

        while (use)
        {
            if (use->block->actual_start != UINT_MAX && (use->offset & 0x40000000u) != 0)
            {
                unsigned use_offset = use->offset & 0xffffu;
                unsigned instruction_index = use->block->actual_start + use_offset;
                int final_offset = static_cast<int>(offset) - static_cast<int>(instruction_index * sizeof(ES_NativeInstruction) + 8);

                /* The offset between the instruction that references the
                   constant and the constant itself can plausibly be negative in
                   one case: the constant is output right after the instruction.
                   Since we're addressing relative the program counter, which
                   is 8 bytes ahead, the next instruction is at offset -4.

                   Normally, in this case, we would have emitted a jump to skip
                   past the constants, so that we don't try to execute words
                   that don't contain instructions, but apparently we didn't do
                   that in this case.

                   Known to happen in one situation: a "tail recursing" function
                   call implemented as a LDR instruction loading the called
                   function's address into the program counter while the link
                   register points to somewhere else.  When the called function
                   returns, it doesn't return to the instruction after the call,
                   and thus we can safely put a constant there. */
                if (final_offset < 0)
                {
                    /* The U bit (which causes the 12-bit unsigned offset to be
                       subtracted from the base register instead of added to it)
                       in all LDR* instructions is bit 23. */
                    base[instruction_index] ^= 1 << 23;
                    /* Then negate the offset. */
                    final_offset = -final_offset;
                }

                if ((use->offset & 0x80000000u) != 0)
                {
                    /* FLDS/FLDD case: offset in instruction is multiplied by 4. */
                    final_offset /= 4;
                    OP_ASSERT((final_offset & 0xff) == final_offset);
                }
                else
                    OP_ASSERT((final_offset & 0xfff) == final_offset);

                base[instruction_index] |= final_offset;

                use->offset ^= 0x40000000u;
            }

            use = use->next_per_constant;
        }
    }

    return TRUE;
}

#endif // ARCHITECTURE_ARM
#endif // ES_NATIVE_SUPPORT
