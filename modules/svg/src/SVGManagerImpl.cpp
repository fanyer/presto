/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#include "modules/svg/src/SVGManagerImpl.h"

#include "modules/svg/SVGLayoutProperties.h"
#include "modules/svg/svg_module.h"
#include "modules/svg/src/SVGElementStateContext.h"
#include "modules/svg/src/SVGDocumentContext.h"
#include "modules/svg/src/SVGDependencyGraph.h"
#include "modules/svg/src/SVGRenderer.h"
#include "modules/svg/src/SVGCache.h"
#include "modules/svg/src/SVGUtils.h"
#include "modules/svg/src/SVGFontImpl.h"
#include "modules/svg/src/SVGFontSize.h"
#include "modules/svg/src/SVGFontTraversalObject.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGAnimationContext.h"
#include "modules/svg/src/SVGEmbeddedSystemFont.h"
#include "modules/svg/src/SVGDynamicChangeHandler.h"
#include "modules/svg/src/SVGImageImpl.h"
#include "modules/svg/src/SVGNavigation.h"
#include "modules/svg/src/SVGInternalEnumTables.h"
#include "modules/svg/src/SVGAttributeParser.h"
#include "modules/svg/src/SVGEditable.h"
#include "modules/svg/src/SVGChildIterator.h"
#include "modules/svg/src/SVGTextElementStateContext.h"
#include "modules/svg/svg_workplace.h"
#include "modules/svg/src/SVGTextTraverse.h"
#include "modules/svg/src/SVGFilter.h"

#include "modules/svg/src/animation/svganimationworkplace.h"

#ifdef SVG_ANIMATION_LOG
# include "modules/svg/src/animation/svganimationlog.h"
#endif // SVG_ANIMATION_LOG

#ifdef SVG_DOM
# include "modules/dom/domenvironment.h"
#endif // SVG_DOM

#include "modules/display/vis_dev.h"
#include "modules/display/fontcache.h"
#include "modules/display/styl_man.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/layout/cssprops.h"
#include "modules/layout/content/content.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/layoutprops.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/pi/OpClipboard.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpPainter.h"
#include "modules/pi/OpBitmap.h"
#include "modules/url/url2.h"

#include "modules/windowcommander/src/WindowCommander.h"

#ifdef WIC_USE_DOWNLOAD_CALLBACK
# include "modules/windowcommander/src/TransferManagerDownload.h"
#endif

#ifdef QUICK
#include "adjunct/quick/panels/NotesPanel.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#endif // QUICK

#ifdef OPERA_CONSOLE
#include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE

#include "modules/zlib/zlib.h"
#include "modules/minpng/minpng.h"

#ifdef VEGA_OPPAINTER_SUPPORT
# include "modules/libvega/src/oppainter/vegaoppainter.h"
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef SVG_GET_PRESENTATIONAL_ATTRIBUTES
# include "modules/style/css_property_list.h"
#endif // SVG_GET_PRESENTATIONAL_ATTRIBUTES

#ifdef USE_OP_CLIPBOARD
#include "modules/dragdrop/clipboard_manager.h"
#endif // USE_OP_CLIPBOARD

#ifdef SVG_DEBUG_UNIFIED_PARSER
const char* const SVG_object_type_strings[] =
{
	"SVGOBJECT_NUMBER",
	"SVGOBJECT_POINT",
	"SVGOBJECT_LENGTH",
	"SVGOBJECT_ENUM",
	"SVGOBJECT_COLOR",
	"SVGOBJECT_PATH",
	"SVGOBJECT_VECTOR",
	"SVGOBJECT_RECT",
	"SVGOBJECT_PAINT",
	"SVGOBJECT_TRANSFORM",
	"SVGOBJECT_MATRIX",
	"SVGOBJECT_KEYSPLINE",
	"SVGOBJECT_STRING",
	"SVGOBJECT_FONTSIZE",
	"SVGOBJECT_TIME",
	"SVGOBJECT_ROTATE",
	"SVGOBJECT_ANIMATION_TIME",
	"SVGOBJECT_ASPECT_RATIO",
	"SVGOBJECT_URL",
#ifdef SVG_SUPPORT_FILTERS
	"SVGOBJECT_ENABLE_BACKGROUND",
#endif // SVG_SUPPORT_FILTERS
	"SVGOBJECT_BASELINE_SHIFT",
	"SVGOBJECT_ANGLE",
	"SVGOBJECT_ORIENT",
	"SVGOBJECT_PATHSEG",
	"SVGOBJECT_NAVREF",
	"SVGOBJECT_REPEAT_COUNT",
	"SVGOBJECT_CLASSOBJECT",
	"SVGOBJECT_UNKNOWN"
};

void PrintElementAttributeCombination(Markup::Type element_type, Markup::AttrType attr_type, SVGObjectType obj_type)
{
	OP_NEW_DBG("PrintElementAttributeCombination", "svg");
	static int counter =0;
	counter++;
	const uni_char* attr_name_string = g_html5_name_mapper->GetAttrNameFromType(attr_type, Markup::SVG, FALSE);
	const uni_char* element_name_string = g_html5_name_mapper->GetNameFromType(element_type, Markup::SVG, FALSE);

	if (!element_name_string)
		element_name_string = UNI_L("Unknown element");

	OP_DBG((UNI_L("Failed parsing (nr %d): [Element]: [%s]"),
			counter,
			element_name_string));
	OP_DBG(("Failed parsing (nr %d): [%s/%s]",
			counter,
			make_singlebyte_in_tempbuffer(attr_name_string, uni_strlen(attr_name_string)),
			SVG_object_type_strings[obj_type]));
}
#endif // SVG_DEBUG_UNIFIED_PARSER

SVGManager::EventData::EventData() :
		type(DOM_EVENT_NONE),
		target(NULL),
		modifiers(0),
		detail(0),
		screenX(0),
		screenY(0),
		offsetX(0),
		offsetY(0),
		clientX(0),
		clientY(0),
		button(0)
{
}

SVGManagerImpl::SVGManagerImpl() :
#ifdef SVG_SUPPORT_EMBEDDED_FONTS
	m_system_font_manager(NULL),
#endif // SVG_SUPPORT_EMBEDDED_FONTS
#ifdef SVG_SUPPORT_TEXTSELECTION
	m_is_in_textselection_mode(FALSE),
#endif // SVG_SUPPORT_TEXTSELECTION
	m_get_presentation_values(TRUE)
	, m_imageref_id_counter(0)
#ifdef _DEBUG
	, seq_num(0)
	, svg_object_refcount(0)
#endif // _DEBUG
{
	SVGPropertyReference::Protect(&default_fill_prop);
	SVGPropertyReference::Protect(&default_stroke_prop);
	SVGPropertyReference::Protect(&dasharray_isnone_prop);
	dasharray_isnone_prop.is_none = TRUE;
	default_url_reference_prop.url_str = NULL;
	default_url_reference_prop.info.url_str_len = 0;
	default_url_reference_prop.info.is_none = 1;
}

SVGManagerImpl::~SVGManagerImpl()
{
#ifdef SVG_SUPPORT_EMBEDDED_FONTS
	OP_DELETE(m_system_font_manager);
#endif // SVG_SUPPORT_EMBEDDED_FONTS

#ifdef SVG_SUPPORT_ANIMATION_LISTENER
	UnregisterAllAnimationListeners();
#endif // SVG_SUPPORT_ANIMATION_LISTENER
}

OP_STATUS SVGManagerImpl::Create()
{
	m_time_averager.Create(12);

#ifdef SVG_SUPPORT_EMBEDDED_FONTS
	m_system_font_manager = OP_NEW(SVGSystemFontManager, ());
	if (!m_system_font_manager)
		return OpStatus::ERR_NO_MEMORY;
#endif // SVG_SUPPORT_EMBEDDED_FONTS

	default_fill_prop.SetPaintType(SVGPaint::RGBCOLOR);
	default_stroke_prop.SetPaintType(SVGPaint::NONE);

	return OpStatus::OK;
}

#ifdef SVG_SUPPORT_EMBEDDED_FONTS
SVGEmbeddedFontData*
SVGManagerImpl::GetMatchingSystemFont(FontAtt& fontatt)
{
	if(!m_system_font_manager->IsCreated())
	{
		OP_STATUS err = m_system_font_manager->Create();
		OP_ASSERT(!"Failed to load fallback fonts, please put modules/svg/script/fntextract/*.dat into the OPFILE_RESOURCES_FOLDER." == OpStatus::IsError(err));
		if(OpStatus::IsError(err))
			return NULL;
	}
	return m_system_font_manager->GetMatchingSystemFont(fontatt);
}
#endif // SVG_SUPPORT_EMBEDDED_FONTS

OP_BOOLEAN
SVGManagerImpl::TransformToElementCoordinates(HTML_Element* element, SVG_DOCUMENT_CLASS *doc, int& x, int& y)
{
	if(!element || !doc)
		return OpStatus::ERR_NULL_POINTER;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!doc_ctx)
	{
		// Not inside a SVG document (or OOM during element creation)
		return OpStatus::ERR;
	}

	OpPoint scaled_pos(x, y);
	RECT offsetRect = {0,0,0,0};

	// What should we do if Get{Box,}Rect fails?
	if (Box* box = doc_ctx->GetSVGRoot()->GetLayoutBox())
		/*success = */box->GetRect(LOGDOC_DOC(doc), CONTENT_BOX, offsetRect);
	scaled_pos.x = scaled_pos.x - offsetRect.left;
	scaled_pos.y = scaled_pos.y - offsetRect.top;

	VisualDevice* vis_dev = doc->GetVisualDevice();
	scaled_pos = vis_dev->ScaleToScreen(scaled_pos);

	SVGMatrix xfrm_to_elm;
#if 1
	SVGUtils::GetElementCTM(element, doc_ctx, &xfrm_to_elm, TRUE /* ScreenCTM */);
	OP_BOOLEAN res = xfrm_to_elm.Invert() ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
#else
	OP_BOOLEAN res = SVGUtils::GetTransformToElement(doc_ctx->GetSVGRoot(), element,
													 doc_ctx, xfrm_to_elm);
#endif
	if (res == OpBoolean::IS_TRUE)
	{
		SVGNumberPair pos(SVGNumber(scaled_pos.x), SVGNumber(scaled_pos.y));
		pos = xfrm_to_elm.ApplyToCoordinate(pos);

		x = pos.x.GetIntegerValue();
		y = pos.y.GetIntegerValue();
	}
	return res;
}

class TreeIntersection
{
public:
	TreeIntersection(SVGIntersectionObject* isect_obj, SVGDocumentContext* doc_ctx)
		: m_isect_object(isect_obj), m_selected(NULL), m_doc_ctx(doc_ctx) {}

	void SetSelectionVector(OpVector<HTML_Element>* selvector)
	{
		m_selected = selvector;
	}

	OP_STATUS Intersect(const SVGNumberPair& intersection_point)
	{
		RETURN_IF_ERROR(Setup(intersection_point));
		return TestIntersection();
	}

	OP_STATUS Intersect(const SVGRect& selection_rect, BOOL enclosure)
	{
		RETURN_IF_ERROR(Setup(selection_rect, enclosure));
		return TestIntersection();
	}

#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGTextSelection& GetTextSelection()
	{
		OP_ASSERT(m_doc_ctx);
		return m_doc_ctx->GetTextSelection();
	}
#endif // SVG_SUPPORT_TEXTSELECTION

	HTML_Element* GetResultElement()
	{
		return m_canvas.GetLastIntersectedElement();
	}

	void GetResultElementIndex(int& logical_start_char_index,
							   int& logical_end_char_index, BOOL& hit_end_half);

private:
	OP_STATUS Setup(const SVGNumberPair& intersection_point);
	OP_STATUS Setup(const SVGRect& selection_rect, BOOL enclosure);

	OP_STATUS CommonSetup();

	OP_STATUS TestIntersection();

	SVGIntersectionCanvas m_canvas;
	OpRect m_test_area;

	SVGIntersectionObject* m_isect_object;

	OpVector<HTML_Element>* m_selected;

	SVGDocumentContext* m_doc_ctx;
};

OP_STATUS TreeIntersection::CommonSetup()
{
	OpRect canvas_area(0, 0, 1, 1);
	if (VisualDevice* vis_dev = m_doc_ctx->GetVisualDevice())
		m_canvas.SetBaseScale(SVGNumber(vis_dev->GetScale()) / 100);

	m_canvas.ResetTransform();

	RETURN_IF_ERROR(m_isect_object->SetupResolver());

	m_isect_object->SetDocumentContext(m_doc_ctx);
	m_isect_object->SetCanvas(&m_canvas);

	m_canvas.SetLastIntersectedElementHitEndHalf(FALSE);
	m_canvas.SetCurrentElement(NULL);
	m_canvas.SetLastIntersectedElement();
	return OpStatus::OK;
}

OP_STATUS TreeIntersection::Setup(const SVGNumberPair& intersection_point)
{
	RETURN_IF_ERROR(CommonSetup());

	m_test_area.Set(intersection_point.x.GetIntegerValue(),
					intersection_point.y.GetIntegerValue(),
					1, 1);

	m_canvas.SetIntersectionMode(intersection_point);
	return OpStatus::OK;
}

OP_STATUS TreeIntersection::Setup(const SVGRect& selection_rect, BOOL enclosure)
{
	RETURN_IF_ERROR(CommonSetup());

	m_test_area = selection_rect.GetSimilarRect();

	if (enclosure) /* Enclosure testing */
		m_canvas.SetEnclosureMode(selection_rect);
	else /* Intersection testing */
		m_canvas.SetIntersectionMode(selection_rect);

	m_canvas.SetSelectionList(m_selected);
	return OpStatus::OK;
}

OP_STATUS TreeIntersection::TestIntersection()
{
	m_canvas.SetDefaults(m_doc_ctx->GetRenderingQuality());
	m_canvas.ConcatTransform(m_doc_ctx->GetTransform());

	OP_STATUS status = SVGTraverser::Traverse(m_isect_object, m_doc_ctx->GetSVGRoot(), NULL);

#if defined(SVG_LOG_ERRORS) && defined(SVG_LOG_ERRORS_RENDER) && defined(_DEBUG)
	if (OpStatus::IsError(status))
	{
		SVG_NEW_ERROR(m_doc_ctx->GetSVGRoot());
		SVG_REPORT_ERROR((UNI_L("%s"), OpSVGStatus::GetErrorString(status)));
	}
#endif // SVG_LOG_ERRORS && SVG_LOG_ERRORS_RENDER && _DEBUG

	return status;
}

void TreeIntersection::GetResultElementIndex(int& logical_start_char_index, int& logical_end_char_index, BOOL& hit_end_half)
{
	m_canvas.GetLastIntersectedElementIndex(logical_start_char_index, logical_end_char_index);
	hit_end_half = m_canvas.GetLastIntersectedElementIndexHitEndHalf();
}

OP_BOOLEAN SVGManagerImpl::FindTextPosition(SVGDocumentContext* doc_ctx,
											const SVGNumberPair& intersection_point,
											SelectionBoundaryPoint& out_pos)
{
	if (!doc_ctx)
		return OpBoolean::IS_FALSE;

	SVGRenderingTreeChildIterator rtci;
	SVGIntersectionObject isect_obj(&rtci);
	TreeIntersection tree_isect(&isect_obj, doc_ctx);

	RETURN_IF_ERROR(tree_isect.Intersect(intersection_point));

	HTML_Element* intersected = tree_isect.GetResultElement();

	if (!intersected)
		return OpBoolean::IS_FALSE;

	int start_logical_char_index = 0, end_logical_char_index;
	BOOL hit_end_half = TRUE;
	tree_isect.GetResultElementIndex(start_logical_char_index, end_logical_char_index, hit_end_half);

	int intersected_char_ofs = hit_end_half ? end_logical_char_index : start_logical_char_index;
	if (intersected_char_ofs < 0)
		intersected_char_ofs = 0;

	out_pos.SetLogicalPosition(intersected, intersected_char_ofs);
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
SVGManagerImpl::FindSVGElement(HTML_Element* element, SVG_DOCUMENT_CLASS *doc, int x, int y, HTML_Element** event_target, int& offset_x, int& offset_y)
{
	OP_NEW_DBG("SVGManagerImpl::FindSVGElement", "svg_intersection");
	OP_ASSERT(event_target != NULL);

	if (!element->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		return OpBoolean::IS_FALSE;

	SVGDocumentContext* elm_doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!elm_doc_ctx)
		return OpStatus::ERR_NO_MEMORY;

	SVGRenderingTreeChildIterator rtci;
	SVGIntersectionObject isect_obj(&rtci);
	TreeIntersection tree_isect(&isect_obj, elm_doc_ctx);

	OpPoint scaled_pos(x, y);

	HTML_Element* box_element = NULL;
	if (HTML_Element* ref_elm = elm_doc_ctx->GetReferencingElement())
	{
		// External document
		SVGDocumentContext* tm_doc_ctx = SVGUtils::GetTopmostSVGDocumentContext(ref_elm);
		OP_ASSERT(tm_doc_ctx);

		box_element = tm_doc_ctx->GetSVGRoot();

		SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(ref_elm);
		if (elm_ctx)
		{
			const OpRect r = elm_ctx->GetScreenExtents();
			scaled_pos.x += r.x;
			scaled_pos.y += r.y;
		}
	}
	else
	{
		box_element = element;
	}

	RECT offsetRect = {0,0,0,0};

	// What should we do if Get{Box,}Rect fails?
	if (Box* box = box_element->GetLayoutBox())
		/*success = */box->GetRect(LOGDOC_DOC(doc), CONTENT_BOX, offsetRect);
	scaled_pos.x = scaled_pos.x - offsetRect.left;
	scaled_pos.y = scaled_pos.y - offsetRect.top;

	VisualDevice* vis_dev = doc->GetVisualDevice();
	scaled_pos = vis_dev->ScaleToScreen(scaled_pos);

	SVGNumberPair pair((SVGNumber)scaled_pos.x, (SVGNumber)scaled_pos.y);
	HTML_Element* elm = FindTopMostElement(tree_isect, pair);
	if (elm == NULL)
		return OpBoolean::IS_FALSE;

	OP_ASSERT(LOGDOC_DOC(doc)->GetDocRoot() &&
			  LOGDOC_DOC(doc)->GetDocRoot()->IsAncestorOf(elm));

#ifdef _DEBUG
	HTML_Element* orig_elm = elm;
#endif // _DEBUG

	if (elm->Parent())
	{
		// We look at layouted parent here since our parent can be
		// a shadow node
		HTML_Element* layout_parent = SVGUtils::GetElementToLayout(elm->Parent());
		HTML_Element* layout_elm = SVGUtils::GetElementToLayout(elm);
		if (layout_elm->IsText() &&
			layout_parent->GetNsType() == NS_SVG)
		{
			// Markup::HTE_TEXT/HE_TEXT can have no event targets. Select
			// parent instead.
			Markup::Type layout_parent_type = layout_parent->Type();
			if (SVGUtils::IsTextClassType(layout_parent_type) ||
				layout_parent_type == Markup::SVGE_A)
			{
				elm = elm->Parent(); // If 'used', the shadow parent
			}
		}
	}
	else
	{
		OP_ASSERT(!"Intersected element without parent!");
		elm = element;
	}
	*event_target = elm;

#ifdef _DEBUG
	HTML_Element* root = elm_doc_ctx->GetSVGRoot();
	HTML_Element* layout_elm = SVGUtils::GetElementToLayout(root);
	const uni_char* str = HTM_Lex::GetElementString(layout_elm->Type(),
													layout_elm->GetNsIdx(),
													FALSE);
	OP_DBG((UNI_L("Original target: %p [%d] Event target: %p [%d] (layouted) type: %s."),
			orig_elm, orig_elm->Type(), elm, elm->Type(),
			str));
#endif // _DEBUG

	OP_DBG(("XY: %d, %d. Scaled pos: %d, %d.", x, y, scaled_pos.x, scaled_pos.y));

#ifdef SVG_SUPPORT_TEXTSELECTION
	if (IsInTextSelectionMode())
	{
		elm_doc_ctx->GetTextSelection().SetMousePosition(scaled_pos);
	}
#endif // SVG_SUPPORT_TEXTSELECTION

	SVGElementContext* e_ctx = AttrValueStore::GetSVGElementContext(*event_target);
	if (e_ctx)
	{
		const OpRect screen_extents = e_ctx->GetScreenExtents();
		if (screen_extents.IsEmpty())
		{
			OP_DBG(("Found SVG element with empty screen extents!"));
			offset_x = scaled_pos.x;
			offset_y = scaled_pos.y;
		}
		else
		{
			OpPoint scaled_elm_pos = screen_extents.TopLeft();

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			scaled_elm_pos = FROM_DEVICE_PIXEL(vis_dev->GetVPScale(), scaled_elm_pos);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

			offset_x = scaled_pos.x - scaled_elm_pos.x;
			offset_y = scaled_pos.y - scaled_elm_pos.y;
		}
	}
	return OpBoolean::IS_TRUE;
}

OP_BOOLEAN
SVGManagerImpl::HandleEvent(const SVGManager::EventData& data)
{
	if (data.target == NULL)
	{
		RETURN_IF_ERROR(HandleDocumentEvent(data));
		return OpBoolean::IS_TRUE;
	}

	SVGDocumentContext* docctx = AttrValueStore::GetSVGDocumentContext(data.target);

	// 1) The element is not in svg context (or OOM has occured)
	// 2) Filters out events in case SVG is in <img> and ecmascript is disabled
	if (!docctx || !docctx->GetSVGImage()->IsInteractive())
		return OpBoolean::IS_FALSE;

	if (data.type == ONMOUSEDOWN)
	{
		BOOL setFocus = TRUE;
		OpInputContext* input_context = g_input_manager->GetKeyboardInputContext();
		if (input_context && input_context->GetInputContextName() &&
			!op_strcmp(input_context->GetInputContextName(), "SVG Editable"))
		{
			SVGEditable* editable = static_cast<SVGEditable*>(input_context);
			if(editable->IsFocused() && editable->GetEditRoot()->IsAncestorOf(data.target))
			{
				setFocus = FALSE;
			}
		}

		if(setFocus)
			docctx->GetSVGImage()->SetFocus(FOCUS_REASON_MOUSE);
	}

	OP_BOOLEAN status = OpBoolean::IS_FALSE;

#ifdef SVG_SUPPORT_TEXTSELECTION
	if(!IsInTextSelectionMode())
#endif // SVG_SUPPORT_TEXTSELECTION
	{
		SVGAnimationWorkplace *animation_workplace = docctx->GetAnimationWorkplace();
		if (animation_workplace)
		{
			OP_STATUS animation_status = animation_workplace->HandleEvent(data);
			if (OpStatus::IsError(animation_status))
				status = animation_status;
		}

#ifdef SVG_SUPPORT_TEXTSELECTION
		if (status == OpBoolean::IS_FALSE)
		{
			docctx->GetTextSelection().MaybeStartSelection(data);
			if (docctx->GetTextSelection().IsSelecting())
			{
				m_is_in_textselection_mode = TRUE;
				status = OpBoolean::IS_TRUE;
			}
			else if (data.type == ONMOUSEUP)
				docctx->GetTextSelection().ClearSelection();
		}
#endif // SVG_SUPPORT_TEXTSELECTION
	}
#ifdef SVG_SUPPORT_TEXTSELECTION
	else if(data.type == ONMOUSEUP && IsInTextSelectionMode())
	{
		m_is_in_textselection_mode = FALSE;
		status = OpBoolean::IS_TRUE;
		docctx->GetTextSelection().EndSelection(data);
	}
#endif // SVG_SUPPORT_TEXTSELECTION

	/* Panning support */
#ifdef SVG_SUPPORT_PANNING
	if ((data.type == ONMOUSEUP || data.type == ONMOUSEDOWN || data.type == ONMOUSEMOVE || data.type == ONMOUSEOVER || data.type == ONMOUSEOUT)
# ifdef SVG_SUPPORT_TEXTSELECTION
		&& !IsInTextSelectionMode()
# endif // SVG_SUPPORT_TEXTSELECTION
		)
	{
		RETURN_IF_ERROR(docctx->UpdateZoomAndPan(data));
	}
#endif // SVG_SUPPORT_PANNING

	if(data.type == ONERROR)
	{
		HandleSVGEvent(SVGERROR, docctx, data.target);
	}
	else if(data.type == ONRESIZE)
	{
		HandleSVGEvent(SVGRESIZE, docctx, data.target);
	}

	return status;
}

OP_STATUS
SVGManagerImpl::HandleDocumentEvent(const SVGManager::EventData& data)
{
	/* SVGLOAD support */
	if (data.type == SVGLOAD && data.frm_doc)
	{
		LogicalDocument *logdoc = data.frm_doc->GetLogicalDocument();
		if (!logdoc)
		{
			// Bug #253813 seems to indicate that we are sent a load
			// event without having a logical document, so we check
			// for that here.
			return OpStatus::OK;
		}

		SVGWorkplaceImpl* wp = static_cast<SVGWorkplaceImpl*>(logdoc->GetSVGWorkplace());
		wp->SetDocumentBegin(g_op_time_info->GetRuntimeMS());

		SVGImage* svg_image = wp->GetFirstSVG();
		while(svg_image)
		{
			SVGImageImpl* svg_image_impl = static_cast<SVGImageImpl*>(svg_image);
			SVGDocumentContext *doc_ctx = svg_image_impl->GetSVGDocumentContext();

			RETURN_IF_ERROR(SVGAnimationWorkplace::Prepare(doc_ctx, doc_ctx->GetSVGRoot()));
			RETURN_IF_ERROR(HandleSVGLoad(doc_ctx));

			svg_image = wp->GetNextSVG(svg_image);
		}

		if (wp->DecSVGLoadCounter())
		{
			// No events to wait for, animations can start.

			svg_image = wp->GetFirstSVG();
			while(svg_image)
			{
				SVGImageImpl* svg_image_impl = static_cast<SVGImageImpl*>(svg_image);
				SVGDocumentContext *doc_ctx = svg_image_impl->GetSVGDocumentContext();

				if (SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace())
					RETURN_IF_ERROR(animation_workplace->StartTimeline());

				svg_image = wp->GetNextSVG(svg_image);
			}
		}
	}

	return OpStatus::OK;
}

/** Find out if there is an element in this document that is
	interested in an event of type <event_type> */
static OP_BOOLEAN HasReceiver(SVGDocumentContext* doc_ctx, DOM_EventType event_type)
{
	OP_BOOLEAN has_receiver = OpBoolean::IS_FALSE;
	HTML_Element *element = doc_ctx->GetLogicalDocument()->GetDocRoot();
	while (element)
	{
		if (element->HasEventHandlerAttribute(LOGDOC_DOC(doc_ctx->GetDocument()), event_type))
		{
			RETURN_IF_MEMORY_ERROR(doc_ctx->ConstructDOMEnvironment());
			has_receiver = OpBoolean::IS_TRUE;
			break;
		}

		SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(element);
		if (elm_ctx && elm_ctx->ListensToEvent(event_type))
		{
			has_receiver = OpBoolean::IS_TRUE;
		}

		element = element->NextActual();
	}
	return has_receiver;
}

OP_STATUS
SVGManagerImpl::HandleSVGEvent(DOM_EventType event_type, SVGDocumentContext* doc_ctx, HTML_Element* target)
{
	BOOL has_receiver = FALSE;

	DOM_Environment *dom_environment = doc_ctx->GetDOMEnvironment();
	if (!dom_environment)
	{
		OP_BOOLEAN receiver_found = HasReceiver(doc_ctx, event_type);
		RETURN_IF_MEMORY_ERROR(receiver_found);

		if (receiver_found == OpBoolean::IS_TRUE)
		{
			dom_environment = doc_ctx->GetDOMEnvironment();
			has_receiver = TRUE;
		}
	}

	OP_STATUS status = OpStatus::OK;
	if (has_receiver || dom_environment)
	{
		status = SendSVGEvent(doc_ctx, dom_environment, event_type, target);
	}

	return status;
}

OP_STATUS
SVGManagerImpl::HandleSVGLoad(SVGDocumentContext* doc_ctx)
{
	BOOL has_receiver = FALSE;

	DOM_Environment *dom_environment = doc_ctx->GetDOMEnvironment();
	if (!dom_environment)
	{
		OP_BOOLEAN receiver_found = HasReceiver(doc_ctx, SVGLOAD);
		RETURN_IF_MEMORY_ERROR(receiver_found);

		if (receiver_found == OpBoolean::IS_TRUE)
		{
			dom_environment = doc_ctx->GetDOMEnvironment();
			has_receiver = TRUE;
		}
	}

	OP_STATUS status = OpStatus::OK;
	if (has_receiver || dom_environment)
	{
		HTML_Element *element = doc_ctx->GetSVGRoot();
		HTML_Element *stop = element ? element->NextSiblingActual() : NULL;
		while (element && element != stop && !OpStatus::IsMemoryError(status))
		{
			if (element->FirstChildActual() != NULL)
				element = element->FirstChildActual();
			else
			{
				if (SVGUtils::HasLoadedRequiredExternalResources(element))
					status = SendSVGEvent(doc_ctx, dom_environment, SVGLOAD, element);

				HTML_Element *suc = element->SucActual();
				while (!suc && element->ParentActual() && !OpStatus::IsMemoryError(status))
				{
					element = element->ParentActual();
					if (SVGUtils::HasLoadedRequiredExternalResources(element))
						status = SendSVGEvent(doc_ctx, dom_environment, SVGLOAD, element);

					suc = element->SucActual();
				}
				element = suc;
			}
		}
	}

	return status;
}

OP_STATUS
SVGManagerImpl::SendSVGEvent(SVGDocumentContext* doc_ctx,
							 DOM_Environment* dom_environment,
							 DOM_EventType eventtype,
							 HTML_Element* element)
{
	if (element->GetNsType() != NS_SVG)
		return OpStatus::OK;

#ifdef XML_EVENTS_SUPPORT
	// When XML_EVENTS_SUPPORT is supported on core-3 this will have
	// to be adjusted with regards to DOM_Environment::EventData.
	if (dom_environment)
	{
		DOM_Environment::EventData data;
		data.type = eventtype;
		data.target = element;
		OP_BOOLEAN result = dom_environment->HandleEvent(data, NULL);

		// Load counter is increased here, so that we can track when
		// to start animations (when all events have been accounted
		// for).
		if (result == OpBoolean::IS_TRUE && eventtype == SVGLOAD)
			doc_ctx->GetSVGImage()->GetSVGWorkplace()->IncSVGLoadCounter();

		return (OpStatus::IsSuccess(result)) ? OpStatus::OK : result;
	}
	else
#endif // XML_EVENTS_SUPPORT
	{
		SVGAnimationWorkplace* animation_workplace = doc_ctx->GetAnimationWorkplace();
		if (animation_workplace)
		{
			SVGManager::EventData data;
			data.type = eventtype;
			data.target = element;
			animation_workplace->HandleEvent(data);
		}
		return OpStatus::OK;
	}
}

HTML_Element*
SVGManagerImpl::FindTopMostElement(TreeIntersection& tree_isect, const SVGNumberPair& intersection_point)
{
	OP_NEW_DBG("SVGManagerImpl::FindTopMostElement", "svg_events");

	RETURN_VALUE_IF_ERROR(tree_isect.Intersect(intersection_point), NULL);

	HTML_Element* intersected = tree_isect.GetResultElement();

	int start_logical_char_index = 0, end_logical_char_index;
	BOOL hit_end_half = TRUE;
	tree_isect.GetResultElementIndex(start_logical_char_index, end_logical_char_index, hit_end_half);

	if (!intersected)
	{
#ifdef _DEBUG
		OP_DBG(("No element found at point (%d, %d)",
				intersection_point.x.GetIntegerValue(),
				intersection_point.y.GetIntegerValue()));
#endif // _DEBUG
		return NULL;
	}

	OP_DBG(("Found element: %p [%d]. Index: %d-%d. Hit end half: %s.",
			intersected, intersected->Type(),
			start_logical_char_index, end_logical_char_index, hit_end_half ? "yes" : "no"));

#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGTextSelection& sel = tree_isect.GetTextSelection();
	sel.SetCurrentIndex(intersected, start_logical_char_index, end_logical_char_index, hit_end_half);
#endif // SVG_SUPPORT_TEXTSELECTION

	return intersected;
}

OP_STATUS
SVGManagerImpl::SelectElements(SVGDocumentContext* doc_ctx,
							   const SVGRect& selection_rect, BOOL need_enclose,
							   OpVector<HTML_Element>& selected)
{
	OP_NEW_DBG("SVGManagerImpl::SelectElements", "svg_events");

	// Cascading detached trees can crash.
	OP_ASSERT(doc_ctx && doc_ctx->GetSVGImage()->IsInTree());

	SVGRenderingTreeChildIterator rtci;
	SVGRectSelectionObject rectsel_obj(&rtci);
	TreeIntersection tree_isect(&rectsel_obj, doc_ctx);
	tree_isect.SetSelectionVector(&selected);

	OP_STATUS status = tree_isect.Intersect(selection_rect, need_enclose);
	if (OpStatus::IsError(status))
	{
		OP_DBG(("Render in intersection mode failed with status: %d\n", status));
	}
	return status;
}

void SVGManagerImpl::InvalidateCaches(HTML_Element* element, SVG_DOCUMENT_CLASS *doc)
{
	OP_NEW_DBG("SVGManagerImpl::InvalidateCaches", "svg_invalid");
	// Calling this method causes a full rerendering of the SVG which is an
	// expensive operation. It should not be called unless absolutely necessary. Use
	// OnSVGDocumentChanged or similar methods instead.

	OP_ASSERT(element); // AttrValueStore::GetSVGDocumentContext will return NULL if so.

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!doc_ctx) // Not in a svg. Can this happen?
		return;

	doc_ctx->GetSVGImage()->InvalidateAll();
}

void SVGManagerImpl::ReportError(HTML_Element* elm, const uni_char* message, SVG_DOCUMENT_CLASS *doc_fallback /* = NULL */, BOOL report_href /* = FALSE */)
{
#ifdef SVG_LOG_ERRORS
	if (!g_console->IsLogging())
		return;

	SVG_DOCUMENT_CLASS *doc = NULL;
	BOOL allow_report_error = FALSE;
	BOOL first_unreported_error = FALSE;
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
	{
		allow_report_error = TRUE;
		doc = doc_fallback;
		first_unreported_error = FALSE;
	}
	else
	{
		doc = doc_ctx->GetDocument();
		allow_report_error = doc_ctx->AllowReportError();
		first_unreported_error = doc_ctx->IsAtFirstUnreportedError();
		doc_ctx->IncReportErrorCount();
	}

	if (allow_report_error && message)
	{
		URL* report_url = NULL;

		OpConsoleEngine::Message msg(OpConsoleEngine::SVG, OpConsoleEngine::Information);
		if(doc)
		{
			URL doc_url = doc->GetURL();

			URL redirected_to = doc_url.GetAttribute(URL::KMovedToURL,TRUE);
			if (!redirected_to.IsEmpty())
				doc_url = redirected_to;

			doc_url.GetAttribute(URL::KUniName,msg.url);

			if (report_href)
			{
				AttrValueStore::GetXLinkHREF(doc_url, elm, report_url,
											 SVG_ATTRFIELD_DEFAULT);
			}

			msg.window = doc->GetWindow()->Id();
		}
		if (first_unreported_error)
			msg.message.Set("More then 100 errors detected. Aborting error reporting.");
		else
		{
			msg.message.Set(message);
			if (report_url)
			{
				msg.message.Append(" (");
				msg.message.Append(report_url->GetAttribute(URL::KUniName_With_Fragment));
				msg.message.Append(")");
			}
		}
		TRAPD(rc, g_console->PostMessageL(&msg));
		OpStatus::Ignore(rc); // FIXME: What to do with this status?
	}
#endif // SVG_LOG_ERRORS
}

/* virtual */ SVGContext*
SVGManagerImpl::CreateDocumentContext(HTML_Element* svg_root, LogicalDocument* logdoc)
{
	OP_ASSERT(logdoc);
	OP_ASSERT(svg_root);
	OP_ASSERT(!svg_root->GetSVGContext());

	SVGDocumentContext* doc_ctx;
	if (OpStatus::IsError(SVGDocumentContext::Make(doc_ctx, svg_root, logdoc)))
		return NULL;

	return doc_ctx;
}

OP_BOOLEAN SVGManagerImpl::NavigateToElement(HTML_Element *element, SVG_DOCUMENT_CLASS *doc, URL** outurl, DOM_EventType event /* = ONCLICK */, const uni_char** window_target /* = NULL */, ShiftKeyState modifiers /* = SHIFTKEY_NONE */)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
#if 0
	if(doc_ctx)
	{
		// This disables navigation to element if inside <img>
		if(doc_ctx->GetSVGImage()->IsInImgElement())
		{
			return OpBoolean::IS_FALSE;
		}
	}
#endif

#ifdef SVG_SUPPORT_PANNING
	if(event == ONCLICK && modifiers != SHIFTKEY_NONE)
	{
#if 0
		*outurl = NULL;
		if (window_target)
		{
			*window_target = NULL;
		}

		return OpBoolean::IS_TRUE;
#else
		return OpBoolean::IS_FALSE;
#endif
	}
#endif // SVG_SUPPORT_PANNING

	BOOL foundAelement = FALSE;
	HTML_Element* real_element = element;
	while (element != NULL)
	{
		real_element = SVGUtils::GetElementToLayout(element);
		if (real_element && real_element->IsMatchingType(Markup::SVGE_A, NS_SVG))
		{
			foundAelement = TRUE;
			break;
		}
		element = element->Parent();
	}

	if (element && doc_ctx && foundAelement)
	{
		OP_ASSERT(doc == doc_ctx->GetDocument());
		URL* elm_url = NULL;

		URL doc_url = doc->GetURL();
		RETURN_IF_ERROR(AttrValueStore::GetXLinkHREF(doc_url, real_element, elm_url));
		if (elm_url != NULL)
		{
			URL redirected_to = doc_url.GetAttribute(URL::KMovedToURL,TRUE);
			if (!redirected_to.IsEmpty())
			{
				doc_url = redirected_to;
			}
			BOOL is_handled_internally = FALSE;

			SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();
			if (event == ONCLICK && doc_url == *elm_url)
			{
				const uni_char* relname = elm_url->UniRelName();
				HTML_Element *elm = SVGUtils::FindElementById(doc_ctx->GetLogicalDocument(), relname);
				if (elm != NULL)
				{
					if(animation_workplace && SVGUtils::IsAnimationElement(elm))
					{
						OP_STATUS status = animation_workplace->Navigate(elm);
						RETURN_IF_ERROR(status);
						is_handled_internally = TRUE;
					}
					else
					{
#if 0
						// this should probably be in CalculateViewport instead, to catch the case where a user navigates directly to an svg url that has a fragment part
						// center/scale if a fragment id:s is provided, see http://dev.w3.org/SVG/profiles/1.2T/publish/linking.html#SVGFragmentIdentifiers
						SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(elm,FALSE);
						if (elm_ctx)
							doc_ctx->FitRectToViewport(elm_ctx->GetScreenBox());
#endif
					}
				}
			}

			if (!is_handled_internally && !SVGUtils::IsURLEqual(doc_url, *elm_url))
			{
				*outurl = elm_url;
			}
		}

		if (window_target)
		{
			*window_target = NULL;
			const uni_char* show = (const uni_char*)real_element->GetAttr(Markup::XLINKA_SHOW, ITEM_TYPE_STRING, NULL, NS_IDX_XLINK);
			if (show && uni_str_eq(show, "new"))
			{
				*window_target = UNI_L("_blank");
			}
			else if(elm_url && elm_url->Type() != URL_JAVASCRIPT)
			{
				SVGString* svg_target_val;
				const uni_char* svg_target;
				if (OpStatus::IsSuccess(AttrValueStore::GetString(real_element, Markup::SVGA_TARGET, &svg_target_val)) &&
					svg_target_val &&
					(svg_target = svg_target_val->GetString()) != NULL &&
					uni_strlen(svg_target) > 0)
				{
					*window_target = svg_target;

					// we don't support replace, make it _self instead
					if (uni_str_eq(svg_target, "_replace"))
					{
						*window_target = UNI_L("_self");
					}
				}
				else
				{
					// _self is the default value according to SVGT12
					// Load in the parent document which is the document with the SVG when seen from the user.
					*window_target = UNI_L("_self");
				}
			}
		}

	}
	else
	{
		*outurl = NULL;
		if (window_target)
		{
			*window_target = NULL;
		}
	}
	return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
SVGManagerImpl::ParseAttribute(HTML_Element* element,
							   SVG_DOCUMENT_CLASS *doc,
							   int attr_name,
							   int ns_idx,
							   const uni_char* str,
							   unsigned strlen,
							   void** outValue,
							   ItemType &outValueType)
{
	NS_Type ns = g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx));
	SVGObjectType object_type = SVGUtils::GetObjectType(element->Type(),
														attr_name, ns);
	SVGObject *result = NULL;

	OP_STATUS err = ParseAttribute(element, doc, object_type, attr_name, ns_idx, str, strlen, &result);

	if (OpStatus::IsMemoryError(err))
	{
		return err;
	}
	else if(OpStatus::IsError(err))
	{
#ifdef SVG_LOG_ERRORS
		OpString attrstr;
		const uni_char* lexattrstr;
		if (ns_idx == NS_IDX_SVG)
			lexattrstr = SVG_Lex::GetAttrString(static_cast<Markup::AttrType>(attr_name));
		else
			lexattrstr = XLink_Lex::GetAttrString(static_cast<Markup::AttrType>(attr_name));

		OpStatus::Ignore(attrstr.Set(str, strlen));

		const uni_char* element_type_str = element->GetTagName();
		SVG_NEW_ERROR2(doc, element); // Need to include doc since element is probably not in the tree yet
		SVG_REPORT_ERROR((UNI_L("Failed attribute on %s element: %s=\"%s\"."),
						element_type_str ? element_type_str : UNI_L("?"),
						lexattrstr ? lexattrstr : UNI_L("?"), attrstr.CStr()));
#endif // defined(SVG_LOG_ERRORS)
	}

	SVGAttribute *svg_attribute = OP_NEW(SVGAttribute, (result));
	if (!svg_attribute)
	{
		OP_DELETE(result);
		return OpStatus::ERR_NO_MEMORY;
	}

	// Check that ToString on the object returns _exactly_ the
	// same string as the string we parsed the object from. We
	// exclude OpBpath and SVGURL from this step since they got
	// their code for handling this themselves. Strings are also
	// excluded for obvious reasons.
	BOOL store_string = TRUE;

	if (result)
	{
		if (result->Type() != SVGOBJECT_PATH &&
			result->Type() != SVGOBJECT_STRING &&
			result->Type() != SVGOBJECT_URL)
		{
			TempBuffer tmp_buf;
			OP_STATUS status = svg_attribute->ToString(&tmp_buf);
			// If there is a OOM condition, we just don't save the
			// overriding string to the attribute
			if (OpStatus::IsSuccess(status) && tmp_buf.GetStorage())
			{
				if (tmp_buf.Length() == strlen && (strlen == 0 || uni_strncmp(tmp_buf.GetStorage(), str, strlen) == 0))
				{
					store_string = FALSE;
				}
			}
		}
		else
		{
			// trust the path, string and url objects to store strings properly
			store_string = FALSE;
		}
	}

	if(store_string)
	{
		// keep broken attribute values, but allow cases where we have a 'result' object to
		// fallback to using that object for the string if there was an error
		err = svg_attribute->SetOverrideString(str, strlen);
		if(OpStatus::IsError(err) && !result)
		{
			OP_DELETE(svg_attribute);
			return err;
		}
	}

	*outValue = svg_attribute;
	outValueType = ITEM_TYPE_COMPLEX;
	return result ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

OP_STATUS
SVGManagerImpl::ParseAttribute(HTML_Element* element,
							   SVG_DOCUMENT_CLASS *doc,
							   SVGObjectType object_type,
							   int attr_name,
							   int ns_idx,
							   const uni_char* str,
							   unsigned strlen,
							   SVGObject** result)
{
	OP_STATUS err = OpStatus::OK;

	NS_Type attr_ns = g_ns_manager->GetNsTypeAt(element->ResolveNsIdx(ns_idx));

	if (attr_ns == NS_SVG)
		err = ParseSVGAttribute(element, doc, object_type, static_cast<Markup::AttrType>(attr_name), str, strlen, result);
	else if (attr_ns == NS_XLINK)
		err = ParseXLinkAttribute(element, doc, object_type, static_cast<Markup::AttrType>(attr_name), str, strlen, result);
	else
		err = OpSVGStatus::TYPE_ERROR;
	return err;
}

OP_STATUS
SVGManagerImpl::ParseXLinkAttribute(HTML_Element* element,
									SVG_DOCUMENT_CLASS *doc,
									SVGObjectType object_type,
									Markup::AttrType attr_name,
									const uni_char* str,
									unsigned strlen,
									SVGObject** outResult)
{
	OP_ASSERT(doc);
	OP_ASSERT(str);
	if (attr_name != Markup::XLINKA_HREF)
		return OpSVGStatus::TYPE_ERROR;

	URL doc_url = doc->GetURL();

	URL redirected_to = doc_url.GetAttribute(URL::KMovedToURL, TRUE);
	if (!redirected_to.IsEmpty())
	{
		doc_url = redirected_to;
	}

	URL* url = SetUrlAttr(str, strlen, DOCUTIL_SETURLATTR(&doc_url), doc->GetHLDocProfile(), FALSE, TRUE);
	if (url)
	{
		SVGURL* svgurl = OP_NEW(SVGURL, (str, strlen, *url));
		OP_DELETE(url);
		if(svgurl)
		{
			*outResult = svgurl;
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
SVGManagerImpl::ParseSVGAttribute(HTML_Element* element,
								  SVG_DOCUMENT_CLASS *doc,
								  SVGObjectType object_type,
								  Markup::AttrType attr_type,
								  const uni_char* str,
								  unsigned strlen,
								  SVGObject** outResult)
{
	Markup::Type element_type =
		(element != NULL) ?
		element->Type() :
		Markup::HTE_UNKNOWN;

	OP_STATUS status = OpStatus::OK;
	SVGObject* result = NULL;

	if (SVGUtils::IsPresentationAttribute(attr_type, element_type))
	{
		if (strlen == 7 && uni_strncmp(str, "inherit", 7) == 0)
		{
			if(object_type == SVGOBJECT_VECTOR)
			{
				result = SVGUtils::CreateSVGVector(SVGUtils::GetVectorObjectType(element_type, attr_type), element_type, attr_type);
				if(!result)
					return OpStatus::ERR_NO_MEMORY;
			}
			else
				RETURN_IF_ERROR(SVGObject::Make(result, object_type));

			result->SetFlag(SVGOBJECTFLAG_INHERIT);
			result->SetStatus(status);

			// And now... The Quirks!
			switch (object_type)
			{
			case SVGOBJECT_ENUM:
				{
					SVGEnum* enumval = static_cast<SVGEnum*>(result);
					enumval->SetEnumType(SVGUtils::GetEnumType(element_type, attr_type));
				}
				break;
			case SVGOBJECT_PAINT:
				{
					SVGPaintObject* paintobj = static_cast<SVGPaintObject*>(result);
					paintobj->GetPaint().SetPaintType(SVGPaint::INHERIT);
				}
				break;
			}

			*outResult = result;
			return status;
		}
	}

	switch(object_type)
	{
		case SVGOBJECT_COLOR:
		{
			SVGColorObject* color = NULL;
			status = SVGAttributeParser::ParseColorValue(str, strlen, &color,
														 attr_type == Markup::SVGA_VIEWPORT_FILL);
			if (OpStatus::IsMemoryError(status))
				return status;
			result = color;
		}
		break;
		case SVGOBJECT_NUMBER:
		{
            SVGNumber num;

#ifdef SVG_SUPPORT_GRADIENTS
			BOOL normalize_percentage = FALSE;
			if (attr_type == Markup::SVGA_OFFSET && element_type == Markup::SVGE_STOP)
			{
				normalize_percentage = TRUE;
			}
#else
			const BOOL normalize_percentage = FALSE;
#endif // SVG_SUPPORT_GRADIENTS

            status = SVGAttributeParser::ParseNumber(str, strlen, normalize_percentage, num);
            if (OpStatus::IsMemoryError(status))
                return status;
			SVGNumberObject* number = OP_NEW(SVGNumberObject, (num));
            if (!number)
                return OpStatus::ERR_NO_MEMORY;

			result = number;
        }
		break;
		case SVGOBJECT_ROTATE:
		{
			SVGRotate* rotate = NULL;
			status = SVGAttributeParser::ParseRotate(str, strlen, rotate);
            if (OpStatus::IsMemoryError(status))
                return status;
			result = rotate;
		}
		break;
		case SVGOBJECT_ORIENT:
		{
			SVGOrient* orient = NULL;
			status = SVGAttributeParser::ParseOrient(str, strlen, orient);
            if (OpStatus::IsMemoryError(status))
                return status;
			result = orient;
		}
		break;
		case SVGOBJECT_ASPECT_RATIO:
		{
			SVGAspectRatio* ratio = NULL;
			status = SVGAttributeParser::ParsePreserveAspectRatio(str, strlen, ratio);
            if (OpStatus::IsMemoryError(status))
                return status;
			result = ratio;
		}
		break;
		case SVGOBJECT_ANIMATION_TIME:
		{
			SVGAnimationTimeObject* animation_time_object = NULL;
			unsigned extra_value =
				(attr_type == Markup::SVGA_SYNCTOLERANCE) ? 1 /* Allow 'default' */ :
				(attr_type == Markup::SVGA_SYNCTOLERANCEDEFAULT) ? 2 /* Allow 'inherit' */ :
				0 /* Allow 'indefinite' */;

			status = SVGAttributeParser::ParseAnimationTimeObject(str, strlen, animation_time_object,
																  extra_value);
			if (OpStatus::IsMemoryError(status))
				return status;
			result = animation_time_object;
		}
		break;
		case SVGOBJECT_LENGTH:
		{
			SVGLengthObject* length = NULL;
			status = SVGAttributeParser::ParseLength(str, strlen, &length);
			if (OpStatus::IsMemoryError(status))
				return status;
			result = length;
		}
		break;
		case SVGOBJECT_VECTOR:
		{
			SVGObjectType vector_object_type = SVGUtils::GetVectorObjectType(element_type, attr_type);
			if (vector_object_type != SVGOBJECT_UNKNOWN)
			{
				// Special pre-processing here
				SVGVector* vector = SVGUtils::CreateSVGVector(vector_object_type, element_type, attr_type);
				if (!vector)
					return OpStatus::ERR_NO_MEMORY;

				if(attr_type == Markup::SVGA_FONT_FAMILY)
				{
					status = SVGAttributeParser::ParseFontFamily(str, strlen, vector);
				}
				else if(attr_type == Markup::SVGA_STROKE_DASHARRAY)
				{
					status = SVGAttributeParser::ParseDashArray(str, strlen, vector);
				}
				else
				{
					status = SVGAttributeParser::ParseVector(str, strlen, vector,
															 vector_object_type == SVGOBJECT_ENUM ? SVGUtils::GetEnumType(element_type, attr_type) : SVGENUM_UNKNOWN);
				}

				if (OpStatus::IsMemoryError(status))
				{
					OP_DELETE(vector);
					return status;
				}

				// Special post-processing here
				if (attr_type == Markup::SVGA_STROKE_DASHARRAY &&
					vector->GetCount() % 2 == 1)
				{
					UINT32 j = vector->GetCount();
					UINT32 times = j;
					for (UINT32 i=0;i<times && OpStatus::IsSuccess(status);i++, j++)
					{
						SVGObject* cpy = vector->Get(i)->Clone();
						if (cpy != NULL)
							vector->Insert(j, cpy);
						else
							status = OpStatus::ERR_NO_MEMORY;
					}
				}
#ifdef ACCESS_KEYS_SUPPORT
				else if (vector_object_type == SVGOBJECT_TIME)
				{
					// Accesskey
					UINT32 count = vector->GetCount();
					for (UINT32 i=0;i<count;i++)
					{
						SVGTimeObject* time_val = static_cast<SVGTimeObject*>(vector->Get(i));
						if (time_val->TimeType() == SVGTIME_ACCESSKEY)
						{
							SVGTimeObject* event_time = time_val;
							uni_char access_key = event_time->GetAccessKey();
							if (access_key && element && doc)
							{
								HLDocProfile *hld_profile = doc->GetHLDocProfile();
								if (hld_profile)
								{
									uni_char access_key_str[] = {access_key, '\0' };
									RETURN_IF_ERROR(hld_profile->AddAccessKey(access_key_str, element));
								}
							}
							else
							{
								OP_ASSERT(0);
							}
						}
					}
				}
#endif // ACCESS_KEYS_SUPPORT
				else if (attr_type == Markup::SVGA_KEYTIMES)
				{
					UINT32 count = vector->GetCount();
					for (UINT32 i=0;i<count;i++)
					{
						SVGNumberObject* num_val = static_cast<SVGNumberObject*>(vector->Get(i));
						if(num_val->value > 1 || num_val->value < 0)
						{
							// This is an error according to SVG 1.1 and 1.2.
							OP_DELETE(vector);
							vector = NULL;
							break;
						}
					}
				}
				result = vector;
			}
		}
		break;
		case SVGOBJECT_PATH:
		{
			OpBpath* path = NULL;
			status = SVGAttributeParser::ParseToNewPath(str, strlen, FALSE, path);
			if (OpStatus::IsMemoryError(status))
			{
				OP_DELETE(path);
				return status;
			}
			if (path != NULL)
			{
				if (OpStatus::IsMemoryError(path->SetString(str, strlen)))
				{
					OP_DELETE(path);
					return OpStatus::ERR_NO_MEMORY;
				}
			}
			result = path;
		}
		break;
		case SVGOBJECT_ENUM:
		{
			SVGEnumType enum_type = SVGUtils::GetEnumType(element_type, attr_type);
			int enum_val = 0;

			status = SVGAttributeParser::ParseEnumValue(str, strlen, enum_type, enum_val);
			if (OpStatus::IsMemoryError(status))
				return status;

			SVGEnum* value = OP_NEW(SVGEnum, (enum_type, enum_val));
			if (!value)
				return OpStatus::ERR_NO_MEMORY;
			result = value;
		}
		break;
		case SVGOBJECT_RECT:
		{
			SVGRectObject* rect = NULL;
			if (attr_type == Markup::SVGA_CLIP)
				status = SVGAttributeParser::ParseClipShape(str, strlen, &rect);
			else
				status = SVGAttributeParser::ParseViewBox(str, strlen, &rect);
			if (OpStatus::IsMemoryError(status))
				return status;
			result = rect;
		}
		break;
#ifdef SVG_SUPPORT_FILTERS
		case SVGOBJECT_ENABLE_BACKGROUND:
		{
			SVGEnableBackgroundObject* eb = NULL;
			status = SVGAttributeParser::ParseEnableBackground(str, strlen, &eb);
			if (OpStatus::IsMemoryError(status))
				return status;
			result = eb;
		}
		break;
#endif // SVG_SUPPORT_FILTERS
		case SVGOBJECT_BASELINE_SHIFT:
		{
			SVGBaselineShiftObject* bl_shift = NULL;
			status = SVGAttributeParser::ParseBaselineShift(str, strlen, &bl_shift);
			if (OpStatus::IsMemoryError(status))
				return status;
			result = bl_shift;
		}
		break;
		case SVGOBJECT_PAINT:
		{
			SVGPaintObject* paint = NULL;
			status = SVGAttributeParser::ParsePaint(str, strlen, &paint);
			if (OpStatus::IsMemoryError(status))
				return status;
			result = paint;
		}
		break;
		case SVGOBJECT_STRING:
		{
			SVGString* str_val = OP_NEW(SVGString, ());
			if (!str_val)
			{
				return OpStatus::ERR_NO_MEMORY;
			}
			str_val->SetString(str, strlen);
			result = str_val;
		}
		break;
		case SVGOBJECT_FONTSIZE:
		{
			SVGFontSizeObject* font_size = NULL;
			status = SVGAttributeParser::ParseFontSize(str, strlen, &font_size);
			if (OpStatus::IsMemoryError(status))
				return status;
			result = font_size;
		}
		break;
		case SVGOBJECT_NAVREF:
		{
			SVGNavRef* navref = NULL;
			status = SVGAttributeParser::ParseNavRef(str, strlen, &navref);
			if (OpStatus::IsMemoryError(status))
				return status;
			result = navref;
		}
		break;
		case SVGOBJECT_REPEAT_COUNT:
		{
			SVGRepeatCountObject* parsed = NULL;
			status = SVGAttributeParser::ParseRepeatCountObject(str, strlen, parsed);
			if (OpStatus::IsMemoryError(status))
				return status;
			result = parsed;
		}
		break;
		case SVGOBJECT_PROXY:
		{
			SVGObject* real_obj = NULL;

			// Attempt to parse as enum
			SVGEnumType enum_type = SVGUtils::GetEnumType(element_type, attr_type);
			int enum_val = 0;

			status = SVGAttributeParser::ParseEnumValue(str, strlen, enum_type, enum_val);
			if (status == OpSVGStatus::ATTRIBUTE_ERROR)
			{
				SVGObjectType proxy_obj_type = SVGUtils::GetProxyObjectType(element_type, attr_type);
				status = ParseSVGAttribute (element, doc,
							proxy_obj_type, attr_type,
							str, strlen,
							&real_obj);

				if (OpStatus::IsMemoryError(status))
					return status;

				status = OpStatus::OK;
			}
			else
			{
				OP_ASSERT(OpStatus::IsSuccess(status));

				SVGEnum* value = OP_NEW(SVGEnum, (enum_type, enum_val));
				if (!value)
					return OpStatus::ERR_NO_MEMORY;

				real_obj = value;
			}

			SVGProxyObject* proxy = OP_NEW(SVGProxyObject, ());
			if (!proxy)
				OP_DELETE(real_obj);
			else
				proxy->SetRealObject(real_obj);

			result = proxy;
		}
		break;
		case SVGOBJECT_CLASSOBJECT:
		{
			if (doc && doc->GetLogicalDocument())
			{
				SVGClassObject* class_val = OP_NEW(SVGClassObject, ());
				ClassAttribute* class_attr;
				if (!class_val || !(class_attr = doc->GetLogicalDocument()->CreateClassAttribute(str, strlen)))
				{
					OP_DELETE(class_val);
					return OpStatus::ERR_NO_MEMORY;
				}
				class_val->SetClassAttribute(class_attr);
				result = class_val;
			}
		}
		break;
		default:
		{
#ifdef SVG_DEBUG_UNIFIED_PARSER
			PrintElementAttributeCombination(element_type, attr_type, object_type);
#endif // SVG_DEBUG_UNIFIED_PARSER
			result = NULL;
		}
		break;
	}

	if (result != NULL)
	{
		result->SetStatus(status);

		OP_ASSERT(!result->Flag(SVGOBJECTFLAG_INHERIT));

		*outResult = result;

		return status;
	}
	else
	{
		return OpStatus::ERR;
	}
}

static OP_STATUS PerformAnimationCommand(HTML_Element* root, SVG_DOCUMENT_CLASS *doc,
										 SVGAnimationWorkplace::AnimationCommand cmd)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(root);
	if (!doc_ctx)
		return OpStatus::ERR_NO_MEMORY;

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();
	if (animation_workplace)
	{
		RETURN_IF_ERROR(animation_workplace->ProcessAnimationCommand(cmd));
	}
	return OpStatus::OK;
}

/* virtual */
OP_STATUS
SVGManagerImpl::StartAnimation(HTML_Element* root, SVG_DOCUMENT_CLASS *doc)
{
	return UnpauseAnimation(root, doc);
}

/* virtual */
OP_STATUS
SVGManagerImpl::StopAnimation(HTML_Element* root, SVG_DOCUMENT_CLASS *doc)
{
	return PerformAnimationCommand(root, doc, SVGAnimationWorkplace::ANIMATION_STOP);
}

/* virtual */
OP_STATUS
SVGManagerImpl::PauseAnimation(HTML_Element* root, SVG_DOCUMENT_CLASS *doc)
{
	return PerformAnimationCommand(root, doc, SVGAnimationWorkplace::ANIMATION_PAUSE);
}

/* virtual */
OP_STATUS
SVGManagerImpl::UnpauseAnimation(HTML_Element* root, SVG_DOCUMENT_CLASS *doc)
{
	return PerformAnimationCommand(root, doc, SVGAnimationWorkplace::ANIMATION_UNPAUSE);
}

#ifdef ACCESS_KEYS_SUPPORT
/* virtual */
OP_STATUS
SVGManagerImpl::HandleAccessKey(SVG_DOCUMENT_CLASS *doc, HTML_Element* element, uni_char access_key)
{
	OP_ASSERT(doc);
	OP_ASSERT(element && element->GetNsType() == NS_SVG);
	OP_ASSERT(access_key != '\0');
	OP_ASSERT(uni_toupper(access_key) == access_key); // Should be upper case

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(
		SVGUtils::GetTopmostSVGRoot(element));
	if (!doc_ctx)
		return OpStatus::ERR;

	SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace();
	if (animation_workplace)
	{
		animation_workplace->HandleAccessKey(access_key);
	}

	return OpStatus::OK;
}
#endif // ACCESS_KEYS_SUPPORT

TempBuffer *
SVGManagerImpl::GetTempBuf()
{
	return &m_temp_buffer;
}

TempBuffer *
SVGManagerImpl::GetEmptyTempBuf()
{
	m_temp_buffer.Clear();
	return &m_temp_buffer;
}

OpFont* SVGManagerImpl::GetFont(FontAtt &fontatt, UINT32 scale, SVGDocumentContext* doc_ctx)
{
	scale = MAX(1, scale); // 0 for scale is not what we want

	OpFont* font = g_font_cache->GetFont(fontatt, scale, doc_ctx->GetDocument());

	if (font)
	{
		if (font->Type() == OpFontInfo::SVG_WEBFONT)
		{
			// this is an svg font.
			SVGFontImpl* svg_font = static_cast<SVGFontImpl*>(font);
			if (OpStatus::IsError(svg_font->CreateFontContents(doc_ctx)))
			{
				OP_ASSERT(!"If this happens we will draw some string very strange. Shouldn't happen");
				ReleaseFont(font);
				return NULL;
			}
		}
	}
#ifdef SVG_SUPPORT_EMBEDDED_FONTS
	else if (SVGEmbeddedFontData* emb_font = GetMatchingSystemFont(fontatt))
	{
		// The global fontcache didn't return a font, and we found a
		// good (scalable) fallback font.
		font = OP_NEW(SVGFontImpl, (emb_font, fontatt.GetSize() * scale / 100));
		if (font)
		{
			if(OpStatus::IsError(m_svg_opfonts.Add(font)))
			{
				OP_DELETE(font);
				font = NULL;
			}
		}
	}
#endif // SVG_SUPPORT_EMBEDDED_FONTS
	else if(fontatt.GetHasOutline())
	{
		// No fallback font, so we fallback to using OpFonts without GetOutline capability.
		FontAtt oppainter_systemfont;
		oppainter_systemfont = fontatt;
		oppainter_systemfont.SetHasGetOutline(FALSE);
		font = g_font_cache->GetFont(oppainter_systemfont, scale, doc_ctx->GetDocument());
	}

	OP_ASSERT(font);
	return font;
}

void SVGManagerImpl::ReleaseFont(OpFont* font)
{
    if (font == NULL)
        return;
	// First we try delete it from our own cache which will return an error if the font wasn't in there
    if(OpStatus::IsError(m_svg_opfonts.Delete(font)))
    {
        g_font_cache->ReleaseFont(font);
    }
	else
	{
		GetGlyphCache().HandleFontDeletion(font);
	}
}

/*virtual*/ OP_STATUS
SVGManagerImpl::CreatePath(SVGPath** path)
{
    if(!path)
        return OpStatus::ERR_NULL_POINTER;

	OpBpath* internal;
	OP_STATUS result = OpBpath::Make(internal, FALSE);
    *path = internal;
    return result;
}

void
SVGManagerImpl::ForceRedraw(SVGDocumentContext* doc_ctx)
{
	OP_ASSERT(doc_ctx);
	SVGImageImpl *image = doc_ctx->GetSVGImage();
	if(image->IsVisible())
		image->ForceUpdate();
}

#if defined(ACTION_SVG_SET_QUALITY_ENABLED) && defined(PREFS_WRITE)
// Helper method to contain the trap (and its ability to overwrite
// local variables)
static void SaveQualityInPrefs(int quality)
{
	TRAPD(rc, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::SVGRenderingQuality, quality));
	OpStatus::Ignore(rc); // Nothing to do anyway, the pref will just
						  // not "stick"
}
#endif // defined(ACTION_SVG_SET_QUALITY_ENABLED) && defined(PREFS_WRITE)

BOOL SVGManagerImpl::OnInputAction(OpInputAction* action, HTML_Element* clicked_element, SVG_DOCUMENT_CLASS* doc)
{
	if(!action || !doc)
		return FALSE;

	HTML_Element *clicked_root = SVGUtils::GetTopmostSVGRoot(clicked_element);

	if(!clicked_root)
	{
		if(doc->GetLogicalDocument())
			clicked_root = doc->GetLogicalDocument()->GetDocRoot();
		else
			return FALSE;
	}

	if (!clicked_root || !clicked_root->IsMatchingType(Markup::SVGE_SVG, NS_SVG))
		return FALSE;

	SVGDocumentContext* docctx = AttrValueStore::GetSVGDocumentContext(clicked_root);
	if (!docctx)
		return FALSE;

	switch (action->GetAction())
	{
#if defined(PAN_SUPPORT) && defined(SVG_SUPPORT_PANNING)
# ifdef ACTION_COMPOSITE_PAN_ENABLED
		case OpInputAction::ACTION_COMPOSITE_PAN:
		{
			INT16* delta = (INT16*)action->GetActionData();
			INT32 ddx = delta[0], ddy = delta[1];
			delta[0] = delta[1] = 0;
			if (ddx || ddy)
				docctx->Pan(ddx, ddy);
			return TRUE;
		}
# endif // ACTION_COMPOSITE_PAN_ENABLED

# ifdef ACTION_PAN_X_ENABLED
		case OpInputAction::ACTION_PAN_X:
		{
			INT32 ddx = action->GetActionData();
			docctx->GetVisualDevice()->SetPanPerformedX();

			// Surface-shift x
			if (ddx)
				docctx->Pan(ddx, 0);
			return TRUE;
		}
# endif // ACTION_PAN_X_ENABLED
# ifdef ACTION_PAN_Y_ENABLED
		case OpInputAction::ACTION_PAN_Y:
		{
			INT32 ddy = action->GetActionData();
			docctx->GetVisualDevice()->SetPanPerformedY();

			// Surface-shift y
			if (ddy)
				docctx->Pan(0, ddy);
			return TRUE;
		}
# endif // ACTION_PAN_Y_ENABLED
#endif // PAN_SUPPORT && SVG_SUPPORT_PANNING
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			SVGAnimationWorkplace *animation_workplace = docctx->GetAnimationWorkplace();

			switch (child_action->GetAction())
			{
#ifdef SVG_SUPPORT_TEXTSELECTION
# ifdef ACTION_COPY_ENABLED
				case OpInputAction::ACTION_COPY:
					{
						child_action->SetEnabled(g_clipboard_manager->ForceEnablingClipboardAction(OpInputAction::ACTION_COPY, docctx->GetDocument()) || !docctx->GetTextSelection().IsEmpty());
						return TRUE;
					}
# endif // ACTION_COPY_ENABLED
# ifdef ACTION_COPY_TO_NOTE_ENABLED
				case OpInputAction::ACTION_COPY_TO_NOTE:
# endif // ACTION_COPY_TO_NOTE_ENABLED
# if defined(ACTION_COPY_ENABLED) || defined(ACTION_COPY_TO_NOTE_ENABLED)
					{
						child_action->SetEnabled(!docctx->GetTextSelection().IsEmpty());
						return TRUE;
					}
# endif // ACTION_COPY_ENABLED || ACTION_COPY_TO_NOTE_ENABLED
#endif // SVG_SUPPORT_TEXTSELECTION
# ifdef ACTION_SVG_RESET_PAN_ENABLED
				case OpInputAction::ACTION_SVG_RESET_PAN:
# endif // ACTION_SVG_RESET_PAN_ENABLED
#ifdef ACTION_SVG_ZOOM_IN_ENABLED
				case OpInputAction::ACTION_SVG_ZOOM_IN:
#endif
#ifdef ACTION_SVG_ZOOM_OUT_ENABLED
				case OpInputAction::ACTION_SVG_ZOOM_OUT:
#endif
#ifdef ACTION_SVG_ZOOM_ENABLED
				case OpInputAction::ACTION_SVG_ZOOM:
#endif
#if defined(ACTION_SVG_ZOOM_IN_ENABLED) || defined(ACTION_SVG_ZOOM_OUT_ENABLED) || defined(ACTION_SVG_ZOOM_ENABLED)
					{
						child_action->SetEnabled(docctx->GetSVGImage()->IsZoomAndPanAllowed());
						return TRUE;
					}
#endif // ACTION_SVG_ZOOM_IN_ENABLED || ACTION_SVG_ZOOM_OUT_ENABLED || ACTION_SVG_ZOOM_ENABLED
#ifdef ACTION_SVG_SET_QUALITY_ENABLED
				case OpInputAction::ACTION_SVG_SET_QUALITY:
					{
						if(!child_action->GetNextInputAction())
						{
							return TRUE;
						}
						else
						{
							OpInputAction* selectedAction = child_action;

							while(selectedAction)
							{
								if(selectedAction->GetActionData() == docctx->GetRenderingQuality())
								{
									selectedAction->SetSelected(TRUE);
									break;
								}

								selectedAction = selectedAction->GetNextInputAction();
							}
						}

						return TRUE;
					}
#endif // ACTION_SVG_SET_QUALITY_ENABLED
#ifdef ACTION_SVG_START_ANIMATION_ENABLED
				case OpInputAction::ACTION_SVG_START_ANIMATION:
					{
						if (animation_workplace)
						{
							child_action->SetEnabled(animation_workplace->IsValidCommand(SVGAnimationWorkplace::ANIMATION_UNPAUSE));
						}
						return TRUE;
					}
#endif // ACTION_SVG_START_ANIMATION_ENABLED
#ifdef ACTION_SVG_PAUSE_ANIMATION_ENABLED
				case OpInputAction::ACTION_SVG_PAUSE_ANIMATION:
					{
						if (animation_workplace)
						{
							child_action->SetEnabled(animation_workplace->IsValidCommand(SVGAnimationWorkplace::ANIMATION_PAUSE));
						}
						return TRUE;
					}
#endif // ACTION_SVG_START_ANIMATION_ENABLED
#ifdef ACTION_SVG_STOP_ANIMATION_ENABLED
				case OpInputAction::ACTION_SVG_STOP_ANIMATION:
					{
						if (animation_workplace)
						{
							child_action->SetEnabled(animation_workplace->IsValidCommand(SVGAnimationWorkplace::ANIMATION_STOP));
						}
						return TRUE;
					}
#endif // ACTION_SVG_STOP_ANIMATION_ENABLED
#ifdef ACTION_OPEN_IMAGE_ENABLED
				case OpInputAction::ACTION_OPEN_IMAGE:
#endif
#ifdef ACTION_COPY_IMAGE_ADDRESS_ENABLED
				case OpInputAction::ACTION_COPY_IMAGE_ADDRESS:
#endif
#if defined(ACTION_OPEN_IMAGE_ENABLED) || defined(ACTION_COPY_IMAGE_ADDRESS)
				{
					// Should be enabled when we are stand-alone
					// documents
					child_action->SetEnabled(clicked_root->ParentActual()->Type() == HE_DOC_ROOT);
					return TRUE;
				}
#endif
#if defined(ACTION_VIEW_DOCUMENT_SOURCE_ENABLED) && defined(QUICK)
				case OpInputAction::ACTION_VIEW_DOCUMENT_SOURCE:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
#endif
#ifdef ACTION_COPY_IMAGE_ENABLED
				case OpInputAction::ACTION_COPY_IMAGE:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
#endif
#if defined(ACTION_SAVE_IMAGE_ENABLED) && !defined(NO_SAVE_SUPPORT)
				case OpInputAction::ACTION_SAVE_IMAGE:
				{
					child_action->SetEnabled(TRUE);
					return TRUE;
				}
#endif // ACTION_SAVE_IMAGE_ENABLED && !NO_SAVE_SUPPORT
			}
			return FALSE;
		}
#ifdef ACTION_SVG_ZOOM_IN_ENABLED
		case OpInputAction::ACTION_SVG_ZOOM_IN:
#endif
#ifdef ACTION_SVG_ZOOM_OUT_ENABLED
		case OpInputAction::ACTION_SVG_ZOOM_OUT:
#endif
#ifdef ACTION_SVG_ZOOM_ENABLED
		case OpInputAction::ACTION_SVG_ZOOM:
#endif
#if defined(ACTION_SVG_ZOOM_IN_ENABLED) || defined(ACTION_SVG_ZOOM_OUT_ENABLED) || defined(ACTION_SVG_ZOOM_ENABLED)
			{
				if(docctx->GetSVGImage()->IsZoomAndPanAllowed())
				{
					Box* box = clicked_root->GetLayoutBox();
					if(box)
					{
						if (docctx->IsStandAlone() &&
							docctx->GetDocument()->IsTopDocument())
						{
							Window* window = doc->GetWindow();
							INT32 old_scale = window->GetScale();
							INT32 scale = old_scale;

							switch(action->GetAction())
							{
# ifdef ACTION_SVG_ZOOM_IN_ENABLED
								case OpInputAction::ACTION_SVG_ZOOM_IN:
									scale += (INT32)action->GetActionData();
									break;
# endif // ACTION_SVG_ZOOM_OUT_ENABLED
# ifdef ACTION_SVG_ZOOM_OUT_ENABLED
								case OpInputAction::ACTION_SVG_ZOOM_OUT:
									scale -= (INT32)action->GetActionData();
									break;
# endif // ACTION_SVG_ZOOM_OUT_ENABLED
# ifdef ACTION_SVG_ZOOM_ENABLED
								case OpInputAction::ACTION_SVG_ZOOM:
									scale = (INT32)action->GetActionData();
									break;
# endif // ACTION_SVG_ZOOM_ENABLED

								default:
									// Since one define of the three
									// defines must be defined, we
									// avoid getting warned about only
									// having a default case here
									break;
							}

							scale = MAX(1, scale);

							window->SetScale(scale);

							VisualDevice* vis_dev = docctx->GetVisualDevice();
							if (vis_dev && vis_dev->GetView())
							{
								OpPoint screen_origin = vis_dev->GetView()->ConvertToScreen(OpPoint(0, 0));
								OpRect action_pos;
								action->GetActionPosition(action_pos);

								OpPoint screen_view_action_pos(action_pos.x - screen_origin.x,
															   action_pos.y - screen_origin.y);
								OpPoint view_action_pos = vis_dev->ScaleToDoc(screen_view_action_pos);
								OpPoint doc_action_pos(view_action_pos.x + vis_dev->GetRenderingViewX(),
													   view_action_pos.y + vis_dev->GetRenderingViewY());

								OpRect visual_viewport = doc->GetVisualViewport();
								int translation_dx = (scale - old_scale) * (doc_action_pos.x - visual_viewport.x) / old_scale;
								int translation_dy = (scale - old_scale) * (doc_action_pos.y - visual_viewport.y) / old_scale;
								visual_viewport.OffsetBy(translation_dx, translation_dy);

								doc->RequestSetVisualViewport(visual_viewport, VIEWPORT_CHANGE_REASON_INPUT_ACTION);
							}
							return TRUE;
						}

						SVGContent* svgcontent = NULL;
						if (Content* content = box->GetContent())
							svgcontent = content->GetSVGContent();

						if (svgcontent)
						{
# ifndef SVG_SUPPORT_PANNING
							// If this is the first time, then insert
							// a div-element as parent. The reason for
							// adding a DIV-element is so that we get
							// scrollbars if the SVG are bigger than
							// the containing element.
							if (clicked_root->Parent() && clicked_root->Parent()->GetInserted() != HE_INSERTED_BY_LAYOUT)
							{
								HTML_Element* div_element = NEW_HTML_Element();
								HTML_Element* parentOfSVG = clicked_root->Parent();

								if (OpStatus::IsMemoryError(div_element->Construct(doc->GetHLDocProfile(), NS_IDX_HTML, HE_DIV, NULL, HE_INSERTED_BY_LAYOUT)))
								{
									DELETE_HTML_Element(div_element);
									return FALSE;
								}

								div_element->SetInserted(HE_INSERTED_BY_LAYOUT);
								clicked_root->Out();
								div_element->Under(parentOfSVG);
								clicked_root->Under(div_element);

								div_element->MarkDirty(doc, TRUE, TRUE);
							}
# endif // !SVG_SUPPORT_PANNING

							INT32 abs_zoom = (INT32)action->GetActionData();
							SVGNumber relative_zoom_factor = SVGNumber(1.0) + (SVGNumber(abs_zoom) / SVGNumber(100));

							// Current scale
							SVGNumber current_scale = docctx->GetCurrentScale();
							SVGNumber scale = current_scale;

							switch(action->GetAction())
							{
# ifdef ACTION_SVG_ZOOM_IN_ENABLED
								case OpInputAction::ACTION_SVG_ZOOM_IN:
									scale *= relative_zoom_factor;
									break;
# endif // ACTION_SVG_ZOOM_OUT_ENABLED
# ifdef ACTION_SVG_ZOOM_OUT_ENABLED
								case OpInputAction::ACTION_SVG_ZOOM_OUT:
									scale /= relative_zoom_factor;
									break;
# endif // ACTION_SVG_ZOOM_OUT_ENABLED
# ifdef ACTION_SVG_ZOOM_ENABLED
								case OpInputAction::ACTION_SVG_ZOOM:
									scale = SVGNumber(abs_zoom) / SVGNumber(100);
									break;
# endif // ACTION_SVG_ZOOM_ENABLED

								default:
									// Since one define of the three
									// defines must be defined, we
									// avoid getting warned about only
									// having a default case here
									break;
							}

							if(scale < 0)
								scale = 0;

# ifdef ACTION_SVG_RESET_PAN_ENABLED
							OpInputAction* pan_action = action->GetNextInputAction();
							if (pan_action && (pan_action->GetAction() == OpInputAction::ACTION_SVG_RESET_PAN))
							{
								SVGPoint* pt = docctx->GetCurrentTranslate();
								pt->x = 0;
								pt->y = 0;

								docctx->UpdateZoom(scale);
							}
							else
# endif // ACTION_SVG_RESET_PAN_ENABLED
							{
								OpRect action_pos;
								action->GetActionPosition(action_pos);

								OpPoint local_action_pos;

								VisualDevice* vis_dev = docctx->GetVisualDevice();
								if (vis_dev && vis_dev->GetView())
								{
									RECT offset_rect = {0,0,0,0};
									if (box->GetRect(LOGDOC_DOC(doc), CONTENT_BOX, offset_rect))
									{
										OpPoint screen_origin = vis_dev->GetView()->ConvertToScreen(OpPoint(0, 0));
										OpPoint screen_view_action_pos(action_pos.x - screen_origin.x,
																	   action_pos.y - screen_origin.y);
										OpPoint view_action_pos = vis_dev->ScaleToDoc(screen_view_action_pos);
										OpPoint doc_action_pos(view_action_pos.x + vis_dev->GetRenderingViewX(),
															   view_action_pos.y + vis_dev->GetRenderingViewY());
										local_action_pos.x = doc_action_pos.x - offset_rect.left;
										local_action_pos.y = doc_action_pos.y - offset_rect.top;
									}
								}

								docctx->ZoomAroundPointBy(local_action_pos, scale - current_scale);
							}
							return TRUE;
						}
					}
				}
			}
			return FALSE;
#endif // ACTION_SVG_ZOOM_ENABLED || ACTION_SVG_ZOOM_OUT_ENABLED || ACTION_SVG_ZOOM_IN_ENABLED
# ifdef ACTION_SVG_RESET_PAN_ENABLED
#ifdef SVG_SUPPORT_PANNING
		case OpInputAction::ACTION_SVG_RESET_PAN:
			{
				if(docctx->GetSVGImage()->IsZoomAndPanAllowed())
				{
					docctx->ResetPan();
					return TRUE;
				}
			}
			return FALSE;
#endif // SVG_SUPPORT_PANNING
# endif // ACTION_SVG_RESET_PAN_ENABLED
#ifdef ACTION_OPEN_IMAGE_ENABLED
		case OpInputAction::ACTION_OPEN_IMAGE:
			{
				// If we are in a IFRAME or stand-alone, this URL
				// should be the correct one.  Otherwise we are part
				// of a xhtml file and then this action shouldn't be
				// enabled.
				URL url = doc->GetURL();

				g_windowManager->OpenURLNamedWindow(url, doc->GetWindow(),
													doc, doc->GetSubWinId(),
													UNI_L("_parent"), TRUE,
													FALSE, FALSE, TRUE);
				return TRUE;
			}
#endif // ACTION_OPEN_IMAGE_ENABLED
#if defined(ACTION_VIEW_DOCUMENT_SOURCE_ENABLED) && defined(QUICK)
		case OpInputAction::ACTION_VIEW_DOCUMENT_SOURCE:
			{
				URL url = doc->GetURL();
				DocumentDesktopWindow *active_doc_window = g_application->GetActiveDocumentDesktopWindow();
				g_application->OpenSourceViewer(&url, (active_doc_window ? active_doc_window->GetID() : 0), active_doc_window->GetWindowCommander()->GetCurrentTitle());
				return TRUE;
			}
#endif // ACTION_VIEW_DOCUMENT_SOURCE_ENABLED && QUICK
#ifdef ACTION_COPY_IMAGE_ADDRESS_ENABLED
		case OpInputAction::ACTION_COPY_IMAGE_ADDRESS:
			{
				OpString url_string;
				URL url = doc->GetURL();

				// This code is inspired by
				// OpInputAction::ACTION_COPY_LINK in
				// adjunct/quick/widgets/OpBrowserView.cpp
				if (!url.IsEmpty())
				{
					url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden_Escaped, url_string);

					// Clipboard
					g_clipboard_manager->CopyURLToClipboard(url_string.CStr(), doc->GetWindow()->GetUrlContextId());
#if defined(_X11_SELECTION_POLICY_)
					// Mouse selection
					g_clipboard_manager->SetMouseSelectionMode(TRUE);
					g_clipboard_manager->CopyURLToClipboard(url_string.CStr(), doc->GetWindow()->GetUrlContextId());
					g_clipboard_manager->SetMouseSelectionMode(FALSE);
#endif
				}
				return TRUE;
			}
#endif // ACTION_COPY_IMAGE_ADDRESS

#if defined(ACTION_SVG_SET_QUALITY_ENABLED) && defined(PREFS_WRITE)
		case OpInputAction::ACTION_SVG_SET_QUALITY:
			{
				// This is an toggle between two different quality
				// settings. If current is enabled read the argument
				// of the next input action and set that as current
				// quality setting. Otherwise read the argument of the
				// next input action and use that as quality setting.
				// Needs to be rewritten if we want to support more
				// than two quality settings.

				int quality;
				if (action->GetNextInputAction() &&
					action->GetActionState() & OpInputAction::STATE_SELECTED)
				{
					quality = action->GetNextInputAction()->GetActionData();
				}
				else
				{
					quality = action->GetActionData();
				}

				if(g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::SVGRenderingQuality) != quality)
				{
					// Change default quality to this, the last set value
					SaveQualityInPrefs(quality);
				}

				if (quality != docctx->GetRenderingQuality())
				{
					// This is a global setting so we should really set it for all svg:s in the system.
					SVGDynamicChangeHandler::MarkWholeSVGForRepaint(docctx);
				}
			}
			return TRUE;
#endif // ACTION_SVG_SET_QUALITY_ENABLED && PREFS_WRITE
#ifdef ACTION_SVG_START_ANIMATION_ENABLED
		case OpInputAction::ACTION_SVG_START_ANIMATION:
			{
				if(OpStatus::IsSuccess(StartAnimation(clicked_root, doc)))
					return TRUE;
				return FALSE;
			}
#endif // ACTION_SVG_START_ANIMATION_ENABLED
#ifdef ACTION_SVG_STOP_ANIMATION_ENABLED
		case OpInputAction::ACTION_SVG_STOP_ANIMATION:
			if(OpStatus::IsSuccess(StopAnimation(clicked_root, doc)))
				return TRUE;
			return FALSE;
#endif // ACTION_SVG_STOP_ANIMATION_ENABLED
#ifdef ACTION_SVG_PAUSE_ANIMATION_ENABLED
		case OpInputAction::ACTION_SVG_PAUSE_ANIMATION:
			if(OpStatus::IsSuccess(PauseAnimation(clicked_root, doc)))
				return TRUE;
			return FALSE;
#endif // ACTION_SVG_PAUSE_ANIMATION_ENABLED
#if defined(ACTION_SAVE_IMAGE_ENABLED) && !defined(NO_SAVE_SUPPORT)
		case OpInputAction::ACTION_SAVE_IMAGE:
			{
				URL url = doc->GetURL();
# ifdef WIC_USE_DOWNLOAD_CALLBACK
				VisualDevice *vd = docctx->GetVisualDevice();
				if (!vd)
					return FALSE;
				DocumentManager* doc_man = doc->GetDocManager();
				ViewActionFlag view_action_flag;
				view_action_flag.Set(ViewActionFlag::SAVE_AS);
				TransferManagerDownloadCallback * download_callback = OP_NEW(TransferManagerDownloadCallback, (doc_man, url, NULL, view_action_flag));
				if (download_callback &&
					vd->GetWindow() &&
					vd->GetWindow()->GetWindowCommander())
				{
					WindowCommander * wic;
					wic = vd->GetWindow()->GetWindowCommander();
					wic->GetDocumentListener()->OnDownloadRequest(wic, download_callback);
					download_callback->Execute();
				}
				else
					OP_DELETE(download_callback);
				return TRUE;
# else // !WIC_USE_DOWNLOAD_CALLBACK
				doc->GetWindow()->SaveAs(url, FALSE, FALSE);
				return TRUE;
# endif // !WIC_USE_DOWNLOAD_CALLBACK
			}
#endif // ACTION_SAVE_IMAGE_ENABLED && !NO_SAVE_SUPPORT
#ifdef ACTION_COPY_IMAGE_ENABLED
		case OpInputAction::ACTION_COPY_IMAGE:
			{
				VisualDevice* vd = docctx->GetVisualDevice();
				if (!vd)
					return FALSE;

				OpRect image_rect = vd->ScaleToScreen(docctx->GetSVGImage()->GetDocumentRect());
				image_rect.width = (docctx->GetCurrentScale() * image_rect.width).GetIntegerValue();
				image_rect.height = (docctx->GetCurrentScale() * image_rect.height).GetIntegerValue();

				SVGUtils::LimitCanvasSize(docctx->GetDocument(), vd, image_rect.width, image_rect.height);

				image_rect.x = image_rect.y = 0;

				SVGRenderer renderer;
				RETURN_VALUE_IF_ERROR(renderer.Create(docctx, image_rect.width, image_rect.height, image_rect.width, image_rect.height, 1.0), FALSE);

				renderer.SetAllowTimeout(FALSE);
				renderer.SetPolicy(SVGRenderer::POLICY_SYNC);
				RETURN_VALUE_IF_ERROR(renderer.Setup(image_rect), FALSE);

				OP_STATUS status = renderer.Update();
				if (OpStatus::IsSuccess(status))
				{
					// Allocate bitmap
					OpBitmap* result_bitmap = NULL;
					status = OpBitmap::Create(&result_bitmap, image_rect.width, image_rect.height, FALSE, TRUE, 0, 0, FALSE);

					if (OpStatus::IsSuccess(status))
					{
						if (OpStatus::IsSuccess(status = renderer.CopyToBitmap(image_rect, &result_bitmap)))
							g_clipboard_manager->PlaceBitmap(result_bitmap, doc->GetWindow()->GetUrlContextId());
						OP_DELETE(result_bitmap);
						return OpStatus::IsSuccess(status) ? TRUE : FALSE;
					}
				}
			}
			return FALSE;
#endif // ACTION_COPY_IMAGE_ENABLED
#ifdef SVG_SUPPORT_TEXTSELECTION
# ifdef ACTION_COPY_ENABLED
		case OpInputAction::ACTION_COPY:
# endif // ACTION_COPY_ENABLED
# if defined(ACTION_COPY_TO_NOTE_ENABLED) && defined(QUICK)
		case OpInputAction::ACTION_COPY_TO_NOTE:
# endif // ACTION_COPY_TO_NOTE_ENABLED && defined(QUICK)
# if defined(ACTION_COPY_ENABLED) || (defined(ACTION_COPY_TO_NOTE_ENABLED) && defined(QUICK))
		{
#  ifdef ACTION_COPY_ENABLED
			if (action->GetAction() == OpInputAction::ACTION_COPY)
			{
#   ifdef USE_OP_CLIPBOARD
				g_clipboard_manager->Copy(doc->GetWindow(), doc->GetWindow()->GetUrlContextId(), doc);
#   endif // USE_OP_CLIPBOARD
				return TRUE;
			}
			else
#  endif // ACTION_COPY_ENABLED
			{
				if (!docctx->GetTextSelection().IsEmpty())
				{
					INT32 sel_text_len = docctx->GetTextSelection().GetText(NULL);
					TempBuffer buffer;
					OP_STATUS err = buffer.Expand(sel_text_len + 1);

					if (OpStatus::IsSuccess(err) && docctx->GetTextSelection().GetText(&buffer))
					{
#  if defined(ACTION_COPY_TO_NOTE_ENABLED) && defined(QUICK)
						if (action->GetAction() == OpInputAction::ACTION_COPY_TO_NOTE)
						{
							NotesPanel::NewNote(buffer.GetStorage(), doc->GetURL().GetAttribute(URL::KUniName_Username_Password_Hidden,TRUE).CStr());
							return TRUE;
						}
#  endif // ACTION_COPY_TO_NOTE_ENABLED && QUICK
					}
				}
			}
			return FALSE;
		}
# endif // ACTION_COPY_ENABLED || (ACTION_COPY_TO_NOTE && QUICK)
# ifdef ACTION_DESELECT_ALL_ENABLED
		case OpInputAction::ACTION_DESELECT_ALL:
		{
			docctx->GetTextSelection().ClearSelection();
			if (HTML_Document* html_doc = doc->GetHtmlDocument())
				html_doc->SetElementWithSelection(NULL);
			return FALSE;
		}
# endif // ACTION_DESELECT_ALL_ENABLED
#endif // SVG_SUPPORT_TEXTSELECTION

		default:
			return FALSE;
	}
}

/* virtual */
OP_STATUS SVGManagerImpl::OnSVGDocumentChanged(SVG_DOCUMENT_CLASS *doc, HTML_Element *parent, HTML_Element *child, BOOL is_addition)
{
	LogicalDocument* log_doc = doc->GetLogicalDocument();

	if (!log_doc)
		return OpStatus::OK; /* Nothing to do */

	SVGWorkplaceImpl* workplace = static_cast<SVGWorkplaceImpl*>(log_doc->GetSVGWorkplace());

	if (parent && parent->GetNsType() == NS_SVG || child->GetNsType() == NS_SVG)
	{
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext((parent && parent->GetNsType() == NS_SVG) ? parent : child);

		if (!doc_ctx)
		{
			// If there is no SVGDocumentContext, remove the subtree directly.
			// While this situation is normally not expected, it may for
			// example happen with resource proxy elements.
			if (!is_addition)
				if (SVGDependencyGraph *dgraph = workplace->GetDependencyGraph())
					dgraph->RemoveSubTree(child);

			return OpStatus::OK;
		}

		return SVGDynamicChangeHandler::HandleDocumentChanged(doc_ctx, parent, child, is_addition);
	}
	else if (is_addition)
	{
		OP_ASSERT(parent == child->Parent());
		// Look for svg:s in the tree rooted in something non-svg
		HTML_Element* stop = static_cast<HTML_Element*>(child->NextSibling());
		HTML_Element* iterator = child;
		while (iterator != stop)
		{
			if (iterator->IsMatchingType(Markup::SVGE_SVG, NS_SVG) && iterator->Parent()->GetNsType() != NS_SVG)
			{
				SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(iterator);
				if (!doc_ctx)
				{
					return OpStatus::ERR_NO_MEMORY;
				}

				RETURN_IF_ERROR(SVGDynamicChangeHandler::HandleDocumentChanged(doc_ctx, iterator->Parent(), iterator, is_addition));

			}
			iterator = iterator->Next();
		}

		// If we have a change in a child of a foreignObject, mark the element for repaint
		HTML_Element* parent_iter = parent;
		while (parent_iter)
		{
			if (parent_iter->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG))
			{
				if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(parent_iter))
					RETURN_IF_ERROR(SVGDynamicChangeHandler::RepaintElement(doc_ctx, parent_iter));

				break;
			}

			parent_iter = parent_iter->Parent();
		}
	}
	else if(SVGDependencyGraph *dgraph = workplace->GetDependencyGraph())
	{
		dgraph->RemoveSubTree(child);
	}

#ifdef SVG_DEBUG_DEPENDENCY_GRAPH
	if(SVGDependencyGraph *dgraph = workplace->GetDependencyGraph())
	{
		if(HTML_Element* doc_root = logdoc->GetDocRoot())
		{
			dgraph->CheckConsistency(doc_root);
		}
	}
#endif // SVG_DEBUG_DEPENDENCY_GRAPH

	return OpStatus::OK;
}

/* virtual */
OP_STATUS SVGManagerImpl::HandleCharacterDataChanged(SVG_DOCUMENT_CLASS *doc, HTML_Element* element)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!doc_ctx)
	{
		// We don't care for changes outside an SVG
		return OpStatus::OK;
	}

	return SVGDynamicChangeHandler::HandleCharacterDataChanged(doc_ctx, element);
}

/* virtual */
OP_STATUS SVGManagerImpl::RepaintElement(SVG_DOCUMENT_CLASS *doc, HTML_Element* element)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!doc_ctx)
	{
		// We don't care for changes outside an SVG
		return OpStatus::OK;
	}

	return SVGDynamicChangeHandler::RepaintElement(doc_ctx, element);
}

/* virtual */
OP_STATUS SVGManagerImpl::HandleInlineIgnored(SVG_DOCUMENT_CLASS* doc, HTML_Element* element)
{
	OP_ASSERT(doc);
	OP_ASSERT(doc->GetLogicalDocument());

	if(!element)
		return OpStatus::ERR_NULL_POINTER;

	if(SVGUtils::IsExternalProxyElement(element))
	{
		URL* url = NULL;
		AttrValueStore::GetXLinkHREF(doc->GetURL(), element, url);
		if(url)
		{
			FramesDocElm *frame = FramesDocElm::GetFrmDocElmByHTML(element);
			if(frame)
			{
				SVGWorkplaceImpl* svg_workplace = static_cast<SVGWorkplaceImpl*>(doc->GetLogicalDocument()->GetSVGWorkplace());
				svg_workplace->SignalInlineIgnored(*url);
			}
		}
	}

	return OpStatus::OK;
}

/* virtual */
OP_STATUS SVGManagerImpl::HandleInlineChanged(SVG_DOCUMENT_CLASS* doc, HTML_Element* element, BOOL is_content_changed)
{
	if(!element)
		return OpStatus::ERR_NULL_POINTER;

	if (SVGUtils::IsExternalProxyElement(element))
	{
		URL* url = NULL;
		AttrValueStore::GetXLinkHREF(doc->GetURL(), element, url);
		if(url)
		{
			FramesDocElm *frame = FramesDocElm::GetFrmDocElmByHTML(element);
			if(frame && frame->IsLoaded())
			{
				SVGWorkplaceImpl* svg_workplace = static_cast<SVGWorkplaceImpl*>(doc->GetLogicalDocument()->GetSVGWorkplace());
				svg_workplace->SignalInlineChanged(*url);

				// We do SVGDynamicChangeHandler::HandleInlineChanged from the workplace in
				// SignalInlineChanged, so we shouldn't have to do it again further down in
				// this method.
				return OpStatus::OK;
			}
		}
	}

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!doc_ctx || doc->IsBeingDeleted())
	{
		// We don't care for changes outside an SVG or to do anything if the document is being deleted
		return OpStatus::OK;
	}

	return SVGDynamicChangeHandler::HandleInlineChanged(doc_ctx, element, is_content_changed);
}

/* virtual */
OP_STATUS SVGManagerImpl::HandleStyleChange(SVG_DOCUMENT_CLASS *doc, HTML_Element* element, unsigned int changes)
{
	OP_ASSERT(element && element->GetNsType() == NS_SVG);
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);

	if (!doc_ctx)
		return OpStatus::OK;

	// Don't do anything with elements that belong to a font. Workaround for bug #343541.
	HTML_Element* parent = element;
	while (parent)
	{
		if (parent->GetNsType() == NS_SVG &&
			(parent->Type() == Markup::SVGE_FONT ||
			 parent->Type() == Markup::SVGE_FONT_FACE))
			return OpStatus::OK;

		parent = parent->Parent();
	}

	if (SVGUtils::IsExternalProxyElement(element))
		return OpStatus::OK;

	if (changes & PROPS_CHANGED_SVG_RELAYOUT)
		if (SVGElementContext* elem_ctx = AttrValueStore::GetSVGElementContext_Unsafe(element))
		{
			elem_ctx->RevalidateBuffering();
			elem_ctx->AddInvalidState(INVALID_FONT_METRICS);
		}

	return SVGDynamicChangeHandler::HandleElementChange(doc_ctx, element, changes);
}

/* virtual */
OP_STATUS SVGManagerImpl::HandleSVGAttributeChange(SVG_DOCUMENT_CLASS *doc, HTML_Element* element, Markup::AttrType attr, NS_Type attr_ns, BOOL was_removed)
{
	// This must be done regardless of where the element is
	if (attr_ns == NS_SVG)
	{
		if (attr == Markup::SVGA_REQUIREDFEATURES ||
			attr == Markup::SVGA_REQUIREDEXTENSIONS ||
			attr == Markup::SVGA_SYSTEMLANGUAGE)
		{
			SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(element);
			if (elm_ctx)
			{
				// This change could effects on the structure, so mark a potential parent
				HTML_Element* parent = element->Parent();
				if (SVGElementContext* parent_context = AttrValueStore::GetSVGElementContext(parent))
					// Setting ADDED, since we want the childlist to
					// be rebuilt, and the new state reflected in the
					// paint tree as well. Preferably we would set
					// STRUCTURE here, but then we'd need to make sure
					// that a potential paint node is yanked from the
					// paint tree.
					parent_context->AddInvalidState(INVALID_ADDED);

				elm_ctx->InvalidateRequirementsCalculation();
			}
		}
	}

	// We use this hook to mark attributes as initialized
	if (!was_removed && (attr_ns == NS_SVG || attr_ns == NS_XLINK) && attr != ATTR_XML)
	{
		AttrValueStore::MarkAsInitialized(element, attr,
										  (attr_ns == NS_SVG) ? NS_IDX_SVG : NS_IDX_XLINK);
	}

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!doc_ctx)
	{
		// We don't care for nodes outside the svg document
		return OpStatus::OK;
	}

	return SVGDynamicChangeHandler::HandleAttributeChange(doc_ctx, element, attr, attr_ns, was_removed);
}

HTML_Element* SVGManagerImpl::GetEventTarget(HTML_Element* svg_elm)
{
	HTML_Element* event_target = SVGUtils::GetElementToLayout(svg_elm);

	if (event_target != svg_elm)
	{
		// This is to prevent us from creating DOM nodes / event targets that
		// are in an external tree.
		HTML_Element* root_1 = svg_elm;
		while(root_1 && root_1->Parent())
		{
			root_1 = root_1->Parent();
		}

		HTML_Element* root_2 = event_target;
		while(root_2 && root_2->Parent())
		{
			root_2 = root_2->Parent();
		}

		if(root_1 != root_2)
		{
			HTML_Element* first_non_shadownode = svg_elm;
			while(first_non_shadownode && SVGUtils::IsShadowNode(first_non_shadownode))
			{
				first_non_shadownode = first_non_shadownode->Parent();
			}

			OP_ASSERT(first_non_shadownode);
			return first_non_shadownode ? first_non_shadownode : event_target;
		}
#ifdef _DEBUG
		OP_ASSERT(TRUE); // Somewhere to put a breakpoint
#endif // _DEBUG
	}
	return event_target;
}

CursorType SVGManagerImpl::GetCursorForElement(HTML_Element* elm)
{
	OP_ASSERT(elm);
	OP_ASSERT(elm->GetNsType() == NS_SVG);
	OP_ASSERT(elm->HasCursorSettings());

	// This is for animation of css properties to work
	if(AttrValueStore::HasObject(elm, Markup::SVGA_CURSOR, NS_IDX_SVG))
	{
		BOOL override_css;
		CursorType cursor;
		if(OpStatus::IsSuccess(AttrValueStore::GetCursor(elm, cursor, &override_css)) && override_css)
		{
			return cursor;
		}
		else
		{
			// We should use CSS if it exist so we have to check if it does
			CssPropertyItem* pi = CssPropertyItem::GetCssPropertyItem(elm, CSSPROPS_CURSOR);

			if (!pi)
			{
				return cursor;
			}
		}
	}

	return elm->GetCursorType();
}

/* virtual */ URL*
SVGManagerImpl::GetXLinkURL(HTML_Element* elm, URL* root_url /*= NULL */)
{
	URL* ret_url;
	if(root_url)
	{
		OpStatus::Ignore(AttrValueStore::GetXLinkHREF(*root_url, elm, ret_url));
	}
	else
	{
		OpStatus::Ignore(AttrValueStore::GetXLinkHREF(URL(), elm, ret_url, SVG_ATTRFIELD_DEFAULT, TRUE));
	}
	return ret_url;
}

BOOL
SVGManagerImpl::IsVisible(HTML_Element* elm)
{
	if (SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(elm))
		return !elm_ctx->GetScreenExtents().IsEmpty();

	return FALSE;
}

BOOL
SVGManagerImpl::IsTextContentElement(HTML_Element* elm)
{
	return (elm->GetNsType() == NS_SVG && SVGUtils::IsTextClassType(elm->Type()));
}

/*virtual */
SVGImage* SVGManagerImpl::GetSVGImage(LogicalDocument* log_doc,
									  HTML_Element* svg_elm)
{
	OP_ASSERT(log_doc);

	if(!svg_elm)
		return NULL;

	//OP_ASSERT(svg_elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG));
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(svg_elm);
	if (doc_ctx && doc_ctx->GetSVGRoot() != svg_elm)
	{
		// This wasn't the topmost svg element inside the svg
		return NULL;
	}

	if (!doc_ctx && SVGUtils::GetTopmostSVGRoot(svg_elm) == svg_elm)
	{
		// Missing document context. Possibly a cloned SVG node, lost its DocumentContext
		doc_ctx = static_cast<SVGDocumentContext*>(CreateDocumentContext(svg_elm, log_doc));

		OP_ASSERT(!doc_ctx || svg_elm->GetSVGContext() == doc_ctx);
	}

	if (doc_ctx)
	{
		SVGImage* svg_image = doc_ctx->GetSVGImage();
		return svg_image;
	}
	return NULL;
}

/* virtual */
OP_STATUS SVGManagerImpl::GetToolTip(HTML_Element* elm, OpString& result)
{
	OP_ASSERT(elm);
	OP_ASSERT(result.IsEmpty()); // Since we'll leave it as it is
	if (elm->GetNsType() == NS_SVG && (int)elm->Type() != Markup::SVGE_SVG)
	{
		HTML_Element* child = elm->FirstChild();

		int child_count = 0;
		// We only look 20 elements into the children to avoid slowing down mousemoves.
		// This can be done without too bad conscience because of this paragraph
		// in the SVG 1.1 specification:
		/*
			Representations of future versions of the SVG language might use
			more expressive representations than DTDs which allow for more
			restrictive mixed content rules. It is strongly recommended that
			at most one 'desc' and at most one 'title' element appear as a
			child of any particular element, and that these elements appear
			before any other child elements (except possibly 'metadata'
			elements) or character data content.
		*/

		while (child && child_count++ < 20)
		{
			HTML_Element* layouted_child = SVGUtils::GetElementToLayout(child);
			if (layouted_child->IsMatchingType(Markup::SVGE_TITLE, NS_SVG))
			{
				unsigned text_data_len = layouted_child->GetTextContentLength();
				uni_char* text_data = result.Reserve(text_data_len+1);
				if (!text_data)
				{
					return OpStatus::ERR_NO_MEMORY;
				}

				layouted_child->GetTextContent(text_data, text_data_len+1);
				// Make the text suitable for tooltip viewing
				result.Strip(TRUE, TRUE);
				// Collapse whitespace? Replace linebreaks?
				return OpStatus::OK;
			}
			child = child->Suc(); // Limit this to a fixed number of elements? Might suck lots of cpu otherwise
		}
	}

	return OpStatus::OK;
}

/*virtual */
void SVGManagerImpl::ClearTextSelection(HTML_Element* elm)
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	OP_ASSERT(elm && elm->GetNsType() == NS_SVG);
	if (SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm))
	{
		if (!doc_ctx->GetTextSelection().IsEmpty())
			doc_ctx->GetTextSelection().ClearSelection();
	}
#endif // SVG_SUPPORT_TEXTSELECTION
}

#ifdef SVG_SUPPORT_EDITABLE
static SVGEditable* GetEditableFromElement(HTML_Element* elm, SVGDocumentContext*& doc_ctx)
{
	OP_ASSERT(elm && elm->GetNsType() == NS_SVG && SVGUtils::IsTextClassType(elm->Type()));

	if(!SVGUtils::IsEditable(elm))
		return NULL;

	doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);

	if (!doc_ctx)
		return NULL;

	HTML_Element* textroot_elm = SVGUtils::GetTextRootElement(elm);

	SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(textroot_elm);
	if (!elm_ctx)
		return NULL;

	SVGTextRootContainer* text_root_cont = elm_ctx->GetAsTextRootContainer();
	OP_ASSERT(text_root_cont);

	return text_root_cont->GetEditable();
}
#endif // SVG_SUPPORT_EDITABLE

/*virtual */
void SVGManagerImpl::RemoveSelectedContent(HTML_Element* elm)
{
#ifdef SVG_SUPPORT_EDITABLE
	SVGDocumentContext* doc_ctx;
	SVGEditable* edit = GetEditableFromElement(elm, doc_ctx);
	if (!edit)
		return;

	SVGCaretPoint start_cp(doc_ctx->GetTextSelection().GetLogicalStartPoint());
	SVGCaretPoint stop_cp(doc_ctx->GetTextSelection().GetLogicalEndPoint());

	edit->RemoveContentCaret(start_cp, stop_cp);
	edit->Invalidate();

	doc_ctx->GetTextSelection().ClearSelection();
#endif // SVG_SUPPORT_EDITABLE
}

/* virtual */
void SVGManagerImpl::ShowCaret(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm)
{
#ifdef SVG_SUPPORT_EDITABLE
	SVGDocumentContext* doc_ctx;
	SVGEditable* edit = GetEditableFromElement(elm, doc_ctx);
	if (!edit)
		return;

	HTML_Element* cursor_elm = doc_ctx->GetTextSelection().GetCursor().GetElement();
	int offset = doc_ctx->GetTextSelection().GetCursor().GetElementCharacterOffset();

	if (!cursor_elm)
		return;

	/* The text selection's cursor is set when a text element is intersected.
	 * It might however happen that no text parent container of the editable is
	 * intersected and still we'd like to show the caret in the editable
	 * like for the case:
	 * <textArea x="0" y="0" width="90" height="90" editable="simple" pointer-events="boundingBox">Text</textArea>
	 * <rect x="0" y="0" width="90" height="90" stroke="black" stroke-width="1" fill="none"/>.
	 * when the rectangle element gets intersected. Default to the editable's text end in such case.
	 */
	if (!elm->IsAncestorOf(cursor_elm))
	{
		edit->m_caret.Place(SVGEditableCaret::PLACE_END);
		cursor_elm = edit->m_caret.m_point.elm;
		offset = edit->m_caret.m_point.ofs;
	}

	edit->ShowCaret(cursor_elm, offset);
#endif // SVG_SUPPORT_EDITABLE
}

/* virtual */
void SVGManagerImpl::HideCaret(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm)
{
#ifdef SVG_SUPPORT_EDITABLE
	SVGDocumentContext* doc_ctx;
	SVGEditable* edit = GetEditableFromElement(elm, doc_ctx);
	if (!edit)
		return;

	edit->HideCaret();
#endif // SVG_SUPPORT_EDITABLE
}

/* virtual */
void SVGManagerImpl::InsertTextAtCaret(HTML_Element* elm, const uni_char* text)
{
#ifdef SVG_SUPPORT_EDITABLE
	if (!text)
		return;

	SVGDocumentContext* doc_ctx;
	SVGEditable* edit = GetEditableFromElement(elm, doc_ctx);
	if (!edit)
		return;

	edit->InsertText(text, uni_strlen(text), TRUE);
	edit->Invalidate();
#endif // SVG_SUPPORT_EDITABLE
}

/* virtual */
BOOL SVGManagerImpl::IsWithinSelection(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm, const OpPoint& doc_point)
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	OP_ASSERT(elm && elm->GetNsType() == NS_SVG);
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return FALSE;

	return doc_ctx->GetTextSelection().Contains(doc, doc_point);
#else
	return FALSE;
#endif // SVG_SUPPORT_TEXTSELECTION
}

/*virtual */
BOOL SVGManagerImpl::GetTextSelectionLength(HTML_Element* elm, unsigned& length)
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	OP_ASSERT(elm && elm->GetNsType() == NS_SVG);
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return FALSE;

	if (!doc_ctx->GetTextSelection().IsEmpty())
	{
		int sel_text_len = doc_ctx->GetTextSelection().GetText(NULL);
		if (sel_text_len >= 0)
		{
			length = static_cast<unsigned>(sel_text_len);
			return TRUE;
		}
	}
	return FALSE;
#else
	return FALSE;
#endif // SVG_SUPPORT_TEXTSELECTION
}

/*virtual */
BOOL SVGManagerImpl::GetTextSelection(HTML_Element* elm, TempBuffer& buffer)
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	OP_ASSERT(elm && elm->GetNsType() == NS_SVG);
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return FALSE;

	if (!doc_ctx->GetTextSelection().IsEmpty())
		return doc_ctx->GetTextSelection().GetText(&buffer) >= 0;
	else
		return FALSE;
#else
	return FALSE;
#endif // SVG_SUPPORT_TEXTSELECTION
}

/*virtual */
void SVGManagerImpl::GetSelectionRects(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm, OpVector<OpRect>& list)
{
#ifdef SVG_SUPPORT_TEXTSELECTION
	OP_ASSERT(elm && elm->GetNsType() == NS_SVG);
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return;

	HTML_Element* textroot_elm = SVGUtils::GetTextRootElement(elm);
	SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(textroot_elm);
	if (!elm_ctx)
		return;

	SVGTextRootContainer* text_root_cont = elm_ctx->GetAsTextRootContainer();
	OP_ASSERT(text_root_cont);

	SVGMatrix matrix;
	if (OpStatus::IsError(SVGUtils::GetElementCTM(text_root_cont->GetHtmlElement(), doc_ctx, &matrix, TRUE /* ScreenCTM */)))
		return;

	HTML_Element* svg_root = doc_ctx->GetSVGRoot();
	OP_ASSERT(svg_root);
	if (!svg_root->GetLayoutBox())
		return;

	RECT rect;
	svg_root->GetLayoutBox()->GetRect(doc_ctx->GetDocument(), BOUNDING_BOX, rect);
	OpRect root_rect(&rect);

	OpAutoVector<SVGRect> svg_rects_list;
	doc_ctx->GetTextSelection().GetRects(svg_rects_list);

	VisualDevice* vis_dev = doc->GetVisualDevice();
	for(UINT32 iter = 0; iter < svg_rects_list.GetCount(); ++iter)
	{
		SVGRect* rect = svg_rects_list.Get(iter);
		*rect = matrix.ApplyToRect(*rect);
		OpRect* op_rect = OP_NEW(OpRect, (rect->GetEnclosingRect()));
		if (!op_rect)
			return;
		OpAutoPtr<OpRect> ap_op_rect(op_rect);
		op_rect->OffsetBy(root_rect.TopLeft());
		*op_rect = vis_dev->ScaleToDoc(*op_rect);
		RETURN_VOID_IF_ERROR(list.Add(op_rect));
		ap_op_rect.release();
	}
#endif // SVG_SUPPORT_TEXTSELECTION
}

/* virtual */ void
SVGManagerImpl::HandleFontDeletion(OpFont* font_about_to_be_deleted)
{
	m_glyph_cache.HandleFontDeletion(font_about_to_be_deleted);

	// Note: If it's possible to interrupt inside an <svg:text> element then
	//       we must clean the SVGFontDescriptor in SVGTextArguments here.
	//       At the time of writing we don't allow this, so we don't need to
	//       do any cleanup.
}

/* virtual */
BOOL SVGManagerImpl::IsEditableElement(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm)
{
	return SVGUtils::IsEditable(elm);
}

/* virtual */
void SVGManagerImpl::BeginEditing(SVG_DOCUMENT_CLASS* doc, HTML_Element* elm, FOCUS_REASON reason)
{
#ifdef SVG_SUPPORT_EDITABLE
	OP_ASSERT(elm && elm->GetNsType() == NS_SVG && SVGUtils::IsTextClassType(elm->Type()));

	if(!IsEditableElement(doc, elm))
		return;

	HTML_Element* textroot_elm = SVGUtils::GetTextRootElement(elm);

	SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(textroot_elm);
	if (!elm_ctx)
		return;

	SVGTextRootContainer* text_root_cont = elm_ctx->GetAsTextRootContainer();
	OP_ASSERT(text_root_cont);
	if (!text_root_cont)
		return;

	SVGEditable* edit = text_root_cont->GetEditable();
	if (!edit)
		return;

	edit->FocusEditableRoot(elm, reason);
#endif // SVG_SUPPORT_EDITABLE
}

/* virtual */
BOOL SVGManagerImpl::IsFocusableElement(SVG_DOCUMENT_CLASS *doc, HTML_Element* elm)
{
	/**
	 * Focusable=auto (should evaluate to true if one of these conditions hold):
	 *	- the a element
	 *	- the text and textArea elements with editable set to true
	 *	- elements that are the target of an animation whose begin or end criteria
	 *	  is triggered by the following user interface events: focusin, focusout, activate
	 *	- elements that are the target or observer of a listener element that is registered
	 *	  on one of the following user interface events: focusin, focusout, activate
	 *
	 *	Also we allow events: focus, blur. Those events are not in the spec, but are handled by
	 *  Mozilla.
	 *
	 * Reference: http://www.w3.org/TR/SVGMobile12/interact.html#focus
	 */
	if(!elm || elm->GetNsType() != NS_SVG || SVGUtils::IsAnimationElement(elm) || elm->IsMatchingType(Markup::SVGE_DEFS, NS_SVG))
		return FALSE;

	BOOL focusable = FALSE;
	SVGEnum* obj;

#ifdef SVG_SUPPORT_TEXTSELECTION
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	OP_ASSERT(doc_ctx);
	if(doc_ctx && (doc_ctx->GetTextSelection().IsSelecting() || !doc_ctx->GetTextSelection().IsEmpty()) && doc_ctx->GetTextSelection().GetElement() == elm)
		return TRUE;
#endif // SVG_SUPPORT_TEXTSELECTION

	if(OpStatus::IsSuccess(AttrValueStore::GetEnumObject(elm, Markup::SVGA_FOCUSABLE,
														 SVGENUM_FOCUSABLE, &obj)))
	{
		if(obj)
		{
			switch(obj->EnumValue())
			{
				case SVGFOCUSABLE_FALSE:
					// This means we are not allowed to focus this element
					return FALSE;
				case SVGFOCUSABLE_TRUE:
					focusable = TRUE;
					break;
				//case SVGFOCUSABLE_AUTO (= just continue with the rest of the processing)
			}
		}
	}

	if(!focusable)
	{
		focusable = (elm->Type() == Markup::SVGE_A);
	}

#ifdef SVG_SUPPORT_EDITABLE
	if(!focusable)
	{
		focusable = IsEditableElement(doc, elm);
	}
#endif // SVG_SUPPORT_EDITABLE

	if(!focusable)
	{
		focusable = elm->HasEventHandler(LOGDOC_DOC(doc), ONFOCUS, TRUE) ||
					elm->HasEventHandler(LOGDOC_DOC(doc), ONBLUR, TRUE) ||
					elm->HasEventHandler(LOGDOC_DOC(doc), DOMFOCUSIN, TRUE) ||
					elm->HasEventHandler(LOGDOC_DOC(doc), DOMFOCUSOUT, TRUE) ||
					elm->HasEventHandler(LOGDOC_DOC(doc), ONFOCUSIN, TRUE) ||
					elm->HasEventHandler(LOGDOC_DOC(doc), ONFOCUSOUT, TRUE) ||
					elm->HasEventHandler(LOGDOC_DOC(doc), DOMACTIVATE, TRUE) ||
					elm->HasEventHandler(LOGDOC_DOC(doc), ONCLICK, TRUE);
	}

	if(!focusable)
	{
		SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(elm);
		if (elm_ctx)
		{
			focusable =
				elm_ctx->ListensToEvent(ONFOCUS) ||
				elm_ctx->ListensToEvent(ONBLUR) ||
				elm_ctx->ListensToEvent(DOMFOCUSIN) ||
				elm_ctx->ListensToEvent(DOMFOCUSOUT) ||
				elm_ctx->ListensToEvent(ONFOCUSIN) ||
				elm_ctx->ListensToEvent(ONFOCUSOUT) ||
				elm_ctx->ListensToEvent(DOMACTIVATE) ||
				elm_ctx->ListensToEvent(ONCLICK);
		}
	}

	// FIXME: Check if visible
	return focusable;
}

/* virtual */
BOOL SVGManagerImpl::HasNavTargetInDirection(HTML_Element* current_elm, int direction, int nway,
											 HTML_Element*& preferred_next_elm)
{
	return
		SVGNavigation::FindElementInDirection(current_elm, direction, nway, preferred_next_elm) &&
		preferred_next_elm != NULL;
}

/* virtual */
OP_STATUS SVGManagerImpl::GetNavigationIterator(HTML_Element* elm, const OpRect* search_area,
												   LayoutProperties* layout_props,
												   SVGTreeIterator** nav_iterator)
{
	SVGFocusIterator* focus_iter = OP_NEW(SVGFocusIterator, ());
	if (!focus_iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = focus_iter->Init(elm, search_area);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(focus_iter);
		return status;
	}

	*nav_iterator = focus_iter;
	return status;
}

#ifdef SEARCH_MATCHES_ALL
/* virtual */
OP_STATUS SVGManagerImpl::GetHighlightUpdateIterator(HTML_Element* elm, LayoutProperties* layout_props, SelectionElm* first_hit, SVGTreeIterator** nav_iterator)
{
	SVGHighlightUpdateIterator* iter = OP_NEW(SVGHighlightUpdateIterator, ());
	if (!iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = iter->Init(elm, first_hit);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(iter);
		return status;
	}

	*nav_iterator = iter;
	return status;
}
#endif // SEARCH_MATCHES_ALL

#ifdef RESERVED_REGIONS
/* virtual */
OP_STATUS SVGManagerImpl::GetReservedRegionIterator(HTML_Element* elm, const OpRect* search_area,
	SVGTreeIterator** region_iterator)
{
	/* Scale search area to screen. */
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return OpStatus::ERR;

	VisualDevice* vis_dev = doc_ctx->GetVisualDevice();
	if (!vis_dev)
		return OpStatus::ERR;

	/* Create and initialize iterator. */
	SVGReservedRegionIterator* region_iter = OP_NEW(SVGReservedRegionIterator, ());
	if (!region_iter)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS status = region_iter->Init(elm, search_area);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(region_iter);
		return status;
	}

	*region_iterator = region_iter;
	return status;
}
#endif // RESERVED_REGIONS

/* virtual */
void SVGManagerImpl::ReleaseIterator(SVGTreeIterator* iter)
{
	OP_DELETE(iter);
}

/* virtual */
OP_STATUS SVGManagerImpl::GetNavigationData(HTML_Element* elm, OpRect& elm_box)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return OpStatus::ERR;

	VisualDevice* vd = doc_ctx->GetVisualDevice();
	if (!vd)
		return OpStatus::ERR;

	SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(elm);
	if (!elm_ctx)
		return OpStatus::ERR;

	elm_box = elm_ctx->GetScreenExtents();

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	elm_box = FROM_DEVICE_PIXEL(vd->GetVPScale(), elm_box);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	elm_box = vd->ScaleToDoc(elm_box);

	return OpStatus::OK;
}

/* virtual */
OP_STATUS SVGManagerImpl::GetElementRect(HTML_Element* elm, OpRect& elm_box)
{
	OpRect elm_rect;
	RETURN_IF_ERROR(GetNavigationData(elm, elm_rect));

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	RETURN_VALUE_IF_NULL(doc_ctx, OpStatus::ERR);

	Box* svg_box = doc_ctx->GetSVGRoot()->GetLayoutBox();
	RETURN_VALUE_IF_NULL(svg_box, OpStatus::ERR);

	RECT offsetRect = {0,0,0,0};
	SVG_DOCUMENT_CLASS* doc = doc_ctx->GetDocument();
	svg_box->GetRect(doc, CONTENT_BOX, offsetRect);
	VisualDevice* vis_dev = doc->GetVisualDevice();

	elm_rect.x += vis_dev->ScaleToDoc(offsetRect.left);
	elm_rect.y += vis_dev->ScaleToDoc(offsetRect.top);
	elm_box = elm_rect;

	return OpStatus::OK;
}

void SVGManagerImpl::ScrollToRect(OpRect doc_rect, SCROLL_ALIGN align, HTML_Element *scroll_to)
{
#ifdef SVG_SUPPORT_PANNING
	if (!scroll_to)
		return;

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(scroll_to);
	if (!doc_ctx)
		return;

	doc_ctx->FitRectToViewport(doc_rect);
#endif // SVG_SUPPORT_PANNING
}

#ifdef SVG_SUPPORT_NAVIGATION

#if defined QUICK || defined SKIN_SUPPORT
#include "modules/skin/OpSkinManager.h"
#endif // QUICK || SKIN_SUPPORT

// Blatantly copied from VisualDevice::UpdateHighlightRect
static void CalcHighlightExtents(VisualDevice* vd, OpRect& rect)
{
#define ADD_HIGHLIGHT_PADDING(vd, rect) do { \
		int padding = MAX(1, vd->ScaleToDoc(3)); \
		rect = rect.InsetBy(-padding, -padding); \
	} while (0)

	ADD_HIGHLIGHT_PADDING(vd, rect);

#ifdef SKIN_HIGHLIGHTED_ELEMENT
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Active Element Inside");
	if (skin_elm)
	{
		INT32 left, top, right, bottom;
		skin_elm->GetPadding(&left, &top, &right, &bottom, 0);
		rect.x -= left;
		rect.y -= top;
		rect.width += left + right;
		rect.height += top + bottom;
	}
#endif
}
#endif // SVG_SUPPORT_NAVIGATION

/* virtual */
void SVGManagerImpl::UpdateHighlight(VisualDevice* vd, HTML_Element* elm, BOOL paint)
{
#ifdef SVG_SUPPORT_NAVIGATION
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
	if (!doc_ctx)
		return;

	SVGElementContext* elm_ctx = AttrValueStore::GetSVGElementContext(elm);
	if (!elm_ctx)
		return;

	OpRect elm_box = elm_ctx->GetScreenExtents();
	if (elm_box.IsEmpty())
		return;

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	elm_box = FROM_DEVICE_PIXEL(vd->GetVPScale(), elm_box);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	HTML_Element* layouted_elm = SVGUtils::GetElementToLayout(elm);

	// We don't want to draw highlight on text elements
	if (layouted_elm->GetNsType() == NS_SVG && SVGUtils::IsTextClassType(layouted_elm->Type()))
		return;

	SVGFocusHighlight focus_type =
		(SVGFocusHighlight)AttrValueStore::GetEnumValue(layouted_elm, Markup::SVGA_FOCUSHIGHLIGHT,
														SVGENUM_FOCUSHIGHLIGHT,
														SVGFOCUSHIGHLIGHT_AUTO);
	if (focus_type == SVGFOCUSHIGHLIGHT_NONE)
		return;

	// FIXME: If text - use text-selection?
	// FIXME: box-offset / doc-pos
	OpRect doc_pos = doc_ctx->GetSVGImage()->GetDocumentRect();

	OpRect doc_elm_box = vd->ScaleToDoc(elm_box);
	if (paint)
	{
		// Calc extents of highlight
		OpRect hilite_extents = doc_elm_box;
		CalcHighlightExtents(vd, hilite_extents);
		hilite_extents = vd->ScaleToScreen(hilite_extents);

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
		hilite_extents = TO_DEVICE_PIXEL(vd->GetVPScale(), hilite_extents);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

		if (SVGRenderer* r = static_cast<SVGRenderer*>(m_cache.Get(SVGCache::RENDERER, doc_ctx)))
			r->GetInvalidState()->AddExtraInvalidation(hilite_extents);
	}

	doc_elm_box.OffsetBy(OpPoint(doc_pos.x - vd->GetRenderingViewX(),
								 doc_pos.y - vd->GetRenderingViewY()));

	if (paint)
	{
		vd->DrawHighlightRect(doc_elm_box);
	}
	else
	{
		vd->UpdateHighlightRect(doc_elm_box);
	}
#endif // SVG_SUPPORT_NAVIGATION
}

/* virtual */ void
SVGManagerImpl::HandleEventFinished(DOM_EventType event, HTML_Element *target)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(target);
	if (!doc_ctx)
		return;

	SVGWorkplaceImpl* wp = doc_ctx->GetSVGImage()->GetSVGWorkplace();

	if (event == SVGLOAD && wp->DecSVGLoadCounter())
	{
		SVGImage *svg_image = wp->GetFirstSVG();
		while (svg_image)
		{
			SVGImageImpl* svg_image_impl = static_cast<SVGImageImpl*>(svg_image);
			SVGDocumentContext *doc_ctx = svg_image_impl->GetSVGDocumentContext();

			if (SVGAnimationWorkplace *animation_workplace = doc_ctx->GetAnimationWorkplace())
				OpStatus::Ignore(animation_workplace->StartTimeline());

			svg_image = wp->GetNextSVG(svg_image);
		}
	}
}

/* virtual */ BOOL
SVGManagerImpl::AllowFrameForElement(HTML_Element* elm)
{
	if(!elm || elm->GetNsType() != NS_SVG)
		return FALSE;

	Markup::Type type = elm->Type();
	return SVGUtils::CanHaveExternalReference(type);
}

/* virtual */ BOOL
SVGManagerImpl::AllowInteractionWithElement(HTML_Element* elm)
{
	if(!elm || elm->GetNsType() != NS_SVG)
		return FALSE;

	Markup::Type type = elm->Type();
	return (type == Markup::SVGE_ANIMATION) || (type == Markup::SVGE_FOREIGNOBJECT);
}

/* virtual */ BOOL
SVGManagerImpl::IsEcmaScriptEnabled(SVG_DOCUMENT_CLASS* frames_document)
{
	if (LogicalDocument* logdoc = frames_document->GetLogicalDocument())
	{
		// Handle the case where something posed as an svg but really contained something else
		if(frames_document->IsInlineFrame() && frames_document->GetDocManager() && frames_document->GetDocManager()->GetFrame())
		{
			FramesDocElm* fdelm = frames_document->GetDocManager()->GetFrame();
			HTML_Element* frame_elm = fdelm->GetHtmlElement();

			if(frame_elm)
			{
				BOOL disable_script = FALSE;

				if(!disable_script)
					disable_script = frame_elm->IsMatchingType(HE_IMG, NS_HTML) || frame_elm->IsMatchingType(HE_INPUT, NS_HTML);
				if(!disable_script)
					disable_script = (frame_elm->GetInserted() == HE_INSERTED_BY_SVG &&
									  frame_elm->IsMatchingType(HE_IFRAME, NS_HTML));

				if(!disable_script)
				{
					if(SVGImageImpl* img = static_cast<SVGImageImpl*>(GetSVGImage(logdoc, logdoc->GetDocRoot())))
					{
						disable_script = img->IsParamSet("render","frozen");
					}
				}

				if(!disable_script)
				{
					FramesDocument* parent_frmdoc = fdelm->GetParentFramesDoc();
					if(LogicalDocument * parent_logdoc = parent_frmdoc->GetLogicalDocument())
					{
						SVGWorkplaceImpl* workplace = static_cast<SVGWorkplaceImpl*>(parent_logdoc->GetSVGWorkplace());
						disable_script = workplace->IsProxyElement(frame_elm);
					}
				}

				if(disable_script)
					return FALSE;

				if(FramesDocument* pdoc = frames_document->GetParentDoc())
				{
					return IsEcmaScriptEnabled(pdoc);
				}
			}
		}
	}

	return TRUE;
}

/* virtual */
const ClassAttribute*
SVGManagerImpl::GetClassAttribute(HTML_Element* elm, BOOL baseVal)
{
	SVGObject *obj;
	AttrValueStore::GetObject(elm, Markup::SVGA_CLASS, NS_IDX_SVG, FALSE, SVGOBJECT_CLASSOBJECT, &obj,
							  baseVal ? SVG_ATTRFIELD_BASE : SVG_ATTRFIELD_DEFAULT, SVGATTRIBUTE_AUTO);
	if (obj != NULL)
	{
		return static_cast<SVGClassObject*>(obj)->GetClassAttribute();
	}
	return NULL;
}

/* virtual */
OP_STATUS
SVGManagerImpl::InsertElement(HTML_Element* element)
{
	// For document roots, we check if we are an external document and
	// set flags accordingly
	SVGDocumentContext *doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!doc_ctx)
		return OpStatus::OK;

	if (doc_ctx->GetElement() == element)
	{
		if (SVG_DOCUMENT_CLASS *document = doc_ctx->GetDocument())
		{
			DocumentManager *doc_manager = document->GetDocManager();
			if (FramesDocElm *frame = doc_manager->GetFrame())
			{
				if (HTML_Element *frame_element = frame->GetHtmlElement())
				{
					if (frame_element->IsMatchingType(Markup::SVGE_ANIMATION, NS_SVG))
					{
						doc_ctx->SetIsExternalAnimation(TRUE);
					}
					else if (SVGUtils::IsExternalProxyElement(frame_element))
					{
						doc_ctx->SetIsExternalResource(TRUE);
					}
				}
#ifdef _DEBUG
				else
				{
					//OP_ASSERT(!"This may mean that the flags are invalid later on.");
				}
#endif // _DEBUG
			}

			// Checking enum to avoid having to create an animation workplace for every svg
			if (AttrValueStore::GetEnumValue(doc_ctx->GetSVGRoot(), Markup::SVGA_TIMELINEBEGIN, SVGENUM_TIMELINEBEGIN, SVGTIMELINEBEGIN_ONLOAD) == SVGTIMELINEBEGIN_ONSTART)
			{
				SVGAnimationWorkplace *animation_workplace = doc_ctx->AssertAnimationWorkplace();
				if (animation_workplace)
					animation_workplace->StartTimeline();
			}
		}
	}

#ifdef SVG_TRUST_SECURE_FLAG
	if (element->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG))
	{
		// ForeignObjects are outside of svg:s control, we can't guarantee they are "secure".
		doc_ctx->GetSVGImage()->SetSecure(FALSE);
	}
#endif // SVG_TRUST_SECURE_FLAG

	return OpStatus::OK;
}

/* virtual */
OP_STATUS
SVGManagerImpl::EndElement(HTML_Element* element)
{
	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(element);
	if (!doc_ctx)
		return OpStatus::OK; // Nothing to do for elements outside
							 // documents

	OP_ASSERT(element->GetNsType() == NS_SVG);

	if (!element->IsText())
		SVGUtils::LoadExternalReferences(doc_ctx, element);

	if (!doc_ctx->IsExternalResource())
	{
		/*
		* Build font information for font elements and for 'separate'
		* font face-elements. 'Separate' font-face means font-face
		* elements outside font elements. They are commonly used for
		* specifing external svg fonts through the font-face-src and
		* font-face-uri elements.
		*/

		HTML_Element *font_element = element;
		while (font_element != NULL &&
				!font_element->IsMatchingType(Markup::SVGE_FONT, NS_SVG) &&
				!font_element->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG))
		{
			font_element = font_element->Parent();
		}

		if (font_element && font_element->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG) && font_element->Parent()->IsMatchingType(Markup::SVGE_FONT, NS_SVG))
			font_element = NULL; // let's rebuild when we reach the font element instead

		// If we are still loading font data let's wait until it's fully downloaded,
		// we'll get HandleInlineChange when it's done
		if (font_element && !SVGUtils::IsLoadingExternalFontResources(font_element))
		{
			RETURN_IF_ERROR(SVGUtils::BuildSVGFontInfo(doc_ctx, font_element));
			SVGDynamicChangeHandler::HandleFontsChanged(doc_ctx);
		}
	}

	OP_STATUS result = SVGDynamicChangeHandler::HandleEndElement(doc_ctx, element);

#ifdef SVG_DEBUG_DEPENDENCY_GRAPH
	if(SVGDependencyGraph *dgraph = doc_ctx->GetDependencyGraph())
	{
		if(LogicalDocument* logdoc = doc_ctx->GetLogicalDocument())
		{
			if(HTML_Element* doc_root = logdoc->GetDocRoot())
			{
				dgraph->CheckConsistency(doc_root);
			}
		}
	}
#endif // SVG_DEBUG_DEPENDENCY_GRAPH

	return result;
}

#ifdef SVG_SUPPORT_ANIMATION_LISTENER
OP_STATUS
SVGManagerImpl::RegisterAnimationListener(SVGAnimationWorkplace::AnimationListener *listener)
{
	AnimationListenerItem *listener_item = OP_NEW(AnimationListenerItem, ());
	if (!listener_item)
		return OpStatus::ERR_NO_MEMORY;

	listener_item->listener = listener;
	listener_item->has_ownership = FALSE;
	listener_item->Into(&animation_listeners);
	return OpStatus::OK;
}

void
SVGManagerImpl::UnregisterAnimationListener(SVGAnimationWorkplace::AnimationListener *listener)
{
	AnimationListenerItem *iter_item = static_cast<AnimationListenerItem *>(animation_listeners.First());
	while(iter_item != NULL)
	{
		if (iter_item->listener == listener)
		{
			iter_item->Out();
			OP_DELETE(iter_item);
			return;
		}
		else
		{
			iter_item = static_cast<AnimationListenerItem *>(iter_item->Suc());
		}
	}
}

void
SVGManagerImpl::UnregisterAllAnimationListeners()
{
	AnimationListenerItem *iter_item = static_cast<AnimationListenerItem *>(animation_listeners.First());
	while(iter_item != NULL)
	{
		AnimationListenerItem *delete_item = iter_item;
		iter_item = static_cast<AnimationListenerItem *>(iter_item->Suc());

		delete_item->Out();
		if (delete_item->has_ownership)
			OP_DELETE(delete_item->listener);
		OP_DELETE(delete_item);
	}
}

void
SVGManagerImpl::ConnectAnimationListeners(SVGAnimationWorkplace *animation_workplace)
{
	AnimationListenerItem *iter_item = static_cast<AnimationListenerItem *>(animation_listeners.First());
	while(iter_item != NULL)
	{
		SVGAnimationWorkplace::AnimationListener *listener = iter_item->listener;
		if (listener->Accept(animation_workplace))
		{
			animation_workplace->RegisterAnimationListener(listener);
		}
		iter_item = static_cast<AnimationListenerItem *>(iter_item->Suc());
	}

#ifdef SVG_ANIMATION_LOG
	AnimationListenerItem *log_listener_item = OP_NEW(AnimationListenerItem, ());
	if (log_listener_item)
	{
		log_listener_item->listener = OP_NEW(SVGAnimationLogListener, ());
		log_listener_item->has_ownership = TRUE;
		log_listener_item->Into(&animation_listeners);

		if (log_listener_item->listener &&
			log_listener_item->listener->Accept(animation_workplace))
		{
			animation_workplace->RegisterAnimationListener(log_listener_item->listener);
		}
	}
#endif // SVG_ANIMATION_LOG
}
#endif // SVG_SUPPORT_ANIMATION_LISTENER

/* virtual */
OP_STATUS SVGManagerImpl::DrawString(VisualDevice* vis_dev, OpFont* font, int x, int y, const uni_char* txt, int len, int char_spacing_extra/* = 0*/)
{
	OpRect area;
	unsigned int rendering_width = MAX(0, vis_dev->ScaleToScreen(vis_dev->GetRenderingViewWidth()));
	unsigned int rendering_height = MAX(0, vis_dev->ScaleToScreen(vis_dev->GetRenderingViewHeight()));

#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	rendering_width = TO_DEVICE_PIXEL(vis_dev->GetVPScale(), rendering_width);
	rendering_height = TO_DEVICE_PIXEL(vis_dev->GetVPScale(), rendering_height);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	SVGBoundingBox bb;
	// Get the bounding box for the string so we know how large bitmap we need to include all rendered pixels
	// ceil the max values and add 1 px extra around, to make sure pixels touched by antialias is included.
	RETURN_IF_ERROR(SVGTextRenderer::SVGFontGetStringBoundingBox(&bb, font, txt, len, char_spacing_extra));
	int iminx = bb.minx.GetIntegerValue() - 1;
	int iminy = bb.miny.GetIntegerValue() - 1;
	int imaxx = bb.maxx.ceil().GetIntegerValue() + 1;
	int imaxy = bb.maxy.ceil().GetIntegerValue() + 1;
	unsigned int string_width = MAX(0, imaxx - iminx);
	unsigned int string_height = MAX(0, imaxy - iminy);

	area.width = MIN(string_width, rendering_width);
	area.height = MIN(string_height, rendering_height);

	// If the area is empty then there shouldn't be any text
	if (area.IsEmpty())
		return OpStatus::OK;

	SVGPainter painter;
	OpBitmap* bm = NULL;
	RETURN_IF_ERROR(OpBitmap::Create(&bm, area.width, area.height, FALSE, TRUE, 0, 0, TRUE));
	OpAutoPtr<OpBitmap> bm_cleaner(bm);

	VEGARenderer renderer;
	RETURN_IF_ERROR(renderer.Init(area.width, area.height));
	VEGARenderTarget* bm_rt = NULL;
	RETURN_IF_ERROR(renderer.createBitmapRenderTarget(&bm_rt, bm));

	OP_STATUS err = painter.BeginPaint(&renderer, bm_rt, area);
	if (OpStatus::IsSuccess(err))
	{
		painter.SetClipRect(area);
		painter.Clear(0, &area);

		painter.SetFlatness(SVGNumber(25) / 100);

		UINT32 color = HTM_Lex::GetColValByIndex(vis_dev->GetColor());
		color = (OP_GET_A_VALUE(color) << 24) | (OP_GET_B_VALUE(color) << 16) |
			(OP_GET_G_VALUE(color) << 8) | OP_GET_R_VALUE(color);

		SVGPaintDesc fillpaint;
		fillpaint.Reset();
		fillpaint.color = color;
		fillpaint.opacity = 255;

		painter.SetFillPaint(fillpaint);

		SVGObjectProperties obj_props;
		obj_props.aa_quality = VEGA_DEFAULT_QUALITY;
		obj_props.fillrule = SVGFILL_NON_ZERO;
		obj_props.filled = TRUE;
		obj_props.stroked = FALSE;

		painter.SetObjectProperties(&obj_props);

		int ofsx = iminx;
		int ofsy = font->Ascent() - imaxy;
		SVGMatrix transform = painter.GetTransform();
		transform.PostMultTranslation(-ofsx, -ofsy);
		painter.SetTransform(transform);

		err = SVGTextRenderer::SVGFontTxtOut(&painter, font, txt, len, char_spacing_extra);

		painter.EndPaint();

		if (OpStatus::IsSuccess(err))
		{
			renderer.setRenderTarget(NULL);

			x -= vis_dev->GetOffsetX();
			y -= vis_dev->GetOffsetY();
			x += ofsx;
			y += ofsy;

			vis_dev->BlitImage(bm, area, OpRect(x, y, area.width, area.height));
		}
	}

	VEGARenderTarget::Destroy(bm_rt);

	return err;
}

/* virtual */
OP_STATUS SVGManagerImpl::GetOpFontInfo(HTML_Element* font_element, OpFontInfo** out_fontinfo)
{
	// Note: This is very similar to the SVGUtils::BuildSVGFontInfo
	// method, should probably merge the two if possible.
	OP_ASSERT(out_fontinfo);

	if (!font_element)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPtr<OpFontInfo> fi(OP_NEW(OpFontInfo, ()));

	if (!fi.get())
		return OpStatus::ERR_NO_MEMORY;

	if (!font_element->IsMatchingType(Markup::SVGE_FONT, NS_SVG) &&
		!font_element->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG))
		return OpStatus::ERR;

	// Create a font context so that we can clean up
	SVGFontElement* font_elm_ctx = AttrValueStore::GetSVGFontElement(font_element);
	if (!font_elm_ctx)
		return OpStatus::ERR_NO_MEMORY; // This indicates OOM

	SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(font_element);
	if (!doc_ctx)
		return OpStatus::ERR;

	SVGFontTraversalObject font_traversal_object(doc_ctx, fi.get());
	RETURN_IF_ERROR(font_traversal_object.CreateResolver());

	RETURN_IF_ERROR(SVGSimpleTraverser::TraverseElement(&font_traversal_object, font_element));

	if (!font_traversal_object.HasGlyphs())
		return OpStatus::ERR;

	fi->SetFontType(OpFontInfo::SVG_WEBFONT);

	*out_fontinfo = fi.release();

	return OpStatus::OK;
}

/* virtual */
OP_STATUS SVGManagerImpl::GetOpFont(HTML_Element* font_element, UINT32 size, OpFont** out_opfont)
{
	if(!out_opfont || !font_element)
		return OpStatus::ERR_NULL_POINTER;

	if (!font_element->IsMatchingType(Markup::SVGE_FONT, NS_SVG) &&
		!font_element->IsMatchingType(Markup::SVGE_FONT_FACE, NS_SVG))
		return OpStatus::ERR;

	SVGFontElement* font_elm_ctx = AttrValueStore::GetSVGFontElement(font_element);
	if (!font_elm_ctx) // This indicates OOM
		return OpStatus::ERR_NO_MEMORY;

	SVGFontImpl* result = font_elm_ctx->CreateFontInstance(size);
	if (result)
	{
		if (OpStatus::IsSuccess(result->CreateFontContents(NULL)))
		{
			if (OpStatus::IsSuccess(RegisterSVGFont(result)))
			{
				*out_opfont = result;
				return OpStatus::OK;
			}
		}
		OP_DELETE(result);
	}
	return OpStatus::ERR_NO_MEMORY;
}

#ifdef SVG_GET_PRESENTATIONAL_ATTRIBUTES

/* virtual */ OP_STATUS
SVGManagerImpl::GetPresentationalAttributes(HTML_Element* elm, HLDocProfile* hld_profile, CSS_property_list* list)
{
	OP_ASSERT(elm);
	OP_ASSERT(elm->GetNsType() == NS_SVG);
	OP_ASSERT(list);

	TempPresentationValues tmp_presentation_values(FALSE);

	LayoutProperties lprops;
	lprops.html_element = elm;

	HTMLayoutProperties *hprops = lprops.GetProps();
	HTMLayoutProperties parent_hprops;

	hprops->Reset();
	parent_hprops.Reset();

	RETURN_IF_ERROR(hprops->SetSVGCssPropertiesFromHtmlAttr(elm, parent_hprops, hld_profile));

	Markup::Type element_type = elm->Type();

	SVGAttributeIterator iterator(elm, SVG_ATTRFIELD_BASE);

	int attr_name;
	int ns_idx;
	BOOL is_special;
	SVGObject* obj;

	while (iterator.GetNext(attr_name, ns_idx, is_special, obj))
	{
		Markup::AttrType attr_type = static_cast<Markup::AttrType>(attr_name);

		if (!SVGUtils::IsPresentationAttribute(attr_type, element_type))
			continue;

		const uni_char* property_name = SVG_Lex::GetAttrString(attr_type);
		short propname = GetCSS_Property(property_name, uni_strlen(property_name));

		// Presentational attributes should have a CSS equivalent.
		OP_ASSERT(propname >= 0);

		const unsigned MAX_PROPS = 2;

		short props[MAX_PROPS]; // ARRAY OK andersr 2011-05-06
		unsigned props_len = 0;

		// Expand shorthands, if necessary.
		switch (propname)
		{
		case CSS_PROPERTY_font:
			// Unsupported.
			break;
		case CSS_PROPERTY_overflow:
			props[0] = CSS_PROPERTY_overflow_x;
			props[1] = CSS_PROPERTY_overflow_y;
			props_len = 2;
			break;
		default:
			// Not a shorthand.
			props[0] = propname;
			props_len = 1;
			break;
		}

		for (unsigned i = 0; i < props_len; i++)
		{
			CSS_decl* decl = LayoutProperties::GetComputedDecl(elm, props[i], CSS_PSEUDO_CLASS_UNKNOWN, hld_profile, &lprops);

			if (decl)
				list->AddDecl(decl, FALSE, CSS_decl::ORIGIN_AUTHOR);
		}
	}

	return OpStatus::OK;
}
#endif // SVG_GET_PRESENTATIONAL_ATTRIBUTES

/* virtual */
void SVGManagerImpl::SwitchDocument(LogicalDocument* logdoc, HTML_Element* root)
{
	OP_ASSERT(logdoc);
	OP_ASSERT(root);
	OP_ASSERT(root->IsMatchingType(Markup::SVGE_SVG, NS_SVG));

	SVGElementContext* elm_ctx = static_cast<SVGElementContext*>(root->GetSVGContext());
	SVGDocumentContext* doc_ctx = elm_ctx ? elm_ctx->GetAsDocumentContext() : NULL;

	if (doc_ctx && doc_ctx->GetLogicalDocument() != logdoc)
		doc_ctx->GetSVGImage()->SwitchDocument(logdoc);
}

/* virtual */
OP_STATUS SVGManagerImpl::ApplyFilter(SVGURLReference ref, SVG_DOCUMENT_CLASS* doc,
									  SVGFilterInputImageGenerator* generator,
									  const OpRect& screen_targetobjectbbox,
									  OpPainter* painter, OpRect& affected_area)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

#ifdef USE_OP_CLIPBOARD
/* virtual */
void SVGManagerImpl::Paste(HTML_Element* elm, OpClipboard* clipboard)
{
#ifdef SVG_SUPPORT_EDITABLE
	SVGDocumentContext* doc_ctx;
	SVGEditable* edit = GetEditableFromElement(elm, doc_ctx);
	if (!edit)
		return;

	edit->OnPaste(clipboard);
#endif // SVG_SUPPORT_EDITABLE
}

/* virtual */
void SVGManagerImpl::Cut(HTML_Element* elm, OpClipboard* clipboard)
{
#ifdef SVG_SUPPORT_EDITABLE
	SVGDocumentContext* doc_ctx;
	SVGEditable* edit = GetEditableFromElement(elm, doc_ctx);
	if (!edit)
		return;

	edit->OnCut(clipboard);
#endif // SVG_SUPPORT_EDITABLE
}

/* virtual */
void SVGManagerImpl::Copy(HTML_Element* elm, OpClipboard* clipboard)
{
#ifdef SVG_SUPPORT_EDITABLE
	SVGDocumentContext* ignored;
	SVGEditable* edit = GetEditableFromElement(elm, ignored);
	if (edit)
		edit->OnCopy(clipboard);
	else
	{
		SVGDocumentContext* doc_ctx = AttrValueStore::GetSVGDocumentContext(elm);
		if(!doc_ctx->GetTextSelection().IsEmpty())
		{
			TempBuffer buff;
			doc_ctx->GetTextSelection().GetText(&buff);
			clipboard->PlaceText(buff.GetStorage(), doc_ctx->GetDocument()->GetWindow()->GetUrlContextId());
		}
	}
#endif // SVG_SUPPORT_EDITABLE
}

/* virtual */
void SVGManagerImpl::ClipboardOperationEnd(HTML_Element* elm)
{
#ifdef SVG_SUPPORT_EDITABLE
	SVGDocumentContext* doc_ctx;
	SVGEditable* edit = GetEditableFromElement(elm, doc_ctx);
	if (!edit)
		return;

	edit->OnEnd();
#endif // SVG_SUPPORT_EDITABLE
}
#endif // USE_OP_CLIPBOARD

#endif // SVG_SUPPORT
