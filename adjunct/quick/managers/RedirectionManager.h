// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// shuais
//

#ifndef __REDIRECTION_MANAGER_H__
#define __REDIRECTION_MANAGER_H__

#include "adjunct/quick/managers/DesktopManager.h"
#include "adjunct/quick/speeddial/SpeedDialListener.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/hotlist/HotlistModel.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class RedirectUrl : public MessageObject
{
public:
	RedirectUrl(OpStringC unique_id);
	virtual ~RedirectUrl();

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	void ResolveUrl(OpStringC url);

	OpString id;
	OpString moved_to_url;
	URL url;

private:
	void OnComplete();
	void OnResolved();
};

class RedirectionManager :	public DesktopManager<RedirectionManager> ,
							public SpeedDialListener,
							public DesktopBookmarkListener,
							public HotlistModelListener
{
public:

	RedirectionManager();
	~RedirectionManager();
	virtual OP_STATUS Init();

	// Get the real url for a partner bookmark/speed dial
	// @param unique_id id of the bookmark or speed dial
	// @param url the redirection url that needs to be resolved
	void ResolveRedirectUrl(OpStringC unique_id, OpStringC url);

	void OnResolved(RedirectUrl*);

	BOOL IsRedirectUrl(OpStringC url);

	// SpeedDialListener
	virtual void OnSpeedDialAdded(const DesktopSpeedDial& entry);
	virtual void OnSpeedDialRemoving(const DesktopSpeedDial& entry){}
	virtual void OnSpeedDialReplaced(const DesktopSpeedDial& old_entry, const DesktopSpeedDial& new_entry){}
	virtual void OnSpeedDialMoved(const DesktopSpeedDial& from_entry, const DesktopSpeedDial& to_entry){}
	virtual void OnSpeedDialPartnerIDAdded(const uni_char* partner_id){}
	virtual void OnSpeedDialPartnerIDDeleted(const uni_char* partner_id){}
	virtual void OnSpeedDialsLoaded();

	// DesktopBookmarkListener
	virtual void OnBookmarkModelLoaded();

	// HotlistModelListener
	virtual void OnHotlistItemAdded(HotlistModelItem* item);
	virtual void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN){}
	virtual void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child){}
	virtual void OnHotlistItemTrashed(HotlistModelItem* item){}
	virtual void OnHotlistItemUnTrashed(HotlistModelItem* item){}
	virtual void OnHotlistItemMovedFrom(HotlistModelItem* item){}

private:

	OpAutoVector<RedirectUrl> m_urls;
};

#endif // __REDIRECTION_MANAGER_H__
