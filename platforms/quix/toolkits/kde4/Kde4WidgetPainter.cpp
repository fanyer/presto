/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "platforms/quix/toolkits/kde4/Kde4WidgetPainter.h"

#include <QStyle>
#include <QPainter>
#include <QStyleOptionSlider>
#include <QScrollBar>
#include <QSlider>
#include <KDE/KApplication>

class Kde4WidgetPainter::Scrollbar : public ToolkitScrollbar
{
public:
	Scrollbar(KApplication* app) : m_app(app), m_hit_part(NONE), m_hover_part(NONE) {}

	// From ToolkitScrollbar
	virtual void SetOrientation(Orientation orientation) { m_info.orientation = orientation == VERTICAL ? Qt::Vertical : Qt::Horizontal; }
	virtual void SetValueAndRange(int value, int min, int max, int visible);
	virtual HitPart GetHitPart(int x, int y, int width, int height);
	virtual void GetKnobRect(int& x, int& y, int& width, int& height);
	virtual void SetHitPart(HitPart part) { m_hit_part = part; }
	virtual void SetHoverPart(HitPart part) { m_hover_part = part; }

	virtual void Draw(uint32_t* bitmap, int width, int height);

	virtual QStyleOptionSlider GetQStyleOptionSlider() {return m_info;}

private:
	void AddActiveSubControl(HitPart part);

	KApplication* m_app;
	QStyleOptionSlider m_info;
	HitPart m_hit_part;
	HitPart m_hover_part;
};


class Kde4WidgetPainter::Slider : public ToolkitSlider
{
public:
	Slider(KApplication* app) : m_app(app), m_hover_part(NONE) {}

	virtual void SetValueAndRange(int value, int min, int max, int num_tick_values);
	virtual void SetOrientation(Orientation orientation);
	virtual void SetState(int state);
	virtual void SetHoverPart(HitPart part);
	virtual void GetKnobRect(int& x, int& y, int& width, int& height);
	virtual void GetTrackPosition(int& start_x, int& start_y, int& stop_x, int& stop_y);

	virtual void Draw(uint32_t* bitmap, int width, int height);

private:
	KApplication* m_app;
	QStyleOptionSlider m_info;
	HitPart m_hover_part;
};


ToolkitScrollbar* Kde4WidgetPainter::CreateScrollbar()
{
	return new Kde4WidgetPainter::Scrollbar(m_app);
}


int Kde4WidgetPainter::GetVerticalScrollbarWidth()
{
	return m_app->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
}

int Kde4WidgetPainter::GetHorizontalScrollbarHeight()
{
	return m_app->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
}

int Kde4WidgetPainter::GetScrollbarFirstButtonSize()
{
	QRect groove = GetVerticalScrollbarGroove(200);

	// First button ends where the groove starts
	return groove.y();
}

int Kde4WidgetPainter::GetScrollbarSecondButtonSize()
{
	QRect groove = GetVerticalScrollbarGroove(200);

	// Second button is everything after the groove
	return 200 - (groove.y() + groove.height());
}

QRect Kde4WidgetPainter::GetVerticalScrollbarGroove(int scrollbar_size)
{
	QStyleOptionSlider info;
	info.maximum = info.pageStep = scrollbar_size;
	info.orientation = Qt::Vertical;
	info.rect = QRect(0, 0, m_app->style()->pixelMetric(QStyle::PM_ScrollBarExtent), scrollbar_size);

	return m_app->style()->subControlRect(QStyle::CC_ScrollBar, &info, QStyle::SC_ScrollBarGroove);
}


void Kde4WidgetPainter::Scrollbar::SetValueAndRange(int value, int min, int max, int visible)
{
	m_info.minimum = min;
	m_info.maximum = max;
	m_info.pageStep = visible;
	m_info.sliderPosition = value;
	m_info.sliderValue = value;
}


void Kde4WidgetPainter::Scrollbar::Draw(uint32_t* bitmap, int width, int height)
{
	QImage image((uchar*)bitmap, width, height, QImage::Format_ARGB32_Premultiplied);
	QPainter painter(&image);

	m_info.rect = QRect(0, 0, width, height);

	m_info.state = QStyle::State_Enabled;
	if (m_info.orientation == Qt::Horizontal)
		m_info.state |= QStyle::State_Horizontal;

	m_info.activeSubControls = QStyle::SC_None;

	if (m_hit_part != NONE)
	{
		AddActiveSubControl(m_hit_part);
		m_info.state |= QStyle::State_Sunken;
	}
	if (m_hover_part != NONE)
	{
		AddActiveSubControl(m_hover_part);
		m_info.state |= QStyle::State_MouseOver;
	}

	AddActiveSubControl(m_hover_part);

	QScrollBar scrollbar;
	painter.fillRect(m_info.rect, scrollbar.palette().color(QPalette::Window));
	m_app->style()->drawComplexControl(QStyle::CC_ScrollBar, &m_info, &painter);
}


void Kde4WidgetPainter::Scrollbar::AddActiveSubControl(HitPart part)
{
	switch (part)
	{
		case ARROW_SUBTRACT:
			m_info.activeSubControls |= QStyle::SC_ScrollBarSubLine; break;
		case ARROW_ADD:
			m_info.activeSubControls |= QStyle::SC_ScrollBarAddLine; break;
		case TRACK_SUBTRACT:
			m_info.activeSubControls |= QStyle::SC_ScrollBarSubPage; break;
		case TRACK_ADD:
			m_info.activeSubControls |= QStyle::SC_ScrollBarAddPage; break;
		case KNOB:
			m_info.activeSubControls |= QStyle::SC_ScrollBarSlider; break;
	}
}


ToolkitScrollbar::HitPart Kde4WidgetPainter::Scrollbar::GetHitPart(int x, int y, int width, int height)
{
	m_info.rect = QRect(0, 0, width, height);
	m_info.state = QStyle::State_Enabled;
	if (m_info.orientation == Qt::Horizontal)
		m_info.state |= QStyle::State_Horizontal;

	switch (m_app->style()->hitTestComplexControl(QStyle::CC_ScrollBar, &m_info, QPoint(x, y)))
	{
		case QStyle::SC_ScrollBarAddLine:
			return ARROW_ADD;
		case QStyle::SC_ScrollBarSubLine:
			return ARROW_SUBTRACT;
		case QStyle::SC_ScrollBarAddPage:
			return TRACK_ADD;
		case QStyle::SC_ScrollBarSubPage:
			return TRACK_SUBTRACT;
		case QStyle::SC_ScrollBarSlider:
			return KNOB;
	}

	return NONE;
}


void Kde4WidgetPainter::Scrollbar::GetKnobRect(int& x, int& y, int& width, int& height)
{
	QRect knob_rect (m_app->style()->subControlRect(QStyle::CC_ScrollBar, &m_info, QStyle::SC_ScrollBarSlider));
	x = knob_rect.x();
	y = knob_rect.y();
	width = knob_rect.width();
	height = knob_rect.height();
}




ToolkitSlider* Kde4WidgetPainter::CreateSlider()
{
	return new Kde4WidgetPainter::Slider(m_app);
}

void Kde4WidgetPainter::Slider::SetValueAndRange(int value, int min, int max, int num_tick_values)
{
	m_info.minimum = min;
	m_info.maximum = max;
	m_info.sliderPosition = value;
	m_info.sliderValue = value;
	if (num_tick_values > 1)
	{
		m_info.tickPosition = QSlider::TicksBelow;
		m_info.tickInterval = (max - min) / (num_tick_values - 1);
	}
}

void Kde4WidgetPainter::Slider::SetOrientation(Orientation orientation)
{
	m_info.orientation = orientation == VERTICAL ? Qt::Vertical : Qt::Horizontal;
}

void Kde4WidgetPainter::Slider::SetState(int state)
{
	m_info.state = QStyle::State_None;
	if (state & ENABLED)
		m_info.state |= QStyle::State_Enabled;
	if (state & FOCUSED)
		m_info.state |= QStyle::State_HasFocus;
	if (state & ACTIVE)
		m_info.state |= QStyle::State_Active;
	if (state & RTL)
		m_info.direction = Qt::RightToLeft;
}

void Kde4WidgetPainter::Slider::SetHoverPart(HitPart part)
{
	if (part == KNOB)
	{
		m_info.activeSubControls |= QStyle::SC_SliderHandle;
		m_info.state |= QStyle::State_MouseOver;
	}
}

void Kde4WidgetPainter::Slider::Draw(uint32_t* bitmap, int width, int height)
{
	QImage image((uchar*)bitmap, width, height, QImage::Format_ARGB32_Premultiplied);
	QPainter painter(&image);

	// TODO. We should try to center the drawn content. subControlRect() for SC_SliderGroove
	// and SC_SliderHandle do not always return a rect that works with all styles.
	// plastique centers the knob, but not the slider when whe x and y is set

	m_info.rect = QRect(0, 0, width, height);

	// This element requires an initialized background due to the non-rectangular
	// shape often drawn. So we do not initialize the backgound here.

	QSlider widget; // oxygen (observed in kde 4.4.5) crashes unless we provide a widget
	m_app->style()->drawComplexControl(QStyle::CC_Slider, &m_info, &painter, &widget);
}

void Kde4WidgetPainter::Slider::GetKnobRect(int& x, int& y, int& width, int& height)
{
	QRect knob_rect (m_app->style()->subControlRect(QStyle::CC_Slider, &m_info, QStyle::SC_SliderHandle));
	x = knob_rect.x();
	y = knob_rect.y();
	width = knob_rect.width();
	height = knob_rect.height();
}

void Kde4WidgetPainter::Slider::GetTrackPosition(int& start_x, int& start_y, int& stop_x, int& stop_y)
{
	int thickness = m_app->style()->pixelMetric(QStyle::PM_SliderThickness);
	if (m_info.orientation == Qt::Horizontal)
	{
		start_x = m_info.rect.x();
		start_y = m_info.rect.y() + (m_info.rect.y()-thickness)/2;

		stop_x = start_x + m_info.rect.width();
		stop_y = start_y;
	}
	else
	{
		start_x = m_info.rect.x() + (m_info.rect.width()-thickness)/2;
		start_y = m_info.rect.y();

		stop_x = start_x;
		stop_y = start_y + m_info.rect.height();
	}
}
