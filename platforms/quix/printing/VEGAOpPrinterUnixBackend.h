/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef VEGAOPPRINTERUNIXBACKEND_H
#define VEGAOPPRINTERUNIXBACKEND_H

#include "modules/libvega/src/postscript/vegaopprinterbackend.h"

class ToolkitPrinterIntegration;

class VEGAOpPrinterUnixBackend : public VEGAOpPrinterBackend
{
public:
	OP_STATUS Init();

	/** Display printer dialog if needed and do other preparations */
	virtual OP_STATUS PreparePrinting();

	/** Getters for printer metrics */
	virtual OP_STATUS GetPrinterResolution(UINT16& horizontal, UINT16& vertical);
	virtual OP_STATUS GetPrintScale(UINT32& scale);

	virtual OP_STATUS GetPaperSize(double& width, double& height);
	virtual OP_STATUS GetMargins(double& top, double& left, double& bottom, double& right);

	virtual bool GetPrintSelectionOnly();

	/** Print. */
	virtual OP_STATUS Print(OpFile& print_file, OpString& print_job_name);
	virtual bool PageShouldBePrinted(int page_number);

private:
	OpAutoPtr<ToolkitPrinterIntegration> m_printer_integration;
};


#endif // VEGAOPPRINTERUNIXBACKEND_H
