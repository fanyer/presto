/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#ifdef ARCHITECTURE_SUPERH

class ES_CodeGenerator
{
public:
    ES_CodeGenerator();

    enum Condition
    {
        CONDITION_EQ, // equal (Z=1)
        CONDITION_NE, // not equal (Z=0)
        CONDITION_CS, // unsigned higher or same (C=1)
        CONDITION_CC, // unsigned lower (C=0)
        CONDITION_MI, // negative (N=1)
        CONDITION_PL, // positive or zero (N=0)
        CONDITION_VS, // overflow (V=1)
        CONDITION_VC, // not overflow (V=0)
        CONDITION_HI, // unsigned higher (C=1 and Z=0)
        CONDITION_LS, // unsigned lower or same (C=0 and Z=1)
        CONDITION_GE, // greater than or equal (V=N)
        CONDITION_LT, // less than (V!=N)
        CONDITION_GT, // greater than (Z=0 and V=N)
        CONDITION_LE, // less than or equal (Z=1 and V!=N)
        CONDITION_AL, // always
        CONDITION_NV // never
    };

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

        REG_SP = REG_R15
    };

    class Memory
    {
    public:
        enum Type
        {
            TYPE_PLAIN,
            /**< Base register, plain and simple. */
            TYPE_AUTO_ADJUST,
            /**< Base register is incremented before or decremented after
                 operation. */
            TYPE_R0_ADDED,
            /**< R0 is added to base register. */
            TYPE_DISPLACEMENT
            /**< Immediate displacement. */
        };

        explicit Memory(ES_CodeGenerator::Register base, Type type = TYPE_PLAIN) : base(base), type(type) {}
        explicit Memory(ES_CodeGenerator::Register base, char displacement) : base(base), type(TYPE_DISPLACEMENT), displacement(displacement) {}

    private:
        friend class ES_CodeGenerator;

        ES_CodeGenerator::Register base;
        Type type;
        int displacement;
    };

    enum OperandSize
    {
        OPSIZE_8,
        OPSIZE_16,
        OPSIZE_32
    };

    void MOV(char immediate, Register dst);
    void MOV(Register src, Register dst);
    void MOV(Memory src, Register dst, OperandSize opsize = OPSIZE_32);
    void MOV(Register dst, Memory src, OperandSize opsize = OPSIZE_32);

    void SUB(Register src, Register dst);
    /**< Subtracts 'src' from 'dst' and stores result in 'dst'. */

    void JSR(Register dst);
    void RTS();
    void NOP();

    void PushPR(); /**< STS.L PR, @-R15 */
    void PopPR();  /**< LDS.L @R15+, PR */

    unsigned char *GetBuffer() { return buffer_base; }
    unsigned GetBufferUsed() { return buffer - buffer_base; }

private:
    void Reserve();

    void Write(unsigned short codeword)
    {
        Reserve();
        *buffer++ = codeword & 0xffu;
        *buffer++ = (codeword >> 8) & 0xffu;
    }

    unsigned char *buffer_base, *buffer, *buffer_top;
};

#endif // ARCHITECTURE_SUPERH
