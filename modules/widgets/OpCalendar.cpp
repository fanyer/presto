/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/widgets/OpCalendar.h"

#include "modules/forms/datetime.h"
#ifdef PLATFORM_FONTSWITCHING
#include "modules/display/fontdb.h"
#endif // PLATFORM_FONTSWITCHING
#include "modules/widgets/OpMonthView.h"
#include "modules/widgets/WidgetWindow.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/pi/OpLocale.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/display/vis_dev.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/viewers/viewers.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/hardcore/mh/messages.h"


/**
 * TODO:
 * Localized display of the date.
 */

/******************************************************************************
**
**	CalendarWindow - a WidgetWindow subclass for implementing the
**	calendar window
**
******************************************************************************/

class OpCalendarWindow : public WidgetWindow
{
	private:

		OpCalendarWindow() :
		   m_hot_tracking(FALSE),
			   m_calendar(NULL),
			   m_monthview(NULL),
			   m_is_closed(FALSE)
		{
		}

		OP_STATUS Construct(OpCalendar* calendar)
		{
			RETURN_IF_ERROR(Init(OpWindow::STYLE_POPUP,
								 calendar->GetParentOpWindow()));
			m_calendar = calendar;
			m_scale = m_calendar->GetVisualDevice()->GetScale();
			m_can_close = TRUE;

			RETURN_IF_ERROR(OpMonthView::Construct(&m_monthview));

			WidgetContainer* widget_container = GetWidgetContainer();
			widget_container->SetHasDefaultButton(FALSE);
			OpWidget* root_widget = widget_container->GetRoot();
			root_widget->GetVisualDevice()->SetScale(m_scale);
			root_widget->AddChild(m_monthview);

//			m_monthview->SetHasCssBorder(TRUE); // XXX?
			m_monthview->SetListener(calendar);
			OpWidget* monthview_root = m_monthview->GetWidgetContainer()->GetRoot();
			monthview_root->SetCanHaveFocusInPopup(TRUE);
			monthview_root->SetParentInputContext(m_calendar);
//			m_monthview->hot_track = TRUE;
			// We must let the monthview know that it is from a form or it will trigger
			// quick code that makes it repaint itself in an infinite loop (see bug 211757)
			m_monthview->SetFormObject(calendar->GetFormObject(TRUE));

			if (m_calendar->GetColor().use_default_foreground_color == FALSE)
			{
				m_monthview->SetForegroundColor(calendar->GetColor().foreground_color);
			}
			if (m_calendar->GetColor().use_default_background_color == FALSE)
			{
				m_monthview->SetBackgroundColor(calendar->GetColor().background_color);
			}

			m_monthview->GetVisualDevice()->SetDocumentManager(m_calendar->GetVisualDevice()->GetDocumentManager());

			m_monthview->SetFontInfo(m_calendar->font_info.font_info,
									m_calendar->font_info.size,
									m_calendar->font_info.italic,
									m_calendar->font_info.weight,
									m_calendar->font_info.justify,
									m_calendar->font_info.char_spacing_extra);

			// Normally we have a focus rect if the user has navigated
			// with the keyboard but here the user might start with
			// the mouse and then switch to keyboard and the "only"
			// way to get the focus rectangle then is to have it
			// from the beginning.
//			m_monthview->SetHasFocusRect(m_calendar->HasFocusRect());
			m_monthview->SetHasFocusRect(TRUE);
			
			m_monthview->SetParentCalendar(m_calendar);

			return OpStatus::OK;
		}

		virtual void OnActivate(BOOL active)
		{
			if (!active)
			{
				Close();
			}
		}

	public:
	    static OP_STATUS Create(OpCalendarWindow **w, OpCalendar* calendar)
		{
			OpCalendarWindow* &new_window = *w;
			new_window = OP_NEW(OpCalendarWindow, ());
			if (!new_window)
				return OpStatus::ERR_NO_MEMORY;
			
			OP_STATUS status = new_window->Construct(calendar);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(new_window);
				new_window = NULL;
				return status;
			}
			return OpStatus::OK;
		}

		virtual ~OpCalendarWindow()
		{
// XXX			m_calendar->BlockPopupByMouse(); // For X11
// XXX			m_calendar->SetBackOldItem();

			if (m_calendar->GetListener())
			{
				m_calendar->GetListener()->OnDropdownMenu(m_calendar, FALSE);
			}

			m_calendar->Invalidate(m_calendar->GetBounds());
			// Moved it to OnClose
			OP_ASSERT(m_calendar->m_popup_window == NULL);
		}

		BOOL HasClosed() const { return m_is_closed; }
		void SetClosed(BOOL closed) { m_is_closed = closed; }

		virtual BOOL OnNeedPostClose()
		{
			return TRUE;
		}

		virtual void OnClose(BOOL user_initiated)
		{
			// From now on we delete ourselves and it should be as if
			// we wasn't here
			m_calendar->m_popup_window = NULL;
#ifdef _DEBUG
//			Debug::SystemDebugOutput(UNI_L("OpCalendarWindow::OnClose called\n"));
#endif // _DEBUG
			if (m_monthview->HasFocusInWidget())
			{
				m_calendar->SetFocus(FOCUS_REASON_ACTIVATE);
			}
			WidgetWindow::OnClose(user_initiated);
		}

		virtual void OnResize(UINT32 width, UINT32 height)
		{
			WidgetWindow::OnResize(width, height);
			m_monthview->SetRect(OpRect(0, 0, width * 100 / m_scale, height * 100 / m_scale));
		}

		OpRect CalculateRect()
		{
			INT32 width;
			INT32 height;
			// Calculate size of calendar. Should ask the monthview
			m_monthview->GetPreferedSize(&width, &height);

			// scale
			width = (width * m_scale) / 100;
			height = (height * m_scale) / 100;

			return GetBestDropdownPosition(m_calendar, width, height);
		}

		void UpdateRect()
		{
			m_scale = m_calendar->GetVisualDevice()->GetScale();
			GetWidgetContainer()->GetRoot()->GetVisualDevice()->SetScale(m_scale, FALSE);
			OpRect rect = CalculateRect();
			m_window->SetOuterPos(rect.x, rect.y);
			m_window->SetInnerSize(rect.width, rect.height);
		}

		void Activate()
		{
#ifdef _DEBUG
//			Debug::SystemDebugOutput(UNI_L("OpCalendarWindow::Activate called\n"));
#endif // _DEBUG
			if (!m_monthview->HasFocusInWidget())
			{
				m_monthview->SetFocus(FOCUS_REASON_ACTION);
			}
		}

	public:
		BOOL			m_hot_tracking;	// if listbox has started hottracking
		OpCalendar*		m_calendar;	   // pointer back to calendar that owns me
		OpMonthView*	m_monthview;   // listbox in this calendar
		INT32			m_scale;
		BOOL			m_can_close;
		BOOL 			m_is_closed;    // Set to TRUE when window is to be closed
};



// == OpCalendar ===========================================================

DEFINE_CONSTRUCT(OpCalendar)

OpCalendar::OpCalendar() :
	m_readonly(FALSE),
	m_has_min_value(FALSE),
	m_has_max_value(FALSE),
	m_has_value(FALSE),
	m_is_hovering_button(FALSE),
	m_has_step(FALSE),
	m_calendar_mode(CALENDAR_MODE_DAY),
	m_popup_window(NULL)
#ifdef PLATFORM_FONTSWITCHING
	, m_preferred_codepage(OpFontInfo::OP_DEFAULT_CODEPAGE)
#endif
{
	OP_STATUS status;

	status = g_main_message_handler->SetCallBack(this,
											 MSG_CLOSE_AUTOCOMPL_POPUP,
											 (MH_PARAM_1)this);

	SetTabStop(TRUE);

#ifdef SKIN_SUPPORT
	SetSkinned(TRUE);
	GetBorderSkin()->SetImage("Dropdown Skin");
#endif

	if (OpStatus::IsError(status))
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}
}

OpCalendar::~OpCalendar()
{
}

/**
 * This is called to refill the widget after reflow or history movement.
 */
OP_STATUS OpCalendar::SetText(const uni_char* text)
{
	if (!text || !*text)
	{
		m_has_value = FALSE;
		Invalidate(GetBounds());
		return OpStatus::OK;
	}

	if (m_calendar_mode == CALENDAR_MODE_DAY)
	{
		DaySpec date;
		// Parse...
		if (!date.SetFromISO8601String(text))
		{
			return OpStatus::ERR;
		}

#if 0 // Won't stop scripts from doing stuff
		if (m_has_min_value && date.IsBefore(m_min_value) ||
			m_has_max_value && date.IsAfter(m_max_value))
		{
			return OpStatus::ERR_OUT_OF_RANGE;
		}
#endif // 0

		return SetValue(date);
	}
	else if (m_calendar_mode == CALENDAR_MODE_MONTH)
	{
		MonthSpec month;
		// Parse...
		if (!month.SetFromISO8601String(text))
		{
			return OpStatus::ERR;
		}
		return SetValue(month);
	}

	OP_ASSERT(m_calendar_mode == CALENDAR_MODE_WEEK);
	WeekSpec week;
	// Parse...
	if (!week.SetFromISO8601String(text))
	{
		return OpStatus::ERR;
	}

#if 0 // Won't stop scripts from doing stuff
	if (m_has_min_value && date.IsBefore(m_min_value) ||
		m_has_max_value && date.IsAfter(m_max_value))
	{
		return OpStatus::ERR_OUT_OF_RANGE;
	}
#endif // 0

	return SetValue(week);
}


void OpCalendar::OnMonthViewLostFocus()
{
	ClosePopup(TRUE);
}

INT32 OpCalendar::GetTextLength()
{
	if (!m_has_value)
	{
		return 0;
	}
	
	if (m_calendar_mode == CALENDAR_MODE_WEEK)
	{
		return m_week_value.GetISO8601StringLength() + 1;
	}
	else if (m_calendar_mode == CALENDAR_MODE_MONTH)
	{
		return m_month_value.GetISO8601StringLength() + 1;
	}
	else
	{
		OP_ASSERT(m_calendar_mode == CALENDAR_MODE_DAY);
		return m_day_value.GetISO8601StringLength() + 1;
	}
}

void OpCalendar::SetReadOnly(BOOL read_only)
{
	m_readonly = read_only;
	// No buttons or anything else to change the look for
}

OP_STATUS OpCalendar::GetText(OpString &str)
{
	if (m_has_value)
	{
		uni_char* buf = str.Reserve(GetTextLength());
		if (!buf)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		if (m_calendar_mode == CALENDAR_MODE_WEEK)
		{
			m_week_value.ToISO8601String(buf);
		}
		else if (m_calendar_mode == CALENDAR_MODE_MONTH)
		{
			m_month_value.ToISO8601String(buf);
		}
		else
		{
			OP_ASSERT(m_calendar_mode == CALENDAR_MODE_DAY);
			m_day_value.ToISO8601String(buf);
		}
	}
	else
	{
		OP_ASSERT(str.IsEmpty());
	}

	return OpStatus::OK;
}

OP_STATUS OpCalendar::GetLocalizedTextValue(OpString &str)
{
	if (m_has_value)
	{
		const int max_size_str = 40;
		uni_char* ptr = str.Reserve(max_size_str);
		if (!ptr)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		time_t val_as_time_t;
		if (m_calendar_mode == CALENDAR_MODE_WEEK)
		{
			// XXX Localize this somehow
			m_week_value.ToISO8601String(ptr);
		}
		else if (m_calendar_mode == CALENDAR_MODE_MONTH)
		{
			// XXX Localize this somehow
			m_month_value.ToISO8601String(ptr);
			DaySpec first_day = m_month_value.FirstDay();
			val_as_time_t = static_cast<time_t>(first_day.AsDouble())*86400;
			struct tm* the_tm = op_localtime(&val_as_time_t);
			if (the_tm)
			{
				// Only supports dates after 1970. :-(
				size_t len = g_oplocale->op_strftime(ptr, max_size_str-1, UNI_L("%b %y"), the_tm);
				ptr[len] = '\0'; // In case it failed
			}
			else
			{
				// fallback. :-(
				m_day_value.ToISO8601String(ptr);
			}
		}
		else
		{
			OP_ASSERT(m_calendar_mode == CALENDAR_MODE_DAY);
			val_as_time_t = static_cast<time_t>(m_day_value.AsDouble())*86400;
			struct tm* the_tm = op_localtime(&val_as_time_t);
			if (the_tm)
			{
				// Only supports dates after 1970. :-(
				size_t len = g_oplocale->op_strftime(ptr, max_size_str-1, UNI_L("%x"), the_tm);
				ptr[len] = '\0'; // In case it failed
			}
			else
			{
				// fallback. :-(
				m_day_value.ToISO8601String(ptr);
			}
		}
	}
	return OpStatus::OK;
}

void OpCalendar::SetMinValueInternal(DaySpec new_min) 
{
	if (!m_has_min_value || new_min.AsDouble() != m_min_value.AsDouble())
	{
		m_has_min_value = TRUE;
		m_min_value = new_min;
	}
}

void OpCalendar::SetMaxValueInternal(DaySpec new_max) 
{ 
	if (!m_has_max_value || new_max.AsDouble() != m_max_value.AsDouble())
	{
		m_has_max_value = TRUE;
		m_max_value = new_max;
	}
}

void OpCalendar::SetMinValue(WeekSpec new_min) 
{
	OP_ASSERT(m_calendar_mode == CALENDAR_MODE_WEEK);
	SetMinValueInternal(new_min.GetFirstDay());
}

void OpCalendar::SetMinValue(MonthSpec new_min) 
{
	OP_ASSERT(m_calendar_mode == CALENDAR_MODE_MONTH);
	SetMinValueInternal(new_min.FirstDay());
}

void OpCalendar::SetMaxValue(WeekSpec new_max) 
{ 
	OP_ASSERT(m_calendar_mode == CALENDAR_MODE_WEEK);
	SetMaxValueInternal(new_max.GetLastDay());
}

void OpCalendar::SetMaxValue(MonthSpec new_max) 
{
	OP_ASSERT(m_calendar_mode == CALENDAR_MODE_MONTH);
	SetMaxValueInternal(new_max.LastDay());
}

void OpCalendar::SetStep(double step_base, double step)
{
	OP_ASSERT(step >= 0.0);
	if (!m_has_step && step != 0.0 ||
		m_has_step && (step != m_step || step_base != m_step_base))
	{
		if (step == 0.0)
		{
			m_has_step = FALSE;
		}
		else
		{
			m_has_step = TRUE;
			m_step_base = step_base;
			m_step = step;
		}
	}
}

void OpCalendar::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (m_popup_window && !m_popup_window->HasClosed())
		m_popup_window->UpdateRect();

	widget_painter->DrawPopupableString(GetBounds(), IsHoveringButton());
}

void OpCalendar::OnMove()
{
	if (m_popup_window && !m_popup_window->HasClosed())
		m_popup_window->UpdateRect();
}

#ifndef MOUSELESS

void OpCalendar::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (m_popup_window && widget == m_popup_window->m_monthview)
	{
		if (!down)
		{
			ClosePopup();
		}
		listener->OnMouseEvent(this, pos, x, y, button, down, nclicks); // Invoke listener!
	}
}

void OpCalendar::OnMouseMove(const OpPoint &point)
{
	g_widget_globals->m_blocking_popup_calender_widget = NULL;

	BOOL is_inside = GetBounds().Contains(point) && !IsDead();
	if (is_inside)
	{
		if (!m_is_hovering_button)
		{
			m_is_hovering_button = TRUE;
			InvalidateAll();
		}
	}

	// If we drag outside the calendar, we will start hottrack the listbox (bypass all mouse input to it)
	if (m_popup_window && GetBounds().Contains(point) == FALSE)
	{
		OpPoint tpoint = ConvertToMonthViewWindow(point);

		if (!m_popup_window->m_hot_tracking)
		{
			m_popup_window->m_hot_tracking = TRUE;
			m_popup_window->m_monthview->OnMouseDown(tpoint, MOUSE_BUTTON_1, 1);
		}
		else
		{
			m_popup_window->m_monthview->OnMouseMove(tpoint);
		}
	}
}

void OpCalendar::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{

	if (listener)
	{
		listener->OnMouseEvent(this, -1, point.x, point.y, button, TRUE, nclicks);
	}

	if (button != MOUSE_BUTTON_1)
		return;

	OpRect r = GetBounds();
	GetInfo()->AddBorder(this, &r);
	r.width -= GetInfo()->GetDropdownButtonWidth(this);

	Invalidate(GetBounds());

#if defined(_X11_SELECTION_POLICY_)
	if (g_widget_globals->m_blocking_popup_calender_widget == this)
	{
		g_widget_globals->m_blocking_popup_calender_widget = NULL;
		return;
	}
#endif

#ifdef _DEBUG
//	Debug::SystemDebugOutput(UNI_L("OpCalendar taking focus since the user clicked for a popup.\n"));
#endif // _DEBUG
	SetFocus(FOCUS_REASON_MOUSE);

	if (!m_readonly)
		ToggleMonthView();
}

void OpCalendar::OnMouseLeave()
{
	if (m_is_hovering_button)
	{
		m_is_hovering_button = FALSE;
		InvalidateAll();
	}
}

#endif // MOUSELESS

void OpCalendar::ClosePopup(BOOL immediately /* = FALSE */)
{
	if (!m_popup_window)
		return;

	m_popup_window->SetClosed(TRUE);

	if (immediately)
	{
		if (m_popup_window->m_can_close)
		{
			// Must take care of focus before it disappears.
			if (m_popup_window->m_monthview->HasFocusInWidget())
			{
				// A child to us had focus, let's take it
#ifdef _DEBUG
//				Debug::SystemDebugOutput(UNI_L("Taking focus from popup before closing it\n"));
#endif // _DEBUG
				SetFocus(FOCUS_REASON_ACTION);
				if (!m_popup_window) // Might have closed
					return;
			}
			m_popup_window->Close(TRUE);
		}
		else
		{
			// We should not close now so post a new closemessage to do it as soon as possible.
			ClosePopup();
		}
		g_widget_globals->m_blocking_popup_calender_widget = NULL;
	}
	else 
	{
		m_popup_window->Show(FALSE); // Must hide. It's important that we don't get any more OnMouseMove etc.
		g_main_message_handler->PostMessage(MSG_CLOSE_AUTOCOMPL_POPUP, (MH_PARAM_1)this, 0);
	}
}

void OpCalendar::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_CLOSE_AUTOCOMPL_POPUP)
		ClosePopup(TRUE);
	else
		OpWidget::HandleCallback(msg, par1, par2);
}


void OpCalendar::ToggleMonthView()
{
	if (m_popup_window)
	{
		ClosePopup();
		return;
	}

	if (listener)
	{
		listener->OnDropdownMenu(this, TRUE);
	}

	OP_STATUS status = OpCalendarWindow::Create(&m_popup_window, this);
	
	if (OpStatus::IsError(status))
	{
		if (OpStatus::IsMemoryError(status))
		{
			g_memory_manager->RaiseCondition(status);//FIXME:OOM - Can't return OOM error.
		}
		return;
	}

	if (m_calendar_mode == CALENDAR_MODE_WEEK)
	{
		m_popup_window->m_monthview->SetToWeekMode();
	}
	else if (m_calendar_mode == CALENDAR_MODE_MONTH)
	{
		m_popup_window->m_monthview->SetToMonthMode();
	}

	OpRect rect = m_popup_window->CalculateRect();
	m_popup_window->Show(TRUE, &rect);

	// now when we have inited the calender with |Show| we can set
	// some other stuff
	if (m_has_min_value)
	{
		m_popup_window->m_monthview->SetMinDay(m_min_value);
	}
	if (m_has_max_value)
	{
		m_popup_window->m_monthview->SetMaxDay(m_max_value);
	}
	if (m_has_step)
	{
		m_popup_window->m_monthview->SetStep(m_step_base, m_step);
	}
	
	if (m_has_value)
	{
		if (m_calendar_mode == CALENDAR_MODE_WEEK)
		{
			// XXX This is broken
			MonthSpec month = {m_week_value.m_year, (m_week_value.m_week-1)*24/105};
			m_popup_window->m_monthview->SetViewedMonth(month);
			m_popup_window->m_monthview->SetLastSelectedWeek(m_week_value);
		}
		else if (m_calendar_mode == CALENDAR_MODE_MONTH)
		{
			m_popup_window->m_monthview->SetViewedMonth(m_month_value);
			m_popup_window->m_monthview->SetLastSelectedDay(m_month_value.FirstDay());
		}
		else
		{
			OP_ASSERT(m_calendar_mode == CALENDAR_MODE_DAY);
			MonthSpec month = {m_day_value.m_year, m_day_value.m_month};
			m_popup_window->m_monthview->SetViewedMonth(month);
			m_popup_window->m_monthview->SetLastSelectedDay(m_day_value);
		}
	}

	m_popup_window->m_monthview->SetReadOnly(m_readonly);

	m_popup_window->Activate();

#ifdef PLATFORM_FONTSWITCHING
	OpView *view = m_popup_window->GetWidgetContainer()->GetOpView();
	view->SetPreferredCodePage(m_script != WritingSystem::Unknown ?
							   OpFontInfo::CodePageFromScript(m_script) :
							   m_preferred_codepage);
#endif // PLATFORM_FONTSWITCHING
}


void OpCalendar::OnClick(OpWidget *object, UINT32 id)
{
	if (object == m_popup_window->m_monthview)
	{
		if (m_popup_window->m_monthview->LastClickWasOnClear())
		{
			m_has_value = FALSE;
		}
		else if (m_calendar_mode == CALENDAR_MODE_DAY)
		{
			DaySpec clicked;
			if (m_popup_window->m_monthview->GetLastSelectedDay(clicked))
			{
#ifdef DEBUG_ENABLE_OPASSERT
				OP_STATUS status =
#endif
					SetValue(clicked);
				OP_ASSERT(OpStatus::IsSuccess(status));
			}
		}
		else if (m_calendar_mode == CALENDAR_MODE_MONTH)
		{
			DaySpec clicked;
			if (m_popup_window->m_monthview->GetLastSelectedDay(clicked))
			{
				MonthSpec month = {clicked.m_year, clicked.m_month };
#ifdef DEBUG_ENABLE_OPASSERT
				OP_STATUS status =
#endif
					SetValue(month);
				OP_ASSERT(OpStatus::IsSuccess(status));
			}
		}
		else
		{
			OP_ASSERT(m_calendar_mode == CALENDAR_MODE_WEEK);
			WeekSpec clicked;
			if (m_popup_window->m_monthview->GetLastSelectedWeek(clicked))
			{
#ifdef DEBUG_ENABLE_OPASSERT
				OP_STATUS status =
#endif
					SetValue(clicked);
				OP_ASSERT(OpStatus::IsSuccess(status));
			}
		}
		ClosePopup();

		if (listener)
		{
			listener->OnChange(this);
		}
	}
}

OpPoint OpCalendar::ConvertToMonthViewWindow(OpPoint point)
{
	point.x += GetRect(TRUE).x;
	point.y += GetRect(TRUE).y;
	point = vis_dev->GetView()->ConvertToScreen(point);
	OpPoint listpoint = m_popup_window->m_monthview->GetVisualDevice()->GetView()->ConvertToScreen(OpPoint(0, 0));
	point.x -= listpoint.x;
	point.y -= listpoint.y;
	return point;
}

void OpCalendar::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	// *w = 150; // Copied from indpwidgetinfo
	const uni_char* sample_date;
	size_t sample_len;
	if (m_calendar_mode == CALENDAR_MODE_DAY)
	{
		sample_date =  UNI_L("1944-06-06");
		sample_len = 10;
	}
	else if (m_calendar_mode == CALENDAR_MODE_WEEK)
	{
		sample_date =  UNI_L("1942-W12");
		sample_len = 8;
	}
	else
	{
		OP_ASSERT(m_calendar_mode == CALENDAR_MODE_MONTH);
		sample_date =  UNI_L("2004-52");
		sample_len = 7;
	}

	OP_ASSERT(uni_strlen(sample_date) == sample_len);
	int str_w = GetVisualDevice()->GetFontStringWidth(sample_date, sample_len);
	*w = str_w + 6 + GetInfo()->GetDropdownButtonWidth(this); // room for borders, padding and the dropdown button
}


OP_STATUS OpCalendar::SetValueInternal(DaySpec new_value)
{
	OP_ASSERT(m_calendar_mode == CALENDAR_MODE_DAY);
	// Quick and dirty
	m_has_value = TRUE;
	m_day_value = new_value;

	// The painter reads this value when it paints the widget

	InvalidateAll();
	return OpStatus::OK;
}

OP_STATUS OpCalendar::SetValue(WeekSpec new_value)
{
	OP_ASSERT(m_calendar_mode == CALENDAR_MODE_WEEK);
	// Quick and dirty
	m_has_value = TRUE;
	m_week_value = new_value;

	// The painter reads this value when it paints the widget

	InvalidateAll();
	return OpStatus::OK;
}

OP_STATUS OpCalendar::SetValue(MonthSpec new_value)
{
	OP_ASSERT(m_calendar_mode == CALENDAR_MODE_MONTH);
	// Quick and dirty
	m_has_value = TRUE;
	m_month_value = new_value;

	// The painter reads this value when it paints the widget

	InvalidateAll();
	return OpStatus::OK;
}

/* virtual */
void OpCalendar::OnKeyboardInputLost(OpInputContext* new_input_context, 
									 OpInputContext* old_input_context, 
									 FOCUS_REASON reason)
{
	OpInputContext* context = new_input_context;
	BOOL child_got_focus = FALSE;
	if (m_popup_window)
	{
		while (context)
		{
			context = context->GetParentInputContext();
			if (context == this)
			{
				child_got_focus = TRUE;
				break;
			}
		}

#ifdef _DEBUG
		OP_ASSERT(child_got_focus == m_popup_window->m_monthview->HasFocusInWidget());
#endif // _DEBUG
	}

#if 0 && defined _DEBUG
	if (child_got_focus)
		Debug::SystemDebugOutput(UNI_L("OpCalendar::OnKeyboardInputLost called with child_got_focus\n"));
	else
		Debug::SystemDebugOutput(UNI_L("OpCalendar::OnKeyboardInputLost called with !child_got_focus\n"));

#endif // _DEBUG
	if (!child_got_focus && m_popup_window)
	{
		ClosePopup();
	}
	// Parent, default implementation
	OpInputContext::OnKeyboardInputLost(new_input_context, old_input_context, reason);
}


void OpCalendar::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (!focus)
	{
#ifdef _DEBUG
//		Debug::SystemDebugOutput(UNI_L("OpCalendar::OnFocus(FALSE, ...)\n"));
#endif // _DEBUG
		// We must be sure that we don't leave any popup windows open when the
		// user switches attention somewhere else.
//		ClosePopup();
	}
	else if (m_popup_window)
	{
#ifdef _DEBUG
//		Debug::SystemDebugOutput(UNI_L("OpCalendar::OnFocus(TRUE, ...) called with m_popup\n"));
#endif // _DEBUG
		// This can happen when windows are activated/inactivated since
		// the input context system then cuts away all focus that is
		// in another window
		m_popup_window->Activate();
//		OP_ASSERT(FALSE);
//		m_popup_window->SetFocus(
	}
#ifdef _DEBUG
	else
	{
//		Debug::SystemDebugOutput(UNI_L("OpCalendar::OnFocus(TRUE, ...) called with no m_popup\n"));
	}
#endif // _DEBUG

	// We look different with and without focus so we need to repaint ourselves.
	Invalidate(GetBounds());
}


/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/
BOOL OpCalendar::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_FOCUS_NEXT_WIDGET:
		case OpInputAction::ACTION_FOCUS_PREVIOUS_WIDGET:
			{
				if (!m_popup_window)
				{
					return FALSE; // Normal focus
				}

#if 1
				BOOL forward = action->GetAction() == OpInputAction::ACTION_FOCUS_NEXT_WIDGET;
				OpWidget* next = m_popup_window->m_monthview->GetNextInternalTabStop(forward);
				if (next)
				{
					next->SetFocus(FOCUS_REASON_KEYBOARD);
				}
				else if (forward)
				{
					m_popup_window->m_monthview->SetFocusToFirstWidget(FOCUS_REASON_ACTION);
				}
				else
				{
					m_popup_window->m_monthview->SetFocusToLastWidget(FOCUS_REASON_ACTION);
				}

				return TRUE;
#else
//				return m_popup_window->m_monthview->OnInputAction(action);
				return FALSE;
#endif

			}

		case OpInputAction::ACTION_CLEAR:
		{
			m_has_value = FALSE;
			Invalidate(GetBounds());
			return TRUE;
		}

		case OpInputAction::ACTION_NEXT_ITEM:
		case OpInputAction::ACTION_PREVIOUS_ITEM:
			{
				// Step to the previous or next day/week/month from the current value (or today if empty)
				BOOL next = action->GetAction() == OpInputAction::ACTION_NEXT_ITEM;
				if (m_calendar_mode == CALENDAR_MODE_DAY)
				{
					DaySpec d = m_has_value ? GetDaySpec() : OpMonthView::GetToday();
					SetValue(next ? d.NextDay() : d.PrevDay());
				}
				else if (m_calendar_mode == CALENDAR_MODE_WEEK)
				{
					WeekSpec w = m_has_value ? GetWeekSpec() : OpMonthView::GetToday().GetWeek();
					SetValue(next ? w.NextWeek() : w.PrevWeek());
				}
				else if (m_calendar_mode == CALENDAR_MODE_MONTH)
				{
					MonthSpec m = m_has_value ? GetMonthSpec() : OpMonthView::GetToday().Month();
					SetValue(next ? m.NextMonth() : m.PrevMonth());
				}
			}
			return TRUE;

		case OpInputAction::ACTION_SHOW_DROPDOWN:
			ToggleMonthView();
			if (!m_popup_window)
			{
				SetFocus(FOCUS_REASON_KEYBOARD); // Take focus so that focus won't leave the widget completely when the popup is closed.
			}
			return TRUE;

		case OpInputAction::ACTION_CLOSE_DROPDOWN:
		case OpInputAction::ACTION_STOP:
			{
				if (m_popup_window)
				{
					ClosePopup();

					return TRUE;
				}
				return FALSE; // Let someone else take care of the stop
			}
	}

	if (m_popup_window)
	{
		return m_popup_window->m_monthview->OnInputAction(action);
	}

	return FALSE;

}

BOOL OpCalendar::IsParentInputContextAvailabilityRequired()
{
	// Normally we don't allow focus to move to a different window, but
	// we really need focus in this one to make it usable.
	if (m_popup_window)
	{
		return FALSE;
	}

	return TRUE;
}
