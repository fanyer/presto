/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/doc/frm_doc.h"
#include "modules/style/css_matchcontext.h"
#include "modules/style/src/css_selector.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/url/url_api.h"
#include "modules/layout/cascade.h"
#include "modules/layout/layoutprops.h"
#include "modules/layout/layout_workplace.h"

void
CSS_MatchContextElm::Init(CSS_MatchContextElm* parent,
						  CSS_MatchContextElm* child,
						  HTML_Element* html_elm,
						  HTML_Element* fullscreen_elm,
						  LinkState visited_links_state)
{
	m_parent = parent;
	m_html_elm = html_elm;
	m_packed_init = 0;
	m_packed.type = html_elm->Type();
	m_packed.ns_idx = html_elm->GetNsIdx();
	m_packed.link_state = visited_links_state;
	if (parent && parent->m_packed.display_none)
		m_packed.display_none = 1;
	OP_ASSERT(!IsLinkStateResolved());

#ifdef DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS
	m_packed2_init = 0;
#endif // DISABLE_HIGHLIGHT_ON_PSEUDO_CLASS

	if (child)
	{
		child->m_parent = this;
#ifdef DOM_FULLSCREEN_MODE
		if (child->m_packed.fullscreen == NO_FULLSCREEN)
		{
			if (html_elm == fullscreen_elm)
				m_packed.fullscreen = FULLSCREEN;
		}
		else
			m_packed.fullscreen = FULLSCREEN_ANCESTOR;
#endif // DOM_FULLSCREEN_MODE
	}
#ifdef DOM_FULLSCREEN_MODE
	else
		if (!parent || parent->m_packed.fullscreen != NO_FULLSCREEN)
		{
			if (parent && parent->m_packed.fullscreen == FULLSCREEN)
				m_packed.fullscreen = NO_FULLSCREEN;
			else if (html_elm == fullscreen_elm)
				m_packed.fullscreen = FULLSCREEN;
			else if (html_elm->IsAncestorOf(fullscreen_elm))
				m_packed.fullscreen = FULLSCREEN_ANCESTOR;
		}
#endif // DOM_FULLSCREEN_MODE
}

void
CSS_MatchContextElm::RetrieveClassAttribute()
{
	OP_ASSERT(m_packed.retrieved_class == 0);

	m_packed.retrieved_class = 1;
	m_class_attr = m_html_elm->HasClass() ? m_html_elm->GetClassAttribute() : NULL;
}

void
CSS_MatchContextElm::RetrieveIdAttribute()
{
	OP_ASSERT(m_packed.retrieved_id == 0);

	m_packed.retrieved_id = 1;
	m_id_attr = m_html_elm->GetId();
}

void
CSS_MatchContextElm::ResolveLinkState(FramesDocument* doc)
{
	OP_ASSERT(!IsLinkStateResolved());

	URL anchor_url = m_html_elm->GetAnchor_URL(doc);
	if (anchor_url.IsEmpty())
		m_packed.link_state = NO_LINK;
	else if (m_packed.link_state == UNRESOLVED)
		m_packed.link_state = anchor_url.GetAttribute(URL::KIsFollowed, URL::KFollowRedirect) ? VISITED : LINK;
	else if (m_packed.link_state == UNRESOLVED_NO_VISITED)
		m_packed.link_state = LINK;
	else
		m_packed.link_state = FramesDocument::IsLinkVisited(doc, anchor_url, m_packed.link_state) ? VISITED : LINK;
}

void
CSS_MatchContextElm::RetrieveDynamicState()
{
	OP_ASSERT(m_packed.retrieved_dynamic_state == 0);

	m_packed.retrieved_dynamic_state = 1;
	m_packed.hovered = m_html_elm->IsHovered();
	m_packed.activated = m_html_elm->IsActivated();
	m_packed.focused = m_html_elm->IsFocused();
}

HTML_Element*
CSS_MatchContext::Operation::Next(HTML_Element* current, BOOL skip_children) const
{
	OP_ASSERT(m_mode == TOP_DOWN);

	HTML_Element* next = skip_children ? NULL : current->FirstChild();
	if (!next)
	{
		for (next = current; next; next = next->Parent())
		{
			m_context->CommitLeaf(next);
			if (next->Suc())
			{
				next = next->Suc();
				break;
			}
		}
	}
	return next;
}

CSS_MatchContext::CSS_MatchContext()
  : m_pool(NULL),
	m_sibling(NULL),
	m_current_leaf(NULL),
	m_current_root(NULL),
	m_next(0),
	m_size(0),
	m_doc(NULL),
	m_fullscreen_elm(NULL)
#ifdef STYLE_GETMATCHINGRULES_API
	, m_listener(NULL)
#endif // STYLE_GETMATCHINGRULES_API
{
	m_packed_init = 0;
	m_packed.visited_links_state = CSS_MatchContextElm::UNRESOLVED;
#ifdef CSS_TRANSITIONS
	m_cascade_elm[COMPUTED_OLD] = NULL;
	m_cascade_elm[COMPUTED_NEW] = NULL;
#endif // CSS_TRANSITIONS
}

CSS_MatchContext::~CSS_MatchContext()
{
	/* Check that we're in a clean state before deleting. */
	OP_ASSERT(m_next == 0);
	OP_DELETEA(m_sibling);
}

void
CSS_MatchContext::ConstructL(size_t pool_size)
{
	OP_ASSERT(m_pool == NULL && m_size == 0 && m_next == 0);

	/* Allocate a context element pool array which includes the
	   single sibling context element at the end. */

	m_sibling = OP_NEWA_L(CSS_MatchContextElm, pool_size+1);
	m_pool = &m_sibling[1];
	m_size = pool_size;
}

void CSS_MatchContext::Begin(FramesDocument* doc, Mode mode, CSS_MediaType media_type, BOOL match_all, BOOL mark_pseudo_bits)
{
	OP_ASSERT(m_packed.mode == INACTIVE && IsCommitted());

	m_doc = doc;
#ifdef DOM_FULLSCREEN_MODE
	m_fullscreen_elm = doc->GetFullscreenElement();
#endif // DOM_FULLSCREEN_MODE

	m_packed.mode = mode;
	m_packed.layout_mode = doc->GetLayoutMode();
	m_packed.xml = !!doc->GetHLDocProfile()->IsXml();
	m_packed.strict_mode = !!doc->GetHLDocProfile()->IsInStrictMode();
	m_packed.pseudo_elm = ELM_NOT_PSEUDO;
	m_packed.match_all = !!match_all;
	m_packed.mark_pseudo_bits = !!mark_pseudo_bits;
	m_packed.prefetch_mode = 0;

	m_sibling->Reset();
	m_nth_cache.Reset();

	/* Set up various flags and retrieve stylesheet list. */

	const uni_char* hostname = doc->GetURL().GetAttribute(URL::KUniHostName).CStr();
	HLDocProfile* hld_prof = doc->GetHLDocProfile();
	LayoutMode layout_mode = doc->GetWindow()->GetLayoutMode();
	BOOL is_ssr = layout_mode == LAYOUT_SSR;

	m_packed.apply_author = (is_ssr && hld_prof->HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD) && hld_prof->GetCSSMode() == CSS_FULL ||
							 !is_ssr && g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(hld_prof->GetCSSMode(), PrefsCollectionDisplay::EnableAuthorCSS), hostname)) || doc->GetWindow()->IsCustomWindow();

	int apply_doc_styles = APPLY_DOC_STYLES_YES;
	if (m_packed.layout_mode != LAYOUT_NORMAL)
		apply_doc_styles = g_pcdisplay->GetIntegerPref(g_pcdisplay->GetPrefFor(doc->GetPrefsRenderingMode(), PrefsCollectionDisplay::DownloadAndApplyDocumentStyleSheets));

	m_packed.apply_partially = (apply_doc_styles == APPLY_DOC_STYLES_SOME);

	m_media_type = media_type;
}

OP_STATUS
CSS_MatchContext::InitTopDownRoot(HTML_Element* root)
{
	/* Make sure we've initiated a TOP_DOWN operation with Begin(),
	   but haven't added any leaves yet. */
	OP_ASSERT(m_packed.mode == TOP_DOWN && m_next == 0 && !m_current_leaf);

	if (root && root->Type() != Markup::HTE_DOC_ROOT)
	{
		m_current_leaf = InitTopDownRootInternal(root);
		if (!m_current_leaf)
			return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

CSS_MatchContextElm*
CSS_MatchContext::InitTopDownRootInternal(HTML_Element* html_elm)
{
	CSS_MatchContextElm* parent_context_elm = NULL;
	HTML_Element* parent = html_elm->ParentActualStyle();
	if (parent && parent->Type() != Markup::HTE_DOC_ROOT)
	{
		parent_context_elm = InitTopDownRootInternal(parent);
		if (!parent_context_elm)
			return NULL;
	}

	CSS_MatchContextElm* context_elm = New();
	if (context_elm)
	{
		context_elm->Init(parent_context_elm, NULL, html_elm, m_fullscreen_elm, LinkState());
		if (!parent_context_elm)
			context_elm->SetIsRoot();
	}
	return context_elm;
}

CSS_MatchContextElm*
CSS_MatchContext::CreateParentElm(CSS_MatchContextElm* child)
{
	CSS_MatchContextElm* context_child = child == m_sibling ? m_current_root : child;

	OP_ASSERT(context_child && context_child == m_current_root);

	HTML_Element* parent_he = context_child->HtmlElement()->ParentActualStyle();

	if (parent_he && parent_he->Type() != Markup::HTE_DOC_ROOT)
	{
		CSS_MatchContextElm* parent = New();
		if (parent)
		{
			parent->Init(NULL, context_child, parent_he, m_fullscreen_elm, LinkState());

			m_current_root = parent;

			if (child != context_child)
			{
				/* child is the sibling. Copy the parent context element pointer
				   from the true context element so that we can navigate to the
				   parent using GetParent(). */

				child->SetParent(parent);
			}
		}
		return parent;
	}
	else
	{
		context_child->SetIsRoot();
		return NULL;
	}
}

unsigned int
CSS_MatchContext::CacheNthCount(const CSS_MatchContextElm* context_elm, CSS_NthCache::Type type, BOOL case_sensitive)
{
	HTML_Element* old_elm = m_nth_cache.m_elements[type];
	HTML_Element* html_elm = context_elm->HtmlElement();

	/* Not necessary to find a value that's already cached. */
	OP_ASSERT(m_nth_cache.m_elements[type] != html_elm);

	BOOL is_sibling = context_elm == m_sibling;

	Markup::Type elm_type = Markup::HTE_UNKNOWN;
	NS_Type ns_type = NS_NONE;
	const uni_char* elm_name = NULL;

	if (static_cast<int>(type) <= CSS_NthCache::NTH_LAST_OF_TYPE)
	{
		elm_type = context_elm->Type();
		ns_type = context_elm->GetNsType();
		if (elm_type == Markup::HTE_UNKNOWN)
			elm_name = html_elm->GetTagName();
	}

	/* Don't store sibling elements in the cache, their position relative
	   to the next element queried for would not be predictable (could be
	   both before and after). Don't even use the previously cached value.
	   We'll just always count and return instead. */

	if (is_sibling)
		old_elm = NULL;
	else
	{
		m_nth_cache.m_elements[type] = html_elm;
		if (static_cast<int>(type) < CSS_NthCache::NTH_TYPE_ARRAY_SIZE)
		{
			m_nth_cache.m_types[type] = context_elm->Type();
			m_nth_cache.m_ns_types[type] = context_elm->GetNsType();
		}

		/* The previously cached element (old_elm) is used for faster computation of the nth-count
		   for html_elm when they have the same parent, and they share the same type for "of-type" counts.
		   The assumption is that old_elm precedes html_elm because of traversal order when matching. */

		if (old_elm)
			if (old_elm->ParentActualStyle() != html_elm->ParentActualStyle())
				/* Don't use the cached value if the elements don't have the same parent. */
				old_elm = NULL;
			else if (static_cast<int>(type) <= CSS_NthCache::NTH_LAST_OF_TYPE)
			{
				/* Don't use the cached value if the elements don't have the same type for of-type counts. */
				if (elm_type == Markup::HTE_UNKNOWN || !old_elm->IsMatchingType(elm_type, ns_type))
					old_elm = NULL;
			}
	}

	/* If the previously cached element have the same parent (and same type),
	   start with the cached count and add the distance between them. */
	unsigned int count = old_elm ? m_nth_cache.m_counts[type] : 0;

	switch (type)
	{
	case CSS_NthCache::NTH_CHILD:
		{
			/* Count the number of elements from this element back to
			   the previously cached element, or back to start. */
			while (html_elm && html_elm != old_elm)
			{
				if (Markup::IsRealElement(html_elm->Type()))
					count++;
				html_elm = html_elm->PredActualStyle();
			}

			/* If we've tried to count relative to old_elm, we should have
			   stopped at that elm, otherwise both old_elm and html_elm are
			   NULL. */
			OP_ASSERT(html_elm == old_elm);
		}
		break;

	case CSS_NthCache::NTH_LAST_CHILD:
		{
			if (old_elm)
			{
				/* Count the number of elements from this element to the previously cached one. */
				while (html_elm && html_elm != old_elm)
				{
					if (Markup::IsRealElement(html_elm->Type()))
					{
						/* The resulting count should always be 1 or higher. */
						OP_ASSERT(count > 1);
						--count;
					}
					html_elm = html_elm->PredActualStyle();
				}

				/* We tried to count relative to old_elm here.
				   Should've stopped at that element. */
				OP_ASSERT(html_elm && html_elm == old_elm);
			}
			else
				/* Count the number of elements from this element to the end. */
				while (html_elm)
				{
					if (Markup::IsRealElement(html_elm->Type()))
						count++;
					html_elm = html_elm->SucActualStyle();
				}
		}
		break;

	case CSS_NthCache::NTH_OF_TYPE:
		{
			/* Count the number of elements of the given type from this element back to
			   the previously cached element, or back to start. */
			while (html_elm && html_elm != old_elm)
			{
				if (html_elm == context_elm->HtmlElement() ||
					Markup::IsRealElement(html_elm->Type()) && MatchElmType(html_elm, elm_type, ns_type, elm_name, case_sensitive))
					count++;
				html_elm = html_elm->PredActualStyle();
			}

			/* If we've tried to count relative to old_elm, we should have
			   stopped at that elm, otherwise both old_elm and html_elm are
			   NULL. */
			OP_ASSERT(html_elm == old_elm);
		}
		break;

	case CSS_NthCache::NTH_LAST_OF_TYPE:
		{
			if (old_elm)
			{
				/* Count the number of elements of the given type from this element to the previously cached one. */
				while (html_elm && html_elm != old_elm)
				{
					if (Markup::IsRealElement(html_elm->Type()) && MatchElmType(html_elm, elm_type, ns_type, elm_name, case_sensitive))
					{
						/* The resulting count should always be 1 or higher. */
						OP_ASSERT(count > 1);
						--count;
					}
					html_elm = html_elm->PredActualStyle();
				}

				/* We tried to count relative to old_elm here.
				   Should've stopped at that element. */
				OP_ASSERT(html_elm && html_elm == old_elm);
			}
			else
				/* Count the number of elements of the given type from this element to the end. */
				while (html_elm)
				{
					if (html_elm == context_elm->HtmlElement() ||
						Markup::IsRealElement(html_elm->Type()) && MatchElmType(html_elm, elm_type, ns_type, elm_name, case_sensitive))
						count++;
					html_elm = html_elm->SucActualStyle();
				}
		}
		break;
	}

	OP_ASSERT(count > 0);
	if (!is_sibling)
		m_nth_cache.m_counts[type] = count;

	return count;
}

void
CSS_MatchContext::InitVisitedLinkPref()
{
	m_packed.visited_links_state = g_pcdisplay ? (CSS_MatchContextElm::LinkState)g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinksState) : CSS_MatchContextElm::UNRESOLVED;
	OP_ASSERT(m_packed.visited_links_state <= CSS_MatchContextElm::UNRESOLVED);
}

#ifdef CSS_TRANSITIONS

OP_STATUS
CSS_MatchContext::CalculateComputedStyle(ComputedStyle which, BOOL ignore_transitions)
{
	OP_STATUS stat = OpStatus::OK;

#ifdef _DEBUG
	/* We create the computed styles while traversing the tree for
	   reloading CSS properties. It is not a problem that the child
	   props are still dirty. In fact they will often be. */

	m_doc->GetLogicalDocument()->GetLayoutWorkplace()->SetAllowDirtyChildPropsDebug(TRUE);
#endif // _DEBUG

	HTML_Element* elm = m_current_leaf->HtmlElement();

	if (m_cascade_elm[which] && m_cascade_elm[which]->html_element == m_current_leaf->HtmlElement())
	{
		/* Recompute style in case we have already done so. Used
		   for CSS Animations that given computed style for an
		   element may add declarations and require recomputing
		   computed style. */

		CleanComputedStyles();

		if (!m_cascade_elm[which])
			/* If we cleaned all the way to the top, recreate the
			   style roots. */

			stat = ConstructComputedStyleRoots();
	}
	else
		if (!m_cascade_elm[which] || m_current_leaf->Parent() && m_current_leaf->Parent()->HtmlElement() != m_cascade_elm[which]->html_element)
			stat = ConstructComputedStyleRoots();

	if (stat == OpStatus::OK)
	{
		OP_ASSERT(!m_current_leaf->Parent() || m_cascade_elm[which]->html_element == m_current_leaf->Parent()->HtmlElement());
		m_cascade_elm[which] = m_cascade_elm[which]->GetChildPropertiesForTransitions(m_doc->GetHLDocProfile(), elm, ignore_transitions);
		if (!m_cascade_elm[which])
			stat = OpStatus::ERR_NO_MEMORY;
		else
		{
			if (which == COMPUTED_OLD)
				m_cascade_elm[which]->ReferenceDeclarations();
			if (m_cascade_elm[which]->GetProps()->display_type == CSS_VALUE_none)
				m_current_leaf->SetDisplayNone();
			m_current_leaf->SetHasComputedStyles();
		}
	}

#ifdef _DEBUG
	m_doc->GetLogicalDocument()->GetLayoutWorkplace()->SetAllowDirtyChildPropsDebug(FALSE);
#endif // _DEBUG

	return stat;
}

OP_STATUS
CSS_MatchContext::CompleteComputedStyleChain(CSS_MatchContextElm* parent_elm)
{
	HLDocProfile* hld_profile = m_doc->GetHLDocProfile();

	if (parent_elm)
	{
		if (m_cascade_elm[COMPUTED_OLD] && m_cascade_elm[COMPUTED_OLD]->html_element == parent_elm->HtmlElement())
			return OpStatus::OK;

		RETURN_IF_ERROR(CompleteComputedStyleChain(parent_elm->Parent()));

		m_cascade_elm[COMPUTED_OLD] = m_cascade_elm[COMPUTED_OLD]->GetChildPropertiesForTransitions(hld_profile, parent_elm->HtmlElement(), FALSE);
		m_cascade_elm[COMPUTED_NEW] = m_cascade_elm[COMPUTED_NEW]->GetChildPropertiesForTransitions(hld_profile, parent_elm->HtmlElement(), FALSE);

		if (!m_cascade_elm[COMPUTED_OLD] || !m_cascade_elm[COMPUTED_NEW])
			return OpStatus::ERR_NO_MEMORY;

		if (m_cascade_elm[COMPUTED_OLD]->GetProps()->display_type == CSS_VALUE_none)
			parent_elm->SetDisplayNone();

		return OpStatus::OK;
	}
	else
	{
		/* If we've reached the root, we either have initial styles, or we need to create them. */
		if (m_cascade_elm[COMPUTED_OLD])
		{
			OP_ASSERT(m_cascade_elm[COMPUTED_NEW] &&
					  m_cascade_elm[COMPUTED_OLD]->html_element == NULL &&
					  m_cascade_elm[COMPUTED_NEW]->html_element == NULL);
			return OpStatus::OK;
		}

		OP_ASSERT(m_cascade_elm[COMPUTED_NEW] == NULL &&
					m_cascade[COMPUTED_OLD].Empty() &&
					m_cascade[COMPUTED_NEW].Empty());

		/* Cascade element for old initial values. */
		LayoutProperties* old_root_cascade = new LayoutProperties();
		if (!old_root_cascade)
			return OpStatus::ERR_NO_MEMORY;
		old_root_cascade->IntoStart(&m_cascade[COMPUTED_OLD]);
		m_cascade_elm[COMPUTED_OLD] = old_root_cascade;

		/* Cascade element for new initial values. */
		LayoutProperties* new_root_cascade = new LayoutProperties();
		if (!new_root_cascade)
			return OpStatus::ERR_NO_MEMORY;
		new_root_cascade->Into(&m_cascade[COMPUTED_NEW]);
		m_cascade_elm[COMPUTED_NEW] = new_root_cascade;

		old_root_cascade->GetProps()->Reset(NULL, hld_profile);
		new_root_cascade->GetProps()->Reset(NULL, hld_profile);

		return OpStatus::OK;
	}
}

OP_STATUS
CSS_MatchContext::ConstructComputedStyleRoots()
{
	VisualDevice* vis_dev = m_doc->GetVisualDevice();

	VDState vd_state = vis_dev->PushState();
	vis_dev->Reset();

	OP_STATUS stat = CSS_MatchContext::CompleteComputedStyleChain(m_current_leaf->Parent());

	vis_dev->PopState(vd_state);

	return stat;
}

void
CSS_MatchContext::CleanComputedStyles()
{
	if (!m_cascade_elm[COMPUTED_OLD] || m_cascade_elm[COMPUTED_OLD]->html_element != m_current_leaf->HtmlElement())
		return;

	OP_ASSERT(m_cascade_elm[COMPUTED_OLD] &&
			  m_cascade_elm[COMPUTED_NEW] &&
			  m_current_leaf &&
			  m_cascade_elm[COMPUTED_OLD]->html_element == m_current_leaf->HtmlElement() &&
			  m_cascade_elm[COMPUTED_NEW]->html_element == m_current_leaf->HtmlElement());

	/* No need to dereference declarations in the part of the chain that was
	   created from CompleteComputedStyleChain because they were never
	   referenced due to the fact that we knew old and new would be the same,
	   hence present and referenced in CSSPROPS for the duration of this match
	   operation. */
	if (m_current_leaf->HasComputedStyles())
		m_cascade_elm[COMPUTED_OLD]->DereferenceDeclarations();

	for (unsigned int i = 0; i < COMPUTED_COUNT; i++)
	{
		m_cascade_elm[i] = m_cascade_elm[i]->Pred();
		if (!m_cascade_elm[i]->html_element)
		{
			m_cascade[i].Clear();
			m_cascade_elm[i] = NULL;
		}
		else
			m_cascade_elm[i]->CleanSuc();
	}

	OP_ASSERT(m_cascade[COMPUTED_OLD].Empty() == m_cascade[COMPUTED_NEW].Empty());
}

#endif // CSS_TRANSITIONS
