
#ifndef ES_CODEGENERATOR_MIPS_INLINES_H
#define ES_CODEGENERATOR_MIPS_INLINES_H

#include "modules/ecmascript/carakan/src/util/es_codegenerator_mips.h"

inline void
ES_CodeGenerator::Move(Register rd, Register rs)
{
    ADDU(rd, rs, REG_ZERO);
}

inline void
ES_CodeGenerator::Add(Register rd, Register rt)
{
    ADDU(rd, rd, rt);
}

inline void
ES_CodeGenerator::Add(Register rt, int immediate)
{
    Add(rt, rt, immediate);
}

inline void
ES_CodeGenerator::And(Register rt, unsigned immediate)
{
    And(rt, rt, immediate);
}

inline void
ES_CodeGenerator::Sub(Register rd, Register rt)
{
    SUBU(rd, rd, rt);
}

inline void
ES_CodeGenerator::Sub(Register rt, int immediate)
{
    Sub(rt, rt, immediate);
}

inline void
ES_CodeGenerator::JumpEQ(JumpTarget *target, Register rt, Register rs, BOOL hint)
{
    Jump(target, ES_NATIVE_CONDITION_EQ(rt, rs), hint);
}

inline void
ES_CodeGenerator::JumpNE(JumpTarget *target, Register rt, Register rs, BOOL hint)
{
    Jump(target, ES_NATIVE_CONDITION_NE(rt, rs), hint);
}

inline void
ES_CodeGenerator::JumpEQ(JumpTarget *target, Register rt, int immediate)
{
    OP_ASSERT(rt != REG_AT);
    Load(REG_AT, immediate);
    Jump(target, ES_NATIVE_CONDITION_EQ(rt, REG_AT));
}

inline void
ES_CodeGenerator::JumpNE(JumpTarget *target, Register rt, int immediate)
{
    OP_ASSERT(rt != REG_AT);
    Load(REG_AT, immediate);
    Jump(target, ES_NATIVE_CONDITION_NE(rt, REG_AT));
}

inline void
ES_CodeGenerator::JumpLT(JumpTarget *target, Register rt, Register rs)
{
    SLT(REG_AT, rt, rs);
    Jump(target, ES_NATIVE_CONDITION_NEZ(REG_AT));
}

inline void
ES_CodeGenerator::JumpLE(JumpTarget *target, Register rt, Register rs)
{
    SLT(REG_AT, rs, rt);
    Jump(target, ES_NATIVE_CONDITION_EQZ(REG_AT));
}

inline void
ES_CodeGenerator::JumpGT(JumpTarget *target, Register rt, Register rs)
{
    SLT(REG_AT, rs, rt);
    Jump(target, ES_NATIVE_CONDITION_NEZ(REG_AT));
}

inline void
ES_CodeGenerator::JumpGE(JumpTarget *target, Register rt, Register rs)
{
    SLT(REG_AT, rt, rs);
    Jump(target, ES_NATIVE_CONDITION_EQZ(REG_AT));
}

inline void
ES_CodeGenerator::B(short offset)
{
    BEQ(REG_ZERO, REG_ZERO, offset);
}

#endif // ES_CODEGENERATOR_MIPS_INLINES_H
