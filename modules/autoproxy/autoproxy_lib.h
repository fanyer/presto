/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2005
 *
 * Initialization of JS libraries
 * Lars T Hansen
 */
#ifndef AUTOPROXY_LIB_H
#define AUTOPROXY_LIB_H

class ES_Runtime;
extern void InitAPCLibrary(ES_Runtime* rt);
extern void GetAPCLibrarySourceCode(int *library_size, const uni_char * const **library_strings);

#endif // AUTOPROXY_LIB_H
