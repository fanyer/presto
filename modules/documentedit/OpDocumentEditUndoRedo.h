/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DOCUMENTEDIT_UNDOREDO_H
#define DOCUMENTEDIT_UNDOREDO_H

#ifdef DOCUMENT_EDIT_SUPPORT

#include "modules/logdoc/htm_elm.h"

class OpDocumentEdit;
class OpDocumentEditUndoRedoStack;

/** A object that simply calls stack->BeginGroup in the constructor and stack->EndGroup in the destructor.
	Used for automatic errorhandling in grouped functions. */

class OpDocumentEditUndoRedoAutoGroup
{
public:
	OpDocumentEditUndoRedoAutoGroup(OpDocumentEditUndoRedoStack* stack);
	~OpDocumentEditUndoRedoAutoGroup();
private:
	OpDocumentEditUndoRedoStack* stack;
};

/** An object for temporarily disabling OpDocumentEditUndoRedoStack's change registration.
 *
 * Create this object on stack and undo-redo mechanism will not register the
 * changes performed on the document till the object is destroyed.
 */
class OpDocumentEditDisableUndoRegistrationAuto
{
public:
	OpDocumentEditDisableUndoRegistrationAuto(OpDocumentEditUndoRedoStack* stack);
	~OpDocumentEditDisableUndoRegistrationAuto();
private:
	OpDocumentEditUndoRedoStack* stack;
};

// == OpDocumentEditUndoRedoEvent ==================================================


class OpDocumentEditUndoRedoEvent : public Link, public Head
#ifdef _DOCEDIT_DEBUG
	, public OpDocumentEditDebugCheckerObject
#endif
{
public:
	OpDocumentEditUndoRedoEvent(
		OpDocumentEdit *edit
		);

	virtual ~OpDocumentEditUndoRedoEvent();

	/**
	 * Registers UNDO/REDO event begin.
	 *
	 * @param containing_elm - an element containing the element to be changed
	 * @param element_changed - the element to be changed
	 * @param type - Type of an event (@see UndoRedoEventType)
	 *
	 */
	OP_STATUS BeginChange(HTML_Element* containing_elm, HTML_Element* element_changed, int type);

	/**
	 * Registers UNDO/REDO event end.
	 *
	 * @param containing_elm - an element containing the element to be changed (the same as passed to BeginChange())
	 *
	 */
	OP_STATUS EndChange(HTML_Element* containing_elm);

	void Undo();
	void Redo();

	OP_STATUS Protect(HTML_Element* element);
	void Unprotect(HTML_Element* element);

	/** Returns if this event can be appended to the previous_event. (If it is the same type of event following the previous one) */
	BOOL IsAppending(OpDocumentEditUndoRedoEvent* previous_event);
	BOOL IsInsertEvent();
	/** Merges this event (and all events merged to it) to the given event */
	BOOL MergeTo(OpDocumentEditUndoRedoEvent *evt) 
	{
		if (evt)
		{
			OP_ASSERT(!InList() || !"This element is currently in a list and it's tried to be merged to some other. Remove it from the list first!");

			Into(evt);
			while (OpDocumentEditUndoRedoEvent* merged = static_cast<OpDocumentEditUndoRedoEvent*>(First()))
			{
				merged->Out();
				merged->Into(evt);
			}
			return TRUE;
		}
		return FALSE;
	}

	
	void SetUserInvisible() { user_invisible = TRUE; }
	BOOL IsUserInvisible() const { return user_invisible; }

	/**
	 * Updates the used memory counter in a event
	 */
	void UpdateBytesUsed();
	/**
	 * Updates the used memory counter in the passed in event and all events merged to it
	 */
	static void UpdateAllBytesUsed(OpDocumentEditUndoRedoEvent *evt);
	/**
	 * Returns an event's used memory counter value
	 */
	UINT32 BytesUsed() const { return bytes_used; }
	/**
	 * Returns sum of the used memory counter values of the passed in event and all events merged to it
	 */
	static UINT32 AllBytesUsed(OpDocumentEditUndoRedoEvent *evt);
	/**
	 * Returns a type of an event (@see UndoRedoEventType)
	 */
	int Type() const { return type; }

private:
	INT32 caret_elm_nr_before;
	INT32 caret_ofs_before;
	INT32 caret_elm_nr_after;
	INT32 caret_ofs_after;
	INT32 elm_nr;
	INT32 changed_elm_nr_before;
	INT32 changed_elm_nr_after;
	INT32 bytes_used;
	HTML_Element* element_changed;
	/**
	 * Class responsible for a proper element insertion (to a proper place in a tree).
	 * It's used e.g. for undoing REMOVE operation.
	 */
	class UndoRedoTreeInserter
	{
	public:
		UndoRedoTreeInserter(OpDocumentEditUndoRedoEvent* e) : evt(e), mark(-1), type(ELEMENT_MARK_NONE)
		{
			OP_ASSERT(evt);
		}

		/** Finds a marker element for the passed in element. The marker element is later used to identify a place where the element must be inserted
		 *
		 * @param elm the element the marker element should be found for.
		 */
		void FindPlaceMarker(HTML_Element* elm);
		/**
		 * Inserts element to a proper place where it should be (i.e. a plece where it was before REMOVE operation)
		 *
		 * @param element - an element to be inserted
		 * @param context - Document Context needed by [Under|Follow|Procede]Safe functions
		 * @param pos - position where the element should be inserted on
		 * @param mark_dirty - whether the element should eb marked as dirty
		 *
		 */
		void Insert(HTML_Element* element, const HTML_Element::DocumentContext& context, INT32 pos, BOOL mark_dirty = TRUE);

	private:
		OpDocumentEditUndoRedoEvent* evt;
		/** Position of the mark element - the element which will be used to insert a given element (@see ElementMarkType) */
		INT32 mark;
		/** A type of the mark element */
		enum ElementMarkType
		{
			ELEMENT_MARK_NONE,
			/** A element used as a mark is a parent of the element to be inserted (so UnderSafe() will be used to insert the element) */
			ELEMENT_MARK_PARENT,
			/** A element used as a mark is a predecessor of the element to be inserted (so FollowSafe() will be used to insert the element) */
			ELEMENT_MARK_PRED,
			/** A element used as a mark is a successor of the element to be inserted (so PrecedeSafe() will be used to insert the element) */
			ELEMENT_MARK_SUC
		};
		ElementMarkType type;

		/** Finds succesor of the elm suitable to be marker of place where the element is.
		 * @return TRUE if any was found.
		 */
		BOOL FindSucMark(HTML_Element* elm);
		/** Finds predecessor of the elm suitable to be marker of place where the element is.
		 * @return TRUE if any was found.
		 */
		BOOL FindPredMark(HTML_Element* elm);
		/** Finds parent of the elm suitable to be marker of place where the element is.
		 * Note: if elm is not the last nor the only child of the parent ELEMENT_MARK_SUC or ELEMENT_MARK_PRED are found
		 * within the parent instead.
		 * @return TRUE if any was found.
		 */
		BOOL FindParentMark(HTML_Element* elm);
	};
	UndoRedoTreeInserter tree_inserter;

	friend class UndoRedoTreeInserter;
	int type;
	BOOL user_invisible;
	BOOL is_ended;
	OpDocumentEdit* m_edit;
	/** A stage an event is currently before (if the stage is UNDO the next possible operation is UNDO) */
	enum Stage
	{
		UNDO,
		REDO
	};
	Stage stage;
	ES_Runtime *runtime;

	HTML_Element* GetElementOfNumber(INT32 nr);
	INT32 GetNumberOfElement(HTML_Element* elm);
	HTML_Element* CreateClone(HTML_Element* elm, BOOL deep);
	OP_STATUS Clone(HTML_Element* target, HTML_Element* source, BOOL deep);
	void Action(int type);
};

// == OpDocumentEditUndoRedoStack ==================================================

class OpDocumentEditUndoRedoStack
	: public OpDocumentEditInternalEventListener
#ifdef _DOCEDIT_DEBUG
	, public OpDocumentEditDebugCheckerObject
#endif
{
	friend class OpDocumentEditDisableUndoRegistrationAuto;
public:
	OpDocumentEditUndoRedoStack(OpDocumentEdit* edit);
	virtual ~OpDocumentEditUndoRedoStack();

	void Clear(BOOL clear_undos = TRUE, BOOL clear_redos = TRUE);

	/**
	 * Registers UNDO/REDO change(s) begin.
	 *
	 * @param containing_elm - an element containing the element to be changed
	 * @param flags - the change flags (@see ChangeFlags)
	 *
	 */
	OP_STATUS BeginChange(HTML_Element* containing_elm, int flags);
	/**
	 * Registers UNDO/REDO change(s) end.
	 *
	 * @param containing_elm - an element containing the element to be changed - the same as passed to BeginChange()
	 *
	 */
	OP_STATUS EndChange(HTML_Element* containing_elm);
	void AbortChange();

	/** The events made between BeginGroup and EndGroup will be merged and handled as one event, even if they aren't appended. */
	void BeginGroup();
	void EndGroup();

	BOOL CanUndo() const { return !!m_undos.Last(); }
	BOOL CanRedo() const { return !!m_redos.First(); }

	INT32 GetBeginCount() const { return m_begin_count; }
	INT32 GetBeginCountCalls() const { return m_begin_count_calls; }
	HTML_Element *GetCurrentContainingElm() const { return m_current_containing_elm; }
	BOOL IsValidAsChangeElm(HTML_Element *helm) const { return helm && (!m_begin_count || !m_current_containing_elm || m_current_containing_elm == helm ||m_current_containing_elm->IsAncestorOf(helm)); }

	void Undo();
	void Redo();

	/** The last change should be merged to the change (or chain of changes) before that */
	void MergeLastChanges();

#ifdef _DOCEDIT_DEBUG // OpDocumentEditDebugCheckerObject
	virtual BOOL CheckBeginCount() { return FALSE; }
#endif

	// OpDocumentEditInternalEventListener interface
	void OnElementOut(HTML_Element *elm);
	void OnElementInserted(HTML_Element* elm);
	void OnElementChange(HTML_Element *elm);
	void OnElementChanged(HTML_Element *elm);

	void SetFlags(int flags) { m_flags = flags; }
	int GetFlags() const { return m_flags; }

private:
	UINT32 m_mem_used;
	Head m_undos;
	Head m_redos;
	OpDocumentEdit* m_edit;
	OpDocumentEditUndoRedoEvent* m_current_event;
	INT32 m_begin_count;
	INT32 m_begin_count_calls;
	INT32 m_group_begin_count;
	INT32 m_group_event_count;
	HTML_Element* m_current_containing_elm;
	int m_flags;
	unsigned int m_disabled_count;

	OP_STATUS BeginChange(HTML_Element* containing_elm, HTML_Element* element_changed, int type, int flags);
	OP_STATUS EndLastChange(HTML_Element* containing_elm);

	void DisableChangeRegistration()  { ++m_disabled_count; }
	void EnableChangeRegistration()   { OP_ASSERT(m_disabled_count > 0); --m_disabled_count; }
};

#endif // DOCUMENT_EDIT_SUPPORT
#endif // DOCUMENTEDIT_UNDOREDO_H
