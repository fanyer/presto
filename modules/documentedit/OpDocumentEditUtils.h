/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef OP_DOCUMENTEDIT_UTILS_H
#define OP_DOCUMENTEDIT_UTILS_H

#ifdef DOCUMENT_EDIT_SUPPORT

#include "modules/logdoc/html.h"
#include "modules/layout/box/box.h"
#include "modules/layout/wordinfoiterator.h"
#ifdef INTERNAL_SPELLCHECK_SUPPORT
#include "modules/spellchecker/opspellcheckerapi.h"
#endif // INTERNAL_SPELLCHECK_SUPPORT

#ifdef _DOCEDIT_DEBUG

class OpDocumentEditDebugCheckerObject
{
public:
	virtual ~OpDocumentEditDebugCheckerObject() {}
	virtual BOOL CheckBeginCount() { return TRUE; }
};

/**
 * Just a debug-construction to be used for checking that the numbers of BeginChange() and EndChange() are equal
 * between entrence of a function and return from the function. To use it, write DEBUG_CHECKER(TRUE); at the first line
 * of a function in a file that includes OpDocumentEdit.h.
 */
class OpDocumentEditDebugChecker
{
public:

	OpDocumentEditDebugChecker(OpDocumentEdit *edit, OpDocumentEditDebugCheckerObject *obj, BOOL check_begin_count, BOOL static_context);
	~OpDocumentEditDebugChecker() ;

private:
	void UpdateCallerInfo(void *call_addr);

private:
	BOOL m_check_begin_count;
	BOOL m_static_context;
	int m_begin_count;
	OpDocumentEdit *m_edit;
	OpDocumentEditDebugCheckerObject *m_obj;
};

#endif // _DOCEDIT_DEBUG

Text_Box *GetTextBox(HTML_Element *helm);

/**
 * This is a base-class for classes who's instances wishes to be forward changes in the logical tree from
 * OpDocumentEdit. For an object to receive those events, it must be registered in OpDocumentEdit by using
 * OpDocumentEdit::AddInternalEventListener, then the corresponding events will be "forwarded" to the object.
 */
class OpDocumentEditInternalEventListener : public Link
{
public:

	virtual ~OpDocumentEditInternalEventListener() { Out(); }
	OpDocumentEditInternalEventListener() : Link() {}

	/** Stop listening to events... */
	virtual void StopListening() { Out(); }

	/** Callback for when an element has been inserted into the logical tree. */
	virtual void OnElementInserted(HTML_Element *helm) {}

	/** Callback for when an element is about to be removed from the logical tree (by HTML_Element::OutSafe). */
	virtual void OnElementOut(HTML_Element *helm) {}

	/** Callback for when an element is about to be deleted (at this point helm is probably not in the logical tree anymore). */
	virtual void OnElementDeleted(HTML_Element *helm) {}

	/** Callback for when an element is about to be changed (e.g. its attributes). */
	virtual void OnElementChange(HTML_Element *helm) {}

	/** Callback for when an element has beed changed (e.g. its attributes ). */
	virtual void OnElementChanged(HTML_Element *helm) {}
};

#ifdef INTERNAL_SPELLCHECK_SUPPORT

class HTML_WordIterator : public OpSpellCheckerWordIterator, public OpDocumentEditInternalEventListener
{
public:
	HTML_WordIterator(OpDocumentEdit *edit, BOOL used_by_session, BOOL mark_separators);
	virtual ~HTML_WordIterator();
	void Reset(BOOL stop_session = TRUE);
	void SetRange(HTML_Element *first, HTML_Element *last);
	void SetAtWord(HTML_Element *helm, int ofs);
	BOOL Active() { return m_active; }
	BOOL BeenUsed() { return m_been_used; }
	OpDocumentEdit *GetDocumentEdit() { return m_edit; }

	HTML_Element *CurrentStartHelm() { return m_current_start; }
	HTML_Element *CurrentStopHelm() { return m_current_stop; }
	BOOL IsInRange(HTML_Element* elm);
	void RestartFromBeginning();
	int CurrentStartOfs() { return m_current_start_ofs; }
	int CurrentStopOfs() { return m_current_stop_ofs; }

	// <<<OpSpellCheckerWordIterator>>>
	virtual const uni_char *GetCurrentWord() { m_been_used = TRUE; return m_current_string.IsEmpty() ? UNI_L("") : m_current_string.CStr(); }
	virtual BOOL ContinueIteration();

	// <<<OpDocumentEditInternalEventListener>>>
	virtual void OnElementOut(HTML_Element *helm);
	virtual void OnElementInserted(HTML_Element *helm);

	/**
	 * Internal docxs function that must be called when the parser or something else causes
	 * an HE_TEXT element to be converted to an HE_TEXTGROUP element.
	 *
	 * @param elm The element that used to be an HE_TEXT but is now an HE_TEXTGROUP.
	 *
	 * @see HTML_Element::AppendText().
	 */
	void OnTextConvertedToTextGroup(HTML_Element* elm);

	/**
	 * Finds the start of a word.
	 *
	 * @return TRUE/FALSE if word was/wasn't found
	 */
	BOOL FindStartOfWord();

	/**
	 * Reads the word found by FindStartOfWord() and sets the position to after the word.
	 *
	 * @param toString  If non-NULL it is set to the word. If NULL the word is just skipped and not stored.
	 *
	 * @return TRUE on success, FALSE otherwise.
	 */
	BOOL ReadWord(OpString* toString);

private:
	void RestartFrom(HTML_Element *helm);

private:
	BOOL m_active;
	BOOL m_been_used;
	BOOL m_used_by_session;
	BOOL m_single_word;
	BOOL m_mark_separators;
	HTML_Element *m_editable_container, *m_after_editable_container;
	HTML_Element *m_current_start, *m_current_stop;
	int m_current_start_ofs, m_current_stop_ofs;
	HTML_Element *m_first, *m_last;
	OpDocumentEdit *m_edit;
	OpString m_current_string;
};

struct SpellWordInfoObject
{
	SpellWordInfoObject() { op_memset(this, 0, sizeof(*this)); }
	SpellWordInfoObject(HTML_Element *helm) { Set(helm); }
	BOOL IsValid() { return !!helm; }

	void Set(HTML_Element *helm)
	{
		Text_Box *box = GetTextBox(helm);
		if(!box || !box->GetWordCount() || !box->GetWords())
		{
			op_memset(this, 0, sizeof(*this));
			return;
		}
		this->helm = helm;
		word_count = box->GetWordCount();
		last_word_misspelled = box->GetWords()[box->GetWordCount()-1].IsMisspelling();
	}

	HTML_Element *helm;
	int word_count;
	BOOL last_word_misspelled;
};

#endif // INTERNAL_SPELLCHECK_SUPPORT

/** Simple way of adding an element in a linked list... */
class OpDocumentEditAutoLink : public Link
{
public:

	virtual ~OpDocumentEditAutoLink() { Out(); }

	/**
	 * @parm object The object that should be kept by this link object.
	 * @parm head The list this link object should be inserted into.
	 */
	OpDocumentEditAutoLink(void *object, Head *head) : Link(), m_object(object) { if(head) Into(head); }

	/** Returns the object beeing kept by this link. */
	void *GetObject() { return m_object; }

	/** Static function that returns the link from list head which keeps object, returns NULL if no such link existed. */
	static OpDocumentEditAutoLink *GetLink(void *object, Head *head);

private:

	void *m_object;
};

/**
 * A class for trying to keep a range of elements (start+stop) where start and stop is under the same container which
 * is given to the constructor. If start is removed will start->Prev() be the new start and if stop is removed will
 * stop->Next() be the new stop element. If start or stop is removed from the logical tree but later inserted again
 * below the container will start or stop change to that element again points at the element that was removed+inserted.
 * If the container is deleted will the object become "invalid".
 */
class OpDocumentEditRangeKeeper : public OpDocumentEditInternalEventListener
#ifdef _DOCEDIT_DEBUG
	, public OpDocumentEditDebugCheckerObject
#endif
{
public:

	virtual ~OpDocumentEditRangeKeeper() {}

	/**
	 * See description of this class for details.
	 * @parm container The container that should be an ancestor of start_elm and stop_elm.
	 * @parm start_elm The start of the range that we should try keep.
	 * @parm start_elm The end of the range that we should try keep.
	 */
	OpDocumentEditRangeKeeper(HTML_Element *container, HTML_Element *start_elm, HTML_Element *stop_elm, OpDocumentEdit *edit);

	/** Returns TRUE if this range of some reason has become "invalid", probably due to that the container has been deleted. */
	BOOL IsValid() { return m_edit && m_container && m_start_elm && m_stop_elm; }

	/** Makes this object "invalid" explicitly and unusable. */
	void MakeInvalid() { m_start_elm = m_stop_elm = m_container = NULL; m_edit = NULL; }

	/** Return the current start element, which might have changed since the object's construction. */
	HTML_Element *GetStartElement() { return m_start_elm; }

	/** Return the current stop element, which might have changed since the object's construction. */
	HTML_Element *GetStopElement() { return m_stop_elm; }

	/** Returns the container which is either the conainer given to the constructor or NULL if the constructor
	    has been deleted or the object has become invalid for any other reason. */
	HTML_Element *GetContainer() { return m_container; }

	// == OpDocumentEditInternalEventListener =======================
	virtual void OnElementInserted(HTML_Element *helm);
	virtual void OnElementOut(HTML_Element *helm);
	virtual void OnElementDeleted(HTML_Element *helm);

private:

	HTML_Element *AdjustInDirection(HTML_Element *helm, BOOL forward, BOOL only_allow_in_direction = FALSE);

private:

	OpDocumentEdit *m_edit;
	HTML_Element *m_container;
	HTML_Element *m_start_elm;
	HTML_Element *m_stop_elm;
	HTML_Element *m_pending_start_elm;
	HTML_Element *m_pending_stop_elm;
};

/**
 * This is a construction which is used for preserving whitespace's collapsing-state at the "edges" around an insertion
 * or deletion. As an example, in OpDocumentEditSelection::RemoveContent, this order of events occurs:
 *
 * OpDocumentEditWsPreserver preserver(start_elm, stop_elm, start_ofs, stop_ofs, m_edit);
 * ...delete stuff between (start_elm, start_ofs) and (stop_elm, stop_ofs)...
 * preserver.WsPreserve();
 *
 * If "pa" is removed from <b>hej </b>pa<b> dig</b> and the spaces are collapsable, then would the space before "dig"
 * be collapsed, but the WsPreserve function ensures that the space will be non-collapsed, because it was non-collapsed
 * before.
 */
class OpDocumentEditWsPreserver : public Link
#ifdef _DOCEDIT_DEBUG
	, public OpDocumentEditDebugCheckerObject
#endif
{
public:

	virtual ~OpDocumentEditWsPreserver() { Out(); }
	OpDocumentEditWsPreserver(OpDocumentEdit *edit) : m_edit(edit) { ClearRange(); }

	/** Creates an instance of this class and calls SetRemoveRange, so it's just constructor + SetRemoveRange in one call, so to say. */
	OpDocumentEditWsPreserver(HTML_Element *start_elm, HTML_Element *stop_elm, int start_ofs, int stop_ofs, OpDocumentEdit *edit);

	/** Sets the preserving range to empty */
	void ClearRange() { m_start_elm = m_stop_elm = NULL; }

	/** Returns TRUE if a call to WsPreserve might have any effect. */
	BOOL MightWsPreserve() { return m_start_elm || m_stop_elm; }

	/** Returns the start-element of the whitespace preserving operation, will be NULL if the last character before the
	    range was not a collapsable space. */
	HTML_Element *GetStartElement() { return m_start_elm; }

	/** Returns the stop-element of the whitespace preserving operation, will be NULL if the first character after the
	    range was not a collapsable space. */
	HTML_Element *GetStopElement() { return m_stop_elm; }

	void SetStartElement(HTML_Element *start_elm) { m_start_elm = start_elm; }
	void SetStopElement(HTML_Element *stop_elm) { m_stop_elm = stop_elm; }

	/** Returns true if there was a collapsable space before the rage and that space was collapsed. */
	BOOL WasStartElementCollapsed() { return m_start_elm && m_start_was_collapsed; }

	/** Returns true there was a collapsable space after the rage and that space was collapsed. */
	BOOL WasStopElementCollapsed() { return m_stop_elm && m_stop_was_collapsed; }

	/**
	 * Sets the range to perform the white space preserving around. FIXME: Give it a better name as it's not only used
	 * when removing something.
	 * @return TRUE if a collapsable space was found just before or after the range, if so we might "preserve" it/them later
	 * using WsPreserve.
	 * @start_elm The element where the e.g. remove-operation should start, the scan for the collapsable whitespace will start
	 * BEFORE (start_elm, start_ofs).
	 * @stop_elm The element where the e.g. remove-operation should stop, the scan for the collapsable whitespace will start
	 * AT (stop_elm, stop_ofs).
	 * @start_ofs The offset into start_elm where the e.g. remove-operation should start, the scan for the collapsable whitespace will start
	 * BEFORE (start_elm, start_ofs).
	 * @stop_ofs The offset into stop_elm where the e.g. remove-operation should stop, the scan for the collapsable whitespace will start
	 * AT (stop_elm, stop_ofs).
	 */
	BOOL SetRemoveRange(HTML_Element *start_elm, HTML_Element *stop_elm, int start_ofs, int stop_ofs, BOOL check_start = TRUE);

	/** If there where any collapsable whitespaces to "preserve", this function performs the preserving. */
	BOOL WsPreserve();

private:

	BOOL GetCollapsedCountFrom(HTML_Element *helm, int ofs, int &found_count);
	HTML_Element *GetNextFriendlyTextHelm(HTML_Element *helm);
	BOOL DeleteCollapsedFrom(HTML_Element *helm, int ofs, BOOL insert_nbsp_first);

private:

	OpDocumentEdit *m_edit;
	HTML_Element *m_start_elm, *m_stop_elm;
	int m_start_ofs, m_stop_ofs;
	BOOL m_start_was_collapsed, m_stop_was_collapsed;
};

/**
 * Class that receives callbacks on deleting html elements in order to
 * reset the pointers in the OpDocumentEditWsPreserver class.
 */
class OpDocumentEditWsPreserverContainer
	: public OpDocumentEditInternalEventListener
{
public:
	OpDocumentEditWsPreserverContainer(OpDocumentEditWsPreserver *preserver, OpDocumentEdit *edit);

	//virtual void OnElementInserted(HTML_Element *helm);
	//virtual void OnElementOut(HTML_Element *helm);
	virtual void OnElementDeleted(HTML_Element *helm);
private:
	OpDocumentEditWsPreserver *m_preserver;
	OpDocumentEdit *m_edit;
};

/**
 * OpDocumentEditWordIterator is an abstraction for easier access to the layout word styling information.
 * It also contains some logic for determining valid caret-positions inside text-elements.
 */

class DocEditWordIteratorSurroundChecker : public WordInfoIterator::SurroundingsInformation
{
private:
	OpDocumentEdit* m_edit;
public:
	DocEditWordIteratorSurroundChecker(OpDocumentEdit *edit) : m_edit(edit) {}
	virtual BOOL HasWsPreservingElmBeside(HTML_Element* helm, BOOL before);
};

class OpDocumentEditWordIterator : public WordInfoIterator
#ifdef _DOCEDIT_DEBUG
	, public OpDocumentEditDebugCheckerObject
#endif
{
public:

	virtual ~OpDocumentEditWordIterator();

	/** Constructor, helm should be a HE_TEXT element and edit an instance of OpDocumentEdit as some functions like
	    OpDocumentEdit::ReflowAndUpdate and OpDocumentEdit::IsFriends are used. */
	OpDocumentEditWordIterator(HTML_Element* helm, OpDocumentEdit *edit);

	/** Returns error status code from the constructor, should be checked before using any functions below. */
	OP_STATUS GetStatus() { return m_status; }

	/** Returns TRUE if there are no valid caret-position in the element. */
	BOOL IsCollapsed() { return !IsValidForCaret(); }

	/** The same as !IsCollapsed(), this means - it returns TRUE if there is a valid caret-offset in the element. */
	BOOL IsValidForCaret(BOOL valid_if_possible = FALSE);

	/** Retrieves the first valid caret-offset into the element in res_ofs, returns FALSE if the element has no valid caret-position. */
	BOOL GetFirstValidCaretOfs(int &res_ofs);

	/** Retrieves the last valid caret-offset into the element in res_ofs, returns FALSE if the element has no valid caret-position. */
	BOOL GetLastValidCaretOfs(int &res_ofs);

	/**
	 * "Snaps" offset ofs to a valid caret-ofset.
	 * @return FALSE if the element has no valid caret-offset.
	 * @parm ofs The offset to start the scan from.
	 * @parm res_ofs The result, if ofs was valid will res_ofs be equals to ofs. If ofs is after the the last valid
	 * caret-offset will we snap to the last valid caret-offset, else we'll snap not the next valid caret-offset.
	 */
	BOOL SnapToValidCaretOfs(int ofs, int &res_ofs);

	/** Same as SnapToValidCaretOfs but res_ofs will be the offset as it would have been if all collapsed characters
	    in the element where removed, ofs should still be an offset into the full text-content of the element though.
		See description of SnapToValidCaretOfs. */
	BOOL GetCaretOfsWithoutCollapsed(int ofs, int &res_ofs);

	/**
	 * Retrieves the next of previous valid character-offset starting from caret-offset ofs.
	 * @return TRUE if such offset was found.
	 * @parm ofs The offset to start from, ofs might be valid or not
	 * @parm res_ofs The result...
	 * @parm TRUE if we should scan forward, else backward.
	 */
	BOOL GetValidCaretOfsFrom(int ofs, int &res_ofs, BOOL forward);

	/** Wrapper around GetValidCaretOfsFrom with forward == TRUE, see GetValidCaretOfsFrom. */
	BOOL GetNextValidCaretOfs(int ofs, int &res_ofs) { return GetValidCaretOfsFrom(ofs,res_ofs,TRUE); }

	/** Wrapper around GetValidCaretOfsFrom with forward == FALSE, see GetValidCaretOfsFrom. */
	BOOL GetPrevValidCaretOfs(int ofs, int &res_ofs) { return GetValidCaretOfsFrom(ofs,res_ofs,FALSE); }

private:
	BOOL HasWsPreservingElmBeside(BOOL before);

	DocEditWordIteratorSurroundChecker m_surround_checker;
	OP_STATUS m_status;
	OpDocumentEdit *m_edit;
	BOOL3 m_is_valid_for_caret;
};

/** Stores position of a non-actual element.
 *
 * Non-actual elements, notably inserted-by-layout, are recreated during
 * reflow therefore a regular pointer to such an element will become invalid
 * after a reflow.
 *
 * This class stores the position of an element relative to its nearest
 * next or previous actual element. Then an element at that relative position
 * may be retrieved even if it has been regenerated by layout.
 * The correct element will only be returned if there have been no changes to
 * that part of the tree.
 */
class NonActualElementPosition
{
public:
	NonActualElementPosition() : m_actual_reference(NULL), m_offset(0), m_forward(FALSE) {}

	/** Initializes the object.
	 *
	 * @param non_actual_element The element whose position will be stored. May be NULL.
	 * @param search_reference_forward Whether to use the next or the previous
	 * actual element as a reference.
	 *
	 * @return
	 *  - OK if initialized successfully,
	 *  - ERR if no reference element can be found.
	 */
	OP_STATUS Construct(HTML_Element* non_actual_element, BOOL search_reference_forward = FALSE);
	HTML_Element* Get() const;
private:
	HTML_Element* m_actual_reference;
	unsigned int m_offset;
	BOOL m_forward;
};

#endif // DOCUMENT_EDIT_SUPPORT

#endif // OP_DOCUMENTEDIT_UTILS_H
