/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
# include "modules/dragdrop/dragdrop_data_utils.h"
# include "modules/dragdrop/src/private_data.h"
# include "modules/pi/OpDragManager.h"
# include "modules/dragdrop/src/events_manager.h"
# include "modules/dragdrop/src/visual_feedback.h"
# include "modules/logdoc/html5parser.h"
# include "modules/logdoc/htm_elm.h"
# include "modules/logdoc/dropzone_attribute.h"
# include "modules/logdoc/selection.h"
# include "modules/doc/html_doc.h"
# include "modules/doc/frm_doc.h"
# include "modules/dochand/win.h"
# include "modules/layout/traverse/traverse.h"
# ifdef DOCUMENT_EDIT_SUPPORT
#  include "modules/documentedit/OpDocumentEdit.h"
# endif // DOCUMENT_EDIT_SUPPORT
# ifdef SVG_SUPPORT
#  include "modules/svg/SVGManager.h"
# endif // DOCUMENT_EDIT_SUPPORT
# include "modules/forms/piforms.h"
# include "modules/widgets/OpWidget.h"
# include "modules/dom/src/domhtml/htmlmicrodata.h"
# include "modules/pi/OpDragObject.h"
# include "modules/layout/content/content.h"
# include "modules/security_manager/include/security_manager.h"
# include "modules/pi/OpDragObject.h"
# include "modules/windowcommander/OpWindowCommander.h"
# include "modules/windowcommander/src/WindowCommander.h"
# include "modules/display/VisDevListeners.h"
# ifdef _KIOSK_MANAGER_
#  include "adjunct/quick/managers/KioskManager.h"
# endif // _KIOSK_MANAGER_
# ifdef DND_USE_LINKS_TEXT_INSTEAD_OF_URL
#  include "modules/logdoc/src/textdata.h"
# endif // DND_USE_LINKS_TEXT_INSTEAD_OF_URL

DragDropManager::DragDropManager()
	: m_pi_drag_manager(NULL)
	, m_is_blocked(FALSE)
	, m_is_cancelled(FALSE)
	, m_mouse_down_status(NONE)
	, m_drop_file_cb(NULL)
{
}

DragDropManager::~DragDropManager()
{
	OP_DELETE(m_pi_drag_manager);
}

OP_STATUS
DragDropManager::Initialize()
{
	RETURN_IF_ERROR(OpDragManager::Create(m_pi_drag_manager));
	return OpStatus::OK;
}

class FileDropConfirmationCallback : public OpDocumentListener::DialogCallback
{
private:
	friend class DragDropManager;
	HTML_Document* document;
	int offset_x;
	int offset_y;
	int x;
	int y;
	int visual_viewport_x;
	int visual_viewport_y;
	ShiftKeyState modifiers;
	DropType drop_type;
public:

	FileDropConfirmationCallback(HTML_Document* document, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, ShiftKeyState modifiers, DropType drop_type)
		: document(document)
		, offset_x(offset_x)
		, offset_y(offset_y)
		, x(x)
		, y(y)
		, visual_viewport_x(visual_viewport_x)
		, visual_viewport_y(visual_viewport_y)
		, modifiers(modifiers)
		, drop_type(drop_type)
	{}

	virtual void OnDialogReply(Reply reply);
};

void
DragDropManager::StartDrag()
{
	OP_ASSERT(m_pi_drag_manager || !"DragDrop Manager is not initialized");
	m_pi_drag_manager->StartDrag();
	m_is_cancelled = FALSE;
	m_mouse_down_status = NONE;
}

void
DragDropManager::StopDrag(BOOL cancelled /* = FALSE */)
{
	OP_ASSERT(m_pi_drag_manager || !"DragDrop Manager is not initialized");
	m_actions_recorder.Reset();
	m_pi_drag_manager->StopDrag(m_is_cancelled ? m_is_cancelled : cancelled);
	m_is_blocked = FALSE;
	m_is_cancelled = FALSE;
	m_mouse_down_status = NONE;
}

void
DragDropManager::StartDrag(OpDragObject* object, CoreViewDragListener* origin_listener, BOOL is_selection)
{
	m_is_blocked = FALSE;
	OP_ASSERT(object);
	SetDragObject(object);
	PrivateDragData* priv_data = OP_NEW(PrivateDragData, ());
	if (!priv_data)
	{
		StopDrag(TRUE);
		return;
	}

	priv_data->SetOriginListener(origin_listener);
	priv_data->SetIsSelection(is_selection);
	object->SetPrivateData(priv_data);
	StartDrag();
}

BOOL
DragDropManager::IsDragging()
{
	OP_ASSERT(m_pi_drag_manager || !"DragDrop Manager is not initialized");
	return m_pi_drag_manager->IsDragging();
}

void
DragDropManager::Block()
{
	m_is_blocked = TRUE;
	GetDragObject()->SetReadOnly(TRUE);
}

void
DragDropManager::Unblock()
{
	m_is_blocked = FALSE;
	GetDragObject()->SetReadOnly(FALSE);
}

BOOL
DragDropManager::IsBlocked()
{
	return m_is_blocked;
}

OpDragObject*
DragDropManager::GetDragObject()
{
	OP_ASSERT(m_pi_drag_manager || !"DragDrop Manager is not initialized");
	return m_pi_drag_manager->GetDragObject();
}

void
DragDropManager::SetDragObject(OpDragObject* drag_object)
{
	OP_ASSERT(m_pi_drag_manager || !"DragDrop Manager is not initialized");
	m_pi_drag_manager->SetDragObject(drag_object);
}

void
DragDropManager::HandleOnDragStartEvent(BOOL cancelled)
{
	OpDragObject* drag_object = GetDragObject();
	if (!cancelled && drag_object)
	{
		StartDrag();
		m_actions_recorder.SetMustReplayNextAction(TRUE);
		m_actions_recorder.ReplayRecordedDragActions();
	}
	else
		StopDrag(TRUE);
}

/**
 * Creates the d'n'd visual feedback. Finds a region that consists of the
 * dragged elements' visible parts on the paint device the document was
 * painted on. Then copies that region to a new bitmap. It sets the bitmap
 * and the bitmap's drag point in drag'n'drop data store represented by
 * OpDragObject class, instance of which is owned by OpDragManager and may
 * be got via g_drag_manager->GetDragObject().
 *
 * @param this_doc - the document in which the feedback bitmap is supposed to be shown.
 * @param elms - a list of elements (with documents the elements are in) the visual feedback is supposed to be made of.
 * @param point - current cursor's position (in document's coordinates).
 *
 * @return OpStatus::OK if all went ok, OpStatus::ERR_NO_MEMORY on OOM and OpStatus::ERR on generic error.
 *
 * @see OpDragObject
 * @see OpDragManager
 */
static OP_STATUS CreateAndSetFeedback(OpDragObject* drag_object, HTML_Document* this_doc, const OpPointerHashTable<HTML_Element, PrivateDragData::DocElm>& elms, const OpPoint& point)
{
	OP_ASSERT(drag_object);
	OpBitmap* drag_bitmap;
	OpPoint in_out_point = point;
	OpRect out_rect;
	RETURN_IF_ERROR(DragDropVisualFeedback::Create(this_doc, elms, in_out_point, out_rect, drag_bitmap));
	drag_object->SetBitmap(drag_bitmap);
	drag_object->SetBitmapPoint(in_out_point);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	OP_ASSERT(priv_data);
	priv_data->SetBitmapRect(out_rect);
	return OpStatus::OK;
}

/**
 * Creates the d'n'd visual feedback. It copies regions given by the list
 * of rectangles from the paint device the document was painted on to a new
 * bitmap. Should be used when the feedback bitmap is supposed to be made
 * out of a page's text selection. It sets the bitmap and the bitmap's drag
 * point in drag'n'drop data store represented by OpDragObject class,
 * instance of which is owned by OpDragManager and may be got via
 * g_drag_manager->GetDragObject().
 *
 * @param this_doc - the document in which the feedback bitmap is supposed to be shown.
 * @param rects - a list of rectangles the visual feedback is supposed to be made of.
 * @param union_rect - smallest rectangle containing all the rectangles in the rects list.
 * @param point - current cursor position in document coordinates.
 *
 * @return OpStatus::OK if all went ok, OpStatus::ERR_NO_MEMORY on OOM and OpStatus::ERR on generic error.
 *
 * @see OpDragObject
 * @see OpDragManager
 */
static OP_STATUS CreateAndSetFeedback(OpDragObject* drag_object, HTML_Document* this_doc, const OpVector<OpRect>* rects, const OpRect& union_rect, const OpPoint& point)
{
	OP_ASSERT(drag_object);
	OpBitmap* drag_bitmap;
	OpPoint in_out_point = point;
	OpRect in_out_rect = union_rect;
	RETURN_IF_ERROR(DragDropVisualFeedback::Create(this_doc, rects, in_out_rect, in_out_point, drag_bitmap));
	drag_object->SetBitmap(drag_bitmap);
	drag_object->SetBitmapPoint(in_out_point);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	OP_ASSERT(priv_data);
	priv_data->SetBitmapRect(in_out_rect);
	return OpStatus::OK;
}

#define DRAG_START_RAISE_OOM_AND_RETURN                        \
do                                                             \
{                                                              \
	StopDrag(TRUE);                                            \
	doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY); \
	return;                                                    \
} while (FALSE)

void
DragDropManager::OnDragStart(CoreViewDragListener* origin_drag_listener, HTML_Document* doc, int start_x, int start_y, ShiftKeyState modifiers, int current_x, int current_y)
{
	if (!doc || m_mouse_down_status == CANCELLED /* The default action of ONMOUSEDOWN has been prevented: don't start the drag */)
	{
		StopDrag(TRUE);
		return;
	}

	FramesDocument* frm_doc = doc->GetFramesDocument();
	VisualDevice* vis_dev = frm_doc->GetVisualDevice();
	BOOL element_is_draggable = FALSE;
	BOOL is_selection = FALSE;
	int view_x = vis_dev->GetRenderingViewX();
	int view_y = vis_dev->GetRenderingViewY();
	int document_x = start_x + view_x;
	int document_y = start_y + view_y;
	OpWidget* captured_widget = NULL;
	HTML_Element* helm_with_selection = NULL;
	HTML_Element* active_helm = NULL;
	HTML_Element* draggable_helm = NULL;
	int offset_x = 0, offset_y = 0;

	// A user pressed the panning keys combination so wants to pan obviously.
	// Disallow the drag then unless the panning was disallowed.
	if ((modifiers & PAN_KEYMASK) == PAN_KEYMASK && vis_dev->GetPanState() != NO)
	{
		StopDrag(TRUE);
		return;
	}

	OpPoint point(document_x, document_y);
	// Document's active element (doc->GetActiveHTMLElement()) might not be set yet if the clicked element has onmousedown handler.
	active_helm = FindIntersectingElement(doc, document_x, document_y, offset_x, offset_y);
	if (active_helm && active_helm->IsFormElement() && active_helm->GetFormObject())
		captured_widget = active_helm->GetFormObject()->GetWidget();
	helm_with_selection = captured_widget && captured_widget->GetFormObject() ? captured_widget->GetFormObject()->GetHTML_Element() : doc->GetElementWithSelection();

#ifndef HAS_NOTEXTSELECTION
	is_selection = !frm_doc->GetSelectingText() && (doc->IsWithinSelection(document_x, document_y) == OpBoolean::IS_TRUE);
#endif // HAS_NOTEXTSELECTION

	if (!is_selection && helm_with_selection)
	{
#ifdef SVG_SUPPORT
		is_selection = helm_with_selection->GetNsType() == NS_SVG && !g_svg_manager->IsInTextSelectionMode() && g_svg_manager->IsWithinSelection(frm_doc, helm_with_selection, point);
#endif // SVG_SUPPORT

		OpRect widget_rect;
		if (captured_widget)
		{
			widget_rect = captured_widget->GetDocumentRect(TRUE);
			captured_widget = captured_widget->GetWidget(OpPoint(document_x - widget_rect.x, document_y - widget_rect.y), TRUE);
		}
		is_selection = is_selection || (helm_with_selection->IsWidgetWithDraggableSelection() &&
		               captured_widget && !captured_widget->IsSelecting() &&
		               captured_widget->IsWithinSelection(document_x - widget_rect.x, document_y - widget_rect.y));
	}

#ifdef DOCUMENT_EDIT_SUPPORT
	if (frm_doc->GetDocumentEdit() && frm_doc->GetDocumentEdit()->m_layout_modifier.IsActive() && frm_doc->GetDocumentEdit()->m_layout_modifier.m_helm == active_helm)
		is_selection = FALSE;
#endif // DOCUMENT_EDIT_SUPPORT

	if (active_helm && !is_selection)
	{
		draggable_helm = active_helm->GetDraggable(frm_doc);
		if (draggable_helm)
		{
			// If the draggable element is an anchor with a text and the drag attempt was done horizontally, refuse to drag in order to
			// allow anchor's text selection (unless the link has unselectable='on').
			if (!draggable_helm->IsAnchorElement(frm_doc) || !active_helm->IsText() || draggable_helm->GetUnselectable() ||
			    current_x == start_x || op_abs(current_x - start_x) < op_abs(current_y - start_y))
				element_is_draggable = TRUE;
		}
	}

	if (element_is_draggable && !is_selection)
	{
		if (captured_widget && captured_widget->GetType() == OpTypedObject::WIDGET_TYPE_SCROLLBAR)
			element_is_draggable = FALSE;
	}

	if (!element_is_draggable && !is_selection)
	{
		StopDrag(TRUE);
		return;
	}

	TextSelection* text_selection = is_selection ? frm_doc->GetTextSelection() : NULL;
	text_selection = text_selection && !text_selection->IsEmpty() ? text_selection : NULL;
	OP_ASSERT(!is_selection || (is_selection && (text_selection || helm_with_selection)));
	// Do not allow to drag anything which was meant to be hidden from the scripts.
	if (element_is_draggable)
	{
		if (!draggable_helm->IsIncludedActual())
		{
			StopDrag(TRUE);
			return;
		}
	}
	else // is_selection == TRUE.
	{
		if (text_selection)
		{
			if (text_selection->GetStartElement()->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD ||
				text_selection->GetEndElement()->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
			{
				StopDrag(TRUE);
				return;
			}
		}
		else if (!helm_with_selection->IsIncludedActual())
		{
			StopDrag(TRUE);
			return;
		}
	}
	HTML_Element* start_elm = NULL;
	HTML_Element* end_elm  = NULL;

	OpDragObject* drag_object = NULL;
	OpTypedObject::Type type = OpTypedObject::DRAG_TYPE_HTML_ELEMENT;
	if (element_is_draggable)
	{
		if (draggable_helm->IsAnchorElement(frm_doc))
			type = OpTypedObject::DRAG_TYPE_LINK;
		else if (draggable_helm->IsMatchingType(Markup::HTE_IMG, NS_HTML))
			type = OpTypedObject::DRAG_TYPE_IMAGE;
	}

	if (OpStatus::IsMemoryError(OpDragObject::Create(drag_object, type)))
		DRAG_START_RAISE_OOM_AND_RETURN;

	// Set drag object so it's owned by OpDragManager and it will take care of releasing it.
	SetDragObject(drag_object);

	OP_ASSERT(drag_object);
	drag_object->SetSource(captured_widget);
	PrivateDragData* drag_private_data = OP_NEW(PrivateDragData, ());
	if (!drag_private_data)
		DRAG_START_RAISE_OOM_AND_RETURN;

	drag_private_data->SetOriginListener(origin_drag_listener);
	drag_private_data->SetIsSelection(is_selection);
	// Set private data so it's owned by OpDragObject and it will take care of releasing it.
	drag_object->SetPrivateData(drag_private_data);

	if (is_selection)
	{
		BOOL has_text = FALSE;
#ifndef HAS_NOTEXTSELECTION
		BOOL include_element_with_selection = FALSE;
		int selected_len = doc->GetSelectedTextLen();
		if (selected_len == 0)
		{
			include_element_with_selection = TRUE;
			selected_len = doc->GetSelectedTextLen(TRUE);
		}

		if (selected_len > 0)
		{
			uni_char* buff = OP_NEWA(uni_char, (selected_len + 1));
			if (!buff)
				DRAG_START_RAISE_OOM_AND_RETURN;
			OpAutoArray<uni_char> aa_buff(buff);
			has_text = doc->GetSelectedText(buff, selected_len + 1, include_element_with_selection);
			if (has_text && *buff)
				if (OpStatus::IsMemoryError(DragDrop_Data_Utils::SetText(drag_object, buff)))
					DRAG_START_RAISE_OOM_AND_RETURN;
		}

		if (text_selection)
		{
			start_elm = text_selection->GetStartSelectionPoint().GetElement();
			end_elm = text_selection->GetEndSelectionPoint().GetElement()->NextSiblingActual();
			drag_private_data->SetSourceHtmlElement(active_helm, doc);
		}
		else
#endif // HAS_NOTEXTSELECTION
		if (helm_with_selection)
		{
			drag_private_data->SetSourceHtmlElement(helm_with_selection, doc);
			start_elm = helm_with_selection;
			end_elm = start_elm->NextSiblingActual();
		}

		if (!has_text && helm_with_selection && helm_with_selection->IsWidgetWithDraggableSelection())
		{
			INT32 start, stop;
			captured_widget->GetSelection(start, stop);
			OpString text;
			if (OpStatus::IsMemoryError(captured_widget->GetText(text)))
				DRAG_START_RAISE_OOM_AND_RETURN;
			if (!text.IsEmpty())
			{
				uni_char* selection = text.CStr();
				selection[stop] = 0;
				if (OpStatus::IsMemoryError(DragDrop_Data_Utils::SetText(drag_object, selection + start)))
					DRAG_START_RAISE_OOM_AND_RETURN;
			}
		}
	}
	else
	{
		start_elm = draggable_helm;
		end_elm = start_elm->NextSiblingActual();
		drag_private_data->SetSourceHtmlElement(start_elm, doc);
		if (start_elm)
		{
			OpString url;
			HTML_Element* a_tag = start_elm->GetAnchorTags(frm_doc);
			if (a_tag)
			{
				MouseListenerClickInfo info;
				info.SetLinkElement(frm_doc, active_helm);
				OpString title;
				info.GetLinkTitle(title);
				if (!title.IsEmpty())
					if (OpStatus::IsMemoryError(DragDrop_Data_Utils::SetDescription(drag_object, title.CStr())))
						DRAG_START_RAISE_OOM_AND_RETURN;
#ifndef DND_USE_LINKS_TEXT_INSTEAD_OF_URL
				URL clicked_url = a_tag->GetAnchor_URL(frm_doc);
				if (!clicked_url.IsEmpty())
				{
					if (OpStatus::IsMemoryError(clicked_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, url)))
						DRAG_START_RAISE_OOM_AND_RETURN;
					if (OpStatus::IsMemoryError(DragDrop_Data_Utils::SetText(drag_object, url.CStr())))
						DRAG_START_RAISE_OOM_AND_RETURN;
				}
#else // DND_USE_LINKS_TEXT_INSTEAD_OF_URL
				TempBuffer tmp_buf;
				if (OpStatus::IsMemoryError(a_tag->AppendEntireContentAsString(&tmp_buf, TRUE, FALSE)))
					DRAG_START_RAISE_OOM_AND_RETURN;
				if (OpStatus::IsMemoryError(DragDrop_Data_Utils::SetText(drag_object, tmp_buf.GetStorage())))
					DRAG_START_RAISE_OOM_AND_RETURN;
#endif //DND_USE_LINKS_TEXT_INSTEAD_OF_URL
			}

			if (start_elm->IsMatchingType(HE_IMG, NS_HTML))
			{
				const uni_char* alt = start_elm->GetStringAttr(Markup::HA_ALT);
				if (alt && *alt)
					if (OpStatus::IsMemoryError(DragDrop_Data_Utils::SetDescription(drag_object, alt)))
						DRAG_START_RAISE_OOM_AND_RETURN;
#ifdef _KIOSK_MANAGER_
				// No dragging images in kiosk mode!
				if (KioskManager::GetInstance()->GetEnabled())
				{
					StopDrag(TRUE);
					return;
				}
#endif

				URL* src = start_elm->GetUrlAttr(Markup::HA_SRC);

				if (src && !src->IsEmpty() && g_secman_instance->IsSafeToExport(*src))
				{
					if (OpStatus::IsMemoryError(src->GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden, url)))
						DRAG_START_RAISE_OOM_AND_RETURN;
					if (OpStatus::IsMemoryError(DragDrop_Data_Utils::SetText(drag_object, url.CStr())))
						DRAG_START_RAISE_OOM_AND_RETURN;
				}
			}
		}
	}

	HTML_Element* iter = start_elm;
	TempBuffer tmp_buf;

	// Make sure we have a DOM Environment.  Drag'n'drop doesn't strictly need
	// the environment itself, but currently API for JSON-ification is exposed
	// from EcmaScript engine only.  And the number of sites using d'n'd, but
	// not javascript is expected to be small.
	if (!frm_doc->GetDOMEnvironment())
		if (OpStatus::IsMemoryError(frm_doc->ConstructDOMEnvironment()))
			DRAG_START_RAISE_OOM_AND_RETURN;
	if (frm_doc->GetDOMEnvironment())
		if (is_selection)
		{
			if (MicrodataTraverse::ExtractMicrodataToJSONFromSelection(start_elm, end_elm, frm_doc, tmp_buf) == OpStatus::OK)
				if (OpStatus::IsMemoryError(drag_object->SetData(tmp_buf.GetStorage(), UNI_L("application/microdata+json"), FALSE, TRUE)))
					DRAG_START_RAISE_OOM_AND_RETURN;
		}
		else
		{
			if (MicrodataTraverse::ExtractMicrodataToJSON(start_elm, frm_doc, tmp_buf) == OpStatus::OK)
				if (OpStatus::IsMemoryError(drag_object->SetData(tmp_buf.GetStorage(), UNI_L("application/microdata+json"), FALSE, TRUE)))
					DRAG_START_RAISE_OOM_AND_RETURN;
		}
	tmp_buf.Clear();

	OpString url;
	while (iter && iter != end_elm)
	{
		if (!iter->IsIncludedActual())
		{
			iter = iter->NextActual();
			continue;
		}

		HTML_Element* a_tag = iter->GetAnchorTags(frm_doc);

		if (a_tag)
		{
			URL clicked_url = a_tag->GetAnchor_URL(frm_doc);
			if (!clicked_url.IsEmpty())
			{
				if (OpStatus::IsMemoryError(DragDrop_Data_Utils::AddURL(drag_object, clicked_url)))
					DRAG_START_RAISE_OOM_AND_RETURN;
			}
		}
		else
		{
			if (iter->IsMatchingType(HE_IMG, NS_HTML))
			{
				URL* src = iter->GetUrlAttr(Markup::HA_SRC);

				if (src && !src->IsEmpty() && g_secman_instance->IsSafeToExport(*src))
				{
					if (OpStatus::IsMemoryError(DragDrop_Data_Utils::AddURL(drag_object, *src)))
						DRAG_START_RAISE_OOM_AND_RETURN;
				}
			}
		}

		iter = iter->NextActual();
	}

	OP_STATUS oom_status = OpStatus::OK;
	if (text_selection)
		oom_status = HTML5Parser::SerializeSelection(*text_selection, &tmp_buf, HTML5Parser::SerializeTreeOptions().SkipAttributes());
	else if (element_is_draggable) // no selection, only one element dragged.
		oom_status = HTML5Parser::SerializeTree(start_elm, &tmp_buf, HTML5Parser::SerializeTreeOptions());

	const uni_char* html = tmp_buf.GetStorage();
	if (OpStatus::IsSuccess(oom_status) && html && *html)
		oom_status = drag_object->SetData(tmp_buf.GetStorage(), HTML_DATA_FORMAT, FALSE, TRUE);

	if (OpStatus::IsMemoryError(oom_status))
		DRAG_START_RAISE_OOM_AND_RETURN;

	if (element_is_draggable)
	{
#ifndef HAS_NOTEXTSELECTION
		frm_doc->EndSelectionIfSelecting(document_x, document_y);
#endif // HAS_NOTEXTSELECTION
		OP_ASSERT(draggable_helm);
		drag_private_data->AddElement(draggable_helm, doc);
		if (OpStatus::IsMemoryError(CreateAndSetFeedback(drag_object, doc, drag_private_data->GetElementsList(), point)))
			frm_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY); // Raise OOM and continue. We may live without this.
	}
	else
	{
		OP_ASSERT(is_selection);
		if (text_selection)
		{
			CalculateSelectionRectsObject selection_rects_getter(frm_doc, &text_selection->GetStartSelectionPoint(), &text_selection->GetEndSelectionPoint());
			selection_rects_getter.Traverse(frm_doc->GetLogicalDocument()->GetRoot());

			if (selection_rects_getter.IsOutOfMemory())
				frm_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY); // Raise OOM and continue.

			const OpVector<OpRect>* rects = selection_rects_getter.GetSelectionRects();
			if (OpStatus::IsMemoryError(CreateAndSetFeedback(drag_object, doc, rects, selection_rects_getter.GetUnionRect(), point)))
				frm_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY); // Raise OOM and continue. We may live without the (full) bitmap.
		}
		else
		{
			OP_ASSERT(doc->GetElementWithSelection());
#ifdef SVG_SUPPORT
			if (doc->GetElementWithSelection()->GetNsType() == NS_SVG)
			{
				OpAutoVector<OpRect> rects;
				OpRect union_rect;
				g_svg_manager->GetSelectionRects(frm_doc, doc->GetElementWithSelection(), rects);
				for (UINT32 iter = 0; iter < rects.GetCount(); ++iter)
					union_rect.UnionWith(*rects.Get(iter));
				if (OpStatus::IsMemoryError(CreateAndSetFeedback(drag_object, doc, &rects, union_rect, point)))
					frm_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY); // Raise OOM and continue. We may live without the (full) bitmap.
			}
			else
#endif // SVG_SUPPORT
			{
				OpAutoVector<OpRect> rects;
				OpRect union_rect;
				if (OpStatus::IsMemoryError(captured_widget->GetSelectionRects(&rects, union_rect, TRUE)))
					frm_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY); // Raise OOM and continue. We may live without the (full) bitmap.
				// Rectangles are in widget coordinates. Translate them to document coordinates.
				OpRect widget_rect = captured_widget->GetDocumentRect(TRUE);
				union_rect.x += widget_rect.x;
				union_rect.y += widget_rect.y;
				for (UINT32 iter = 0; iter < rects.GetCount(); ++iter)
				{
					OpRect* rect = rects.Get(iter);
					rect->x += widget_rect.x;
					rect->y += widget_rect.y;
				}
				if (OpStatus::IsMemoryError(CreateAndSetFeedback(drag_object, doc, &rects, union_rect, point)))
					frm_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY); // Raise OOM and continue. We may live without the (full) bitmap.
			}
		}
	}

	frm_doc->GetWindow()->DisplayLinkInformation(NULL, ST_ASTRING, NULL); // Removes an existing status bar or tooltip.

	// Update the data.
	drag_private_data->m_target_doc = doc;
	SetDragObject(drag_object);
	m_actions_recorder.Reset();

	if (m_mouse_down_status > NONE && m_mouse_down_status < CANCELLED)
	{
		// The result of ONMOUSEDOWN is not known yet. Force ONDRAGSTART postponing.
		Block();
		m_actions_recorder.OnDragActionHandle();
	}
	else
	{
		OP_ASSERT(m_mouse_down_status == NONE || m_mouse_down_status == ALLOWED);
		m_mouse_down_status = NONE;
	}

	DragAction(doc, ONDRAGSTART, document_x, document_y, frm_doc->GetVisualViewX(), frm_doc->GetVisualViewY(), offset_x, offset_y, (modifiers & SHIFTKEY_SHIFT) != 0, (modifiers & SHIFTKEY_CTRL) != 0, (modifiers & SHIFTKEY_ALT) != 0);
	// Check if we're still dragging since DragAction could have cancelled the drag.
	if (IsDragging())
	{
		drag_private_data->m_timer.Start(PrivateDragData::DRAG_SEQUENCE_PERIOD);
		drag_private_data->m_last_known_drag_point.x = current_x;
		drag_private_data->m_last_known_drag_point.y = current_y;
	}
}

#undef DRAG_START_RAISE_OOM_AND_RETURN

void
DragDropManager::OnDragEnter()
{
	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data)
	{
		priv_data = OP_NEW(PrivateDragData, ());
		// In case of OOM NULL will be set as the private data. This is OK since in all places the private data is used it's checked against NULL.
		drag_object->SetPrivateData(priv_data);
	}
}

CoreViewDragListener*
DragDropManager::GetOriginDragListener()
{
	OpDragObject* drag_object = GetDragObject();
	if (drag_object)
	{
		PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
		if (!priv_data)
			return NULL;

		return priv_data->GetOriginListener();
	}

	return NULL;
}

void
DragDropManager::SetOriginDragListener(CoreViewDragListener* origin_listener)
{
	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data && origin_listener)
	{
		OnDragEnter();
		priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	}

	if (priv_data)
		priv_data->SetOriginListener(origin_listener);
}

void
DragDropManager::OnDragMove(HTML_Document* doc, int x, int y, ShiftKeyState modifiers, BOOL force /* = FALSE */)
{
	if (!IsDragging())
		return;

	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);

	if (IsBlocked())
	{
		drag_object->SetDropType(DROP_NONE);
		drag_object->SetVisualDropType(DROP_NONE);
	}

	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data)
	{
		drag_object->SetDropType(DROP_NONE);
		drag_object->SetVisualDropType(DROP_NONE);
		return;
	}
	priv_data->m_timer.Stop();
	if (!force && priv_data->m_last_known_drag_point.x == x && priv_data->m_last_known_drag_point.y == y)
	{
		priv_data->m_timer.Start(PrivateDragData::DRAG_SEQUENCE_PERIOD);
		return;
	}

	priv_data->m_last_known_drag_point.x = x;
	priv_data->m_last_known_drag_point.y = y;

	priv_data->m_target_doc = doc;

	FramesDocument* frm_doc = NULL;
	int document_x = x;
	int document_y = y;
	int view_x = 0;
	int view_y = 0;
	if (!doc)
	{
		drag_object->SetDropType(DROP_NONE);
		drag_object->SetVisualDropType(DROP_NONE);
	}
	else
	{
		VisualDevice* vis_dev = doc->GetVisualDevice();
		view_x = vis_dev->GetRenderingViewX();
		view_y = vis_dev->GetRenderingViewY();
		document_x += view_x;
		document_y += view_y;
		frm_doc = doc->GetFramesDocument();
	}

	HTML_Element* dnd_source_element = priv_data->GetSourceHtmlElement();
	HTML_Document* src_doc = priv_data->GetElementsDocument(dnd_source_element);
	int offset_x = 0, offset_y = 0;
	int visual_vp_x = frm_doc ? frm_doc->GetVisualViewX() : 0;
	int visual_vp_y = frm_doc ? frm_doc->GetVisualViewY() : 0;
	if (src_doc)
		DragAction(src_doc, ONDRAG, document_x, document_y, visual_vp_x, visual_vp_y, offset_x, offset_y, (modifiers & SHIFTKEY_SHIFT) != 0, (modifiers & SHIFTKEY_CTRL) != 0, (modifiers & SHIFTKEY_ALT) != 0);
	else if (doc)
	{
		HTML_Element* current_immediate_selection = FindIntersectingElement(doc, document_x, document_y, offset_x, offset_y);
		doc->SetPreviousImmediateSelectionElement(doc->GetImmediateSelectionElement());
		doc->SetImmediateSelectionElement(current_immediate_selection);
		if (current_immediate_selection && current_immediate_selection != doc->GetPreviousImmediateSelectionElement() &&
			current_immediate_selection != doc->GetCurrentTargetElement())
			DragAction(doc, ONDRAGENTER, document_x, document_y, visual_vp_x, visual_vp_y, offset_x, offset_y, (modifiers & SHIFTKEY_SHIFT) != 0, (modifiers & SHIFTKEY_CTRL) != 0, (modifiers & SHIFTKEY_ALT) != 0);
		else if (doc->GetCurrentTargetElement())
			DragAction(doc, ONDRAGOVER, document_x, document_y, visual_vp_x, visual_vp_y, offset_x, offset_y, (modifiers & SHIFTKEY_SHIFT) != 0, (modifiers & SHIFTKEY_CTRL) != 0, (modifiers & SHIFTKEY_ALT) != 0);
	}

	priv_data->m_scroller.SetLastMovePos(x, y);
	if (!priv_data->m_scroller.IsStarted())
		priv_data->m_scroller.Start();

	priv_data->m_timer.Start(PrivateDragData::DRAG_SEQUENCE_PERIOD);
}

void
DragDropManager::OnDragLeave(ShiftKeyState modifiers)
{
	if (!IsDragging())
		return;

	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	// The target we'll enter should change this.
	drag_object->SetDropType(DROP_NONE);
	drag_object->SetVisualDropType(DROP_NONE);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data)
		return;
	priv_data->m_timer.Stop();
	priv_data->m_scroller.Stop();
	HTML_Document* target_doc = priv_data->GetTargetDocument();

	if (!target_doc)
		return;

	target_doc->GetWindow()->SetCursor(CURSOR_NO_DROP);

	VisualDevice* vis_dev = target_doc->GetVisualDevice();
	int document_x = priv_data->m_last_known_drag_point.x + vis_dev->GetRenderingViewX();
	int document_y = priv_data->m_last_known_drag_point.y + vis_dev->GetRenderingViewY();
	int offset_x = 0, offset_y = 0;
	FramesDocument* frm_doc = target_doc->GetFramesDocument();
	if (DragAction(target_doc, ONDRAGLEAVE, document_x, document_y, frm_doc->GetVisualViewX(), frm_doc->GetVisualViewY(), offset_x, offset_y, (modifiers & SHIFTKEY_SHIFT) != 0, (modifiers & SHIFTKEY_CTRL) != 0, (modifiers & SHIFTKEY_ALT) != 0))
	{
		// The target we'll enter should change this.
		target_doc->SetCurrentTargetElement(NULL);
		target_doc->SetImmediateSelectionElement(NULL);
		target_doc->SetPreviousImmediateSelectionElement(NULL);
		priv_data->m_target_doc = NULL;
	}
}

void
DragDropManager::OnDragCancel(int x, int y, ShiftKeyState modifiers)
{
	if (!IsDragging())
		return;

	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	drag_object->SetDropType(DROP_NONE);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data)
	{
		StopDrag(TRUE);
		return;
	}
	priv_data->m_timer.Stop();
	priv_data->m_scroller.Stop();
	HTML_Document* target_doc = priv_data->GetTargetDocument();

	int document_x = x;
	int document_y = y;
	int offset_x = 0, offset_y = 0;
	int visual_vp_x = 0, visual_vp_y = 0;
	if (target_doc)
	{
		target_doc->GetWindow()->UseDefaultCursor();
		VisualDevice* vis_dev = target_doc->GetVisualDevice();
		document_x += vis_dev->GetRenderingViewX();
		document_y += vis_dev->GetRenderingViewY();
		m_actions_recorder.CancelPending();
		GetElementPaddingOffset(target_doc->GetCurrentTargetElement(), target_doc, document_x, document_y, offset_x, offset_y);
		FramesDocument* frm_doc = target_doc->GetFramesDocument();
		visual_vp_x = frm_doc->GetVisualViewX();
		visual_vp_y = frm_doc->GetVisualViewY();
		if (DragAction(target_doc, ONDRAGLEAVE, document_x, document_y, visual_vp_x, visual_vp_y, offset_x, offset_y, (modifiers & SHIFTKEY_SHIFT) != 0, (modifiers & SHIFTKEY_CTRL) != 0, (modifiers & SHIFTKEY_ALT) != 0))
		{
			target_doc->SetCurrentTargetElement(NULL);
			target_doc->SetImmediateSelectionElement(NULL);
			target_doc->SetPreviousImmediateSelectionElement(NULL);
			priv_data->m_target_doc = NULL;
		}

	}

	HTML_Element* dnd_source_element = priv_data->GetSourceHtmlElement();
	HTML_Document* src_doc = priv_data->GetElementsDocument(dnd_source_element);
	if (src_doc)
	{
		// Remember that the drag was cancelled so a proper notification is sent to a platform.
		m_is_cancelled = TRUE;
		DragAction(src_doc, ONDRAGEND, document_x, document_y, visual_vp_x, visual_vp_y, offset_x, offset_y, (modifiers & SHIFTKEY_SHIFT) != 0, (modifiers & SHIFTKEY_CTRL) != 0, (modifiers & SHIFTKEY_ALT) != 0);
	}
	else // We couldn't send ONDRAGEND which would end and clean up d'n'd so we have to do it here.
		StopDrag(TRUE);
}

void
DragDropManager::OnDragDrop(int x, int y, ShiftKeyState modifiers)
{
	if (!IsDragging())
		return;

	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);

	if (IsBlocked())
	{
		drag_object->SetDropType(DROP_NONE);
		drag_object->SetVisualDropType(DROP_NONE);
	}

	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data)
	{
		drag_object->SetDropType(DROP_NONE);
		StopDrag();
		return;
	}
	priv_data->m_timer.Stop();
	priv_data->m_scroller.Stop();
	priv_data->m_last_known_drag_point.x = x;
	priv_data->m_last_known_drag_point.y = y;
	HTML_Document* target_doc = priv_data->GetTargetDocument();

	if (!target_doc)
	{
		drag_object->SetDropType(DROP_NONE);
		StopDrag();
		return;
	}

	target_doc->GetWindow()->UseDefaultCursor();
	VisualDevice* vis_dev = target_doc->GetVisualDevice();
	int view_x = vis_dev->GetRenderingViewX();
	int view_y = vis_dev->GetRenderingViewY();
	int document_x = x + view_x;
	int document_y = y + view_y;
	int offset_x = 0, offset_y = 0;
	GetElementPaddingOffset(target_doc->GetCurrentTargetElement(), target_doc, document_x, document_y, offset_x, offset_y);
	FramesDocument* frm_doc = target_doc->GetFramesDocument();
	DragAction(target_doc, ONDROP, document_x, document_y, frm_doc->GetVisualViewX(), frm_doc->GetVisualViewY(), offset_x, offset_y, (modifiers & SHIFTKEY_SHIFT) != 0, (modifiers & SHIFTKEY_CTRL) != 0, (modifiers & SHIFTKEY_ALT) != 0);
}

void
DragDropManager::OnDragEnd(int x, int y, ShiftKeyState modifiers)
{
	if (!IsDragging())
		return;

	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	OP_ASSERT(priv_data);
	if (!priv_data)
	{
		drag_object->SetDropType(DROP_NONE);
		StopDrag();
		return;
	}
	priv_data->m_timer.Stop();
	priv_data->m_scroller.Stop();
	priv_data->m_last_known_drag_point.x = -1;
	priv_data->m_last_known_drag_point.y = -1;
	HTML_Document* target_doc = priv_data->GetTargetDocument();

	int document_x = x;
	int document_y = y;
	int offset_x = 0, offset_y = 0;
	int view_x = 0, view_y = 0, visual_view_x = 0, visual_view_y = 0;
	if (target_doc)
	{
		FramesDocument* target_frm_doc = target_doc->GetFramesDocument();
		target_doc->GetWindow()->UseDefaultCursor();
		VisualDevice* vis_dev =  target_doc->GetVisualDevice();
		view_x = vis_dev->GetRenderingViewX();
		view_y = vis_dev->GetRenderingViewY();
		document_x += view_x;
		document_y += view_y;
		visual_view_x = target_frm_doc->GetVisualViewX();
		visual_view_y = target_frm_doc->GetVisualViewY();
	}
	HTML_Element* dnd_source_element = priv_data->GetSourceHtmlElement();
	HTML_Document* src_doc = priv_data->GetElementsDocument(dnd_source_element);

	if (src_doc)
		DragAction(src_doc, ONDRAGEND, document_x, document_y, visual_view_x, visual_view_y, offset_x, offset_y, (modifiers & SHIFTKEY_SHIFT) != 0, (modifiers & SHIFTKEY_CTRL) != 0, (modifiers & SHIFTKEY_ALT) != 0);
	else // We couldn't send ONDRAGEND which would end and clean up d'n'd so we have to do it here.
		StopDrag();
}

void
DragDropManager::HandleOnDragEvent(HTML_Document* doc, int document_x, int document_y, int offset_x, int offset_y, ShiftKeyState modifiers, BOOL cancelled)
{
	OP_ASSERT(GetDragObject());
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(GetDragObject()->GetPrivateData());
	OP_ASSERT(priv_data);
	if (cancelled)
	{
		priv_data->m_timer.Stop();
		m_actions_recorder.Reset();
		if (doc)
		{
			FramesDocument* frames_doc = doc->GetFramesDocument();
			VisualDevice* vis_dev =  frames_doc->GetVisualDevice();
			document_x -= vis_dev->GetRenderingViewX();
			document_y -= vis_dev->GetRenderingViewY();
		}
		OnDragCancel(document_x, document_y, modifiers);
		return;
	}

	HTML_Document* target = priv_data->m_target_doc;
	if (target)
	{
		int offset_x = 0, offset_y = 0;
		HTML_Element* immediate_selection_elm = FindIntersectingElement(target, document_x, document_y, offset_x, offset_y);
		target->SetPreviousImmediateSelectionElement(target->GetImmediateSelectionElement());
		target->SetImmediateSelectionElement(immediate_selection_elm);
		HTML_Element* dnd_target_element = target->GetCurrentTargetElement();
		if (immediate_selection_elm &&
			immediate_selection_elm != target->GetPreviousImmediateSelectionElement() &&
			immediate_selection_elm != dnd_target_element)
		{
			SetProperDropType(ONDRAGENTER);
			HandleDragEvent(target,
							ONDRAGENTER,
							immediate_selection_elm,
							document_x, document_y,
							offset_x, offset_y,
							modifiers);
		}
		else if (dnd_target_element)
		{
			SetProperDropType(ONDRAGOVER);
			HandleDragEvent(target,
							ONDRAGOVER,
							dnd_target_element,
							document_x, document_y,
							offset_x, offset_y,
							modifiers);
		}
		else
		{
			m_actions_recorder.AllowNestedReplay();
			m_actions_recorder.SetMustReplayNextAction(TRUE);
		}
	}
	else
	{
		m_actions_recorder.AllowNestedReplay();
		m_actions_recorder.SetMustReplayNextAction(TRUE);
	}

	if (m_actions_recorder.GetMustReplayNextAction())
		m_actions_recorder.ReplayRecordedDragActions();
}

static DropzoneAttribute* GetDropZoneAttribute(HTML_Element* element)
{
	OP_ASSERT(element);
	DropzoneAttribute* dropzone_attr = NULL;
	HTML_Element* iter = element;
	do
	{
		NS_Type ns = iter->GetNsType();
		if (ns == NS_HTML)
			dropzone_attr = static_cast<DropzoneAttribute*>(iter->GetAttr(Markup::HA_DROPZONE, ITEM_TYPE_COMPLEX, NULL));
#ifdef SVG_SUPPORT
		else if (ns == NS_SVG)
			dropzone_attr = static_cast<DropzoneAttribute*>(iter->GetAttr(Markup::SVGA_DROPZONE, ITEM_TYPE_COMPLEX, NULL, NS_IDX_SVG));
#endif // SVG_SUPPORT
		iter = iter->ParentActual();
	}
	while(iter && !dropzone_attr);

	return dropzone_attr;
}

static void SetDropTarget(HTML_Document* doc, HTML_Element* this_elm, OpDragObject* drag_object, int offset_x, int offset_y)
{
	doc->SetCurrentTargetElement(this_elm);
	if (this_elm->IsWidgetAcceptingDrop())
		if (FormObject* form_object = this_elm->GetFormObject())
			form_object->GetWidget()->GenerateOnDragMove(drag_object, OpPoint(offset_x, offset_y));
}

static BOOL HasAcceptableData(OpDragObject* object, HTML_Element* elm, FramesDocument* doc, BOOL& is_docedit)
{
	is_docedit = FALSE;
#ifdef DOCUMENT_EDIT_SUPPORT
	is_docedit = elm->IsContentEditable(TRUE) || doc->GetDesignMode();
#endif // DOCUMENT_EDIT_SUPPORT
	return DragDrop_Data_Utils::HasData(object, TEXT_DATA_FORMAT) ||
	       (is_docedit && DragDrop_Data_Utils::HasData(object, HTML_DATA_FORMAT));
}

void
DragDropManager::HandleOnDragEnterEvent(HTML_Element* this_elm, HTML_Document* doc, int document_x, int document_y, int offset_x, int offset_y, ShiftKeyState modifiers, BOOL cancelled)
{
	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	OP_ASSERT(priv_data);
	if (doc)
	{
		OP_ASSERT(this_elm);
		FramesDocument* frames_doc = doc->GetFramesDocument();
		HTML_Element* old_dnd_target_element = doc->GetCurrentTargetElement();
		if (cancelled)
			SetDropTarget(doc, this_elm, drag_object, offset_x, offset_y);
		else
		{
			BOOL handled = FALSE;
			DropzoneAttribute* dropzone_attr = GetDropZoneAttribute(this_elm);
			BOOL is_docedit = FALSE;
			if (HasAcceptableData(drag_object, this_elm, frames_doc, is_docedit))
			{
#ifdef DOCUMENT_EDIT_SUPPORT
				if (is_docedit)
				{
					handled = TRUE;
					doc->SetCurrentTargetElement(this_elm);
					OpDocumentEdit* doc_edit = frames_doc->GetDocumentEdit();
					if (doc_edit && frames_doc->GetCaretPainter()) // CaretPainter can be NULL in OOM situations.
					{
						frames_doc->GetCaretPainter()->DragCaret(TRUE);
						doc_edit->m_caret.Place(document_x, document_y, FALSE, FALSE, TRUE);
					}
				}
				else
#endif // DOCUMENT_EDIT_SUPPORT
				{
#ifdef SVG_SUPPORT
					if (this_elm->GetNsType() == NS_SVG && g_svg_manager->IsEditableElement(frames_doc, this_elm))
					{
						handled = TRUE;
						doc->SetCurrentTargetElement(this_elm);
						g_svg_manager->ShowCaret(frames_doc, this_elm);
					}
					else
#endif // SVG_SUPPORT
					{
						if (this_elm->IsWidgetAcceptingDrop())
						{
							handled = TRUE;
							doc->SetCurrentTargetElement(this_elm);
							if (FormObject* form_object = this_elm->GetFormObject())
								form_object->GetWidget()->GenerateOnDragMove(drag_object, OpPoint(offset_x, offset_y));
						}
					}
				}
			}

			if (!handled && dropzone_attr)
			{
				const OpVector<DropzoneAttribute::AcceptedData>& dropzone_data = dropzone_attr->GetAcceptedData();
				for (UINT32 iter = 0; iter < dropzone_data.GetCount(); ++iter)
				{
					if ((dropzone_data.Get(iter)->m_data_kind == DropzoneAttribute::AcceptedData::DATA_KIND_FILE &&
						DragDrop_Data_Utils::HasData(drag_object, dropzone_data.Get(iter)->m_data_type.CStr(), TRUE)) ||
						(dropzone_data.Get(iter)->m_data_kind == DropzoneAttribute::AcceptedData::DATA_KIND_STRING &&
						DragDrop_Data_Utils::HasData(drag_object, dropzone_data.Get(iter)->m_data_type.CStr())))
					{
						doc->SetCurrentTargetElement(this_elm);
						handled = TRUE;
						break;
					}
				}
			}

			// This is not the speced part but it's needed for site compat.
			// If an element or any of its parents has ondragover handler it should be
			// set as the target. Both Chrome and Firefox behave like this.
			if (!handled)
			{
				handled = this_elm->HasEventHandler(frames_doc, ONDRAGOVER, FALSE);
				if (handled)
					SetDropTarget(doc, this_elm, drag_object, offset_x, offset_y);
			}

			if (!handled && doc->GetCurrentTargetElement() != frames_doc->GetLogicalDocument()->GetBodyElm())
			{
				doc->SetCurrentTargetElement(frames_doc->GetLogicalDocument()->GetBodyElm());
				SetProperDropType(ONDRAGENTER);
				priv_data->m_is_body_onenter_special = TRUE;
				HandleDragEvent(doc,
								ONDRAGENTER,
								doc->GetCurrentTargetElement(),
								document_x, document_y,
								offset_x, offset_y,
								modifiers);
			}
		}

		if (old_dnd_target_element && old_dnd_target_element != doc->GetCurrentTargetElement())
		{
			SetProperDropType(ONDRAGLEAVE);
			HandleDragEvent(doc,
							ONDRAGLEAVE,
							old_dnd_target_element,
							document_x, document_y,
							offset_x, offset_y,
							modifiers);
		}

		if (!priv_data->m_is_body_onenter_special)
		{
			if (doc->GetCurrentTargetElement())
			{
				SetProperDropType(ONDRAGOVER);
				HandleDragEvent(doc,
								ONDRAGOVER,
								doc->GetCurrentTargetElement(),
								document_x, document_y,
								offset_x, offset_y,
								modifiers);
			}
			else
			{
				m_actions_recorder.AllowNestedReplay();
				m_actions_recorder.SetMustReplayNextAction(TRUE);
			}
		}

		priv_data->m_is_body_onenter_special = FALSE;
	}
	else
	{
		m_actions_recorder.AllowNestedReplay();
		m_actions_recorder.SetMustReplayNextAction(TRUE);
	}

	if (m_actions_recorder.GetMustReplayNextAction())
		m_actions_recorder.ReplayRecordedDragActions();
}

void
DragDropManager::HandleOnDragOverEvent(HTML_Document* doc, int document_x, int document_y, int offset_x, int offset_y, BOOL cancelled)
{
	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);

	if (doc)
	{
		FramesDocument* frames_doc = doc->GetFramesDocument();

#ifdef DOCUMENT_EDIT_SUPPORT
		OpDocumentEdit* doc_edit = frames_doc->GetDocumentEdit();
#endif // DOCUMENT_EDIT_SUPPORT

		HTML_Element* dnd_current_target = doc->GetCurrentTargetElement();
		if (dnd_current_target)
		{
			if (cancelled)
			{
				unsigned effects_allowed = drag_object->GetEffectsAllowed();
				DropType drop = drag_object->GetDropType();
				if (effects_allowed != DROP_UNINITIALIZED &&
					((effects_allowed & DROP_COPY) == 0 || drop != DROP_COPY) &&
					((effects_allowed & DROP_LINK) == 0 || drop != DROP_LINK) &&
					((effects_allowed & DROP_MOVE) == 0 || drop != DROP_MOVE))
						drag_object->SetDropType(DROP_NONE);

#ifdef DOCUMENT_EDIT_SUPPORT
				if (dnd_current_target->IsContentEditable(TRUE) || frames_doc->GetDesignMode())
				{
					if (frames_doc->GetCaretPainter() && doc_edit)
					{
						frames_doc->GetCaretPainter()->DragCaret(TRUE);
						doc_edit->m_caret.Place(document_x, document_y, FALSE, FALSE, TRUE);
					}
				}
				else
#endif // DOCUMENT_EDIT_SUPPORT
				{
#ifdef SVG_SUPPORT
					if (dnd_current_target->GetNsType() == NS_SVG && g_svg_manager->IsEditableElement(frames_doc, dnd_current_target))
						g_svg_manager->ShowCaret(frames_doc, dnd_current_target);
					else
#endif // SVG_SUPPORT
					if (dnd_current_target->IsWidgetAcceptingDrop())
					{
						if (FormObject* form_object = dnd_current_target->GetFormObject())
							form_object->GetWidget()->GenerateOnDragMove(drag_object, OpPoint(offset_x, offset_y));
					}
				}
			}
			else
			{
				BOOL handled = FALSE;
				DropzoneAttribute* dropzone_attr = GetDropZoneAttribute(dnd_current_target);
				BOOL is_docedit = FALSE;
				if (HasAcceptableData(drag_object, dnd_current_target, frames_doc, is_docedit))
				{
					if (dnd_current_target->IsWidgetAcceptingDrop())
					{
						handled = TRUE;
						if (drag_object->GetDropType() == DROP_NONE)
							drag_object->SetDropType(DROP_COPY);
						if (FormObject* form_object = dnd_current_target->GetFormObject())
							form_object->GetWidget()->GenerateOnDragMove(drag_object, OpPoint(offset_x, offset_y));
					}
#ifdef DOCUMENT_EDIT_SUPPORT
					else if (is_docedit)
					{
						handled = TRUE;
						OpPoint caret_point;
						CaretPainter* caret_painter = frames_doc->GetCaretPainter();
						if (caret_painter && doc_edit)
						{
							caret_painter->DragCaret(TRUE);
							doc_edit->m_caret.Place(document_x, document_y, FALSE, FALSE, TRUE);
							caret_point = caret_painter->GetCaretPosInDocument();
						}
						if (drag_object->GetDropType() == DROP_NONE)
							drag_object->SetDropType(DROP_COPY);
						else if (drag_object->GetDropType() == DROP_MOVE)
						{
							OP_BOOLEAN is_within_selection = doc->IsWithinSelection(caret_point.x, caret_point.y);
							if (is_within_selection == OpBoolean::IS_TRUE)
							{
								drag_object->SetDropType(DROP_NONE);
								if (caret_painter)
									caret_painter->DragCaret(FALSE);
							}
						}
					}
#endif // DOCUMENT_EDIT_SUPPORT
					else
					{
#ifdef SVG_SUPPORT
						if (dnd_current_target->GetNsType() == NS_SVG && g_svg_manager->IsEditableElement(frames_doc, dnd_current_target))
						{
							handled = TRUE;
							if (drag_object->GetDropType() == DROP_NONE)
								drag_object->SetDropType(DROP_COPY);
							g_svg_manager->ShowCaret(frames_doc, dnd_current_target);
						}
#endif // SVG_SUPPORT
					}
				}

				if (!handled && dropzone_attr)
				{
					DropType type = ((dropzone_attr->GetOperation() == DropzoneAttribute::OPERATION_COPY) ?
									DROP_COPY : ((dropzone_attr->GetOperation() == DropzoneAttribute::OPERATION_MOVE) ? DROP_MOVE : DROP_LINK));
					const OpVector<DropzoneAttribute::AcceptedData>& dropzone_data = dropzone_attr->GetAcceptedData();
					for (UINT32 iter = 0; iter < dropzone_data.GetCount(); ++iter)
					{
						if ((dropzone_data.Get(iter)->m_data_kind == DropzoneAttribute::AcceptedData::DATA_KIND_FILE &&
							DragDrop_Data_Utils::HasData(drag_object, dropzone_data.Get(iter)->m_data_type.CStr(), TRUE)) ||
							(dropzone_data.Get(iter)->m_data_kind == DropzoneAttribute::AcceptedData::DATA_KIND_STRING &&
							DragDrop_Data_Utils::HasData(drag_object, dropzone_data.Get(iter)->m_data_type.CStr())))
						{
							handled = TRUE;
							drag_object->SetDropType(type);
							break;
						}
					}
				}

				if (!handled)
					drag_object->SetDropType(DROP_NONE);
			}
		}
		else
			drag_object->SetDropType(DROP_NONE);

		DropType drop = drag_object->GetDropType();
		drag_object->SetVisualDropType(drop);
		switch (drop)
		{
			case DROP_NONE:
				frames_doc->GetWindow()->SetCursor(CURSOR_NO_DROP, TRUE);
				break;
			case DROP_LINK:
				frames_doc->GetWindow()->SetCursor(CURSOR_DROP_LINK, TRUE);
				break;
			case DROP_COPY:
				frames_doc->GetWindow()->SetCursor(CURSOR_DROP_COPY, TRUE);
				break;
			case DROP_MOVE:
				frames_doc->GetWindow()->SetCursor(CURSOR_DROP_MOVE, TRUE);
				break;
			default:
				frames_doc->GetWindow()->SetCursor(CURSOR_DEFAULT_ARROW, TRUE);
		}
	}
	else
	{
		drag_object->SetDropType(DROP_NONE);
		drag_object->SetVisualDropType(DROP_NONE);
	}

	m_actions_recorder.SetMustReplayNextAction(TRUE);
	m_actions_recorder.ReplayRecordedDragActions();
}

void
DragDropManager::HandleOnDragLeaveEvent(HTML_Element* this_elm, HTML_Document* doc)
{
	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);

	if (this_elm->IsWidgetAcceptingDrop() && this_elm->GetFormObject())
		this_elm->GetFormObject()->GetWidget()->GenerateOnDragLeave(drag_object);

	if (doc)
	{
		FramesDocument* frames_doc = doc->GetFramesDocument();
#ifdef DOCUMENT_EDIT_SUPPORT
		if (this_elm->IsContentEditable(TRUE) || frames_doc->GetDesignMode())
		{
			if (frames_doc->GetCaretPainter())
				frames_doc->GetCaretPainter()->DragCaret(FALSE);
		}
		else
#endif // DOCUMENT_EDIT_SUPPORT
		{
#ifdef SVG_SUPPORT
			if (this_elm->GetNsType() == NS_SVG && g_svg_manager->IsEditableElement(frames_doc, this_elm))
				g_svg_manager->HideCaret(frames_doc, this_elm);
#endif // SVG_SUPPORT
		}
	}

	if (m_actions_recorder.GetMustReplayNextAction())
		m_actions_recorder.ReplayRecordedDragActions();
}

void
DragDropManager::HandleOnDropEvent(HTML_Document* doc, int document_x, int document_y, int offset_x, int offset_y, ShiftKeyState modifiers, BOOL cancelled)
{
	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	OP_ASSERT(priv_data);
	BOOL dropped = TRUE;
	AutoNullOnDeleteElementRef ondragend_target_elm;
	ondragend_target_elm.SetElm(priv_data->GetSourceHtmlElement());
	HTML_Document* src_doc = priv_data->GetElementsDocument(ondragend_target_elm.GetElm());
	if (!cancelled && doc)
	{
		FramesDocument* frames_doc = doc->GetFramesDocument();
		HTML_Element* dnd_current_target = doc->GetCurrentTargetElement();
		BOOL is_docedit = FALSE;
		if (HasAcceptableData(drag_object, dnd_current_target, frames_doc, is_docedit))
		{
			const uni_char* text = DragDrop_Data_Utils::GetStringData(drag_object, TEXT_DATA_FORMAT);
			if (dnd_current_target->IsWidgetAcceptingDrop())
			{
				if (FormObject* form_object = dnd_current_target->GetFormObject())
					form_object->GetWidget()->GenerateOnDragDrop(drag_object, OpPoint(offset_x, offset_y));
			}
#ifdef DOCUMENT_EDIT_SUPPORT
			else if (is_docedit)
			{
				if (OpDocumentEdit* doc_edit = frames_doc->GetDocumentEdit())
				{
					CaretPainter* caret_painter = frames_doc->GetCaretPainter();
					if (caret_painter)
					{
						caret_painter->DragCaret(TRUE);

						doc_edit->m_caret.Place(document_x, document_y, FALSE, FALSE, TRUE);
						const uni_char* html = DragDrop_Data_Utils::GetStringData(drag_object, HTML_DATA_FORMAT);
						OP_STATUS status;
						if (html && *html)
						{
							status = doc_edit->InsertTextHTML(html, uni_strlen(html), NULL, NULL, NULL, TIDY_LEVEL_NORMAL, FALSE);
							if (OpStatus::IsError(status))
							{
								if (OpStatus::IsMemoryError(status))
									frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
								dropped = FALSE;
							}
						}
						else if (OpStatus::IsError(status = doc_edit->InsertText(text, uni_strlen(text), FALSE, FALSE)))
						{
							if (OpStatus::IsMemoryError(status))
								frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
							dropped = FALSE;
						}

						OpPoint caret_point = caret_painter->GetCaretPosInDocument();
						caret_painter->DragCaret(FALSE);
						doc_edit->SetFocus(FOCUS_REASON_MOUSE);

						if (dropped)
						{
							if (drag_object->GetDropType() == DROP_MOVE)
							{
								SelectionState sel_state = doc_edit->GetSelectionState(FALSE, FALSE, FALSE);
								if(sel_state.HasContent() && doc_edit->ActualizeSelectionState(sel_state))
								{
									/* The selected content wil be removed. What's more one of elements
									   being in that content is the source, thus ONDRAGEND should be fired on it.
									   However removing the source element will cause the event won't be fired.
									   Since there might be some parent elements being outside the selection which handles ONDRAGEND,
									   find the nearest one and fire ONDRAGEND on it.
									 */
									HTML_Element* end_handling_elm = NULL;
									AutoNullOnDeleteElementRef handling_elm_ref;
									HTML_Element* iter = ondragend_target_elm.GetElm();
									while (iter && iter->IsInSelection())
										iter = iter->ParentActual();

									if (iter)
									{
										FramesDocument* src_frm_doc = src_doc ? src_doc->GetFramesDocument() : NULL;
										iter->HasEventHandler(src_frm_doc, ONDRAGEND, NULL, &end_handling_elm, ES_PHASE_ANY);
										handling_elm_ref.SetElm(end_handling_elm);
									}

									doc_edit->m_caret.Place(caret_point.x, caret_point.y, FALSE, FALSE, TRUE);
									doc_edit->m_selection.RemoveContent(TRUE, sel_state.start_elm, sel_state.stop_elm, sel_state.start_ofs, sel_state.stop_ofs, FALSE);

									if (!ondragend_target_elm.GetElm() && handling_elm_ref.GetElm())
										ondragend_target_elm.SetElm(handling_elm_ref.GetElm());
								}
							}
							else
								doc_edit->m_caret.Place(caret_point.x, caret_point.y, FALSE, FALSE, TRUE);
						}

						caret_painter->RestartBlink();
					}
					else
						dropped = FALSE;
				}
			}
#endif // DOCUMENT_EDIT_SUPPORT
#ifdef SVG_SUPPORT
			else if (dnd_current_target->GetNsType() == NS_SVG && g_svg_manager->IsEditableElement(frames_doc, dnd_current_target))
			{
				g_svg_manager->InsertTextAtCaret(dnd_current_target, text);
				g_svg_manager->BeginEditing(frames_doc, dnd_current_target, FOCUS_REASON_MOUSE);
			}
			else
#endif // SVG_SUPPORT
				drag_object->SetDropType(DROP_NONE);
		}
	}

	priv_data->SetIsDropped(dropped);
	if (src_doc && ondragend_target_elm.GetElm())
	{
		HandleDragEvent(src_doc,
						ONDRAGEND,
						ondragend_target_elm.GetElm(),
						document_x, document_y,
						offset_x, offset_y,
						modifiers);
	}
	else
		StopDrag();
}

void
DragDropManager::HandleOnDragEndEvent(HTML_Document* doc)
{
	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	OP_ASSERT(priv_data);
	if (doc)
	{
		FramesDocument* frames_doc = doc->GetFramesDocument();
		HTML_Element* source_elm = priv_data->GetSourceHtmlElement();
		HTML_Document* target_doc = priv_data->GetTargetDocument();
		HTML_Element* target_elm = target_doc ? target_doc->GetCurrentTargetElement() : NULL;
		if (target_elm && drag_object->GetDropType() == DROP_MOVE && priv_data->IsDropped())
		{
			if (source_elm && (target_elm->IsWidgetAcceptingDrop()
#ifdef DOCUMENT_EDIT_SUPPORT
				|| (target_doc->GetFramesDocument()->GetDesignMode() || (target_elm->IsContentEditable(TRUE)))
#endif // DOCUMENT_EDIT_SUPPORT
#ifdef SVG_SUPPORT
				|| (source_elm->GetNsType() == NS_SVG && g_svg_manager->IsEditableElement(frames_doc, source_elm))
#endif // SVG_SUPPORT
				))
			{
				if (source_elm->IsWidgetWithDraggableSelection())
				{
					OpWidget* captured_widget = static_cast<OpWidget*>(drag_object->GetSource());
					if (captured_widget && !captured_widget->IsReadOnly())
					{
						OpString text;
						if (OpStatus::IsMemoryError(captured_widget->GetText(text)))
						{
							frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
							return;
						}

						FormObject* target_form_obj = target_elm->IsWidgetAcceptingDrop() ? target_elm->GetFormObject() : NULL;
						OpWidget* target_widget = NULL;
						if (target_form_obj)
							target_widget = target_form_obj->GetWidget();

						if (target_widget)
						{
							VisualDevice* vis_dev = frames_doc->GetVisualDevice();
							int document_x = priv_data->m_last_known_drag_point.x + vis_dev->GetRenderingViewX();
							int document_y = priv_data->m_last_known_drag_point.y + vis_dev->GetRenderingViewY();
							OpRect widget_rect = target_widget->GetDocumentRect(TRUE);
							OpWidget* target_widget_child = target_widget->GetWidget(OpPoint(document_x - widget_rect.x, document_y - widget_rect.y), TRUE);
							target_widget = target_widget_child ? target_widget_child : target_widget;
						}

						// If the text is dropped to the same widget it was dragged from the widget takes care of everything.
						if (captured_widget != target_widget)
						{
							INT32 selection_offset_start, selection_offset_end;
							captured_widget->GetSelection(selection_offset_start, selection_offset_end);
							text.Delete(selection_offset_start, selection_offset_end - selection_offset_start);
							if (OpStatus::IsMemoryError(captured_widget->SetTextWithUndo(text.CStr())))
								frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
						}
					}
				}
#ifdef SVG_SUPPORT
				else if (source_elm->GetNsType() == NS_SVG && g_svg_manager->IsEditableElement(frames_doc, source_elm))
					g_svg_manager->RemoveSelectedContent(source_elm);
#endif // SVG_SUPPORT
			}
		}
	}

	if (!priv_data->IsDropped() && drag_object->GetDropType() != DROP_NONE)
		drag_object->SetDropType(DROP_NONE); // So the platform gets the correct drop effect on d'n'd end.

	StopDrag(m_is_cancelled);
}

void
DragDropManager::OnElementRemove(HTML_Element* elm)
{
	if (IsDragging())
	{
		OpDragObject* drag_object = GetDragObject();
		OP_ASSERT(drag_object);
		PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
		if (priv_data)
			priv_data->OnElementRemove(elm);
	}
}

void
DragDropManager::OnDocumentUnload(HTML_Document* doc)
{
	if (IsDragging())
	{
		OpDragObject* drag_object = GetDragObject();
		OP_ASSERT(drag_object);
		PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
		if (priv_data)
			priv_data->OnDocumentUnload(doc);

		if (m_mouse_down_status != NONE && m_mouse_down_status != ALLOWED)
			StopDrag(TRUE);
		else
			m_actions_recorder.OnDocumentUnload(doc);
		CancelFileDropDialog(doc);
	}
}

void
DragDropManager::SetProperDropType(DOM_EventType event)
{
	if (OpDragObject* drag_object = GetDragObject())
	{
		if (event == ONDRAGOVER || event == ONDRAGENTER)
		{
			DropType suggested = drag_object->GetSuggestedDropType();
			if (suggested == DROP_UNINITIALIZED)
			{
				PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
				OP_ASSERT(priv_data);
				unsigned effects_allowed = drag_object->GetEffectsAllowed();
				if (effects_allowed == DROP_NONE)
					drag_object->SetDropType(DROP_NONE);
				else if ((effects_allowed & DROP_COPY) != 0)
					drag_object->SetDropType(DROP_COPY);
				else if ((effects_allowed & DROP_LINK) != 0)
					drag_object->SetDropType(DROP_LINK);
				else if (effects_allowed == DROP_MOVE)
					drag_object->SetDropType(DROP_MOVE);
				else if (effects_allowed == DROP_UNINITIALIZED)
				{
					if (HTML_Element* source_elm = priv_data->GetSourceHtmlElement())
					{
#ifdef DOCUMENT_EDIT_SUPPORT
						HTML_Document* target_doc = priv_data->GetTargetDocument();
						HTML_Document* src_doc = priv_data->GetElementsDocument(source_elm);
						HTML_Element* target_elm = target_doc ? target_doc->GetCurrentTargetElement() : NULL;
#endif // DOCUMENT_EDIT_SUPPORT

						/* We have to check this every time ONDRAGENTER or ONDRAGOVER is fired.
						Moreover it cannot be set in OpDragObject when it's created
						because this would be reflected in DOM (dataTransfer.dragEffect)
						and would break the spec saying that dataTransfer.dragEffect
						must be 'uninitialized' on drag'n'drop initialization. */
						if (source_elm->IsWidgetWithDraggableSelection() &&
							priv_data->IsSelection())
							drag_object->SetDropType(DROP_MOVE);
#ifdef DOCUMENT_EDIT_SUPPORT
						else if ((target_elm && source_elm->IsContentEditable(TRUE) && target_elm->IsContentEditable(TRUE)) ||
						         (target_doc->GetFramesDocument() && target_doc->GetFramesDocument()->GetDesignMode() &&
						         src_doc->GetFramesDocument() && src_doc->GetFramesDocument()->GetDesignMode()))
							drag_object->SetDropType(DROP_MOVE);
#endif // DOCUMENT_EDIT_SUPPORT
						else if (source_elm->IsMatchingType(HE_A, NS_HTML))
							drag_object->SetDropType(DROP_LINK);
						else
							drag_object->SetDropType(DROP_COPY);
					}
					else
						drag_object->SetDropType(DROP_COPY);
				}
				else
					drag_object->SetDropType(DROP_COPY);
			}
			else
				drag_object->SetDropType(suggested);
		}
		else if (event != ONDROP && event != ONDRAGEND)
			drag_object->SetDropType(DROP_NONE);
	}
	else
		OP_ASSERT(event == ONDRAGSTART);
}

HTML_Element*
DragDropManager::FindIntersectingElement(HTML_Document* doc, int x, int y, int& offset_x, int& offset_y)
{
	if (!doc)
		return NULL;

	FramesDocument* frames_doc = doc->GetFramesDocument();
	LogicalDocument* logdoc = frames_doc->GetLogicalDocument();

	HTML_Element* h_elm = NULL;

	if (logdoc && logdoc->GetRoot())
	{

		IntersectionObject intersection(frames_doc, LayoutCoord(x), LayoutCoord(y), TRUE);

#ifdef LAYOUT_YIELD_REFLOW
		LayoutWorkplace* wp = logdoc->GetLayoutWorkplace();
		if (wp)
			wp->SetCanYield(FALSE);
#endif // LAYOUT_YIELD_REFLOW

		intersection.Traverse(logdoc->GetRoot());

		if (intersection.IsOutOfMemory())
		{
			frames_doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return NULL;
		}

		Box* inner_box = intersection.GetInnerBox();

		offset_x = intersection.GetRelativeX();
		offset_y = intersection.GetRelativeY();

		if (inner_box)
		{
			h_elm = inner_box->GetHtmlElement();

			// We sometimes have text nodes here for internal purposes. Externally we'll show an element
			// so the offset needs to be relative to that element (which is the parent of the text node).
			if (h_elm->IsText())
			{
				HTML_Element* parent = h_elm->ParentActual();
				if (parent && parent->GetLayoutBox())
				{
					LayoutCoord elm_offset_x;
					LayoutCoord elm_offset_y;
					if (intersection.GetRelativeToBox(parent->GetLayoutBox(), elm_offset_x, elm_offset_y))
					{
						offset_x = elm_offset_x;
						offset_y = elm_offset_y;
					}
				}
			}

			if (h_elm->Type() == HE_DOC_ROOT)
				h_elm = logdoc->GetDocRoot();
		}

#ifdef SVG_SUPPORT
		if (h_elm && h_elm->IsMatchingType(Markup::SVGE_SVG, NS_SVG) && frames_doc->GetShowImages())
		{
			HTML_Element* event_target = NULL;

			g_svg_manager->FindSVGElement(h_elm, frames_doc, x, y, &event_target, offset_x, offset_y);
			if (event_target != NULL)
				h_elm = event_target;
		}
#endif // SVG_SUPPORT

		if (h_elm)
		{
			HTML_Element* actual_element = h_elm;

			if (actual_element->GetInserted() == HE_INSERTED_BY_LAYOUT)
				actual_element = actual_element->ParentActual();

			// Image with usemap?
			if (HTML_Element* linked_element = actual_element->GetLinkedElement(frames_doc, offset_x, offset_y, h_elm))
				h_elm = linked_element;
		}
	}

	return h_elm;
}

OP_STATUS
DragDropManager::DragDrop(HTML_Document* doc, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, ShiftKeyState modifiers)
{
	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	HTML_Element* dnd_source_element = priv_data->GetSourceHtmlElement();
	HTML_Document* src_doc = priv_data->GetElementsDocument(dnd_source_element);
	BOOL notify_end = TRUE;
	m_actions_recorder.CancelPending();
	OP_STATUS status = OpStatus::OK;
	if (doc && doc->GetCurrentTargetElement())
	{
		if (drag_object->GetDropType() != DROP_NONE)
		{
			status = HandleDragEvent(doc,
									ONDROP,
									doc->GetCurrentTargetElement(),
									x, y,
									offset_x, offset_y,
									modifiers,
									&visual_viewport_x, &visual_viewport_y);
			if (OpStatus::IsSuccess(status))
				notify_end = FALSE;
		}
		else
		{
			m_actions_recorder.SetMustReplayNextAction(TRUE);
			status = HandleDragEvent(doc,
									ONDRAGLEAVE,
									doc->GetCurrentTargetElement(),
									x, y,
									offset_x, offset_y,
									modifiers,
									&visual_viewport_x, &visual_viewport_y);
		}
	}

	if (notify_end)
	{
		if (src_doc)
			DragAction(src_doc, ONDRAGEND, x, y, visual_viewport_x, visual_viewport_y, offset_x, offset_y, (modifiers & SHIFTKEY_SHIFT) != 0, (modifiers & SHIFTKEY_CTRL) != 0, (modifiers & SHIFTKEY_ALT) != 0);
		else // We couldn't send ONDRAGEND event which would end and clean up d'n'd so we have to do it here.
			StopDrag();
	}

	doc->GetWindow()->UseDefaultCursor();

	return status;
}

/* virtual */
void
FileDropConfirmationCallback::OnDialogReply(Reply reply)
{
	g_drag_manager->m_drop_file_cb = NULL;
	if (g_drag_manager->IsDragging())
	{
		if (reply != REPLY_YES)
			g_drag_manager->GetDragObject()->SetDropType(DROP_NONE);
		else
			g_drag_manager->GetDragObject()->SetDropType(drop_type);

		g_drag_manager->DragDrop(document, x, y, visual_viewport_x, visual_viewport_y, offset_x, offset_y, modifiers);
	}

	OP_DELETE(this);
}

void
DragDropManager::CancelFileDropDialog(HTML_Document* doc)
{
	if (m_drop_file_cb && doc == m_drop_file_cb->document)
	{
		WindowCommander* wincmd = doc->GetWindow()->GetWindowCommander();
		wincmd->GetDocumentListener()->OnCancelDialog(wincmd, m_drop_file_cb);
		OnDragCancel(m_drop_file_cb->x, m_drop_file_cb->y, m_drop_file_cb->modifiers);
		OP_DELETE(m_drop_file_cb);
		m_drop_file_cb = NULL;
	}
}

void
DragDropManager::ShowFileDropDialog(HTML_Document* doc, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, ShiftKeyState modifiers)
{
	m_drop_file_cb = OP_NEW(FileDropConfirmationCallback, (doc, x, y, visual_viewport_x, visual_viewport_y, offset_x, offset_y, modifiers, GetDragObject()->GetDropType()));
	if (m_drop_file_cb)
	{
		WindowCommander* win_cmd = doc->GetWindow()->GetWindowCommander();
		OpString scheme;
		URL url = doc->GetFramesDocument()->GetSecurityContext();
		const uni_char* destination = url.GetAttribute(URL::KUniHostName).CStr();
		if (!destination)
		{
			if (OpStatus::IsSuccess(scheme.Set(url.GetAttribute(URL::KProtocolName))))
				scheme.Append(UNI_L(":..."));

			destination = scheme.CStr();
		}
		win_cmd->GetDocumentListener()->OnFileDropConfirm(win_cmd, destination, m_drop_file_cb);
		Block();
		doc->GetWindow()->UseDefaultCursor();
	}
	else
		doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
}

BOOL
DragDropManager::DragAction(HTML_Document* doc, DOM_EventType event, int x, int y, int visual_viewport_x, int visual_viewport_y, int offset_x, int offset_y, BOOL shift_pressed, BOOL control_pressed, BOOL alt_pressed)
{
	if (!doc)
		return FALSE;

	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	OP_ASSERT(priv_data);
	HTML_Element* dnd_source_element = priv_data->GetSourceHtmlElement();
#ifdef DEBUG_ENABLE_OPASSERT
	HTML_Document* src_doc = priv_data->GetElementsDocument(dnd_source_element);
#endif // DEBUG_ENABLE_OPASSERT
	ShiftKeyState modifiers = (shift_pressed ? SHIFTKEY_SHIFT : 0) | (control_pressed ? SHIFTKEY_CTRL : 0) | (alt_pressed ? SHIFTKEY_ALT : 0);
	BOOL handled = FALSE;

	if (m_actions_recorder.IsDragActionBeingProcessed() || m_actions_recorder.HasRecordedDragActions() && !m_actions_recorder.IsReplaying())
	{
		if (OpStatus::IsMemoryError(m_actions_recorder.RecordAction(doc, event, x, y, visual_viewport_x, visual_viewport_y, offset_x, offset_y, shift_pressed, control_pressed, alt_pressed)))
		{
			// If ONDROP couldn't be recorded pretend the d'n'd ends.
			if (event == ONDROP)
				event = ONDRAGEND;
			goto raise_oom;
		}
		return FALSE;
	}

	SetProperDropType(event);
	switch (event)
	{
		case ONDRAG:
			{
				if (dnd_source_element)
				{
					m_actions_recorder.OnDragActionHandle();
					OP_ASSERT(src_doc == doc || !"This event must be fired on the drag source document.");
					if (HandleDragEvent(doc,
										ONDRAG,
										dnd_source_element,
										x, y,
										offset_x, offset_y,
										modifiers,
										&visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}
					handled = TRUE;
				}
			}
			break;

		case ONDRAGSTART:
			{
				if (dnd_source_element)
				{
					m_actions_recorder.OnDragActionHandle();
					OP_ASSERT(src_doc == doc || !"This event must be fired on the drag source document.");
					if (HandleDragEvent(doc,
										ONDRAGSTART,
										dnd_source_element,
										x, y,
										offset_x, offset_y,
										modifiers,
										&visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}
					handled = TRUE;
				}
			}
			break;

		case ONDRAGENTER:
			{
				if (HTML_Element* immediate_selection = doc->GetImmediateSelectionElement())
				{
					m_actions_recorder.OnDragActionHandle();
					if (HandleDragEvent(doc,
										ONDRAGENTER,
										immediate_selection,
										x, y,
										offset_x, offset_y,
										modifiers,
										&visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}
					handled = TRUE;
				}
			}
			break;

		case ONDROP:
			{
				if (doc->GetCurrentTargetElement())
					m_actions_recorder.OnDragActionHandle();

				if (drag_object->GetDropType() != DROP_NONE)
				{
					if (DragDrop_Data_Utils::HasFiles(drag_object))
					{
						ShowFileDropDialog(doc, x, y, visual_viewport_x, visual_viewport_y, offset_x, offset_y, modifiers);
						return FALSE;
					}
				}

				if (OpStatus::IsMemoryError(DragDrop(doc, x, y, visual_viewport_x, visual_viewport_y, offset_x, offset_y, modifiers)))
				{
					goto raise_oom;
				}
			}
			return TRUE;

		case ONDRAGLEAVE:
			{
				if (HTML_Element* current_target = doc->GetCurrentTargetElement())
				{
					m_actions_recorder.OnDragActionHandle();
					m_actions_recorder.SetMustReplayNextAction(TRUE);
					if (HandleDragEvent(doc,
										ONDRAGLEAVE,
										current_target,
										x, y,
										offset_x, offset_y,
										modifiers,
										&visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}
					handled = TRUE;
				}
			}
			break;

		case ONDRAGEND:
			{
				if (dnd_source_element)
				{
					m_actions_recorder.OnDragActionHandle();
					OP_ASSERT(src_doc == doc || !"This event must be fired on the drag source document.");
					if (HandleDragEvent(doc,
										ONDRAGEND,
										dnd_source_element,
										x, y,
										offset_x, offset_y,
										modifiers,
										&visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}
					handled = TRUE;
				}
			}
			break;

		case ONDRAGOVER:
			{
				if (HTML_Element* dnd_target_elm = doc->GetCurrentTargetElement())
				{
					m_actions_recorder.OnDragActionHandle();
					if (HandleDragEvent(doc,
										ONDRAGOVER,
										dnd_target_elm,
										x, y,
										offset_x, offset_y,
										modifiers,
										&visual_viewport_x, &visual_viewport_y) == OpStatus::ERR_NO_MEMORY)
					{
						goto raise_oom;
					}
					handled = TRUE;
				}
			}
			break;

		default:
			return FALSE;
	}

	// DragDrop() called for ONDROP should take care of everything so it does not need to be taken care of here and below (OOM handling).
	if (!handled)
	{
		// In case an important drag event (a one of driving or ending) could not be handled
		// due to e.g. the target element being deleted in a meanwhile make sure d'n'd will not end up in a wrong state
		if (event == ONDRAGEND)
			StopDrag();
		else
		{
			m_actions_recorder.AllowNestedReplay();
			m_actions_recorder.SetMustReplayNextAction(TRUE);
			m_actions_recorder.ReplayRecordedDragActions();
		}
		handled = TRUE;
	}

	return handled;

raise_oom:
	if (event == ONDRAGEND)
		StopDrag();
	else
		m_actions_recorder.OnOOM();
	doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	return TRUE;
}

OP_STATUS
DragDropManager::HandleDragEvent(HTML_Document* html_doc, DOM_EventType event, HTML_Element* target,
                                 int document_x, int document_y, int offset_x, int offset_y,
                                 ShiftKeyState modifiers, int* visual_viewport_x /* = NULL */, int* visual_viewport_y /* = NULL */)
{
	if (!html_doc)
		return OpStatus::OK;

	FramesDocument* doc = html_doc->GetFramesDocument();

	OP_BOOLEAN handled = OpBoolean::IS_FALSE;

	BOOL allowed = event == ONDRAGSTART || event == ONDRAG || event == ONDRAGEND || g_drag_manager->IsURLAllowed(GetDragObject(), doc->GetSecurityContext());
	if (!doc->IsPrintDocument() && allowed)
	{
#ifdef SUPPORT_VISUAL_ADBLOCK
		if (!doc->GetWindow()->GetContentBlockEditMode())
#endif // SUPPORT_VISUAL_ADBLOCK
		{
			VisualDevice* vis_dev = doc->GetVisualDevice();
			CoreView* view = vis_dev->GetView();
			OpPoint screen_offset = view ? view->ConvertToScreen(OpPoint(0, 0)) : OpPoint(0, 0);

			if (FramesDocument::NeedToFireEvent(doc, target, NULL, event))
			{
				OP_STATUS status = doc->ConstructDOMEnvironment();

				if (OpStatus::IsSuccess(status))
				{
					DOM_Environment::EventData data;

					data.type = event;
					data.detail = 0;
					data.target = target;
					data.modifiers = modifiers;
					data.button = MOUSE_BUTTON_1;
					int view_x = visual_viewport_x ? *visual_viewport_x : doc->GetVisualViewX();
					int view_y = visual_viewport_y ? *visual_viewport_y : doc->GetVisualViewY();
					data.screenX = document_x - view_x + screen_offset.x;
					data.screenY = document_y - view_y + screen_offset.y;
					data.clientX = document_x - view_x;
					data.clientY = document_y - view_y;
					data.offsetX = offset_x;
					data.offsetY = offset_y;
					data.calculate_offset_lazily = FALSE;
					data.relatedTarget = NULL;

					handled = doc->GetDOMEnvironment()->HandleEvent(data, NULL);
				}
				else
					handled = status;
			}
		}
	}

	if (handled == OpBoolean::IS_FALSE)
	{
#ifndef MOUSELESS
		if (event == ONDRAGENTER && doc->GetHtmlDocument())
			doc->GetHtmlDocument()->UpdateMouseMovePosition(document_x, document_y);
#endif // !MOUSELESS

		HTML_Element::HandleEventOptions opts;
		opts.related_target = NULL;
		opts.offset_x = offset_x;
		opts.offset_y = offset_y;
		opts.document_x = document_x;
		opts.document_y = document_y;
		opts.sequence_count_and_button_or_key_or_delta = MOUSE_BUTTON_1;
		opts.modifiers = modifiers;
		opts.synthetic = FALSE;
		target->HandleEvent(event, doc, target, opts);
	}

	return OpStatus::IsMemoryError(handled) ? handled : OpStatus::OK;
}

void
DragDropManager::MoveToNextSequence()
{
	m_actions_recorder.AllowNestedReplay();
	m_actions_recorder.SetMustReplayNextAction(TRUE);
	m_actions_recorder.ReplayRecordedDragActions();
}

static int GetScrollDelta(int size, int distance_to_right_or_bottom_edge, int distance_to_left_or_top_edge)
{
	// Get the size of the margin and deltas in px.
	int margin = ((DND_SCROLL_MARGIN * size + 99) / 100);
	// Take minimal margin in px into account.
	margin = MAX(margin, DND_SCROLL_MARGIN_MIN_PX);
	// Margin must not be bigger that 50% of size - 1 px.
	margin = MIN(margin, (size + 1) / 2 - 1);
	int delta_min = (DND_SCROLL_DELTA_MIN * size + 99) / 100;
	int delta_max = (DND_SCROLL_DELTA_MAX * size + 99) / 100;
	int scroll_delta = 0;
	if (margin > 0)
	{
		if (distance_to_right_or_bottom_edge <= margin)
			scroll_delta = delta_min + (margin - distance_to_right_or_bottom_edge) * (delta_max - delta_min) / margin;
		else if (distance_to_left_or_top_edge <= margin)
			scroll_delta = -(delta_min + (margin - distance_to_left_or_top_edge) * (delta_max - delta_min) / margin);
	}

	return scroll_delta;
}

BOOL
DragDropManager::ScrollIfNeeded(int x, int y)
{
	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	OP_ASSERT(priv_data);
	HTML_Document* target_doc = priv_data->GetTargetDocument();
	if (!target_doc)
		return FALSE;

	VisualDevice* vis_dev = target_doc->GetVisualDevice();
	int current_x = x + vis_dev->GetRenderingViewX();
	int current_y = y + vis_dev->GetRenderingViewY();
	return ScrollIfNeededInternal(target_doc->GetFramesDocument(), current_x, current_y);
}

BOOL
DragDropManager::ScrollIfNeededInternal(FramesDocument* doc, int x, int y)
{
	if (!doc)
		return FALSE;

	BOOL scrolled = FALSE;
	OpPoint point(x, y);
	int dummy_offset_x, dummy_offset_y;
	HTML_Element* intersecting = doc->GetHtmlDocument() ? FindIntersectingElement(doc->GetHtmlDocument(), x, y, dummy_offset_x, dummy_offset_y) : NULL;
	ScrollableArea* sc = GetScrollableParent(intersecting);
	HTML_Element* scrollable_elm;
	while (sc)
	{
		Box* box = sc->GetOwningBox();
		scrollable_elm = box->GetHtmlElement();
		if (sc->HasHorizontalScrollbar() || sc->HasVerticalScrollbar())
		{
			int dx = 0, dy = 0;
			OpRect bounding_rect;
			// Get transform.
			AffinePos pos = sc->GetPosInDocument();
			// Get the box.
			bounding_rect = box->GetBorderEdges();
			// Get box's vertexts.
			OpPoint top_left = bounding_rect.TopLeft();
			OpPoint top_right = bounding_rect.TopRight();
			OpPoint bottom_left = bounding_rect.BottomLeft();
			OpPoint bottom_right = bounding_rect.BottomRight();
			// Transform the vertexts.
			pos.Apply(top_left);
			pos.Apply(top_right);
			pos.Apply(bottom_left);
			pos.Apply(bottom_right);
			double bottom_left_x = static_cast<double>(bottom_left.x);
			double bottom_left_y = static_cast<double>(bottom_left.y);
			double top_left_x = static_cast<double>(top_left.x);
			double top_left_y = static_cast<double>(top_left.y);
			double bottom_right_x = static_cast<double>(bottom_right.x);
			double bottom_right_y = static_cast<double>(bottom_right.y);
			double top_right_x = static_cast<double>(top_right.x);
			double top_right_y = static_cast<double>(top_right.y);

			if (sc->HasHorizontalScrollbar())
			{
				// Calculate the coefficients of lines connecting the box vertexts.
				double top_left_bottom_left_line_coef_a = -bottom_left_y + top_left_y;
				double top_left_bottom_left_line_coef_b = bottom_left_x - top_left_x;
				double top_left_bottom_left_line_coef_c = -(bottom_left_x * top_left_y) + top_left_x * bottom_left_y;

				double top_right_bottom_right_line_coef_a = -bottom_right_y + top_right_y;
				double top_right_bottom_right_line_coef_b = bottom_right_x - top_right_x;
				double top_right_bottom_right_line_coef_c = -(bottom_right_x * top_right_y) + top_right_x * bottom_right_y;

				// Calculate the distance to the lines.
				double distance_left_edge = op_fabs(top_left_bottom_left_line_coef_a * point.x + top_left_bottom_left_line_coef_b * point.y + top_left_bottom_left_line_coef_c) /
				op_sqrt((top_left_bottom_left_line_coef_a * top_left_bottom_left_line_coef_a) + (top_left_bottom_left_line_coef_b * top_left_bottom_left_line_coef_b));
				double distance_right_edge = op_fabs(top_right_bottom_right_line_coef_a * point.x + top_right_bottom_right_line_coef_b * point.y + top_right_bottom_right_line_coef_c) /
				op_sqrt((top_right_bottom_right_line_coef_a * top_right_bottom_right_line_coef_a) + (top_right_bottom_right_line_coef_b * top_right_bottom_right_line_coef_b ));

				dx = GetScrollDelta(bounding_rect.width, static_cast<int>(distance_right_edge), static_cast<int>(distance_left_edge));
			}
			if (sc->HasVerticalScrollbar())
			{
				double top_left_top_right_line_coef_a = -top_right_y + top_left_y;
				double top_left_top_right_line_coef_b = top_right_x - top_left_x;
				double top_left_top_right_line_coef_c = -(top_right_x * top_left_y) + top_left_x * top_right_y;

				double bottom_right_bottom_left_line_coef_a = -bottom_left_y + bottom_right_y;
				double bottom_right_bottom_left_line_coef_b = bottom_left_x - bottom_right_x;
				double bottom_right_bottom_left_line_coef_c = -(bottom_left_x * bottom_right_y) + bottom_right_x * bottom_left_y;

				double distance_top_edge = op_fabs(top_left_top_right_line_coef_a * point.x + top_left_top_right_line_coef_b * point.y + top_left_top_right_line_coef_c) /
				op_sqrt((top_left_top_right_line_coef_a * top_left_top_right_line_coef_a) + (top_left_top_right_line_coef_b * top_left_top_right_line_coef_b ));
				double distance_bottom_edge = op_fabs(bottom_right_bottom_left_line_coef_a * point.x + bottom_right_bottom_left_line_coef_b * point.y + bottom_right_bottom_left_line_coef_c) /
				op_sqrt((bottom_right_bottom_left_line_coef_a * bottom_right_bottom_left_line_coef_a) + (bottom_right_bottom_left_line_coef_b * bottom_right_bottom_left_line_coef_b));

				dy = GetScrollDelta(bounding_rect.width, static_cast<int>(distance_bottom_edge), static_cast<int>(distance_top_edge));
			}
			scrolled |= dx != 0 || dy != 0;
			if (dx != 0)
				sc->SetViewX(LayoutCoord(sc->GetViewX() + dx), TRUE);
			if (dy != 0)
				sc->SetViewY(LayoutCoord(sc->GetViewY() + dy), TRUE);
		}

		sc = GetScrollableParent(scrollable_elm->Parent());
	}

	OpRect view_port = doc->GetVisualViewport();
	// Get the distances.
	int distance_right_edge = view_port.x + view_port.width - point.x;
	int distance_left_edge = point.x - view_port.x;
	int distance_bottom_edge = view_port.y + view_port.height - point.y;
	int distance_top_edge = point.y - view_port.y;
	int dx = GetScrollDelta(view_port.width, distance_right_edge, distance_left_edge);
	int dy = GetScrollDelta(view_port.height, distance_bottom_edge, distance_top_edge);

	if (dx != 0 || dy != 0)
	{
		scrolled |= TRUE;
		int newx = view_port.x + dx;
		long newy = view_port.y + dy;
		doc->RequestSetVisualViewPos(newx, newy, VIEWPORT_CHANGE_REASON_INPUT_ACTION);
	}

	FramesDocument* parent = doc->GetParentDoc();
	if (parent)
	{
		FramesDocElm* elm = parent->GetFrmDocElmByDoc(doc);
		AffinePos pos = elm->GetAbsPos();
		VisualDevice* vis_dev = doc->GetVisualDevice();
		point.x -= vis_dev->GetRenderingViewX();
		point.y -= vis_dev->GetRenderingViewY();
		point += vis_dev->GetInnerPosition();
		pos.Apply(point);
		scrolled |= ScrollIfNeededInternal(parent, point.x, point.y);
	}

	return scrolled;
}

OP_STATUS
DragDropManager::AddElement(HTML_Document* doc, HTML_Element* elm, HTML_Document* elm_doc)
{
	if (!doc || !elm || !elm_doc)
		return OpStatus::ERR_NULL_POINTER;

	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	RETURN_IF_ERROR(priv_data->AddElement(elm, elm_doc));
	OpRect new_rect;
	OpPoint new_point(0,0);
	OpBitmap* drag_bitmap;
	RETURN_IF_ERROR(DragDropVisualFeedback::Create(doc, priv_data->GetElementsList(), new_point, new_rect, drag_bitmap));
	OpPoint old_point = drag_object->GetBitmapPoint();
	OpRect old_rect = priv_data->GetBitmapRect();
	new_point.x = old_point.x + MAX(old_rect.x - new_rect.x, 0);
	new_point.y = old_point.y + MAX(old_rect.y - new_rect.y, 0);
	drag_object->SetBitmapPoint(new_point);
	priv_data->SetBitmapRect(new_rect);
	drag_object->SetBitmap(drag_bitmap);
	return OpStatus::OK;
}

OP_STATUS
DragDropManager::SetFeedbackElement(HTML_Document* doc, HTML_Element* elm, HTML_Document* elm_doc, const OpPoint& point)
{
	if (!doc || !elm || !elm_doc)
		return OpStatus::ERR_NULL_POINTER;

	OpAutoPointerHashTable<HTML_Element, PrivateDragData::DocElm> elements;
	PrivateDragData::DocElm* doc_elm = OP_NEW(PrivateDragData::DocElm, ());
	RETURN_OOM_IF_NULL(doc_elm);
	OpAutoPtr<PrivateDragData::DocElm> auto_doc_elm(doc_elm);
	doc_elm->document = elm_doc;
	RETURN_IF_ERROR(elements.Add(elm, auto_doc_elm.get()));
	auto_doc_elm.release();
	OpPoint dummy;
	OpRect out;
	OpBitmap* drag_bitmap;
	RETURN_IF_ERROR(DragDropVisualFeedback::Create(doc, elements, dummy, out, drag_bitmap));
	OpDragObject* drag_object = GetDragObject();
	OP_ASSERT(drag_object);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	drag_object->SetBitmapPoint(point);
	priv_data->SetBitmapRect(out);
	drag_object->SetBitmap(drag_bitmap);
	return OpStatus::OK;
}

/* static */
BOOL
DragDropManager::IsOperaSpecialMimeType(const uni_char* type)
{
	return uni_stri_eq(FILE_DATA_FORMAT, (type));
}

static OP_STATUS GetOrigin(URL& url, OpString& origin)
{
	if (url.Type() != URL_FILE)
	{
		RETURN_IF_ERROR(origin.Set(url.GetAttribute(URL::KProtocolName)));
		ServerName* name = url.GetServerName();
		if(!origin.IsEmpty() && name)
		{
			RETURN_IF_ERROR(origin.AppendFormat(UNI_L("://%s"), name->GetUniName().CStr()));
			if (url.GetAttribute(URL::KServerPort) != 0)
				RETURN_IF_ERROR(origin.AppendFormat(UNI_L(":%d"), url.GetAttribute(URL::KServerPort)));

			return OpStatus::OK;
		}
	}

	RETURN_IF_ERROR(origin.Set("null"));
	return OpStatus::OK;
}

/* static */
OP_STATUS
DragDropManager::SetOriginURL(OpDragObject* drag_object, URL& url)
{
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data)
		priv_data = OP_NEW(PrivateDragData, ());
	RETURN_OOM_IF_NULL(priv_data);
	drag_object->SetPrivateData(priv_data);
	RETURN_IF_ERROR(GetOrigin(url, priv_data->m_orign_url));
	return OpStatus::OK;
}

/* static */
const uni_char*
DragDropManager::GetOriginURL(OpDragObject* drag_object)
{
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data)
		return NULL;

	return priv_data->m_orign_url.CStr();
}

/* static */
OP_STATUS
DragDropManager::AddAllowedTargetURL(OpDragObject* drag_object, URL& url)
{
	OpString origin;
	RETURN_IF_ERROR(GetOrigin(url, origin));
	RETURN_IF_ERROR(AddAllowedTargetURL(drag_object, origin.CStr()));
	return OpStatus::OK;
}

/* static */
OP_STATUS
DragDropManager::AddAllowedTargetURL(OpDragObject* drag_object, const uni_char* url)
{
	drag_object->SetProtectedMode(TRUE);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data)
		priv_data = OP_NEW(PrivateDragData, ());
	RETURN_OOM_IF_NULL(priv_data);
	drag_object->SetPrivateData(priv_data);

	return priv_data->m_allowed_target_origin_urls.AppendFormat(UNI_L("%s\r\n"), url);
}

/* static */
OP_STATUS
DragDropManager::AllowAllURLs(OpDragObject* drag_object)
{
	drag_object->SetProtectedMode(FALSE);
	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data)
		priv_data = OP_NEW(PrivateDragData, ());
	RETURN_OOM_IF_NULL(priv_data);
	drag_object->SetPrivateData(priv_data);
	priv_data->m_allowed_target_origin_urls.Empty();
	return OpStatus::OK;
}


/* static */
BOOL
DragDropManager::IsURLAllowed(OpDragObject* drag_object, URL& url)
{
	if (!drag_object->IsInProtectedMode())
		return TRUE;

	PrivateDragData* priv_data = static_cast<PrivateDragData*>(drag_object->GetPrivateData());
	if (!priv_data)
		return TRUE;

	const uni_char* present = priv_data->m_allowed_target_origin_urls.CStr();
	if (!present) // If no URL added we treat all as allowed.
		return TRUE;

	TempBuffer uri_list;
	RETURN_VALUE_IF_ERROR(uri_list.Append(present), FALSE);
	uni_char* uri_list_data = uri_list.GetStorage();

	size_t url_len = uni_strcspn(uri_list_data, UNI_L("\r\n"));
	BOOL checked_all = FALSE;
	while (!checked_all)
	{
		uni_char *url_from_list = uri_list_data;
		if (uni_strlen(url_from_list) == url_len)
			checked_all = TRUE;
		else
			uri_list_data = uri_list_data + url_len + 2;

		url_from_list[url_len] = 0;

		URL target_url = g_url_api->GetURL(url_from_list, url.GetContextId());
		OpSecurityContext source_context(url);
		OpSecurityContext target_context(target_url);

		BOOL allowed = FALSE;
		OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_STANDARD, source_context, target_context, allowed);
		if (allowed)
			return TRUE;

		url_len = uni_strcspn(uri_list_data, UNI_L("\r\n"));
	}

	return FALSE;
}

void
DragDropManager::GetElementPaddingOffset(HTML_Element* elm, HTML_Document* doc, int x, int y, int& offset_x, int& offset_y)
{
	offset_x = 0;
	offset_y = 0;
	RECT rect;
	if (elm && doc && elm->GetBoxRect(doc->GetFramesDocument(), PADDING_BOX, rect))
	{
		offset_x = x - rect.left;
		offset_y = y - rect.top;
	}
}

void
DragDropManager::OnMouseDownDefaultAction(FramesDocument* doc, HTML_Element* elm, BOOL cancelled)
{
	if (m_mouse_down_status == SENT)
	{
		if (m_actions_recorder.HasRecordedDragActions())
		{
			// The drag has been put off. Start it if ONMOUSEDOWN hasn't been cancelled or cancel it otherwise.
			PrivateDragData* priv_data = static_cast<PrivateDragData*>(GetDragObject()->GetPrivateData());
			HTML_Element* src_elm = priv_data->GetSourceHtmlElement();
			HTML_Element* mouse_drag_elm = elm->GetDraggable(doc);
			if (mouse_drag_elm && src_elm != mouse_drag_elm) // We wait for ONMOUSEDOWN for the different element.
				return;

			m_mouse_down_status = NONE;
			if (cancelled)
				StopDrag(TRUE);
			else
			{
				Unblock();
				m_actions_recorder.SetMustReplayNextAction(TRUE);
				m_actions_recorder.ReplayRecordedDragActions();
			}
		}
		else
			m_mouse_down_status = cancelled ? CANCELLED : ALLOWED;
	}
}

void
DragDropManager::OnSendMouseDown()
{
	// Change the mouse down state only when it's relevant.
	if (!IsDragging() || (IsDragging() && !IsBlocked()) || m_mouse_down_status == DELAYED)
		m_mouse_down_status = SENT;
}

void
DragDropManager::OnRecordMouseDown()
{
	if (!IsDragging() || (IsDragging() && !IsBlocked()))
		m_mouse_down_status = DELAYED;
}
#endif // DRAG_SUPPORT
