/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
* @author Owner: Karianne Ekern (karie), Deepak Arora (deepaka)
*
*/

#ifndef BOOKMARK_PROPERTIES_CONTROLLER_H
#define BOOKMARK_PROPERTIES_CONTROLLER_H

#include "adjunct/quick_toolkit/contexts/OkCancelDialogContext.h"
#include "adjunct/desktop_util/adt/opproperty.h"

class HotlistModelItem;
class OpDropDown;
class OpBookmarkView;

class BookmarkPropertiesController
	: public OkCancelDialogContext
{
public:
	BookmarkPropertiesController(HotlistModelItem* item);

private:
	OP_STATUS				InitDropDown();
	OP_STATUS				PopulateDropDown(OpDropDown* dropdown);
	BOOL					ValidateNickName(const OpStringC& nick) const;
	BOOL					ParentChanged() const;
	void					OnTextChanged(const OpStringC& text);
	inline static bool		IsNonTrashFolder(HotlistModelItem* folder);

	// OkCancelDialogContext
	virtual void InitL();
	virtual void OnOk();
	virtual BOOL DisablesAction(OpInputAction* action);

	HotlistModelItem*		m_bookmark;
	OpBookmarkView*			m_bookmark_view;
	OpProperty<bool>		m_in_panel;
	OpProperty<bool>		m_on_bookmarkbar;
	OpProperty<int>			m_parent_id;
	OpProperty<OpString>	m_name;
	OpProperty<OpString>	m_url;
	OpProperty<OpString>	m_nick;
	OpProperty<OpString>	m_desc;
	bool					m_is_valid_nickname;
};

#endif //BOOKMARK_PROPERTIES_CONTROLLER_H
