/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 2008-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef BOOKMARK_OPERATION_H
#define BOOKMARK_OPERATION_H

class BookmarkItem;
class BookmarkAttribute;
class BookmarkOperationListener;

/**
 * A bookmark operation, currently support for add, move and
 * delete. For add the first operand is the bookmark to add and the
 * second the parent folder. For a move operation the first operand is
 * the bookmark to move and the second operand is the folder to move
 * to. For a delete operation the first operand is the bookmark to
 * delete and the second operand is unused. If FEATURE_SYNC is enabled
 * there is also a sync flag which control if this operation should be
 * synced or not.
 */
class BookmarkOperation : public Link
{
public:
	enum BookmarkOperationType
	{
		OPERATION_ADD,
		OPERATION_MOVE,
		OPERATION_DELETE,
		OPERATION_ATTR_CHANGE,
		OPERATION_SWAP,
		OPERATION_MOVE_AFTER,
		OPERATION_NONE
	};

	BookmarkOperation() :
		first_operand(NULL),
		second_operand(NULL),
		attr_operand(NULL),
		operation_type(OPERATION_NONE),
#ifdef SUPPORT_DATA_SYNC
		should_sync(FALSE),
		from_sync(FALSE),
#endif // SUPPORT_DATA_SYNC
		m_listener(NULL) { }

	void SetFirstOperand(BookmarkItem *operand) { first_operand = operand; }
	void SetSecondOperand(BookmarkItem *operand) { second_operand = operand; }
	void SetAttrOperand(BookmarkAttribute *attr) { attr_operand = attr; }
	void SetType(BookmarkOperationType type) { operation_type = type; }
#ifdef SUPPORT_DATA_SYNC
	void SetSync(BOOL sync) { should_sync = sync; }
	void SetFromSync(BOOL sync) { from_sync = sync; }
#endif // SUPPORT_DATA_SYNC
	void SetListener(BookmarkOperationListener *listener) { m_listener = listener; }
	BookmarkOperationListener* GetListener() const { return m_listener; }

	BookmarkItem* GetFirstOperand() const { return first_operand; }
	BookmarkItem* GetSecondOperand() const { return second_operand; }
	BookmarkAttribute* GetAttrOperand() const { return attr_operand; }
	BookmarkOperationType GetType() const { return operation_type; }
#ifdef SUPPORT_DATA_SYNC
	BOOL ShouldSync() const { return should_sync; }
	BOOL FromSync() { return from_sync; }
#endif // SUPPORT_DATA_SYNC
private:
	BookmarkItem *first_operand;
	BookmarkItem *second_operand;
	BookmarkAttribute *attr_operand;

	BookmarkOperationType operation_type;
#ifdef SUPPORT_DATA_SYNC
	BOOL should_sync;
	BOOL from_sync;
#endif // SUPPORT_DATA_SYNC
	BookmarkOperationListener *m_listener;
};

class BookmarkOperationListener
{
public:
	virtual ~BookmarkOperationListener() { }
	virtual void OnOperationCompleted(BookmarkItem *bookmark, BookmarkOperation::BookmarkOperationType operation_type, OP_STATUS res) = 0;
};

#endif // BOOKMARK_OPERATION_H
