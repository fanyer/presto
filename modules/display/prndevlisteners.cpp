/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef GENERIC_PRINTING

#include "modules/dochand/win.h"
#include "modules/dochand/docman.h"
#include "modules/display/prn_dev.h"
#include "modules/display/prndevlisteners.h"

// ======================================================
// PrintListener
// ======================================================


PrintListener::PrintListener(PrintDevice* prn_dev) :
	prn_dev(prn_dev)
{	
}

void PrintListener::OnPrint()
{
	DocumentManager* doc_man = prn_dev->GetDocumentManager();
	if (doc_man)
	{
		Window * window = doc_man->GetWindow();
		if (window)
			window->PrintNextPage(window->GetPrinterInfo(FALSE));
	}
}

#endif // GENERIC_PRINTING

