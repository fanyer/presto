/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef KDE4_UI_SETTINGS_H
#define KDE4_UI_SETTINGS_H

#include "platforms/quix/toolkits/ToolkitUiSettings.h"

#include <QColor>
#include <QWidget>
#include <QEvent>

class KApplication;

class Kde4UiSettings : public ToolkitUiSettings
{
public:
	Kde4UiSettings(KApplication* application) : m_application(application) {}

	virtual bool DefaultButtonOnRight() { return false; }

	virtual uint32_t GetSystemColor(SystemColor color);

	virtual bool GetDefaultFont(FontDetails& font);

	virtual void GetFontRenderSettings(FontRenderSettings& settings) {}

private:
	uint32_t MakeColor(const QColor& color) { return (color.alpha() << 24) | (color.red() << 16) | (color.green() << 8) | color.blue(); }

	KApplication* m_application;
};


class DummyWidget: public QWidget
{
public:
	DummyWidget()
		: m_hasStyleChanged(false)
		, m_hasPaletteChanged(false)
		, m_hasFontChanged(false)
	{ }

	bool eventFilter(QObject *o, QEvent *e)
	{
		if (e->type() == QEvent::ApplicationPaletteChange)
		{
			m_hasPaletteChanged =  true;
		}
		if (e->type() == QEvent::StyleChange)
		{
			m_hasStyleChanged = true;
		}
		if (e->type() == QEvent::ApplicationFontChange)
		{
			m_hasFontChanged = true;
		}
		return false;  
	}
	bool HasStyleChanged() {return m_hasStyleChanged;}
	bool HasPaletteChanged() {return m_hasPaletteChanged;}
	bool HasFontChanged() {return m_hasFontChanged;}
	void ResetState()
	{
		m_hasStyleChanged   = false;
		m_hasPaletteChanged = false;
		m_hasFontChanged    = false;
	}
private:
	bool m_hasStyleChanged;
	bool m_hasPaletteChanged;
	bool m_hasFontChanged;
};

#endif // KDE4_UI_SETTINGS_H
