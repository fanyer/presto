/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef CHANGENICK_H
#define CHANGENICK_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"

/***********************************************************************************
**
**	ChangeNickDialog
**
***********************************************************************************/

class ChangeNickDialog : public Dialog
{
	public:
		enum ChangeType { CHANGE_NORMAL, CHANGE_NICK_IN_USE };


								ChangeNickDialog(ChangeType change_type, UINT16 account_id, const OpStringC& old_nick = OpString());
		virtual					~ChangeNickDialog() {};

		Type					GetType()				{return DIALOG_TYPE_CHANGE_NICK;}
		const char*				GetWindowName()			{return "Change Nick Dialog";}

		virtual void			OnInit();
		virtual UINT32			OnOk();
		virtual void			OnCancel();

	private:

		ChangeType	m_change_type;
		OpString	m_old_nick;
		UINT16		m_account_id;
};


/***********************************************************************************
**
**	RoomPasswordDialog
**
***********************************************************************************/

class RoomPasswordDialog : public Dialog
{
	public:

								RoomPasswordDialog(UINT16 account_id, const OpStringC& room);
		virtual					~RoomPasswordDialog() {};

		Type					GetType()				{return DIALOG_TYPE_ROOM_PASSWORD;}
		const char*				GetWindowName()			{return "Room Password Dialog";}

		virtual BOOL			GetModality() { return FALSE;}
		BOOL					HideWhenDesktopWindowInActive()	{ return TRUE; }
		virtual void			OnInit();
		virtual UINT32			OnOk();
		virtual void			OnCancel();

	private:

		OpString	m_room;
		UINT16		m_account_id;
};

#endif //CHANGENICK_H
