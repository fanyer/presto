/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _NAMESPACE_H_
#define _NAMESPACE_H_

#include "modules/util/simset.h"

/// Reserved namespace indices. All values above NS_IDX_DEFAULT
/// are indices into the namespace table and must match the entries
/// prefilled in NamespaceManager::InitL()
enum NS_Idx
{
	NS_IDX_ANY_BUT_OMF      = -3, ///< Special value to be used intead NS_IDX_ANY_NAMESPACE in mail to disallow mails to style omf elements.
	NS_IDX_ANY_NAMESPACE	= -2, ///< used in CSS
	NS_IDX_NOT_FOUND		= -1,
/// The following defines must be in the same order as the lines in
/// NS_List::Init(). DO NOT CHANGE THE ORDER OF ONLY ONE OF THEM
	NS_IDX_DEFAULT		= 0,
	NS_IDX_HTML         = 1, ///< Used by plain HTML, same namespace as XHTML according to HTML 5
	NS_IDX_XHTML		= 2,
	NS_IDX_WML			= 3,
	NS_IDX_SVG			= 4,
	NS_IDX_VXML			= 5,
	NS_IDX_EVENT		= 6,
	NS_IDX_OMF			= 7,
	NS_IDX_XV			= 8,
	NS_IDX_XML			= 9,
	NS_IDX_XMLNS		= 10,
	NS_IDX_XMLNS_DEF	= 11,
	NS_IDX_XLINK		= 12,
	NS_IDX_MATHML		= 13,
	NS_IDX_CUE			= 14
};

/// The namespace types. These are not the same as the indices since
/// several different indices can point to the same namespace type.
enum NS_Type
{
    NS_NONE = 0,
    NS_HTML,
    NS_WML,
    NS_OPF,
    NS_CSR,
    NS_SVG,
    NS_VXML,
    NS_EVENT,
	NS_OMF,
	NS_XV,
	NS_XML,
	NS_XMLNS,
	NS_ATOM03,
	NS_ATOM10,
	NS_XLINK,
	NS_ARIA,
	NS_MATHML,
	NS_CUE,
    NS_USER,
	NS_SPECIAL,
    NS_LAST
};

#define NS_DIFFERENT    0x0
#define NS_URI_MATCH    0x1
#define NS_PREFIX_MATCH 0x2
#define NS_EQUAL        0x3

/// Storage of the entries in the global namespace table.
class NS_Element
{
private:
	UINT		m_ref_count; ///< Reference counter.
#ifdef _DEBUG
	UINT		m_ref_count_a; ///< attributes, Used to debug reference counting
	UINT		m_ref_count_i; ///< list items, Used to debug reference counting
#endif // _DEBUG

    NS_Type     m_type; ///< Namespace type
    uni_char    *m_uri; ///< Namespace URI
    uni_char    *m_prefix; ///< Namespace prefix

public:
    NS_Element(NS_Type new_type) : m_ref_count(0), 
#ifdef _DEBUG
		m_ref_count_a(0), m_ref_count_i(0), 
#endif // _DEBUG
		m_type(new_type), m_uri(NULL), m_prefix(NULL) {}
    ~NS_Element();
	/// Secondary constructor. One of these must be called before use.
    OP_STATUS   Init(const uni_char *new_uri, const uni_char *new_prefix);
	/// Secondary constructor. One of these must be called before use.
    OP_STATUS   Init(const uni_char *new_uri, UINT uri_len, const uni_char *new_prefix, UINT prefix_len);

	/// Returns TRUE if both the c_uri and c_prefix matches the corresponding
	/// member values exactly.
    BOOL        IsEqual(const uni_char *c_uri, UINT c_uri_len, const uni_char *c_prefix, UINT c_prefix_len);
	/// Returns TRUE if c_uri matches the member URI exactly.
	BOOL		IsEqualUri(const uni_char *c_uri, UINT c_uri_len);

    NS_Type         GetType() { return m_type; }
    const uni_char* GetUri() { return m_uri; }
    const uni_char* GetPrefix() { return m_prefix; }

    OP_STATUS       SetUri(const uni_char *new_uri, int new_uri_len);

#ifdef _DEBUG
	UINT		GetRefCount() { return m_ref_count + m_ref_count_a + m_ref_count_i; }
#else //_DEBUG
	UINT		GetRefCount() { return m_ref_count; }
#endif // _DEBUG
	/// Must be called once every time the index of this element is stored in
	/// another datastructure.
	void		IncRefCount() { m_ref_count++; }
	/// Must be called once for every IncRefCount() when no longer storing the
	/// index of this element in another datastructure.
	void		DecRefCount() { OP_ASSERT(m_ref_count > 0); if (m_ref_count > 0) m_ref_count--; }
#ifdef _DEBUG
	void		IncRefCountA() { m_ref_count_a++; }
	void		DecRefCountA() { OP_ASSERT(m_ref_count_a > 0); if (m_ref_count_a > 0) m_ref_count_a--; }
	void		IncRefCountI() { m_ref_count_i++; }
	void		DecRefCountI() { OP_ASSERT(m_ref_count_i > 0); if (m_ref_count_i > 0) m_ref_count_i--; }
#else // _DEBUG
	void		IncRefCountA() { m_ref_count++; }
	void		DecRefCountA() { OP_ASSERT(m_ref_count > 0); if (m_ref_count > 0) m_ref_count--; }
	void		IncRefCountI() { m_ref_count++; }
	void		DecRefCountI() { OP_ASSERT(m_ref_count > 0); if (m_ref_count > 0) m_ref_count--; }
#endif // _DEBUG
};

/// Used for keeping a list of namespace indices used in a document.
class NS_ListElm
{
private:
	int			m_ns_idx; ///< Index used.
	int			m_level; ///< Element level in the tree.
	NS_ListElm	*m_suc; ///< Next entry in the list.

public:
			NS_ListElm(int new_ns_idx, int level);
			~NS_ListElm();

	int		GetNsIdx() { return m_ns_idx; }
	int		GetLevel() { return m_level; }
	/// Returns TRUE if the c_uri and c_prefix match the NS_Element exactly.
	UINT	IsEqual(const uni_char *c_uri, UINT c_uri_len, const uni_char *c_prefix, UINT c_prefix_len);

	void	Precede(NS_ListElm *after) { m_suc = after; }

	NS_ListElm	*Suc() { return m_suc; }
};

/// Global class for adminstrating namespace lookups.
/// Has a table of all namespaces used in the browser. The namespaces will
/// be filled in dynamically when encountered in a document. Some common 
/// standard namespaces will be prefilled into the table on startup.
class NamespaceManager
{
private:
    int m_last_used; ///< The index of the last entry in the table.
	int m_last_preinserted; ///< The index of the last prefilled entry.
    int m_size; ///< Max number of entries.
    NS_Element **m_elements; ///< The namespace table for storing namespaces.

public:
                NamespaceManager() : m_last_used(0), m_last_preinserted(0), m_size(0), m_elements(NULL) {};
                ~NamespaceManager();
    void        InitL(int initial_size); ///< Secondary constructor. \param[in] initial_size Number of elements in the global namespace table

	/// Get namespace element at an index in the table.
    NS_Element* GetElementAt(int i) const { OP_ASSERT(i >= 0 && i < m_last_used); return m_elements[i]; }
	/// Get namespace type at an index in the table.
    NS_Type     GetNsTypeAt(int i) const { return GetElementAt(i)->GetType(); }
	/// Get namespace prefix at an index in the table.
	const uni_char*
				GetPrefixAt(int i) { return GetElementAt(i)->GetPrefix(); }

	/// Returns TRUE if the elements at two indices have the same namespace type.
	BOOL		CompareNsType(int ns_idx1, int ns_idx2);

	/// Returns a namespace matching a URI. Case sensitive
	///\param[in] uri Namespace URI string, need not be nul terminated
	///\param[in] uri_len Length of uri
    NS_Type     FindNsType(const uni_char *uri, UINT uri_len);
	/// NOT USED: Returns a namespace matching a URI and a prefix. Case sensitive
	///\param[in] uri Namespace URI string, need not be nul terminated
	///\param[in] uri_len Length of uri
	///\param[in] prefix Prefix string, need not be nul terminated
	///\param[in] prefix_len Length of prefix
    NS_Type     FindNsType(const uni_char *uri, UINT uri_len, const uni_char *prefix, UINT prefix_len);

	/// Return the index of the element matching both the prefix and URI.
	///\param[in] strict_prefix NOT USED. Must match prefix if TRUE.
    int         FindNsIdx(const uni_char *uri, UINT uri_len, const uni_char *prefix, UINT prefix_len, BOOL strict_prefix = FALSE);

	/// Return the index of a element matching the URI.
	///\param[in] uri Namespace URI string, need not be nul terminated
	///\param[in] uri_len Length of uri
	int			FindNsIdx(const uni_char *uri, UINT uri_len);
	
	/// INTERNAL: Insert a new namespace element into the global table.
    void        InsertElementL(NS_Type id, const uni_char *uri, const uni_char *prefix);

	/// Get the namespace index matching a URI and prefix. Will create a new if it doesn't exist.
	///\param[in] uri The namespace URI, need not be nul terminated
	///\param[in] url_len Length of uri
	///\param[in] prefix The namespace URI, need not be nul terminated. Can be empty or NULL
	///\param[in] prefix_len Length of prefix
    int         GetNsIdx(const uni_char *uri, UINT uri_len, const uni_char *prefix, UINT prefix_len);
	/// NOT USED: Get the namespace index matching a URI and prefix. Will create a new if it doesn't exist.
    int         GetNsIdx(const uni_char *uri, UINT uri_len, const uni_char *prefix, UINT prefix_len, BOOL force_create, BOOL force_html = FALSE, BOOL strict_prefix = FALSE);

	/// Returns a namespace index to an element with the same URI as that of the given
	/// by the namespace index argument but without a prefix. Will return the same index
	/// as the argument if it doesn't have a prefix.
	int			GetNsIdxWithoutPrefix(int ns_idx);

	/// Add an entry to the global namespace table.
	///\param[in] new_ns_type The numeric namespace type of the new namespace
	///\param[in] uri The namespace URI, need not be nul terminated
	///\param[in] uri_len Length of uri
	///\param[in] prefix The namespace prefix, need not be nul terminated. Can be empty or NULL
	///\param[in] prefix_len Length of prefix
    int         AddNs(NS_Type new_ns_type, const uni_char *uri, UINT uri_len, const uni_char *prefix, UINT prefix_len);

	/// Increase reference count for element at index.
	void		IncNsRefCount(int ns_idx) { OP_ASSERT(ns_idx >= 0 && ns_idx < m_last_used); m_elements[ns_idx]->IncRefCount(); }
	/// Decrease reference count for element at index.
	void		DecNsRefCount(int ns_idx) { OP_ASSERT(ns_idx >= 0 && ns_idx < m_last_used); m_elements[ns_idx]->DecRefCount(); }
};

#endif // _NAMESPACE_H_
