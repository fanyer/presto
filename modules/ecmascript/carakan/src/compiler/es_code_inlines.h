/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#ifndef ES_CODE_INLINES_H
#define ES_CODE_INLINES_H

#ifdef ES_NATIVE_SUPPORT
inline BOOL
ES_CodeStatic::CanHaveNativeDispatcher()
{
    return exception_handlers_count == 0
#ifdef ECMASCRIPT_DEBUGGER
           && has_debug_code == 0
#endif // ECMASCRIPT_DEBUGGER
           && codewords_count < (1 << (8 * sizeof(unsigned) - ES_CodeOptimizationData::RegisterAccess::SHIFT_WRITE_CW_INDEX));
}
#endif // ES_NATIVE_SUPPORT

#endif // ES_CODE_INLINES_H
