/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#ifdef ES_NATIVE_SUPPORT
#ifdef ARCHITECTURE_MIPS

#include "modules/ecmascript/carakan/src/util/es_codegenerator_mips.h"
#include "modules/ecmascript/carakan/src/util/es_codegenerator_base_functions.h"

#ifdef NATIVE_DISASSEMBLER_SUPPORT
#include "modules/ecmascript/carakan/src/util/es_native_disass.h"
#endif // NATIVE_DISASSEMBLER_SUPPORT

ES_CodeGenerator::ES_CodeGenerator(OpMemGroup *arena) :
    ES_CodeGenerator_Base(arena)
{
}

/* static */ ES_CodeGenerator::FPUType
ES_CodeGenerator::GetFPUType()
{
    unsigned int fir = 0;

#ifdef __GNUC__
    // read FIR register in FPU.
    asm ("cfc1 %0, $0" : "=r" (fir));
#else // __GNUC__
#error "Implement code to read FIR."
#endif // __GNUC__

#if _MIPS_FPSET == 32
    // Check F64 and D bits.
    if ((fir & 0x420000) == 0x420000)
        return FPU_DOUBLE;

    /* The code has been built for a 64-bit FPU but is running
       on a device with a 32-bit FPU, consider rebuilding with
       the correct settings. This assert is only a warning. */
    OP_ASSERT(!(fir & 0x20000));

#elif _MIPS_FPSET == 16
    // Check D bit.
    if (fir & 0x20000)
        return FPU_SINGLE;

#else // _MIPS_FPSET
#error "Unknown FPU register set."
#endif // _MIPS_FPSET

    return FPU_NONE;
}

static unsigned
InvertCondition(unsigned jump_condition)
{
    switch (jump_condition >> 26)
    {
    case MIPS::BEQ:
        return (MIPS::BNE << 26) | (jump_condition & 0x3ffffff);
    case MIPS::BNE:
        return (MIPS::BEQ << 26) | (jump_condition & 0x3ffffff);
    case MIPS::BLEZ:
        return (MIPS::BGTZ << 26) | (jump_condition & 0x3ffffff);
    case MIPS::BGTZ:
        return (MIPS::BLEZ << 26) | (jump_condition & 0x3ffffff);
    case MIPS::REGIMM:
        switch ((jump_condition >> 16) & 0x1f)
        {
        case MIPS::BLTZ:
            return (MIPS::BGEZ << 16) | (jump_condition & 0xffe0ffff);
        case MIPS::BGEZ:
            return (MIPS::BLTZ << 16) | (jump_condition & 0xffe0ffff);
        default:
            OP_ASSERT(FALSE);
            return (MIPS::SPECIAL << 26) | MIPS::BREAK;
        }
    case MIPS::COP1:
        OP_ASSERT(((jump_condition >> 21) & 0x1f) == MIPS::BC1);
        return (jump_condition & 0xfffeffff) | (~jump_condition & 0x1000);
    default:
        OP_ASSERT(FALSE);
        return (MIPS::SPECIAL << 26) | MIPS::BREAK;
    }
}

static unsigned
WriteJump(unsigned *write, unsigned *target, unsigned jump_condition, BOOL hint)
{
    unsigned delay_slot;

    if (hint)
    {
        write--;
        delay_slot = write[0];
    }
    else
        delay_slot = 0; // NOP

    int offset = target - write - 1;

    if (offset >= SHRT_MIN && offset <= SHRT_MAX)
    {
        write[0] = jump_condition | (offset & 0xffff);
        write[1] = delay_slot;

        return hint ? 1 : 2;
    }

    if (jump_condition == ES_NATIVE_UNCONDITIONAL)
    {
        OP_ASSERT(((INTPTR) write & 0xf0000000) == ((INTPTR) target & 0xf0000000));
        OP_ASSERT(((INTPTR) write & 0x0ffffffc) != 0x0ffffffc);

        write[0] = MIPS_J(MIPS::J, (INTPTR) target >> 2);
        write[1] = delay_slot;

        return hint ? 1 : 2;
    }

    OP_ASSERT(((INTPTR) (write + 2) & 0xf0000000) == ((INTPTR) target & 0xf0000000));
    OP_ASSERT(((INTPTR) (write + 2) & 0x0ffffffc) != 0x0ffffffc);

    write[0] = InvertCondition(jump_condition) | 3;
    write[1] = delay_slot;
    write[2] = MIPS_J(MIPS::J, (INTPTR) target >> 2);
    write[3] = 0; // NOP

    return hint ? 3 : 4;
}

unsigned
ES_CodeGenerator::CalculateMaximumDistance(Block *src, Block *dst)
{
    Block *block = src->next;
    unsigned distance = 0;

    while (block != dst)
    {
        unsigned block_length = block->end - block->start;

        if (block->jump_condition != ES_NATIVE_NOT_A_JUMP)
            block_length += block->jump_size;

        distance += block_length;

        if (distance > SHRT_MAX)
            return INT_MAX;

        block = block->next;
    }

    return distance;
}

const OpExecMemory*
ES_CodeGenerator::GetCode(OpExecMemoryManager *execmem, void *constant_reg)
{
    FinalizeBlockList();

    Block *block;

    unsigned size = 0;

    // Pass 0: Calculate max possible code size and optimize jumps to jumps.

    block = first_block;
    while (block)
    {
        size += block->end - block->start;

        if (block->jump_condition != ES_NATIVE_NOT_A_JUMP)
        {
            Block *target = block->jump_target;

            while (target->start == target->end)
            {
                if (target->jump_condition == ES_NATIVE_UNCONDITIONAL)
                    target = target->jump_target;
                else if (target->jump_condition == ES_NATIVE_NOT_A_JUMP)
                    target = target->next;
                else
                    break;
            }

            block->jump_target = target;

            if (block->jump_target == block->next)
                block->jump_condition = ES_NATIVE_NOT_A_JUMP;
            else
            {
                if (block->jump_condition == ES_NATIVE_UNCONDITIONAL)
                    block->jump_size = block->hint ? 1 : 2;
                else
                    block->jump_size = block->hint ? 3 : 4;

                size += block->jump_size;
            }
        }

        block = block->next;
    }

    BOOL large_function = size > SHRT_MAX;

    const OpExecMemory *result;
    unsigned *write_base;

    result = execmem->AllocateL(size * sizeof(ES_NativeInstruction));
    write_base = reinterpret_cast<unsigned *>(result->address);

    // The executable memory must not straddle a 256MB boundary in case the J instruction might be used.
    if (large_function && (((INTPTR) write_base & 0xf0000000) != ((INTPTR) (write_base + size + 1) & 0xf0000000)))
    {
        const OpExecMemory *result2;

        // Try allocating another block. The chance is slim that it too will straddle a boundary.
        result2 = execmem->AllocateL(size * sizeof(ES_NativeInstruction));
        write_base = reinterpret_cast<unsigned *>(result2->address);

        if (((INTPTR) write_base & 0xf0000000) != ((INTPTR) (write_base + size + 1) & 0xf0000000))
        {
            // Still no luck.
            OpExecMemoryManager::Free(result);
            OpExecMemoryManager::Free(result2);
            LEAVE(OpStatus::ERR_NO_MEMORY);
        }

        OpExecMemoryManager::Free(result);
        result = result2;
    }

    unsigned *write = write_base;

    unsigned constant_reg_hi, constant_reg_lo;

    constant_reg_hi = (INTPTR) constant_reg >> 16;
    constant_reg_lo = (INTPTR) constant_reg & 0xffff;

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

    // Pass 1: Copy code to destination and fill in base register loads, constant loads and
    //         backward jumps. Predict size of forward jumps.

    block = first_block;
    while (block)
    {
        unsigned count = block->end - block->start, *read = buffer_base + block->start;

        block->actual_start = write - write_base;

        for (unsigned index = 0; index < count; ++index)
            *write++ = *read++;

        Block::DataSectionPointer *load = block->first_data_section_pointer;

        while (load)
        {
            write_base[block->actual_start + load->offset] |= constant_reg_hi;
            write_base[block->actual_start + load->offset + 1] |= constant_reg_lo;
            load = load->next;
        }

        Constant::Use *use = block->first_constant_use;

        while (use)
        {
            OP_ASSERT(constant_reg);
            OP_ASSERT(use->constant->offset >= SHRT_MIN && use->constant->offset <= SHRT_MAX);

            write_base[block->actual_start + use->offset] |= (use->constant->offset & 0xffff);
            use = use->next_per_block;
        }

        if (block->jump_condition != ES_NATIVE_NOT_A_JUMP)
        {
            OP_ASSERT(!block->jump_is_call);

            if (block->jump_target->actual_start != UINT_MAX)
            {
                // Backward jump.
                write += WriteJump(write, write_base + block->jump_target->actual_start, block->jump_condition, block->hint);
                block->jump_condition = ES_NATIVE_NOT_A_JUMP;
            }
            else
            {
                // Forward jump.
                Block *source = block, *next = source->next;
                Block *target = block->jump_target;

                while (next != target && next->start == next->end && next->jump_condition == ES_NATIVE_NOT_A_JUMP)
                {
                    source = next;
                    next = source->next;
                }

                if (next != target)
                {
                    if (block->jump_condition == ES_NATIVE_UNCONDITIONAL)
                        write += block->hint ? 1 : 2;
                    else
                    {
                        block->jump_size = block->hint ? 1 : 2;

                        if (large_function)
                        {
                            unsigned distance = CalculateMaximumDistance(block, block->jump_target);

                            if (distance > SHRT_MAX)
                                block->jump_size += 2;
                        }

                        write += block->jump_size;
                    }

                    // Filter the list so that only forward jumps remain in pass 2.
                    *block_ptr = block;
                    block_ptr = &block->next;
                }
            }
        }

        block->actual_end = write - write_base;

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

    // Pass 2: Fill in forward jumps.

    *block_ptr = NULL;
    block = first_block;

    while (block)
    {
        OP_ASSERT(block->jump_condition != ES_NATIVE_NOT_A_JUMP);
        OP_ASSERT(block->jump_target->actual_start != block->actual_end);

        unsigned actual_size = WriteJump(write_base + block->actual_end - block->jump_size, write_base + block->jump_target->actual_start, block->jump_condition, block->hint);

        OP_ASSERT(actual_size <= block->jump_size);

        if (actual_size < block->jump_size)
        {
            for (unsigned i = actual_size; i < block->jump_size; i++)
                write_base[block->actual_end - i] = 0; // NOP
        }

        block = block->next;
    }

#ifdef NATIVE_DISASSEMBLER_SUPPORT
    if (disassemble)
    {
        nd_range->end = (write - write_base) * sizeof(ES_NativeInstruction);

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
#endif // NATIVE_DISASSEMBLER_SUPPORT

    return result;
}


// Convenience "assembler macros", may output multiple instructions and/or clobber REG_AT.

void
ES_CodeGenerator::LoadDataSectionPointer()
{
    Reserve();
    current_block->AddDataSectionPointerLoad(GetBufferUsed() - current_block->start, arena);
    LUI(REG_S6, 0);
    ORI(REG_S6, REG_S6, 0);
}

void
ES_CodeGenerator::Load(Register rt, Constant *constant)
{
    Reserve();
    constant->AddUse(current_block, GetBufferUsed() - current_block->start, arena);
    LW(rt, 0, REG_S6);
}

void
ES_CodeGenerator::Load(FPURegister ft, Constant *constant)
{
    Reserve();
    constant->AddUse(current_block, GetBufferUsed() - current_block->start, arena);
    LDC1(ft, 0, REG_S6);
}

void
ES_CodeGenerator::Load(Register rt, int immediate)
{
    if (immediate >= SHRT_MIN && immediate <= SHRT_MAX)
        ADDIU(rt, REG_ZERO, immediate);
    else if (immediate >= 0 && immediate <= USHRT_MAX)
        ORI(rt, REG_ZERO, immediate);
    else
    {
        LUI(rt, immediate >> 16);

        if (immediate & 0xffff)
            ORI(rt, rt, immediate & 0xffff);
    }
}

void
ES_CodeGenerator::Load(Register rt, int offset, Register base)
{
    if (offset >= SHRT_MIN && offset <= SHRT_MAX)
        LW(rt, offset, base);
    else
    {
        OP_ASSERT(base != REG_AT);

        LUI(REG_AT, offset >> 16);

        // TODO: When possible, put the low bits in the LW offset and eliminate the ORI.
        if (offset & 0xffff)
            ORI(REG_AT, REG_AT, offset);

        ADDU(REG_AT, base, REG_AT);
        LW(rt, 0, REG_AT);
    }
}

void
ES_CodeGenerator::Store(Register rt, int offset, Register base)
{
    if (offset >= SHRT_MIN && offset <= SHRT_MAX)
        SW(rt, offset, base);
    else
    {
        OP_ASSERT(base != REG_AT);

        LUI(REG_AT, offset >> 16);

        // TODO: When possible, put the low bits in the SW offset and eliminate the ORI.
        if (offset & 0xffff)
            ORI(REG_AT, REG_AT, offset);

        ADDU(REG_AT, base, REG_AT);
        SW(rt, 0, REG_AT);
    }
}

void
ES_CodeGenerator::Add(Register rt, Register rs, int immediate)
{
    if (immediate >= SHRT_MIN && immediate <= SHRT_MAX)
        ADDIU(rt, rs, immediate);
    else
    {
        OP_ASSERT(rs != REG_AT);
        LUI(REG_AT, immediate >> 16);

        if (immediate & 0xffff)
            ORI(REG_AT, REG_AT, immediate);

        ADDU(rt, rs, REG_AT);
    }
}

void
ES_CodeGenerator::And(Register rt, Register rs, unsigned immediate)
{
    if (immediate <= USHRT_MAX)
        ANDI(rt, rs, immediate);
    else
    {
        OP_ASSERT(rs != REG_AT);
        LUI(REG_AT, immediate >> 16);

        if (immediate & 0xffff)
            ORI(REG_AT, REG_AT, immediate);

        AND(rt, rs, REG_AT);
    }
}

void
ES_CodeGenerator::JumpLT(JumpTarget *target, Register rt, int immediate)
{
    if (immediate >= SHRT_MIN && immediate <= SHRT_MAX)
        SLTI(REG_AT, rt, immediate);
    else
    {
        OP_ASSERT(rt != REG_AT);
        Load(REG_AT, immediate);
        SLT(REG_AT, rt, REG_AT);
    }
    Jump(target, ES_NATIVE_CONDITION_NEZ(REG_AT));
}

void
ES_CodeGenerator::JumpLE(JumpTarget *target, Register rt, int immediate)
{
    if ((immediate + 1) >= SHRT_MIN && (immediate + 1) <= SHRT_MAX)
    {
        SLTI(REG_AT, rt, immediate + 1);
        Jump(target, ES_NATIVE_CONDITION_NEZ(REG_AT));
    }
    else
    {
        OP_ASSERT(rt != REG_AT);
        Load(REG_AT, immediate);
        SLT(REG_AT, REG_AT, rt);
        Jump(target, ES_NATIVE_CONDITION_EQZ(REG_AT));
    }
}

void
ES_CodeGenerator::JumpGT(JumpTarget *target, Register rt, int immediate)
{
    if ((immediate + 1) >= SHRT_MIN && (immediate + 1) <= SHRT_MAX)
    {
        SLTI(REG_AT, rt, immediate + 1);
        Jump(target, ES_NATIVE_CONDITION_EQZ(REG_AT));
    }
    else
    {
        OP_ASSERT(rt != REG_AT);
        Load(REG_AT, immediate);
        SLT(REG_AT, REG_AT, rt);
        Jump(target, ES_NATIVE_CONDITION_NEZ(REG_AT));
    }
}

void
ES_CodeGenerator::JumpGE(JumpTarget *target, Register rt, int immediate)
{
    if (immediate >= SHRT_MIN && immediate <= SHRT_MAX)
        SLTI(REG_AT, rt, immediate);
    else
    {
        OP_ASSERT(rt != REG_AT);
        Load(REG_AT, immediate);
        SLT(REG_AT, rt, REG_AT);
    }
    Jump(target, ES_NATIVE_CONDITION_EQZ(REG_AT));
}

void
ES_CodeGenerator::JumpLTU(JumpTarget *target, Register rt, unsigned immediate)
{
    if (immediate <= SHRT_MAX || immediate >= (UINT_MAX - SHRT_MAX))
        SLTIU(REG_AT, rt, immediate);
    else
    {
        OP_ASSERT(rt != REG_AT);
        Load(REG_AT, immediate);
        SLTU(REG_AT, rt, REG_AT);
    }
    Jump(target, ES_NATIVE_CONDITION_NEZ(REG_AT));
}

void
ES_CodeGenerator::JumpLEU(JumpTarget *target, Register rt, unsigned immediate)
{
    if (immediate < UINT_MAX && ((immediate + 1) <= SHRT_MAX || (immediate + 1) >= (UINT_MAX - SHRT_MAX)))
    {
        SLTIU(REG_AT, rt, immediate + 1);
        Jump(target, ES_NATIVE_CONDITION_NEZ(REG_AT));
    }
    else
    {
        OP_ASSERT(rt != REG_AT);
        Load(REG_AT, immediate);
        SLT(REG_AT, REG_AT, rt);
        Jump(target, ES_NATIVE_CONDITION_EQZ(REG_AT));
    }
}

void
ES_CodeGenerator::JumpGTU(JumpTarget *target, Register rt, unsigned immediate)
{
    if (immediate < UINT_MAX && ((immediate + 1) <= SHRT_MAX || (immediate + 1) >= (UINT_MAX - SHRT_MAX)))
    {
        SLTIU(REG_AT, rt, immediate + 1);
        Jump(target, ES_NATIVE_CONDITION_EQZ(REG_AT));
    }
    else
    {
        OP_ASSERT(rt != REG_AT);
        Load(REG_AT, immediate);
        SLTU(REG_AT, REG_AT, rt);
        Jump(target, ES_NATIVE_CONDITION_NEZ(REG_AT));
    }
}

void
ES_CodeGenerator::JumpGEU(JumpTarget *target, Register rt, unsigned immediate)
{
    if (immediate <= SHRT_MAX || immediate >= (UINT_MAX - SHRT_MAX))
        SLTIU(REG_AT, rt, immediate);
    else
    {
        OP_ASSERT(rt != REG_AT);
        Load(REG_AT, immediate);
        SLTU(REG_AT, rt, REG_AT);
    }
    Jump(target, ES_NATIVE_CONDITION_EQZ(REG_AT));
}

void
ES_CodeGenerator::Sub(Register rt, Register rs, int immediate)
{
    if (immediate >= (SHRT_MIN + 1) && immediate <= SHRT_MAX)
        ADDIU(rt, rs, -immediate);
    else
    {
        OP_ASSERT(rs != REG_AT);
        LUI(REG_AT, immediate >> 16);

        if (immediate & 0xffff)
            ORI(REG_AT, REG_AT, immediate);

        SUBU(rt, rs, REG_AT);
    }
}

// Simple one-to-one assembler instructions.

void
ES_CodeGenerator::ABS(FPUFormat fmt, FPURegister fd, FPURegister fs)
{
    Reserve();
    *buffer++ = MIPS_FR(fmt, 0, fs, fd, MIPS::ABS);
}

void
ES_CodeGenerator::ADD(FPUFormat fmt, FPURegister fd, FPURegister fs, FPURegister ft)
{
    Reserve();
    *buffer++ = MIPS_FR(fmt, ft, fs, fd, MIPS::F_ADD);
}

void
ES_CodeGenerator::ADDIU(Register rt, Register rs, short immediate)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::ADDIU, rs, rt, immediate);
}

void
ES_CodeGenerator::ADDU(Register rd, Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::ADDU);
}

void
ES_CodeGenerator::AND(Register rd, Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::AND);
}

void
ES_CodeGenerator::ANDI(Register rt, Register rs, unsigned short immediate)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::ANDI, rs, rt, immediate);
}

void
ES_CodeGenerator::BC1F(short offset, int cc)
{
    Reserve();
    *buffer++ = MIPS_FB(cc, 0, 0, offset);
}

void
ES_CodeGenerator::BC1T(short offset, int cc)
{
    Reserve();
    *buffer++ = MIPS_FB(cc, 0, 1, offset);
}

void
ES_CodeGenerator::BEQ(Register rt, Register rs, short offset)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::BEQ, rs, rt, offset);
}

void
ES_CodeGenerator::BNE(Register rt, Register rs, short offset)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::BNE, rs, rt, offset);
}

void
ES_CodeGenerator::BREAK(unsigned code)
{
    Reserve();
    *buffer++ = (MIPS::SPECIAL << 26) | ((code & 0xfffff) << 6) | MIPS::BREAK;
}

void
ES_CodeGenerator::C(FPUCondition cond, FPUFormat fmt, FPURegister fs, FPURegister ft, int cc)
{
    Reserve();
    *buffer++ = MIPS_FR(fmt, ft, fs, cc << 2, 0x30 | cond);
}

void
ES_CodeGenerator::CEIL(FPUFormat dfmt, FPUFormat sfmt, FPURegister fd, FPURegister fs)
{
    OP_ASSERT(dfmt == L || dfmt == W);
    Reserve();
    *buffer++ = MIPS_FR(sfmt, 0, fs, fd, dfmt == L ? MIPS::CEIL_L : MIPS::CEIL_W);
}

void
ES_CodeGenerator::CFC1(Register rt, FPUControlReg fs)
{
    Reserve();
    *buffer++ = MIPS_FT(MIPS::CFC1, rt, fs);
}

void
ES_CodeGenerator::CVT(FPUFormat dfmt, FPUFormat sfmt, FPURegister fd, FPURegister fs)
{
    Reserve();
    *buffer++ = MIPS_FR(sfmt, 0, fs, fd, MIPS::CVT_S + dfmt);
}

void
ES_CodeGenerator::DIV(Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, 0, 0, MIPS::DIV);
}

void
ES_CodeGenerator::DIV(FPUFormat fmt, FPURegister fd, FPURegister fs, FPURegister ft)
{
    Reserve();
    *buffer++ = MIPS_FR(fmt, ft, fs, fd, MIPS::F_DIV);
}

void
ES_CodeGenerator::FLOOR(FPUFormat dfmt, FPUFormat sfmt, FPURegister fd, FPURegister fs)
{
    OP_ASSERT(dfmt == L || dfmt == W);
    Reserve();
    *buffer++ = MIPS_FR(sfmt, 0, fs, fd, dfmt == L ? MIPS::FLOOR_L : MIPS::FLOOR_W);
}

void
ES_CodeGenerator::JALR(Register rs, Register rd)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, 0, rd, 0, MIPS::JALR);
}

void
ES_CodeGenerator::JR(Register rs)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, 0, 0, 0, MIPS::JR);
}

void
ES_CodeGenerator::LB(Register rt, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::LB, base, rt, offset);
}

void
ES_CodeGenerator::LBU(Register rt, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::LBU, base, rt, offset);
}

void
ES_CodeGenerator::LDC1(FPURegister ft, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::LDC1, base, ft, offset);
}

void
ES_CodeGenerator::LH(Register rt, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::LH, base, rt, offset);
}

void
ES_CodeGenerator::LHU(Register rt, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::LHU, base, rt, offset);
}

void
ES_CodeGenerator::LUI(Register rt, unsigned short immediate)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::LUI, 0, rt, immediate);
}

void
ES_CodeGenerator::LW(Register rt, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::LW, base, rt, offset);
}

void
ES_CodeGenerator::LWC1(FPURegister ft, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::LWC1, base, ft, offset);
}

void
ES_CodeGenerator::MFC1(Register rt, FPURegister fs)
{
    Reserve();
    *buffer++ = MIPS_FT(MIPS::MFC1, rt, fs);
}

void
ES_CodeGenerator::MFHC1(Register rt, FPURegister fs)
{
    Reserve();
    *buffer++ = MIPS_FT(MIPS::MFHC1, rt, fs);
}

void
ES_CodeGenerator::MFHI(Register rd)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, 0, 0, rd, 0, MIPS::MFHI);
}

void
ES_CodeGenerator::MFLO(Register rd)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, 0, 0, rd, 0, MIPS::MFLO);
}

void
ES_CodeGenerator::MOV(FPUFormat fmt, FPURegister fd, FPURegister fs)
{
    Reserve();
    *buffer++ = MIPS_FR(fmt, 0, fs, fd, MIPS::MOV);
}

void
ES_CodeGenerator::MOVF(Register rd, Register rs, int cc)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, cc << 2, rd, 0, MIPS::MOVCI);
}

void
ES_CodeGenerator::MOVN(Register rd, Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::MOVN);
}

void
ES_CodeGenerator::MOVT(Register rd, Register rs, int cc)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, (cc << 2) | 1, rd, 0, MIPS::MOVCI);
}

void
ES_CodeGenerator::MOVZ(Register rd, Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::MOVZ);
}

void
ES_CodeGenerator::MTC1(Register rt, FPURegister fs)
{
    Reserve();
    *buffer++ = MIPS_FT(MIPS::MTC1, rt, fs);
}

void
ES_CodeGenerator::MTHC1(Register rt, FPURegister fs)
{
    Reserve();
    *buffer++ = MIPS_FT(MIPS::MTHC1, rt, fs);
}

void
ES_CodeGenerator::MUL(FPUFormat fmt, FPURegister fd, FPURegister fs, FPURegister ft)
{
    Reserve();
    *buffer++ = MIPS_FR(fmt, ft, fs, fd, MIPS::F_MUL);
}

void
ES_CodeGenerator::MULT(Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, 0, 0, MIPS::MULT);
}

void
ES_CodeGenerator::NEG(FPUFormat fmt, FPURegister fd, FPURegister fs)
{
    Reserve();
    *buffer++ = MIPS_FR(fmt, 0, fs, fd, MIPS::NEG);
}

void
ES_CodeGenerator::NOP()
{
    Reserve();
    *buffer++ = 0; // MIPS_R(MIPS::SPECIAL, 0, 0, 0, 0, MIPS::SLL);
}

void
ES_CodeGenerator::NOR(Register rd, Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::NOR);
}

void
ES_CodeGenerator::OR(Register rd, Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::OR);
}

void
ES_CodeGenerator::ORI(Register rt, Register rs, unsigned short immediate)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::ORI, rs, rt, immediate);
}

void
ES_CodeGenerator::SB(Register rt, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::SB, base, rt, offset);
}

void
ES_CodeGenerator::SDC1(FPURegister ft, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::SDC1, base, ft, offset);
}

void
ES_CodeGenerator::SH(Register rt, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::SH, base, rt, offset);
}

void
ES_CodeGenerator::SLL(Register rd, Register rt, unsigned sa)
{
    OP_ASSERT(sa < 32);

    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, 0, rt, rd, sa, MIPS::SLL);
}

void
ES_CodeGenerator::SLLV(Register rd, Register rt, Register rs)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::SLLV);
}

void
ES_CodeGenerator::SLT(Register rd, Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::SLT);
}

void
ES_CodeGenerator::SLTI(Register rt, Register rs, short immediate)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::SLTI, rs, rt, immediate);
}

void
ES_CodeGenerator::SLTIU(Register rt, Register rs, unsigned short immediate)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::SLTIU, rs, rt, immediate);
}

void
ES_CodeGenerator::SLTU(Register rd, Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::SLTU);
}

void
ES_CodeGenerator::SQRT(FPUFormat fmt, FPURegister fd, FPURegister fs)
{
    Reserve();
    *buffer++ = MIPS_FR(fmt, 0, fs, fd, MIPS::SQRT);
}

void
ES_CodeGenerator::SRA(Register rd, Register rt, int sa)
{
    OP_ASSERT(sa < 32);
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, 0, rt, rd, sa, MIPS::SRA);
}

void
ES_CodeGenerator::SRAV(Register rd, Register rt, Register rs)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::SRAV);
}

void
ES_CodeGenerator::SRL(Register rd, Register rt, int sa)
{
    OP_ASSERT(sa < 32);
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, 0, rt, rd, sa, MIPS::SRL);
}

void
ES_CodeGenerator::SRLV(Register rd, Register rt, Register rs)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::SRLV);
}

void
ES_CodeGenerator::SUB(FPUFormat fmt, FPURegister fd, FPURegister fs, FPURegister ft)
{
    Reserve();
    *buffer++ = MIPS_FR(fmt, ft, fs, fd, MIPS::F_SUB);
}

void
ES_CodeGenerator::SUBU(Register rd, Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::SUBU);
}

void
ES_CodeGenerator::SW(Register rt, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::SW, base, rt, offset);
}

void
ES_CodeGenerator::SWC1(FPURegister ft, short offset, Register base)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::SWC1, base, ft, offset);
}

void
ES_CodeGenerator::ROUND(FPUFormat dfmt, FPUFormat sfmt, FPURegister fd, FPURegister fs)
{
    OP_ASSERT(dfmt == L || dfmt == W);
    Reserve();
    *buffer++ = MIPS_FR(sfmt, 0, fs, fd, dfmt == L ? MIPS::ROUND_L : MIPS::ROUND_W);
}

void
ES_CodeGenerator::TRUNC(FPUFormat dfmt, FPUFormat sfmt, FPURegister fd, FPURegister fs)
{
    OP_ASSERT(dfmt == L || dfmt == W);
    Reserve();
    *buffer++ = MIPS_FR(sfmt, 0, fs, fd, dfmt == L ? MIPS::TRUNC_L : MIPS::TRUNC_W);
}

void
ES_CodeGenerator::XOR(Register rd, Register rs, Register rt)
{
    Reserve();
    *buffer++ = MIPS_R(MIPS::SPECIAL, rs, rt, rd, 0, MIPS::XOR);
}

void
ES_CodeGenerator::XORI(Register rt, Register rs, unsigned short immediate)
{
    Reserve();
    *buffer++ = MIPS_I(MIPS::XORI, rs, rt, immediate);
}

#endif // ARCHITECTURE_MIPS
#endif // ES_NATIVE_SUPPORT
