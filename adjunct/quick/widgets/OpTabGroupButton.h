/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * Author: Adam Minchinton
 */

#ifndef OP_TABGROUPBUTTON_H
#define OP_TABGROUPBUTTON_H

#include "adjunct/quick/widgets/OpPagebarItem.h"
#include "adjunct/quick/models/DesktopGroupModelItem.h"
#include "adjunct/quick/widgets/OpIndicatorButton.h"

////////////////////////////////////////////////////////////////////////////////////////

class OpTabGroupButton : public OpPagebarItem, public DesktopGroupListener
{
protected:
	OpTabGroupButton(DesktopGroupModelItem& group);

public:
	~OpTabGroupButton();

	static OP_STATUS	Construct(OpTabGroupButton** obj, DesktopGroupModelItem& group);

	virtual BOOL		IsOfType(Type type) { return type == WIDGET_TYPE_TAB_GROUP_BUTTON || OpPagebarItem::IsOfType(type); }
	virtual void		GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	virtual void		GetToolTipText(OpToolTip* tooltip, OpInfoText& text);

	virtual void		OnLayout();
	virtual void		OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks);

	virtual void		Click(BOOL plus_action = FALSE);
	void				SetSkin();

	virtual INT32 		GetDropArea(INT32 x, INT32 y) { return y <= 0 ? DROP_RIGHT : DROP_BOTTOM; }

	// From DesktopGroupListener
	virtual void		OnCollapsedChanged(DesktopGroupModelItem* group, bool collapsed);
	virtual void		OnActiveDesktopWindowChanged(DesktopGroupModelItem* group, DesktopWindow* active) { RefreshIndicatorState(); }
	virtual void		OnGroupDestroyed(DesktopGroupModelItem* group) { m_group = NULL; }

	void				AddIndicator(OpIndicatorButton::IndicatorType type);
	void				RemoveIndicator(OpIndicatorButton::IndicatorType type);
	void				RefreshIndicatorState();

private:
	void CalculateAndSetMinWidth();
	void UpdateButtonsSkinType();

	DesktopGroupModelItem* m_group;
	OpButton* m_expander_button;
	OpIndicatorButton* m_indicator_button;
};

#endif // OP_TABGROUPBUTTON_H
