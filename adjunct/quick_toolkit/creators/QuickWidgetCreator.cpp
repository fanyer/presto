/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/creators/QuickWidgetCreator.h"

#include "adjunct/quick_toolkit/creators/QuickGridRowCreator.h"
#include "adjunct/quick_toolkit/readers/UIReader.h"
#include "adjunct/quick_toolkit/widgets/WidgetTypeMapping.h"
#include "adjunct/ui_parser/ParserIterators.h"

#include "adjunct/m2_ui/widgets/RichTextEditor.h"
#include "adjunct/quick/widgets/DesktopFileChooserEdit.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick_toolkit/widgets/DesktopToggleButton.h"
#include "adjunct/quick_toolkit/widgets/NullWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickAddressDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickButtonStrip.h"
#include "adjunct/quick_toolkit/widgets/QuickBrowserView.h"
#include "adjunct/quick_toolkit/widgets/QuickButton.h"
#include "adjunct/quick_toolkit/widgets/QuickCheckBox.h"
#include "adjunct/quick_toolkit/widgets/QuickComposeEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickComposite.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"
#include "adjunct/quick_toolkit/widgets/QuickEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickExpand.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickDynamicGrid.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickGroupBox.h"
#include "adjunct/quick_toolkit/widgets/QuickIcon.h"
#include "adjunct/quick_toolkit/widgets/QuickLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickMultilineLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickNumberEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"
#include "adjunct/quick_toolkit/widgets/QuickPagingLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickRadioButton.h"
#include "adjunct/quick_toolkit/widgets/QuickRichTextLabel.h"
#include "adjunct/quick_toolkit/widgets/QuickSearchEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickSeparator.h"
#include "adjunct/quick_toolkit/widgets/QuickScrollContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickSkinElement.h"
#include "adjunct/quick_toolkit/widgets/QuickTabs.h"
#include "adjunct/quick_toolkit/widgets/QuickTreeView.h"
#include "adjunct/quick_toolkit/widgets/QuickFlowLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickCentered.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWrapLayout.h"
#include "adjunct/desktop_util/adt/typedobjectcollection.h"

#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "adjunct/quick/widgets/OpBookmarkView.h"
#include "adjunct/quick/widgets/OpComposeEdit.h"
#include "adjunct/quick/widgets/OpFindTextBar.h"
#include "adjunct/quick/widgets/OpStatusField.h"
#include "adjunct/quick/widgets/OpHistoryView.h"
#include "adjunct/quick/widgets/OpLinksView.h"
#include "adjunct/quick/widgets/OpPersonalbar.h"
#include "adjunct/quick/widgets/OpProgressField.h"
#include "adjunct/quick/widgets/OpSearchDropDown.h"
#include "adjunct/quick/widgets/OpSearchEdit.h"
#include "adjunct/quick/widgets/OpTrustAndSecurityButton.h"
#include "adjunct/quick/widgets/OpZoomDropDown.h"
#include "adjunct/quick_toolkit/widgets/OpExpand.h"
#include "adjunct/quick_toolkit/widgets/OpHelptooltip.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "adjunct/quick_toolkit/widgets/OpImageWidget.h"
#include "adjunct/quick_toolkit/widgets/OpProgressbar.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "adjunct/quick_toolkit/widgets/OpSeparator.h"
#include "adjunct/quick_toolkit/widgets/OpTabs.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpNumberEdit.h"
#include "modules/widgets/OpSlider.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/scope/src/scope_manager.h"

OP_STATUS
QuickWidgetStateCreator::CreateWidgetStateAttributes(OpString & text, OpAutoPtr<OpInputAction>& action, OpString8 & widget_image, INT32& data)
{
	RETURN_IF_ERROR(GetLog().Evaluate(GetTranslatedScalarStringFromMap("string", text), "ERROR retrieving parameter 'string'"));
	if (text.IsEmpty())
		GetLog().OutputEntry("WARNING: widget needs to specify the 'string' parameter");
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarStringFromMap("skin-image", widget_image), "ERROR retrieving parameter 'skin-image'"));
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarIntFromMap("data", data), "ERROR retrieving parameter 'data'"));

	// This one should be last
	RETURN_IF_ERROR(GetLog().Evaluate(CreateInputActionFromMap(action), "ERROR creating input action for widget"));
	
	return OpStatus::OK;
}


////////// QuickWidgetCreator
QuickWidgetCreator::QuickWidgetCreator(TypedObjectCollection& widgets, ParserLogger& log, bool dialog_reader)
	: QuickUICreator(log)
	, m_widgets(&widgets)
	, m_has_quick_window(dialog_reader)
{
}

////////// QuickWidgetCreator
QuickWidgetCreator::~QuickWidgetCreator()
{}


////////// CreateWidget
OP_STATUS
QuickWidgetCreator::CreateWidget(OpAutoPtr<QuickWidget>& widget)
{
	QuickWidget* created = 0;
	OP_STATUS status = ConstructInitializedWidgetForType(created);

	widget.reset(created);
	return status;
}


////////// ConstructInitializedWidgetForType
OP_STATUS
QuickWidgetCreator::ConstructInitializedWidgetForType(QuickWidget *& widget)
{
	OpString8 widget_type_str;
	RETURN_IF_ERROR(GetScalarStringFromMap("type", widget_type_str));
	if (widget_type_str.IsEmpty())
	{
		GetLog().OutputEntry("ERROR: widget doesn't have the 'type' attribute specified");
		return OpStatus::ERR;
	}

	OpString8 widget_name;
	if (GetLog().IsLogging()) 
	{
		RETURN_IF_ERROR(GetScalarStringFromMap("name", widget_name));
		if (widget_name.IsEmpty())
			RETURN_IF_ERROR(widget_name.Set(widget_type_str));
	}
	
	ParserLogger::AutoIndenter indenter(GetLog(), "Reading '%s'", widget_name);

	OpTypedObject::Type widget_type = WidgetTypeMapping::FromString(widget_type_str);
	switch (widget_type)
	{
		case OpTypedObject::WIDGET_TYPE_STACK_LAYOUT:		RETURN_IF_ERROR(ConstructStackLayout(widget)); break;

		case OpTypedObject::WIDGET_TYPE_GRID_LAYOUT:		RETURN_IF_ERROR(ConstructInitWidget<QuickGrid>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_DYNAMIC_GRID_LAYOUT: RETURN_IF_ERROR(ConstructInitWidget<QuickDynamicGrid>(widget, NeedsTestSupport() ? QuickDynamicGrid::FORCE_TESTABLE : QuickDynamicGrid::NORMAL)); break;
		case OpTypedObject::WIDGET_TYPE_PAGING_LAYOUT:		RETURN_IF_ERROR(ConstructInitWidget<QuickPagingLayout>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_BUTTON:				RETURN_IF_ERROR(ConstructInitWidget<QuickButton>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_SLIDER:				RETURN_IF_ERROR(ConstructInitWidget<QuickSlider>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_ZOOM_SLIDER:		RETURN_IF_ERROR(ConstructInitWidget<QuickZoomSlider>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_ZOOM_MENU_BUTTON:	RETURN_IF_ERROR(ConstructInitWidget<QuickZoomMenuButton>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_RADIOBUTTON:		RETURN_IF_ERROR(ConstructInitWidget<QuickRadioButton>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_CHECKBOX:			RETURN_IF_ERROR(ConstructInitWidget<QuickCheckBox>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_LISTBOX:			RETURN_IF_ERROR(ConstructInitWidget<QuickListBox>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_TREEVIEW:			RETURN_IF_ERROR(ConstructInitWidget<QuickTreeView>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_DROPDOWN:			RETURN_IF_ERROR(ConstructInitWidget<QuickDropDown>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_EDIT:				RETURN_IF_ERROR(ConstructInitWidget<QuickEdit>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_MULTILINE_EDIT:		RETURN_IF_ERROR(ConstructInitWidget<QuickMultilineEdit>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_COMPOSE_EDIT:		RETURN_IF_ERROR(ConstructInitWidget<QuickComposeEdit>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_TRUST_AND_SECURITY_BUTTON:	RETURN_IF_ERROR(ConstructInitWidget<QuickTrustAndSecurityButton>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_DESKTOP_LABEL:		RETURN_IF_ERROR(ConstructInitWidget<QuickLabel>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_WEBIMAGE:			RETURN_IF_ERROR(ConstructInitWidget<QuickWebImageWidget>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_FILECHOOSER_EDIT:	RETURN_IF_ERROR(ConstructInitWidget<QuickDesktopFileChooserEdit>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_FOLDERCHOOSER_EDIT:	RETURN_IF_ERROR(ConstructInitWidget<QuickDesktopFileChooserEdit>(widget)); widget->GetTypedObject<QuickDesktopFileChooserEdit>()->GetOpWidget()->SetSelectorMode(DesktopFileChooserEdit::FolderSelector); break;
		case OpTypedObject::WIDGET_TYPE_TOOLBAR:			RETURN_IF_ERROR(ConstructInitWidget<QuickToolbar>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_FINDTEXTBAR:		RETURN_IF_ERROR(ConstructInitWidget<QuickFindTextBar>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_BUTTON_STRIP:		RETURN_IF_ERROR(ConstructInitWidget<QuickButtonStrip>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_GROUP:				RETURN_IF_ERROR(ConstructInitWidget<QuickGroup>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_SPLITTER:			RETURN_IF_ERROR(ConstructInitWidget<QuickSplitter>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_BROWSERVIEW:
		{
			if (m_has_quick_window)
				RETURN_IF_ERROR(ConstructInitWidget<QuickDialogBrowserView>(widget)); 
			else
				RETURN_IF_ERROR(ConstructInitWidget<QuickOpBrowserView>(widget)); 
			break;
		}
		case OpTypedObject::WIDGET_TYPE_PROGRESSBAR:		RETURN_IF_ERROR(ConstructInitWidget<QuickProgressBar>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_PERSONALBAR:		RETURN_IF_ERROR(ConstructInitWidget<QuickPersonalbar>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_STATUS_FIELD:		RETURN_IF_ERROR(ConstructInitWidget<QuickStatusField>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_ICON:				RETURN_IF_ERROR(ConstructInitWidget<QuickIcon>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_EXPAND:				RETURN_IF_ERROR(ConstructInitWidget<QuickExpand>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_PROGRESS_FIELD:		RETURN_IF_ERROR(ConstructInitWidget<QuickProgressField>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_ADDRESS_DROPDOWN:	RETURN_IF_ERROR(ConstructInitWidget<QuickAddressDropDown>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_SEARCH_DROPDOWN:	RETURN_IF_ERROR(ConstructInitWidget<QuickSearchDropDown>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_SEARCH_EDIT:		RETURN_IF_ERROR(ConstructInitWidget<QuickSearchEdit>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_ZOOM_DROPDOWN:		RETURN_IF_ERROR(ConstructInitWidget<QuickZoomDropDown>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_SCROLLBAR:			RETURN_IF_ERROR(ConstructInitWidget<QuickScrollbar>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_QUICK_FIND:			RETURN_IF_ERROR(ConstructInitWidget<QuickQuickFind>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_HISTORY_VIEW:		RETURN_IF_ERROR(ConstructInitWidget<QuickHistoryView>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_BOOKMARKS_VIEW:		RETURN_IF_ERROR(ConstructInitWidget<QuickBookmarkView>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_GADGETS_VIEW:		RETURN_IF_ERROR(ConstructInitWidget<QuickHotlistView>(widget, OpTypedObject::WIDGET_TYPE_GADGETS_VIEW)); break;
#ifdef WEBSERVER_SUPPORT
		case OpTypedObject::WIDGET_TYPE_UNITE_SERVICES_VIEW:	RETURN_IF_ERROR(ConstructInitWidget<QuickHotlistView>(widget, OpTypedObject::WIDGET_TYPE_UNITE_SERVICES_VIEW)); break;
#endif
		case OpTypedObject::WIDGET_TYPE_CONTACTS_VIEW:		RETURN_IF_ERROR(ConstructInitWidget<QuickHotlistView>(widget, OpTypedObject::WIDGET_TYPE_CONTACTS_VIEW)); break;
		case OpTypedObject::WIDGET_TYPE_NOTES_VIEW:			RETURN_IF_ERROR(ConstructInitWidget<QuickHotlistView>(widget, OpTypedObject::WIDGET_TYPE_NOTES_VIEW)); break;
		case OpTypedObject::WIDGET_TYPE_LINKS_VIEW:			RETURN_IF_ERROR(ConstructInitWidget<QuickLinksView>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_NUMBER_EDIT:		RETURN_IF_ERROR(ConstructInitWidget<QuickNumberEdit>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_MULTILINE_LABEL:	RETURN_IF_ERROR(ConstructInitWidget<QuickMultilineLabel>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_SEPARATOR:			RETURN_IF_ERROR(ConstructInitWidget<QuickSeparator>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_RICHTEXT_LABEL:		RETURN_IF_ERROR(ConstructInitWidget<QuickRichTextLabel>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_RICH_TEXT_EDITOR:	RETURN_IF_ERROR(ConstructInitWidget<QuickRichTextEditor>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_HELP_TOOLTIP:		RETURN_IF_ERROR(ConstructInitWidget<QuickHelpTooltip>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_GROUP_BOX:			RETURN_IF_ERROR(ConstructInitWidget<QuickGroupBox>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_TOGGLE_BUTTON:		RETURN_IF_ERROR(ConstructInitWidget<QuickToggleButton>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_TABS:				RETURN_IF_ERROR(ConstructInitWidget<QuickTabs>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_SCROLL_CONTAINER:	RETURN_IF_ERROR(ConstructInitWidget<QuickScrollContainer>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_SKIN_ELEMENT:		RETURN_IF_ERROR(ConstructInitWidget<QuickSkinElement>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_COMPOSITE:			RETURN_IF_ERROR(ConstructInitWidget<QuickComposite>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_FLOW_LAYOUT:		RETURN_IF_ERROR(ConstructInitWidget<QuickFlowLayout>(widget, NeedsTestSupport() ? QuickFlowLayout::FORCE_TESTABLE : QuickFlowLayout::NORMAL)); break;
		case OpTypedObject::WIDGET_TYPE_CENTERED:			RETURN_IF_ERROR(ConstructInitWidget<QuickCentered>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_EMPTY:				RETURN_IF_ERROR(ConstructInitWidget<NullWidget>(widget)); break;
		case OpTypedObject::WIDGET_TYPE_WRAP_LAYOUT:		RETURN_IF_ERROR(ConstructInitWidget<QuickWrapLayout>(widget)); break;
		default:
		{
			GetLog().OutputEntry("ERROR: unknown widget type: '%s'", widget_type_str);
			return OpStatus::ERR_NOT_SUPPORTED;
		}
	}

	indenter.Done();
	return OpStatus::OK;
}


////////// ConstructStackLayout
OP_STATUS
QuickWidgetCreator::ConstructStackLayout(QuickWidget*& layout)
{
	// get stack layout orientation first. default: vertical (?)
	OpString8 orientation_str;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarStringFromMap("orientation", orientation_str), "ERROR retrieving 'orientation' attribute"));

	QuickStackLayout::Orientation orientation = QuickStackLayout::VERTICAL;
	if (orientation_str.Compare("horizontal") == 0)
	{
		orientation = QuickStackLayout::HORIZONTAL;
	}
	else
	{
		OP_ASSERT(orientation_str.IsEmpty() || orientation_str.Compare("vertical") == 0);
		if (orientation_str.HasContent() && orientation_str.Compare("vertical") != 0)
		{
			GetLog().OutputEntry("ERROR: attribute 'orientation' needs to be either unspecified, 'vertical' or 'horizontal'. You specified '%s'", orientation_str.CStr());
		}
	}

	OpAutoPtr<QuickStackLayout> new_layout (OP_NEW(QuickStackLayout, (orientation)));
	if (!new_layout.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(SetWidgetParameters(new_layout.get()));
	RETURN_IF_ERROR(SetWidgetName(new_layout.get()));
	RETURN_IF_ERROR(SetGenericWidgetParameters(new_layout.get()));
	layout = new_layout.release();
	return OpStatus::OK;
}

////////// ConstructInitWidget
template<typename Type>
OP_STATUS
QuickWidgetCreator::ConstructInitWidget(QuickWidget*& widget)
{
	OpAutoPtr<Type> new_widget (OP_NEW(Type, ()));
	if (!new_widget.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_widget->Init());
	RETURN_IF_ERROR(SetWidgetParameters(new_widget.get()));
	RETURN_IF_ERROR(SetTextWidgetParameters(new_widget.get()));
	RETURN_IF_ERROR(SetWidgetName(new_widget.get()));
	RETURN_IF_ERROR(SetWidgetSkin(new_widget.get()));
	RETURN_IF_ERROR(SetGenericWidgetParameters(new_widget.get()));
	widget = new_widget.release();
	return OpStatus::OK;
}


////////// ConstructInitWidget
template<typename Type, typename Arg>
OP_STATUS
QuickWidgetCreator::ConstructInitWidget(QuickWidget*& widget, Arg init_argument)
{
	OpAutoPtr<Type> new_widget (OP_NEW(Type, ()));
	if (!new_widget.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(new_widget->Init(init_argument));
	RETURN_IF_ERROR(SetWidgetParameters(new_widget.get()));
	RETURN_IF_ERROR(SetTextWidgetParameters(new_widget.get()));
	RETURN_IF_ERROR(SetWidgetName(new_widget.get()));
	RETURN_IF_ERROR(SetWidgetSkin(new_widget.get()));
	RETURN_IF_ERROR(SetGenericWidgetParameters(new_widget.get()));
	widget = new_widget.release();
	return OpStatus::OK;
}


////////// SetGenericWidgetParameters
OP_STATUS
QuickWidgetCreator::SetGenericWidgetParameters(QuickWidget * widget)
{
	UINT32 minwidth = WidgetSizes::UseDefault;
	GetWidgetSizeFromMap("minimum-width", minwidth);
	if (minwidth != WidgetSizes::UseDefault)
		widget->SetMinimumWidth(minwidth);

	UINT32 minheight = WidgetSizes::UseDefault;
	GetWidgetSizeFromMap("minimum-height", minheight);
	if (minheight != WidgetSizes::UseDefault)
		widget->SetMinimumHeight(minheight);

	UINT32 nomwidth = WidgetSizes::UseDefault;
	GetWidgetSizeFromMap("nominal-width", nomwidth);
	if (nomwidth != WidgetSizes::UseDefault)
		widget->SetNominalWidth(nomwidth);

	UINT32 nomheight = WidgetSizes::UseDefault;
	GetWidgetSizeFromMap("nominal-height", nomheight);
	if (nomheight != WidgetSizes::UseDefault)
		widget->SetNominalHeight(nomheight);

	UINT32 prefwidth = WidgetSizes::UseDefault;
	GetWidgetSizeFromMap("preferred-width", prefwidth);
	if (prefwidth != WidgetSizes::UseDefault)
		widget->SetPreferredWidth(prefwidth);

	UINT32 prefheight = WidgetSizes::UseDefault;
	GetWidgetSizeFromMap("preferred-height", prefheight);
	if (prefheight != WidgetSizes::UseDefault)
		widget->SetPreferredHeight(prefheight);

	UINT32 fixedwidth = WidgetSizes::UseDefault;
	GetWidgetSizeFromMap("fixed-width", fixedwidth);
	if (fixedwidth != WidgetSizes::UseDefault)
		widget->SetFixedWidth(fixedwidth);

	UINT32 fixedheight = WidgetSizes::UseDefault;
	GetWidgetSizeFromMap("fixed-height", fixedheight);
	if (fixedheight != WidgetSizes::UseDefault)
		widget->SetFixedHeight(fixedheight);

	WidgetSizes::Margins margins(WidgetSizes::UseDefault);
	GetWidgetSizeFromMap("left-margin", margins.left);
	GetWidgetSizeFromMap("right-margin", margins.right);
	GetWidgetSizeFromMap("top-margin", margins.top);
	GetWidgetSizeFromMap("bottom-margin", margins.bottom);
	// Don't overwrite what SetTextWidgetParameter may have done
	if (!margins.IsDefault())
		widget->SetMargins(margins);

	bool hidden = false;
	RETURN_IF_ERROR(GetScalarBoolFromMap("hidden", hidden));
	if (hidden)
		widget->Hide();

	return OpStatus::OK;
}


////////// ReadWidgetName
OP_STATUS
QuickWidgetCreator::ReadWidgetName(OpString8& name)
{
	RETURN_IF_ERROR(GetScalarStringFromMap("name", name));
	if (name.IsEmpty())
	{
		GetLog().OutputEntry("WARNING: unnamed widget used");
	}
	return OpStatus::OK;
}

////////// SetWidgetName
template<typename OpWidgetType>
OP_STATUS
QuickWidgetCreator::SetWidgetName(QuickOpWidgetWrapperNoInit<OpWidgetType> * widget)
{
	OpString8 name;
	RETURN_IF_ERROR(ReadWidgetName(name));
	if (name.HasContent())
	{
		RETURN_IF_ERROR(m_widgets->Put(name, widget));
		widget->GetOpWidget()->SetName(name);
	}
	return OpStatus::OK;
}

template<typename OpWidgetType>
OP_STATUS
QuickWidgetCreator::SetWidgetName(QuickBackgroundWidget<OpWidgetType> * widget)
{
	OpString8 name;
	RETURN_IF_ERROR(ReadWidgetName(name));
	if (name.HasContent())
	{
		RETURN_IF_ERROR(m_widgets->Put(name, widget));
		widget->GetOpWidget()->SetName(name);
	}

	return OpStatus::OK;
}

OP_STATUS
QuickWidgetCreator::SetWidgetName(QuickWidget * widget)
{
	OpString8 name;
	RETURN_IF_ERROR(ReadWidgetName(name));
	if (name.HasContent())
	{
		RETURN_IF_ERROR(m_widgets->Put(name, widget));

		if (widget->IsOfType<QuickDynamicGrid>())
			widget->GetTypedObject<QuickDynamicGrid>()->SetName(name);
		else if (widget->IsOfType<QuickExpand>())
			widget->GetTypedObject<QuickExpand>()->SetName(name);
		else if (widget->IsOfType<QuickGroupBox>())
			widget->GetTypedObject<QuickGroupBox>()->SetName(name);
		else if (widget->IsOfType<QuickBrowserView>())
			widget->GetTypedObject<QuickBrowserView>()->GetOpBrowserView()->SetName(name);
		else if (widget->IsOfType<QuickFlowLayout>())
			widget->GetTypedObject<QuickFlowLayout>()->SetName(name);
	}
	return OpStatus::OK;
}


////////// SetTextWidgetParameters
OP_STATUS
QuickWidgetCreator::SetTextWidgetParameters(QuickTextWidget * widget)
{
	OpString translated_text;
	RETURN_IF_ERROR(GetLog().Evaluate(GetTranslatedScalarStringFromMap("string", translated_text), "ERROR reading node 'string'"));

	if (translated_text.HasContent())
		RETURN_IF_ERROR(widget->SetText(translated_text));

	UINT32 mincharwidth = WidgetSizes::UseDefault;
	GetCharacterSizeFromMap("minimum-width", mincharwidth);
	if (mincharwidth != WidgetSizes::UseDefault)
		widget->SetMinimumCharacterWidth(mincharwidth);

	UINT32 mincharheight = WidgetSizes::UseDefault;
	GetCharacterSizeFromMap("minimum-height", mincharheight);
	if (mincharheight != WidgetSizes::UseDefault)
		widget->SetMinimumCharacterHeight(mincharheight);

	UINT32 nomcharwidth = WidgetSizes::UseDefault;
	GetCharacterSizeFromMap("nominal-width", nomcharwidth);
	if (nomcharwidth != WidgetSizes::UseDefault)
		widget->SetNominalCharacterWidth(nomcharwidth);

	UINT32 nomcharheight = WidgetSizes::UseDefault;
	GetCharacterSizeFromMap("nominal-height", nomcharheight);
	if (nomcharheight != WidgetSizes::UseDefault)
		widget->SetNominalCharacterHeight(nomcharheight);

	UINT32 prefcharwidth = WidgetSizes::UseDefault;
	GetCharacterSizeFromMap("preferred-width", prefcharwidth);
	if (prefcharwidth != WidgetSizes::UseDefault)
		widget->SetPreferredCharacterWidth(prefcharwidth);

	UINT32 prefcharheight = WidgetSizes::UseDefault;
	GetCharacterSizeFromMap("preferred-height", prefcharheight);
	if (prefcharheight != WidgetSizes::UseDefault)
		widget->SetPreferredCharacterHeight(prefcharheight);

	UINT32 fixedcharwidth = WidgetSizes::UseDefault;
	GetCharacterSizeFromMap("fixed-width", fixedcharwidth);
	if (fixedcharwidth != WidgetSizes::UseDefault)
		widget->SetFixedCharacterWidth(fixedcharwidth);

	UINT32 fixedcharheight = WidgetSizes::UseDefault;
	GetCharacterSizeFromMap("fixed-height", fixedcharheight);
	if (fixedcharheight != WidgetSizes::UseDefault)
		widget->SetFixedCharacterHeight(fixedcharheight);

	WidgetSizes::Margins margins(WidgetSizes::UseDefault);
	GetCharacterSizeFromMap("left-margin", margins.left);
	GetCharacterSizeFromMap("right-margin", margins.right);
	GetCharacterSizeFromMap("top-margin", margins.top);
	GetCharacterSizeFromMap("bottom-margin", margins.bottom);
	widget->SetCharacterMargins(margins);

	int font_rel_size = 100;
	RETURN_IF_ERROR(GetScalarIntFromMap("font-rel-size", font_rel_size));
	widget->SetRelativeSystemFontSize(font_rel_size);

	OpString8 font_weight;
	RETURN_IF_ERROR(GetScalarStringFromMap("font-weight", font_weight));
	QuickOpWidgetBase::FontWeight quick_font_weight = QuickOpWidgetBase::WEIGHT_NORMAL;

	if (font_weight == "bold")
		quick_font_weight = QuickOpWidgetBase::WEIGHT_BOLD;
	else if (font_weight == "default")
		quick_font_weight = QuickOpWidgetBase::WEIGHT_DEFAULT;

	widget->SetSystemFontWeight(quick_font_weight);

	OpString8 ellipsis;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarStringFromMap("ellipsis", ellipsis),
				"ERROR reading node 'ellipsis'"));
	if (ellipsis.HasContent())
	{
		ELLIPSIS_POSITION position = ELLIPSIS_END;
		if (ellipsis == "center")
			position = ELLIPSIS_CENTER;
		else if (ellipsis == "none")
			position = ELLIPSIS_NONE;
		else if (ellipsis != "end")
			GetLog().OutputEntry("WARNING: ellipsis must be set to either 'none', 'center', or 'end'. Assuming value 'end'.");
		widget->SetEllipsis(position);
	}

	return OpStatus::OK;
}


////////// SetWidgetSkin
OP_STATUS
QuickWidgetCreator::SetWidgetSkin(GenericQuickOpWidgetWrapper * widget)
{
	OpString8 skin_image;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarStringFromMap("skin-image", skin_image), "ERROR retrieving parameter 'skin-image'"));
	if (skin_image.HasContent())
		widget->SetSkin(skin_image);

	return OpStatus::OK;
}


////////// SetReadOnly
template<typename OpWidgetType>
OP_STATUS
QuickWidgetCreator::SetReadOnly(OpWidgetType * widget)
{
	bool read_only = false;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("read-only", read_only), "ERROR retrieving parameter 'read-only'"));
	widget->SetReadOnly(read_only);

	return OpStatus::OK;
}


////////// SetGhostText
template<typename OpWidgetType>
OP_STATUS
QuickWidgetCreator::SetGhostText(OpWidgetType * widget)
{
	OpString ghost_text;
	RETURN_IF_ERROR(GetLog().Evaluate(GetTranslatedScalarStringFromMap("ghost-string", ghost_text), " ERROR reading node 'ghost-string'"));
	if (ghost_text.HasContent())
		RETURN_IF_ERROR(widget->SetGhostText(ghost_text.CStr()));

	return OpStatus::OK;
}


////////// SetContent
template<typename Type>
OP_STATUS
QuickWidgetCreator::SetContent(Type * widget, const OpStringC8& widget_name)
{
	OpAutoPtr<QuickWidget> content_widget;

	ParserLogger::AutoIndenter indenter(GetLog(), "Reading node 'content'");
	RETURN_IF_ERROR(CreateWidgetFromMapNode("content", content_widget, *m_widgets, m_has_quick_window));

	if (!content_widget.get())
		GetLog().OutputEntry("WARNING: widget without content: ", widget_name);

	widget->SetContent(content_widget.release());
	indenter.Done();
	return OpStatus::OK;
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickComposite * composite)
{
	return SetContent(composite, "Composite");
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickGroupBox * group_box)
{
	return SetContent(group_box, "GroupBox");
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickScrollContainer * scroll)
{
	return SetContent(scroll, "ScrollContainer");
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickSkinElement * skin_element)
{
	bool needs_dynamic_padding = false;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("dynamic-padding", needs_dynamic_padding), "ERROR reading node 'dynamic-padding'"));
	skin_element->SetDynamicPadding(needs_dynamic_padding);

	return SetContent(skin_element, "SkinElement");
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickToggleButton * toggle_button)
{
	ParserNodeSequence node;
	RETURN_IF_ERROR(GetLog().Evaluate(GetNodeFromMap("states", node), "ERROR reading node 'states'"));

	ParserLogger::AutoIndenter indenter(GetLog(), "Reading toggle button states");

	for (ParserSequenceIterator it(node); it; ++it)
	{
		ParserNodeMapping item_node;
		GetUINode()->GetChildNodeByID(it.Get(), item_node);

		QuickWidgetStateCreator item_creator(GetLog());;
		RETURN_IF_ERROR(item_creator.Init(&item_node));

		OpAutoPtr<OpInputAction> action;
		OpString text;
		OpString8 skin_image;
		INT32 data;

		RETURN_IF_ERROR(item_creator.CreateWidgetStateAttributes(text, action, skin_image, data));

		OP_ASSERT(action.get() != NULL);
		RETURN_IF_ERROR(toggle_button->GetOpWidget()->AddToggleState(action.get(), text, skin_image));
		action.release();
	}

	indenter.Done();
	return SetWidgetParameters(static_cast<OpButton*>(toggle_button->GetOpWidget()));;
}

OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickRichTextLabel* rich_text_label)
{
	bool wrap = FALSE;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("wrap", wrap), "ERROR reading node 'wrap'"));
	rich_text_label->GetOpWidget()->SetWrapping(wrap);

	return OpStatus::OK;
}

////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickButton* button)
{
	OpAutoPtr<OpInputAction> action;
	RETURN_IF_ERROR(CreateInputActionFromMap(action));
	button->GetOpWidget()->SetAction(action.release());
	
	bool is_default = false;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("default", is_default), "ERROR reading node 'default'"));
	button->SetDefault(is_default);

	return SetWidgetParameters(static_cast<OpButton*>(button->GetOpWidget()));
}

////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(OpButton* button)
{
	OpString8 button_style;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarStringFromMap("button-style", button_style), "ERROR retrieving parameter 'button-style'"));
	if (button_style.HasContent())
	{
		if (button_style.Compare("toolbar-image") == 0)
		{
			button->SetButtonTypeAndStyle(OpButton::TYPE_TOOLBAR, OpButton::STYLE_IMAGE);
		}
		else if (button_style.Compare("toolbar-text") == 0)
		{
			button->SetButtonTypeAndStyle(OpButton::TYPE_TOOLBAR, OpButton::STYLE_TEXT);
		}
		else if (button_style.Compare("toolbar-text-right") == 0)
		{
			button->SetButtonTypeAndStyle(OpButton::TYPE_TOOLBAR, OpButton::STYLE_IMAGE_AND_TEXT_ON_RIGHT);
		}
		else
		{
			RETURN_IF_ERROR(GetLog().OutputEntry("ERROR: button-style not recognized: %s", button_style));
		}
	}

	if (IsScalarInMap(("fixed-image")))
	{
		bool fixed_image = false;
		RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("fixed-image", fixed_image), "ERROR reading node 'fixed-image'"));
		button->SetFixedImage(fixed_image);
	}

	OpString8 skin_border_image;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarStringFromMap("skin-border-image", skin_border_image), "ERROR retrieving parameter 'skin-border-image'"));
	if (skin_border_image.HasContent())
		button->GetBorderSkin()->SetImage(skin_border_image.CStr());

	OpString8 skin_foreground_image;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarStringFromMap("skin-foreground-image", skin_foreground_image), "ERROR retrieving parameter 'skin-foreground-image'"));
	if (skin_foreground_image.HasContent())
		button->GetForegroundSkin()->SetImage(skin_foreground_image.CStr());

	bool is_tab_stop = false;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("tab-stop", is_tab_stop), "ERROR reading node 'tab-stop'"));
	button->SetTabStop(is_tab_stop && g_op_ui_info->IsFullKeyboardAccessActive());

	return OpStatus::OK;
}

////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickLabel* label)
{
	bool selectable = false;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("selectable", selectable),
				"ERROR reading node 'selectable'"));
	label->GetOpWidget()->SetSelectable(selectable);

	return OpStatus::OK;
}

////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickMultilineLabel* label)
{
	bool selectable = false;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("selectable", selectable),
				"ERROR reading node 'selectable'"));
	label->GetOpWidget()->SetLabelMode(selectable);

	return OpStatus::OK;
}

////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickWrapLayout* layout)
{
	INT32 max_visible = 0;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarIntFromMap("max-lines", max_visible), "ERROR reading max-lines"));
	if (max_visible != 0)
		layout->SetMaxVisibleLines(max_visible, false);

	return SetWidgetParameters(static_cast<QuickLayoutBase*>(layout));
}

OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickIcon* icon)
{
	OpString8 image_name;
	RETURN_IF_ERROR(GetScalarStringFromMap("skin-image", image_name));
	if (image_name.HasContent())
		icon->SetImage(image_name);

	bool allow_scaling = false;
	RETURN_IF_ERROR(GetScalarBoolFromMap("allow-scaling", allow_scaling));
	if (allow_scaling)
		icon->SetAllowScaling();

	return OpStatus::OK;
}

////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickButtonStrip * button_strip)
{
	OpString8 help_anchor8;
	RETURN_IF_ERROR(GetScalarStringFromMap("help_anchor", help_anchor8));

	ParserNodeSequence node;
	RETURN_IF_ERROR(GetNodeFromMap("buttons", node));
	if (node.IsEmpty())
	{
		GetLog().OutputEntry("ERROR: a button strip needs to specify buttons");
		return OpStatus::ERR;
	}

	ParserLogger::AutoIndenter indenter(GetLog(), "Reading button strip buttons");

	for (ParserSequenceIterator it(node); it; ++it)
	{
		ParserNodeMapping button_node;
		GetUINode()->GetChildNodeByID(it.Get(), button_node);
		QuickWidgetCreator widget_creator(*m_widgets, GetLog(), m_has_quick_window);
		RETURN_IF_ERROR(widget_creator.Init(&button_node));

		OpAutoPtr<QuickWidget> button;
		RETURN_IF_ERROR(widget_creator.CreateWidget(button));

		if (help_anchor8.HasContent())
		{
			QuickButton* btn = button->GetTypedObject<QuickButton>();
			OpInputAction* action = btn ? btn->GetOpWidget()->GetAction() : NULL;
			if (action && action->GetAction() == OpInputAction::ACTION_SHOW_HELP)
			{
				OpString help_anchor;
				RETURN_IF_ERROR(help_anchor.Set(help_anchor8));
				action->SetActionDataString(help_anchor);
			}
		}

		RETURN_IF_ERROR(button_strip->InsertIntoPrimaryContent(button.release()));
	}

	indenter.Done();

	// TODO: possible enhancement: handle dynamic help anchors?

	OpAutoPtr<QuickWidget> special_content;
	RETURN_IF_ERROR(GetLog().Evaluate(CreateWidgetFromMapNode("special-content", special_content, *m_widgets, m_has_quick_window), "ERROR reading node 'special-content'"));
	if (!special_content.get())
		return OpStatus::OK;

	return button_strip->SetSecondaryContent(special_content.release());
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickAddressDropDown * drop_down)
{
	return SetGhostText(drop_down->GetOpWidget());
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickDropDown * drop_down)
{
	RETURN_IF_ERROR(SetGhostText(drop_down->GetOpWidget()));

	bool editable = false;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("editable", editable), "ERROR reading node 'editable'"));
	if (editable)
		drop_down->GetOpWidget()->SetEditableText();

	ParserLogger::AutoIndenter indenter(GetLog(), "Reading dropdown elements");

	ParserNodeSequence node;
	RETURN_IF_ERROR(GetLog().Evaluate(GetNodeFromMap("elements", node), "ERROR reading node 'elements'"));

	for (ParserSequenceIterator it(node); it; ++it)
	{
		ParserNodeMapping item_node;
		GetUINode()->GetChildNodeByID(it.Get(), item_node);

		QuickWidgetStateCreator item_creator(GetLog());
		RETURN_IF_ERROR(item_creator.Init(&item_node));

		OpAutoPtr<OpInputAction> action;
		OpString text;
		OpString8 skin_image;
		INT32 data;

		RETURN_IF_ERROR(item_creator.CreateWidgetStateAttributes(text, action, skin_image, data));
		RETURN_IF_ERROR(drop_down->AddItem(text, -1, NULL, data, action.get(), skin_image));
		action.release();
	}

	indenter.Done();

	return OpStatus::OK;
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickEdit * edit)
{
	bool password_mode = false;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("password", password_mode), "ERROR reading node 'password'"));
	edit->GetOpWidget()->SetPasswordMode(password_mode);

	bool force_ltr = false;
	RETURN_IF_ERROR(GetLog().Evaluate(GetScalarBoolFromMap("force-ltr", force_ltr), "ERROR reading node 'force-ltr'"));
	edit->GetOpWidget()->SetForceTextLTR(force_ltr);

	RETURN_IF_ERROR(SetReadOnly(edit->GetOpWidget()));
	RETURN_IF_ERROR(SetGhostText(edit->GetOpWidget()));

	return OpStatus::OK;
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickMultilineEdit * edit)
{
	return SetReadOnly(edit->GetOpWidget());
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickExpand * expand)
{
	OpString translated_text;
	RETURN_IF_ERROR(GetLog().Evaluate(GetTranslatedScalarStringFromMap("string", translated_text), "ERROR reading node 'string'"));

	if (translated_text.HasContent())
		RETURN_IF_ERROR(expand->SetText(translated_text, translated_text));
	else
		GetLog().OutputEntry("WARNING: text widget has no text set");

	return SetContent(expand, "Expand");
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickSelectable * selectable)
{
	ParserLogger::AutoIndenter indenter(GetLog(), "Reading content");

	OpAutoPtr<OpInputAction> action;
	RETURN_IF_ERROR(CreateInputActionFromMap(action));
	selectable->GetOpWidget()->SetAction(action.release());

	OpAutoPtr<QuickWidget> content_widget;
	RETURN_IF_ERROR(CreateWidgetFromMapNode("content", content_widget, *m_widgets, m_has_quick_window));
	if (content_widget.get())
		selectable->SetChild(content_widget.release());

	indenter.Done();
	return OpStatus::OK;
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickDesktopFileChooserEdit * chooser)
{
	OpString title;
	RETURN_IF_ERROR(GetLog().Evaluate(GetTranslatedScalarStringFromMap("title", title),
				"ERROR reading node 'title'"));
	RETURN_IF_ERROR(chooser->GetOpWidget()->SetTitle(title.CStr()));

	OpString filter;
	RETURN_IF_ERROR(GetLog().Evaluate(GetTranslatedScalarStringFromMap("filter-string", filter),
				"ERROR reading node 'filter-string'"));
	RETURN_IF_ERROR(chooser->GetOpWidget()->SetFilterString(filter.CStr()));

	return OpStatus::OK;
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickStackLayout * stack_layout)
{
	// A grid/stack without elements is valid in case it needs to be filled
	// in at runtime
	ParserNodeSequence node;
	RETURN_IF_ERROR(GetNodeFromMap("elements", node));

	if (node.IsEmpty())
		GetLog().OutputEntry("WARNING: StackLayout doesn't have the 'elements' node specified.");

	ParserLogger::AutoIndenter indenter(GetLog(), "Reading stack layout elements");

	for (ParserSequenceIterator it(node); it; ++it)
	{
		ParserNodeMapping child_node;
		GetUINode()->GetChildNodeByID(it.Get(), child_node);

		QuickWidgetCreator widget_creator(*m_widgets, GetLog(), m_has_quick_window);
		RETURN_IF_ERROR(widget_creator.Init(&child_node));

		OpAutoPtr<QuickWidget> widget;
		RETURN_IF_ERROR(widget_creator.CreateWidget(widget));
		RETURN_IF_ERROR(stack_layout->InsertWidget(widget.release()));
	}

	indenter.Done();

	return SetWidgetParameters(static_cast<GenericGrid*>(stack_layout));
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickGrid * grid_layout)
{
	// A grid/stack without elements is valid in case it needs to be filled
	// in at runtime or a template is used
	ParserNodeSequence node;
	RETURN_IF_ERROR(GetNodeFromMap("elements", node));

	if (node.IsEmpty() && !grid_layout->GetTypedObject<QuickDynamicGrid>())
		GetLog().OutputEntry("WARNING: GridLayout doesn't have the 'elements' node specified.");

	ParserLogger::AutoIndenter indenter(GetLog(), "Reading grid layout elements");

	for (ParserSequenceIterator it(node); it; ++it)
	{
		ParserLogger::AutoIndenter indenter(GetLog(), "Reading elements of grid row");

		ParserNodeMapping row_node;
		if (!GetUINode()->GetChildNodeByID(it.Get(), row_node))
		{
			GetLog().OutputEntry("WARNING: found a node that is not of type MAPPING. Ignoring element.");
			indenter.Done();
			continue;
		}
		ParserNodeIDTable table;
		RETURN_IF_ERROR(GetLog().Evaluate(row_node.GetHashTable(table), "ERROR: couldn't retrieve hash table from mapping node"));

		ParserNodeIDTableData * data;
		RETURN_IF_ERROR(GetLog().Evaluate(table.GetData("elements", &data), "ERROR: 'elements' node is not specified in map"));

		ParserNodeSequence row_elements_node;
		row_node.GetChildNodeByID(data->data_id, row_elements_node);

		RETURN_IF_ERROR(grid_layout->AddRow());

		for (ParserSequenceIterator row_it(row_elements_node); row_it; ++row_it)
		{
			ParserNodeMapping child_node;
			GetUINode()->GetChildNodeByID(row_it.Get(), child_node);

			QuickWidgetCreator widget_creator(*m_widgets, GetLog(), m_has_quick_window);
			RETURN_IF_ERROR(widget_creator.Init(&child_node));

			OpAutoPtr<QuickWidget> widget;
			RETURN_IF_ERROR(widget_creator.CreateWidget(widget));

			INT32 colspan = 1;
			RETURN_IF_ERROR(GetLog().Evaluate(widget_creator.GetScalarIntFromMap("colspan", colspan), "ERROR reading colspan"));
			RETURN_IF_ERROR(grid_layout->InsertWidget(widget.release(), colspan));
		}

		indenter.Done();
	}

	indenter.Done();

	return SetWidgetParameters(static_cast<GenericGrid*>(grid_layout));
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickDynamicGrid * grid_layout)
{
	ParserNodeSequence template_node;
	if (OpStatus::IsSuccess(GetNodeFromMap("template", template_node)))
	{
		QuickGridRowCreator* row_creator = OP_NEW(QuickGridRowCreator, (template_node, GetLog(), m_has_quick_window));
		if (!row_creator)
			return OpStatus::ERR_NO_MEMORY;
		grid_layout->SetTemplateInstantiator(row_creator);
	}

	return SetWidgetParameters(static_cast<QuickGrid*>(grid_layout));
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(GenericGrid * grid_layout)
{
	// Get horizontal centering property (default off)
	bool hcenter = false;
	RETURN_IF_ERROR(GetScalarBoolFromMap("hcenter", hcenter));
	grid_layout->SetCenterHorizontally(hcenter);

	// Get vertical centering property (default on)
	bool vcenter = true;
	RETURN_IF_ERROR(GetScalarBoolFromMap("vcenter", vcenter));
	grid_layout->SetCenterVertically(vcenter);

	bool uniform_cells = false;
	RETURN_IF_ERROR(GetScalarBoolFromMap("uniform-cells", uniform_cells));
	RETURN_IF_ERROR(grid_layout->SetUniformCells(uniform_cells));

	return OpStatus::OK;
}


////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickTabs * tabs)
{
	ParserNodeSequence node;
	RETURN_IF_ERROR(GetNodeFromMap("elements", node));
	if (node.IsEmpty())
	{
		GetLog().OutputEntry("ERROR: Tabs must specify an 'elements' sequence");
		return OpStatus::ERR;
	}

	ParserLogger::AutoIndenter indenter(GetLog(), "Reading tab elements");

	for (ParserSequenceIterator it(node); it; ++it)
	{
		ParserNodeMapping child_node;
		GetUINode()->GetChildNodeByID(it.Get(), child_node);

		QuickWidgetCreator widget_creator(*m_widgets, GetLog(), m_has_quick_window);
		RETURN_IF_ERROR(widget_creator.Init(&child_node));

		OpAutoPtr<QuickWidget> widget;
		RETURN_IF_ERROR(widget_creator.CreateWidget(widget));

		OpString8 name;
		RETURN_IF_ERROR(widget_creator.ReadWidgetName(name));

		OpString title;
		RETURN_IF_ERROR(GetLog().Evaluate(widget_creator.GetTranslatedScalarStringFromMap("title", title), "ERROR: could not retrieve tab title"));
		RETURN_IF_ERROR(tabs->InsertTab(widget.release(), title, name));
	}

	indenter.Done();

	return OpStatus::OK;
}

////////// SetWidgetParameters
OP_STATUS 
QuickWidgetCreator::SetWidgetParameters(QuickPagingLayout * paging_layout)
{
	ParserNodeSequence node;
	RETURN_IF_ERROR(GetNodeFromMap("elements", node));
	if (node.IsEmpty())
	{
		GetLog().OutputEntry("ERROR: Paging Layout must specify an 'elements' sequence");
		return OpStatus::ERR;
	}

	ParserLogger::AutoIndenter indenter(GetLog(), "Reading pages");

	for (ParserSequenceIterator it(node); it; ++it)
	{
		ParserNodeMapping child_node;
		GetUINode()->GetChildNodeByID(it.Get(), child_node);

		QuickWidgetCreator widget_creator(*m_widgets, GetLog(), m_has_quick_window);
		RETURN_IF_ERROR(widget_creator.Init(&child_node));

		OpAutoPtr<QuickWidget> widget;
		RETURN_IF_ERROR(widget_creator.CreateWidget(widget));

		RETURN_IF_ERROR(paging_layout->InsertPage(widget.get()));
		widget.release();
	}

	indenter.Done();

	return OpStatus::OK;
}

////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickLayoutBase * layout)
{
	// A grid/stack without elements is valid in case it needs to be filled
	// in at runtime
	ParserNodeSequence node;
	RETURN_IF_ERROR(GetNodeFromMap("elements", node));

	if (node.IsEmpty())
		GetLog().OutputEntry("WARNING: WrapLayout doesn't have the 'elements' node specified.");

	GetLog().OutputEntry("Reading WrapLayout elements...");
	GetLog().IncreaseIndentation();

	for (ParserSequenceIterator it(node); it; ++it)
	{
		ParserNodeMapping child_node;
		GetUINode()->GetChildNodeByID(it.Get(), child_node);

		QuickWidgetCreator widget_creator(*m_widgets, GetLog(), m_has_quick_window);
		RETURN_IF_ERROR(widget_creator.Init(&child_node));

		OpAutoPtr<QuickWidget> widget;
		RETURN_IF_ERROR(widget_creator.CreateWidget(widget));
		RETURN_IF_ERROR(layout->InsertWidget(widget.release()));
	}

	GetLog().DecreaseIndentation();
	GetLog().OutputEntry("Done reading WrapLayout elements.");

	return OpStatus::OK;
}

////////// SetWidgetParameters
OP_STATUS
QuickWidgetCreator::SetWidgetParameters(QuickCentered * quick_centered)
{
	return SetContent(quick_centered, "Centered");
}

bool
QuickWidgetCreator::NeedsTestSupport()
{
	return g_scope_manager->desktop_window_manager->IsEnabled() || g_pcui && g_pcui->GetIntegerPref(PrefsCollectionUI::UIPropertyExaminer); 
}
