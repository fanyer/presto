/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

inline void
ES_CodeGenerator::Move(Register src, Register dst)
{
    OR(src, src, dst, FALSE);
}

inline void
ES_CodeGenerator::Move(short immediate, Register dst)
{
    ADDI(REG_ZERO, immediate, dst);
}
