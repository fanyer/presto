// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Huib Kleinhout
//

/************************************ GenericUndoStack ************************************
 * Base classes that can be used to easily implement an undo stack.
 * The general concept is that you every time the user performs an action that, you add
 * an Item to the stack that stores the changes. This item, that inherits from GenericUndoItem
 * also should contain the logic to Undo and Redo the change.
 * The stack (GenericUndoStack) manages the list of undo items, and can be called to Undo
 * or Redo the last user action.
 *
 * HOW TO START
 * To start using it, you need to implement the GenericUndoItem interface, so it contains
 * the data of the changes and the logic to restore the changes.
 * Most likely, you want to extend GenericUndoStack too. For instance to add helper functions
 * for adding items to the stack. See speeddialundo for an example.
 *
 ******************************************************************************************/

#ifndef __GENERIC_UNDO_H__
#define __GENERIC_UNDO_H__

/* Base class that hold undo/redo data and logic. All function are pure virtuals, so they
 * need to be implemented.
 * When Undo() is called, the Item should revert the change it stores. When Redo is called,
 * the change should be reapplied. Undo/Redo always follows order of the change (ie no selective undo)
 * This means that
 * - Undo is always called first
 * - Undo and Redo calls are alternating
 * - Undo is not called when a later change is not undone yet
 * - Redo is not called when an earlier change is not redone yet
 */
class GenericUndoItem : public Link
{
public:
	/* Main API: Undo() and Redo()
	 * The functions undo and redo the changed stored int the object.
	 * If undo/redo cannot be executed, the function should return an error and make sure
	 * that no change is made at all. This requirement is needed to ensure that Undo/Redo can be
	 * called again (for instance when memory is freed, and the function failed because there was 
	 * no memory). It also ensures the subsequent undo items remain valid if they are based on
	 * previous items.
	 */

	/* Undo the change
	 * @return	OpStatus::OK when Undo was completed succesfully. When undo could not be executed
	 *			completely, the error should be returned and nothing should be changed.
	 */
	virtual OP_STATUS	Undo() = 0;

	/* Redo the change
	 * @return	OpStatus::OK when Redo was completed succesfully. When redo could not be executed
	 *			completely, the error should be returned and nothing should be changed.
	 */
	virtual OP_STATUS	Redo() = 0;

	/* The number of bytes that are used to store this item in memory. To make memory management
	 * easier this function should always return the same value. In general, the size of an instance 
	 * equals sizeof(*this) and the size of the dynamic memory that is used (the memory pointers point to)
	 *
	 * @return	The number of bytes used by this object instance
	 */
	virtual size_t		BytesUsed() const = 0;

	virtual ~GenericUndoItem() {}
};

/* Base class of the undo stack. Can be used directly, but you might want to
 * add implementation specific functions. For a text edit control, this could f.i. be AddText(...) 
 * and DeleteText(...). Those function would insert items (implemeting GenericUndoItem) into the stack
 */
class GenericUndoStack
{
public:
	/*** Main API: Functions for Undoing and Redoing changes. ***/
	/* Undo the previous change
	 * @return If an error is returned, nothing is undone
	 */
	virtual OP_STATUS	Undo();

	/* Revert the the previous undo
	 * @return If an error is returned, nothing is redone
	 */
	virtual OP_STATUS	Redo();

	/* Test whether undo/redo is can be called
	 * @return TRUE if an undo/redo item is available, and Undo()/Redo() can be called
	 */
	virtual bool		IsUndoAvailable() { return (m_undo_stack.Last() != NULL); }
	virtual bool		IsRedoAvailable() { return (m_redo_stack.Last() != NULL); }

	/* Change the maximum amount of memory then should be used by the undo/redo stack.
	 * When the limit is reached, the oldest changes will be removed. The limit will be applied
	 * immidiately, but if there is redo history, it will be kept available.
	 * @param max_number_of_bytes	Maximum amount of memory used by the items in the undo and redo stack.
	 */
	void	SetMemoryLimit(const UINT32 max_number_of_bytes);

	/* Delete all undo and redo history */
	void	Empty();

protected:
	/* Insert an item into the undo stack. Calling this function invalidates all possible redo's,
	 * so it will delete all items in the redo stack. Older items in the undo stack might get deleted 
	 * when the memory limit is reached.
	 * @param item	Undo item that should be added to the stack
	 */
	void	AddItem(GenericUndoItem* item);

	GenericUndoStack();
	virtual ~GenericUndoStack();
private:
	void	RemoveItem(GenericUndoItem* item);
	void	ClearRedoStack();
	void	CleanupIfNeeded();

	Head		m_undo_stack;
	Head		m_redo_stack;
	size_t		m_memory_limit;	//< Maximum mumber of bytes that can be used by the undo stack
	size_t		m_memory_used;	//< Number of bytes used in the undo/redo stacks
};

#endif // __GENERIC_UNDO_H__
