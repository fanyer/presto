/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef GTK_SKIN_ELEMENT_H
#define GTK_SKIN_ELEMENT_H

#include "platforms/quix/toolkits/NativeSkinElement.h"
#include "platforms/quix/toolkits/gtk2/GtkPorting.h"
#include <gtk/gtk.h>

#ifdef GTK3
# define OP_PI (3.14159265)
#endif

class GtkSkinElement : public NativeSkinElement
{
public:
	GtkSkinElement() : m_layout(0), m_widget(0), m_all_widgets(0) {}
	virtual ~GtkSkinElement();

	void SetLayout(GtkWidget* layout) { m_layout = layout; }

	virtual void Draw(uint32_t* bitmap, int width, int height, const NativeRect& clip_rect, int state);
	virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state) {}
	virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state);
	virtual void ChangeDefaultSize(int& width, int& height, int state) {}
	virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) {}
	virtual void ChangeDefaultSpacing(int &spacing, int state) {}

protected:
	GdkWindow* GetGdkWindow() { return IsTopLevel() ? gtk_widget_get_window(m_widget) : gtk_widget_get_parent_window(m_widget); }

	virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state) = 0;
	virtual GtkWidget* CreateWidget() = 0;
	virtual bool RespectAlpha() { return true; }
	virtual bool IsTopLevel() { return false; }

	void ChangeTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state);
	bool CreateInternalWidget();

	/**
	 * Return style name, that should be passed to gtk_style_context_add_class in order to access
	 * widget-specific properties (e.g font colour).
	 */
	virtual const char* GetStyleContextClassName() const { return NULL; }

	virtual GtkWidget* GetTextWidget() const { return NULL; }

#ifdef GTK3
	virtual GtkStateFlags GetGtkStateFlags(int state);
	void CairoDraw(uint32_t* bitmap, int width, int height, GtkStyle* style, int state);
#else
	void DrawWithAlpha(uint32_t* bitmap, int width, int height, GdkRectangle& clip_rect, GtkStyle* style, int state);
	void DrawSolid(uint32_t* bitmap, int width, int height, GdkRectangle& clip_rect, GtkStyle* style, int state);
#if defined(GTK2_DEPRECATED)
	GdkPixbuf* DrawOnBackground(GdkGC* background_gc, int width, int height, GdkRectangle& clip_rect, GtkStyle* style, int state);
#else
	GdkPixbuf* DrawOnBackground(double red, double green, double blue, int width, int height, GdkRectangle& clip_rect, GtkStyle* style, int state);
#endif // GTK2_DEPRECATED
	static inline uint32_t GetARGB(guchar* pixel, uint8_t alpha = 0xff) { return (alpha << 24) | (pixel[0] << 16) | (pixel[1] << 8) | pixel[2]; }
	static inline uint32_t GetARGB(guchar* black_pixel, guchar* white_pixel) { return GetARGB(black_pixel, black_pixel[0] - white_pixel[0] + 255); }
#endif // !GTK3
	virtual GtkStateType GetGtkState(int state);

	static int Round(double num);
#ifdef DEBUG
	static void PrintState(int state);
#endif
	static void RealizeSubWidgets(GtkWidget* widget, gpointer widget_table);

	GtkWidget* m_layout;
	GtkWidget* m_widget;
	GHashTable* m_all_widgets;
};

namespace GtkSkinElements
{
	class EmptyElement : public GtkSkinElement
	{
		virtual void Draw(uint32_t* bitmap, int width, int height, const NativeRect& clip_rect, int state);
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state) {}
		virtual GtkWidget* CreateWidget() { return 0; }
	};

	class ScrollbarElement : public GtkSkinElement
	{
	public:
		enum Orientation
		{
			HORIZONTAL,
			VERTICAL
		};
	
		ScrollbarElement(Orientation orientation) : m_orientation(orientation) {}
		virtual GtkWidget* CreateWidget()
		{
#ifdef GTK3
			return gtk_scrollbar_new(m_orientation == VERTICAL ? GTK_ORIENTATION_VERTICAL : GTK_ORIENTATION_HORIZONTAL, NULL);
#else
			return m_orientation == VERTICAL ? gtk_vscrollbar_new(0) : gtk_hscrollbar_new(0);
#endif
		}
	
	protected:
		Orientation m_orientation;
	};
	
	class ScrollbarDirection : public ScrollbarElement
	{
	public:
		enum Direction
		{
			UP,
			DOWN,
			LEFT,
			RIGHT
		};
	
		ScrollbarDirection(Direction direction) : ScrollbarElement(direction < LEFT ? VERTICAL : HORIZONTAL), m_direction(direction) {}
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
	
	private:
		GtkArrowType GetArrow();
		Direction m_direction;
	};
	
	class ScrollbarKnob : public ScrollbarElement
	{
	public:
		ScrollbarKnob(Orientation orientation) : ScrollbarElement(orientation) {}
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
	};
	
	class ScrollbarBackground : public ScrollbarElement
	{
	public:
		ScrollbarBackground(Orientation orientation) : ScrollbarElement(orientation) {}
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual bool RespectAlpha() { return false; }
		virtual void ChangeDefaultSize(int& width, int& height, int state);
	};
	
	class DropdownButton : public EmptyElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state) {}
		virtual GtkWidget* CreateWidget() { return gtk_combo_box_new_with_entry(); }
		virtual void ChangeDefaultSize(int& width, int& height, int state);
	};

	class Dropdown : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget() { return gtk_combo_box_new(); }
		virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
		virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) { ChangeTextColor(red, green, blue, alpha, state); }
	};
	
	class DropdownEdit : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
		virtual GtkWidget* CreateWidget()
		{
			// unfortunately, we can't use gtk_combo_box_entry_new as drop-in replacement in here
			// it has different style and completely different arrow
#ifdef GTK3
			return gtk_combo_box_new_with_entry();
#else
			return gtk_combo_box_entry_new();
#endif
		}
	};
	
	class PushButton : public GtkSkinElement
	{
	public:
		PushButton(bool focused) : m_focused(focused) {}
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget() { return gtk_button_new_with_label("OK"); }
		virtual void ChangeDefaultSize(int& width, int& height, int state);
		virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) { ChangeTextColor(red, green, blue, alpha, state); }

		bool m_focused;
	};
	
	class RadioButton : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget() { return gtk_radio_button_new(0); }
	};
	
	class CheckBox : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget() { return gtk_check_button_new(); }
	};
	
	class EditField : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget() { return gtk_entry_new(); }
		virtual bool RespectAlpha() { return false; } // DSK-316828
	};

	class MultiLineEditField : public EditField
	{
		virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
	};

	class Label : public EmptyElement
	{
		virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state);
	};

	class Browser : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget() { return gtk_hbox_new(1, 1); }
	};

	class Dialog : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
		virtual GtkWidget* CreateWidget() { return gtk_label_new(NULL); }
	};

	class DialogPage : public EmptyElement
	{
		virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state) { left = top = right = bottom = 0; }
		virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state);
	};

	class DialogButtonStrip : public EmptyElement
	{
		virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state) { left = top = right = bottom = 0; }
		virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state);
	};
	
	class DialogTabPage : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
		virtual GtkWidget* CreateWidget() { return gtk_notebook_new(); }
	};
	
	class TabButton : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
		virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state);
		virtual GtkWidget* CreateWidget() { return gtk_notebook_new(); }
	};
	
	class TabSeparator : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state) { left = 0; }
		virtual GtkWidget* CreateWidget() { return gtk_notebook_new(); }
	};
	
	class Menu : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget() { return gtk_menu_bar_new(); }
	};
	
	class MenuButton : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) { ChangeTextColor(red, green, blue, alpha, state); }
		virtual GtkWidget* CreateWidget();
		GtkWidget* m_window;

#ifdef GTK3
		virtual GtkStateFlags GetGtkStateFlags(int state);
		virtual GtkWidget* GetTextWidget() const { return gtk_bin_get_child(GTK_BIN(m_widget)); }
#else
		virtual GtkStateType GetGtkState(int state);
#endif
	
	public:
		MenuButton() : m_window(0) {}
		~MenuButton() { gtk_widget_destroy(m_window); m_window = 0; m_widget = 0; }

	};
	
	class PopupMenu : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget() { return gtk_menu_new(); }
	};
	
	class PopupMenuButton : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) { ChangeTextColor(red, green, blue, alpha, state); }
	protected:
		virtual GtkWidget* CreateWidget();
		GtkWidget* m_menu;
	public:
		PopupMenuButton() : m_menu(0) {}
		~PopupMenuButton(){ gtk_widget_destroy(m_menu);  m_menu = 0; m_widget = 0; /* m_widget is a child of m_menu*/ }
	};

	class MenuRightArrow : public PopupMenuButton
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
	};

	class ListItem : public PopupMenuButton
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) { ChangeTextColor(red, green, blue, alpha, state); }
		virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state) {}
	};

	class SliderTrack : public GtkSkinElement
	{
	public:
		SliderTrack(bool horizontal) : GtkSkinElement(),m_horizontal(horizontal){}
	private:
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultSize(int& width, int& height, int state)
		{ 
			if(m_horizontal)
			{
				width  = 0; 
				height = 5; 
			}
			else
			{
				width  = 5;
				height = 0;
			}
		}
		virtual GtkWidget* CreateWidget() 
		{ 
			if (m_horizontal)
				return gtk_hscale_new_with_range(0.0, 100.0, 1.0);
			else
				return gtk_vscale_new_with_range(0.0, 100.0, 1.0);
		}
	private:
		bool m_horizontal;
	};

	class SliderKnob : public GtkSkinElement
	{
	public:
		SliderKnob(bool horizontal) : GtkSkinElement(),m_horizontal(horizontal){}
	private:
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultSize(int& width, int& height, int state);

		virtual GtkWidget* CreateWidget() 
		{ 
			if (m_horizontal)
				return gtk_hscale_new_with_range(0.0, 100.0, 1.0);
			else
				return gtk_vscale_new_with_range(0.0, 100.0, 1.0);
		}
	private:
		bool m_horizontal;
	};

	class HeaderButton : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget();
		virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) { ChangeTextColor(red, green, blue, alpha, state); }
	};

	class Toolbar : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget() { return gtk_toolbar_new(); }
	};

    class MainbarButton : public GtkSkinElement
	{
	public:
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) { ChangeTextColor(red, green, blue, alpha, state); }
		virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
		virtual void ChangeDefaultSpacing(int &spacing, int state);
		virtual GtkWidget* CreateWidget() {return gtk_button_new(); }
	};
	
	class PersonalbarButton : public MainbarButton
	{
	};

	class Tooltip : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget();
		virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) { ChangeTextColor(red, green, blue, alpha, state); }
		virtual bool IsTopLevel() { return true; }
		virtual const char* GetStyleContextClassName() const { return "tooltip"; }
	};

	class MenuSeparator : public GtkSkinElement
	{
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state);
		virtual GtkWidget* CreateWidget() { return gtk_separator_menu_item_new(); }
		void ChangeDefaultSize(int& width, int& height, int state);
	};


#ifdef DEBUG
	/** @brief Debug skin element that draws RED everywhere */
	class RedElement : public GtkSkinElement
	{
		virtual void Draw(uint32_t* bitmap, int width, int height, const NativeRect& clip_rect, int state);
		virtual void GtkDraw(op_gdk_surface *pixmap, int width, int height, GdkRectangle& clip_rect, GtkWidget* widget, GtkStyle* style, int state) {}
		virtual GtkWidget* CreateWidget() { return 0; }

	};
#endif // DEBUG
};

#endif // GTK_SKIN_ELEMENT_H
