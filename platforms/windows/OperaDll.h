/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

//*****************************
// add exported functions here
//*****************************

#ifdef __cplusplus
extern "C" {
#endif

// OpStart : Hold on... start Opera
__declspec(dllexport) int OpStart(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow);

// OpSetSpawnPath : When we're running as a dll, we should know who started us. This is to be used when settings default handler etc.
__declspec(dllexport) int OpSetSpawnPath(uni_char* path);

#ifdef __cplusplus
}
#endif
