/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef GTK_PRINTER_INTEGRATION_H
#define GTK_PRINTER_INTEGRATION_H

#include "platforms/quix/toolkits/ToolkitPrinterIntegration.h"
#include "platforms/quix/toolkits/gtk2/GtkPorting.h"
#include "platforms/quix/toolkits/gtk2/GtkUtils.h"

#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

class GtkPrinterIntegration : public ToolkitPrinterIntegration
{
public:
	GtkPrinterIntegration(GtkWidget* gtk_window);
	virtual ~GtkPrinterIntegration();

	virtual bool NeedsPrinterHelper() { return false; }
	virtual bool SupportsOfflinePrinting() { return true; }
	virtual bool Init(ToolkitPrinterHelper* helper);
	virtual void SetCaption(const char* caption);

	virtual bool RunPrintDialog(X11Types::Window parent);
	virtual bool Print(const char* print_file_path, const char* print_job_name);
	virtual bool PageShouldBePrinted(int page_number);

	void SetDialogResponse(int response);

	virtual bool GetPrinterResolution(int& horizontal, int& vertical);
	void SetPrinterResolution(int horizontal, int vertical);

	virtual bool GetPrintScale(int& scale);
	void SetPrintScale(int scale);

	virtual bool GetPaperSize(double& width, double& height);
	void SetPaperSize(double width, double height);

	virtual bool GetMargins(double& top, double& left, double& bottom, double& right);
	void SetMargins(double top, double left, double bottom, double right);

	virtual bool GetPrintSelectionOnly();


	void SetGtkPrintSettings(GtkPrintSettings* settings) { gtk_print_settings = settings; }
	void SetGtkPageSetup(GtkPageSetup* page_setup) { gtk_page_setup = page_setup; }
	void SetGtkPrinter(GtkPrinter* printer) { gtk_printer = printer; }

private:
	GtkWidget* gtk_window;
	GtkWidget* gtk_print_dialog;
	GtkPrintSettings* gtk_print_settings;
	GtkPageSetup* gtk_page_setup;
	GtkPrinter* gtk_printer;

	int dialog_response;

	int resolution_horizontal;
	int resolution_vertical;
	int print_scale;

	// Paper size in inches
	double paper_width;
	double paper_height;

	double margin_top;
	double margin_left;
	double margin_bottom;
	double margin_right;

};

#endif // GTK_PRINTER_INTEGRATION_H
