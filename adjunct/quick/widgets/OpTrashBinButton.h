 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- 
  *
  * Copyright (C) 2000-2011 Opera Software AS. All rights reserved.
  *
  * This file is part of the Opera web browser. It may not be distributed
  * under any circumstances.
  */

/** @file OpTrashBinButton.h
  *
  * Contains the declaration of the class OpTrashBinButton
  *
  * @author Deepak Arora
  *
  */

#ifndef OP_TRASH_BIN_BUTTON_H
#define OP_TRASH_BIN_BUTTON_H

#include "modules/widgets/OpButton.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"

class BrowserDesktopWindow;

class OpTrashBinButton
	: public OpButton
	, public DesktopWindowCollection::Listener
{
public:
	static OP_STATUS Construct(OpTrashBinButton** obj);
	virtual ~OpTrashBinButton() { Cleanup(); }
	
	//DesktopWindowCollection::Listener
	virtual void OnDesktopWindowAdded(DesktopWindow* window) { }
	virtual void OnDocumentWindowUrlAltered(DocumentDesktopWindow* document_window, const OpStringC& url) { } 
	virtual void OnDesktopWindowRemoved(DesktopWindow* window);
	virtual void OnCollectionItemMoved(DesktopWindowCollectionItem* item, DesktopWindowCollectionItem* old_parent, DesktopWindowCollectionItem* old_previous) {}

	//OpWidget
	virtual void OnRemoving() { Cleanup(); }

protected:
	OpTrashBinButton() : m_bdw(NULL) { }
	
	//OpTrashBinButton->OpButton->OpWidget->MessageObject
	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	//OpButton
	virtual BOOL	IsOfType(Type type) { return type == WIDGET_TYPE_TRASH_BIN_BUTTON || OpButton::IsOfType(type); }
	virtual	Type	GetType() { return WIDGET_TYPE_TRASH_BIN_BUTTON; }
	virtual void	GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows);

private:
	OP_STATUS		Init();
	void			Cleanup();

private:
	BrowserDesktopWindow *m_bdw;
	static OpPointerHashTable <DesktopWindow , OpVector<OpTrashBinButton> > m_trash_bin_collection;
};

#endif // OP_TRASH_BIN_BUTTON_H
