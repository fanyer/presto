/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/widgets/OpWidget.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpTextCollection.h"
#ifdef SKIN_SUPPORT
# include "modules/skin/IndpWidgetPainter.h"
#endif
#include "modules/widgets/OpDropDown.h"

void LockedDropDownCloser::CloseLockedDropDowns()
{
	const UINT32 locked_dd_count = locked_dropdowns.GetCount();
	if (locked_dd_count > 0)
	{
		for (UINT32 i = 0; i < locked_dd_count; ++i)
		{
			locked_dropdowns.Get(i)->ClosePopup(TRUE);
		}
		locked_dropdowns.Clear();
	}
}

WidgetsModule::WidgetsModule()
	: OperaModule() // dummy, so what's left without #if is still valid
	, tcinfo(0)
	, widgetpaintermanager(0)
#ifdef WIDGETS_IME_SUPPORT
	, im_listener(0)
#endif // WIDGETS_IME_SUPPORT
	, widget_globals(0)
	, m_delete_lock_count(0)
	, m_has_failed_to_post_delete_message(FALSE)
{
}

void
WidgetsModule::InitL(const OperaInitInfo& info)
{
	OpWidget::InitializeL();
	widgetpaintermanager = OP_NEW_L(OpWidgetPainterManager, ());
	tcinfo = OP_NEW_L(OP_TCINFO, ());

#ifdef WIDGETS_IME_SUPPORT
	im_listener = OP_NEW_L(WidgetInputMethodListener, ());
	im_spawning = FALSE;
#endif // WIDGETS_IME_SUPPORT

#if defined(SKIN_SUPPORT)
	widgetpaintermanager->SetPrimaryWidgetPainter(OP_NEW_L(IndpWidgetPainter, ()));
#endif // SKIN_SUPPORT
}

void
WidgetsModule::Destroy()
{
#ifdef WIDGETS_IME_SUPPORT
	OP_DELETE(im_listener);
	im_listener = NULL;
#endif // WIDGETS_IME_SUPPORT

	OP_DELETE(tcinfo);
	OP_DELETE(widgetpaintermanager);
	widgetpaintermanager = NULL;

    if (!m_deleted_widgets.Empty())
    {
        ClearDeleteWidgetsMessage();
        m_deleted_widgets.Clear();
    }
    OP_ASSERT(!g_main_message_handler->HasCallBack(this, MSG_DELETE_WIDGETS, (MH_PARAM_1)this));

	OpWidget::Free();
}

void WidgetsModule::AddExternalListener(OpWidgetExternalListener *listener)
{
	listener->Into(&external_listeners);
}

void WidgetsModule::RemoveExternalListener(OpWidgetExternalListener *listener)
{
	listener->Out();
}


void WidgetsModule::PostDeleteWidget(OpWidget* widget)
{
    if (!m_delete_lock_count && (m_deleted_widgets.Empty() || m_has_failed_to_post_delete_message))
        PostDeleteWidgetsMessage();
    widget->Into(&m_deleted_widgets);

    OP_ASSERT(m_delete_lock_count || g_main_message_handler->HasCallBack(this, MSG_DELETE_WIDGETS, (MH_PARAM_1)this) || m_has_failed_to_post_delete_message);
}

// virtual
void WidgetsModule::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    switch (msg)
    {
	case MSG_DELETE_WIDGETS:
        OP_ASSERT(!m_delete_lock_count);
        OP_ASSERT(!m_deleted_widgets.Empty());

		m_deleted_widgets.Clear();
        g_main_message_handler->UnsetCallBack(this, MSG_DELETE_WIDGETS, par1);
        break;
    }
}
void WidgetsModule::PostDeleteWidgetsMessage()
{
    MH_PARAM_1 p1 = (MH_PARAM_1)this;

    OP_ASSERT(!g_main_message_handler->HasCallBack(this, MSG_DELETE_WIDGETS, p1));
    OP_ASSERT(!m_delete_lock_count);

    m_has_failed_to_post_delete_message = FALSE;
    if (OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_DELETE_WIDGETS, p1)))
        m_has_failed_to_post_delete_message = TRUE;
    else if (!g_main_message_handler->PostMessage(MSG_DELETE_WIDGETS, p1, 0))
    {
        m_has_failed_to_post_delete_message = TRUE;
        g_main_message_handler->UnsetCallBack(this, MSG_DELETE_WIDGETS, p1);
    }
}
void WidgetsModule::ClearDeleteWidgetsMessage()
{
    MH_PARAM_1 p1 = (MH_PARAM_1)this;

    OP_ASSERT(g_main_message_handler->HasCallBack(this, MSG_DELETE_WIDGETS, p1));

    m_has_failed_to_post_delete_message = FALSE;
    g_main_message_handler->RemoveDelayedMessage(MSG_DELETE_WIDGETS, p1, 0);
    g_main_message_handler->UnsetCallBack(this, MSG_DELETE_WIDGETS, p1);
}

LockDeletedWidgetsCleanup::LockDeletedWidgetsCleanup()
{
    WidgetsModule* wm = &g_opera->widgets_module;
    ++ wm->m_delete_lock_count;
    if (wm->m_delete_lock_count == 1 && !wm->m_deleted_widgets.Empty())
        wm->ClearDeleteWidgetsMessage();

    OP_ASSERT(!g_main_message_handler->HasCallBack(wm, MSG_DELETE_WIDGETS, (MH_PARAM_1)wm));
}
LockDeletedWidgetsCleanup::~LockDeletedWidgetsCleanup()
{
    WidgetsModule* wm = &g_opera->widgets_module;

    OP_ASSERT(wm->m_delete_lock_count);
    OP_ASSERT(!g_main_message_handler->HasCallBack(wm, MSG_DELETE_WIDGETS, (MH_PARAM_1)wm));

    -- wm->m_delete_lock_count;
    if (wm->m_delete_lock_count == 0 && !wm->m_deleted_widgets.Empty())
        wm->PostDeleteWidgetsMessage();

    OP_ASSERT(g_main_message_handler->HasCallBack(wm, MSG_DELETE_WIDGETS, (MH_PARAM_1)wm) ==
              (!wm->m_delete_lock_count && !wm->m_deleted_widgets.Empty()));
}

