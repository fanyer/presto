// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Author: Petter Nilsen <pettern@opera.com>
//
#include "core/pch.h"

#include "modules/util/OpTypedObject.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "adjunct/quick/managers/ToolbarManager.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/widgets/OpPersonalbar.h"
#include "adjunct/quick/widgets/DownloadExtensionBar.h"
#include "adjunct/quick/widgets/InstallPersonabar.h"
#include "adjunct/quick/panels/HotlistPanel.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

ToolbarManager::ToolbarManager()
{

}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////

ToolbarManager::~ToolbarManager()
{
	m_toolbars.DeleteAll();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

OpRect ToolbarManager::LayoutToAvailableRect(ToolbarType toolbar_type, OpWidget *root_widget, const OpRect& rect, BOOL compute_rect_only)
{
	OpToolbar *toolbar = FindToolbar(toolbar_type, root_widget);
	if(toolbar)
	{
		return toolbar->LayoutToAvailableRect(rect, compute_rect_only);
	}
	return rect;
}

BOOL ToolbarManager::SetAlignment(ToolbarType toolbar_type, OpWidget *root_widget, OpBar::Alignment alignment)
{
	OpToolbar *toolbar = FindToolbar(toolbar_type, root_widget);
	if(toolbar)
	{
		return toolbar->SetAlignment(alignment);
	}
	return FALSE;
}

template <typename ToolbarClass>
ToolbarClass* ToolbarManager::ConstructToolbar()
{
	ToolbarClass *toolbar;
	RETURN_VALUE_IF_ERROR(ToolbarClass::Construct(&toolbar), NULL);
	return toolbar;
}

OpToolbar* ToolbarManager::CreateToolbar(ToolbarType type, OpWidget *root_widget)
{
	OP_STATUS s;
	OpAutoPtr<OpToolbar> toolbar;

	switch(type)
	{
		case PersonalbarInline:
		{
			toolbar = ConstructToolbar< OpPersonalbarInline >();
			break;
		}
		case Personalbar:
		{
			toolbar = ConstructToolbar< OpPersonalbar >();
			break;
		}
		case InstallPersonabar:
		{
			toolbar = ConstructToolbar< InstallPersonaBar >();
			break;
		}
		case DownloadExtensionbar:
		{
			toolbar = ConstructToolbar< DownloadExtensionBar >();
			break;
		}
		default:
			OP_ASSERT(!"Called with unknown type!");
			break;
	}
	if(toolbar.get())
	{
		s = RegisterToolbar(type, root_widget, toolbar.get());
		RETURN_VALUE_IF_ERROR(s, NULL);

		root_widget->AddChild(toolbar.get());

		return toolbar.release();
	}
	return NULL;
}

BOOL ToolbarManager::HandleAction(OpWidget *root_widget, OpInputAction *action)
{
	switch(action->GetAction())
	{
		case OpInputAction::ACTION_FOCUS_PERSONAL_BAR:
		{
			OpToolbar* toolbar = FindToolbar(PersonalbarInline, root_widget, FALSE);
			if(!toolbar)
			{
				toolbar = FindToolbar(Personalbar, root_widget, FALSE);
			}
			if(toolbar)
			{
				root_widget->SetHasFocusRect(TRUE);
				toolbar->SetFocus(FOCUS_REASON_ACTION);
//				g_input_manager->RestoreKeyboardInputContext(toolbar, NULL, FOCUS_REASON_ACTION);
				return TRUE;
			}
		}
	}
	return FALSE;
}
OP_STATUS ToolbarManager::RegisterToolbar(ToolbarType toolbar_type, OpWidget *root_widget, OpToolbar *toolbar)
{
	OpAutoPtr<ToolbarManagerItem> item(OP_NEW(ToolbarManagerItem, (toolbar_type, root_widget, toolbar)));
	if(!item.get())
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	RETURN_IF_ERROR(m_toolbars.Add(item.get()));

	item.release();

	return OpStatus::OK;
}

void ToolbarManager::UnregisterToolbars(OpWidget *root_widget)
{
	UINT32 n;

	for(n = 0; n < m_toolbars.GetCount(); n++)
	{
		ToolbarManagerItem *item = m_toolbars.Get(n);
		if(item->m_root_widget == root_widget)
		{
			m_toolbars.Remove(n);
			OP_DELETE(item);
			n--;
		}
	}
}

OpToolbar*	ToolbarManager::FindToolbar(ToolbarType type, OpWidget *root_widget, BOOL create_if_needed)
{
	UINT32 n;

	for(n = 0; n < m_toolbars.GetCount(); n++)
	{
		ToolbarManagerItem *item = m_toolbars.Get(n);
		if(item->m_type == type && item->m_root_widget == root_widget)
		{
			return item->m_toolbar;
		}
	}
	if(create_if_needed)
	{
		return CreateToolbar(type, root_widget);
	}
	return NULL;
}
