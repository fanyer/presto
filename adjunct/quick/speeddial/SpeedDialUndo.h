// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Huib Kleinhout
//

#ifndef __SPEEDDIAL_UNDO_H__
#define __SPEEDDIAL_UNDO_H__

#ifdef SUPPORT_SPEED_DIAL

#include "GenericUndo.h"
#include "adjunct/quick_toolkit/windows/DesktopWindowListener.h"

class SpeedDialData;


class SpeedDialUndo : public GenericUndoStack, public DesktopWindowListener
{
public:	
	SpeedDialUndo() : m_last_desktopwindow_with_undo(NULL), m_locked(false) {}
	~SpeedDialUndo();

	OP_STATUS AddInsert(INT32 pos);
	OP_STATUS AddRemove(INT32 pos);
	OP_STATUS AddReplace(INT32 pos);
	OP_STATUS AddMove(INT32 from_pos, INT32 to_pos);

	virtual OP_STATUS Undo();
	virtual OP_STATUS Redo();

	// Implementing DesktopWindowListener
	virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated);

private:
	class MoveItem : public GenericUndoItem
	{
	public:
		MoveItem(INT32 from_pos, INT32 to_pos);

		virtual OP_STATUS Undo();
		virtual OP_STATUS Redo();

		virtual size_t BytesUsed() const { return sizeof(*this); }

	private:
		INT32 m_from_pos;
		INT32 m_to_pos;
	};

	class ModifyItem : public GenericUndoItem
	{
	public:
		virtual size_t BytesUsed() const;

	protected:
		ModifyItem() : m_pos(-1), m_data(NULL) {}
		~ModifyItem();

		INT32 m_pos;
		SpeedDialData* m_data;
	};

	class InsertRemoveItem : public ModifyItem
	{
	public:
		static OP_STATUS CreateInsert(INT32 pos, InsertRemoveItem*& item);
		static OP_STATUS CreateRemove(INT32 pos, InsertRemoveItem*& item);

		virtual OP_STATUS Undo() { return m_data == NULL ? Reremove() : Reinsert(); }
		virtual OP_STATUS Redo() { return m_data != NULL ? Reinsert() : Reremove(); }

	private:
		InsertRemoveItem() {}

		OP_STATUS Reinsert();
		OP_STATUS Reremove();
	};

	class ReplaceItem : public ModifyItem
	{
	public:
		static OP_STATUS Create(INT32 pos, ReplaceItem*& item);

		virtual OP_STATUS Undo() { return Replace(); }
		virtual OP_STATUS Redo() { return Replace(); }

	private:
		ReplaceItem() {}

		OP_STATUS Replace();
	};


	void Add(GenericUndoItem* item);

	/*	Set the last window in which a speed dial changed happened, the undo stack will be cleaned
		when this window closes.
	    @param desktopwindow	Last window in which a change was made that was recorded in the stack
								When desktopwindow=NULL, the current desktopwindow will be cleared
	*/
	void SetLastUndoDesktopWindow(DesktopWindow* desktopwindow);

	DesktopWindow* m_last_desktopwindow_with_undo;		// The last desktop window in which the user changed speeddial
														// We are waiting for this to be closed, so the undo stack can be emptied.

	bool m_locked;
};

#endif // SUPPORT_SPEED_DIAL
#endif // __SPEEDDIAL_UNDO_H__
