/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OP_HELP_TOOLTIP_H
#define OP_HELP_TOOLTIP_H

#include "modules/widgets/OpWidget.h"
#include "modules/inputmanager/inputaction.h"

class OpButton;
class OpLabel;

class OpHelpTooltip : public OpWidget
{
protected:
	OpHelpTooltip();
	~OpHelpTooltip();

public:
	static OP_STATUS	Construct(OpHelpTooltip** obj);

	OP_STATUS			Init();

	OP_STATUS			SetText(const uni_char* text);
	OP_STATUS			GetText(OpString& text);		

	/**
	 * Associates the "More" button with a help topic.  The help page will be
	 * loaded using the Opera help system (ACTION_SHOW_HELP).
	 *
	 * @param topic the topic name
	 * @param force_new_window whether the help page must open in a new window
	 * @return status
	 */
	OP_STATUS			SetHelpTopic(const OpStringC& topic, BOOL force_new_window = TRUE);

	/**
	 * Associates the "More" button with a help URL.  The help page will be
	 * loaded in a new window like a regular URL
	 * (ACTION_OPEN_URL_IN_NEW_WINDOW).
	 *
	 * @param url_name the URL name
	 * @return status
	 */
	OP_STATUS			SetHelpUrl(const OpStringC& url_name);

	/*
	 * Make sure to SetRect() or at least Relayout() after calling this function.
	 */
	INT32				GetHeightForWidth(INT32 width);

	virtual void		GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);
	virtual void		OnLayout();
	virtual void		OnResize(INT32* new_w, INT32* new_h) {ResetRequiredSize();}
	virtual BOOL		OnInputAction(OpInputAction* action);
	virtual void		OnFocus(BOOL focus, FOCUS_REASON reason);
	virtual const char*	GetInputContextName() {return "Help tooltip widget";}
	virtual void		OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

private:
	OP_STATUS			SetHelpAction(OpInputAction* action);

	OpButton*			m_close_button;
	OpButton*			m_more_button;
	OpLabel*			m_label;
};

#endif //OP_HELP_TOOLTIP_H
