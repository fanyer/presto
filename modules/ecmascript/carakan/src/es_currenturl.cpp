/* -*- Mode: c++; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2004
 *
 * class ES_CurrentURL
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "modules/ecmascript/carakan/src/es_currenturl.h"

#ifdef CRASHLOG_CAP_PRESENT

const char *g_ecmaCurrentURL;

ES_CurrentURL::ES_CurrentURL(ES_Runtime *runtime)
{
    Initialize(runtime);
}

ES_CurrentURL::ES_CurrentURL(ES_Heap *heap)
{
    Initialize(heap->GetFirstRuntime());
}

ES_CurrentURL::~ES_CurrentURL()
{
    buffer[0] = 0;
    g_ecmaCurrentURL = NULL;
}

void
ES_CurrentURL::Initialize(ES_Runtime *runtime)
{
    if (!runtime)
        buffer[0] = 0;
    else
    {
        unsigned length = es_minu(sizeof buffer - 19, runtime->url.Length());

        op_memcpy(buffer, "CarakanCurrentURL=", 18);
        op_memcpy(buffer + 18, runtime->url.CStr(), length);
        buffer[18 + length] = 0;
    }

    g_ecmaCurrentURL = buffer;
}

#endif // CRASHLOG_CAP_PRESENT
