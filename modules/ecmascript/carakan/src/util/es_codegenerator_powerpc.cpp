/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#ifdef ARCHITECTURE_POWERPC

#include "modules/ecmascript/carakan/src/util/es_codegenerator_powerpc.h"

ES_CodeGenerator::ES_CodeGenerator()
    : buffer_base(NULL),
      buffer(NULL),
      buffer_top(NULL)
{
}

ES_CodeGenerator::Operand::Operand(ES_CodeGenerator::Register base)
  : type(TYPE_OFFSET),
    base(base),
    offset(0)
{
}

ES_CodeGenerator::Operand::Operand(ES_CodeGenerator::Register base, short offset)
  : type(TYPE_OFFSET),
    base(base),
    offset(offset)
{
}

ES_CodeGenerator::Operand::Operand(ES_CodeGenerator::Register base, ES_CodeGenerator::Register index)
  : type(TYPE_INDEX),
    base(base),
    index(index)
{
}

void
ES_CodeGenerator::AND(Register op1, Register op2, Register dst, BOOL update_cr0)
{
    Reserve();

    unsigned codeword = 31 << 26 | op1 << 21 | dst << 16 | op2 << 11 | 28 << 1 | (update_cr0 ? 1 : 0);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::NAND(Register op1, Register op2, Register dst, BOOL update_cr0)
{
    Reserve();

    unsigned codeword = 31 << 26 | op1 << 21 | dst << 16 | op2 << 11 | 476 << 1 | (update_cr0 ? 1 : 0);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::OR(Register op1, Register op2, Register dst, BOOL update_cr0)
{
    Reserve();

    unsigned codeword = 31 << 26 | op1 << 21 | dst << 16 | op2 << 11 | 444 << 1 | (update_cr0 ? 1 : 0);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::XOR(Register op1, Register op2, Register dst, BOOL update_cr0)
{
    Reserve();

    unsigned codeword = 31 << 26 | op1 << 21 | dst << 16 | op2 << 11 | 316 << 1 | (update_cr0 ? 1 : 0);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::ADD(Register op1, Register op2, Register dst, BOOL update_cr0, BOOL update_xer)
{
    Reserve();

    unsigned codeword = 31 << 26 | dst << 21 | op1 << 16 | op2 << 11 | (update_xer ? 1 : 0) << 10 | 266 << 1 | (update_cr0 ? 1 : 0);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::ADDI(Register op1, short immediate, Register dst)
{
    Reserve();

    unsigned codeword = 14 << 26 | dst << 21 | op1 << 16 | static_cast<unsigned short>(immediate);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::ADDIS(Register op1, short immediate, Register dst)
{
    Reserve();

    unsigned codeword = 15 << 26 | dst << 21 | op1 << 16 | static_cast<unsigned short>(immediate);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::SUB(Register op1, Register op2, Register dst, BOOL update_cr0, BOOL update_xer)
{
    Reserve();

    unsigned codeword = 31 << 26 | dst << 21 | op2 << 16 | op1 << 11 | (update_xer ? 1 : 0) << 10 | 40 << 1 | (update_cr0 ? 1 : 0);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::LDW(const Operand &src, Register dst, BOOL update)
{
    Reserve();

    unsigned codeword = dst << 21 | src.base << 16;

    if (src.type == Operand::TYPE_OFFSET)
        codeword |= (update ? 33 : 32) << 26 | static_cast<unsigned short>(src.offset);
    else
        codeword |= 31 << 26 | src.index << 11 | (update ? 55 : 23) << 1;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::STW(Register src, const Operand &dst, BOOL update)
{
    Reserve();

    unsigned codeword = src << 21 | dst.base << 16;

    if (dst.type == Operand::TYPE_OFFSET)
        codeword |= (update ? 37 : 36) << 26 | static_cast<unsigned short>(dst.offset);
    else
        codeword |= 31 << 26 | dst.index << 11 | (update ? 183 : 151) << 1;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::LMW(const Operand &src, Register dst)
{
    Reserve();

    OP_ASSERT(src.type == Operand::TYPE_OFFSET);

    unsigned codeword = 46 << 26 | dst << 21 | src.base << 16 | static_cast<unsigned short>(src.offset);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::SMW(Register src, const Operand &dst)
{
    Reserve();

    OP_ASSERT(dst.type == Operand::TYPE_OFFSET);

    unsigned codeword = 47 << 26 | src << 21 | dst.base << 16 | static_cast<unsigned short>(dst.offset);

    *buffer++ = codeword;
}

void
ES_CodeGenerator::MFCR(Register dst)
{
    Reserve();

    unsigned codeword = 31 << 26 | dst << 21 | 19 << 1;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::MFLR(Register dst)
{
    Reserve();

    unsigned codeword = 31 << 26 | dst << 21 | 8 << 16 | 339 << 1;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::MTLR(Register src)
{
    Reserve();

    unsigned codeword = 31 << 26 | src << 21 | 8 << 16 | 467 << 1;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::MFCTR(Register dst)
{
    Reserve();

    unsigned codeword = 31 << 26 | dst << 21 | 9 << 16 | 339 << 1;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::MTCTR(Register src)
{
    Reserve();

    unsigned codeword = 31 << 26 | src << 21 | 9 << 16 | 467 << 1;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::MTCRF(unsigned char mask, Register dst)
{
    Reserve();

    unsigned codeword = 31 << 26 | dst << 21 | mask << 12 | 144 << 1;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::BLR()
{
    Reserve();

    unsigned codeword = 0x4e800020; // 19 << 26 | 10 << 21 | 16 << 1;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::BCTRL()
{
    Reserve();

    unsigned codeword = 0x4e800421u; // 19 << 26 | 10 << 21 | 528 << 1 | 1;

    *buffer++ = codeword;
}

void
ES_CodeGenerator::Reserve()
{
    if (buffer == buffer_top)
    {
        unsigned new_size = buffer == NULL ? 64 : (buffer_top - buffer_base) * 2;
        unsigned *new_buffer = OP_NEWA(unsigned, new_size);

        for (unsigned *p = buffer_base, *q = new_buffer; p != buffer; ++p, ++q)
            *q = *p;

        buffer = new_buffer + (buffer - buffer_base);
        buffer_top = new_buffer + new_size;

        OP_DELETEA(buffer_base);
        buffer_base = new_buffer;
    }
}

#endif // ARCHITECTURE_POWERPC
