/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT
#ifdef _UNIX_DESKTOP_

#include "adjunct/widgetruntime/RuntimeGadgetManagerDialog.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/widgetruntime/GadgetUtils.h"
#include "adjunct/widgetruntime/models/GadgetsTreeModelItemImpl.h"
#include "adjunct/widgetruntime/models/GadgetsTreeModelItem.h"

#include "modules/gadgets/OpGadgetClass.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/widgets/OpButton.h"


const char* const RuntimeGadgetManagerDialog::MANAGER_ABOUT_LABEL	= "Manager_about_label";
const char* const RuntimeGadgetManagerDialog::WIDGETLIST			= "Widgetlist_treeview";
const char* const RuntimeGadgetManagerDialog::UNINSTALL_BUTTON		= "Uninstall_button";
const char* const RuntimeGadgetManagerDialog::WIDGET_NAME_LABEL		= "Widget_name_label";
const char* const RuntimeGadgetManagerDialog::AUTHOR_NAME_LABEL		= "Author_name_label";
const char* const RuntimeGadgetManagerDialog::WIDGET_DESC_LABEL		= "Widget_desc_label";

RuntimeGadgetManagerDialog::RuntimeGadgetManagerDialog()
	: m_view(NULL)
{}


OP_STATUS RuntimeGadgetManagerDialog::Init()
{
	RETURN_IF_ERROR(m_model.Init());

	RETURN_IF_ERROR( Dialog::Init(g_application->GetActiveDesktopWindow()) );

	m_view = static_cast<OpTreeView*>(GetWidgetByName(WIDGETLIST));
	if (m_view)
	{
		m_view->SetShowColumnHeaders(FALSE);
		m_model.SetActive(TRUE);
		m_view->SetTreeModel(&m_model);

		// should these be moved to OnInitVisibility?
		m_view->SetHaveWeakAssociatedText(TRUE);
		m_view->SetRestrictImageSize(FALSE);
		m_view->SetShowThreadImage(FALSE);
		m_view->SetAllowMultiLineIcons(TRUE);
		m_view->SetColumnImageFixedDrawArea(ICON_COLUMN, 
				GADGET_ICON_SIZE + 2);
		m_view->SetColumnFixedWidth(ICON_COLUMN, 
				GADGET_ICON_SIZE + 2);
		m_view->SetColumnFixedWidth(SEPARATOR_COLUMN,
				SEPARATOR_COLUMN_WIDTH);
		m_view->SetExtraLineHeight(2);
		m_view->SetVerticalAlign(WIDGET_V_ALIGN_TOP);
		m_view->SetMultiselectable(FALSE);
	}

	OpLabel *widget_name_label = static_cast<OpLabel*>(
			GetWidgetByName(WIDGET_NAME_LABEL));
	if (widget_name_label)
	{
		widget_name_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		widget_name_label->SetRelativeSystemFontSize(120);
	}

	OpLabel* about_label = static_cast<OpLabel*>(
			GetWidgetByName(MANAGER_ABOUT_LABEL));
	if (about_label)
	{
		about_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		about_label->SetRelativeSystemFontSize(150);
	}
	ClearWidgetLabels();
	// UpdateGadgetList();
	return OpStatus::OK;
}


BOOL RuntimeGadgetManagerDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE:
		{
			if( m_view && m_view->GetSelectedItemPos() != -1 )
			{
				if(OpStatus::IsSuccess( RemoveSelectedGadget() ))
				{
					// UpdateGadgetList();
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_DELETE:
				{
					if(m_view)
					{
						child_action->SetEnabled( m_view->GetSelectedItemPos()>=0 );
					}
					else
					{
						child_action->SetEnabled( FALSE );
					}
					return TRUE;
				}
			}
		}
	}
	return Dialog::OnInputAction(action);
}


void RuntimeGadgetManagerDialog::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	int pos = m_view->GetSelectedItemModelPos();
	if(pos >= 0)
	{
		GadgetsTreeModelItem *item = m_model.GetItemByIndex(pos);
		const OpGadgetClass& gc = item->GetGadgetClass();
		OpString name, by_author, author, description, format;
		// OpGadgetClass getters should really be const.
		const_cast<OpGadgetClass&>(gc).GetGadgetName(name);
		const_cast<OpGadgetClass&>(gc).GetGadgetAuthor(author);
		const_cast<OpGadgetClass&>(gc).GetGadgetDescription(description);
		author.Strip();
		if (author.HasContent())
		{
			OpStatus::Ignore(g_languageManager->GetString(
						Str::D_WIDGET_INSTALL_BY_AUTHOR, format));
			OpStatus::Ignore(by_author.AppendFormat(format, author.CStr()));
		}
		else
		{
			OpStatus::Ignore(g_languageManager->GetString(
						Str::D_WIDGET_INSTALL_NO_AUTHOR, by_author));
		}
		SetWidgetText(WIDGET_NAME_LABEL, name);
		SetWidgetText(AUTHOR_NAME_LABEL, by_author);
		SetWidgetText(WIDGET_DESC_LABEL, description);
	}
	else
	{
		ClearWidgetLabels();
	}
}


OP_STATUS RuntimeGadgetManagerDialog::RemoveSelectedGadget()
{
	int pos = m_view->GetSelectedItemModelPos();
	if( pos != -1 )
	{
		GadgetsTreeModelItem *item = m_model.GetItemByIndex(pos);
		item->OnDelete();
		// m_model.RefreshModel();
		ClearWidgetLabels();
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}


void RuntimeGadgetManagerDialog::ClearWidgetLabels()
{
	SetWidgetText(WIDGET_NAME_LABEL, "");
	SetWidgetText(AUTHOR_NAME_LABEL, "");
	SetWidgetText(WIDGET_DESC_LABEL, "");
}


#endif // _UNIX_DESKTOP_
#endif // WIDGET_RUNTIME_SUPPORT
