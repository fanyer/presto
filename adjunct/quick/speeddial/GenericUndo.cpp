// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Huib Kleinhout
//

#include "core/pch.h"

#include "GenericUndo.h"

//some modest default limit
#define MAX_GENERIC_UNDO_MEMORY 10*1024

GenericUndoStack::GenericUndoStack() :
	m_memory_limit(MAX_GENERIC_UNDO_MEMORY),
	m_memory_used(0) { }

GenericUndoStack::~GenericUndoStack()
{
	Empty();
}

OP_STATUS GenericUndoStack::Undo()
{
	if (IsUndoAvailable())
	{
		GenericUndoItem* item = static_cast<GenericUndoItem*>(m_undo_stack.Last());
		size_t old_size = item->BytesUsed();
		RETURN_IF_ERROR(item->Undo());
		size_t new_size = item->BytesUsed();
		m_memory_used = m_memory_used + new_size - old_size;
		item->Out();
		item->Into(&m_redo_stack);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

OP_STATUS GenericUndoStack::Redo()
{
	if (IsRedoAvailable())
	{
		GenericUndoItem* item = static_cast<GenericUndoItem*>(m_redo_stack.Last());
		size_t old_size = item->BytesUsed();
		RETURN_IF_ERROR(item->Redo());
		size_t new_size = item->BytesUsed();
		m_memory_used = m_memory_used + new_size - old_size;
		item->Out();
		item->Into(&m_undo_stack);
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

void GenericUndoStack::SetMemoryLimit(const UINT32 max_number_of_bytes)
{
	m_memory_limit = max_number_of_bytes;
	CleanupIfNeeded();
}

void GenericUndoStack::Empty()
{
#ifdef _DEBUG
	// In DEBUG build, remove all items and decrease memory counter for each removal. When done,
	// m_memory_used should be zero.
	// Loop through the undo stack and remove all items:
	while (m_undo_stack.First())
	{
		GenericUndoItem* item = static_cast<GenericUndoItem*>(m_undo_stack.First());
		RemoveItem(item);
	}
	// remove all items from redo stack
	ClearRedoStack();
	OP_ASSERT(m_memory_used == 0);	// undo stack is empty, but something went wrong with memory counting
									// contact the code owner if this happens
#else
	// In RELEASE build, just delete everything and reset the memory counter
	m_undo_stack.Clear();
	m_redo_stack.Clear();
	m_memory_used = 0;
#endif
}

void GenericUndoStack::AddItem(GenericUndoItem* item) 
{ 
	OP_ASSERT(item); //prepare for crashing: this function assumes an item
	// add the change to the undo stack
	item->Into(&m_undo_stack);
	// redo is invalid now, clear the stack
	ClearRedoStack();
	// update counter memory that is used
	m_memory_used += item->BytesUsed();
	//remove some items from the undo stack if it has grown to big
	CleanupIfNeeded();
}

void GenericUndoStack::RemoveItem(GenericUndoItem* item) 
{ 
	OP_ASSERT(item); //prepare for crashing: this function assumes an item
	//decrease memory usage counter with the number of bytes that the item used
	m_memory_used -= item->BytesUsed();
	//take it out of the undo or redo stack
	item->Out();
	//delete it
	OP_DELETE(item);
}

void GenericUndoStack::ClearRedoStack()
{
	while (m_redo_stack.First())
	{
		GenericUndoItem* item = static_cast<GenericUndoItem*>(m_redo_stack.First());
		RemoveItem(item);
	}
}

void GenericUndoStack::CleanupIfNeeded()
{
	while (m_memory_used > m_memory_limit)
	{
		Link* oldest_item = m_undo_stack.First();
		OP_ASSERT(oldest_item);	//If this happens, the stack is empty but it looks as if memory is used
								//probably because some items report inconsitent values when 
								//BytesUsed() is called.
		if (oldest_item == NULL)
			break;
		//take the oldest item out of the stack and clean it from memory
		GenericUndoItem* undo_item = static_cast<GenericUndoItem*>(oldest_item);		
		RemoveItem(undo_item);			
	}
}
