/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Rune Myrland (runem)
 */


#include "Kde4SkinElement.h"

#include <QDebug>
#include <QRect>
#include <QPaintDevice>
#include <QPainter>
#include <QStylePainter>
#include <QStyleOptionHeader>
#include <QStyleOptionTabWidgetFrame>
#include <QStyleOptionMenuItem>
#include <QTableView>
#include <QComboBox>
#include <QToolButton>
#include <QLineEdit>

namespace Kde4SkinElements
{
	QStyle::State NativeStateToStyleState(int state)
	{
		QStyle::State style_state = QStyle::State_None;

		if (state & NativeSkinElement::STATE_HOVER)
			style_state |= QStyle::State_MouseOver;
		if (state & NativeSkinElement::STATE_PRESSED)
			style_state |= QStyle::State_Sunken;
		if (state & NativeSkinElement::STATE_SELECTED)
			style_state |= QStyle::State_Selected;
		if (state & NativeSkinElement::STATE_FOCUSED)
			style_state |= QStyle::State_HasFocus;
		if (!(state & NativeSkinElement::STATE_DISABLED))
			style_state |= QStyle::State_Enabled;

		return style_state;
	}
};
using namespace Kde4SkinElements;


Kde4SkinElement::Kde4SkinElement()
{
}

void Kde4SkinElement::Draw(uint32_t* bitmap, int width, int height, const NativeRect& clip_rect, int state)
{
	QImage image((uchar*)bitmap, width, height, QImage::Format_ARGB32_Premultiplied);

	if (IsOpaque())
	{
		QWidget widget;
		image.fill(widget.palette().color(QPalette::Window).rgb());
	}
	else
	{
		image.fill(0);
	}

	QPainter painter(&image);
	painter.setClipRect(QRect(clip_rect.x, clip_rect.y, clip_rect.width, clip_rect.height));

	DrawElement(painter, width, height, state);
}


void Kde4SkinElement::ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state)
{
	int horizontal = qApp->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);
	int vertical = qApp->style()->pixelMetric(QStyle::PM_LayoutVerticalSpacing);

	if (horizontal < 0)
		horizontal = qApp->style()->layoutSpacing(ControlType(), ControlType(), Qt::Horizontal);

	if (vertical < 0)
		vertical = qApp->style()->layoutSpacing(ControlType(), ControlType(), Qt::Vertical);

	left = right = horizontal;
	top = bottom = vertical;
}


template<class OptionClass, class WidgetClass> void Kde4SkinElement::DrawControl(QStyle::ControlElement control, QPainter& painter, int width, int height, int state)
{
	OptionClass options;
	SetOptions(options, width, height, state);

	DrawControl<WidgetClass>(control, painter, options);
}


template<class WidgetClass> void Kde4SkinElement::DrawControl(QStyle::ControlElement control, QPainter& painter, QStyleOption& options)
{
	WidgetClass widget;
	qApp->style()->drawControl(control, &options, &painter, &widget);
}


void Kde4SkinElement::SetOptions(QStyleOption& options, int width, int height, int state)
{
	options.state = NativeStateToStyleState(state);
	options.rect = QRect(0, 0, width, height);
	options.direction = (state & STATE_RTL) ? Qt::RightToLeft : Qt::LeftToRight;
}


#ifdef DEBUG
void UnsupportedWidget::DrawElement(QPainter& painter, int width, int height, int state)
{
	int r = 255, g = 192, b = 192;
	if (state & NativeSkinElement::STATE_HOVER)
		r /= 2;
	if (state & NativeSkinElement::STATE_PRESSED)
		g /= 2;
	if (state & NativeSkinElement::STATE_SELECTED)
		b /= 2;
	if (!(state & NativeSkinElement::STATE_DISABLED)) {
		r = r * 2 / 3;
		g = g * 2 / 3;
		b = b * 2 / 3;
	}

	QColor c(r, g, b);

	// Paint red to show clearly where unsupported stuff is
	painter.fillRect(QRect(0, 0, width, height), c);
}
#endif // DEBUG


void PushButton::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionButton options;
	SetOptions(options, width, height, state);
	if (!(state&NativeSkinElement::STATE_PRESSED))
		options.state |= QStyle::State_Raised;
	DrawControl<QPushButton>(QStyle::CE_PushButton, painter, options);
}

void MainbarButton::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionToolButton options;
	SetOptions(options, width, height, state);
	if (state & NativeSkinElement::STATE_HOVER)
		qApp->style()->drawPrimitive(QStyle::PE_PanelButtonTool, &options, &painter);
}

void MainbarButton::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	left   = 4; // values from the unix skin file - should probably use some metrics?
	top    = 3;
	right  = 5;
	bottom = 4;
}

void MainbarButton::ChangeDefaultSpacing(int& spacing, int state)
{
	spacing = 2; // value from the unix skin file
}

void MainbarButton::ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state)
{
	QToolButton toolbutton;
	QPalette palette = toolbutton.palette();

	QColor color;
	if (state & NativeSkinElement::STATE_DISABLED)
		color = palette.color(QPalette::Disabled, QPalette::Text); 
	else
		color = palette.color(QPalette::Active, QPalette::Text); 

	alpha = color.alpha();
	red   = color.red();
	green = color.green();
	blue  = color.blue();
}

void WindowBackground::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOption options;
	SetOptions(options, width, height, state);

	QWidget widget;
	widget.setWindowFlags(Qt::Window);
	widget.resize(width, height);
	qApp->style()->polish(&widget);

	qApp->style()->drawPrimitive(QStyle::PE_Widget, &options, &painter, &widget);
}


void PushButton::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	left = right = qApp->style()->pixelMetric(QStyle::PM_ButtonMargin);
}


void CheckBox::ChangeDefaultSize(int& width, int& height, int state)
{
	width = qApp->style()->pixelMetric(QStyle::PM_IndicatorWidth);
	height = qApp->style()->pixelMetric(QStyle::PM_IndicatorHeight);
}


void CheckBox::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionButton options;
	SetOptions(options, width, height, state);

	if (state & NativeSkinElement::STATE_INDETERMINATE)
		options.state |= QStyle::State_NoChange;
	else if (state & NativeSkinElement::STATE_SELECTED)
		options.state |= QStyle::State_On;
	else
		options.state |= QStyle::State_Off;

	qApp->style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &options, &painter);
}


void RadioButton::ChangeDefaultSize(int& width, int& height, int state)
{
	width = qApp->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth);
	height = qApp->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorHeight);
}

void RadioButton::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionButton options;
	SetOptions(options, width, height, state);

	if((state & NativeSkinElement::STATE_SELECTED))
		options.state |= QStyle::State_On;

	qApp->style()->drawPrimitive(QStyle::PE_IndicatorRadioButton, &options, &painter);
}


void DefaultPushButton::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionButton options;
	SetOptions(options, width, height, state);
	options.features |= QStyleOptionButton::DefaultButton;
	if (!(state&NativeSkinElement::STATE_PRESSED))
		options.state |= QStyle::State_Raised;
	DrawControl<QPushButton>(QStyle::CE_PushButton, painter, options);
}


void MenuBar::DrawElement(QPainter& painter, int width, int height, int state)
{
	QMainWindow dummymainwindow; // Do not revert the order of these two
	QMenuBar widget;

	QRect menubar_rect = QRect(0, 0, width, height);
	QRegion emptyArea(menubar_rect);

	widget.setParent(&dummymainwindow);
	widget.setGeometry(menubar_rect);

	// Draw menubar border
	int w = qApp->style()->pixelMetric(QStyle::PM_MenuBarPanelWidth);
	if (w > 0)
	{
		QRegion borderReg;
		borderReg += QRect(0, 0, w, height); //left
		borderReg += QRect(width-w, 0, w, height); //right
		borderReg += QRect(0, 0, width, w); //top
		borderReg += QRect(0, height-w, width, w); //bottom
		painter.setClipRegion(borderReg);
		emptyArea -= borderReg;

		QStyleOptionFrame frameoptions;
		frameoptions.rect = menubar_rect;
		frameoptions.palette = widget.palette();
		frameoptions.state = QStyle::State_None;
		frameoptions.lineWidth = w;
		frameoptions.midLineWidth = 0;
		qApp->style()->drawPrimitive(QStyle::PE_PanelMenuBar, &frameoptions, &painter, &widget);
	}

	// Style empty menubar area
	QStyleOptionMenuItem options;
	options.rect = menubar_rect;
	options.menuRect = menubar_rect;
	options.palette = widget.palette();
	options.state = QStyle::State_None;
	options.menuItemType = QStyleOptionMenuItem::EmptyArea;
	options.checkType = QStyleOptionMenuItem::NotCheckable;

	QBrush brush;
	brush.setColor(widget.palette().color(QPalette::Button));
	brush.setStyle(Qt::SolidPattern);
	painter.setBackground(brush);

	painter.setClipRegion(emptyArea);

	qApp->style()->drawControl(QStyle::CE_MenuBarEmptyArea, &options, &painter, &widget);
}

void PopupMenu::DrawElement(QPainter& painter, int width, int height, int state)
{
	WindowBackground::DrawElement(painter, width, height, state);

	//int borderwidth = qApp->style()->pixelMetric(QStyle::PM_MenuBarPanelWidth);
	//if (borderwidth == 0) // If the given borderwidth is zero we may draw the border anyway since native Qt applications in that case may solve this in a different way

	QString styleString = QString::fromLatin1(qApp->style()->metaObject()->className()); // Arg! We us this until the popup menues are properly styled
	if (styleString.contains("oxygen", Qt::CaseInsensitive) || styleString.contains("QGtkStyle", Qt::CaseInsensitive)) // TODO: When we no longer support Qt 4.4.x - remove styleString.contains("QGtkStyle", Qt::CaseInsensitive fom this if statement
	{
		painter.save();
		QMenu m;
		QRect rect(0, 0, width, height);
		QColor borderColor = m.palette().background().color().darker(178);
		painter.setPen(borderColor);
		painter.drawRect(rect.adjusted(0, 0, -1, -1));
		painter.restore();
	}
	else
	{
		{
			QStyleOptionMenuItem options;
			SetOptions(options, width, height, state);
			options.state = QStyle::State_None;
			options.checkType = QStyleOptionMenuItem::NotCheckable;
			options.maxIconWidth = 0;
			options.tabWidth = 0;
			qApp->style()->drawPrimitive(QStyle::PE_FrameMenu, &options, &painter); // TODO: When we no longer support Qt 4.4.x - use QStyle::PE_PanelMenu here
		}

		int fw = qApp->style()->pixelMetric(QStyle::PM_MenuPanelWidth);
		if (fw > 0)
		{
			QMenu menu;
			QStyleOptionFrame options;
			SetOptions(options, width, height, state);
			options.palette = menu.palette();
			options.state = QStyle::State_None;
			options.lineWidth = fw;
			options.midLineWidth = 0;
			qApp->style()->drawPrimitive(QStyle::PE_FrameMenu, &options, &painter);
		}
	}
}

void MenuButton::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionMenuItem options;
	SetOptions(options, width, height, state);
	options.menuItemType = QStyleOptionMenuItem::Normal;
	options.checkType = QStyleOptionMenuItem::NotCheckable;

	if (state & NativeSkinElement::STATE_HOVER)
	{
		options.state |= QStyle::State_Selected;
		options.state |= QStyle::State_HasFocus;
	}

	DrawControl<QMenuBar>(QStyle::CE_MenuBarItem, painter, options);
}

void MenuButton::ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state)
{

// List of some popular styles to be faked until we know a more generic way to find the text colors
// Bespin
// QCDEStyle
// QCleanlooksStyle
// QGtkStyle (not yet by use of QGtkStyle due to compile errors for older Qt versions)
// IaOraKde
// IaOraQt
// OxygenStyle
// PolyesterStyle
// QWindowsStyle
// SkulptureStyle
// ... more to be added ?

	QMenuBar widget;
	QPalette palette;
	QColor color;

	palette = widget.palette();
 
	QString styleString = QString::fromLatin1(qApp->style()->metaObject()->className());

	if (styleString.contains("QCleanlooksStyle", Qt::CaseInsensitive) || styleString.contains("QCDEStyle", Qt::CaseInsensitive) || 
		styleString.contains("QWindowsStyle", Qt::CaseInsensitive))
	{
		color = palette.color(QPalette::Active, QPalette::ButtonText);
	}
	else if (styleString.contains("SkulptureStyle", Qt::CaseInsensitive) || styleString.contains("PolyesterStyle", Qt::CaseInsensitive) || 
			 styleString.contains("OxygenStyle", Qt::CaseInsensitive) || styleString.contains("QPlastiqueStyle", Qt::CaseInsensitive))
	{
		color = palette.color(QPalette::Active, QPalette::WindowText); 	
	}
	else
	{
		color = palette.color(QPalette::Active, QPalette::Text);
	}	
	
	
	if (styleString.contains("Bespin", Qt::CaseInsensitive))
	{
		if (state & NativeSkinElement::STATE_HOVER || state & NativeSkinElement::STATE_SELECTED)
		{
			color = palette.color(QPalette::Active, QPalette::Window); 
		}
	}
	else if (styleString.contains("IaOraKde", Qt::CaseInsensitive) || styleString.contains("IaOraQt", Qt::CaseInsensitive))
	{
		if (state & NativeSkinElement::STATE_HOVER || state & NativeSkinElement::STATE_SELECTED)
		{
			color = palette.color(QPalette::Active, QPalette::HighlightedText); 
		}
	}
	else if (styleString.contains("QCleanlooksStyle", Qt::CaseInsensitive) || styleString.contains("QGtkStyle", Qt::CaseInsensitive) || 
			 styleString.contains("PolyesterStyle", Qt::CaseInsensitive))
	{
		if (state & NativeSkinElement::STATE_SELECTED)
		{
			color = palette.color(QPalette::Active, QPalette::HighlightedText); 	
		}
	}
	else if (styleString.contains("SkulptureStyle", Qt::CaseInsensitive))
	{
		if (state & NativeSkinElement::STATE_HOVER || state & NativeSkinElement::STATE_SELECTED)
		{
			color = palette.color(QPalette::Active, QPalette::ButtonText); 	
		}
	}

	alpha = color.alpha();
	red   = color.red();
	green = color.green();
	blue  = color.blue();
}

void MenuSeparator::ChangeDefaultSize(int& width, int& height, int state)
{
	height = 3;
}

void MenuSeparator::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionMenuItem options;
	SetOptions(options, width, height, state);
	options.menuItemType = QStyleOptionMenuItem::Separator;
	DrawControl<QMenu>(QStyle::CE_MenuItem, painter, options);
}

void PopupMenuButton::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionMenuItem options;
	SetOptions(options, width, height, state);
	options.palette = qApp->palette();

	// Manually setting options, because we're using state hover the way state selected is used elsewhere
	options.rect = QRect(0, 0, width, height);

	if (state & NativeSkinElement::STATE_HOVER)
		options.state |= QStyle::State_Selected;

	if (!(state & NativeSkinElement::STATE_DISABLED))
		options.state |= QStyle::State_Enabled;
	else
		options.palette.setCurrentColorGroup(QPalette::Disabled);
	
	if (state & NativeSkinElement::STATE_FOCUSED)
		options.menuItemType = QStyleOptionMenuItem::SubMenu; // arrow 
	else
		options.menuItemType = QStyleOptionMenuItem::Normal;

	if (state & STATE_SELECTED)
	{
		options.checkType = QStyleOptionMenuItem::Exclusive; // bullet
		options.checked = true;
	}
	else if (state & STATE_PRESSED)
	{
		options.checkType = QStyleOptionMenuItem::NonExclusive; // checkmark
		options.checked = true;
	}

	DrawControl<QMenu>(QStyle::CE_MenuItem, painter, options); 
}
void PopupMenuButton::ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state)
{

	// Also here as for MenuButton::ChangeDefaultTextColor we currently need to fake popular widget styles.
	// Only way to avoid this seems to let Qt draw the text ...?

	QMenu widget;
	QPalette palette;
	QColor color;

	palette = widget.palette();
 
	QString styleString = QString::fromLatin1(qApp->style()->metaObject()->className());

	// Default
	if (state & NativeSkinElement::STATE_DISABLED)
		color = palette.color(QPalette::Disabled, QPalette::Text); 
	else
		color = palette.color(QPalette::Active, QPalette::Text); 

	if (styleString.contains("QCDEStyle", Qt::CaseInsensitive))
	{
		if (state & NativeSkinElement::STATE_DISABLED)
			color = palette.color(QPalette::Disabled, QPalette::WindowText);
		else
			color = palette.color(QPalette::Active, QPalette::ButtonText);
	}
	else if (styleString.contains("OxygenStyle", Qt::CaseInsensitive))
	{
		if (state & NativeSkinElement::STATE_DISABLED)
			color = palette.color(QPalette::Disabled, QPalette::WindowText); 
		else
			color = palette.color(QPalette::Active, QPalette::WindowText); 	
	}
	else if (styleString.contains("SkulptureStyle", Qt::CaseInsensitive))
	{
		if (state & NativeSkinElement::STATE_HOVER || state & NativeSkinElement::STATE_SELECTED)
			color = palette.color(QPalette::Active, QPalette::ButtonText);
		else if (state & NativeSkinElement::STATE_DISABLED)
			color = palette.color(QPalette::Disabled, QPalette::WindowText); 
		else
			color = palette.color(QPalette::Active, QPalette::WindowText); 	
	}
	else if (styleString.contains("IaOraKde", Qt::CaseInsensitive) || styleString.contains("IaOraQt", Qt::CaseInsensitive) || 
			 styleString.contains("QGtkStyle", Qt::CaseInsensitive) || styleString.contains("PolyesterStyle", Qt::CaseInsensitive) ||
			 styleString.contains("QCleanlooksStyle", Qt::CaseInsensitive))
	{
		if (state & NativeSkinElement::STATE_HOVER || state & NativeSkinElement::STATE_SELECTED)
			color = palette.color(QPalette::Active, QPalette::HighlightedText);
	}
	else if (styleString.contains("QPlastiqueStyle", Qt::CaseInsensitive))
	{
		if (state & NativeSkinElement::STATE_HOVER || state & NativeSkinElement::STATE_SELECTED)
			color = palette.color(QPalette::Active, QPalette::HighlightedText); 	
		else if (!(state & NativeSkinElement::STATE_DISABLED))
			color = palette.color(QPalette::Active, QPalette::WindowText);
	}
	else if (styleString.contains("QWindowsStyle", Qt::CaseInsensitive))
	{
		if (state & NativeSkinElement::STATE_HOVER || state & NativeSkinElement::STATE_SELECTED)
			color = palette.color(QPalette::Active, QPalette::HighlightedText); 	
		else if (!(state & NativeSkinElement::STATE_DISABLED))
			color = palette.color(QPalette::Active, QPalette::ButtonText); 	
	}

	alpha = color.alpha();
	red   = color.red();
	green = color.green();
	blue  = color.blue();
}

void HeaderButton::DrawElement(QPainter& painter, int width, int height, int state)
{
	DrawControl<QStyleOptionHeader, QTableView>(QStyle::CE_Header, painter, width, height, state);
}


void HeaderBar::DrawElement(QPainter& painter, int width, int height, int state)
{
	DrawControl<QStyleOption, QTableView>(QStyle::CE_HeaderEmptyArea, painter, width, height, state);
}


void EditControl::DrawElement(QPainter& painter, int width, int height, int state)
{

	// Remove STATE_HOVER and STATE_FOCUSED (set by fix for DSK-293790). This causes cosmetic borders
	// to be drawn by the toolkit - and is something we may wish. We remove them here as long as we
	// for the time being need to remove them for MultiLineEditControl (to be consistent).
	state &= ~STATE_HOVER;
	state &= ~STATE_FOCUSED;

	QStyleOptionFrame options;
	SetOptions(options, width, height, state);

	options.state |= QStyle::State_Sunken;

	QString styleString = QString::fromLatin1(qApp->style()->metaObject()->className()); 
	bool is_oxygen_style = styleString.contains("oxygen", Qt::CaseInsensitive);

	if (!is_oxygen_style) // Temporar fix for DSK-DSK-334250 - Don't apply styled frames for oxygen style ...
		options.lineWidth = qApp->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
	else
		options.lineWidth = 0;

	if (is_oxygen_style) // ... but a simple border
	{
		painter.save();
		QLineEdit m;
		QRect rect(0, 0, width, height);
		QColor borderColor = m.palette().background().color().darker(178);
		painter.setPen(borderColor);
		painter.drawRect(rect.adjusted(0, 0, -1, -1));
		// painter.drawRoundedRect(rect.adjusted(0, 0, -1, -1), 5.0, 5.0);
		painter.restore();
	}

	qApp->style()->drawPrimitive(QStyle::PE_PanelLineEdit, &options, &painter);
}

void MultiLineEditControl::DrawElement(QPainter& painter, int width, int height, int state)
{

	// Remove STATE_HOVER and STATE_FOCUSED (set by fix for DSK-293790). This causes cosmetic borders
	// to be drawn by the toolkit - and is something we may wish. Unfortunately this causes lines to be
	// drawn in the edit field when scrolling (DSK-292229).
	state &= ~STATE_HOVER;
	state &= ~STATE_FOCUSED;

	QStyleOptionFrame options;
	SetOptions(options, width, height, state);
	options.state |= QStyle::State_Sunken;

	QString styleString = QString::fromLatin1(qApp->style()->metaObject()->className()); 
	bool is_oxygen_style = styleString.contains("oxygen", Qt::CaseInsensitive);

	if (!is_oxygen_style) // Temporar fix for DSK-292229 - Don't apply styled frames for oxygen style ...
		options.lineWidth = qApp->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
	else
		options.lineWidth = 0;

	qApp->style()->drawPrimitive(QStyle::PE_PanelLineEdit, &options, &painter);

	if (is_oxygen_style) // ... but a simple border
	{
		painter.save();
		QLineEdit m;
		QRect rect(0, 0, width, height);
		QColor borderColor = m.palette().background().color().darker(178);
		painter.setPen(borderColor);
		painter.drawRect(rect.adjusted(0, 0, -1, -1));
		// painter.drawRoundedRect(rect.adjusted(0, 0, -1, -1), 5.0, 5.0);
		painter.restore();
	}
}

void MultiLineEditControl::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	QString styleString = QString::fromLatin1(qApp->style()->metaObject()->className()); // Arg! We us this until the popup menues are properly styled
	if (styleString.contains("oxygen", Qt::CaseInsensitive))  // Temporar fix for DSK-292229 - Don't change padding
	 	return;

	int lineWidth = qApp->style()->pixelMetric(QStyle::PM_MenuPanelWidth); // Check PM_MenuPanelWidth
	left = lineWidth;
	top = lineWidth+1;
	right = lineWidth;
	bottom = lineWidth;
}

void TreeViewControl::DrawElement(QPainter& painter, int width, int height, int state)
{
	// Remove STATE_HOVER and STATE_FOCUSED (set by fix for DSK-293790). This causes cosmetic borders
	// to be drawn by the toolkit - and is something we may wish. Unfortunately this causes lines to be
	// drawn in the edit field when scrolling (DSK-292229).
	state &= ~STATE_HOVER;
	state &= ~STATE_FOCUSED;

	QStyleOptionFrame options;
	SetOptions(options, width, height, state);

	options.state |= QStyle::State_Sunken;
	options.lineWidth = qApp->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

	qApp->style()->drawPrimitive(QStyle::PE_PanelLineEdit, &options, &painter);
}

void TreeViewControl::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	int lineWidth = qApp->style()->pixelMetric(QStyle::PM_MenuPanelWidth); // Check PM_MenuPanelWidth
	left = lineWidth;
	top = lineWidth+1;
	right = lineWidth;
	bottom = lineWidth;
}

void Browser::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionFrame options;
	SetOptions(options, width, height, state);

	options.lineWidth = qApp->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);

	qApp->style()->drawPrimitive(QStyle::PE_Frame, &options, &painter);
}


void ListItem::DrawElement(QPainter& painter, int width, int height, int state)
{
	//QStyleOptionViewItem options;
	//NativeStateToStyleState(state, &options);
	//painter.drawControl(QStyle::CE_ItemViewItem, &options);
	if (state & NativeSkinElement::STATE_SELECTED) {
		QPalette p;
		painter.fillRect(QRect(0, 0, width, height), p.highlight());
	}
	else {
		painter.setCompositionMode(QPainter::CompositionMode_Clear);
		painter.eraseRect(QRect(0, 0, width, height));
	}
}


void TabButton::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionTabV2 options; // TODO: When we no longer support Qt 4.4.x - use QStyleOptionTabV3
	SetOptions(options, width, height, state);

	if ((state & STATE_TAB_FIRST) && (state & STATE_TAB_LAST))
	{
		options.position = QStyleOptionTab::OnlyOneTab;
	}
	else if (state & STATE_TAB_FIRST)
	{
		options.position = QStyleOptionTab::Beginning;
	}
	else if (state & STATE_TAB_LAST)
	{
		options.position = QStyleOptionTab::End;
		options.rect.setWidth(options.rect.width() - 1);
	}
	else
	{
		options.position = QStyleOptionTab::Middle;
	}

	if (state & STATE_TAB_PREV_SELECTED)
	{
		options.selectedPosition = QStyleOptionTab::PreviousIsSelected;
	}
	else if (state & STATE_TAB_NEXT_SELECTED)
	{
		options.selectedPosition = QStyleOptionTab::NextIsSelected;
	}
	else
	{
		options.selectedPosition = QStyleOptionTab::NotAdjacent;
	}

	QTabWidget dummytabwidget; // Do not revert the order of these two
	QTabBar widget;

	dummytabwidget.setGeometry(QRect(-100, -100, 0, 0));
	widget.setParent(&dummytabwidget); // the parent widget need to be castable to QTabWidget

	if (state & STATE_SELECTED)
	{
		painter.fillRect(QRect(2, height-8, width-5, 12), widget.palette().window());
	}
	qApp->style()->drawControl(QStyle::CE_TabBarTab, &options, &painter, &widget);
}


void TabButton::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	left   = 10;
	top    = TOP_PADDING;
	right  = 10;
	bottom = BOTTOM_PADDING;

	QString styleString = QString::fromLatin1(qApp->style()->metaObject()->className());
	m_opaque = styleString.contains("QPlastiqueStyle", Qt::CaseInsensitive);
	bool is_gtk = styleString.contains("gtk", Qt::CaseInsensitive);
	bool is_oxygen = styleString.contains("oxygen", Qt::CaseInsensitive);

	// Adjust text position within tab for some styles
	if (is_gtk)
	{
		top += 2;
		bottom += 2;
	}
	else if (is_oxygen)
	{
		top += 1;
		bottom += 3;
	}

	// This ensures that text in an active tab is shifted
	if (state & STATE_SELECTED)
	{
		if (is_gtk)
		{
			// Due to the modification in ChangeDefaultMargin()
			top -= 2;
			bottom += 2;
		}
		else
		{
			top -= 1;
			bottom += 1;
		}
	}
}


void TabButton::ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state)
{
	// Align to left edge
	left = -2;
	right = 2;

	QString styleString = QString::fromLatin1(qApp->style()->metaObject()->className());
	if (styleString.contains("gtk", Qt::CaseInsensitive))
		bottom = bottom - 2;
}


void TabBar::DrawElement(QPainter& painter, int width, int height, int state)
{
	QWidget widget;
	painter.fillRect(0,0,width,height, widget.palette().window());

	QStyleOptionTabWidgetFrame options; // TODO: When we no longer support Qt 4.4.x - use QStyleOptionTabWidgetFrameV2
	SetOptions(options, width, height, state);
	options.lineWidth = qApp->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
	//options.tabBarSize = QSize(width-??, 20); // We want this info

	QString styleString = QString::fromLatin1(qApp->style()->metaObject()->className());
	if (styleString.contains("cde", Qt::CaseInsensitive))
		options.rect = QRect(0, height-3, width, height);
	else if (styleString.contains("motif", Qt::CaseInsensitive))
		options.rect = QRect(0, height-4, width, height);
	else if (styleString.contains("windows", Qt::CaseInsensitive))
		options.rect = QRect(0, height-4, width, height);
	else if (styleString.contains("cleanlooks", Qt::CaseInsensitive))
		options.rect = QRect(0, height-4, width, height);
	else if (styleString.contains("gtk", Qt::CaseInsensitive))
		options.rect = QRect(-1, height-1, width+2, height);
	else if (styleString.contains("plastique", Qt::CaseInsensitive))
		options.rect = QRect(0, height-4, width, height);
	else if (styleString.contains("oxygen", Qt::CaseInsensitive))
		options.rect = QRect(0, height-9, width, height);
	else if (styleString.contains("IaOraKde", Qt::CaseInsensitive) || styleString.contains("IaOraQt", Qt::CaseInsensitive) )
		options.rect = QRect(0, height-3, width, height);
	else if (styleString.contains("Bespin", Qt::CaseInsensitive) )
		options.rect = QRect(0, height-2, width, height);
	else
	{
		options.rect = QRect(0, height-4, width, height);
	}

	QSize t(3, 20); // Some widget styles require this 
	options.tabBarSize = t;
	
	if (styleString.contains("SkulptureStyle", Qt::CaseInsensitive) || styleString.contains("gtk", Qt::CaseInsensitive) )
	{
		options.shape =  QTabBar::RoundedNorth;
	}
	else
	{
		options.shape =  QTabBar::RoundedSouth;	
	}

	qApp->style()->drawPrimitive(QStyle::PE_FrameTabWidget, &options, &painter);
}

void TabBar::ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state) 
{
}


void TabPage::DrawElement(QPainter& painter, int width, int height, int state)
{
	QWidget widget;
	painter.fillRect(0,0,width,height, widget.palette().window());

	QStyleOptionTabWidgetFrame options;  // TODO: When we no longer support Qt 4.4.x - use QStyleOptionTabWidgetFrameV2
	SetOptions(options, width, height, state);
	options.lineWidth = qApp->style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
	//options.tabBarSize = QSize(width-??, 20); // We want this info
	options.rect = QRect(0, -8, width, height+8);

	QSize t(1, 1); // Some widget styles require this  
	options.tabBarSize = t;

	qApp->style()->drawPrimitive(QStyle::PE_FrameTabWidget, &options, &painter);
}


void TabPage::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	left = right = top = bottom = 10;
}

void DropDown::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionComboBox options;
	SetOptions(options, width, height, state);

	QComboBox widget;
	qApp->style()->drawComplexControl(QStyle::CC_ComboBox, &options, &painter, &widget);
}

void DropDown::ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state)
{
	if (state & STATE_RTL)
	{
		left = 6;
		right = 3;
	}
	else
	{
		left = 3;
		right = 6;
	}
}

void Tooltip::DrawElement(QPainter& painter, int width, int height, int state)
{
	QStyleOptionFrame options;
	SetOptions(options, width, height, state);
	options.lineWidth = qApp->style()->pixelMetric(QStyle::PM_ToolTipLabelFrameWidth);
	qApp->style()->drawPrimitive(QStyle::PE_PanelTipLabel, &options, &painter);
}
