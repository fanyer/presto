/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2009-2011
 *
 * WebGL GLSL compiler -- lexical analysis.
 *
 * Patterned on es_lexer, esp. implementation.
 */
#include "core/pch.h"

#ifdef CANVAS3D_SUPPORT
#include "modules/webgl/src/wgl_base.h"
#include "modules/webgl/src/wgl_ast.h"
#include "modules/webgl/src/wgl_lexer.h"
#include "modules/webgl/src/wgl_symbol.h"
#include "modules/webgl/src/wgl_context.h"
#include "modules/webgl/src/wgl_string.h"

static const uni_char CHAR_HT = 0x09;
static const uni_char CHAR_LF = 0x0A;
static const uni_char CHAR_VT = 0x0B;
static const uni_char CHAR_FF = 0x0C;
static const uni_char CHAR_CR = 0x0D;
static const uni_char CHAR_BOM = 0xFEFF;  // 'Byte order mark' (AKA Zero Width No-Break Space)

/* GLES2.0 does not support line-continuation characters in #defines,
   so that functionality is not exposed. */
/* #define WGL_SUPPORT_LINE_CONTINUATION_CHARACTERS */

#define WGL_GLSL_VERSION_NUMBER 100
#define WGL_GLSL_VERSION_NUMBER_STRING UNI_L("100")

/** Number of lines to include in lexer error messages */
#define WGL_LINES_ERROR_CONTEXT 3

#define wgl_isalnum(x) ((x) >= 'a' && (x) <= 'z' || (x) >= 'A' && (x) <= 'Z' || (x) >= '0' && (x) <= '9')
#define wgl_isalpha(x) ((x) >= 'a' && (x) <= 'z' || (x) >= 'A' && (x) <= 'Z')
#define wgl_isident0(x) (wgl_isalpha(x) || (x) == '_')
#define wgl_isident(x) (wgl_isalnum(x) || (x) == '_')
#define wgl_isdigit(x) ((x) >= '0' && (x) <= '9')
#define wgl_isxdigit(x) (wgl_isdigit(x) || (x) >= 'a' && (x) <='f' || (x) >= 'A' && (x) <= 'F')
#define wgl_isspace(x)  ((x) == 0x20 || (x) >= 0x09 && (x) <= 0x0d)
#define wgl_isspace_no_nl(x)  ((x) == 0x20 || (x) == 0x09 || (x) == 0x0b || (x) == 0x0c)

#define MATCH2(z,a,b) (buf_ptr[z] == a && buf_ptr[z+1] == b)
#define MATCH3(z,a,b,c) (buf_ptr[z] == a && buf_ptr[z+1] == b && buf_ptr[z+2] == c)
#define MATCH4(z,a,b,c,d) (buf_ptr[z] == a && buf_ptr[z+1] == b && buf_ptr[z+2] == c && buf_ptr[z+3] == d)
#define MATCH5(z,a,b,c,d,e) (buf_ptr[z] == a && buf_ptr[z+1] == b && buf_ptr[z+2] == c && buf_ptr[z+3] == d && buf_ptr[z+4] == e)
#define MATCH6(z,a,b,c,d,e,f) (buf_ptr[z] == a && buf_ptr[z+1] == b && buf_ptr[z+2] == c && buf_ptr[z+3] == d && buf_ptr[z+4] == e && buf_ptr[z+5] == f)
#define MATCH7(z,a,b,c,d,e,f,g) (buf_ptr[z] == a && buf_ptr[z+1] == b && buf_ptr[z+2] == c && buf_ptr[z+3] == d && buf_ptr[z+4] == e && buf_ptr[z+5] == f && buf_ptr[z+6] == g)
#define MATCH8(z,a,b,c,d,e,f,g,h) (buf_ptr[z] == a && buf_ptr[z+1] == b && buf_ptr[z+2] == c && buf_ptr[z+3] == d && buf_ptr[z+4] == e && buf_ptr[z+5] == f && buf_ptr[z+6] == g && buf_ptr[z+7] == h)
#define MATCH9(z,a,b,c,d,e,f,g,h,i) (buf_ptr[z] == a && buf_ptr[z+1] == b && buf_ptr[z+2] == c && buf_ptr[z+3] == d && buf_ptr[z+4] == e && buf_ptr[z+5] == f && buf_ptr[z+6] == g && buf_ptr[z+7] == h && buf_ptr[z+8] == i)
#define MATCH10(z,a,b,c,d,e,f,g,h,i,j) (buf_ptr[z] == a && buf_ptr[z+1] == b && buf_ptr[z+2] == c && buf_ptr[z+3] == d && buf_ptr[z+4] == e && buf_ptr[z+5] == f && buf_ptr[z+6] == g && buf_ptr[z+7] == h && buf_ptr[z+8] == i && buf_ptr[z+9] == j)
#define MATCH11(z,a,b,c,d,e,f,g,h,i,j,k) (buf_ptr[z] == a && buf_ptr[z+1] == b && buf_ptr[z+2] == c && buf_ptr[z+3] == d && buf_ptr[z+4] == e && buf_ptr[z+5] == f && buf_ptr[z+6] == g && buf_ptr[z+7] == h && buf_ptr[z+8] == i && buf_ptr[z+9] == j && buf_ptr[z+10] == k)

#define KWDEND( x ) !wgl_isident(buf_ptr[x])

WGL_LexBuffer::WGL_LexBuffer(WGL_Context *context)
    : context(context)
    , source_context(NULL)
    , input_segments(NULL)
    , input_segments_used(0)
    , input_segments_allocated(0)
    , current_input_segment(0)
    , buf_start(NULL)
    , buf_ptr(NULL)
    , buf_end(NULL)
    , buf_ptr_save(NULL)
    , line_number(0)
    , in_smallbuffer(FALSE)
    , flush_smallbuffer(FALSE)
    , consumed_from_next_segment(0)
    , is_pound_recognised(FALSE)
{
}

WGL_Lexer::WGL_Lexer(WGL_Context *context)
    : WGL_LexBuffer(context)
    , numeric_buffer(NULL)
    , numeric_buffer_length(0)
{
}

WGL_Lexer::~WGL_Lexer()
{
}

/* static */ void
WGL_Lexer::MakeL(WGL_Lexer *&lexer, WGL_Context *context)
{
    lexer = OP_NEWGRO_L(WGL_Lexer, (context), context->Arena());
}

BOOL
WGL_LexBuffer::SkipComment()
{
    while (TRUE)
    {
        if (AtEndOfInput() || !buf_ptr[0])
            return FALSE;

        if (buf_end - buf_ptr < 2)
        {
            FillBuffer();
            continue;
        }
        if (buf_ptr[0] == '*' && buf_ptr[1] == '/')
        {
            *buf_ptr++ = ' ';
            *buf_ptr++ = ' ';
            return TRUE;
        }
        else if (buf_ptr[0] == '\r')
        {
            if (buf_end - buf_ptr < 2)
            {
                FillBuffer();
                if (AtEndOfInput())
                    return FALSE;
            }
            if (buf_ptr[1] == '\n')
                buf_ptr++;

            line_number++;
        }
        else if (buf_ptr[0] == '\n')
            line_number++;

        *buf_ptr++ = ' ';
    }
}

WGL_CPP::WGL_CPP(WGL_Context *context)
        : WGL_LexBuffer(context)
        , cpp_inactive_nesting(0)
        , is_silent(FALSE)
        , seen_cpp_directive(FALSE)
{
}

static void
ReleaseDefineParams(const void *key, void *define_data)
{
    reinterpret_cast<WGL_CPP::DefineData *>(define_data)->parameters.Clear();
}

WGL_CPP::~WGL_CPP()
{
    defines_map.ForEach(ReleaseDefineParams);
    defines_map.RemoveAll();
}

void
WGL_CPP::InitializeL()
{
    WGL_String *std = WGL_String::Make(context, UNI_L("GL_ES"));
    WGL_CPP::DefineData *define_data = OP_NEWGRO_L(DefineData, (context), context->Arena());
    define_data->rhs = UNI_L("1");
    LEAVE_IF_ERROR(defines_map.Add(std, define_data));

    std = WGL_String::Make(context, UNI_L("__VERSION__"));
    define_data = OP_NEWGRO_L(DefineData, (context), context->Arena());
    define_data->rhs = WGL_GLSL_VERSION_NUMBER_STRING;
    LEAVE_IF_ERROR(defines_map.Add(std, define_data));

    /* If the extension requires it, announce it as supported via CPP defines. */
    unsigned exts_count = context->GetExtensionCount();
    for (unsigned i = 0; i < exts_count; i++)
    {
        WGL_GLSLBuiltins::Extension id;
        if (context->GetExtensionIndex(i, id))
        {
            const uni_char *cpp_supported = NULL;
            if (context->GetExtensionInfo(id, NULL, NULL, &cpp_supported) && cpp_supported)
                AddDefineL(WGL_String::Make(context, cpp_supported), UNI_L("1"));
        }
    }
}

BOOL
WGL_CPP::IsTrue(BOOL is_defined, const uni_char *id, unsigned id_length)
{
    if ((id_length == 5 && uni_strncmp(id, UNI_L("GL_ES"), 5) == 0) ||
        FindExpansion(id, id_length) != NULL)
        return is_defined;
    else
        return !is_defined;
}

const uni_char *
WGL_CPP::FindExpansion(const uni_char *s, unsigned len, DefineData **out_data)
{
    OpHashIterator *iter = defines_map.GetIterator();
    OP_STATUS status = iter ? iter->First() : OpStatus::ERR;
    while (OpStatus::IsSuccess(status))
    {
        const WGL_String *nm = reinterpret_cast<const WGL_String *>(iter->GetKey());
        if (nm && len == nm->length && uni_strncmp(nm->value, s, len) == 0)
        {
            DefineData *data = static_cast<DefineData *>(iter->GetData());
            if (out_data)
                *out_data = data;
            OP_DELETE(iter);
            return data->rhs;
        }
        status = iter->Next();
    }
    OP_DELETE(iter);
    if (uni_strncmp(s, UNI_L("__LINE__"), len) == 0)
    {
        uni_char *line_str = OP_NEWGROA_L(uni_char, 16, context->Arena());
        uni_snprintf(line_str, 16, UNI_L("%u"), line_number);
        if (out_data)
        {
            *out_data = OP_NEWGRO_L(DefineData, (context), context->Arena());
            (*out_data)->rhs = line_str;
        }
        return const_cast<uni_char *>(line_str);
    }
    else if (len == 8 && uni_strncmp(s, UNI_L("__FILE__"), len) == 0)
    {
        const uni_char *src = source_context ? source_context : context->GetSourceContext();
        if (out_data)
        {
            *out_data = OP_NEWGRO_L(DefineData, (context), context->Arena());
            (*out_data)->rhs = src;
        }
        return src;
    }
    else
        return NULL;
}

const uni_char *
WGL_CPP::HandleMacroExpansion(WGL_String *name, DefineData *data, OpVector<WGL_String> *names_expanded)
{
    OpVector<uni_char> arguments;
    unsigned param_count;
    if (data && ((param_count = data->parameters.GetCount()) > 0 || data->IsFunctionDefine()))
    {
        SkipWhitespace();
        if (buf_ptr[0] == '(')
        {
            Eat();
            SkipWhitespace();
            BOOL in_quotes = FALSE;
            unsigned open_parens = 0;
            const uni_char *arg_start = BufferPosition();
            while (!AtEndOfInput() && param_count > 0)
            {
                if (buf_ptr[0] == '\\' && in_quotes)
                {
                    Eat();
                    if (buf_ptr[0] == '"')
                        Eat();
                }
                else if (buf_ptr[0] == '"')
                {
                    Eat();
                    in_quotes = !in_quotes;
                }
                else if ((buf_ptr[0] == '(' || (buf_ptr[0] == ')' && open_parens > 0)) && !in_quotes)
                {
                    if (buf_ptr[0] == '(')
                        open_parens++;
                    else
                        open_parens--;
                    Eat();
                }
                else if ((buf_ptr[0] == ',' && !in_quotes && open_parens == 0) || (open_parens == 0 && buf_ptr[0] == ')' && !in_quotes))
                {
                    unsigned arg_length = ComputeBufferOffset(arg_start);

                    /* Don't include whitespace that surrounds the argument. */
                    while (arg_length > 0)
                    {
                        if (!wgl_isspace(BufferPeekBehind(arg_length - 1)))
                            break;
                        arg_length--;
                    }

                    uni_char *arg = OP_NEWGROA_L(uni_char, arg_length + 1, context->Arena());
                    BufferCopy(arg, arg_start, arg_length);
                    arg[arg_length] = 0;

                    LEAVE_IF_ERROR(arguments.Add(arg));
                    if (buf_ptr[0] == ')' && param_count > 1)
                    {
                        AddError(WGL_Error::CPP_MACRO_MISMATCH, Storage(context, name));
                        return NULL;
                    }
                    param_count--;
                    if (param_count != 0)
                        Eat();
                    arg_start = BufferPosition();
                }
                else
                    Eat();
            }
            if (!AtEndOfInput() && buf_ptr[0] != ')')
            {
                AddError(WGL_Error::CPP_MACRO_MISMATCH, Storage(context, name));
                return NULL;
            }
            else
                Eat();
        }
        else
        {
            AddError(WGL_Error::CPP_MACRO_MISMATCH, Storage(context, name));
            return NULL;
        }
    }

    const uni_char *rhs_ptr = data->rhs;
    const uni_char *expanded_start = rhs_ptr;
    while (*rhs_ptr)
    {
        if (wgl_isident0(rhs_ptr[0]))
        {
            const uni_char *id_end = rhs_ptr + 1;
            while (wgl_isident(*id_end))
                id_end++;

            WGL_String *arg_str = WGL_String::Make(context, rhs_ptr, static_cast<unsigned>(id_end - rhs_ptr));
            DefineData *new_data;
            int index;
            /* Names are not repeatedly expanded. */
            if (names_expanded->Find(arg_str) >= 0)
                rhs_ptr = id_end + 1;
            else if ((index = data->parameters.Find(arg_str)) >= 0)
            {
                uni_char *arg = arguments.Get(index);
                unsigned old_expanded_len = static_cast<unsigned>(rhs_ptr - expanded_start);
                unsigned arg_len = static_cast<unsigned>(uni_strlen(arg));
                unsigned rhs_len = static_cast<unsigned>(uni_strlen(id_end));
                uni_char *new_str = OP_NEWGROA_L(uni_char, old_expanded_len + arg_len + rhs_len + 1, context->Arena());

                if (old_expanded_len)
                    uni_strncpy(new_str, expanded_start, rhs_ptr - expanded_start);
                uni_strncpy(new_str + old_expanded_len, arg, arg_len);
                uni_strncpy(new_str + old_expanded_len + arg_len, id_end, rhs_len);
                new_str[old_expanded_len + arg_len + rhs_len] = 0;
                /* Yes, old buffer is simply dropped - arena deallocation tidies up (and can handle the load..) */
                rhs_ptr = new_str + old_expanded_len + arg_len;
                expanded_start = new_str;
            }
            else if (OpStatus::IsSuccess(defines_map.GetData(arg_str, &new_data)))
            {
                names_expanded->Add(arg_str);
                const uni_char *expansion = HandleMacroExpansion(arg_str, new_data, names_expanded);
                names_expanded->Remove(names_expanded->GetCount() - 1);

                unsigned old_expanded_len = static_cast<unsigned>(rhs_ptr - expanded_start);
                unsigned exp_len = expansion ? static_cast<unsigned>(uni_strlen(expansion)) : 0;
                unsigned rhs_len = static_cast<unsigned>(uni_strlen(id_end));
                uni_char *new_str = OP_NEWGROA_L(uni_char, old_expanded_len + exp_len + rhs_len + 1, context->Arena());

                if (old_expanded_len)
                    uni_strncpy(new_str, expanded_start, rhs_ptr - expanded_start);
                if (expansion)
                    uni_strncpy(new_str + old_expanded_len, expansion, exp_len);
                uni_strncpy(new_str + old_expanded_len + exp_len, id_end, rhs_len);
                new_str[old_expanded_len + exp_len + rhs_len] = 0;
                /* Copy in the expanded string, but do not process it further. */
                rhs_ptr = new_str + exp_len;
                expanded_start = new_str;
            }
            else
                rhs_ptr = id_end;
        }
        else
            rhs_ptr++;
    }

    /* In preparation for making this an input segment, pad its length
       and append NULs so as not run the risk of peeking past the end. */
    if (uni_strlen(expanded_start) < WGL_PARM_LEXBUFFER_GUARANTEED)
    {
        uni_char *expanded_long = OP_NEWGROA_L(uni_char, WGL_PARM_LEXBUFFER_GUARANTEED, context->Arena());
        op_memset(expanded_long, 0, WGL_PARM_LEXBUFFER_GUARANTEED * sizeof(uni_char));
        uni_strcpy(expanded_long, expanded_start);
        return expanded_long;
    }
    else
        return expanded_start;
}

void
WGL_CPP::SplitBufferSegment(unsigned index, const uni_char *position, unsigned skip)
{
    OP_ASSERT(index <= input_segments_used);
    if (position < input_segments[index].segment_text + input_segments[index].segment_length)
    {
        if (input_segments_used + 1 >= input_segments_allocated)
        {
            input_segments_allocated *= 2;
            OP_ASSERT(input_segments_allocated > input_segments_used);
            if (input_segments_allocated <= input_segments_used)
                /* Unlikely, but number of segments overflowed. Reported as an OOM. */
                LEAVE(OpStatus::ERR_NO_MEMORY);

            InputSegment *new_segments = OP_NEWGROA_L(InputSegment, input_segments_allocated, context->Arena());
            op_memcpy(new_segments, input_segments, input_segments_used * sizeof(InputSegment));
            /* Note: overwrite assumes arena allocation freeing up previous. */
            input_segments = new_segments;
        }

        if (position == input_segments[index].segment_text && skip == input_segments[index].segment_length)
        {
            input_segments[index].segment_ptr = input_segments[index].segment_text;
            input_segments[index].segment_length = 0;
            input_segments_used--;
        }
        else
        {
            op_memmove(&input_segments[index + 1], &input_segments[index], (input_segments_used - index + 1) * sizeof(InputSegment));
            input_segments[index].segment_ptr = const_cast<uni_char *>(position);
            input_segments[index].segment_length = position - input_segments[index].segment_text;

            input_segments[index + 1].segment_text = input_segments[index + 1].segment_ptr = const_cast<uni_char *>(position) + skip;
            input_segments[index + 1].segment_length -= input_segments[index].segment_length + skip;
        }

        if (index == current_input_segment)
        {
            buf_start = buf_ptr = input_segments[index + 1].segment_ptr;
            buf_end = input_segments[index + 1].segment_ptr + input_segments[index + 1].segment_length;
            AdvanceInputSegment();
            in_smallbuffer = FALSE;
            consumed_from_next_segment = 0;
        }
        input_segments_used++;
    }
    /* Resync the smallbuffer */
    if (in_smallbuffer)
    {
        flush_smallbuffer = TRUE;
        FillBufferIfNeeded();
    }
}

void
WGL_CPP::InsertBufferSmall(const uni_char *s, unsigned length)
{
    OP_ASSERT(in_smallbuffer);
    InputSegment *current_segment = &input_segments[current_input_segment];

    unsigned new_buffer_length = current_segment->segment_length + length;
    if (!FinalInputSegment())
        new_buffer_length += (current_segment + 1)->segment_length;

    uni_char *new_buffer = OP_NEWGROA_L(uni_char, new_buffer_length, context->Arena());

    if (smallbuffer_start - smallbuffer <= ConsumedFromCurrent())
    {
        unsigned len_segment = current_segment->segment_length - ConsumedFromCurrent() + static_cast<unsigned>(smallbuffer_start - smallbuffer);
        op_memcpy(new_buffer, current_segment->segment_text, len_segment * sizeof(uni_char));
        op_memcpy(new_buffer + len_segment, s, length * sizeof(uni_char));
        if (buf_ptr - smallbuffer <= ConsumedFromCurrent())
        {
            unsigned rem_length = ConsumedFromCurrent() - static_cast<unsigned>(buf_ptr - smallbuffer);
            op_memcpy(new_buffer + len_segment + length, current_segment->segment_text + current_segment->segment_length - rem_length, rem_length * sizeof(uni_char));
            if (!FinalInputSegment())
                op_memcpy(new_buffer + (len_segment + length + rem_length), (current_segment + 1)->segment_text, (current_segment + 1)->segment_length * sizeof(uni_char));
            current_segment->segment_length = len_segment + length + rem_length + (!FinalInputSegment() ? (current_segment + 1)->segment_length : 0);
        }

        else
        {
            unsigned rem_length = static_cast<unsigned>(buf_ptr - smallbuffer) - ConsumedFromCurrent();
            op_memcpy(new_buffer + len_segment + length, (current_segment + 1)->segment_text + rem_length, ((current_segment + 1)->segment_length - rem_length) * sizeof(uni_char));
            current_segment->segment_length = len_segment + length + ((current_segment + 1)->segment_length - rem_length);
        }
        if (current_input_segment < input_segments_used - 1)
        {
            op_memmove(current_segment + 1, current_segment + 2, (input_segments_used - current_input_segment) * sizeof(InputSegment));
            input_segments_used--;
        }
        current_segment->segment_text = new_buffer;
        current_segment->segment_ptr = new_buffer + len_segment;
        buf_start = current_segment->segment_ptr;
        buf_ptr = const_cast<uni_char *>(buf_start);
        buf_end = buf_start + current_segment->segment_length;

        consumed_from_next_segment = 0;
    }
    else
    {
        unsigned len_segment = current_segment->segment_length;
        op_memcpy(new_buffer, current_segment->segment_text, len_segment * sizeof(uni_char));
        op_memcpy(new_buffer + len_segment, s, length * sizeof(uni_char));
        unsigned rem_length = consumed_from_next_segment;
        op_memcpy(new_buffer + (len_segment + length), (current_segment + 1)->segment_text + rem_length, ((current_segment + 1)->segment_length - rem_length) * sizeof(uni_char));
        current_segment->segment_length = len_segment + length + ((current_segment + 1)->segment_length - rem_length);
        if (current_input_segment < input_segments_used - 1)
        {
            op_memmove(current_segment + 1, current_segment + 2, (input_segments_used - current_input_segment) * sizeof(InputSegment));
            input_segments_used--;
        }
        current_segment->segment_text = new_buffer;
        current_segment->segment_ptr = new_buffer + len_segment;
        buf_start = current_segment->segment_ptr;
        buf_ptr = const_cast<uni_char *>(buf_start);
        buf_end = buf_start + current_segment->Available();
    }

    in_smallbuffer = FALSE;
    return;
}

void
WGL_CPP::InsertBuffer(const uni_char *s, unsigned length)
{
    if (input_segments_used + 2 >= input_segments_allocated)
    {
        /* Two segments are usually and at most inserted.
           Doubling the size will expand the table enough but
           for the N == 1 case, hence the use of (+3) below. */
        input_segments_allocated = MAX(2 * input_segments_allocated, input_segments_used + 3);
        OP_ASSERT(input_segments_allocated > (input_segments_used + 2));
        if (input_segments_allocated < (input_segments_used + 3))
            /* Unlikely, but number of segments overflowed. Reported as an OOM. */
            LEAVE(OpStatus::ERR_NO_MEMORY);

        InputSegment *new_segments = OP_NEWGROA_L(InputSegment, input_segments_allocated, context->Arena());
        op_memcpy(new_segments, input_segments, input_segments_used * sizeof(InputSegment));
        /* Note: overwrite assumes arena allocation freeing up previous. */
        input_segments = new_segments;
    }
    InputSegment *current_segment = &input_segments[current_input_segment];
    if (in_smallbuffer)
    {
        /* Too many cases to consider, so if we end up expanding while in the
           smallbuffer we just create a new super segment. */
        InsertBufferSmall(s, length);
        return;
    }

    /* Split up current segment; what's consumed, new chunk, what's left. */
    else if (current_segment->segment_text < current_segment->segment_ptr)
    {
        OP_ASSERT(!in_smallbuffer);
        if (!FinalInputSegment())
            op_memmove(current_segment + 3, current_segment + 1, (input_segments_used - current_input_segment - 1) * sizeof(InputSegment));
        (current_segment + 2)->segment_text = (current_segment + 2)->segment_ptr = buf_ptr;
        OP_ASSERT(current_segment->segment_length > static_cast<unsigned>(buf_ptr - current_segment->segment_text));
        (current_segment + 2)->segment_length = current_segment->segment_length - static_cast<unsigned>(buf_ptr - current_segment->segment_text);

        (current_segment + 1)->segment_text = (current_segment + 1)->segment_ptr = const_cast<uni_char *>(s);
        (current_segment + 1)->segment_length = length;

        current_segment->segment_length = current_segment->Used();

        AdvanceInputSegment();
        input_segments_used++;
    }
    else
    {
        if (current_input_segment + 1 < input_segments_used)
            op_memmove(current_segment + 2, current_segment + 1, (input_segments_used - current_input_segment) * sizeof(InputSegment));

        /* If expanded name appears at the start of the current segment, adjust
           start so that the lexer won't encounter the now-replaced name. */
        (current_segment + 1)->segment_text = (current_segment + 1)->segment_ptr = buf_ptr;
        (current_segment + 1)->segment_length = uni_strlen(buf_ptr);

        current_segment->segment_text = current_segment->segment_ptr = const_cast<uni_char *>(s);
        current_segment->segment_length = length;
    }

    current_segment = &input_segments[current_input_segment];
    input_segments_used++;
    buf_start = current_segment->segment_ptr;
    buf_ptr = const_cast<uni_char *>(buf_start);
    buf_end = buf_start + current_segment->Available();
}

void
WGL_CPP::HandleDefine()
{
    SkipWhitespace();
    WGL_String *name = NULL;
    int tok = LexId(name);
    if (tok < 0 || tok == WGL_Token::TOKEN_ILLEGAL)
    {
        AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);
        SkipUntilNextLine(TRUE);
        return;
    }

    DefineData *define_data = OP_NEWGRO_L(DefineData, (context), context->Arena());
    AddDefineL(name, define_data);
    if (buf_ptr[0] == '(')
    {
        Eat();
        SkipWhitespace(FALSE);
        define_data->SetIsFunctionDefine();
        const uni_char *current_name = GetBufferPosition();
        while (!AtEndOfInput())
        {
            if (buf_ptr[0] == ')')
            {
                if (current_name < buf_ptr)
                    define_data->AddParam(current_name, static_cast<unsigned>(buf_ptr - current_name));
                Eat();
                break;
            }
            if (wgl_isident0(buf_ptr[0]))
            {
                WGL_String *arg_name = NULL;
                int tok = LexId(arg_name);
                if (tok < 0 || tok == WGL_Token::TOKEN_ILLEGAL)
                {
                    Eat();
                    continue;
                }
                define_data->AddParam(arg_name);
                SkipWhitespace();
                current_name = buf_ptr;
                if (!AtEndOfInput() && buf_ptr[0] == ',')
                {
                    Eat();
                    SkipWhitespace();
                }
                else if (AtEndOfInput() || buf_ptr[0] != ')')
                {
                    AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);
                    SkipUntilNextLine(TRUE);
                    return;
                }
            }
            else
            {
                AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);
                SkipUntilNextLine(TRUE);
                return;
            }
        }
    }
    SkipWhitespace(FALSE);

#ifdef WGL_SUPPORT_LINE_CONTINUATION_CHARACTERS
    while (buf_ptr[0] == '\\' && buf_end - buf_ptr > 1)
    {
        if (buf_ptr[1] == '\r' || buf_ptr[1] == '\n')
        {
            Eat();
            SkipWhitespace();
        }
        else
            break;
    }
#endif // WGL_SUPPORT_LINE_CONTINUATION_CHARACTERS
    const uni_char *rhs_start = buf_ptr;
    while (!AtEndOfInput())
    {
        if (buf_ptr[0] == '\r' || buf_ptr[0] == '\n')
            break;
        else if ((buf_ptr[0] == '/' && buf_ptr[1] == '/'))
            /* line comment acts as terminator. */
            break;
#if WGL_SUPPORT_LINE_CONTINUATION_CHARACTERS
        else if (buf_ptr[0] == '\\' && buf_end - buf_ptr > 1)
        {
            if (buf_ptr[1] == '\r' || buf_ptr[1] == '\n')
            {
                buf_ptr[0] = buf_ptr[1] = ' ';
                SkipWhitespace();
            }
        }
#endif // WGL_SUPPORT_LINE_CONTINUATION_CHARACTERS
        else
            Eat();
    }
    const uni_char *rhs_end = buf_ptr;
    while (rhs_end > rhs_start && wgl_isspace(*(rhs_end - 1)))
        rhs_end--;

    if (rhs_start >= rhs_end)
        define_data->rhs = UNI_L("");
    else
    {
        unsigned rhs_length = static_cast<unsigned>(rhs_end - rhs_start);
        uni_char *rhs = OP_NEWGROA_L(uni_char, rhs_length + 1, context->Arena());
        uni_strncpy(rhs, rhs_start, rhs_length);
        rhs[rhs_length] = 0;

        define_data->rhs = rhs;
    }
}

void
WGL_CPP::HandleUndef()
{
    SkipWhitespace();
    unsigned id_length = 0;
    const uni_char *id_start = LexIdentifier(id_length);
    if (id_length == 0)
    {
        AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);
        SkipUntilNextLine(TRUE);
        return;
    }
    else
    {
        WGL_String *name = WGL_String::Make(context, id_start, id_length);
        DefineData *data;
        defines_map.Remove(name, &data);
        if (data)
            data->parameters.Clear();
    }
}

void
WGL_CPP::HandleExtension()
{
    unsigned id_length = 0;
    const uni_char *id_start = LexIdentifier(id_length);
    SkipWhitespace(FALSE);
    if (buf_ptr[0] != ':')
    {
        AddError(WGL_Error::CPP_EXTENSION_NOT_SUPPORTED, id_start, static_cast<unsigned>(buf_ptr - id_start), FALSE);
        SkipUntilNextLine(TRUE);
    }
    else
    {
        Eat();
        SkipWhitespace();
        unsigned beh_length = 0;
        const uni_char *behaviour_start = LexIdentifier(beh_length);
        switch (CheckExtension(id_start, id_length, behaviour_start, beh_length))
        {
        case ExtensionUnsupported:
            AddError(WGL_Error::CPP_EXTENSION_NOT_SUPPORTED, id_start, static_cast<unsigned>(buf_ptr - id_start), FALSE);
            break;
        case ExtensionIllegal:
            AddError(WGL_Error::CPP_EXTENSION_DIRECTIVE_ERROR, id_start, static_cast<unsigned>(buf_ptr - id_start), FALSE);
            break;
        case ExtensionWarnUnsupported:
            AddError(WGL_Error::CPP_EXTENSION_WARN_NOT_SUPPORTED, id_start, static_cast<unsigned>(buf_ptr - id_start), FALSE);
            break;
        default:
            OP_ASSERT(!"Unexpected extension status");
        case ExtensionOK:
            break;
        }
    }
}

void
WGL_LexBuffer::HandleLinePragma()
{
    SkipWhitespace(FALSE);
    if (!wgl_isdigit(buf_ptr[0]))
    {
        AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);
        SkipUntilNextLine(TRUE);
        return;
    }
    line_number = LexNumLiteral();
    SkipWhitespace(FALSE);
    if (wgl_isdigit(buf_ptr[0]))
    {
        const uni_char *source_start = buf_ptr;
        Eat();
        while (wgl_isdigit(buf_ptr[0]))
            Eat();
        uni_char *src = OP_NEWGROA_L(uni_char, buf_ptr - source_start + 1, context->Arena());
        /* NOTE: this is the source-string-number, as defined by the spec, not the "file" string
           portion that other CPP dialects support. */
        uni_strncpy(src, source_start, buf_ptr - source_start);
        src[buf_ptr - source_start] = 0;
        source_context = src;
    }
    if (!wgl_isspace(buf_ptr[0]))
        AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);

    SkipUntilNextLine(TRUE);
    return;
}

void
WGL_CPP::AddDefineL(WGL_String *name, DefineData *data)
{
    OP_ASSERT(name);
    DefineData *old_data = NULL;
    if (OpStatus::IsSuccess(defines_map.GetData(name, &old_data)))
    {
        if (!data->rhs)
            AddError(WGL_Error::CPP_SYNTAX_ERROR, UNI_L("") );
        else if (data->parameters.GetCount() != old_data->parameters.GetCount() || !uni_str_eq(data->rhs, old_data->rhs))
            AddError(WGL_Error::CPP_DUPLICATE_DEFINE, name->value);
    }
    else
        LEAVE_IF_ERROR(defines_map.Add(name, data));
}

void
WGL_CPP::AddDefineL(WGL_String *name, const uni_char *val)
{
    OP_ASSERT(name);
    DefineData *old_data = NULL;
    if (OpStatus::IsSuccess(defines_map.GetData(name, &old_data)))
    {
        if (old_data->parameters.GetCount() != 0 || !uni_str_eq(old_data->rhs, val))
            AddError(WGL_Error::CPP_DUPLICATE_DEFINE, name->value);

        return;
    }
    DefineData *ddata = OP_NEWGRO_L(DefineData, (context), context->Arena());
    ddata->rhs = val;
    LEAVE_IF_ERROR(defines_map.Add(name, ddata));
}

void
WGL_CPP::RemoveDefineL(WGL_String *name)
{
    if (name)
    {
        DefineData *old_data = NULL;
        LEAVE_IF_ERROR(defines_map.Remove(name, &old_data));
        OP_ASSERT(old_data);
    }
}

void
WGL_CPP::DefineData::AddParam(const uni_char *s, unsigned l)
{
    WGL_String *p = WGL_String::Make(context, s, l);
    LEAVE_IF_ERROR(parameters.Add(p));
}

void
WGL_CPP::DefineData::AddParam(WGL_String *p)
{
    LEAVE_IF_ERROR(parameters.Add(p));
}


WGL_CPP::ExprResult::Operator
WGL_CPP::LexOperator(BOOL eat)
{
    unsigned eat_offset = eat ? 0 : 1;
    ExprResult::Operator op = ExprResult::None;
 again:
    switch (buf_ptr[0])
    {
    case '*':
        if (eat)
            Eat();
        op = ExprResult::Mul;
        break;
    case '/':
        if (eat)
            Eat();
        if (buf_ptr[eat_offset] == '*')
        {
            if (eat)
                Eat();
            else
                Eat(2);
            if (!SkipComment())
            {
                AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);
                return op;
            }
            SkipWhitespace();
            goto again;
        }
        else if (buf_ptr[eat_offset] == '/')
        {
            SkipUntilNextLine(TRUE);
            goto again;
        }
        else
            op = ExprResult::Div;
        break;
    case '%':
        if (eat) Eat();
        op = ExprResult::Mod;
        break;
    case '+':
        if (eat) Eat();
        op = ExprResult::Add;
        break;
    case '-':
        if (eat) Eat();
        op = ExprResult::Sub;
        break;
    case '<':
        if (eat) Eat();
        if (buf_ptr[eat_offset] == '<')
        {
            if (eat) Eat();
            op = ExprResult::LShift;
        }
        else if (buf_ptr[eat_offset] == '=')
        {
            if (eat) Eat();
            op = ExprResult::Le;
        }
        else
            op = ExprResult::Lt;
        break;
    case '>':
        if (eat) Eat();
        if (buf_ptr[eat_offset] == '>')
        {
            if (eat) Eat();
            op = ExprResult::RShift;
        }
        else if (buf_ptr[eat_offset] == '=')
        {
            if (eat) Eat();
            op = ExprResult::Ge;
        }
        else
            op = ExprResult::Gt;
        break;
    case '=':
        if (eat) Eat();
        if (buf_ptr[eat_offset] == '=')
        {
            if (eat) Eat();
            op = ExprResult::Eq;
        }
        else
        {
            AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);
            return op;
        }
        break;
    case '!':
        if (eat) Eat();
        if (buf_ptr[eat_offset] == '=')
        {
            if (eat) Eat();
            op = ExprResult::NEq;
        }
        else
        {
            AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);
            return op;
        }
        break;
    case '&':
        if (eat) Eat();
        if (buf_ptr[eat_offset] == '&')
        {
            if (eat) Eat();
            op = ExprResult::And;
        }
        else
            op = ExprResult::BitAnd;
        break;
    case '|':
        if (eat) Eat();
        if (buf_ptr[eat_offset] == '|')
        {
            if (eat) Eat();
            op = ExprResult::Or;
        }
        else
            op = ExprResult::BitOr;
        break;
    case '^':
        if (eat) Eat();
        op = ExprResult::BitXor;
        break;
    default:
        break;
    }
    return op;
}

/* static */ int
WGL_CPP::ExprResult::ToInt(ExprResult &r)
{
    if (r.kind == Bool)
        return (r.bool_result ? 1 : 0);
    else if (r.kind == Int)
        return (r.num_result);
    else
        return 0;
}

/* static */ WGL_CPP::ExprResult
WGL_CPP::EvalExpression(ExprResult::Operator op, ExprResult e1, ExprResult e2)
{
    OP_ASSERT(e1.kind != ExprResult::Error && e2.kind != ExprResult::Error);
    switch (op)
    {
    case ExprResult::Mul:
    case ExprResult::Div:
    case ExprResult::Mod:
    case ExprResult::Add:
    case ExprResult::Sub:
    case ExprResult::LShift:
    case ExprResult::RShift:
    case ExprResult::BitAnd:
    case ExprResult::BitOr:
    case ExprResult::BitXor:
    {
        int i1 = ExprResult::ToInt(e1);
        int i2 = ExprResult::ToInt(e2);
        switch (op)
        {
        case ExprResult::Mul:
            return ExprResult(i1 * i2);
        case ExprResult::Div:
            if (i2 == 0)
                return ExprResult(0);
            else
                return ExprResult(i1 / i2);
        case ExprResult::Mod:
            if (i2 == 0)
                return ExprResult(0);
            else
                return ExprResult(i1 % i2);
        case ExprResult::Add:
            return ExprResult(i1 + i2);
        case ExprResult::Sub:
            return ExprResult(i1 - i2);
        case ExprResult::LShift:
            return ExprResult(i1 << i2);
        case ExprResult::RShift:
            return ExprResult(i1 >> i2);
        case ExprResult::BitAnd:
            return ExprResult(i1 & i2);
        case ExprResult::BitOr:
            return ExprResult(i1 | i2);
        case ExprResult::BitXor:
            return ExprResult(i1 ^ i2);
        }
        break;
    }
    case ExprResult::Lt:
    case ExprResult::Le:
    case ExprResult::Eq:
    case ExprResult::NEq:
    case ExprResult::Ge:
    case ExprResult::Gt:
    {
        int i1 = ExprResult::ToInt(e1);
        int i2 = ExprResult::ToInt(e2);
        switch (op)
        {
        case ExprResult::Lt:  return ExprResult(i1 < i2, FALSE);
        case ExprResult::Le:  return ExprResult(i1 <= i2, FALSE);
        case ExprResult::Eq:  return ExprResult(i1 == i2, FALSE);
        case ExprResult::NEq: return ExprResult(i1 != i2, FALSE);
        case ExprResult::Ge: return ExprResult(i1 >= i2, FALSE);
        case ExprResult::Gt: return ExprResult(i1 > i2, FALSE);
        }
    }
    case ExprResult::And:
    case ExprResult::Or:
    {
        int i1 = ExprResult::ToInt(e1);
        int i2 = ExprResult::ToInt(e2);
        if (op == ExprResult::And)
            return ExprResult(i1 > 0 && i2 > 0, FALSE);
        else
            return ExprResult(i1 > 0 || i2 > 0, FALSE);
        break;
    }
    default:
        OP_ASSERT(!"Unhandled operator");
        break;
    }
    return ExprResult(ExprResult::ERROR_TAG);
}

unsigned
WGL_CPP::ExprResult::GetPrecedence(ExprResult::Operator x)
{
    return (static_cast<unsigned>(ExprResult::Or) - static_cast<unsigned>(x));
}

void
WGL_CPP::PushSilentEvalError()
{
    LEAVE_IF_ERROR(silent_eval_contexts.Add(is_silent));
    is_silent = TRUE;
}

void
WGL_CPP::PopSilentEvalError()
{
    is_silent = silent_eval_contexts.Remove(silent_eval_contexts.GetCount() - 1) != 0;
}

WGL_CPP::ExprResult
WGL_CPP::ParseBinaryExpression(unsigned prec)
{
    ExprResult e1 = ParseLitExpression();
    if (e1.kind == ExprResult::Error)
        return e1;

    SkipWhitespace();
    ExprResult::Operator op = LexOperator(FALSE);
    for (unsigned p1 = ExprResult::GetPrecedence(op); p1 >= prec && op != ExprResult::None; p1--)
        while (ExprResult::GetPrecedence(op) == p1)
        {
            LexOperator(TRUE);
            SkipWhitespace();
            switch (op)
            {
            case ExprResult::And:
                if (e1.kind == ExprResult::Bool && !e1.bool_result || e1.kind == ExprResult::Int && e1.num_result == 0)
                    PushSilentEvalError();
                break;
            case ExprResult::Or:
                if (e1.kind == ExprResult::Bool && e1.bool_result ||  e1.kind == ExprResult::Int && e1.num_result != 0)
                    PushSilentEvalError();
                break;
            default:
                break;
            }
            ExprResult e2 = ParseBinaryExpression(p1 + 1);
            PopSilentEvalError();
            SkipWhitespace();
            if (e2.kind == ExprResult::Error)
                return e2;

            e1 = EvalExpression(op, e1, e2);
            if (e1.kind == ExprResult::Error)
                return e1;

            op = LexOperator(FALSE);
        }

    return e1;
}

WGL_CPP::ExprResult
WGL_CPP::ParseExpression()
{
    return ParseBinaryExpression(ExprResult::GetPrecedence(ExprResult::Or));
}

const uni_char *
WGL_CPP::LexIdentifier(unsigned &result_length)
{
    const uni_char *id_start = buf_ptr;
    uni_char *id_result = NULL;
    unsigned id_length = 0;
    while (!AtEndOfInput() && wgl_isident(buf_ptr[0]))
    {
        /* If we straddle a segment, it is rare unless repeated macro expansions, build up a copy. */
        if (buf_ptr >= (buf_end - 1))
        {
            unsigned seg_length = static_cast<unsigned>(buf_ptr - id_start + 1);
            uni_char *id_copy = OP_NEWGROA_L(uni_char, id_length + seg_length + 1, context->Arena());
            if (id_result)
                uni_strcpy(id_copy, id_result);
            uni_strncpy(id_copy + id_length, id_start, seg_length);
            id_length += seg_length;
            id_copy[id_length] = 0;

            id_result = id_copy;
            Eat();
            id_start = buf_ptr;
        }
        else
            Eat();
    }
    if (id_result && buf_ptr > id_start)
    {
        unsigned seg_length = static_cast<unsigned>(buf_ptr - id_start + 1);
        uni_char *id_copy = OP_NEWGROA_L(uni_char, id_length + seg_length + 1, context->Arena());
        uni_strcpy(id_copy, id_result);
        uni_strncpy(id_copy + id_length, id_start, seg_length);

        result_length = id_length + seg_length;
        return id_copy;
    }
    else if (id_result)
    {
        result_length = id_length;
        return id_result;
    }
    else
    {
        result_length = static_cast<unsigned>(buf_ptr - id_start);
        return id_start;
    }
}

unsigned
WGL_LexBuffer::LexNumLiteral()
{
    BOOL is_hex = FALSE;
    unsigned value = 0;
    if (buf_ptr[0] == '0')
    {
        Eat();
        if (buf_ptr[0] == 'x' || buf_ptr[0] == 'X')
        {
            Eat();
            is_hex = TRUE;
        }
    }

    while (!AtEndOfInput())
    {
        if (!is_hex && wgl_isdigit(buf_ptr[0]))
        {
            unsigned next = value * 10 + static_cast<unsigned>(buf_ptr[0] - '0');
            if (next < value)
                return 0; // overflow
            else
                value = next;
            Eat();
        }
        else if (is_hex && wgl_isxdigit(buf_ptr[0]))
        {
            unsigned dig = 0;
            if (wgl_isdigit(buf_ptr[0]))
                dig = static_cast<unsigned>(buf_ptr[0] - '0');
            else if (buf_ptr[0] >= 'A' && buf_ptr[0] <= 'F')
                dig = static_cast<unsigned>(buf_ptr[0] - 'A');
            else
                dig = static_cast<unsigned>(buf_ptr[0] - 'a');

            unsigned next = value * 16 + dig;
            if (next < value)
                return 0; // overflow
            else
                value = next;

            Eat();
        }
        else
        {
            if (*buf_ptr == 'u' || *buf_ptr == 'U')
                buf_ptr++;
            return value;
        }

        FillBufferIfNeeded();
    }

    return 0;
}

WGL_CPP::ExprResult
WGL_CPP::ParseLitExpression()
{
again:
    SkipWhitespace();
    if (buf_ptr[0] == '/' && buf_ptr[1] == '/')
    {
        SkipUntilNextLine(TRUE);
        goto again;
    }
    else if (buf_ptr[0] == '/' && buf_ptr[1] == '*')
    {
        if (!SkipComment())
            return ExprResult(ExprResult::ERROR_TAG);
        goto again;
    }
    else if (buf_ptr[0] == '(')
    {
        const uni_char *expr_start = buf_ptr;
        Eat();
        ExprResult result = ParseExpression();
        SkipWhitespace();
        if (buf_ptr[0] == ')')
        {
            Eat();
            return result;
        }
        else
        {
            AddError(WGL_Error::CPP_SYNTAX_ERROR, expr_start, WGL_LINES_ERROR_CONTEXT, TRUE);
            return ExprResult(ExprResult::ERROR_TAG);
        }
    }
    else if (buf_ptr[0] == '-' || buf_ptr[0] == '+' || buf_ptr[0] == '~' || buf_ptr[0] == '!')
    {
        uni_char op = buf_ptr[0];
        Eat();
        ExprResult result = ParseExpression();
        switch (op)
        {
        case '-':
            switch (result.kind)
            {
            case ExprResult::Int:
                result.num_result = -result.num_result;
                return result;
            case ExprResult::Bool:
                result.bool_result = !result.bool_result;
                return result;
            default:
                return result;
            }
        case '+':
            return result;
        case '~':
            switch (result.kind)
            {
            case ExprResult::Int:
                result.num_result = ~result.num_result;
                return result;
            case ExprResult::Bool:
                result.bool_result = !result.bool_result;
                return result;
            default:
                return result;
            }
        case '!':
            switch (result.kind)
            {
            case ExprResult::Int:
                result.num_result = !result.num_result;
                return result;
            case ExprResult::Bool:
                result.bool_result = !result.bool_result;
                return result;
            default:
                return result;
            }
        }
    }
    else if (MATCH7(0, 'd','e','f','i','n','e','d') && KWDEND(7))
    {
        Eat(7);
        SkipWhitespace();
        BOOL with_paren = FALSE;
        if (buf_ptr[0] == '(')
        {
            Eat();
            with_paren = TRUE;
            SkipWhitespace();
        }
        unsigned id_length = 0;
        const uni_char *id_start = LexIdentifier(id_length);
        BOOL is_defined = FALSE;
        if (id_length > 0)
            is_defined = IsTrue(TRUE, id_start, id_length);
        if (with_paren)
        {
            SkipWhitespace();
            if (buf_ptr[0] != ')')
            {
                AddError(WGL_Error::CPP_SYNTAX_ERROR, id_start, id_length, FALSE);
                return ExprResult(ExprResult::ERROR_TAG);
            }
            else
                Eat();
        }
        return ExprResult(is_defined, FALSE);
    }
    else if (wgl_isdigit(buf_ptr[0]))
        return ExprResult(static_cast<int>(LexNumLiteral()));
    else if (wgl_isident(buf_ptr[0]))
    {
        unsigned id_length = 0;
        const uni_char *id_begin_buf = BufferPosition();
        const uni_char *id_begin = GetBufferPosition();
        const uni_char *id_start = LexIdentifier(id_length);
        DefineData *data = NULL;
        const uni_char *expansion = FindExpansion(id_start, id_length, &data);

        if (!expansion)
        {
            if (is_silent)
                return ExprResult(0);
            else
            {
                AddError(WGL_Error::CPP_UNKNOWN_ID, id_start, id_length, FALSE);
                return ExprResult(ExprResult::ERROR_TAG);
            }
        }
        else
        {
            OpVector<WGL_String> names_expanded;
            WGL_String *name = WGL_String::Make(context, id_start, id_length);
            LEAVE_IF_ERROR(names_expanded.Add(name));

            /* Limit the segment where the expanded identifier started */
            for (unsigned i = 0; i < input_segments_used; i++)
                if (input_segments[i].Within(id_begin))
                {
                    SplitBufferSegment(i, id_begin_buf, id_length);
                    break;
                }

            expansion = HandleMacroExpansion(name, data, &names_expanded);
            names_expanded.Clear();
            if (expansion)
            {
                if (in_smallbuffer)
                    if (WithinSmallBuffer(id_begin))
                        smallbuffer_start = const_cast<uni_char* >(id_begin);
                    else
                        smallbuffer_start = smallbuffer;

                InsertBuffer(expansion, static_cast<unsigned>(uni_strlen(expansion)));
                return ParseLitExpression();
            }
        }
    }
    else
    {
        AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);
        SkipUntilNextLine(TRUE);
    }
    return ExprResult(ExprResult::ERROR_TAG);
}

void
WGL_CPP::HandleIf()
{
    ExprResult result = ParseExpression();
    if (result.kind == ExprResult::Error)
        return;

    cpp_contexts.Add(cpp_inactive_nesting);
    int i = ExprResult::ToInt(result);
    cpp_inactive_nesting = (i == 0) ? 1 : 0;
    if (!AtEndOfInput() && buf_ptr[0] == '#')
        is_pound_recognised = TRUE;
}

void
WGL_CPP::HandleIfdef(BOOL is_def)
{
    SkipWhitespace();

    if (!wgl_isident0(buf_ptr[0]))
    {
        AddError(WGL_Error::CPP_SYNTAX_ERROR, buf_ptr, WGL_LINES_ERROR_CONTEXT, TRUE);
        SkipUntilNextLine(TRUE);
    }
    else
    {
        unsigned id_length = 0;
        const uni_char *id_start = LexIdentifier(id_length);

        BOOL not_is_defined = !IsTrue(is_def, id_start, id_length);
        SkipUntilNextLine(TRUE);
        cpp_contexts.Add(cpp_inactive_nesting);
        cpp_inactive_nesting = not_is_defined ? 1 : 0;
    }
}

void
WGL_LexBuffer::SkipUntilNextLine(BOOL skip_whitespace)
{
    while (!AtEndOfInput() && buf_ptr[0] != '\n')
        Eat();

    if (skip_whitespace)
    {
        /* Optionally, skip whitespace on next line (and check if next char is a '#') */
        SkipWhitespace();
        if (!AtEndOfInput() && buf_ptr[0] == '#')
            is_pound_recognised = TRUE;
    }
    else if (!AtEndOfInput())
        Eat();

}

void
WGL_LexBuffer::AdvanceUntilNextLine(BOOL skip_whitespace)
{
    while (!AtEndOfInput() && buf_ptr[0] != '\n')
    {
        *buf_ptr = ' ';
        Eat();
    }

    if (skip_whitespace)
    {
        /* Optionally, skip whitespace on next line (and check if next char is a '#') */
        SkipWhitespace();
        if (!AtEndOfInput() && buf_ptr[0] == '#')
            is_pound_recognised = TRUE;
    }
    else if (!AtEndOfInput())
        Eat();
}

void
WGL_LexBuffer::SkipWhitespace(BOOL include_new_line)
{
    while (!AtEndOfInput() && include_new_line ? wgl_isspace(buf_ptr[0]) : wgl_isspace_no_nl(buf_ptr[0]))
    {
        if (buf_ptr[0] == '\r')
        {
            if (buf_ptr[1] != '\n')
                line_number++;
        }
        else if (buf_ptr[0] == '\n')
            line_number++;

        Eat();
    }
}

void
WGL_CPP::Advance(unsigned count)
{
    while (!AtEndOfInput() && count > 0)
    {
        if (cpp_inactive_nesting > 0)
            *buf_ptr = ' ';
        count--;
        Eat();
    }
}

void
WGL_CPP::CPP()
{
    BOOL in_quotes = FALSE;
    BOOL in_multi_comment = FALSE;
    BOOL in_line_comment = FALSE;

    SkipWhitespace();
    if (!AtEndOfInput() && buf_ptr[0] == '#')
        is_pound_recognised = TRUE;

    while (!AtEndOfInput())
    {
        if (in_line_comment)
        {
            SkipUntilNextLine(TRUE);
            in_line_comment = FALSE;
        }
        else if (in_multi_comment)
        {
            while (TRUE)
            {
                while (!AtEndOfInput() && buf_ptr[0] != '*')
                    Eat();

                if (AtEndOfInput())
                    break;
                Eat();
                if (buf_ptr[0] == '/')
                {
                    Eat();
                    in_multi_comment = FALSE;
                    break;
                }
            }
        }
        else if (in_quotes)
        {
            while (!AtEndOfInput())
            {
                if (buf_ptr[0] == '\\')
                {
                    Eat();
                    if (AtEndOfInput())
                        break;

                    if (buf_ptr[0] == '"')
                        Eat();
                }
                else if (buf_ptr[0] == '"')
                {
                    Eat();
                    in_quotes = FALSE;
                    break;
                }
                else
                    Eat();
            }
        }
        else
        {
            if (cpp_inactive_nesting > 0)
            {
                switch (buf_ptr[0])
                {
                case '\n':
                {
                    SkipWhitespace();
                    FillBufferIfNeeded();
                    if (!AtEndOfInput() && buf_ptr[0] == '#')
                        is_pound_recognised = TRUE;
                    break;
                }
                case '#':
                    if (is_pound_recognised)
                    {
                        *buf_ptr++ = ' ';
                        while (!AtEndOfInput() && buf_ptr[0] == ' ')
                            Eat();
                        if (buf_end - buf_ptr < 4)
                            FillBuffer();
                        if (buf_end - buf_ptr < 4)
                            break;
                        if (MATCH4(0,'e','l','s','e'))
                        {
                            Advance(4);
                            if (cpp_inactive_nesting == 1)
                            {
                                cpp_inactive_nesting--;
                                SkipUntilNextLine(TRUE);
                            }
                        }
                        else if (MATCH2(0,'i','f') && wgl_isspace_no_nl(buf_ptr[2]))
                        {
                            cpp_inactive_nesting++;
                            AdvanceUntilNextLine(TRUE);
                        }
                        else if (MATCH5(0,'i','f','d','e','f'))
                        {
                            Advance(5);
                            cpp_inactive_nesting++;
                        }
                        else if (MATCH6(0,'i','f','n','d','e','f'))
                        {
                            cpp_inactive_nesting++;
                            AdvanceUntilNextLine(TRUE);
                        }
                        else if (MATCH4(0,'e','n','d','i'))
                        {
                            Advance(4);
                            if (cpp_inactive_nesting > 1 && buf_ptr[0] == 'f')
                                cpp_inactive_nesting--;
                            else
                            {
                                FillBufferIfNeeded();
                                if (!AtEndOfInput() && buf_ptr[0] == 'f')
                                    Advance();

                                unsigned is_skipping = 0;
                                if (cpp_contexts.GetCount() > 0)
                                {
                                    is_skipping = cpp_contexts.Get(cpp_contexts.GetCount() - 1);
                                    cpp_contexts.Remove(cpp_contexts.GetCount() - 1);
                                }
                                cpp_inactive_nesting = is_skipping;
                                SkipUntilNextLine(TRUE);
                            }
                        }
                    }
                    else
                        Advance();
                    break;
                default:
                    Advance();
                    }
            }
            else
            {
                switch (buf_ptr[0])
                {
                case '/':
                    Advance();
                    FillBufferIfNeeded();
                    if (AtEndOfInput())
                        break;
                    if (buf_ptr[0] == '/')
                        in_line_comment = TRUE;
                    break;
                case '\\':
                    Advance();
                    FillBufferIfNeeded();
                    if (AtEndOfInput())
                        break;
                    if (buf_ptr[0] == '"')
                        Advance();
                    break;
                case '"':
                    Advance();
                    in_quotes = !in_quotes;
                    break;
                case '\n':
                {
                    SkipWhitespace();
                    FillBufferIfNeeded();
                    if (!AtEndOfInput() && buf_ptr[0] == '#')
                        is_pound_recognised = TRUE;
                    else
                        break;
                }
                case '#':
                    if (is_pound_recognised)
                    {
                        uni_char *cpp_start = buf_ptr;
                        is_pound_recognised = FALSE;
                        Advance();
                        FillBufferIfNeeded();
                        SkipWhitespace(FALSE);
                        if (buf_end - buf_ptr < 7)
                        {
                            FillBuffer();
                            if (buf_end - buf_ptr < 6)
                                break;
                        }
                        if (MATCH2(0,'i','f') && KWDEND(2))
                        {
                            Advance(2);
                            HandleIf();
                        }
                        else if (MATCH5(0,'i','f','d','e','f') && KWDEND(5))
                        {
                            Advance(5);
                            HandleIfdef(TRUE);
                            if (cpp_inactive_nesting)
                                cpp_start[0] = cpp_start[1] = '/';
                        }
                        else if (MATCH6(0,'i','f','n','d','e','f') && KWDEND(6))
                        {
                            Advance(6);
                            FillBufferIfNeeded();
                            if (!AtEndOfInput() && buf_ptr[0] == ' ')
                                HandleIfdef(FALSE);
                        }
                        else if (MATCH6(0,'d','e','f','i','n','e') && KWDEND(6))
                        {
                            Advance(6);
                            FillBufferIfNeeded();
                            if (!AtEndOfInput() && wgl_isspace_no_nl(buf_ptr[0]))
                            {
                                buf_ptr++;
                                HandleDefine();
                            }
                        }
                        else if (MATCH5(0, 'e','r','r','o','r') && KWDEND(5))
                        {
                            Advance(5);
                            FillBufferIfNeeded();
                            SkipWhitespace();
                            const uni_char *error_start = buf_ptr;
                            SkipUntilNextLine(FALSE);
                            /* Leave out line terminating characters from error fragment. */
                            unsigned error_len = static_cast<unsigned>(buf_ptr - error_start);
                            while (error_len > 0)
                            {
                                if (error_start[error_len - 1] != '\r' && error_start[error_len - 1] != '\n')
                                    break;

                                error_len--;
                            }
                            AddError(WGL_Error::CPP_ERROR_ERROR, error_start, error_len, FALSE);
                        }
                        else if (MATCH5(0, 'u','n','d','e','f') && KWDEND(5))
                        {
                            Advance(5);
                            FillBufferIfNeeded();
                            if (!AtEndOfInput() && wgl_isspace_no_nl(buf_ptr[0]))
                            {
                                buf_ptr++;
                                HandleUndef();
                            }
                        }
                        else if (MATCH7(0, 'v', 'e', 'r', 's', 'i', 'o', 'n') && KWDEND(7))
                        {
                            Advance(7);
                            SkipWhitespace(FALSE);
                            if (!wgl_isdigit(buf_ptr[0]))
                                AddError(WGL_Error::CPP_VERSION_ILLEGAL, cpp_start, WGL_LINES_ERROR_CONTEXT, TRUE);
                            else
                            {
                                unsigned version = LexNumLiteral();
                                if (version != WGL_GLSL_VERSION_NUMBER)
                                    AddError(WGL_Error::CPP_VERSION_UNSUPPORTED, cpp_start, WGL_LINES_ERROR_CONTEXT, TRUE);
                            }
                            if (seen_cpp_directive)
                                AddError(WGL_Error::CPP_VERSION_NOT_FIRST, cpp_start, WGL_LINES_ERROR_CONTEXT, TRUE);

                            SkipUntilNextLine(TRUE);
                        }
                        else if (MATCH6(0, 'p', 'r', 'a', 'g', 'm', 'a') && KWDEND(6))
                            SkipUntilNextLine(TRUE);
                        else if (MATCH9(0, 'e', 'x', 't', 'e', 'n', 's', 'i', 'o', 'n') && KWDEND(9))
                        {
                            Advance(9);
                            SkipWhitespace(FALSE);
                            HandleExtension();
                        }
                        else if (MATCH4(0, 'l', 'i', 'n', 'e') && KWDEND(4))
                        {
                            Advance(4);
                            HandleLinePragma();
                        }
                        else if (MATCH5(0,'e', 'n', 'd', 'i', 'f') && KWDEND(5))
                        {
                            if (cpp_inactive_nesting == 0)
                                cpp_start[0] = cpp_start[1] = '/';
                            Advance(5);
                            FillBufferIfNeeded();
                            SkipUntilNextLine(TRUE);

                            unsigned restored_inactive = 0;
                            if (cpp_contexts.GetCount() > 0)
                            {
                                restored_inactive = cpp_contexts.Get(cpp_contexts.GetCount() - 1);
                                cpp_contexts.Remove(cpp_contexts.GetCount() - 1);
                            }
                            cpp_inactive_nesting = restored_inactive;
                        }
                        else if (MATCH4(0,'e','l','s','e') && KWDEND(4))
                        {
                            Advance(4);
                            FillBufferIfNeeded();
                            SkipUntilNextLine(TRUE);
                            cpp_inactive_nesting = 1;
                        }
                        else
                        {
                            SkipWhitespace(FALSE);
                            if (!AtEndOfInput() && !(buf_ptr[0] == '\r' || buf_ptr[0] == '\n'))
                                AddError(WGL_Error::CPP_SYNTAX_ERROR, cpp_start, WGL_LINES_ERROR_CONTEXT, TRUE);
                        }
                        seen_cpp_directive = TRUE;
                    }
                    else
                        Advance();
                    break;
                default:
                    if (wgl_isident0(buf_ptr[0]))
                    {
                        unsigned id_length = 0;
                        const uni_char *id_begin = buf_ptr;
                        const uni_char *id_begin_buf = BufferPosition();
                        const uni_char *id_start = LexIdentifier(id_length);
                        DefineData *data = NULL;
                        const uni_char *expansion = FindExpansion(id_start, id_length, &data);

                        if (expansion)
                        {
                            OpVector<WGL_String> names_expanded;
                            WGL_String *name = WGL_String::Make(context, id_start, id_length);
                            LEAVE_IF_ERROR(names_expanded.Add(name));

                            /* Mark a segment as ending just before the CPP (macro) name was
                               located. The immediately following CPP name will then be
                               shifted past when walking through the buffer segments, the
                               segment following having the expansion instead.

                               Notice that the 'buffer bound/resolved' position of the name
                               (id_begin_buf) is used, as we are adjusting the buffer segments.
                               [id_begin might be referring to the "virtual" smallbuffer.] */

                            for (unsigned i = 0; i < input_segments_used; i++)
                                if (input_segments[i].Within(id_begin_buf))
                                {
                                    SplitBufferSegment(i, id_begin_buf, id_length);
                                    break;
                                }

                            expansion = HandleMacroExpansion(name, data, &names_expanded);
                            names_expanded.Clear();
                            if (expansion)
                            {
                                if (in_smallbuffer)
                                    if (WithinSmallBuffer(id_begin))
                                        smallbuffer_start = const_cast<uni_char *>(id_begin);
                                    else
                                        smallbuffer_start = smallbuffer;

                                InsertBuffer(expansion, static_cast<unsigned>(uni_strlen(expansion)));
                            }
                        }
                    }
                    else
                        Eat();
                }
                FillBufferIfNeeded();
            }
        }
    }
}

BOOL
WGL_LexBuffer::HaveErrors()
{
    return WGL_Error::HaveErrors(context->GetFirstError());
}

WGL_Error *
WGL_LexBuffer::GetFirstError()
{
    return context->GetFirstError();
}

void
WGL_LexBuffer::ResetL(WGL_ProgramText* program_array, unsigned elements)
{
    OP_ASSERT(elements > 0);

    input_segments = OP_NEWGROA_L(InputSegment, elements, context->Arena());
    for (unsigned i = 0; i < elements; i++)
    {
        unsigned buffer_length = program_array[i].program_text_length;
        uni_char *buffer_copy = OP_NEWGROA_L(uni_char, buffer_length + 1, context->Arena());
        if (buffer_length > 0)
            uni_strncpy(buffer_copy, program_array[i].program_text, buffer_length);
        buffer_copy[buffer_length] = 0;
        input_segments[i].segment_text = input_segments[i].segment_ptr = buffer_copy;
        input_segments[i].segment_length = buffer_length;
    }

    input_segments_allocated = input_segments_used = elements;

    current_input_segment = 0;
    consumed_from_next_segment = 0;
    line_number = 1;
    smallbuffer_length = 0;

    buf_start = input_segments[0].segment_ptr;
    buf_end = buf_start + input_segments[0].segment_length;

    if (buf_start < buf_end && *buf_start == CHAR_BOM)
        buf_start++;

    buf_ptr = const_cast<uni_char *>(buf_start);
    in_smallbuffer = FALSE;
}

void
WGL_LexBuffer::Reset(WGL_LexBuffer *buffer)
{
    input_segments = buffer->input_segments;
    input_segments_allocated = buffer->input_segments_allocated;
    input_segments_used = buffer->input_segments_used;

    /* Reset the segment 'current pointer's; processing them afresh. */
    for (unsigned i = 0; i < input_segments_used; i++)
        input_segments[i].segment_ptr = input_segments[i].segment_text;

    current_input_segment = 0;
    consumed_from_next_segment = 0;
    line_number = 1;
    smallbuffer_length = 0;

    buf_start = input_segments[0].segment_text;
    buf_end = buf_start + input_segments[0].segment_length;

    if (buf_start < buf_end && *buf_start == CHAR_BOM)
        buf_start++;

    buf_ptr = const_cast<uni_char *>(buf_start);
    in_smallbuffer = FALSE;
}

void
WGL_LexBuffer::AddError(WGL_Error::Type err, const uni_char *s, unsigned len, BOOL length_as_lines)
{
    WGL_Error *error = OP_NEWGRO_L(WGL_Error, (context, err, context->GetSourceContext(), line_number), context->Arena());

    error->SetMessageL(s, len, length_as_lines, buf_end);
    context->AddError(error);
}

void
WGL_LexBuffer::AddError(WGL_Error::Type err, const uni_char *s)
{
    WGL_Error *error = OP_NEWGRO_L(WGL_Error, (context, err, context->GetSourceContext(), line_number), context->Arena());
    error->SetMessageL(s);
    context->AddError(error);
}

BOOL
WGL_LexBuffer::InputSegment::Within(const uni_char *p) const
{
    return (p >= segment_text && p < (segment_text + segment_length));
}

void
WGL_Lexer::ConstructL(WGL_ProgramText* program_array, unsigned elements, BOOL for_vertex)
{
    context->ResetL(for_vertex);
    WGL_CPP cpp(context);
    cpp.ResetL(program_array, elements);
    cpp.InitializeL();
    cpp.CPP();

    Reset(&cpp);
    FillBuffer();
    SkipWhitespace();
    if (!AtEndOfInput() && buf_ptr[0] == '#')
        is_pound_recognised = TRUE;
}

void
WGL_LexBuffer::FillBufferIfNeeded()
{
    if (flush_smallbuffer || buf_ptr == buf_end)
        FillBuffer();
}

/* The input is read from one of two buffers:
     - the original input
     - a small temporary buffer
   When FillBuffer is called, it will make input point to
   at least WGL_PARM_LEXBUFFER_GUARANTEED valid/predictable characters,
   which is longer than any lookahead needed by the decision
   tree and longer than almost all identifiers in practice.

   It will prefer to point into the original input but will
   copy input into the temporary buffer when the original
   input crosses a buffer boundary or is very near the end.
   */
void
WGL_LexBuffer::FillBuffer()
{
    OP_ASSERT(buf_ptr <= buf_end);

    if (!flush_smallbuffer && buf_end - buf_ptr >= WGL_PARM_LEXBUFFER_GUARANTEED)
        return;

    if (!in_smallbuffer)
    {
        if (buf_ptr == buf_end)
        {
            if (FinalInputSegment())
            {
                /* EOF, switch to smallbuffer and fill with NULs */
                in_smallbuffer = TRUE;
                op_memset(smallbuffer, 0, sizeof(smallbuffer));
                smallbuffer_length = 0;
                buf_start = smallbuffer;
                buf_ptr = const_cast<uni_char *>(buf_start);
                buf_end = smallbuffer + ARRAY_SIZE(smallbuffer);
            }
            else
                /* Jump to the next input buffer. */
                goto switch_to_next_buffer;
        }
        else
        {
            /* Copy unconsumed input from the current element into smallbuffer,
               then follow it with input from the next element(s) and with NULs
               if the input ends. */
            unsigned nlive = static_cast<unsigned>(buf_end-buf_ptr);
            smallbuffer_length = nlive;
            in_smallbuffer = TRUE;
            unsigned nlive_msize = nlive*sizeof(uni_char);
            op_memset(smallbuffer + nlive, 0, sizeof(smallbuffer) - nlive_msize);
            op_memcpy(smallbuffer, buf_ptr, nlive_msize);
            buf_start = smallbuffer;
            goto fill_smallbuffer_if_possible;
        }
    }
    else
    {
        if (buf_ptr == buf_end)
        {
            if (FinalInputSegment())
            {
                /* Shift unconsumed input in smallbuffer and pad with NULs. */
                unsigned small_consumed = buf_ptr - smallbuffer;
                unsigned nlive = small_consumed > static_cast<unsigned>(smallbuffer_length) ? 0 : smallbuffer_length - small_consumed;

                smallbuffer_length = nlive;

                unsigned nlive_msize = nlive * sizeof(uni_char);
                op_memmove(smallbuffer, buf_ptr, nlive_msize);
                op_memset(smallbuffer + nlive, 0, sizeof(smallbuffer) - nlive_msize);

                buf_ptr = smallbuffer;
            }
            else
                /* Abandon smallbuffer and go to next buffer */
                goto switch_to_next_buffer;
        }
        else
        {
            if (consumed_from_next_segment + (buf_ptr - smallbuffer) >= WGL_PARM_LEXBUFFER_GUARANTEED)
            {
                /* If we were to refill smallbuffer we would end up with input
                   entirely from the next buffer, so move to the next buffer. */
                consumed_from_next_segment -= static_cast<unsigned>(buf_end - buf_ptr);
                goto switch_to_next_buffer;
            }
            else
            {
                /* Shift input in the smallbuffer, then consume more to fill it if possible */
                unsigned small_consumed = static_cast<unsigned>(buf_ptr - smallbuffer);
                unsigned nlive = small_consumed > static_cast<unsigned>(smallbuffer_length) ? 0 : smallbuffer_length - small_consumed;
                smallbuffer_length = nlive;
                unsigned nlive_msize = nlive * sizeof(uni_char);
                op_memmove(smallbuffer, buf_ptr, nlive_msize);
                op_memset(smallbuffer + nlive, 0, sizeof(smallbuffer) - nlive_msize);

                goto fill_smallbuffer_if_possible;
            }
        }
    }

finished:
    OP_ASSERT(buf_end - buf_ptr >= WGL_PARM_LEXBUFFER_GUARANTEED);
    flush_smallbuffer = FALSE;
    return;

fill_smallbuffer_if_possible:
    buf_ptr = smallbuffer;
    buf_end = smallbuffer + ARRAY_SIZE(smallbuffer);

    while (smallbuffer_length < WGL_PARM_LEXBUFFER_GUARANTEED && !FinalInputSegment())
    {
        InputSegment *in_segment = &input_segments[current_input_segment + 1];

        unsigned available = in_segment->Available() - consumed_from_next_segment;
        unsigned space = ARRAY_SIZE(smallbuffer) - smallbuffer_length;
        unsigned tocopy = MIN(available, space);
        op_memcpy(smallbuffer + smallbuffer_length, in_segment->segment_ptr + consumed_from_next_segment, tocopy * sizeof(uni_char));
        consumed_from_next_segment += tocopy;
        smallbuffer_length += tocopy;
        if (consumed_from_next_segment == in_segment->Available())
        {
            AdvanceInputSegment();
            consumed_from_next_segment = 0;
        }
    }
    goto finished;

switch_to_next_buffer:
    AdvanceInputSegment();
    in_smallbuffer = FALSE;
    buf_start = input_segments[current_input_segment].segment_ptr;
    buf_ptr = const_cast<uni_char *>(buf_start);
    buf_end = buf_ptr + input_segments[current_input_segment].Available();
    if (buf_ptr < buf_end)
        buf_ptr += consumed_from_next_segment;
    consumed_from_next_segment = 0;
    if (buf_end - buf_ptr < WGL_PARM_LEXBUFFER_GUARANTEED)
        FillBuffer();
    goto finished;
}

/* It's acceptable if this is slow, it is only ever called when a NUL
   is seen in the input. */
BOOL
WGL_LexBuffer::AtEndOfInput()
{
    if (*buf_ptr != 0 || !FinalInputSegment())
        return FALSE;

    if (!in_smallbuffer)
        return buf_ptr >= buf_end;
    else
        return buf_ptr >= smallbuffer + smallbuffer_length;
}

void
WGL_LexBuffer::ConsumeInput(unsigned n)
{
    while (n > 0)
    {
        unsigned buf_left = static_cast<unsigned>(buf_end - buf_ptr);
        if (n < buf_left)
        {
            buf_ptr += n;
            return;
        }
        else
        {
            n -= buf_left;
            buf_ptr = const_cast<uni_char *>(buf_end);
            FillBuffer();
        }
    }
}

const uni_char *
WGL_LexBuffer::BufferPosition()
{
    if (in_smallbuffer)
    {
        unsigned segment_divide = ConsumedFromCurrent();
        if (static_cast<unsigned>(buf_ptr - smallbuffer) < segment_divide)
            return input_segments[current_input_segment].segment_ptr + (buf_ptr - smallbuffer);
        else
            return input_segments[current_input_segment + 1].segment_text + (buf_ptr - smallbuffer - segment_divide);
    }
    else
        return buf_ptr;
}

unsigned
WGL_LexBuffer::ComputeBufferOffset(const uni_char *position)
{
    /* Common case: 'position' in current segment. */
    if (buf_start <= position && position <= buf_ptr)
        return (buf_ptr - position);

    unsigned count = buf_ptr - buf_start;
    if (in_smallbuffer)
        return count;

    unsigned segment_index = current_input_segment;
    while (segment_index > 0)
    {
        segment_index--;
        if (input_segments[segment_index].segment_text <= position && position <= input_segments[segment_index].segment_ptr)
            return (count + (input_segments[segment_index].segment_ptr - position));

        count += input_segments[segment_index].Used();
    }
    OP_ASSERT(!"Buffer position not in current buffer; cannot happen.");
    return 0;
}

uni_char
WGL_LexBuffer::BufferPeekBehind(unsigned offset)
{
    /* Common case: Intra-segment offset in current. */
    if (static_cast<unsigned>(buf_ptr - buf_start) >= offset)
        return (*(buf_ptr - offset));

    offset -= (buf_ptr - buf_start);
    unsigned segment_index = current_input_segment;
    while (offset > 0 && segment_index > 0)
    {
        segment_index--;
        if (input_segments[segment_index].Used() >= offset)
            return *(input_segments[segment_index].segment_ptr - offset);

        offset -= input_segments[segment_index].Used();
    }
    OP_ASSERT(!"Attempted to offset past buffer start; cannot happen.");
    return 0;
}

void
WGL_LexBuffer::BufferCopy(uni_char *result, const uni_char *start, unsigned length)
{
    if (length == 0)
        return;

    /* Determine starting segment. */
    unsigned segment_index = current_input_segment;

    /* "Flush" current buffer pointer while copying. */
    uni_char *saved_buf_ptr = input_segments[current_input_segment].segment_ptr;
    input_segments[current_input_segment].segment_ptr = buf_ptr;

    while (segment_index > 0)
    {
        if (input_segments[segment_index].segment_text <= start && start <= input_segments[segment_index].segment_ptr)
            break;
        segment_index--;
    }
    OP_ASSERT(input_segments[segment_index].segment_text <= start && start <= input_segments[segment_index].segment_ptr);

    while (length > 0)
    {
        OP_ASSERT(segment_index <= current_input_segment);

        unsigned segment_length = static_cast<unsigned>(input_segments[segment_index].segment_ptr - start);
        if (segment_length >= length)
        {
            op_memcpy(result, start, length * sizeof(uni_char));
            input_segments[current_input_segment].segment_ptr = saved_buf_ptr;
            return;
        }
        else
        {
            op_memcpy(result, start, segment_length * sizeof(uni_char));
            result += segment_length;
            length -= segment_length;

            segment_index++;
            start = input_segments[segment_index].segment_text;
        }
    }
    OP_ASSERT(!"Buffer copy extends past end of buffer; cannot happen.");
    input_segments[current_input_segment].segment_ptr = saved_buf_ptr;
}

#define RETURN2(tok, inc) \
    do {int TO_RETURN = tok; \
        buf_ptr += inc; \
        return TO_RETURN; \
    } while(0)

#define RETURN_TYPE_NAME(tok, inc) \
    do {lexeme.val_ty = tok; \
        buf_ptr += inc; \
        return WGL_Token::TOKEN_PRIM_TYPE_NAME; \
    } while(0)

int
WGL_Lexer::GetToken()
{
    buf_ptr_save = buf_ptr;
start:
    if (buf_end - buf_ptr < MAX_FIXED_TOKEN_LENGTH)
        FillBuffer();

    switch (buf_ptr[0])
    {
    case 0:
        if (AtEndOfInput())
            return WGL_Token::TOKEN_EOF;
        else
            return WGL_Token::TOKEN_ILLEGAL;
    case CHAR_HT:
    case CHAR_VT:
    case CHAR_FF:
    case ' ':
    case CHAR_BOM:
        buf_ptr++;
        goto start;
    case '\r':
        line_number++;
        buf_ptr++;
        if (*buf_ptr == '\n')
            buf_ptr++;
        if (*buf_ptr == '#')
            is_pound_recognised = TRUE;
        goto start;
    case '\n':
        line_number++;
        buf_ptr++;
        if (*buf_ptr == '#')
            is_pound_recognised = TRUE;
        goto start;
    case '#':
        if (is_pound_recognised)
        {
            Eat();
            SkipWhitespace(FALSE);
            if (MATCH4(0,'l','i','n','e') && wgl_isspace_no_nl(buf_ptr[4]))
            {
                Eat(5);
                HandleLinePragma();
                is_pound_recognised = FALSE;
                goto start;
            }
        }
        SkipUntilNextLine(TRUE);
        goto start;
    case ',':
        RETURN2(WGL_Token::TOKEN_COMMA, 1);
    case '?':
        RETURN2(WGL_Token::TOKEN_COND, 1);
    case ';':
        RETURN2(WGL_Token::TOKEN_SEMI, 1);
    case '~':
        RETURN2(WGL_Token::TOKEN_COMPLEMENT, 1);
    case '(':
        RETURN2(WGL_Token::TOKEN_LPAREN, 1);
    case ')':
        RETURN2(WGL_Token::TOKEN_RPAREN, 1);
    case '[':
        RETURN2(WGL_Token::TOKEN_LBRACKET, 1);
    case ']':
        RETURN2(WGL_Token::TOKEN_RBRACKET, 1);
    case '{':
        RETURN2(WGL_Token::TOKEN_LBRACE, 1);
    case '}':
        RETURN2(WGL_Token::TOKEN_RBRACE, 1);
    case ':':
        RETURN2(WGL_Token::TOKEN_COLON, 1);
    case '.':
        if (wgl_isdigit(buf_ptr[1]))
            return LexDecimalNumber();
        else
            RETURN2(WGL_Token::TOKEN_DOT, 1);
    case '!':
        if (buf_ptr[1] == '=')
            RETURN2(WGL_Token::TOKEN_NEQ, 2);
        else
            RETURN2(WGL_Token::TOKEN_NOT, 1);
    case '+':
        switch (buf_ptr[1])
        {
        case '=': RETURN2(WGL_Token::TOKEN_ADD_ASSIGN, 2);
        case '+': RETURN2(WGL_Token::TOKEN_INC, 2);
        default: RETURN2(WGL_Token::TOKEN_ADD, 1);
        }
    case '-':
        switch (buf_ptr[1])
        {
        case '=': RETURN2(WGL_Token::TOKEN_SUB_ASSIGN, 2);
        case '-': RETURN2(WGL_Token::TOKEN_DEC, 2);
        default: RETURN2(WGL_Token::TOKEN_SUB, 1);
        }
    case '*':
        if (buf_ptr[1] == '=')
            RETURN2(WGL_Token::TOKEN_MUL_ASSIGN, 2);
        else
            RETURN2(WGL_Token::TOKEN_MUL, 1);
#ifdef WGL_SUPPORT_LINE_CONTINUATION_CHARACTERS
    case '\\':
        if (buf_ptr[1] == '\n')
        {
            line_number++;
            buf_ptr += 2;
            goto start;
        }
        else if (buf_ptr[1] == '\r')
        {
            line_number++;
            buf_ptr += 2;
            if (*buf_ptr == '\n')
                buf_ptr++;
            goto start;
        }
        break;
#endif // WGL_SUPPORT_LINE_CONTINUATION_CHARACTERS
    case '/':
        switch (buf_ptr[1])
        {
        case '=': RETURN2( WGL_Token::TOKEN_DIV_ASSIGN, 2 );
        case '*':
        case '/':
        handle_comment:
            switch(WGL_Lexer::SkipLine())
            {
            case (-1):
                return WGL_Token::TOKEN_ILLEGAL;
            case '\n':
            default:
                buf_ptr_save = buf_ptr;
                goto start;
            }
        default: RETURN2( WGL_Token::TOKEN_DIV, 1 );
        }
    case '%':
        if (buf_ptr[1] == '=')
            RETURN2( WGL_Token::TOKEN_MOD_ASSIGN, 2 );
        else
            RETURN2( WGL_Token::TOKEN_MOD, 1);
    case '&':
        switch(buf_ptr[1])
        {
        case '=': RETURN2( WGL_Token::TOKEN_AND_ASSIGN, 2 );
        case '&': RETURN2( WGL_Token::TOKEN_AND, 2 );
        default: RETURN2( WGL_Token::TOKEN_BITAND, 1 );
        }
    case '|':
        switch(buf_ptr[1])
        {
        case '=': RETURN2( WGL_Token::TOKEN_OR_ASSIGN, 2 );
        case '|': RETURN2( WGL_Token::TOKEN_OR, 2 );
        default: RETURN2( WGL_Token::TOKEN_BITOR, 1 );
        }
    case '^':
        switch(buf_ptr[1])
        {
        case '=': RETURN2(WGL_Token::TOKEN_XOR_ASSIGN, 2);
        case '^': RETURN2(WGL_Token::TOKEN_XOR, 2);
        default: RETURN2(WGL_Token::TOKEN_BITXOR, 1);
        }
    case '=':
        if (buf_ptr[1] != '=')
            RETURN2( WGL_Token::TOKEN_EQUAL, 1 );
        else
            RETURN2( WGL_Token::TOKEN_EQ, 2 );
    case '>':
        switch (buf_ptr[1])
        {
        case '>':
            if (buf_ptr[2] == '=')
                RETURN2( WGL_Token::TOKEN_RIGHT_ASSIGN, 3 );
            else
                RETURN2( WGL_Token::TOKEN_SHIFTR, 2 );
        case '=': RETURN2( WGL_Token::TOKEN_GE, 2 );
        default: RETURN2( WGL_Token::TOKEN_GT, 1 );
        }
    case '<':
        switch (buf_ptr[1])
        {
        case '!':
        /* Extension:  HTML <!-- comment initial, which we treat like a // comment.
           Note that <!-- is a valid but unlikely token stream: "x<!--y".  */
            if (buf_ptr[2] == '-' && buf_ptr[3] == '-')
                goto handle_comment;
            else
                RETURN2( WGL_Token::TOKEN_LT, 1 );
        case '<':
            if (buf_ptr[2] == '=')
                RETURN2( WGL_Token::TOKEN_LEFT_ASSIGN, 3 );
            else
                RETURN2( WGL_Token::TOKEN_SHIFTL, 2 );
        case '=': RETURN2( WGL_Token::TOKEN_LE, 2 );
        default: RETURN2( WGL_Token::TOKEN_LT, 1 );
        }
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        if (buf_ptr[0] == '0' && (buf_ptr[1] == 'x' || buf_ptr[1] == 'X'))
            return LexHexNumber();
        else
            return LexDecimalNumber();
    case 'a':
        if (MATCH8(1,'t','t','r','i','b','u','t','e') && KWDEND(9))
            RETURN2(WGL_Token::TOKEN_ATTRIBUTE, 9);
        break;
    case 'b':
        if (MATCH4(1,'r','e','a','k') && KWDEND(5))
            RETURN2(WGL_Token::TOKEN_BREAK, 5);
        else if (MATCH3(1,'o','o','l') && KWDEND(4))
            RETURN2(WGL_Token::TOKEN_BOOL, 4);
        else if (MATCH3(1,'v','e','c'))
            switch(buf_ptr[4])
            {
            case '2':
            case '3':
            case '4':
                if (KWDEND(5))
                    RETURN_TYPE_NAME(buf_ptr[4] == '2' ? TYPE_BVEC2 : (buf_ptr[4] == '3' ? TYPE_BVEC3 : TYPE_BVEC4), 5);
                else
                    break;
            default:
                break;
            }
        else
            break;
    case 'c':
        switch (buf_ptr[1])
        {
        case 'a':
            switch (buf_ptr[2])
            {
            case 's':
                if (buf_ptr[3] == 'e' && KWDEND(4))
                    RETURN2(WGL_Token::TOKEN_CASE, 4);
                else
                    break;
            default:
                break;
            }
            break;
        case 'o':
            if (buf_ptr[2] == 'n')
            {
                switch (buf_ptr[3])
                {
                case 's':
                    if (buf_ptr[4] == 't' && KWDEND(5))
                        RETURN2(WGL_Token::TOKEN_CONST, 5);
                    else
                        break;
                case 't':
                    if (MATCH4(4,'i','n','u','e') && KWDEND(8))
                        RETURN2(WGL_Token::TOKEN_CONTINUE, 8);
                    else
                        break;
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
        break;
    case 'd':
        switch (buf_ptr[1])
        {
        case 'e':
            switch (buf_ptr[2])
            {
            case 'f':
                if (MATCH4(3,'a','u','l','t') && KWDEND(7))
                    RETURN2(WGL_Token::TOKEN_DEFAULT, 7);
                else
                    break;
            default:
                break;
            }
            break;
        case 'i':
            if (MATCH5(2,'s','c','a','r','d') && KWDEND(7))
                RETURN2(WGL_Token::TOKEN_DISCARD, 7);
            break;
        case 'o':
            if (KWDEND(2))
                RETURN2(WGL_Token::TOKEN_DO, 2);
            break;

        default:
            break;
        }
        break;
    case 'e':
        if (MATCH3(1,'l','s','e') && KWDEND(4))
            RETURN2(WGL_Token::TOKEN_ELSE, 4);
        break;
    case 'f':
        switch (buf_ptr[1])
        {
        case 'a':
            if (MATCH3(2,'l','s','e') && KWDEND(5))
                RETURN2(WGL_Token::TOKEN_CONST_FALSE, 5);
            break;
        case 'l':
            if (MATCH3(2,'o','a','t') && KWDEND(5))
                RETURN2(WGL_Token::TOKEN_FLOAT, 5);
            break;
        case 'o':
            if (buf_ptr[2] == 'r' && KWDEND(3))
                RETURN2(WGL_Token::TOKEN_FOR, 3);
            break;
        default:
            break;
        }
        break;
    case 'h':
        if (MATCH4(1,'i','g','h','p') && KWDEND(5))
            RETURN2(WGL_Token::TOKEN_HIGH_PREC, 5);
        break;
    case 'i':
        switch (buf_ptr[1])
        {
        case 'f':
            if (KWDEND(2))
                RETURN2( WGL_Token::TOKEN_IF, 2 );
            break;
        case 'n':
            if (MATCH7(2,'v','a','r','i','a','n','t') && KWDEND(9))
                RETURN2(WGL_Token::TOKEN_INVARIANT, 9);
            else if (MATCH3(2,'o','u','t') && KWDEND(5))
                RETURN2(WGL_Token::TOKEN_INOUT, 5);
            else if (buf_ptr[2] == 't' && KWDEND(3))
                RETURN2(WGL_Token::TOKEN_INT, 3);
            else if (KWDEND(2))
                RETURN2(WGL_Token::TOKEN_IN_, 2);
            break;
        case 's':
            if (MATCH6(2,'a','m','p','l','e','r'))
                if (MATCH7(8,'1','D','A','r','r','a','y') && KWDEND(15))
                    RETURN_TYPE_NAME(TYPE_ISAMPLER1DARRAY, 15);
                else if (MATCH7(8,'2','D','A','r','r','a','y') && KWDEND(15))
                    RETURN_TYPE_NAME(TYPE_ISAMPLER2DARRAY, 15);
                else if (MATCH6(8,'2','D','R','e','c','t') && KWDEND(14))
                    RETURN_TYPE_NAME(TYPE_ISAMPLER2DRECT, 14);
                else if (MATCH4(8,'C','u','b','e') && KWDEND(12))
                    RETURN_TYPE_NAME(TYPE_ISAMPLERCUBE, 12);
                else if (MATCH6(8,'B','u','f','f','e','r') && KWDEND(14))
                    RETURN_TYPE_NAME(TYPE_ISAMPLERBUFFER, 14);
                else if (MATCH2(8,'1','D') && KWDEND(10))
                    RETURN_TYPE_NAME(TYPE_ISAMPLER1D, 10);
                else if (MATCH9(8,'2','D','M','S','A','r','r','a','y') && KWDEND(17))
                    RETURN_TYPE_NAME(TYPE_ISAMPLER2DMSARRAY, 17);
                else if (MATCH4(8,'2','D','M','S') && KWDEND(12))
                    RETURN_TYPE_NAME(TYPE_ISAMPLER2DMS, 12);
                else if (MATCH2(8,'2','D') && KWDEND(10))
                    RETURN_TYPE_NAME(TYPE_ISAMPLER2D, 10);
                else if (MATCH2(8,'3','D') && KWDEND(10))
                    RETURN_TYPE_NAME(TYPE_ISAMPLER3D, 10);
                else
                    break;
            else
                break;
        case 'v':
            if (MATCH2(2,'e','c'))
                switch(buf_ptr[4])
                {
                case '2':
                    if (KWDEND(5))
                        RETURN_TYPE_NAME(TYPE_IVEC2, 5);
                    break;
                case '3':
                    if (KWDEND(5))
                        RETURN_TYPE_NAME(TYPE_IVEC3, 5);
                    break;
                case '4':
                    if (KWDEND(5))
                        RETURN_TYPE_NAME(TYPE_IVEC4, 5);
                    break;
                default:
                    break;
                }
            else
                break;
        default:
            break;
        }
        break;
    case 'l':
        switch(buf_ptr[1])
        {
        case 'a':
            if (MATCH4(2,'y','o','u','t') && KWDEND(6))
                RETURN2(WGL_Token::TOKEN_LAYOUT, 6);
            break;
        case 'o':
            if (MATCH2(2,'w','p') && KWDEND(4))
                RETURN2(WGL_Token::TOKEN_LOW_PREC, 4);
            break;
        default:
            break;
        }
        break;
    case 'm':
        if (MATCH2(1,'a','t'))
            switch(buf_ptr[3])
            {
            case '2':
                if (buf_ptr[4] == 'x')
                    switch(buf_ptr[5])
                    {
                    case '2':
                        if (KWDEND(6))
                            RETURN_TYPE_NAME(TYPE_MAT2X2, 6);
                        break;
                    case '3':
                        if (KWDEND(6))
                            RETURN_TYPE_NAME(TYPE_MAT2X3, 6);
                        break;
                    case '4':
                        if (KWDEND(6))
                            RETURN_TYPE_NAME(TYPE_MAT2X4, 6);
                        break;
                    default:
                        if (KWDEND(4))
                            RETURN_TYPE_NAME(TYPE_MAT2, 4);
                        break;
                    }
                else
                    if (KWDEND(4))
                        RETURN_TYPE_NAME(TYPE_MAT2, 4);
                    else
                        break;
            case '3':
                if (buf_ptr[4] == 'x')
                    switch(buf_ptr[5])
                    {
                    case '2':
                        if (KWDEND(6))
                            RETURN_TYPE_NAME(TYPE_MAT3X2, 6);
                        break;
                    case '3':
                        if (KWDEND(6))
                            RETURN_TYPE_NAME(TYPE_MAT3X3, 6);
                        break;
                    case '4':
                        if (KWDEND(6))
                            RETURN_TYPE_NAME(TYPE_MAT3X4, 6);
                        break;
                    default:
                        if (KWDEND(4))
                            RETURN_TYPE_NAME(TYPE_MAT3, 4);
                        break;
                    }
                else
                    if (KWDEND(4))
                        RETURN_TYPE_NAME(TYPE_MAT3, 4);
                    else
                        break;
                break;
            case '4':
                if (buf_ptr[4] == 'x')
                    switch(buf_ptr[5])
                    {
                    case '2':
                        if (KWDEND(6))
                            RETURN_TYPE_NAME(TYPE_MAT4X2, 6);
                        break;
                    case '3':
                        if (KWDEND(6))
                            RETURN_TYPE_NAME(TYPE_MAT4X3, 6);
                        break;
                    case '4':
                        if (KWDEND(6))
                            RETURN_TYPE_NAME(TYPE_MAT4X4, 6);
                        break;
                    default:
                        if (KWDEND(4))
                            RETURN_TYPE_NAME(TYPE_MAT4, 4);
                        break;
                    }
                else
                    if (KWDEND(4))
                        RETURN_TYPE_NAME(TYPE_MAT4, 4);
                    else
                        break;
            default:
                break;
            }
        else if (MATCH6(1,'e','d','i','u','m','p') && KWDEND(7))
            RETURN2(WGL_Token::TOKEN_MED_PREC, 7);

        break;
    case 'o':
        if (MATCH2(1,'u','t') && KWDEND(3))
            RETURN2(WGL_Token::TOKEN_OUT_, 3);
        break;
    case 'p':
        if (MATCH8(1,'r','e','c','i','s','i','o','n') && KWDEND(9))
            RETURN2(WGL_Token::TOKEN_PRECISION, 9);
        break;
    case 'r':
        if (MATCH5(1,'e','t','u','r','n') && KWDEND(6))
            RETURN2(WGL_Token::TOKEN_RETURN, 6);
        break;
    case 's':
        if (MATCH5(1,'w','i','t','c','h') && KWDEND(6))
            RETURN2(WGL_Token::TOKEN_SWITCH, 6);
        else if (MATCH5(1,'t','r','u','c','t') && KWDEND(6))
            RETURN2(WGL_Token::TOKEN_STRUCT, 6);
        else if (MATCH6(1,'a','m','p','l','e','r'))
            switch(buf_ptr[7])
            {
            case '1':
                if (buf_ptr[8] == 'D')
                    if (MATCH6(9,'S','h','a','d','o','w') && KWDEND(15))
                        RETURN_TYPE_NAME(TYPE_SAMPLER1DSHADOW, 15);
                    else if (MATCH5(9,'A','r','r','a','y'))
                        if (MATCH6(14,'S','h','a','d','o','w') && KWDEND(20))
                            RETURN_TYPE_NAME(TYPE_SAMPLER1DARRAYSHADOW, 20);
                        else if (KWDEND(14))
                            RETURN_TYPE_NAME(TYPE_SAMPLER1DARRAY, 14);
                        else
                            break;
                    else if (KWDEND(9))
                        RETURN_TYPE_NAME(TYPE_SAMPLER1D, 9);
                    else
                        break;
                else
                    break;
            case '2':
                if (buf_ptr[8] == 'D')
                {
                    if (MATCH6(9,'S','h','a','d','o','w') && KWDEND(15))
                        RETURN_TYPE_NAME(TYPE_SAMPLER2DSHADOW, 15);
                    if (MATCH10(9,'R','e','c','t','S','h','a','d','o','w') && KWDEND(19))
                        RETURN_TYPE_NAME(TYPE_SAMPLER2DRECTSHADOW, 19);
                    if (MATCH4(9,'R','e','c','t') && KWDEND(13))
                        RETURN_TYPE_NAME(TYPE_SAMPLER2DRECT, 13);
                    if (MATCH7(9,'M','S','A','r','r','a','y') && KWDEND(16))
                        RETURN_TYPE_NAME(TYPE_SAMPLER2DMSARRAY, 16);
                    if (MATCH2(9,'M','S') && KWDEND(11))
                        RETURN_TYPE_NAME(TYPE_SAMPLER2DMS, 11);
                    else if (MATCH5(9,'A','r','r','a','y'))
                    {
                        if (MATCH6(14,'S','h','a','d','o','w') && KWDEND(20))
                            RETURN_TYPE_NAME(TYPE_SAMPLER2DARRAYSHADOW, 20);
                        else if (KWDEND(14))
                            RETURN_TYPE_NAME(TYPE_SAMPLER2DARRAY, 14);
                    }
                    else if (KWDEND(9))
                        RETURN_TYPE_NAME(TYPE_SAMPLER2D, 9);
                }
                break;
            case '3':
                if (buf_ptr[8] == 'D' && KWDEND(9))
                    RETURN_TYPE_NAME(TYPE_SAMPLER3D, 9);
                break;
            case 'C':
                if (MATCH3(8,'u','b','e'))
                {
                    if (MATCH6(11,'S','h','a','d','o','w') && KWDEND(17))
                        RETURN_TYPE_NAME(TYPE_SAMPLERCUBESHADOW, 17);
                    else if (KWDEND(11))
                        RETURN_TYPE_NAME(TYPE_SAMPLERCUBE, 11);
                }
                break;
            case 'B':
                if (MATCH5(8,'u','f','f','e','r') && KWDEND(13))
                    RETURN_TYPE_NAME(TYPE_SAMPLERBUFFER, 13);
                break;
            default:
                break;
            }
        break;
    case 't':
        if (MATCH3(1,'r','u','e') && KWDEND(4))
            RETURN2(WGL_Token::TOKEN_CONST_TRUE, 4);
        break;

    case 'u':
        if (MATCH6(1,'n','i','f','o','r','m') && KWDEND(7))
            RETURN2(WGL_Token::TOKEN_UNIFORM, 7);
        else if (MATCH3(1,'v','e','c'))
            switch(buf_ptr[4])
            {
            case '2':
                if (KWDEND(5))
                    RETURN_TYPE_NAME(TYPE_UVEC2, 5);
                break;
            case '3':
                if (KWDEND(5))
                    RETURN_TYPE_NAME(TYPE_UVEC3, 5);
                break;
            case '4':
                if (KWDEND(5))
                    RETURN_TYPE_NAME(TYPE_UVEC4, 5);
                break;
            default:
                break;
            }
        else if (MATCH7(1,'s','a','m','p','l','e','r'))
        {
            if (MATCH7(8,'1','D','A','r','r','a','y') && KWDEND(15))
                RETURN_TYPE_NAME(TYPE_USAMPLER1DARRAY, 15);
            else if (MATCH7(8,'2','D','A','r','r','a','y') && KWDEND(15))
                RETURN_TYPE_NAME(TYPE_USAMPLER2DARRAY, 15);
            else if (MATCH4(8,'C','u','b','e') && KWDEND(12))
                RETURN_TYPE_NAME(TYPE_USAMPLERCUBE, 12);
            else if (MATCH6(8,'B','u','f','f','e','r') && KWDEND(14))
                RETURN_TYPE_NAME(TYPE_USAMPLERBUFFER, 14);
            else if (MATCH2(8,'1','D') && KWDEND(10))
                RETURN_TYPE_NAME(TYPE_USAMPLER1D, 10);
            else if (MATCH2(8,'2','D') && KWDEND(10))
                RETURN_TYPE_NAME(TYPE_USAMPLER2D, 10);
            else if (MATCH4(8,'2','D','M','S') && KWDEND(12))
                RETURN_TYPE_NAME(TYPE_USAMPLER2DMS, 12);
            else if (MATCH9(8,'2','D','M','S','A','r','r','a','y') && KWDEND(17))
                RETURN_TYPE_NAME(TYPE_USAMPLER2DMSARRAY, 17);
            else if (MATCH6(8,'2','D','R','e','c','t') && KWDEND(14))
                RETURN_TYPE_NAME(TYPE_USAMPLER2DRECT, 14);
            else if (MATCH2(8,'3','D') && KWDEND(10))
                RETURN_TYPE_NAME(TYPE_USAMPLER3D, 10);
        }
        break;
    case 'v':
        switch (buf_ptr[1])
        {
        case 'a':
            if (MATCH5(2,'r','y','i','n','g') && KWDEND(7))
                RETURN2(WGL_Token::TOKEN_VARYING, 7);
            break;
        case 'e':
            if (buf_ptr[2] == 'c')
                switch(buf_ptr[3])
                {
                case '2':
                    if (KWDEND(4))
                        RETURN_TYPE_NAME(TYPE_VEC2, 4);
                    break;
                case '3':
                    if (KWDEND(4))
                        RETURN_TYPE_NAME(TYPE_VEC3, 4);
                    break;
                case '4':
                    if (KWDEND(4))
                        RETURN_TYPE_NAME(TYPE_VEC4, 4);
                    break;
                default:
                    break;
                }
            break;
        case 'o':
            if (MATCH2(2,'i','d') && KWDEND(4))
                RETURN2(WGL_Token::TOKEN_VOID, 4);
            break;
        }
        break;
    case 'w':
        if (MATCH4(1,'h','i','l','e') && KWDEND(5))
            RETURN2(WGL_Token::TOKEN_WHILE, 5);
        break;
    default:
        break;
    }

    int tok = LexId(lexeme.val_id);
    if (tok < 0)
        goto start;
    else
        return tok;
}

/* Identifier lexer.

   We may end up here without recognizing a single character of input,
   so must check that even *input is a valid identifier char and signal
   an error if not.  But the first char will not be 0..9.

   Common cases:
    - \<linebreak> does not occur in identifier
    - identifier will usually be very short
    - identifier will usually not cross buffer boundary
*/
int WGL_LexBuffer::LexId(WGL_String *&id_result)
{
    {
        if (buf_end - buf_ptr < WGL_PARM_LEXBUFFER_GUARANTEED)
            FillBuffer();

        /* Common case: short identifier entirely within the input buffer,
           terminated by something easy in the ASCII range. */
        const uni_char *r_buf_ptr = buf_ptr;

        if (!wgl_isident0(*r_buf_ptr))
            return WGL_Token::TOKEN_ILLEGAL;

        while (r_buf_ptr != buf_end && wgl_isident(*r_buf_ptr))
            ++r_buf_ptr;
        if (r_buf_ptr != buf_end && !wgl_isident(*r_buf_ptr))
        {
            if (buf_ptr == r_buf_ptr) // an identifier of length 0!?
            {
                ++buf_ptr;
                return WGL_Token::TOKEN_ILLEGAL;
            }
            else
            {
                id_result = WGL_String::Make(context, buf_ptr, static_cast<unsigned>(r_buf_ptr - buf_ptr));
                buf_ptr = const_cast<uni_char *>(r_buf_ptr);
                return WGL_Token::TOKEN_IDENTIFIER;
            }
        }
        // Fall back to slower code: 'buf_ptr' still points to the first char

        uni_char shortbuf[WGL_PARM_LEXBUFFER_GUARANTEED*2]; // ARRAY OK 2011-07-05 wonko
        uni_char *idbuf = shortbuf;     // Points to current collection buffer
        uni_char *idp = shortbuf;       // Points to next free space
        uni_char *idlim = shortbuf + ARRAY_SIZE(shortbuf) - 1;  // Points to space beyond last free; leave space for a terminator
        BOOL linebreak_escape=FALSE;        // Set to TRUE if \<linebreak> seen
        uni_char c = 0;

        while (TRUE)
        {
            if (buf_end - buf_ptr < 6)
                FillBuffer();
            if (buf_ptr[0] < 128)
            {
                if (wgl_isident(buf_ptr[0]))
                {
                    c = *buf_ptr++;
                    goto addchar;
                }
#ifdef WGL_SUPPORT_LINE_CONTINUATION_CHARACTERS
                else if (buf_ptr[0] == '\\')
                {
                    if (buf_ptr[1] == '\n')
                    {
                        linebreak_escape=TRUE;
                        line_number++;
                        buf_ptr+=2;
                        continue;
                    }
                    else if (buf_ptr[1] == '\r' && buf_ptr[2] == '\n')
                    {
                        linebreak_escape=TRUE;
                        line_number++;
                        buf_ptr+=3;
                        continue;
                    }
                    else
                        goto ident_found;
                }
#endif // WGL_SUPPORT_LINE_CONTINUATION_CHARACTERS
                else
                    goto ident_found;
            }
            else if (buf_ptr[0] == CHAR_BOM)
                goto ident_found;
            else
                goto invalid;

        addchar:
            if (idp == idlim)
            {
                unsigned current_length = static_cast<unsigned>(idp - idbuf);
                unsigned newbuf_length = (current_length + 1) * 2;
                uni_char *newidbuf = OP_NEWGROA_L(uni_char, newbuf_length, context->Arena());

                op_memcpy(newidbuf, idbuf, current_length * sizeof(uni_char));
                idbuf = newidbuf;
                idp = idbuf + current_length;
                idlim = idbuf + newbuf_length - 1;      // Leave space for a terminator
            }
            *idp++ = c;
        }

    ident_found:
        if (idp == idbuf)
        {
            if (linebreak_escape)
                return -1;      // Just a \<linebreak>

            /* Empty identifier, return ILLEGAL */
        }
        else
        {
            id_result = WGL_String::Make(context, idbuf, static_cast<unsigned>(idp - idbuf));
            return WGL_Token::TOKEN_IDENTIFIER;
        }
    }

invalid:
    ++buf_ptr;
    return WGL_Token::TOKEN_ILLEGAL;
}

unsigned
WGL_Lexer::GetErrorContext(uni_char *buffer, unsigned length)
{
    if (AtEndOfInput() || buf_end - buf_ptr_save == 1 || length == 0)
    {
        buffer[0] = 0;
        return 0;
    }
    else
    {
        length = MIN(length - 1, static_cast<unsigned>(buf_end - buf_ptr_save));
        uni_strncpy(buffer, buf_ptr_save, length);
        buffer[length] = 0;
        return length;
    }
}

int
WGL_LexBuffer::SkipLine()
{
start:
    if (buf_end - buf_ptr < MAX_FIXED_TOKEN_LENGTH)
        FillBuffer();

    switch (buf_ptr[0])
    {
    case CHAR_HT:
    case CHAR_VT:
    case CHAR_FF:
    case ' ':
    case CHAR_BOM:
        buf_ptr++;
        goto start;
    case '\r':
        line_number++;
        buf_ptr++;
        if (*buf_ptr == '\n')
            buf_ptr++;
        return '\n';
    case '\n':
        line_number++;
        buf_ptr++;
        return '\n';
    case '/':
        switch (buf_ptr[1])
        {
        case '*':
            buf_ptr += 2;
            {
                BOOL lineterminator_seen = FALSE;
                while (TRUE)
                {
                    if (buf_end - buf_ptr < 2)
                    {
                        FillBuffer();
                        if (AtEndOfInput())
                            return -1;  // EOF in comment
                    }
                    if (buf_ptr[0] == '*' && buf_ptr[1] == '/')
                    {
                        buf_ptr += 2;
                        if (lineterminator_seen)
                            return '\n';
                        else
                            goto start;
                    }
                    else if (buf_ptr[0] == '\r')
                    {
                        if (buf_end - buf_ptr < 2)
                        {
                            FillBuffer();
                            if (AtEndOfInput())
                                return -1;  // EOF in comment
                        }
                        if (buf_ptr[1] == '\n')
                            buf_ptr++;
                        lineterminator_seen = TRUE;
                        line_number++;
                    }
                    else if (buf_ptr[0] == '\n')
                    {
                        lineterminator_seen = TRUE;
                        line_number++;
                    }
                    buf_ptr++;
                }
            }
        case '/':
            buf_ptr += 2;
        linecomment:
            while (TRUE)
            {
                if (buf_end - buf_ptr < 1)
                {
                    FillBuffer();
                    if (AtEndOfInput())
                        goto start;
                }
                if (buf_ptr[0] == '\r' || buf_ptr[0] == '\n')
                    goto start;
                buf_ptr++;
            }
        default:
            goto significant_char;
        }
    case '<':
        switch (buf_ptr[1])
        {
        case '!':
            if (buf_ptr[2] == '-' &&
                buf_ptr[3] == '-')
            {
                /* Extension:  HTML <!-- comment initial, which we treat like a // comment.
                   Note that <!-- is a valid but unlikely token stream: "x<!--y".
                */
                buf_ptr += 4;
                goto linecomment;
            }
            else
                goto significant_char;
        default:
            goto significant_char;
        }
    default:
    significant_char:
        return buf_ptr[0] == ':' ? (buf_ptr[1] == ':' ? ' ' : buf_ptr[0]) : buf_ptr[0];
    }
}

/* Common case:
     - it is a smallish integer (certainly less than 2^31)
     - the string representing the number fits entirely within the input buffer
   Assuming this, construct the integer value on the fly, and when the end
   is found, just return the value.

   If we find that we are looking at a float, then we're really looking to grab
   a string and convert that.

   If we reach the end of the buffer, then it gets to be too hairy to deal with the
   simple case, so fall back on the float code.

   If the integer is about to overflow then fall back on the float code.
*/
int
WGL_Lexer::LexDecimalNumber()
{
    if (buf_end - buf_ptr < WGL_PARM_LEXBUFFER_GUARANTEED)
        FillBuffer();

    // Common case
    const uni_char *buf_ptr_origin = buf_ptr;
    unsigned i = 0;
    // Any 9-digit decimal number can be represented as a 32-bit unsigned
    int k = 9;

    while (k > 0 && wgl_isdigit(*buf_ptr))
    {
        --k;
        i = i*10 + (*buf_ptr++ - '0');
    }

    if (!wgl_isdigit(*buf_ptr) && *buf_ptr != '.' && *buf_ptr != 'e' && *buf_ptr != 'E')
    {
        if (*buf_ptr == 'u' || *buf_ptr == 'U')
            buf_ptr++;

        lexeme.val_uint = i;
        return WGL_Token::TOKEN_CONST_UINT;
    }

    // Hard case
    //
    // ({dec_number}\.{dec_number}?([eE][-+]?{dec_number})?)|({dec_number}?\.{dec_number}([eE][-+]?{dec_number})?)
    //
    // States:
    //   0  before anything
    //   1  initial digit string, before .
    //   2  after . but before any exponent
    //   3  after e but before + or -
    //   4  after + or -

    if (!numeric_buffer)
    {
        numeric_buffer_length = 60;
        numeric_buffer = OP_NEWGROA_L(uni_char, numeric_buffer_length, context->Arena());
    }

    uni_char *numbuf = numeric_buffer;
    uni_char *nump = numeric_buffer;
    uni_char *numlim = numeric_buffer + numeric_buffer_length - 1;

    unsigned digits_in_fraction = 0;
    unsigned digits_in_exponent = 0;
    unsigned state;

    buf_ptr = const_cast<uni_char *>(buf_ptr_origin);
    state = 0;
    while (TRUE)
    {
        if (buf_end - buf_ptr < 1)
            FillBuffer();

        if (wgl_isdigit(*buf_ptr))
        {
            if (state == 0)
                state = 1;
            else if (state == 3)
                state = 4;

            if (state == 1 || state == 2)
                digits_in_fraction++;
            else if (state == 4)
                digits_in_exponent++;

            goto addchar;
        }
        else if (*buf_ptr == '.')
        {
            if (state >= 2)
                goto done;
            state = 2;
            goto addchar;
        }
        else if (*buf_ptr == 'e' || *buf_ptr == 'E')
        {
            if (state != 1 && state != 2)
                goto done;
            state = 3;
            goto addchar;
        }
        else if (*buf_ptr == '+' || *buf_ptr == '-')
        {
            if (state != 3)
                goto done;
            state = 4;
            goto addchar;
        }
        else
            goto done;

    addchar:
        if (nump == numlim)
        {
            unsigned current_length = static_cast<unsigned>(nump - numbuf);
            unsigned newbuf_length = (current_length + 1) * 2;
            uni_char *newnumbuf = OP_NEWGROA_L(uni_char, newbuf_length, context->Arena());
            op_memcpy(newnumbuf, numbuf, current_length * sizeof(uni_char));
            numeric_buffer = numbuf = newnumbuf;
            nump = numbuf + current_length;
            numlim = numbuf + newbuf_length - 1;        // Leave space for a terminator
            numeric_buffer_length = newbuf_length;
        }
        *nump++ = *buf_ptr++;
    }

done:
    if (state == 0 || state == 3 || digits_in_fraction == 0 || (state == 4 && digits_in_exponent == 0))
        return WGL_Token::TOKEN_ILLEGAL;

    *nump = 0;
    double d = uni_strtod(numbuf, NULL);
    WGL_Token::Token lit_token = WGL_Token::TOKEN_CONST_FLOAT;
    if (state == 1 && op_isintegral(d))
    {
        if (d >= static_cast<double>(INT_MIN) && d <= static_cast<double>(INT_MAX))
        {
            lexeme.val_int = static_cast<int>(d);
            lit_token = WGL_Token::TOKEN_CONST_INT;
        }
        else if (d >= 0 && d <= static_cast<double>(UINT_MAX))
        {
            lexeme.val_uint = static_cast<unsigned>(d);
            lit_token = WGL_Token::TOKEN_CONST_UINT;
        }
        else
            lexeme.val_double = d;
    }
    else
        lexeme.val_double = d;

    return lit_token;
}

/* Common case:
     - it is less than 2^31
     - the string representing the number fits entirely within the input buffer
   Assuming this, construct the integer value on the fly, and when the end
   is found, just return the value.

   If the integer is about to overflow then switch to using floats, but there is
   no special end-of-buffer handling.
*/
int
WGL_Lexer::LexHexNumber()
{
    double d = 0.0;
    if (numeric_buffer)
        *numeric_buffer = 0;

    buf_ptr += 2;

    while (TRUE)
    {
        if (buf_end == buf_ptr)
        {
            FillBuffer();
            if (buf_end == buf_ptr)
                break;
        }
        if (wgl_isdigit(*buf_ptr))
            d = d * 16 + (*buf_ptr++ - '0');
        else if (*buf_ptr >= 'a' && *buf_ptr <= 'f')
            d = d * 16 + (*buf_ptr++ - 'a' + 10);
        else if (*buf_ptr >= 'A' && *buf_ptr <= 'F')
            d = d * 16 + (*buf_ptr++ - 'A' + 10);
        else
            break;
    }
    if (d <= static_cast<double>(INT_MAX))
    {
        lexeme.val_int = static_cast<int>(d);
        return WGL_Token::TOKEN_CONST_INT;
    }
    else
    {
        lexeme.val_double = d;
        return WGL_Token::TOKEN_CONST_FLOAT;
    }
}

WGL_CPP::ExtensionStatus
WGL_CPP::CheckExtension(const uni_char *extension, unsigned extension_length, const uni_char *behaviour, unsigned behaviour_length)
{
    if (behaviour_length >= 7 && uni_strncmp(behaviour, UNI_L("require"), 7) == 0)
    {
        if (context->IsSupportedExtension(extension, extension_length))
        {
            const uni_char *cpp_define = NULL;
            if (context->EnableExtension(extension, extension_length, &cpp_define) && cpp_define)
                AddDefineL(WGL_String::Make(context, cpp_define), UNI_L("1"));
            return ExtensionOK;
        }
        else
            return ExtensionUnsupported;
    }
    else if (behaviour_length >= 7 && uni_strncmp(behaviour, UNI_L("disable"), 7) == 0)
    {
        if (extension_length == 3 && uni_strncmp(extension, UNI_L("all"), 3) == 0)
        {
            for (unsigned i = 0; i < context->GetExtensionCount(); i++)
            {
                const uni_char *cpp_define = NULL;
                if (context->DisableExtension(i, &cpp_define) && cpp_define)
                    RemoveDefineL(WGL_String::Make(context, cpp_define));
            }
            return ExtensionOK;
        }
        else if (context->IsSupportedExtension(extension, extension_length))
        {
            const uni_char *cpp_define = NULL;
            if (context->DisableExtension(extension, extension_length, &cpp_define) && cpp_define)
                RemoveDefineL(WGL_String::Make(context, cpp_define));

            return ExtensionOK;
        }
        else
            return ExtensionWarnUnsupported;
    }
    else if (behaviour_length >= 6 && uni_strncmp(behaviour, UNI_L("enable"), 6) == 0)
    {
        if (extension_length == 3 && uni_strncmp(extension, UNI_L("all"), 3) == 0)
            return ExtensionUnsupported;
        else if (context->IsSupportedExtension(extension, extension_length))
        {
            const uni_char *cpp_define = NULL;
            if (context->EnableExtension(extension, extension_length, &cpp_define) && cpp_define)
                AddDefineL(WGL_String::Make(context, cpp_define), UNI_L("1"));
            return ExtensionOK;
        }
        else
            return ExtensionWarnUnsupported;
    }
    else if (behaviour_length >= 4 && uni_strncmp(behaviour, UNI_L("warn"), 4) == 0)
    {
        /* No real support for a warnings-only mode, but signal unsupported ones. */
        if (extension_length == 3 && uni_strncmp(extension, UNI_L("all"), 3) == 0)
            return ExtensionOK;
        else if (context->IsSupportedExtension(extension, extension_length))
            return ExtensionOK;
        else
            return ExtensionUnsupported;
    }
    else
        return ExtensionIllegal;
}

#endif // CANVAS3D_SUPPORT
