/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#ifdef ARCHITECTURE_THUMB

#include "modules/ecmascript/carakan/src/util/es_codegenerator_arm.h"

ES_CodeGenerator::ES_CodeGenerator()
    : buffer_base(NULL),
      buffer(NULL),
      buffer_top(NULL)
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

ES_CodeGenerator::Operand::Operand(unsigned immediate)
    : is_register(FALSE)
{
    unsigned rotate_amount = 0;

    while ((immediate & ~0xffu) != 0)
    {
        OP_ASSERT(rotate_amount < 16);

        ++rotate_amount;
        immediate = immediate << 2 | (immediate & 0xc0000000u) >> 30;
    }

    u.imm.value = immediate;
    u.imm.rotate_amount = rotate_amount;
}

void
ES_CodeGenerator::BX(FullRegister src, Condition condition)
{
    Reserve();

    Instruction codeword = 0x4700 | src << 3;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::BLX(FullRegister src, Condition condition)
{
    Reserve();

    Instruction codeword = 0x4780 | src << 3;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::ADD(FullRegister src, FullRegister dst)
{
    Reserve();

    Instruction codeword = 0x4400 | (dst & 8) << 4 | src << 3 | (dst & 7);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::AddToSP(unsigned amount)
{
    OP_ASSERT((amount & 3) == 0 && amount < 512);

    Reserve();

    Instruction codeword = 0xb000 | amount >> 2;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::AddToSP(unsigned amount, Register dst)
{
    OP_ASSERT((amount & 3) == 0 && amount < 1020);

    Reserve();

    Instruction codeword = 0xa800 | dst << 8 | amount >> 2;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::AddToPC(unsigned amount, Register dst)
{
    OP_ASSERT((amount & 3) == 0 && amount < 1020);

    Reserve();

    Instruction codeword = 0xa000 | dst << 8 | amount >> 2;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::CMP(FullRegister op1, FullRegister op2)
{
    Reserve();

    Instruction codeword = 0x4500 | (op1 & 8) << 4 | op2 << 3 | (op1 & 7);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::MOV(FullRegister src, FullRegister dst)
{
    Reserve();

    Instruction codeword = 0x4600 | (dst & 8) << 4 | src << 3 | (dst & 7);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::PUSH(unsigned registers, Condition condition)
{
    OP_ASSERT(condition == CONDITION_AL);
    OP_ASSERT((registers & (0xff00 ^ (1 << REG_LR))) == 0);

    Reserve();

    Instruction codeword = 0xb400 | ((registers & (1 << REG_LR)) != 0 ? 0x100 : 0) | (registers & 0xff);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::POP(unsigned registers, Condition condition)
{
    OP_ASSERT(condition == CONDITION_AL);
    OP_ASSERT((registers & (0xff00 ^ (1 << REG_PC))) == 0);

    Reserve();

    Instruction codeword = 0xbc00 | ((registers & (1 << REG_PC)) != 0 ? 0x100 : 0) | (registers & 0xff);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::DataProcessing(DataProcessingOpCode opcode, Register op1, const Operand &op2, Register dst, BOOL set_condition_codes, Condition condition)
{
    OP_ASSERT(condition == CONDITION_AL);
    OP_ASSERT(set_condition_codes = TRUE);

    Reserve();

    Instruction codeword = 0;

    switch (opcode)
    {
    case OPCODE_AND:
        OP_ASSERT(op2.is_register && !op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
        OP_ASSERT(op1 == dst);

        codeword = 0x4000 | op2.u.reg.base << 3 | dst;
        break;

    case OPCODE_EOR:
        OP_ASSERT(op2.is_register && !op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
        OP_ASSERT(op1 == dst);

        codeword = 0x4040 | op2.u.reg.base << 3 | dst;
        break;

    case OPCODE_SUB:
        if (op2.is_register)
        {
            OP_ASSERT(!op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
            codeword = 0x1a00 | op2.u.reg.base << 6 | op1 << 3 | dst;
        }
        else
        {
            OP_ASSERT(op2.u.imm.value < 8 && op2.u.imm.rotate_amount == 0 || op2.u.imm.value < 256 && op1 == dst);
            if (op2.u.imm.value < 8)
                codeword = 0x1e00 | op2.u.imm.value << 6 | op1 << 3 | dst;
            else
                codeword = 0x3800 | dst << 8 | op2.u.imm.value;
        }
        break;

    case OPCODE_RSB:
            codeword = 0x4180;
            break;

    case OPCODE_ADD:
        if (op2.is_register)
        {
            OP_ASSERT(!op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
            codeword = 0x1800 | op2.u.reg.base << 6 | op1 << 3 | dst;
        }
        else
        {
            OP_ASSERT(op2.u.imm.value < 8 && op2.u.imm.rotate_amount == 0 || op2.u.imm.value < 256 && op1 == dst);
            if (op2.u.imm.value < 8)
                codeword = 0x1c00 | op2.u.imm.value << 6 | op1 << 3 | dst;
            else
                codeword = 0x3000 | dst << 8 | op2.u.imm.value;
        }
        break;

    case OPCODE_ADC:
        OP_ASSERT(op2.is_register && !op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
        OP_ASSERT(op1 == dst);

        codeword = 0x4140 | op2.u.reg.base << 3 | dst;
        break;

    case OPCODE_SBC:
        OP_ASSERT(op2.is_register && !op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
        OP_ASSERT(op1 == dst);

        codeword = 0x4180 | op2.u.reg.base << 3 | dst;
        break;

    case OPCODE_RSC:
        OP_ASSERT(FALSE);
        break;

    case OPCODE_TST:
        OP_ASSERT(op2.is_register && !op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
        OP_ASSERT(op1 == dst);

        codeword = 0x4200 | op2.u.reg.base << 3 | dst;
        break;

    case OPCODE_TEQ:
        OP_ASSERT(FALSE);
        break;

    case OPCODE_CMP:
        if (op2.is_register)
        {
            OP_ASSERT(!op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
            codeword = 0x8280 | op2.u.reg.base << 3 | op1;
        }
        else
        {
            OP_ASSERT(op2.u.imm.value < 256);
            codeword = 0x3800 | op1 << 8 | op2.u.imm.value;
        }
        break;

    case OPCODE_CMN:
        OP_ASSERT(op2.is_register && !op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);

        codeword = 0x4380 | op2.u.reg.base << 3 | op1;
        break;

    case OPCODE_ORR:
        OP_ASSERT(op2.is_register && !op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
        OP_ASSERT(op1 == dst);

        codeword = 0x4300 | op2.u.reg.base << 3 | dst;
        break;

    case OPCODE_MOV:
        if (op2.is_register)
        {
            OP_ASSERT(!op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
            codeword = 0x1c00 | op2.u.reg.base << 3 | op1;
        }
        else
        {
            OP_ASSERT(op2.u.imm.value < 256);
            codeword = 0x2000 | dst << 8 | op2.u.imm.value;
        }
        break;

    case OPCODE_BIC:
        OP_ASSERT(op2.is_register && !op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
        OP_ASSERT(op1 == dst);

        codeword = 0x4380 | op2.u.reg.base << 3 | dst;
        break;

    case OPCODE_MVN:
        OP_ASSERT(op2.is_register && !op2.u.reg.is_register_shift && op2.u.reg.u.shift_amount == 0);
        OP_ASSERT(op1 == dst);

        codeword = 0x43a0 | op2.u.reg.base << 3 | dst;
        break;
    }

    *buffer++ = codeword;
}

void
ES_CodeGenerator::Multiply(BOOL accumulate, Register op1, Register op2, Register acc, Register dst, BOOL set_condition_codes, Condition condition)
{
    OP_ASSERT(condition == CONDITION_AL);
    OP_ASSERT(set_condition_codes);
    OP_ASSERT(!accumulate);
    OP_ASSERT(op1 == dst);

    Reserve();

    Instruction codeword = 0x4340 | op2 << 3 | dst;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::SingleDataTransfer(BOOL load, BOOL write_back, OperandSize opsize, BOOL up, BOOL pre_indexing, Register base, BOOL register_offset, unsigned offset_imm, Register offset_reg, ShiftType offset_shift_type, unsigned offset_shift_amount, Register src_dst, Condition condition)
{
    OP_ASSERT(condition == CONDITION_AL);
    OP_ASSERT(!write_back);
    OP_ASSERT(pre_indexing);

    Instruction codeword;

    if (register_offset)
    {
        OP_ASSERT(offset_shift_amount == 0);

        if (opsize == OPSIZE_8)
            codeword = 0x5400;
        else if (opsize == OPSIZE_16)
            codeword = 0x5200;
        else
            codeword = 0x5000;

        if (load)
            codeword |= 0x0800;

        codeword |= base << 6 | offset_reg << 3 | src_dst;
    }
    else
    {
        OP_ASSERT((offset_imm & (opsize - 1)) == 0);
        OP_ASSERT(offset_imm / opsize < 32);

        if (opsize == OPSIZE_8)
            codeword = 0x7000;
        else if (opsize == OPSIZE_16)
            codeword = 0x8000;
        else
            codeword = 0x6000;

        if (load)
            codeword |= 0x0800;

        codeword |= (offset_imm / opsize) << 6 | base << 3 | src_dst;
    }

    Reserve();

    *buffer++ = codeword;
}

void
ES_CodeGenerator::BlockDataTransfer(BOOL load, BOOL write_back, BOOL up, BOOL pre_indexing, Register base, unsigned registers, Condition condition)
{
    OP_ASSERT(condition == CONDITION_AL);
    OP_ASSERT(registers != 0);
    OP_ASSERT((registers & ~0xffu) == 0);
    OP_ASSERT(!pre_indexing);
    OP_ASSERT(up);
    OP_ASSERT(write_back);

    Reserve();

    Instruction codeword = 0xc000 | (load ? 0x0800 : 0) | base << 8 | registers;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::Reserve()
{
    if (buffer == buffer_top)
    {
        unsigned new_size = buffer == NULL ? 64 : (buffer_top - buffer_base) * 2;
        Instruction *new_buffer = OP_NEWA(Instruction, new_size);

        for (Instruction *p = buffer_base, *q = new_buffer; p != buffer; ++p, ++q)
            *q = *p;

        buffer = new_buffer + (buffer - buffer_base);
        buffer_top = new_buffer + new_size;

        OP_DELETEA(buffer_base);
        buffer_base = new_buffer;
    }
}

#endif // ARCHITECTURE_THUMB
