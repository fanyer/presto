/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/compiler/es_source_location.h"
#include "modules/ecmascript/carakan/src/compiler/es_lexer.h"
#include "modules/ecmascript/carakan/src/kernel/es_string.h"

void
ES_SourceCode::Set(JStringStorage *source_, unsigned index, unsigned line, unsigned column, unsigned length_, unsigned script_guid_)
{
    source = source_;
    index_offset = index;
    line_offset = line - 1;
    column_offset = column;
    length = length_ == UINT_MAX ? source->length - index : length_;
    script_guid = script_guid_;
}

JString *
ES_SourceCode::GetSource(ES_Context *context)
{
    if (source)
        return JString::Make(context, source, index_offset, length);
    else
        return NULL;
}

JString *
ES_SourceCode::GetExtent(ES_Context *context, const ES_SourceLocation &location, BOOL first_line_only)
{
    unsigned length = location.Length();

    if (first_line_only)
    {
        const uni_char *storage = source->storage + location.Index() + location_offset;
        unsigned new_length = 0;

        while (new_length < length && !ES_Lexer::IsLinebreak(storage[new_length]))
            ++new_length;

        length = new_length;
    }

    return JString::Make(context, source, location.Index() + location_offset, length);
}

#ifdef _DEBUG

JString *
ES_SourceCode::GetExtentWithContext(ES_Context *context, const ES_SourceLocation &location, unsigned lines)
{
    const uni_char *source_start = source->storage + index_offset;
    const uni_char *extent_start = source->storage + location.Index() + location_offset;
    const uni_char *extent_end = extent_start + location.Length();
    const uni_char *source_end = source_start + length;

    unsigned limit = lines / 2 + 1, before = 0, after = 0;

    while (extent_start != source_start && before < limit)
    {
        --extent_start;

        if (ES_Lexer::IsLinebreak(*extent_start))
        {
            ++before;

            if (before == limit)
                ++extent_start;
            else if (extent_start != source_start &&
                     extent_start[-1] != *extent_start &&
                     ES_Lexer::IsLinebreak(extent_start[-1]))
                --extent_start;
        }
    }

    while (extent_end != source_end && after < limit)
    {
        ++extent_end;

        if (ES_Lexer::IsLinebreak(extent_end[-1]))
        {
            ++after;

            if (after == limit)
                --extent_end;
            else if (extent_end != source_end &&
                     extent_end[-1] != *extent_end &&
                     ES_Lexer::IsLinebreak(*extent_end))
                ++extent_end;
        }
    }

    return JString::Make(context, source, index_offset + (extent_start - source_start), extent_end - extent_start);
}

#endif // _DEBUG

unsigned
ES_SourceCode::GetScriptGuid()
{
    return script_guid;
}

void
ES_SourceCode::Resolve(const ES_SourceLocation &location, unsigned &index, unsigned &line, unsigned &column)
{
    index = location.Index() + location_offset;
    line = location.Line();
    column = line == line_offset + 1 ? column_offset : 0;
    column += ES_Lexer::GetColumn(source->storage, index);
}

void
ES_SourceCode::Resolve(const ES_SourceLocation &location, const ES_StaticSourceData *source_data, ES_SourcePosition &position)
{
    unsigned index, line, column;

    Resolve(location, index, line, column);

    unsigned doc_line = source_data ? source_data->document_line : 1;
    unsigned doc_column = source_data ? source_data->document_column : 0;

    position = ES_SourcePosition(index, line, column, doc_line, doc_column);
}
