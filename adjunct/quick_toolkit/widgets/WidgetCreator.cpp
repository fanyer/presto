/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/WidgetCreator.h"

#include "adjunct/m2_ui/widgets/MailSearchField.h"
#include "adjunct/m2_ui/widgets/RichTextEditor.h"
#include "adjunct/quick/widgets/OpComposeEdit.h"
#include "adjunct/quick/widgets/OpTrustAndSecurityButton.h"
#include "adjunct/quick/widgets/OpFindTextBar.h"
#include "adjunct/quick/widgets/OpPersonalbar.h"
#include "adjunct/quick/widgets/OpProgressField.h"
#include "adjunct/quick/widgets/OpStatusField.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/widgets/OpSearchDropDown.h"
#include "adjunct/quick/widgets/OpSearchEdit.h"
#include "adjunct/quick/widgets/OpZoomDropDown.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "adjunct/quick/widgets/OpHistoryView.h"
#include "adjunct/quick/widgets/OpBookmarkView.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick/widgets/OpLinksView.h"
#include "adjunct/quick/widgets/OpExtensionButton.h"
#include "adjunct/quick/widgets/OpExtensionSet.h"
#include "adjunct/quick_toolkit/widgets/OpBrowserView.h"
#include "adjunct/quick_toolkit/widgets/OpButtonStrip.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick_toolkit/widgets/OpTabs.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"
#include "adjunct/quick_toolkit/widgets/OpSplitter.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpImageWidget.h"
#include "adjunct/quick_toolkit/widgets/OpExpand.h"
#include "adjunct/quick_toolkit/widgets/OpHelptooltip.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/widgets/OpSeparator.h"
#include "adjunct/quick_toolkit/widgets/OpRichTextLabel.h"
#include "adjunct/quick_toolkit/widgets/WidgetTypeMapping.h"
#include "adjunct/quick_toolkit/widgets/OpPasswordStrength.h"
#include "adjunct/quick_toolkit/widgets/DesktopOpDropDown.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpSlider.h"
#include "modules/widgets/OpRadioButton.h"
#include "modules/widgets/OpCheckBox.h"
#include "modules/widgets/OpScrollbar.h"
#include "modules/widgets/OpListBox.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpNumberEdit.h"

#include "adjunct/quick/application/QuickContextualUI.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/OpLineParser.h"

namespace
{
	const char* const s_resize_names[] =
	{
		"Fixed",
		"Move right",
		"Size right",
		"Center right",
		"Move down",
		"Move",
		"Size right Move down",
		"Center right Move down",
		"Size down",
		"Move right Size down",
		"Size",
		"Center right Size down",
		"Center down",
		"Move right Center down",
		"Size right Center down",
		"Center"
	};
};

WidgetCreator::WidgetCreator()
	: m_widget_type(OpTypedObject::UNKNOWN_TYPE)
	, m_string_id(Str::NOT_A_STRING)
	, m_resize_flags(0)
	, m_end(FALSE)
	, m_value(0)
	, m_action(0)
	, m_parent(0)
{}

WidgetCreator::~WidgetCreator()
{
	OP_DELETE(m_action);
}

OP_STATUS WidgetCreator::SetWidgetData(const uni_char* widget)
{
	OpLineParser line ( widget );
	OpString8 type;
	OpString type16;
	
	if (SkipEntry(line, type16))
		return OpStatus::ERR_OUT_OF_RANGE;

	type.Set(type16);
	m_widget_type = GetWidgetTypeFromString ( type );

	if ( m_widget_type == OpTypedObject::UNKNOWN_TYPE )
		return OpStatus::OK;
	
	line.GetNextLanguageString ( m_text, &m_string_id );
	
	RETURN_IF_ERROR ( line.GetNextToken8 ( m_name ) );
	
	line.GetNextValue ( m_rect.x );
	line.GetNextValue ( m_rect.y );
	line.GetNextValue ( m_rect.width );
	line.GetNextValue ( m_rect.height );

	OpString8 resize;
	RETURN_IF_ERROR ( line.GetNextToken8 ( resize ) );
	m_resize_flags = GetResizeFlagsFromString ( resize );
	
	OpString8 end;
	RETURN_IF_ERROR ( line.GetNextToken8 ( end ) );
	m_end = end.CompareI ( "End" ) == 0;
	
	line.GetNextValue ( m_value );
	
	return OpStatus::OK;
}

void WidgetCreator::SetActionData(const uni_char* action)
{
	if (OpStatus::IsError(OpInputAction::CreateInputActionsFromString(action, m_action)))
		m_action = 0;
}

OpWidget* WidgetCreator::CreateWidget()
{
	OpWidget* widget = ConstructWidgetForType();

	if (!widget)
		return 0;

	const BOOL add_as_first = OpTypedObject::WIDGET_TYPE_HELP_TOOLTIP == m_widget_type;
	
	m_parent->AddChild(widget, FALSE, add_as_first);

	if (m_action)
	{
		widget->SetAction(m_action);
		m_action = 0;
	}

	// Set resize effect first.  Subsequent widget configuration calls might
	// cause the widget to be updated for RTL.
	widget->SetXResizeEffect(GetXResizeEffect(m_resize_flags));
	widget->SetYResizeEffect(GetYResizeEffect(m_resize_flags));

	widget->SetStringID(m_string_id);
	widget->SetText(m_text.CStr());
	widget->SetName(m_name.CStr());
	widget->SetRect(m_rect);
	widget->SetOriginalRect(m_rect);

	if (OpStatus::IsError(widget->init_status))
	{
		widget->Delete();
		return 0;
	}

	return widget;
}

OpWidget* WidgetCreator::ConstructWidgetForType()
{
	switch ( m_widget_type )
	{
		case OpTypedObject::WIDGET_TYPE_BUTTON:				return ConstructWidget<OpButton>();
		case OpTypedObject::WIDGET_TYPE_SLIDER:				return ConstructWidget<OpSlider>();
		case OpTypedObject::WIDGET_TYPE_ZOOM_SLIDER:		return ConstructWidget<OpZoomSlider>();
		case OpTypedObject::WIDGET_TYPE_ZOOM_MENU_BUTTON:	return ConstructWidget<OpZoomMenuButton>();
		case OpTypedObject::WIDGET_TYPE_RADIOBUTTON:		return ConstructWidget<OpRadioButton>();
		case OpTypedObject::WIDGET_TYPE_CHECKBOX:			return ConstructWidget<OpCheckBox>();
		case OpTypedObject::WIDGET_TYPE_LISTBOX:			return ConstructWidget<OpListBox>();
		case OpTypedObject::WIDGET_TYPE_TREEVIEW:			return ConstructWidget<OpTreeView>();
		case OpTypedObject::WIDGET_TYPE_DROPDOWN:			return ConstructWidget<DesktopOpDropDown>();
		case OpTypedObject::WIDGET_TYPE_EDIT:				return ConstructWidget<OpEdit>();
		case OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT:		return ConstructWidget<OpMultilineEdit>();
		case OpTypedObject::WIDGET_TYPE_COMPOSE_EDIT:   	return ConstructWidget<OpComposeEdit>();
		case OpTypedObject::WIDGET_TYPE_TRUST_AND_SECURITY_BUTTON: return ConstructWidget<OpTrustAndSecurityButton>();
		case OpTypedObject::WIDGET_TYPE_LABEL:
		case OpTypedObject::WIDGET_TYPE_DESKTOP_LABEL:		return ConstructWidget<OpLabel>();
		case OpTypedObject::WIDGET_TYPE_WEBIMAGE:			return ConstructWidget<OpWebImageWidget>();
		case OpTypedObject::WIDGET_TYPE_FILECHOOSER_EDIT:	return ConstructWidget<DesktopFileChooserEdit>();
		case OpTypedObject::WIDGET_TYPE_FOLDERCHOOSER_EDIT:	return ConstructWidget<DesktopFileChooserEdit>(DesktopFileChooserEdit::FolderSelector);
		case OpTypedObject::WIDGET_TYPE_TOOLBAR:			return ConstructWidget<OpToolbar>();
		case OpTypedObject::WIDGET_TYPE_FINDTEXTBAR:		return ConstructWidget<OpFindTextBar>();
		case OpTypedObject::WIDGET_TYPE_BUTTON_STRIP:		return ConstructWidget<OpButtonStrip>(g_op_ui_info->DefaultButtonOnRight());
		case OpTypedObject::WIDGET_TYPE_GROUP:				return ConstructWidget<OpGroup>();
		case OpTypedObject::WIDGET_TYPE_SPLITTER:			return ConstructWidget<OpSplitter>();
		case OpTypedObject::WIDGET_TYPE_BROWSERVIEW:		return ConstructWidget<OpBrowserView>();
		case OpTypedObject::WIDGET_TYPE_PROGRESSBAR:		return ConstructWidget<OpProgressBar>();
		case OpTypedObject::WIDGET_TYPE_TABS:				return ConstructWidget<OpTabs>();
		case OpTypedObject::WIDGET_TYPE_PERSONALBAR:		return ConstructWidget<OpPersonalbar>();
		case OpTypedObject::WIDGET_TYPE_STATUS_FIELD:		return ConstructWidget<OpStatusField>();
		case OpTypedObject::WIDGET_TYPE_ICON:				return ConstructWidget<OpIcon>();
		case OpTypedObject::WIDGET_TYPE_EXPAND:				return ConstructWidget<OpExpand>();
		case OpTypedObject::WIDGET_TYPE_PROGRESS_FIELD:		return ConstructWidget<OpProgressField>();
		case OpTypedObject::WIDGET_TYPE_WORKSPACE:			return ConstructWidget<OpWorkspace>();
		case OpTypedObject::WIDGET_TYPE_ADDRESS_DROPDOWN:	return ConstructWidget<OpAddressDropDown>();
		case OpTypedObject::WIDGET_TYPE_SEARCH_DROPDOWN:	return ConstructWidget<OpSearchDropDown>();
		case OpTypedObject::WIDGET_TYPE_SEARCH_EDIT:		return ConstructWidget<OpSearchEdit>();
		case OpTypedObject::WIDGET_TYPE_ZOOM_DROPDOWN:		return ConstructWidget<OpZoomDropDown>();
		case OpTypedObject::WIDGET_TYPE_SCROLLBAR:			return ConstructWidget<OpScrollbar>();
		case OpTypedObject::WIDGET_TYPE_QUICK_FIND:			return ConstructWidget<OpQuickFind>();
		case OpTypedObject::WIDGET_TYPE_HISTORY_VIEW:		return ConstructWidget<OpHistoryView>();
		case OpTypedObject::WIDGET_TYPE_BOOKMARKS_VIEW:		return ConstructWidget<OpBookmarkView>();
		case OpTypedObject::WIDGET_TYPE_GADGETS_VIEW:		return ConstructWidget<OpHotlistView>(OpTypedObject::WIDGET_TYPE_GADGETS_VIEW);
#ifdef WEBSERVER_SUPPORT
		case OpTypedObject::WIDGET_TYPE_UNITE_SERVICES_VIEW:return ConstructWidget<OpHotlistView>(OpTypedObject::WIDGET_TYPE_UNITE_SERVICES_VIEW);
#endif
		case OpTypedObject::WIDGET_TYPE_CONTACTS_VIEW:		return ConstructWidget<OpHotlistView>(OpTypedObject::WIDGET_TYPE_CONTACTS_VIEW);
		case OpTypedObject::WIDGET_TYPE_NOTES_VIEW:			return ConstructWidget<OpHotlistView>(OpTypedObject::WIDGET_TYPE_NOTES_VIEW);
		case OpTypedObject::WIDGET_TYPE_LINKS_VIEW:			return ConstructWidget<OpLinksView>();
		case OpTypedObject::WIDGET_TYPE_NUMBER_EDIT:		return ConstructWidget<OpNumberEdit>();
		case OpTypedObject::WIDGET_TYPE_MULTILINE_LABEL:	return ConstructWidget<OpMultilineEdit>();
		case OpTypedObject::WIDGET_TYPE_SEPARATOR:			return ConstructWidget<OpSeparator>();
		case OpTypedObject::WIDGET_TYPE_RICH_TEXT_EDITOR:	return ConstructWidget<RichTextEditor>();
		case OpTypedObject::WIDGET_TYPE_HELP_TOOLTIP:		return ConstructWidget<OpHelpTooltip>();
		case OpTypedObject::WIDGET_TYPE_MAIL_SEARCH_FIELD:	return ConstructWidget<MailSearchField>();
		case OpTypedObject::WIDGET_TYPE_RICHTEXT_LABEL:		return ConstructWidget<OpRichTextLabel>();	
		case OpTypedObject::WIDGET_TYPE_PASSWORD_STRENGTH:	return ConstructWidget<OpPasswordStrength>();
	}

	return 0;
}

template<typename T> T* WidgetCreator::ConstructWidget()
{
	T* widget;
	if (OpStatus::IsSuccess(T::Construct(&widget)))
	{
		DoWidgetSpecificHacks(widget);
		return widget;
	}

	return 0;
}

template<typename T, typename U> T* WidgetCreator::ConstructWidget(U construct_argument)
{
	T* widget;
	if (OpStatus::IsSuccess(T::Construct(&widget, construct_argument)))
	{
		DoWidgetSpecificHacks(widget);
		return widget;
	}

	return 0;
}

void WidgetCreator::DoWidgetSpecificHacks(OpButton* widget)
{
	if ( m_action && m_action->HasActionImage() )
	{
		// A specific skin image has been set for this action. Currently this turns the button into a clickable
		// image. Once the dialog.ini format has changed to something more flexible, this behaviour should specified
		// with a parameter such as <button style="image">, so other styles and types are possible too. (huibk 04.02.2007)
		widget->SetButtonTypeAndStyle ( OpButton::TYPE_CUSTOM, OpButton::STYLE_IMAGE );
		widget->GetBorderSkin()->SetImage ( m_action->GetActionImage ( FALSE ) );
		widget->GetBorderSkin()->SetForeground ( TRUE );
	}
}

void WidgetCreator::DoWidgetSpecificHacks(OpLabel* label)
{
	if ( m_value )
		label->SetRelativeSystemFontSize ( m_value );
}

void WidgetCreator::DoWidgetSpecificHacks(OpBrowserView* browser_view)
{
	browser_view->SetEmbeddedInDialog(TRUE);
}

void WidgetCreator::DoWidgetSpecificHacks(OpIcon* icon)
{
	// The icon is special and the Language string is actually the Skin name to use
	if ( m_text.HasContent() )
	{
		OpString8 skin_name;

		skin_name.Set ( m_text.CStr() );
		icon->SetImage ( skin_name.CStr() );

		// Empty the string since it's not a string
		m_text.Empty();
	}
}

void WidgetCreator::DoWidgetSpecificHacks(OpMultilineEdit* widget)
{
	if (m_widget_type == OpTypedObject::WIDGET_TYPE_MULTILINE_LABEL)
		widget->SetLabelMode();
}

void WidgetCreator::DoWidgetSpecificHacks(OpHelpTooltip* tooltip)
{
	if (OpStatus::IsSuccess(tooltip->Init()))
	{
		if (m_action && m_action->HasActionDataString())
		{
			URL url = g_url_api->GetURL(m_action->GetActionDataString());
			OpString8 host_name;
			if (OpStatus::IsSuccess(url.GetAttribute(URL::KHostName, host_name))
					&& host_name.HasContent())
			{
				tooltip->SetHelpUrl(m_action->GetActionDataString());
			}
			else
			{
				tooltip->SetHelpTopic(m_action->GetActionDataString());
			}
		}
		tooltip->SetVisibility(FALSE);
	}
}

INT32 WidgetCreator::GetResizeFlagsFromString(const OpStringC8& name)
{
	for (size_t i = 0; i < ARRAY_SIZE(s_resize_names); i++)
	{
		if (name.CompareI(s_resize_names[i]) == 0)
			return i;
	}

	return 0;
}

RESIZE_EFFECT WidgetCreator::GetXResizeEffect(INT32 resize_flags)
{
	return static_cast<RESIZE_EFFECT>(resize_flags & 0x3);
}

RESIZE_EFFECT WidgetCreator::GetYResizeEffect(INT32 resize_flags)
{
	return static_cast<RESIZE_EFFECT>((resize_flags >> 2) & 0x3);
}

OpTypedObject::Type WidgetCreator::GetWidgetTypeFromString(const OpStringC8& widget_name)
{
	if (!widget_name.HasContent())
		return OpTypedObject::UNKNOWN_TYPE;

	unsigned length = widget_name.Length();

	// Don't count trailing digits
	while (length > 0 && op_isdigit(widget_name[length - 1]))
		length--;

	return WidgetTypeMapping::FromString(widget_name.CStr(), length);
}
