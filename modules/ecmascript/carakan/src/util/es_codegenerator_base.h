/* -*- mode: c++; indent-tabs-mode: nil; tab-width: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_CODEGENERATOR_BASE_H
#define ES_CODEGENERATOR_BASE_H

#include "modules/memory/src/memory_executable.h"

/* The following needs to be defined before including this file:

     typedef XXX ES_NativeInstruction;
     enum ES_NativeJumpCondition;

   The enumeration ES_NativeJumpCondition needs to have ES_NATIVE_UNCONDITIONAL
   as one of the enumerators.  It's integer value is insignificant.

   Optionally define ES_FUNCTION_CONSTANT_DATA_SECTION to place constants into
   separate per function sections that are addressed relative a dedicated base
   register. The default is to intersperse constants with the code and use PC
   relative addressing.

   When using constant data sections, define ES_DATA_SECTION_MAX_SIZE to the
   maximum allowable section size, and ES_DATA_SECTION_MAX_POSITIVE_OFFSET to
   the maximum distance from the base register that can be addressed using
   positive values.

   When using PC relative addressing optionally define ES_MAXIMUM_BLOCK_LENGTH
   to the maximum number of ES_NativeInstruction entities allowed in a
   ES_CodeGenerator::Block to avoid overflowing the PC relative offset.

   Additionally, ES_NATIVE_MAXIMUM_INSTRUCTION_LENGTH must be defined one way or
   another to evaluate to an integer that defines how long the longest
   instruction that might be generated is, in ES_NativeInstruction entities. */

class ES_CodeGenerator;

/** Base class for native code generator utility class, with shared
    functionality for managing jump targets, basic blocks and out-of-order
    blocks.

    For each support architecture there will be a sub-class that implements the
    GetCode() function and adds functions for actually emitting instructions. */
class ES_CodeGenerator_Base
{
public:
    class Block;
    class OutOfOrderBlock;
    class JumpTarget;

    ES_CodeGenerator_Base(OpMemGroup *arena = NULL);
    /**< Constructor.  The 'arena' argument is an arena from which all temporary
         memory is allocated.  Can be NULL, in which case all such memory is
         allocated elsewhere and freed by the destructor. */
    ~ES_CodeGenerator_Base();
    /**< Destructor.  Frees any owned resources. */

    void Reset();
    /**< Reset the code generator to its initial state.  All generated code is
         thrown away.  Equivalent to destroying the object and allocating a new
         one. */

    virtual const OpExecMemory *GetCode(OpExecMemoryManager *execmem, void *constant_reg = NULL) = 0;
    /**< Perform the final code generation into executable memory allocated from
         'execmem'.  Finalize() must be called to actually make the code
         executable.  Until that is done, the returned memory is still writable
         and not executable, so if it needs to be modified (for instance by
         storing the data of a "blob" constant) this needs to be done between
         the call to GetData() and the call to Finalize().

         Implemented by the architecture specific sub-class. */

    void Finalize(OpExecMemoryManager *execmem, const OpExecMemory *code);
    /**< Finalize the executable memory by making it read-only and executable. */

#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
    OP_STATUS CreateDataSection(ES_NativeCodeBlock *ncblock);
    /**< Allocate and populate the constant data section.

         May return ERR_NOT_SUPPORTED if the section grows larger than
         ES_DATA_SECTION_MAX_SIZE. It's preferred that ES_Native::CheckLimits()
         tries to detect this situation beforehand thus avoiding a costly
         code generation. */
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION

    /** Helper class representing a constant stored in memory near the final
        generated code.  Typical use case is when instruction set limitations
        forces use of relative address modes (relative the instruction pointer)
        or when the data is inherently specific to the generated code and best
        stored in conjunction with it. */
    class Constant
    {
    private:
        friend class ES_CodeGenerator_Base;
        friend class ES_CodeGenerator_Base::Block;
        friend class ES_CodeGenerator;

        Constant()
        {
            address_of = NULL;
            first_use = NULL;
#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
            instance = NULL;
#else // ES_FUNCTION_CONSTANT_DATA_SECTION
            first_instance = NULL;
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION
        }

        unsigned size;
        /**< Size of the stored data. */
        unsigned alignment;
        /**< Alignment requirement. */
        Constant *address_of;
        /**< A secondary constant whose value will be a pointer to this
             constant. */

        Constant *next;
        /**< Next constant. */

        /** Helper class representing a reference to the constant. */
        class Use
        {
        public:
            Constant *constant;
            /**< The referenced constant. */
            ES_CodeGenerator_Base::Block *block;
            /**< Block in which the reference occured. */
            unsigned offset;
            /**< Offset into the block to where the reference is.  Exactly what
                 this points to and how the reference is resolved depends on the
                 architecture. */

            Use *next_per_constant;
            /**< Next reference to this constant in any block. */
            Use *next_per_block;
            /**< Next reference to any constant in the same block. */
        };

        Use *first_use;
        /**< First reference to the constant. */

        void AddUse(ES_CodeGenerator_Base::Block *block, unsigned offset, OpMemGroup *arena);
        /**< Records a reference to the constant. */

#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
        int offset;
        /**< Offset from base register. */

        void *instance;
        /**< Pointer to the location in the final generated data section. */
#else // ES_FUNCTION_CONSTANT_DATA_SECTION
        /** Helper class representing an instance of the constant. */
        class Instance
        {
        public:
            unsigned offset;
            /**< Offset from the start of the final generated code to start of
                 the instance of the constant. */

            Instance *next;
            /**< Next instance. */
        };

        Instance *first_instance;
        /**< First instance of the constant. */

        void AddInstance(unsigned offset, OpMemGroup *arena);
        /**< Records an instance of the constant. */
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION

        enum Type { TYPE_INT, TYPE_DOUBLE, TYPE_2DOUBLE, TYPE_POINTER, TYPE_BLOB };

        Type type;
        /**< Type of data stored in the constant. */

        /** The data to store.  In the case of a "blob" constant, the data is
            written later. */
        union
        {
            int i;
            double d;
            struct { double d1, d2; } d2;
            const void *p;
        } value;
    };

    /** Helper class representing roughly a "basic block."  The beginning of a
        block is the only possible target of a jump instruction, and the jumps
        can only occur at the end of a block, so the code is typically divided
        into blocks at jump targets and jump instructions.

        Instructions are written one block at a time into a buffer of non-
        executable memory.  Blocks are not necessarily written into this buffer
        in the "right" order; they are reordered when the final code is
        generated into the actual executable memory.

        The primary mechanism for unorder is out-of-order blocks.  See the
        documentation of 'OutOfOrderBlock' for more information. */
    class Block
    {
    private:
        friend class ES_CodeGenerator_Base;
        friend class ES_CodeGenerator_Base::JumpTarget;
        friend class ES_CodeGenerator_Base::Constant;
        friend class ES_CodeGenerator;

        /* Jump to beginning of block: */

        BOOL is_jump_target;
        /**< TRUE if the beginning of this block is a jump target, that is, if
             there's a JumpTarget whose 'block' member points to this block. */
        BOOL align_block;
        /**< TRUE if the beginning of this block should be aligned to optimize
             jumps to it.  Only used if 'is_jump_target' is TRUE. */

        /* Jump at end of block: */

        ES_NativeJumpCondition jump_condition;
        /**< If this block ends with a jump, this is the condition of that jump.
             Values are defined by the architecture.  There will be one value
             that represents an unconditional jump.  There will also be one
             value that represents that this isn't a jump. */
        unsigned jump_target_id;
        /**< If this block ends with a jump, this is the ID of the JumpTarget to
             which that jump jumps. */
        Block *jump_target;
        /**< If this block ends with a jump, this block is the target of that
             jump.  Will be NULL for forward jumps until the jump has been
             resolved. */
        unsigned jump_size;
        /**< If this block ends with a jump, this is the size of the jump.  Used
             only during final code generation, and only on some
             architectures. */
        BOOL hint;
        /**< When this block ends with a conditional jump:
             On IA32, TRUE if there's a static hint whether that jump will
             be taken or not.
             On MIPS, TRUE if the preceding instruction should be placed in the
             jump delay slot, otherwise pad with NOP. */
        BOOL branch_taken;
        /**< If 'hint' is TRUE, this is the hint: TRUE if the jump will probably
             be taken, FALSE if the jump will probably not be taken. */
        BOOL jump_is_call;
        /**< if this block ends with a jump, TRUE if that jump should be a
             "subroutine call," FALSE if it should be a plain jump.  Note: not
             all architectures support conditional subroutine calls. */

        /* Location in 'buffer': */

        unsigned start;
        /**< Index of first MachineInstruction element in 'buffer' of this
             block.  Because of out-of-order blocks and alignments and such this
             doesn't at necessarily at all represent where the block will be in
             the final generated code. */
        unsigned end;
        /**< Index of first MachineInstruction element in 'buffer' after this
             block. */

        /* Location in final generated code: */

        unsigned actual_start;
        /**< Used during final code generation as the index of the first
             MachineInstruction element of this block in the final generated
             code. */
        unsigned actual_end;
        /**< Used during final code generation as the index of the first
             MachineInstruction element after this block in the final generated
             code. */

#ifdef ES_CODEGENERATOR_RELATIVE_CALLS
        /* Relative call resolution: */

        /** Helper class representing a relative call inside this block, used
            during final code generation to fix the call's offset. */
        class RelativeCall
        {
        public:
            unsigned offset;
            /**< Offset into the block of the first MachineInstruction of the
                 call. */
            void *address;
            /**< Absolute address of the function to call. */

            RelativeCall *next;
            /**< Next relative call. */
        };

        RelativeCall *relative_calls;
        /**< First relative call in the block, or NULL if there are none. */
#endif // ES_CODEGENERATOR_RELATIVE_CALLS

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
        /* Disassembly annotation. */
        class Annotation
        {
        public:
            char *value;
            /**< Annotation value.  Should be one or more lines, each terminated
                 by a line feed character. */

            unsigned offset;
            /**< Offset into the block of the first MachineInstruction to be
                 annotated. */

            Annotation *next;
            /**< Next annotation. */
        };

        Annotation *annotations;
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

#ifdef ES_FUNCTION_CONSTANT_DATA_SECTION
        /** Helper class to represent where the base register is loaded with
            a pointer to the constant data section. */
        class DataSectionPointer
        {
        public:
            unsigned offset;
            /**< Offset into the block to where the the load occured.
                 Exactly what this points to and how the load is resolved
                 depends on the architecture. */

            DataSectionPointer *next;
            /**< Next load. */
        };

        DataSectionPointer *first_data_section_pointer;
        /**< The list of data section pointer loads in the block. */

        void AddDataSectionPointerLoad(unsigned offset, OpMemGroup *arena);
        /**< Register a data section load at the given offset in the block. */
#endif // ES_FUNCTION_CONSTANT_DATA_SECTION

        /* Administrative: */

        OutOfOrderBlock *ooo_block;
        /**< The out-of-order block that this block belongs to, or NULL if it
             doesn't belong to one. */

        Block *previous;
        /**< Previous block, either in the code generator's global list, or in
             owner out-of-order block's list. */
        Block *next;
        /**< Next block, either in the code generator's global list, or in
             owner out-of-order block's list. */

        Block *next_with_jump;
        /**< Next block that ends with an unresolved forward jump. */

        Constant::Use *first_constant_use;
        /**< First reference to a constant in this block. */

        Constant::Use **constant_use;
        /**< Points either to 'first_constant_use' or the 'next_per_block'
             pointer of the last use in the list. */

#ifdef _DEBUG
        unsigned maximum_distance, min_calculated_length;
#endif // _DEBUG
    };

    void StartPrologue();
    /**< Start generating prologue.  All instructions generated from now until
         EndPrologue() is called are inserted at the beginning of the final
         generated code.  This supports generating the body of a function first,
         then the prologue and epilogue with whatever code turned out to be
         needed while generating the body. */

    void EndPrologue();
    /**< Stop generating prologue.  Instructions generated from now on will be
         inserted where they would have been before StartPrologue() was
         called. */

    Block *SetProloguePoint();
    /**< Define that the current location is the appropriate place to start
         inserting instructions after the next call to StartPrologue().  The
         returned block is the previous "prologue point."

         This function, together with ResetProloguePoint(), can be used to
         generate a function complete with prologue and all entirely inside
         another function, which might be the case if a function is inlined into
         another. */

    void ResetProloguePoint(Block *prologue_point);
    /**< Restores the "prologue point" to where it was before SetProloguePoint()
         was called (assuming the 'prologue_point' argument is the block that
         SetProloguePoint() returned.) */

    /** Helper class representing a portion of code that is generated in the
        middle of a code flow it shouldn't be part of.  Typically used for the
        "else" branch of an "if" where the condition is expected to be true.
        For cache locality, the unlikely branch is moved out of the way.

        An out-of-order block has two jump targets.  One is the jump target to
        jump to to enter the alternate code path, the other one, called the
        "continuation target," is jumped to at the end of the code path.  The
        "caller" is responsible for defining where the continuation target is.

        If the code in the out-of-order block unconditionally jumps away or
        returns from the function, the automatic jump to the continuation target
        can be skipped. */
    class OutOfOrderBlock
    {
    public:
        JumpTarget *GetJumpTarget() { return target; }
        /**< Returns the jump target to jump to to enter the out-of-order
             block. */
        JumpTarget *GetContinuationTarget() { return continuation; }
        /**< Returns the jump target that the out-of-order block jumps to at the
             end. */

    private:
        friend class ES_CodeGenerator_Base;
        friend class ES_CodeGenerator;

        Block *first_block;
        /**< First block in the out-of-order block. */
        Block *last_block;
        /**< Last block in the out-of-order block. */
        Block *previous_current_block;
        /**< The current block before generation of the out-of-order block
             began.  When generation of the out-of-order block finishes, this
             becomes the current block again. */

        JumpTarget *target;
        /**< Returned by GetJumpTarget(). */
        JumpTarget *continuation;
        /**< Returned by GetContinuationTarget(). */

        BOOL align_block;
        /**< TRUE if the first block of the out-of-order block should be
             aligned. */
        BOOL finalized;
        /**< TRUE if SetOutOfOrderContinuationPoint() has been used. */

#ifdef NATIVE_DISASSEMBLER_SUPPORT
        TempBuffer annotation_buffer;
        /**< Current annotation value; moved to the current block by Reserve(). */
#endif // NATIVE_DISASSEMBLER_SUPPORT

        OutOfOrderBlock *previous;
        /**< Previous out-of-order block. */
        OutOfOrderBlock *next;
        /**< Next out-of-order block. */

        OutOfOrderBlock *interrupted;
        /**< The out-of-order block containing this out-of-order block. */
    };

    OutOfOrderBlock *StartOutOfOrderBlock(BOOL align_block = FALSE);
    /**< Start generation of a new out-of-order block.  The instructions
         generated from now until EndOutOfOrderBlock() is called will not be in
         the current control flow, and will thus not get executed unless
         explicitly jumped to.  Typical use case is a "slow case", such as in

           if (type(a) == int32 && type(b) == int32)
             r = int32_add(a, b);
           else
             handle_all_other_cases();

           return r;

         where the condition checks and the "int32_add()" code is in the main
         control flow, and the "handle_all_other_cases()" code is in an
         out-of-order block.  The code in the out-of-order block would typically
         be generated first, between calls to StartOutOfOrderBlock() and
         EndOutOfOrderBlock(), then the condition is evaluated, resulting in one
         or more conditional jumps to the out-of-order block's jump target,
         followed by the "int32_add()" code.  Then, before "return" is
         generated, SetOutOfOrderContinuationPoint() is used to define where
         control should return to after the out-of-order block, and finally
         "return" is generated.  In the resulting machine code, the condition
         check, the "int32_add()" and the "return" will be adjacent in memory,
         while the "handle_all_other_cases()" code will be somewhere else,
         leading to good cache locality of the code that we think will run.

         @param align_block If TRUE, the first instruction in the out-of-order
                            block will be aligned at a cache line boundary
                            or whatever other point that is advantageous.

         @return The out-of-order block. */

    BOOL EndOutOfOrderBlock(BOOL jump_to_continuation_point = TRUE);
    /**< End generation of the code in an out-of-order block.  This
         automatically emits a jump to the out-of-order block's continuation
         jump target, unless 'jump_to_continuation_point' is FALSE.

         @return FALSE if the out-of-order block contains zero instructions,
                 TRUE otherwise. */

    void SetOutOfOrderContinuationPoint(OutOfOrderBlock *ooo_block, BOOL align_target = FALSE);
    /**< Define the "contination point" of an out-of-order block.  It works the
         same way as SetJumpTarget(). */

    /** Helper class representing a jump target, that is, a point in the code
        that can be reached via a jump.  It can be passed to Jump() to emit an
        normal jump to it, or its address can be extracted after final code
        generation for more advanced uses. */
    class JumpTarget
    {
    public:
        void *GetAddress(void *base) { return static_cast<ES_NativeInstruction *>(base) + block->actual_start; }
        /**< Returns the absolute address of the targetted instruction,
             relative the beginning of the final generated code. */

    private:
        friend class ES_CodeGenerator_Base;
        friend class ES_CodeGenerator;

        JumpTarget *next;
        /**< Next jump target. */
        JumpTarget *previous;
        /**< Previous jump target. */

        Block *block;
        /**< The block whose beginning is targetted. */
        Block *first_with_jump;
        /**< First block that ends with a jump to this target. */
        unsigned id;
        /**< Unique ID. */
        unsigned uses;
        /**< Number of uses. */
    };

    JumpTarget *Here(BOOL align_target = FALSE);
    /**< Allocates and returns a jump target targetting the next generated
         instruction.  Automatically ends the current block and starts a new
         one. */

    JumpTarget *ForwardJump();
    /**< Allocates and returns a jump target targetting an instruction that will
         be generated in the future.  SetJumpTarget() must be used to define the
         actual target, but the jump target can be used to generate jumps before
         that. */

    void Jump(JumpTarget *target, ES_NativeJumpCondition condition = ES_NATIVE_UNCONDITIONAL, BOOL hint = FALSE, BOOL branch_taken = TRUE);
    /**< Generate a jump instruction jumping to the instruction specified by
         'target'.  The 'condition' argument can be used to generate a
         conditional jump.  The 'hint' and 'branch_taken' arguments can be used
         generate static branch prediction hints, if the architecture supports
         it.  (If the architecture doesn't support them, they will just be
         ignored.)

         On MIPS the hint is used to indicate if the preceding instruction
         is safe to move into the jump delay slot (branch_taken is unused).

         This function doesn't actually emit an instruction, it merely records
         in the current block that it ends in a jump.  The actual jump
         instruction is generated during final code generation. */

    void Call(JumpTarget *target, ES_NativeJumpCondition condition = ES_NATIVE_UNCONDITIONAL, BOOL hint = FALSE, BOOL branch_taken = TRUE);
    /**< Generate a call instruction calling the "sub-routine" starting with the
         instruction specified by 'target'.  The 'condition' argument can be
         used to generate a conditional jump.  The 'hint' and 'branch_taken'
         arguments can be used generate static branch prediction hints, if the
         architecture supports it.  (If the architecture doesn't support them,
         they will just be ignored.)

         Conditional calls are not suppoted on MIPS.

         This function doesn't actually emit an instruction, it merely records
         in the current block that it ends in a call.  The actual call
         instruction is generated during final code generation. */

    void SetJumpTarget(JumpTarget *target, BOOL align_target = FALSE, Block *target_block = NULL);
    /**< Define the instruction to which a jump target previously created using
         ForwardJump() jumps.  The target instruction is the next generated
         instruction. */

    Constant *NewConstant(int i, unsigned alignment = sizeof(int));
    /**< Allocate and return a constant of type int. */
    Constant *NewConstant(double d, unsigned alignment = sizeof(double));
    /**< Allocate and return a constant of type double. */
    Constant *NewConstant(double d1, double d2, unsigned alignment = 2 * sizeof(double));
    /**< Allocate and return a constant of type "pair of doubles."  Usable for
         SSE2 data. */
    Constant *NewConstant(const void *p, unsigned alignment = sizeof(void *));
    /**< Allocate and return a constant of type pointer. */

    Constant *NewBlob(unsigned size, unsigned alignment = sizeof(void *));
    /**< Allocate and return a constant of type "blob."  This is simply a blob
         of arbitrary data.  The caller can fetch a pointer to the actual
         location of the blob after final code generation, at which point the
         actual data can be written. */

    Constant *AddressOf(Constant *constant);
    /**< Return a constant whose value is the address of 'constant'. */

    void *GetBlobStorage(void *base, Constant *constant);
    /**< Fetch a pointer to the final local of a blob constant. */

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
    void Annotate(const uni_char *string, unsigned length = UINT_MAX);
    /**< Add annotation to be output when disassembling.  Applies to (and will
         be output right before) the next instruction that is emitted. */
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT

protected:
    OpMemGroup *arena;
    /**< Arena used for allocations.  This is either the argument to the
         constructor or a pointer to 'local_arena' if the argument to the
         constructor was NULL. */
    OpMemGroup local_arena;
    /**< Fallback arena. */

    Block *first_block;
    /**< First block if the main control flow of the generated code.  The order
         of this list determines the order that instructions are stored in the
         final generated code. */
    Block *last_block;
    /**< Last block if the main control flow of the generated code. */
    Block *current_block;
    /**< Current block. */
    Block *prologue_point;
    /**< Prologue point, when overridden by SetProloguePoint().  If NULL, the
         prologue point is simply at the beginning of the generated code. */

    Block *free_blocks;
    /**< Blocks are allocated in chunks of 64.  This is the pointer to the first
         free block in the current chunk (or NULL if no chunk has been
         allocated.) */
    Block *free_blocks_end;
    /**< End of current chunk.  If 'free_blocks==free_blocks_end' a new chunk
         needs to be allocated. */

    OutOfOrderBlock *first_ooo_block;
    /**< First out-of-order block.  The order in this list typically not
         important. */
    OutOfOrderBlock *last_ooo_block;
    /**< Last out-of-order block. */
    OutOfOrderBlock *current_ooo_block;
    /**< Current out-of-order block, or NULL if the main control flow is being
         generated. */

    JumpTarget *first_target;
    /**< First jump target.  The order of this list is not significant. */
    JumpTarget *last_target;
    /**< Last jump target. */

    JumpTarget *free_targets;
    /**< Jump targets are allocated in chunks of 64.  This is the pointer to the
         first free jump target in the current chunk (or NULL if no chunk has
         been allocated.) */
    JumpTarget *free_targets_end;
    /**< End of current chunk.  If 'free_targets==free_targets_end' a new chunk
         needs to be allocated. */

    unsigned jump_target_id_counter;
    /**< Counter used to generate unique jump target IDs. */

    Constant *first_constant;
    /**< First constant. */

    BOOL in_prologue;
    /**< TRUE while between calls to StartPrologue() and EndPrologue(). */

    Block *AddBlock(BOOL start_prologue = FALSE);
    /**< Create a new block.  The 'start_prologue' is used by StartPrologue() to
         force creation of a new block inserted first (or at the prologue
         point.)  If 'start_prologue' is FALSE, the new block is inserted after
         the current block.  The new block is set as the current block and
         returned. */

    void FinalizeBlockList();
    /**< Finalize the list of blocks by appending all blocks contained in each
         out-of-order block at the end of the main control flow list, and
         defining the jump target for each of the out-of-order blocks. */

    void Reserve();
    /**< Ensure there is enough space in 'buffer' to write one more instruction.
         Also allocate a new block if necessary (for instance if the current
         block ends in a jump.)  This function must be called at the beginning
         of every architecture function that generates an instruction. */

    ES_NativeInstruction *buffer_base;
    /**< The base of the current buffer. */
    ES_NativeInstruction *buffer;
    /**< The point at which to write the next instruction. */
    ES_NativeInstruction *buffer_top;
    /**< The end of the current buffer.  If 'buffer==buffer_top' the buffer is
         exactly full. */

    unsigned GetBufferUsed() { return buffer - buffer_base; }
    /**< Returns the number of ES_NativeInstruction elements used in 'buffer'. */

    void GrowBuffer();
    /**< Grow the current buffer by an appropriate amount. */

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
    TempBuffer annotation_buffer;
    /**< Current annotation value; moved to the current block by Reserve(). */

    void AddAnnotation();
    /**< Add annotation if current annotation buffer isn't empty. */
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
};

inline void
ES_CodeGenerator_Base::Reserve()
{
    if (!current_block || current_block->end != UINT_MAX || current_block->jump_condition != ES_NATIVE_NOT_A_JUMP || current_block->ooo_block != current_ooo_block)
        AddBlock();

    if (current_block->start == UINT_MAX)
        current_block->start = buffer - buffer_base;
#ifdef ES_MAXIMUM_BLOCK_LENGTH
    else
    {
        if (buffer && ((buffer - buffer_base) - current_block->start) > ES_MAXIMUM_BLOCK_LENGTH)
            AddBlock();
    }
#endif // ES_MAXIMUM_BLOCK_LENGTH

    if (buffer == NULL || buffer_top - buffer < ES_NATIVE_MAXIMUM_INSTRUCTION_LENGTH)
        GrowBuffer();

#ifdef NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
    AddAnnotation();
#endif // NATIVE_DISASSEMBLER_ANNOTATION_SUPPORT
}

#endif // ES_CODEGENERATOR_BASE_H
