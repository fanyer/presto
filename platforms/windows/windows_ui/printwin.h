/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _PRINTWIN_H_
#define _PRINTWIN_H_

BOOL cdPrint(Window* window, BOOL use_default = FALSE);
BOOL cdPrintSetup( HWND hWndParent);
LONG ucPrintDocFormatted();
LONG ucPrintPagePrinted(WORD page);
LONG ucPrintingFinished(Window* window);
LONG ucPrintingAborted(Window* window);

#endif //_PRINTWIN_H_