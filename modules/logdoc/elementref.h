/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef ELEMENTREF_H
#define ELEMENTREF_H

#include "modules/util/simset.h"
#include "modules/logdoc/src/html5/html5base.h"

class FramesDocument;

/**
 * A reference to a HTML_Element to simplify usage when the element is in risk
 * of being deleted or removed from the tree while the reference is in existence.
 *
 * See elementref.html in the documentation directory for detailed information
 * about usage and implementation of the ElementRef class.
 */
class ElementRef
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT
public:
#include "modules/logdoc/elementreftype.h"

protected:
	// Don't create ElementRefs directly, use one of the sub classes
	ElementRef() : m_elm(NULL), m_suc(NULL), m_pred(NULL) {}
	ElementRef(HTML_Element *elm) : m_elm(NULL), m_suc(NULL), m_pred(NULL) { SetElm(elm); }
	virtual	~ElementRef() { UnlinkFromList(); }

public:
	/**
	 * Resets the ElementRef to not not reference anything anymore (sets the ref to NULL)
	 */
	void			Reset() { Unreference(); }

	/**
	 * Sets the element which is to be referenced
	 */

	virtual void	SetElm(HTML5ELEMENT *elm);
	/**
	 * Gets a pointer to the element which is referenced.  May be NULL.
	 */
	HTML5ELEMENT*	GetElm() const { return m_elm; }

	const HTML5ELEMENT* 	operator->() const { return m_elm; }
	HTML5ELEMENT* 	operator->() { return m_elm; }

	/**
	 * Called when the element is about to be deleted.  It may actually
	 * be kept alive for some time longer, but you can't count on it, unless
	 * you later get an OnInserted call.  If the reference is kept (something
	 * only KeepAliveElementRef should ever do) you may get several calls
	 * to OnDelete.
	 */
	virtual	void	OnDelete(FramesDocument *document) {}

	/**
	 * Called when the element has been removed from the tree it was part of.
	 * It is however not being deleted (yet), and may be inserted into a tree again
	 * later (in which there will be a call to OnInserted).
	 * If the element isn't removed from the tree until that happens as part of
	 * the final deletion process you won't get a call to this function.
	 */
	virtual	void	OnRemove(FramesDocument *document) {}

	/**
	 * Called when the element has been inserted into a tree (but not an element
	 * inserted into the tree for the first time as a part of parsing).
	 * The element has been inserted by the time this function is called, so
	 * you can check Parent() etc. to check where it was inserted.
	 * @param old_document  If the element is just moved from one tree to another this
	 *   will be the document it's moved from.  May be the same as new_document or NULL.
	 * @param new_document  The document which owns the tree where the element is inserted.
	 *   May be NULL if the tree the element is inserted into is not connected to any FramesDocument.
	 */
	virtual void	OnInsert(FramesDocument *old_document, FramesDocument *new_document) {}

	/**
	 * Used to check if a reference is of a certain type
	 * @returns TRUE if the ref is of type type.
	 */
	virtual BOOL	IsA(ElementRefType type) { return type == ElementRef::NORMAL; }

	/**
	 * @return Next reference in element's list of references.
	 */
	ElementRef*		NextRef() { return m_suc; }

protected:
	virtual void	Unreference() { UnlinkFromList(); }
	void			UnlinkFromList()
	{
		if (m_suc) // If we've a successor, unchain us
		{
			OP_ASSERT(m_suc->m_pred == this);
			m_suc->m_pred = m_pred;
		}

		if (m_pred) // If we've a predecessor, unchain us
		{
			OP_ASSERT(m_pred->m_suc == this);
			m_pred->m_suc = m_suc;
		}
		else if (m_elm) // If we don't, we're the first in the list
			AdvanceFirstRef();

		m_pred = NULL;
		m_suc = NULL;
		m_elm = NULL;
	}

	void			InsertAfter(ElementRef *pred, HTML5ELEMENT *elm)
	{
		OP_ASSERT(!m_elm && pred && !m_pred && !m_suc);
		m_elm = elm;
		m_pred = pred;
		if (pred->m_suc)
		{
			m_suc = pred->m_suc;
			m_suc->m_pred = this;
		}
		pred->m_suc = this;
	}

private:
	friend class HTML_Element;
	/**
	 * Used by HTML_Element::Clean() to move the reference temporarily to a
	 * separate list while signaling removal or deletion.
	 */
	void			DetachAndMoveAfter(ElementRef *pred)
	{
		if (m_suc)
		{
			OP_ASSERT(m_suc->m_pred == this);
			m_suc->m_pred = m_pred;
		}

		if (m_pred)
		{
			OP_ASSERT(m_pred->m_suc == this);
			m_pred->m_suc = m_suc;
		}

		m_pred = pred;

		if (pred->m_suc)
		{
			m_suc = pred->m_suc;
			m_suc->m_pred = this;
		}
		else
			m_suc = NULL;

		pred->m_suc = this;
	}

	void	AdvanceFirstRef();

	HTML5ELEMENT*	m_elm;

	ElementRef*		m_suc;
	ElementRef*		m_pred;
};

/**
 * Class which just NULLs the ref when it's deleted or removed from the tree
 */
class AutoNullElementRef : public ElementRef
{
public:
	AutoNullElementRef(HTML5ELEMENT *elm) : ElementRef(elm) {}
	AutoNullElementRef() {}

	virtual void	OnDelete(FramesDocument *document) { Reset(); }
	virtual	void	OnRemove(FramesDocument *document) { Reset(); }
};

/**
 * Class which just NULLs the ref when it's deleted
 */
class AutoNullOnDeleteElementRef : public ElementRef
{
public:
	AutoNullOnDeleteElementRef(HTML5ELEMENT *elm) : ElementRef(elm) {}
	AutoNullOnDeleteElementRef() {}

	virtual void	OnDelete(FramesDocument *document) { Reset(); }
};

/**
 * Class which keeps the element alive as long as the reference exists
 */
class KeepAliveElementRef : public ElementRef
{
public:
	KeepAliveElementRef(HTML5ELEMENT *elm) : ElementRef(elm), m_free_element_on_delete(FALSE) {}
	KeepAliveElementRef() : m_free_element_on_delete(FALSE) {}
	virtual	~KeepAliveElementRef() { HTML_Element* elm = GetElm(); UnlinkFromList(); DeleteElementIfNecessary(elm); }

	virtual BOOL	IsA(ElementRefType type) { return type == ElementRef::KEEP_ALIVE; }

	virtual void	OnDelete(FramesDocument *document);
	virtual void	OnInsert(FramesDocument *old_document, FramesDocument *new_document);

	/**
	 *  Tell the reference that it should delete the element as soon as it is unreferenced.
	 *  This means this reference owns the element, and no one else should delete it, or
	 *  reference it after this element stops referencing it.
	 */
	virtual void	FreeElementOnDelete() { m_free_element_on_delete = TRUE; }

protected:
	virtual void	Unreference();

private:
	/**
	 * Deletes the element if m_free_element_on_delete is TRUE.
	 * @param elm  Must be the element we referenced when m_free_element_on_delete was set.
	 */
	void			DeleteElementIfNecessary(HTML_Element* elm);

	BOOL			m_free_element_on_delete;
};
#endif // ELEMENTREF_H
