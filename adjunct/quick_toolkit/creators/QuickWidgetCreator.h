/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef QUICK_WIDGET_CREATOR_H
#define QUICK_WIDGET_CREATOR_H

#include "adjunct/quick_toolkit/creators/QuickUICreator.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetDecls.h"
#include "adjunct/quick_toolkit/widgets/QuickBackgroundWidget.h"

class GenericGrid;
class TypedObjectCollection;

/*
 * 
 */
class QuickWidgetStateCreator : public QuickUICreator
{
public:
	QuickWidgetStateCreator(ParserLogger& log) : QuickUICreator(log) {}
	OP_STATUS	CreateWidgetStateAttributes(OpString & text, OpAutoPtr<OpInputAction>& action, OpString8 & widget_image, INT32& data);
};

/*
 * @brief Class that creates a tree of QuickWidgets based on a YAML document and starting node.
 *
 * To be used like this:
 *  QuickWidgetCreator creator;
 *  RETURN_IF_ERROR(creator.Init(content_node));
 *  QuickWidget * widget = NULL;
 *  RETURN_IF_ERROR(creator.CreateWidget(widget));
 */
class QuickWidgetCreator : public QuickUICreator
{
public:
	QuickWidgetCreator(TypedObjectCollection& widgets, ParserLogger& log, bool dialog_reader);
	~QuickWidgetCreator();

	/** Create a widget from the node that this creator was initialized with
	  * @param widget On success, will contain newly created widget
	  * @return status
	  */
	OP_STATUS	CreateWidget(OpAutoPtr<QuickWidget>& widget);

private:
	OP_STATUS			ConstructInitializedWidgetForType(QuickWidget *& widget);

	OP_STATUS			ConstructStackLayout(QuickWidget*& layout);

	template<typename Type>
		OP_STATUS ConstructInitWidget(QuickWidget*& widget);

	template<typename Type, typename Arg>
		OP_STATUS ConstructInitWidget(QuickWidget*& widget, Arg init_argument);

	OP_STATUS	SetGenericWidgetParameters(QuickWidget * widget);

	template<typename OpWidgetType>
		OP_STATUS SetWidgetName(QuickOpWidgetWrapperNoInit<OpWidgetType> * widget);

	template<typename OpWidgetType>
		OP_STATUS SetWidgetName(QuickBackgroundWidget<OpWidgetType> * widget);

	OP_STATUS	SetWidgetName(QuickWidget * widget);

	OP_STATUS	SetTextWidgetParameters(QuickTextWidget * widget);
	OP_STATUS	SetTextWidgetParameters(...) { return OpStatus::OK; }

	OP_STATUS	SetWidgetSkin(GenericQuickOpWidgetWrapper * widget);
	OP_STATUS	SetWidgetSkin(...) { return OpStatus::OK; }

	template<typename OpWidgetType>
		OP_STATUS SetReadOnly(OpWidgetType * widget);

	template<typename OpWidgetType>
		OP_STATUS SetGhostText(OpWidgetType * widget);

	template<typename Type>
		OP_STATUS SetContent(Type* widget, const OpStringC8& widget_name);

	// Widget-specific initialization
	OP_STATUS	SetWidgetParameters(QuickAddressDropDown * drop_down);
	OP_STATUS	SetWidgetParameters(QuickButton * button);
	OP_STATUS	SetWidgetParameters(QuickButtonStrip * button_strip);
	OP_STATUS	SetWidgetParameters(QuickComposite * composite);
	OP_STATUS	SetWidgetParameters(QuickDesktopFileChooserEdit * chooser);
	OP_STATUS	SetWidgetParameters(QuickDropDown * drop_down);
	OP_STATUS	SetWidgetParameters(QuickEdit * edit);
	OP_STATUS   SetWidgetParameters(QuickMultilineEdit * edit);
	OP_STATUS	SetWidgetParameters(QuickExpand * expand);
	OP_STATUS	SetWidgetParameters(QuickSelectable * selectable);
	OP_STATUS	SetWidgetParameters(QuickStackLayout * stack_layout);
	OP_STATUS	SetWidgetParameters(QuickGrid * grid_layout);
	OP_STATUS	SetWidgetParameters(QuickDynamicGrid * grid_layout);
	OP_STATUS	SetWidgetParameters(GenericGrid * grid_layout);
	OP_STATUS	SetWidgetParameters(QuickGroupBox * group_box);
	OP_STATUS	SetWidgetParameters(QuickScrollContainer * scroll);
	OP_STATUS	SetWidgetParameters(QuickSkinElement * skin_element);
	OP_STATUS	SetWidgetParameters(QuickTabs * tabs);
	OP_STATUS	SetWidgetParameters(QuickToggleButton * toggle_button);
	OP_STATUS	SetWidgetParameters(QuickPagingLayout * paging_layout);
	OP_STATUS   SetWidgetParameters(QuickLabel* label);
	OP_STATUS   SetWidgetParameters(QuickMultilineLabel* label);
	OP_STATUS   SetWidgetParameters(QuickIcon* icon);
	OP_STATUS	SetWidgetParameters(QuickRichTextLabel* rich_text_label);
	OP_STATUS	SetWidgetParameters(QuickCentered* quick_centered);
	OP_STATUS	SetWidgetParameters(QuickLayoutBase * layout);
	OP_STATUS	SetWidgetParameters(QuickWrapLayout * layout);
	OP_STATUS	SetWidgetParameters(OpButton * button);
	OP_STATUS	SetWidgetParameters(...) { return OpStatus::OK; }

	OP_STATUS	ReadWidgetName(OpString8& name);
	inline static bool NeedsTestSupport();

	TypedObjectCollection*	m_widgets;
	bool					m_has_quick_window;
};

#endif // QUICK_WIDGET_CREATOR_H
