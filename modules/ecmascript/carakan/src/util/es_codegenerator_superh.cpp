/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#ifdef ARCHITECTURE_SUPERH

#include "modules/ecmascript/carakan/src/util/es_codegenerator_superh.h"

ES_CodeGenerator::ES_CodeGenerator()
    : buffer_base(NULL),
      buffer(NULL),
      buffer_top(NULL)
{
}

void
ES_CodeGenerator::MOV(char immediate, Register dst)
{
    /* <1110 Rn Imm> with Rn=dst */
    Write(0xe000 | immediate | dst << 8);
}

void
ES_CodeGenerator::MOV(Register src, Register dst)
{
    /* <0110 Rn Rm 0011> with Rn=dst and Rm=src */
    Write(0x6003 | src << 4 | dst << 8);
}

void
ES_CodeGenerator::MOV(Memory src, Register dst, OperandSize opsize)
{
    unsigned short codeword;

    if (src.type == Memory::TYPE_DISPLACEMENT && opsize != OPSIZE_32)
    {
        /* MOV.B and MOV.W can only encode R0 as destination register. */
        OP_ASSERT(dst == REG_R0);

        codeword = (opsize == OPSIZE_8 ? 0x8400 : 0x8500) | src.base << 4 | (src.displacement & 0xf);
    }
    else
    {
        switch (src.type)
        {
        case Memory::TYPE_PLAIN:
            codeword = 0x6000;
            break;

        case Memory::TYPE_AUTO_ADJUST:
            codeword = 0x6004;
            break;

        case Memory::TYPE_R0_ADDED:
            codeword = 0x000c;
            break;

        default:
            codeword = 0x5000 | (src.displacement & 0xf);
        }

        if (src.type != Memory::TYPE_DISPLACEMENT)
            if (opsize == OPSIZE_16)
                codeword |= 1;
            else
                codeword |= 2;

        codeword |= (src.base << 4) | (dst << 8);
    }

    Write(codeword);
}

void
ES_CodeGenerator::MOV(Register src, Memory dst, OperandSize opsize)
{
    unsigned short codeword;

    if (dst.type == Memory::TYPE_DISPLACEMENT && opsize != OPSIZE_32)
    {
        /* MOV.B and MOV.W can only encode R0 as destination register. */
        OP_ASSERT(src == REG_R0);

        codeword = (opsize == OPSIZE_8 ? 0x8000 : 0x8100) | dst.base << 4 | (dst.displacement & 0xf);
    }
    else
    {
        switch (dst.type)
        {
        case Memory::TYPE_PLAIN:
            codeword = 0x2000;
            break;

        case Memory::TYPE_AUTO_ADJUST:
            codeword = 0x2004;
            break;

        case Memory::TYPE_R0_ADDED:
            codeword = 0x0004;
            break;

        default:
            codeword = 0x1000 | (dst.displacement & 0xf);
        }

        if (dst.type != Memory::TYPE_DISPLACEMENT)
            if (opsize == OPSIZE_16)
                codeword |= 1;
            else
                codeword |= 2;

        codeword |= (dst.base << 8) | (src << 4);
    }

    Write(codeword);
}

void
ES_CodeGenerator::SUB(Register src, Register dst)
{
    /* < 0011 Rm Rn 1000 > with Rm=src and Rn=dst */
    Write(0x3008 | src << 4 | dst << 8);
}

void
ES_CodeGenerator::JSR(Register dst)
{
    /* < 0100 Rn 00001011 > with Rn=dst */
    Write(0x400b | dst << 8);
}

void
ES_CodeGenerator::RTS()
{
    /* < 0000000000001101 > */
    Write(0x000b);
}

void
ES_CodeGenerator::NOP()
{
    /* < 0000000000001001 > */
    Write(0x0009);
}

void
ES_CodeGenerator::PushPR()
{
    /* <0100 Rn 00100010> with Rn=R15 */
    Write(0x4f22);
}

void
ES_CodeGenerator::PopPR()
{
    /* <0100 Rn 00100110> with Rn=R15 */
    Write(0x4f26);
}

void
ES_CodeGenerator::Reserve()
{
    if (!buffer || buffer >= buffer_top - 1)
    {
        unsigned new_size = (buffer_top - buffer_base) * 2;
        if (new_size == 0)
            new_size = 256;
        unsigned char *new_buffer = OP_NEWA(unsigned char, new_size);

        for (unsigned char *p = buffer_base, *q = new_buffer; p != buffer; ++p, ++q)
            *q = *p;

        buffer = new_buffer + (buffer - buffer_base);
        buffer_top = new_buffer + new_size;

        OP_DELETEA(buffer_base);
        buffer_base = new_buffer;
    }
}

#endif // ARCHITECTURE_SUPERH
