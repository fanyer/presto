/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** 
*/

#ifndef OPERA_DESKTOP_STARTER_H
#define OPERA_DESKTOP_STARTER_H

struct GpuInfo;

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*PF_OperaDesktopStart)(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow);
typedef int (*PF_OperaSetSpawnerPath)(LPWSTR path);
typedef int (*PF_OperaSetLaunchMan)(void* launch_man);
typedef uni_char* (*PF_OperaGetNextUninstallFile)();
typedef void (*PF_OpWaitFileWasPresent)();
typedef void (*PF_OpSetGpuInfo)(GpuInfo* gpu_info);
#ifdef __cplusplus
}
#endif

#endif //this

