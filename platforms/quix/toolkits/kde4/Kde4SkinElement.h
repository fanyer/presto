/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Rune Myrland (runem)
 */


#ifndef KDE4_SKIN_ELEMENT_H
#define KDE4_SKIN_ELEMENT_H


#include <KDE/KApplication>
#include <QStyleOptionSlider>
#include <QtGui/QPushButton>
#include <QtGui/QMainWindow>
#include <QtGui/QWidget>
#include <QtGui/QTabBar>
#include <QtGui/QWidgetAction>
#include <QtGui/QMenu>
#include <QtGui/QMenuBar>
#include <QtGui/QLabel>
#include <QImage>
#include <QDebug>
#include "platforms/quix/toolkits/NativeSkinElement.h"

namespace Kde4SkinElements
{
class Kde4SkinElement : public NativeSkinElement
{
public:
	Kde4SkinElement();
	virtual ~Kde4SkinElement() {}

	// From NativeSkinElement
	virtual void Draw(uint32_t* bitmap, int width, int height, const NativeRect& clip_rect, int state);

	virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state) {}
	virtual void ChangeDefaultSize(int& width, int& height, int state) {}
	virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state) {}
	virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state);
	virtual void ChangeDefaultSpacing(int &spacing, int state) {}

protected:
	virtual QSizePolicy::ControlType ControlType() { return QSizePolicy::DefaultType; }
	virtual void DrawElement(QPainter& painter, int width, int height, int state) = 0;
	virtual bool IsOpaque() { return false; }
	template<class OptionClass, class WidgetClass>
		void DrawControl(QStyle::ControlElement control, QPainter& painter, int width, int height, int state);
	template<class WidgetClass>
		void DrawControl(QStyle::ControlElement control, QPainter& painter, QStyleOption& options);
	void SetOptions(QStyleOption& options, int width, int height, int state);

};

#ifdef DEBUG
/** Dummy widget that paints its area red.
 * Used as a placeholder for unsupported widgets in the UI.
 */
class UnsupportedWidget : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
};
#endif // DEBUG


class WindowBackground : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual bool IsOpaque() { return true; }
};

class PushButton : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
	virtual QSizePolicy::ControlType ControlType() { return QSizePolicy::PushButton; }
};


class CheckBox : public Kde4SkinElement {
public:
	virtual void ChangeDefaultSize(int& width, int& height, int state);
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual QSizePolicy::ControlType ControlType() { return QSizePolicy::CheckBox; }
};


class RadioButton : public Kde4SkinElement {
public:
	virtual void ChangeDefaultSize(int& width, int& height, int state);
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual QSizePolicy::ControlType ControlType() { return QSizePolicy::RadioButton; }
};


class DefaultPushButton : public PushButton {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
};


class MenuBar : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual bool IsOpaque() { return true; }
};

class PopupMenu : public WindowBackground {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
};

class MenuButton : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state);
};


class MenuArrow : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state) {}
};


class PopupMenuButton : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state);
};

class MenuSeparator : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual void ChangeDefaultSize(int& width, int& height, int state);
};

class HeaderButton : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
};

class MainbarButton : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual void ChangeDefaultTextColor(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t& alpha, int state);
	virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
	virtual void ChangeDefaultSpacing(int &spacing, int state);
};

class PersonalbarButton : public MainbarButton {
};

class HeaderBar : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
};


class EditControl : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual QSizePolicy::ControlType ControlType() { return QSizePolicy::LineEdit; }
};

class MultiLineEditControl : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
	virtual QSizePolicy::ControlType ControlType() { return QSizePolicy::LineEdit; }
};

class TreeViewControl : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
};

class Browser: public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
};


class ListItem : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state) {}
};


class TabButton : public Kde4SkinElement {
public:
	TabButton() : Kde4SkinElement(), m_opaque(false) {}

	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual bool IgnoreCache() { return true; }
	virtual bool IsOpaque() { return m_opaque; }
	virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
	virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state);

	static const int TOP_PADDING = 5;
	static const int BOTTOM_PADDING = 3;
private:
	bool m_opaque;
};


class TabBar : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual bool IsOpaque() { return true; }
	virtual void ChangeDefaultMargin(int& left, int& top, int& right, int& bottom, int state);
};


class TabPage : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
};


class DropDown : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
	virtual void ChangeDefaultPadding(int& left, int& top, int& right, int& bottom, int state);
	virtual QSizePolicy::ControlType ControlType() { return QSizePolicy::ComboBox; }
};


class DropDownButton : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state) {}
};


class Tooltip : public Kde4SkinElement {
public:
	virtual void DrawElement(QPainter& painter, int width, int height, int state);
};


};

#endif
