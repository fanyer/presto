/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UNDOREDOSTACK_H
#define UNDOREDOSTACK_H

#ifdef WIDGETS_UNDO_REDO_SUPPORT

/**
 * UndoRedoEvent is storing data about some change in a textfield.
 * It can be either removed or added text, and selection start and stop.
 * Removal events can have a special type which is replace. A replace event
 * means that the entire contents of the edit field were replaced by something
 * else. Replace events must be followed by an insertion event. The replace
 * event has the previous text, and the insertion event has the text that
 * replaced the former.
 **/
class UndoRedoEvent : public Link
{
public:
	typedef enum {
		EV_TYPE_INVALID = 0,
		EV_TYPE_INSERT,
		EV_TYPE_REMOVE,
		EV_TYPE_REPLACE,
#ifdef WIDGETS_IME_SUPPORT
		EV_TYPE_EMPTY,
#endif // WIDGETS_IME_SUPPORT
	} EventType;

	/**
	 * Constructs an event object for an insert event
	 */
	static UndoRedoEvent* Construct(INT32 caret_pos, const uni_char* str, INT32 len);

	/**
	 * Constructs an event object for a remove event
	 */
	static UndoRedoEvent* Construct(INT32 caret_pos, INT32 sel_start, INT32 sel_stop, const uni_char* removed_str);

	/**
	 * Constructs an event object for a replace event
	 */
	static UndoRedoEvent* ConstructReplace(INT32 caret_pos, INT32 sel_start, INT32 sel_stop, const uni_char* removed_str, INT32 removed_str_length);

#ifdef WIDGETS_IME_SUPPORT
	static UndoRedoEvent* ConstructEmpty();
#endif // WIDGETS_IME_SUPPORT

	UndoRedoEvent();
	~UndoRedoEvent();

	/**
	 * This method initializes this object as an insert event
	 *
	 * @param caret_pos   caret position before text insertion
	 * @param str         string of text added
	 * @param len         length of str
	 */
	OP_STATUS Create(INT32 caret_pos, const uni_char* str, INT32 len);

	/**
	 * This method initializes this object as a remove event
	 *
	 * @param caret_pos             caret position before text removal
	 * @param sel_start             selection start before text removal
	 * @param sel_stop              selection stop before text removal
	 * @param removed_str           string of text removed
	 */
	OP_STATUS Create(INT32 caret_pos, INT32 sel_start, INT32 sel_stop, const uni_char* removed_str);

	/**
	 * This method initializes this object as a replace event
	 *
	 * @param caret_pos             caret position before text removal
	 * @param sel_start             selection start before text removal
	 * @param sel_stop              selection stop before text removal
	 * @param removed_str           string of text removed
	 * @param removed_str_length    length of str
	 */
	OP_STATUS CreateReplace(INT32 caret_pos, INT32 sel_start, INT32 sel_stop, const uni_char* removed_str, INT32 removed_str_length);

#ifdef WIDGETS_IME_SUPPORT
	OP_STATUS CreateEmpty();
#endif // WIDGETS_IME_SUPPORT

	inline EventType GetType() const { return event_type; };

	UINT32 BytesUsed();
public:

	/*
	 * In an insert event str stores the text that was added
	 * In a remove event str stores the text that was removed
	 * In a replace event str stores the text that was replaced, not the new one
	 */
	uni_char* str;
	/*
	 * Length of str
	 */
	INT32 str_length;
	/**
	 * Size of buffer pointed by str. Tells maximum number of chars
	 * that str can contain, excluding the terminating 0
	 */
	INT32 str_size;
	/*
	 * caret_pos stores the caret position before the text changes, if this is an insert or replace event
	 */
	INT32 caret_pos;
	/*
	 * sel_start stores the beginning of the selection of text that was removed on a remove event
	 */
	INT32 sel_start;
	/*
	 * sel_start stores the end of the selection of text that was removed on a remove event
	 */
	INT32 sel_stop;

private:

	friend class UndoRedoStack;
	OP_STATUS Append(const uni_char *instr, INT32 len);
	OP_STATUS AppendDeleted(const uni_char *instr, INT32 inlen);

#ifndef UNDO_EVERY_CHARACTER
	/**
	 * Round number up to be a multiple of eight.
	 * Will be used for reallocations when appending text to events
	 */
	static INT32 RoundSizeUpper(INT32 size){ return (size + 7) & ~0x7; }
#else
	/**
	 * Because reallocations will not be used, just use this size
	 * to allocate the buffer
	 */
	static INT32 RoundSizeUpper(INT32 size){ return size; }
#endif //UNDO_EVERY_CHARACTER

	/*
	 * type of event: insert, replace, remove
	 */
	EventType event_type : 16;

	/*
	 * Is appended? Set in Append and AppendDeleted.
	 */
	bool is_appended : 1;
};

/** UndoRedoStack is keeping track of changes done to a textfield. Changes are added with
	SubmitInsert and SubmitRemove, and changes old can be received with Undo() and Redo() */

class UndoRedoStack
{
public:
	UndoRedoStack();
	~UndoRedoStack();

	/** Remove all stored changes in the undo list and/or redo list */
	void Clear(BOOL clear_undos = TRUE, BOOL clear_redos = TRUE);

	/** Clears all redo-events and adds a new undoable event for inserted text.
	 *
	 *  @param caret_pos        caret position where text is inserted
	 *  @param str              text that was inserted
	 *  @param no_append        If no_append is FALSE and UNDO_EVERY_CHARACTER is
	 *                          not defined, this event may be appended to the
	 *                          previous one. If no_append is TRUE, this will
	 *                          be a separate event.
	 *  @param len              length of st 
	*/
	OP_STATUS SubmitInsert(INT32 caret_pos, const uni_char* str, BOOL no_append, INT32 len);

	/**
	 *  Clears all redo-events and adds a new undoable event for removing text.
	 *  sel_start and sel_stop will be used to save selection and tell
	 *  the amount of chars that will be saved on the undo event
	 *
	 *  @param caret_pos      caret position before event
	 *  @param sel_start      selection start (anchor) before event
	 *  @param sel_stop       selection end (offset) before event
	 *  @param removed_str    buffer with removed text
	 *  
	 **/
	OP_STATUS SubmitRemove(INT32 caret_pos, INT32 sel_start, INT32 sel_stop, const uni_char* removed_str);

	/**
	 *  Clears all redo-events and adds a new undoable event for replacing all text.
	 *  sel_start and sel_stop will be used only to save selection, while
	 *  removed_str_length will have the amount of chars that should be saved on the undo event
	 *
	 *  @param caret_pos              caret position before event
	 *  @param sel_start              selection start (anchor) before event
	 *  @param sel_stop               selection end (offset) before event
	 *  @param removed_str            buffer with removed text
	 *  @param removed_str_length     length of removed_str in chars
	 *  @param inserted_str           buffer with new text
	 *  @param inserted_str_length    length of inserted_str in chars
	 **/
	OP_STATUS SubmitReplace(INT32 caret_pos, INT32 sel_start, INT32 sel_stop,
				const uni_char* removed_str, INT32 removed_str_length, 
				const uni_char* inserted_str, INT32 inserted_str_length);

#ifdef WIDGETS_IME_SUPPORT
	/**
	   due to how the IME support is implemented we sometimes need to
	   submit a dummy event to the undo stack.
	 */
	OP_STATUS SubmitEmpty();
#endif // WIDGETS_IME_SUPPORT

	/** Return TRUE if there is events in the undo-list */
	inline BOOL CanUndo() const { return !!undos.Last(); }

	/** Return TRUE if there is events in the redo-list */
	inline BOOL CanRedo() const { return !!redos.First(); }

	/** If there is a event in the undo-list, it will be moved to the
		redo-list and returned from this function. */
	UndoRedoEvent* Undo();

	/** If there is a event in the redo-list, it will be moved to the
		undo-list and returned from this function. */
	UndoRedoEvent* Redo();

	/**
	 * This method peeks the last Undo event in the stack (the nearest one)
	 * without changing the undo stack
	 *
	 * @return the event object, or NULL if the stack is empty
	 */
	inline UndoRedoEvent* PeekUndo() const { return (UndoRedoEvent*) undos.Last(); };

	/**
	 * This method peeks the first Redo event in the stack (the nearest one)
	 * without changing the undo stack
	 *
	 * @return the event object, or NULL if the stack is empty
	 */
	inline UndoRedoEvent* PeekRedo() const { return (UndoRedoEvent*) redos.First(); };

	/** Checks if MAX_MEM_PER_UNDOREDOSTACK is reached, and removes stuff if necessary. */
	void CheckMemoryUsage();

private:
	UINT32 mem_used;
	Head undos;
	Head redos;
};

#endif // WIDGETS_UNDO_REDO_SUPPORT

#endif // UNDOREDOSTACK_H
