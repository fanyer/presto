/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#ifndef ES_COMPILER_H
#define ES_COMPILER_H

#include "modules/ecmascript/carakan/src/compiler/es_code.h"
#include "modules/ecmascript/carakan/src/compiler/es_parser.h"
#include "modules/util/adt/opvector.h"

class ES_SourceRecord;
class ES_Statement;
class ES_CallExpr;

class ES_Compiler
{
public:
    class JumpTarget;
    friend class JumpTarget;

    class Register;
    friend class Register;

private:
    class JumpOffset
    {
    public:
        unsigned bytecode_index;
        BOOL has_condition;
        JumpOffset *next;
    };

    JumpOffset *AllocateJumpOffset();
    void FreeJumpOffset(JumpOffset *offset);

    JumpOffset *unused_jump_offsets;

    class Jump;
    friend class Jump;

    class Jump
    {
    public:
        Jump(ES_Compiler &compiler)
            : compiler(compiler),
              refcount(0),
              local_values_snapshot(0),
              first_offset(NULL),
              first_address(NULL),
              used_in_table_switch(FALSE),
              previous(NULL),
              ptr(NULL),
              next(NULL)
        {
        }

        void SetPointer(Jump **previous_ptr)
        {
            ptr = previous_ptr;
            previous = *ptr;
            if (previous)
                previous->ptr = &previous;
        }

        void Reset()
        {
            JumpOffset *jump_offset = first_offset;
            while (jump_offset)
            {
                JumpOffset *next = jump_offset->next;
                compiler.FreeJumpOffset(jump_offset);
                jump_offset = next;
            }
            jump_offset = first_address;
            while (jump_offset)
            {
                JumpOffset *next = jump_offset->next;
                compiler.FreeJumpOffset(jump_offset);
                jump_offset = next;
            }

            local_values_snapshot = 0;
            first_offset = NULL;
            first_address = NULL;
            used_in_table_switch = FALSE;
            previous = NULL;
            ptr = NULL;
        }

        static void Free(Jump *jump) { jump->Reset(); jump->compiler.FreeJump(jump); }

        ES_Compiler &compiler;

        unsigned refcount;
        /**< Reference counted by JumpTarget. */

        unsigned target_address;
        /**< For forward jumps holds key=(UINT_MAX - n), where key is
             used as the offset value for all jump instructions
             jumping to this address.

             For backward jumps holds the actual target address. */

        unsigned local_values_snapshot;

        JumpOffset *first_offset, *first_address;
        BOOL used_in_table_switch;

        Jump *previous, **ptr;

        void AddJumpOffset(unsigned bytecode_index, BOOL has_condition)
        {
            JumpOffset *jump_offset = compiler.AllocateJumpOffset();
            jump_offset->bytecode_index = bytecode_index;
            jump_offset->has_condition = has_condition;
            jump_offset->next = first_offset;
            first_offset = jump_offset;
        }

        void AddJumpAddress(unsigned bytecode_index)
        {
            JumpOffset *jump_offset = compiler.AllocateJumpOffset();
            jump_offset->bytecode_index = bytecode_index;
            jump_offset->next = first_address;
            first_address = jump_offset;
        }

        Jump *next;
    };

    Jump *AllocateJump();
    void FreeJump(Jump *jump);

    Jump *unused_jumps;

    class TemporaryRegister
    {
    public:
        TemporaryRegister(ES_Compiler *compiler)
            : refcount(0),
              next(0),
              prev(0),
              compiler(compiler),
              index(compiler->GetTemporaryIndex())
        {
        }

        unsigned refcount;
        /**< Referenced counted by Register. */

        TemporaryRegister *next;
        /**< Next currently unused register (higher index). */
        TemporaryRegister *prev;
        /**< Next currently unused register (lower index). */

        ES_Compiler *compiler;
        /**< The compiler. */

        unsigned index;
        /**< Register index. */

        static void Release(TemporaryRegister *reg)
        {
            reg->compiler->ReleaseTemporaryRegister(reg);
        }
    };

public:
    enum CodeType
    {
        CODETYPE_GLOBAL,
        CODETYPE_FUNCTION,
        CODETYPE_EVAL,
        CODETYPE_STRICT_EVAL
    };

    class Register
    {
    public:
        Register()
            : temporary(0),
              index(~0u),
              is_reusable(FALSE)
        {
        }

        Register(const Register &other)
            : temporary(0)
        {
            *this = other;
        }

        ~Register()
        {
            if (temporary && --temporary->refcount == 0)
                TemporaryRegister::Release(temporary);
        }

        const Register &operator= (const Register &other)
        {
            index = other.index;
            is_reusable = other.is_reusable;

            if (temporary != other.temporary)
            {
                if (temporary && --temporary->refcount == 0)
                    TemporaryRegister::Release(temporary);

                temporary = other.temporary;

                if (temporary)
                    ++temporary->refcount;
            }

            return *this;
        }

        BOOL IsValid() const { return index != ~0u; }
        BOOL IsReusable() const { return is_reusable || temporary && temporary->refcount == 1; }
        BOOL IsTemporary() const { return temporary != 0; }
        void Reset() { *this = Register(); }
        void SetIsReusable() { is_reusable = TRUE; }

        operator ES_CodeWord () const { OP_ASSERT(index != ~0u); return ES_CodeWord(index); }

        BOOL operator== (const Register &other) const { return index == other.index; }
        BOOL operator!= (const Register &other) const { return index != other.index; }

    private:
        friend class ES_Compiler;

        explicit Register(unsigned index)
            : temporary(0),
              index(index),
              is_reusable(FALSE)
        {
        }

        Register(TemporaryRegister *temporary)
            : temporary(temporary),
              index(temporary->index),
              is_reusable(FALSE)
        {
            ++temporary->refcount;
        }

        TemporaryRegister *temporary;
        unsigned index;
        BOOL is_reusable;
    };

    class JumpTarget
    {
    public:
        JumpTarget()
            : jump(0)
        {
        }

        JumpTarget(const JumpTarget &other)
            : jump(other.jump)
        {
            if (jump)
                ++jump->refcount;
        }

        ~JumpTarget()
        {
            if (jump && --jump->refcount == 0)
            {
                if (jump->ptr)
                {
                    *jump->ptr = jump->previous;
                    if (jump->previous)
                        jump->previous->ptr = jump->ptr;
                }
                jump->Free(jump);
            }
        }

        const JumpTarget &operator= (const JumpTarget &other)
        {
            if (jump != other.jump)
            {
                if (jump && --jump->refcount == 0)
                {
                    if (jump->ptr)
                    {
                        *jump->ptr = jump->previous;
                        if (jump->previous)
                            jump->previous->ptr = jump->ptr;
                    }
                    jump->Free(jump);
                }

                jump = other.jump;

                if (jump)
                    ++jump->refcount;
            }

            return *this;
        }

        BOOL operator== (const JumpTarget &other)
        {
            return jump == other.jump;
        }

        ES_CodeWord::Offset Offset(unsigned location, BOOL has_condition) const
        {
            /* Note: 'location' is the bytecode offset of the code word
               containing offset operand to a jump instruction.  This offset is
               relative the code word following the jump instruction, and since
               all jump instructions are two code words long, that means
               'location + 1'. */
            if (jump->target_address > INT_MAX)
            {
                jump->AddJumpOffset(location, has_condition);
                return jump->target_address;
            }
            else
                return jump->target_address - (location);
        }

        ES_CodeWord::Offset Address(unsigned location = UINT_MAX) const
        {
            if (jump->target_address > INT_MAX)
            {
                OP_ASSERT(location != UINT_MAX);
                jump->AddJumpAddress(location);
                return jump->target_address;
            }
            else
                return jump->target_address;
        }

        void SetUsedInTableSwitch()
        {
            jump->used_in_table_switch = TRUE;
        }

        void SetLocalValuesSnapshot(unsigned snapshot) const
        {
            jump->local_values_snapshot = snapshot;
        }

        unsigned GetLocalValuesSnapshot() const
        {
            return jump->local_values_snapshot;
        }

    private:
        friend class ES_Compiler;

        JumpTarget(Jump *jump)
            : jump(jump)
        {
            if (jump)
                ++jump->refcount;
        }

        Jump *jump;
    };

    ES_Compiler(ES_Runtime *runtime, ES_Context *context, ES_Global_Object *global_object, CodeType codetype, unsigned register_frame_offset, ES_FunctionCode **lexical_scopes = NULL, unsigned lexical_scopes_count = 0);
    ~ES_Compiler();

    ES_Context *GetContext() { return context; }
    ES_Global_Object *GetGlobalObject() { return global_object; }

    CodeType GetCodeType() { return codetype; }

    void SetParser(ES_Parser *parser);
    ES_Parser *GetParser() { return parser; }

    void SetUsesArguments(BOOL value) { uses_arguments = value; }
    void SetUsesEval(BOOL value) { uses_eval = value; }
    void SetHasOuterScopeChain(BOOL value) { has_outer_scope_chain = value; }
    void SetClosures(ES_FunctionCode **new_closures, unsigned new_closures_count) { closures = new_closures; closures_count = new_closures_count; }
    void SetLexicalScopes(ES_FunctionCode **new_lexical_scopes, unsigned new_lexical_scopes_count) { lexical_scopes = new_lexical_scopes; lexical_scopes_count = new_lexical_scopes_count; }
    void SetInnerScope(unsigned *inner_scopes, unsigned inner_scopes_count);
    void SetIsFunctionExpr(BOOL value) { is_function_expr = value; }
    void SetIsStrictMode() { is_strict_mode = TRUE; }

    BOOL IsStrictMode() { return is_strict_mode; }

    void SetEmitDebugInformation(BOOL value) { emit_debug_information = value; }

    BOOL CompileProgram(unsigned functions_count, ES_FunctionCode **&functions,
                        unsigned statements_count, ES_Statement **statements,
                        BOOL generate_result, ES_ProgramCode *&code);

    BOOL CompileFunction(JString *name, JString *debug_name,
                         unsigned formals_count, JString **formals,
                         unsigned functions_count, ES_Parser::FunctionData *functions,
                         unsigned statements_count, ES_Statement **statements,
                         ES_FunctionCode *&code);

    void InitializeCode(ES_Code *code);

    void EmitInstruction(ES_Instruction instruction);
    /**< Emit instruction with no operands. */
    void EmitInstruction(ES_Instruction instruction, const Register &reg1);
    /**< Emit instruction  + register operand unless 'reg1' is invalid. */
    void EmitInstruction(ES_Instruction instruction, const Register &reg1, ES_CodeWord word2);
    /**< Emit instruction  + register operand + other operand unless 'reg1' is invalid. */
    void EmitInstruction(ES_Instruction instruction, const Register &reg1, ES_CodeWord word2, ES_CodeWord word3);
    /**< Emit instruction  + register operand + two other operands unless 'reg1' is invalid. */
    void EmitInstruction(ES_Instruction instruction, const Register &reg1, ES_CodeWord word2, ES_CodeWord word3, ES_CodeWord word4);
    /**< Emit instruction  + register operand + three other operands unless 'reg1' is invalid. */
    void EmitInstruction(ES_Instruction instruction, const Register &reg1, ES_CodeWord word2, ES_CodeWord word3, ES_CodeWord word4, ES_CodeWord word5);
    /**< Emit instruction  + register operand + four other operands unless 'reg1' is invalid. */
    void EmitInstruction(ES_Instruction instruction, const Register &reg1, const JumpTarget &jt);
    /**< Emit instruction  + register operand + other operand unless 'reg1' is invalid. */
    void EmitJump(const Register *condition_on, ES_Instruction instruction, const JumpTarget &jt);
    /**< Emit jump instruction. */
    void EmitJump(const Register *condition_on, ES_Instruction instruction, const JumpTarget &jt, ES_CodeWord word2);
    /**< Emit jump instruction with extra operand. */
    void EmitInstruction(ES_Instruction instruction, ES_CodeWord word1);
    /**< Emit instruction with non-register first operand. */
    void EmitInstruction(ES_Instruction instruction, ES_CodeWord word1, ES_CodeWord word2);
    /**< Emit instruction with non-register first operand. */
    void EmitInstruction(ES_Instruction instruction, ES_CodeWord word1, ES_CodeWord word2, ES_CodeWord word3);
    /**< Emit instruction with non-register first operand. */
    void EmitInstruction(ES_Instruction instruction, ES_CodeWord word1, ES_CodeWord word2, ES_CodeWord word3, ES_CodeWord word4);
    /**< Emit instruction with non-register first operand. */

    void EmitPropertyAccessor(ES_Instruction instruction, JString *name, const Register &obj, const Register &value);
    /**< Emit ESI_GETN_IMM or ESI_PUTN_IMM, or the 'length' specific
         variants, as appropriate. */

    void EmitPropertyAccessor(ES_Instruction instruction, const Register &name, const Register &obj, const Register &value);
    /**< Emit ESI_GET or ESI_PUT. */

    void EmitGlobalAccessor(ES_Instruction instruction, JString *name, const Register &reg, BOOL quiet = FALSE);
    /**< Used for ESI_GET_GLOBAL (with operands swapped) and ESI_PUT_GLOBAL.
         The compiler remembers the access and might optimize it into
         ESI_*_GLOBAL_IMM later. */

    void EmitScopedAccessor(ES_Instruction instruction, JString *name, const Register *reg1, const Register *reg2 = NULL, BOOL for_call_or_typeof = FALSE);
    /**< Used for ESI_GET_SCOPE, ESI_GET_SCOPE_REF, ESI_PUT_SCOPE and ESI_DELETE_SCOPE. */

#ifdef ES_COMBINED_ADD_SUPPORT
    void EmitADDN(const Register &dst, unsigned count, const Register *srcs);
#endif // ES_COMBINED_ADD_SUPPORT

    void EmitCONSTRUCT_OBJECT(const Register &dst, unsigned class_index, unsigned count, const Register *values);

    BOOL PushWantObject() { BOOL previous_want_object = want_object; want_object = TRUE; return previous_want_object; }
    void PopWantObject(BOOL previous_want_object) { want_object = previous_want_object; }

    BOOL IsFormal(const Register &reg) { return reg.IsValid() && reg.index >= 2 && reg.index < formals_count + 2; }
    BOOL IsLocal(const Register &reg) { return reg.IsValid() && reg.index >= 2 && !reg.IsTemporary(); }
    BOOL SafeAsThisRegister(const Register &reg);

    void StoreRegisterValue(const Register &reg, const ES_Value_Internal &value);

    BOOL ValueTracked(const Register &reg);

    void SetLocalValue(const Register &reg, const ES_Value_Internal &value, BOOL value_stored);
    BOOL GetLocalValue(ES_Value_Internal &value, const Register &reg, BOOL &is_stored);
    void ResetLocalValue(const Register &reg, BOOL flush);
    void FlushLocalValue(const Register &reg);

    unsigned GetLocalValuesSnapshot() { return local_value_snapshot++; }
    void ResetLocalValues(unsigned snapshot, BOOL flush = TRUE);
    void FlushLocalValues(unsigned snapshot = 0);

    void *BackupLocalValues();
    void RestoreLocalValues(void *data);
    void SuspendValueTracking() { ++suspend_value_tracking; }
    void ResumeValueTracking() { --suspend_value_tracking; }

    void RecordNestedRead(JString *name);
    void RecordNestedWrite(JString *name);
    BOOL HasNestedRead(const Register &reg);
    BOOL HasNestedWrite(const Register &reg);

    JString *GetLocal(const Register &reg);

    Register This() { return FixedRegister(0); }
    Register Callee() { OP_ASSERT(codetype == CODETYPE_FUNCTION); return FixedRegister(1); }
    Register Local(JString *name, BOOL for_assignment = FALSE);
    Register Temporary(BOOL for_call_arguments = FALSE, const Register *temporary = NULL);
    /**< Get new or unused temporary register.  If 'for_call_arguments' is TRUE,
         care is taken to ensure that additional calls to Temporary() after this
         one returns registers in a consequtive sequence in the register frame.
         In other words, the first unused temporary register with no used
         temporary registers following it is returned. */
    Register Invalid() { Register invalid; return invalid; }

    JumpTarget ForwardJump();
    JumpTarget BackwardJump();
    void SetJumpTarget(const JumpTarget &jt);

    Register &TryBlockReturnValueRegister() { return try_block_return_value; }
    /**< Special temporary register for returning values when returning from a
         try block which causes one or more finally blocks to be executed. */

    Register &TryBlockHaveReturnedRegister() { return try_block_have_returned; }
    /**< Special temporary register for signalling that a 'return' from within
         'try-finally' block has executed. On having unwounded all 'finally'
         blocks, the outermost 'finally' then returns from the function. */

    JumpTarget FunctionReturnTarget() { return function_return_value; }
    /**< Jump target for returning values when returning from a try block. */

    void PushRethrowTarget(const Register &reg);
    void PopRethrowTarget();
    Register GetRethrowTarget() { return current_rethrow_target->target; }
    /**< Registers containing the address to jump to when execution
         of finally is finished. */

    void PushFinallyTarget(const JumpTarget &target);
    void PopFinallyTarget();
    const JumpTarget *GetFinallyTarget();
    /**< Target of the closest enclosing finally handler. */

    enum ContinuationTargetType
    {
        TARGET_BREAK,
        TARGET_CONTINUE,
        TARGET_FINALLY
    };

    void PushContinuationTarget(const JumpTarget &target, ContinuationTargetType t, JString *label = NULL);
    void PopContinuationTargets(unsigned count);

    const JumpTarget *FindBreakTarget(BOOL &crosses_finally, JString *label = NULL);
    const JumpTarget *FindContinueTarget(BOOL &crosses_finally, JString *label = NULL);

    void PushLabel(JString *label);
    JString *PopLabel();
    BOOL IsDuplicateLabel(JString *label);

    BOOL GetGlobalIndex(unsigned &index, JString *name);

    ES_CodeWord::Index Identifier(JString *name);
    ES_CodeWord::Index String(JString *string);
    JString *String(ES_CodeWord::Index index);
    ES_CodeWord::Index Double(double value);
    ES_CodeWord::Index RegExp(ES_RegExp_Information *info);
    ES_CodeWord::Index Function(ES_FunctionCode *function_code);

    void AddConstantArrayLiteral(ES_CodeStatic::ConstantArrayLiteral *&cal, unsigned &cal_index, unsigned elements_count, unsigned array_length);

    BOOL HasLexicalScopes() { return lexical_scopes_count != 0; }
    BOOL GetLexical(ES_CodeWord::Index &scope_index, ES_CodeWord::Index &variable_index, JString *name, BOOL &is_read_only);

    void AddVariable(JString *name, BOOL is_argument);
    void AddGlobalDeclaration(JString *name);

    void PushInnerScope(const Register &robject);
    void PopInnerScope();

    void StartExceptionHandler(const JumpTarget &handler_target, BOOL is_finally);
    void EndExceptionHandler();
    void PushCaughtException(JString *name, const Register &value);
    void PopCaughtException();
    BOOL IsInTryBlock();

    BOOL HasNestedFunctions() { return functions_count != 0; }
    BOOL UsesEval() { return uses_eval; }
    BOOL HasInnerScopes() { return current_inner_scopes_count != 0; }
    BOOL HasOuterScopes() { return has_outer_scope_chain; }
    BOOL HasUnknownScope() { return codetype == CODETYPE_EVAL || HasInnerScopes() || UsesEval() || has_outer_scope_chain; }
    /**< Returns TRUE if there are objects other than the global
         object on the scope chain. */
    BOOL IsRedirectedCall(ES_CallExpr *expr) { return expr == redirected_call; }

    void AssignedVariable(JString *name);
    void StartSimpleLoopVariable(JString *name, int lower_bound, int upper_bound);
    void EndSimpleLoopVariable(JString *name);

    unsigned CurrentInstructionIndex() { return BytecodeUsed(); }
    void SetCodeWord(unsigned index, ES_CodeWord value) { bytecode_base[index] = value; }

    unsigned AddLoop(unsigned start);

    unsigned GetFormatStringCache() { return format_string_caches_used++; }
    unsigned GetEvalCache() { return eval_caches_used++; }

    ES_CodeStatic::SwitchTable *AddSwitchTable(unsigned &index, int minimum, int maximum);

    BOOL HasSingleEvalCall();

    JString *GetLengthIdentifier() { return ident_length; }
    JString *GetEvalIdentifier() { return ident_eval; }
    JString *GetApplyIdentifier() { return ident_apply; }

    unsigned GetInnerScopeIndex();
    unsigned AddObjectLiteralClass(ES_CodeStatic::ObjectLiteralClass *&klass);
    unsigned GetObjectLiteralClassIndex() { return object_literal_classes_used++; }

    ES_Parser::FunctionData &GetFunctionData(unsigned index) { return function_data[index]; }

    const ES_SourceLocation *PushSourceLocation(const ES_SourceLocation *new_location)
    {
        const ES_SourceLocation *old_location = current_source_location;
        current_source_location = new_location;
        current_source_location_used = FALSE;
        return old_location;
    }

    void PopSourceLocation(const ES_SourceLocation *old_location)
    {
        current_source_location = old_location;
        current_source_location_used = FALSE;
    }

    void AddDebugRecord(ES_CodeStatic::DebugRecord::Type type, const ES_SourceLocation &location);

    void SetError(ES_Parser::ErrorCode code, const ES_SourceLocation &loc) { parser->SetError(code, loc); }
    BOOL HasError() { return parser->HasError(); }

    OpMemGroup *Arena();

private:
    Register FixedRegister(unsigned index) { return Register(index); }

    void EnsureBytecodeAllocation(unsigned nwords);
    unsigned BytecodeUsed() { return bytecode - bytecode_base; }

    ES_Runtime *runtime;
    ES_Context *context;
    ES_Global_Object *global_object;
    ES_Parser *parser;
    CodeType codetype;

    JString *ident_length, *ident_eval, *ident_apply;
    BOOL uses_arguments, uses_eval, uses_get_scope_ref, uses_lexical_scope, has_outer_scope_chain, is_function_expr, is_void_function, is_strict_mode;
    ES_CallExpr *redirected_call;

    unsigned formals_count;

    ES_FunctionCode **lexical_scopes;
    unsigned lexical_scopes_count;

    ES_FunctionCode **closures;
    unsigned closures_count;

    ES_Statement **statements;
    unsigned statements_count;

    unsigned register_frame_offset;

    ES_CodeWord *bytecode_base, *bytecode;
    unsigned bytecode_allocated, maximum_jump_target;

    Jump *first_jump;

    unsigned object_literal_classes_used;

    class ObjectLiteralClass
    {
    public:
        unsigned index;
        ES_CodeStatic::ObjectLiteralClass klass;

        ObjectLiteralClass *next;
    };

    ObjectLiteralClass *object_literal_classes;

    unsigned property_get_caches_used;
    unsigned property_put_caches_used;

public:
    void ReleaseTemporaryRegister(TemporaryRegister *reg)
    {
        OP_ASSERT(reg->index <= highest_used_temporary_index);
        OP_ASSERT(reg->prev == NULL && reg->next == NULL);

        if (reg->index == highest_used_temporary_index)
        {
            reg->next = high_unused_temporaries;
            high_unused_temporaries = reg;
            --highest_used_temporary_index;

            while (TemporaryRegister *unused = last_unused_temporary)
                if (unused->index == highest_used_temporary_index)
                {
                    last_unused_temporary = unused->prev;
                    if (unused->prev)
                        unused->prev->next = NULL;
                    else
                        first_unused_temporary = NULL;

                    unused->prev = NULL;
                    unused->next = high_unused_temporaries;
                    high_unused_temporaries = unused;
                    --highest_used_temporary_index;
                }
                else
                    break;
        }
        else
        {
            TemporaryRegister **list = &last_unused_temporary, *next = NULL;

            while (*list && (*list)->index > reg->index)
            {
                OP_ASSERT((*list)->prev != *list);
                next = *list;
                list = &next->prev;
            }

            OP_ASSERT(reg != *list);
            OP_ASSERT(reg != next);

            reg->prev = *list;
            reg->next = next;

            if (*list)
                (*list)->next = reg;
            *list = reg;

            if (!reg->prev)
                first_unused_temporary = reg;
        }
    }

    unsigned GetTemporaryIndex()
    {
        unsigned index = ++highest_used_temporary_index;
        if (index > highest_ever_temporary_index)
            highest_ever_temporary_index = index;
        return index;
    }

private:
    TemporaryRegister *first_unused_temporary, *last_unused_temporary;
    /**< Double linked list of unused temporaries with index lower than highest_used_temporary_index. */
    TemporaryRegister *high_unused_temporaries;
    /**< Single linked list of unused temporaries with index higher than highest_used_temporary_index. */
    unsigned highest_used_temporary_index, highest_ever_temporary_index;

    Register global_result_value;

    unsigned forward_jump_id_counter;

    ES_Value_Internal *known_local_value;
    BOOL *local_value_known;
    unsigned local_value_snapshot, *local_value_generation, *local_value_stored, *local_value_nested_read, *local_value_nested_write;
    unsigned suspend_value_tracking;

    ES_Identifier_Mutable_List *locals_table;
    ES_Identifier_List *strings_table, *globals_table, *property_get_caches_table, *property_put_caches_table;
    ES_Identifier_Hash_Table *global_caches_table;

    unsigned GetPropertyGetCacheIndex(JString *name);
    unsigned GetPropertyPutCacheIndex(JString *name);
    unsigned RecordGlobalCacheAccess(JString *name, ES_CodeStatic::GlobalCacheAccess kind);

    OpVector<ES_RegExp_Information> regexps;

    double *doubles;
    unsigned doubles_count;

    OpVector<ES_CodeStatic::ConstantArrayLiteral> constant_array_literals;

    unsigned *global_accesses;
    unsigned global_accesses_count;

    JString *current_function_name;

    ES_FunctionCodeStatic **functions_static;
    ES_FunctionCode **functions;
    ES_Parser::FunctionData *function_data;
    unsigned functions_count, functions_offset;

    Register try_block_return_value;
    Register try_block_have_returned;
    JumpTarget function_return_value;

    class RethrowTarget
    {
    public:
        Register target;
        RethrowTarget *previous;
    };

    RethrowTarget *current_rethrow_target;

    class FinallyTarget
    {
    public:
        JumpTarget jump_target;
        FinallyTarget *previous;
    };

    FinallyTarget *current_finally_target;

    class ContinuationTarget
    {
    public:
        ContinuationTargetType type;
        JumpTarget jump_target;
        JString *label;
        BOOL is_finally;
        ContinuationTarget *previous;
    };

    ContinuationTarget *current_continuation_target;

    class LabelSet
    {
    public:
        JString *label;
        LabelSet *previous;
    };

    LabelSet *current_unused_label_set;

public:
    class CodeExceptionHandlerElement
    {
    public:
        ES_CodeStatic::ExceptionHandler code_handler;
        CodeExceptionHandlerElement *previous;
    };

private:
    friend void ExtractExceptionHandlers(ES_CodeStatic::ExceptionHandler *&handlers, unsigned &handlers_count, CodeExceptionHandlerElement *elements);

    class ExceptionHandler
    {
    public:
        JumpTarget handler_target;
        BOOL is_finally;
        unsigned start_index;
        unsigned end_index;
        CodeExceptionHandlerElement *nested;

        ExceptionHandler *previous;
    };

    ExceptionHandler *current_exception_handler;
    CodeExceptionHandlerElement *toplevel_exception_handlers;

    ES_CodeStatic::ExceptionHandler **code_exception_handlers;
    unsigned *code_exception_handlers_count, code_exception_handlers_depth;

    class CaughtException
    {
    public:
        JString *name;
        ES_Compiler::Register value;

        CaughtException *previous;
    };

    CaughtException *caught_exception;

#ifdef ES_NATIVE_SUPPORT
    class CurrentSimpleLoopVariable
    {
    public:
        JString *name;
        int lower_bound;
        int upper_bound;
        unsigned start_index;
        unsigned end_index;
        BOOL tainted;

        CurrentSimpleLoopVariable *previous;
    };

    CurrentSimpleLoopVariable *current_simple_loop_variable;
    ES_CodeStatic::VariableRangeLimitSpan *variable_range_limit_span;
#endif // ES_NATIVE_SUPPORT

    unsigned *current_inner_scopes, current_inner_scopes_count, current_inner_scopes_allocated, cached_inner_scopes_index;

    ES_CodeStatic::InnerScope *final_inner_scopes;
    unsigned final_inner_scopes_count;

    class Loop
    {
    public:
        unsigned start, jump;
        Loop *next;
    } ;

    Loop *first_loop, *last_loop;
    unsigned loops_count;

    unsigned format_string_caches_used;
    unsigned eval_caches_used;

    class SwitchTable
    {
    public:
        unsigned index;
        ES_CodeStatic::SwitchTable *switch_table;
        SwitchTable *next;
    };

    SwitchTable *first_switch_table;
    unsigned switch_tables_used;

    BOOL want_object;

    BOOL HasSetExtentInformation();
    void SetExtentInformation(ES_Instruction instruction);

    BOOL emit_debug_information;

    const ES_SourceLocation *current_source_location;
    BOOL current_source_location_used;

    ES_CodeStatic::DebugRecord *debug_records;
    unsigned debug_records_count, debug_records_allocated;
};

class ES_ExpressionVisitor
{
public:
    virtual BOOL Visit(ES_Expression *expression) = 0;
};

class ES_StatementVisitor
    : public ES_ExpressionVisitor
{
public:
    virtual BOOL Enter(ES_Statement *statement, BOOL &skip) = 0;
    virtual BOOL Leave(ES_Statement *statement) = 0;
};

class ES_ResetLocalValues
    : public ES_StatementVisitor
{
public:
    ES_ResetLocalValues(ES_Compiler &compiler, BOOL flush_extra = FALSE)
        : compiler(compiler),
          flush_extra(flush_extra),
          flush_reg_uses(NULL)
    {
    }

    virtual BOOL Visit(ES_Expression *expression);
    virtual BOOL Enter(ES_Statement *statement, BOOL &skip);
    virtual BOOL Leave(ES_Statement *statement);

    void SetFlushIfRegisterUsed(const ES_Compiler::Register *reg);

private:
    BOOL IsLocal(ES_Expression *expression, ES_Compiler::Register &reg);
    void ResetIfIdentifier(ES_Expression *expression);
    void FlushIfIdentifier(ES_Expression *expression);

    ES_Compiler &compiler;
    BOOL flush_extra;
    const ES_Compiler::Register *flush_reg_uses;
};

void Initialize(ES_FunctionCode *code, ES_FunctionCodeStatic *data, ES_Global_Object *global_object, ES_ProgramCodeStaticReaper *program_reaper);
void Initialize(ES_ProgramCode *code, ES_ProgramCodeStatic *data, ES_Global_Object *global_object);

#endif // ES_COMPILER_H
