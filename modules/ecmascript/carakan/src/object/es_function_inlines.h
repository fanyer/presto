/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1999-2010
 */

#ifndef ES_FUNCTION_INLINES_H
#define ES_FUNCTION_INLINES_H

#include "modules/ecmascript/carakan/src/object/es_function.h"
#include "modules/ecmascript/carakan/src/builtins/es_function_builtins.h"

inline unsigned
ES_Function::GetLength()
{
    return function_code ? function_code->GetData()->formals_count : data.builtin.information >> 16;
}

inline BOOL
ES_Function::IsBoundFunction()
{
    return data.builtin.call == ES_FunctionBuiltins::bound_call;
}

#endif // ES_FUNCTION_INLINES_H
