/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_GOTOINTRANETBAR_H
#define OP_GOTOINTRANETBAR_H

#include "adjunct/quick/widgets/OpInfobar.h"

class WandInfo;
class OpRichTextLabel;

/***********************************************************************************
**
**	OpGoToIntranetBar
**  Shows button/link to open a webpage.
**
***********************************************************************************/

class OpGoToIntranetBar : public OpInfobar
{
protected:
	OpGoToIntranetBar();
	~OpGoToIntranetBar();

public:
	static OP_STATUS	Construct(OpGoToIntranetBar** obj);

	void Show(const uni_char *address, BOOL save_typed_history);
	void Hide(BOOL focus_page = TRUE);
	void OnUrlChanged();

	// == Hooks ======================

	virtual void		OnAlignmentChanged();
	virtual void		OnReadContent(PrefsSection *section);

	// == OpInputContext ======================

	virtual BOOL			OnInputAction(OpInputAction* action);
	virtual const char*		GetInputContextName() {return "Go To Intranet Bar";}
private:
	OpRichTextLabel* m_label;
	OpString m_address;
	double m_show_time;
	BOOL m_hide_when_url_change;
	BOOL m_save_typed_history;
};

#endif // OP_GOTOINTRANETBAR_H
