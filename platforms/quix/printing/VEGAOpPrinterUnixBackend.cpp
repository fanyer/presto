/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "platforms/quix/printing/VEGAOpPrinterUnixBackend.h"
#include "platforms/quix/printing/CupsHelper.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/quix/toolkits/ToolkitPrinterIntegration.h"

#include "platforms/posix/posix_native_util.h"
#include "platforms/unix/base/x11/x11_dialog.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"
#include "platforms/unix/base/x11/x11_windowwidget.h"
#include "modules/util/opfile/opfile.h"

#include "modules/locale/oplanguagemanager.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"



OP_STATUS VEGAOpPrinterBackend::Create(VEGAOpPrinterBackend** new_printerbackend)
{
	OP_ASSERT(new_printerbackend != NULL);
	*new_printerbackend = new VEGAOpPrinterUnixBackend();
	if (*new_printerbackend == NULL)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}


OP_STATUS VEGAOpPrinterUnixBackend::Init()
{
	if (!g_toolkit_library)
		return OpStatus::ERR_NULL_POINTER;

	// Create native platform printer integration
	m_printer_integration = g_toolkit_library->CreatePrinterIntegration();
	if (!m_printer_integration.get())
		return OpStatus::ERR_NO_MEMORY;

	CupsHelper* helper = NULL;
	if (m_printer_integration->NeedsPrinterHelper())
	{
		helper = OP_NEW(CupsHelper, ());
		if (!helper)
			return OpStatus::ERR_NO_MEMORY;

		helper->Init();

		if (!helper->HasConnection() && !m_printer_integration->SupportsOfflinePrinting())
		{
			// libcups centric error message

			OpString title;
			TRAPD(rc,g_languageManager->GetStringL(Str::D_PRINT_DIALOG_FAILURE, title));

			OpString msg, tmp;
			TRAP(rc,g_languageManager->GetStringL(Str::D_PRINT_DIALOG_LIBRARY_ERROR, msg));
			TRAP(rc,g_languageManager->GetStringL(Str::D_GENERIC_COULD_NOT_LOAD, tmp));
			RETURN_IF_ERROR(msg.Append("\n\n"));
			RETURN_IF_ERROR(msg.Append(tmp));
			RETURN_IF_ERROR(msg.Append("  "));
			RETURN_IF_ERROR(msg.Append(helper->GetLibraryName()));
			SimpleDialog::ShowDialog(WINDOW_NAME_PRINT_FAIL, NULL, msg.CStr(), title.CStr(),Dialog::TYPE_OK, Dialog::IMAGE_ERROR);

			return OpStatus::ERR;
		}
	}

	if (!m_printer_integration->Init(helper))
		return OpStatus::ERR;

	OpString8 caption;
	OpString caption_string;
	TRAPD(rc, g_languageManager->GetStringL(Str::DI_QT_PRINT_DIALOGTITLE, caption_string));
	RETURN_IF_ERROR(caption.SetUTF8FromUTF16(caption_string));
	m_printer_integration->SetCaption(caption);

	return OpStatus::OK;
}


OP_STATUS VEGAOpPrinterUnixBackend::PreparePrinting()
{
	if (!m_printer_integration.get())
		return OpStatus::ERR;

	X11Widget* parent = X11WindowWidget::GetActiveToplevelWidget();
	if (!parent)
		return OpStatus::ERR;

	if (g_toolkit_library->BlockOperaInputOnDialogs())
		g_x11->GetWidgetList()->SetModalToolkitParent(parent);
	g_x11->GetWidgetList()->SetCursor(CURSOR_DEFAULT_ARROW);

	// Run native printer dialog. If user cancels (false from run) we return ERR to cancel everything.

	X11Dialog::PrepareForShowingDialog(false);
	bool ok = m_printer_integration->RunPrintDialog(parent->GetWindowHandle());

	g_x11->GetWidgetList()->SetModalToolkitParent(0);
	g_x11->GetWidgetList()->RestoreCursor();

	return ok ? OpStatus::OK : OpStatus::ERR;
}


OP_STATUS VEGAOpPrinterUnixBackend::GetPrinterResolution(UINT16& horizontal, UINT16& vertical)
{
	if (!m_printer_integration.get())
		return OpStatus::ERR;

	int h, v;
	if (!m_printer_integration->GetPrinterResolution(h, v))
		return OpStatus::ERR;

	horizontal = h;
	vertical = v;

	return OpStatus::OK;
}


OP_STATUS VEGAOpPrinterUnixBackend::GetPrintScale(UINT32& scale)
{
	if (!m_printer_integration.get())
		return OpStatus::ERR;

	int print_scale;
	if (!m_printer_integration->GetPrintScale(print_scale))
		return OpStatus::ERR;

	scale = roundl(print_scale);

	return OpStatus::OK;
}


OP_STATUS VEGAOpPrinterUnixBackend::GetPaperSize(double& width, double& height)
{
	if (!m_printer_integration.get())
		return OpStatus::ERR;

	if (!m_printer_integration->GetPaperSize(width, height))
		return OpStatus::ERR;

	return OpStatus::OK;
}


OP_STATUS VEGAOpPrinterUnixBackend::GetMargins(double& top, double& left, double& bottom, double& right)
{
	if (!m_printer_integration.get())
		return OpStatus::ERR;

	if (!m_printer_integration->GetMargins(top, left, bottom, right))
		return OpStatus::ERR;

	return OpStatus::OK;
}


bool VEGAOpPrinterUnixBackend::GetPrintSelectionOnly()
{
	return m_printer_integration.get() && m_printer_integration->GetPrintSelectionOnly();
}


OP_STATUS VEGAOpPrinterUnixBackend::Print(OpFile& print_file, OpString& print_job_name)
{
	if (!m_printer_integration.get())
		return OpStatus::ERR;

	PosixNativeUtil::NativeString native_print_file(print_file.GetFullPath());
	PosixNativeUtil::NativeString native_job_name(print_job_name);

	OP_STATUS rc = m_printer_integration->Print(native_print_file.get(), native_job_name.get());

	print_file.Delete();

	return rc;
}


bool VEGAOpPrinterUnixBackend::PageShouldBePrinted(int page_number)
{
	if (!m_printer_integration.get())
		return false;

	return m_printer_integration->PageShouldBePrinted(page_number);
}




