/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/widgets/OpSpeedDialView.h"

#include "adjunct/desktop_util/rtl/uidirection.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#include "adjunct/desktop_util/widget_utils/WidgetThumbnailGenerator.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/controller/ExtensionPrefsController.h"
#include "adjunct/quick/controller/SpeedDialConfigController.h"
#include "adjunct/quick/dialogs/SpeedDialGlobalConfigDialog.h"
#ifdef _X11_SELECTION_POLICY_
# include "adjunct/quick/managers/DesktopClipboardManager.h"
#endif // _X11_SELECTION_POLICY_
#include "adjunct/quick/managers/DesktopExtensionsManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/speeddial/SpeedDialThumbnail.h"
#include "adjunct/quick/widgets/OpSpeedDialSearchWidget.h"
#include "adjunct/quick/widgets/OpZoomDropDown.h"
#include "adjunct/quick_toolkit/widgets/OpImageBackground.h"
#include "adjunct/quick_toolkit/widgets/QuickAnimatedWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickBackgroundWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickFlowLayout.h"
#include "adjunct/quick_toolkit/widgets/QuickLayoutBase.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickCentered.h"
#include "adjunct/quick_toolkit/widgets/QuickGrid/QuickStackLayout.h"
#include "modules/display/vis_dev.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/WidgetContainer.h"

#ifdef _DEBUG
 #if defined (MSWIN)
 #define SPEEDDIAL_DEBUG Win32DebugLogging
#else
 #define SPEEDDIAL_DEBUG ((void *)0)
#endif // WINDOWS
#endif // _DEBUG

namespace
{
	const UINT32 RELOAD_ON_RESIZE_TIMEOUT = 500; // ms
}

class OpSpeedDialView::TopLineLayout: public QuickLayoutBase
{
public:
	explicit TopLineLayout(const QuickScrollContainer& container)
		: m_container(container)
		, m_search_field(0)
		, m_config_button(0)
		, m_outer_window_width(0)
		, m_scrollbar_width(0)
	{}

		OP_STATUS SetSearchField(QuickWidget * searchfield)
		{
				/* If m_search_field is non-null, it must be removed
				 * from the list in QuickLayoutBase.
				 */
				OP_ASSERT(m_search_field == 0);
			m_search_field = searchfield;
				return InsertWidget(searchfield);
		}

		OP_STATUS SetGlobalConfigButton(QuickWidget * configbutton)
		{
				/* If m_config_button is non-null, it must be removed
				 * from the list in QuickLayoutBase.
				 */
				OP_ASSERT(m_config_button == 0);
			m_config_button = configbutton;
				return InsertWidget(configbutton);
		}

		void CropToWidth(unsigned window_width, unsigned scrollbar_width)
		{
			m_outer_window_width = window_width;
			m_scrollbar_width = scrollbar_width;
		}

	// Override QuickWidget
	virtual OP_STATUS Layout(const OpRect& rect)
		{
			OpRect cropped_rect = rect;
			if (!m_config_button)
				return OpStatus::OK;

			OP_ASSERT(m_outer_window_width > 0);
			if (UiDirection::Get() == UiDirection::RTL && !m_container.IsScrollbarVisible())
				cropped_rect.x += m_scrollbar_width;
			cropped_rect.width = m_outer_window_width - m_scrollbar_width;

			unsigned sw, cw, layout;
			GetLayoutWidths(cropped_rect.width, &sw, &cw, &layout);
			unsigned sh_min = 0;
			unsigned sh_max = 0;
			if (m_search_field)
				GetHeightRange(m_search_field, sw, cropped_rect.height, &sh_min, &sh_max);
			unsigned ch_min, ch_max;
			GetHeightRange(m_config_button, cw, cropped_rect.height, &ch_min, &ch_max);
			OpRect sr;
			sr.x = cropped_rect.x + (cropped_rect.width - sw) / 2;
			sr.width = sw;
			OpRect cr;
			cr.x = cropped_rect.x + cropped_rect.width - cw;
			cr.y = cropped_rect.y;
			cr.width = cw;
			if (layout == 0)
			{
				// Single line
				if (m_search_field)
				{
					sr.y = cropped_rect.y;
					sr.height = sh_max;
				};
				cr.height = ch_max;
			}
			else
			{
				// Two lines
				if (cropped_rect.height > (int)(sh_min + ch_min))
				{
					unsigned extra = cropped_rect.height - sh_min - ch_min;
					cr.height = ch_min + extra/2;
					if (cr.height > (int)ch_max)
						cr.height = ch_max;
					sr.y = cr.y + cr.height;
					sr.height = cropped_rect.height - sr.y;
					if (sr.height > (int)sh_max)
						sr.height = sh_max;
				}
				else
				{
					cr.height = ch_min;
					sr.y = cr.y + cr.height;
					sr.height = sh_min;
				};
			};
			if (m_search_field)
				RETURN_IF_ERROR(m_search_field->Layout(sr, cropped_rect));
			RETURN_IF_ERROR(m_config_button->Layout(cr, cropped_rect));
			return OpStatus::OK;
		};

	virtual BOOL HeightDependsOnWidth()
		{
			return TRUE;
		};

private:
	const QuickScrollContainer& m_container;
	QuickWidget * m_search_field;
	QuickWidget * m_config_button;
	unsigned m_outer_window_width;
	unsigned m_scrollbar_width;

	// Override QuickWidget

	virtual unsigned GetDefaultMinimumWidth()
		{
			if (!m_config_button)
				return 0;
			if (!m_search_field)
				return m_config_button->GetMinimumWidth();
			unsigned search_width = m_search_field->GetMinimumWidth();
			unsigned config_width = m_config_button->GetMinimumWidth();
			if (config_width > search_width)
				return config_width;
			return search_width;
		};

	virtual unsigned GetDefaultMinimumHeight(unsigned width)
		{
			if (!m_config_button)
				return 0;
			if (!m_search_field)
				return m_config_button->GetMinimumHeight(width);
			unsigned search_width = m_search_field->GetMinimumWidth();
			unsigned search_height = m_search_field->GetMinimumHeight(search_width);
			unsigned config_width = m_config_button->GetMinimumWidth();
			unsigned config_height = m_config_button->GetMinimumHeight(config_width);
			if (width < config_width * 2 + search_width)
				return search_height + config_height;
			if (config_height < search_height)
				return search_height;
			return config_height;
		};

	virtual unsigned GetDefaultNominalWidth()
		{
			/* Or should this be just wide enough to have all
			 * elements on a single line?  (And I guess it should
			 * use the elements' nominal size rather than minimum
			 * size...)
			 */
			return GetDefaultMinimumWidth();
		};

	virtual unsigned GetDefaultNominalHeight(unsigned width)
		{
			/* Or should this be just wide enough to have all
			 * elements on a single line?  (And I guess it should
			 * use the elements' nominal size rather than minimum
			 * size...)
			 */
			return GetDefaultMinimumHeight(width);
		};

	virtual unsigned GetDefaultPreferredWidth()
		{
			return WidgetSizes::Fill;
		};

	virtual unsigned GetDefaultPreferredHeight(unsigned width)
		{
			if (!m_config_button)
				return 0;
			if (!m_search_field)
				return m_config_button->GetPreferredHeight(width);
			unsigned sw, cw, layout;
			GetLayoutWidths(width, &sw, &cw, &layout);
			unsigned sh = m_search_field->GetPreferredHeight(sw);
			unsigned ch = m_config_button->GetPreferredHeight(cw);
			if (layout == 0) // single line
			{
				if (sh > ch)
					return sh;
				return ch;
			}
				// The range UINT_MAX-3 up to UINT_MAX is reserved values, so we use values out of this range. See WidgetSizes enum.
				if (sh > UINT_MAX - 100)
				return sh;
				if (ch > UINT_MAX - 100)
				return ch;
			return sh + ch;
		};

	virtual void GetDefaultMargins(WidgetSizes::Margins& margins)
		{
			margins.left = margins.right = 0;
			margins.top = margins.bottom = 0;
		};

	void GetLayoutWidths(unsigned layout_width, unsigned * search_w, unsigned * conf_w, unsigned * layout)
		{
			if (!m_config_button)
			{
				*layout = 0;
				*search_w = 0;
				*conf_w = 0;
				return;
			};
			unsigned conf_minw, conf_maxw;
			unsigned search_minw, search_maxw;
			GetWidthRange(m_config_button, layout_width, &conf_minw, &conf_maxw);
			GetWidthRange(m_search_field, layout_width, &search_minw, &search_maxw);
			if (search_minw + conf_minw * 2 > layout_width)
			{
				*layout = 1;
				*search_w = search_maxw;
				*conf_w = conf_maxw;
				return;
			};
			unsigned extra = layout_width - search_minw - conf_minw * 2;
			unsigned sw = search_minw + extra / 2;
			if (sw > search_maxw)
				sw = search_maxw;
			unsigned cw = (layout_width - sw) / 2;
			if (cw > conf_maxw)
				cw = conf_maxw;
			*layout = 0;
			*search_w = sw;
			*conf_w = cw;
		};
};

OpSpeedDialView::OpSpeedDialView()
	: m_search_field(NULL)
	, m_global_config_dialog(NULL)
	, m_adding(false)
	, m_scale(-1.0)
	, m_config_controller(NULL)
	, m_extension_prefs_controller(NULL)
	, m_content(NULL)
	, m_thumbnail_flow(NULL)
	, m_background(NULL)
	, m_focused_thumbnail(NULL)
	, m_top_line_layout(NULL)
	, m_speeddial_show(NULL)
{
	m_reload_on_resize_timer.SetTimerListener(this);
}

OpSpeedDialView::~OpSpeedDialView()
{
	if (g_speeddial_manager)
	{
		g_speeddial_manager->RemoveListener(*this);
		g_speeddial_manager->RemoveConfigurationListener(this);
	}
}

void OpSpeedDialView::OnAdded()
{
	// Increase the count of open speed dial tabs
	g_speeddial_manager->AddTab();

	OpWidget::OnAdded();
}

void OpSpeedDialView::OnRemoved()
{
	m_reload_on_resize_timer.Stop();
	m_reload_on_resize_timer.SetTimerListener(NULL);

	g_speeddial_manager->RemoveListener(*this);
	g_speeddial_manager->RemoveConfigurationListener(this);

	// Decrement the count of open speed dial tabs
	g_speeddial_manager->RemoveTab();

	CloseAllDialogs();

	if (m_config_controller)
		m_config_controller->SetListener(NULL);
	if (m_extension_prefs_controller)
		m_extension_prefs_controller->SetListener(NULL);

	OP_DELETE(m_content);
	m_content = NULL;

	OpWidget::OnRemoved();
}

void OpSpeedDialView::OnDeleted()
{
	RemoveListener(this);
	g_main_message_handler->UnsetCallBacks(this);
	OpWidget::OnDeleted();
}

OP_STATUS OpSpeedDialView::Create(OpWidget* container)
{
	RETURN_IF_ERROR(InitSpeedDialHidden(container));
	RETURN_IF_ERROR(InitSpeedDial(container));

	Show(false);

	m_doc_view_fav_icon.SetImage("Window Speed Dial Icon");

	return OpStatus::OK;
}

OP_STATUS OpSpeedDialView::InitSpeedDial(OpWidget* container)
{
	RETURN_IF_ERROR(Init());
	RETURN_IF_ERROR(AddListener(this));
	container->AddChild(this);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilityPrunesMe(ACCESSIBILITY_PRUNE_WHEN_INVISIBLE);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	SetTabStop(TRUE);

	return OpStatus::OK;
}

OP_STATUS OpSpeedDialView::InitSpeedDialHidden(OpWidget* container)
{
	OpAutoPtr<OpInputAction> action(OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_SPEEDDIAL_CONTENTS)));
	RETURN_OOM_IF_NULL(action.get());
	action->SetActionData(1);

	OpString str;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_SHOW_SPEED_DIAL, str));

	OpButton* button;
	RETURN_IF_ERROR(OpButton::Construct(&button, OpButton::TYPE_TOOLBAR, OpButton::STYLE_TEXT));
	OpAutoPtr<OpButton> auto_button(button);
	auto_button->SetName("Speed Dial hide or show");
	auto_button->SetTabStop(TRUE);
	OpStatus::Ignore(auto_button->SetText(str.CStr()));
	auto_button->SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);
	auto_button->SetAction(action.release());
#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	auto_button->AccessibilityPrunesMe(ACCESSIBILITY_PRUNE_WHEN_INVISIBLE);
#endif //ACCESSIBILITY_EXTENSION_SUPPORT

	container->AddChild(auto_button.get());

	m_speeddial_show = auto_button.release();

	return OpStatus::OK;
}

void OpSpeedDialView::Show(bool visible)
{
	if (visible)
	{
		if (g_speeddial_manager->GetState() == SpeedDialManager::Folded)
		{
			SetVisibility(FALSE);

			m_speeddial_show->SetVisibility(TRUE);
			GetParent()->SetBackgroundColor(OP_RGB(0XFF,0xFF,0xFF));
			GetParent()->GetWidgetContainer()->SetEraseBg(TRUE);
		}
		else
		{
			SetVisibility(TRUE);
			m_speeddial_show->SetVisibility(FALSE);
		}
	}
	else
	{
		SetVisibility(FALSE);
		m_speeddial_show->SetVisibility(FALSE);
	}
}

bool OpSpeedDialView::IsVisible()
{
	return (OpWidget::IsVisible()) || (m_speeddial_show && m_speeddial_show->IsVisible());
}

void OpSpeedDialView::Layout()
{
	if (OpWidget::IsVisible())
		OnLayout();
}

void OpSpeedDialView::SetRect(OpRect rect)
{
	if (!IsVisible())
		return;

	if (OpWidget::IsVisible())
		OpWidget::SetRect(rect);
	else
	{
		if (m_speeddial_show->IsVisible())
		{
			INT32 w,h;
			m_speeddial_show->GetRequiredSize(w,h);
			m_speeddial_show->SetRect(OpRect(rect.x + rect.width - w, rect.y + rect.height - h, w, h));
		}
	}
}

void OpSpeedDialView::GetThumbnailImage(Image& image)
{
	if (OpWidget::IsVisible())
	{
		image = GetThumbnailImage();
		OP_ASSERT(!image.IsEmpty());
	}
}

OP_STATUS OpSpeedDialView::GetTooltipText(OpInfoText& text)
{
	OpString loc_str;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_HL_TREE_TITLE, loc_str));

	OpString title;
	RETURN_IF_ERROR(GetTitle(title));
	OP_ASSERT(title.HasContent());

	return text.AddTooltipText(loc_str.CStr(), title.CStr());
}

double OpSpeedDialView::GetWindowScale()
{
	return g_speeddial_manager->GetThumbnailScale();
}

void OpSpeedDialView::QueryZoomSettings(double &min, double &max, double &step, const OpSlider::TICK_VALUE* &tick_values, UINT8 &num_tick_values)
{
	static const OpSlider::TICK_VALUE tick_values_speeddial[] =
													{ {SpeedDialManager::MinimumZoomLevel, FALSE },
													{0.5, TRUE },
													{1  , TRUE },
													{1.5, TRUE },
													{SpeedDialManager::MaximumZoomLevel, FALSE } };

	min = SpeedDialManager::MinimumZoomLevel;
	max = SpeedDialManager::MaximumZoomLevel;
	step = SpeedDialManager::ScaleDelta;
	num_tick_values = ARRAY_SIZE(tick_values_speeddial);
	tick_values = tick_values_speeddial;
}

void OpSpeedDialView::UpdateButtonVisibility()
{
	QuickWidget* plus = m_thumbnail_flow->GetWidget(m_thumbnail_flow->GetWidgetCount() - 1);
	if (!plus)
		return;

	if (IsPlusButtonShown() && !plus->IsVisible())
		plus->Show();
	else if (!IsPlusButtonShown() && plus->IsVisible())
		plus->Hide();

	if (IsReadOnly())
		m_config_button->Hide();
	else
		m_config_button->Show();
}

void OpSpeedDialView::UpdateBackground()
{
	if (g_speeddial_manager->IsBackgroundImageEnabled())
	{
		const OpRect rect = GetBounds();
		m_background->SetImage(g_speeddial_manager->GetBackgroundImage(rect.width, rect.height));
		m_background->SetLayout(g_speeddial_manager->GetImageLayout());
	}
	else
	{
		m_background->SetImage(Image());
	}
	m_background->InvalidateAll();
}

void OpSpeedDialView::OnResize(INT32* new_w, INT32* new_h)
{
	// Size matters for SVG only.
	if (g_speeddial_manager->IsBackgroundImageEnabled())
	{
		m_background->SetImage(g_speeddial_manager->GetBackgroundImage(*new_w, *new_h));
	}
}

void OpSpeedDialView::OnTimeOut(OpTimer* timer)
{
	if (timer == &m_reload_on_resize_timer)
		ZoomSpeedDial();
	else
		OpWidget::OnTimeOut(timer);
}

BOOL OpSpeedDialView::OnScrollAction(INT32 delta, BOOL vertical, BOOL smooth)
{
	return m_content->SetScroll(delta, smooth);
}

void OpSpeedDialView::OnContentScrolled(int scroll_current_value, int min, int max)
{
	CloseAllDialogs();
}

OP_STATUS OpSpeedDialView::Init()
{
	SetSkinned(TRUE);

	SetName("Speed dial");

	RETURN_IF_ERROR(g_speeddial_manager->AddConfigurationListener(this));
	RETURN_IF_ERROR(g_speeddial_manager->AddListener(*this));

	m_content = OP_NEW(QuickScrollContainer,
				(QuickScrollContainer::VERTICAL, QuickScrollContainer::SCROLLBAR_AUTO));
	RETURN_OOM_IF_NULL(m_content);
	RETURN_IF_ERROR(m_content->Init());
	m_content->SetRespectScrollbarMargin(false);
	m_content->SetListener(this);
	m_content->SetContainer(this);

	QuickBackgroundWidget<OpImageBackground>* stack_background = OP_NEW(QuickBackgroundWidget<OpImageBackground>, ());
	if (!stack_background || OpStatus::IsError(stack_background->Init()))
	{
		OP_DELETE(stack_background);
		return OpStatus::ERR;
	}
	m_content->SetContent(stack_background);
	stack_background->SetPreferredHeight(WidgetSizes::Fill);

	m_background = stack_background->GetOpWidget();
	m_background->SetSkinned(TRUE);
	m_background->GetBorderSkin()->SetImage("Speed Dial Widget Skin");

	UpdateBackground();

	// Create the main stack layout
	QuickStackLayout* stack(OP_NEW(QuickStackLayout, (QuickStackLayout::VERTICAL)));
	RETURN_OOM_IF_NULL(stack);
	stack_background->SetContent(stack);

	// Create the layout for the search field and the config button
	m_top_line_layout = OP_NEW(TopLineLayout, (*m_content));
	RETURN_OOM_IF_NULL(m_top_line_layout);
	RETURN_IF_ERROR(stack->InsertWidget(m_top_line_layout));

	// DefaultSpeeddialSearchType of -1 means no search field
	OpAutoPtr<OpSpeedDialSearchWidget> search_widget(OP_NEW(OpSpeedDialSearchWidget, ()));
	RETURN_OOM_IF_NULL(search_widget.get());
	RETURN_IF_ERROR(search_widget->Init());
	search_widget->SetName("Speed Dial search field");

	m_search_field = QuickWrap(search_widget.release());
	RETURN_OOM_IF_NULL(m_search_field);
	RETURN_IF_ERROR(m_top_line_layout->SetSearchField(m_search_field));

	if (!g_searchEngineManager->GetDefaultSpeedDialSearch())
		m_search_field->Hide();

	// Create the global configuration button
	OpButton* global_config_button;
	RETURN_IF_ERROR(OpButton::Construct(&global_config_button, OpButton::TYPE_CUSTOM, OpButton::STYLE_TEXT));
	m_config_button = QuickWrap(global_config_button);
	RETURN_OOM_IF_NULL(m_config_button);
	RETURN_IF_ERROR(m_top_line_layout->SetGlobalConfigButton(m_config_button));

	if (IsReadOnly())
		m_config_button->Hide();
	global_config_button->SetButtonStyle(OpButton::STYLE_IMAGE_AND_TEXT_CENTER);
	global_config_button->SetName("Speed Dial Global Configuration");
	global_config_button->GetBorderSkin()->SetImage("Speed Dial Button Skin", "Link Button Skin");
	global_config_button->GetForegroundSkin()->SetImage("Speed Dial Configure Icon");

	OpString str;
	global_config_button->SetTabStop(TRUE);
	global_config_button->SetSystemFont(OP_SYSTEM_FONT_UI_DIALOG);

	OpInputAction * action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_SPEEDDIAL_GLOBAL_CONFIG));
	RETURN_OOM_IF_NULL(action);
	RETURN_IF_ERROR(g_languageManager->GetString(Str::D_SPEEDDIAL_GLOBAL_CONFIGURATION, str));
	action->GetActionInfo().SetStatusText(str);
	global_config_button->SetAction(action);

	OpAutoPtr<QuickCentered> centered_thumbnails(OP_NEW(QuickCentered, ()));
	RETURN_OOM_IF_NULL(centered_thumbnails.get());

	// Create the thumbnail layout and put it in the UI
	m_thumbnail_flow = OP_NEW(QuickFlowLayout, ());
	RETURN_OOM_IF_NULL(m_thumbnail_flow);
	m_thumbnail_flow->SetName("speeddial_flow_layout");
	centered_thumbnails->SetContent(m_thumbnail_flow);
	RETURN_IF_ERROR(stack->InsertWidget(centered_thumbnails.release()));

	RETURN_IF_ERROR(CreateThumbnails());

	m_content->SetParentOpWidget(this);

	UpdateButtonVisibility();

	return OpStatus::OK;
}

OP_STATUS OpSpeedDialView::CreateThumbnails()
{
	for (UINT32 i = 0; i < g_speeddial_manager->GetTotalCount(); i++)
	{
		const DesktopSpeedDial& entry = *g_speeddial_manager->GetSpeedDial(i);
		RETURN_IF_ERROR(CreateThumbnail(entry, false));
	}
	return OpStatus::OK;
}

OP_STATUS OpSpeedDialView::CreateThumbnail(const DesktopSpeedDial& entry, bool animate)
{
	typedef QuickAnimatedWidget<SpeedDialThumbnail> QuickSpeedDialThumbnail;
	OpAutoPtr<QuickSpeedDialThumbnail> quick_thumbnail(OP_NEW(QuickSpeedDialThumbnail, ()));
	RETURN_OOM_IF_NULL(quick_thumbnail.get());
	RETURN_IF_ERROR(quick_thumbnail->Init());

	SpeedDialThumbnail* thumbnail = quick_thumbnail->GetOpWidget();
	RETURN_IF_ERROR(thumbnail->SetEntry(&entry));
	thumbnail->SetLocked(IsReadOnly());

	const int pos = g_speeddial_manager->FindSpeedDial(&entry);
	if (pos < 0 || pos > (int)m_thumbnails.GetCount())
	{
		OP_ASSERT(!"Out of sync with SpeedDialManager");
		return OpStatus::ERR;
	}
	RETURN_IF_ERROR(m_thumbnails.Insert(pos, thumbnail));

	quick_thumbnail->SetListener(m_thumbnail_flow);
	RETURN_IF_ERROR(m_thumbnail_flow->InsertWidget(quick_thumbnail.release(), pos));

	SetCellSize(pos);

	if (animate)
	{
		DoLayout(); // must do the layout explicitly first, before we do the following animation
		m_thumbnails.Get(pos)->AnimateThumbnailIn();
	}

	return OpStatus::OK;
}

void OpSpeedDialView::UpdateZoomButton()
{
	DesktopWindow* window = GetParentDesktopWindow();
	if (!window)
		return;

	OpZoomMenuButton* zoom_button = static_cast<OpZoomMenuButton*>(window->GetWidgetByType(WIDGET_TYPE_ZOOM_MENU_BUTTON));
	if (!zoom_button)
		return;

	zoom_button->UpdateZoom(g_speeddial_manager->GetIntegerThumbnailScale());
}

void OpSpeedDialView::OnContentChanged(OpSpeedDialView* speeddial)
{
	if (OpWidget::IsVisible() && g_application->GetActiveDesktopWindow())
		g_application->GetActiveDesktopWindow()->BroadcastDesktopWindowContentChanged();
}

void OpSpeedDialView::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget->GetType() == WIDGET_TYPE_ZOOM_SLIDER && button == MOUSE_BUTTON_1)
	{
		if (down)
			g_speeddial_manager->StartScaling();
		else
			g_speeddial_manager->EndScaling();
	}
}

void OpSpeedDialView::OnSpeedDialConfigurationStarted(const DesktopWindow& window)
{
	if (&window != GetParentDesktopWindow())
	{
		CloseGlobalConfigDialog();
		CloseAddDialog();
	}
}

void OpSpeedDialView::OnSpeedDialColumnLayoutChanged()
{
	DoLayout();
	NotifyContentChanged();
}

void OpSpeedDialView::SetMinimumAndPreferredSizes()
{
	OP_ASSERT(g_speeddial_manager->IsScaleAutomatic());
	OP_ASSERT(m_thumbnails.GetCount() || !g_speeddial_manager->HasLoadedConfig());

	if (m_thumbnails.GetCount())
	{
		unsigned min_width, min_height, pref_width, pref_height;
		int padding_x, padding_y;
		g_speeddial_manager->GetMinimumAndMaximumCellSizes(min_width, min_height, pref_width, pref_height);
		m_thumbnails.Get(0)->GetPadding(padding_x, padding_y);
		m_thumbnail_flow->SetFixedPadding(padding_x, padding_y);

		for(unsigned i = 0; i < m_thumbnail_flow->GetWidgetCount(); ++i)
		{
			QuickWidget* cell =  m_thumbnail_flow->GetWidget(i);
			cell->SetMinimumWidth(min_width);
			cell->SetMinimumHeight(min_height);
			cell->SetPreferredWidth(pref_width);
			cell->SetPreferredHeight(pref_height);
			cell->SetNominalWidth(pref_width);
			cell->SetNominalHeight(pref_height);
		}
	}
}

void OpSpeedDialView::SetCellsSizes()
{
	for(unsigned i = 0; i < m_thumbnail_flow->GetWidgetCount(); ++i)
	{
		SetCellSize(i);
	}
}

void OpSpeedDialView::SetCellSize(int pos)
{
	QuickWidget* cell = m_thumbnail_flow->GetWidget(pos);
	OpWidget* op_widget = m_thumbnails.Get(pos);

	OP_ASSERT(cell);
	OP_ASSERT(op_widget);

	int width, height;
	op_widget->GetRequiredSize(width, height);

	cell->SetFixedWidth(width);
	cell->SetFixedHeight(height);
}

void OpSpeedDialView::OnSpeedDialScaleChanged()
{
	DoLayout();
	NotifyContentChanged();
}

void OpSpeedDialView::OnSpeedDialBackgroundChanged()
{
	UpdateBackground();
	NotifyContentChanged();
}

void OpSpeedDialView::OnSpeedDialAdded(const DesktopSpeedDial& entry)
{
	CloseAddDialog();
	CloseExtensionPrefsDialog();
	OpStatus::Ignore(CreateThumbnail(entry, true));
	UpdateButtonVisibility();
	NotifyContentChanged();
}

void OpSpeedDialView::OnSpeedDialRemoving(const DesktopSpeedDial& entry)
{
	const INT32 pos = g_speeddial_manager->FindSpeedDial(&entry);
	if (pos < 0 || UINT32(pos) >= m_thumbnails.GetCount())
	{
		OP_ASSERT(!"Out of sync with SpeedDialManager");
		return;
	}

	if (IsConfiguring(*m_thumbnails.Get(pos)))
	{
		CloseAddDialog();
		CloseExtensionPrefsDialog();
	}

	m_thumbnails.Remove(pos);
	m_thumbnail_flow->RemoveWidget(pos);

	NotifyContentChanged();
}

void OpSpeedDialView::OnSpeedDialReplaced(const DesktopSpeedDial& old_entry, const DesktopSpeedDial& new_entry)
{
	const INT32 pos = g_speeddial_manager->FindSpeedDial(&new_entry);
	if (pos < 0 || UINT32(pos) >= m_thumbnails.GetCount())
	{
		OP_ASSERT(!"Out of sync with SpeedDialManager");
		return;
	}

	if (IsConfiguring(*m_thumbnails.Get(pos)))
	{
		CloseAddDialog();
		CloseExtensionPrefsDialog();
	}

	m_thumbnails.Get(pos)->SetEntry(&new_entry);

	// If it's the plus button that was replaced, unhide the thumbnail.
	if (!m_thumbnail_flow->GetWidget(pos)->IsVisible())
		m_thumbnail_flow->GetWidget(pos)->Show();

	NotifyContentChanged();
}

void OpSpeedDialView::OnSpeedDialMoved(const DesktopSpeedDial& from_entry, const DesktopSpeedDial& to_entry)
{
	int from_pos = g_speeddial_manager->FindSpeedDial(&from_entry);
	int to_pos   = g_speeddial_manager->FindSpeedDial(&to_entry);
	OP_ASSERT(from_pos >= 0 && to_pos >= 0 && from_pos != to_pos);

	CloseAllDialogs();

	m_thumbnail_flow->MoveWidget(from_pos, to_pos);

	SpeedDialThumbnail* sdt = m_thumbnails.Remove(from_pos);
	m_thumbnails.Insert(to_pos, sdt);
	sdt->SetZ(Z_TOP);

	NotifyContentChanged();
}

void OpSpeedDialView::OnSpeedDialsLoaded()
{
	// This is for the case when Opera delays loading of speeddial.ini
	// until it gets IP-based country code (DSK-351304). Speed dial
	// tabs created before country check is finished will be initially
	// empty and have to be initialized when speeddial.ini is loaded.
	OP_ASSERT(m_thumbnails.GetCount() == 0);
	CreateThumbnails();
	if (!IsReadOnly())
	{
		m_config_button->Show();
	}
	// Plus button and background image are shown during relayout.
}

BOOL OpSpeedDialView::OnContextMenu(const OpPoint& point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	g_application->GetMenuHandler()->ShowPopupMenu(
			IsPlusButtonShown() ? "Speed Dial Popup Menu" : "Speed Dial Popup Menu Plus",
			PopupPlacement::AnchorAtCursor(), 0, keyboard_invoked);
	return TRUE;
}

void OpSpeedDialView::StartAdding()
{
	m_adding = true;
	if (!IsPlusButtonShown())
	{
		m_thumbnail_flow->GetWidget(m_thumbnail_flow->GetWidgetCount() - 1)->Show();
	}
}

void OpSpeedDialView::EndAdding()
{
	m_adding = false;
	if (!IsPlusButtonShown())
	{
		m_thumbnail_flow->GetWidget(m_thumbnail_flow->GetWidgetCount() - 1)->Hide();
	}
}

BOOL OpSpeedDialView::IsReadOnly() const
{
	return g_speeddial_manager->GetState() == SpeedDialManager::ReadOnly || !g_speeddial_manager->HasLoadedConfig();
}

bool OpSpeedDialView::IsPlusButtonShown() const
{
	return !IsReadOnly() && g_speeddial_manager->IsPlusButtonShown();
}

BOOL OpSpeedDialView::IsConfiguring(const SpeedDialThumbnail& thumbnail) const
{
	return (m_config_controller != NULL && m_config_controller->GetThumbnail() == &thumbnail ) ||
		(m_extension_prefs_controller != NULL && m_extension_prefs_controller->GetThumbnail() == &thumbnail);
}

OP_STATUS OpSpeedDialView::ConfigureSpeeddial(SpeedDialThumbnail& thumbnail, SpeedDialConfigController* controller)
{
	if (m_config_controller && (m_config_controller->GetThumbnail() == &thumbnail))
	{
			return OpStatus::OK;
	}

	CloseAllDialogs();

	for (unsigned i = 0; i < m_thumbnails.GetCount(); ++i)
	{
		if (m_thumbnails.Get(i)->IsFocused(TRUE))
		{
			m_focused_thumbnail = m_thumbnails.Get(i);
			break;
		}
	}

	if (!controller || OpStatus::IsError(controller->SetThumbnail(thumbnail)))
	{
		OP_DELETE(controller);
		m_focused_thumbnail = NULL;
		return OpStatus::ERR;
	}

	m_config_controller = controller;
	m_config_controller->SetListener(this);
	const OP_STATUS result = ShowDialog(m_config_controller, g_global_ui_context, GetParentDesktopWindow());
	if (OpStatus::IsSuccess(result))
	{
		CloseGlobalConfigDialog();
		if (thumbnail.GetEntry()->IsEmpty())
			StartAdding();
	}

	return result;
}

void OpSpeedDialView::OnLayout()
{
	DoLayout();
	OpWidget::OnLayout();
}

void OpSpeedDialView::UpdateScale()
{
	OP_ASSERT(g_speeddial_manager->IsScaleAutomatic());
	OP_ASSERT(m_thumbnail_flow->GetWidgetCount() || !g_speeddial_manager->HasLoadedConfig());

	if (m_thumbnail_flow->GetWidgetCount())
	{
		int padding_width = 0, padding_height = 0;
		m_thumbnails.Get(0)->GetPadding(padding_width, padding_height);

		unsigned cell_width = m_thumbnail_flow->GetWidget(0)->GetPreferredWidth() - padding_width;
		double scale = cell_width / static_cast<double>(g_speeddial_manager->GetDefaultCellWidth());

		if (g_speeddial_manager->SetThumbnailScale(scale))
		{
			// New scale was set so we must resize
			m_reload_on_resize_timer.Start(RELOAD_ON_RESIZE_TIMEOUT);
		}
	}
}

void OpSpeedDialView::OnWindowActivated(BOOL activate)
{
	if (m_thumbnail_flow->GetWidgetCount() > 0)
	{
		int padding_width = 0, padding_height = 0;
		m_thumbnails.Get(0)->GetPadding(padding_width, padding_height);
		unsigned cell_width = m_thumbnail_flow->GetWidget(0)->GetPreferredWidth() - padding_width;
		double scale = cell_width / static_cast<double>(g_speeddial_manager->GetDefaultCellWidth());
		if (activate && m_scale > 0.0 && scale != m_scale)
			m_reload_on_resize_timer.Start(RELOAD_ON_RESIZE_TIMEOUT);
		m_scale = scale;
	}
}

void OpSpeedDialView::DoLayout()
{
	OpRect rect = GetRect();
	m_background->GetBorderSkin()->AddPadding(rect);
	int scrollbar_width = static_cast<int>(m_content->GetScrollbarSize());
	scrollbar_width = min(scrollbar_width, rect.width);

	if (GetVisibleThumbnailCount() > 0)
	{
		// if number_of_columns is 0, number of colums are determined automatically
		int number_of_columns = g_pcui->GetIntegerPref(PrefsCollectionUI::NumberOfSpeedDialColumns);
		if (number_of_columns <= 0)
		{
			if (g_speeddial_manager->IsScaleAutomatic())
			{
				m_thumbnail_flow->ResetBreaks();
			}
			else
			{
				SetCellsSizes();
				m_thumbnail_flow->FitToWidth(rect.width - scrollbar_width);
			}
		}
		else
		{
			if (!SpeedDialManager::GetInstance()->IsScaleAutomatic())
			{
				/* if we are in automatic mode cells sizes will be set in UpdateScale()
				   and this if is cheaper than redundant call of SetCellsSizes
				 */
				SetCellsSizes();
			}
			m_thumbnail_flow->SetHardBreak(number_of_columns);
		}

		if (g_speeddial_manager->IsScaleAutomatic())
		{
			SetMinimumAndPreferredSizes();
			OpRect modified_rect = rect;
			modified_rect.width  -= scrollbar_width;
			modified_rect.height -= static_cast<int>(m_top_line_layout->GetPreferredHeight(modified_rect.width));
			m_thumbnail_flow->FitToRect(modified_rect);
			// QuickFlowLayout calculated new dimensions for speed dial's cells so we must set new scale
			UpdateScale();
		}

		if (m_global_config_dialog != NULL)
			m_global_config_dialog->OnThumbnailScaleChanged(g_speeddial_manager->GetThumbnailScale());

		UpdateZoomButton();
	}

	OpWindow* window = GetParentOpWindow();
	unsigned window_width, window_height;
	window->GetInnerSize(&window_width, &window_height);
	m_top_line_layout->CropToWidth(window_width, scrollbar_width);

	m_content->Layout(GetBounds());
}

void OpSpeedDialView::ReloadSpeedDial()
{
	for (UINT32 i = 0; i < m_thumbnails.GetCount(); ++i)
		m_thumbnails.Get(i)->OnSpeedDialExpired();
}

void OpSpeedDialView::ZoomSpeedDial()
{
	for (UINT32 i = 0; i < m_thumbnails.GetCount(); ++i)
		m_thumbnails.Get(i)->OnSpeedDialEntryScaleChanged();
}

void OpSpeedDialView::OnSettingsChanged(DesktopSettings* settings)
{
	OpString title, url;
	OpString thumb_title, thumb_url;

	if(settings->IsChanged(SETTINGS_SPEED_DIAL))
	{
		if (g_speeddial_manager->GetState() == SpeedDialManager::Folded
			|| g_speeddial_manager->GetState() == SpeedDialManager::Shown)
			Show(true);

		UpdateButtonVisibility();
		for (UINT32 i = 0; i < m_thumbnails.GetCount(); i++)
			m_thumbnails.Get(i)->SetLocked(IsReadOnly());
	}

	if (settings->IsChanged(SETTINGS_SEARCH) && m_search_field)
	{
		if (g_searchEngineManager->GetDefaultSpeedDialSearch())
			m_search_field->Show();
		else
			m_search_field->Hide();
	}
}

void OpSpeedDialView::OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if (new_input_context != GetWidgetByName("Speed Dial Global Configuration"))
		CloseAllDialogs();

	OpWidget::OnKeyboardInputGained(new_input_context, old_input_context, reason);
}

void OpSpeedDialView::OnMouseDownOnThumbnail(MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_3
			&& !g_speeddial_manager->IsScaleAutomatic()
			&& (g_op_system_info->GetShiftKeyState() & SHIFTKEY_CTRL))
	{
		g_speeddial_manager->SetThumbnailScale(SpeedDialManager::DefaultZoomLevel);
		return;
	}

	if (button != MOUSE_BUTTON_3)
	{
		CloseExtensionPrefsDialog();
		CloseGlobalConfigDialog();
	}
}

void OpSpeedDialView::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	// Things OpSpeedDialView must do on mouse down on background only.

#if defined(_X11_SELECTION_POLICY_)
	if( button == MOUSE_BUTTON_3 && !(g_op_system_info->GetShiftKeyState() & SHIFTKEY_CTRL))
	{
		OpString text;
		g_desktop_clipboard_manager->GetText(text, true);
		if (text.HasContent())
		{
			g_application->OpenURL( text, MAYBE, MAYBE, MAYBE );
		}
		return;
	}
#endif

	// Things OpSpeedDialView must do on mouse down anywhere witin its rect.

	OnMouseDownOnThumbnail(button, nclicks);

	OpWidget::OnMouseDown(point, button, nclicks);
}

BOOL OpSpeedDialView::OnMouseWheel(INT32 delta, BOOL vertical)
{
	ShiftKeyState keystate = GetVisualDevice()->GetView()->GetShiftKeys();
	if (!(keystate & DISPLAY_SCALE_MODIFIER))
	{
		/* Mouse wheel events should scroll the content of the speed dial.
		 * If the content is not scrollable, the scroll container will
		 * pass the event on to the next handler.  Which is us.  And since
		 * we don't want any other effects to happen on mouse wheel, we'll
		 * block the event here.
		 */
		return TRUE;
	}
	if (!vertical || g_speeddial_manager->IsScaleAutomatic())
	{
		return FALSE;
	}
	if (delta != 0)
	{
		bool scale_up = delta < 0;
		g_speeddial_manager->ChangeScaleByDelta(scale_up);
	}

	return TRUE;
}

BOOL OpSpeedDialView::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_ZOOM_TO:
				{
					return g_speeddial_manager->IsScaleAutomatic();
				}
			}
			break;
		}

		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_THUMB_CONFIGURE:
				{
					child_action->SetEnabled(!IsReadOnly());
					return TRUE;
				}

				case OpInputAction::ACTION_THUMB_CLOSE_PAGE:
				{
					const DesktopSpeedDial* entry = g_speeddial_manager->GetSpeedDial(*child_action);
					child_action->SetEnabled(!IsReadOnly() && entry != NULL && !entry->IsEmpty());
					return TRUE;
				}

				case OpInputAction::ACTION_SHOW_SPEEDDIAL_GLOBAL_CONFIG:
				{
					child_action->SetEnabled(!IsReadOnly());
					child_action->SetSelected(!IsReadOnly() && m_global_config_dialog != NULL);
					return TRUE;
				}

				case OpInputAction::ACTION_ZOOM_IN:
				case OpInputAction::ACTION_ZOOM_OUT:
				case OpInputAction::ACTION_ZOOM_TO:
				{
					 child_action->SetEnabled(!g_speeddial_manager->IsScaleAutomatic());

					 if (child_action->GetAction() == OpInputAction::ACTION_ZOOM_TO)
						 child_action->SetSelected(!g_speeddial_manager->IsScaleAutomatic() && action->GetActionData() == (int)(g_speeddial_manager->GetThumbnailScale() * 100 + 0.5));

					 return TRUE;
				}

				case OpInputAction::ACTION_RELOAD:
				{
					 BOOL enable = OpWidget::IsVisible();
					 if (enable)
						enable = GetVisibleThumbnailCount() ? TRUE : FALSE;
					 child_action->SetEnabled(enable);

					 if (child_action->GetActionOperator() !=  OpInputAction::OPERATOR_PLUS && !child_action->GetNextInputAction())
						child_action->SetSelected(FALSE);

					 return TRUE;
				}

				case OpInputAction::ACTION_UNDO:
				{
					 child_action->SetEnabled(g_speeddial_manager->IsUndoAvailable());
					 return TRUE;
				}

				case OpInputAction::ACTION_REDO:
				{
					 child_action->SetEnabled(g_speeddial_manager->IsRedoAvailable());
					 return TRUE;
				}

				case OpInputAction::ACTION_SHOW_SPEEDDIAL_CONTENTS:
				{
					child_action->SetEnabled(g_speeddial_manager->HasLoadedConfig());
					return TRUE;
				}

				case OpInputAction::ACTION_FIND:
				case OpInputAction::ACTION_SHOW_ZOOM_POPUP_MENU:
				case OpInputAction::ACTION_PRINT_DOCUMENT:
				case OpInputAction::ACTION_PRINT_PREVIEW:
				case OpInputAction::ACTION_CUT:
				case OpInputAction::ACTION_COPY:
				case OpInputAction::ACTION_PASTE:
				case OpInputAction::ACTION_COPY_TO_NOTE:
				case OpInputAction::ACTION_SELECT_ALL:
				case OpInputAction::ACTION_STOP:
				{
					 child_action->SetEnabled(FALSE);
					 return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_RELOAD:
		{
		 	 ReloadSpeedDial();
			 return TRUE;
		}

		case OpInputAction::ACTION_ZOOM_OUT:
		case OpInputAction::ACTION_ZOOM_IN:
		{
			 bool scale_up = action->GetAction() == OpInputAction::ACTION_ZOOM_IN;
			 g_speeddial_manager->ChangeScaleByDelta(scale_up);
			 return TRUE;
		}

		case OpInputAction::ACTION_ZOOM_TO:
		{
			 g_speeddial_manager->SetThumbnailScale(action->GetActionData() * 0.01);
			 return TRUE;
		}

		case OpInputAction::ACTION_UNDO:
		{
			 g_speeddial_manager->Undo(this);
			 return TRUE;
		}

		case OpInputAction::ACTION_REDO:
		{
			 g_speeddial_manager->Redo(this);
			 return TRUE;
		}

		case OpInputAction::ACTION_SHOW_SPEEDDIAL_CONTENTS:
		{
			// Switch between Shown and Folded
			 BOOL fold = !action->GetActionData();
			 g_speeddial_manager->SetState(fold?SpeedDialManager::Folded : SpeedDialManager::Shown);
			 OpStatus::Ignore(g_speeddial_manager->Save());
			 Show(true);
			 return TRUE;
		}

		case OpInputAction::ACTION_THUMB_CLOSE_PAGE:
		{
			const INT32 pos = g_speeddial_manager->FindSpeedDial(*action);
			const DesktopSpeedDial* entry = g_speeddial_manager->GetSpeedDial(pos);
			if (!IsReadOnly() && entry != NULL && !entry->IsEmpty())
			{
				CloseAllDialogs();
				m_thumbnails.Get(pos)->AnimateThumbnailOut(true);
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SHOW_SPEEDDIAL_EXTENSION_OPTIONS:
		{
			const INT32 pos = g_speeddial_manager->FindSpeedDial(*action);
			const DesktopSpeedDial* entry = g_speeddial_manager->GetSpeedDial(pos);
			if (entry != NULL)
			{
				OP_ASSERT(entry->GetExtensionWUID().HasContent());

				// close any other dialogs first
				CloseAllDialogs();

				SpeedDialThumbnail* thumbnail = m_thumbnails.Get(pos);
				m_extension_prefs_controller = OP_NEW(ExtensionPrefsController, (*thumbnail));
				RETURN_VALUE_IF_NULL(m_extension_prefs_controller, TRUE);
				g_global_ui_context->AddChildContext(m_extension_prefs_controller);
				m_extension_prefs_controller->SetListener(this);
				if (OpStatus::IsError(g_desktop_extensions_manager->OpenExtensionOptionsPage(
								entry->GetExtensionWUID(), *m_extension_prefs_controller)))
				{
					OP_DELETE(m_extension_prefs_controller);
					m_extension_prefs_controller = NULL;
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SHOW_SPEEDDIAL_GLOBAL_CONFIG:
		{
			if (m_global_config_dialog)
			{
				CloseGlobalConfigDialog();
			}
			else
			{
				// close any other dialogs first
				CloseAllDialogs();

				m_global_config_dialog = OP_NEW(SpeedDialGlobalConfigDialog, ());
				if (m_global_config_dialog)
				{
					OpWidget *config_widget = GetWidgetByName("Speed Dial Global Configuration");
					if (config_widget)
					{
						if (GetScreenRect().Contains(config_widget->GetScreenRect()))
						{
							m_global_config_dialog->DialogPlacementRelativeWidget(config_widget);
						}
					}

					if (OpStatus::IsSuccess(m_global_config_dialog->Init(GetParentDesktopWindow())))
					{
						g_speeddial_manager->StartConfiguring(*GetParentDesktopWindow());
						m_global_config_dialog->SetDialogListener(this);
						m_global_config_dialog->OnThumbnailScaleChanged(g_speeddial_manager->GetThumbnailScale());
						g_input_manager->UpdateAllInputStates();
					}
					else
					{
						m_global_config_dialog = NULL;
					}
				}
			}
			return TRUE;
		}

		case OpInputAction::ACTION_THUMB_EDIT:
		case OpInputAction::ACTION_THUMB_CONFIGURE:
		{
			if (IsReadOnly())
				return TRUE;

			INT32 pos = g_speeddial_manager->FindSpeedDial(*action);
			if (pos < 0)
				pos = g_speeddial_manager->GetTotalCount() - 1;
			const DesktopSpeedDial* entry = g_speeddial_manager->GetSpeedDial(pos);
			if (entry != NULL && 0 <= pos && UINT32(pos) < m_thumbnails.GetCount())
			{
				const SpeedDialConfigController::Mode mode
						= action->GetAction() == OpInputAction::ACTION_THUMB_EDIT
								? SpeedDialConfigController::EDIT : SpeedDialConfigController::ADD;
				if (mode == SpeedDialConfigController::ADD)
					g_speeddial_manager->StartConfiguring(*GetParentDesktopWindow());

				OpStatus::Ignore(ConfigureSpeeddial(
							*m_thumbnails.Get(pos), OP_NEW(SpeedDialConfigController, (mode))));
			}
			return TRUE;
		}
	}
	return OpWidget::OnInputAction(action);
}

void OpSpeedDialView::OnClose(Dialog* dialog)
{
	if (dialog == m_global_config_dialog)
	{
		m_global_config_dialog = NULL;
		g_input_manager->UpdateAllInputStates();
	}
}

void OpSpeedDialView::OnDialogClosing(DialogContext* context)
{
	OP_NEW_DBG("OpSpeedDialView::OnDialogClosing", "speeddial");

	if (context == m_config_controller)
	{
		if (m_adding)
			EndAdding();

		if (m_focused_thumbnail != NULL)
		{
			OP_DBG(("Restoring thumbnail focus"));
			m_focused_thumbnail->SetFocus(FOCUS_REASON_OTHER);
			m_focused_thumbnail = NULL;
		}
		else
		{
			OP_DBG(("Restoring background focus"));
		}

		m_config_controller = NULL;
	}
	else if (context == m_extension_prefs_controller)
	{
		m_extension_prefs_controller = NULL;
	}
}

void OpSpeedDialView::OnShow(BOOL show)
{
	if (!show)
	{
		CloseAllDialogs();
	}
}

void OpSpeedDialView::CloseAddDialog()
{
	if (m_config_controller)
		m_config_controller->CancelDialog();
}

void OpSpeedDialView::CloseExtensionPrefsDialog()
{
	if (m_extension_prefs_controller)
		m_extension_prefs_controller->CancelDialog();
}

void OpSpeedDialView::CloseGlobalConfigDialog()
{
	if (m_global_config_dialog)
	{
		if (!m_global_config_dialog->IsClosing())
			m_global_config_dialog->CloseDialog(FALSE);
	}
}

void OpSpeedDialView::CloseAllDialogs()
{
	CloseAddDialog();
	CloseExtensionPrefsDialog();
	CloseGlobalConfigDialog();
}

unsigned OpSpeedDialView::GetVisibleThumbnailCount() const
{
	OP_ASSERT(m_thumbnails.GetCount() > 0);
	return m_thumbnails.GetCount() - (IsPlusButtonShown() || m_adding ? 0 : 1);
}

Image OpSpeedDialView::GetThumbnailImage()
{
	return GetThumbnailImage(THUMBNAIL_WIDTH, THUMBNAIL_HEIGHT);
}

Image OpSpeedDialView::GetThumbnailImage(UINT32 width, UINT32 height)
{
	// Must first sync layout in case this speed dials window hasn't
	// been visible yet (then it hasn't been layouted either)
	SyncLayout();

	if (rect.width == 0 || rect.height == 0)
		return Image();

	WidgetThumbnailGenerator thumbnail_generator(*this);

	return thumbnail_generator.GenerateThumbnail(width, height);
}

void OpSpeedDialView::NotifyContentChanged()
{
	for (OpListenersIterator it(m_listeners); m_listeners.HasNext(it); )
		m_listeners.GetNext(it)->OnContentChanged(this);
}

void OpSpeedDialView::ShowThumbnail(INT32 pos)
{
	OP_NEW_DBG("OpSpeedDialView::ShowThumbnail", "speeddial");
	OP_DBG(("pos = ") << pos);
	OP_DBG(("count = ") << m_thumbnails.GetCount());

	OP_ASSERT(0 <= pos && unsigned(pos) < m_thumbnails.GetCount());

	m_thumbnails.Get(pos)->SetVisibility(FALSE);

	g_main_message_handler->SetCallBack(this, MSG_SCROLL_SPEED_DIAL_PAGE_TO_VIEW, MH_PARAM_1(this));
	g_main_message_handler->PostMessage(MSG_SCROLL_SPEED_DIAL_PAGE_TO_VIEW, MH_PARAM_1(this), pos);
}

void OpSpeedDialView::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_NEW_DBG("OpSpeedDialView::HandleCallback", "speeddial");
	OP_DBG(("msg = %i", msg));
	OP_DBG(("this = %p", this));
	OP_DBG(("par1 = %i", par1));

	switch (msg)
	{
		case MSG_SCROLL_SPEED_DIAL_PAGE_TO_VIEW:
		{
			g_main_message_handler->UnsetCallBack(this, MSG_SCROLL_SPEED_DIAL_PAGE_TO_VIEW, MH_PARAM_1(this));

			const INT32 pos = par2;
			OP_ASSERT(0 <= pos && unsigned(pos) < m_thumbnails.GetCount());

			const OpRect r = m_thumbnails.Get(pos)->GetRect();
			if (m_content)
				m_content->SetScroll(r.y, TRUE);

			g_main_message_handler->SetCallBack(this, MSG_ANIMATE_THUMBNAIL, par1);
			g_main_message_handler->PostDelayedMessage(MSG_ANIMATE_THUMBNAIL, par1, pos, 100);
			return;
		}

		case MSG_ANIMATE_THUMBNAIL:
		{
			g_main_message_handler->UnsetCallBack(this, MSG_ANIMATE_THUMBNAIL, par1);

			const INT32 pos = par2;
			OP_ASSERT(0 <= pos && unsigned(pos) < m_thumbnails.GetCount());
			m_thumbnails.Get(pos)->AnimateThumbnailIn();
			return;
		}
	}

	OpWidget::HandleCallback(msg, par1, par2);
}
