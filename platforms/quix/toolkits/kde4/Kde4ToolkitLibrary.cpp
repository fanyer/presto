/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 * @author Rune Myrland (runem)
 */

#include "platforms/quix/toolkits/kde4/Kde4ToolkitLibrary.h"
#include "platforms/quix/toolkits/kde4/Kde4SkinElement.h"
#include "platforms/quix/toolkits/kde4/Kde4UiSettings.h"
#include "platforms/quix/toolkits/kde4/Kde4ColorChooser.h"
#include "platforms/quix/toolkits/kde4/Kde4FileChooser.h"
#include "platforms/quix/toolkits/kde4/Kde4Mainloop.h"
#include "platforms/quix/toolkits/kde4/Kde4WidgetPainter.h"
#include "platforms/quix/toolkits/kde4/Kde4PrinterIntegration.h"

#include <KDE/KApplication>
#include <KDE/KCmdLineArgs>
#include <KDE/KService>

#include <QDebug>
#include <QStyleOptionSlider>

#include <X11/Xlib.h>

#include <signal.h>

ToolkitLibrary* CreateToolkitLibrary()
{
	return new Kde4ToolkitLibrary();
}

Kde4ToolkitLibrary::Kde4ToolkitLibrary()
	: m_app(0)
	, m_settings(0)
	, m_loop(0)
	, m_widget_painter(0)
	, m_dummywidget(0)
{
	m_kde_version[0] = 0;
}

Kde4ToolkitLibrary::~Kde4ToolkitLibrary()
{
	delete m_widget_painter;
	delete m_settings;
	delete m_app;
	delete m_dummywidget;
}


bool Kde4ToolkitLibrary::Init(X11Types::Display* display)
{
	static int argc = 1;
	static const char* argv[] = { "Kde4ToolkitLibrary", 0 };

	// Save X error handler - GTK might steal it
	XErrorHandler handler = XSetErrorHandler(0);

	// Save current signal handlers - KApplication is about to steal them
	struct sigaction defaultaction;
	defaultaction.sa_handler = SIG_DFL;
	defaultaction.sa_flags = 0;

	struct SigData
	{
		int num;
		struct sigaction action;
	} saved_signals[] =
	{
		{ SIGSEGV, {} },
		{ SIGILL, {} },
		{ SIGFPE, {} },
		{ SIGABRT, {} },
		{ SIGTRAP, {} },
		{ SIGBUS, {} }
	};

	for (unsigned int i=0; i< sizeof(saved_signals)/sizeof(saved_signals[0]); i++)
		sigaction(saved_signals[i].num, &defaultaction, &saved_signals[i].action );

	// Initialize KApplication
	KCmdLineArgs::init(argc, (char**)argv, "Opera", 0, ki18n("Opera"), 0);
	m_app = new KApplication; //(display, argc, (char**)argv, "Opera", true);

	// Restore current signal handlers
	for (unsigned int i=0; i< sizeof(saved_signals)/sizeof(saved_signals[0]); i++)
		sigaction(saved_signals[i].num, &saved_signals[i].action, 0 );

	XSetErrorHandler(handler);

	if (!m_app)
		return false;

	// Current style
	m_stylestring = QString::fromLatin1(m_app->style()->metaObject()->className());

	// Make sure that the event loop gets to process initial events
	m_app->sendPostedEvents();

	m_settings = new Kde4UiSettings(m_app);
	if (!m_settings)
		return false;

	m_widget_painter = new Kde4WidgetPainter(m_app);
	if (!m_widget_painter)
		return false;

	m_loop = new Kde4Mainloop(m_app);
	if (!m_loop)
		return false;

	m_dummywidget = new DummyWidget();
	if (!m_dummywidget)
		return false;

	m_app->installEventFilter(m_dummywidget);

	return true;
}

bool Kde4ToolkitLibrary::IsStyleChanged()
{
	m_loop->SetCanCallRunSlice(false);
	m_app->sendPostedEvents();
   	m_app->processEvents();
	m_loop->SetCanCallRunSlice(true);

	bool is_style_changed = false;
	if (m_dummywidget->HasStyleChanged())
	{
		QString styleString = QString::fromLatin1(qApp->style()->metaObject()->className());
		if (styleString.compare(m_stylestring)) // See DSK-366163
		{
			m_stylestring = styleString;
			is_style_changed = true;
		}
	}
	else if (m_dummywidget->HasPaletteChanged())
		is_style_changed = true;

	m_dummywidget->ResetState();
	return is_style_changed;
}

NativeSkinElement* Kde4ToolkitLibrary::GetNativeSkinElement(NativeSkinElement::NativeType type)
{
	using namespace Kde4SkinElements;

	switch (type) {
	case NativeSkinElement::NATIVE_PUSH_BUTTON:
		return new PushButton;
	case NativeSkinElement::NATIVE_PUSH_DEFAULT_BUTTON:
		return new DefaultPushButton;
	case NativeSkinElement::NATIVE_TAB_BUTTON:
		return new TabButton;
	case NativeSkinElement::NATIVE_TABS:
		return new TabBar;
	case NativeSkinElement::NATIVE_DIALOG_TAB_PAGE:
		return new TabPage;
	case NativeSkinElement::NATIVE_POPUP_MENU_BUTTON:
		return new PopupMenuButton;
	case NativeSkinElement::NATIVE_MENU:
		return new MenuBar;
	case NativeSkinElement::NATIVE_MENU_BUTTON:
		return new MenuButton;
	case NativeSkinElement::NATIVE_POPUP_MENU:
		return new PopupMenu;
	case NativeSkinElement::NATIVE_DIALOG:
	case NativeSkinElement::NATIVE_DIALOG_PAGE:
	case NativeSkinElement::NATIVE_BROWSER_WINDOW:
	case NativeSkinElement::NATIVE_WINDOW:
	case NativeSkinElement::NATIVE_DIALOG_BUTTON_BORDER:
		return new WindowBackground;
	case NativeSkinElement::NATIVE_HEADER_BUTTON:
		return new HeaderButton;

	case NativeSkinElement::NATIVE_RADIO_BUTTON:
		return new RadioButton;
	case NativeSkinElement::NATIVE_CHECKBOX:
		return new CheckBox;

	case NativeSkinElement::NATIVE_DROPDOWN:
		return new DropDown;

	case NativeSkinElement::NATIVE_EDIT:
		return new EditControl;

	case NativeSkinElement::NATIVE_DROPDOWN_BUTTON:
	case NativeSkinElement::NATIVE_DROPDOWN_LEFT_BUTTON:
		return new DropDownButton;

	case NativeSkinElement::NATIVE_DROPDOWN_EDIT:
		return new DropDown;

	case NativeSkinElement::NATIVE_TREEVIEW:
	case NativeSkinElement::NATIVE_LISTBOX:
		return new TreeViewControl;

	case NativeSkinElement::NATIVE_MULTILINE_EDIT:
		return new MultiLineEditControl;

	case NativeSkinElement::NATIVE_BROWSER:
		return new Browser();

	case NativeSkinElement::NATIVE_LISTITEM:
		return new ListItem;
	case NativeSkinElement::NATIVE_MENU_RIGHT_ARROW:
		return new MenuArrow;

	case NativeSkinElement::NATIVE_MENU_SEPARATOR:
		return new MenuSeparator;
	
	case NativeSkinElement::NATIVE_MAINBAR_BUTTON:
		return new MainbarButton;
	case NativeSkinElement::NATIVE_PERSONALBAR_BUTTON:
		return new PersonalbarButton;

	case NativeSkinElement::NATIVE_TOOLTIP:
		return new Tooltip;
		break;
	}

#ifdef DEBUG
	qDebug() << "opera: Unkown widget: " << type;
	// uncommenting this line will cause selftest to fail in skin module
	// return new UnsupportedWidget();
#endif
	return 0;
}

ToolkitUiSettings* Kde4ToolkitLibrary::GetUiSettings()
{
	return m_settings;
}

ToolkitWidgetPainter* Kde4ToolkitLibrary::GetWidgetPainter()
{
	return m_widget_painter;
}

ToolkitColorChooser* Kde4ToolkitLibrary::CreateColorChooser()
{
	return new Kde4ColorChooser();
}

ToolkitFileChooser* Kde4ToolkitLibrary::CreateFileChooser()
{
	return new Kde4FileChooser(m_app);
}

void Kde4ToolkitLibrary::SetMainloopRunner(ToolkitMainloopRunner* runner)
{
	m_loop->SetRunner(runner);
}

ToolkitPrinterIntegration* Kde4ToolkitLibrary::CreatePrinterIntegration()
{
	return new Kde4PrinterIntegration;
}

const char* Kde4ToolkitLibrary::ToolkitInformation()
{ 
	if (!m_kde_version[0])
	{
		snprintf(m_kde_version, sizeof(m_kde_version), "KDE %d.%d.%d using %s", KDE::versionMajor(), KDE::versionMinor(), KDE::versionRelease(), qApp->style()->metaObject()->className());
		m_kde_version[sizeof(m_kde_version)-1]=0; // No one knows what snprintf actually do
	}
	return m_kde_version;
}


void Kde4ToolkitLibrary::GetMenuBarLayout(MenuBarLayout& layout)
{
	layout.frame_thickness = qApp->style()->pixelMetric(QStyle::PM_MenuBarPanelWidth);
	layout.spacing         = qApp->style()->pixelMetric(QStyle::PM_MenuBarItemSpacing);
	layout.h_margin        = qApp->style()->pixelMetric(QStyle::PM_MenuBarHMargin);
	layout.v_margin        = qApp->style()->pixelMetric(QStyle::PM_MenuBarVMargin);
	// Looks better if there are no margins by default
	layout.bottom_margin   = layout.frame_thickness+layout.v_margin == 0 ? 1 : 0;

	QMenuBar dummymenubar;
	if (dummymenubar.style()->styleHint(QStyle::SH_DrawMenuBarSeparator))
	{
		layout.separate_help = true;
	}
}

void Kde4ToolkitLibrary::GetPopupMenuLayout(PopupMenuLayout& layout) 
{
	layout.frame_thickness = qApp->style()->pixelMetric(QStyle::PM_MenuPanelWidth);
	layout.vertical_screen_edge_reflects = false;
}
