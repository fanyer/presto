/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#ifdef ARCHITECTURE_POWERPC

class ES_CodeGenerator
{
public:
    ES_CodeGenerator();

    enum Register
    {
        REG_R0,
        REG_R1,
        REG_R2,
        REG_R3,
        REG_R4,
        REG_R5,
        REG_R6,
        REG_R7,
        REG_R8,
        REG_R9,
        REG_R10,
        REG_R11,
        REG_R12,
        REG_R13,
        REG_R14,
        REG_R15,
        REG_R16,
        REG_R17,
        REG_R18,
        REG_R19,
        REG_R20,
        REG_R21,
        REG_R22,
        REG_R23,
        REG_R24,
        REG_R25,
        REG_R26,
        REG_R27,
        REG_R28,
        REG_R29,
        REG_R30,
        REG_R31,

        REG_ZERO = REG_R0,
        REG_SP = REG_R1
    };

    class Operand
    {
    public:
        enum Type
        {
            TYPE_OFFSET,
            TYPE_INDEX
        };

        Operand(ES_CodeGenerator::Register base);
        Operand(ES_CodeGenerator::Register base, short offset);
        Operand(ES_CodeGenerator::Register base, ES_CodeGenerator::Register index);

        Type type;
        ES_CodeGenerator::Register base;
        union
        {
            short offset;
            ES_CodeGenerator::Register index;
        };
    };

    void AND(Register op1, Register op2, Register dst, BOOL update_cr0 = TRUE);
    void NAND(Register op1, Register op2, Register dst, BOOL update_cr0 = TRUE);
    void OR(Register op1, Register op2, Register dst, BOOL update_cr0 = TRUE);
    void XOR(Register op1, Register op2, Register dst, BOOL update_cr0 = TRUE);

    void ADD(Register op1, Register op2, Register dst, BOOL update_cr0 = FALSE, BOOL update_xer = FALSE);
    void ADDI(Register op1, short immediate, Register dst);
    void ADDIS(Register op1, short immediate, Register dst);
    void SUB(Register op1, Register op2, Register dst, BOOL update_cr0 = FALSE, BOOL update_xer = FALSE);

    void Move(Register src, Register dst);
    void Move(short immediate, Register dst);

    void LDW(const Operand &src, Register dst, BOOL update = FALSE);
    void STW(Register src, const Operand &dst, BOOL update = FALSE);

    void LMW(const Operand &src, Register dst);
    void SMW(Register src, const Operand &dst);

    void MFCR(Register dst);
    void MFLR(Register dst);
    void MTLR(Register src);
    void MFCTR(Register dst);
    void MTCTR(Register src);
    void MTCRF(unsigned char mask, Register src);

    void BLR();
    void BCTRL();

    unsigned *GetBuffer() { return buffer_base; }
    unsigned GetBufferUsed() { return buffer - buffer_base; }

private:
    void Reserve();

    unsigned *buffer_base, *buffer, *buffer_top;
};

#include "modules/ecmascript/carakan/src/util/es_codegenerator_powerpc_inlines.h"

#endif // ARCHITECTURE_POWERPC
