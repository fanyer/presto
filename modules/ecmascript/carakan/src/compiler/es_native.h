/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#ifndef ES_NATIVE_H
#define ES_NATIVE_H

#include "modules/ecmascript/carakan/src/compiler/es_native_arch.h"
#include "modules/memory/src/memory_executable.h"

#ifdef NATIVE_DISASSEMBLER_SUPPORT
# include "modules/ecmascript/carakan/src/compiler/es_disassembler.h"
#endif // NATIVE_DISASSEMBLER_SUPPORT

#ifdef ES_NATIVE_SUPPORT

/*
 * Flags describing the native code flavour.  These define limitations of the
 * architecture specific implementations of various Emit* functions, and thus
 * how the architecture independent code may use those functions.
 *
 * ES_BITWISE_SUPPORT_IMMEDIATE
 *
 *   If defined, one of the source operands to bitwise operations can be an
 *   immediate value.  If not defined, such an immediate value must be loaded
 *   into a native register first.
 *
 * ES_BITWISE_SUPPORT_VIRTUAL
 *
 *   If defined, bitwise operations can access the value in a virtual register
 *   (assuming it's of type ESTYPE_INT32) without using a native register.  If
 *   not defined, the value must be loaded into a native register first.
 *
 * ES_BITWISE_EXPLICIT_TARGET
 *
 *   If defined, bitwise operations have three effective operands, one target
 *   operand and two source operands.  If not defined, they only have two
 *   operands, where one is both source and target, and thus one of the given
 *   source operands must be the same as the target operand.
 */

class ES_Function;

class ES_NativeCodeBlock
    : public Link
{
public:
    enum Type
    {
        TYPE_FUNCTION,
        TYPE_CONSTRUCTOR,
        TYPE_LOOP,
        TYPE_DEPRECATED
    };

    ES_NativeCodeBlock(ES_Context *context, Type type);
    virtual ~ES_NativeCodeBlock();

    void SetExecMemory(const OpExecMemory *b) { block = b; }
    void *GetAddress() { return block->address; }

#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
    void SetDataSection(void *ds, int register_offset) { data_section = ds; base_register_offset = register_offset; }
    /**< Set the data section associated with this block of code.

         @param ds Pointer to the data section. ES_NativeCodeBlock takes
             ownership of the memory.

         @param register_offset Offsets the base register relative to the data
             section start.

             For large data sections it may be necessary to use negative offsets
             to achieve the full range of register relative addressing. In that case
             use register_offset to let the base register point into the middle of
             the data section.
    */

    void *GetDataSectionRegister() { return (char *) data_section + base_register_offset; }
    /**< Returns the address that should be loaded into the base register. */
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION

    void SetFunctionCode(ES_FunctionCode *function_code);
    void SetFunctionObject(ES_Function *function_object);
    void SetLoop(ES_Code *code, unsigned loop_index, ES_Object *global_object);

    void Reset();
    void Deprecate() { type = TYPE_DEPRECATED; }

    BOOL IsFunction() { return type == TYPE_FUNCTION; }
    BOOL IsConstructor() { return type == TYPE_CONSTRUCTOR; }
    BOOL IsLoop() { return type == TYPE_LOOP; }

    unsigned GetLoopIndex() { return data.loop.loop_index; }
    BOOL IsLoopValid(ES_Object *global_object);

#ifdef DEBUG_ENABLE_OPASSERT
    BOOL ContainsAddress(void *address) { return address >= block->address && address < static_cast<char *>(block->address) + block->size; }
#endif // DEBUG_ENABLE_OPASSERT

private:
    Type type;
    const OpExecMemory *block;

#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
    void *data_section;
    /**< Pointer to the data section associated with this code. */

    int base_register_offset;
    /**< Base register offset relative to the data section start. */
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION

    union
    {
        ES_FunctionCode *function_code;
        ES_Function *function_object;

        struct Loop
        {
            ES_Code *code;
            unsigned loop_index;
            unsigned global_object_class_id;
        } loop;
    } data;
};

#ifdef ARCHITECTURE_CAP_FPU
# define ESTYPE_BITS_SUPPORTED_NUMBER (ESTYPE_BITS_INT32 | ESTYPE_BITS_DOUBLE)
#else // ARCHITECTURE_CAP_FPU
# define ESTYPE_BITS_SUPPORTED_NUMBER ESTYPE_BITS_INT32
#endif // ARCHITECTURE_CAP_FPU

/* Bitwise operations convert their operands to int32, which makes null and undefined
   easy to handle (null/undefined -> NaN -> 0). */
#define ESTYPE_BITS_SUPPORTED_NUMBER_BITWISE (ESTYPE_BITS_SUPPORTED_NUMBER | ESTYPE_BITS_NULL | ESTYPE_BITS_UNDEFINED)

class ES_Native
    : public ES_ArchitecureMixin
{
public:
    ES_Native(ES_Execution_Context *context, ES_Code *code, OpMemGroup *arena = NULL, ES_CodeGenerator *alternate_cg = NULL);
    ~ES_Native();

    /* First argument to the trampoline function is an array of
       pointers to the following objects in this order:

         (ES_Execution_Context *)        context
         (ES_Value_Internal *)           context->reg
         (ES_NativeStackFrame **)        &context->native_stack_frame
         (unsigned char **)              &context->stack_ptr
       #ifdef ARCHITECTURE_AMD64
         (void **)                       g_ecma_instruction_handlers
       #endif // ARCHITECTURE_AMD64
         (ES_Value_Internal *)           register frame
         (void *)                        trampoline to call directly
         (void *)                        native dispatcher to call via trampoline
         (void *)                        &ES_Execution_Context::ReconstructNativeStack
         (void *)                        ES_Execution_Context::stack_limit

       The last two pointers may be the same, in which case the native
       dispatcher is simply called directly without a trampoline.  The
       trampoline is used when we want to call
       ES_Execution_Context::ReconstructNativeStack before calling the actual
       native dispatcher.
     */

    enum TrampolinePointers
    {
        TRAMPOLINE_POINTER_CONTEXT,
        TRAMPOLINE_POINTER_CONTEXT_REG,
        TRAMPOLINE_POINTER_CONTEXT_NATIVE_STACK_FRAME,
        TRAMPOLINE_POINTER_CONTEXT_STACK_PTR,
#ifdef ARCHITECTURE_AMD64
        TRAMPOLINE_POINTER_INSTRUCTION_HANDLERS,
#endif // ARCHITECTURE_AMD64
        TRAMPOLINE_POINTER_REGISTER_FRAME,
        TRAMPOLINE_POINTER_TRAMPOLINE_DISPATCHER, // real dispatcher or trampoline
        TRAMPOLINE_POINTER_NATIVE_DISPATCHER,     // real dispatcher
        TRAMPOLINE_POINTER_RECONSTRUCT_NATIVE_STACK,
        TRAMPOLINE_POINTER_STACK_LIMIT,
#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
        TRAMPOLINE_POINTER_DATA_SECTION_REGISTER,
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION
        TRAMPOLINE_POINTERS_COUNT
    };

    static BOOL (*GetBytecodeToNativeTrampoline())(void **, unsigned);
    static void *GetReconstructNativeStackTrampoline(BOOL prologue_entry_point);

    enum ThrowPointers
    {
        THROW_POINTER_CONTEXT,
        THROW_POINTER_CONTEXT_NATIVE_STACK_FRAME,
        THROW_POINTER_CONTEXT_STACK_PTR,

        THROW_POINTERS_COUNT
    };

    static void (*GetThrowFromMachineCode())(void **);

#ifdef DEBUG_ENABLE_OPASSERT
    static BOOL IsAddressInBytecodeToNativeTrampoline(void *address);
#endif // DEBUG_ENABLE_OPASSERT

#ifdef USE_CUSTOM_DOUBLE_OPS
    static double (*CreateAddDoubles())(double lhs, double rhs);
    static double (*CreateSubtractDoubles())(double lhs, double rhs);
    static double (*CreateMultiplyDoubles())(double lhs, double rhs);
    static double (*CreateDivideDoubles())(double lhs, double rhs);
    static double (*CreateSin())(double arg);
    static double (*CreateCos())(double arg);
    static double (*CreateTan())(double arg);
#endif // USE_CUSTOM_DOUBLE_OPS

    enum { LIGHT_WEIGHT_MAX_REGISTERS = 8 };

    ES_NativeCodeBlock *CreateNativeDispatcher(ES_CodeWord *entry_point_cw = NULL, void **entry_point_address = NULL, BOOL prologue_entry_point = FALSE, BOOL light_weight_allowed = TRUE);

    ES_NativeCodeBlock *CreateNativeConstructorDispatcher(ES_Object *prototype, ES_Class *klass, ES_CodeWord *entry_point_cw = NULL, void **entry_point_address = NULL);

    BOOL CreateNativeLoopDispatcher(unsigned loop_index);
    void SetIsLoopDispatcher(unsigned loop_index0) { loop_dispatcher = TRUE; loop_index = loop_index0; }

    BOOL StartInlinedFunctionCall(ES_Class *this_object_class, unsigned argc);
    void GenerateInlinedFunctionCall(ES_CodeGenerator::OutOfOrderBlock *failure);

    OpMemGroup *Arena() { return arena; }

    OpMemGroup *arena, local_arena;

    ES_Execution_Context *context;

    ES_Code *code;
    /**< The bytecode object for which we're generating native code. If code
         needs to be updated before generating native code, make sure to set
         code by calling UpdateCode. */

    ES_FunctionCode *fncode;
    /**< NULL or static_cast<ES_FunctionCode *>(code). */

    ES_CodeOptimizationData *optimization_data;
    /**< Copied from 'code'. */

    BOOL is_trivial;
    /**< Cannot ever call a slow case. */

    BOOL is_light_weight;
    /**< Probably won't call a slow case.  Gives a native dispatcher with no
         prologue or epilogue, which, if it needs to call a slow case, first
         runs its "prologue" and then causes the generation of a non-light
         weight dispatcher and switches to it.  If the cause of the slow case
         call is eliminated later (via type profiling or property cache updates)
         another light weight dispatcher might be created. */
    BOOL light_weight_accesses_cached_globals;

    BOOL eliminate_integer_overflows;
    /**< Guess that no integer increment or decrement ever overflows, and
         generate that doesn't handle it.  If it does happen, regenerate the
         native dispatcher instead. */

    BOOL allow_inlined_calls;
    /**< Allow inlined function calls. */

    unsigned instruction_index;
    /**< Instruction index of current bytecode instruction. */
    unsigned cw_index;
    /**< Code word index of current bytecode instruction. */
    unsigned start_cw_index;
    /**< Code word index of start of the current arithmetic block. */

    ES_CodeOptimizationData::JumpTarget *current_jump_target;
    /**< Next jump target. */

    ES_CodeGenerator primary_cg;

    ES_CodeGenerator &cg;
    /**< Architecture specific code generator. */

#ifdef NATIVE_DISASSEMBLER_SUPPORT
    ES_Disassembler annotator;
#endif // NATIVE_DISASSEMBLER_SUPPORT

#ifdef _DEBUG
    char *current_disassembly;
#endif // _DEBUG

    class ArithmeticBlock;
    class NativeRegister;
    class VirtualRegister;

    class RegisterAllocation
    {
    public:
        ArithmeticBlock *arithmetic_block;
        NativeRegister *native_register;
        VirtualRegister *virtual_register;

        unsigned start;
        /**< Code word index of first instruction where this allocation is
             valid.  A read/convert allocation takes place before the specified
             instruction, a write/copy allocation takes place after the
             specified instruction. */
        unsigned end;
        /**< Code word index of last instruction where this allocation is
             valid. */

        enum Type
        {
            TYPE_READ,
            /**< At the allocation's start, the value should be read from
                 virtual register into native register.  It needn't be written
                 back during the allocation or when it ends. */
            TYPE_WRITE,
            /**< At the allocation's start, the value is calculated into the
                 native register and thus needn't be read from the virtual
                 register.  It needs to be written back, unless the virtual
                 register is a temporary register and the value is not used past
                 the allocation. */
            TYPE_COPY,
            /**< At the allocation's start, the value should be copied from a
                 native register currently allocated to the same virtual
                 register.  The two native registers may be of different types.
                 If the previous native register is a double register and the
                 new native register is an integer register, a ToInt32()
                 conversion should be performed. */
            TYPE_CONVERT,
            /**< At the allocation's start, the value should be imported from
                 the virtual register via type checks and (possibly)
                 conversion. */
            TYPE_SCRATCH,
            /**< Spceial: a scratch register for the single instruction included
                 in the allocation's range. */
            TYPE_SPECIAL
            /**< Special: the native register has a special role throughout the
                 dispatcher. */
        };

        Type type;
        BOOL initial_argument_value;
        BOOL intermediate_write;

        RegisterAllocation *native_previous, *native_next;
        RegisterAllocation *virtual_previous, *virtual_next;

        BOOL ReadFromStorage() { return type == TYPE_READ; }
        BOOL WriteToStorage() { return type == TYPE_WRITE; }
    };

    class NativeRegister
    {
    public:
        RegisterAllocation *first_allocation, *last_allocation, *current_allocation, *previous_allocation;
        /**< Allocations of this native register. */

        /* Static architecture dependent information about the register. */

        enum Type
        {
            TYPE_INTEGER,
            TYPE_DOUBLE
        };

        Type type;
        /**< Integer or double? */

        unsigned register_code;
        /**< Depends on architecture, but typically maps to the hardware number
             of the register. */

        ES_Value_Internal current_constant_value;
        /**< Current constant value (used with care within arithmetic blocks.) */
    };

    NativeRegister *native_registers;
    /**< Native registers.  If there are both integer registers and double
         registers, all integer registers must precede all double registers.  */
    unsigned native_registers_count, integer_registers_count, double_registers_count;

    NativeRegister *NR(unsigned index) { return &native_registers[index]; }
    unsigned Index(NativeRegister *nr) { return nr - native_registers; }

    BOOL IsAllocated(NativeRegister *nr) { return nr->current_allocation && nr->current_allocation->start <= cw_index && nr->current_allocation->end >= cw_index; }

    class VirtualRegister
    {
    public:
        unsigned index;
        /**< Register index, plain and simple. */

        BOOL is_temporary;
        /**< TRUE for temporary registers. */

        RegisterAllocation *first_allocation, *last_allocation, *current_allocation, *stored_allocation;

        unsigned deferred_copy_from;
        /**< If not UINT_MAX, the current value in this virtual register was
             copied from that virtual register. */

        unsigned number_of_deferred_copies;
        /**< Number of other virtual registers whose 'deferred_copy_from' names
             this register.  If this register is about to be written to, any
             deferred copies need to be executed first, and this counter is
             pretty much just here to keep track of whether we need to look for
             deferred copies from this register before writing to it. */

        unsigned number_of_uses;
        /**< Number of times this register was read from or written to. */

        BOOL integer_no_overflow;
        /**< This is an integer (even if the profiling says it any kind of
             number) and if it overflows, trigger a regeneration of the native
             dispatcher instead of handling it. */

        ES_ValueType type_known_in_arithmetic_block;
        ES_Value_Internal value_known_in_arithmetic_block;
        BOOL value_stored, value_needs_flush;

        /* Convenience functions: */

        BOOL CurrentlyAllocated() { return current_allocation != NULL; }
        /**< Valid during register allocation only. */

        ES_Native::NativeRegister *CurrentNR() { return current_allocation ? current_allocation->native_register : NULL; }

        int stack_frame_offset;
        /**< Used for virtual registers whose values are temporarily stored in
             the native stack frame instead of in the virtual register during an
             arithmetic block. */
        NativeRegister *stack_frame_register;
        /**< If 'stack_frame_offset' == INT_MAX - 1, the virtual register's
             value isn't actually stored in the stack frame at all, but in this
             native register. */
        ES_ValueType stack_frame_type;
        /**< Type of value stored in the stack frame location. */
    };

    enum PseudoVirtualRegisters
    {
        VR_IMMEDIATE = -1,
        VR_SCRATCH_ALLOCATIONS = -2,
        VR_PSEUDO_COUNT = -3
    };

    VirtualRegister *virtual_registers;
    unsigned virtual_registers_count;

    VirtualRegister *VR(unsigned index) { return &virtual_registers[index]; }

    /* "Magic" virtual registers. */
    VirtualRegister *ImmediateVR() { return &virtual_registers[VR_IMMEDIATE]; }
    VirtualRegister *ScratchAllocationsVR() { return &virtual_registers[VR_SCRATCH_ALLOCATIONS]; }

    void Use(VirtualRegister *vr) { ++vr->number_of_uses; if (vr->current_allocation && vr->current_allocation->start > cw_index) vr->current_allocation->start = cw_index; }

    ES_CodeWord *CurrentCodeWord() { return &code->data->codewords[cw_index]; }

    BOOL IsAllocated(VirtualRegister *vr) { return vr->current_allocation && vr->current_allocation->start <= cw_index; }

    BOOL IsReadAllocated(VirtualRegister *vr);
    BOOL IsWriteAllocated(VirtualRegister *vr);

    NativeRegister *ReadNR(VirtualRegister *vr);
    /**< Native register for reading 'vr' at current instruction. */
    NativeRegister *WriteNR(VirtualRegister *vr);
    /**< Virtual register for writing 'vr' at current instruction. */

    NativeRegister *GetScratchNR();

    class JumpTarget
    {
    public:
        ES_CodeOptimizationData::JumpTarget *data;
        /**< The analyzis data for this jump target. */

        ES_CodeGenerator::JumpTarget *forward_jump;
        /**< One jump target pointer for each instruction that jumps forward to
             here (or NULL if there are none.)  When the target instruction is
             found, each of these jump targets are "aimed" to the current
             machine code location. */

        ES_CodeGenerator::JumpTarget *here;
        /**< If there are any backwards jumps to this instruction, this jump
             target is allocated as their destination. */
    };

    JumpTarget *jump_targets, *next_jump_target;
    unsigned jump_targets_count;

    class SwitchTable
    {
    public:
        SwitchTable(ES_CodeStatic::SwitchTable *data1, ES_Code::SwitchTable *data2)
            : data1(data1),
              data2(data2),
              targets(NULL),
              default_target(NULL),
              blob(NULL),
              next(NULL)
        {
        }

        ES_CodeStatic::SwitchTable *data1;
        ES_Code::SwitchTable *data2;
        ES_CodeGenerator::JumpTarget **targets, *default_target;
        ES_CodeGenerator::Constant *blob;

        SwitchTable *next;
    };

    SwitchTable *first_switch_table, *last_switch_table;

    BOOL is_allocating_registers;

    BOOL can_specialize;
    BOOL specialized;

    ES_CodeWord *entry_point_cw;
    unsigned entry_point_cw_index;
    BOOL prologue_entry_point;
    ES_CodeGenerator::JumpTarget *entry_point_jump_target;

    BOOL constructor;
    ES_Object *constructor_prototype;
    ES_Class *constructor_class;
    ES_Class *constructor_final_class;
    unsigned constructor_final_class_count;

    BOOL is_inlined_function_call;
    ES_Class *this_object_class;
    unsigned inlined_call_argc;

    class Operand
    {
    public:
        Operand() : native_register(NULL), virtual_register(NULL) {}
        Operand(ES_Native::NativeRegister *native_register) : native_register(native_register), virtual_register(NULL), value(NULL), codeword(NULL) {}
        Operand(ES_Native::VirtualRegister *virtual_register) : native_register(NULL), virtual_register(virtual_register), value(NULL), codeword(NULL) {}
        Operand(ES_Native::VirtualRegister *virtual_register, const double *value) : native_register(NULL), virtual_register(virtual_register), value(value), codeword(NULL) {}
        Operand(ES_Native::VirtualRegister *virtual_register, const ES_CodeWord *codeword) : native_register(NULL), virtual_register(virtual_register), value(NULL), codeword(codeword) {}

        BOOL IsValid() const { return native_register != NULL || virtual_register != NULL; }
        BOOL IsNative() const { return native_register != NULL; }
        BOOL IsVirtual() const { return virtual_register != NULL && !IsImmediate(); }
        BOOL IsImmediate() const { return native_register == NULL && (value != NULL || codeword != NULL); }
        BOOL operator==(const Operand &other) const { return !IsImmediate() && native_register == other.native_register && virtual_register == other.virtual_register; }
        BOOL operator!=(const Operand &other) const { return !(*this == other); }

        ES_Native::NativeRegister *native_register;
        ES_Native::VirtualRegister *virtual_register;
        const double *value;
        const ES_CodeWord *codeword;
    };

    Operand InOperand(ArithmeticBlock *arithmetic_block, VirtualRegister *vr, BOOL ignore_register_allocation = FALSE, BOOL prioritize_immediate = FALSE);
    Operand InOperand(VirtualRegister *vr, ES_CodeWord *immediate);
    Operand OutOperand(VirtualRegister *vr);

    BOOL loop_dispatcher;
    ES_Code::LoopIO *first_loop_io;
    BOOL owns_loop_io;
    unsigned loop_index, loop_io_start, loop_io_end;
    ES_Class *global_object_class;

    ES_Code::LoopIO *AddLoopIO(ES_Code::LoopIO::Type type, JString *name, unsigned index, BOOL input, BOOL output);
    ES_Code::LoopIO *FindLoopIO(JString *name);
    void ReleaseLoopIOChain();

    void InitializeJumpTargets();
    /**< Initialize jump targets. */

    JumpTarget *FindJumpTarget(unsigned target_index, BOOL optional = FALSE);
    /**< Find jump target record for jumps to 'target_index'. */

    void InitializeVirtualRegisters();
    /**< Allocate 'virtual_registers' array and initialize it. */

    RegisterAllocation *StartAllocation(RegisterAllocation::Type type, ArithmeticBlock *arithmetic_block, NativeRegister *native_register, VirtualRegister *virtual_register, BOOL make_current);

    class FunctionCall
    {
    public:
        ES_CodeGenerator::JumpTarget *return_target;
        ES_CodeWord *codeword;

        FunctionCall *next;
    };

    FunctionCall *first_function_call;
    unsigned function_call_count;

    void SetFunctionCallReturnTarget(ES_CodeGenerator::JumpTarget *return_target);

    class ArithmeticInstructionProfile
    {
    public:
        ArithmeticInstructionProfile() : target_vr(NULL), lhs_vr(NULL), rhs_vr(NULL), target_type(ESTYPE_UNDEFINED) {}

        VirtualRegister *target_vr, *lhs_vr, *rhs_vr;
        ES_ValueType target_type;

        unsigned lhs_handled:8;  ///< LHS types that will be handled inline (that is, not fail to slow case)
        unsigned lhs_possible:8; ///< LHS types that can possibly occur (0xff means anything, same as lhs_handled means no slow case)
        unsigned rhs_handled:8;  ///< RHS types that will be handled inline (that is, not fail to slow case)
        unsigned rhs_possible:8; ///< RHS types that can possibly occur (0xff means anything, same as rhs_handled means no slow case)
        unsigned target_constant:1;
        unsigned lhs_constant:1;
        unsigned rhs_constant:1;
        unsigned truncate_to_int32:1;

        int lhs_immediate, rhs_immediate, target_immediate;
    };

    BOOL StartsArithmeticBlock(unsigned instruction_index, unsigned &failing_index, BOOL &uncertain_instruction);
    BOOL ContinuesArithmeticBlock(unsigned start_instruction_index, unsigned instruction_index, unsigned &failing_index, BOOL &uncertain_instruction);

    void ProfileArithmeticInstruction(unsigned start_instruction_index, unsigned instruction_index, BOOL &uncertain_instruction);

    ArithmeticInstructionProfile *instruction_profiles;

    class ArithmeticBlock
    {
    public:
        ArithmeticBlock(unsigned index)
            : index(index),
              returns(FALSE),
              instruction_profiles(NULL),
              first_delayed_copy(NULL),
              slow_case(NULL)
        {
        }

        unsigned index;
        unsigned start_instruction_index, end_instruction_index, last_uncertain_instruction_index;
        unsigned start_cw_index, end_cw_index, last_uncertain_cw_index;
        BOOL returns;

        ArithmeticInstructionProfile *instruction_profiles;

        class DelayedCopy
        {
        public:
            DelayedCopy(unsigned cw_index, VirtualRegister *source, VirtualRegister *target, DelayedCopy *next)
                : cw_index(cw_index),
                  source(source),
                  target(target),
                  next(next)
            {
            }

            unsigned cw_index;
            VirtualRegister *source, *target;
            DelayedCopy *next;
        };
        DelayedCopy *first_delayed_copy;

        void AddDelayedCopy(unsigned cw_index, VirtualRegister *source, VirtualRegister *target, OpMemGroup *arena);

        ES_CodeGenerator::OutOfOrderBlock *slow_case;

        ArithmeticBlock *next;
    };

    unsigned arithmetic_block_count;
    ArithmeticBlock *first_arithmetic_block, *last_arithmetic_block;

    class TopLevelLoop
    {
    public:
        TopLevelLoop(unsigned start_cw_index, unsigned end_cw_index)
            : start_cw_index(start_cw_index),
              end_cw_index(end_cw_index),
              preconditions(NULL)
        {
        }

        unsigned start_cw_index, end_cw_index;

        class PreCondition
        {
        public:
            PreCondition(unsigned vr_index, ES_ValueType checked_type, PreCondition *next)
                : vr_index(vr_index),
                  checked_type(checked_type),
                  next(next)
            {
            }

            unsigned vr_index;
            /**< Virtual register index. */

            ES_ValueType checked_type;
            /**< Type that was checked before the loop. */

            PreCondition *next;
        };

        PreCondition *preconditions;
    };

    TopLevelLoop *current_loop;

    VirtualRegister *enumeration_object_register, *enumeration_name_register;
    unsigned enumeration_end_index;

    class InlinedFunctionCall
    {
    public:
        InlinedFunctionCall(ES_Native *native)
            : function_fetched(FALSE),
              uses_this_object(FALSE),
              native(native)
        {
        }

        unsigned get_cw_index;
        /**< Code word index of ESI_GETN_IMM or ESI_GET_GLOBAL instruction that
             fetched the function to call. */

        unsigned call_cw_index;
        /**< Code word index of ESI_CALL instruction. */

        ES_Class *this_object_class;
        /**< Class of the this object in the inlined call. */

        ES_Function *function;
        /**< Function to inline. */

        BOOL function_fetched;
        BOOL uses_this_object;

        ES_Native *native;

        InlinedFunctionCall *next;
    };

    InlinedFunctionCall *inlined_function_call;

    void CreateArithmeticBlockSlowPath();

    void TerminateAllocation(RegisterAllocation *allocation);
    RegisterAllocation *AllocateRegister(RegisterAllocation::Type allocation_type, NativeRegister::Type register_type, ArithmeticBlock *arithmetic_block, VirtualRegister *virtual_register, NativeRegister *native_register = NULL);
    RegisterAllocation *AllocateRegister(RegisterAllocation::Type allocation_type, ES_ValueType value_type, ArithmeticBlock *arithmetic_block, VirtualRegister *virtual_register, NativeRegister *native_register = NULL);

    void AllocateRegisters();
    void AllocateRegistersInArithmeticBlock(ArithmeticBlock *arithmetic_block);

    BOOL IsSafeWrite(ArithmeticBlock *arithmetic_block, VirtualRegister *vr, unsigned cw_index);
    BOOL IsIntermediateWrite(ArithmeticBlock *arithmetic_block, VirtualRegister *vr, unsigned cw_index);

    void PrepareForInput(VirtualRegister *virtual_register);
    void GenerateCode();
    void GenerateCodeInArithmeticBlock(ArithmeticBlock *arithmetic_block);

    OP_STATUS Finish(ES_NativeCodeBlock *ncblock);

#ifdef ES_VALUES_AS_NANS
    BOOL FlushToVirtualRegisters(ArithmeticBlock *arithmetic_block, BOOL only_integers = FALSE, BOOL keep_states = FALSE);
#else // ES_VALUES_AS_NANS
    void FlushToVirtualRegisters(ArithmeticBlock *arithmetic_block);
#endif // ES_VALUES_AS_NANS

    BOOL IsType(VirtualRegister *virtual_register, ES_ValueType type);
    /**< Return TRUE if the specified virtual register is known to contain a
         value of the specified type. */

    BOOL IsProfiledType(unsigned operand_index, unsigned mask);
    /**< Profiling information is available, and it says that the register
         specified by operand 'operand_index' of the current instruction has
         ever had (for input operands) or have been assigned a value of any of
         the types included in 'mask' value of type 'type' (for output
         operands.) */

    BOOL IsTrampled(VirtualRegister *virtual_register);
    /**< Returns TRUE if this register's "known" type or value can essentially
         be changed at any time, because it can be trampled via the function's
         arguments array or via the lexical scope of nested functions. */

    BOOL GetType(VirtualRegister *virtual_register, ES_ValueType &type);
    /**< Return TRUE if the specified virtual register is known to contain a
         certain type, and if so also set 'type' to that type. */

    BOOL IsImmediate(VirtualRegister *virtual_register, ES_Value_Internal &value, BOOL allow_double =  FALSE);
    /**< Return TRUE if the specified virtual register's value is known to be an
         int32 immediate. */

    void UseInPlace(VirtualRegister *virtual_register);
    /**< Signal that the virtual register's value is about to be used in-place.
         If the current value is an immediate that hasn't been written into the
         register yet, make sure to write it there now. */

    BOOL GetConditionalTargets(unsigned cw_index, Operand &value_target, ES_CodeGenerator::JumpTarget *&true_target, ES_CodeGenerator::JumpTarget *&false_target);

    static BOOL IsHandledInline(ES_Code *code, ES_CodeWord *word);
    /**< In practice: this instruction does *not* force a non-light weight
         dispatcher. */
    static BOOL HasIntrinsicSideEffects(ES_Code *code, ES_CodeWord *word);
    /**< Has side-effects without calling a slow case.  In practice: any kind of
         PUT instruction, or any instruction whose destination register stores
         the value of a formal parameter. */

    static BOOL CanAllocateObject(ES_Class *klass, unsigned nindexed);

    /** Simple data container. */
    class ObjectAllocationData
    {
    public:
        unsigned length;
        /**< Initial value of an array's length property. */
        GC_Tag gctag;
        /**< Initial value for ES_Header::bits. */
        unsigned object_bits;
        /**< Initial value for ES_Object::object_bits. */
        unsigned main_bytes;
        /**< Number of bytes to allocate for ES_Object part. */
        unsigned named_bytes;
        /**< Number of bytes to allocate for ES_Properties part. */
        unsigned indexed_bytes;
        /**< Number of bytes to allocate for ES_Compact_Indexed_Properties part. */
    };

    static void GetObjectAllocationData(ObjectAllocationData &data, ES_Class *actual_klass, ES_Class *final_klass, unsigned property_count, unsigned *nindexed, ES_Compact_Indexed_Properties *representation);
    /**< Calculate all the values with which a freshly allocated object of the
         given class (possibly with the given indexed properties representation)
         should be initialized. */

    class PropertyCache
    {
    public:
        unsigned size;
        /**< The number of usable entries in the property cache. */
        unsigned handled;
        /**< Types encountered in the property cache. */
    };

    PropertyCache *property_get_caches;
    /**< Data gathered from the get caches for the ES_Code currently being compiled. */
    PropertyCache *property_put_caches;
    /**< Data gathered from the put caches for the ES_Code currently being compiled. */

    void AnalyzePropertyCaches(ES_Code *code);
    unsigned PropertyCacheSize(unsigned index, BOOL is_get_cache) const;
    unsigned PropertyCacheType(unsigned index, BOOL is_get_cache) const;

    BOOL EliminateIntegerOverflows();

    void StartLoop(unsigned start_cw_index, unsigned end_cw_index);
    BOOL IsPreConditioned(VirtualRegister *vr, ES_ValueType type);

    BOOL CheckInlineFunction(unsigned get_cw_index, VirtualRegister *vr_function, BOOL *need_class_check_on_entry);
    BOOL CheckIsLightWeight(ES_Code *code, BOOL *accesses_cached_globals, BOOL *accesses_lexical_scope);

    BOOL CheckPropertyValueTransfer(VirtualRegister *property_value_vr);

    void SetPropertyValueTransferRegister(NativeRegister *property_value_transfer_nr, unsigned offset, BOOL needs_type_check);
    void ClearPropertyValueTransferRegister();
    void SetPropertyValueTransferFailJumpTarget(ES_CodeGenerator::JumpTarget *continuation_target);

    BOOL EmitTypeConversionCheck(ES_StorageType type, ES_StorageType source_type, ES_Native::VirtualRegister *source_vr, ES_CodeGenerator::JumpTarget *slow_case, ES_CodeGenerator::JumpTarget *&null_target, ES_CodeGenerator::JumpTarget *&int_to_double_target);
    void EmitTypeConversionHandlers(VirtualRegister *source_vr, ES_CodeGenerator::Register properties, unsigned offset, ES_CodeGenerator::JumpTarget *null_target, ES_CodeGenerator::JumpTarget *int_to_double_target);

    void EmitSlowInstructionCall();
    /**< Emit code to handle the current instruction slowly, as it cannot be handled inline.
         Will finalise and generate the required code to recover from a property value transfer
         that may currently be in flight. */

    /* Helper functions for various emits. */
    static int  ES_CALLING_CONVENTION CompareShiftyObjects(ES_Execution_Context *context, ES_Object *object1, ES_Object *object2) REALIGN_STACK;
    static int  ES_CALLING_CONVENTION GetDoubleAsSwitchValue(ES_Execution_Context *context, ES_Value_Internal *value) REALIGN_STACK;
    static int  ES_CALLING_CONVENTION InstanceOf(ES_Execution_Context *context, ES_Object *object, ES_Object *constructor) REALIGN_STACK;
    static void ES_CALLING_CONVENTION MathSin(ES_Value_Internal *reg) REALIGN_STACK;
    static void ES_CALLING_CONVENTION MathCos(ES_Value_Internal *reg) REALIGN_STACK;
    static void ES_CALLING_CONVENTION MathTan(ES_Value_Internal *reg) REALIGN_STACK;
    static void ES_CALLING_CONVENTION MathSqrt(ES_Value_Internal *reg) REALIGN_STACK;
    static void ES_CALLING_CONVENTION MathFloor(ES_Value_Internal *reg) REALIGN_STACK;
    static void ES_CALLING_CONVENTION MathCeil(ES_Value_Internal *reg) REALIGN_STACK;
    static void ES_CALLING_CONVENTION MathAbs(ES_Value_Internal *reg) REALIGN_STACK;
    static int  ES_CALLING_CONVENTION MathPow(ES_Value_Internal *reg) REALIGN_STACK;
    static void ES_CALLING_CONVENTION PrepareThisObject(ES_Execution_Context *context, ES_Value_Internal *this_vr) REALIGN_STACK;
    static void ES_CALLING_CONVENTION PrepareThisObjectLightWeight(ES_Execution_Context *context, ES_Value_Internal *this_vr) REALIGN_STACK;
    static int  ES_CALLING_CONVENTION GetDoubleAsInt(ES_Value_Internal *value);
    static int  ES_CALLING_CONVENTION GetDoubleAsIntRound(ES_Value_Internal *value);
    static void ES_CALLING_CONVENTION StoreFloatAsDouble(float *src, double *dst) REALIGN_STACK;
    static void ES_CALLING_CONVENTION StoreDoubleAsFloat(double *src, float *dst) REALIGN_STACK;
    static void ES_CALLING_CONVENTION StoreIntAsDouble(int src, double *dst) REALIGN_STACK;
    static void ES_CALLING_CONVENTION StoreIntAsFloat(int src, float *dst) REALIGN_STACK;
    static void ES_CALLING_CONVENTION StoreUIntAsDouble(unsigned src, double *dst) REALIGN_STACK;

    /* Functions implemented per architecture in es_native_$(arch).cpp: */

    void UpdateCode(ES_Code *new_code);
    /**< Updates the ES_Native object with the new code. Called when a
         new code object for a native loop dispatcher has been created. */

    BOOL CheckLimits();
    /**< Inspect ES_Native::code and return FALSE if the architecture doesn't
         support JIT compiling that code. */

    void InitializeNativeRegisters();
    /**< Set up register allocator appropriately by allocating and populating
         the 'native_registers' array. */

    void EmitRegisterTypeCheck(VirtualRegister *source, ES_ValueType value_type, ES_CodeGenerator::JumpTarget *slow_case = NULL, BOOL invert_result = FALSE);
    /**< Emit a dynamic register type check, and call instruction handler if it
         fails. */

    void EmitLoadRegisterValue(NativeRegister *target, VirtualRegister *source, ES_CodeGenerator::JumpTarget *type_check_fail = NULL);
    /**< Load a virtual register's value into a native register. */

    void EmitStoreRegisterValue(VirtualRegister *target, NativeRegister *source, BOOL write_type = TRUE, BOOL saved_condition = FALSE);
    /**< Store a native register's value into a virtual register. */

    void EmitStoreGlobalObject(VirtualRegister *target);
    void EmitStoreConstantBoolean(VirtualRegister *target, BOOL value);

    void EmitConvertRegisterValue(NativeRegister *target, VirtualRegister *source, unsigned handled, unsigned possible, ES_CodeGenerator::JumpTarget *slow_case);
    /**< Load a virtual register's value into a native register, with type check
         and conversion. */

    void EmitSetRegisterType(VirtualRegister *target, ES_ValueType type);
    /**< Set only a virtual register's type.  If 'type' is ESTYPE_UNDEFINED or
         ESTYPE_NULL this is of course sort of equivalent to setting its value
         as well. */

    void EmitSetRegisterValue(VirtualRegister *virtual_register, const ES_Value_Internal &value, BOOL write_type, BOOL saved_condition = FALSE);
    /**< Load a constant value directly into a virtual register. */

    void EmitSetRegisterValueFromStackFrameStorage(VirtualRegister *virtual_register);

    BOOL IsComplexStore(VirtualRegister *virtual_register, NativeRegister *native_register);
    /**< Returns TRUE if storing the native register to the given virtual register may side-effect the condition flags. */

    void EmitSaveCondition();
    /**< Save away processor condition flags from within a block. */

    void EmitRestoreCondition();
    /**< Restore the condition flags (EFLAGS) previously saved by EmitSaveCondition(). */

    void EmitRegisterCopy(const Operand &source, const Operand &target, ES_CodeGenerator::JumpTarget *slow_case);
    /**< Copy value from 'source' to 'target'.  If one is an integer register
         and the other is a double register, the value should be converted
         (trivial from integer to double, as by ToInt32() from double to
         integer.)*/

    void EmitRegisterCopy(VirtualRegister *source, VirtualRegister *target);

    void EmitRegisterInt32Copy(VirtualRegister *source, VirtualRegister *target);

    void EmitRegisterInt32Assign(const Operand &source, const Operand &target);
    void EmitRegisterInt32Assign(int source, NativeRegister *target);
    void EmitRegisterDoubleAssign(const double *value, const Operand &target);
    void EmitRegisterStringAssign(JString *value, const Operand &target);

    void EmitToPrimitive(VirtualRegister *source);
    /**< Emit a call to the instruction handler for ESI_TOPRIMITIVE(1) if the
         value in the register is an object. */

    void EmitInstructionHandlerCall();
    /**< Simply emit a call to an appropriate instruction handler (the
         instruction code to handle is 'word->instruction'.) */

    void EmitSlowCaseCall(BOOL failed_inlined_function = FALSE);
    void EmitLightWeightFailure();

    void EmitExecuteBytecode(unsigned start_instruction_index, unsigned end_instruction_index, BOOL last_in_slow_case);

    enum Int32ArithmeticType
    {
        INT32ARITHMETIC_LSHIFT,
        INT32ARITHMETIC_RSHIFT_SIGNED,
        INT32ARITHMETIC_RSHIFT_UNSIGNED,
        INT32ARITHMETIC_AND,
        INT32ARITHMETIC_OR,
        INT32ARITHMETIC_XOR,
        INT32ARITHMETIC_ADD,
        INT32ARITHMETIC_SUB,
        INT32ARITHMETIC_MUL,
        INT32ARITHMETIC_DIV,
        INT32ARITHMETIC_REM
    };

    void EmitInt32Arithmetic(Int32ArithmeticType type, const Operand &target, const Operand &lhs, const Operand &rhs, BOOL as_condition, ES_CodeGenerator::JumpTarget *overflow_target);
    /**< Emit int32 arithmetic instruction.  If the result isn't an
         int32 (arithmetic over- or underflow,) */

    void EmitInt32Complement(const Operand &target, const Operand &source, BOOL as_condition);
    /**< Emit int32 one's complement negation. */

    void EmitInt32Negate(const Operand &target, const Operand &source, BOOL as_condition, ES_CodeGenerator::JumpTarget *overflow_target);
    /**< Emit int32 two's complement negation. */

    void EmitInt32IncOrDec(BOOL inc, const Operand &target, ES_CodeGenerator::JumpTarget *overflow_target);
    /**< Emit increment-by-one or decrement-by-one instruction.  If
         'check_overflow' is TRUE, check for overflow and if so call slow case
         handler instead without modifying the target virtual register. */

    enum RelationalType
    {
        RELATIONAL_EQ,
        RELATIONAL_NEQ,
        RELATIONAL_LT,
        RELATIONAL_LTE,
        RELATIONAL_GT,
        RELATIONAL_GTE
    };

    void EmitInt32Relational(RelationalType relational_type, const Operand &lhs, const Operand &rhs, const Operand *value_target, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target, ArithmeticBlock *arithmetic_block);
    /**< Emit comparison between int32:s in 'lhs' and 'rhs'.  Only one of
         'value_target', 'true_target' and 'false_target' will be non-NULL.  If
         'value_target' is non-NULL, store either true (zero) or false (one)
         there depending on the outcome.  Otherwise, jump to the non-NULL jump
         target if the result is true or false, respectively, and otherwise do
         nothing (that is, let execution continue forward.) */

    enum DoubleArithmeticType
    {
        DOUBLEARITHMETIC_ADD,
        DOUBLEARITHMETIC_SUB,
        DOUBLEARITHMETIC_MUL,
        DOUBLEARITHMETIC_DIV
    };

    void EmitDoubleArithmetic(DoubleArithmeticType type, const Operand &target, const Operand &lhs, const Operand &rhs, BOOL as_condition);
    /**< Emit double arithmetic instruction. */

    void EmitDoubleRelational(RelationalType type, const Operand &lhs, const Operand &rhs, const Operand *value_target, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target, ArithmeticBlock *arithmetic_block);
    /**< Emit double arithmetic instruction. */

    void EmitDoubleNegate(const Operand &target, const Operand &source, BOOL as_condition);
    /**< Emit double negation, with type check on 'source', and upgrade from
         ESTYPE_INT32 if necessary. */

    void EmitDoubleIncOrDec(BOOL inc, const Operand &target);

    void EmitNullOrUndefinedComparison(BOOL eq, VirtualRegister *vr, const Operand *value_target, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target);
    /**< Emit equality or non-equality comparison to null or undefined.  (The
         result is the same whether the constant operand is null or undefined,
         since null and undefined compares equal.) */

    void EmitStrictNullOrUndefinedComparison(BOOL eq, VirtualRegister *vr, ES_ValueType type, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target);
    /**< Emit strict equality or non-equality comparison to null or
         undefined. */

    void EmitComparison(BOOL eq, BOOL strict, VirtualRegister *lhs, VirtualRegister *rhs, unsigned handled_types, const Operand *value_target, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target);

    void EmitInstanceOf(VirtualRegister *object, VirtualRegister *constructor, const Operand *value_target, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target);

    void EmitConditionalJump(ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target, BOOL check_implicit_boolean = FALSE, ArithmeticBlock *arithmetic_block = NULL);
    /**< Emit conditional jump according to the current value of the implicit
         boolean register. */

    void EmitConditionalJumpOnValue(VirtualRegister *value, ES_CodeGenerator::JumpTarget *true_target, ES_CodeGenerator::JumpTarget *false_target);
    /**< Emit conditional jump whose condition is ToBoolean() applied to the
         value pointed to by the current value in the 'value' register. */

    void EmitNewObject(VirtualRegister *target_vr, ES_Class *klass, ES_CodeWord *first_property, unsigned nproperties);
    void EmitNewObjectValue(ES_CodeWord *word, unsigned offset);
    void EmitNewObjectSetPropertyCount(unsigned nproperties);

    void EmitNewArray(VirtualRegister *target_vr, unsigned length, unsigned &capacity, ES_Compact_Indexed_Properties *representation);
    void EmitNewArrayValue(VirtualRegister *target_vr, ES_CodeWord *word, unsigned index);
    void EmitNewArrayReset(VirtualRegister *target_vr, unsigned start_index, unsigned end_index);

    void EmitCall(VirtualRegister *this_vr, VirtualRegister *function_vr, unsigned argc, ES_BuiltInFunction builtin = ES_BUILTIN_FN_DISABLE);
    /**< Emit a call to a bytecode function or a builtin/host function. */

    void EmitRedirectedCall(VirtualRegister *function_vr, VirtualRegister *apply_vr, BOOL check_constructor_object_result);
    /**< Emit a "redirected" call to a function. */

    void EmitApply(VirtualRegister *apply_vr, VirtualRegister *this_vr, VirtualRegister *function_vr, unsigned argc);
    /**< Emit function call via ESI_APPLY. */

    void EmitEval(VirtualRegister *this_vr, VirtualRegister *function_vr, unsigned argc);
    /**< Check that the called function really is builtin eval, and if so, call
         ES_Execution_Context::EvalFromMachineCode, otherwise do a regular
         function call. */

    ES_CodeGenerator::OutOfOrderBlock *EmitInlinedCallPrologue(VirtualRegister *this_vr, VirtualRegister *function_vr, ES_Function *function, unsigned char *mark_failure, unsigned argc, BOOL function_fetched);
    void EmitInlinedCallEpilogue(VirtualRegister *this_vr, VirtualRegister *function_vr, unsigned argc);

    ES_CodeGenerator::JumpTarget *EmitInlineResumption(unsigned class_id, VirtualRegister *object_vr, ES_CodeGenerator::JumpTarget *failure, unsigned char *mark_failure);
    /**< Emit the re-entry code that will jumped to on return from native
         dispatcher regeneration, if the regeneration happened across a
         call to a now-inlined function. If it did, then the code needs
         to perform the validity check of the object we're invoking the
         inlined function over (as the code we would otherwise return
         straight to will be after the 'standard' check performed on
         method lookup.)

         @param class_id The class id of the object having the inlined
                method.
         @param object_vr The register holding the 'this' object of the
                method call.
         @param failure Where to go on a failed class ID check.
         @param mark_failure If the check fails, write 0x1 at this
                (byte) address. This feeds back information to the
                native code generator that this particular inlining
                failed, which can be used to temper future inlining
                attempts. */

    void EmitReturn();
    /**< Emit a "slow" return. */

    void EmitNormalizeValue(VirtualRegister *vr, ES_CodeGenerator::JumpTarget *slow_case = NULL);
    /**< Emit type check on 'vr' and go to slow case if not of type
         ESTYPE_INT32 or ESTYPE_DOUBLE. Otherwise check if 'vr'
         contains an integer and downgrade to ESTYPE_INT32 and go to
         slow case otherwise.*/

    void EmitInt32IndexedGet(VirtualRegister *target, VirtualRegister *object, VirtualRegister *index, unsigned constant_index);
    void EmitInt32IndexedPut(VirtualRegister *object, VirtualRegister *index, unsigned constant_index, VirtualRegister *source, BOOL known_type, BOOL known_value, const ES_Value_Internal &value, BOOL is_push = FALSE);

    void EmitInt32ByteArrayGet(VirtualRegister *target, VirtualRegister *object, VirtualRegister *index, unsigned constant_index);
    void EmitInt32ByteArrayPut(VirtualRegister *object, VirtualRegister *index, unsigned constant_index, VirtualRegister *source, int *known_value);

    void EmitInt32TypedArrayGet(VirtualRegister *target, VirtualRegister *object, VirtualRegister *index, unsigned constant_index, unsigned type_array_bits);
    void EmitInt32TypedArrayPut(VirtualRegister *object, VirtualRegister *index, unsigned constant_index, unsigned type_array_bits, VirtualRegister *source, unsigned char source_type_bits, ES_Value_Internal *known_value, ES_ValueType *known_type);

    void EmitGetEnumerated(VirtualRegister *target, VirtualRegister *object, VirtualRegister *name);
    void EmitPutEnumerated(VirtualRegister *object, VirtualRegister *name, VirtualRegister *source);

    void EmitInt32StringIndexedGet(VirtualRegister *target, VirtualRegister *object, VirtualRegister *index, unsigned constant_index, BOOL returnCharCode = FALSE);

    class GetCacheGroup
    {
    public:
        ES_StorageType storage_type;
        /**< Common storage type of cache entries in this group. */
        BOOL single_offset;
        /**< True if all entries in this group has the same offset. */
        unsigned common_offset;
        /**< If 'single_offset', this is the offset common to all entries. */
        BOOL only_secondary_entries;
        /**< TRUE if all entries are "secondary," that is, have only a prototype
             cache whose class ID and limit checks are handled by other groups,
             with which they don't share storage type. */

        class Entry
        {
        public:
            Entry()
                : limit(0),
                  positive(FALSE),
                  prototype(NULL),
                  prototype_secondary_entry(FALSE),
                  negative(FALSE)
            {
            }

            unsigned class_id;
            /**< Class ID of this entry. */

            ES_LayoutIndex limit;
            /**< Cache limit.  UINT_MAX if no limit check is needed. */

            BOOL positive;
            /**< If TRUE, a successful limit check means the property exists at
                 'positive_offset'. */
            unsigned positive_offset;
            /**< Offset of this entry. */

            ES_Object *prototype;
            /**< If non-NULL, this is a prototype cache if the limit check
                 fails. */
            unsigned prototype_offset;
            /**< Offset in prototype object. */
            ES_StorageType prototype_storage_type;
            /**< If 'prototype' is non-NULL, this is the property's storage type
                 in the prototype.  If this is not the same as the group's
                 'storage_type', a failed limit check for this entry means we
                 should jump to another group's value copying code.  This will
                 only happen if this entry's 'positive' is TRUE. */
            BOOL prototype_secondary_entry;
            /**< If TRUE, this is a "seconday entry" for this cache record,
                 meaning that there was an entry in another group for the same
                 cache record.  This happens when the prototype cache record is
                 paired with a positive cache with a different storage type.  No
                 code should be generated for this entry.  Secondary entries
                 will always come before regular entries in a cache. */

            BOOL negative;
            /**< If TRUE, failed limit check means property doesn't exist.  If
                 FALSE, and 'prototype' is NULL, failed limit check means go to
                 slow-case. */
        };

        const Entry *entries;
        /**< Individual cache entries. */
        unsigned entries_count;
        /**< Number of cache entries. */
    };

    class GetCacheNegative
    {
    public:
        GetCacheNegative()
            : limit(0)
        {
        }

        unsigned class_id;
        /**< Class ID of this entry. */
        ES_LayoutIndex limit;
        /**< Cache limit.  UINT_MAX if no limit check is needed. */
    };

    class GetCacheAnalysis
    {
    public:
        GetCacheAnalysis()
            : groups(NULL),
              groups_count(0),
              negatives(NULL),
              negatives_count(0)
        {
        }

        const GetCacheGroup *groups;
        /**< Cache entries grouped by storage type.  Is NULL if 'groups_count'
             is zero. */
        unsigned groups_count;
        /**< Number of elements in 'groups' array.  Can be zero, if there are no
             positive cache records (or no valid/supported cache entries.) */
        const GetCacheNegative *negatives;
        /**< Cache entries for class IDs where we only know that the property
             doesn't exist.  Negative cache entries paired with positive cache
             entries are not included in this array.

             Is NULL if 'negatives_count' is zero. */
        unsigned negatives_count;
        /**< Number of elements in 'negatives' array.  Can be zero, if there are
             strictly negative cache records. */
    };

    GetCacheAnalysis *get_cache_analyses;
    /**< Analyses of get caches.  The length of this array is the value of
         'code->data->property_get_caches_count'. */

    void EmitPrimitiveNamedGet(VirtualRegister *target, ES_Object *check_object, ES_Object *property_object, unsigned class_id, unsigned property_offset, ES_StorageType storage_type, ES_Object *function);
    void EmitFetchFunction(VirtualRegister *target_vr, ES_Object *function, VirtualRegister *object_vr, unsigned class_id, BOOL fetch_value);

    void EmitNamedGet(VirtualRegister *target, VirtualRegister *object, const GetCacheAnalysis &analysis, BOOL for_inlined_function_call = FALSE, BOOL fetch_value = TRUE);

    void EmitNamedPut(VirtualRegister *object, VirtualRegister *source, JString *name, BOOL has_complex_cache, ES_StorageType source_type);
    void EmitLengthGet(VirtualRegister *target, VirtualRegister *object, unsigned handled, unsigned possible, ES_CodeGenerator::JumpTarget *slow_case);

    void EmitFetchPropertyValue(VirtualRegister *target_vr, VirtualRegister *object_vr, ES_Object *object, ES_Class *klass, unsigned property_index);

    void EmitGlobalGet(VirtualRegister *target, unsigned class_id, unsigned cached_offset, unsigned cached_type);
    void EmitGlobalPut(VirtualRegister *source, unsigned class_id, unsigned cached_offset, unsigned cached_type, ES_StorageType source_type);

    void EmitGlobalImmediateGet(VirtualRegister *target, unsigned index, BOOL for_inlined_function_call = FALSE, BOOL fetch_value = TRUE);
    void EmitGlobalImmediatePut(unsigned index, VirtualRegister *source);

    void EmitLexicalGet(VirtualRegister *target, VirtualRegister *function, unsigned scope_index, unsigned variable_index);

    void EmitFormatString(VirtualRegister *target, VirtualRegister *source, unsigned cache_index);

    void EmitTableSwitch(VirtualRegister *value, int minimum, int maximum, ES_CodeGenerator::Constant *table, ES_CodeGenerator::JumpTarget *default_target);

    ES_CodeGenerator::JumpTarget *EmitPreConditionsStart(unsigned cw_index);

    void EmitArithmeticBlockSlowPath(ArithmeticBlock *arithmetic_block);

    ES_CodeGenerator::JumpTarget *EmitEntryPoint(BOOL custom_light_weight_entry = FALSE);

    void EmitTick();

    void GeneratePrologue();
    /**< Generate dispatcher function prologue.  This function will be called
         after all regular code has been generated, since it will need to know
         what native registers have been used to know which it needs to store on
         the stack. */

    void GenerateEpilogue();
    /**< Generate dispatcher function epilogue. */

    class StackFrameStorage
    {
    public:
        int offset;
        unsigned size;

        StackFrameStorage *next;
    };

    StackFrameStorage *first_stack_frame_storage, *unused_stack_frame_storage;

    void AllocateStackFrameStorage(ArithmeticBlock *arithmetic_block, VirtualRegister *vr, ES_ValueType type);
    void ConvertStackFrameStorage(ArithmeticBlock *arithmetic_block, VirtualRegister *vr, ES_ValueType type);
    void FreeStackFrameStorage(VirtualRegister *vr);

    VirtualRegister *property_value_write_vr;
    VirtualRegister *property_value_read_vr;
    NativeRegister *property_value_nr;
    unsigned property_value_offset;
    unsigned property_value_write_offset;
    BOOL property_value_needs_type_check;
    ES_CodeGenerator::JumpTarget *property_value_fail;

    ES_CodeGenerator::JumpTarget *epilogue_jump_target;
    ES_CodeGenerator::JumpTarget *failure_jump_target;

    ES_CodeGenerator::OutOfOrderBlock *current_slow_case;
    ES_CodeGenerator::OutOfOrderBlock *light_weight_failure;
    ES_CodeGenerator::JumpTarget *light_weight_wrong_argc;
    ES_CodeGenerator::JumpTarget *current_true_target;
    ES_CodeGenerator::JumpTarget *current_false_target;

    BOOL has_called_slow_case;

    unsigned code_stack_offset;
    unsigned variable_object_stack_offset;
};

#endif // ES_NATIVE_SUPPORT
#endif // ES_NATIVE_H

