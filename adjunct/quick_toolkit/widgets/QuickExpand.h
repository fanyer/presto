/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef QUICK_EXPAND_H
#define QUICK_EXPAND_H

#include "adjunct/quick_toolkit/widgets/DesktopToggleButton.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContent.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetDecls.h"


class QuickExpand
		: public QuickWidget
		, public QuickWidgetContainer
		, public DesktopToggleButton::Listener
{
	IMPLEMENT_TYPEDOBJECT(QuickWidget);

public:
	QuickExpand();
	virtual ~QuickExpand();

	OP_STATUS Init();

	OP_STATUS SetText(const OpStringC& expand_text, const OpStringC& collapse_text);

	void SetContent(QuickWidget* content);
	void SetName(const OpStringC8& name);

	void Expand() { if (!IsExpanded()) m_expand_button.GetOpWidget()->Toggle(); }
	void Collapse() { if (IsExpanded()) m_expand_button.GetOpWidget()->Toggle(); }

	///// from DesktopToggleButton::Listener
	virtual void      OnToggle() { UpdateContent(); }

	///// from QuickWidget
	virtual BOOL      HeightDependsOnWidth() { return IsExpanded() && m_content->HeightDependsOnWidth(); }
	virtual OP_STATUS Layout(const OpRect& rect);
	virtual void      SetParentOpWidget(class OpWidget* parent);
	virtual void      Show();
	virtual void      Hide();
	virtual BOOL      IsVisible() { return m_is_visible; }
	virtual void      SetEnabled(BOOL enabled);

	///// from QuickWidgetContainer
	virtual void OnContentsChanged() { BroadcastContentsChanged(); }
	bool IsExpanded() { return m_expand_button.GetOpWidget()->GetValue() != 0; }
protected:
	///// from QuickWidget
	virtual unsigned GetDefaultMinimumWidth();
	virtual unsigned GetDefaultMinimumHeight(unsigned width);
	virtual unsigned GetDefaultNominalWidth();
	virtual unsigned GetDefaultNominalHeight(unsigned width);
	virtual unsigned GetDefaultPreferredWidth();
	virtual unsigned GetDefaultPreferredHeight(unsigned width);
	virtual void     GetDefaultMargins(WidgetSizes::Margins& margins);

private:
	int GetVerticalSpacing();
	void UpdateContent();

	QuickToggleButton  m_expand_button;
	QuickWidgetContent m_content;

	BOOL m_is_visible;
};

#endif // QUICK_EXPAND_H
