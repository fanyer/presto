/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

/* DataProcessing */

inline void
ES_CodeGenerator::NOP()
{
    MOV(REG_R0, REG_R0);
}

inline void
ES_CodeGenerator::AND(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_AND, op1, op2, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::EOR(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_EOR, op1, op2, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::SUB(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_SUB, op1, op2, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::RSB(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_RSB, op1, op2, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::ADD(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_ADD, op1, op2, dst, set_condition_codes, condition);
}

#ifdef ARCHITECTURE_ARM

ARM_INLINE void
ES_CodeGenerator::AddToSP(unsigned amount)
{
    ADD(REG_SP, amount, REG_SP);
}

ARM_INLINE void
ES_CodeGenerator::AddToSP(unsigned amount, Register dst)
{
    ADD(REG_SP, amount, dst);
}

ARM_INLINE void
ES_CodeGenerator::AddToPC(unsigned amount, Register dst)
{
    ADD(REG_PC, amount, dst);
}

#endif // ARCHITECTURE_ARM

inline void
ES_CodeGenerator::ADC(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_ADC, op1, op2, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::SBC(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_SBC, op1, op2, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::RSC(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_RSC, op1, op2, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::TST(Register op1, const Operand &op2, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_TST, op1, op2, REG_R0L, SET_CONDITION_CODES, condition);
}

inline void
ES_CodeGenerator::TEQ(Register op1, const Operand &op2, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_TEQ, op1, op2, REG_R0L, SET_CONDITION_CODES, condition);
}

inline void
ES_CodeGenerator::CMP(Register op1, const NegOperand &op2, ES_NativeJumpCondition condition)
{
    DataProcessingOpCode opcode;

    if (!op2.is_register && op2.negated)
        opcode = OPCODE_CMN;
    else
        opcode = OPCODE_CMP;

    DataProcessing(opcode, op1, op2, REG_R0L, SET_CONDITION_CODES, condition);
}

inline void
ES_CodeGenerator::CMN(Register op1, const Operand &op2, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_CMN, op1, op2, REG_R0L, SET_CONDITION_CODES, condition);
}

inline void
ES_CodeGenerator::ORR(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_ORR, op1, op2, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::MOV(const NotOperand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessingOpCode opcode;

    if (!op2.is_register && op2.inverted)
        opcode = OPCODE_MVN;
    else
        opcode = OPCODE_MOV;

    DataProcessing(opcode, REG_R0L, op2, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::BIC(Register op1, const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_BIC, op1, op2, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::MVN(const Operand &op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    DataProcessing(OPCODE_MVN, REG_R0L, op2, dst, set_condition_codes, condition);
}

/* Multiply */

inline void
ES_CodeGenerator::MUL(Register op1, Register op2, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    Multiply(FALSE, op1, op2, REG_R0L, dst, set_condition_codes, condition);
}

inline void
ES_CodeGenerator::MLA(Register op1, Register op2, Register acc, Register dst, SetConditionCodes set_condition_codes, ES_NativeJumpCondition condition)
{
    Multiply(TRUE, op1, op2, acc, dst, set_condition_codes, condition);
}

/* static */ inline BOOL
ES_CodeGenerator::SupportedOffset(int offset, LoadStoreType type)
{
    if (type == LOAD_STORE_WORD || type == LOAD_STORE_UNSIGNED_BYTE)
        return offset >= -4095 && offset <= 4095;
    else if (type == LOAD_STORE_DOUBLE)
        return offset >= -1023 && offset <= 1023;
    else
        return offset >= -255 && offset <= 255;
}

inline void
ES_CodeGenerator::LDRB(Register base, int offset, Register dst, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    SingleDataTransfer(TRUE, write_back, OPSIZE_8, offset >= 0, pre_indexing, base, FALSE, offset < 0 ? -offset : offset, REG_R0L, SHIFT_LOGICAL_LEFT, 0, dst, condition);
}

inline void
ES_CodeGenerator::LDRB(Register base, Register offset, ShiftType shift_type, unsigned shift_amount, BOOL up, Register dst, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    SingleDataTransfer(TRUE, write_back, OPSIZE_8, up, pre_indexing, base, TRUE, 0, offset, shift_type, shift_amount, dst, condition);
}

inline void
ES_CodeGenerator::LDR(Register base, int offset, Register dst, ES_NativeJumpCondition condition)
{
    SingleDataTransfer(TRUE, FALSE, OPSIZE_32, offset >= 0, TRUE, base, FALSE, offset < 0 ? -offset : offset, REG_R0L, SHIFT_LOGICAL_LEFT, 0, dst, condition);
}

inline void
ES_CodeGenerator::LDR(Register base, int offset, Register dst, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    SingleDataTransfer(TRUE, write_back, OPSIZE_32, offset >= 0, pre_indexing, base, FALSE, offset < 0 ? -offset : offset, REG_R0L, SHIFT_LOGICAL_LEFT, 0, dst, condition);
}

inline void
ES_CodeGenerator::LDR(Register base, Register offset, ShiftType shift_type, unsigned shift_amount, BOOL up, Register dst, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    SingleDataTransfer(TRUE, write_back, OPSIZE_32, up, pre_indexing, base, TRUE, 0, offset, shift_type, shift_amount, dst, condition);
}

inline void
ES_CodeGenerator::STRB(Register src, Register base, int offset, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    SingleDataTransfer(FALSE, write_back, OPSIZE_8, offset >= 0, pre_indexing, base, FALSE, offset < 0 ? -offset : offset, REG_R0L, SHIFT_LOGICAL_LEFT, 0, src, condition);
}

inline void
ES_CodeGenerator::STRB(Register src, Register base, Register offset, ShiftType shift_type, unsigned shift_amount, BOOL up, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    SingleDataTransfer(FALSE, write_back, OPSIZE_8, up, pre_indexing, base, TRUE, 0, offset, shift_type, shift_amount, src, condition);
}

inline void
ES_CodeGenerator::STRH(Register src, Register base, int offset, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    HalfWordDataTransfer(FALSE, write_back, offset >= 0, pre_indexing, base, FALSE, offset < 0 ? -offset : offset, REG_R0L, src, condition);
}

inline void
ES_CodeGenerator::STRH(Register src, Register base, Register offset, BOOL up, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    HalfWordDataTransfer(FALSE, write_back, up, pre_indexing, base, TRUE, 0, offset, src, condition);
}

inline void
ES_CodeGenerator::STR(Register src, Register base, int offset, ES_NativeJumpCondition condition)
{
    SingleDataTransfer(FALSE, FALSE, OPSIZE_32, offset >= 0, TRUE, base, FALSE, offset < 0 ? -offset : offset, REG_R0L, SHIFT_LOGICAL_LEFT, 0, src, condition);
}

inline void
ES_CodeGenerator::STR(Register src, Register base, int offset, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    SingleDataTransfer(FALSE, write_back, OPSIZE_32, offset >= 0, pre_indexing, base, FALSE, offset < 0 ? -offset : offset, REG_R0L, SHIFT_LOGICAL_LEFT, 0, src, condition);
}

inline void
ES_CodeGenerator::STR(Register src, Register base, Register offset, ShiftType shift_type, unsigned shift_amount, BOOL up, BOOL write_back, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    SingleDataTransfer(FALSE, write_back, OPSIZE_32, up, pre_indexing, base, TRUE, 0, offset, shift_type, shift_amount, src, condition);
}

inline void
ES_CodeGenerator::LDM(Register base, unsigned registers, BOOL write_back, BOOL up, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    BlockDataTransfer(TRUE, write_back, up, pre_indexing, base, registers, condition);
}

inline void
ES_CodeGenerator::STM(Register base, unsigned registers, BOOL write_back, BOOL up, BOOL pre_indexing, ES_NativeJumpCondition condition)
{
    BlockDataTransfer(FALSE, write_back, up, pre_indexing, base, registers, condition);
}

inline void
ES_CodeGenerator::PUSH(Register src, ES_NativeJumpCondition condition)
{
    STR(src, REG_SP, -4, TRUE, TRUE, condition);
}

inline void
ES_CodeGenerator::POP(Register dst, ES_NativeJumpCondition condition)
{
    LDR(REG_SP, 4, dst, TRUE, FALSE, condition);
}

#ifdef ARCHITECTURE_ARM

ARM_INLINE void
ES_CodeGenerator::PUSH(unsigned registers, ES_NativeJumpCondition condition)
{
    BlockDataTransfer(FALSE, TRUE, FALSE, TRUE, REG_SP, registers, condition);
}

ARM_INLINE void
ES_CodeGenerator::POP(unsigned registers, ES_NativeJumpCondition condition)
{
    BlockDataTransfer(TRUE, TRUE, TRUE, FALSE, REG_SP, registers, condition);
}

#endif // ARCHITECTURE_ARM
