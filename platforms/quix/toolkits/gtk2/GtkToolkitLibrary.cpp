/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/gtk2/GtkToolkitLibrary.h"

#include "platforms/quix/toolkits/gtk2/GtkSkinElement.h"
#include "platforms/quix/toolkits/gtk2/GtkColorChooser.h"
#include "platforms/quix/toolkits/gtk2/GtkFileChooser.h"
#include "platforms/quix/toolkits/ToolkitMainloopRunner.h"
#include "platforms/quix/toolkits/gtk2/GtkPrinterIntegration.h"
#include "platforms/quix/toolkits/gtk2/GtkWidgetPainter.h"
#include <X11/Xlib.h>
#include <locale.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#ifdef GTK3
static void style_set_cb(GtkWidget *widget, GtkStyle *old_style, gpointer data);
#endif

GtkToolkitLibrary* GtkToolkitLibrary::s_instance = 0;

ToolkitLibrary* CreateToolkitLibrary()
{
	return new GtkToolkitLibrary();
}

GtkToolkitLibrary::GtkToolkitLibrary()
	: m_window(0)
	, m_proto_layout(0)
	, m_settings(0)
	, m_timer_id(0)
	, m_widget_painter(0)
	, m_runner(0)
#ifdef GTK3
	, m_is_style_changed(false)
#endif
{
	m_gtk_version[0] = 0;
	s_instance = this;
}

GtkToolkitLibrary::~GtkToolkitLibrary()
{
	delete m_widget_painter;
	delete m_settings;

	if (m_window)
		gtk_widget_destroy(m_window);

#ifndef GTK3
	GdkDisplay* display = gdk_display_get_default();
	if (display)
		gdk_display_close(display);
#endif
}

bool GtkToolkitLibrary::Init(X11Types::Display* display)
{
	// Save X error handler and locale - GTK might steal them
	XErrorHandler handler = XSetErrorHandler(0);
	const char* locale = setlocale(LC_ALL, 0);
	char* locale_backup = 0;
	if (locale)
		locale_backup = strndup(locale, 50);

	bool success = gtk_init_check(0, 0);

	// Restore X error handler and locale
	if (locale_backup)
	{
		setlocale(LC_ALL, locale_backup);
		free(locale_backup);
	}
	XSetErrorHandler(handler);

	if (!success)
	{
		fprintf(stderr, "GtkToolkit: error: gtk_init_check failed!\n");
		return false;
	}

#ifndef GTK3
	// Gtk2 Adwaita uses engine "adwaita", override (this is a hack for DSK-378831)
	gtk_rc_parse_string("style \"menu_framed_box\" { engine \"pixmap\" {} }");
#endif

	m_window = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_widget_realize(m_window);
	m_proto_layout = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(m_window), m_proto_layout);

	current_style = gtk_widget_get_style(m_window);

	m_settings = new GtkToolkitUiSettings;
	if (!m_settings || !m_settings->Init(m_proto_layout))
	{
		fprintf(stderr, "GtkToolkit: error: Couldn't initialize GtkSettings!\n");
		return false;
	}

	m_widget_painter = new GtkWidgetPainter();
	if (!m_widget_painter)
		return false;

	m_settings->ChangeStyle(current_style);

#ifdef GTK3
	g_signal_connect(m_window, "style-set", G_CALLBACK (style_set_cb), this);
#endif

	return true;
}

NativeSkinElement* GtkToolkitLibrary::GetNativeSkinElement(NativeSkinElement::NativeType type)
{
	using namespace GtkSkinElements;

	GtkSkinElement* skin_element = 0;

	switch (type)
	{
		case NativeSkinElement::NATIVE_SCROLLBAR_VERTICAL_UP:
			skin_element = new ScrollbarDirection(ScrollbarDirection::UP); break;
		case NativeSkinElement::NATIVE_SCROLLBAR_VERTICAL_DOWN:
			skin_element = new ScrollbarDirection(ScrollbarDirection::DOWN); break;
		case NativeSkinElement::NATIVE_SCROLLBAR_HORIZONTAL_LEFT:
			skin_element = new ScrollbarDirection(ScrollbarDirection::LEFT); break;
		case NativeSkinElement::NATIVE_SCROLLBAR_HORIZONTAL_RIGHT:
			skin_element = new ScrollbarDirection(ScrollbarDirection::RIGHT); break;
		case NativeSkinElement::NATIVE_SCROLLBAR_HORIZONTAL_KNOB:
			skin_element = new ScrollbarKnob(ScrollbarKnob::HORIZONTAL); break;
		case NativeSkinElement::NATIVE_SCROLLBAR_VERTICAL_KNOB:
			skin_element = new ScrollbarKnob(ScrollbarKnob::VERTICAL); break;
		case NativeSkinElement::NATIVE_SCROLLBAR_HORIZONTAL:
			skin_element = new ScrollbarBackground(ScrollbarBackground::HORIZONTAL); break;
		case NativeSkinElement::NATIVE_SCROLLBAR_VERTICAL:
			skin_element = new ScrollbarBackground(ScrollbarBackground::VERTICAL); break;
		case NativeSkinElement::NATIVE_DROPDOWN_BUTTON:
		case NativeSkinElement::NATIVE_DROPDOWN_LEFT_BUTTON:
			skin_element = new DropdownButton(); break;
		case NativeSkinElement::NATIVE_DROPDOWN:
			skin_element = new Dropdown(); break;
		case NativeSkinElement::NATIVE_DROPDOWN_EDIT:
			skin_element = new DropdownEdit(); break;
		case NativeSkinElement::NATIVE_PUSH_BUTTON:
			skin_element = new PushButton(false); break;
		case NativeSkinElement::NATIVE_PUSH_DEFAULT_BUTTON:
			skin_element = new PushButton(true); break;
		case NativeSkinElement::NATIVE_RADIO_BUTTON:
			skin_element = new RadioButton(); break;
		case NativeSkinElement::NATIVE_CHECKBOX:
			skin_element = new CheckBox(); break;
		case NativeSkinElement::NATIVE_EDIT:
			skin_element = new EditField(); break;
		case NativeSkinElement::NATIVE_MULTILINE_EDIT:
		case NativeSkinElement::NATIVE_LISTBOX:
		case NativeSkinElement::NATIVE_TREEVIEW:
			skin_element = new MultiLineEditField(); break;
		case NativeSkinElement::NATIVE_BROWSER:
			skin_element = new Browser(); break;
		case NativeSkinElement::NATIVE_DIALOG:
			skin_element = new Dialog; break;
		case NativeSkinElement::NATIVE_DIALOG_PAGE:
			skin_element = new DialogPage; break;
		case NativeSkinElement::NATIVE_DIALOG_BUTTON_BORDER:
			skin_element = new DialogButtonStrip; break;
		case NativeSkinElement::NATIVE_DIALOG_TAB_PAGE:
			skin_element = new DialogTabPage(); break;
		case NativeSkinElement::NATIVE_TAB_BUTTON:
			skin_element = new TabButton(); break;
		case NativeSkinElement::NATIVE_TABS:
			skin_element = new TabSeparator(); break;
		case NativeSkinElement::NATIVE_MENU:
			skin_element = new Menu(); break;
		case NativeSkinElement::NATIVE_MENU_BUTTON:
			skin_element = new MenuButton(); break;
		case NativeSkinElement::NATIVE_MENU_RIGHT_ARROW:
			skin_element = new MenuRightArrow(); break;
		case NativeSkinElement::NATIVE_POPUP_MENU:
			skin_element = new PopupMenu(); break;
		case NativeSkinElement::NATIVE_POPUP_MENU_BUTTON:
			skin_element = new PopupMenuButton(); break;
		case NativeSkinElement::NATIVE_LISTITEM:
			skin_element = new ListItem(); break;
		case NativeSkinElement::NATIVE_SLIDER_HORIZONTAL_TRACK:
			skin_element = new SliderTrack(true); break;
		case NativeSkinElement::NATIVE_SLIDER_VERTICAL_TRACK:
			skin_element = new SliderTrack(false); break;
		case NativeSkinElement::NATIVE_SLIDER_HORIZONTAL_KNOB:
			skin_element = new SliderKnob(true); break;
		case NativeSkinElement::NATIVE_SLIDER_VERTICAL_KNOB:
			skin_element = new SliderKnob(false); break;
		case NativeSkinElement::NATIVE_HEADER_BUTTON:
			skin_element = new HeaderButton(); break;
		case NativeSkinElement::NATIVE_BROWSER_WINDOW:
		case NativeSkinElement::NATIVE_WINDOW:
			skin_element = new Toolbar(); break;
	    case NativeSkinElement::NATIVE_TOOLTIP:		
			skin_element = new Tooltip(); break;
	    case NativeSkinElement::NATIVE_MENU_SEPARATOR:
			skin_element = new MenuSeparator(); break;
	    case NativeSkinElement::NATIVE_MAINBAR_BUTTON:
			skin_element = new MainbarButton(); break;
	    case NativeSkinElement::NATIVE_PERSONALBAR_BUTTON:
			skin_element = new PersonalbarButton(); break;
		case NativeSkinElement::NATIVE_LABEL:
			skin_element = new Label(); break;
#ifdef DEBUG
		default:
			fprintf(stderr, "Not implemented: %d, "
							"uncomment following line to use red\n", type);
			// uncommenting this line will cause selftest to fail in skin module
			// skin_element = new RedElement(); break;
#endif
	}

	if (skin_element)
		skin_element->SetLayout(m_proto_layout);

	return skin_element;
}

bool GtkToolkitLibrary::IsStyleChanged()
{

	SetCanCallRunSlice(false);
	GtkUtils::ProcessEvents();
	SetCanCallRunSlice(true);

	GtkStyle *tmp_style = gtk_widget_get_style(m_window);
	if (tmp_style != current_style)
	{
		current_style = tmp_style;;
		m_settings->ChangeStyle(tmp_style);
		return true;
	}

#ifdef GTK3
	if (m_is_style_changed)
	{
		m_is_style_changed = false;
		return true;
	}
#endif

	return false;
}

ToolkitWidgetPainter* GtkToolkitLibrary::GetWidgetPainter()
{
	return m_widget_painter;
}

ToolkitColorChooser* GtkToolkitLibrary::CreateColorChooser()
{
	return new GtkToolkitColorChooser();
}

ToolkitFileChooser* GtkToolkitLibrary::CreateFileChooser()
{
	return new GtkToolkitFileChooser();
}

void GtkToolkitLibrary::SetMainloopRunner(ToolkitMainloopRunner* runner)
{
	m_runner = runner;

	SetCanCallRunSlice(m_runner != 0);
}

void GtkToolkitLibrary::SetCanCallRunSlice(bool value)
{
	if (value && !s_instance->m_timer_id)
	{
		s_instance->m_timer_id = g_timeout_add(0, &RunSlice, 0);
	}
	else if (!value && s_instance->m_timer_id)
	{
		g_source_remove(s_instance->m_timer_id);
		s_instance->m_timer_id = 0;
	}
}

const char* GtkToolkitLibrary::ToolkitInformation()
{
	if (!m_gtk_version[0])
	{
		snprintf(m_gtk_version, sizeof(m_gtk_version)-64, "Gtk %d.%d.%d using ", gtk_major_version, gtk_minor_version, gtk_micro_version);
		m_settings->GetThemeName(m_gtk_version + strlen(m_gtk_version));
	}
	return m_gtk_version;
}

void GtkToolkitLibrary::GetPopupMenuLayout(PopupMenuLayout& layout)
{
	layout.arrow_overlap = 8;
	layout.arrow_width = 16;
	layout.text_margin = 15;
	layout.frame_thickness = 0;
	layout.right_margin = 1;
}

gboolean GtkToolkitLibrary::RunSlice(gpointer data)
{
	if (!s_instance->m_runner)
		return FALSE;

	unsigned waitms = s_instance->m_runner->RunSlice();

	if (s_instance->m_timer_id)
		g_source_remove(s_instance->m_timer_id);

	if (waitms != UINT_MAX)
		s_instance->m_timer_id = g_timeout_add(waitms, &RunSlice, 0);
	else
		s_instance->m_timer_id = 0;

	return FALSE;
}

ToolkitPrinterIntegration* GtkToolkitLibrary::CreatePrinterIntegration()
{
	return new GtkPrinterIntegration(m_window);
}

#ifdef GTK3
static void style_set_cb(GtkWidget *widget, GtkStyle *old_style, gpointer data)
{
	GtkToolkitLibrary* toolkit_library = (GtkToolkitLibrary*) data;
	toolkit_library->SetIsStyleChanged(true);
}
#endif
