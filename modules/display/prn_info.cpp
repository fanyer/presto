/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef _PRINT_SUPPORT_

#include "modules/display/prn_info.h"

#if defined(_MACINTOSH_)
# include "platforms/mac/pi/MacOpPrinterController.h"
#endif

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"

#include "modules/display/prn_dev.h"
#include "modules/doc/frm_doc.h"

#if defined (GENERIC_PRINTING)
# include "modules/pi/OpPrinterController.h"
#elif defined(MSWIN)
# include "platforms/windows/pi/WindowsOpPrinterController.h"
#endif

#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
# include "modules/widgets/WidgetContainer.h"
# include "adjunct/quick/Application.h"
# include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
# include "adjunct/quick_toolkit/windows/DesktopWindow.h"
# ifdef MSWIN
#  include "platforms/windows/pi/WindowsOpMessageLoop.h"
# endif // MSWIN
# ifdef _UNIX_DESKTOP_
#  include "platforms/unix/base/x11/x11_opmessageloop.h"
# endif // _UNIX_DESKTOP_
#endif // PRINT_SELECTION_USING_DUMMY_WINDOW

#ifdef MSWIN
extern PRINTDLG global_print_dlg;
# include "platforms/windows/pi/WindowsOpWindow.h"
#endif // MSWIN

PrinterInfo::PrinterInfo()
{
	print_dev = NULL;

	next_page = 1;
	last_page = USHRT_MAX;
	print_selected_only = FALSE;
	user_requested_print_selection = FALSE;

#if defined (GENERIC_PRINTING)
	platform_data = NULL;
	print_listener = NULL;
#elif defined (MSWIN)
	hdc = NULL;
	pdevnames = NULL;
	pdevmode = NULL;
#endif
}

#ifdef GENERIC_PRINTING

PrinterInfo::PrinterInfo(void * platform_data)
{
	print_dev = NULL;
	next_page = 1;
	last_page = USHRT_MAX;
	print_selected_only = FALSE;
	user_requested_print_selection = FALSE;
	print_listener = NULL;
	this->platform_data = platform_data;
}

#endif

PrinterInfo::~PrinterInfo()
{
	OP_DELETE(print_dev);
#ifdef GENERIC_PRINTING
	OP_DELETE(print_listener);
#endif

#ifdef MSWIN
	if (hdc)
	{
		DeleteDC(hdc);
		hdc = NULL;
    }

	if (pdevmode != NULL)
	{
		GlobalFree(pdevmode);
	}

	if (pdevnames != NULL)
	{
		GlobalFree(pdevnames);
	}
#endif
}

#ifdef MSWIN
extern BOOL printer_initial_landscape;
extern BOOL printer_current_landscape;
#endif


#ifdef MSWIN
BOOL PrinterInfo::GetUserPrinterSettings( HWND hWndParent, BOOL as_settings_dialog, BOOL use_default, BOOL selection)
{
	BOOL fOk = FALSE;

	global_print_dlg.Flags   = PD_RETURNDC;

//	Flags = PD_RETURNDEFAULT | PD_RETURNDC

	OP_ASSERT (!(use_default && as_settings_dialog));

	if (use_default)
	{
		global_print_dlg.Flags |= PD_RETURNDEFAULT;
	}
	else
	{
		if (as_settings_dialog)
			global_print_dlg.Flags |= PD_PRINTSETUP;
		if (selection)
			global_print_dlg.Flags |= PD_SELECTION;
		else
			global_print_dlg.Flags |= PD_NOSELECTION;
	}

	global_print_dlg.nFromPage         = 1;
	global_print_dlg.nToPage	       = 1;				// do we have the actual number of pages available here? Use it if so.
	global_print_dlg.nMinPage          = 0;
	global_print_dlg.nMaxPage          = USHRT_MAX;				// window->doc_manager->print_doc->page_break_element?
	global_print_dlg.hwndOwner         = hWndParent;

	fOk = PrintDlg(&global_print_dlg);

	if (!fOk)
	{
    	DWORD err = CommDlgExtendedError();

    	if (err && err!=PDERR_NODEFAULTPRN)
    	{
			if (pdevmode)
			{
				GlobalFree(global_print_dlg.hDevMode);
    			global_print_dlg.hDevMode = NULL;
			}
			if (pdevnames)
			{
				GlobalFree(global_print_dlg.hDevNames);
	    		global_print_dlg.hDevNames = NULL;
			}

			fOk = PrintDlg(&global_print_dlg);
		}
	}

	if (fOk)
	{
		DEVMODE* devmode = (DEVMODE*)GlobalLock(global_print_dlg.hDevMode);
		if(!devmode) return FALSE; // Will prevent crash, but still wan't print
		printer_initial_landscape = devmode->dmOrientation == DMORIENT_LANDSCAPE;
		printer_current_landscape = printer_initial_landscape;
		devmode->dmICMMethod = DMICMMETHOD_NONE;
		::ResetDC(global_print_dlg.hDC, devmode);
		GlobalUnlock(global_print_dlg.hDevMode);
	}

	return fOk;
}
#endif

#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
void PrinterInfo::MakeSelectionWindow(Window * window, Window ** selection_window)
{
	FramesDocument *doc = window->GetActiveFrameDoc();

	if (doc)
	{
		long sel_text_len = doc->GetTopDocument()->GetSelectedTextLen();

		if (sel_text_len > 0)
		{
			OpBrowserView* browser_view;
			RETURN_VOID_IF_ERROR(OpBrowserView::Construct(&browser_view));

			browser_view->SetVisibility(FALSE);

			g_application->GetActiveDesktopWindow()->GetWidgetContainer()->GetRoot()->AddChild(browser_view);

			*selection_window = browser_view->GetWindow();

			if (*selection_window)
			{
				(*selection_window)->SetType(WIN_TYPE_PRINT_SELECTION);
				(*selection_window)->SetScale(window->GetScale());
				(*selection_window)->SetCSSMode(window->GetCSSMode());
				(*selection_window)->SetForceEncoding(window->GetForceEncoding());

				URL selectionURL = g_url_api->GetNewOperaURL();
				URL dummyURL;
				OpString title;

				OpStatus::Ignore(window->GetWindowTitle(title));

				selectionURL.Unload();
				selectionURL.ForceStatus(URL_LOADING);
				selectionURL.SetAttribute(URL::KMIME_ForceContentType, "text/html;charset=utf-16");

				(*selection_window)->OpenURL(selectionURL,DocumentReferrer(dummyURL),FALSE,FALSE,-1,FALSE);

				selectionURL.WriteDocumentData(URL::KNormal, UNI_L("<html>\n<head>\n<title>"));
				selectionURL.WriteDocumentData(URL::KHTMLify, title.CStr());
				selectionURL.WriteDocumentData(URL::KNormal, UNI_L("</title>\n</head>\n<body>\n"));

				uni_char* selText = OP_NEWA(uni_char, sel_text_len + 1);

				if (selText && doc->GetSelectedText(selText, sel_text_len + 1))
				{
					uni_char* start		= selText;
					uni_char* iterator	= selText;

					while (*iterator)
					{
						if (iterator[0] == '\r' && iterator[1] == '\n')
						{
							if (iterator[2] == '\r' && iterator[3] == '\n')
							{
								if ((iterator - start) > 0)
								{
									selectionURL.WriteDocumentData(URL::KHTMLify, start, (iterator - start));
									selectionURL.WriteDocumentData(URL::KNormal, UNI_L("<p>\n"));
								}

								iterator += 4;
								start = iterator;
							}
							else
							{
								if ((iterator - start) > 0)
								{
									selectionURL.WriteDocumentData(URL::KHTMLify, start, (iterator - start));
									selectionURL.WriteDocumentData(URL::KNormal, UNI_L("<br>\n"));
								}

								iterator += 2;
								start = iterator;
							}
						}
						else
						{
							iterator++;
						}
					}

					if ((iterator - start) > 0)
					{
						selectionURL.WriteDocumentData(URL::KHTMLify, start, (iterator - start));
					}
				}
				OP_DELETEA(selText);

				selectionURL.WriteDocumentData(URL::KNormal, UNI_L("\n</body>\n</html>"));
				selectionURL.WriteDocumentDataFinished();

#if defined MSWIN
				g_windows_message_loop->DispatchAllPostedMessagesNow();
#elif defined _UNIX_DESKTOP_
				X11OpMessageLoop::Self()->DispatchAllPostedMessagesNow();
#else
#error PRINT_SELECTION_USING_DUMMY_WINDOW seems to need DispatchAllPostedMessagesNow
#endif

				// make sure it has been reflowed

				if ((*selection_window)->GetCurrentDoc())
					((FramesDocument*)(*selection_window)->GetCurrentDoc())->Reflow(FALSE);
			}
		}
	}
	return;
}
#endif // PRINT_SELECTION_USING_DUMMY_WINDOW




#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
OP_STATUS PrinterInfo::GetPrintDevice(PrintDevice *&pdev, BOOL use_default, BOOL selection, Window* window, Window** selection_window)
{
	if (print_dev)
	{
		pdev = print_dev;
		return OpStatus::OK;
	}

#ifdef MSWIN
	HDC phdc = NULL;

	if (!use_default || !global_print_dlg.hDevMode)
	{
		HWND hWndParent = NULL;
		if(window)
		{
			hWndParent = static_cast<WindowsOpWindow*>(window->GetOpWindow())->m_hwnd;
		}

		if(!hWndParent)
		{
			DesktopWindow* activewindow = g_application->GetActiveDesktopWindow();
			if(activewindow && activewindow->GetOpWindow())
			{
				hWndParent = (HWND)static_cast<WindowsOpWindow*>(activewindow->GetOpWindow())->m_hwnd;
			}
			else
			{
				hWndParent = g_main_hwnd;
			}
		}

		if (!GetUserPrinterSettings( hWndParent, FALSE, use_default, selection))
		{
			return OpStatus::ERR; // Is this an OOM condition? (mstensho)
		}

		phdc = global_print_dlg.hDC;
		last_page = global_print_dlg.nToPage;

		if (global_print_dlg.Flags & PD_PAGENUMS)
		{
			last_page = global_print_dlg.nToPage;
		}
		else
		{
			last_page = USHRT_MAX;
		}
	}

	if (use_default && global_print_dlg.hDevMode && global_print_dlg.hDevNames)
	{
		DEVMODE* devmode = (DEVMODE*)GlobalLock(global_print_dlg.hDevMode);
		pdevmode = GlobalAlloc(GHND, (devmode->dmSize + devmode->dmDriverExtra));
		DEVMODE* new_devmode = (DEVMODE*)GlobalLock(pdevmode);

		if(new_devmode)
		{
			op_memcpy(new_devmode, devmode, devmode->dmSize + devmode->dmDriverExtra);
		}

		DEVNAMES* devnames = (DEVNAMES*)GlobalLock(global_print_dlg.hDevNames);
		size_t pdev_size = devnames->wOutputOffset +
			(uni_strlen((uni_char*)devnames + devnames->wOutputOffset) + 1 +
			 uni_strlen((uni_char*)devnames + devnames->wDriverOffset) + 1 +
			 uni_strlen((uni_char*)devnames + devnames->wDeviceOffset) + 1) *
			 sizeof(uni_char);
		pdevnames = GlobalAlloc(GHND, pdev_size);
		DEVNAMES* new_devnames = (DEVNAMES*)GlobalLock(pdevnames);
		*new_devnames = *devnames;
		uni_strcpy((uni_char*)new_devnames+new_devnames->wDriverOffset, (uni_char*)devnames+devnames->wDriverOffset);
		uni_strcpy((uni_char*)new_devnames+new_devnames->wDeviceOffset, (uni_char*)devnames+devnames->wDeviceOffset);
		uni_strcpy((uni_char*)new_devnames+new_devnames->wOutputOffset, (uni_char*)devnames+devnames->wOutputOffset);

		hdc = CreateDC((LPCTSTR)new_devnames+new_devnames->wDriverOffset, (LPCTSTR)new_devnames+new_devnames->wDeviceOffset, NULL, new_devmode);
		phdc = hdc;

		GlobalUnlock(pdevmode);
		GlobalUnlock(global_print_dlg.hDevMode);
		GlobalUnlock(pdevnames);
		GlobalUnlock(global_print_dlg.hDevNames);
	}

	if (phdc)
	{
		WindowsOpPrinterController* winPrinterController = OP_NEW(WindowsOpPrinterController, ());
		if (!winPrinterController)
		{
			return NULL;
		}
		winPrinterController->Init(phdc);
		winPrinterController->SetScale(g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrinterScale));

#ifdef PRINT_SELECTION_USING_DUMMY_WINDOW
		if (window && selection_window && global_print_dlg.Flags & PD_SELECTION && g_application->GetActiveDesktopWindow())
		{
			MakeSelectionWindow(window, selection_window);
		}

		global_print_dlg.Flags &= ~PD_SELECTION;

		print_dev = OP_NEW(PrintDevice, (selection_window && *selection_window ? (*selection_window)->DocManager() : window->DocManager()));
#else
		print_dev = OP_NEW(PrintDevice, (window->DocManager()));
#endif
		if (!print_dev)
		{
			OP_DELETE(winPrinterController);
			return OpStatus::ERR_NO_MEMORY;
		}
		if ((print_dev->Init(winPrinterController)) != OpStatus::OK)
		{
			OP_DELETE(winPrinterController);
			OP_DELETE(print_dev);
			print_dev = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}
#elif defined(POSTSCRIPT_PRINTING)
	PsOpPrinterController *ctrl
		= OP_NEW(PsOpPrinterController, (window->GetPreviewMode()));
	if (!ctrl)
		return OpStatus::ERR_NO_MEMORY;

	if (ctrl->Init() == OpStatus::ERR_NO_MEMORY)
	{
		OP_DELETE(ctrl);
		return OpStatus::ERR_NO_MEMORY;
	}

	int scale = g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrinterScale);
	ctrl->SetScale(scale);
	print_dev = OP_NEW(PrintDevice, (window->DocManager()));
	if (!print_dev)
	{
		OP_DELETE(ctrl);
		return OpStatus::ERR_NO_MEMORY;
	}
	if ((print_dev->Init(ctrl)) != OpStatus::OK)
	{
		OP_DELETE(ctrl);
		OP_DELETE(print_dev);
		print_dev = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	print_dev->SetScale(scale, FALSE);
#elif defined(GENERIC_PRINTING)
	OpPrinterController * printerController;
	OP_STATUS ret = OpPrinterController::Create(&printerController, use_default);
	if (ret != OpStatus::OK)
		return ret;

	OpWindow * opw = NULL;
	if (window)
		opw = const_cast<OpWindow *>(window->GetOpWindow());
	if (opw)
		opw = opw->GetRootWindow();
	
	OP_STATUS res = printerController->Init(opw);
	if (res != OpStatus::OK)
	{
		OP_DELETE(printerController);
		return res;
	}

#ifdef PI_PRINT_SELECTION
	if (window && selection_window && printerController->GetPrintSelectionOnly() && g_application->GetActiveDesktopWindow())
		MakeSelectionWindow(window, selection_window);
#endif
	
	if (selection_window && *selection_window)
		print_dev = OP_NEW(PrintDevice, ((*selection_window)->DocManager()));
	else if(window)
		print_dev = OP_NEW(PrintDevice, (window->DocManager()));
	if (!print_dev)
	{
		OP_DELETE(printerController);
		return OpStatus::ERR_NO_MEMORY;
	}
	if ((print_dev->Init(printerController)) != OpStatus::OK)
	{
		OP_DELETE(printerController);
		OP_DELETE(print_dev);
		print_dev = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	print_dev->SetScale(g_pcprint->GetIntegerPref(PrefsCollectionPrint::PrinterScale), FALSE);

	print_listener = OP_NEW(PrintListener, (print_dev));
	if (!print_listener)
	{
		OP_DELETE(printerController);
		OP_DELETE(print_dev);
		print_dev = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	printerController->SetPrintListener(print_listener);
#endif

	pdev = print_dev;
	return OpStatus::OK;
}

#else // PRINT_SELECTION_USING_DUMMY_WINDOW

OP_STATUS PrinterInfo::GetPrintDevice(PrintDevice *&pdev, BOOL use_default, BOOL selection, Window* window)
{
	if (print_dev)
	{
		pdev = print_dev;
		return OpStatus::OK;
	}
	OpPrinterController * printerController;
	OP_STATUS ret = OpPrinterController::Create(&printerController, use_default);
	if (ret != OpStatus::OK)
		return ret;

#if defined(_MACINTOSH_)
	if (!use_default)
	{
		if (!MacOpPrinterController::GetPrintDialog(NULL, window?window->GetWindowTitle():NULL))
		{
			return OpStatus::ERR;
		}
	}

	next_page = MacOpPrinterController::GetSelectedStartPage();
	last_page = MacOpPrinterController::GetSelectedEndPage();
#endif
	// Init must set default printer scale
	OpWindow * opw = NULL;
	if (window)
		opw = const_cast<OpWindow *>(window->GetOpWindow());
	if (opw)
		opw = opw->GetRootWindow();
	
	OP_STATUS res = printerController->Init(opw);
	if ( res != OpStatus::OK)
	{
		OP_DELETE(printerController);
		return res;
	}
	// By uncommenting this, the gui can not initialize a scale,
	// as that will be overwritten by the call to SetScale
	//printerController->SetScale(prefsManager->GetPrinterScale());
	print_dev = window ? OP_NEW(PrintDevice, (window->DocManager())) : NULL;
	if (!print_dev)
	{
		OP_DELETE(printerController);
		return OpStatus::ERR_NO_MEMORY;
	}
	if ((print_dev->Init(printerController)) != OpStatus::OK)
	{
		OP_DELETE(printerController);
		OP_DELETE(print_dev);
		print_dev = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	pdev = print_dev;
	print_listener = OP_NEW(PrintListener, (pdev));
	if (!print_listener)
	{
		OP_DELETE(printerController);
		OP_DELETE(print_dev);
		print_dev = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	printerController->SetPrintListener(print_listener);
	return OpStatus::OK;

}

#endif // PRINT_SELECTION_USING_DUMMY_WINDOW


#ifndef WIN32

LONG ucPrintDocFormatted()
{
	return 0;
}

LONG ucPrintPagePrinted(WORD page)
{
	return 0;
}

LONG ucPrintingFinished(Window* window)
{
	return 0;
}

LONG ucPrintingAborted(Window* window)
{
	return 0;
}

#endif

#endif // _PRINT_SUPPORT_
