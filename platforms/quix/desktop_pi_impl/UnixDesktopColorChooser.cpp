/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#include "core/pch.h"

#include "platforms/quix/desktop_pi_impl/UnixDesktopColorChooser.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"
#include "platforms/quix/toolkits/ToolkitColorChooser.h"
#include "platforms/unix/base/x11/x11_dialog.h"
#include "platforms/unix/base/x11/x11_globals.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_widget.h"
#include "platforms/unix/base/x11/x11_widgetlist.h"


BOOL UnixDesktopColorChooser::m_color_chooser_dialog_is_active = FALSE;


UnixDesktopColorChooser::UnixDesktopColorChooser(
										ToolkitColorChooser *toolkit_chooser)
	: m_chooser(toolkit_chooser)
{
	OP_ASSERT(m_chooser);
}


UnixDesktopColorChooser::~UnixDesktopColorChooser()
{
	OP_DELETE(m_chooser);
}


OP_STATUS UnixDesktopColorChooser::Show(COLORREF initial_color,
										OpColorChooserListener *listener,
										DesktopWindow *parent)
{
	X11Dialog::PrepareForShowingDialog(false);

	OP_ASSERT(listener);
	OP_ASSERT(parent);
	if(!m_chooser || m_color_chooser_dialog_is_active)
		return OpStatus::ERR;

	X11Widget *widget = NULL;
	X11Types::Window x11_parent_id = 0;
	if(parent)
	{
		// same trick in UnixDesktopFileChooser
		X11OpWindow* native_window = static_cast<X11OpWindow*>(parent->GetOpWindow());
		if(native_window && !native_window->IsToplevel())
			native_window = native_window->GetNativeWindow();
		widget = native_window->GetOuterWidget();
		x11_parent_id = widget ? widget->GetWindowHandle() : 0;
	}

	// set our brand new dialog to be modal for it's parent:
	if (g_toolkit_library->BlockOperaInputOnDialogs())
		g_x11->GetWidgetList()->SetModalToolkitParent(widget);

	m_color_chooser_dialog_is_active = TRUE;
	BOOL ok = m_chooser->Show(x11_parent_id, (UINT32)initial_color);
	m_color_chooser_dialog_is_active = FALSE;
	if (ok)
		listener->OnColorSelected((COLORREF)m_chooser->GetColor());

	// disable modality
	g_x11->GetWidgetList()->SetModalToolkitParent(NULL);

	return OpStatus::OK;
}

