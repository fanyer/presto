/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Espen Sand
 */
#include "core/pch.h"

#include "platforms/unix/base/x11/x11_opbitmap.h"
#include "platforms/unix/base/x11/x11_opclipboard.h"
#include "platforms/unix/base/x11/x11_opfilechooser.h"
#include "platforms/unix/base/x11/x11_opmessageloop.h"
#include "platforms/unix/base/x11/x11_opscreeninfo.h"
#include "platforms/unix/base/x11/x11_opsysteminfo.h"
#include "platforms/unix/base/x11/x11_opwindow.h"
#include "platforms/unix/base/x11/x11_globals.h"

#include "platforms/unix/product/x11quick/x11_global_desktop_application.h"
#include "platforms/unix/product/x11quick/x11_desktop_popup_menu_handler.h"

#include "platforms/x11api/plugins/plugin_unified_window.h"
#include "platforms/quix/environment/DesktopEnvironment.h"
#include "platforms/quix/desktop_pi_impl/UnixDesktopColorChooser.h"
#include "platforms/quix/desktop_pi_impl/UnixDesktopFileChooser.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"

#include "adjunct/desktop_pi/DesktopColorChooser.h"
#include "adjunct/desktop_pi/DesktopOpView.h"
#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/quick_toolkit/widgets/OpNotifier.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
# include "modules/pi/OpAccessibilityAdapter.h"
#endif

#include "adjunct/desktop_pi/DesktopMultimediaPlayer.h"
#include "modules/pi/OpPluginWindow.h"


OP_STATUS OpSystemInfo::Create(OpSystemInfo **new_obj)
{
	OpAutoPtr<X11OpSystemInfo> x11_op_system_info(OP_NEW(X11OpSystemInfo, ()));
	if (!x11_op_system_info.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(x11_op_system_info->Construct());

	*new_obj = x11_op_system_info.release();

	return OpStatus::OK;
}

#ifndef VEGA_OPPAINTER_SUPPORT
OP_STATUS OpFontManager::Create(OpFontManager **new_obj)
{
	*new_obj = OP_NEW(X11OpFontManager, ());
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS res = ((X11OpFontManager *)*new_obj)->Init();
	if (OpStatus::IsError(res))
	{
		OP_DELETE(new_obj);
		new_obj = 0;
	}
	return res;
}

OP_STATUS OpBitmap::Create(OpBitmap **new_obj, UINT32 width, UINT32 height,
	BOOL transparent, BOOL alpha, UINT32 transpcolor, INT32 indexed,
	BOOL must_support_painter)
{
	X11OpBitmap* local = OP_NEW(X11OpBitmap, (transparent, alpha, transpcolor));

	OP_STATUS result = local ? local->Construct(width, height, true) : OpStatus::ERR_NO_MEMORY;
	if (local && OpStatus::IsError(result))
	{
		OP_DELETE(local);
		*new_obj = 0;
	}
	else
		*new_obj = local;

	return result;
}
#endif // !VEGA_OPPAINTER_SUPPORT

OP_STATUS DesktopOpView::Create(OpView **new_obj, OpWindow *parentwindow)
{
	*new_obj = OP_NEW(X11OpView, ());
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS result = ((X11OpView *)*new_obj)->Init(parentwindow);
	if (OpStatus::IsError(result))
	{
		OP_DELETE(*new_obj);
		*new_obj = 0;
	};
	return result;
}

OP_STATUS DesktopOpView::Create(OpView **new_obj, OpView *parentview)
{
	*new_obj = OP_NEW(X11OpView, ());
	if (!*new_obj)
		return OpStatus::ERR_NO_MEMORY;
	OP_STATUS result = ((X11OpView *)*new_obj)->Init(parentview);
	if (OpStatus::IsError(result))
	{
		OP_DELETE(*new_obj);
		*new_obj = 0;
	};
	return result;
}

OP_STATUS DesktopOpWindow::Create(DesktopOpWindow **new_obj)
{
	*new_obj = OP_NEW(X11OpWindow, ());
	return *new_obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS OpScreenInfo::Create(OpScreenInfo **new_obj)
{
	*new_obj = OP_NEW(X11OpScreenInfo, ());
	return *new_obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS OpUiInfo::Create(OpUiInfo **new_obj)
{
	*new_obj = OP_NEW(X11OpUiInfo, ());
	return *new_obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS OpClipboard::Create(OpClipboard **new_obj)
{
	if (X11OpClipboard::s_x11_op_clipboard)
	{
		OP_ASSERT(!"OpClipboard already exists");
		return OpStatus::ERR;
	}
	X11OpClipboard::s_x11_op_clipboard = OP_NEW(X11OpClipboard, ());
	RETURN_OOM_IF_NULL(X11OpClipboard::s_x11_op_clipboard);
	OP_STATUS rc = X11OpClipboard::s_x11_op_clipboard->Init();
	if (OpStatus::IsError(rc))
	{
		OP_DELETE(X11OpClipboard::s_x11_op_clipboard);
		X11OpClipboard::s_x11_op_clipboard = NULL;
	}
	*new_obj = X11OpClipboard::s_x11_op_clipboard;
	return rc;
}

#if !defined WIDGETS_CAP_WIC_FILESELECTION || !defined WIC_CAP_FILESELECTION
OP_STATUS OpFileChooser::Create(OpFileChooser **new_obj)
{
	*new_obj = OP_NEW(X11OpFileChooser, ());
	return *new_obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}
#endif // !defined WIDGETS_CAP_WIC_FILESELECTION || !defined WIC_CAP_FILESELECTION

OP_STATUS OpColorChooser::Create(OpColorChooser **new_obj)
{
	*new_obj = OP_NEW(UnixDesktopColorChooser, (g_toolkit_library->CreateColorChooser()));

	return *new_obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;	
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
// Implementation in quix/
OP_STATUS OpAccessibilityAdapter::Create(OpAccessibilityAdapter** adapter,
										 OpAccessibleItem* accessible_item)
{
	*adapter = 0;
	return OpStatus::OK;
}

OP_STATUS OpAccessibilityAdapter::SendEvent(OpAccessibleItem* sender, Accessibility::Event evt)
{
	return OpStatus::OK;
}
#endif // ACCESSIBILITY_EXTENSION_SUPPORT

OP_STATUS DesktopFileChooser::Create(DesktopFileChooser** newObj)
{
	*newObj = OP_NEW(UnixDesktopFileChooser, (g_toolkit_library->CreateFileChooser()));

	return *newObj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS OpMultimediaPlayer::Create(OpMultimediaPlayer** newObj)
{
#if 0
	*newObj = OP_NEW(UnixOpMultimediaPlayer, ());
	return *newObj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
#else
	OP_ASSERT(!"Sorry, no media player");
	return OpStatus::ERR;
#endif
}

OP_STATUS DesktopGlobalApplication::Create(DesktopGlobalApplication** desktop_application)
{
	*desktop_application = OP_NEW(X11DesktopGlobalApplication, ());
	if (!*desktop_application)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

OP_STATUS DesktopPopupMenuHandler::Create(DesktopPopupMenuHandler** menu_handler)
{
	*menu_handler = OP_NEW(X11DesktopPopupMenuHandler, ());
	if (!*menu_handler)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}


OP_STATUS DesktopNotifier::Create(DesktopNotifier** new_notifier)
{
	// FIXME: We might want our own version of OpNotifier
	*new_notifier = OP_NEW(OpNotifier,());
	if (!*new_notifier)
		return OpStatus::ERR_NO_MEMORY;
	return OpStatus::OK;
}

#ifdef M2_SUPPORT
bool DesktopNotifier::UsesCombinedMailAndFeedNotifications()
{
	return true;
}
#endif // M2_SUPPORT
