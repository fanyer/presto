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
# include "modules/dragdrop/src/private_data.h"
# include "modules/dragdrop/src/visual_feedback.h"
# include "modules/pi/OpDragObject.h"
# include "modules/logdoc/htm_elm.h"
# include "modules/layout/layout_workplace.h"
# include "modules/layout/traverse/traverse.h"
# include "modules/doc/frm_doc.h"
# include "modules/dochand/fdelm.h"
# ifdef SVG_SUPPORT
#  include "modules/svg/SVGManager.h"
# endif // DOCUMENT_EDIT_SUPPORT

class VisualFeedbackElementSearchCustomizer : public ElementSearchCustomizer
{
public:
	VisualFeedbackElementSearchCustomizer(HTML_Element* elm)
		: ElementSearchCustomizer(FALSE, TRUE, FALSE)
		, m_search_elm(elm)
	 {}

	virtual BOOL AcceptElement(HTML_Element* elm, FramesDocument* doc)
	{
		return m_search_elm == elm || m_search_elm->IsAncestorOf(elm);
	}
private:
	HTML_Element* m_search_elm;
};

class BitmapAnchor
{
	OpBitmap* m_bitmap_to_clean;
	BOOL m_release_painter;
public:
	BitmapAnchor() : m_bitmap_to_clean(NULL), m_release_painter(FALSE) {}
	void Set(OpBitmap* bmp) { m_bitmap_to_clean = bmp; }
	void SetReleasePainter(BOOL release_painter) { m_release_painter = release_painter; }
	void Release() { Set(NULL); SetReleasePainter(FALSE); }
	~BitmapAnchor()
	{
		if (m_bitmap_to_clean)
		{
			if (m_release_painter)
				m_bitmap_to_clean->ReleasePainter();
			OP_DELETE(m_bitmap_to_clean);
		}
	}
};

// If there is no top frameset AffinePos to a top document is returned.
static void GetAffinePosToTopFramesetLeafHTMLDocument(FramesDocument* this_doc, AffinePos& pos, FramesDocument*& document_we_have_affine_pos_to)
{
	// Get transforms to the top document.
	FramesDocument* doc_iter = this_doc;
	while (doc_iter)
	{
		document_we_have_affine_pos_to = doc_iter;
		pos.PrependTranslation(-doc_iter->GetVisualDevice()->GetRenderingViewX(), -doc_iter->GetVisualDevice()->GetRenderingViewY());
		FramesDocument* next_doc = doc_iter->GetParentDoc();
		if (next_doc)
		{
			// Get this document's affine pos relative to its parent.
			FramesDocElm* elm = next_doc->GetFrmDocElmByDoc(doc_iter);
			if (elm)
			{
				if (doc_iter->GetTopDocument() != doc_iter->GetTopFramesDoc())
				{
					AffinePos elm_abs_pos = elm->GetAbsPos();
#ifdef CSS_TRANSFORMS
					// Matrix AffinePos(es) seem to have the left-side scrollbar offset taken into account where non-matrix ones don't.
					if (!elm_abs_pos.IsTransform())
#endif // CSS_TRANSFORMS
					{
						OpPoint inner_pos = doc_iter->GetVisualDevice()->GetInnerPosition();
						// If this document has a left side scrollbar it'll be taken into account in affine pos so it needs to be compensated.
						pos.PrependTranslation(inner_pos.x, inner_pos.y);
					}
					pos.Prepend(elm_abs_pos);
				}
				else
					break;
			}
		}
		doc_iter = next_doc;
	}
}

static OP_STATUS ScaleAddToBitmap(FramesDocument* doc, OpPainter* dst_painter, const OpRect& crect, const OpRect& cunion_rect)
{
	OP_ASSERT(dst_painter);

	OP_STATUS status = OpStatus::OK;
	OpRect rect = doc->GetVisualDevice()->ScaleToScreen(crect);
	OpPainter* painter = doc->GetVisualDevice()->GetView()->GetPainter(rect, TRUE);
	RETURN_OOM_IF_NULL(painter);

	OpPoint offset(rect.x - cunion_rect.x, rect.y - cunion_rect.y);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	offset = TO_DEVICE_PIXEL(doc->GetVisualDevice()->GetVPScale(), offset);
#endif // PIXEL_SCALE_RENDERING_SUPPORT

	OpBitmap* bitmap = painter->CreateBitmapFromBackground(rect);
	if (!bitmap)
	{
		status = OpStatus::ERR;
		goto painter_cleanup;
	}

	dst_painter->DrawBitmap(bitmap, offset);
	OP_DELETE(bitmap);

painter_cleanup:
	doc->GetVisualDevice()->GetView()->ReleasePainter(rect);
	return status;
}

/* static */
OP_STATUS
DragDropVisualFeedback::Create(HTML_Document* this_doc, const OpPointerHashTable<HTML_Element, PrivateDragData::DocElm>& elms, OpPoint& in_out_point, OpRect& out_rect, OpBitmap*& drag_bitmap)
{
	if (!this_doc)
		return OpStatus::ERR_NULL_POINTER;

	OpHashIterator* iter = const_cast<OpPointerHashTable<HTML_Element, PrivateDragData::DocElm>&>(elms).GetIterator();
	RETURN_OOM_IF_NULL(iter);
	OpAutoPtr<OpHashIterator> auto_iter(iter);

	OpPoint point = in_out_point;
	OpRect union_rect;
	const unsigned num_passes = 2; // One for getting union rectangle and one for getting all pieces on the bitmap.
	drag_bitmap = NULL;
	OpPainter* drag_painter = NULL;
	BitmapAnchor bmp_cleaner;
	BOOL do_blitting = FALSE;
	for (unsigned pass = 0; pass < num_passes; ++pass)
	{
		if (OpStatus::IsError(iter->First()))
			// No data to read.
			return OpStatus::OK;
		if (do_blitting)
		{
			union_rect = this_doc->GetVisualDevice()->ScaleToScreen(union_rect);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			const PixelScaler& scaler = this_doc->GetVisualDevice()->GetVPScale();
			int width_pixel = TO_DEVICE_PIXEL(scaler, union_rect.width);
			int height_pixel = TO_DEVICE_PIXEL(scaler, union_rect.height);
			RETURN_IF_ERROR(OpBitmap::Create(&drag_bitmap, width_pixel, height_pixel, TRUE, TRUE, 0, 0, TRUE));
#else
			RETURN_IF_ERROR(OpBitmap::Create(&drag_bitmap, union_rect.width, union_rect.height, TRUE, TRUE, 0, 0, TRUE));
#endif // PIXEL_SCALE_RENDERING_SUPPORT
			bmp_cleaner.Set(drag_bitmap);
			drag_painter = drag_bitmap->GetPainter();
			RETURN_OOM_IF_NULL(drag_painter);
			bmp_cleaner.SetReleasePainter(TRUE);
			UINT8 color[4] = {0};
			drag_bitmap->SetColor(color, TRUE, NULL);
		}
		do
		{
			RECT rect = {0, 0, 0, 0};
			OpRect op_rect;
			HTML_Element* elm = static_cast<HTML_Element*>(const_cast<void*>(iter->GetKey()));
			PrivateDragData::DocElm* doc_elm = static_cast<PrivateDragData::DocElm*>(iter->GetData());
			HTML_Document* html_doc = doc_elm->document;
			if (!html_doc)
				continue;
			FramesDocument* doc = html_doc->GetFramesDocument();
			// Get element's bounding box.
			if (elm->GetLayoutBox())
			{
				if (!elm->GetLayoutBox()->GetRect(this_doc->GetFramesDocument(), BOUNDING_BOX, rect))
					continue;
				op_rect.x = rect.left;
				op_rect.y = rect.top;
				op_rect.width = rect.right - rect.left;
				op_rect.height = rect.bottom - rect.top;
			}
#ifdef SVG_SUPPORT
			else if (elm->GetNsType() == NS_SVG)
			{
					g_svg_manager->GetElementRect(elm, op_rect);
			}
#endif // SVG_SUPPORT
			else
				continue;

			AffinePos pos(0,0);
			FramesDocument* document_we_have_affine_pos_to;
			GetAffinePosToTopFramesetLeafHTMLDocument(doc, pos, document_we_have_affine_pos_to);
			OP_ASSERT(document_we_have_affine_pos_to);
			OpRect top_rendering_viewport = document_we_have_affine_pos_to->GetVisualDevice()->GetRenderingViewport();
			// Calculate the visible document rectangle.
			OpRect visual_rect = doc->GetVisualDevice()->GetVisibleDocumentRect();
			pos.Apply(visual_rect);
			OpRect visible_area_limit(0, 0, top_rendering_viewport.width, top_rendering_viewport.height);
			visual_rect.IntersectWith(visible_area_limit);

			// Find visible part of a proper element(s).
			VisualFeedbackElementSearchCustomizer customizer(elm);
			DocumentElementCollection elements;
			if (!doc->GetLogicalDocument())
				continue;
			LayoutWorkplace* layout_workplace =  doc->GetLogicalDocument()->GetLayoutWorkplace();
			RETURN_IF_ERROR(layout_workplace->SearchForElements(customizer, op_rect, elements));

#ifdef SVG_SUPPORT
			if (elements.GetCount() == 0 && elm->GetNsType() == NS_SVG)
			{
				pos.Apply(op_rect);
				op_rect.IntersectWith(visual_rect);
				if (op_rect.IsEmpty())
					continue;
				if (do_blitting)
					RETURN_IF_ERROR(ScaleAddToBitmap(doc, drag_painter, op_rect, union_rect));
				else
					union_rect.UnionWith(op_rect);
			}
			else
#endif // SVG_SUPPORT
			{
				for (UINT32 iter = 0; iter < elements.GetCount(); iter++)
				{
					OpDocumentElement* element = elements.Get(iter);
					const DocumentElementRegion& element_region = element->GetRegion();
					unsigned int rect_count = static_cast<unsigned int>(element_region.GetCount());

					// Apply transforms and create bitmap from the element's region.
					for (unsigned int j = 0; j < rect_count; j++)
					{
						OpRect region = element_region.Get(j)->rect;
						if (element_region.Get(j)->affine_pos)
						{
							AffinePos full_transform = *element_region.Get(j)->affine_pos;
							full_transform.Prepend(pos);
							full_transform.Apply(region);
						}
						else
							pos.Apply(region);

						region.IntersectWith(visual_rect);
						if (region.IsEmpty())
							continue;
						if (do_blitting)
							RETURN_IF_ERROR(ScaleAddToBitmap(doc, drag_painter, region, union_rect));
						else
							union_rect.UnionWith(region);
					}
				}
			}
		} while (OpStatus::IsSuccess(iter->Next()));
		do_blitting = TRUE;
	}
	bmp_cleaner.Release();
	drag_bitmap->ReleasePainter();

	out_rect = union_rect;

	AffinePos this_pos(0, 0);
	FramesDocument* unused;
	GetAffinePosToTopFramesetLeafHTMLDocument(this_doc->GetFramesDocument(), this_pos, unused);
	this_pos.Apply(point);
	point = this_doc->GetVisualDevice()->ScaleToScreen(point);
	point.x -= union_rect.x;
	point.y -= union_rect.y;

	in_out_point = point;

	return OpStatus::OK;
}

/* static */
OP_STATUS
DragDropVisualFeedback::Create(HTML_Document* this_doc, const OpVector<OpRect>* rects, OpRect& in_out_rect, OpPoint& in_out_point, OpBitmap*& drag_bitmap)
{
	if (!this_doc || !rects)
		return OpStatus::ERR_NULL_POINTER;

	drag_bitmap = NULL;
	AffinePos pos(0, 0);
	FramesDocument* document_we_have_affine_pos_to;
	GetAffinePosToTopFramesetLeafHTMLDocument(this_doc->GetFramesDocument(), pos, document_we_have_affine_pos_to);

	OpPoint point = in_out_point;
	OpRect union_rect = in_out_rect;
	pos.Apply(union_rect);
	VisualDevice* vis_dev = this_doc->GetVisualDevice();

	OP_ASSERT(document_we_have_affine_pos_to);
	OpRect top_rendering_viewport = document_we_have_affine_pos_to->GetVisualDevice()->GetRenderingViewport();
	// Calculate the visible document rectangle.
	OpRect visual_rect = this_doc->GetVisualDevice()->GetVisibleDocumentRect();
	pos.Apply(visual_rect);
	OpRect visible_area_limit(0, 0, top_rendering_viewport.width, top_rendering_viewport.height);
	visual_rect.IntersectWith(visible_area_limit);

	BitmapAnchor bmp_cleaner;
	union_rect.IntersectWith(visual_rect);
	union_rect = vis_dev->ScaleToScreen(union_rect);
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
	const PixelScaler& scaler = this_doc->GetVisualDevice()->GetVPScale();
	int width_pixel = TO_DEVICE_PIXEL(scaler, union_rect.width);
	int height_pixel = TO_DEVICE_PIXEL(scaler, union_rect.height);
	RETURN_IF_ERROR(OpBitmap::Create(&drag_bitmap, width_pixel, height_pixel, TRUE, TRUE, 0, 0, TRUE));
#else
	RETURN_IF_ERROR(OpBitmap::Create(&drag_bitmap, union_rect.width, union_rect.height, TRUE, TRUE, 0, 0, TRUE));
#endif // PIXEL_SCALE_RENDERING_SUPPORT
	bmp_cleaner.Set(drag_bitmap);
	OpPainter* drag_painter = drag_bitmap->GetPainter();
	RETURN_OOM_IF_NULL(drag_painter);
	bmp_cleaner.SetReleasePainter(TRUE);
	UINT8 color[4] = {0};
	drag_bitmap->SetColor(color, TRUE, NULL);

	// Apply transforms and create bitmap from the rectangles.
	for (UINT32 iter = 0; iter < rects->GetCount(); iter++)
	{
		OpRect* region = rects->Get(iter);
		pos.Apply(*region);
		region->IntersectWith(visual_rect);
		if (region->IsEmpty())
			continue;
		RETURN_IF_ERROR(ScaleAddToBitmap(this_doc->GetFramesDocument(), drag_painter, *region, union_rect));
	}
	bmp_cleaner.Release();
	drag_bitmap->ReleasePainter();

	pos.Apply(point);
	point = vis_dev->ScaleToScreen(point);
	point.x -= union_rect.x;
	point.y -= union_rect.y;

	in_out_point = point;
	in_out_rect = union_rect;

	return OpStatus::OK;
}

#endif // DRAG_SUPPORT