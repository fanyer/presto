/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"

#ifdef ES_NATIVE_SUPPORT
#ifdef ARCHITECTURE_IA32

#include "modules/ecmascript/carakan/src/util/es_codegenerator_ia32.h"
#include "modules/ecmascript/carakan/src/util/es_codegenerator_base_functions.h"

#ifdef NATIVE_DISASSEMBLER_SUPPORT
#include "modules/ecmascript/carakan/src/util/es_native_disass.h"
#endif // NATIVE_DISASSEMBLER_SUPPORT

#ifdef PRETEND_NO_FPU
# define FPU_OPERATION() \
    OP_ASSERT(!"Invalid FPU operation emitted!"); \
    *((int *) NULL) = 10
#else // PRETEND_NO_FPU
# define FPU_OPERATION()
#endif // PRETEND_NO_FPU

enum
{
    OP_RM_R,
    OP_R_RM,
    OP_RM32_IMM8,
    OP_RM32_IMM8_REG,
    OP_RM32_IMM32,
    OP_RM32_IMM32_REG,
    OP_RM8_IMM8,
    OP_RM8_IMM8_REG
};

unsigned char OPs_MOV[] = { 0x8b, 0x89, 0, 0, 0xc7, 0, 0xc6, 0 };
unsigned char OPs_MOVQ[] = { 0x6e, 0x7e, 0, 0, 0, 0, 0, 0 };
unsigned char OPs_ADD[] = { 0x03, 0x01, 0x83, 0, 0x81, 0, 0x80, 0 };
unsigned char OPs_ADC[] = { 0x13, 0x11, 0x83, 2, 0x81, 2, 0x80, 2 };
unsigned char OPs_SUB[] = { 0x2b, 0x29, 0x83, 5, 0x81, 5, 0x80, 5 };
unsigned char OPs_SBB[] = { 0x1b, 0x19, 0x83, 3, 0x81, 3, 0x80, 3 };
unsigned char OPs_LEA[] = { 0, 0x8d, 0, 0, 0, 0, 0, 0 };
unsigned char OPs_AND[] = { 0x23, 0x21, 0x83, 4, 0x81, 4, 0x80, 4 };
unsigned char OPs_OR[] = { 0x0b, 0x09, 0x83, 1, 0x81, 1, 0x80, 1 };
unsigned char OPs_XOR[] = { 0x33, 0x31, 0x83, 6, 0x81, 6, 0x80, 6 };
unsigned char OPs_CMP[] = { 0x3b, 0x39, 0x83, 7, 0x81, 7, 0x80, 7 };
unsigned char OPs_TEST[] = { 0x85, 0x85, 0, 0, 0xf7, 0, 0, 0 };
unsigned char OPs_SHL[] = { 0, 0, 0xc1, 4, 0, 0, 0, 0 };
unsigned char OPs_SHL1[] = { 0, 0, 0xd1, 4, 0, 0, 0, 0 };
unsigned char OPs_SHLCL[] = { 0, 0, 0xd3, 4, 0, 0, 0, 0 };
unsigned char OPs_SAR[] = { 0, 0, 0xc1, 7, 0, 0, 0, 0 };
unsigned char OPs_SAR1[] = { 0, 0, 0xd1, 7, 0, 0, 0, 0 };
unsigned char OPs_SARCL[] = { 0, 0, 0xd3, 7, 0, 0, 0, 0 };
unsigned char OPs_SHR[] = { 0, 0, 0xc1, 5, 0, 0, 0, 0 };
unsigned char OPs_SHR1[] = { 0, 0, 0xd1, 5, 0, 0, 0, 0 };
unsigned char OPs_SHRCL[] = { 0, 0, 0xd3, 5, 0, 0, 0, 0 };
unsigned char OPs_IMUL[] = { 0xaf, 0, 0, 0, 0, 0, 0, 0 };

#ifdef ARCHITECTURE_AMD64

static unsigned char
CalculateREX(const ES_CodeGenerator::Operand &rm, const ES_CodeGenerator::Operand &reg, ES_CodeGenerator::OperandSize size)
{
    unsigned char REX = 0x40;

    if (size == ES_CodeGenerator::OPSIZE_64)
        REX |= 8;

    if (reg.base != ES_CodeGenerator::REG_NONE)
        REX |= (reg.base & 8) >> 1;
#ifdef ARCHITECTURE_SSE
    else if (reg.type == ES_CodeGenerator::Operand::TYPE_XMM)
        REX |= (reg.xmm & 8) >> 1;
#endif // ARCHITECTURE_SSE

    if (rm.index != ES_CodeGenerator::REG_NONE)
        REX |= (rm.index & 8) >> 2;

    if (rm.base != ES_CodeGenerator::REG_NONE)
        REX |= (rm.base & 8) >> 3;
#ifdef ARCHITECTURE_SSE
    else if (rm.type == ES_CodeGenerator::Operand::TYPE_XMM)
        REX |= (rm.xmm & 8) >> 3;
#endif // ARCHITECTURE_SSE

    return REX;
}

#endif // ARCHITECTURE_AMD64

static unsigned char
CalculateModRM(const ES_CodeGenerator::Operand &rm, const ES_CodeGenerator::Operand &reg)
{
    unsigned char ModRM;

    if (reg.type == ES_CodeGenerator::Operand::TYPE_REGISTER)
        ModRM = (reg.base & 7) << 3;
#ifdef ARCHITECTURE_SSE
    else if (reg.type == ES_CodeGenerator::Operand::TYPE_XMM)
        ModRM = (reg.xmm & 7) << 3;
#endif // ARCHITECTURE_SSE
    else
        ModRM = (reg.reg) << 3;

    if (rm.type == ES_CodeGenerator::Operand::TYPE_REGISTER)
        ModRM |= (3 << 6) | (rm.base & 7);
#ifdef ARCHITECTURE_SSE
    else if (rm.type == ES_CodeGenerator::Operand::TYPE_XMM)
        ModRM |= (3 << 6) | (rm.xmm & 7);
#endif // ARCHITECTURE_SSE
    else if (rm.UseSIB())
        ModRM |= 4;
    else if (rm.base == ES_CodeGenerator::REG_NONE)
        ModRM |= 5;
    else
        ModRM |= (rm.base & 7);

    // displacement (only 32-bit) without base uses Mod=0 (which usually means "no displacement")
    if (rm.base != ES_CodeGenerator::REG_NONE)
        if (rm.UseDisp8())
            ModRM |= 1 << 6;
        else if (rm.UseDisp32())
            ModRM |= 2 << 6;

    return ModRM;
}

static unsigned char
CalculateSIB(const ES_CodeGenerator::Operand &rm)
{
    unsigned char SIB = rm.scale << 6;

    if (rm.base == ES_CodeGenerator::REG_NONE)
        SIB |= ES_CodeGenerator::REG_BP;
    else
        SIB |= (rm.base & 7);

    if (rm.index == ES_CodeGenerator::REG_NONE)
        SIB |= ES_CodeGenerator::REG_SP << 3;
    else
        SIB |= (rm.index & 7) << 3;

    return SIB;
}

#ifdef ARCHITECTURE_AMD64

inline BOOL
ES_CodeGenerator::Operand::UseREX() const
{
    switch (type)
    {
    case TYPE_REGISTER:
        return (base & 8) != 0;

#ifdef ARCHITECTURE_SSE
    case TYPE_XMM:
        return (xmm & 8) != 0;
#endif // ARCHITECTURE_SSE

    case TYPE_MEMORY:
        return (base & 8) != 0 || (index & 8) != 0;

    default:
        return FALSE;
    }
}

#endif // ARCHITECTURE_AMD64

#ifndef ARCHITECTURE_AMD64
#if defined _MSC_VER && _MSC_VER < 1700
extern "C" BOOL __sse2_available;
#else // _MSC_VER
const unsigned char cv_SupportsSSE2[] = {
    0x50 + ES_CodeGenerator::REG_BX,            // push   $ebx
    0xb8, 0x01, 0x00, 0x00, 0x00,               // mov    $0x1,%eax
    0x0f, 0xa2,                                 // cpuid
    0x58 + ES_CodeGenerator::REG_BX,            // pop    $ebx
    0xf7, 0xc2, 0x00, 0x00, 0x00, 0x04,         // test   $0x4000000,%edx
    0x74, 0x06,                                 // je     +6
    0xb8, 0x01, 0x00, 0x00, 0x00,               // mov    $0x1,%eax
    0xc3,                                       // ret
    0x33, 0xc0,                                 // xor    %eax,%eax
    0xc3                                        // ret
};
#endif // _MSC_VER
#endif // ARCHITECTURE_AMD64

const unsigned char cv_SupportsSSE41[] = {
    0x50 + ES_CodeGenerator::REG_BX,            // push   $ebx
    0xb8, 0x01, 0x00, 0x00, 0x00,               // mov    $0x1,%eax
    0x0f, 0xa2,                                 // cpuid
    0x58 + ES_CodeGenerator::REG_BX,            // pop    $ebx
    0xf7, 0xc1, 0x00, 0x00, 0x08, 0x00,         // test   $0x80000,%ecx
    0x74, 0x06,                                 // je     +6
    0xb8, 0x01, 0x00, 0x00, 0x00,               // mov    $0x1,%eax
    0xc3,                                       // ret
    0x33, 0xc0,                                 // xor    %eax,%eax
    0xc3                                        // ret
};

#ifdef ES_OVERRIDE_FPMODE
# include "modules/prefs/prefsmanager/collections/pc_js.h"
#endif // ES_OVERRIDE_FPMODE

#if !defined PRETEND_NO_FPU && !defined CONSTANT_DATA_IS_EXECUTABLE
static void CreateSSECheckFunctionL(const OpExecMemory *&block, const unsigned char* machine_code, size_t machine_code_size)
{
    block = g_executableMemory->AllocateL(machine_code_size);
    op_memcpy(block->address, machine_code, machine_code_size);
    OpExecMemoryManager::FinalizeL(block);
}

static OP_STATUS CreateSSECheckFunction(const OpExecMemory *&block, const unsigned char* machine_code, size_t machine_code_size)
{
    TRAPD(status, CreateSSECheckFunctionL(block, machine_code, machine_code_size));
    return status;
}

static OP_STATUS SetupSSECheckFunction(const OpExecMemory *&block, const unsigned char* machine_code, size_t machine_code_size)
{
    if (OpStatus::IsError(CreateSSECheckFunction(block, machine_code, machine_code_size)))
    {
        if (block)
        {
            g_executableMemory->Free(block);
            block = NULL;
        }
        return OpStatus::ERR_NO_MEMORY;
    }
    return OpStatus::OK;
}
#endif // !PRETEND_NO_FPU && !CONSTANT_DATA_IS_EXECUTABLE

/* static */ BOOL
ES_CodeGenerator::SupportsSSE2()
{
#ifdef PRETEND_NO_FPU
    return FALSE;
#elif defined(ARCHITECTURE_AMD64)
    return TRUE;
#else
# ifdef ES_OVERRIDE_FPMODE
    if (g_pcjs->GetStringPref(PrefsCollectionJS::FPMode).CompareI("x87") == 0)
        return FALSE;
# endif // ES_OVERRIDE_FPMODE

# if defined _MSC_VER && _MSC_VER < 1700
    return __sse2_available;
# else // _MSC_VER
    union Cast
    {
        const void *ptr;
        BOOL (*fn)();
    } cast;

#  ifdef CONSTANT_DATA_IS_EXECUTABLE
    cast.ptr = cv_SupportsSSE2;
#  else // CONSTANT_DATA_IS_EXECUTABLE
    static const OpExecMemory *&g_block = g_ecma_sse2_block;

    if (!g_block && OpStatus::IsError(SetupSSECheckFunction(g_block, cv_SupportsSSE2, sizeof cv_SupportsSSE2)))
        return FALSE;

    cast.ptr = static_cast<unsigned char *>(g_block->address);
#  endif // CONSTANT_DATA_IS_EXECUTABLE

    return cast.fn();
# endif // _MSC_VER
#endif // ES_OVERRIDE_FPMODE
}

/* static */ BOOL
ES_CodeGenerator::SupportsSSE41()
{
#ifdef PRETEND_NO_FPU
    return FALSE;
#else
    union Cast
    {
        const unsigned char *ptr;
        BOOL (*fn)();
    } cast;

# ifdef CONSTANT_DATA_IS_EXECUTABLE
    cast.ptr = cv_SupportsSSE41;
# else // CONSTANT_DATA_IS_EXECUTABLE
    static const OpExecMemory *&g_block = g_ecma_sse41_block;

    if (!g_block && OpStatus::IsError(SetupSSECheckFunction(g_block, cv_SupportsSSE41, sizeof cv_SupportsSSE41)))
        return FALSE;

    cast.ptr = static_cast<unsigned char *>(g_block->address);
# endif // CONSTANT_DATA_IS_EXECUTABLE

    return cast.fn();
#endif // PRETEND_NO_FPU
}

ES_CodeGenerator::ES_CodeGenerator(OpMemGroup *arena0)
    : ES_CodeGenerator_Base(arena0)
#ifdef NATIVE_DISASSEMBLER_SUPPORT
    , disassemble(FALSE)
#endif // NATIVE_DISASSEMBLER_SUPPORT
    , supports_SSE2(SupportsSSE2())
    , supports_SSE41(SupportsSSE41())
#ifdef _DEBUG
    , dump_array(FALSE)
#endif // _DEBUG
{
}

#ifdef NATIVE_DISASSEMBLER_SUPPORT

void
ES_CodeGenerator::EnableDisassemble()
{
    extern FILE *g_native_disassembler_file;

    if (g_native_disassembler_file)
        disassemble = TRUE;
}

#endif // NATIVE_DISASSEMBLER_SUPPORT

ES_CodeGenerator::~ES_CodeGenerator()
{
}

#ifdef _DEBUG
void
ES_CodeGenerator::NOP()
{
    Reserve();

    WriteByte(0x90);
}

void
ES_CodeGenerator::INT3()
{
    Reserve();

    WriteByte(0xcc);
}
#endif

void
ES_CodeGenerator::MOV(const Operand &source, const Operand &target, OperandSize size)
{
    if (source.type == Operand::TYPE_REGISTER && target.type == Operand::TYPE_REGISTER && source.base == target.base && size == OPSIZE_POINTER)
        return;

    if (source.type == Operand::TYPE_IMMEDIATE || source.type == Operand::TYPE_IMMEDIATE64)
    {
        if (source.pointer != 0 && target.type == Operand::TYPE_REGISTER)
        {
            Reserve();

#ifdef ARCHITECTURE_AMD64
            if (reinterpret_cast<UINTPTR>(source.pointer) > UINT_MAX)
                size = OPSIZE_64;
            else
                size = OPSIZE_32;

            if (target.UseREX() || size == OPSIZE_64)
                WriteByte(CalculateREX(target, Operand(REG_NONE), size));
#endif // ARCHITECTURE_AMD64

            WriteByte(0xb8 + (target.base & 7));
            Write(source.pointer);

            return;
        }

#if 0
        /* This is not safe; MOV doesn't touch EFLAGS but XOR does, and the
           caller might rely on the fact that EFLAGS is left alone. */
        if (source.immediate == 0 && target.type == Operand::TYPE_REGISTER)
        {
            XOR(target, target, size);
            return;
        }
#endif // 0

        if (target.type == Operand::TYPE_REGISTER)
        {
            Reserve();

#ifdef ARCHITECTURE_AMD64
            OP_ASSERT(!(source.type == Operand::TYPE_IMMEDIATE64) || size == OPSIZE_64);

            if (target.UseREX() || source.type == Operand::TYPE_IMMEDIATE64)
                WriteByte(CalculateREX(target, Operand(REG_NONE), size));
#endif // ARCHITECTURE_AMD64

            WriteByte(0xb8 + (target.base & 7));

#ifdef ARCHITECTURE_AMD64
            if (source.type == Operand::TYPE_IMMEDIATE64)
                Write(source.immediate);
            else
#endif // ARCHITECTURE_AMD64
                WriteImmediate(source.immediate, size);

            return;
        }
    }
    else if (source.type == Operand::TYPE_MEMORY && source.pointer && target.base == ES_CodeGenerator::REG_AX)
    {
#ifdef ARCHITECTURE_AMD64
        if (size == OPSIZE_64)
            WriteByte(CalculateREX(Operand(REG_NONE), Operand(REG_NONE), size));
#endif // ARCHITECTURE_AMD64

        WriteByte(0xa1);
        Write(source.pointer);

        return;
    }
    else if (size == OPSIZE_8 && source.type != Operand::TYPE_IMMEDIATE)
    {
        Reserve();

        Operand rm, reg;
        unsigned char op;

        if (source.type == Operand::TYPE_MEMORY)
            rm = source, reg = target, op = 0x8a;
        else
            rm = target, reg = source, op = 0x88;

        OP_ASSERT(reg.base < REG_SP);

#ifdef ARCHITECTURE_AMD64
        if (source.UseREX() || target.UseREX())
            WriteByte(CalculateREX(rm, reg, OPSIZE_8));
#endif // ARCHITECTURE_AMD64

        WriteByte(op);
        WriteModRM(rm, reg);
        WriteAddress(rm);

        return;
    }

    OP_ASSERT(source.type != Operand::TYPE_MEMORY || target.type != Operand::TYPE_MEMORY);
    Generic2Operands(OPs_MOV, source, target, size);
}

void
ES_CodeGenerator::MOVDQ(const Operand &source, const Operand &target, OperandSize size)
{
    Reserve();

    WriteByte(0x66);

    if (source.type == ES_CodeGenerator::Operand::TYPE_XMM && target.type == ES_CodeGenerator::Operand::TYPE_XMM)
    {
#if defined ARCHITECTURE_AMD64 && defined ARCHITECTURE_SSE
        if (source.xmm >= ES_CodeGenerator::REG_XMM8 || target.xmm >= ES_CodeGenerator::REG_XMM8)
            WriteByte(CalculateREX(target, source, size));
#endif // ARCHITECTURE_AMD64 && ARCHITECTURE_SSE

        WriteByte(0x0f);
        WriteByte(0xd6);
        WriteModRM(target, source);
    }
    else
        Generic2Operands(OPs_MOVQ, source, target, size, 0x0f);
}

void
ES_CodeGenerator::MOVSX(const Operand &source, const Operand &target, OperandSize msize, OperandSize rsize)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (rsize == OPSIZE_64 || source.UseREX() || target.UseREX())
        WriteByte(CalculateREX(target, source, rsize));
#endif // ARCHITECTURE_AMD64

    WriteByte(0x0f);
    WriteByte(msize == OPSIZE_8 ? 0xbe : 0xbf);
    WriteModRM(source, target);
    WriteAddress(source);
}

void
ES_CodeGenerator::MOVZX(const Operand &source, const Operand &target, OperandSize msize, OperandSize rsize)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (rsize == OPSIZE_64 || source.UseREX() || target.UseREX())
        WriteByte(CalculateREX(target, source, rsize));
#endif // ARCHITECTURE_AMD64

    WriteByte(0x0f);
    WriteByte(msize == OPSIZE_8 ? 0xb6 : 0xb7);
    WriteModRM(source, target);
    WriteAddress(source);
}

void
ES_CodeGenerator::MovePointerIntoRegister(void *ptr, ES_CodeGenerator::Register reg)
{
#ifdef ARCHITECTURE_AMD64
    if (reinterpret_cast<UINTPTR>(ptr) <= UINT_MAX)
        /* 32-bit operation automatically zeroes the upper 32 bits of the 64-bit register. */
        MOV(Operand(static_cast<int>(reinterpret_cast<UINTPTR>(ptr))), reg, ES_CodeGenerator::OPSIZE_32);
    else
        MOV(IMMEDIATE(reinterpret_cast<UINTPTR>(ptr)), reg, ES_CodeGenerator::OPSIZE_64);
#else // ARCHITECTURE_AMD64
    MOV(IMMEDIATE(ptr), reg, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64
}

void
ES_CodeGenerator::ADD(const Operand &source, const Operand &target, OperandSize size)
{
    if (source.type == Operand::TYPE_IMMEDIATE && (source.immediate < -128 || source.immediate > 127) && target.type == Operand::TYPE_REGISTER && target.base == REG_AX)
    {
        Reserve();

#ifdef ARCHITECTURE_AMD64
        if (size == OPSIZE_64)
            WriteByte(0x48); // REX.W
#endif // ARCHITECTURE_AMD64

        WriteByte(0x05);
        WriteImmediate(source.immediate, size);
    }
    else
        Generic2Operands(OPs_ADD, source, target, size);
}

void
ES_CodeGenerator::ADC(const Operand &source, const Operand &target, OperandSize size)
{
    if (source.type == Operand::TYPE_IMMEDIATE && (source.immediate < -128 || source.immediate > 127) && target.type == Operand::TYPE_REGISTER && target.base == REG_AX)
    {
        Reserve();

#ifdef ARCHITECTURE_AMD64
        if (size == OPSIZE_64)
            WriteByte(0x48); // REX.W
#endif // ARCHITECTURE_AMD64

        WriteByte(0x15);
        WriteImmediate(source.immediate, size);
    }
    else
        Generic2Operands(OPs_ADC, source, target, size);
}

void
ES_CodeGenerator::SUB(const Operand &source, const Operand &target, OperandSize size)
{
    if (source.type == Operand::TYPE_IMMEDIATE && target.type == Operand::TYPE_REGISTER && target.base == REG_AX)
    {
        Reserve();

#ifdef ARCHITECTURE_AMD64
        if (size == OPSIZE_64)
            WriteByte(0x48); // REX.W
#endif // ARCHITECTURE_AMD64

        WriteByte(0x2d);
        WriteImmediate(source.immediate, size);
    }
    else
        Generic2Operands(OPs_SUB, source, target, size);
}

void
ES_CodeGenerator::SBB(const Operand &source, const Operand &target, OperandSize size)
{
    if (source.type == Operand::TYPE_IMMEDIATE && target.type == Operand::TYPE_REGISTER && target.base == REG_AX)
    {
        Reserve();

#ifdef ARCHITECTURE_AMD64
        if (size == OPSIZE_64)
            WriteByte(0x48); // REX.W
#endif // ARCHITECTURE_AMD64

        WriteByte(0x1d);
        WriteImmediate(source.immediate, size);
    }
    else
        Generic2Operands(OPs_SBB, source, target, size);
}

void
ES_CodeGenerator::LEA(const Operand &source, const Operand &target, OperandSize size)
{
    Generic2Operands(OPs_LEA, target, source, size);
}

void
ES_CodeGenerator::AND(const Operand &source, const Operand &target, OperandSize size)
{
    Generic2Operands(OPs_AND, source, target, size);
}

void
ES_CodeGenerator::OR(const Operand &source, const Operand &target, OperandSize size)
{
    Generic2Operands(OPs_OR, source, target, size);
}

void
ES_CodeGenerator::XOR(const Operand &source, const Operand &target, OperandSize size)
{
    Generic2Operands(OPs_XOR, source, target, size);
}

void
ES_CodeGenerator::SHL(const Operand &count, const Operand &target, OperandSize size)
{
    if (count.type == Operand::TYPE_IMMEDIATE)
        if (count.immediate == 1)
            Generic2Operands(OPs_SHL1, count, target, size);
        else
            Generic2Operands(OPs_SHL, count, target, size);
    else
    {
        Reserve();

        OP_ASSERT(count.type == Operand::TYPE_REGISTER && count.base == REG_CX);

#ifdef ARCHITECTURE_AMD64
        if (target.UseREX() || size == OPSIZE_64)
            WriteByte(CalculateREX(target, Dummy(4), size));
#endif // ARCHITECTURE_AMD64

        WriteByte(0xd3);
        WriteModRM(target, Dummy(4));
        WriteAddress(target);
    }
}

void
ES_CodeGenerator::SAR(const Operand &count, const Operand &target, OperandSize size)
{
    if (count.type == Operand::TYPE_IMMEDIATE)
        if (count.immediate == 1)
            Generic2Operands(OPs_SAR1, count, target, size);
        else
            Generic2Operands(OPs_SAR, count, target, size);
    else
    {
        Reserve();

        OP_ASSERT(count.type == Operand::TYPE_REGISTER && count.base == REG_CX);

#ifdef ARCHITECTURE_AMD64
        if (target.UseREX() || size == OPSIZE_64)
            WriteByte(CalculateREX(target, Dummy(7), size));
#endif // ARCHITECTURE_AMD64

        WriteByte(0xd3);
        WriteModRM(target, Dummy(7));
        WriteAddress(target);
    }
}

void
ES_CodeGenerator::SHR(const Operand &count, const Operand &target, OperandSize size)
{
    if (count.type == Operand::TYPE_IMMEDIATE)
        if (count.immediate == 1)
            Generic2Operands(OPs_SHR1, count, target, size);
        else
            Generic2Operands(OPs_SHR, count, target, size);
    else
    {
        Reserve();

        OP_ASSERT(count.type == Operand::TYPE_REGISTER && count.base == REG_CX);

#ifdef ARCHITECTURE_AMD64
        if (target.UseREX() || size == OPSIZE_64)
            WriteByte(CalculateREX(target, Dummy(5), size));
#endif // ARCHITECTURE_AMD64

        WriteByte(0xd3);
        WriteModRM(target, Dummy(5));
        WriteAddress(target);
    }
}

void
ES_CodeGenerator::NEG(const Operand &target, OperandSize size)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (target.UseREX() || size == OPSIZE_64)
        WriteByte(CalculateREX(target, Dummy(3), size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0xf7);
    WriteModRM(target, Dummy(3));
    WriteAddress(target);
}

void
ES_CodeGenerator::NOT(const Operand &target, OperandSize size)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (target.UseREX() || size == OPSIZE_64)
        WriteByte(CalculateREX(target, Dummy(2), size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0xf7);
    WriteModRM(target, Dummy(2));
    WriteAddress(target);
}

void
ES_CodeGenerator::INC(const Operand &target, OperandSize size)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (target.UseREX() || size == OPSIZE_64)
        WriteByte(CalculateREX(target, Dummy(0), size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0xff);
    WriteModRM(target, Dummy(0));
    WriteAddress(target);
}

void
ES_CodeGenerator::DEC(const Operand &target, OperandSize size)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (target.UseREX() || size == OPSIZE_64)
        WriteByte(CalculateREX(Dummy(0), target, size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0xff);
    WriteModRM(target, Dummy(1));
    WriteAddress(target);
}

void
ES_CodeGenerator::CDQ()
{
    Reserve();
    WriteByte(0x99);
}

void
ES_CodeGenerator::IMUL(const Operand &source, const Operand &target, OperandSize size)
{
    OP_ASSERT(target.type == Operand::TYPE_REGISTER);
    Generic2Operands(OPs_IMUL, source, target, size, 0x0f);
}

void
ES_CodeGenerator::IMUL(const Operand &source, OperandSize size)
{
    OP_ASSERT(source.type == Operand::TYPE_REGISTER || source.type == Operand::TYPE_MEMORY);

    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (source.UseREX() || size == OPSIZE_64)
        WriteByte(CalculateREX(Dummy(5), source, size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0xf7);
    WriteModRM(source, Dummy(5));
    WriteAddress(source);
}

void
ES_CodeGenerator::IMUL(const Operand &source, int immediate, const Operand &target, OperandSize size)
{
    OP_ASSERT(source.type == Operand::TYPE_REGISTER || source.type == Operand::TYPE_MEMORY);
    OP_ASSERT(target.type == Operand::TYPE_REGISTER);

    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (source.UseREX() || size == OPSIZE_64)
        WriteByte(CalculateREX(source, target, size));
#endif // ARCHITECTURE_AMD64

    WriteByte(immediate >= -128 && immediate <= 127 ? 0x6b : 0x69);
    WriteModRM(source, target);

    WriteAddress(source);

    if (immediate >= -128 && immediate <= 127)
        Write(static_cast<char>(immediate));
    else
        Write(immediate);
}

void
ES_CodeGenerator::IDIV(const Operand &source, OperandSize size)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (source.UseREX() || size == OPSIZE_64)
        WriteByte(CalculateREX(source, Dummy(7), size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0xf7);
    WriteModRM(source, Dummy(7));

    WriteAddress(source);
}

void
ES_CodeGenerator::CMP(const Operand &source, const Operand &target, OperandSize size)
{
    OP_ASSERT(target.type != Operand::TYPE_IMMEDIATE);
    Generic2Operands(OPs_CMP, source, target, size);
}

void
ES_CodeGenerator::TEST(const Operand &source, const Operand &target, OperandSize size)
{
    OP_ASSERT(target.type != Operand::TYPE_IMMEDIATE);
    Generic2Operands(OPs_TEST, source, target, size);
}

void
ES_CodeGenerator::JMP(const Operand &target)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (target.UseREX())
        WriteByte(CalculateREX(target, Dummy(4), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

    WriteByte(0xff);
    WriteModRM(target, Dummy(4));
    WriteAddress(target);
}

const unsigned char OPs_CMOVcc[] = {
    0x0,  // ES_NATIVE_UNCONDITIONAL
    0x44, // ES_NATIVE_CONDITION_EQUAL
    0x45, // ES_NATIVE_CONDITION_NOT_EQUAL
    0x42, // ES_NATIVE_CONDITION_BELOW
    0x43, // ES_NATIVE_CONDITION_NOT_BELOW
    0x46, // ES_NATIVE_CONDITION_BELOW_EQUAL
    0x47, // ES_NATIVE_CONDITION_NOT_BELOW_EQUAL
    0x4c, // ES_NATIVE_CONDITION_LESS
    0x4d, // ES_NATIVE_CONDITION_NOT_LESS
    0x4e, // ES_NATIVE_CONDITION_LESS_EQUAL
    0x4f, // ES_NATIVE_CONDITION_NOT_LESS_EQUAL
    0x40, // ES_NATIVE_CONDITION_OVERFLOW
    0x41, // ES_NATIVE_CONDITION_NOT_OVERFLOW
    0x4a, // ES_NATIVE_CONDITION_PARITY
    0x4b, // ES_NATIVE_CONDITION_NOT_PARITY
    0x48, // ES_NATIVE_CONDITION_SIGN
    0x49, // ES_NATIVE_CONDITION_NOT_SIGN
};

void
ES_CodeGenerator::CMOV(const Operand &source, const Operand &target, Condition condition, OperandSize size)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (size == OPSIZE_64 || source.UseREX() || target.UseREX())
        WriteByte(CalculateREX(source, target, size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0x0f);
    WriteByte(OPs_CMOVcc[condition]);
    WriteModRM(source, target);
    WriteAddress(source);
}

const unsigned char OPs_SETcc[] = {
    0x0,  // ES_NATIVE_UNCONDITIONAL
    0x94, // CONDITION_EQUAL
    0x95, // CONDITION_NOT_EQUAL
    0x92, // CONDITION_BELOW
    0x93, // CONDITION_NOT_BELOW
    0x96, // CONDITION_BELOW_EQUAL
    0x97, // CONDITION_NOT_BELOW_EQUAL
    0x9c, // CONDITION_LESS
    0x9d, // CONDITION_NOT_LESS
    0x9e, // CONDITION_LESS_EQUAL
    0x9f, // CONDITION_NOT_LESS_EQUAL
    0x90, // CONDITION_OVERFLOW
    0x91, // CONDITION_NOT_OVERFLOW
    0x9a, // CONDITION_PARITY
    0x9b, // CONDITION_NOT_PARITY
    0x98, // CONDITION_SIGN
    0x99, // CONDITION_NOT_SIGN
};

void
ES_CodeGenerator::SET(const Operand &target, Condition condition)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (target.UseREX())
        WriteByte(CalculateREX(target, Dummy(0), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

    WriteByte(0x0f);
    WriteByte(OPs_SETcc[condition]);
    WriteModRM(target, Dummy(0));
    WriteAddress(target);
}

void
ES_CodeGenerator::FLD(const Operand &source, OperandSize size)
{
    FPU_OPERATION();

    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (source.UseREX())
        WriteByte(CalculateREX(source, Dummy(0), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

    if (size == OPSIZE_32)
        WriteByte(0xd9);
    else
        WriteByte(0xdd);

    WriteModRM(source, Dummy(0));

    WriteAddress(source);
}

void
ES_CodeGenerator::FILD(const Operand &source, OperandSize size)
{
    FPU_OPERATION();

    Reserve();

    Operand dummy(Operand::TYPE_NONE, size == 64 ? 5 : 0);

#ifdef ARCHITECTURE_AMD64
    if (source.UseREX())
        WriteByte(CalculateREX(source, dummy, OPSIZE_32));
#endif // ARCHITECTURE_AMD64

    if (size == OPSIZE_32)
        WriteByte(0xdb);
    else
        WriteByte(0xdf);

    WriteModRM(source, dummy);

    WriteAddress(source);
}

void
ES_CodeGenerator::FLDZ()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xee);
}

void
ES_CodeGenerator::FLD1()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xe8);
}

void
ES_CodeGenerator::FST(const Operand &target, BOOL pop, OperandSize size)
{
    FPU_OPERATION();

    OP_ASSERT(size == OPSIZE_32 || size == OPSIZE_64);
    OP_ASSERT(target.type == Operand::TYPE_MEMORY);

    Reserve();

    unsigned code = pop ? 3 : 2;

#ifdef ARCHITECTURE_AMD64
    if (target.UseREX())
        WriteByte(CalculateREX(target, Operand(Operand::TYPE_NONE, code), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

    if (size == OPSIZE_32)
        WriteByte(0xd9);
    else
        WriteByte(0xdd);

    WriteModRM(target, Operand(Operand::TYPE_NONE, code));
    WriteAddress(target);
}

void
ES_CodeGenerator::FST(int target, BOOL pop)
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xdd);
    WriteByte((pop ? 0xd8 : 0xd0) + target);
}

void
ES_CodeGenerator::GenericMemX87(const Operand &mem, int opcode, int reg)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (mem.UseREX())
        WriteByte(CalculateREX(mem, Operand(Operand::TYPE_NONE, reg), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

    WriteByte(opcode);

    WriteModRM(mem, Operand(Operand::TYPE_NONE, reg));
    WriteAddress(mem);
}

void
ES_CodeGenerator::FIST(const Operand &target, BOOL pop, OperandSize size)
{
    FPU_OPERATION();

    switch (size)
    {
    case OPSIZE_16:
        GenericMemX87(target, 0xdf, pop ? 3 : 2);
        break;

    case OPSIZE_32:
        GenericMemX87(target, 0xdb, pop ? 3 : 2);
        break;

    default:
        OP_ASSERT(size == OPSIZE_64 && pop);
        GenericMemX87(target, 0xdf, 7);
    }
}

void
ES_CodeGenerator::FADD(const Operand &source)
{
    FPU_OPERATION();

    GenericMemX87(source, 0xdc, 0);
}

void
ES_CodeGenerator::FADD(int source, int target, BOOL pop)
{
    FPU_OPERATION();

    OP_ASSERT(source == 0 || target == 0);
    OP_ASSERT(!pop || source == 0);

    if (target == 0)
    {
        WriteByte(0xd8);
        WriteByte(0xf0 + source);
    }
    else
    {
        WriteByte(pop ? 0xde : 0xdc);
        WriteByte(0xf8 + target);
    }
}

void
ES_CodeGenerator::FIADD(const Operand &source, OperandSize size)
{
    FPU_OPERATION();

    OP_ASSERT(size == OPSIZE_32 || size == OPSIZE_16);
    GenericMemX87(source, size == OPSIZE_32 ? 0xda : 0xde, 0);
}

void
ES_CodeGenerator::FSUB(const Operand &source, BOOL reverse)
{
    FPU_OPERATION();

    GenericMemX87(source, 0xdc, reverse ? 5 : 4);
}

void
ES_CodeGenerator::FSUB(int source, int target, BOOL pop, BOOL reverse)
{
    FPU_OPERATION();

    OP_ASSERT(source == 0 || target == 0);
    OP_ASSERT(!pop || source == 0);

    if (target == 0)
    {
        WriteByte(0xd8);
        WriteByte((reverse ? 0xe8 : 0xe0) + source);
    }
    else
    {
        WriteByte(pop ? 0xde : 0xdc);
        WriteByte((reverse ? 0xe0 : 0xe8) + target);
    }
}

void
ES_CodeGenerator::FISUB(const Operand &source, BOOL reverse, OperandSize size)
{
    FPU_OPERATION();

    OP_ASSERT(size == OPSIZE_32 || size == OPSIZE_16);
    GenericMemX87(source, size == OPSIZE_32 ? 0xda : 0xde, reverse ? 5 : 4);
}

void
ES_CodeGenerator::FMUL(const Operand &source)
{
    FPU_OPERATION();

    GenericMemX87(source, 0xdc, 1);
}

void
ES_CodeGenerator::FMUL(int source, int target, BOOL pop)
{
    FPU_OPERATION();

    OP_ASSERT(source == 0 || target == 0);
    OP_ASSERT(!pop || source == 0);

    if (target == 0)
    {
        WriteByte(0xd8);
        WriteByte(0xc0 + source);
    }
    else
    {
        WriteByte(pop ? 0xde : 0xdc);
        WriteByte(0xc8 + target);
    }
}

void
ES_CodeGenerator::FIMUL(const Operand &source, OperandSize size)
{
    FPU_OPERATION();

    OP_ASSERT(size == OPSIZE_32 || size == OPSIZE_16);
    GenericMemX87(source, size == OPSIZE_32 ? 0xda : 0xde, 1);
}

void
ES_CodeGenerator::FDIV(const Operand &source, BOOL reverse)
{
    FPU_OPERATION();

    GenericMemX87(source, 0xdc, reverse ? 7 : 6);
}

void
ES_CodeGenerator::FDIV(int source, int target, BOOL pop, BOOL reverse)
{
    FPU_OPERATION();

    OP_ASSERT(source == 0 || target == 0);
    OP_ASSERT(!pop || source == 0);

    if (target == 0)
    {
        WriteByte(0xd8);
        WriteByte((reverse ? 0xf8 : 0xf0) + source);
    }
    else
    {
        WriteByte(pop ? 0xde : 0xdc);
        WriteByte((reverse ? 0xf0 : 0xf8) + target);
    }
}

void
ES_CodeGenerator::FIDIV(const Operand &source, BOOL reverse, OperandSize size)
{
    FPU_OPERATION();

    OP_ASSERT(size == OPSIZE_32 || size == OPSIZE_16);
    GenericMemX87(source, size == OPSIZE_32 ? 0xda : 0xde, reverse ? 7 : 6);
}

#if 0
void
ES_CodeGenerator::FADDP()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xde);
    WriteByte(0xc1);
}

void
ES_CodeGenerator::FSUBP()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xde);
    WriteByte(0xe9);
}

void
ES_CodeGenerator::FDIVP()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xde);
    WriteByte(0xf9);
}

void
ES_CodeGenerator::FREMP()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xf5);
}
#endif // 0

void
ES_CodeGenerator::FCHS()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xe0);
}

void
ES_CodeGenerator::FUCOMI(int operand, BOOL pop)
{
    FPU_OPERATION();

    Reserve();

    WriteByte(pop ? 0xdf : 0xdb);
    WriteByte(0xe8 + operand);
}

void
ES_CodeGenerator::FXAM()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xe5);
}

void
ES_CodeGenerator::FNSTSW()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xdf);
    WriteByte(0xe0);
}

void
ES_CodeGenerator::FSIN()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xfe);
}

void
ES_CodeGenerator::FCOS()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xff);
}

void
ES_CodeGenerator::FSINCOS()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xfb);
}

void
ES_CodeGenerator::FSQRT()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xfa);
}

void
ES_CodeGenerator::FRNDINT()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xfc);
}

void
ES_CodeGenerator::FLDLN2()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xed);
}

void
ES_CodeGenerator::FYL2X()
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteByte(0xf1);
}

void
ES_CodeGenerator::FNSTCW(const Operand &target)
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteModRM(target, Dummy(7));
    WriteAddress(target);
}

void
ES_CodeGenerator::FLDCW(const Operand &source)
{
    FPU_OPERATION();

    Reserve();

    WriteByte(0xd9);
    WriteModRM(source, Dummy(5));
    WriteAddress(source);
}

void
ES_CodeGenerator::EMMS()
{
    WriteByte(0x0f);
    WriteByte(0x77);
}

#ifdef ARCHITECTURE_SSE

void
ES_CodeGenerator::GenericSSE2(unsigned prefix, unsigned op, const Operand &source, XMM target, OperandSize size)
{
    FPU_OPERATION();

    OP_ASSERT(supports_SSE2);

    Reserve();

    WriteByte(prefix);

#ifdef ARCHITECTURE_AMD64
    if (source.UseREX() || target >= REG_XMM8 || size == OPSIZE_64)
        WriteByte(CalculateREX(source, Operand(target), size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0x0f);
    WriteByte(op);
    WriteModRM(source, Operand(target));

    WriteAddress(source);
}

void
ES_CodeGenerator::MOVSD(const Operand &source, XMM target)
{
    GenericSSE2(0xf2, 0x10, source, target);
}

void
ES_CodeGenerator::MOVSD(XMM source, const Operand &target)
{
    GenericSSE2(0xf2, 0x11, target, source);
}

void
ES_CodeGenerator::MOVSS(XMM source, const Operand &target)
{
    GenericSSE2(0xf3, 0x11, target, source);
}

void
ES_CodeGenerator::MOVD(XMM source, const Operand &target)
{
    GenericSSE2(0x66, 0x7e, target, source, OPSIZE_32);
}

void
ES_CodeGenerator::MOVQ(XMM source, const Operand &target)
{
#ifdef ARCHITECTURE_AMD64
    GenericSSE2(0x66, 0x7e, target, source, OPSIZE_64);
#else
    GenericSSE2(0x66, 0xd6, target, source);
#endif // ARCHITECTURE_AMD64
}

void
ES_CodeGenerator::MOVDQA(const Operand &source, XMM target)
{
    GenericSSE2(0xf3, 0x6f, source, target);
}

void
ES_CodeGenerator::MOVDQA(XMM source, const Operand &target)
{
    GenericSSE2(0xf3, 0x7f, target, source);
}

void
ES_CodeGenerator::CVTSI2SD(const Operand &source, XMM target, OperandSize size)
{
    GenericSSE2(0xf2, 0x2a, source, target, size);
}

void
ES_CodeGenerator::CVTSI2SS(const Operand &source, XMM target, OperandSize size)
{
    GenericSSE2(0xf3, 0x2a, source, target, size);
}

void
ES_CodeGenerator::CVTSD2SI(const Operand &source, Register target, OperandSize size)
{
    FPU_OPERATION();

    OP_ASSERT(supports_SSE2);

    Reserve();

    WriteByte(0xf2);

#ifdef ARCHITECTURE_AMD64
    if (source.UseREX() || target >= REG_R8 || size == OPSIZE_64)
        WriteByte(CalculateREX(source, Operand(Operand::TYPE_REGISTER, target), size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0x0f);
    WriteByte(0x2d);
    WriteModRM(source, Operand(Operand::TYPE_REGISTER, target));

    WriteAddress(source);
}

void
ES_CodeGenerator::CVTTSD2SI(const Operand &source, Register target, OperandSize size)
{
    FPU_OPERATION();

    OP_ASSERT(supports_SSE2);

    Reserve();

    WriteByte(0xf2);

#ifdef ARCHITECTURE_AMD64
    if (source.UseREX() || target >= REG_R8 || size == OPSIZE_64)
        WriteByte(CalculateREX(source, Operand(Operand::TYPE_REGISTER, target), size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0x0f);
    WriteByte(0x2c);
    WriteModRM(source, Operand(Operand::TYPE_REGISTER, target));
    WriteAddress(source);
}

void
ES_CodeGenerator::CVTTSS2SI(const Operand &source, Register target, OperandSize size)
{
    FPU_OPERATION();

    OP_ASSERT(supports_SSE2);

    Reserve();

    WriteByte(0xf3);

#ifdef ARCHITECTURE_AMD64
    if (source.UseREX() || target >= REG_R8 || size == OPSIZE_64)
        WriteByte(CalculateREX(source, Operand(Operand::TYPE_REGISTER, target), size));
#endif // ARCHITECTURE_AMD64

    WriteByte(0x0f);
    WriteByte(0x2c);
    WriteModRM(source, Operand(Operand::TYPE_REGISTER, target));
    WriteAddress(source);
}

void
ES_CodeGenerator::CVTSD2SS(const Operand &source, XMM target)
{
    GenericSSE2(0xf2, 0x5a, source, target);
}

void
ES_CodeGenerator::CVTSS2SD(const Operand &source, XMM target)
{
    GenericSSE2(0xf3, 0x5a, source, target);
}

void
ES_CodeGenerator::ADDSD(const Operand &source, XMM target)
{
    GenericSSE2(0xf2, 0x58, source, target);
}

void
ES_CodeGenerator::SUBSD(const Operand &source, XMM target)
{
    GenericSSE2(0xf2, 0x5c, source, target);
}

void
ES_CodeGenerator::MULSD(const Operand &source, XMM target)
{
    GenericSSE2(0xf2, 0x59, source, target);
}

void
ES_CodeGenerator::DIVSD(const Operand &source, XMM target)
{
    GenericSSE2(0xf2, 0x5e, source, target);
}

void
ES_CodeGenerator::ANDPD(const Operand &source, XMM target)
{
    GenericSSE2(0x66, 0x54, source, target);
}

void
ES_CodeGenerator::ANDNPD(const Operand &source, XMM target)
{
    GenericSSE2(0x66, 0x55, source, target);
}

void
ES_CodeGenerator::ORPD(const Operand &source, XMM target)
{
    GenericSSE2(0x66, 0x56, source, target);
}

void
ES_CodeGenerator::XORPD(const Operand &source, XMM target)
{
    GenericSSE2(0x66, 0x57, source, target);
}

void
ES_CodeGenerator::UCOMISD(const Operand &source, XMM target)
{
    GenericSSE2(0x66, 0x2e, source, target);
}

void
ES_CodeGenerator::UCOMISS(const Operand &source, XMM target)
{
    GenericSSE2(0x90, 0x2e, source, target);
}

void
ES_CodeGenerator::SQRTSD(const Operand &source, XMM target)
{
    GenericSSE2(0xf2, 0x51, source, target);
}

void
ES_CodeGenerator::RCPSS(const Operand &source, XMM target)
{
    GenericSSE2(0xf3, 0x53, source, target);
}

void
ES_CodeGenerator::ROUNDSD(const Operand &source, XMM target, RoundMode mode)
{
    FPU_OPERATION();

    OP_ASSERT(supports_SSE2);
    OP_ASSERT(supports_SSE41);

    Reserve();

    WriteByte(0x66);

#ifdef ARCHITECTURE_AMD64
    if (source.UseREX() || target >= REG_XMM8)
        WriteByte(CalculateREX(source, Operand(target), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

    WriteByte(0x0f);
    WriteByte(0x3a);
    WriteByte(0x0b);

    WriteModRM(source, Operand(target));
    WriteAddress(source);

    char modebits = 8;

    switch (mode)
    {
    case ROUND_MODE_floor:
        modebits |= 1;
        break;

    case ROUND_MODE_ceil:
        modebits |= 2;
    }

    WriteByte(modebits);
}

#endif // ARCHITECTURE_SSE

void
ES_CodeGenerator::CALL(const Operand &target)
{
#ifdef ARCHITECTURE_AMD64_WINDOWS
    SUB(ES_CodeGenerator::IMMEDIATE(32), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64_WINDOWS
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (target.UseREX())
        WriteByte(CalculateREX(target, Dummy(2), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

    WriteByte(0xff);
    WriteModRM(target, Dummy(2));
    WriteAddress(target);
#ifdef ARCHITECTURE_AMD64_WINDOWS
    ADD(ES_CodeGenerator::IMMEDIATE(32), ES_CodeGenerator::REG_SP, ES_CodeGenerator::OPSIZE_POINTER);
#endif // ARCHITECTURE_AMD64_WINDOWS
}

void
ES_CodeGenerator::CALL(void *f)
{
    Reserve();

#ifdef ARCHITECTURE_AMD64
    if (reinterpret_cast<UINTPTR>(f) <= UINT_MAX)
        /* 32-bit operation automatically zeroes the upper 32 bits of the 64-bit register. */
        MOV(IMMEDIATE(reinterpret_cast<UINTPTR>(f)), REG_AX, OPSIZE_32);
    else
    {
        Constant *constant = NewConstant(f);
        MOV(CONSTANT(constant), REG_AX, ES_CodeGenerator::OPSIZE_64);
    }

    CALL(REG_AX);
#else // ARCHITECTURE_AMD64
    WriteByte(0xe8);

    if (f)
    {
        Block::RelativeCall *relative_call = OP_NEWGRO_L(Block::RelativeCall, (), arena);
        relative_call->offset = GetBufferUsed() - current_block->start;
        relative_call->address = f;
        relative_call->next = current_block->relative_calls;
        current_block->relative_calls = relative_call;
    }

    Write(static_cast<int>(0));
#endif // ARCHITECTURE_AMD64
}

void
ES_CodeGenerator::CALL(void (*f)())
{
    union
    {
        void (*f)();
        void *p;
    } cast;

    cast.f = f;
    CALL(cast.p);
}

void
ES_CodeGenerator::RET()
{
    Reserve();

    WriteByte(0xc3);
}

void
ES_CodeGenerator::RET(short n)
{
    Reserve();

    WriteByte(0xc2);
    Write(n);
}

void
ES_CodeGenerator::PUSH(const Operand &op, OperandSize size)
{
    Reserve();

    if (op.type == Operand::TYPE_REGISTER && size == OPSIZE_POINTER)
    {
#ifdef ARCHITECTURE_AMD64
        if (op.UseREX())
            WriteByte(CalculateREX(op, Dummy(0), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

        WriteByte(0x50 + (op.base & 7));
    }
    else if (op.type == Operand::TYPE_IMMEDIATE)
    {
        WriteByte(0x68);

#ifdef ARCHITECTURE_AMD64
        OP_ASSERT(!op.pointer);
#else // ARCHITECTURE_AMD64
        if (op.pointer)
            Write(op.pointer);
        else
#endif // ARCHITECTURE_AMD64
            WriteImmediate(op.immediate, size);
    }
    else
    {
#ifdef ARCHITECTURE_AMD64
        if (op.UseREX())
            WriteByte(CalculateREX(op, Dummy(6), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

        WriteByte(0xff);
        WriteModRM(op, Dummy(6));

        WriteSIB(op);

        if (op.UseDisp8())
            Write(static_cast<char>(op.disp));
        else if (op.UseDisp32())
            Write(op.disp);
    }
}

void
ES_CodeGenerator::POP(const Operand &op, OperandSize size)
{
    Reserve();

    if (op.type == Operand::TYPE_REGISTER && size == OPSIZE_POINTER)
    {
#ifdef ARCHITECTURE_AMD64
        if (op.UseREX())
            WriteByte(CalculateREX(op, Dummy(0), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

        WriteByte(0x58 + (op.base & 7));
    }
    else
    {
#ifdef ARCHITECTURE_AMD64
        if (op.UseREX())
            WriteByte(CalculateREX(op, Dummy(6), OPSIZE_32));
#endif // ARCHITECTURE_AMD64

        WriteByte(0x8f);
        WriteModRM(op, Dummy(0));

        WriteSIB(op);

        if (op.UseDisp8())
            Write(static_cast<char>(op.disp));
        else if (op.UseDisp32())
            Write(op.disp);
    }
}

void
ES_CodeGenerator::CPUID()
{
    Reserve();

    WriteByte(0x0f);
    WriteByte(0xa2);
}

void
ES_CodeGenerator::PUSHFD()
{
    Reserve();

    WriteByte(0x9c);
}

void
ES_CodeGenerator::POPFD()
{
    Reserve();

    WriteByte(0x9d);
}

#ifdef _DEBUG

void
ES_CodeGenerator::Dump(unsigned char *begin, unsigned char *end)
{
#ifdef _STANDALONE
    /* In GDB, set a break point on this function and execute the command

         disassemble begin end

       when that break point is hit.  The final generated code will then be
       dumped when GetCode() is called to retrieve it. */

    if (dump_array)
    {
        printf("{ ");

        for (unsigned char *ptr = begin; ptr != end;)
        {
            printf("0x%02x", *ptr);
            if (++ptr != end)
                printf(", ");
        }

        printf(" }");
    }
#endif // _STANDALONE
}

#endif // _DEBUG

#include <stdio.h>
//#include <errno.h>
#include <string.h>

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

        if (block->is_jump_target && block->align_block)
            block_length += 15;

        distance += block_length;

#ifdef _DEBUG
        if (block_length < block->min_calculated_length)
            block->min_calculated_length = block_length;
#endif // _DEBUG

        if (distance > 128)
            return INT_MAX;

        block = block->next;
    }

    if (dst->is_jump_target && dst->align_block)
        distance += 15;

    return distance;
}

const char *nops[] = {
    0,
    "\x90",
    "\x66\x90",
    "\x0f\x1f\x00",
    "\x0f\x1f\x40\x00",
    "\x0f\x1f\x44\x00\x00",
    "\x66\x0f\x1f\x44\x00\x00",
    "\x0f\x1f\x80\x00\x00\x00\x00",
    "\x0f\x1f\x84\x00\x00\x00\x00\x00",
    "\x66\x0f\x1f\x84\x00\x00\x00\x00\x00"
};

static void
WriteNOPs(unsigned char *buffer, unsigned length)
{
    while (length >= 9)
    {
        op_memcpy(buffer, nops[9], 9);
        buffer += 9;
        length -= 9;
    }

    op_memcpy(buffer, nops[length], length);
}

#ifdef _STANDALONE
unsigned g_allocated_executable_memory = 0;
#endif // _STANDALONE

const OpExecMemory *
ES_CodeGenerator::GetCode(OpExecMemoryManager *execmem, void* /* constant_reg */)
{
    unsigned maximum_length = GetBufferUsed();

    FinalizeBlockList();

    Block *block = first_block;
    while (block)
    {
        if (block->is_jump_target && block->align_block)
            maximum_length += 15;
        if (block->jump_condition != ES_NATIVE_NOT_A_JUMP)
        {
            if (block->jump_condition == ES_NATIVE_UNCONDITIONAL)
                block->jump_size = 5;
            else
                block->jump_size = 6 + (block->hint ? 1 : 0);

            maximum_length += block->jump_size;
        }

        block = block->next;
    }

    if (Constant *constant = first_constant)
    {
        if (maximum_length & 63)
            maximum_length += 64 - (maximum_length & 63);

        do
        {
            if (maximum_length & (constant->alignment - 1))
                maximum_length += constant->alignment - (maximum_length & (constant->alignment - 1));

            maximum_length += constant->size;
        }
        while ((constant = constant->next) != NULL);
    }

#ifdef _STANDALONE
    g_allocated_executable_memory += maximum_length;
#endif // _STANDALONE

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
    Block::Annotation *all_annotations = NULL, **annotations_pointer = &all_annotations;
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

    const OpExecMemory *memory_block = execmem->AllocateL(maximum_length);
    unsigned char *memory = static_cast<unsigned char *>(memory_block->address);

    block = first_block;

    unsigned char *old_buffer = buffer_base;
    BOOL forward_jumps = FALSE;

    buffer = memory;

    while (block)
    {
        if (block->is_jump_target && block->align_block && (reinterpret_cast<UINTPTR>(buffer) & 15) != 0)
        {
            unsigned alignment = 16 - (reinterpret_cast<UINTPTR>(buffer) & 15);
            WriteNOPs(buffer, alignment);
            buffer += alignment;
        }

        OP_ASSERT(block->actual_start == UINT_MAX);

        unsigned length = block->end - block->start;

        op_memcpy(buffer, old_buffer + block->start, length);

#ifdef ES_CODEGENERATOR_RELATIVE_CALLS
        Block::RelativeCall *relative_call = block->relative_calls;
        while (relative_call)
        {
            unsigned char *old_buffer = buffer;

            buffer += relative_call->offset;
            OP_ASSERT(buffer[0] == 0 && buffer[1] == 0 && buffer[2] == 0 && buffer[3] == 0);
            Write(static_cast<int>(static_cast<unsigned char *>(relative_call->address) - (static_cast<unsigned char *>(buffer) + 4)));

            buffer = old_buffer;
            relative_call = relative_call->next;
        }
#endif // ES_CODEGENERATOR_RELATIVE_CALLS

        block->actual_start = buffer - memory;

        buffer += length;

        if (block->jump_condition != ES_NATIVE_NOT_A_JUMP)
        {
            Block *target = block->jump_target;

            if (target->actual_start != UINT_MAX)
            {
                WriteJump(block->jump_condition, static_cast<int>(target->actual_start) - static_cast<int>(buffer - memory), block->hint, block->branch_taken);
                block->jump_condition = ES_NATIVE_NOT_A_JUMP;
            }
            else if (block->next != target)
            {
                unsigned distance = CalculateMaximumDistance(block, target);
#ifdef _DEBUG
                block->maximum_distance = distance;
#endif // _DEBUG
                if (distance < 128)
                    block->jump_size = 2 + (block->hint ? 1 : 0);
                buffer += block->jump_size;

                forward_jumps = TRUE;
            }
            else
                block->jump_condition = ES_NATIVE_NOT_A_JUMP;
        }

        block->actual_end = buffer - memory;

#ifdef _DEBUG
        if (block->min_calculated_length != UINT_MAX)
            OP_ASSERT(block->actual_end - block->actual_start <= block->min_calculated_length);
#endif // _DEBUG

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

        block = block->next;
    }

#if defined _DEBUG || defined NATIVE_DISASSEMBLER_SUPPORT
    unsigned char *memory_end = buffer;
#endif // _DEBUG || NATIVE_DISASSEMBLER_SUPPORT

    if (forward_jumps)
    {
        unsigned char *stored_buffer = buffer;

        for (Block *block = first_block, *next_block; block; block = next_block)
        {
            next_block = block->next;

            if (block->jump_condition != ES_NATIVE_NOT_A_JUMP)
            {
                int offset = block->jump_target->actual_start - block->actual_end;
#ifdef _DEBUG
                OP_ASSERT(offset <= static_cast<int>(block->maximum_distance));
#endif // _DEBUG

                if (offset == 0)
                    if (block->jump_target != block)
                    {
                        WriteNOPs(memory + block->actual_end - block->jump_size, block->jump_size);
                        continue;
                    }

                buffer = memory + block->actual_end - block->jump_size;

                WriteJump(block->jump_condition, offset, block->hint, block->branch_taken, block->jump_size > 3);

                OP_ASSERT(buffer <= memory + block->actual_end);
            }
        }

        buffer = stored_buffer;
    }

#ifdef _DEBUG
    Dump(memory, memory_end);
#endif // _DEBUG

    if (Constant *constant = first_constant)
    {
        unsigned offset = buffer - memory;

        if (offset & 15)
            offset += 16 - (offset & 15);

        do
            if (constant->first_use)
            {
                if (offset & (constant->alignment - 1))
                    offset += constant->alignment - (offset & (constant->alignment - 1));

                constant->AddInstance(offset, arena);

#ifdef ARCHITECTURE_AMD64
                buffer = memory + offset;

                switch (constant->type)
                {
                case Constant::TYPE_INT:
                    Write(constant->value.i);
                    break;

                case Constant::TYPE_DOUBLE:
                    Write(constant->value.d);
                    break;

                case Constant::TYPE_2DOUBLE:
                    Write(constant->value.d2.d1);
                    Write(constant->value.d2.d2);
                    break;

                case Constant::TYPE_POINTER:
                    Write(constant->value.p);
                    break;
                }
#endif // ARCHITECTURE_AMD64

                Constant::Use *use = constant->first_use;
                do
                {
                    buffer = memory + use->block->actual_start + use->offset;

#ifdef ARCHITECTURE_AMD64
                    unsigned rip_base = buffer - memory + 4;
                    Write(static_cast<int>(offset - rip_base));
#else // ARCHITECTURE_AMD64
                    Write(static_cast<int>(reinterpret_cast<INTPTR>(memory + offset)));
#endif // ARCHITECTURE_AMD64
                }
                while ((use = use->next_per_constant) != NULL);

                offset += constant->size;
            }
        while ((constant = constant->next) != NULL);

        buffer = memory + offset;
    }

#ifdef NATIVE_DISASSEMBLER_SUPPORT
    if (disassemble)
    {
        ES_NativeDisassembleRange range;

        range.type = ES_NDRT_CODE;
        range.start = 0;
        range.end = memory_end - memory;
        range.next = 0;

        ES_NativeDisassembleData data;

        data.ranges = &range;
        data.annotations = 0;

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
        ES_NativeDisassembleAnnotation **nd_annotations = &data.annotations;

        Block::Annotation *annotation = all_annotations;
        while (annotation)
        {
            struct ES_NativeDisassembleAnnotation *nd_annotation = *nd_annotations = OP_NEWGRO_L(struct ES_NativeDisassembleAnnotation, (), arena);
            nd_annotations = &nd_annotation->next;

            nd_annotation->offset = annotation->offset;
            nd_annotation->value = annotation->value;
            nd_annotation->next = NULL;

            annotation = annotation->next;
        }
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

        ES_NativeDisassemble(memory, memory_end - memory, ES_NDM_DEBUGGING, &data);
    }
#endif // NATIVE_DISASSEMBLER_SUPPORT

    return memory_block;
}

inline void
ES_CodeGenerator::Write(char byte)
{
    *buffer++ = static_cast<unsigned char>(byte);
}

inline void
ES_CodeGenerator::WriteByte(unsigned char byte)
{
    *buffer++ = byte;
}

inline void
ES_CodeGenerator::Write(short i)
{
    unsigned short u = i;
    *buffer++ = u & 0xff;
    *buffer++ = (u >> 8) & 0xff;
}

inline void
ES_CodeGenerator::Write(int i)
{
    Write(static_cast<unsigned>(i));
}

inline void
ES_CodeGenerator::Write(unsigned u)
{
    *buffer++ = u & 0xff;
    *buffer++ = (u >> 8) & 0xff;
    *buffer++ = (u >> 16) & 0xff;
    *buffer++ = u >> 24;
}

#ifdef ARCHITECTURE_AMD64

inline void
ES_CodeGenerator::Write(INTPTR i)
{
    *buffer++ = i & 0xff;
    *buffer++ = (i >> 8) & 0xff;
    *buffer++ = (i >> 16) & 0xff;
    *buffer++ = (i >> 24) & 0xff;
    *buffer++ = (i >> 32) & 0xff;
    *buffer++ = (i >> 40) & 0xff;
    *buffer++ = (i >> 48) & 0xff;
    *buffer++ = i >> 56;
}

#endif // ARCHITECTURE_AMD64

inline void
ES_CodeGenerator::Write(double d)
{
    unsigned high, low;

    op_explode_double(d, high, low);

    Write(low);
    Write(high);
}

inline void
ES_CodeGenerator::Write(const void *p)
{
#ifdef ARCHITECTURE_AMD64
    Write(reinterpret_cast<INTPTR>(p));
#else // ARCHITECTURE_AMD64
    Write(static_cast<int>(reinterpret_cast<INTPTR>(p)));
#endif // ARCHITECTURE_AMD64
}

inline void
ES_CodeGenerator::WriteModRM(const ES_CodeGenerator::Operand &rm, const ES_CodeGenerator::Operand &reg)
{
    WriteByte(CalculateModRM(rm, reg));
}

inline void
ES_CodeGenerator::WriteSIB(const ES_CodeGenerator::Operand &rm)
{
    if (rm.UseSIB())
        WriteByte(CalculateSIB(rm));
}

inline void
ES_CodeGenerator::WriteAddress(const Operand &operand)
{
    WriteSIB(operand);

    if (operand.type == Operand::TYPE_CONSTANT)
    {
        operand.constant->AddUse(current_block, GetBufferUsed() - current_block->start, arena);
        Write(static_cast<unsigned>(0));
    }
    else
    {
        if (operand.UseDisp8())
            Write(static_cast<char>(operand.disp));
        else if (operand.UseDisp32())
            Write(operand.disp);
    }
}

inline void
ES_CodeGenerator::WriteImmediate(INTPTR immediate, OperandSize size)
{
    switch (size)
    {
    case OPSIZE_8:
        WriteByte(static_cast<signed char>(immediate));
        break;

    case OPSIZE_16:
        Write(static_cast<short>(immediate));
        break;

#ifdef ARCHITECTURE_AMD64
    case OPSIZE_64:
        OP_ASSERT(immediate >= INT_MIN && immediate <= INT_MAX);
#endif // ARCHITECTURE_AMD64

    case OPSIZE_32:
        Write(static_cast<int>(immediate));
        break;
    }
}

void
ES_CodeGenerator::Generic2Operands(unsigned char OPs[4], const Operand &op1, const Operand &op2, OperandSize size, unsigned char extra_prefix)
{
    Reserve();

    unsigned char opcode;
    BOOL imm8 = FALSE;
    int immediate = 0;

    const Operand *rm, *reg;
    Operand local;

    if (size == OPSIZE_16)
        WriteByte(0x66);

    OP_ASSERT(size != OPSIZE_8 || op1.type == Operand::TYPE_IMMEDIATE && op1.immediate >= -128 && op1.immediate <= 127);
    OP_ASSERT(op2.type != Operand::TYPE_IMMEDIATE);

    Operand::Type op1_type = op1.type;

#ifdef ARCHITECTURE_AMD64
    if (op1_type == Operand::TYPE_IMMEDIATE64)
    {
        OP_ASSERT(size == OPSIZE_32 || static_cast<int>(op1.immediate) == op1.immediate);
        op1_type = Operand::TYPE_IMMEDIATE;
    }
#endif // ARCHITECTURE_AMD64

    if (op1_type == Operand::TYPE_IMMEDIATE)
    {
#ifdef ARCHITECTURE_AMD64
        OP_ASSERT(!op1.pointer);
#else // ARCHITECTURE_AMD64
        if (op1.pointer)
            immediate = reinterpret_cast<INTPTR>(op1.pointer);
        else
#endif // ARCHITECTURE_AMD64
            immediate = op1.immediate;

        imm8 = immediate >= -128 && immediate <= 127 && OPs[size == OPSIZE_8 ? OP_RM8_IMM8 : OP_RM32_IMM8] != 0;
        opcode = OPs[size == OPSIZE_8 ? OP_RM8_IMM8 : (imm8 ? OP_RM32_IMM8 : OP_RM32_IMM32)];
        rm = &op2;
        local = Operand(Operand::TYPE_NONE, OPs[size == OPSIZE_8 ? OP_RM8_IMM8_REG : (imm8 ? OP_RM32_IMM8_REG : OP_RM32_IMM32_REG)]);
        reg = &local;
    }
    else
    {
        opcode = OPs[OP_RM_R];
        rm = &op1;
        reg = &op2;
    }

    if (!rm->UseSIB() &&
        (reg->type == Operand::TYPE_MEMORY || reg->type == Operand::TYPE_CONSTANT))
    {
        opcode = OPs[OP_R_RM];
        rm = &op2;
        reg = &op1;
    }

#ifdef ARCHITECTURE_AMD64
    if (size == OPSIZE_64 || rm->UseREX() || reg->UseREX())
        WriteByte(CalculateREX(*rm, *reg, size));
#endif // ARCHITECTURE_AMD64

    if (extra_prefix != 0)
        WriteByte(extra_prefix);

    WriteByte(opcode);
    WriteModRM(*rm, *reg);
    WriteAddress(*rm);

    if (op1_type == Operand::TYPE_IMMEDIATE && OPs != OPs_SHL1 && OPs != OPs_SAR1 && OPs != OPs_SHR1)
        if (imm8)
            Write(static_cast<char>(immediate));
        else if (size == OPSIZE_16)
            Write(static_cast<short>(immediate));
        else
            Write(immediate);
}

const unsigned char OPs_Jcc[] = {
    0xeb, 0xe9, // ES_NATIVE_UNCONDITIONAL
    0x74, 0x84, // CONDITION_EQUAL
    0x75, 0x85, // CONDITION_NOT_EQUAL
    0x72, 0x82, // CONDITION_BELOW
    0x73, 0x83, // CONDITION_NOT_BELOW
    0x76, 0x86, // CONDITION_BELOW_EQUAL
    0x77, 0x87, // CONDITION_NOT_BELOW_EQUAL
    0x7c, 0x8c, // CONDITION_LESS
    0x7d, 0x8d, // CONDITION_NOT_LESS
    0x7e, 0x8e, // CONDITION_LESS_EQUAL
    0x7f, 0x8f, // CONDITION_NOT_LESS_EQUAL
    0x70, 0x80, // CONDITION_OVERFLOW
    0x71, 0x81, // CONDITION_NOT_OVERFLOW
    0x7a, 0x8a, // CONDITION_PARITY
    0x7b, 0x8b, // CONDITION_NOT_PARITY
    0x78, 0x88, // CONDITION_SIGN
    0x79, 0x89, // CONDITION_NOT_SIGN
};

void
ES_CodeGenerator::WriteJump(Condition condition, int offset, BOOL hint, BOOL branch_taken, BOOL force_rel32)
{
    if (-128 <= (offset - (2 + static_cast<int>(hint))) && offset <= 127 && !force_rel32)
    {
        if (offset <= 0)
            offset -= 2 + static_cast<int>(hint);
        if (hint)
            WriteByte(branch_taken ? 0x3e : 0x2e);
        WriteByte(OPs_Jcc[condition * 2]);
        Write(static_cast<char>(offset));
    }
    else
    {
        if (offset <= 0)
            offset -= condition == ES_NATIVE_UNCONDITIONAL ? 5 : 6 + static_cast<int>(hint);
        if (hint)
            WriteByte(branch_taken ? 0x3e : 0x2e);
        if (condition != ES_NATIVE_UNCONDITIONAL)
            WriteByte(0x0f);
        WriteByte(OPs_Jcc[condition * 2 + 1]);
        Write(offset);
    }
}

#endif // ARCHITECTURE_IA32
#endif // ES_NATIVE_SUPPORT
