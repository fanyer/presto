/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/QuickWindow.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick_toolkit/contexts/UiContext.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/widgets/WidgetContainer.h"

class QuickWindow::DesktopWindowHelper : public DesktopWindow
{
public:
	DesktopWindowHelper(QuickWindow& window, OpTypedObject::Type type) : m_window(window), m_type(type) {}

	virtual Type GetType()		   { return m_type; }
	virtual Type GetOpWindowType() { return m_type == WINDOW_TYPE_TOPLEVEL ? WINDOW_TYPE_BROWSER : GetType(); }
	virtual	const char* GetFallbackSkin() { return m_window.GetDefaultSkin(); }
	virtual const char* GetWindowName() { return m_window.GetName(); }
	virtual DesktopWindow* GetParentDesktopWindow() { return m_window.m_parent; }
	virtual void OnActivate(BOOL activate, BOOL first_time) { if (activate && first_time) GetWidgetContainer()->GetRoot()->RestoreFocus(FOCUS_REASON_ACTIVATE); }
	virtual void UpdateLanguage() { DesktopWindow::UpdateLanguage(); m_window.OnContentsChanged(); }

private:
	QuickWindow& m_window;
	const OpTypedObject::Type m_type;
};

QuickWindow::QuickWindow(OpWindow::Style style, OpTypedObject::Type type)
	: m_widgets(0)
	, m_desktop_window(0)
	, m_parent(0)
	, m_type(type)
	, m_style(style)
	, m_effects(0)
	, m_context(0)
	, m_scheduled_relayout(FALSE)
	, m_resize_on_content_change(TRUE)
	, m_screen_properties(0)
	, m_decoration_width(0)
	, m_decoration_height(0)
	, m_block_closing(false)
{
	g_main_message_handler->SetCallBack(this, MSG_QUICKTK_RELAYOUT, reinterpret_cast<MH_PARAM_1>(this));
}

QuickWindow::~QuickWindow()
{
	g_main_message_handler->UnsetCallBacks(this);

	OP_DELETE(m_widgets);
	m_content.SetContent(0);

	OP_DELETE(m_screen_properties);

	// Destroy the desktop window last, in case other members held
	// references to it
	if (m_desktop_window)
	{
		m_desktop_window->SetParentInputContext(0);
		m_desktop_window->RemoveListener(this);
		m_desktop_window->Close(TRUE);
	}
}

void QuickWindow::OnDesktopWindowClosing(DesktopWindow* desktop_window,
		BOOL user_initiated)
{
	OP_ASSERT(desktop_window != NULL && desktop_window == m_desktop_window);

	if (m_parent)
		m_parent->RestoreFocus(FOCUS_REASON_OTHER);

	m_desktop_window = 0;

	// It may happen that a disabled button in a (YAML) dialog wants to call its listener while the dialog is closing.
	if (desktop_window->GetWidgetContainer())
		desktop_window->GetWidgetContainer()->SetWidgetListener(NULL);

	// The parent OpWidget will likely disappear. Disconnect the content from that tree.
	m_content->SetParentOpWidget(0);

	if (m_context)
		m_context->OnUiClosing();

	// The context will be destroyed soon.
	m_context = 0;
}

OP_STATUS QuickWindow::Init()
{
	return OpStatus::OK;
}

OP_STATUS QuickWindow::SetTitle(const OpStringC& title)
{
	RETURN_IF_ERROR(m_title.Set(title));
	if (m_desktop_window)
		m_desktop_window->SetTitle(title.CStr()?title.CStr():UNI_L(""));

	return OpStatus::OK;
}

void QuickWindow::SetContent(QuickWidget* widget)
{
	m_content.SetContent(widget);
	m_content->SetContainer(this);
}

void QuickWindow::SetWidgetCollection(TypedObjectCollection* widgets)
{
	OP_DELETE(m_widgets);
	m_widgets = widgets;
}

OP_STATUS QuickWindow::Show()
{
	OP_NEW_DBG("QuickWindow::Show", "quicktoolkit");

	RETURN_IF_ERROR(InitDesktopWindow());
	RETURN_IF_ERROR(OnShowing());

	const unsigned nominal_width = GetNominalWidth();
	const unsigned nominal_height = GetNominalHeight(nominal_width);
	m_desktop_window->SetMinimumInnerSize(nominal_width, nominal_height);

	const unsigned max_width = GetMaximumInnerWidth();
	m_desktop_window->SetMaximumInnerSize(max_width, GetMaximumInnerHeight(max_width));

	const OpRect rect(DEFAULT_SIZEPOS, DEFAULT_SIZEPOS, nominal_width, nominal_height);
	
	bool is_blocking_dialog = m_style == OpWindow::STYLE_MODAL_DIALOG ||
	                          m_style == OpWindow::STYLE_BLOCKING_DIALOG;
	// For blocking dialogs, both the DesktopWindow and UiContext are closed
	// already after this line or this object may be even deleted
	m_desktop_window->Show(TRUE, &rect, OpWindow::RESTORED, FALSE, TRUE, TRUE);

	if (is_blocking_dialog)
		return OpStatus::OK;

	FitToScreen();

	if (m_context)
		m_context->OnUiCreated();

	return OpStatus::OK;
}

void QuickWindow::OnContentsChanged()
{
	if (m_scheduled_relayout)
		return;
	
	g_main_message_handler->PostMessage(MSG_QUICKTK_RELAYOUT, reinterpret_cast<MH_PARAM_1>(this), 0);
	m_scheduled_relayout = TRUE;
}

OP_STATUS QuickWindow::InitDesktopWindow()
{
	m_desktop_window = OP_NEW(DesktopWindowHelper, (*this, m_type));
	if (!m_desktop_window)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_desktop_window->Init(m_style, m_parent, m_effects));
	GetRoot()->SetSkinIsBackground(SkinIsBackground());

	RETURN_IF_ERROR(m_desktop_window->AddListener(this));
	m_desktop_window->SetTitle(m_title.CStr());
	m_desktop_window->SetParentInputContext(m_context);
	m_content->SetParentOpWidget(GetRoot());

	OpAutoPtr<OpScreenProperties> properties (OP_NEW(OpScreenProperties, ()));
	if (!properties.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(g_op_screen_info->GetProperties(properties.get(), m_desktop_window->GetOpWindow()));
	m_screen_properties = properties.release();

	return OpStatus::OK;
}

void QuickWindow::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_NEW_DBG("QuickWindow::HandleCallback", "quicktoolkit");
	if (msg == MSG_QUICKTK_RELAYOUT)
	{
		m_scheduled_relayout = FALSE;

		if (!m_desktop_window)
			return;

		if (m_resize_on_content_change)
		{
			UINT32 width = 0;
			UINT32 height = 0;
			m_desktop_window->GetInnerSize(width, height);
			if (UpdateSizes(width, height))
				FitToScreen();
			else
				LayoutContent();
		}
		else
		{
			LayoutContent();
		}
	}
}

void QuickWindow::OnDesktopWindowResized(DesktopWindow* desktop_window, INT32 width, INT32 height)
{
	OP_NEW_DBG("QuickWindow::OnDesktopWindowResized", "quicktoolkit");
	UpdateDecorationSizes();
	if (!UpdateSizes(width, height))
		LayoutContent();
}

bool QuickWindow::UpdateSizes(unsigned width, unsigned height)
{
	OP_NEW_DBG("QuickWindow::UpdateSizes", "quicktoolkit");
	OP_DBG(("%dx%d", width, height));

	// Contents change / window width change may imply
	// minimum size / height change.  Update window size accordingly.

	const unsigned min_width = GetNominalWidth();
	const unsigned min_height = GetNominalHeight(width);
	if (width != min_width || height != min_height)
		m_desktop_window->SetMinimumInnerSize(min_width, min_height);

	const unsigned pref_width = GetMaximumInnerWidth();
	const unsigned pref_height = GetMaximumInnerHeight(width);
	if (width != pref_width || height != pref_height)
		m_desktop_window->SetMaximumInnerSize(pref_width, pref_height);

	unsigned new_width = max(width, min_width);
	new_width = min(new_width, pref_width);
	unsigned new_height = max(height, min_height);
	new_height = min(new_height, pref_height);

	if (new_width != width || new_height != height)
	{
		m_desktop_window->SetInnerSize(new_width, new_height); // postpone layout until after resize
		return true;
	}

	return false;
}

void QuickWindow::UpdateDecorationSizes()
{
	OP_NEW_DBG("QuickWindow::UpdateDecorationSizes", "quicktoolkit");

	UINT32 outer_width = 0;
	UINT32 outer_height = 0;
	m_desktop_window->GetOuterSize(outer_width, outer_height);
	UINT32 inner_width = 0;
	UINT32 inner_height = 0;
	m_desktop_window->GetInnerSize(inner_width, inner_height);
	OP_DBG(("inner: %dx%d", inner_width, inner_height));
	OP_DBG(("outer: %dx%d", outer_width, outer_height));

	OP_ASSERT(outer_width >= inner_width && outer_height >= inner_height);
	m_decoration_width = outer_width - inner_width;
	m_decoration_height = outer_height - inner_height;
}

void QuickWindow::FitToScreen()
{
	OpRect outer_rect;
	m_desktop_window->GetRect(outer_rect);
	m_desktop_window->FitRectToScreen(outer_rect);
	m_desktop_window->SetOuterPos(outer_rect.x, outer_rect.y);
	m_desktop_window->SetOuterSize(outer_rect.width, outer_rect.height);
}

void QuickWindow::LayoutContent()
{
	OpRect rect = GetRoot()->GetRect();
	GetRoot()->AddPadding(rect);
	m_content->Layout(rect);
}

OpWidget* QuickWindow::GetRoot()
{
	return m_desktop_window->GetWidgetContainer()->GetRoot();
}

unsigned QuickWindow::GetHorizontalPadding()
{
	return GetRoot()->GetPaddingLeft() + GetRoot()->GetPaddingRight();
}

unsigned QuickWindow::GetVerticalPadding()
{
	return GetRoot()->GetPaddingTop() + GetRoot()->GetPaddingBottom();
}

unsigned QuickWindow::GetContentWidth(unsigned window_width)
{
	return window_width == WidgetSizes::UseDefault ? window_width : window_width - GetHorizontalPadding();
}

unsigned QuickWindow::GetNominalWidth()
{
	OP_NEW_DBG("QuickWindow::GetNominalWidth", "quicktoolkit");

	unsigned width = m_content->GetNominalWidth() + GetHorizontalPadding();
	OP_DBG(("         width = ") << width);

	if (m_screen_properties)
	{
		width = min(width, (unsigned)m_screen_properties->workspace_rect.width - m_decoration_width);
		OP_DBG(("adjusted width = ") << width);
	}

	return width;
}

unsigned QuickWindow::GetNominalHeight(unsigned width)
{
	OP_NEW_DBG("QuickWindow::GetNominalHeight", "quicktoolkit");

	unsigned height = m_content->GetNominalHeight(GetContentWidth(width)) + GetVerticalPadding();
	OP_DBG(("         height = ") << height);

	if (m_screen_properties)
	{
		height = min(height, (unsigned)m_screen_properties->workspace_rect.height - m_decoration_height);
		OP_DBG(("adjusted height = ") << height);
	}

	return height;
}

unsigned QuickWindow::GetPreferredWidth()
{
	unsigned width = m_content->GetPreferredWidth();
	if (width >= WidgetSizes::UseDefault)
		return width;

	return width + GetHorizontalPadding();
}

unsigned QuickWindow::GetPreferredHeight(unsigned width)
{
	unsigned height = m_content->GetPreferredHeight(GetContentWidth(width));
	if (height >= WidgetSizes::UseDefault)
		return height;

	return height + GetVerticalPadding();
}

unsigned QuickWindow::GetMaximumInnerWidth()
{
	unsigned pref_width = GetPreferredWidth();
	if (pref_width >= WidgetSizes::UseDefault)
		pref_width = UINT_MAX;
	return max(pref_width, GetNominalWidth());
}

unsigned QuickWindow::GetMaximumInnerHeight(unsigned width)
{
	unsigned pref_height = GetPreferredHeight(width);
	if (pref_height >= WidgetSizes::UseDefault)
		pref_height = UINT_MAX;
	return max(pref_height, GetNominalHeight(width));
}
