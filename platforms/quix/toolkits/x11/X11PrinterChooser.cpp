/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Espen Sand
 */

#include "core/pch.h"

#include "X11PrinterChooser.h"

#include "platforms/quix/dialogs/PrintDialog.h"
#include "platforms/quix/toolkits/ToolkitPrinterHelper.h"
#include "platforms/unix/base/x11/x11utils.h"
#include "platforms/posix/posix_native_util.h"

#include "modules/util/opfile/opfile.h"


X11ToolkitPrinterIntegration::X11ToolkitPrinterIntegration()
	:m_helper(NULL)
	,m_accepted(false)
{
}


X11ToolkitPrinterIntegration::~X11ToolkitPrinterIntegration()
{
	if (m_helper)
		m_state.Reset(m_helper);
}


bool X11ToolkitPrinterIntegration::Init(ToolkitPrinterHelper* helper)
{
	m_helper = helper;
	if (m_helper)
		return OpStatus::IsSuccess(m_state.Init(m_helper));
	else
		return true;
}


bool X11ToolkitPrinterIntegration::RunPrintDialog(X11Types::Window parent)
{
	m_accepted = false;

	PrintDialog* dialog = OP_NEW(PrintDialog,(this, m_state));
	if (!dialog)
		return false;

	// This functions does not return until after dialog is closed
	dialog->Init(X11Utils::GetDesktopWindowFromX11Window(parent));

	return m_accepted;
}

bool X11ToolkitPrinterIntegration::Print(const char* print_file_path, const char* print_job_name)
{
	if (m_state.is_file)
	{
		if (m_state.file.HasContent())
		{
			OpString src_filename;
			RETURN_VALUE_IF_ERROR(PosixNativeUtil::FromNative(print_file_path, &src_filename), false);
			if (src_filename.HasContent())
			{
				OpFile src_file;
				src_file.Construct(src_filename);

				OpFile dest_file;
				dest_file.Construct(m_state.file);

				if (m_state.file.Compare(src_filename))
				{
					RETURN_VALUE_IF_ERROR(dest_file.CopyContents(&src_file, FALSE), false);
					src_filename.Delete(FALSE);
				}
			}
		}
		return true;
	}

	OpString8 printer_name;
	RETURN_VALUE_IF_ERROR(PosixNativeUtil::ToNative(m_state.printer_name, &printer_name), false);

	if (!m_helper->SetPrinter(printer_name.CStr()))
		return false;

	m_helper->SetCopies(m_state.copies);

	return m_helper->PrintFile(print_file_path, print_job_name);
}

bool X11ToolkitPrinterIntegration::PageShouldBePrinted(int page_number)
{
	if (m_state.range == PrintDialog::PrinterState::PrintPages)
		return page_number >= m_state.from && page_number <= m_state.to;
	else if (m_state.range == PrintDialog::PrinterState::PrintAll || 
			 m_state.range == PrintDialog::PrinterState::PrintSelection)
		return true;
	return false;
}

bool X11ToolkitPrinterIntegration::GetPrinterResolution(int& horizontal, int& vertical)
{
	horizontal = m_state.horizontal_dpi;
	vertical = m_state.vertical_dpi;
	return true;
}

bool X11ToolkitPrinterIntegration::GetPrintScale(int& scale)
{
	scale = m_state.scale;
	return true;
}

bool X11ToolkitPrinterIntegration::GetPaperSize(double& width, double& height)
{
	width  = (double)(m_state.media_width_mm) / 25.4;
	height = (double)(m_state.media_height_mm) / 25.4;

	if (!m_state.portrait)
	{
		double tmp = width;
		width  = height;
		height = tmp;
	}

	return true;

}

bool X11ToolkitPrinterIntegration::GetMargins(double& top, double& left, double& bottom, double& right)
{
	top = (double)(m_state.top_dmm) / 254;
	left = (double)(m_state.left_dmm) / 254;
	bottom = (double)(m_state.bottom_dmm) / 254;
	right = (double)(m_state.right_dmm) / 254;
	return true;
}


bool X11ToolkitPrinterIntegration::GetPrintSelectionOnly()
{
	return m_state.range == PrintDialog::PrinterState::PrintSelection;
}
