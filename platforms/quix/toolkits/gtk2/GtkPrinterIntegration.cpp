/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#include "platforms/quix/toolkits/gtk2/GtkPrinterIntegration.h"
#include "platforms/quix/toolkits/gtk2/GtkToolkitLibrary.h"

#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static void print_dialog_response_cb(GtkDialog *dialog, gint response_id, gpointer data);
#ifdef GTK3
static void print_job_complete_cb(GtkPrintJob *print_job, gpointer user_data, const GError *error);
#else
static void print_job_complete_cb(GtkPrintJob *print_job, gpointer user_data, GError *error);
#endif
static void print_job_userdata_destroy_cb(gpointer data);


GtkPrinterIntegration::GtkPrinterIntegration(GtkWidget* gtk_window) 
	: gtk_window(gtk_window),
	  gtk_print_dialog(0),
	  gtk_print_settings(0),
	  gtk_page_setup(0),
	  gtk_printer(0),
	  dialog_response(0),
	  resolution_horizontal(0),
	  resolution_vertical(0),
	  print_scale(100),
	  paper_width(0),
	  paper_height(0),
	  margin_top(0),
	  margin_left(0),
	  margin_bottom(0),
	  margin_right(0)
{
}

GtkPrinterIntegration::~GtkPrinterIntegration()
{
	if (gtk_print_settings)
		g_object_unref(gtk_print_settings);

	if (gtk_print_dialog)
		gtk_widget_destroy(GTK_WIDGET(gtk_print_dialog));
}


bool GtkPrinterIntegration::Init(ToolkitPrinterHelper* helper)
{
	gtk_print_dialog = gtk_print_unix_dialog_new("Opera printing", GTK_WINDOW(gtk_window));

	// Get and set default paper metrics
	GtkPageSetup* gtk_page_setup = gtk_print_unix_dialog_get_page_setup(GTK_PRINT_UNIX_DIALOG(gtk_print_dialog));
	if (gtk_page_setup)
	{
		GtkPaperSize* gtk_paper_size = gtk_page_setup_get_paper_size(gtk_page_setup);
		if (gtk_paper_size)
		{
			SetPaperSize(gtk_paper_size_get_width(gtk_paper_size, GTK_UNIT_INCH),
						 gtk_paper_size_get_height(gtk_paper_size, GTK_UNIT_INCH));
		}

		SetMargins(gtk_page_setup_get_top_margin(gtk_page_setup, GTK_UNIT_INCH),
				   gtk_page_setup_get_left_margin(gtk_page_setup, GTK_UNIT_INCH),
				   gtk_page_setup_get_bottom_margin(gtk_page_setup, GTK_UNIT_INCH),
 				   gtk_page_setup_get_right_margin(gtk_page_setup, GTK_UNIT_INCH));
	}

	return true;
}


void GtkPrinterIntegration::SetCaption(const char* caption)
{
	gtk_window_set_title(GTK_WINDOW(gtk_print_dialog), caption);
}


bool GtkPrinterIntegration::RunPrintDialog(X11Types::Window parent)
{
	if (!gtk_print_dialog)
		return false;
	g_signal_connect(G_OBJECT(gtk_print_dialog), "response", G_CALLBACK (print_dialog_response_cb), this);

	// Needed to allow change of paper size
	gtk_print_unix_dialog_set_embed_page_setup(GTK_PRINT_UNIX_DIALOG(gtk_print_dialog), true);
	gtk_print_unix_dialog_set_support_selection(GTK_PRINT_UNIX_DIALOG(gtk_print_dialog), true);
	gtk_print_unix_dialog_set_has_selection(GTK_PRINT_UNIX_DIALOG(gtk_print_dialog), true);

	gtk_print_unix_dialog_set_manual_capabilities(GTK_PRINT_UNIX_DIALOG(gtk_print_dialog), (GtkPrintCapabilities)
		( GTK_PRINT_CAPABILITY_SCALE
		| GTK_PRINT_CAPABILITY_GENERATE_PS));

	gtk_window_present(GTK_WINDOW(gtk_print_dialog));

	XSetTransientForHint(GDK_WINDOW_XDISPLAY(gtk_widget_get_window(gtk_print_dialog)),
						 GDK_WINDOW_XID(gtk_widget_get_window(gtk_print_dialog)), parent);
	GtkUtils::SetResourceName(gtk_print_dialog, "printdialog");

	gtk_main();  // GTK main loop will run until dialog closes and produces response callback

	while (gtk_events_pending()) 
		gtk_main_iteration();
	gdk_flush();

	return (dialog_response == GTK_RESPONSE_OK) ? true : false;
}


bool GtkPrinterIntegration::Print(const char* print_file_path, const char* print_job_name)
{
	if (!gtk_print_settings || !gtk_page_setup || !gtk_printer)
		return false;

	GtkPrintJob * gtk_print_job = gtk_print_job_new(print_job_name, gtk_printer, gtk_print_settings, gtk_page_setup);
	if (!gtk_print_job)
		return false;

	GError *error;
	bool result = gtk_print_job_set_source_file(gtk_print_job, print_file_path, &error);
	if (!result)
		return false;

	gtk_print_job_send(gtk_print_job, print_job_complete_cb, NULL, print_job_userdata_destroy_cb);
	g_object_unref(gtk_print_job);

	// Fix for DSK-325937: Call to gtk_main() or gtk_main_iteration() result in call to GtkToolkitLibrary::TimeOutCallback() 
	// and runner->RunSlice() which causes an another print messages to be posted.
	GtkToolkitLibrary::SetCanCallRunSlice(false);

	gtk_main();  // GTK main loop will run until print job finishes and produces print_job_complete_cb() call
	gtk_widget_destroy(GTK_WIDGET(gtk_print_dialog));

	while (gtk_events_pending()) 
		gtk_main_iteration();
	gdk_flush();

	GtkToolkitLibrary::SetCanCallRunSlice(true); // Fix for DSK-325937.

	return true;
}


bool GtkPrinterIntegration::PageShouldBePrinted(int page_number)
{
	if (!gtk_print_settings)
		return true;

	switch (gtk_print_settings_get_print_pages(gtk_print_settings))
	{
		case GTK_PRINT_PAGES_ALL:
		case GTK_PRINT_PAGES_SELECTION:
			return true;
		case GTK_PRINT_PAGES_RANGES:
		{
			gint num_ranges;
			GtkPageRange* gtk_page_ranges = gtk_print_settings_get_page_ranges(gtk_print_settings, &num_ranges);
			for (gint i = 0; i < num_ranges; i++)
			{
				if (gtk_page_ranges[i].start <= page_number && page_number <= gtk_page_ranges[i].end)
				{
					g_free(gtk_page_ranges);
					return true;
				}
			}
			g_free(gtk_page_ranges);
			return false;
		}
		case GTK_PRINT_PAGES_CURRENT:
			assert(!"Current page not supported - should not be available in dialog");
			return true;
		default:
			assert(!"Unknown page selection mode");
			break;
	}

	return true;
}


void GtkPrinterIntegration::SetDialogResponse(int response)
{
	dialog_response = response;
}


bool GtkPrinterIntegration::GetPrinterResolution(int& horizontal, int& vertical)
{
	if (resolution_horizontal == 0 || resolution_vertical == 0)
		return false;

	horizontal = resolution_horizontal;
	vertical = resolution_vertical;
	return true;
}

void GtkPrinterIntegration::SetPrinterResolution(int horizontal, int vertical)
{
	resolution_horizontal = horizontal;
	resolution_vertical = vertical;
}


bool GtkPrinterIntegration::GetPrintScale(int& scale)
{
	scale = print_scale;
	return true;
}

void GtkPrinterIntegration::SetPrintScale(int scale)
{
	print_scale = scale;
}


bool GtkPrinterIntegration::GetPaperSize(double& width, double& height)
{
	if (paper_width == 0 || paper_height == 0)
		return false;
	
	width = paper_width;
	height = paper_height;
	return true;
}


void GtkPrinterIntegration::SetPaperSize(double width, double height)
{
	paper_width = width;
	paper_height = height;
}


bool GtkPrinterIntegration::GetMargins(double& top, double& left, double& bottom, double& right)
{
	if (margin_top == 0 || margin_left == 0 || margin_bottom == 0 || margin_right == 0)
		return false;

	top = margin_top;
	left = margin_left;
	bottom = margin_bottom;
	right = margin_right;
	return true;
}

void GtkPrinterIntegration::SetMargins(double top, double left, double bottom, double right)
{
	margin_top = top;
	margin_left = left;
	margin_bottom = bottom;
	margin_right = right;
}

bool GtkPrinterIntegration::GetPrintSelectionOnly()
{
	return gtk_print_settings && gtk_print_settings_get_print_pages(gtk_print_settings) == GTK_PRINT_PAGES_SELECTION;
}



/**************************
 * Static GTK+ functions 
 **************************/

static void print_dialog_response_cb(GtkDialog *dialog, gint response_id, gpointer data)
{
	GtkPrintUnixDialog* print_dialog = GTK_PRINT_UNIX_DIALOG(dialog);
	GtkPrinterIntegration* printer_integration = (GtkPrinterIntegration*) data;

	// This will control return value of RunPrintDialog()
	printer_integration->SetDialogResponse(response_id);

	GtkPrintSettings* gtk_print_settings = gtk_print_unix_dialog_get_settings(print_dialog);
	GtkPrinter* gtk_printer = gtk_print_unix_dialog_get_selected_printer(print_dialog);

	printer_integration->SetGtkPrintSettings(gtk_print_settings);
	printer_integration->SetGtkPrinter(gtk_printer);

	printer_integration->SetPrintScale(gtk_print_settings_get_scale(gtk_print_settings));

	GtkPageSetup* gtk_page_setup = gtk_print_unix_dialog_get_page_setup(print_dialog);
	if (gtk_page_setup)
	{
		printer_integration->SetGtkPageSetup(gtk_page_setup);
		GtkPaperSize* gtk_paper_size = gtk_page_setup_get_paper_size(gtk_page_setup);
		if (gtk_paper_size)
		{
			printer_integration->SetPaperSize(gtk_paper_size_get_width(gtk_paper_size, GTK_UNIT_INCH),
											  gtk_paper_size_get_height(gtk_paper_size, GTK_UNIT_INCH));
		}

		printer_integration->SetMargins(gtk_page_setup_get_top_margin(gtk_page_setup, GTK_UNIT_INCH),
										gtk_page_setup_get_left_margin(gtk_page_setup, GTK_UNIT_INCH),
										gtk_page_setup_get_bottom_margin(gtk_page_setup, GTK_UNIT_INCH),
 										gtk_page_setup_get_right_margin(gtk_page_setup, GTK_UNIT_INCH));
	}

	gtk_widget_hide(GTK_WIDGET(dialog));
	gtk_main_quit();
}


#ifdef GTK3
static void print_job_complete_cb(GtkPrintJob *print_job, gpointer user_data, const GError *error)
#else
static void print_job_complete_cb(GtkPrintJob *print_job, gpointer user_data, GError *error)
#endif
{
	if (error)
		printf("opera: Print job failed.\n");
	gtk_main_quit();
}


static void print_job_userdata_destroy_cb(gpointer data)
{
	// We currently don't allocate any user data when sending the print job
}
