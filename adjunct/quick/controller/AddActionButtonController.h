/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef ADDACTIONBUTTONCONTROLLER_H
#define ADDACTIONBUTTONCONTROLLER_H

#include "adjunct/quick/controller/SimpleDialogController.h"

class AddActionButtonController : public SimpleDialogController
{
	public:
		AddActionButtonController(OpToolbar * toolbar, BOOL is_button, OpInputAction * input_action, int pos, BOOL force);
		OP_STATUS SetData(const uni_char* action_url, const uni_char* action_title);

	private:
		void				InitL();
		virtual void		OnOk();

		OpString			m_actionurl;
		OpString			m_actiontitle;
		INT32				m_pos;
		BOOL				m_force;
		OpToolbar * 		m_toolbar;
		OpInputAction * 	m_input_action;
		BOOL				m_is_button;
};

#endif // ADDACTIONBUTTONCONTROLLER_H
