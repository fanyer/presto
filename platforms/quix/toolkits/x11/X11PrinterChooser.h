/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Espen Sand
 */

#ifndef X11_PRINTER_CHOOSER_H
#define X11_PRINTER_CHOOSER_H

#include "platforms/quix/toolkits/ToolkitPrinterIntegration.h"
#include "platforms/quix/dialogs/PrintDialog.h"

class X11ToolkitPrinterIntegration : public ToolkitPrinterIntegration, public PrintDialog::PrintDialogListener
{
public:
	X11ToolkitPrinterIntegration();
	~X11ToolkitPrinterIntegration();
	
	bool NeedsPrinterHelper() { return true; }
	bool SupportsOfflinePrinting() { return true; }
	void SetCaption(const char* caption) {}

	bool Init(ToolkitPrinterHelper* helper);
	bool RunPrintDialog(X11Types::Window parent);
	bool Print(const char* print_file_path, const char* print_job_name);
	bool PageShouldBePrinted(int page_number);
	bool GetPrinterResolution(int& horizontal, int& vertical);
	bool GetPrintScale(int& scale);
	bool GetPaperSize(double& width, double& height);
	bool GetMargins(double& top, double& left, double& bottom, double& right);	
	bool GetPrintSelectionOnly();

	// PrintDialog::PrintDialogListener
	void OnDialogClose(bool accepted) { m_accepted = accepted; }

private:
	ToolkitPrinterHelper* m_helper;
	bool m_accepted;
	PrintDialog::PrinterState m_state;
	
};

#endif // X11_PRINTER_CHOOSER_H
