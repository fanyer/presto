/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/layout/cascade.h"

#include "modules/layout/content_generator.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/box/blockbox.h"
#include "modules/layout/box/flexitem.h"
#include "modules/layout/box/inline.h"
#include "modules/layout/box/tables.h"
#include "modules/layout/content/flexcontent.h"
#include "modules/layout/traverse/traverse.h"
#include "modules/url/url2.h"
#include "modules/probetools/probepoints.h"
#include "modules/style/css_media.h"
#include "modules/style/css_matchcontext.h"
#include "modules/logdoc/urlimgcontprov.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/logdoc/selection.h"
#include "modules/img/image.h"
#include "modules/viewers/viewers.h"
#include "modules/forms/formvalue.h"
#include "modules/locale/locale-enum.h"

#ifdef MEDIA_SUPPORT
#include "modules/media/media.h"
#include "modules/media/mediaelement.h"
#endif //MEDIA_SUPPORT

#include "modules/dochand/win.h"

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_print.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/locale/oplanguagemanager.h"

#ifdef SVG_SUPPORT
# include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

#ifdef _PLUGIN_SUPPORT_
#include "modules/ns4plugins/opns4pluginhandler.h"
#endif // _PLUGIN_SUPPORT_

#ifdef JS_PLUGIN_ATVEF_VISUAL
# include "modules/jsplugins/src/js_plugin_object.h"
# include "modules/dom/domutils.h"
#endif

#ifdef SKIN_SUPPORT
# include "modules/skin/OpSkinManager.h"
#endif // SKIN_SUPPORT

#ifdef URL_FILTER
# include "modules/dom/src/extensions/domextensionurlfilter.h"
#endif // URL_FILTER

static inline void
SetNewLayoutBox(HTML_Element* elm, Box* new_box)
{
	/* The old layout box should have been deleted/removed at this point. */

	OP_ASSERT(!elm->GetLayoutBox());
	OP_DELETE(elm->GetLayoutBox());
	elm->SetLayoutBox(new_box);
}

/** Return TRUE if the specified element needs a table-cell parent inserted.

	The code assumes that someone else has already verified that the
	element is inside of some sort of table structure (or it wouldn't
	need a table-cell parent in the first place). */

static BOOL
NeedsTableCellParent(LayoutInfo& info, HTML_Element* html_element, CSSValue white_space)
{
	Markup::Type element_type = html_element->Type();

	if (element_type == Markup::HTE_TEXT)
		return TextRequiresLayout(info.doc, html_element->TextContent(), white_space);
	else
		if (element_type == Markup::HTE_TEXTGROUP)
		{
			HTML_Element* end = html_element->NextSibling();

			for (HTML_Element* elm = html_element->FirstChild(); elm != end; elm = elm->Next())
				if (NeedsTableCellParent(info, elm, white_space))
					return TRUE;

			return FALSE;
		}
		else
			if (element_type == Markup::HTE_INPUT && html_element->GetInputType() == INPUT_HIDDEN)
				return FALSE;
			else
				return TRUE;
}

/** Return TRUE if the specified element needs a flexbox item parent inserted.

	The code assumes that someone else has already verified that the element is
	inside of a flexbox (or it wouldn't need a flexbox item parent in the first
	place). */

static BOOL
NeedsFlexItemParent(LayoutInfo& info, LayoutProperties* cascade)
{
	HTML_Element* html_element = cascade->html_element;
	Markup::Type element_type = html_element->Type();
	const HTMLayoutProperties& props = *cascade->GetProps();

	switch (element_type)
	{
		/* Text nodes with only whitespace (regardless of what the the
		   'white-space' property says) are not to be rendered. */

	case Markup::HTE_TEXT:
		return TextRequiresLayout(info.doc, html_element->TextContent(), CSS_VALUE_normal);

	case Markup::HTE_TEXTGROUP:
		{
			HTML_Element* end = html_element->NextSibling();

			for (HTML_Element* child = html_element->FirstChild(); child != end; child = child->Next())
				if (child->Type() == Markup::HTE_TEXT)
				{
					if (TextRequiresLayout(info.doc, child->TextContent(), CSS_VALUE_normal))
						return TRUE;
				}
				else
					return TRUE;

			return FALSE;
		}

	case Markup::HTE_BR:
	case Markup::HTE_WBR:
		return TRUE;

	default:
		if (element_type == Markup::HTE_INPUT && html_element->GetInputType() == INPUT_HIDDEN)
			return FALSE; // Hidden input will never generate a layout box.

		return props.position == CSS_VALUE_absolute || props.position == CSS_VALUE_fixed;
	}
}

#define EmptyStr UNI_L("");
#define DefaultQuote UNI_L("\"");

/* This pool has two purposes: First, it takes some load off the malloc,
   because these elements are large and large elements can be hard
   to allocate.  Second, because there are very few such elements but
   they are heavily used, it provides them with improved locality on
   systems with tiny caches and poor mallocs (Nokia/Symbian type things). */

void*
LayoutProperties::operator new(size_t nbytes) OP_NOTHROW
{
	return g_layout_properties_pool->New(sizeof(LayoutProperties));
}

void
LayoutProperties::operator delete(void* p, size_t nbytes)
{
	g_layout_properties_pool->Delete(p);
}

/** Get open or close quotation mark at given level. */

const uni_char*
LayoutProperties::GetQuote(unsigned int quote_level, BOOL open_quote)
{
	if (props.quotes_cp)
	{
		if (props.quotes_cp->GetDeclType() == CSS_DECL_TYPE)
		{
			if (props.quotes_cp->TypeValue() == CSS_VALUE_none)
				return EmptyStr;
		}
		else if (props.quotes_cp->GetDeclType() == CSS_DECL_GEN_ARRAY)
		{
			unsigned arr_len = unsigned(props.quotes_cp->ArrayValueLen());

			OP_ASSERT(arr_len > 0 && arr_len % 2 == 0);

			unsigned int max_level = arr_len / 2 - 1;
			const CSS_generic_value* gen_arr = props.quotes_cp->GenArrayValue();

			if (quote_level > max_level)
				quote_level = max_level;

			unsigned int i = quote_level * 2;
			if (!open_quote)
				i++;

			OP_ASSERT(i < arr_len);

			if (gen_arr[i].value_type == CSS_STRING_LITERAL)
				return gen_arr[i].value.string;
			else
				return DefaultQuote;
		}
	}

	return DefaultQuote;
}

/** Determine whether page/column breaking inside this element should be avoided. */

void
LayoutProperties::GetAvoidBreakInside(const LayoutInfo& info, BOOL& avoid_page_break_inside, BOOL& avoid_column_break_inside)
{
	// First check with ancestors.

	if (container)
	{
		avoid_page_break_inside = container->AvoidPageBreakInside();

		if (container == multipane_container)
			/* Forget about ancestors and what they think about column
			   breaking. Whatever they say doesn't apply in this context. We
			   have a clean slate. */

			avoid_column_break_inside = FALSE;
		else
			avoid_column_break_inside = container->AvoidColumnBreakInside();
	}
	else
		if (table)
		{
			avoid_page_break_inside = table->AvoidPageBreakInside();
			avoid_column_break_inside = table->AvoidColumnBreakInside();
		}
		else
			if (flexbox)
			{
				avoid_page_break_inside = flexbox->AvoidPageBreakInside();
				avoid_column_break_inside = flexbox->AvoidColumnBreakInside();
			}
			else
			{
				avoid_page_break_inside = FALSE;
				avoid_column_break_inside = FALSE;
			}

	// Then check if this element wants to avoid breaking.

#ifdef PAGED_MEDIA_SUPPORT
	if (info.paged_media != PAGEBREAK_OFF)
		if (props.page_props.break_inside == CSS_VALUE_avoid || props.page_props.break_inside == CSS_VALUE_avoid_page)
			avoid_page_break_inside = TRUE;

	if (multipane_container && multipane_container->GetPagedContainer())
	{
		/* For paged containers 'avoid-page' means "avoid splitting
		   this over multiple panes" in the columnizer. */

		if (props.page_props.break_inside == CSS_VALUE_avoid || props.page_props.break_inside == CSS_VALUE_avoid_page)
			avoid_column_break_inside = TRUE;
	}
	else
#endif // PAGED_MEDIA_SUPPORT
		if (props.page_props.break_inside == CSS_VALUE_avoid || props.page_props.break_inside == CSS_VALUE_avoid_column)
			avoid_column_break_inside = TRUE;
}

/** Check if table or table cell in cascade has a specified width. */

BOOL
LayoutProperties::HasTableSpecifiedWidth() const
{
	int column = 0;

	for (LayoutProperties* cascade = Pred(); cascade && cascade->html_element; cascade = cascade->Pred())
	{
		Box* box = cascade->html_element->GetLayoutBox();

		if (box)
			if (box->IsTableCell())
				column = ((TableCellBox*) box)->GetColumn();
			else
			{
				TableContent* table = box->GetTableContent();

				if (table && (table->HasColumnSpecifiedWidth(column) || cascade->GetProps()->content_width != CONTENT_WIDTH_AUTO))
					return TRUE;
			}
	}

	return FALSE;
}

Image
LayoutProperties::GetListMarkerImage(FramesDocument* doc
#ifdef SVG_SUPPORT
								     , SVGImage*& svg_image
#endif // SVG_SUPPORT
								     )
{
	Image img;

	OP_ASSERT(html_element->GetIsListMarkerPseudoElement());

	if (props.list_style_image && doc->GetShowImages())
	{
#ifdef SKIN_SUPPORT
		if (props.list_style_image.IsSkinURL())
		{
			char name8[120]; /* ARRAY OK 2011-06-06 pdamek */
			uni_cstrlcpy(name8, props.list_style_image+2, ARRAY_SIZE(name8));
			OpSkinElement* skin_elm = g_skin_manager->GetSkinElement(name8);

			if (skin_elm)
				img = skin_elm->GetImage(0);
		}
		else
#endif // SKIN_SUPPORT
		{
			OP_ASSERT(html_element->Parent());
			HEListElm* helm = html_element->Parent()->GetHEListElm(FALSE);
			if (helm)
			{
				img = helm->GetImage();
#ifdef SVG_SUPPORT
				UrlImageContentProvider* content_provider = helm->GetUrlContentProvider();
				if (content_provider)
					svg_image = content_provider->GetSVGImageRef() ? content_provider->GetSVGImageRef()->GetSVGImage() : NULL;
#endif // SVG_SUPPORT
			}
		}
	}

	return img;
}

/** Find active table row group for this element. */

TableRowGroupBox*
LayoutProperties::FindTableRowGroup() const
{
	for (LayoutProperties* cascade = Pred(); cascade && cascade->html_element; cascade = cascade->Pred())
	{
		Box* box = cascade->html_element->GetLayoutBox();

		if (box)
			if (box->IsTableRowGroup())
				return (TableRowGroupBox*) box;
			else
				if (box->GetTableContent())
					return NULL;
	}

	return NULL;
}

/** Find active parent for this element. */

HTML_Element*
LayoutProperties::FindParent() const
{
	for (LayoutProperties* cascade = Pred(); cascade; cascade = cascade->Pred())
		if (cascade->html_element && !cascade->html_element->GetLayoutBox()->IsNoDisplayBox())
			return cascade->html_element;

	return NULL;
}

/** Helper function for finding out whether a html element establishes
	a containing block for fixed positioned elements.

	Normally only the root does this, i.e. the ancestor without a
	parent, but for CSS transforms also transformed elements
	establishes a containing block for fixed positioned elements. */

static inline BOOL IsContainingBlockForFixedPos(HTML_Element *html_element)
{
#ifdef CSS_TRANSFORMS
	if (Box* box = html_element->GetLayoutBox())
		return (!html_element->Parent() || box->GetTransformContext());
	else
		return FALSE;
#else
	return !html_element->Parent();
#endif
}

/** Find containing element for this absolute positioned element. */

HTML_Element*
LayoutProperties::FindContainingElementForAbsPos(BOOL fixed) const
{
	for (LayoutProperties* cascade = Pred(); cascade && cascade->html_element; cascade = cascade->Pred())
	{
		if (fixed)
		{
			if (IsContainingBlockForFixedPos(cascade->html_element))
				return cascade->html_element;
		}
		else
			if (Box* box = cascade->html_element->GetLayoutBox())
				if (box->IsPositionedBox())
					return cascade->html_element;
	}

	return NULL;
}

/** Find the nearest multi-pane container. */

Container*
LayoutProperties::FindMultiPaneContainer(BOOL find_paged) const
{
	if (multipane_container)
	{
#ifdef PAGED_MEDIA_SUPPORT
		if (find_paged)
		{
			if (multipane_container->GetPagedContainer())
				return multipane_container;
		}
		else
#endif // PAGED_MEDIA_SUPPORT
			if (multipane_container->GetMultiColumnContainer())
				return multipane_container;

		const LayoutProperties* cascade = this;

		while (cascade->html_element->GetLayoutBox() != multipane_container->GetPlaceholder())
		{
			if (!cascade->html_element->GetLayoutBox()->IsColumnizable())
				return NULL;

			cascade = cascade->Pred();
		}

		return cascade->FindMultiPaneContainer(find_paged);
	}

	return NULL;
}

/** Find the offsetParent of an element. */

LayoutProperties*
LayoutProperties::FindOffsetParent(HLDocProfile *hld_profile)
{
	/* We define offsetParent of an HTMLElement node A like this (attempting to
	 * copy MSIE's behavior as much as reasonable):
	 *
	 * If node A implements the HTMLHtmlElement or HTMLBodyElement interface or
	 * its computed value of the position property is fixed, its offsetParent
	 * attribute must be null.
	 *
	 * Otherwise, the offsetParent attribute of node A must be the nearest
	 * ancestor node for which at least one of the following is true:
	 * - The computed value of the position property of the ancestor node is
	 *   different from static.
	 * - The ancestor node implements the HTMLBodyElement interface.
	 * - The computed value of the position property of node A is static and
	 *   the ancestor node implements one of the following interfaces:
	 *     HTMLTableCellElement
	 *     HTMLTableElement
	 *
	 * If no such ancestor is found, offsetParent must be null.
	 *
	 * An up-to-date version of the complete offset* spec can probably be found
	 * here: http://dev.w3.org/csswg/cssom-view/#offset-attributes
	 */

	Markup::Type type = html_element->Type();

	if (!html_element->GetLayoutBox() ||
		html_element->GetNsType() == NS_HTML && (type == Markup::HTE_BODY || type == Markup::HTE_HTML) ||
		props.position == CSS_VALUE_fixed)
		return 0;

	for (LayoutProperties* cascade = Pred(); cascade && cascade->html_element; cascade = cascade->Pred())
	{
		HTML_Element *elm = cascade->html_element;
		Markup::Type ptype = elm->Type();

		if (ptype == Markup::HTE_DOC_ROOT)
			return 0;

		if (cascade->GetProps()->position != CSS_VALUE_static)
			return cascade;

		if (ptype == Markup::HTE_BODY && elm->GetNsType() == NS_HTML)
			return cascade;
		else
			if (props.position == CSS_VALUE_static && elm->GetInserted() != HE_INSERTED_BY_LAYOUT && elm->GetNsType() == NS_HTML)
				switch (ptype)
				{
				case Markup::HTE_TABLE:
				case Markup::HTE_TD:
				case Markup::HTE_TH:
					return cascade;
				}
	}

	return 0;
}

/** Get the layout properties for child. */

LayoutProperties*
LayoutProperties::GetChildCascade(LayoutInfo& info, HTML_Element* child, BOOL invert_run_in)
{
	LayoutProperties* layout_props = Suc();

	if (!layout_props)
	{
		// Append new property to end of cascade

		layout_props = new LayoutProperties();

		if (!layout_props)
			return NULL;

		layout_props->Follow(this);
	}
	else
		if (layout_props->html_element != child)
			if (OpStatus::IsMemoryError(layout_props->Finish(&info)))
				return NULL;

	LayoutProperties* parent = this;

	layout_props->html_element = child;

	if (info.doc && !info.doc->IsWaitingForStyles())
		if (!layout_props->Inherit(info.hld_profile, parent, invert_run_in ? INVERT_RUN_IN : NOFLAGS))
			return NULL;

	return layout_props;
}

LayoutProperties*
LayoutProperties::CloneCascade(Head& prop_list, HLDocProfile* hld_profile) const
{
	if (CloneCascadeInternal(prop_list, this, hld_profile) == OpStatus::ERR_NO_MEMORY)
		return NULL;

	OP_ASSERT(prop_list.Last());

	return (LayoutProperties*) prop_list.Last();
}

/* static */ OP_STATUS
LayoutProperties::CloneCascadeInternal(Head& prop_list, const LayoutProperties* old_layout_props, HLDocProfile* hld_profile)
{
	if (!old_layout_props->html_element)
	{
		OP_ASSERT(!old_layout_props->Pred());
		return OpStatus::OK;
	}

	if (old_layout_props->Pred())
		RETURN_IF_MEMORY_ERROR(CloneCascadeInternal(prop_list, old_layout_props->Pred(), hld_profile));

	HTML_Element* html_element = old_layout_props->html_element;
	LayoutProperties* layout_props = new LayoutProperties();

	if (!layout_props)
		return OpStatus::ERR_NO_MEMORY;

	layout_props->Into(&prop_list);
	layout_props->html_element = html_element;

	// Just copy everything from the old properties

	if (!layout_props->props.Copy(old_layout_props->props))
		return OpStatus::ERR_NO_MEMORY;

	if (old_layout_props->cascading_props)
		if (layout_props->WantToModifyProperties())
		{
			if (!layout_props->cascading_props->Copy(*old_layout_props->cascading_props))
				return OpStatus::ERR_NO_MEMORY;
		}
		else
			return OpStatus::ERR_NO_MEMORY;

	layout_props->container = old_layout_props->container;
	layout_props->multipane_container = old_layout_props->multipane_container;
	layout_props->table = old_layout_props->table;
	layout_props->space_manager = old_layout_props->space_manager;
	layout_props->stacking_context = old_layout_props->stacking_context;

	return OpStatus::OK;
}

BOOL
LayoutProperties::Inherit(HLDocProfile* hld_profile, LayoutProperties* parent, int flags)
{
	OP_PROBE6(OP_PROBE_DOC_LAYOUTPROPERTIES_INHERIT);

	op_yield();

	if (html_element->Type() == Markup::HTE_TEXT)
	{
		if (!parent->html_element)
			return FALSE; //OOM

		Box* parent_layout_box = parent->html_element->GetLayoutBox();

		multipane_container = NULL;

		if (parent_layout_box)
		{
			container = parent_layout_box->GetContainer();

			if (container)
			{
				if (container->IsMultiPaneContainer())
					multipane_container = container;
			}
			else
				if (!parent_layout_box->GetTableContent() && !parent_layout_box->GetFlexContent())
					container = parent->container;

			flexbox = container ? NULL : parent_layout_box->GetFlexContent();
		}
		else
		{
			container = parent->container;
			flexbox = NULL;
		}

		if (!multipane_container)
			multipane_container = parent->multipane_container;

		table = NULL;
		space_manager = NULL;
		stacking_context = NULL;

		if (!props.Copy(parent->GetCascadingProperties()))
			return FALSE; //OOM

		props.bg_images.Reset();
		props.border_image.Reset();

		if (props.content_cp && !html_element->GetIsGeneratedContent())
			props.display_type = CSS_VALUE_none;

		props.SetDisplayProperties(hld_profile->GetVisualDevice());

		if (props.display_type != CSS_VALUE_none)
			props.display_type = CSS_VALUE_inline;
	}
	else
	{
#ifdef SVG_SUPPORT
		/* If this is an SVG foreignobject, this is the layout root, and certain members
		   must be initialized to NULL. However, SVG foreignobject elements may be
		   inserted by layout (e.g. pseudo element children of real SVG foreignobject
		   elements), in which case we need to behave as normally. */

		if (html_element->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG) &&
			html_element->GetInserted() != HE_INSERTED_BY_LAYOUT)
		{
			if (OpStatus::IsMemoryError(props.GetCssProperties(html_element, parent, hld_profile, NULL, !!(flags & IGNORE_TRANSITIONS))))
				return FALSE;

			delete cascading_props;
			cascading_props = NULL;

			container = NULL;
			multipane_container = NULL;
			table = NULL;
			flexbox = NULL;
			space_manager = NULL;
			stacking_context = NULL;

			if (!HTMLayoutProperties::AllocateSVGProps(props.svg, parent->GetProps()->svg))
				return FALSE;
		}
		else
#endif // SVG_SUPPORT
		{
			BOOL treat_as_inline_run_in = FALSE;

			if (Box* layout_box = html_element->GetLayoutBox())
			{
				treat_as_inline_run_in = layout_box->IsInlineRunInBox();

				if (flags & INVERT_RUN_IN)
					treat_as_inline_run_in = !treat_as_inline_run_in;
			}

			if (HTML_Element* inherit_element = treat_as_inline_run_in ? HTMLayoutProperties::FindInlineRunInContainingElm(html_element) : parent->html_element)
			{
				Box* inherit_layout_box = inherit_element->GetLayoutBox();

				multipane_container = NULL;

				if (inherit_layout_box)
				{
					space_manager = inherit_layout_box->GetLocalSpaceManager();
					stacking_context = inherit_layout_box->GetLocalStackingContext();
					table = inherit_layout_box->GetTableContent();
					flexbox = table ? NULL : inherit_layout_box->GetFlexContent();

					if (parent->GetProps()->display_type == CSS_VALUE_none || table || flexbox)
					{
#ifdef DEBUG
						OP_ASSERT(hld_profile->GetLayoutWorkplace());

						// let through CreateCascade for now. This means no users of a CreateCascade may rely on container in LayoutProperties or html_element->layout_box types. Dangerous.

						if (hld_profile->GetLayoutWorkplace()->IsReflowing() || hld_profile->GetLayoutWorkplace()->IsTraversing())
						{
							OP_ASSERT(!parent->html_element->GetLayoutBox() || table || flexbox);
						}
#endif
						container = NULL;
					}
					else
					{
						container = inherit_layout_box->GetContainer();

						if (container)
						{
							if (container->IsMultiPaneContainer())
								multipane_container = container;
						}
						else
							container = parent->container;

						table = parent->table;
					}

					if (!space_manager)
						space_manager = parent->space_manager;

					if (!stacking_context)
						stacking_context = parent->stacking_context;
				}
				else
				{
					if (parent->GetProps()->display_type == CSS_VALUE_none)
					{
#ifdef DEBUG
						OP_ASSERT(hld_profile->GetLayoutWorkplace());

						// let through CreateCascade for now. This means no users of a CreateCascade may rely on container in LayoutProperties or html_element->layout_box types. Dangerous.

						if (hld_profile->GetLayoutWorkplace()->IsReflowing() || hld_profile->GetLayoutWorkplace()->IsTraversing())
						{
							OP_ASSERT(!parent->html_element->GetLayoutBox());
						}
#endif
						container = NULL;
					}
					else
						container = parent->container;

					table = parent->table;
					flexbox = parent->flexbox;
					space_manager = parent->space_manager;
					stacking_context = parent->stacking_context;
				}

				if (!multipane_container)
					multipane_container = parent->multipane_container;
			}

			if (OpStatus::IsMemoryError(props.GetCssProperties(html_element, parent, hld_profile, parent->container == container ? NULL : container, !!(flags & IGNORE_TRANSITIONS))))
				return FALSE;

			delete cascading_props;
			cascading_props = NULL;
		}
	}

	if (!RectifyInvalidLayoutValues())
		return FALSE;

	return TRUE;
}

/** Get the layout properties for child. */

LayoutProperties*
LayoutProperties::GetChildProperties(HLDocProfile* hld_profile, HTML_Element* child)
{
	LayoutProperties* parent = this;

	if (child->Parent() != html_element)
	{
		Box* layout_box = child->GetLayoutBox();

		if (!layout_box || !layout_box->IsInlineRunInBox())
		{
			/* When traversing, a missing cascade element means that we
			   have a block inside an inline element. */

			parent = Suc();
			/* See if the succeeding cascade element is an ancestor of child and in that case
			   build the cascade from it instead. */
			if (parent && parent->html_element)
				for (HTML_Element* search = child->Parent(); search != html_element; search = search->Parent())
					if (search == parent->html_element)
						return parent->GetChildProperties(hld_profile, child);

			parent = GetChildProperties(hld_profile, child->Parent());

			if (!parent)
				return NULL;
		}
	}

	LayoutProperties* layout_props = parent->Suc();

	if (!layout_props)
	{
		// Append new property to end of cascade

		layout_props = new LayoutProperties();

		if (!layout_props)
			return NULL;
		layout_props->Follow(parent);
	}
	else if (layout_props->html_element && child != layout_props->html_element)
	{
		/* Otherwise we would have a cascade of layout_props->html_element bypassing
		   the child or we are inherting the props for child from a wrong cascade. */
		OP_ASSERT(!child->IsAncestorOf(layout_props->html_element));

		parent->CleanSuc();
	}

	if (!layout_props->html_element)
	{
		layout_props->html_element = child;

		if (!layout_props->Inherit(hld_profile, parent))
			return NULL;

#ifdef DOC_SPOTLIGHT_API
		if (hld_profile->GetFramesDocument()->GetSpotlightedElm() == child)
		{
			layout_props->GetProps()->SetIsInOutline(FALSE);
# if defined(SKIN_OUTLINE_SUPPORT)
			if (layout_props->WantToModifyProperties(TRUE))
			{
				HTMLayoutProperties* props = layout_props->GetProps();
				props->outline.style = CSS_VALUE__o_highlight_border;
				props->SetIsInOutline(TRUE);
				OpSkinElement* skin_elm_inside = g_skin_manager->GetSkinElement("Active Element Inside");
				if (skin_elm_inside)
				{
					INT32 border_width;
					skin_elm_inside->GetBorderWidth(&border_width, &border_width, &border_width, &border_width, 0);
					props->outline.width = border_width;
				}
			}
# endif // SKIN_OUTLINE_SUPPORT
		}
#endif // DOC_SPOTLIGHT_API
	}

	return layout_props;
}

LayoutProperties*
LayoutProperties::GetChildPropertiesForTransitions(HLDocProfile* hld_profile, HTML_Element* child, BOOL ignore_transitions)
{
	LayoutProperties* parent = this;
	LayoutProperties* layout_props = Suc();

	if (!layout_props)
	{
		layout_props = new LayoutProperties();
		if (!layout_props)
			return NULL;
		layout_props->Follow(parent);
	}

	OP_ASSERT(!layout_props->html_element);

	layout_props->html_element = child;

	int flags = ignore_transitions ? IGNORE_TRANSITIONS : NOFLAGS;
	if (!layout_props->Inherit(hld_profile, parent, flags))
	{
		layout_props->Clean();
		return NULL;
	}

	return layout_props;
}

/** Remove cascade from layout structure. */

void
LayoutProperties::RemoveCascade(LayoutInfo& info)
{
	if (html_element)
	{
		if (Suc())
			Suc()->RemoveCascade(info);

		Box* layout_box = html_element->GetLayoutBox();

		// Remove all reflow states

		if (layout_box)
			layout_box->CleanReflowState();
	}
}

/** Finish laying out boxes. */

OP_STATUS
LayoutProperties::Finish(LayoutInfo* info)
{
	if (html_element)
	{
		if (Suc())
		{
			RETURN_IF_ERROR(Suc()->Finish(info));

			if (!html_element)
				return OpStatus::OK; // already finished because of end of first line
		}

		if (Box* layout_box = html_element->GetLayoutBox())
		{
			switch (layout_box->FinishLayout(*info))
			{
			case LAYOUT_END_FIRST_LINE:
				// Do it again
				return Finish(info);

			case LAYOUT_OUT_OF_MEMORY:
				return OpStatus::ERR_NO_MEMORY;
			}
		}

		Clean();
	}

	return OpStatus::OK;
}

/** Clean succeeding LayoutProperties until one with html_element = NULL is reached. */

void
LayoutProperties::CleanSuc() const
{
	for (LayoutProperties* layout_props = Suc(); layout_props && layout_props->html_element; layout_props = layout_props->Suc())
		layout_props->Clean();
}

/** Remove first-line properties. */

void
LayoutProperties::RemoveFirstLineProperties()
{
	CleanSuc();
	use_first_line_props = FALSE;
	delete cascading_props;  // FIXME:REPORT:MEMMAN-KILSMO
	cascading_props = NULL;
}

/** We want to modify properties, make a copy for the cascade. */

BOOL
LayoutProperties::WantToModifyProperties(BOOL copy)
{
	if (cascading_props)
	{
		if (copy)
		{
			if (!cascading_props->Copy(props))
				return FALSE;
		}
		else
			cascading_props->Reset();
	}
	else
		if (copy)
		{
			cascading_props = new HTMLayoutProperties;
			if (!cascading_props || !cascading_props->Copy(props))
			{
				delete cascading_props;
				cascading_props = NULL;
			}
		}
		else
		{
			cascading_props = new HTMLayoutProperties;

			if (cascading_props)
				cascading_props->Reset();
		}

	return cascading_props != NULL;
}

LayoutProperties::AutoFirstLine::AutoFirstLine(HLDocProfile* hld_profile)
	: m_first_line(NULL)
	, m_hld_profile(hld_profile)
{
	OP_ASSERT(hld_profile);
}

LayoutProperties::AutoFirstLine::AutoFirstLine(HLDocProfile* hld_profile, HTML_Element* html_element)
	: m_first_line(NULL)
	, m_hld_profile(hld_profile)
{
	OP_ASSERT(hld_profile);

	Insert(html_element);
}

LayoutProperties::AutoFirstLine::~AutoFirstLine()
{
	if (m_first_line)
	{
		m_first_line->Out();
		m_first_line->Clean(m_hld_profile->GetFramesDocument());
	}
}

void
LayoutProperties::AutoFirstLine::Insert(HTML_Element* html_element)
{
	if (m_first_line)
	{
		OP_ASSERT(!"First-line element already inserted.");
		return;
	}

	m_first_line = g_anonymous_first_line_elm;
	m_first_line->SetType(html_element->Type());
	int ns_idx = html_element->GetNsIdx();
	m_first_line->SetNsIdx(ns_idx);
	g_ns_manager->GetElementAt(ns_idx)->IncRefCount();
	m_first_line->Precede(html_element);
}

/** Add first-line pseudo properties to cascade. */

OP_STATUS
LayoutProperties::AddFirstLineProperties(HLDocProfile* hld_profile)
{
	HTMLayoutProperties props;

	AutoFirstLine first_line(hld_profile, html_element);

	CSS_MediaType media_type = hld_profile->GetFramesDocument()->GetMediaType();
	RETURN_IF_ERROR(CssPropertyItem::LoadCssProperties(first_line.Elm(), g_op_time_info->GetRuntimeMS(), hld_profile, media_type));
	RETURN_IF_ERROR(props.GetCssProperties(first_line.Elm(), this, hld_profile, container, FALSE));

	if (!cascading_props->Copy(props))
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/** Skip over branch and terminate parents that might have
	terminators in this branch. */

BOOL
LayoutProperties::SkipBranch(LayoutInfo& info, BOOL skip_space_manager, BOOL skip_z_list)
{
	if (skip_space_manager)
		space_manager->SkipElement(info, html_element);

	Box* box = html_element->GetLayoutBox();

	if (skip_z_list)
	{
		StackingContext* local_stacking_context = box->GetLocalStackingContext();

		/* We have boxes which create a new stacking context without being the containing
		   block for absolute positioned descendants. In that case we need to skip the
		   branch. */

		if (local_stacking_context && !box->IsPositionedBox())
		{
			local_stacking_context->Restart();
			if (!local_stacking_context->SkipElement(html_element, info))
				return FALSE;
		}

		if (stacking_context && !stacking_context->SkipElement(html_element, info))
			return FALSE;
	}

	if (props.display_type == CSS_VALUE_list_item && box->IsBlockBox() && box->HasListMarkerBox() && container)
		/* Need to process the list number, since that affects the further list items
		   in the container. But only if the box of the list marker already exists.
		   Otherwise the number will be processed in BlockBox::FinishLayout or InlineBox::Layout. */

		container->GetNewListItemValue(html_element);

	if (multipane_container)
		multipane_container->SkipPaneFloats(html_element);

	return TRUE;
}

/** Create cascade for element. OBS! The user of this generated cascade may not
    rely on that the LayoutProperties::container or any box type of the html_elements
	in the cascade are of the correct type. */

/* static */ LayoutProperties*
LayoutProperties::CreateCascadeInternal(HTML_Element* html_element, Head& prop_list, HLDocProfile* hld_profile, BOOL dont_normalize)
{
	LayoutProperties* layout_props;

	OP_ASSERT(prop_list.Empty()); /* Previously, this method was able to re-use elements in
									 prop_list. I couldn't find it actually being used anywhere,
									 and with LayoutProperties pooling it shouldn't be necessary
									 either. So I removed the ability. */

	BOOL vd_state_pushed = FALSE;
	VDState vd_state;

	HTML_Element* parent = html_element ? html_element->Parent() : NULL;

	if (parent)
	{
		layout_props = LayoutProperties::CreateCascadeInternal(parent, prop_list, hld_profile, dont_normalize);

		if (!layout_props)
			return NULL;
	}
	else
	{
		layout_props = new LayoutProperties();

		if (!layout_props)
			return NULL;

		layout_props->Into(&prop_list);

#ifdef CURRENT_STYLE_SUPPORT
		if (dont_normalize && !layout_props->GetProps()->CreatePropTypes())
			return NULL;
#endif // CURRENT_STYLE_SUPPORT

		vd_state = hld_profile->GetVisualDevice()->PushState();
		vd_state_pushed = TRUE;
		hld_profile->GetVisualDevice()->Reset();

#ifdef CURRENT_STYLE_SUPPORT
		layout_props->GetProps()->Reset(NULL, hld_profile, dont_normalize);
#else // CURRENT_STYLE_SUPPORT
		layout_props->GetProps()->Reset(NULL, hld_profile);
#endif // CURRENT_STYLE_SUPPORT
	}

	if (html_element && (layout_props = new LayoutProperties()))
	{
		layout_props->Into(&prop_list);

#ifdef CURRENT_STYLE_SUPPORT
		if (dont_normalize && !layout_props->GetProps()->CreatePropTypes())
			layout_props = NULL;
		else
#endif // CURRENT_STYLE_SUPPORT
		{
			layout_props->html_element = html_element;

			LayoutProperties* parent_props = layout_props->Pred();

			if (!layout_props->Inherit(hld_profile, parent_props))
				layout_props = NULL;
		}
	}

	if (vd_state_pushed)
		hld_profile->GetVisualDevice()->PopState(vd_state);

	return layout_props;
}

/** Create the cascade for the specified element and put it in prop_list.
	Return the last element in the cascade, or NULL on allocation failure.
	If NULL is returned, prop_list is guaranteed to be empty. OBS! The user
	of this generated cascade may not rely on that the LayoutProperties::container
	or any box type of the html_elements in the cascade are of the correct type. */

/* static */ LayoutProperties*
LayoutProperties::CreateCascade(HTML_Element* html_element, Head& prop_list, HLDocProfile* hld_profile
#ifdef CURRENT_STYLE_SUPPORT
								, BOOL dont_normalize
#endif
	)
{
	/** Sub trees may not be sent to this function, only rooted trees */

	/* This assertion is so beautiful that it deserves a comment: hld_profile points to a
	   FramesDocument. So does LayoutWorkplace. At this point they may not be the same, if we're
	   working on a print document. hld_profile either points to the original document or the print
	   document, while LayoutWorkplace always points to the print document, luckily. For further
	   information / amusement, search the code for SetFramesDocument().

	   rune@opera.com: I had to add (!html_element) when only creating the cascade element for the
	   initial properties. I don't know if that added anything to the amusement. */

	OP_ASSERT(!html_element || hld_profile->GetLayoutWorkplace()->GetFramesDocument()->GetDocRoot() && hld_profile->GetLayoutWorkplace()->GetFramesDocument()->GetDocRoot()->IsAncestorOf(html_element));

	/* We need to make sure that properties are up to date before generating the cascade.
	   This is always the case when calling this method from reflow or traverse, but not so otherwise. */

	LayoutWorkplace* wp = hld_profile->GetLayoutWorkplace();
	BOOL old_can_yield = FALSE;

	if (wp)
	{
		old_can_yield = wp->CanYield();
		wp->SetCanYield(FALSE);
	}

	hld_profile->GetLayoutWorkplace()->ReloadCssProperties();

	if (wp)
		wp->SetCanYield(old_can_yield);

	LayoutProperties* props = CreateCascadeInternal(html_element, prop_list, hld_profile
#ifdef CURRENT_STYLE_SUPPORT
													, dont_normalize
#endif
		);

	if (!props)
		prop_list.Clear();

	return props;
}

BOOL LayoutProperties::RectifyInvalidLayoutValues()
{
	BOOL has_copied = FALSE;

	if (props.IsInsideReplacedContent())
	{
		/* Can't set these values if we are children of replaced content */

		if (props.display_type != CSS_VALUE_inline)
		{
			if (!WantToModifyProperties(TRUE))
				return FALSE;

			has_copied = TRUE;
			props.display_type = CSS_VALUE_inline;
		}

		if (props.float_type != CSS_VALUE_none)
		{
			if (!has_copied)
			{
				if (!WantToModifyProperties(TRUE))
					return FALSE;
				has_copied = TRUE;
			}
			props.float_type = CSS_VALUE_none;
		}

		if (props.position != CSS_VALUE_static)
		{
			if (!has_copied)
			{
				if (!WantToModifyProperties(TRUE))
					return FALSE;
				has_copied = TRUE;
			}
			props.position = CSS_VALUE_static;
		}
	}
	return TRUE;
}

/** Promote children of the specified element to siblings following it.

	@param doc The document the elements live in.
	@param element The parent whose children are to be promoted.
	@param first_child First child of 'element' to be promoted. If NULL,
	all children will be promoted. */

static void PromoteChildren(FramesDocument *doc, HTML_Element* element, HTML_Element* first_child = NULL)
{
	while (HTML_Element* child = element->LastChild())
	{
		if (child->GetLayoutBox())
			/* Must remove any layout boxes now, since they may be taken out of
			   the necessary table element structure here, and calling any
			   method (e.g. Invalidate()) on them afterwards is wrong and
			   potentially dangerous then. */

			child->RemoveLayoutBox(doc);

		child->Out();
		child->Follow(element);

		if (child == first_child)
			break;
	}
}

/* static */
void LayoutProperties::RemoveElementsInsertedByLayout(LayoutInfo& info, HTML_Element*& element)
{
	do
	{
		// Remove children inserted by layout.

		HTML_Element* child = element->FirstChild();

		while (child)
		{
			HTML_Element* next_child = child->Suc();

			if (child->GetInserted() == HE_INSERTED_BY_LAYOUT)
			{
				PromoteChildren(info.doc, child);

				next_child = child->Suc();

				if (child->OutSafe(info.doc))
					child->Free(info.doc);
			}

			child = next_child;
		}

		// If this element element was inserted by layout, remove it.

		if (element->GetInserted() == HE_INSERTED_BY_LAYOUT)
		{
			HTML_Element* new_element = element->FirstChild();

			PromoteChildren(info.doc, element);

			if (element->OutSafe(info.doc))
				element->Free(info.doc);

			element = new_element;

			OP_ASSERT(element);

			continue; // Remove children inserted by the DOM element as well.
		}

		break;
	}
	while (1);
}

LayoutProperties::LP_STATE
LayoutProperties::LayoutElement(LayoutInfo& info)
{
	HTML_Element* old_stop_before = info.stop_before;
	BOOL should_layout_children = html_element->GetLayoutBox()->ShouldLayoutChildren();

	/* First lay out only this element, excluding its children. Set a stop
	   element to make that happen. */

	if (should_layout_children)
		info.stop_before = html_element->Next();
	else
		/* If we're not going to descend into children of this element, we need
		   to look for the next sibling, and stop there. This is strictly only
		   needed when laying out with ::first-line. */

		info.stop_before = html_element->NextSibling();

	if (container && container->IsCssFirstLine())
	{
		LayoutProperties* container_cascade = container->GetPlaceholder()->GetCascade();

		OP_ASSERT(container_cascade);

		if (container_cascade)
			switch (container->LayoutWithFirstLine(container_cascade, info, html_element, LayoutCoord(0)))
			{
			case LAYOUT_OUT_OF_MEMORY:
				return OUT_OF_MEMORY;

			case LAYOUT_YIELD:
				return YIELD;
			}
	}
	else
		switch (html_element->GetLayoutBox()->Layout(this, info))
		{
		case LAYOUT_OUT_OF_MEMORY:
			return OUT_OF_MEMORY;

		case LAYOUT_YIELD:
			return YIELD;
		}

	info.stop_before = old_stop_before;

	HTML_Element* first_child = html_element->FirstChild();

	if (first_child && should_layout_children)
		// Then lay out all children.

		switch (html_element->GetLayoutBox()->LayoutChildren(this, info, first_child))
		{
		case LAYOUT_OUT_OF_MEMORY:
			return OUT_OF_MEMORY;

		case LAYOUT_YIELD:
			return YIELD;

		case LAYOUT_STOP:
			return STOP;
		}

	return CONTINUE;
}

BOOL
LayoutProperties::HasRealContent()
{
	OP_ASSERT(html_element->GetIsBeforePseudoElement() || html_element->GetIsAfterPseudoElement());

	if (html_element->IsMatchingType(Markup::HTE_BR, NS_HTML))
		/* BR has some peculiarities when it comes to generated content. There
		   will be added linebreaks and stuff if no BrBox has been created. */

		return TRUE;

	if (!props.content_cp)
		return FALSE;

	return props.content_cp->ArrayValueLen() > 0;
}

BOOL
LayoutProperties::CreatePseudoElement(LayoutInfo& info, BOOL before)
{
	HTML_Element* pseudo_element = NEW_HTML_Element();

	if (!pseudo_element || OpStatus::IsMemoryError(pseudo_element->Construct(info.hld_profile, html_element->GetNsIdx(), html_element->Type(), NULL, HE_INSERTED_BY_LAYOUT)))
	{
		DELETE_HTML_Element(pseudo_element);
		return FALSE;
	}

	if (before && html_element->FirstChild() != NULL)
		pseudo_element->Precede(html_element->FirstChild());
	else
		pseudo_element->Under(html_element);

	if (before)
		pseudo_element->SetIsBeforePseudoElement();
	else
		pseudo_element->SetIsAfterPseudoElement();

	return TRUE;
}

BOOL
LayoutProperties::CreateListItemMarker(LayoutInfo& info)
{
	HTML_Element* list_item_marker = NEW_HTML_Element();

	if (!list_item_marker || OpStatus::IsMemoryError(list_item_marker->Construct(info.hld_profile, html_element->GetNsIdx(), html_element->Type(), NULL, HE_INSERTED_BY_LAYOUT)))
	{
		DELETE_HTML_Element(list_item_marker);
		return FALSE;
	}

	if (html_element->FirstChild() != NULL)
		list_item_marker->Precede(html_element->FirstChild());
	else
		list_item_marker->Under(html_element);

	list_item_marker->SetIsListMarkerPseudoElement();

	return TRUE;
}

BOOL
LayoutProperties::InsertListItemMarkerTextNode()
{
#ifdef DEBUG
	switch (props.list_style_type)
	{
	case CSS_VALUE_decimal:
	case CSS_VALUE_lower_roman:
	case CSS_VALUE_upper_roman:
	case CSS_VALUE_lower_alpha:
	case CSS_VALUE_upper_alpha:
	case CSS_VALUE_lower_latin:
	case CSS_VALUE_upper_latin:
	case CSS_VALUE_lower_greek:
	case CSS_VALUE_armenian:
	case CSS_VALUE_georgian:
	case CSS_VALUE_decimal_leading_zero:
	case CSS_VALUE_hebrew:
	case CSS_VALUE_cjk_ideographic:
	case CSS_VALUE_hiragana:
	case CSS_VALUE_katakana:
	case CSS_VALUE_hiragana_iroha:
	case CSS_VALUE_katakana_iroha:
		break;
	default:
		OP_ASSERT(!"This is not a marker that needs a text node!");
	}

	OP_ASSERT(html_element->GetIsListMarkerPseudoElement());
#endif // DEBUG

	HTML_Element* text_node = HTML_Element::CreateText(NULL, 0, FALSE, FALSE, FALSE);

	if (!text_node)
		return FALSE;

	OP_ASSERT(text_node->Type() == Markup::HTE_TEXT);

	text_node->SetInserted(HE_INSERTED_BY_LAYOUT);
	text_node->SetIsGeneratedContent();
	text_node->Under(html_element);

	return TRUE;
}

LayoutProperties::ListMarkerContentType
LayoutProperties::GetListMarkerContentType(FramesDocument* doc)
{
	OP_ASSERT(props.HasListItemMarker());

#ifdef CSS_GRADIENT_SUPPORT
	if (props.list_style_image_gradient)
		return LIST_MARKER_BULLET;
#endif // CSS_GRADIENT_SUPPORT

#ifdef SVG_SUPPORT
	SVGImage* svg_image = NULL;
	Image img = GetListMarkerImage(doc, svg_image);

	if (svg_image)
		return LIST_MARKER_BULLET;
#else // SVG_SUPPORT
	Image img = GetListMarkerImage(doc);
#endif // SVG_SUPPORT

	if (img.Width() == 0 || img.Height() == 0)
		img.PeekImageDimension();

	if (img.Width() > 0 && img.Height() > 0)
		return LIST_MARKER_BULLET;

	switch (props.list_style_type)
	{
	case CSS_VALUE_disc:
	case CSS_VALUE_circle:
	case CSS_VALUE_square:
	case CSS_VALUE_box:
		return LIST_MARKER_BULLET;
	case CSS_VALUE_decimal:
	case CSS_VALUE_lower_roman:
	case CSS_VALUE_upper_roman:
	case CSS_VALUE_lower_alpha:
	case CSS_VALUE_upper_alpha:
	case CSS_VALUE_lower_latin:
	case CSS_VALUE_upper_latin:
	case CSS_VALUE_lower_greek:
	case CSS_VALUE_armenian:
	case CSS_VALUE_georgian:
	case CSS_VALUE_decimal_leading_zero:
	case CSS_VALUE_hebrew:
	case CSS_VALUE_cjk_ideographic:
	case CSS_VALUE_hiragana:
	case CSS_VALUE_katakana:
	case CSS_VALUE_hiragana_iroha:
	case CSS_VALUE_katakana_iroha:
		return LIST_MARKER_TEXT;
	default:
		OP_ASSERT(!"Every list style type should be handled!");
	case CSS_VALUE_none:
		return LIST_MARKER_NONE; // If we end up here, it means the image for the marker is not available at the moment.
	}
}

LayoutProperties::LP_STATE
LayoutProperties::CreateListMarkerBox(ListMarkerContentType type)
{
	OP_ASSERT(html_element->GetIsListMarkerPseudoElement());
	OP_ASSERT(type != LIST_MARKER_NONE); // We shouldn't be creating a layout box for such marker.

	InlineBox* box;
	Content* content;

	switch (type)
	{
	case LIST_MARKER_TEXT:
		content = OP_NEW(InlineContent, ());
		break;
	case LIST_MARKER_BULLET:
		content = OP_NEW(BulletContent, ());
		break;
	default:
		content = NULL;
		OP_ASSERT(!"Marker content type not handled!");
	}

	if (!content)
		return OUT_OF_MEMORY;

	box = OP_NEW(InlineBox, (html_element, content));

	if (!box)
	{
		OP_DELETE(content);
		return OUT_OF_MEMORY;
	}

	content->SetPlaceholder(box);
	SetNewLayoutBox(html_element, box);

	return CONTINUE;
}

BOOL
LayoutProperties::AddGeneratedContent(LayoutInfo& info)
{
	ContentGenerator *content_generator = g_opera->layout_module.GetContentGenerator();
	LayoutProperties* parent_cascade = Pred();
	BOOL is_br = html_element->IsMatchingType(Markup::HTE_BR, NS_HTML);

	while (parent_cascade->html_element->GetInserted() == HE_INSERTED_BY_LAYOUT)
		parent_cascade = parent_cascade->Pred();

	content_generator->Reset();

	if (is_br && html_element->GetIsAfterPseudoElement() && !parent_cascade->GetProps()->content_cp && !html_element->Pred())
		// BR::after elements should add a line break before the content.

		if (!content_generator->AddLineBreak())
			return FALSE;

	if (CSS_decl* content_decl = GetProps()->content_cp)
	{
		// Go through the content property and add content accordingly.

		short gen_arr_len = content_decl->ArrayValueLen();
		const CSS_generic_value* gen_arr = content_decl->GenArrayValue();

		for (int i=0; i<gen_arr_len; i++)
			switch (gen_arr[i].value_type)
			{
			case CSS_STRING_LITERAL:
			case CSS_ESCAPED_STRING_LITERAL:
				if (!content_generator->AddString(gen_arr[i].value.string))
					return FALSE;

				break;

			case CSS_FUNCTION_ATTR:
				{
					TempBuffer tmp_buf;
					HTML_Element* dom_element = html_element;

					while (dom_element->GetInserted() == HE_INSERTED_BY_LAYOUT)
						dom_element = dom_element->Parent();

					Markup::Type elm_type = dom_element->GetNsType() == NS_HTML ? dom_element->Type() : Markup::HTE_UNKNOWN;
					const uni_char* attr_val = dom_element->GetAttrValue(gen_arr[i].value.string, NS_IDX_ANY_NAMESPACE, elm_type, &tmp_buf);

					if (attr_val)
						if (!content_generator->AddString(attr_val))
							return FALSE;
				}
				break;

			case CSS_FUNCTION_URL:
				if (!content_generator->AddURL(gen_arr[i].value.string))
					return FALSE;
				break;

#ifdef SKIN_SUPPORT
			case CSS_FUNCTION_SKIN:
				OP_ASSERT(gen_arr[i].value.string[0] == 's' && gen_arr[i].value.string[1] == ':');
				if (!content_generator->AddSkinElement(gen_arr[i].value.string+2))
					return FALSE;
				break;
#endif // SKIN_SUPPORT

			case CSS_FUNCTION_COUNTER:
			case CSS_FUNCTION_COUNTERS:
				{
					OpString counter_str;

					if (OpStatus::IsMemoryError(info.workplace->GetCounters()->AddCounterValues(counter_str, html_element, gen_arr[i].value_type, gen_arr[i].value.string)))
						return FALSE;

					if (!content_generator->AddString(counter_str))
						return FALSE;

					break;
				}

			case CSS_IDENT:
				{
					short quote_val = gen_arr[i].value.type;

					OP_ASSERT(CSS_is_quote_val(quote_val));

					BOOL use_quote = (quote_val == CSS_VALUE_open_quote || quote_val == CSS_VALUE_close_quote);
					BOOL open_quote = (quote_val == CSS_VALUE_open_quote || quote_val == CSS_VALUE_no_open_quote);

					if (!open_quote)
						use_quote = info.DecQuoteLevel() && use_quote;

					if (use_quote)
					{
						const uni_char* quote = GetQuote(info.GetQuoteLevel(), open_quote);
						if (!content_generator->AddString(quote))
							return FALSE;
					}

					if (open_quote)
						info.IncQuoteLevel();
				}
				break;

			case CSS_FUNCTION_LANGUAGE_STRING:
				{
					OpString lang_str;
					OP_STATUS stat = g_languageManager->GetString((Str::LocaleString)gen_arr[i].value.number, lang_str);
					if (OpStatus::IsError(stat) || !content_generator->AddString(lang_str.CStr()))
						return FALSE;
				}
				break;
			}
	}

	if (is_br && html_element->GetIsBeforePseudoElement() && !parent_cascade->GetProps()->content_cp)
		// BR::before elements should add a line break after the content.

		if (!content_generator->AddLineBreak())
			return FALSE;

	// Create HTML elements for the content generated.

	for (const ContentGenerator::Content* content = content_generator->GetContent();
		 content != NULL;
		 content = (ContentGenerator::Content*)content->Suc())
	{
		HTML_Element* element = NULL;

		if (OpStatus::IsMemoryError(content->CreateElement(element, info.hld_profile)))
			return FALSE;
		else
		{
			element->SetInserted(HE_INSERTED_BY_LAYOUT);
			element->SetIsGeneratedContent();
			element->Under(html_element);
		}
	}

	return TRUE;
}

LayoutProperties::LP_STATE
LayoutProperties::CreateTextBox(LayoutInfo& info)
{
	/* Don't create a text layout box if the element is child of a flexbox,
	   since that means that we have already refused to wrap it inside an
	   anonymous flex item, which means that the text (typically only
	   whitespace) should not be rendered. */

	if (flexbox)
		return CONTINUE;

	Box* text_box = NULL;

	if (html_element->Type() == Markup::HTE_TEXTGROUP)
	{
		text_box = OP_NEW(NoDisplayBox, (html_element));

		if (!text_box)
			return OUT_OF_MEMORY;
	}
	else
	{
		const uni_char* text = html_element->TextContent();
		const int text_length = html_element->GetTextContentLength();

#ifdef DOCUMENT_EDIT_SUPPORT
		if (!text || (!*text && !info.doc->GetDocumentEdit()))
#else
		if (!text || !*text)
#endif
			// Ignore this text element if empty

			return CONTINUE;
		else
		{
			if (!TextRequiresLayout(info.doc, text, (CSSValue) props.white_space))
				/* Ignore this text element if it only contains
				white-space and these are to be collapsed (or if it's not in a container). */

				if (!container || container->GetCollapseWhitespace())
					return CONTINUE;

			if (!html_element->GetIsFirstLetterPseudoElement() && container && container->IsCssFirstLetter())
				if (!html_element->GetIsGeneratedContent() ||
					!html_element->Parent()->GetIsFirstLetterPseudoElement() && !html_element->Parent()->GetIsListMarkerPseudoElement())
					if (!html_element->Pred() || !html_element->Pred()->GetIsFirstLetterPseudoElement())
					{
						unsigned int first_letter_len = GetFirstLetterLength(text, text_length);
						if (first_letter_len > 0)
						{
							LayoutProperties* parent = this;

							HTML_Element* first_letter_root = NULL;
							HTML_Element* inner_first_letter = NULL;

							// Iterate through containers for which this text node contains the its first letter.

							while (parent)
							{
								// Find the next container for this first-letter pseudo element.
								parent = parent->Pred();

								while (parent && !parent->html_element->GetLayoutBox()->GetContainer())
									parent = parent->Pred();

								if (parent)
								{
									Container* current_container = parent->html_element->GetLayoutBox()->GetContainer();

									if (current_container->HasCssFirstLetter() == FIRST_LETTER_NO)
										parent = NULL;
									else
									{
										if ((current_container->HasCssFirstLetter() & FIRST_LETTER_THIS))
										{
											// Create the first-letter pseudo-element

											HTML_Element* first_letter_element = NEW_HTML_Element();

											if (!first_letter_element)
												return OUT_OF_MEMORY;

											if (OpStatus::IsMemoryError(first_letter_element->Construct(info.hld_profile, parent->html_element->GetNsIdx(), parent->html_element->Type(), NULL, HE_INSERTED_BY_LAYOUT)))
											{
												DELETE_HTML_Element(first_letter_element);
												return OUT_OF_MEMORY;
											}

											if (first_letter_root)
											{
												first_letter_root->Out();
												first_letter_root->Under(first_letter_element);
											}
											else
												inner_first_letter = first_letter_element;

											first_letter_root = first_letter_element;

											first_letter_element->SetIsFirstLetterPseudoElement();
											first_letter_element->Under(parent->html_element);

											if (OpStatus::IsMemoryError(CssPropertyItem::LoadCssProperties(first_letter_element, g_op_time_info->GetRuntimeMS(), info.hld_profile, info.media_type)))
											{
												first_letter_element->Free(info.doc);
												return OUT_OF_MEMORY;
											}
										}

										Content_Box* box = current_container->GetPlaceholder();
										if (!box->IsBlockBox() || !((BlockBox*)box)->IsInStack())
											parent = NULL;
									}

									current_container->FirstLetterFinished();
								}
							}

							if (first_letter_root)
							{
								first_letter_root->Out();
								first_letter_root->Precede(html_element);

								// Create a text element with a copy of the first letter(s)

								HTML_Element* first_letter_text_element = NEW_HTML_Element();

								if (!first_letter_text_element)
									return OUT_OF_MEMORY;

								if (OpStatus::IsMemoryError(first_letter_text_element->Construct(info.hld_profile, text, first_letter_len)))
								{
									DELETE_HTML_Element(first_letter_text_element);
									return OUT_OF_MEMORY;
								}

								first_letter_text_element->SetIsGeneratedContent();
								first_letter_text_element->SetInserted(HE_INSERTED_BY_LAYOUT);
								first_letter_text_element->Under(inner_first_letter);

								// Lay out first-letter pseudo-element

								HTML_Element* old_stop_before = info.stop_before;
								HTML_Element* old_html_element = html_element;

								Clean();

								LayoutProperties* cascade = Pred()->GetChildCascade(info, first_letter_root);

								if (!cascade)
									return OUT_OF_MEMORY;

								info.stop_before = old_html_element;

								if (cascade->CreateLayoutBox(info) == OUT_OF_MEMORY)
									return OUT_OF_MEMORY;

								info.stop_before = old_stop_before;

								// Continue with this element

								cascade = cascade->Pred()->GetChildCascade(info, old_html_element);

								if (!cascade)
									return OUT_OF_MEMORY;

								OP_ASSERT(this == cascade);
							}
						}
					}

					props.SetDisplayProperties(info.visual_device);

					text_box = OP_NEW(Text_Box, (html_element));

					if (!text_box)
						return OUT_OF_MEMORY;

					if (info.text_selection && info.text_selection->IsDirty())
					{
						HTML_Element* start_element = info.text_selection->GetStartElement();
						HTML_Element* end_element = info.text_selection->GetEndElement();

						if (start_element == html_element ||
							(!info.start_selection_done && start_element->GetLayoutBox() && start_element->Precedes(html_element)))
							info.start_selection_done = TRUE;

						if (end_element == html_element ||
							(info.start_selection_done && end_element->GetLayoutBox() && end_element->Precedes(html_element)))
						{
							info.text_selection->MarkClean();
							info.text_selection = NULL;
						}

						if (info.start_selection_done && (info.text_selection || end_element == html_element))
							text_box->SetSelected(TRUE);
					}
		}
	}

	SetNewLayoutBox(html_element, text_box);

	return CONTINUE;
}

/* Check and insert missing table elements both in element tree and cascade.

   Returns INSERTED_PARENT if element was inserted. */

LayoutProperties::LP_STATE
LayoutProperties::CheckAndInsertMissingTableElement(LayoutInfo& info, Markup::Type create_type)
{
	LayoutProperties* parent_cascade = Pred();
	int parent_display_type = parent_cascade->GetProps()->display_type;
	int display_type = props.display_type;

	OP_ASSERT(create_type == Markup::HTE_TABLE || create_type == Markup::HTE_TBODY || create_type == Markup::HTE_TR || create_type == Markup::HTE_TD);
	OP_ASSERT(parent_cascade->html_element->GetLayoutBox());

	Box *parent_box = parent_cascade->html_element->GetLayoutBox();

	switch (display_type)
	{
	case CSS_VALUE_table_column:
		if (parent_box->IsTableColGroup())
			return CONTINUE;

		// fall-through
	case CSS_VALUE_table_row_group:
	case CSS_VALUE_table_header_group:
	case CSS_VALUE_table_footer_group:
	case CSS_VALUE_table_column_group:
	case CSS_VALUE_table_caption:
		if (parent_box->GetTableContent())
			return CONTINUE;

		break;

	case CSS_VALUE_table_row:
		if (parent_box->IsTableRowGroup())
			return CONTINUE;

		break;

	case CSS_VALUE_table_cell:
		if (parent_box->IsTableRow())
			return CONTINUE;

		break;
	}

	if (parent_cascade->html_element->GetInserted() == HE_INSERTED_BY_LAYOUT
		&& !parent_cascade->html_element->GetIsPseudoElement())
	{
		/* The parent is inserted by the layout engine. Check if this child
		   really fits under it. If it doesn't, it needs to be moved out. */

		BOOL illegal = TRUE;

		switch (display_type)
		{
		case CSS_VALUE_table_footer_group:
		case CSS_VALUE_table_header_group:
		case CSS_VALUE_table_row_group:
		case CSS_VALUE_table_column_group:
		case CSS_VALUE_table_caption:
			if (parent_display_type == CSS_VALUE_table_row_group ||
				parent_display_type == CSS_VALUE_table_footer_group ||
				parent_display_type == CSS_VALUE_table_header_group ||
				parent_display_type == CSS_VALUE_table_column_group ||
				parent_display_type == CSS_VALUE_table_caption)
				break;

		case CSS_VALUE_table_row:
			if (parent_display_type == CSS_VALUE_table_row)
				break;

		case CSS_VALUE_table_cell:
			if (parent_display_type == CSS_VALUE_table_cell)
				break;

		default:
			{
				/*	Elements which are not table elements must have a
					table element parent in the source document. Otherwise
					move it up. */
				LayoutProperties* real_parent_cascade = parent_cascade;
				BOOL inner_table_inserted_by_layout = FALSE;
				while (real_parent_cascade && real_parent_cascade->html_element->GetInserted() == HE_INSERTED_BY_LAYOUT)
				{
					/* Sometimes it happens that element like table-column-group is inserted in a table row of another table.
					   Table repair code will detect such case and will wrap incorrect element with a new anonymous table
					   (basically we will have table created by a layout inserted in another table). As a side effect,
					   all wrapped element's siblings will became elements of that new inner table. This is a case of
					   CORE-19455 where table-column group is inside table row causing all cells following that element
					   become elements of newly created table (and having table-column-group defined styles applied).

					   Code below checks if element that is going to be laid out does not belong to outer table and if
					   so makes it being moved up to a place where it belongs). This is done by checking if path between
					   element and it's first ancestor that was not inserted by layout does not contain any anonymous
					   table elements.

					   CSS spec says that such anonymous table box should span all consecutive sibling boxes of wrapped element
					   that require a 'table' parent: 'table-row', 'table-row-group', 'table-header-group', 'table-footer-group',
					   'table-column', 'table-column-group', and 'table-caption', including any anonymous 'table-row'.
					   This is guaranteed by one of earlier checks as any element that require 'table' parent will not be touched.
					   However 'table-column' and 'table-row' require either 'table-column-group' or 'table-row-group' as a parent
					   (but those may not be yet available in newly created anonymous table) so we need to exclude theme here
					   to make sure we will not touch them too.
					*/


					if (display_type != CSS_VALUE_table_row && display_type != CSS_VALUE_table_column
						&& real_parent_cascade->GetProps()->display_type == CSS_VALUE_table)
						inner_table_inserted_by_layout = TRUE;

					real_parent_cascade = real_parent_cascade->Pred();
				}

				if (real_parent_cascade)
				{
					switch (real_parent_cascade->GetProps()->display_type)
					{
					case CSS_VALUE_table_footer_group:
					case CSS_VALUE_table_header_group:
					case CSS_VALUE_table_row_group:
					case CSS_VALUE_table_column_group:
					case CSS_VALUE_table_caption:
					case CSS_VALUE_table_row:
					case CSS_VALUE_table_cell:
					case CSS_VALUE_table:
					case CSS_VALUE_inline_table:
						if (!inner_table_inserted_by_layout)
							illegal = FALSE;
						break;
					default:
						break;
					}
					break;
				}
				else
					illegal = FALSE;
			}
			break;
		}

		if (illegal)
		{
			/* The layout-inserted parent is not a suitable parent for this
			   element; move this element and the siblings following it one
			   level up. */

			HTML_Element* parent = html_element->Parent();

			while (HTML_Element* element = parent->LastChild())
			{
				if (Box* box = element->GetLayoutBox())
				{
					/* The element is about to be moved in the tree, so its
					   layout box is going to be useless (and potentially
					   dangerous to use). Invalidate it while we still have the
					   chance, and remove it. */

					box->Invalidate(Pred(), info);
					element->RemoveLayoutBox(info.doc);
				}

				element->Out();
				element->Follow(parent);
				element->SetDirty(ELM_BOTH_DIRTY);

				if (element == html_element)
					break;
			}

			/* The cascade entry is now illegal (since the element associated
			   with it has been moved up) - clean it. */

			Clean();

			return ELEMENT_MOVED_UP;
		}
	}

	HTML_Element* element;

	// Parent does not have proper type - need to insert the missing element

	HTML_Element* inserted_element = NEW_HTML_Element();

	if (!inserted_element)
		return OUT_OF_MEMORY;

	if (inserted_element->Construct(info.hld_profile, NS_IDX_HTML, create_type, NULL, HE_INSERTED_BY_LAYOUT) == OpStatus::ERR_NO_MEMORY)
	{
		DELETE_HTML_Element(inserted_element);
		return OUT_OF_MEMORY;
	}

	if (parent_cascade->html_element->Type() == Markup::HTE_TABLE)
	{
		/* Parent was a table with an incompatible display type.  We
		   want cellspacing and cellpadding to be preserved, so these
		   attributes are copied.  Perhaps all attributes should be
		   copied. */

		if (parent_cascade->html_element->HasNumAttr(Markup::HA_CELLSPACING))
		{
			void* val = parent_cascade->html_element->GetAttr(Markup::HA_CELLSPACING, ITEM_TYPE_NUM, (void*)0);
			if (inserted_element->SetAttr(Markup::HA_CELLSPACING, ITEM_TYPE_NUM, val) < 0)
			{
				DELETE_HTML_Element(inserted_element);
				return OUT_OF_MEMORY;
			}
		}

		if (parent_cascade->html_element->HasNumAttr(Markup::HA_CELLPADDING))
		{
			void* val = parent_cascade->html_element->GetAttr(Markup::HA_CELLPADDING, ITEM_TYPE_NUM, (void*)0);
			if (inserted_element->SetAttr(Markup::HA_CELLPADDING, ITEM_TYPE_NUM, val) < 0)
			{
				DELETE_HTML_Element(inserted_element);
				return OUT_OF_MEMORY;
			}
		}
	}

	// Put this element under the new inserted element.

	inserted_element->Precede(html_element);
	html_element->Out();
	html_element->Under(inserted_element);

	BOOL move_suc = TRUE;

	for (element = html_element; element; element = element->LastChild())
		if (element->GetIsBeforePseudoElement())
		{
			/* If the new inserted element was inserted because of ::before, we shouldn't
			   move successors under it (since they don't belong to the ::before
			   element). */

			move_suc = FALSE;
			break;
		}
		else
			if (element->GetInserted() != HE_INSERTED_BY_LAYOUT)
				break;

	if (move_suc)
		/* Put all successors under the new inserted element since they might become part
		   of the inserted table object (we cannot tell at this point (need the cascade
		   first)). If it turns out later that they cannot be part of the table, they
		   will be moved back up. */

		for (element = inserted_element->Suc(); element; element = inserted_element->Suc())
		{
			BOOL found_after = FALSE;

			for (HTML_Element* child = element; child && child->GetInserted() == HE_INSERTED_BY_LAYOUT; child = child->LastChild())
				if (child->GetIsAfterPseudoElement())
				{
					// Found an ::after pseudo element. It shall not be part of this table.

					found_after = TRUE;
					break;
				}

			if (found_after)
				break;

			if (element->GetLayoutBox())
			{
				/* The element is about to be moved in the tree, so its layout
				   box will become useless (and potentially dangerous to
				   use). Remove it. */

				element->RemoveLayoutBox(info.doc);
				element->SetDirty(ELM_EXTRA_DIRTY);
			}

			element->Out();
			element->Under(inserted_element);
		}

	/* The cascade entry is now illegal (since the element associated with it
	   has got a new parent) - clean it. */

	Clean();

	return INSERTED_PARENT;
}

LayoutProperties::LP_STATE LayoutProperties::InsertMapAltTextElements(LayoutInfo& info)
{
	OP_ASSERT(html_element->Type() == Markup::HTE_IMG || html_element->Type() == Markup::HTE_OBJECT || html_element->Type() == Markup::HTE_INPUT);

	OpString translated_alt_text;

	LogicalDocument* logdoc = info.doc->GetLogicalDocument();
	if (!logdoc)
	{
		OP_ASSERT(0);
		return CONTINUE;
	}

	URL* map_url = html_element->GetUrlAttr(Markup::HA_USEMAP, NS_IDX_HTML, logdoc);

	if (!map_url)
		return OUT_OF_MEMORY;

	HTML_Element* map_element = logdoc->GetNamedHE(map_url->UniRelName());

	if (!map_element)
		return CONTINUE;

	HTML_Element* link_element = map_element->GetNextLinkElementInMap(TRUE, map_element);

	while (link_element)
	{
		if (link_element->IsMatchingType(Markup::HTE_AREA, NS_HTML))
		{
			HTML_Element* last_elm = html_element;

			const uni_char* alt_text = link_element->GetStringAttr(Markup::HA_ALT);

			const uni_char* href = link_element->GetStringAttr(Markup::HA_HREF);

			if (href)
			{
				HTML_Element* a_elm = NEW_HTML_Element();

				if (!a_elm)
					return OUT_OF_MEMORY;

				if (a_elm->Construct(info.hld_profile, NS_HTML, Markup::HTE_A, NULL) == OpStatus::ERR_NO_MEMORY ||
					a_elm->SetAttr(Markup::HA_HREF, ITEM_TYPE_STRING, (void*)href, FALSE, NS_IDX_DEFAULT) < 0)
				{
					DELETE_HTML_Element(a_elm);
					return OUT_OF_MEMORY;
				}

				a_elm->Under(last_elm);
				a_elm->SetInserted(HE_INSERTED_BY_LAYOUT);
				last_elm = a_elm;
			}

			if (alt_text)
			{
				HTML_Element* alt_elm = NEW_HTML_Element();

				if (!alt_elm)
					return OUT_OF_MEMORY;

				if (alt_elm->Construct(info.hld_profile, alt_text, uni_strlen(alt_text)) == OpStatus::ERR_NO_MEMORY)
				{
					DELETE_HTML_Element(alt_elm);
					return OUT_OF_MEMORY;
				}
				alt_elm->Under(last_elm);
				alt_elm->SetInserted(HE_INSERTED_BY_LAYOUT);
			}
		}

		link_element = link_element->GetNextLinkElementInMap(TRUE, map_element);
	}

	return CONTINUE;
}

#ifdef ALT_TEXT_AS_TEXT_BOX

LayoutProperties::LP_STATE LayoutProperties::InsertAltTextElement(LayoutInfo& info)
{
	OP_ASSERT(html_element->Type() == Markup::HTE_IMG || html_element->Type() == Markup::HTE_OBJECT || html_element->Type() == Markup::HTE_INPUT);

	OpString translated_alt_text;
	const uni_char* alt_txt;

	if (html_element->HasAttr(Markup::HA_ALT))
		alt_txt = html_element->GetStringAttr(Markup::HA_ALT);
	else if (info.doc->GetLayoutMode() == LAYOUT_CSSR || info.doc->GetLayoutMode() == LAYOUT_SSR || !info.doc->GetShowImages())
	{
		TRAPD(retv, g_languageManager->GetStringL(Str::SI_DEFAULT_IMG_ALT_TEXT, translated_alt_text));

		if (retv != OpStatus::OK)
			alt_txt = UNI_L("Image");
		else
			alt_txt = translated_alt_text.CStr();
	}
	else
		alt_txt = UNI_L("");

	OP_ASSERT(alt_txt);

	if (alt_txt)
	{
		// Create a text element from the alt text
		HTML_Element* alt_elm = NEW_HTML_Element();

		if (!alt_elm)
			return OUT_OF_MEMORY;

		alt_elm->Construct(info.hld_profile, alt_txt, uni_strlen(alt_txt)); // FIXME OOM
		alt_elm->Under(html_element);
		alt_elm->SetInserted(HE_INSERTED_BY_LAYOUT);
	}

	return CONTINUE;
}

#endif

/** Create and insert an anonymous flex item element.

	The HTML element in 'cascade' and all its consecutive siblings will be
	reparented under this anonymous flex item element. */

static LayoutProperties::LP_STATE
InsertFlexItemParent(LayoutInfo& info, LayoutProperties* cascade)
{
	// Create anonymous flex item element.

	HTML_Element* anon_item_elm = NEW_HTML_Element();

	if (!anon_item_elm ||
		OpStatus::IsMemoryError(anon_item_elm->Construct(info.hld_profile, NS_HTML, Markup::LAYOUTE_ANON_FLEX_ITEM, NULL, HE_INSERTED_BY_LAYOUT)))
	{
		DELETE_HTML_Element(anon_item_elm);
		return LayoutProperties::OUT_OF_MEMORY;
	}

	// Insert anonymous flex item element into the tree.

	anon_item_elm->Precede(cascade->html_element);

	/* Move this element and all its consecutive siblings under the anonymous
	   flex item. It may be that not all of them belong here. If an element can
	   constitute a flex item on its own, it doesn't belong under this
	   anonymous item, but that is too early to tell at this point. We will
	   detect that during creation of such a box, and move it (and its
	   consecutive siblings) out of the anonymous flex item created here. */

	const HTMLayoutProperties& props = *cascade->GetProps();

	/* If this is an absolutely positioned element, it needs to live alone in a special
	   anonymous flex item. */

	BOOL only_this = props.position == CSS_VALUE_absolute || props.position == CSS_VALUE_fixed;

	for (HTML_Element* element = cascade->html_element; element;)
	{
		HTML_Element* next = only_this ? NULL : element->Suc();

		if (element->GetLayoutBox())
		{
			/* The element is about to be moved in the tree, so its layout
			   box will become useless (and potentially dangerous to
			   use). Remove it. */

			element->RemoveLayoutBox(info.doc);
			element->SetDirty(ELM_EXTRA_DIRTY);
		}

		element->Out();
		element->Under(anon_item_elm);
		element = next;
	}

	/* The cascade entry is now illegal (since the element associated with it
	   has got a new parent) - clean it. */

	cascade->Clean();

	return LayoutProperties::INSERTED_PARENT;
}

/** Return TRUE if the specified element is an anonymous flex item. */

static inline BOOL
IsAnonymousFlexItem(HTML_Element* html_element)
{
	BOOL is_anon_flexitem = html_element->Type() == Markup::LAYOUTE_ANON_FLEX_ITEM;

	OP_ASSERT(!is_anon_flexitem ||
			  html_element->Parent() &&
			  html_element->Parent()->GetLayoutBox() &&
			  html_element->Parent()->GetLayoutBox()->GetFlexContent());

	return is_anon_flexitem;
}

// Load any inline urls

OP_LOAD_INLINE_STATUS
LayoutProperties::LoadInlineURL(LayoutInfo& info,
								URL* inline_url,
								InlineResourceType inline_type,
								BOOL from_user_css,
								BOOL &may_need_reflow)
{
	OP_LOAD_INLINE_STATUS load_inline_stat = OpStatus::OK;

	if (inline_url && !inline_url->IsEmpty())
	{
		if (info.media_type != CSS_MEDIA_TYPE_PRINT)
		{
			load_inline_stat = info.doc->LoadInline(inline_url, html_element, inline_type, LoadInlineOptions().FromUserCSS(from_user_css));

			if (load_inline_stat == LoadInlineStatus::USE_LOADED && html_element->HasRealSizeDependentCss())
				may_need_reflow = TRUE;
		}
#ifdef _PRINT_SUPPORT_
		else
			if (inline_type == IMAGE_INLINE)
				load_inline_stat = info.hld_profile->AddPrintImageElement(html_element, inline_type, inline_url);
#endif
	}

	return load_inline_stat;
}

/** Helper function for finding out whether we should treat the
    current properties as position:relative or not.

    Normally the position property defines this, but for CSS
    transforms also transformed elements should be treated as relative
    positioned elements in a number of aspects. */

static inline BOOL HandleAsRelPos(const HTMLayoutProperties &props)
{
#ifdef CSS_TRANSFORMS
	return props.position == CSS_VALUE_relative ||
		(props.position == CSS_VALUE_static && props.transforms_cp != NULL);
#else
	return props.position == CSS_VALUE_relative;
#endif
}

/** Helper inline function to avoid a preprocessor directive or
	two. */

static inline BOOL IsTransformed(const HTMLayoutProperties &props)
{
#ifdef CSS_TRANSFORMS
	return props.transforms_cp != NULL;
#else
	return FALSE;
#endif
}

/* Macro to create a box that may be transformed and a helper macro to
   establish that code paths that can't create transformable boxes. */

#ifdef CSS_TRANSFORMS
# define NEW_TRANSFORMABLE_BOX(BoxType, args)							\
	(props.transforms_cp ?												\
	 OP_NEW(Transformed##BoxType, args) :								\
	 OP_NEW(BoxType, args))
# define ASSERT_NO_TRANSFORM() OP_ASSERT(props.transforms_cp == NULL);
#else
# define NEW_TRANSFORMABLE_BOX(BoxType, args)	\
	(OP_NEW(BoxType, args))
# define ASSERT_NO_TRANSFORM() ((void)0)
#endif

LayoutProperties::LP_STATE
LayoutProperties::CreateBox(LayoutInfo& info, Markup::Type element_type, CSSValue display_type, BOOL& may_need_reflow)
{
	if (element_type == Markup::HTE_TEXT || element_type == Markup::HTE_TEXTGROUP)
		return CreateTextBox(info);
	else
	{
		Box* layout_box = NULL;
		Content_Box* content_box = NULL;
		Content* content = NULL;
		BOOL content_set = FALSE;
		const HTMLayoutProperties& props = GetCascadingProperties();
		BOOL create_root = element_type == Markup::HTE_DOC_ROOT;

#ifdef SVG_SUPPORT
		if (!create_root &&
			html_element->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG) &&
			!html_element->HasAttr(Markup::XLINKA_HREF, NS_IDX_XLINK))
		{
			if (!html_element->Parent() || html_element->Parent()->GetNsType() != NS_SVG)
				/* Not in an SVG tree. Refuse box creation. We don't want it to be a
				   child of an HTML layout box, especially not a child of some unsuitable
				   parent, such as table row or replaced content. */

				return CONTINUE;

			create_root = TRUE;

			/* Create NoDisplayBox objects all the way up to the nearest ancestor
			   that has a layout box. */

			for (HTML_Element* ancestor = html_element->Parent();
				 ancestor && !ancestor->GetLayoutBox();
				 ancestor = ancestor->Parent())
				if (Box* ancestor_layout_box = OP_NEW(NoDisplayBox, (ancestor)))
					SetNewLayoutBox(ancestor, ancestor_layout_box);
				else
					return OUT_OF_MEMORY;
		}
#endif // SVG_SUPPORT

		if (create_root && !html_element->GetIsPseudoElement())
		{
#ifdef PAGED_MEDIA_SUPPORT
			if (HTMLayoutProperties::IsPagedOverflowValue(info.workplace->GetViewportOverflowX()))
				content = OP_NEW(RootPagedContainer, ());
			else
#endif // PAGED_MEDIA_SUPPORT
				content = OP_NEW(RootContainer, ());

			if (content == NULL)
				return OUT_OF_MEMORY;

			content_set = TRUE;
			content_box = OP_NEW(AbsoluteZRootBox, (html_element, content));
		}
		else
		{
			HTML_Element* parent = html_element->Parent();

			if (!props.content_cp && !html_element->GetIsPseudoElement())
				if (html_element->GetNsType() == NS_HTML)
				{
					content_set = TRUE;

					switch (element_type)
					{
					case Markup::HTE_BR:
					case Markup::HTE_WBR:
						if (html_element->GetInserted() != HE_INSERTED_BY_LAYOUT &&
							(display_type == CSS_VALUE_inline || display_type == CSS_VALUE_block) &&
							!html_element->HasBeforeOrAfter())
						{
							if (element_type == Markup::HTE_BR)
								layout_box = OP_NEW(BrBox, (html_element));
							else
								layout_box = OP_NEW(WBrBox, (html_element));

							if (layout_box)
							{
								SetNewLayoutBox(html_element, layout_box);

								return CONTINUE;
							}
							else
								return OUT_OF_MEMORY;
						}
						content_set = FALSE;
						break;

					case Markup::HTE_METER:
					case Markup::HTE_PROGRESS:
						content = OP_NEW(ProgressContent, ());
						break;
					case Markup::HTE_IMG:
					{
						if (html_element->HasAttr(Markup::HA_USEMAP) && (html_element->ShowAltText() || !info.doc->GetWindow()->ShowImages()))
						{
							InsertMapAltTextElements(info);
							content_set = FALSE;
						}
						else
#ifdef ALT_TEXT_AS_TEXT_BOX
							if ( (html_element->ShowAltText() || !info.doc->GetWindow()->ShowImages() ) &&
								 (info.hld_profile->IsInStrictMode() ||
								  (html_element->HasAttr(Markup::HA_ALT) && props.content_width == CONTENT_WIDTH_AUTO && props.content_height == CONTENT_HEIGHT_AUTO) ||
								  (info.doc->GetLayoutMode() == LAYOUT_SSR || info.doc->GetLayoutMode() == LAYOUT_CSSR))
								)
							{
								InsertAltTextElement(info);
								content_set = FALSE;
							}
							else
#endif
								content = OP_NEW(ImageContent, ());
					}
					break;

#if defined _PLUGIN_SUPPORT_ || defined SVG_SUPPORT
#ifdef _APPLET_2_EMBED_
					case Markup::HTE_APPLET:
#endif
					case Markup::HTE_EMBED:
					{
						URL* embed_url = html_element->GetEMBED_URL(info.doc->GetLogicalDocument());
						OP_LOAD_INLINE_STATUS st = OpStatus::OK;
						OpStatus::Ignore(st);

						if (info.doc->IsReflowing()
#ifdef _PLUGIN_SUPPORT_
							&& !info.workplace->PluginsDisabled()
#endif // _PLUGIN_SUPPORT_
							)
							content = info.workplace->GetStoredReplacedContent(html_element);

#ifdef _PLUGIN_SUPPORT_
						if (info.workplace->PluginsDisabled() && content)
						{
							content->Disable(info.doc);
							OP_DELETE(content);
							content=0;
						}
#endif // _PLUGIN_SUPPORT_

#ifdef ON_DEMAND_PLUGIN
						if (html_element->IsPluginPlaceholder() && !content)
						{
							content = OP_NEW(PluginPlaceholderContent, (info.doc, html_element));
							if (!content)
								st = OpStatus::ERR_NO_MEMORY;

							embed_url = NULL;
						}
#endif

						if (embed_url)
						{
							st = LoadInlineURL(info, embed_url, EMBED_INLINE, FALSE, may_need_reflow);

							if (OpStatus::IsError(st) && !OpStatus::IsMemoryError(st))
							{
								OP_BOOLEAN trusted = IsTrustedInline(embed_url);

								if (OpStatus::IsMemoryError(trusted))
									st = OpStatus::ERR_NO_MEMORY;
								else
									if (trusted == OpBoolean::IS_TRUE)
										st = OpStatus::OK;
							}
						}
#ifdef _PLUGIN_SUPPORT_
						if (!OpStatus::IsMemoryError(st))
						{
							if (!info.workplace->PluginsDisabled())
							{
								if (!content)
									content = OP_NEW(EmbedContent, (info.doc));
							}

							if (OpStatus::IsError(st) && content && content->IsEmbed())
							{
								HTML_Element* object_element = html_element->Parent();

								((EmbedContent*) content)->SetPluginLoadingFailed();

								if (object_element && object_element->Type() == Markup::HTE_OBJECT)
								{
									URL tmp_url = object_element->GetImageURL(FALSE, info.doc->GetLogicalDocument());

									embed_url = &tmp_url;

									if (embed_url->IsEmpty() && (object_element->GetEndTagFound() || info.doc->GetLogicalDocument()->IsParsed()))
									{
										// check if there is a <PARAM> with name == filename or movie

										uni_char* param_str = NULL;

										if (OpStatus::IsMemoryError(UniSetStr(param_str, object_element->GetParamURL())))
											return OUT_OF_MEMORY;

										if (param_str)
										{
											if (object_element->SetAttr(Markup::HA_DATA, ITEM_TYPE_STRING, (void*)param_str, TRUE) < 0)
											{
												OP_DELETEA(param_str);
												return OUT_OF_MEMORY;
											}
# if defined(WEB_TURBO_MODE)
											object_element->SetObjectIsEmbed();
# endif // WEB_TURBO_MODE
											embed_url = object_element->GetUrlAttr(Markup::HA_DATA, NS_IDX_HTML, info.doc->GetLogicalDocument());
										}
									}

									st = LoadInlineURL(info, embed_url, GENERIC_INLINE, FALSE, may_need_reflow);
								}
							}
						}

						if (OpStatus::IsMemoryError(st))
#endif // _PLUGIN_SUPPORT_
						{
							OP_DELETE(content);

							if (OpStatus::IsMemoryError(st))
								return OUT_OF_MEMORY;

							content = NULL;
							content_set = FALSE;
						}
					}

					break;
#endif // _PLUGIN_SUPPORT_ || SVG_SUPPORT


#ifdef CANVAS_SUPPORT
					case Markup::HTE_CANVAS:
						if (DOM_Environment::IsEnabled(info.doc) && g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::CanvasEnabled, info.doc->GetURL()))
						{
							if (info.doc->IsReflowing())
								content = info.workplace->GetStoredReplacedContent(html_element);

							if (!content)
								content = OP_NEW(CanvasContent, ());
						}
						else
							content_set = FALSE;
						break;

#endif // CANVAS_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
					case Markup::HTE_AUDIO:
					case Markup::HTE_VIDEO:
						if (info.doc->IsReflowing())
							content = info.workplace->GetStoredReplacedContent(html_element);

						if (!content)
						{
							if (element_type == Markup::HTE_VIDEO)
								content = OP_NEW(VideoWrapperContainer, ());
							else
								content = OP_NEW(MediaContent, ());
						}

						break;

					case Markup::MEDE_VIDEO_DISPLAY:
						content = OP_NEW(VideoContent, ());
						break;

					case Markup::MEDE_VIDEO_TRACKS:
						content = OP_NEW(BlockContainer, ());
						break;

					case Markup::MEDE_VIDEO_CONTROLS:
						content = OP_NEW(MediaControlsContent, ());
						break;
#endif // MEDIA_HTML_SUPPORT

					case Markup::HTE_OBJECT:
#if defined (MEDIA_JIL_PLAYER_SUPPORT) || defined (MEDIA_CAMERA_SUPPORT)
						if (info.doc->IsReflowing()
#ifdef SUPPORT_VISUAL_ADBLOCK
							&& !info.doc->GetWindow()->IsContentBlockerEnabled()
#endif // SUPPORT_VISUAL_ADBLOCK
							)
							content = info.workplace->GetStoredReplacedContent(html_element);

						// Check if there is media content in object element.

						if(!content && html_element->GetMedia())
							content = OP_NEW(MediaContent, ());

						if(!content)
#endif // (MEDIA_JIL_PLAYER_SUPPORT) || defined (MEDIA_CAMERA_SUPPORT)
						{
							content_set = FALSE;
#ifdef PLUGIN_AUTO_INSTALL
							/* Notify the platform plugin manager that a plugin is missing
							   in case it is needed by an OBJECT tag and the displayed content is
							   its fallback content, effectively not creating EmbedContent and not
							   going through EmbedContent::ShowEmbed(). */

							if (html_element)
							{
								const uni_char *c_mime_type = html_element->GetStringAttr(Markup::HA_TYPE);
								if (c_mime_type)
								{
									OpString s_mime_type;
									s_mime_type.Set(c_mime_type);
									info.doc->NotifyPluginMissing(s_mime_type);
								}
							}
#endif // PLUGIN_AUTO_INSTALL
						}

						break;

					case Markup::HTE_IFRAME:
					{
						URL* referenced_url = html_element->GetUrlAttr(html_element->Type() == Markup::HTE_OBJECT ? Markup::HA_DATA : Markup::HA_SRC, NS_IDX_HTML, info.doc->GetLogicalDocument());

						BOOL show_iframe = info.doc->IsAllowedIFrame(referenced_url);

						BOOL is_embedded_svg = FALSE;
#ifdef SVG_SUPPORT
						if (referenced_url && (referenced_url->ContentType() == URL_SVG_CONTENT) &&
							(html_element->Type() != Markup::HTE_IFRAME || (html_element->Type() == Markup::HTE_IFRAME && html_element->GetInserted() == HE_INSERTED_BY_SVG)))
							is_embedded_svg = TRUE;
#endif // SVG_SUPPORT

						if (info.doc->IsGeneratedByOpera() || is_embedded_svg ||
							(show_iframe && g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::IFramesEnabled, info.doc->GetURL())))
							content = OP_NEW(IFrameContent, ());
						else
							content_set = FALSE;
						break;
					}
#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_
					case Markup::HTE_KEYGEN:
#endif
					case Markup::HTE_SELECT:
						content = OP_NEW(SelectContent, ());
						break;

					case Markup::HTE_TEXTAREA:
						content = OP_NEW(TextareaContent, ());
						break;

					case Markup::HTE_BUTTON:
						content = OP_NEW(ButtonContainer, ());
						break;

					case Markup::HTE_INPUT:
						if (html_element->GetInputType() == INPUT_IMAGE)
						{
							URL inline_url = html_element->GetImageURL(FALSE, info.doc->GetLogicalDocument());

							OP_LOAD_INLINE_STATUS st = LoadInlineURL(info, &inline_url, IMAGE_INLINE, FALSE, may_need_reflow);
							if (OpStatus::IsMemoryError(st))
								return OUT_OF_MEMORY;

							if (html_element->HasAttr(Markup::HA_USEMAP) && (html_element->ShowAltText() || !info.doc->GetWindow()->ShowImages()))
							{
								InsertMapAltTextElements(info);
								content_set = FALSE;
							}
							else
#ifdef ALT_TEXT_AS_TEXT_BOX
								if (html_element->ShowAltText() &&
									(info.hld_profile->IsInStrictMode() ||
									 (html_element->HasAttr(Markup::HA_ALT) && props.content_width == CONTENT_WIDTH_AUTO && props.content_height == CONTENT_HEIGHT_AUTO) ||
									 (info.doc->GetLayoutMode() == LAYOUT_SSR || info.doc->GetLayoutMode() == LAYOUT_CSSR))
									)
								{
									InsertAltTextElement(info);
									content_set = FALSE;
								}
								else
#endif
									content = OP_NEW(ImageContent, ());
						}
						else
							content = OP_NEW(InputFieldContent, ());
						break;

					default:
						content_set = FALSE;
						break;
					}
				}
#ifdef MEDIA_HTML_SUPPORT
				else
					if (html_element->IsMatchingType(Markup::CUEE_ROOT, NS_CUE))
					{
						OP_ASSERT(html_element->GetInserted() == HE_INSERTED_BY_TRACK);

						content = OP_NEW(BlockContainer, ());
						content_set = TRUE;
					}
#endif // MEDIA_HTML_SUPPORT
#ifdef SVG_SUPPORT
				else
					if (html_element->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
					{
						content = OP_NEW(SVGContent, ());
						content_set = TRUE;
					}
					else if (html_element->GetNsType() == NS_SVG && html_element->Type() != Markup::SVGE_FOREIGNOBJECT)
					{
						return CONTINUE;
					}
#endif // SVG_SUPPORT

			if (!content_set)
			{
				content_set = TRUE;

				switch (display_type)
				{
				case CSS_VALUE_table:
				case CSS_VALUE_inline_table:
					if (props.border_collapse == CSS_VALUE_collapse)
						content = OP_NEW(TableCollapsingBorderContent, ());
					else
						content = OP_NEW(TableContent, ());

					break;

				case CSS_VALUE_table_row:
				case CSS_VALUE_table_row_group:
				case CSS_VALUE_table_header_group:
				case CSS_VALUE_table_footer_group:
				case CSS_VALUE_table_column:
				case CSS_VALUE_table_column_group:
					content_set = FALSE;
					break;

				case CSS_VALUE_table_cell:
					if (props.IsMultiColumn(element_type))
					{
						content = OP_NEW(MultiColumnContainer, ());
						break;
					}

					// fall-through

				case CSS_VALUE_table_caption:
					content = OP_NEW(Container, ());
					break;

				case CSS_VALUE_compact:
				case CSS_VALUE_run_in:
				case CSS_VALUE_block:
				case CSS_VALUE_inline_block:
				case CSS_VALUE_list_item:
					if (props.IsMultiColumn(element_type))
#ifdef PAGED_MEDIA_SUPPORT
						if (HTMLayoutProperties::IsPagedOverflowValue(props.overflow_x))
							content = OP_NEW(PagedContainer, ());
						else
#endif // PAGED_MEDIA_SUPPORT
							content = OP_NEW(MultiColumnContainer, ());
					else
						if (props.overflow_x != CSS_VALUE_visible)
#ifdef PAGED_MEDIA_SUPPORT
							if (HTMLayoutProperties::IsPagedOverflowValue(props.overflow_x))
								content = OP_NEW(PagedContainer, ());
							else
#endif // PAGED_MEDIA_SUPPORT
								content = OP_NEW(ScrollableContainer, ());
						else
							if (props.IsShrinkToFit(element_type))
								content = OP_NEW(ShrinkToFitContainer, ());
							else
								content = OP_NEW(BlockContainer, ());
					break;

				case CSS_VALUE_flex:
				case CSS_VALUE_inline_flex:
					if (props.overflow_x != CSS_VALUE_visible
#ifdef PAGED_MEDIA_SUPPORT
						&& !HTMLayoutProperties::IsPagedOverflowValue(props.overflow_x) // Not supported by flexbox
#endif // PAGED_MEDIA_SUPPORT
						)
						content = OP_NEW(ScrollableFlexContent, ());
					else
						content = OP_NEW(FlexContent, ());
					break;

				case CSS_VALUE__wap_marquee:
					content = OP_NEW(MarqueeContainer, ());
					break;

				default:
					content = OP_NEW(InlineContent, ());
					break;
				}
			}

			if (content_set && content == NULL)
				return OUT_OF_MEMORY;

			if (flexbox)
				// Create a flex item unless table correction is required first.

				if (props.display_type != CSS_VALUE_table_cell &&
					props.display_type != CSS_VALUE_table_row &&
					props.display_type != CSS_VALUE_table_row_group &&
					props.display_type != CSS_VALUE_table_header_group &&
					props.display_type != CSS_VALUE_table_footer_group &&
					props.display_type != CSS_VALUE_table_column &&
					props.display_type != CSS_VALUE_table_column_group &&
					props.display_type != CSS_VALUE_table_caption)
				{
					/* We should not end up here with an absolutely positioned box, as it
					   should be wrapped inside a special anonymous flex item. */

					OP_ASSERT(props.position != CSS_VALUE_absolute && props.position != CSS_VALUE_fixed);

					content_box = NEW_TRANSFORMABLE_BOX(FlexItemBox, (html_element, content));

					if (!content_box)
					{
						OP_DELETE(content);
						return OUT_OF_MEMORY;
					}

					SetNewLayoutBox(html_element, content_box);
					content->SetPlaceholder(content_box);
					return CONTINUE;
				}

			switch (display_type)
			{
			case CSS_VALUE_table_row:
				if (parent)
				{
					LP_STATE status = CheckAndInsertMissingTableElement(info, Markup::HTE_TBODY);

					if (status != CONTINUE)
						return status;
				}

				if (table->GetCollapseBorders())
					if (props.position == CSS_VALUE_relative)
						if (props.z_index != CSS_ZINDEX_auto)
							layout_box = OP_NEW(PositionedZRootTableCollapsingBorderRowBox, (html_element));
						else
							layout_box = OP_NEW(PositionedTableCollapsingBorderRowBox, (html_element));
					else
						layout_box = OP_NEW(TableCollapsingBorderRowBox, (html_element));
				else
					if (props.position == CSS_VALUE_relative)
						if (props.z_index != CSS_ZINDEX_auto)
							layout_box = OP_NEW(PositionedZRootTableRowBox, (html_element));
						else
							layout_box = OP_NEW(PositionedTableRowBox, (html_element));
					else
						layout_box = OP_NEW(TableRowBox, (html_element));

				break;

			case CSS_VALUE_table_row_group:
			case CSS_VALUE_table_header_group:
			case CSS_VALUE_table_footer_group:
				if (parent)
				{
					LP_STATE status = CheckAndInsertMissingTableElement(info, Markup::HTE_TABLE);

					if (status != CONTINUE)
					{
						OP_ASSERT(content == NULL && content_set == FALSE);
						return status;
					}
				}

				if (props.position == CSS_VALUE_relative)
					if (props.z_index != CSS_ZINDEX_auto)
						layout_box = OP_NEW(PositionedZRootTableRowGroupBox, (html_element));
					else
						layout_box = OP_NEW(PositionedTableRowGroupBox, (html_element));
				else
					layout_box = OP_NEW(TableRowGroupBox, (html_element));

				break;

			case CSS_VALUE_table_column:
			case CSS_VALUE_table_column_group:
				if (parent)
				{
					LP_STATE status = CheckAndInsertMissingTableElement(info, Markup::HTE_TABLE);

					if (status != CONTINUE)
					{
						OP_ASSERT(content == NULL && content_set == FALSE);
						return status;
					}
				}

				layout_box = OP_NEW(TableColGroupBox, (html_element));
				break;

			case CSS_VALUE_table_cell:
				if (parent)
				{
					LP_STATE status = CheckAndInsertMissingTableElement(info, Markup::HTE_TR);

					if (status != CONTINUE)
					{
						OP_DELETE(content);
						return status;
					}
				}

				if (parent && ((TableRowBox*) parent->GetLayoutBox())->GetNextColumn() < MAX_COLUMNS)
				{
					OP_ASSERT(table);

					/* Adding a cell box means that we need to recalculate column min/max
					   widths (if the table has colspanned cells, adding a cell may cause
					   min/max to shrink). */

					table->TableContent::ClearMinMaxWidth();

					if (table->GetCollapseBorders())
						if (props.position == CSS_VALUE_relative)
							if (props.z_index != CSS_ZINDEX_auto)
								content_box = OP_NEW(PositionedZRootTableCollapsingBorderCellBox, (html_element, content));
							else
								content_box = OP_NEW(PositionedTableCollapsingBorderCellBox, (html_element, content));
						else
							content_box = OP_NEW(TableCollapsingBorderCellBox, (html_element, content));
					else
						if (table->NeedSpecialBorders())
							if (props.position == CSS_VALUE_relative)
								if (props.z_index != CSS_ZINDEX_auto)
									content_box = OP_NEW(PositionedZRootTableBorderCellBox, (html_element, content));
								else
									content_box = OP_NEW(PositionedTableBorderCellBox, (html_element, content));
							else
								content_box = OP_NEW(TableBorderCellBox, (html_element, content));
						else
							if (props.position == CSS_VALUE_relative)
								if (props.z_index != CSS_ZINDEX_auto)
									content_box = OP_NEW(PositionedZRootTableCellBox, (html_element, content));
								else
									content_box = OP_NEW(PositionedTableCellBox, (html_element, content));
							else
								content_box = OP_NEW(TableCellBox, (html_element, content));
				}
				else
				{
					OP_DELETE(content);
					return CONTINUE;
				}

				break;

			case CSS_VALUE_table_caption:
				if (props.position == CSS_VALUE_relative)
					if (props.z_index != CSS_ZINDEX_auto)
						content_box = OP_NEW(PositionedZRootTableCaptionBox, (html_element, content));
					else
						content_box = OP_NEW(PositionedTableCaptionBox, (html_element, content));
				else
					content_box = OP_NEW(TableCaptionBox, (html_element, content));

				break;

			case CSS_VALUE_table:
			case CSS_VALUE_block:
			case CSS_VALUE_list_item:
			case CSS_VALUE__wap_marquee:
			case CSS_VALUE_flex:
			{
				BOOL handled_as_float = FALSE;

				if (props.float_type != CSS_VALUE_none)
				{
					if (props.float_type == CSS_VALUE_left || props.float_type == CSS_VALUE_right)
					{
						handled_as_float = TRUE;

						if (HandleAsRelPos(props))
							if (props.z_index != CSS_ZINDEX_auto || IsTransformed(props) || props.opacity != 255)
								content_box = NEW_TRANSFORMABLE_BOX(PositionedZRootFloatingBox, (html_element, content));
							else
							{
								ASSERT_NO_TRANSFORM();
								content_box = OP_NEW(PositionedFloatingBox, (html_element, content));
							}
						else
						{
							if (props.opacity == 255)
								content_box = OP_NEW(FloatingBox, (html_element, content));
							else
								content_box = OP_NEW(ZRootFloatingBox, (html_element, content));
						}
					}
					else
						if (multipane_container && CSS_is_gcpm_float_val(props.float_type))
							/* A GCPM float must live in the same block formatting context as the
							   one established by the nearest ancestral multi-pane container. */

							if (space_manager == multipane_container->GetPlaceholder()->GetLocalSpaceManager())
							{
								handled_as_float = TRUE;

								if (HandleAsRelPos(props))
									if (props.z_index != CSS_ZINDEX_auto || IsTransformed(props) || props.opacity != 255)
										content_box = NEW_TRANSFORMABLE_BOX(PositionedZRootFloatedPaneBox, (html_element, content));
									else
									{
										ASSERT_NO_TRANSFORM();
										content_box = OP_NEW(PositionedFloatedPaneBox, (html_element, content));
									}
								else
								{
									if (props.opacity == 255)
										content_box = OP_NEW(FloatedPaneBox, (html_element, content));
									else
										content_box = OP_NEW(ZRootFloatedPaneBox, (html_element, content));
								}
							}
				}

				if (!handled_as_float)
					if (HandleAsRelPos(props))
					{
						if (props.z_index != CSS_ZINDEX_auto || IsTransformed(props) || props.opacity != 255)
						{
							if (props.NeedsSpaceManager(element_type))
								content_box = NEW_TRANSFORMABLE_BOX(PositionedSpaceManagerZRootBox, (html_element, content));
							else
								content_box = NEW_TRANSFORMABLE_BOX(PositionedZRootBox, (html_element, content));
						}
						else
						{
							ASSERT_NO_TRANSFORM();

							if (props.NeedsSpaceManager(element_type))
								content_box = OP_NEW(PositionedSpaceManagerBlockBox, (html_element, content));
							else
								content_box = OP_NEW(PositionedBlockBox, (html_element, content));
						}
					}
					else
						if (props.position != CSS_VALUE_static)
						{
							if (props.z_index != CSS_ZINDEX_auto || IsTransformed(props) || props.opacity != 255)
								content_box = NEW_TRANSFORMABLE_BOX(AbsoluteZRootBox, (html_element, content));
							else
							{
								ASSERT_NO_TRANSFORM();
								content_box = OP_NEW(AbsolutePositionedBox, (html_element, content));
							}
						}
						else
						{
							ASSERT_NO_TRANSFORM();

							if (props.NeedsSpaceManager(element_type))
							{
								if (props.opacity == 255)
									content_box = OP_NEW(SpaceManagerBlockBox, (html_element, content));
								else
									content_box = OP_NEW(ZRootSpaceManagerBlockBox, (html_element, content));
							}
							else
							{
								if (props.opacity == 255)
									content_box = OP_NEW(BlockBox, (html_element, content));
								else
									content_box = OP_NEW(ZRootBlockBox, (html_element, content));
							}
						}

				break;
			}

			case CSS_VALUE_compact:
				content_box = OP_NEW(BlockCompactBox, (html_element, content));
				break;

			case CSS_VALUE_run_in:
#ifdef CSS_TRANSFORMS
				if (props.transforms_cp)
					content_box = OP_NEW(TransformedPositionedZRootBox, (html_element, content));
				else
#endif
					content_box = OP_NEW(BlockBox, (html_element, content));
				break;

			case CSS_VALUE_inline_block:
			case CSS_VALUE_inline_table:
			case CSS_VALUE_inline_flex:
				if (HandleAsRelPos(props))
				{
					if (props.z_index != CSS_ZINDEX_auto || IsTransformed(props) || props.opacity != 255)
						content_box = NEW_TRANSFORMABLE_BOX(InlineBlockZRootBox, (html_element, content));
					else
					{
						ASSERT_NO_TRANSFORM();
						content_box = OP_NEW(PositionedInlineBlockBox, (html_element, content));
					}
				}
				else
				{
					if (props.opacity == 255)
						content_box = OP_NEW(InlineBlockBox, (html_element, content));
					else
						content_box = OP_NEW(ZRootInlineBlockBox, (html_element, content));
				}

				break;

			default:
				if (HandleAsRelPos(props))
				{
					if (props.z_index != CSS_ZINDEX_auto || IsTransformed(props) || props.opacity != 255)
						content_box = NEW_TRANSFORMABLE_BOX(InlineZRootBox, (html_element, content));
					else
					{
						ASSERT_NO_TRANSFORM();
						content_box = OP_NEW(PositionedInlineBox, (html_element, content));
					}
				}
				else
				{
					if (props.opacity == 255)
						content_box = OP_NEW(InlineBox, (html_element, content));
					else
						content_box = OP_NEW(ZRootInlineBox, (html_element, content));
				}

				break;
			}
		}

		if (content_box)
		{
			if (!content)
			{
				OP_DELETE(content_box);

				return OUT_OF_MEMORY;
			}

			content->SetPlaceholder(content_box);
			layout_box = content_box;
		}
		else
		{
			if (!layout_box)
			{
				OP_DELETE(content);

				return OUT_OF_MEMORY;
			}

			OP_ASSERT(!content); // leaking memory here
		}

		SetNewLayoutBox(html_element, layout_box);

		return CONTINUE;
	}
}

LayoutProperties::LP_STATE
LayoutProperties::CreateChildLayoutBox(LayoutInfo& info, HTML_Element* child)
{
	OP_PROBE6(OP_PROBE_LAYOUTPROPERTIES_CREATECHILDLAYOUTBOX);

	OP_ASSERT(!child->HasDirtyChildProps());

	if (child->IsPropsDirty())
	{
		/* Should really only need to end up here for layout-inserted elements.
		   When we rewrite generated content handling (CORE-44094), we should
		   be able to drop the call to LoadCssProperties. Other elements which
		   are inserted by layout get their non-initial computed style set in
		   HTMLayoutProperties::SetCssPropertiesFromHtmlAttr and can just get
		   MarkPropsClean below. */

		OP_ASSERT(child->GetInserted() == HE_INSERTED_BY_LAYOUT);

		if (child->GetPseudoElement())
		{
			CssPropertyItem::LoadCssProperties(child, g_op_time_info->GetRuntimeMS(), info.hld_profile, info.media_type);
			if (info.hld_profile->GetIsOutOfMemory())
				return OUT_OF_MEMORY;
		}
		else
			child->MarkPropsClean();
	}

	Box *old_layout_box = child->GetLayoutBox();
	LayoutProperties* child_cascade = GetChildCascade(info, child, old_layout_box && old_layout_box->IsInlineRunInBox());

	if (!child_cascade)
		return OUT_OF_MEMORY;

	/* Attempt to generate a layout box for the child.

	   The box generation recursion goes like this:

	       CreateLayoutBox() -> LayoutElement() -> Box::LayoutChildren() -> CreateChildLayoutBox() -> CreateLayoutBox() -> ... */

	return child_cascade->CreateLayoutBox(info);
}

#ifdef MEDIA_HTML_SUPPORT

OP_STATUS
LayoutProperties::InsertVideoElements(LayoutInfo& info)
{
	HTML_Element* video_display = NEW_HTML_Element();
	HTML_Element* video_tracks = NEW_HTML_Element();
	HTML_Element* video_controls = NEW_HTML_Element();

	if (!video_display || !video_tracks || !video_controls ||
		/* Create an element for displaying the video data itself. */

		OpStatus::IsMemoryError(video_display->Construct(info.hld_profile, NS_IDX_HTML,
														 Markup::MEDE_VIDEO_DISPLAY,
														 NULL, HE_INSERTED_BY_LAYOUT)) ||
		/* Create an element for displaying the video <track> data. */

		OpStatus::IsMemoryError(video_tracks->Construct(info.hld_profile, NS_IDX_HTML,
														Markup::MEDE_VIDEO_TRACKS,
														NULL, HE_INSERTED_BY_LAYOUT)) ||
		/* Create an element for displaying the video controls. */

		OpStatus::IsMemoryError(video_controls->Construct(info.hld_profile, NS_IDX_HTML,
														  Markup::MEDE_VIDEO_CONTROLS,
														  NULL, HE_INSERTED_BY_LAYOUT)))
	{
		DELETE_HTML_Element(video_display);
		DELETE_HTML_Element(video_tracks);
		DELETE_HTML_Element(video_controls);
		return OpStatus::ERR_NO_MEMORY;
	}

	video_display->Under(html_element);
	video_tracks->Under(html_element);
	video_controls->Under(html_element);

	if (MediaElement* media_element = html_element->GetMediaElement())
		media_element->AttachCues(video_tracks);

	return OpStatus::OK;
}

OP_STATUS
LayoutProperties::InsertCueBackgroundBox(LayoutInfo& info)
{
	OP_ASSERT(html_element->IsMatchingType(Markup::CUEE_ROOT, NS_CUE));

	HTML_Element* background_box = NEW_HTML_Element();

	if (!background_box ||
		OpStatus::IsMemoryError(background_box->Construct(info.hld_profile, NS_IDX_CUE,
														  Markup::CUEE_BACKGROUND,
														  NULL, HE_INSERTED_BY_LAYOUT)))
	{
		DELETE_HTML_Element(background_box);
		return OpStatus::ERR_NO_MEMORY;
	}

	// Move root's children to background box.

	while (HTML_Element* child = html_element->FirstChild())
	{
		child->Out();
		child->Under(background_box);
	}

	background_box->Under(html_element);
	return OpStatus::OK;
}

#endif // MEDIA_HTML_SUPPORT

LayoutProperties::LP_STATE
LayoutProperties::CreateLayoutBox(LayoutInfo& info)
{
	html_element->SetDirty(ELM_NOT_DIRTY|ELM_MINMAX_DELETED);

	OP_ASSERT(!html_element->HasNoLayout());

	Markup::Type element_type = html_element->Type();

	CSSValue display_type = (CSSValue) props.display_type;
	BOOL may_need_reflow = FALSE;

	if (html_element->GetLayoutBox())
	{
		if (html_element->GetFormObject())
			props.SetDisplayProperties(info.visual_device);

		/* lock the images in the cache for some time to avoid
		   re-decoding */
		imgManager->BeginGraceTime();
		html_element->RemoveLayoutBox(info.doc);
		imgManager->EndGraceTime();
	}

	OP_MEMORY_VAR ListMarkerContentType list_marker_content_type = LIST_MARKER_NONE;
	BOOL no_layout_box = display_type == CSS_VALUE_none || (props.visibility != CSS_VALUE_visible && info.doc->GetHandheldEnabled());
#ifdef SVG_SUPPORT
	no_layout_box = no_layout_box || (html_element->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG) && html_element->HasAttr(Markup::XLINKA_HREF, NS_IDX_XLINK));
#endif // SVG_SUPPORT

	if (html_element->GetIsListMarkerPseudoElement())
	{
		list_marker_content_type = GetListMarkerContentType(info.doc);
		if (list_marker_content_type == LIST_MARKER_NONE)
			/* We end up here when list-style-type is 'none' and the
			   list-style-image resource is not yet available for
			   display. In such a case we don't need a box for the
			   list item marker. */
			return CONTINUE;
	}

	if (no_layout_box)
		return CONTINUE;

	if ((html_element->GetIsBeforePseudoElement() || html_element->GetIsAfterPseudoElement()) && !HasRealContent())
		/* There won't be any generated content under this ::before or ::after pseudo
		   element. Don't generate a box on it then, and more importantly, don't process
		   counter-increase and counter-reset properties on it. */

		return CONTINUE;

#ifdef SUPPORT_VISUAL_ADBLOCK
	if (g_urlfilter->HasExcludeRules() &&
		info.visual_device->GetWindow()->IsContentBlockerEnabled() &&
		html_element->GetNsType() == NS_HTML)
	{
		if (IsElementBlocked(info))
			return CONTINUE;
	}
#endif // SUPPORT_VISUAL_ADBLOCK

	HTML_Element* containing_element = Box::GetContainingElement(html_element);
	NS_Type ns_type = html_element->GetNsType();

	if (containing_element)
	{
		Box* containing_box = containing_element->GetLayoutBox();

		if (containing_element->GetNsType() == NS_HTML)
		{
			if (containing_box->IsContentReplaced())
				switch (containing_element->Type())
				{
				case Markup::HTE_OBJECT:
				case Markup::HTE_APPLET:
				case Markup::HTE_IFRAME:
				case Markup::HTE_EMBED:
#ifdef CANVAS_SUPPORT
				case Markup::HTE_CANVAS:
#endif // CANVAS_SUPPORT
					return CONTINUE;
				}

#ifdef MEDIA_HTML_SUPPORT
			if (containing_element->Type() == Markup::HTE_VIDEO)
				switch (html_element->Type())
				{
				case Markup::MEDE_VIDEO_DISPLAY:
				case Markup::MEDE_VIDEO_TRACKS:
				case Markup::MEDE_VIDEO_CONTROLS:
					break;
				default:
					return CONTINUE;
				}
#endif // MEDIA_HTML_SUPPORT
		}

		if (Pred()->flexbox)
		{
			HTML_Element* parent = Pred()->html_element;

			if (IsAnonymousFlexItem(parent))
			{
				// We are inside an anonymous flex item wrapper.

				BOOL promote = FALSE;

				OP_ASSERT(!flexbox);
				OP_ASSERT(parent->GetLayoutBox()->IsFlexItemBox());

				if (html_element->Pred())
					if (props.position == CSS_VALUE_absolute || props.position == CSS_VALUE_fixed)
						/* Only one absolutely positioned box per anonymous flex item is
						   allowed. Since each absolutely positioned box may propagate
						   properties to its item, it needs to have it all to itself. */

						promote = TRUE;
					else
						/* If the item we're inside is a special one for an absolutely
						   positioned box, we cannot add anything more there. */

						promote = ((FlexItemBox*) parent->GetLayoutBox())->IsAbsolutePositionedSpecialItem();

				if (!promote)
					promote = !NeedsFlexItemParent(info, this);

				if (promote)
				{
					/* This element doesn't belong inside the current anonymous flex
					   item. It can either constitute an item on its own, or it is an
					   absolutely positioned box that isn't the first child of the
					   item. Move this element and all its consecutive siblings up one
					   level, out of the anonymous wrapper. */

					PromoteChildren(info.doc, Pred()->html_element, html_element);
					Clean();

					return ELEMENT_MOVED_UP;
				}
			}
		}

		if (flexbox)
		{
			if (NeedsFlexItemParent(info, this))
			{
				OP_ASSERT(html_element->Type() != Markup::LAYOUTE_ANON_FLEX_ITEM);
				return InsertFlexItemParent(info, this);
			}
		}
		else if (!containing_box->GetContainer())
		{
#ifdef SVG_SUPPORT
			if (ns_type == NS_SVG)
			{
				// foreignObject establishes a root layout box.

				if (element_type != Markup::SVGE_FOREIGNOBJECT)
					/* This is not a foreignObject, but maybe we have a descendant
					   foreignObject, in which case we need to generate NoDisplayBox
					   objects all the way down to it. */

					for (HTML_Element* child = html_element->FirstChild(); child; child = child->Suc())
					{
						LP_STATE status = CreateChildLayoutBox(info, child);

						OP_ASSERT(status != ELEMENT_MOVED_UP && status != INSERTED_PARENT);

						if (status != CONTINUE)
							return status;
					}

				return CONTINUE;
			}
			else
				if (!html_element->Parent()->GetLayoutBox())
					/* When looking for an SVG foreignObject descendant, we may have
					   to walk a boxless subtree, but if we have no container, stop
					   looking when encountering something from a non-SVG namespace,
					   or we'll crash in various unpleasant ways. */

					return CONTINUE;
				else
#endif // SVG_SUPPORT
					if (containing_box->IsTableColGroup())
						/* The only allowed child box type of TableColGroupBox is TableColGroupBox.
						   The only allowed nesting of such boxes is table-column inside of table-column-group. */

						if (display_type != CSS_VALUE_table_column || !((TableColGroupBox*)containing_box)->IsGroup())
							return CONTINUE;

			if (display_type != CSS_VALUE_table_column &&
				display_type != CSS_VALUE_table_column_group &&
				display_type != CSS_VALUE_table_row &&
				display_type != CSS_VALUE_table_row_group &&
				display_type != CSS_VALUE_table_header_group &&
				display_type != CSS_VALUE_table_footer_group)
			{
				if (containing_box->IsTableRow() || containing_box->IsTableRowGroup() || containing_box->GetTableContent())
				{
					if (display_type != CSS_VALUE_table_cell && display_type != CSS_VALUE_table_caption)
						/* Non-table* display type inside a table structural element (row,
						   row-group or table). We have to insert a table cell if this element
						   needs a layout box, to complete the table structure. */

						if (NeedsTableCellParent(info, html_element, (CSSValue) props.white_space))
						{
							OP_ASSERT(html_element->Parent() && containing_box == html_element->Parent()->GetLayoutBox());

							// Insert missing TD

							LP_STATE state = CheckAndInsertMissingTableElement(info, Markup::HTE_TD);

							switch (state)
							{
							case OUT_OF_MEMORY:
							case INSERTED_PARENT:
							case ELEMENT_MOVED_UP:
								return state;
							}

							OP_ASSERT(state == CONTINUE);
						}
				}
				else
					if (ns_type == NS_HTML || element_type == Markup::HTE_TEXT || element_type == Markup::HTE_TEXTGROUP)
					{
						/* This must be a descendant of an option or a textarea element,
						   we are only interested in text and option elements. */

						switch (element_type)
						{
						case Markup::HTE_TEXT:
						case Markup::HTE_TEXTGROUP:
							switch (containing_element->Type())
							{
							case Markup::HTE_OPTION:
							case Markup::HTE_TEXTAREA:
								if (OpStatus::IsMemoryError(containing_box->LayoutFormContent(info.workplace)))
									return OUT_OF_MEMORY;
							}
							break;

						case Markup::HTE_OPTION:
						case Markup::HTE_OPTGROUP:
							if (Box* parent_box = html_element->Parent()->GetLayoutBox())
							{
								if (parent_box->IsContentBox() && parent_box->GetContent()->GetSelectContent() && parent_box->GetHtmlElement()->Type() == Markup::HTE_SELECT ||
									parent_box->IsOptionGroupBox())
								{
									Box* layout_box;
									if (element_type == Markup::HTE_OPTION)
									{
										OptionBox* box = OP_NEW(OptionBox, (html_element));
										if (!box || OpStatus::IsError(box->Construct(containing_box, info.workplace)))
										{
											OP_DELETE(box);
											return OUT_OF_MEMORY;
										}
										layout_box = box;
									}
									else
										layout_box = OP_NEW(OptionGroupBox, (html_element, containing_box));

									if (!layout_box)
										return OUT_OF_MEMORY;

									SetNewLayoutBox(html_element, layout_box);

									if (element_type == Markup::HTE_OPTGROUP)
									{
										/* Layout potential option children */
										layout_box->Layout(this,info);
									}
								}
							}
							break;

						default:
							/* Ignore all other elements. */
							break;
						}

						return CONTINUE;
					}
					else
						// fallback for other namespaces

						return CONTINUE;
			}

			if (info.doc->GetHandheldEnabled() &&
				html_element->IsMatchingType(Markup::HTE_BR, NS_HTML) &&
				container &&
				container->IgnoreSSR_BR(html_element))
				element_type = Markup::HTE_TEXT;
		}
	}

#ifdef JS_PLUGIN_SUPPORT
	/* If this OBJECT belongs to a jsplugin, we do not normally
	   give it a visual representation, unless it is a special
	   ATVEF plug-in, which gets a TV visualisation and is
	   handed positioning callbacks. */
	BOOL is_invisible_jsplugin = FALSE;
	OP_BOOLEAN is_jsplugin = html_element->IsJSPluginObject(info.hld_profile, NULL);
	if (OpStatus::IsMemoryError(is_jsplugin))
		return OUT_OF_MEMORY;
	else if (is_jsplugin == OpBoolean::IS_TRUE)
	{
# ifdef JS_PLUGIN_ATVEF_VISUAL
		ES_Object* esobj = DOM_Utils::GetJSPluginObject(html_element->GetESElement());
		if (esobj != NULL)
		{
			/* Get the JS_Plugin_HTMLObjectElement_Object that corresponds
			   to the OBJECT tag. */
			EcmaScript_Object* ecmascriptobject =
				ES_Runtime::GetHostObject(esobj);
			OP_ASSERT(ecmascriptobject->IsA(ES_HOSTOBJECT_JSPLUGIN));
			JS_Plugin_Object* jsobject =
				static_cast<JS_Plugin_Object*>(ecmascriptobject);
			/* Check if we requested visualisation when initializing this
			   OBJECT. */
			if (!jsobject->IsAtvefVisualPlugin())
				is_invisible_jsplugin = TRUE;
		}
# else
		/* Always invisible when JS_PLUGIN_ATVEF_VISUAL is unset. */
		is_invisible_jsplugin = TRUE;
# endif
	}
#endif

#ifdef _APPLET_2_EMBED_
	/* Check if fallback content should be shown instead of applet content */
	BOOL applet_fallback = element_type == Markup::HTE_APPLET &&
		(!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled, info.doc->GetURL()) || // if plugins are disabled, show fallback content
		info.doc->GetWindow()->GetForcePluginsDisabled() ||
		html_element->CheckLocalCodebase(info.doc->GetLogicalDocument(), element_type) == Markup::HTE_OBJECT); // if non-local applet has local codebase url, show fallback content
#endif // _APPLET_2_EMBED_

	if (ns_type == NS_HTML && (element_type == Markup::HTE_OBJECT
#ifdef _APPLET_2_EMBED_
		 || applet_fallback
#endif // _APPLET_2_EMBED_
			) &&
#ifdef JS_PLUGIN_SUPPORT
		!is_invisible_jsplugin &&
#endif
		!props.content_cp)
	{
		URL tmp_url = html_element->GetImageURL(FALSE, info.doc->GetLogicalDocument());
		URL* inline_url = &tmp_url;

		if (inline_url->IsEmpty() && (html_element->GetEndTagFound() || info.doc->GetLogicalDocument()->IsParsed()) && !html_element->GetFirstElm(Markup::HTE_EMBED))
		{
			// check if there is a <PARAM> with name == filename or movie

			uni_char* param_str = NULL;
			if (OpStatus::IsMemoryError(UniSetStr(param_str, html_element->GetParamURL())))
				return OUT_OF_MEMORY;

			if (param_str)
			{
				if (html_element->SetAttr(Markup::HA_DATA, ITEM_TYPE_STRING, (void*)param_str, TRUE) < 0)
				{
					OP_DELETEA(param_str);
					return OUT_OF_MEMORY;
				}
				inline_url = html_element->GetUrlAttr(Markup::HA_DATA, NS_IDX_HTML, info.doc->GetLogicalDocument());
			}
		}

#ifdef ON_DEMAND_PLUGIN
		BOOL block_inline_loading = FALSE;
		if (info.doc->IsPluginPlaceholderCandidate(html_element))
		{
			// We need to filter out plugins for the object tag so that they are not loaded when in turbo mode.
			// This is done to avoid loading plugins that we intend to display with the plugin place holder icon anyway.
			// If we did not block these we would just end up cancelling these requests later causing pipelining issues.
			// The way this is implemented is not the best solution, as documented in the code review:
			// https://codereview.oslo.osa/r/4204/
			// Much of the code here belong in GetResolvedObjectType.

			OpString mime_type;
			OP_STATUS oom = mime_type.Set(html_element->GetStringAttr(Markup::HA_TYPE));
			if (OpStatus::IsMemoryError(oom))
				return OUT_OF_MEMORY;

			if (!mime_type.IsEmpty())
			{
				Viewer* viewer = g_viewers->FindViewerByMimeType(mime_type);
				if (viewer && viewer->GetAction() == VIEWER_PLUGIN)
					block_inline_loading = TRUE;
			}
			else
			{
				OpString file_ext;
				oom = inline_url->GetAttribute(URL::KUniNameFileExt_L, file_ext, URL::KNoRedirect);
				if (OpStatus::IsMemoryError(oom))
					return OUT_OF_MEMORY;

				Viewer* viewer = g_viewers->FindViewerByExtension(file_ext);
				if (viewer && viewer->GetAction() == VIEWER_PLUGIN)
					block_inline_loading = TRUE;
			}
		}
#endif // ON_DEMAND_PLUGIN

		OP_LOAD_INLINE_STATUS st = OpStatus::OK;
#ifdef ON_DEMAND_PLUGIN
		if (!block_inline_loading)
#endif // ON_DEMAND_PLUGIN
			st = LoadInlineURL(info, inline_url, GENERIC_INLINE, FALSE, may_need_reflow);

		if (OpStatus::IsMemoryError(st))
			return OUT_OF_MEMORY;

		OP_BOOLEAN resolve_status = OpBoolean::IS_FALSE;

		if (OpStatus::IsError(st))
		{
			OP_BOOLEAN trusted = IsTrustedInline(inline_url);

			if (OpStatus::IsMemoryError(trusted))
				return OUT_OF_MEMORY;
			else if (trusted == OpBoolean::IS_TRUE)
			{
				/* Trusted URLs that won't be loaded by Opera, but it may be loaded by
				   an external application. */
				element_type = Markup::HTE_EMBED;
			}
			else
			{
				/* Won't load this inline URL. Fall back to child content. */
				element_type = Markup::HTE_OBJECT;
			}
		}
		else
#ifdef _APPLET_2_EMBED_
		if (applet_fallback)
			resolve_status = OpBoolean::IS_TRUE;
		else
#endif // _APPLET_2_EMBED_
		{
			resolve_status = html_element->GetResolvedObjectType(inline_url, element_type, info.hld_profile->GetLogicalDocument());
#ifdef ON_DEMAND_PLUGIN
			if (resolve_status == OpBoolean::IS_FALSE && block_inline_loading)
			{
				resolve_status = OpBoolean::IS_TRUE;
				element_type = Markup::HTE_EMBED;
			}
#endif // ON_DEMAND_PLUGIN
		}

		if (resolve_status == OpBoolean::IS_FALSE)
		{
			if (!inline_url || inline_url->IsEmpty() || OpStatus::IsSuccess(st))
				/* Don't know how to handle object yet, wait for inline_url or object to
				   complete. */

				return CONTINUE;
		}
		else
			if (resolve_status == OpBoolean::IS_TRUE)
			{
				InlineResourceType inline_type = IMAGE_INLINE;

				BOOL convert_to_object =
				((element_type == Markup::HTE_IMG || element_type == Markup::HTE_EMBED) && !info.doc->GetShowImages()) ||
					(element_type == Markup::HTE_EMBED
#ifdef _PLUGIN_SUPPORT_
					&& !g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled, info.doc->GetURL())
#endif
						);

#ifdef _APPLET_2_EMBED_
				/* If plugins are disabled, fallback content should be shown instead of applet content */
				applet_fallback = applet_fallback ||
					(element_type == Markup::HTE_APPLET &&
					(!g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::PluginsEnabled, info.doc->GetURL()) ||
					info.doc->GetWindow()->GetForcePluginsDisabled()));

				convert_to_object = convert_to_object || applet_fallback;
#endif // _APPLET_2_EMBED_

				if (convert_to_object)
					element_type = Markup::HTE_OBJECT;

				switch (element_type)
				{
				case Markup::HTE_IMG:
					break;

#ifdef _PLUGIN_SUPPORT_
				case Markup::HTE_APPLET:
				case Markup::HTE_EMBED:
					inline_type = EMBED_INLINE;

# ifdef ON_DEMAND_PLUGIN
					if (info.doc->IsPluginPlaceholderCandidate(html_element))
					{
						html_element->SetIsPluginPlaceholder();
						inline_url = NULL;
					}
# endif // ON_DEMAND_PLUGIN
					break;
#endif // _PLUGIN_SUPPORT_

				default:
					inline_url = NULL;
					break;
				}

				OP_LOAD_INLINE_STATUS st = LoadInlineURL(info, inline_url, inline_type, FALSE, may_need_reflow);
				if (OpStatus::IsError(st))
				{
					element_type = Markup::HTE_OBJECT;
					if (OpStatus::IsMemoryError(st))
						return OUT_OF_MEMORY;
				}
			}
			else
			{
				OP_ASSERT(OpStatus::IsMemoryError(resolve_status));
				return OUT_OF_MEMORY;
			}
	}
	else if (element_type == Markup::HTE_EMBED && ns_type == NS_HTML && !props.content_cp)
	{
		/* Primarily adhere to the mimetype in the type attribute if there is one (html5 spec). */
		URL* tmp_url = html_element->GetEMBED_URL(info.doc->GetLogicalDocument());
		OP_LOAD_INLINE_STATUS st = OpStatus::OK;
		if (tmp_url)
		{
			st = LoadInlineURL(info, tmp_url, GENERIC_INLINE, FALSE, may_need_reflow);

			if (OpStatus::IsMemoryError(st))
				return OUT_OF_MEMORY;
		}

		OP_BOOLEAN resolve_status = html_element->GetResolvedEmbedType(tmp_url, element_type, info.hld_profile->GetLogicalDocument());
		if (resolve_status == OpBoolean::IS_FALSE)
		{
			if (!tmp_url || tmp_url->IsEmpty() || OpStatus::IsSuccess(st))
				/* Don't know how to handle object yet, wait for inline_url or object to
				   complete. */

				return CONTINUE;
		}
		else if (resolve_status == OpBoolean::IS_TRUE)
		{
			InlineResourceType inline_type = EMBED_INLINE;
			switch (element_type)
			{
			case Markup::HTE_IMG:
				inline_type = IMAGE_INLINE;
				break;

			case Markup::HTE_EMBED:
#ifdef ON_DEMAND_PLUGIN
				if (info.doc->IsPluginPlaceholderCandidate(html_element))
					html_element->SetIsPluginPlaceholder();
#endif // ON_DEMAND_PLUGIN
				break;

			default:
				tmp_url = NULL;
				break;
			}


			st = LoadInlineURL(info, tmp_url, inline_type, FALSE, may_need_reflow);
			if (OpStatus::IsError(st))
			{
				element_type = Markup::HTE_EMBED;
				if (OpStatus::IsMemoryError(st))
					return OUT_OF_MEMORY;
			}
		}
		else
		{
			OP_ASSERT(OpStatus::IsMemoryError(resolve_status));
			return OUT_OF_MEMORY;
		}
	}
#ifdef ON_DEMAND_PLUGIN
	else if (element_type == Markup::HTE_APPLET && ns_type == NS_HTML)
	{
		if (info.doc->IsPluginPlaceholderCandidate(html_element))
			html_element->SetIsPluginPlaceholder();
	}
#endif // ON_DEMAND_PLUGIN
	else if (ns_type == NS_HTML && (element_type == Markup::HTE_IMG || (element_type == Markup::HTE_INPUT && html_element->GetInputType() == INPUT_IMAGE)))
	{
		URL tmp_url = html_element->GetImageURL(FALSE, info.doc->GetLogicalDocument());
		URL* inline_url = &tmp_url;

		BOOL from_user_css =
			html_element->GetInserted() == HE_INSERTED_BY_LAYOUT &&
			Pred()->props.content_cp &&
			Pred()->props.content_cp->GetUserDefined();

		/* Part of assert guard for CORE-21650. See below. */

		OP_ASSERT(html_element);

		OP_LOAD_INLINE_STATUS st = LoadInlineURL(info, inline_url, IMAGE_INLINE, from_user_css, may_need_reflow);

		if (OpStatus::IsMemoryError(st))
			return OUT_OF_MEMORY;

		/* The crash below was tracked with a lot of duplicates in CORE-21650. The hypothesis is that LoadInlineURL
		   messes with the cascade. html_element is NULL here, but was ok when checking "from_user_css" above.
		   This fix is pretty lame, but should help us from the immediate crash if this happens again.
		   mg@opera.com (or core-layout) will be interested if the assert below happens. Particularly if the assert above
		   was not triggered. */

		OP_ASSERT(html_element);
		if (!html_element)
			return OUT_OF_MEMORY;

		OP_BOOLEAN resolve_status = html_element->GetResolvedImageType(inline_url, element_type, !info.hld_profile->IsParsed(), info.doc);
		if (OpStatus::IsMemoryError(resolve_status))
			return OUT_OF_MEMORY;

		if (resolve_status == OpBoolean::IS_FALSE)
			return CONTINUE;
	}
#ifdef MEDIA_HTML_SUPPORT
	else if (element_type == Markup::HTE_VIDEO && ns_type == NS_HTML)
	{
		URL tmp_url = html_element->GetImageURL(FALSE, info.doc->GetLogicalDocument());
		URL* inline_url = &tmp_url;

		BOOL from_user_css =
			html_element->GetInserted() == HE_INSERTED_BY_LAYOUT &&
			Pred()->props.content_cp &&
			Pred()->props.content_cp->GetUserDefined();

		OP_LOAD_INLINE_STATUS st = LoadInlineURL(info, inline_url, VIDEO_POSTER_INLINE, from_user_css, may_need_reflow);

		if (OpStatus::IsMemoryError(st))
			return OUT_OF_MEMORY;

		OP_STATUS status = InsertVideoElements(info);
		if (OpStatus::IsMemoryError(status))
			return OUT_OF_MEMORY;

		/* Rewrite the display type so that the element gets a block
		   formatting context. */

		if (display_type == CSS_VALUE_inline)
			display_type = CSS_VALUE_inline_block;
	}
	else if (element_type == Markup::CUEE_ROOT && ns_type == NS_CUE)
	{
		// Insert the background box.

		if (OpStatus::IsMemoryError(InsertCueBackgroundBox(info)))
			return OUT_OF_MEMORY;
	}
#endif // MEDIA_HTML_SUPPORT

	if (info.workplace->HasDocRootProps(html_element, element_type))
	{
		if (OpStatus::IsMemoryError(SetDocRootProps(info.hld_profile, element_type)))
			return OUT_OF_MEMORY;

#ifdef PAGED_MEDIA_SUPPORT
		if (HTML_Element* root_elm = info.doc->GetDocRoot())
			if (Box* root_box = root_elm->GetLayoutBox())
				if (Container* root_container = root_box->GetContainer())
					if (!HTMLayoutProperties::IsPagedOverflowValue(info.workplace->GetViewportOverflowX()) !=
						(root_container->GetPagedContainer() == NULL))
						info.workplace->SetReflowElement(root_elm);
#endif // PAGED_MEDIA_SUPPORT
	}

	LP_STATE state;

	if (html_element->GetIsListMarkerPseudoElement())
		state = CreateListMarkerBox(list_marker_content_type);
	else
		state = CreateBox(info, element_type, display_type, may_need_reflow);

	if (state != CONTINUE || !html_element->GetLayoutBox())
		/* Don't lay out the element if we're told not to continue, or if
		   it didn't get a box (e.g. INPUT TYPE="hidden", and also in many
		   cases text nodes consisting only of whitespace). */

		return state;

	if (element_type != Markup::HTE_TEXT)
	{
		// Load background images

		OP_LOAD_INLINE_STATUS load_inline_stat = props.bg_images.LoadImages(info.media_type, info.doc, html_element);

		// Load list style image

		if (props.list_style_image && display_type == CSS_VALUE_list_item && load_inline_stat != OpStatus::ERR_NO_MEMORY)
		{
			URL li_url = g_url_api->GetURL(*info.hld_profile->BaseURL(), props.list_style_image);

			if (!li_url.IsEmpty())
#ifdef _PRINT_SUPPORT_
				if (info.media_type == CSS_MEDIA_TYPE_PRINT)
					load_inline_stat = info.hld_profile->AddPrintImageElement(html_element, IMAGE_INLINE, &li_url);
				else
#endif
					load_inline_stat = info.doc->LoadInline(&li_url, html_element, IMAGE_INLINE, LoadInlineOptions().FromUserCSS(props.list_style_image.IsUserDefined()));
		}

		// Load border-image

		if (props.border_image.border_img && load_inline_stat != OpStatus::ERR_NO_MEMORY)
		{
			URL bg_url = g_url_api->GetURL(*info.hld_profile->BaseURL(), props.border_image.border_img);
			load_inline_stat = info.doc->LoadInline(&bg_url, html_element, BORDER_IMAGE_INLINE, LoadInlineOptions().FromUserCSS(props.border_image.border_img.IsUserDefined()));
		}

		if (load_inline_stat == OpStatus::ERR_NO_MEMORY)
			return OUT_OF_MEMORY;

		// Set counters

		if (props.counter_reset_cp)
			if (info.workplace->GetCounters()->SetCounters(html_element, props.counter_reset_cp, TRUE) == OpStatus::ERR_NO_MEMORY)
				return OUT_OF_MEMORY;

		if (props.counter_inc_cp)
			if (info.workplace->GetCounters()->SetCounters(html_element, props.counter_inc_cp, FALSE) == OpStatus::ERR_NO_MEMORY)
				return OUT_OF_MEMORY;

		/* Verify that there is no generated content or old pseudo elements
		   lying around, since we're about to generate new ones (if they
		   are required). */

		OP_ASSERT(!html_element->HasBefore() ||
				  !html_element->FirstChild() ||
				  !html_element->FirstChild()->GetIsBeforePseudoElement());

		OP_ASSERT(!html_element->HasAfter() ||
				  !html_element->LastChild() ||
				  !html_element->LastChild()->GetIsAfterPseudoElement() &&
				  !html_element->LastChild()->GetIsGeneratedContent());

		if (html_element->GetIsListMarkerPseudoElement() && list_marker_content_type == LIST_MARKER_TEXT)
		{
			if (!InsertListItemMarkerTextNode())
				return OUT_OF_MEMORY;
		}
		else if (props.content_cp || html_element->GetIsBeforePseudoElement() || html_element->GetIsAfterPseudoElement())
		{
			/* There's a 'content' property to process, or this is a ::before or
			   ::after pseudo-element. The reason why it's not enough to just see if
			   the 'content' property is present, is because of some special handling
			   of BR elements that we have. Our BR handling is somewhat exotic, and
			   may be responsible for bugs like CORE-1098. */

			if (!AddGeneratedContent(info))
				return OUT_OF_MEMORY;
		}

		// Don't generate ::before/::after for replaced content.
		if (!html_element->GetLayoutBox()->IsContentReplaced())
		{
			if (html_element->HasBefore())
				if (!CreatePseudoElement(info, TRUE))
					return OUT_OF_MEMORY;

			if (html_element->HasAfter())
				if (!CreatePseudoElement(info, FALSE))
					return OUT_OF_MEMORY;
		}

		if (props.display_type == CSS_VALUE_list_item && props.HasListItemMarker())
			if (!CreateListItemMarker(info))
				return OUT_OF_MEMORY;
	}

	state = LayoutElement(info);

	switch (state)
	{
	case OUT_OF_MEMORY:
	case YIELD:
	case STOP:
		return state;
	}

	OP_ASSERT(state == CONTINUE);

#ifdef ACCESS_KEYS_SUPPORT
	if (element_type != Markup::HTE_TEXT || html_element->IsMatchingType(Markup::HTE_AREA, NS_HTML))
	{
		if (props.accesskey_cp)
		{
			if (props.accesskey_cp->GetDeclType() == CSS_DECL_GEN_ARRAY && info.hld_profile)
			{
				BOOL key_added = FALSE;
				short gen_arr_len = props.accesskey_cp->ArrayValueLen();
				const CSS_generic_value* gen_arr = props.accesskey_cp->GenArrayValue();

				for (int i = 0; i < gen_arr_len; i++)
				{
					if (gen_arr[i].value_type == CSS_STRING_LITERAL)
					{
						BOOL conflict = FALSE;
						if (OpStatus::IsMemoryError(info.hld_profile
							->AddAccessKeyFromStyle(gen_arr[i].value.string, html_element, conflict)))
						{
							return OUT_OF_MEMORY;
						}
						if (!conflict)
							key_added = TRUE;
					}
					else if (gen_arr[i].value_type == CSS_COMMA)
					{
						if (key_added)
							break;
					}
				}
			}
		}
# ifdef _WML_SUPPORT_
		else if (html_element->IsMatchingType(WE_DO, NS_WML) && !html_element->GetIsPseudoElement() && info.hld_profile)
		{
			BOOL dummy;
			const uni_char *type = html_element->GetStringAttr(WA_TYPE, NS_IDX_WML);

			if (OpStatus::IsMemoryError(info.hld_profile->AddAccessKeyFromStyle(type, html_element, dummy)))
			{
				return OUT_OF_MEMORY;
			}
		}
# endif // _WML_SUPPORT_
	}
#endif // ACCESS_KEYS_SUPPORT

	if (may_need_reflow)
	{
		ReplacedContent* replaced_content = NULL;

		if (Box* box = html_element->GetLayoutBox())
			if (Content* content = box->GetContent())
				if (content->IsReplaced())
					replaced_content = (ReplacedContent*) content;

		if (replaced_content)
			replaced_content->RequestReflow(info.workplace);
		else
			OP_ASSERT(!"Element not marked for reflow when it should have been");
	}

	return CONTINUE;
}

#ifdef SUPPORT_VISUAL_ADBLOCK

BOOL
LayoutProperties::IsElementBlocked(LayoutInfo& info)
{
	// Error pages share the same address as the site that caused them so take care to not block anything on them.
	if (info.doc->GetDocManager()->ErrorPageShown())
		return FALSE;

	HTML_ElementType element_type = html_element->Type();
	URL url;
	URL* img_url = NULL;

	if (element_type == Markup::HTE_OBJECT || element_type == Markup::HTE_EMBED)
		img_url = html_element->GetEMBED_URL();
	else
		if (element_type == Markup::HTE_IFRAME)
			img_url = (URL*) html_element->GetUrlAttr(ATTR_SRC, NS_IDX_HTML, info.doc->GetLogicalDocument());
		else
		{
			url = html_element->GetImageURL(FALSE, info.doc->GetLogicalDocument());
			img_url = &url;
		}

	if (img_url && !img_url->IsEmpty() || element_type == Markup::HTE_HTML)
	{
		URL ctx_url = info.visual_device->GetWindow()->GetCurrentShownURL();
		URLFilterDOMListenerOverride lsn_over(info.doc->GetDOMEnvironment(), html_element);
		BOOL load = FALSE;
		BOOL widget = info.visual_device->GetWindow()->GetType() == WIN_TYPE_GADGET;
		HTMLResource resource = HTMLLoadContext::ConvertElement(html_element);
		HTMLLoadContext ctx(resource, ctx_url.GetServerName(), &lsn_over, widget);

		if (!img_url || img_url->IsEmpty())
			img_url = &ctx_url;

		OpStatus::Ignore(g_urlfilter->CheckURL(img_url, load, info.visual_device->GetWindow(), &ctx, (info.workplace && info.workplace->GetReflowCount() > 0) ? FALSE : TRUE));

		if (!load)
			// Block the URL if not in content block mode.

			return !info.doc->GetWindow()->GetContentBlockEditMode();
	}

	return FALSE;
}

#endif // SUPPORT_VISUAL_ADBLOCK

BOOL
LayoutProperties::ConvertToInlineRunin(LayoutInfo& info, Container* old_container)
{
	InlineContent* content = OP_NEW(InlineContent, ());

	if (content)
	{
		InlineBox* inline_run_in;

		if (props.display_type == CSS_VALUE_compact)
			inline_run_in = OP_NEW(InlineCompactBox, (html_element, content));
		else
			inline_run_in = OP_NEW(InlineRunInBox, (html_element, content));

		if (inline_run_in)
		{
			old_container->RemovePreviousLayoutElement(info);

			content->SetPlaceholder(inline_run_in);

			OP_DELETE(html_element->GetLayoutBox());
			html_element->SetLayoutBox(inline_run_in);

			return TRUE;
		}
	}

	OP_DELETE(content);

	return FALSE;
}

LayoutProperties*
LayoutProperties::GetCascadeAndLeftAbsEdge(HLDocProfile* hld_profile, HTML_Element* element, LayoutCoord& left_edge, BOOL static_position)
{
	HTML_Element* parent = element->Parent();
	Box* box = element->GetLayoutBox();
	BOOL new_static_position = static_position;
	LayoutProperties* parent_cascade = this;

	if (box->IsAbsolutePositionedBox())
	{
		AbsolutePositionedBox* abs_box = (AbsolutePositionedBox*)box;

		if (!abs_box->IsRightAligned() && abs_box->HasFixedPosition())
			new_static_position = abs_box->GetHorizontalOffset() == NO_HOFFSET;
	}

	if (parent)
		parent_cascade = GetCascadeAndLeftAbsEdge(hld_profile, parent, left_edge, new_static_position);

	if (!parent_cascade)
		return NULL;

	LayoutProperties* cascade = parent_cascade->Suc();

	if (!cascade)
	{
		cascade = new LayoutProperties();

		if (!cascade)
			return NULL;

		cascade->Follow(parent_cascade);
	}

	if (cascade->html_element != element)
	{
		parent_cascade->CleanSuc();
		cascade->html_element = element;
		if (!cascade->Inherit(hld_profile, parent_cascade))
			return NULL;
	}

	const HTMLayoutProperties& props = cascade->props;

	if (box->IsAbsolutePositionedBox())
	{
		if (static_position)
			left_edge += props.padding_left;

		if (props.left != HPOSITION_VALUE_AUTO)
			left_edge += props.left;

		left_edge += props.margin_left + LayoutCoord(props.border.left.width); // + cached parent left border???
	}
	else
		if (static_position)
		{
			if (box->IsBlockBox())
			{
				OP_ASSERT(((BlockBox*)box)->HasFixedLeft());

				left_edge += LayoutCoord(props.border.left.width) + props.padding_left + props.margin_left;

				if (box->IsPositionedBox() && props.left != HPOSITION_VALUE_AUTO)
					left_edge += props.left;
			}
			else
				if (box->IsTableCell())
				{
					LayoutProperties* table_cascade = parent_cascade->Pred()->Pred();
					TableContent* table = table_cascade->html_element->GetLayoutBox()->GetTableContent();
					left_edge += table->GetNormalColumnX(((TableCellBox*)box)->GetColumn(), table_cascade->GetProps()->border_spacing_horizontal);
					left_edge -= table_cascade->GetProps()->padding_left;
				}
		}

	return cascade;
}

/** Change font and update some of the font attributes (ascent, descent ...) */

void
LayoutProperties::ChangeFont(VisualDevice* visual_device, int font_number)
{
	visual_device->SetFont(font_number);
	props.SetTextMetrics(visual_device);
}

/** Check if the protocol of the specified inline URL is trusted, which
	means that we may pass it to another application for loading. */

OP_BOOLEAN
LayoutProperties::IsTrustedInline(URL* inline_url)
{
	OpString protocol;
	RETURN_IF_MEMORY_ERROR(protocol.Set(inline_url->GetAttribute(URL::KProtocolName)));
	int index = g_pcdoc->GetTrustedProtocolIndex(protocol);
	return index == -1 ? OpBoolean::IS_FALSE : OpBoolean::IS_TRUE;
}

/** Extract properties from this element that are to be stored as "global document properties".

	A document has a concept of document background, document font, etc. Normally
	these are extracted from HTML and BODY elements. Will cause the entire document
	area to be repainted when an element sets "global document background".*/

OP_STATUS
LayoutProperties::SetDocRootProps(HLDocProfile* hld_profile, Markup::Type element_type) const
{
	LayoutWorkplace *workplace = hld_profile->GetLayoutWorkplace();
	VisualDevice* vis_dev = hld_profile->GetVisualDevice();
	DocRootProperties& doc_root_props = workplace->GetDocRootProperties();

	OP_ASSERT(workplace->HasDocRootProps(html_element, element_type));

	if (element_type == Markup::HTE_HTML)
		doc_root_props.SetBorder(props.border);

	BOOL use_background = element_type == Markup::HTE_HTML || !doc_root_props.HasBackground();
	if (use_background)
	{
		doc_root_props.SetBackgroundColor(props.bg_color);
		doc_root_props.SetTransparentBackground(props.GetBgIsTransparent());
		doc_root_props.SetBackgroundImages(props.bg_images);

		if (doc_root_props.HasBackground())
		{
# ifdef QUICK
			FramesDocument* doc = hld_profile->GetFramesDocument();
			if (doc->GetLayoutMode() == LAYOUT_SSR || doc->GetLayoutMode() == LAYOUT_CSSR)
				/* In order to make the faked document width more visible on desktop, document root is gray in SSR mode.
				   Code in VerticalBox::PaintBgAndBorder() handles this. */

				doc_root_props.SetBackgroundElementType(Markup::HTE_HTML);
			else
# endif
				doc_root_props.SetBackgroundElementType(element_type);
		}

		if (!props.bg_images.IsEmpty() || props.bg_color != USE_DEFAULT_COLOR)
			// If element has background on it, update entire screen

			vis_dev->UpdateAll();
	}

	if (HTMLayoutProperties::IsViewportMagicElement(html_element, hld_profile))
	{
		if (html_element->Type() == Markup::HTE_HTML)
			// Overflow on HTML applies to the viewport, not the element itself.

			workplace->SetViewportOverflow(props.computed_overflow_x, props.computed_overflow_y);
		else
		{
			// Overflow on BODY applies to the viewport, if HTML has overflow visible.

			short viewport_overflow_x = workplace->GetViewportOverflowX();
			short viewport_overflow_y = workplace->GetViewportOverflowY();

			if (viewport_overflow_x == CSS_VALUE_visible && viewport_overflow_y == CSS_VALUE_visible)
			{
				viewport_overflow_x = props.computed_overflow_x;
				viewport_overflow_y = props.computed_overflow_y;
				workplace->SetViewportOverflow(viewport_overflow_x, viewport_overflow_y);
			}
		}
	}

	return OpStatus::OK;
}

void
LayoutProperties::AddZElement(ZElement *z_element)
{
	if (stacking_context)
		stacking_context->AddZElement(z_element);
}

static CSS_decl*
GetComputedWidthDecl(short property, const HTMLayoutProperties *hprops, Box *layout_box, BOOL is_current_style)
{
	OP_ASSERT(hprops != NULL);

#ifdef CURRENT_STYLE_SUPPORT
	if (is_current_style)
	{
		if (hprops->types->m_width == CSS_IDENT)
		{
			if (hprops->content_width == CONTENT_WIDTH_O_SKIN)
				return OP_NEW(CSS_type_decl, (property, CSS_VALUE__o_skin));
			else
				return OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		}
		else if (hprops->content_width == CONTENT_WIDTH_AUTO)
			return OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		else
		{
			// Percentage or regular value
			const short int unit = hprops->types->m_width;
			const float value = (unit == CSS_PERCENTAGE) ? LayoutFixedToFloat(LayoutFixed(-hprops->content_width)) : float(hprops->content_width);
			return OP_NEW(CSS_number_decl, (property, value, unit));
		}
	}
	else
#endif // CURRENT_STYLE_SUPPORT
		if (layout_box)
		{
			/* Take box-sizing into consideration and subtract padding and
			   border from the width of the layout box. */
			float calculated_width = float(layout_box->GetWidth());
			if (calculated_width > 0.0f)
			{
				if (hprops->box_sizing == CSS_VALUE_content_box)
				{
					/* The width of the layout box is border box, remove
					   padding and border to get content box */

					calculated_width -= (hprops->padding_left + hprops->padding_right +
										 hprops->border.left.width + hprops->border.right.width);
				}
			}
			return OP_NEW(CSS_number_decl, (property, calculated_width, CSS_PX));
		}
		else
		{
			if (hprops->content_width >= 0)
				return OP_NEW(CSS_number_decl, (property, (float)hprops->content_width, CSS_PX));
			else
				if (hprops->content_width > CONTENT_WIDTH_SPECIAL)
					return OP_NEW(CSS_number_decl, (property, LayoutFixedToFloat(LayoutFixed(-hprops->content_width)), CSS_PERCENTAGE));
				else
					return OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		}
}

static CSS_decl*
GetComputedHeightDecl(short property, const HTMLayoutProperties *hprops, Box *layout_box, BOOL is_current_style)
{
	OP_ASSERT(hprops != NULL);
#ifndef CURRENT_STYLE_SUPPORT
	OP_ASSERT(!is_current_style || !"No current style support enabled");
#endif

#ifdef CURRENT_STYLE_SUPPORT
	if (is_current_style)
	{
		if (hprops->types->m_height == CSS_IDENT)
		{
			if (hprops->content_height == CONTENT_HEIGHT_O_SKIN)
			{
				return OP_NEW(CSS_type_decl, (property, CSS_VALUE__o_skin));
			}
			else
			{
				return OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
			}
		}
		else if (hprops->content_height == CONTENT_HEIGHT_AUTO)
			return OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		else
		{
			// Percentage or regular value
			const short int unit = hprops->types->m_height;
			const float value = (unit == CSS_PERCENTAGE) ?
				LayoutFixedToFloat(LayoutFixed(-hprops->content_height))
				: float(hprops->content_height);
			return OP_NEW(CSS_number_decl, (property, value, unit));
		}
	}
	else
#endif // CURRENT_STYLE_SUPPORT
		if (layout_box)
		{
			/* Take box-sizing into consideration and subtract padding and
			   border from the height of the layoutbox. */
			float calculated_height = float(layout_box->GetHeight());
			if (calculated_height > 0.0f)
			{
				if (hprops->box_sizing == CSS_VALUE_content_box)
				{
					/* The height of the layout box is border box, remove
					   padding and border to get content box */

					calculated_height -= (hprops->padding_top + hprops->padding_bottom +
										  hprops->border.top.width + hprops->border.bottom.width);
				}
			}
			return OP_NEW(CSS_number_decl, (property, calculated_height, CSS_PX));
		}
		else
		{
			if (hprops->content_height >= 0)
				return OP_NEW(CSS_number_decl, (property, (float)hprops->content_height, CSS_PX));
			else
				if (hprops->content_height > CONTENT_HEIGHT_SPECIAL)
					return OP_NEW(CSS_number_decl, (property, LayoutFixedToFloat(LayoutFixed(-hprops->content_height)), CSS_PERCENTAGE));
				else
					return OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		}
}

/* static */ CSS_decl*
LayoutProperties::GetComputedColorDecl(short property, COLORREF color_val, BOOL is_current_style /* = FALSE */)
{
#ifndef CURRENT_STYLE_SUPPORT
	OP_ASSERT(!is_current_style || !"No current style support enabled");
#endif

	if (color_val == USE_DEFAULT_COLOR)
	{
		switch (property)
		{
		case CSS_PROPERTY_color:
			return OP_NEW(CSS_long_decl, (property, 0L));
		default:
			return OP_NEW(CSS_type_decl, (property, (CSS_VALUE_transparent)));
		}
	}
	if (color_val == CSS_COLOR_transparent)
		return OP_NEW(CSS_type_decl, (property, CSS_VALUE_transparent));
	else if (color_val == CSS_COLOR_invert)
		return OP_NEW(CSS_type_decl, (property, CSS_VALUE_invert));
	else if ((color_val & CSS_COLOR_KEYWORD_TYPE_named) &&
			!(color_val & ~CSS_COLOR_KEYWORD_TYPE_index & ~CSS_COLOR_KEYWORD_TYPE_named))
	{
#ifdef CURRENT_STYLE_SUPPORT
		if (is_current_style)
		{
			if ((color_val & CSS_COLOR_KEYWORD_TYPE_ui_color) == CSS_COLOR_KEYWORD_TYPE_ui_color) // UI color
				return OP_NEW(CSS_type_decl, (property, static_cast<CSSValue>(color_val & ~CSS_COLOR_KEYWORD_TYPE_ui_color)));
			else
			{
				const uni_char *col_name = HTM_Lex::GetColNameByIndex(color_val & CSS_COLOR_KEYWORD_TYPE_index);
				long keyword = CSS_GetKeyword(col_name, uni_strlen(col_name));
				if (CSS_is_ui_color_val(keyword))
					color_val = keyword | CSS_COLOR_KEYWORD_TYPE_ui_color;
				return OP_NEW(CSS_color_decl, (property, color_val));
			}
		}
		else
#endif
			return OP_NEW(CSS_long_decl, (property, HTM_Lex::GetColValByIndex(color_val)));
	}
	else
		return OP_NEW(CSS_long_decl, (property, color_val));
}

/* Find pseudo target. Return NULL if the corresponding pseudo
   element isn't availible. */

static HTML_Element *FindPseudoTarget(HTML_Element *elm, short pseudo)
{
	HTML_Element *target_elm = elm;
	switch (pseudo)
	{
	case CSS_PSEUDO_CLASS_AFTER:
		if (elm->HasAfter())
			target_elm = elm->LastChild();
		else
			target_elm = NULL;
		break;

	case CSS_PSEUDO_CLASS_BEFORE:
		if (elm->HasBefore())
			target_elm = elm->FirstChild();
		else
			target_elm = NULL;
		break;

	case CSS_PSEUDO_CLASS_FIRST_LETTER:
		{
			HTML_Element *candidate = (HTML_Element*)elm->FirstLeaf();
			do
			{
				while (candidate && !candidate->IsText())
					candidate = (HTML_Element*) candidate->NextLeaf();

				if (candidate)
				{
					if (!elm->IsAncestorOf(candidate))
					{
						candidate = NULL;
						break;
					}

					if (candidate->Parent() && candidate->Parent()->GetIsFirstLetterPseudoElement())
						break;
					else
						candidate = (HTML_Element*)candidate->NextLeaf();
				}
			} while (candidate);

			if (candidate)
				target_elm = candidate;
			else
				target_elm = NULL;
		}
		break;
	}

	return target_elm;
}

/* Helper macro to select style unit. */
#ifdef CURRENT_STYLE_SUPPORT
# define STYLE_UNIT(CURVAL, DEFVAL) (is_current_style ? (CURVAL) : (static_cast<int>(DEFVAL)))
#else
# define STYLE_UNIT(CURVAL, DEFVAL) (DEFVAL)
#endif

/* static */ CSS_decl*
LayoutProperties::GetComputedDecl(HTML_Element* elm, short property, short pseudo, HLDocProfile *hld_profile)
{
	return GetComputedDecl(elm, property, pseudo, hld_profile, FALSE);
}

/* static */ CSS_decl*
LayoutProperties::GetComputedDecl(HTML_Element* elm, short property, short pseudo, HLDocProfile *hld_profile, LayoutProperties *lprops)
{
	return GetComputedDecl(elm, property, pseudo, hld_profile, lprops, FALSE);
}

#ifdef CURRENT_STYLE_SUPPORT

/* static */ CSS_decl*
LayoutProperties::GetCurrentStyleDecl(HTML_Element* elm, short property, short pseudo, HLDocProfile *hld_profile)
{
	return GetComputedDecl(elm, property, pseudo, hld_profile, TRUE);
}

#endif // CURRENT_STYLE_SUPPORT

static CSS_decl*
CreateBorderWidthDecl(short property, short value, int unit)
{
	switch (unit)
	{
	case CSS_IDENT:
	{
		// translate back from resolved values to css values
		CSSValue css_val;
		switch (value)
		{
		case CSS_BORDER_WIDTH_THIN:
			css_val = CSS_VALUE_thin;
			break;
		case CSS_BORDER_WIDTH_MEDIUM:
			css_val = CSS_VALUE_medium;
			break;
		case CSS_BORDER_WIDTH_THICK:
			css_val = CSS_VALUE_thick;
			break;
		default:
			OP_ASSERT(!"Not reached");
			css_val = CSS_VALUE_UNSPECIFIED;
		}
		return OP_NEW(CSS_type_decl, (property, css_val));
		break;
	}
	case CSS_PX:
	default:
		return OP_NEW(CSS_number_decl, (property, value, unit));
	}
}

static CSS_decl*
CreateShadowsDecl(short property, const Shadows* shadows, const CSSLengthResolver& length_resolver, BOOL include_spread)
{
	CSS_decl* ret_decl = NULL;

	if (int shadow_count = shadows->GetCount())
	{
		CSS_decl* shadows_cp = shadows->Get();
		int arr_len = shadows_cp->ArrayValueLen();
		const CSS_generic_value* arr = shadows_cp->GenArrayValue();

		// Determine the amount of 'inset' identifiers that needed to be output.
		int inset_shadow_count = 0;
		for (int i = 0; i < arr_len; ++i)
			if (arr[i].GetValueType() == CSS_IDENT &&
				arr[i].GetType() == CSS_VALUE_inset)
				inset_shadow_count++;

		OP_ASSERT(inset_shadow_count <= shadow_count);

		// 5 values per shadow (4 if include_spread is FALSE), separated by commas
		int gen_arr_length = include_spread ? 5 : 4;
		gen_arr_length *= shadow_count;
		// optional insets, separated by commas
		gen_arr_length += inset_shadow_count + (shadow_count - 1);
		CSS_generic_value* gen_arr = OP_NEWA(CSS_generic_value, gen_arr_length);
		ret_decl = CSS_heap_gen_array_decl::Create(property, gen_arr, gen_arr_length);
		if (!ret_decl)
			return NULL;

		Shadow shadow;
		Shadows::Iterator iter(*shadows);

		CSS_generic_value* ga = gen_arr;

		while (iter.GetNext(length_resolver, shadow))
		{
			if (ga != gen_arr)
			{
				ga->SetValueType(CSS_COMMA);

				ga++;
			}

			ga->SetValueType(CSS_DECL_COLOR);
			ga++->value.color = shadow.color;

			ga->SetValueType(CSS_PX);
			ga++->SetReal(float(shadow.left));

			ga->SetValueType(CSS_PX);
			ga++->SetReal(float(shadow.top));

			ga->SetValueType(CSS_PX);
			ga++->SetReal(float(shadow.blur));

			if (include_spread)
			{
				ga->SetValueType(CSS_PX);
				ga++->SetReal(float(shadow.spread));
			}

			if (shadow.inset)
			{
				ga->SetValueType(CSS_IDENT);
				ga->SetType(CSS_VALUE_inset);

				ga++;
			}
		}

		OP_ASSERT(ga == gen_arr + gen_arr_length);
	}
	else
		ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));

	return ret_decl;
}

/* static */ CSS_decl*
LayoutProperties::GetComputedDecl(HTML_Element* elm, short property, short pseudo, HLDocProfile *hld_profile, BOOL is_current_style)
{
	FramesDocument *frm_doc = hld_profile->GetFramesDocument();
	OP_ASSERT(frm_doc->IsCurrentDoc());

#ifndef CURRENT_STYLE_SUPPORT
	if (is_current_style)
	{
		OP_ASSERT(!"Current style support is disabled but requested");

		/* Return corresponding !is_current_style value instead. */
		is_current_style = FALSE;
	}
#endif

	if (!frm_doc->IsCurrentDoc())
	{
		/* Crash fix: we can't reflow an inactive document, and we can't compute
		   a declaration in a dirty tree.  However, callers will interpret this
		   as OOM, so they need to stop calling this function too. */
		OP_ASSERT(!"GetComputedDecl not supported for inactive documents");
		return NULL;
	}

	LogicalDocument *logdoc = frm_doc->GetLogicalDocument();
	if (logdoc)
		if (HTML_Element *root = logdoc->GetRoot())
			if (root->IsAncestorOf(elm))
				frm_doc->Reflow(FALSE); // FIXME: OOM

	CSS_decl *ret_decl = NULL;
	Head prop_list;
	LayoutProperties* lprops = NULL;

	HTML_Element *target_elm = FindPseudoTarget(elm, pseudo);
	if (!target_elm)
		goto exit_function;

	hld_profile->GetVisualDevice()->Reset();
	lprops = LayoutProperties::CreateCascade(target_elm, prop_list, hld_profile
#ifdef CURRENT_STYLE_SUPPORT
		, is_current_style
#endif // CURRENT_STYLE_SUPPORT
		);

	if (!lprops)
		goto exit_function;

	if (pseudo == CSS_PSEUDO_CLASS_SELECTION)
		if (!PropertyToSelectionProperty(property))
			goto exit_function;

	if (pseudo == CSS_PSEUDO_CLASS_FIRST_LINE)
	{
		if (lprops->WantToModifyProperties(TRUE))
			lprops->AddFirstLineProperties(hld_profile);
		else
			goto exit_function;
	}

	ret_decl = GetComputedDecl(elm, property, pseudo, hld_profile, lprops, is_current_style);

exit_function:
	prop_list.Clear();

	return ret_decl;

}

static CSS_decl*
GetComputedBorderRadius(CSSProperty property, short start, BOOL start_is_decimal, short end, BOOL end_is_decimal)
{
	float radius_start, radius_end;
	CSSValueType radius_start_type, radius_end_type;

	radius_start = (float)start;
	radius_start_type = CSS_PX;

	if (start_is_decimal)
		radius_start /= 100;

	if (radius_start < 0)
	{
		radius_start = - radius_start;
		radius_start_type = CSS_PERCENTAGE;
	}

	radius_end = (float)end;
	radius_end_type = CSS_PX;

	if (end_is_decimal)
		radius_end /= 100;

	if (radius_end < 0)
	{
		radius_end = - radius_end;
		radius_end_type = CSS_PERCENTAGE;
	}

	return OP_NEW(CSS_number2_decl, (property, radius_start, radius_end, radius_start_type, radius_end_type));
}

/* static */ CSS_decl*
LayoutProperties::GetComputedDecl(HTML_Element* elm, short property, short pseudo, HLDocProfile *hld_profile, LayoutProperties *lprops, BOOL is_current_style)
{
	CSS_decl* ret_decl = NULL;

	const HTMLayoutProperties *hprops = &lprops->GetCascadingProperties();

	/* This switch is roughly sorted by the order of CSS_PROPERTY_* to
	   aid the compiler to generate efficient code. Short-hand and SVG
	   properties doesn't follow this however. */

	switch(property)
	{
	case CSS_PROPERTY_top:
		{
			if (hprops->top == VPOSITION_VALUE_AUTO || hprops->position == CSS_VALUE_static)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
			else
			{
				const int unit = STYLE_UNIT(hprops->types->m_top, CSS_PX);
				ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->top, unit));
			}
		}
		break;
	case CSS_PROPERTY_clip:
		{
			CSS_number4_decl *new_decl = OP_NEW(CSS_number4_decl, (property));
			if (new_decl)
			{
				LayoutCoord val = hprops->clip_top;

				if (val == CLIP_NOT_SET || val == CLIP_AUTO)
					val = LayoutCoord(0);

				new_decl->SetValue(0, STYLE_UNIT(hprops->types->m_clip_top, CSS_PX), (float)val);

				val = hprops->clip_right;

				if (val == CLIP_NOT_SET || val == CLIP_AUTO)
					val = LayoutCoord(0);

				new_decl->SetValue(1, STYLE_UNIT(hprops->types->m_clip_right, CSS_PX), (float)val);

				val = hprops->clip_bottom;

				if (val == CLIP_NOT_SET || val == CLIP_AUTO)
					val = LayoutCoord(0);

				new_decl->SetValue(2, STYLE_UNIT(hprops->types->m_clip_bottom, CSS_PX), (float)val);

				val = hprops->clip_left;
				if (val == CLIP_NOT_SET || val == CLIP_AUTO)
					val = LayoutCoord(0);

				new_decl->SetValue(3, STYLE_UNIT(hprops->types->m_clip_left, CSS_PX), (float)val);
			}

			ret_decl = new_decl;
		}
		break;
	case CSS_PROPERTY_left:
		{
			if (hprops->left == HPOSITION_VALUE_AUTO || hprops->position == CSS_VALUE_static)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
			else
			{
				const int unit = STYLE_UNIT(hprops->types->m_left, CSS_PX);
				ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->left, unit));
			}
		}
		break;
	case CSS_PROPERTY_page:
		{
			if (!hprops->page_props.page)
			{
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
				break;
			}

			CSS_string_decl *tmp_decl = OP_NEW(CSS_string_decl, (property, CSS_string_decl::StringDeclString));
			if (tmp_decl && OpStatus::IsMemoryError(tmp_decl->CopyAndSetString(hprops->page_props.page, uni_strlen(hprops->page_props.page))))
			{
				OP_DELETE(tmp_decl);
				break;
			}
			ret_decl = tmp_decl;
		}
		break;
	case CSS_PROPERTY_size:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_portrait));
		break;
	case CSS_PROPERTY_clear:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->clear)));
		break;
	case CSS_PROPERTY_color:
		ret_decl = GetComputedColorDecl(CSS_PROPERTY_color, hprops->font_color, is_current_style);
		break;
	case CSS_PROPERTY_float:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->float_type)));
		break;
	case CSS_PROPERTY_width:
		ret_decl = GetComputedWidthDecl(CSS_PROPERTY_width, hprops, lprops->html_element->GetLayoutBox(), is_current_style);
		break;
	case CSS_PROPERTY_right:
		{
			if (hprops->right == HPOSITION_VALUE_AUTO || hprops->position == CSS_VALUE_static)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
			else
			{
				const int unit = STYLE_UNIT(hprops->types->m_right, CSS_PX);
				ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->right, unit));
			}
		}
		break;
	case CSS_PROPERTY_widows:
		ret_decl = OP_NEW(CSS_long_decl, (property, hprops->page_props.widows));
		break;
	case CSS_PROPERTY_bottom:
		{
			if (hprops->bottom == VPOSITION_VALUE_AUTO || hprops->position == CSS_VALUE_static)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
			else
			{
				const int unit = STYLE_UNIT(hprops->types->m_bottom, CSS_PX);
				ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->bottom, unit));
			}
		}
		break;
	case CSS_PROPERTY_cursor:
		{
			switch (hprops->cursor)
			{
			case CURSOR_URI:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_pointer)); break;
			case CURSOR_CROSSHAIR:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_crosshair)); break;
			case CURSOR_CUR_POINTER:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_pointer)); break;
			case CURSOR_MOVE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_move)); break;
			case CURSOR_E_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_e_resize)); break;
			case CURSOR_NE_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_ne_resize)); break;
			case CURSOR_NW_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_nw_resize)); break;
			case CURSOR_N_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_n_resize)); break;
			case CURSOR_SE_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_se_resize)); break;
			case CURSOR_SW_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_sw_resize)); break;
			case CURSOR_S_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_s_resize)); break;
			case CURSOR_W_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_w_resize)); break;
			case CURSOR_TEXT:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_text)); break;
			case CURSOR_WAIT:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_wait)); break;
			case CURSOR_HELP:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_help)); break;
			case CURSOR_PROGRESS:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_progress)); break;
			case CURSOR_CONTEXT_MENU:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_context_menu)); break;
			case CURSOR_CELL:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_cell)); break;
			case CURSOR_VERTICAL_TEXT:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_vertical_text)); break;
			case CURSOR_ALIAS:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_alias)); break;
			case CURSOR_COPY:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_copy)); break;
			case CURSOR_NO_DROP:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_no_drop)); break;
			case CURSOR_NOT_ALLOWED:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_not_allowed)); break;
			case CURSOR_EW_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_ew_resize)); break;
			case CURSOR_NS_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_ns_resize)); break;
			case CURSOR_NESW_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_nesw_resize)); break;
			case CURSOR_NWSE_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_nwse_resize)); break;
			case CURSOR_COL_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_col_resize)); break;
			case CURSOR_ROW_RESIZE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_row_resize)); break;
			case CURSOR_ALL_SCROLL:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_all_scroll)); break;
			case CURSOR_NONE:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none)); break;
			case CURSOR_ZOOM_IN:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_zoom_in)); break;
			case CURSOR_ZOOM_OUT:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_zoom_out)); break;
			case CURSOR_AUTO:				// Used by layout engine only!
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto)); break;
			case CURSOR_DEFAULT_ARROW:
			case CURSOR_HOR_SPLITTER:
			case CURSOR_VERT_SPLITTER:
			case CURSOR_ARROW_WAIT:
			case CURSOR_NUM_CURSORS:			// added 26/7-00 <JB>
			default:
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_default)); break;
			}
		}
		break;

	case CSS_PROPERTY_column_width:
		if (hprops->column_width == CONTENT_WIDTH_AUTO)
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		else
			ret_decl = OP_NEW(CSS_number_decl, (property, (float) hprops->column_width, STYLE_UNIT(hprops->types->m_column_width, CSS_PX)));
		break;

	case CSS_PROPERTY_column_count:
		if (hprops->column_count == SHRT_MIN)
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		else
			ret_decl = OP_NEW(CSS_number_decl, (property, (float) hprops->column_count, CSS_NUMBER));
		break;

	case CSS_PROPERTY_column_gap:
#ifdef CURRENT_STYLE_SUPPORT
		if (is_current_style && hprops->types->m_column_gap == CSS_IDENT)
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_normal));
		else
#endif // CURRENT_STYLE_SUPPORT
			ret_decl = OP_NEW(CSS_number_decl, (property, (float) hprops->column_gap, STYLE_UNIT(hprops->types->m_column_gap, CSS_PX)));
		break;

	case CSS_PROPERTY_column_fill:
		if (hprops->column_fill == CSS_VALUE_balance)
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_balance));
		else
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		break;

	case CSS_PROPERTY_column_rule_width:
		ret_decl = CreateBorderWidthDecl(property, hprops->column_rule.width, STYLE_UNIT(hprops->types->m_column_rule_width, CSS_PX));
		break;

	case CSS_PROPERTY_column_rule_color:
		ret_decl = GetComputedColorDecl(property, hprops->column_rule.color, is_current_style);
		break;

	case CSS_PROPERTY_column_rule_style:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->column_rule.style)));
		break;

	case CSS_PROPERTY_column_span:
		if (hprops->column_span == COLUMN_SPAN_ALL)
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_all));
		else
			if (hprops->column_span == 1)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
			else
				ret_decl = OP_NEW(CSS_number_decl, (property, (float) hprops->column_span, CSS_NUMBER));
		break;

	case CSS_PROPERTY_height:
		ret_decl = GetComputedHeightDecl(CSS_PROPERTY_height, hprops, lprops->html_element->GetLayoutBox(), is_current_style);
		break;
	case CSS_PROPERTY_quotes:
		{
			CSS_decl *tmp_decl = hprops->quotes_cp;
			if (tmp_decl && tmp_decl->GetDeclType() == CSS_DECL_GEN_ARRAY)
				ret_decl = CSS_copy_gen_array_decl::Create(property, tmp_decl->GenArrayValue(), tmp_decl->ArrayValueLen());
			else
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
		}
		break;
	case CSS_PROPERTY_content:
		{
			CSS_decl *tmp_decl = hprops->content_cp;
			if (tmp_decl && tmp_decl->GetDeclType() == CSS_DECL_GEN_ARRAY)
				ret_decl = CSS_copy_gen_array_decl::Create(property, tmp_decl->GenArrayValue(), tmp_decl->ArrayValueLen());
			else
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
		}
		break;
	case CSS_PROPERTY_display:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->display_type)));
		break;
	case CSS_PROPERTY_orphans:
		ret_decl = OP_NEW(CSS_long_decl, (property, hprops->page_props.orphans));
		break;
	case CSS_PROPERTY_opacity:
		ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->opacity / 255.0f, CSS_NUMBER));
		break;
	case CSS_PROPERTY_overflow_wrap:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->overflow_wrap)));
		break;
	case CSS_PROPERTY_box_decoration_break:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->box_decoration_break)));
		break;
	case CSS_PROPERTY_z_index:
		if (hprops->z_index == CSS_ZINDEX_auto)
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		else
			ret_decl = OP_NEW(CSS_long_decl, (property, hprops->z_index));
		break;
	case CSS_PROPERTY_overflow_x:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->computed_overflow_x)));
		break;
	case CSS_PROPERTY_overflow_y:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->computed_overflow_y)));
		break;
	case CSS_PROPERTY_position:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->position)));
		break;
#ifdef SUPPORT_TEXT_DIRECTION
	case CSS_PROPERTY_direction:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->direction)));
		break;
	case CSS_PROPERTY_unicode_bidi:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->unicode_bidi)));
		break;
#endif
	case CSS_PROPERTY_font_size:
#ifdef CURRENT_STYLE_SUPPORT
		if (is_current_style)
		{
			if (hprops->types->m_font_size == CSS_IDENT)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->font_size)));
			else
				ret_decl = OP_NEW(CSS_number_decl, (property, LayoutFixedToFloat(hprops->decimal_font_size), hprops->types->m_font_size));
		}
		else
#endif // CURRENT_STYLE_SUPPORT
		{
			float font_size;

#ifdef SVG_SUPPORT
			if (hprops->svg && elm->GetNsType() == NS_SVG)
				font_size = hprops->svg->fontsize.GetFloatValue();
			else
#endif // SVG_SUPPORT
				font_size = LayoutFixedToFloat(hprops->decimal_font_size_constrained);

			ret_decl = OP_NEW(CSS_number_decl, (property, font_size, CSS_PX));
		}
		break;
	case CSS_PROPERTY_max_width:
		if (hprops->max_width < 0)
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
		else
		{
			const int unit = STYLE_UNIT(hprops->types->m_max_width, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->max_width, unit));
		}
		break;
	case CSS_PROPERTY_min_width:
		{
			if (hprops->min_width == CONTENT_SIZE_AUTO)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
			else
			{
				const int unit = STYLE_UNIT(hprops->types->m_min_width, CSS_PX);
				ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->min_width, unit));
			}
			break;
		}
	case CSS_PROPERTY_max_height:
		if (hprops->max_height < 0)
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
		else
		{
			const int unit = STYLE_UNIT(hprops->types->m_max_height, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->max_height, unit));
		}
		break;
	case CSS_PROPERTY_min_height:
		{
			if (hprops->min_height == CONTENT_SIZE_AUTO)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
			else
			{
				const int unit = STYLE_UNIT(hprops->types->m_min_height, CSS_PX);
				ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->min_height, unit));
			}
			break;
		}
	case CSS_PROPERTY_margin_top:
		if (hprops->GetMarginTopAuto())
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		else
		{
			const int unit = STYLE_UNIT(hprops->types->m_margin_top, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->margin_top, unit));
		}
		break;
	case CSS_PROPERTY_text_align:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->text_align)));
		break;
	case CSS_PROPERTY_font_style:
		ret_decl = OP_NEW(CSS_type_decl, (property, hprops->font_italic == FONT_STYLE_NORMAL ? CSS_VALUE_normal : CSS_VALUE_italic));
		break;
	case CSS_PROPERTY_visibility:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->visibility)));
		break;
	case CSS_PROPERTY_resize:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->resize)));
		break;
	case CSS_PROPERTY_box_sizing:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->box_sizing)));
		break;
	case CSS_PROPERTY_white_space:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->white_space)));
		break;
	case CSS_PROPERTY_line_height:
		if (hprops->line_height_i == LINE_HEIGHT_SQUEEZE_I)
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_normal));
		else if (hprops->line_height_i < LayoutFixed(0))
		{
#ifdef CURRENT_STYLE_SUPPORT
			if (is_current_style)
				ret_decl = OP_NEW(CSS_number_decl, (property, LayoutFixedToFloat(-hprops->line_height_i), CSS_NUMBER));
			else
#endif // CURRENT_STYLE_SUPPORT
				ret_decl = OP_NEW(CSS_number_decl, (property, LayoutFixedToFloat(hprops->decimal_font_size_constrained) * LayoutFixedToFloat(-hprops->line_height_i) , CSS_PX));
		}
		else
		{
			const int unit = STYLE_UNIT(hprops->types->m_line_height, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, LayoutFixedToFloat(hprops->line_height_i), unit));
		}
		break;
	case CSS_PROPERTY_font_family:
		{
			CSS_generic_value val;
			val.SetValueType(CSS_IDENT);

			/* Both font numbers and CSSValues are stored in the declaration array. A font number is
			   stored as a plain CSSValue, while a "real" CSSValue is tagged with
			   CSS_GENERIC_FONT_FAMILY. */

			val.SetType(CSSValue(hprops->font_number));
			ret_decl = CSS_copy_gen_array_decl::Create(property, &val, 1);
		}
		break;
	case CSS_PROPERTY_font_weight:
#ifdef CURRENT_STYLE_SUPPORT
		if (is_current_style)
		{
			if (hprops->types->m_font_weight == CSS_IDENT)
			{
				switch (hprops->font_weight)
				{
				case 4:
					ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_normal));
					break;
				case 7:
					ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_bold));
					break;
				case CSS_VALUE_bolder:
				case CSS_VALUE_lighter:
					ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->font_weight)));
					break;
				}
			}
			else
				ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->font_weight, CSS_NUMBER));
		}
		else
#endif // CURRENT_STYLE_SUPPORT
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->font_weight, CSS_NUMBER));
		break;
	case CSS_PROPERTY_padding_top:
		{
			const int unit = STYLE_UNIT(hprops->types->m_padding_top, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->padding_top, unit));
			break;
		}
	case CSS_PROPERTY_margin_left:
		{
			const int unit = STYLE_UNIT(hprops->types->m_margin_left, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->margin_left, unit));
			break;
		}
	case CSS_PROPERTY_text_indent:
		{
			const int unit = STYLE_UNIT(hprops->types->m_text_indent, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->text_indent, unit));
			break;
		}
	case CSS_PROPERTY_empty_cells:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->empty_cells)));
		break;
	case CSS_PROPERTY_font_stretch:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_normal));
		break;
	case CSS_PROPERTY_margin_right:
		{
			const int unit = STYLE_UNIT(hprops->types->m_margin_right, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->margin_right, unit));
			break;
		}
	case CSS_PROPERTY_padding_left:
		{
			const int unit = STYLE_UNIT(hprops->types->m_padding_left, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->padding_left, unit));
			break;
		}
	case CSS_PROPERTY_table_layout:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->table_layout)));
		break;
	case CSS_PROPERTY_caption_side:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->caption_side)));
		break;
	case CSS_PROPERTY_font_variant:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->small_caps)));
		break;
	case CSS_PROPERTY_word_spacing:
#ifdef CURRENT_STYLE_SUPPORT
		if (is_current_style)
		{
			if (hprops->types->m_word_spacing == CSS_IDENT)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_normal));
			else
				ret_decl = OP_NEW(CSS_number_decl, (property, LayoutFixedToFloat(hprops->word_spacing_i), hprops->types->m_word_spacing));
		}
		else
#endif // CURRENT_STYLE_SUPPORT
			ret_decl = OP_NEW(CSS_number_decl, (property, LayoutFixedToFloat(hprops->word_spacing_i), CSS_PX));
		break;
	case CSS_PROPERTY_margin_bottom:
#ifdef CURRENT_STYLE_SUPPORT
		if (is_current_style)
		{
			if (hprops->GetMarginBottomAuto())
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
			else
				ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->margin_bottom, hprops->types->m_margin_bottom));
		}
		else
#endif // CURRENT_STYLE_SUPPORT
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->margin_bottom, CSS_PX));
		break;
	case CSS_PROPERTY_padding_right:
#ifdef CURRENT_STYLE_SUPPORT
		ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->padding_right, is_current_style ? hprops->types->m_padding_right : static_cast<int>(CSS_PX)));
#else // CURRENT_STYLE_SUPPORT
		ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->padding_right, CSS_PX));
#endif // CURRENT_STYLE_SUPPORT
		break;
	case CSS_PROPERTY_counter_reset:
		{
			CSS_decl *tmp_decl = hprops->counter_reset_cp;
			if (tmp_decl && tmp_decl->GetDeclType() == CSS_DECL_GEN_ARRAY)
				ret_decl = CSS_copy_gen_array_decl::Create(property, tmp_decl->GenArrayValue(), tmp_decl->ArrayValueLen());
			else
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
		}
		break;
	case CSS_PROPERTY_outline_color:
		ret_decl = GetComputedColorDecl(CSS_PROPERTY_outline_color, hprops->outline.color, is_current_style);
		break;
	case CSS_PROPERTY_outline_style:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->outline.style)));
		break;
	case CSS_PROPERTY_outline_width:
		{
			const int unit = STYLE_UNIT(hprops->types->m_outline_width, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->outline.width, unit));
			break;
		}
	case CSS_PROPERTY_border_spacing:
		{
			const int unit_hor = STYLE_UNIT(hprops->types->m_brd_spacing_hor, CSS_PX);
			const int unit_ver = STYLE_UNIT(hprops->types->m_brd_spacing_ver, CSS_PX);

			ret_decl = OP_NEW(CSS_number2_decl, (property, (float)hprops->border_spacing_horizontal,
												 (float)hprops->border_spacing_vertical, unit_hor, unit_ver));
			break;
		}
	case CSS_PROPERTY_padding_bottom:
		{
			const int unit = STYLE_UNIT(hprops->types->m_padding_bottom, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->padding_bottom, unit));
			break;
		}
	case CSS_PROPERTY_letter_spacing:
		{
			const int unit = STYLE_UNIT(hprops->types->m_letter_spacing, CSS_PX);
			ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->letter_spacing, unit));
			break;
		}
	case CSS_PROPERTY_text_transform:
		switch (hprops->text_transform)
		{
		case CSS_TEXT_TRANSFORM_none:
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none)); break;
		case CSS_TEXT_TRANSFORM_capitalize:
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_capitalize)); break;
		case CSS_TEXT_TRANSFORM_uppercase:
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_uppercase)); break;
		case CSS_TEXT_TRANSFORM_lowercase:
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_lowercase)); break;
		case CSS_TEXT_ALIGN_inherit:
		default:
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none)); break;
		}
		break;
	case CSS_PROPERTY_vertical_align:
		switch (hprops->vertical_align_type)
		{
		case CSS_VALUE_baseline:
		case CSS_VALUE_top:
		case CSS_VALUE_bottom:
		case CSS_VALUE_sub:
		case CSS_VALUE_super:
		case CSS_VALUE_text_top:
		case CSS_VALUE_middle:
		case CSS_VALUE_text_bottom:
			ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->vertical_align_type)); break);
		default:
			ret_decl = OP_NEW(CSS_number_decl, (property, (float) hprops->vertical_align, CSS_PX); break);
		}
		break;
	case CSS_PROPERTY_text_decoration:
		if (hprops->text_decoration)
		{
			short text_dec = TEXT_DECORATION_NONE;
			if (hprops->text_decoration & TEXT_DECORATION_UNDERLINE)
				text_dec |= CSS_TEXT_DECORATION_underline;
			if (hprops->text_decoration & TEXT_DECORATION_LINETHROUGH)
				text_dec |= CSS_TEXT_DECORATION_line_through;
			if (hprops->text_decoration & TEXT_DECORATION_OVERLINE)
				text_dec |= CSS_TEXT_DECORATION_overline;
			if (hprops->text_decoration & TEXT_DECORATION_BLINK)
				text_dec |= CSS_TEXT_DECORATION_blink;

			/* Text decoration flags are stored as CSSValue, carefully chosen not to
			   collide with other valid values for text-decoration. */

			ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(text_dec)));
		}
		else
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
		break;
	case CSS_PROPERTY_border_collapse:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->border_collapse)));
		break;
	case CSS_PROPERTY_list_style_type:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->list_style_type)));
		break;
	case CSS_PROPERTY_background_color:
		ret_decl = GetComputedColorDecl(CSS_PROPERTY_background_color, BackgroundColor(*hprops), is_current_style);
		break;
	case CSS_PROPERTY_border_top_color:
		ret_decl = GetComputedColorDecl(CSS_PROPERTY_border_top_color, hprops->border.top.color, is_current_style);
		break;
	case CSS_PROPERTY_border_top_style:
		{
			LayoutProperties* parent_lprops = lprops;
			while (parent_lprops && parent_lprops->props.border.top.style == CSS_VALUE_inherit)
				parent_lprops = parent_lprops->Pred();

			if (!parent_lprops || parent_lprops->props.border.top.style == BORDER_STYLE_NOT_SET)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
			else
				ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(parent_lprops->props.border.top.style)));
		}
		break;
	case CSS_PROPERTY_border_top_width:
		{
			const int unit = STYLE_UNIT(hprops->types->m_brd_top_width, CSS_PX);
			ret_decl = CreateBorderWidthDecl(property, hprops->border.top.width, unit);
			break;
		}
	case CSS_PROPERTY_font_size_adjust:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
		break;
	case CSS_PROPERTY_list_style_image:
		{
			if (!hprops->list_style_image)
			{
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
				break;
			}

			ret_decl = OP_NEW(CSS_string_decl, (property, hprops->list_style_image.IsSkinURL() ? CSS_string_decl::StringDeclSkin : CSS_string_decl::StringDeclUrl));
			if (ret_decl && OpStatus::IsMemoryError(((CSS_string_decl *) ret_decl)->CopyAndSetString(hprops->list_style_image, uni_strlen(hprops->list_style_image))))
			{
				OP_DELETE(ret_decl);
				ret_decl = NULL;
			}
		}
		break;
	case CSS_PROPERTY_page_break_after:
	case CSS_PROPERTY_break_after:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->page_props.break_after)));
		break;

	case CSS_PROPERTY_border_left_color:
		ret_decl = GetComputedColorDecl(CSS_PROPERTY_border_left_color, hprops->border.left.color, is_current_style);
		break;
	case CSS_PROPERTY_border_left_style:
		{
			LayoutProperties* parent_lprops = lprops;
			while (parent_lprops && parent_lprops->props.border.left.style == CSS_VALUE_inherit)
				parent_lprops = parent_lprops->Pred();

			if (!parent_lprops || parent_lprops->props.border.left.style == BORDER_STYLE_NOT_SET)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
			else
				ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(parent_lprops->props.border.left.style)));
		}
		break;
	case CSS_PROPERTY_border_left_width:
		{
			const int unit = STYLE_UNIT(hprops->types->m_brd_left_width, CSS_PX);
			ret_decl = CreateBorderWidthDecl(property, hprops->border.left.width, unit);
			break;
		}
	case CSS_PROPERTY_page_break_before:
	case CSS_PROPERTY_break_before:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->page_props.break_before)));
		break;
	case CSS_PROPERTY_page_break_inside:
	case CSS_PROPERTY_break_inside:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->page_props.break_inside)));
		break;
	case CSS_PROPERTY_counter_increment:
		{
			CSS_decl *tmp_decl = hprops->counter_inc_cp;
			if (tmp_decl && tmp_decl->GetDeclType() == CSS_DECL_GEN_ARRAY)
				ret_decl = CSS_copy_gen_array_decl::Create(property, tmp_decl->GenArrayValue(), tmp_decl->ArrayValueLen());
			else
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
		}
		break;
	case CSS_PROPERTY_border_right_color:
		ret_decl = GetComputedColorDecl(CSS_PROPERTY_border_right_color, hprops->border.right.color, is_current_style);
		break;
	case CSS_PROPERTY_border_right_style:
		{
			LayoutProperties* parent_lprops = lprops;
			while (parent_lprops && parent_lprops->props.border.right.style == CSS_VALUE_inherit)
				parent_lprops = parent_lprops->Pred();

			if (!parent_lprops || parent_lprops->props.border.right.style == BORDER_STYLE_NOT_SET)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
			else
				ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(parent_lprops->props.border.right.style)));
		}
		break;
	case CSS_PROPERTY_border_right_width:
		{
			const int unit = STYLE_UNIT(hprops->types->m_brd_right_width, CSS_PX);
			ret_decl = CreateBorderWidthDecl(property, hprops->border.right.width, unit);
			break;
		}
	case CSS_PROPERTY_border_bottom_color:
		ret_decl = GetComputedColorDecl(CSS_PROPERTY_border_bottom_color, hprops->border.bottom.color, is_current_style);
		break;
	case CSS_PROPERTY_border_bottom_style:
		{
			LayoutProperties* parent_lprops = lprops;
			while (parent_lprops && parent_lprops->props.border.bottom.style == CSS_VALUE_inherit)
				parent_lprops = parent_lprops->Pred();

			if (!parent_lprops || parent_lprops->props.border.bottom.style == BORDER_STYLE_NOT_SET)
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
			else
				ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(parent_lprops->props.border.bottom.style)));
		}
		break;
	case CSS_PROPERTY_border_bottom_width:
		{
			const int unit = STYLE_UNIT(hprops->types->m_brd_bottom_width, CSS_PX);
			ret_decl = CreateBorderWidthDecl(property, hprops->border.bottom.width, unit);
			break;
		}
	case CSS_PROPERTY_list_style_position:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->list_style_pos)));
		break;

	case CSS_PROPERTY_image_rendering:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->image_rendering)));
		break;

#ifdef CSS_TRANSFORMS
	case CSS_PROPERTY_transform:
		{
			if (hprops->transforms_cp)
			{
				AffineTransform transform;
				CSSTransforms css_transforms;
				if (Box *box = lprops->html_element->GetLayoutBox())
				{
					css_transforms.SetTransformFromProps(*hprops, hld_profile->GetFramesDocument(),
														 box->GetWidth(), box->GetHeight());
					css_transforms.GetTransform(transform);
				}
				else
					transform.LoadIdentity();

				ret_decl = CSS_transform_list::MakeFromAffineTransform(transform);
			}
			else
			{
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
			}
		}
		break;

	case CSS_PROPERTY_transform_origin:
		{
			float origin_x = 0, origin_y = 0;

			if (Box *box = lprops->html_element->GetLayoutBox())
			{
				CSSTransforms css_transforms;

				css_transforms.SetOriginX(hprops->transform_origin_x, hprops->GetTransformOriginXIsPercent(), hprops->GetTransformOriginXIsDecimal());
				css_transforms.SetOriginY(hprops->transform_origin_y, hprops->GetTransformOriginYIsPercent(), hprops->GetTransformOriginYIsDecimal());
				css_transforms.ComputeTransformOrigin(box->GetWidth(), box->GetHeight(), origin_x, origin_y);
			}

			ret_decl = OP_NEW(CSS_number2_decl, (property, origin_x, origin_y, CSS_PX, CSS_PX));
		}
		break;
#endif // CSS_TRANSFORMS

#ifdef CSS_TRANSITIONS
	case CSS_PROPERTY_transition_property:
		{
			CSS_decl* tmp_decl = hprops->transition_property;
			if (tmp_decl)
				ret_decl = tmp_decl->CreateCopy();
			else
				ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_all));
		}
		break;
	case CSS_PROPERTY_transition_delay:
	case CSS_PROPERTY_transition_duration:
		{
			CSS_decl* tmp_decl = property == CSS_PROPERTY_transition_delay ? hprops->transition_delay : hprops->transition_duration;
			if (tmp_decl)
				ret_decl = tmp_decl->CreateCopy();
			else
			{
				CSS_generic_value zero_sec;
				zero_sec.SetReal(0);
				zero_sec.SetValueType(CSS_SECOND);
				ret_decl = CSS_copy_gen_array_decl::Create(property, &zero_sec, 1);
			}
		}
		break;
#ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_timing_function:
#endif // CSS_ANIMATIONS
	case CSS_PROPERTY_transition_timing_function:
		{
			CSS_decl* tmp_decl = hprops->transition_timing;

#ifdef CSS_ANIMATIONS
			if (property == CSS_PROPERTY_animation_timing_function)
				tmp_decl = hprops->animation_timing_function;
#endif // CSS_ANIMATIONS

			if (tmp_decl)
				ret_decl = tmp_decl->CreateCopy();
			else
			{
				CSS_generic_value ease[4];
				for (int i=0; i<4; i++)
					ease[i].SetValueType(CSS_NUMBER);
				ease[0].SetReal(0.25f);
				ease[1].SetReal(0.1f);
				ease[2].SetReal(0.25f);
				ease[3].SetReal(1.0f);

				ret_decl = CSS_copy_gen_array_decl::Create(property, ease, 4);
			}
		}
		break;
#endif // CSS_TRANSITIONS
#ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_fill_mode:
		{
			CSS_decl* tmp_decl = hprops->animation_fill_mode;
			if (tmp_decl)
				ret_decl = tmp_decl->CreateCopy();
			else
			{
				CSS_generic_value none;
				none.SetType(CSS_VALUE_none);
				none.SetValueType(CSS_IDENT);
				ret_decl = CSS_copy_gen_array_decl::Create(property, &none, 1);
			}
		}
		break;
	case CSS_PROPERTY_animation_name:
		{
			CSS_decl* tmp_decl = hprops->animation_name;
			if (tmp_decl)
				ret_decl = tmp_decl->CreateCopy();
			else
			{
				const uni_char* none_str = UNI_L("none");
				CSS_generic_value none;
				none.SetString(const_cast<uni_char*>(none_str));
				none.SetValueType(CSS_STRING_LITERAL);
				ret_decl = CSS_copy_gen_array_decl::Create(property, &none, 1);
			}
		}
		break;
	case CSS_PROPERTY_animation_delay:
	case CSS_PROPERTY_animation_duration:
		{
			CSS_decl* tmp_decl = property == CSS_PROPERTY_animation_delay ? hprops->animation_delay : hprops->animation_duration;
			if (tmp_decl)
				ret_decl = tmp_decl->CreateCopy();
			else
			{
				CSS_generic_value zero_sec;
				zero_sec.SetReal(0);
				zero_sec.SetValueType(CSS_SECOND);
				ret_decl = CSS_copy_gen_array_decl::Create(property, &zero_sec, 1);
			}
		}
		break;
	case CSS_PROPERTY_animation_iteration_count:
		{
			CSS_decl* tmp_decl = hprops->animation_iteration_count;
			if (tmp_decl)
				ret_decl = tmp_decl->CreateCopy();
			else
			{
				CSS_generic_value one;
				one.SetReal(1);
				one.SetValueType(CSS_NUMBER);
				ret_decl = CSS_copy_gen_array_decl::Create(property, &one, 1);
			}
		}
		break;
	case CSS_PROPERTY_animation_direction:
		{
			CSS_decl* tmp_decl = hprops->animation_direction;
			if (tmp_decl)
				ret_decl = tmp_decl->CreateCopy();
			else
			{
				CSS_generic_value normal;
				normal.SetType(CSS_VALUE_normal);
				normal.SetValueType(CSS_IDENT);
				ret_decl = CSS_copy_gen_array_decl::Create(property, &normal, 1);
			}
		}
		break;
	case CSS_PROPERTY_animation_play_state:
		{
			CSS_decl* tmp_decl = hprops->animation_play_state;
			if (tmp_decl)
				ret_decl = tmp_decl->CreateCopy();
			else
			{
				CSS_generic_value running;
				running.SetType(CSS_VALUE_running);
				running.SetValueType(CSS_IDENT);
				ret_decl = CSS_copy_gen_array_decl::Create(property, &running, 1);
			}
		}
		break;
#endif // CSS_ANIMATIONS

	case CSS_PROPERTY_background_image:
	case CSS_PROPERTY_background_repeat:
	case CSS_PROPERTY_background_clip:
	case CSS_PROPERTY_background_size:
	case CSS_PROPERTY_background_position:
	case CSS_PROPERTY_background_origin:
	case CSS_PROPERTY_background_attachment:
		ret_decl = hprops->bg_images.CloneProperty(property);
		break;

	case CSS_PROPERTY__o_object_fit:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->object_fit_position.fit)));
		break;

	case CSS_PROPERTY__o_object_position:
		{
			CSS_generic_value values[2]; /* ARRAY OK 2009-09-10 lstorset */

			if (hprops->object_fit_position.x_percent)
			{
				values[0].SetValueType(CSS_PERCENTAGE);
				values[0].SetReal(LayoutFixedToFloat(LayoutFixed(hprops->object_fit_position.x)));
			}
			else
			{
				values[0].SetValueType(CSS_PX);
				values[0].SetReal(float(hprops->object_fit_position.x));
			}
			if (hprops->object_fit_position.y_percent)
			{
				values[1].SetValueType(CSS_PERCENTAGE);
				values[1].SetReal(LayoutFixedToFloat(LayoutFixed(hprops->object_fit_position.y)));
			}
			else
			{
				values[1].SetValueType(CSS_PX);
				values[1].SetReal(float(hprops->object_fit_position.y));
			}

			ret_decl = CSS_copy_gen_array_decl::Create(property, values, 2);
			break;
		}

	case CSS_PROPERTY_box_shadow:
	case CSS_PROPERTY_text_shadow:
		{
			BOOL include_spread = property == CSS_PROPERTY_box_shadow;
			const Shadows* shadows = include_spread ? &hprops->box_shadows : &hprops->text_shadows;
			CSSLengthResolver length_resolver(hld_profile->GetVisualDevice(), FALSE, 0, hprops->decimal_font_size_constrained, hprops->font_number, hld_profile->GetFramesDocument()->RootFontSize());
			ret_decl = CreateShadowsDecl(CSSProperty(property), shadows, length_resolver, include_spread);
		}
		break;

	case CSS_PROPERTY_border_top_left_radius:
		ret_decl = GetComputedBorderRadius(CSSProperty(property),
										   hprops->border.top.radius_start, hprops->border.top.packed.radius_start_is_decimal,
										   hprops->border.left.radius_start, hprops->border.left.packed.radius_start_is_decimal);
		break;
	case CSS_PROPERTY_border_top_right_radius:
		ret_decl = GetComputedBorderRadius(CSSProperty(property),
										   hprops->border.top.radius_end, hprops->border.top.packed.radius_end_is_decimal,
										   hprops->border.right.radius_start, hprops->border.right.packed.radius_start_is_decimal);
		break;
	case CSS_PROPERTY_border_bottom_left_radius:
		ret_decl = GetComputedBorderRadius(CSSProperty(property),
										   hprops->border.bottom.radius_start, hprops->border.bottom.packed.radius_start_is_decimal,
										   hprops->border.left.radius_end, hprops->border.left.packed.radius_end_is_decimal);
		break;
	case CSS_PROPERTY_border_bottom_right_radius:
		ret_decl = GetComputedBorderRadius(CSSProperty(property),
										   hprops->border.bottom.radius_end, hprops->border.bottom.packed.radius_end_is_decimal,
										   hprops->border.right.radius_end, hprops->border.right.packed.radius_end_is_decimal);
		break;

	case CSS_PROPERTY_flex_grow:
		ret_decl = OP_NEW(CSS_number_decl, (property, hprops->flex_grow, CSS_NUMBER));
		break;
	case CSS_PROPERTY_flex_shrink:
		ret_decl = OP_NEW(CSS_number_decl, (property, hprops->flex_shrink, CSS_NUMBER));
		break;
	case CSS_PROPERTY_flex_basis:
		if (hprops->flex_basis < CONTENT_SIZE_MIN)
			ret_decl = OP_NEW(CSS_type_decl, (property, CSS_VALUE_auto));
		else
			if (hprops->flex_basis < 0)
				ret_decl = OP_NEW(CSS_number_decl, (property, LayoutFixedToFloat(LayoutFixed(-hprops->flex_basis)), CSS_PERCENTAGE));
			else
			{
				const int unit = STYLE_UNIT(hprops->types->m_flex_basis, CSS_PX);
				ret_decl = OP_NEW(CSS_number_decl, (property, float(hprops->flex_basis), unit));
			}
		break;
	case CSS_PROPERTY_justify_content:
	case CSS_PROPERTY_align_content:
	case CSS_PROPERTY_align_items:
	case CSS_PROPERTY_align_self:
	case CSS_PROPERTY_flex_wrap:
	case CSS_PROPERTY_flex_direction:
		ret_decl = OP_NEW(CSS_type_decl, (property, hprops->flexbox_modes.GetValue(CSSProperty(property))));
		break;
	case CSS_PROPERTY_order:
		ret_decl = OP_NEW(CSS_long_decl, (property, hprops->order));
		break;

	// Shorthand properties
	case CSS_PROPERTY_font:
	case CSS_PROPERTY_border:
	case CSS_PROPERTY_margin:
	case CSS_PROPERTY_padding:
	case CSS_PROPERTY_outline:
	case CSS_PROPERTY_overflow:
	case CSS_PROPERTY_background:
	case CSS_PROPERTY_list_style:
	case CSS_PROPERTY_border_top:
	case CSS_PROPERTY_border_left:
	case CSS_PROPERTY_border_color:
	case CSS_PROPERTY_border_right:
	case CSS_PROPERTY_border_style:
	case CSS_PROPERTY_border_width:
	case CSS_PROPERTY_border_bottom:
	case CSS_PROPERTY_border_radius:
	case CSS_PROPERTY_columns:
	case CSS_PROPERTY_column_rule:
	case CSS_PROPERTY_flex:
	case CSS_PROPERTY_flex_flow:
		/* Shorthand properties are not handled here.
		   See ::CSS_IsShorthandProperty() and ::CSS_GetShorthandProperties()
		   in the style module. */

		OP_ASSERT(!"Shorthand properties don't have a computed value");
		break;

#if defined(SVG_SUPPORT)
	case CSS_PROPERTY_fill:
	case CSS_PROPERTY_stroke:
	case CSS_PROPERTY_stop_color:
	case CSS_PROPERTY_flood_color:
	case CSS_PROPERTY_lighting_color:
	case CSS_PROPERTY_stop_opacity:
	case CSS_PROPERTY_stroke_opacity:
	case CSS_PROPERTY_fill_opacity:
	case CSS_PROPERTY_flood_opacity:
	case CSS_PROPERTY_buffered_rendering:
	case CSS_PROPERTY_alignment_baseline:
	case CSS_PROPERTY_clip_rule:
	case CSS_PROPERTY_fill_rule:
	case CSS_PROPERTY_color_interpolation:
	case CSS_PROPERTY_color_interpolation_filters:
	case CSS_PROPERTY_color_rendering:
	case CSS_PROPERTY_dominant_baseline:
	case CSS_PROPERTY_pointer_events:
	case CSS_PROPERTY_shape_rendering:
	case CSS_PROPERTY_stroke_linejoin:
	case CSS_PROPERTY_stroke_linecap:
	case CSS_PROPERTY_text_anchor:
	case CSS_PROPERTY_text_rendering:
	case CSS_PROPERTY_vector_effect:
	case CSS_PROPERTY_writing_mode:
	case CSS_PROPERTY_color_profile:
	case CSS_PROPERTY_glyph_orientation_horizontal: // fall-through is intended
	case CSS_PROPERTY_glyph_orientation_vertical:
	case CSS_PROPERTY_baseline_shift:
	case CSS_PROPERTY_clip_path:
	case CSS_PROPERTY_filter:
	case CSS_PROPERTY_marker:
	case CSS_PROPERTY_marker_end:
	case CSS_PROPERTY_marker_mid:
	case CSS_PROPERTY_marker_start:
	case CSS_PROPERTY_mask:
	case CSS_PROPERTY_enable_background:
	case CSS_PROPERTY_kerning:
	case CSS_PROPERTY_stroke_width:
	case CSS_PROPERTY_stroke_miterlimit:
	case CSS_PROPERTY_stroke_dashoffset:
	case CSS_PROPERTY_stroke_dasharray:
	case CSS_PROPERTY_audio_level:
	case CSS_PROPERTY_solid_color:
	case CSS_PROPERTY_display_align:
	case CSS_PROPERTY_solid_opacity:
	case CSS_PROPERTY_viewport_fill:
	case CSS_PROPERTY_line_increment:
	case CSS_PROPERTY_viewport_fill_opacity:

		/* If there are no SVG properties cascaded to here, the
		   HTMLayoutProperties::svg member field will be NULL, which
		   yields an empty value. */

		if (SvgProperties* svg_props = hprops->svg)
		{
			OpStatus::Ignore(svg_props->GetCSSDecl(&ret_decl, elm, property, pseudo, hld_profile
#ifdef CURRENT_STYLE_SUPPORT
												   ,is_current_style
#endif // CURRENT_STYLE_SUPPORT
												   ));
		}
		break;
#endif // SVG_SUPPORT
	case CSS_PROPERTY_text_overflow:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->text_overflow)));
		break;
#ifdef CSS_MINI_EXTENSIONS
	case CSS_PROPERTY__o_focus_opacity:
		ret_decl = OP_NEW(CSS_number_decl, (property, (float)hprops->focus_opacity / 255.0f, CSS_NUMBER));
		break;
	case CSS_PROPERTY__o_mini_fold:
		ret_decl = OP_NEW(CSS_type_decl, (property, CSSValue(hprops->mini_fold)));
		break;
#endif // CSS_MINI_EXTENSIONS
	}

	return ret_decl;
}

#undef STYLE_UNIT


/* static */ int
LayoutProperties::CalculatePixelValue(HTML_Element* elm, CSS_decl *decl, FramesDocument *frm_doc, BOOL is_horizontal)
{
	OP_ASSERT(decl);

	if (decl->GetDeclType() == CSS_DECL_NUMBER)
	{
		CSSLengthResolver length_resolver(frm_doc->GetVisualDevice());

		short value_type = ((CSS_number_decl*)decl)->GetValueType(0);

		switch (value_type)
		{
		case CSS_PERCENTAGE:
		case CSS_NUMBER:
		case CSS_EM:
		case CSS_REM:
		case CSS_EX:
			OP_ASSERT(elm->Parent());

			if (value_type == CSS_PERCENTAGE)
			{
				HTML_Element* containing_element = Box::GetContainingElement(elm);
				if (containing_element && containing_element->GetLayoutBox())
					length_resolver.SetPercentageBase(float(is_horizontal ? containing_element->GetLayoutBox()->GetWidth() : containing_element->GetLayoutBox()->GetHeight()));
			}
			else if (value_type == CSS_REM)
				length_resolver.SetRootFontSize(frm_doc->RootFontSize());
			else
			{
				AutoDeleteHead prop_list;

				if (LayoutProperties* layoutprops = LayoutProperties::CreateCascade(elm->Parent(), prop_list, frm_doc->GetHLDocProfile()))
				{
					const HTMLayoutProperties& props = layoutprops->GetCascadingProperties();
					length_resolver.SetFontSize(props.decimal_font_size_constrained);
					length_resolver.SetFontNumber(props.font_number);
				}
			}
			break;
		}

		return length_resolver.GetLengthInPixels(((CSS_number_decl*)decl)->GetNumberValue(0), value_type);
	}

	return 0;
}

#ifdef CSS_TRANSITIONS
void LayoutProperties::ReferenceDeclarations()
{
	CSS_decl::Ref(props.bg_images.GetPositions());
	CSS_decl::Ref(props.bg_images.GetSizes());
	CSS_decl::Ref(props.transforms_cp);
	CSS_decl::Ref(props.transition_property);
	CSS_decl::Ref(props.box_shadows.Get());
	CSS_decl::Ref(props.text_shadows.Get());
}

void LayoutProperties::DereferenceDeclarations()
{
	CSS_decl::Unref(props.bg_images.GetPositions());
	CSS_decl::Unref(props.bg_images.GetSizes());
	CSS_decl::Unref(props.transforms_cp);
	CSS_decl::Unref(props.transition_property);
	CSS_decl::Unref(props.box_shadows.Get());
	CSS_decl::Unref(props.text_shadows.Get());
}
#endif // CSS_TRANSITIONS
