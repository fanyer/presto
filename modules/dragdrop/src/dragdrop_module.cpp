/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/dragdrop/dragdrop_module.h"
#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
# include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

DragdropModule::DragdropModule()
#if defined DRAG_SUPPORT || defined USE_OP_CLIPBOARD
	:
#ifdef DRAG_SUPPORT
	m_dragdrop_manager(NULL),
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
	m_clipboard_manager(NULL)
#endif // USE_OP_CLIPBOARD
#endif // DRAG_SUPPORT || USE_OP_CLIPBOARD
{
}

/* virtual */ void
DragdropModule::InitL(const OperaInitInfo& info)
{
#ifdef DRAG_SUPPORT
	m_dragdrop_manager = OP_NEW_L(DragDropManager, ());
	LEAVE_IF_ERROR(m_dragdrop_manager->Initialize());
#endif // DRAG_SUPPORT
#ifdef USE_OP_CLIPBOARD
	m_clipboard_manager = OP_NEW_L(ClipboardManager, ());
	LEAVE_IF_ERROR(m_clipboard_manager->Initialize());
#endif // USE_OP_CLIPBOARD
}

/* virtual */ void
DragdropModule::Destroy()
{
#ifdef USE_OP_CLIPBOARD
	OP_DELETE(m_clipboard_manager);
	m_clipboard_manager = NULL;
#endif // USE_OP_CLIPBOARD
#ifdef DRAG_SUPPORT
	OP_DELETE(m_dragdrop_manager);
	m_dragdrop_manager = NULL;
#endif // DRAG_SUPPORT
}
