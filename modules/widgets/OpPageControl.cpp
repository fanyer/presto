/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef PAGED_MEDIA_SUPPORT

#include "modules/widgets/OpPageControl.h"

#include "modules/display/vis_dev.h"
#include "modules/widgets/OpButton.h"

/** Special radio button for the page control. They shouldn't take focus. */
class OpPcRadioButton : public OpRadioButton
{
public:
	static OP_STATUS Construct(OpPcRadioButton** obj);
	BOOL IsInputContextAvailable(FOCUS_REASON reason) { return FALSE; }
};

DEFINE_CONSTRUCT(OpPcRadioButton);

void OpPageControl::PcWidgetListener::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (!down || button != MOUSE_BUTTON_1)
		return;

	unsigned int count = 0;

	for (OpWidget* child = page_control->GetFirstChild(); child; child = child->Suc())
		if (child->GetType() == WIDGET_TYPE_RADIOBUTTON)
		{
			BOOL checked = child == widget;

			if (!checked != !child->GetValue())
			{
				child->SetValue(checked);

				if (checked && page_control->page_control_listener)
					page_control->page_control_listener->OnPageChange(count);
			}

			count++;
		}
}

OP_STATUS OpPageControl::Construct(OpPageControl** obj)
{
	*obj = OP_NEW(OpPageControl, ());

	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS res = (*obj)->Init();

	if (OpStatus::IsError(res))
	{
		OP_DELETE(*obj);
		*obj = NULL;
		return res;
	}

	return OpStatus::OK;
}

OpPageControl::OpPageControl()
	: widget_listener(this),
	  page_control_listener(NULL),
	  current_page(0),
	  page_count(0),
	  packed_init(0)
{
}

void OpPageControl::SetPage(unsigned int page)
{
	if (current_page != page)
	{
		current_page = page;

		if (packed.one_button_per_page)
		{
			unsigned int count = 0;

			for (OpWidget* child = GetFirstChild(); child; child = child->Suc())
				if (child->GetType() == WIDGET_TYPE_RADIOBUTTON)
				{
					child->SetValue(count == current_page);
					count++;
				}
		}
		else
			Invalidate(GetBounds());
	}
}

void OpPageControl::SetPageCount(unsigned int count)
{
	if (page_count != count)
	{
		page_count = count;

		if (OpStatus::IsMemoryError(Layout()))
			ReportOOM();
	}
}

void OpPageControl::OnLayout()
{
	OpWidget::OnLayout();

	if (OpStatus::IsMemoryError(Layout()))
		ReportOOM();
}

void OpPageControl::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (OpStatus::IsMemoryError(Paint(widget_painter)))
		ReportOOM();
}

#ifndef MOUSELESS

void OpPageControl::OnSetCursor(const OpPoint &point)
{
}

void OpPageControl::OnMouseMove(const OpPoint &point)
{
	BOOL over_left = IsOverLeftButton(point);
	BOOL over_right = IsOverRightButton(point);

	if (!packed.left_hovered != !over_left || !packed.right_hovered != !over_right)
	{
		packed.left_hovered = over_left;
		packed.right_hovered = over_right;
		Invalidate(GetBounds());
	}
}

void OpPageControl::OnMouseLeave()
{
	packed.left_hovered = 0;
	packed.right_hovered = 0;
	Invalidate(GetBounds());
}

void OpPageControl::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button != MOUSE_BUTTON_1)
		return;

	int page_delta = 0;
	BOOL over_left = IsOverLeftButton(point);
	BOOL over_right = IsOverRightButton(point);

	if (over_left)
		page_delta =
#ifdef SUPPORT_TEXT_DIRECTION
			packed.is_rtl && !packed.is_vertical ? 1 :
#endif // SUPPORT_TEXT_DIRECTION
			-1;
	else
		if (over_right)
			page_delta =
#ifdef SUPPORT_TEXT_DIRECTION
				packed.is_rtl && !packed.is_vertical ? -1 :
#endif // SUPPORT_TEXT_DIRECTION
				1;

	if (!packed.left_pressed != !over_left || !packed.right_pressed != !over_right)
	{
		packed.left_pressed = over_left;
		packed.right_pressed = over_right;
		Invalidate(GetBounds());
	}

	int new_page = (int) current_page + page_delta;

	if (new_page < 0)
		new_page = 0;

	if ((unsigned int) new_page != current_page && page_control_listener)
		page_control_listener->OnPageChange(new_page);
}

void OpPageControl::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button != MOUSE_BUTTON_1)
		return;

	packed.left_pressed = 0;
	packed.right_pressed = 0;
	Invalidate(GetBounds());
}

#endif // !MOUSELESS

void OpPageControl::OnResize(INT32* new_w, INT32* new_h)
{
	OpWidget::OnResize(new_w, new_h);

	if (OpStatus::IsMemoryError(Layout()))
		ReportOOM();
}

OP_STATUS OpPageControl::Init()
{
	return OpStatus::OK;
}

void OpPageControl::GetLeftSwitcherDimensions(int& x, int& width)
{
	width = GetHeight();
	x = 0;
}

void OpPageControl::GetRightSwitcherDimensions(int& x, int& width)
{
	width = GetHeight();
	x = GetBounds().width - width;
}

void OpPageControl::GetUpSwitcherDimensions(int& x, int& width)
{
	width = GetHeight();
	x = 0;
}

void OpPageControl::GetDownSwitcherDimensions(int& x, int& width)
{
	width = GetHeight();
	x = GetBounds().width - width;
}

BOOL OpPageControl::IsOverLeftButton(const OpPoint& point)
{
	int x, width;

	if (packed.is_vertical)
		GetUpSwitcherDimensions(x, width);
	else
		GetLeftSwitcherDimensions(x, width);

	return point.x >= x && point.x < x + width;
}

BOOL OpPageControl::IsOverRightButton(const OpPoint& point)
{
	int x, width;

	if (packed.is_vertical)
		GetDownSwitcherDimensions(x, width);
	else
		GetRightSwitcherDimensions(x, width);

	return point.x >= x && point.x < x + width;
}

void OpPageControl::DeleteChildren()
{
	for (OpWidget* child = GetFirstChild(); child;)
	{
		OpWidget* next_child = child->Suc();

		if (child->GetType() == WIDGET_TYPE_RADIOBUTTON)
			child->Delete();

		child = next_child;
	}
}

OP_STATUS OpPageControl::Layout()
{
	DeleteChildren();
	packed.one_button_per_page = 0;

#ifdef SKIN_SUPPORT
	SetSkinned(FALSE);
#endif // SKIN_SUPPORT

	if (page_count <= 0)
		return OpStatus::OK;

	OpRect rect = GetBounds();

	// Create one radio button for each page, if there is enough space.

	int radio_button_size = CalculateRadioButtonSize();
	int y = (rect.height - radio_button_size) / 2 + 1;
	int available_buttons_width = rect.width - radio_button_size * 2;
	int increment = radio_button_size + 4;

	if (available_buttons_width > 0 &&
		increment * page_count <= (unsigned int) available_buttons_width)
	{
		packed.one_button_per_page = 1;

		// No need to cram stuff together if we have plenty of space.

		increment = radio_button_size * 3 + 4;

		if (increment * page_count > (unsigned int) available_buttons_width)
			increment = available_buttons_width / page_count;

#ifdef SUPPORT_TEXT_DIRECTION
		if (packed.is_rtl && !packed.is_vertical)
			increment = -increment;
#endif // SUPPORT_TEXT_DIRECTION

		int pos = (rect.width - (radio_button_size + increment * (page_count - 1))) / 2;
		unsigned int count = 0;

		for (unsigned i = 0; i < page_count; i++)
		{
			OpPcRadioButton* button = NULL;

			RETURN_IF_ERROR(OpPcRadioButton::Construct(&button));
			OpWidget::AddChild(button);
			button->SetListener(&widget_listener);

			if (count == current_page)
				button->SetValue(1);

			button->SetRect(OpRect(pos, y, radio_button_size, radio_button_size));
			pos += increment;

			count++;
		}
	}
#ifdef SKIN_SUPPORT
	else
	{
		/* Not enough space for one radio button for each page. Display page
		   info and previous / next buttons instead. No widgets will be created
		   for this, as there exists no suitable class for it. Just set a
		   background skin and deal with the rest on paint. */

		GetBorderSkin()->SetImage("Page Control Horizontal Skin");
		SetSkinned(TRUE);
	}
#endif // SKIN_SUPPORT

	return OpStatus::OK;
}

OP_STATUS OpPageControl::Paint(OpWidgetPainter* widget_painter)
{
	if (packed.one_button_per_page)
	{
		// Draw a horizontal line above the page buttons.

		vis_dev->SetColor(GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_DARK));

		OpPoint left;
		OpPoint right;

#ifdef SUPPORT_TEXT_DIRECTION
		if (packed.is_rtl && !packed.is_vertical)
		{
			left = GetLastChild()->GetRect().TopLeft();
			right = GetFirstChild()->GetRect().TopRight();
		}
		else
#endif // SUPPORT_TEXT_DIRECTION
		{
			left = GetFirstChild()->GetRect().TopLeft();
			right = GetLastChild()->GetRect().TopRight();
		}

		int radio_button_size = CalculateRadioButtonSize();

		vis_dev->DrawLine(left + OpPoint(-radio_button_size / 2, -3),
						  right + OpPoint(radio_button_size / 2 - 1, -3),
						  1);
	}
	else
	{
		// Draw background, if the skin system hasn't already done so.

		widget_painter->DrawPageControlBackground(GetBounds());

		// Draw left and right page switcher buttons.

		int x, width;
		int left, right;

		if (packed.is_vertical)
		{
			GetUpSwitcherDimensions(x, width);
			left = x + width;
			widget_painter->DrawPageControlButton(OpRect(x, 0, width, rect.height), ARROW_UP, packed.left_pressed, packed.left_hovered);

			GetDownSwitcherDimensions(x, width);
			right = x - 1;
			widget_painter->DrawPageControlButton(OpRect(x, 0, width, rect.height), ARROW_DOWN, packed.right_pressed, packed.right_hovered);
		}
		else
		{
			GetLeftSwitcherDimensions(x, width);
			left = x + width;
			widget_painter->DrawPageControlButton(OpRect(x, 0, width, rect.height), ARROW_LEFT, packed.left_pressed, packed.left_hovered);

			GetRightSwitcherDimensions(x, width);
			right = x - 1;
			widget_painter->DrawPageControlButton(OpRect(x, 0, width, rect.height), ARROW_RIGHT, packed.right_pressed, packed.right_hovered);
		}

		// Draw page info text.

		int available_width = right - left;
		OpString txt;

		RETURN_IF_ERROR(txt.AppendFormat(UNI_L("%d / %d"), current_page + 1, page_count));

		int text_width = vis_dev->GetTxtExtent(txt.CStr(), txt.Length());

		if (text_width > available_width)
		{
			txt.Empty();
			RETURN_IF_ERROR(txt.AppendFormat(UNI_L("%d"), current_page + 1));
			text_width = vis_dev->GetTxtExtent(txt.CStr(), txt.Length());
		}

		if (text_width < available_width)
		{
			int y = (rect.height - (int) vis_dev->GetFontHeight()) / 2;
			int offset = (rect.width - text_width) / 2;

			vis_dev->SetColor(GetInfo()->GetSystemColor(OP_SYSTEM_COLOR_TEXT));
			vis_dev->TxtOut(offset, y, txt.CStr(), txt.Length());
		}
	}

	return OpStatus::OK;
}

#endif // PAGED_MEDIA_SUPPORT
