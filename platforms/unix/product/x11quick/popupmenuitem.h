/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#ifndef __POPUP_MENU_ITEM__
#define __POPUP_MENU_ITEM__

#include "modules/widgets/OpWidget.h"
#include "platforms/quix/toolkits/ToolkitLibrary.h"

class OpWidgetImage;
class PopupMenu;


class PopupMenuComponent : public Link
{
public:
	struct PaintProperties
	{
		OpFont* normal_font;
		OpFont* bold_font;
		unsigned font_number;
		int max_image_width;
		int max_image_height;
		int max_text_width;
	};

	virtual bool IsSelectable() const = 0;
	virtual bool IsSeparator() const = 0;
	virtual PopupMenu* GetSubMenu() const = 0;
	virtual OpInputAction* TakeInputAction() = 0;
	virtual OpInputAction* GetInputAction() const = 0; 
	virtual uni_char GetAcceleratorKey() const = 0;
	virtual int Paint(OpPainter* painter, const OpRect& rect, bool selected, const PaintProperties& properties) = 0;

};

class PopupMenuItem : public PopupMenuComponent
{
public:
	enum MenuItemState
	{
		MI_CHECKED   = 1 << 0,
		MI_SELECTED  = 1 << 1,
		MI_DISABLED  = 1 << 2,
		MI_BOLD      = 1 << 3
	};

	PopupMenuItem(UINT32 state, OpInputAction* action, PopupMenu* submenu, OpWidget* widget);
	~PopupMenuItem();

	OP_STATUS SetName(const OpStringC& name);
	OP_STATUS SetShortcut(const OpStringC& shortcut) { return m_shortcut.Set(shortcut.CStr(), m_widget); }
	OP_STATUS SetWidgetImage(const OpWidgetImage* image);

	OpWidgetString& GetName() { return m_name; }
	OpWidgetString& GetShortcut() { return m_shortcut; }

	virtual bool IsSelectable() const { return !(m_state&MI_DISABLED); }
	virtual bool IsSeparator() const { return false; }
	virtual PopupMenu* GetSubMenu() const { return m_submenu; }
	virtual OpInputAction* TakeInputAction();
	virtual OpInputAction* GetInputAction() const { return m_action; } 
	virtual uni_char GetAcceleratorKey() const;
	virtual int Paint(OpPainter* painter, const OpRect& rect, bool selected, const PaintProperties& properties);


	bool IsEnabled() const { return IsSelectable(); }
	bool IsChecked();
	bool IsSelected(); 
	bool IsBold();

public:
	// We should scale all images that are bigger (width or height)
	static const unsigned MaxImageSize = 22;
	static ToolkitLibrary::PopupMenuLayout m_layout;

private:
	/**
	 * @return the rect for a menu item's element, adjusted for UI direction
	 */
	OpRect AdjustChildRect(const OpRect& child_rect, const OpRect& parent_rect) const;

	OpWidgetString m_name;
	OpWidgetString m_shortcut;
	OpInputAction* m_action;
	OpWidgetImage* m_image;
	PopupMenu* m_submenu;
	OpWidget* m_widget;
	UINT32 m_state; // Populated with MenuItemState
	uni_char m_accelerator_key;
};

class PopupMenuSeparator : public PopupMenuComponent
{
public:
	virtual bool IsSelectable() const { return FALSE; }
	virtual bool IsSeparator() const { return TRUE; }
	virtual PopupMenu* GetSubMenu() const { return 0; }
	virtual OpInputAction* TakeInputAction() { return 0; }
	virtual OpInputAction* GetInputAction() const { return 0; }
	virtual uni_char GetAcceleratorKey() const { return 0; }
	virtual int Paint(OpPainter* painter, const OpRect& rect, bool selected, const PaintProperties& properties);

	static int GetItemHeight();

	static const int SeparatorHeight = 4;  // Default value
};

#endif
