/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2001 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/logdoc/namespace.h"


//************************************************
//***************** NS_Element *******************
//************************************************

NS_Element::~NS_Element()
{
    if( m_uri )
        OP_DELETEA(m_uri); // FIXME:REPORT_MEMMAN-KILSMO
    if( m_prefix )
        OP_DELETEA(m_prefix); // FIXME:REPORT_MEMMAN-KILSMO
}

OP_STATUS NS_Element::Init(const uni_char *new_uri, const uni_char *new_prefix)
{
    return Init(new_uri, uni_strlen(new_uri), new_prefix, uni_strlen(new_prefix));
}


OP_STATUS NS_Element::Init(const uni_char *new_uri, UINT uri_len, const uni_char *new_prefix, UINT prefix_len)
{
    if( m_uri )
        OP_DELETEA(m_uri); // FIXME:REPORT_MEMMAN-KILSMO

    m_uri = OP_NEWA(uni_char, uri_len + 1); // FIXME:REPORT_MEMMAN-KILSMO
    if( ! m_uri )
        return OpStatus::ERR_NO_MEMORY;
    uni_strncpy(m_uri, new_uri, uri_len);
    m_uri[uri_len] = '\0';

    if( m_prefix )
        OP_DELETEA(m_prefix); // FIXME:REPORT_MEMMAN-KILSMO

    m_prefix = OP_NEWA(uni_char, prefix_len + 1); // FIXME:REPORT_MEMMAN-KILSMO
    if( ! m_prefix )
    {
        OP_DELETEA(m_uri); // FIXME:REPORT_MEMMAN-KILSMO
		m_uri = NULL;
        return OpStatus::ERR_NO_MEMORY;
    }
    if (prefix_len)
        uni_strncpy(m_prefix, new_prefix, prefix_len);
    m_prefix[prefix_len] = '\0';

    return OpStatus::OK;
}

BOOL NS_Element::IsEqualUri(const uni_char *c_uri, UINT c_uri_len)
{
	return uni_strlen(m_uri) == c_uri_len && uni_strncmp(m_uri, c_uri, c_uri_len) == 0;
}

BOOL NS_Element::IsEqual(const uni_char *c_uri, UINT c_uri_len, const uni_char *c_prefix, UINT c_prefix_len)
{
	return uni_strlen(m_uri) == c_uri_len && uni_strncmp(m_uri, c_uri, c_uri_len) == 0 &&
	       uni_strlen(m_prefix) == c_prefix_len && (c_prefix_len == 0 || uni_strncmp(m_prefix, c_prefix, c_prefix_len) == 0);
}

OP_STATUS NS_Element::SetUri(const uni_char *new_uri, int new_uri_len)
{
    if( new_uri )
    {
        OP_DELETEA(m_uri); // FIXME:REPORT_MEMMAN-KILSMO
        m_uri = OP_NEWA(uni_char, new_uri_len + 1); // FIXME:REPORT_MEMMAN-KILSMO
        if( ! m_uri )
            return OpStatus::ERR_NO_MEMORY;

        uni_strncpy(m_uri, new_uri, new_uri_len);
        m_uri[new_uri_len] = '\0';
    }
    return OpStatus::OK;
}

//************************************************
//***************** NS_ListElm *******************
//************************************************

NS_ListElm::NS_ListElm(int new_ns_idx, int level)
  : m_ns_idx(new_ns_idx),
    m_level(level),
    m_suc(NULL)
{
    g_ns_manager->GetElementAt(m_ns_idx)->IncRefCountI();
}

NS_ListElm::~NS_ListElm()
{
    g_ns_manager->GetElementAt(m_ns_idx)->DecRefCountI();
}

UINT NS_ListElm::IsEqual(const uni_char *c_uri, UINT c_uri_len, const uni_char *c_prefix, UINT c_prefix_len)
{
    return g_ns_manager->GetElementAt(m_ns_idx)->IsEqual(c_uri, c_uri_len, c_prefix, c_prefix_len);
}

//******************************************************
//***************** NamespaceManager *******************
//******************************************************

NamespaceManager::~NamespaceManager()
{
    if( m_last_used > 0 )
    {
        for(int i = 0; i < m_last_used; i++)
        {
            if( m_elements[i] )
			{
				if (i <= m_last_preinserted)
					m_elements[i]->DecRefCount();

				//OP_ASSERT(m_elements[i]->GetRefCount() == 0);
                OP_DELETE(m_elements[i]); // FIXME:REPORT_MEMMAN-KILSMO
			}
        }
        OP_DELETEA(m_elements); // FIXME:REPORT_MEMMAN-KILSMO
    }
}

void NamespaceManager::InitL(int initial_size)
{
    if( initial_size > 0 )
    {
        m_size = initial_size;
        m_elements = OP_NEWA_L(NS_Element *, m_size); // FIXME:REPORT_MEMMAN-KILSMO

        for(int i = 0; i < m_size; i++)
            m_elements[i] = NULL;

        // The sequence of the following lines is important for the defines found
        // at the top of namespace.h (NS_IDX_HTML, NS_IDX_WML, NS_IDX_...)
        // !!! DO NOT CHANGE THE ORDER IN ONLY ONE OF THESE PLACES !!! (stighal, 2002-03-21)
		InsertElementL(NS_USER, UNI_L(""), UNI_L("")); // NS_IDX_DEFAULT
		InsertElementL(NS_HTML, UNI_L("http://www.w3.org/1999/xhtml"), UNI_L("")); // NS_IDX_HTML
		InsertElementL(NS_HTML, UNI_L("http://www.w3.org/1999/xhtml"), UNI_L("")); // NS_IDX_XHTML
		InsertElementL(NS_WML, UNI_L("http://www.wapforum.org/2001/wml"), UNI_L("")); // NS_IDX_WML
		InsertElementL(NS_SVG, UNI_L("http://www.w3.org/2000/svg"), UNI_L("")); // NS_IDX_SVG
		InsertElementL(NS_VXML, UNI_L("http://www.w3.org/2001/vxml"), UNI_L("")); // NS_IDX_VXML
		InsertElementL(NS_EVENT, UNI_L("http://www.w3.org/2001/xml-events"), UNI_L("")); // NS_IDX_EVENT
		InsertElementL(NS_OMF, UNI_L("http://www.opera.com/2003/omf"), UNI_L("")); // NS_IDX_OMF
		InsertElementL(NS_XV, UNI_L("http://www.voicexml.org/2002/xhtml+voice"), UNI_L("")); // NS_IDX_XV
		InsertElementL(NS_XML, UNI_L("http://www.w3.org/XML/1998/namespace"), UNI_L("xml")); // NS_IDX_XML
		InsertElementL(NS_XMLNS, UNI_L("http://www.w3.org/2000/xmlns/"), UNI_L("xmlns")); // NS_IDX_XMLNS
		InsertElementL(NS_XMLNS, UNI_L("http://www.w3.org/2000/xmlns/"), UNI_L("")); // NS_IDX_XMLNS_DEF
		InsertElementL(NS_XLINK, UNI_L("http://www.w3.org/1999/xlink"), UNI_L("")); // NS_IDX_XLINK
		InsertElementL(NS_MATHML, UNI_L("http://www.w3.org/1998/Math/MathML"), UNI_L("")); // NS_IDX_MATHML
		InsertElementL(NS_CUE, UNI_L("http://www.opera.com/2012/cue"), UNI_L("")); // NS_IDX_CUE

		// Namespace URI for HTML that is used in examples in the XML Namespaces specification.
		InsertElementL(NS_ATOM03, UNI_L("http://purl.org/atom/ns#"), UNI_L("atom"));
		InsertElementL(NS_ATOM10, UNI_L("http://www.w3.org/2005/Atom"), UNI_L("atom"));
    }
}

BOOL NamespaceManager::CompareNsType(int ns_idx1, int ns_idx2)
{
	OP_ASSERT(ns_idx1 >= 0 && ns_idx2 >= 0 && ns_idx1 < m_last_used && ns_idx2 < m_last_used);
	return m_elements[ns_idx1]->GetType() == m_elements[ns_idx2]->GetType();
}

NS_Type NamespaceManager::FindNsType(const uni_char *uri, UINT uri_len)
{
	// Only search the preinserted elements, since those represent all the
	// namespace types we support and all the uri-s we recognize.
	for (int i = NS_IDX_XHTML; i <= m_last_preinserted; ++i)
		if (m_elements[i]->IsEqualUri(uri, uri_len))
			return m_elements[i]->GetType();
	return NS_USER;
}

/* FIXME: remove this when no other modules call it. */
NS_Type NamespaceManager::FindNsType(const uni_char *uri, UINT uri_len, const uni_char *prefix, UINT prefix_len)
{
	return FindNsType(uri, uri_len);
}

int NamespaceManager::FindNsIdx(const uni_char *uri, UINT uri_len, const uni_char *prefix, UINT prefix_len, BOOL strict_prefix /*= FALSE*/)
{
    for(int i = m_last_used - 1; i >= 0; i--)
    {
        if( m_elements[i]->IsEqual(uri, uri_len, prefix, prefix_len) )
            return i;
    }

    return NS_IDX_NOT_FOUND;
}

int NamespaceManager::FindNsIdx(const uni_char *uri, UINT uri_len)
{
    for(int i = m_last_used - 1; i >= 0; i--)
    {
        if( m_elements[i]->IsEqualUri(uri, uri_len) )
            return i;
    }

    return NS_IDX_NOT_FOUND;
}

void NamespaceManager::InsertElementL(NS_Type type, const uni_char *uri, const uni_char *prefix)
{
    if( m_last_used < m_size )
    {
        m_elements[m_last_used] = OP_NEW_L(NS_Element, (type)); // FIXME:REPORT_MEMMAN-KILSMO
        if( m_elements[m_last_used]->Init(uri, prefix) == OpStatus::ERR_NO_MEMORY )
        {
            OP_DELETE(m_elements[m_last_used]); // FIXME:REPORT_MEMMAN-KILSMO
            m_elements[m_last_used] = NULL;
            LEAVE(OpStatus::ERR_NO_MEMORY);
        }
		m_elements[m_last_used]->IncRefCount();
		m_last_preinserted = m_last_used;
        m_last_used++;
    }
}

// Used to add a user namespace
int NamespaceManager::AddNs(NS_Type new_ns_type, const uni_char *uri, UINT uri_len, const uni_char *prefix, UINT prefix_len)
{
    if( m_last_used < m_size )
    {
        m_elements[m_last_used] = OP_NEW(NS_Element, (new_ns_type != NS_NONE ? new_ns_type : NS_USER)); // FIXME:REPORT_MEMMAN-KILSMO
        if( !m_elements[m_last_used] || m_elements[m_last_used]->Init(uri, uri_len, prefix, prefix_len) == OpStatus::ERR_NO_MEMORY )
        {
            OP_DELETE(m_elements[m_last_used]); // FIXME:REPORT_MEMMAN-KILSMO
            m_elements[m_last_used] = NULL;
            return NS_IDX_NOT_FOUND;
        }
        return m_last_used++;
    }

    return NS_IDX_NOT_FOUND;
}

int NamespaceManager::GetNsIdx(const uni_char *uri, UINT uri_len, const uni_char *prefix, UINT prefix_len)
{
	if (!uri || uri_len == 0)
		return NS_IDX_DEFAULT;

    int ns_idx = FindNsIdx(uri, uri_len, prefix, prefix_len);

	if (ns_idx == NS_IDX_NOT_FOUND)
		ns_idx = AddNs(FindNsType(uri, uri_len), uri, uri_len, prefix, prefix_len);

	if (ns_idx == NS_IDX_NOT_FOUND)
	{
		// The list is full, fall back to the null namespace.
		OP_ASSERT(FALSE);
		ns_idx = NS_IDX_DEFAULT;
	}

	return ns_idx;
}

/* FIXME: remove this when no other modules call it. */
int NamespaceManager::GetNsIdx(const uni_char *uri, UINT uri_len, const uni_char *prefix, UINT prefix_len, BOOL force_create /*=FALSE*/, BOOL force_html /*=FALSE*/, BOOL strict_prefix /*=FALSE*/)
{
    return GetNsIdx(uri, uri_len, prefix, prefix_len);
}

int NamespaceManager::GetNsIdxWithoutPrefix(int ns_idx)
{
	if (ns_idx < 0)
		return NS_IDX_NOT_FOUND;

	if (*(m_elements[ns_idx]->GetPrefix()) == 0)
		return ns_idx;

	const uni_char *uri = m_elements[ns_idx]->GetUri();
	int uri_len = uni_strlen(uri);

    for (int i = m_last_used - 1; i >= 0; i--)
    {
        if (m_elements[i]->IsEqualUri(uri, uri_len) && *(m_elements[i]->GetPrefix()) == 0)
            return i;
    }

	return AddNs(GetNsTypeAt(ns_idx), uri, uri_len, NULL, 0);
}
