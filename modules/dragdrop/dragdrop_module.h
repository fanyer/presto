/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef DRAGDROP_MODULE_H
# define DRAGDROP_MODULE_H

# ifdef DRAG_SUPPORT
class DragDropManager;
# endif // DRAG_SUPPORT

# ifdef USE_OP_CLIPBOARD
class ClipboardManager;
# endif // USE_OP_CLIPBOARD

class DragdropModule : public OperaModule
{
public:
	DragdropModule();

	virtual void		InitL(const OperaInitInfo& info);
	virtual void		Destroy();

# ifdef DRAG_SUPPORT
	/**
	 * An instance of the drag-drop manager.
	 * @see DragDropManager
	 */
	DragDropManager*	m_dragdrop_manager;
# endif // DRAG_SUPPORT
# ifdef USE_OP_CLIPBOARD
	/**
	 * An instance of the clipboard manager.
	 * @see ClipboardManager
	 */
	ClipboardManager* m_clipboard_manager;
# endif // USE_OP_CLIPBOARD
};

# ifdef DRAG_SUPPORT
#  define g_drag_manager g_opera->dragdrop_module.m_dragdrop_manager
# endif // DRAG_SUPPORT

# ifdef USE_OP_CLIPBOARD
#  define g_clipboard_manager g_opera->dragdrop_module.m_clipboard_manager
# endif // USE_OP_CLIPBOARD

# define DRAGDROP_MODULE_REQUIRED

#endif // !DRAGDROP_MODULE_H
