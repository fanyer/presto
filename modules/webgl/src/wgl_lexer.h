/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- lexical analysis.
 *
 * Originally based on es_lexer.
 */
#ifndef WGL_LEXER_H
#define WGL_LEXER_H

#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_error.h"
#include "modules/webgl/src/wgl_symbol.h"
#include "modules/util/adt/opvector.h"
#include "modules/util/OpHashTable.h"

#define WITH_PRECEDENCE(x) ((x) << 7)
#define IS_OPERATOR(x) ((x) << 14)
#define GET_TOKEN_PRECEDENCE(x) (((x)>>7) & 0x7f)
#define GET_TOKEN_OPERATOR(x)  static_cast<WGL_Expr::Operator>(((x)>>14) & 0x7f)

#define TO_PRECEDENCE_LEVEL(x) static_cast<WGL_Type::Precision>(1 + ((x) - WGL_Token::TOKEN_LOW_PREC))
#define TO_STORAGE_QUALIFIER(x) static_cast<WGL_TypeQualifier::Storage>((x) - WGL_Token::TOKEN_CONST + 1)
#define TO_INVARIANT_QUALIFIER(x) static_cast<WGL_TypeQualifier::InvariantKind>((x) - WGL_Token::TOKEN_INVARIANT + 1)
#define TO_PARAM_DIRECTION(x) static_cast<WGL_Param::Direction>((x) - WGL_Token::TOKEN_IN_ + 1)

class WGL_Token
{
public:
   /* Lexeme encoding is straightforwardly done via an enum:
      Operators marked as reserved are illegal per OpenGL ES Shading Language section 5.1.
      A source containing illegal operators will NOT parse successfully.
   */
   enum Token
   {
     TOKEN_COND = 0 | WITH_PRECEDENCE(WGL_Expr::PrecCond),
     TOKEN_COMMA = 1 | WITH_PRECEDENCE(WGL_Expr::PrecComma) | IS_OPERATOR(WGL_Expr::OpComma),
     TOKEN_EQUAL = 2 | WITH_PRECEDENCE(WGL_Expr::PrecAssign) | IS_OPERATOR(WGL_Expr::OpNop),
     TOKEN_ADD_ASSIGN = 3 | WITH_PRECEDENCE(WGL_Expr::PrecAssign) | IS_OPERATOR(WGL_Expr::OpAdd),
     TOKEN_SUB_ASSIGN = 4 | WITH_PRECEDENCE(WGL_Expr::PrecAssign) | IS_OPERATOR(WGL_Expr::OpSub),
     TOKEN_MUL_ASSIGN = 5 | WITH_PRECEDENCE(WGL_Expr::PrecAssign) | IS_OPERATOR(WGL_Expr::OpMul),
     TOKEN_DIV_ASSIGN = 6 | WITH_PRECEDENCE(WGL_Expr::PrecAssign) | IS_OPERATOR(WGL_Expr::OpDiv),
     TOKEN_MOD_ASSIGN = 7,   // reserved
     TOKEN_LEFT_ASSIGN = 8,  // reserved
     TOKEN_RIGHT_ASSIGN = 9, // reserved
     TOKEN_AND_ASSIGN = 10 | WITH_PRECEDENCE(WGL_Expr::PrecAssign) | IS_OPERATOR(WGL_Expr::OpAnd),
     TOKEN_XOR_ASSIGN = 11 | WITH_PRECEDENCE(WGL_Expr::PrecAssign) | IS_OPERATOR(WGL_Expr::OpXor),
     TOKEN_OR_ASSIGN = 12 | WITH_PRECEDENCE(WGL_Expr::PrecAssign) | IS_OPERATOR(WGL_Expr::OpOr),

     TOKEN_EQ = 13 | WITH_PRECEDENCE(WGL_Expr::PrecEq) | IS_OPERATOR(WGL_Expr::OpEq),
     TOKEN_NEQ = 14 | WITH_PRECEDENCE(WGL_Expr::PrecEq) | IS_OPERATOR(WGL_Expr::OpNEq),
     TOKEN_LT = 15 | WITH_PRECEDENCE(WGL_Expr::PrecRel) | IS_OPERATOR(WGL_Expr::OpLt),
     TOKEN_GT = 16 | WITH_PRECEDENCE(WGL_Expr::PrecRel) | IS_OPERATOR(WGL_Expr::OpGt),
     TOKEN_LE = 17 | WITH_PRECEDENCE(WGL_Expr::PrecRel) | IS_OPERATOR(WGL_Expr::OpLe),
     TOKEN_GE = 18 | WITH_PRECEDENCE(WGL_Expr::PrecRel) | IS_OPERATOR(WGL_Expr::OpGe),

     TOKEN_SHIFTL = 19, // reserved
     TOKEN_SHIFTR = 20, // reserved

     TOKEN_ADD = 21 | WITH_PRECEDENCE(WGL_Expr::PrecAdd) | IS_OPERATOR(WGL_Expr::OpAdd),
     TOKEN_SUB = 22 | WITH_PRECEDENCE(WGL_Expr::PrecAdd) | IS_OPERATOR(WGL_Expr::OpSub),
     TOKEN_MUL = 23 | WITH_PRECEDENCE(WGL_Expr::PrecMul) | IS_OPERATOR(WGL_Expr::OpMul),
     TOKEN_DIV = 24 | WITH_PRECEDENCE(WGL_Expr::PrecMul) | IS_OPERATOR(WGL_Expr::OpDiv),
     TOKEN_MOD = 25, // reserved

     TOKEN_INC = 26 | WITH_PRECEDENCE(WGL_Expr::PrecPrefix) | IS_OPERATOR(WGL_Expr::OpPreInc),
     TOKEN_DEC = 27 | WITH_PRECEDENCE(WGL_Expr::PrecPrefix) | IS_OPERATOR(WGL_Expr::OpPreDec),
     TOKEN_NOT = 28 | WITH_PRECEDENCE(WGL_Expr::PrecPrefix) | IS_OPERATOR(WGL_Expr::OpNot),
     TOKEN_NEGATE = 29 | WITH_PRECEDENCE(WGL_Expr::PrecPrefix) | IS_OPERATOR(WGL_Expr::OpNegate),
     TOKEN_COMPLEMENT = 30, // reserved

     TOKEN_IDENTIFIER = 31,

     TOKEN_VOID = 32,
     TOKEN_FLOAT = 33,
     TOKEN_INT = 34,
     TOKEN_BOOL = 35,
     TOKEN_TYPE_NAME = 36,

     TOKEN_BREAK = 38,
     TOKEN_CONTINUE = 39,
     TOKEN_DO = 40,
     TOKEN_ELSE = 41,
     TOKEN_FOR = 42,
     TOKEN_IF = 43,
     TOKEN_DISCARD = 44,
     TOKEN_RETURN = 45,
     TOKEN_SWITCH = 46,
     TOKEN_CASE = 47,
     TOKEN_DEFAULT = 48,
     TOKEN_WHILE = 49,

     TOKEN_CONST = 51,
     TOKEN_ATTRIBUTE = 52,
     TOKEN_VARYING = 53,
     TOKEN_UNIFORM = 54,

     TOKEN_IN_ = 55,
     TOKEN_OUT_ = 56,
     TOKEN_INOUT = 57,

     TOKEN_LAYOUT = 61,

     TOKEN_STRUCT = 62,
     TOKEN_CONST_FLOAT = 63,
     TOKEN_CONST_INT = 64,
     TOKEN_CONST_UINT = 65,
     TOKEN_CONST_FALSE = 66,
     TOKEN_CONST_TRUE = 67,

     /* 68 is unused (was field selection.) 69, 70 likewise.
        Use if further tokens needed. */

     TOKEN_AND = 71 | WITH_PRECEDENCE(WGL_Expr::PrecLogAnd) | IS_OPERATOR(WGL_Expr::OpAnd),
     TOKEN_OR = 72 | WITH_PRECEDENCE(WGL_Expr::PrecLogOr) | IS_OPERATOR(WGL_Expr::OpOr),
     TOKEN_XOR = 73 | WITH_PRECEDENCE(WGL_Expr::PrecLogXor) | IS_OPERATOR(WGL_Expr::OpXor),

     TOKEN_LPAREN = 74,
     TOKEN_RPAREN = 75,
     TOKEN_LBRACKET = 76,
     TOKEN_RBRACKET = 77,
     TOKEN_LBRACE = 78,
     TOKEN_RBRACE = 79,
     TOKEN_DOT = 80,
     TOKEN_COLON = 81,
     TOKEN_SEMI = 82,
     TOKEN_BITAND = 83, // reserved
     TOKEN_BITOR = 84,   // reserved
     TOKEN_BITXOR = 85, // reserved
     TOKEN_INVARIANT = 86,

     TOKEN_LOW_PREC = 87,
     TOKEN_MED_PREC = 88,
     TOKEN_HIGH_PREC = 89,
     TOKEN_PRECISION = 90,
     TOKEN_ILLEGAL = 91,

     TOKEN_PRIM_TYPE_NAME = 94,
     TOKEN_EOF = 95
   };
};

/* Longest lookahead is 21 chars, for 'sampler1darrayshadow' */
#define MAX_FIXED_TOKEN_LENGTH  21
#define WGL_PARM_LEXBUFFER_GUARANTEED (2*MAX_FIXED_TOKEN_LENGTH)

class WGL_LexBuffer
{
public:
    WGL_LexBuffer(WGL_Context *context);

    virtual ~WGL_LexBuffer()
    {
    }

    void FillBuffer();
    void FillBufferIfNeeded();

    void ResetL(WGL_ProgramText *program_array, unsigned elements);

    int GetLineNumber() const { return line_number; }

    BOOL HaveErrors();
    WGL_Error *GetFirstError();

protected:
    WGL_Context *context;

    const uni_char *source_context;

    class InputSegment
    {
    public:
        InputSegment()
            : segment_text(NULL),
              segment_length(0),
              segment_ptr(NULL)
        {
        }

        uni_char *segment_text;
        /**< Start of the segment. A segment encompasses codepoints
             in the [segment_text, segment_text + segment_length)
             range. */

        unsigned segment_length;
        /**< Number of codepoints in this segment. */

        uni_char *segment_ptr;
        /**< >= segment_text. Points at current codepoint. */

        BOOL Within(const uni_char *p) const;
        /**< Returns TRUE if 'p' is within this segment. That is,
             in the [segment_text, segment_text + segment_length)
             range. */

        inline unsigned Used() const { return static_cast<unsigned>(segment_ptr - segment_text); }
        /**< Returns the number of codepoints currently in this segment. */

        inline unsigned Available() const { OP_ASSERT(segment_length >= Used()); return (segment_length - Used()); }
        /**< Returns the number of remaining codepoints that this segment
             can hold. */
    };

    InputSegment *input_segments;
    /**< Pointer to lexer input buffers; grows if the lexer
         inserts new segments (e.g., when expanding CPP macros.) */

    unsigned input_segments_used;
    /**< Number of input segments used. */

    unsigned input_segments_allocated;
    /**< Number of input segments allocated. */

    unsigned current_input_segment;
    /**< The index of the element that is the current input segment. */

    const uni_char *buf_start;
    /**< Pointer to the first character in the current input segment. */

    uni_char *buf_ptr;
    /**< Pointer to next input character. */

    const uni_char *buf_end;
    /**< Pointer to limit of current input segment. That is,
         one codepoint beyond the end. */

    uni_char *buf_ptr_save;
    /**< Buffer pointer at start of current lexeme. */

    int line_number;

    BOOL in_smallbuffer;
    /**< TRUE if smallbuffer is currently active. */

    BOOL flush_smallbuffer;
    /**< Set to TRUE to have the smallbuffer contents be
         invalidated on next buffer read/fill operation. */

    int smallbuffer_length;
    /**< Number of codepoints currently kept in the smallbuffer.
         That is, codepoints in the range

           [smallbuffer_start, smallbuffer_start + smallbuffer_length)

         are valid. */

    unsigned consumed_from_next_segment;
    /**< Number of codepoints consumed from the next input element
         (the one at current_input_segment + 1) */

    int ConsumedFromCurrent() { return smallbuffer_length - consumed_from_next_segment; }
    /**< Returns the number of smallbuffer codepoints belonging
         to the current input segment. */

    const uni_char *smallbuffer_start;
    /**< Start of the smallbuffer. */

    uni_char smallbuffer[WGL_PARM_LEXBUFFER_GUARANTEED]; // ARRAY OK 2011-07-05 wonko
    /**< Temporary buffer for handling buffer crossings.

         The 'smallbuffer' is an input segment/buffer used near the end
         of a buffer segment (end-of-buffer being a special case), with
         input being copied into it when segment end nears.

         This is done so that the lexer can do direct lexeme matching
         (lookahead) without having to consider buffer overruns.
         The size of the buffer must as a result be greater or equal
         to the longest lexeme. The small buffer is tail-padded with
         NULs so as to allow for easy EOB testing. */

    BOOL WithinSmallBuffer(const uni_char *str) { return str >= smallbuffer && str < (smallbuffer + WGL_PARM_LEXBUFFER_GUARANTEED); }
    /**< Returns TRUE if the given string is within the smallbuffer.
         The assumption being that a string that starts within this
         buffer is wholly contained within it. */

    BOOL FinalInputSegment() { return (current_input_segment == input_segments_used - 1); }
    void AdvanceInputSegment() { current_input_segment++; }

    const uni_char *BufferPosition();
    /**< Return the current buffer input position as a reference
         into the input buffer. */

    const uni_char *GetBufferPosition() { OP_ASSERT(!in_smallbuffer); return buf_ptr; }

    unsigned ComputeBufferOffset(const uni_char *position);
    /**< Compute the number of characters between the current input
         position and the given argument. The position is
         assumed to be referring to some earlier buffer
         position (including current.) */

    uni_char BufferPeekBehind(unsigned offset);
    /**< Return the character at 'offset' positions prior to current.
         It is illegal to use an offset that refers to a position
         before the start of the input buffer.

         @param offset The position before the current input point. */

    void BufferCopy(uni_char *result, const uni_char *start, unsigned length);
    /**< Starting at buffer position 'start', copy 'length' characters
         into 'result'. */

    BOOL is_pound_recognised;

    BOOL AtEndOfInput();
    void ConsumeInput(unsigned nchars);

    int SkipLine();
    BOOL SkipComment();

    void Eat(unsigned d = 1) { buf_ptr += d; FillBufferIfNeeded(); }

    void AddError(WGL_Error::Type err, const uni_char *s, unsigned len, BOOL length_as_lines);
    void AddError(WGL_Error::Type err, const uni_char *s);

    void Reset(WGL_LexBuffer *buffer);
    int LexId(WGL_String *&result);
    void SkipWhitespace(BOOL include_new_line = TRUE);
    void SkipUntilNextLine(BOOL skip_whitespace);
    void AdvanceUntilNextLine(BOOL skip_whitespace);
    unsigned LexNumLiteral();

    void HandleLinePragma();
};

class WGL_CPP
    : public WGL_LexBuffer
{
public:
    WGL_CPP(WGL_Context *context);

    virtual ~WGL_CPP();

    void InitializeL();
    void CPP();

    enum ExtensionStatus
    {
        ExtensionUnsupported,
        ExtensionIllegal,
        ExtensionWarnUnsupported,
        ExtensionOK
    };

    ExtensionStatus CheckExtension(const uni_char *extension, unsigned extension_length, const uni_char *behaviour, unsigned behaviour_length);

    class DefineData
    {
    public:
        DefineData(WGL_Context *context)
            : is_function(FALSE)
            , rhs(NULL)
            , context(context)
        {
        }

        OpVector<WGL_String> parameters;
        /**< The named parameters of the macro. */

        BOOL is_function;
        /**< If FALSE and 'parameters' is empty, the DefineData represents
             a simple name to substitute for. If TRUE and 'parameters' is
             empty, a nullary macro function. Otherwise a macro function
             with one or more arguments. */

        const uni_char *rhs;
        /**< The string to substitute for upon expansion of this DefineData. */

        void AddParam(const uni_char *s, unsigned len);
        void AddParam(WGL_String *s);

        void SetIsFunctionDefine() { is_function = TRUE; }
        BOOL IsFunctionDefine() { return is_function; }

    private:
        WGL_Context *context;
    };

private:
    void AddOutputSegment(const uni_char *data, unsigned length);

    unsigned cpp_inactive_nesting;
    BOOL is_silent;
    BOOL seen_cpp_directive;

    OpINT32Vector cpp_contexts;
    OpINT32Vector silent_eval_contexts;

    void HandleIfdef(BOOL is_defined);
    void HandleIf();
    void HandleDefine();
    void HandleUndef();
    void HandleExtension();
    BOOL IsTrue(BOOL is_defined, const uni_char *id, unsigned id_length);

    const uni_char *LexIdentifier(unsigned &result_length);

    OpPointerHashTable<WGL_String, DefineData> defines_map;

    void AddDefineL(WGL_String *name, DefineData *data);
    void AddDefineL(WGL_String *name, const uni_char *value);
    void RemoveDefineL(WGL_String *name);

    const uni_char *FindExpansion(const uni_char *s, unsigned len, DefineData **data = NULL);
    const uni_char *HandleMacroExpansion(WGL_String *name, DefineData *data, OpVector<WGL_String> *names_expanded);
    void InsertBuffer(const uni_char *s, unsigned length);
    /**< Insert the given string at the current position in the input buffer.

         The current input segment is split up or reorganised to have the
         newly inserted segment string appear at the current buffer position
         when the buffer is traversed. */

    void InsertBufferSmall(const uni_char *s, unsigned length);
    /**< Buffer helper method for InsertBuffer(), handling insertion at the
         tail of input buffer segments (when input is kept in a separate
         smallbuffer.) */

    void SplitBufferSegment(unsigned index, const uni_char *position, unsigned skip);
    /**< End/terminate input segment 'index' at the given position, adding a
         following segment that starts at (position + skip) and includes
         remaining contents of the 'index' segment.

         The split operation is used to snip out CPP names from the input segment
         sequence, subsequently inserting their expanded contents into an input
         segment at position (index + 1). */

    void Advance(unsigned count = 1);

    class ExprResult
    {
    public:
        enum Kind
        {
            Int,
            Bool,
            Error
        };

        enum ERROR_KIND
        {
            ERROR_TAG = -1
        };

        enum Operator
        {
            None = 0,
            Mul,
            Div,
            Mod,
            Add,
            Sub,
            LShift,
            RShift,
            Lt,
            Le,
            Eq,
            NEq,
            Ge,
            Gt,
            BitAnd,
            BitXor,
            BitOr,
            And,
            Or
        };

        ExprResult(int i)
            : kind(Int)
            , num_result(i)
            , bool_result(FALSE)
            , error_result(INT_MIN)
        {
        }

        ExprResult(enum ERROR_KIND x)
            : kind(Error)
            , num_result(0)
            , bool_result(FALSE)
            , error_result(x)
        {
        }

        ExprResult(BOOL b, BOOL unused)
            : kind(Bool)
            , num_result(0)
            , bool_result(b)
            , error_result(INT_MIN)
        {
        }

        Kind kind;
        int num_result;
        BOOL bool_result;
        int error_result;

        static unsigned GetPrecedence(Operator x);
        static int ToInt(ExprResult &r);
    };

    ExprResult ParseBinaryExpression(unsigned prec);
    ExprResult ParseExpression();
    ExprResult ParseLitExpression();
    ExprResult::Operator LexOperator(BOOL eat_input);

    static ExprResult EvalExpression(ExprResult::Operator op, ExprResult e1, ExprResult e2);
    void PushSilentEvalError();
    void PopSilentEvalError();
};

class WGL_Lexer
    : public WGL_LexBuffer
{
public:
    virtual ~WGL_Lexer();

    static void MakeL(WGL_Lexer *&lexer, WGL_Context *context);
    void ConstructL(WGL_ProgramText *program_array, unsigned size, BOOL for_vertex);

    int GetToken();

    unsigned GetErrorContext(uni_char *buffer, unsigned len);
    /**< Copy at most 'len' characters from current buffer; used when generating
         context for error messages. Returns length of string. */

    union Lexeme
    {
        double val_double;
        WGL_String *val_id;
        int val_int;
        unsigned val_uint;
        unsigned val_ty;
        WGL_String *val_string;
    };

    uni_char *numeric_buffer;
    unsigned numeric_buffer_length;

    Lexeme lexeme;

    enum  BuiltinTypes
    {
        TYPE_VEC2,
        TYPE_VEC3,
        TYPE_VEC4,
        TYPE_BVEC2,
        TYPE_BVEC3,
        TYPE_BVEC4,
        TYPE_IVEC2,
        TYPE_IVEC3,
        TYPE_IVEC4,
        TYPE_UVEC2,
        TYPE_UVEC3,
        TYPE_UVEC4,
        TYPE_MAT2,
        TYPE_MAT3,
        TYPE_MAT4,
        TYPE_MAT2X2,
        TYPE_MAT2X3,
        TYPE_MAT2X4,
        TYPE_MAT3X2,
        TYPE_MAT3X3,
        TYPE_MAT3X4,
        TYPE_MAT4X2,
        TYPE_MAT4X3,
        TYPE_MAT4X4,
        TYPE_SAMPLER1D,
        TYPE_SAMPLER2D,
        TYPE_SAMPLER3D,
        TYPE_SAMPLERCUBE,
        TYPE_SAMPLER1DSHADOW,
        TYPE_SAMPLER2DSHADOW,
        TYPE_SAMPLERCUBESHADOW,
        TYPE_SAMPLER1DARRAY,
        TYPE_SAMPLER2DARRAY,
        TYPE_SAMPLER1DARRAYSHADOW,
        TYPE_SAMPLER2DARRAYSHADOW,
        TYPE_SAMPLERBUFFER,
        TYPE_SAMPLER2DMS,
        TYPE_SAMPLER2DMSARRAY,
        TYPE_SAMPLER2DRECT,
        TYPE_SAMPLER2DRECTSHADOW,
        TYPE_ISAMPLER1D,
        TYPE_ISAMPLER2D,
        TYPE_ISAMPLER3D,
        TYPE_ISAMPLERCUBE,
        TYPE_ISAMPLER1DARRAY,
        TYPE_ISAMPLER2DARRAY,
        TYPE_ISAMPLER2DRECT,
        TYPE_ISAMPLERBUFFER,
        TYPE_ISAMPLER2DMS,
        TYPE_ISAMPLER2DMSARRAY,
        TYPE_USAMPLER1D,
        TYPE_USAMPLER2D,
        TYPE_USAMPLER3D,
        TYPE_USAMPLERCUBE,
        TYPE_USAMPLER1DARRAY,
        TYPE_USAMPLER2DARRAY,
        TYPE_USAMPLER2DRECT,
        TYPE_USAMPLERBUFFER,
        TYPE_USAMPLER2DMS,
        TYPE_USAMPLER2DMSARRAY
    };

private:
    WGL_Lexer(WGL_Context *context);

    int LexString();
    int LexDecimalNumber();
    int LexHexNumber();

};

#endif // WGL_LEXER_H
