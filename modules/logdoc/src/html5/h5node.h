/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef H5NODE_H
#define H5NODE_H

#include "modules/logdoc/logdoc.h"
#include "modules/logdoc/src/html5/html5attrcopy.h"
#include "modules/logdoc/markup.h"
#include "modules/logdoc/src/html5/html5tokenbuffer.h"
#include "modules/util/tempbuf.h"

class HTML5Token;
class HTML5TokenWrapper;
class H5Element;
class HTML5AttrCopy;
class ElementRef;

/**
 * H5Node is the base class for the nodes used in the logical tree when running
 * the HTML 5 parser in standalone mode. We are not using HTML_Element to avoid
 * having to include a lot of code that isn't really needed to just test the
 * tree construction.
 * To avoid having to insert too many #ifdefs and have the parser code as clean 
 * as possible we try to add the most commonly used methods from HTML_Element
 * here, even if it is not really needed and does nothing.
 */

class H5Node
{
public:
	enum Type
	{
		TEXT,
		DOCTYPE,
		COMMENT,
		ELEMENT
	};

	H5Node(Type type)
		: m_parent(NULL)
		, m_pred(NULL)
		, m_suc(NULL)
		, m_type(type) {}
	virtual ~H5Node();

	virtual H5Node*		Pred() const { return m_pred; }
	virtual H5Node*		Suc() const { return m_suc; }
	virtual H5Node*		SucActual() const { return m_suc; }
	virtual H5Element*	Parent() const { return m_parent; }
	virtual H5Node*		FirstChild() const = 0;
	virtual H5Node*		FirstChildActual() { return FirstChild(); }
	virtual H5Node*		LastChild() const = 0;
	virtual H5Node*		Next() const;

	/**
	 * Called when the element is deleted.
	 * See documentation for HTML_Element::Clean()
	 */
	virtual BOOL		Clean() = 0;

	/// Inserts the element in a tree as the last child under parent.
	void				Under(H5Element *parent);
	/// Inserts the element in a tree as the first child before node_after.
	void				Precede(H5Node *node_after);

	/// Removes the element from its parent, siblings and children.
	void				Out();

	/// @returns The node type of this node.
	Type			GetType() const { return m_type; }
	/// @returns The element type of the node.
	virtual Markup::Type
					Type() const = 0;
	/// @returns TRUE if the node is a text node.
	BOOL			IsText() { return m_type == TEXT; }
	/// @returns TRUE if the node is a script element
	virtual BOOL	IsScriptElement() { return FALSE; }

private:
	H5Element*	m_parent;
	H5Node*		m_pred;
	H5Node*		m_suc;

	Type	m_type;
};


/**
 * H5Element represents all elements that can have children in the logical tree.
 */
class H5Element : public H5Node
{
public:
	friend class H5Node;
	friend class ElementRef;
	friend void	HTML5TokenBuffer::ReleaseBuffer();

	H5Element(Markup::Type elm_type, Markup::Ns ns)
		: H5Node(H5Node::ELEMENT)
		, m_first_child(NULL)
		, m_last_child(NULL)
		, m_elm_type(elm_type)
		, m_ns(ns)
		, m_name(NULL)
		, m_attrs(NULL)
		, m_attr_count(0)
		, m_first_ref(NULL) {}
	virtual ~H5Element();

	virtual H5Node*	FirstChild() const { return m_first_child; }
	virtual H5Node*	LastChild() const { return m_last_child; }

	virtual BOOL	Clean();
	virtual void	Free() { OP_DELETE(this); }

	virtual Markup::Type
						Type() const { return m_elm_type; }
	Markup::Ns	GetNs() const { return m_ns; }
	virtual BOOL		IsText() { return FALSE; }
	virtual BOOL		IsScriptElement() { return m_elm_type == Markup::HTE_SCRIPT && (m_ns == Markup::HTML || m_ns == Markup::SVG); }

	const uni_char*	GetName() const { return m_name; }
	void			SetNameL(const uni_char *name, unsigned len);
	/**
	 * Checks if the element has the specified name. The name will only be checked if type
	 * is HTE_UNKNOWN. The check is case sensitive.
	 * @param name A null terminated string representing the name of the element. Only used if type is not known.
	 * @param type The element type to check for, or HTE_UNKONWN if the name is to be used.
	 * @returns TRUE if the element has the type type or the name name in case type is unknown.
	 */
	BOOL			IsNamed(const uni_char *name, Markup::Type type);

	/**
	 * Creates attribute entries for all the attributes found in the token.
	 * @param token A token containing all the attributes the elements should have.
	 */
	void			InitAttributesL(HTML5TokenWrapper *token);
	unsigned		GetAttributeCount() const { return m_attr_count; }
	/**
	 * @param name Null terminated string containing the name of the attribute to fetch.
	 * @returns The value of the attribute with the given name or NULL if no such attribute exists.
	 */
	const uni_char*	GetAttribute(const uni_char *name) const;
	/**
	 * Adds a specified attribute from a tokenizer token.
	 * @param attr_idx The index into the attribute list in token.
	 */
	void			AddAttributeL(HTML5Token *token, unsigned attr_idx);
	/**
	 * Adds an attribute to the element with specified name and value.
	 * @param name Name of the attribute to add, need not be null terminated.
	 * @param name_len Length of the string pointed to by name.
	 * @param value Value of the attribute to add, need not be null terminated.
	 * @param value_len Length of the string pointed to by value.
	 */
	void			AddAttributeL(const uni_char *name, unsigned name_len, const uni_char *value, unsigned value_len);
	HTML5AttrCopy*	GetAttributeByIndex(unsigned idx) { return &m_attrs[idx]; }
	/**
	 * Sorts the attribute list according to their names.
	 */
	void			SortAttributes();

private:
	H5Node*				m_first_child;
	H5Node*				m_last_child;

	Markup::Type		m_elm_type;
	Markup::Ns			m_ns;
	uni_char*			m_name;
	HTML5AttrCopy*		m_attrs;
	unsigned			m_attr_count;
	ElementRef*			m_first_ref;
};


/**
 * H5Comment represents comment nodes in the logical tree. The contents of
 * the comment can be fetched with GetData().
 */

class H5Comment : public H5Node
{
public:
	H5Comment() : H5Node(H5Node::COMMENT), m_data(NULL) {}
	virtual ~H5Comment();

	virtual H5Node*	FirstChild() const { return NULL; }
	virtual H5Node*	LastChild() const { return NULL; }

	virtual BOOL	Clean();

	virtual Markup::Type
					Type() const { return Markup::HTE_COMMENT; }
	virtual BOOL	IsText() { return FALSE; }

	/**
	 * Secondary constructor. Sets the data content of the comment from the given tokenizer token.
	 */
	void			InitDataL(HTML5Token *token);

	const uni_char*	GetData() const { return m_data; }
	void			SetDataL(const uni_char *data, unsigned len);

private:
	uni_char*	m_data;
};


/**
 * H5Doctype represents the doctype node in the logical tree. The name, public id
 * and system identifier is the only data stored.
 */

class H5Doctype : public H5Node
{
public:
	H5Doctype(LogicalDocument *logdoc) : H5Node(H5Node::DOCTYPE), m_logdoc(logdoc) {}
	virtual ~H5Doctype();

	/**
	 * Secondary constructor. Sets the data for the doctype.
	 */
	void			InitL(const uni_char *name, const uni_char *public_id, const uni_char *system_id);

	virtual H5Node*	FirstChild() const { return NULL; }
	virtual H5Node*	LastChild() const { return NULL; }

	virtual BOOL	Clean();

	virtual Markup::Type
					Type() const { return Markup::HTE_DOCTYPE; }
	virtual BOOL	IsText() { return FALSE; }

	const uni_char*	GetName() const { return m_logdoc->GetDoctypeName(); }
	const uni_char*	GetPublicIdentifier() const { return m_logdoc->GetDoctypePubId(); }
	const uni_char*	GetSystemIdentifier() const { return m_logdoc->GetDoctypeSysId(); }

private:
	LogicalDocument*	m_logdoc;
};


/**
 * H5Text represents all the text nodes in the logical tree. The text content can be
 * fetched with Content()
 */

class H5Text : public H5Node
{
public:
	H5Text() : H5Node(H5Node::TEXT) {}
	virtual ~H5Text();

	virtual H5Node*	FirstChild() const { return NULL; }
	virtual H5Node*	LastChild() const { return NULL; }

	virtual BOOL	Clean();

	virtual Markup::Type
					Type() const { return Markup::HTE_TEXT; }
	virtual BOOL	IsText() { return TRUE; }

	const uni_char*	Content() const { return m_text.GetStorage(); }
	void			SetTextL(const uni_char *text, unsigned len);
	void			AppendTextL(const uni_char *text, unsigned len);

private:
	TempBuffer	m_text;
};

#endif // H5NODE_H
