/* -*- Mode: c++; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  1995 - 2004
 *
 * class ES_CurrentURL
 */

#ifndef ES_CURRENTURL_H
#define ES_CURRENTURL_H

#ifdef CRASHLOG_CAP_PRESENT

class ES_Runtime;
class ES_Heap;

class ES_CurrentURL
{
public:
    ES_CurrentURL(ES_Runtime *runtime);
    ES_CurrentURL(ES_Heap *heap);

    ~ES_CurrentURL();

private:
    void Initialize(ES_Runtime *runtime);
    char buffer[256]; // ARRAY OK 2011-06-08 sof
};

#endif // CRASHLOG_CAP_PRESENT
#endif // ES_CURRENTURL_H
