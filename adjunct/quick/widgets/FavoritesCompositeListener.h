/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef FAVORITES_COMPOSITE_LISTENER_H
#define FAVORITES_COMPOSITE_LISTENER_H

#include "modules/hardcore/mh/messobj.h"

#include "adjunct/quick/hotlist/HotlistModel.h"
#include "adjunct/quick/speeddial/SpeedDialListener.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"

class OpAddressDropDown;

class FavoritesCompositeListener
	: private MessageObject
	, public DesktopBookmarkListener
	, public HotlistModelListener
	, public SpeedDialListener
{
public:
	FavoritesCompositeListener(OpAddressDropDown *addr_dd);
	~FavoritesCompositeListener();

	// MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// DesktopBookmarkListener
	virtual void OnBookmarkModelLoaded();

	// HotlistModelListener
	virtual void OnHotlistItemAdded(HotlistModelItem* item);
	virtual void OnHotlistItemChanged(HotlistModelItem* item, BOOL moved_as_child, UINT32 changed_flag = HotlistModel::FLAG_UNKNOWN);
	virtual void OnHotlistItemRemoved(HotlistModelItem* item, BOOL removed_as_child);
	virtual void OnHotlistItemTrashed(HotlistModelItem* item);
	virtual void OnHotlistItemUnTrashed(HotlistModelItem* item) { }
	virtual void OnHotlistItemMovedFrom(HotlistModelItem* item) { }

	// SpeedDialListener
	void OnSpeedDialAdded(const DesktopSpeedDial& entry);
	void OnSpeedDialRemoving(const DesktopSpeedDial& entry);
	void OnSpeedDialReplaced(const DesktopSpeedDial& old_entry, const DesktopSpeedDial& new_entry) { }
	void OnSpeedDialMoved(const DesktopSpeedDial& from_entry, const DesktopSpeedDial& to_entry) { }
	void OnSpeedDialPartnerIDAdded(const uni_char* partner_id) { }
	void OnSpeedDialPartnerIDDeleted(const uni_char* partner_id) { }
	void OnSpeedDialsLoaded() { }

private:
	void UpdateFavoritesWidget();

	OpAddressDropDown *m_dropdown;
};

#endif //FAVORITES_COMPOSITE_LISTENER_H
