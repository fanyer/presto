/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef CUPS_HELPER_H
#define CUPS_HELPER_H

#include "platforms/quix/toolkits/ToolkitPrinterHelper.h"

struct CupsFunctions;
class OpDll;

class CupsHelper : public ToolkitPrinterHelper
{
public:
	CupsHelper();
	~CupsHelper();

	OP_STATUS Init();
	OP_STATUS Init(CupsFunctions* functions) { m_functions = functions; return OpStatus::OK; }

	/**
	 * Returns the library filename (no path components) that the helper will
	 * attempt to load
	 */
	const uni_char* GetLibraryName();

	// From ToolkitPrinterHelper

	virtual bool HasConnection();

	virtual bool SetPrinter(const char* printername) { return OpStatus::IsSuccess(m_printername.Set(printername)); }

	virtual void SetCopies(int copies) { m_copies = copies; }

	virtual bool PrintFile(const char* filename, const char* jobname);

	virtual bool GetDestinations(DestinationInfo**& info);

	virtual void ReleaseDestinations(DestinationInfo**& info);

	virtual bool GetPaperSizes(DestinationInfo& info);

private:
	struct CupsOptions;
	OP_STATUS CreateOptions(CupsOptions& options);
	OP_STATUS SetOption(CupsOptions& options, int option_type);

private:
	class CupsHelperPrivate* m_private;
	CupsFunctions* m_functions;
	OpString8 m_printername;
	int m_copies;
	OpDLL* m_cups;
	OP_STATUS m_init_status;
};

#endif // CUPS_HELPER_H
