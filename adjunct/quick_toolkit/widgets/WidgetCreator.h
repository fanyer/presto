/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef WIDGET_CREATOR_H
#define WIDGET_CREATOR_H

#include "modules/locale/locale-enum.h"
#include "modules/util/OpTypedObject.h"
#include "modules/widgets/OpWidget.h"

class OpIcon;
class OpHelpTooltip;
class OpLabel;

/** @brief A fire-once class to create a widget based on data parsed from a string
  * Example usage:
  *
  * WidgetCreator creator;
  * creator.SetWidgetData(UNI_L("Button, DI_STRING, NiceButton, 0, 0, 10, 10, Fixed"));
  * creator.SetActionData(UNI_L("Delete"));
  * creator.SetParent(parent);
  * OpWidget* new_widget = creator.CreateWidget();
  */
class WidgetCreator
{
public:
	WidgetCreator();
	~WidgetCreator();

	/** Retrieve widget setup data from a text string
	  * @param widget The string with setup data to use
	  * The format of the text string is as follows:
	  * widget			= widget-type ',' widget-string ',' widget-name ',' widget-coordinates ',' widget-resize [ ',' widget-end ]
	  * widget-type		= string ; see WidgetCreator.cpp for list of widget types
	  * widget-string	= string-id / string ; string to use on widget
	  * widget-name		= string ; name that can be used to identify widget
	  * widget-coordinates = top ',' left ',' width ',' height ; location and size of widget in pixels, relative to parent widget or dialog
	  * widget-resize   = string ; see WidgetCreator.cpp for list of resize possibilities
	  * widget-end		= "End" ; Use this to signify that this widget forme the end of a group
	  */
	OP_STATUS SetWidgetData(const uni_char* widget);

	/** Retrieve action data to use on this widget from a text string
	  * @param action The string with action data to use
	  * The format of the text string is as follows:
	  * action-list 	= action *( action-operator action )
	  * action			= action-name [ ',' action-data [ ',' action-string [ ',' action-image ] ] ]
	  * action-data 	= string ',' integer / integer ',' string ; the action is free to use this data in any way it wants
	  * action-string   = string-id | string ; string to associate with action for display in UI
	  * action-image	= string ; Name of skin element to associate with action for display in UI
	  * action-operator = '|' / '&' / '>' / '+' ; or, and, next, plus, to connect several actions
	  */
	void SetActionData(const uni_char* action);

	/** Set parent for widget to create
	  */
	void SetParent(OpWidget* parent) { m_parent = parent; }

	/** Create a widget conforming to specifications given by Set*() functions
	  * @return A newly created widget owned by its parent, or NULL if something went wrong
	  */
	OpWidget* CreateWidget();

	/** @return Whether the widget created by this WidgetCreator formed the end of a group
	  */
	BOOL IsEndWidget() const { return m_end; }

private:
	OpWidget* ConstructWidgetForType();
	INT32 GetResizeFlagsFromString(const OpStringC8& name);
	RESIZE_EFFECT GetXResizeEffect(INT32 resize_flags);
	RESIZE_EFFECT GetYResizeEffect(INT32 resize_flags);
	OpTypedObject::Type GetWidgetTypeFromString(const OpStringC8& widget_name);
	template<typename T> T* ConstructWidget();
	template<typename T, typename U> T* ConstructWidget(U construct_argument);

	// Widget-specific hacks when constructing
	void DoWidgetSpecificHacks(OpWidget*) {}
	void DoWidgetSpecificHacks(OpButton* widget);
	void DoWidgetSpecificHacks(OpLabel* label);
	void DoWidgetSpecificHacks(OpBrowserView* browser_view);
	void DoWidgetSpecificHacks(OpIcon* icon);
	void DoWidgetSpecificHacks(OpMultilineEdit* widget);
	void DoWidgetSpecificHacks(OpHelpTooltip* tooltip);

	OpTypedObject::Type m_widget_type;
	OpString m_text;
	Str::LocaleString m_string_id;
	OpString8 m_name;
	OpRect m_rect;
	INT32 m_resize_flags;
	BOOL m_end;
	INT32 m_value;
	OpInputAction* m_action;
	OpWidget* m_parent;
};

#endif // WIDGET_CREATOR_H
