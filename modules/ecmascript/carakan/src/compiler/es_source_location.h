/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2002 - 2006
 *
 * Bytecode compiler for ECMAScript -- overview, common data and functions
 *
 * @author Jens Lindstrom
 */

#ifndef ES_SOURCE_LOCATION_H
#define ES_SOURCE_LOCATION_H


#include "modules/util/opstring.h"


class ES_Context;
class ES_Fragments;
class ES_SourceLocation;
class ES_StaticSourceData;


class ES_SourceCode
{
public:
    ES_SourceCode()
        : source(NULL),
          owns_source(FALSE),
          index_offset(0),
          location_offset(0),
          line_offset(0),
          column_offset(0),
          length(0),
          script_guid(0)
    {
    }

    ~ES_SourceCode()
    {
        if (owns_source)
            op_free(source);
    }

    void Set(JStringStorage *source, unsigned index, unsigned line, unsigned column, unsigned length, unsigned script_guid);
    void SetLocationOffset(unsigned offset) { location_offset = offset; }

    JStringStorage *GetSource() { return source; }
    JString *GetSource(ES_Context *context);
    JString *GetExtent(ES_Context *context, const ES_SourceLocation &location, BOOL first_line_only = FALSE);

#ifdef _DEBUG
    JString *GetExtentWithContext(ES_Context *context, const ES_SourceLocation &location, unsigned lines);
#endif // _DEBUG

    unsigned GetScriptGuid();

    void Resolve(const ES_SourceLocation &location, unsigned &index, unsigned &line, unsigned &column);

    void Resolve(const ES_SourceLocation &location, const ES_StaticSourceData *source_data, ES_SourcePosition &position);
    /**< Calculate the ES_SourcePosition from an ES_SourceLocation and
         (optionally) an ES_StaticSourceData.

         @param [in] location The ES_SourceLocation to resolve.
         @param [in] source_data The object with additional information about the
                                 location from the document parser. May be NULL.
         @param [out] position The object to recieve the results. */
private:
    JStringStorage *source;
    /**< The whole original source code. */
    BOOL owns_source;

    unsigned index_offset;
    /**< Number of characters at the beginning of 'source' that is not part of
         this source code. */
    unsigned location_offset;
    /**< Value added to the index of all ES_SourceLocation objects resolved
         relative this source code. */
    unsigned line_offset;
    /**< Number of line breaks between the beginning of 'source' and the
         'index_offset'th character in 'source'. */
    unsigned column_offset;
    /**< Number of characters between the last line break before the
         'index_offset'th character in 'source' and the 'index_offset'th
         character itself. */
    unsigned length;
    /**< Length of source code. */
    unsigned script_guid;
    /**< Unique script id. */
};

class ES_SourceLocation
{
public:
    ES_SourceLocation()
        : low(0xffffffffu),
          high(0xffffffffu)
    {
    }

    ES_SourceLocation(unsigned index, unsigned line, unsigned length = 0xfffffu)
    {
        Set(index, line, length);
    }

    ES_SourceLocation(const ES_SourceLocation &start, const ES_SourceLocation &end)
    {
        Set(start.Index(), start.Line(), end.Index() + end.Length() - start.Index());
    }

    void Initialize()
    {
        low = high = 0xffffffffu;
    }

    void Set(unsigned index, unsigned line, unsigned length)
    {
        if (index > 0xffffffu) index = 0xffffffu;
        if (line > 0xfffffu) line = 0xfffffu;
        if (length > 0xfffffu) length = 0xfffffu;

        low = index | ((line & 0xffu) << 24);
        high = ((line & 0xfff00u) >> 8) | (length << 12);
    }

    void SetLength(unsigned length)
    {
        if (length > 0xfffff) length = 0xfffffu;
        high = (high & 0xfffu) | (length << 12);
    }

    BOOL IsValid() const { return low != 0xffffffffu || high != 0xffffffffu; }
    unsigned Index() const { return low & 0xffffffu; }
    unsigned Line() const { return ((low & 0xff000000u) >> 24) | ((high & 0xfffu) << 8); }
    unsigned Length() const { return ((high & 0xfffff000u) >> 12); }

    BOOL operator==(const ES_SourceLocation &other) { return low == other.low && high == other.high; }
    BOOL operator!=(const ES_SourceLocation &other) { return low != other.low || high != other.high; }

private:
    unsigned low, high;
};

#endif // ES_SOURCE_LOCATION_H
