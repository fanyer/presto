/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dochand/fdelm.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"
#include "modules/dochand/win.h"
#include "modules/display/prn_dev.h"
#include "modules/probetools/probepoints.h"
#include "modules/dom/domenvironment.h"
#include "modules/dom/domutils.h"
#include "modules/hardcore/mh/constant.h"
#include "modules/pi/ui/OpUiInfo.h"

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
# include "modules/widgets/OpScrollbar.h"
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/security_manager/include/security_manager.h"

void FramesDocElm::SetFrameScrolling(BYTE scrolling)
{
	frame_scrolling = scrolling;
}

void FramesDocElm::Reset(int type, int val, FramesDocElm* parent_frmset, HTML_Element *helm)
{
	frame_spacing = 0;
	SetFrameBorder(TRUE);
	SetFrameBorders(FALSE, FALSE, FALSE, FALSE);
	BOOL normal_frames_mode = !GetParentFramesDoc()->GetSmartFrames() && !GetParentFramesDoc()->GetFramesStacked();

	if (!helm)
		helm = GetHtmlElement();

	if (helm)
	{
		SetIsFrameset(helm->IsMatchingType(HE_FRAMESET, NS_HTML));
		SetFrameNoresize(IsFrameset() || helm->GetFrameNoresize());
		SetFrameScrolling(helm->GetFrameScrolling());
		/* Images should never have scrollbars.  If this is a pseudo iframe used
		   to implement for instance an SVG image in an IMG element, then make
		   sure we don't display any scrollbars in it. */
		if (helm->IsMatchingType(HE_IMG, NS_HTML))
			frame_scrolling = SCROLLING_NO;

		frame_margin_width = helm->GetFrameMarginWidth();
		frame_margin_height = helm->GetFrameMarginHeight();

		BOOL check_parent_frame_border = TRUE;

		short frame_spacing_attr = ATTR_FRAMESPACING;
		if (!helm->HasAttr(ATTR_FRAMESPACING))
		{
			if (helm->HasAttr(ATTR_BORDER))
				frame_spacing_attr = ATTR_BORDER;
			else
				frame_spacing_attr = 0;
		}

		if (frame_spacing_attr)
		{
			frame_spacing = helm->GetNumAttr(frame_spacing_attr);
			if (frame_spacing == 0)
			{
				SetFrameBorder(FALSE);
				check_parent_frame_border = FALSE;
			}
		}
		else if (parent_frmset)
			frame_spacing = parent_frmset->GetFrameSpacing();
		else
			frame_spacing = FRAME_BORDER_SIZE;

		if (helm->HasAttr(ATTR_FRAMEBORDER))
			SetFrameBorder(helm->GetBoolAttr(ATTR_FRAMEBORDER));
		else if (check_parent_frame_border && parent_frmset)
			SetFrameBorder(parent_frmset->GetFrameBorder());
	}
	else
	{
		SetIsFrameset(TRUE);
		SetFrameNoresize(TRUE);
		frame_scrolling = SCROLLING_AUTO;
		frame_margin_width = 0;
		frame_margin_height = 0;
		//frameset_border = 0;
	}

	pos.SetTranslation(0, 0);
	width = 0;
	height = 0;

	packed1.size_type = type;
	size_val = val;
	SetIsRow(TRUE);

	if (frm_dev)
		frm_dev->SetScrollType(normal_frames_mode ? (VisualDevice::ScrollType) frame_scrolling : VisualDevice::VD_SCROLLING_NO);
}

void FramesDocElm::CheckSpecialObject(HTML_Element* he)
{
	if (he && he->IsMatchingType(HE_OBJECT, NS_HTML))
	{
		const uni_char* class_id = he->GetStringAttr(ATTR_CLASSID);
		if (class_id && uni_stri_eq(class_id, "HTTP://WWW.OPERA.COM/ERA"))
		{
			for (HTML_Element* child = he->FirstChild(); child; child = child->Suc())
				if (child->IsMatchingType(HE_PARAM, NS_HTML))
				{
					const uni_char* param_name = child->GetPARAM_Name();
					if (param_name && uni_stri_eq(param_name, "RM"))
					{
						packed1.special_object = 1;
						packed1.special_object_layout_mode = LAYOUT_NORMAL;

						const uni_char* param_value = child->GetPARAM_Value();
						if (param_value)
						{
							if (uni_stri_eq(param_value, "1"))
								packed1.special_object_layout_mode = LAYOUT_SSR;
							else
								if (uni_stri_eq(param_value, "2"))
									packed1.special_object_layout_mode = LAYOUT_CSSR;
								else
									if (uni_stri_eq(param_value, "3"))
										packed1.special_object_layout_mode = LAYOUT_AMSR;
									else
										if (uni_stri_eq(param_value, "4"))
											packed1.special_object_layout_mode = LAYOUT_MSR;
#ifdef TV_RENDERING
										else
											if (uni_stri_eq(param_value, "5"))
												packed1.special_object_layout_mode = LAYOUT_TVR;
#endif // TV_RENDERING

						}
					}
				}
		}
	}
}

/*virtual*/	void FramesDocElm::FDEElmRef::OnDelete(FramesDocument *document)
{
	m_fde->DetachHtmlElement();

	if (m_fde->IsInlineFrame() && (!document || !document->IsPrintCopyBeingDeleted()))
	{
		m_fde->Out();
		FramesDocument *doc = m_fde->GetCurrentDoc();
		/* This function might be called from a script executing in
		the iframe or during reflow, in which case the iframe must be
		deleted later. */
		if (document && doc && (doc->IsESActive(TRUE) || doc->IsReflowing()))
			document->DeleteIFrame(m_fde);
		else
			OP_DELETE(m_fde);
	}
}

FramesDocElm::FramesDocElm(int id, int x, int y, int w, int h, FramesDocument* frm_doc, HTML_Element* he, VisualDevice* vd, int type, int val, BOOL inline_frm, FramesDocElm* parent_frmset, BOOL secondary) : m_helm(this)
{
	OP_ASSERT(frm_doc);
	OP_ASSERT(!he || he->GetInserted() != HE_INSERTED_BY_PARSE_AHEAD);

	packed1_init = 0;

	parent_frm_doc = frm_doc;
	parent_layout_input_ctx = NULL;

	packed1.is_inline = inline_frm;
	packed1.is_in_doc_coords = inline_frm;
	packed1.is_secondary = secondary;
	packed1.normal_row = TRUE;

	sub_win_id = id;

    doc_manager = NULL;
    frm_dev = NULL;

	if (he && !secondary)
	{
#ifdef _DEBUG
		OP_ASSERT(FramesDocElm::GetFrmDocElmByHTML(he) == NULL || !"The element we create a FramesDocElm for already has a FramesDocElm. Doh");
#endif // _DEBUG
		CheckSpecialObject(he);
	}

#ifdef _PRINT_SUPPORT_
	m_print_twin_elm = NULL;
#endif // _PRINT_SUPPORT_

	Reset(type, val, parent_frmset, he);

	/* FIXME: VIEWPORT_SUPPORT should make sure that the following coordinates
	   are scaled according to IsInDocCoords() (if anyone cares, that is). */

	pos.SetTranslation(x, y);

	width = w;
	height = h;

#ifndef MOUSELESS
	drag_val = 0;
	drag_offset = 0;
#endif // !MOUSELESS

	normal_width = w;
	normal_height = h;

	reinit_data = NULL;

	// turn off scrolling if this is a frame generated by an object element in mail window
	if (he && inline_frm && he->IsMatchingType(HE_OBJECT, NS_HTML) && vd && vd->GetWindow()->IsMailOrNewsfeedWindow())
		frame_scrolling = SCROLLING_NO;

	frame_index = FRAME_NOT_IN_ROOT;
}

FramesDocElm::~FramesDocElm()
{
	packed1.is_being_deleted = TRUE;

	FramesDocument* top_doc = parent_frm_doc->GetTopDocument();

	top_doc->FramesDocElmDeleted(this);

	parent_frm_doc->GetMessageHandler()->UnsetCallBack(parent_frm_doc, MSG_REINIT_FRAME, (MH_PARAM_1) this);

	RemoveReinitData();

	if (m_helm.GetElm())
		DetachHtmlElement();

	OP_DELETE(doc_manager);
	OP_DELETE(frm_dev);
}

AffinePos FramesDocElm::ToDocIfScreen(const AffinePos& val) const
{
	if (IsInDocCoords())
		return val;
	else
	{
		VisualDevice* vis_dev = doc_manager->GetWindow()->VisualDev();
		return vis_dev->ScaleToDoc(val);
	}
}

int FramesDocElm::ToDocIfScreen(int val, BOOL round_up) const
{
	if (IsInDocCoords())
		return val;
	else
	{
		VisualDevice* vis_dev = doc_manager->GetWindow()->VisualDev();
		int result = vis_dev->ScaleToDoc(val);

		if (round_up)
			result = vis_dev->ApplyScaleRoundingNearestUp(result);

		return result;
	}
}

int FramesDocElm::ToScreenIfScreen(int val, BOOL round_up) const
{
	if (IsInDocCoords())
		return val;
	else
	{
		VisualDevice* vis_dev = doc_manager->GetWindow()->VisualDev();
		int result = vis_dev->ScaleToScreen(val);

		if (round_up)
			result = vis_dev->ApplyScaleRoundingNearestUp(result);

		return result;
	}
}

AffinePos FramesDocElm::GetDocOrScreenAbsPos() const
{
	if (Parent() && Parent()->Parent())
	{
		AffinePos parent_pos = Parent()->GetDocOrScreenAbsPos();
		parent_pos.Append(pos);
		return parent_pos;
	}
	return pos;
}

int FramesDocElm::GetDocOrScreenAbsX() const
{
	OpPoint abs_pos = GetDocOrScreenAbsPos().GetTranslation();
	return abs_pos.x;
}

int FramesDocElm::GetDocOrScreenAbsY() const
{
	OpPoint abs_pos = GetDocOrScreenAbsPos().GetTranslation();
	return abs_pos.y;
}

AffinePos FramesDocElm::GetPos() const
{
	return ToDocIfScreen(pos);
}

int FramesDocElm::GetX() const
{
	return ToDocIfScreen(pos.GetTranslation().x, FALSE);
}

int FramesDocElm::GetY() const
{
	return ToDocIfScreen(pos.GetTranslation().y, FALSE);
}

void FramesDocElm::SetX(int x)
{
	OpPoint new_pos = pos.GetTranslation();
	new_pos.x = ToScreenIfScreen(x, FALSE);
	pos.SetTranslation(new_pos.x, new_pos.y);
}

void FramesDocElm::SetY(int y)
{
	OpPoint new_pos = pos.GetTranslation();
	new_pos.y = ToScreenIfScreen(y, FALSE);
	pos.SetTranslation(new_pos.x, new_pos.y);
}

OP_STATUS FramesDocElm::Init(HTML_Element *helm, VisualDevice* vd, OpView* clipview)
{
	/* NOTE: GetHtmlElement() will return NULL for the secondary objects
	   created per row in BuildTree() if both rows and cols attributes were
	   specified.  This is fully compatible with what this function currently
	   uses 'helm' for, but beware when changing this function! */

	if (helm)
	{
		OP_ASSERT(!packed1.is_secondary);
		SetHtmlElement(helm);
		RETURN_IF_ERROR(SetName(helm->GetStringAttr(ATTR_NAME)));
		RETURN_IF_MEMORY_ERROR(frame_id.Set(helm->GetId()));
	}

    doc_manager = OP_NEW(DocumentManager, (parent_frm_doc->GetWindow(), this, parent_frm_doc));

    if (!doc_manager || OpStatus::IsMemoryError(doc_manager->Construct()))
        return OpStatus::ERR_NO_MEMORY;

	if (vd && helm && helm->Type() != HE_FRAMESET
#ifdef _PRINT_SUPPORT_
		&& !vd->IsPrinter()
#endif
		)
	{
		VisualDevice::ScrollType scroll_type = (VisualDevice::ScrollType) frame_scrolling;

		if (GetParentFramesDoc()->GetFramesStacked())
			scroll_type = VisualDevice::VD_SCROLLING_NO;
		if (!IsInlineFrame() && GetParentFramesDoc()->GetSmartFrames())
			scroll_type = VisualDevice::VD_SCROLLING_NO;

		OP_STATUS result = vd->GetNewVisualDevice(frm_dev, doc_manager, scroll_type, vd->GetView());
		if (OpStatus::IsError(result))
			return result;

		doc_manager->SetVisualDevice(frm_dev);

		BOOL hide = FALSE;

# ifdef _PRINT_SUPPORT_
		// In preview mode we don't want more than one visible visual device,
		// otherwise the frames will not be shown, since we only use one
		// visual device for drawing even if there are frames in the document.
		if (vd->GetWindow()->GetPreviewMode()) //rg test with printing from preview.
			hide = TRUE;
# endif // _PRINT_SUPPORT_

		// Have to set IFRAMES as hidden by default because they are not hidden until
		// formatting is done. For empty IFRAMES with "visibility: hidden" formatting will
		// never be done.
		if (helm->IsMatchingType(HE_IFRAME, NS_HTML) && frm_dev)
			hide = TRUE;

		if (hide)
			Hide(FALSE);
	}
	else
		frm_dev = NULL;

    return OpStatus::OK;
}

#ifndef MOUSELESS
FramesDocElm* FramesDocElm::IsSeparator(int x, int y/*, int border*/)
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (doc)
	{
		if (doc->IsFrameDoc())
		{
			FramesDocElm* fde = doc->GetFrmDocRoot();

			if (fde)
				return fde->IsSeparator(x, y/*, fde->Border()*/);
		}

		return 0;
	}
	else
	{
		FramesDocElm* fde = FirstChild();
		while (fde)
		{
			if (IsRow())
			{
				if (y > fde->GetY() && y < fde->GetY() + fde->GetHeight())
					return fde->IsSeparator(x, y - fde->GetY());
			}
			else
				if (x > fde->GetX() && x < fde->GetX() + fde->GetWidth())
					return fde->IsSeparator(x - fde->GetX(), y);

			FramesDocElm* prev_fde = fde->Pred();

			if (prev_fde && prev_fde->CanResize() && fde->CanResize())
			{
				if (IsRow())
				{
					if (y <= fde->GetY() && y >= prev_fde->GetY() + prev_fde->GetHeight())
						return fde;
				}
				else
					if (x <= fde->GetX() && x >= prev_fde->GetX() + prev_fde->GetWidth())
						return fde;
			}

			fde = fde->Suc();
		}
	}

	return 0;
}

void FramesDocElm::StartMoveSeparator(int x, int y)
{
	FramesDocElm* prnt = Parent();
	if (!prnt)
		return;

	int border = 0;
	FramesDocElm* prev_fde = Pred();

	if (prnt->IsRow())
	{
		if (prev_fde)
			border = GetY() - prev_fde->GetY() - prev_fde->GetHeight();
		drag_val = GetAbsY() - border;
		drag_offset = drag_val - y;
	}
	else
	{
		if (prev_fde)
			border = GetX() - prev_fde->GetX() - prev_fde->GetWidth();
		drag_val = GetAbsX() - border;
		drag_offset = drag_val - x;
	}

	MoveSeparator(x, y);
}

void FramesDocElm::MoveSeparator(int x, int y)
{
	FramesDocElm* prnt = Parent();
	if (!prnt)
		return;

	int border = 0;
	FramesDocElm* fde = Pred();

	if (prnt->IsRow())
	{
		if (fde)
			border = GetY() - fde->GetY() - fde->GetHeight();

		int prnt_absy = prnt->GetAbsY();
		int use_y = y - prnt_absy + drag_offset;

		if (use_y < GetY())
		{
			// check min pos
			fde = Pred();
			if (fde)
			{
				int p_min_y = fde->GetMinHeight() + fde->GetY() + border;
				if (use_y < p_min_y)
					use_y = p_min_y;
			}
			else
				if (use_y < 0)
					use_y = 0;
		}
		else
			if (use_y > GetY())
			{
				// check max pos
				int max_y = GetMaxY() - border;

				if (use_y > max_y)
					use_y = max_y;
			}

		if (use_y != GetY())
		{
			drag_val = prnt_absy + use_y;
			Reformat(x, y + drag_offset);
		}
	}
	else
	{
		if (fde)
			border = GetX() - fde->GetX() - fde->GetWidth();

		int prnt_absx = prnt->GetAbsX();
		int use_x = x - prnt_absx + drag_offset;

		if (use_x < GetX())
		{
			// check min pos
			fde = Pred();
			if (fde)
			{
				int p_min_x = fde->GetMinWidth() + fde->GetX() + border;
				if (use_x < p_min_x)
					use_x = p_min_x;
			}
			else
				if (use_x < 0)
					use_x = 0;
		}
		else
			if (use_x > GetX())
			{
				// check max pos
				int max_x = GetMaxX() - border;

				if (use_x > max_x)
					use_x = max_x;
			}

		if (use_x != GetX())
		{
			drag_val = prnt_absx + use_x;
			Reformat(x + drag_offset, y);
		}
	}
}

void FramesDocElm::Reformat(int x, int y)
{
	FramesDocElm* prnt = Parent();
	if (!prnt)
		return;

	int border = 0;
	FramesDocElm* prev_fde = Pred();
	if (prev_fde)
	{
		if (prnt->IsRow())
			border = GetY() - prev_fde->GetY() - prev_fde->GetHeight();
		else
			border = GetX() - prev_fde->GetX() - prev_fde->GetWidth();
	}

	if (prnt->IsRow())
		SetY(drag_val - prnt->GetAbsY() + border);
	else
		SetX(drag_val - prnt->GetAbsX() + border);

	// set new width if changed
	BOOL reformat = FALSE;
	if (prnt->IsRow())
	{
		int new_height;

		if (Suc())
			new_height = Suc()->GetY() - GetY() - border;
		else
			new_height = prnt->GetHeight() - GetY();

		if (new_height != GetHeight())
		{
			reformat = TRUE;
			SetGeometry(GetPos(), GetWidth(), new_height);
			if (prev_fde)
				prev_fde->SetGeometry(prev_fde->GetPos(), prev_fde->GetWidth(), GetY() - prev_fde->GetY() - border);
		}
	}
	else
	{
		int new_width;

		if (Suc())
			new_width = Suc()->GetX() - GetX() - border;
		else
			new_width = prnt->GetWidth() - GetX();

		if (new_width != GetWidth())
		{
			reformat = TRUE;
			SetGeometry(GetPos(), new_width, GetHeight());
			if (prev_fde)
				prev_fde->SetGeometry(prev_fde->GetPos(), GetX() - prev_fde->GetX() - border, prev_fde->GetHeight());
		}
	}

	// propagate sizes and reformat this and previous frame if necessary
	if (reformat)
	{
		OP_STATUS status = OpStatus::OK;
		OpStatus::Ignore(status);

		if (prev_fde)
		{
			prev_fde->PropagateSizeChanges(FALSE, !prnt->IsRow(), FALSE, prnt->IsRow()/*, border*/);

			// Make sure that changes are propagated to hidden children
			if (prev_fde->GetCurrentDoc())
			{
				if (prev_fde->FormatFrames(prnt->IsRow(), !prnt->IsRow()) == OpStatus::ERR_NO_MEMORY)
					status = OpStatus::ERR_NO_MEMORY;
			}
		}

		PropagateSizeChanges(!prnt->IsRow(), FALSE, prnt->IsRow(), FALSE/*, border*/);

		FramesDocument* doc = GetCurrentDoc();

		// Make sure that changes are propagated to hidden children
		if (doc)
		{
			if (FormatFrames(prnt->IsRow(), !prnt->IsRow()) == OpStatus::ERR_NO_MEMORY)
				status = OpStatus::ERR_NO_MEMORY;
		}

		while (prnt->Parent())
			prnt = prnt->Parent();

		if (status == OpStatus::ERR_NO_MEMORY)
			GetWindow()->RaiseCondition(status);
	}
}

void FramesDocElm::EndMoveSeparator(int x, int y) //, int border)
{
	FramesDocElm* prnt = Parent();
	if (!prnt)
		return;

	Reformat(x, y);
}

BOOL FramesDocElm::CanResize()
{
	BOOL can_resize = TRUE;

	FramesDocElm* fde = FirstChild();
	if (fde)
	{
		while (can_resize && fde)
		{
			can_resize = fde->CanResize();
			fde = fde->Suc();
		}
	}
	else
		can_resize = !GetFrameNoresize();

	return can_resize;
}

int FramesDocElm::GetMaxX()
{
	int max_x = GetWidth();
	FramesDocElm* fde = FirstChild();
	if (IsRow())
	{
		while (fde)
		{
			int x = fde->GetMaxX();
			if (x < max_x)
				max_x = x;
			fde = fde->Suc();
		}
	}
	else
		if (fde)
			max_x = fde->GetMaxX();

	return max_x + GetX();
}

int FramesDocElm::GetMaxY()
{
	int max_y = GetHeight();
	FramesDocElm* fde = FirstChild();
	if (!IsRow())
	{
		while (fde)
		{
			int y = fde->GetMaxY();
			if (y < max_y)
				max_y = y;
			fde = fde->Suc();
		}
	}
	else
		if (fde)
			max_y = fde->GetMaxY();

	return max_y + GetY();
}

int FramesDocElm::GetMinWidth()
{
	int min_w = 0;
	FramesDocElm* fde = FirstChild();
	if (IsRow())
	{
		while (fde)
		{
			int w = fde->GetMinWidth();
			if (w > min_w)
				min_w = w;
			fde = fde->Suc();
		}
	}
	else
		if (fde)
			min_w = fde->GetMinWidth();

	return min_w;
}

int FramesDocElm::GetMinHeight()
{
	int min_h = 0;
	FramesDocElm* fde = FirstChild();
	if (!IsRow())
	{
		while (fde)
		{
			int h = fde->GetMinHeight();
			if (h > min_h)
				min_h = h;
			fde = fde->Suc();
		}
	}
	else
		if (fde)
			min_h = fde->GetMinHeight();

	return min_h;
}

#endif // !MOUSELESS

BOOL FramesDocElm::IsInDocCoords() const
{
	if (packed1.is_in_doc_coords)
		return TRUE;

	return doc_manager->GetWindow()->GetTrueZoom();
}

AffinePos FramesDocElm::GetAbsPos() const
{
	return ToDocIfScreen(GetDocOrScreenAbsPos());
}

int FramesDocElm::GetAbsX() const
{
	return ToDocIfScreen(GetDocOrScreenAbsX(), FALSE);
}

int FramesDocElm::GetAbsY() const
{
	return ToDocIfScreen(GetDocOrScreenAbsY(), FALSE);
}

int FramesDocElm::GetWidth() const
{
	return ToDocIfScreen(width, TRUE);
}

int FramesDocElm::GetHeight() const
{
	return ToDocIfScreen(height, TRUE);
}

int FramesDocElm::GetNormalWidth() const
{
	return ToDocIfScreen(normal_width, TRUE);
}

int FramesDocElm::GetNormalHeight() const
{
	return ToDocIfScreen(normal_height, TRUE);
}

void FramesDocElm::SetPosition(const AffinePos& doc_pos)
{
	if (IsInDocCoords())
	{
		pos = doc_pos;
	}
	else
	{
		VisualDevice* vis_dev = GetWindow()->VisualDev();

		pos = vis_dev->ScaleToScreen(doc_pos);
	}

	UpdateGeometry();
}

void FramesDocElm::SetSize(int w, int h)
{
	if (IsInDocCoords())
	{
		width = w;
		height = h;
		normal_width = w;
		normal_height = h;
	}
	else
	{
		VisualDevice* vis_dev = GetWindow()->VisualDev();

		width = vis_dev->ScaleToScreen(w);
		height = vis_dev->ScaleToScreen(h);
		normal_width = vis_dev->ScaleToScreen(w);
		normal_height = vis_dev->ScaleToScreen(h);
	}

	if (FramesDocument* doc = GetCurrentDoc())
		doc->RecalculateLayoutViewSize(TRUE); // FIXME: assuming "user action"

	UpdateGeometry();
}

void FramesDocElm::SetGeometry(const AffinePos& doc_pos, int w, int h)
{
	if (IsInDocCoords())
	{
		pos = doc_pos;
		width = w;
		height = h;
		normal_width = w;
		normal_height = h;
	}
	else
	{
		VisualDevice* vis_dev = GetWindow()->VisualDev();

		pos = vis_dev->ScaleToScreen(doc_pos);
		width = vis_dev->ScaleToScreen(w);
		height = vis_dev->ScaleToScreen(h);
		normal_width = vis_dev->ScaleToScreen(w);
		normal_height = vis_dev->ScaleToScreen(h);
	}

	if (FramesDocument* doc = GetCurrentDoc())
		doc->RecalculateLayoutViewSize(TRUE); // FIXME: assuming "user action"

	UpdateGeometry();
}

void FramesDocElm::ForceHeight(int h)
{
	if (IsInDocCoords())
		height = h;
	else
	{
		VisualDevice* vis_dev = GetWindow()->VisualDev();

		height = vis_dev->ScaleToScreen(h);
	}

	UpdateGeometry();
}

OP_STATUS FramesDocElm::ShowFrames()
{
	if (frm_dev)
	{
		BOOL update_scrollbar = !frm_dev->GetVisible();

		RETURN_IF_ERROR(Show());

#ifdef SVG_SUPPORT
		SVGCheckWantMouseEvents();
#endif // SVG_SUPPORT

		if (update_scrollbar)
			frm_dev->UpdateScrollbars();
	}

	for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
		RETURN_IF_ERROR(fde->ShowFrames());

	return OpStatus::OK;
}

void FramesDocElm::HideFrames()
{
	Hide(FALSE);

	for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
		fde->HideFrames();
}

void FramesDocElm::UpdateGeometry()
{
	if (frm_dev)
	{
		VisualDevice* parent_vd = GetParentFramesDoc()->GetVisualDevice();

		OP_ASSERT(parent_vd);
		if (!parent_vd)
			return;

		int view_x = parent_vd->ScaleToScreen(parent_vd->GetRenderingViewX());
		int view_y = parent_vd->ScaleToScreen(parent_vd->GetRenderingViewY());

		int old_rendering_width = frm_dev->GetRenderingViewWidth();
		int old_rendering_height = frm_dev->GetRenderingViewHeight();

		AffinePos doc_or_screen_pos = GetDocOrScreenAbsPos();

		AffinePos screen_pos;
		int width_screen_coords;
		int height_screen_coords;

		if (IsInDocCoords())
		{
			screen_pos = frm_dev->ScaleToScreen(doc_or_screen_pos);

#ifdef CSS_TRANSFORMS
			// If page is zoomed, we don't want to apply the
			// zoom/scalefactor to the width/height, but to the
			// transform.
			if (screen_pos.IsTransform())
			{
				width_screen_coords = width;
				height_screen_coords = height;
			}
			else
#endif // CSS_TRANSFORMS
			{
				width_screen_coords = frm_dev->ScaleToScreen(width);
				height_screen_coords = frm_dev->ScaleToScreen(height);
			}
		}
		else
		{
			screen_pos = doc_or_screen_pos;
			width_screen_coords = width;
			height_screen_coords = height;
		}

		AffinePos view_ctm(-view_x, -view_y);
		screen_pos.Prepend(view_ctm);

		frm_dev->SetRenderingViewGeometryScreenCoords(screen_pos,
													  width_screen_coords,
													  height_screen_coords);

		if (FramesDocument* doc = GetCurrentDoc())
			if (old_rendering_width != frm_dev->GetRenderingViewWidth() || old_rendering_height != frm_dev->GetRenderingViewHeight())
				if (g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::ShowActiveFrame, doc->GetHostName()))
				{
					DocumentManager* top_doc_man = doc_manager->GetWindow()->DocManager();
					if (FramesDocument* top_frames_doc = top_doc_man->GetCurrentDoc())
						if (FramesDocument* active_doc = top_frames_doc->GetActiveSubDoc())
							if (active_doc == doc)
								// Make sure that the frame border is updated.

								frm_dev->UpdateAll();
				}
	}
}

FramesDocElm* FramesDocElm::Parent() const
{
	if (packed1.is_deleted)
		return NULL;

	return (FramesDocElm *) Tree::Parent();
}

#ifndef MOUSELESS
void FramesDocElm::PropagateSizeChanges(BOOL left, BOOL right, BOOL top, BOOL bottom/*, int border*/)
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (!doc)
	{
		FramesDocElm* fde, *prev_fde;
		if (IsRow())
		{
			if (bottom)
			{
				// update last child height
				fde = LastChild();

				if (fde)
					if (fde->GetHeight() != GetHeight() - fde->GetY())
					{
						fde->SetGeometry(fde->GetPos(), fde->GetWidth(), GetHeight() - fde->GetY());
						fde->PropagateSizeChanges(left, right, top, bottom);
					}
			}
			else
				if (top)
				{
					// update all child y pos
					prev_fde = 0;

					for (fde = LastChild(); fde; fde = fde->Pred())
					{
						int old_h = fde->GetHeight();
						int new_h = old_h;
						if (!fde->Pred())
						{
							// update first child y pos and height
							fde->SetY(0);
							if (prev_fde)
								new_h = prev_fde->GetY() - GetFrameSpacing();
							else
								new_h = GetHeight();
						}
						else
							if (prev_fde)
								fde->SetY(prev_fde->GetY() - fde->GetHeight() - GetFrameSpacing());
							else
								fde->SetY(GetHeight() - fde->GetHeight());

						if (old_h != new_h)
						{
							fde->SetSize(fde->GetWidth(), new_h);
							fde->PropagateSizeChanges(left, right, top, bottom);
						}

						prev_fde = fde;
					}
				}
				else
					if (left || right)
						for (fde = FirstChild(); fde; fde = fde->Suc())
						{
							if (fde->GetWidth() == GetWidth())
								break; // all widths equal

							// update all child width
							fde->SetSize(GetWidth(), fde->GetHeight());
							fde->PropagateSizeChanges(left, right, top, bottom);
						}
		}
		else
		{
			if (right)
			{
				// update last child width
				fde = LastChild();
				if (fde)
					if (fde->GetWidth() != GetWidth() - fde->GetX())
					{
						fde->SetSize(GetWidth() - fde->GetX(), fde->GetHeight());
						fde->PropagateSizeChanges(left, right, top, bottom);
					}
			}
			else
				if (left)
				{
					// update all child x pos
					prev_fde = 0;

					for (fde = LastChild(); fde; fde = fde->Pred())
					{
						int old_w = fde->GetWidth();
						int new_w = old_w;

						if (!fde->Pred())
						{
							// update first child x pos and width
							fde->SetX(0);
							if (prev_fde)
								new_w = prev_fde->GetX() - GetFrameSpacing();
							else
								new_w = GetWidth();
						}
						else
							if (prev_fde)
								fde->SetX(prev_fde->GetX() - fde->GetWidth() - GetFrameSpacing());
							else
								fde->SetX(GetWidth() - fde->GetWidth());

						if (old_w != new_w)
						{
							fde->SetSize(new_w, fde->GetHeight());
							fde->PropagateSizeChanges(left, right, top, bottom);
						}

						prev_fde = fde;
					}
				}
				else
					if (top || bottom)
						for (fde = FirstChild(); fde; fde = fde->Suc())
						{
							if (fde->GetHeight() == GetHeight())
								break; // all heights equal

							// update all child height
							fde->SetSize(fde->GetWidth(), GetHeight());
							fde->PropagateSizeChanges(left, right, top, bottom);
						}
		}
	}
}
#endif // !MOUSELESS

DocListElm* FramesDocElm::GetHistoryElmAt(int pos, BOOL forward)
{
    if (doc_manager)
    {
        DocListElm *tmp_dle = doc_manager->CurrentDocListElm();

        if (tmp_dle)
        {
            if (forward)
            {
                while (tmp_dle && tmp_dle->Number() < pos)
                    tmp_dle = tmp_dle->Suc();
            }
            else // forward
            {
                while (tmp_dle && tmp_dle->Number() > pos)
                    tmp_dle = tmp_dle->Pred();
            }

            if (tmp_dle)
            {
                if (tmp_dle->Number() == pos)
                    return tmp_dle;
                else
                    return tmp_dle->Doc()->GetHistoryElmAt(pos, forward);
            }
        }

        FramesDocElm *tmp_fde = FirstChild();

        while (tmp_fde)
        {
            tmp_dle = tmp_fde->GetHistoryElmAt(pos, forward);

            if (tmp_dle)
                return tmp_dle;

            tmp_fde = tmp_fde->Suc();
        }
    }

    return NULL;
}

void FramesDocElm::CheckHistory(int decrement, int& minhist, int& maxhist)
{
	doc_manager->CheckHistory(decrement, minhist, maxhist);

	FramesDocElm *fde = FirstChild();
	while (fde)
	{
		fde->CheckHistory(decrement, minhist, maxhist);
		fde = fde->Suc();
	}
}

void FramesDocElm::RemoveFromHistory(int from)
{
	doc_manager->RemoveFromHistory(from);

	FramesDocElm *fde = FirstChild();
	while (fde)
	{
		fde->RemoveFromHistory(from);
		fde = fde->Suc();
	}
}

void FramesDocElm::RemoveUptoHistory(int to)
{
	doc_manager->RemoveUptoHistory(to);

	FramesDocElm *fde = FirstChild();
	while (fde)
	{
		fde->RemoveUptoHistory(to);
		fde = fde->Suc();
	}
}

void FramesDocElm::RemoveElementFromHistory(int pos)
{
	doc_manager->RemoveElementFromHistory(pos);

	FramesDocElm *fde = FirstChild();
	while (fde)
	{
		fde->RemoveElementFromHistory(pos);
		fde = fde->Suc();
	}
}

HTML_Element *FramesDocElm::GetHtmlElement()
{
	OP_PROBE4(OP_PROBE_FDELM_GETHTMLELEMENT);

	if (packed1.is_secondary)
		return Parent() ? Parent()->GetHtmlElement() : NULL;
	return m_helm.GetElm();
}

void FramesDocElm::SetHtmlElement(HTML_Element *elm)
{
	OP_ASSERT(m_helm.GetElm() == NULL);
	m_helm.SetElm(elm);
}

void FramesDocElm::DetachHtmlElement()
{
	OP_ASSERT(m_helm.GetElm());
	m_helm.Reset();
}

const uni_char* FramesDocElm::GetName()
{
	return name.CStr();
}

OP_STATUS FramesDocElm::SetName(const uni_char* str)
{
	return name.Set(str);
}

OP_STATUS FramesDocElm::Undisplay(BOOL will_be_destroyed)
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();
    OP_STATUS status = OpStatus::OK;

	if (doc)
  		status = doc->Undisplay(will_be_destroyed);
	else
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
            if (fde->Undisplay(will_be_destroyed) == OpStatus::ERR_NO_MEMORY)
                status = OpStatus::ERR_NO_MEMORY;

    return status;
}

OP_STATUS FramesDocElm::Show()
{
	if (frm_dev)
		RETURN_IF_ERROR(frm_dev->Show(parent_frm_doc->GetVisualDevice()->GetView()));

	return OpStatus::OK;
}

void FramesDocElm::Hide(BOOL free_views /*= FALSE*/)
{
	if (frm_dev)
		frm_dev->Hide(free_views);
}

void FramesDocElm::CheckFrameEdges(BOOL& outer_top, BOOL& outer_left, BOOL& outer_right, BOOL& outer_bottom, BOOL top_checked, BOOL left_checked, BOOL right_checked, BOOL bottom_checked)
{
	if (!IsFrameset())
	{
		outer_top = TRUE;
		outer_left = TRUE;
		outer_right = TRUE;
		outer_bottom = TRUE;
	}

	FramesDocElm* prnt = Parent();

	if (prnt)
	{
		if (prnt->IsRow())
		{
			if (!top_checked && Pred())
			{
				outer_top = FALSE;
				top_checked = TRUE;
			}

			if (!bottom_checked && Suc())
			{
				outer_bottom = FALSE;
				bottom_checked = TRUE;
			}
		}
		else
		{
			if (!left_checked && Pred())
			{
				outer_left = FALSE;
				left_checked = TRUE;
			}

			if (!right_checked && Suc())
			{
				outer_right = FALSE;
				right_checked = TRUE;
			}
		}

		prnt->CheckFrameEdges(outer_top, outer_left, outer_right, outer_bottom, top_checked, left_checked, right_checked, bottom_checked);
	}
}

OP_STATUS FramesDocElm::ReactivateDocument()
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (doc)
		return doc->ReactivateDocument();

	OP_STATUS stat = OpStatus::OK;

	for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
        if (fde->ReactivateDocument() == OpStatus::ERR_NO_MEMORY)
            stat = OpStatus::ERR_NO_MEMORY;

    return stat;
}

BOOL FramesDocElm::IsLoaded(BOOL inlines_loaded)
{
	if (!doc_manager->IsCurrentDocLoaded(inlines_loaded))
		return FALSE;

	for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
		if (!fde->IsLoaded(inlines_loaded))
			return FALSE;

	return TRUE;
}

void FramesDocElm::SetInlinesUsed(BOOL used)
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (doc)
  		doc->SetInlinesUsed(used);

	for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
		fde->SetInlinesUsed(used);
}

void FramesDocElm::Free(BOOL layout_only/*=FALSE*/, FramesDocument::FreeImportance free_importance /*= FramesDocument::UNIMPORTANT*/)
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (doc)
		doc->Free(layout_only, free_importance);
	else
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
			fde->Free(layout_only, free_importance);
}

OP_BOOLEAN FramesDocElm::CheckSource()
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

    OP_BOOLEAN stat = OpStatus::OK;

	if (doc)
		stat = doc->CheckSource();
	else
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
        {
            stat = fde->CheckSource();

            if (OpStatus::IsError(stat))
                break;
        }

    return stat;
}

void FramesDocElm::StopLoading(BOOL format, BOOL abort/*=FALSE*/)
{
	doc_manager->StopLoading(format, FALSE, abort);

	for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
		fde->StopLoading(format, abort);
}

OP_STATUS FramesDocElm::SetMode(BOOL win_show_images, BOOL win_load_images, CSSMODE win_css_mode, CheckExpiryType check_expiry)
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (doc)
	{
		if (!frm_dev->GetView())
		{
			RETURN_IF_ERROR(Show());
		}

		doc_manager->SetCheckExpiryType(check_expiry);
  		return doc->SetMode(win_show_images, win_load_images, win_css_mode, check_expiry);
	}
	else
	{
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
			if (fde->SetMode(win_show_images, win_load_images, win_css_mode, check_expiry) == OpStatus::ERR_NO_MEMORY)
				return OpStatus::ERR_NO_MEMORY;

		return OpStatus::OK;
	}
}

OP_STATUS FramesDocElm::SetAsCurrentDoc(BOOL state, BOOL visible_if_current)
{
	if (!state || GetCurrentDoc() && !GetCurrentDoc()->IsCurrentDoc())
		SetOnLoadCalled(FALSE);

    OP_STATUS stat = doc_manager->UpdateCallbacks(state);

	FramesDocElm* fde = FirstChild();
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (!fde || doc)
	{
		if (doc && !state)
		{
			stat = doc->Undisplay();

			if (doc->SetAsCurrentDoc(state, visible_if_current) == OpStatus::ERR_NO_MEMORY)
                stat = OpStatus::ERR_NO_MEMORY;
		}

        if (OpStatus::IsError(stat))
            return stat;

		if (frm_dev)
		{
			if (doc && doc->IsFrameDoc() || (IsInlineFrame() || !doc || !doc->IsFrameDoc()) && (!state || visible_if_current))
			{
				if (state)
					RETURN_IF_ERROR(Show());
				else
					Hide(TRUE);
			}
		}

		if (doc && state)
			stat = doc->SetAsCurrentDoc(state, visible_if_current);
	}
	else
	{
		// check if any windows even if no document
		if (frm_dev && (!IsInlineFrame() || !state))
			Hide(TRUE);

		for (; fde; fde = fde->Suc())
			if (OpStatus::IsMemoryError(fde->SetAsCurrentDoc(state, visible_if_current)))
				stat = OpStatus::ERR_NO_MEMORY;
	}

    return stat;
}

void FramesDocElm::ReloadIfModified()
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (!IsFrameset())
	{
		if (doc)
		{
			URL doc_url = doc->GetURL();
			DocumentReferrer doc_ref_url = doc->GetRefURL();

			doc_manager->OpenURL(doc_url, doc_ref_url, TRUE, TRUE, FALSE, FALSE, NotEnteredByUser, FALSE, TRUE);
		}
		else
		{
			// FIXME: OOM
			LoadFrames();
		}
	}
	else
	{
		OP_ASSERT(!doc);

		FramesDocElm* fde = FirstChild();

		if (fde)
		{
			for (; fde; fde = fde->Suc())
				fde->ReloadIfModified();
		}
		else
			FramesDocument::CheckOnLoad(NULL, this);
	}
}

int ScaleToMSR(int val, FramesDocument* doc, BOOL row)
{
	if (row)
		return (val * doc->GetLayoutViewHeight()) / 600;
	else
		return (val * doc->GetLayoutViewWidth()) / 800;
}

FramesDocElm* FindFramesDocElm(Head* existing_frames, HTML_Element* he)
{
	for (FramesDocElm* fde = (FramesDocElm*) existing_frames->Last(); fde; fde = (FramesDocElm*) fde->Pred())
		if (fde->GetHtmlElement() == he)
		{
			fde->Out();
			return fde;
		}

	return NULL;
}

OP_STATUS FramesDocElm::BuildTree(FramesDocument* top_frm_doc, Head* existing_frames)
{
	OP_STATUS stat = DocStatus::DOC_CANNOT_FORMAT;
	OpStatus::Ignore(stat);

	if (HTML_Element *helm = GetHtmlElement())
	{
		if (helm->IsMatchingType(HE_FRAME, NS_HTML))
			return DocStatus::DOC_FORMATTING;
		else if (helm->IsMatchingType(HE_FRAMESET, NS_HTML))
		{
			const uni_char* row_spec = helm->GetFramesetRowspec();
			const uni_char* col_spec = helm->GetFramesetColspec();

			VisualDevice* parent_vd = GetParentFramesDoc()->GetVisualDevice();

			if (row_spec && col_spec)
			{
				//not a tree

				// 22/04/97: changed this to take rows first since Netscape 3.01 always takes rows first
				int first_count = helm->GetFramesetRowCount();
				int next_count = helm->GetFramesetColCount();

				SetIsRow(TRUE);

				SetNormalRow(TRUE);

				int first_child_count = 0;
				HTML_Element* he = helm->FirstChildActual();
				while (he && first_child_count < first_count)
				{
				  	if (he->GetNsType() != NS_HTML || (he->Type() != HE_FRAMESET && he->Type() != HE_FRAME))
				  	{
				  		he = he->SucActual();
				  		continue;
				  	}

				  	int val = 1;
				  	int type = FRAMESET_RELATIVE_SIZED;
					helm->GetFramesetRow(first_child_count++, val, type);

					FramesDocElm* felm = NULL;

					if (existing_frames)
						felm = FindFramesDocElm(existing_frames, helm);

					if (felm)
						felm->Reset(type, val, this, helm);
					else
					{
						felm = OP_NEW(FramesDocElm, (top_frm_doc->GetNewSubWinId(), 0, 0, 0, 0, GetParentFramesDoc(), helm, parent_vd, type, val, FALSE, this, TRUE));

						if (!felm)
							return DocStatus::ERR_NO_MEMORY;

						if (OpStatus::IsError(felm->Init(NULL, parent_vd, NULL)))
						{
							OP_DELETE((felm));
							felm = NULL;
							return DocStatus::ERR_NO_MEMORY;
						}
					}

					felm->Under(this);
					felm->SetIsRow(FALSE);

					felm->SetNormalRow(FALSE);
					if (GetParentFramesDoc()->GetFramesStacked())
						felm->SetIsRow(TRUE);

					int next_child_count = 0;

					while (he && next_child_count < next_count)
					{
					  	if (he->GetNsType() == NS_HTML && (he->Type() == HE_FRAMESET || he->Type() == HE_FRAME))
					  	{
						  	int val = 1;
						  	int type = FRAMESET_RELATIVE_SIZED;
							helm->GetFramesetCol(next_child_count++, val, type);

							FramesDocElm* new_felm = NULL;

							if (existing_frames)
								new_felm = FindFramesDocElm(existing_frames, he);

							if (new_felm)
								new_felm->Reset(type, val, felm, he);
							else
							{
								new_felm = OP_NEW(FramesDocElm, (top_frm_doc->GetNewSubWinId(), 0, 0, 0, 0, GetParentFramesDoc(), he, parent_vd, type, val, FALSE, felm));

								if (!new_felm)
									return DocStatus::ERR_NO_MEMORY;

								if (OpStatus::IsError(new_felm->Init(he, parent_vd, NULL)))
								{
									OP_DELETE((new_felm));
									return DocStatus::ERR_NO_MEMORY;
								}
							}

							new_felm->Under(felm);
							stat = new_felm->BuildTree(top_frm_doc, existing_frames);
						}

						he = he->SucActual();
					}
				}
			}
			else
			{
				SetIsRow(helm->GetFramesetRowspec() != 0);

				if (!IsRow())
					SetIsRow(helm->GetFramesetColspec() == 0); // default row ???

				SetNormalRow(IsRow());

				int child_count = 0;

				for (HTML_Element* he = helm->FirstChildActual(); he; he = he->SucActual())
				  	if (he->GetNsType() == NS_HTML && (he->Type() == HE_FRAMESET || he->Type() == HE_FRAME))
				  	{
					  	int val = 100;
					  	int type = FRAMESET_PERCENTAGE_SIZED;
					  	BOOL has_spec = FALSE;

					  	if (IsRow())
					  		has_spec = helm->GetFramesetRow(child_count++, val, type);
					  	else
					  		has_spec = helm->GetFramesetCol(child_count++, val, type);

						// ignore children outside the rowspec/colspec if specified
						if (has_spec || (IsRow() && !row_spec && child_count == 1) || (!IsRow() && !col_spec && child_count == 1))
						{
							FramesDocElm* felm = NULL;

							if (existing_frames)
								felm = FindFramesDocElm(existing_frames, he);

							if (felm)
								felm->Reset(type, val, this, he);
							else
							{
								felm = OP_NEW(FramesDocElm, (top_frm_doc->GetNewSubWinId(), 0, 0, 0, 0, GetParentFramesDoc(), he, parent_vd, type, val, FALSE, this));

								if (!felm)
									return DocStatus::ERR_NO_MEMORY;

								if (OpStatus::IsError(felm->Init(he, parent_vd, NULL)))
								{
									OP_DELETE((felm));
									return DocStatus::ERR_NO_MEMORY;
								}
							}

							felm->Under(this);
							stat = felm->BuildTree(top_frm_doc, existing_frames);
						}
					}
			}

			// If we have an empty frameset, call onload because it won't be triggered by any child frames.
			if (!this->FirstChild())
				return FramesDocument::CheckOnLoad(NULL, this);

			if (GetParentFramesDoc()->GetFramesStacked())
				SetIsRow(TRUE);

			// check percentage
			int psum = 0;

			for (FramesDocElm* felm = FirstChild(); felm; felm = felm->Suc())
				if (felm->GetSizeType() == FRAMESET_PERCENTAGE_SIZED)
					psum += felm->size_val;

			if (psum > 100)
				for (FramesDocElm* felm = FirstChild(); felm; felm = felm->Suc())
					if (felm->GetSizeType() == FRAMESET_PERCENTAGE_SIZED)
						felm->size_val = (int)(((int)felm->size_val*100) / psum);
		}
	}

	return stat;
}

void FramesDocElm::SetRootSize(int width, int height)
{
	OP_ASSERT(!frm_dev);
	OP_ASSERT(!Parent());

	this->width = width;
	this->height = height;
	this->normal_width = width;
	this->normal_height = height;
}

OP_STATUS FramesDocElm::FormatFrames(BOOL format_rows, BOOL format_cols)
{
	OP_STATUS stat = DocStatus::DOC_CANNOT_FORMAT;

	OpStatus::Ignore(stat);

	FramesDocElm* inherit_fde = Parent();

	if (!inherit_fde)
		inherit_fde = parent_frm_doc->GetDocManager()->GetFrame();

	if (inherit_fde && !packed1.is_in_doc_coords)
		packed1.is_in_doc_coords = inherit_fde->packed1.is_in_doc_coords;

	if (!IsFrameset())
	{
		if (GetFrameBorder())
		{
			if (IsInlineFrame())
				SetFrameBorders(TRUE, TRUE, TRUE, TRUE);
			else
			{
				BOOL outer_top, outer_left, outer_right, outer_bottom;
				CheckFrameEdges(outer_top, outer_left, outer_right, outer_bottom);
				SetFrameBorders(!outer_top, !outer_left, !outer_right, !outer_bottom);
			}
		}

		return DocStatus::DOC_FORMATTING;
	}
	else
	{
		FramesDocElm* felm;
		int size_used = 0;
		BOOL use_doc_coords = IsInDocCoords();

		if (!Parent())
			if (FramesDocElm* frame = parent_frm_doc->GetDocManager()->GetFrame())
			{
				// Root frameset in a sub-document. Set available size.

				width = frame->width;
				height = frame->height;
				normal_width = frame->normal_width;
				normal_height = frame->normal_height;
			}

		int avail_size = (IsRow()) ? height : width;

		if (parent_frm_doc->GetSmartFrames())
			avail_size = (IsRow()) ? normal_height : normal_width;

		int abs_size_used = 0;
		int extra_space = 0;
		int child_count = 0;
		int abs_child_count = 0;
		int rel_childs = 0;
		int per_val = 0;
		VisualDevice* top_vis_dev = doc_manager->GetWindow()->VisualDev();
		FramesDocument* top_doc = doc_manager->GetWindow()->GetCurrentDoc();

		BOOL scale_to_msr = GetLayoutMode() != LAYOUT_NORMAL && GetParentFramesDoc()->GetSmartFrames();

		for (felm = FirstChild(); felm; felm = felm->Suc())
		{
			int val = felm->size_val;
			int type = felm->GetSizeType();

			if (scale_to_msr && type == FRAMESET_ABSOLUTE_SIZED)
				val = ScaleToMSR(val, top_doc, IsRow());

			if ((IsRow() && format_rows) || (!IsRow() && format_cols))
			{
				if (type == FRAMESET_ABSOLUTE_SIZED)
				{
					if (!use_doc_coords)
						val = top_vis_dev->ScaleToScreen(val);
					size_used += val;
					abs_size_used += val;
					abs_child_count++;
				}
				else
					if (type == FRAMESET_PERCENTAGE_SIZED)
					{
						int use_size = int((avail_size * val) / 100);

						size_used += use_size;
						per_val += val;
					}
					else
						rel_childs++;
			}

			if (!GetParentFramesDoc()->GetSmartFrames())
				if (felm->Pred())
					avail_size -= GetFrameSpacing();

			child_count++;
		}

		if (avail_size < 0)
			avail_size = 0;

		int abs_size_avail = abs_size_used;

		if (abs_size_avail > avail_size || child_count == abs_child_count)
			abs_size_avail = avail_size;

		int per_size_avail = ((avail_size * per_val) / 100);
		if (per_size_avail > avail_size - abs_size_avail)
			per_size_avail = avail_size - abs_size_avail;

		int per_count = child_count - abs_child_count - rel_childs;
		BOOL per2rel = per_count && !rel_childs;
		if (per2rel)
			rel_childs = per_count;

		if (per_count && per_size_avail/per_count < RELATIVE_MIN_SIZE)
		{
			if (abs_child_count)
			{
				int take_from_abs = RELATIVE_MIN_SIZE * per_count;

				if (take_from_abs > abs_size_avail/2)
					take_from_abs = abs_size_avail / 2;

				abs_size_avail -= take_from_abs;
				per_size_avail += take_from_abs;
			}
		}

		if (!rel_childs)
		{
			extra_space = avail_size - size_used;

			if (extra_space < 0)
				extra_space = 0;
		}

		BOOL add_to_abs_sized = (avail_size < abs_size_used);

		size_used = 0;

		int i = 0;
		int rel_child_count = 0;
		int rel_child_val = 0;

		FramesDocElm* prev_felm = NULL;

		for (felm = FirstChild(); felm; felm = felm->Suc())
		{
			int val = felm->size_val;
			int type = felm->GetSizeType();

			if (scale_to_msr && type == FRAMESET_ABSOLUTE_SIZED)
				val = ScaleToMSR(val, top_doc, IsRow());

			int new_width = (IsRow()) ? width : felm->width;
			int new_height = (IsRow()) ? felm->height : height;

			if ((IsRow() && format_rows) || (!IsRow() && format_cols))
			{
				OpPoint new_pos = felm->pos.GetTranslation();

				if (prev_felm)
				{
					int frm_ex = 0;

					if (!GetParentFramesDoc()->GetSmartFrames())
						frm_ex += GetFrameSpacing();

					OpPoint prev_pos = prev_felm->pos.GetTranslation();
					if (IsRow())
						new_pos.y = prev_pos.y + prev_felm->height + frm_ex;
					else
						new_pos.x = prev_pos.x + prev_felm->width + frm_ex;
				}

				felm->pos.SetTranslation(new_pos.x, new_pos.y);

				if (!felm->Suc() && !rel_childs)
				{
					// use rest of space
					int use_size = (int) (avail_size - size_used);

					size_used += use_size;

					if (IsRow())
						new_height = use_size;
					else
						new_width = use_size;
				}
				else
				{
					int add_space = 0;

					if (extra_space && (add_to_abs_sized || type == FRAMESET_PERCENTAGE_SIZED))
					{
						int ccount = child_count-i-rel_childs+rel_child_count;

						if (!add_to_abs_sized)
							ccount = child_count-abs_child_count-i-rel_childs+rel_child_count;

						if (ccount)
							add_space = (int) (extra_space/ccount);

						extra_space -= add_space;
					}

					if (type == FRAMESET_ABSOLUTE_SIZED)
					{
						int use_size = val;

						if (abs_size_used)
							use_size = (int)(((int)val * abs_size_avail) / abs_size_used);

						use_size += add_space;

						if (use_size < 0)
						{
							if (extra_space)
								extra_space += use_size;
							use_size = 0;
						}

						if (!use_doc_coords)
							use_size = top_vis_dev->ScaleToScreen(use_size);

						size_used += use_size;

						if (IsRow())
							new_height = use_size;
						else
							new_width = use_size;
					}
					else if (type == FRAMESET_PERCENTAGE_SIZED && !per2rel)
					{
						int use_size = 0;

						if (per_val)
							use_size = (int) ((per_size_avail*val) / per_val);

						use_size += add_space;

						if (use_size < 0)
						{
							if (extra_space)
								extra_space += use_size;

							use_size = 0;
						}

						size_used += use_size;

						if (IsRow())
							new_height = use_size;
						else
							new_width = use_size;
					}
					else
					{
						rel_child_count++;
						rel_child_val += val;
					}
				}
			}

			// we set the size without redrawing to avoid drawing in wrong position
			felm->width = new_width;
			felm->height = new_height;

			felm->normal_width = new_width;
			felm->normal_height = new_height;

			prev_felm = felm;
			i++;
		}

		if ((IsRow() && format_rows) || (!IsRow() && format_cols))
		{
			// set size on relatively sized frames
			if (rel_child_count && ((!IsRow() && size_used < width) || (IsRow() && size_used < height)))
			{
				if (!rel_child_val)
					rel_child_val = 1;

				i = 1;
				float rel_size = ((float)avail_size - size_used) / rel_child_val;

				prev_felm = NULL;

				for (felm = FirstChild(); felm; felm = felm->Suc())
				{
					OpPoint new_pos = felm->pos.GetTranslation();

					if (prev_felm)
					{
						int frm_ex = 0;

						if (!GetParentFramesDoc()->GetSmartFrames())
							frm_ex += GetFrameSpacing();

						OpPoint prev_pos = prev_felm->pos.GetTranslation();
						if (IsRow())
							new_pos.y = prev_pos.y + prev_felm->height + frm_ex;
						else
							new_pos.x = prev_pos.x + prev_felm->width + frm_ex;
					}

					felm->pos.SetTranslation(new_pos.x, new_pos.y);

					int new_width = felm->width;
					int new_height = felm->height;

					if (felm->GetSizeType() == FRAMESET_RELATIVE_SIZED || (per2rel && felm->GetSizeType() == FRAMESET_PERCENTAGE_SIZED))
					{
						int child_size;

						if (i == rel_childs)
							child_size = (int) (avail_size - size_used); // use rest of space
						else
							child_size = (int) (rel_size * felm->size_val);

						if (IsRow())
							new_height = child_size;
						else
							new_width = child_size;

						size_used += child_size;
						i++;
					}

					// we set the size without redrawing to avoid drawing in wrong position
					felm->width = new_width;
					felm->height = new_height;

					felm->normal_width = new_width;
					felm->normal_height = new_height;

					prev_felm = felm;
				}
			}
		}

		// format each frame

		for (felm = FirstChild(); felm; felm = felm->Suc())
		{
			if (parent_frm_doc->GetFramesStacked())
				/* Document height (root layout box height) never gets smaller than
				   layout viewport height, and in frame stacking mode, we want frames to
				   take up as little space as possible. Therefore, set layout viewport
				   height to 0. */

				felm->normal_height = 0;

			stat = felm->FormatFrames(format_rows, format_cols);

			if (FramesDocument* doc = felm->GetCurrentDoc())
				doc->RecalculateLayoutViewSize(TRUE); // FIXME: assuming "user action". Is that correct?
		}
	}

	if (stat == DocStatus::DOC_CANNOT_FORMAT)
		SetOnLoadCalled(TRUE);

	return stat;
}

OP_STATUS FramesDocElm::LoadFrames(ES_Thread *origin_thread)
{
	if (IsFrameset())
	{
		for (FramesDocElm* felm = FirstChild(); felm; felm = felm->Suc())
			RETURN_IF_ERROR(felm->LoadFrames());

		if (!FirstChild())
			FramesDocument::CheckOnLoad(NULL, this);
	}
	else
	{
		URL frm_url;

		URL about_blank_url = g_url_api->GetURL("about:blank");

		if (HTML_Element *helm = GetHtmlElement())
		{
			URL* url_ptr = NULL;

			if (helm->GetNsType() == NS_HTML)
			{
				short attr = ATTR_SRC;

				if (helm->Type() == HE_OBJECT)
					attr = ATTR_DATA;

				url_ptr = helm->GetUrlAttr(attr, NS_IDX_HTML, GetParentFramesDoc() ? GetParentFramesDoc()->GetLogicalDocument() : NULL);
			}
#ifdef SVG_SUPPORT
			else if (g_svg_manager->AllowFrameForElement(helm))
			{
				URL* root_url = &GetParentFramesDoc()->GetURL();
				url_ptr = g_svg_manager->GetXLinkURL(helm, root_url);
			}
#endif // SVG_SUPPORT

			if (url_ptr && !url_ptr->IsEmpty())
				frm_url = *url_ptr;
			else
				frm_url = about_blank_url;
		}
		else
		{
			// if helm==NULL then this is a frame with freed content and current doc has the original url
			FramesDocument* doc = doc_manager->GetCurrentDoc();

			if (doc)
				frm_url = doc->GetURL();
		}

#ifdef _MIME_SUPPORT_
		// check if frm_url equals to any ancestor of this frame
		FramesDocument* ancestor_doc = GetParentFramesDoc();
#endif // _MIME_SUPPORT_

		BOOL bypass_frame_access_check = FALSE;
#ifdef _MIME_SUPPORT_
		if (ancestor_doc->GetSuppress(frm_url.Type()) ||
			DocumentManager::IsSpecialURL(frm_url))
		{
			if (ancestor_doc->MakeFrameSuppressUrl(frm_url) == OpStatus::ERR_NO_MEMORY)
				return OpStatus::OK;
			bypass_frame_access_check = TRUE;
		}
#endif // _MIME_SUPPORT_

#if DOCHAND_MAX_FRAMES_ON_PAGE > 0
		if (!IsContentAllowed() && (!(frm_url == about_blank_url)))
			frm_url = about_blank_url;
#endif // DOCHAND_MAX_FRAMES_ON_PAGE > 0

		if (!doc_manager->GetCurrentDoc() &&
			 (frm_url == about_blank_url || frm_url.Type() == URL_JAVASCRIPT)
#ifdef ON_DEMAND_PLUGIN
			 // Allow frame to load a placeholder even when plugin element has no URL to load.
			 && !GetHtmlElement()->IsPluginPlaceholder()
#endif // ON_DEMAND_PLUGIN
			)
		{
			RETURN_IF_ERROR(doc_manager->CreateInitialEmptyDocument(frm_url.Type() == URL_JAVASCRIPT, frm_url == about_blank_url && origin_thread != NULL));
			if (frm_url == about_blank_url)
			{
				/* If a thread initiated the loading of about:blank, then add an
				   async onload check. Once it is signalled and the 'origin_thread'
				   hasn't caused the document to be navigated away, only then will
				   an about:blank onload be sent. */
				if (origin_thread && doc_manager->GetCurrentDoc())
					RETURN_IF_ERROR(doc_manager->GetCurrentDoc()->ScheduleAsyncOnloadCheck(origin_thread, TRUE));

				/* Nothing more to do. */
				return OpStatus::OK;
			}
		}

		DocumentReferrer frm_ref_url(GetParentFramesDoc());
		DocumentManager *parent_docman = GetParentFramesDoc()->GetDocManager();

		BOOL check_if_expired = parent_docman->GetCheckExpiryType() != CHECK_EXPIRY_NEVER;
		BOOL reload = parent_docman->GetReload();
		BOOL user_initiated = FALSE;
		BOOL create_doc_now = GetParentFramesDoc()->IsGeneratedByOpera() && frm_url.Status(TRUE) == URL_LOADED && !GetParentFramesDoc()->IsReflowing();
		BOOL is_walking_in_history = parent_docman->IsWalkingInHistory();

		doc_manager->SetUseHistoryNumber(parent_docman->CurrentDocListElm()->Number());
		doc_manager->SetReload(reload);
		doc_manager->SetReloadFlags(parent_docman->GetReloadDocument(), parent_docman->GetConditionallyRequestDocument(), parent_docman->GetReloadInlines(), parent_docman->GetConditionallyRequestInlines());

		DocumentManager::OpenURLOptions options;
		options.user_initiated = user_initiated;
		options.create_doc_now = create_doc_now;
		options.is_walking_in_history = is_walking_in_history;
		options.bypass_url_access_check = bypass_frame_access_check;
		options.origin_thread = origin_thread;
		options.from_html_attribute = TRUE;
		doc_manager->OpenURL(frm_url, frm_ref_url, check_if_expired, reload, options);

		// In case we created a new document for a javascript url and the script load
		// command failed completely, we need to clean up the about:blank state
		if (frm_url.Type() == URL_JAVASCRIPT && doc_manager->GetLoadStatus() == NOT_LOADING)
		{
			doc_manager->GetCurrentDoc()->SetWaitForJavascriptURL(FALSE);
			doc_manager->GetCurrentDoc()->CheckFinishDocument();
		}
	}

	return OpStatus::OK;
}

OP_STATUS FramesDocElm::FormatDocs()
{
	OP_STATUS stat = DocStatus::DOC_OK;
	OpStatus::Ignore(stat);

	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (doc)
		stat = doc->Reflow(TRUE);
	else
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
			if (fde->FormatDocs() == OpStatus::ERR_NO_MEMORY)
				stat = OpStatus::ERR_NO_MEMORY;

	return stat;
}

#ifdef _PRINT_SUPPORT_
OP_STATUS FramesDocElm::CopyFrames(FramesDocElm* new_frm)
{
	new_frm->SetIsRow(IsRow());
	FramesDocElm* fde = FirstChild();
	while (fde)
	{
		OP_ASSERT(!fde->m_helm.GetElm() || fde->m_print_twin_elm || !"It need to have a print twin to connect it to");
		FramesDocElm* new_fde = OP_NEW(FramesDocElm, (fde->GetSubWinId(), 0, 0, 0, 0, new_frm->GetParentFramesDoc(), fde->m_print_twin_elm, NULL, fde->GetSizeType(), fde->GetSizeVal(), fde->IsInlineFrame(), new_frm));
        if( ! new_fde )
            return OpStatus::ERR_NO_MEMORY;

        if (OpStatus::IsError(new_fde->Init(fde->m_print_twin_elm, NULL, NULL)))
        {
            OP_DELETE((new_fde));
            return OpStatus::ERR_NO_MEMORY;
        }

		new_fde->Under(new_frm);
		RETURN_IF_ERROR(fde->CopyFrames(new_fde));
		fde = fde->Suc();
	}

	// FIXME: potentially incorrectly assuming print preview here (but there's nothing new about that):
	RETURN_IF_MEMORY_ERROR(new_frm->GetDocManager()->SetPrintMode(TRUE, this, TRUE));

	return OpStatus::OK;
}

OP_STATUS FramesDocElm::CreatePrintLayoutAllPages(PrintDevice* pd, FramesDocElm* print_root)
{
	FramesDocument* doc = GetCurrentDoc();

	if (doc)
	{
		int page_ypos = 0;
		FramesDocElm* prev_fde = print_root->LastChild();

		if (prev_fde)
			page_ypos = prev_fde->GetAbsY() + prev_fde->GetHeight();

		FramesDocElm* fde;
		HTML_Element* html_element;
		if (IsInlineFrame())
		{
			OP_ASSERT(m_print_twin_elm); // Without this the cloned FramesDocElm won't be found anyway
			if (!m_print_twin_elm)
			{
				return OpStatus::OK;
			}
			OP_ASSERT(GetFrmDocElmByHTML(m_print_twin_elm) == NULL);
			html_element = m_print_twin_elm;
			fde = OP_NEW(FramesDocElm, (GetSubWinId(), GetAbsX(), GetAbsY(), pd->GetRenderingViewWidth(), pd->GetRenderingViewHeight(),
										print_root->GetParentFramesDoc(), m_print_twin_elm, pd, FRAMESET_ABSOLUTE_SIZED, 0, TRUE, print_root));
		}
		else
		{
			html_element = NULL;
			fde = OP_NEW(FramesDocElm, (GetSubWinId(), 0, page_ypos, pd->GetRenderingViewWidth(), pd->GetRenderingViewHeight(),
										print_root->GetParentFramesDoc(), NULL, pd, FRAMESET_ABSOLUTE_SIZED, 0, FALSE, print_root));
		}

        if (!fde)
            return OpStatus::ERR_NO_MEMORY;

        if (OpStatus::IsError(fde->Init(html_element, pd, NULL)))
        {
            OP_DELETE((fde));
            fde = NULL;
            return OpStatus::ERR_NO_MEMORY;
        }

#ifdef _DEBUG
		if (IsInlineFrame())
		{
			OP_ASSERT(fde->GetHtmlElement() == m_print_twin_elm);
		}
#endif // _DEBUG

		fde->Under(print_root);

		if (fde->GetDocManager()->SetPrintMode(TRUE, this, TRUE) == OpStatus::ERR_NO_MEMORY)
        {
			fde->Out();
			OP_DELETE(fde);
            return OpStatus::ERR_NO_MEMORY;
        }

		int w = fde->GetWidth();
		int h = fde->GetHeight();

		FramesDocument* printdoc = fde->GetDocManager()->GetPrintDoc();

		if (printdoc)
		{
			w = printdoc->Width();
			h = printdoc->Height();
		}

		AffinePos fde_abs_pos = fde->GetAbsPos();
		fde->SetGeometry(fde_abs_pos, w, h);
		print_root->SetGeometry(print_root->GetAbsPos(), w, fde_abs_pos.GetTranslation().y + h);
	}
	else
	{
		FramesDocElm* fde = FirstChild();

		while (fde)
		{
			RETURN_IF_ERROR(fde->CreatePrintLayoutAllPages(pd, print_root));
			fde = fde->Suc();
		}
	}

	return OpStatus::OK;
}
#endif // _PRINT_SUPPORT_

/* static */
FramesDocElm* FramesDocElm::GetFrmDocElmByHTML(HTML_Element * elm)
{
	if (elm)
	{
		ElementRef *ref = elm->GetFirstReferenceOfType(ElementRef::FRAMESDOCELM);
		if (ref)
			return static_cast<FDEElmRef*>(ref)->GetFramesDocElm();
	}

	return NULL;
}

#ifdef _PRINT_SUPPORT_
void FramesDocElm::Display(VisualDevice* vd, const RECT& rect)
{
	Window* window = GetDocManager()->GetWindow();
	FramesDocument* doc = doc_manager->GetPrintDoc();

	if (doc)
	{
		int abs_x = GetAbsX();
		int abs_y = GetAbsY();

		if (IsInlineFrame())
		{
			OpRect frame_clip_rect(0, 0, GetWidth(), GetHeight());
			frame_clip_rect = vd->ScaleToDoc(frame_clip_rect);

			vd->BeginClipping(frame_clip_rect);

			VDState vd_state = vd->PushState();

			doc->Display(rect, vd);

			vd->PopState(vd_state);
			vd->EndClipping();
		}
		else if (window->GetFramesPrintType() == PRINT_AS_SCREEN)
		{
			vd->TranslateView(-abs_x, -abs_y);

			OpRect frame_clip_rect(0, 0, GetWidth(), GetHeight());

			vd->BeginClipping(frame_clip_rect);

			if (vd->IsPrinter() && !IsInlineFrame())
				doc->PrintPage((PrintDevice*) vd, 1, FALSE);
			else
				doc->Display(rect, vd);

			vd->EndClipping();

			vd->TranslateView(abs_x, abs_y);
		}
		else
		{
			vd->TranslateView(0, -abs_y);

			doc->Display(rect, vd);

			vd->TranslateView(0, abs_y);
		}
	}
	else
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
			fde->Display(vd, rect);
}

#endif // _PRINT_SUPPORT_

void FramesDocElm::DisplayBorder(VisualDevice* vd)
{
	UINT32 gray = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON);
	UINT32 lgray = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_LIGHT);
	UINT32 dgray = g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON_DARK);

	BOOL scale_100 = !IsInDocCoords();
	UINT32 old_scale = 0;

	if (scale_100)
		old_scale = vd->SetTemporaryScale(100);

	if (frame_spacing)
	{
		// If a frame is not visible, clear that area so we don't show parts of old documents there.
		FramesDocElm* fde = FirstChild();
		if ((!fde || (GetVisualDevice() && !GetVisualDevice()->GetVisible())) && !GetWindow()->IsBackgroundTransparent())
		{
			// Not sure why this case happens yet but it does when nesting framesets. /emil
			vd->SetBgColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_DOCUMENT_BACKGROUND));
			vd->DrawBgColor(OpRect(GetAbsX(), GetAbsY(), GetWidth(), GetHeight()));
		}

		// Paint background between frames
		vd->SetBgColor(g_op_ui_info->GetSystemColor(OP_SYSTEM_COLOR_BUTTON));

		while (fde && fde->Suc())
		{
			FramesDocElm* fde_suc = fde->Suc();

			if (IsRow())
			{
				int y = fde->GetY() + fde->GetHeight();
				vd->DrawBgColor(OpRect(GetX(), GetY() + y, GetWidth(), fde_suc->GetY() - y));
			}
			else
			{
				int x = fde->GetX() + fde->GetWidth();
				vd->DrawBgColor(OpRect(GetX() + x, GetY(), fde_suc->GetX() - x, GetHeight()));
			}

			fde = fde_suc;
		}

		UINT32 black = OP_RGB(0, 0, 0);
		if (GetFrameTopBorder())
		{
			vd->SetColor32(dgray);
			vd->DrawLine(OpPoint(GetAbsX() - 2, GetAbsY() - 2), GetWidth() + 4, TRUE, 1);
			vd->SetColor32(black);
			vd->DrawLine(OpPoint(GetAbsX() - 1, GetAbsY() - 1), GetWidth() + 2, TRUE, 1);
		}
		if (GetFrameBottomBorder())
		{
			vd->SetColor32(gray);
			vd->DrawLine(OpPoint(GetAbsX(), GetAbsY() + GetHeight()), GetWidth(), TRUE, 1);
			vd->SetColor32(lgray);
			vd->DrawLine(OpPoint(GetAbsX() - 1, GetAbsY() + GetHeight() + 1), GetWidth() + 2, TRUE, 1);
		}
		if (GetFrameLeftBorder())
		{
			vd->SetColor32(dgray);
			vd->DrawLine(OpPoint(GetAbsX() - 2, GetAbsY() - 2), GetHeight() + 4, FALSE, 1);
			vd->SetColor32(black);
			vd->DrawLine(OpPoint(GetAbsX() - 1, GetAbsY() - 1), GetHeight() + 2, FALSE, 1);
		}
		if (GetFrameRightBorder())
		{
			vd->SetColor32(gray);
			vd->DrawLine(OpPoint(GetAbsX() + GetWidth(), GetAbsY() - 1), GetHeight() + 2, FALSE, 1);
			vd->SetColor32(lgray);
			vd->DrawLine(OpPoint(GetAbsX() + GetWidth() + 1, GetAbsY() - 2), GetHeight() + 4, FALSE, 1);
		}
	}

	if (scale_100)
		vd->SetTemporaryScale(old_scale);

	for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
		fde->DisplayBorder(vd);
}

#ifdef _PRINT_SUPPORT_

OP_DOC_STATUS FramesDocElm::PrintPage(PrintDevice* pd, int page_num, BOOL selected_only)
{
	FramesDocElm* fde = FirstChild();
	if (fde)
	{
		while (fde)
		{
			OP_STATUS dstat = fde->PrintPage(pd, page_num, selected_only);
			if (dstat == DocStatus::DOC_PAGE_OUT_OF_RANGE)
				page_num -= fde->CountPages();
			else
				return dstat;

			fde = fde->Suc();
		}
		return DocStatus::DOC_CANNOT_PRINT;
	}
	else
	{
		DocumentManager *old_doc_man = pd->GetDocumentManager();
		pd->SetDocumentManager(doc_manager);
		OP_DOC_STATUS ret = doc_manager->PrintPage(pd, page_num, selected_only);
		pd->SetDocumentManager(old_doc_man);
		return ret;
	}
}
#endif // _PRINT_SUPPORT_

int FramesDocElm::CountPages()
{
#ifdef _PRINT_SUPPORT_
	FramesDocument* doc = doc_manager->GetPrintDoc();
	if (doc)
		return doc->CountPages();
	else
#endif // _PRINT_SUPPORT_
	{
		int page_count = 0;
		FramesDocElm* fde = FirstChild();
		while (fde)
		{
			page_count += fde->CountPages();
			fde = fde->Suc();
		}
		return page_count;
	}
}

LayoutMode FramesDocElm::GetLayoutMode() const
{
	if (packed1.special_object)
		return (LayoutMode) packed1.special_object_layout_mode;
	else
		return parent_frm_doc->GetLayoutMode();
}

void FramesDocElm::CheckERA_LayoutMode()
{
	FramesDocument* doc = GetCurrentDoc();
	if (doc)
	{
		if (doc && doc->IsFrameDoc())
			doc->CheckERA_LayoutMode();
	}
	else
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
			fde->CheckERA_LayoutMode();
}

void FramesDocElm::UpdateFrameMargins(HTML_Element *he)
{
	if (he)
	{
		frame_margin_width = he->GetFrameMarginWidth();
		frame_margin_height = he->GetFrameMarginHeight();

		if (frame_margin_width == 0) // 0 isn't allowed according to spec and brakes updating (Bug 96454)
			frame_margin_width = 1;
		if (frame_margin_height == 0)
			frame_margin_height = 1;
	}
}

void FramesDocElm::SetCurrentHistoryPos(int n, BOOL parent_doc_changed, BOOL is_user_initiated)
{
	FramesDocument* doc = GetCurrentDoc();
	if (doc)
		doc_manager->SetCurrentHistoryPos(n, parent_doc_changed, is_user_initiated);
	else
	{
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
			fde->SetCurrentHistoryPos(n, parent_doc_changed, is_user_initiated);
	}
}

FramesDocument*	FramesDocElm::GetTopFramesDoc()
{
	return GetParentFramesDoc()->GetTopFramesDoc();
}

void FramesDocElm::CheckSmartFrames(BOOL on)
{
	OP_ASSERT(!IsInlineFrame());

	pos.SetTranslation(0, 0);
	width = 0;
	height = 0;

	if (frm_dev)
		frm_dev->SetScrollType(on ? VisualDevice::VD_SCROLLING_NO : (VisualDevice::ScrollType) frame_scrolling);

	FramesDocument* doc = GetCurrentDoc();
	if (doc && doc->IsFrameDoc())
		doc->CheckSmartFrames(on);

	for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
		fde->CheckSmartFrames(on);
}

void FramesDocElm::ExpandFrameSize(int inc_width, int inc_height)
{
	OP_ASSERT(inc_width >= 0);
	OP_ASSERT(inc_height >= 0);

	width += inc_width;
	height += inc_height;

	FramesDocElm* fde = FirstChild();

	if (fde)
	{
		int inc_used = 0;
		int child_count = 0;

		for (; fde; fde = fde->Suc())
			child_count++;

		int child_width_inc = inc_width;
		int child_height_inc = inc_height;

		if (IsRow())
			child_height_inc = (inc_height + child_count - 1) / child_count;
		else
			child_width_inc = (inc_width + child_count - 1) / child_count;

		for (fde = FirstChild(); fde; fde = fde->Suc())
		{
			if (IsRow())
				fde->pos.AppendTranslation(0, inc_used);
			else
				fde->pos.AppendTranslation(inc_used, 0);

			fde->ExpandFrameSize(child_width_inc, child_height_inc);

			if (IsRow())
			{
				inc_used += child_height_inc;
				if (inc_used == inc_height)
					child_height_inc = 0;
			}
			else
			{
				inc_used += child_width_inc;
				if (inc_used == inc_width)
					child_width_inc = 0;
			}
		}
	}
	else
	{
		FramesDocument* doc = GetCurrentDoc();
		if (doc && doc->IsFrameDoc())
			doc->ExpandFrameSize(inc_width, inc_height);
	}
}

void FramesDocElm::CalculateFrameSizes(int frames_policy)
{
	BOOL use_doc_coords = IsInDocCoords();

	FramesDocElm* fde = FirstChild();

	if (fde)
	{
		int next_y = 0;

		if (frames_policy == FRAMES_POLICY_FRAME_STACKING)
		{
			for (; fde; fde = fde->Suc())
			{
				OpPoint fde_pos = fde->pos.GetTranslation();
				fde_pos.y = next_y;
				fde->pos.SetTranslation(fde_pos.x, fde_pos.y);
				fde->width = width;
				fde->CalculateFrameSizes(frames_policy);
				next_y += fde->height;
			}

			height = next_y;
		}
		else if (frames_policy == FRAMES_POLICY_SMART_FRAMES)
		{
			int next_x = 0;

			width = normal_width;
			height = normal_height;

			for (; fde; fde = fde->Suc())
			{
				OpPoint fde_pos = fde->pos.GetTranslation();
				if (IsRow())
					fde_pos.y = next_y;
				else
					fde_pos.x = next_x;

				fde->pos.SetTranslation(fde_pos.x, fde_pos.y);

				fde->CalculateFrameSizes(frames_policy);

				if (IsRow())
				{
					if (width < fde->width)
						width = fde->width;

					next_y += fde->height;
				}
				else
				{
					if (height < fde->height)
						height = fde->height;

					next_x += fde->width;
				}
			}

			if (IsRow())
				height = next_y;
			else
				width = next_x;

			for (fde = FirstChild(); fde; fde = fde->Suc())
			{
				if (IsRow())
				{
					if (width > fde->width)
						fde->ExpandFrameSize(width - fde->width, 0);
				}
				else
				{
					if (height > fde->height)
						fde->ExpandFrameSize(0, height - fde->height);
				}
			}
		}
	}
	else
	{

		OP_ASSERT(!IsInlineFrame());

		FramesDocument* doc = GetCurrentDoc();
		if (doc)
		{
			if (doc && doc->IsFrameDoc())
				doc->CalculateFrameSizes();

			if (frames_policy == FRAMES_POLICY_FRAME_STACKING)
			{
				if (!HasExplicitZeroSize())
					height = use_doc_coords ? doc->Height() : doc->GetVisualDevice()->ScaleToScreen(doc->Height());
				else
					height = 0;
			}
			else if (frames_policy == FRAMES_POLICY_SMART_FRAMES)
			{
				int doc_height = doc->Height();
				int doc_width = doc->GetHtmlDocument() ? doc->GetHtmlDocument()->Width() : doc->Width();

				if (!use_doc_coords)
				{
					doc_height = doc->GetVisualDevice()->ScaleToScreen(doc_height);
					doc_width = doc->GetVisualDevice()->ScaleToScreen(doc_width);
				}

				width = normal_width;
				height = normal_height;

				if (!HasExplicitZeroSize() && height < doc_height)
					height = doc_height;

				if (!HasExplicitZeroSize() && width < doc_width)
					width = doc_width;
			}
		}
	}
}

void FramesDocElm::CheckFrameStacking(BOOL stack_frames)
{
	SetIsRow(stack_frames || packed1.normal_row);
	packed1.is_inline = stack_frames && !Parent();

	pos.SetTranslation(0, 0);
	width = 0;
	height = 0;

	if (frm_dev)
		frm_dev->SetScrollType(stack_frames ? VisualDevice::VD_SCROLLING_NO : (VisualDevice::ScrollType) frame_scrolling);

	for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
		fde->CheckFrameStacking(stack_frames);
}

BOOL FramesDocElm::HasExplicitZeroSize()
{
	return !size_val && packed1.size_type == FRAMESET_ABSOLUTE_SIZED;
}

OP_STATUS FramesDocElm::HandleLoading(OpMessage msg, URL_ID url_id, MH_PARAM_2 user_data)
{
    OP_STATUS stat = OpStatus::OK;

	FramesDocElm* fde = FirstChild();

	if (fde)
	{
		for (; fde; fde = fde->Suc())
        {
			stat = fde->HandleLoading(msg, url_id, user_data);
            if( OpStatus::IsError(stat) )
                break;
        }
	}
	else if (!IsFrameset())
		stat = doc_manager->HandleLoading((doc_manager->GetLoadStatus() == WAIT_FOR_HEADER && msg == MSG_URL_DATA_LOADED) ? MSG_HEADER_LOADED : msg, url_id, user_data);

    return stat;
}

#ifdef _PRINT_SUPPORT_
void FramesDocElm::DeletePrintCopy()
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();
	if (doc)
		doc->DeletePrintCopy();
	else
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
			fde->DeletePrintCopy();
}
#endif // _PRINT_SUPPORT_

void FramesDocElm::UpdateSecurityState(BOOL include_loading_docs)
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();
	if (doc)
		doc_manager->UpdateSecurityState(include_loading_docs);
	else
	{
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
		{
			fde->UpdateSecurityState(include_loading_docs);
		}
	}
}

void FramesDocElm::AppendChildrenToList(Head* list)
{
	for (FramesDocElm* fde = FirstChild(); fde; fde = FirstChild())
	{
		fde->AppendChildrenToList(list);
		fde->Out();
		if (fde->packed1.is_secondary)
			OP_DELETE(fde);
		else
			fde->Into(list);
	}
}

int FramesDocElm::CountFrames()
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (doc || frm_dev)
	{
		if (doc && doc->IsFrameDoc())
			return doc->CountFrames();
		else
			return 1;
	}
	else
	{
		int count = 0;

		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
			count += fde->CountFrames();

		return count;
	}
}

FramesDocElm* FramesDocElm::GetFrameByNumber(int& num)
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	if (doc || frm_dev)
	{
		if (doc && doc->IsFrameDoc())
			return doc->GetFrameByNumber(num);
		else
		{
			num--;

			if (num == 0)
				return this;
			else
				return NULL;
		}
	}
	else
	{
		FramesDocElm* frame = NULL;

		for (FramesDocElm* fde = FirstChild(); !frame && fde; fde = fde->Suc())
			frame = fde->GetFrameByNumber(num);

		return frame;
	}
}

FramesDocElm* FramesDocElm::LastLeafActive()
{
	FramesDocElm *last_leaf = this;
	FramesDocElm *child = last_leaf->LastChildActive();

	while (child)
	{
		last_leaf = child;
		child = last_leaf->LastChildActive();
	}

	return last_leaf;
}

FramesDocElm* FramesDocElm::ParentActive()
{
	if (Parent())
		return Parent();

	return GetParentFramesDoc()->GetDocManager()->GetFrame();
}

FramesDocElm* FramesDocElm::FirstChildActive()
{
	FramesDocElm *candidate = (GetCurrentDoc() && GetCurrentDoc()->GetIFrmRoot())
		? GetCurrentDoc()->GetIFrmRoot()->FirstChild() : NULL;

	while (candidate)
	{
		if (candidate->GetVisualDevice())
		{
			FramesDocument *frm_doc = candidate->GetCurrentDoc();
			if (frm_doc && frm_doc->GetFrmDocRoot())
			{
				FramesDocElm *tmp_candidate = frm_doc->GetFrmDocRoot()->FirstChildActive();
				if (tmp_candidate)
					return tmp_candidate;
			}
			else
				return candidate;
		}

		candidate = candidate->Suc();
	}

	candidate = FirstChild();
	while (candidate)
	{
		if (candidate->GetVisualDevice())
		{
			FramesDocument *frm_doc = candidate->GetCurrentDoc();
			if (frm_doc && frm_doc->GetFrmDocRoot())
			{
				FramesDocElm *tmp_candidate = frm_doc->GetFrmDocRoot()->FirstChildActive();
				if (tmp_candidate)
					return tmp_candidate;
			}
			else
				return candidate;
		}

		candidate = candidate->FirstChild();
	}

	return NULL;
}

FramesDocElm* FramesDocElm::NextActive()
{
	FramesDocElm *candidate = FirstChildActive();
	if (candidate)
		return candidate;

	for (FramesDocElm *leaf = this; leaf; leaf = leaf->ParentActive())
		if (leaf->Suc())
		{
			FramesDocElm *candidate = leaf->Suc();
			if (candidate->GetVisualDevice())
			{
				FramesDocument *frm_doc = candidate->GetCurrentDoc();
				if (frm_doc && frm_doc->GetFrmDocRoot())
				{
					FramesDocElm *tmp_candidate = frm_doc->GetFrmDocRoot()->NextActive();
					if (tmp_candidate)
						return tmp_candidate;
				}
				else
					return candidate;
			}
			else
			{
				FramesDocElm *tmp_candidate = candidate->NextActive();
				if (tmp_candidate)
					return tmp_candidate;
			}
		}

	return NULL;
}

FramesDocElm* FramesDocElm::LastChildActive()
{
	FramesDocElm *candidate = LastChild();
	if (candidate)
	{
		if (candidate->GetVisualDevice())
		{
			FramesDocument *frm_doc = candidate->GetCurrentDoc();
			if (frm_doc && frm_doc->GetFrmDocRoot())
			{
				FramesDocElm *tmp_candidate = frm_doc->GetFrmDocRoot()->LastChildActive();
				if (tmp_candidate)
					return tmp_candidate;
			}
			else
				return candidate;
		}
		else
			return candidate->LastChildActive();
	}

	candidate = (GetCurrentDoc() && GetCurrentDoc()->GetFrmDocRoot())
		? GetCurrentDoc()->GetFrmDocRoot() : NULL;

	if (candidate)
	{
		FramesDocElm *tmp_candidate = candidate->LastChildActive();
		if (tmp_candidate)
			return tmp_candidate;
	}

	candidate = (GetCurrentDoc() && GetCurrentDoc()->GetIFrmRoot())
		? GetCurrentDoc()->GetIFrmRoot()->LastChild() : NULL;

	if (candidate)
	{
		while (candidate)
		{
			if (candidate->GetVisualDevice())
			{
				FramesDocument *frm_doc = candidate->GetCurrentDoc();
				if (frm_doc && frm_doc->GetFrmDocRoot())
				{
					FramesDocElm *tmp_candidate = frm_doc->GetFrmDocRoot()->LastChildActive();
					if (tmp_candidate)
						return tmp_candidate;
				}
				else
					return candidate;
			}
			candidate = candidate->Pred();
		}
	}

	return NULL;
}

FramesDocElm* FramesDocElm::PrevActive()
{
	if (Pred())
	{
		FramesDocElm* leaf = Pred();
		FramesDocElm* last_child = leaf->LastChildActive();

		while (last_child)
		{
			leaf = last_child;
			last_child = leaf->LastChildActive();
		}

		return leaf;
	}

	FramesDocElm *candidate = ParentActive();
	if (candidate)
	{
		if (candidate->GetVisualDevice())
		{
			FramesDocument *frm_doc = candidate->GetCurrentDoc();
			if (frm_doc && !frm_doc->GetFrmDocRoot())
				return candidate;
		}

		return candidate->PrevActive();
	}

	return NULL;
}

OP_STATUS FramesDocElm::OnlineModeChanged()
{
	if (GetCurrentDoc())
		return doc_manager->OnlineModeChanged();
	else
	{
		OP_STATUS status = OpStatus::OK;
		for (FramesDocElm *child = FirstChild(); child; child = child->Suc())
			if (OpStatus::IsMemoryError(child->OnlineModeChanged()))
			{
				OpStatus::Ignore(status);
				status = OpStatus::ERR_NO_MEMORY;
			}
		return status;
	}
}

void FramesDocElm::OnRenderingViewportChanged(const OpRect &rendering_rect)
{
	FramesDocument* doc = doc_manager->GetCurrentDoc();

	// Translate rect to local coordinates and clip
	OpRect local_rect(rendering_rect.x - GetX(), rendering_rect.y - GetY(), rendering_rect.width, rendering_rect.height);
	OpRect local_visible_rect(0, 0, GetWidth(), GetHeight());
	local_visible_rect.IntersectWith(local_rect);

	if (doc)
		doc->OnRenderingViewportChanged(local_visible_rect);
	else
		for (FramesDocElm* fde = FirstChild(); fde; fde = fde->Suc())
			fde->OnRenderingViewportChanged(local_visible_rect);
}

OP_STATUS FramesDocElm::SetReinitData(int history_num, BOOL visible, LayoutMode frame_layout_mode)
{
	if (reinit_data)
	{
		if (history_num != -1)
			reinit_data->m_history_num = history_num;
		reinit_data->m_visible = visible;
		reinit_data->m_old_layout_mode = frame_layout_mode;
		return OpStatus::OK;
	}
	else
	{
		reinit_data = OP_NEW(FrameReinitData, (history_num, visible, frame_layout_mode));
		if (!reinit_data)
			return OpStatus::ERR_NO_MEMORY;
		return OpStatus::OK;
	}
}

void FramesDocElm::RemoveReinitData()
{
	OP_DELETE(reinit_data);
	reinit_data = NULL;
}

BOOL FramesDocElm::IsFrameRoot(FramesDocElm* head)
{
	FramesDocElm* iframe_root = parent_frm_doc->GetIFrmRoot();
	FramesDocElm* frame_root = parent_frm_doc->GetFrmDocRoot();
	while (head)
	{
		if (head == iframe_root || head == frame_root)
			return TRUE;

		head = head->Parent();
	}

	return FALSE;
}

void FramesDocElm::Out()
{
#if DOCHAND_MAX_FRAMES_ON_PAGE > 0
	FramesDocument *top_doc = parent_frm_doc->GetTopDocument();
	DocumentTreeIterator iter(this);
	iter.SetIncludeThis();
	iter.SetIncludeEmpty();
	while (iter.Next())
	{
		FramesDocElm *fde = iter.GetFramesDocElm();
		if (fde->frame_index != FRAME_NOT_IN_ROOT)
		{
			top_doc->OnFrameRemoved();
			fde->frame_index = FRAME_NOT_IN_ROOT;
		}
	}
#endif // DOCHAND_MAX_FRAMES_ON_PAGE > 0

#ifdef SCOPE_PROFILER
	parent_frm_doc->GetDocManager()->OnRemoveChild(GetDocManager());
#endif // SCOPE_PROFILER

	Tree::Out();
}

void FramesDocElm::Under(FramesDocElm* elm)
{
#if DOCHAND_MAX_FRAMES_ON_PAGE > 0
	FramesDocument *top_doc = parent_frm_doc->GetTopDocument();
	if (IsFrameRoot(elm))
		top_doc->OnFrameAdded();

	frame_index = top_doc->GetNumFramesAdded();
#endif // DOCHAND_MAX_FRAMES_ON_PAGE > 0

#ifdef SCOPE_PROFILER
	parent_frm_doc->GetDocManager()->OnAddChild(GetDocManager());
#endif // SCOPE_PROFILER

	Tree::Under(elm);
}

BOOL FramesDocElm::IsContentAllowed()
{
#if DOCHAND_MAX_FRAMES_ON_PAGE > 0
	return ((frame_index - parent_frm_doc->GetTopDocument()->GetNumFramesRemoved()) <= DOCHAND_MAX_FRAMES_ON_PAGE);
#else
	return TRUE;
#endif // DOCHAND_MAX_FRAMES_ON_PAGE > 0
}

#ifdef NEARBY_INTERACTIVE_ITEM_DETECTION
OP_STATUS FramesDocElm::AddFrameScrollbars(OpRect& rect_of_interest, List<InteractiveItemInfo>& list)
{
	OpScrollbar *h_scr, *v_scr;
	OpRect clip_rect;
	OpRect vis_viewport = frm_dev->GetRenderingViewport(); // This is the same as visual viewport for a frame.
	OpPoint inner_pos = frm_dev->GetInnerPosition();

	frm_dev->GetScrollbarObjects(&h_scr, &v_scr);

	for (int i = 0; i < 2; i++)
	{
		OpScrollbar* current_scrollbar = i ? h_scr : v_scr;

		if (!(current_scrollbar && current_scrollbar->IsVisible()))
			continue;

		OpRect rect = current_scrollbar->GetRect();
		/* Adjust the rect position according to the document view's top left, because we want to keep it
		   relative to the point (in this frame), where the document's top left is. */
		rect.OffsetBy(-inner_pos);

		rect = frm_dev->ScaleToDoc(rect);
		// Offset by visual viewport position, so that we are in the same coordinate system as rect_of_interest/
		rect.OffsetBy(vis_viewport.x, vis_viewport.y);

		if (!rect.Intersecting(rect_of_interest))
			continue;

		// Remove the part of the rect_of_interest that overlap the current scrollbar.
		if (current_scrollbar->IsHorizontal())
		{
			if (rect_of_interest.y >= rect.y && rect.Bottom() > rect_of_interest.y)
			{
				INT32 diff = rect.Bottom() - rect_of_interest.y;
				rect_of_interest.y += diff;
				rect_of_interest.height -= diff;
			}
			else if (rect_of_interest.Bottom() > rect.y)
				rect_of_interest.height -= rect_of_interest.Bottom() - rect.y;
		}
		else
		{
			if (rect_of_interest.x >= rect.x && rect.Right() > rect_of_interest.x)
			{
				INT32 diff = rect.Right() - rect_of_interest.x;
				rect_of_interest.x += diff;
				rect_of_interest.width -= diff;
			}
			else if (rect_of_interest.Right() > rect.x)
				rect_of_interest.width -= rect_of_interest.Right() - rect.x;
		}

		if (clip_rect.IsEmpty()) // Compute that only once.
		{
			clip_rect = GetParentFramesDoc()->GetVisualDevice()->GetDocumentInnerViewClipRect();
			GetAbsPos().ApplyInverse(clip_rect);
			// Scrollbar's rect is already adjusted by the document's visual viewport top left.
			clip_rect.OffsetBy(vis_viewport.x, vis_viewport.y);
		}

		rect.IntersectWith(clip_rect);
		/* This is implied from the method's assumption about visiblity of the frame.
		   First of all, clip_rect can't be empty otherwise this frame can't be visible inside
		   top document's visual viewport at all. Secondly if scrollbar rect intersects the
		   rect_of_interest, which is inside the clip_rect, it must also intersect the clip_rect. */
		OP_ASSERT(!rect.IsEmpty());

		InteractiveItemInfo* item = InteractiveItemInfo::CreateInteractiveItemInfo(1,
			InteractiveItemInfo::INTERACTIVE_ITEM_TYPE_SCROLLBAR);

		if (!item)
			return OpStatus::ERR_NO_MEMORY;

		InteractiveItemInfo::ItemRect* rect_array = item->GetRects();
		rect_array[0].rect = rect;
		rect_array[0].affine_pos = NULL;

		item->Into(&list);
	}

	return OpStatus::OK;
}
#endif // NEARBY_INTERACTIVE_ITEM_DETECTION

void FramesDocElm::SetParentLayoutInputContext(OpInputContext* context)
{
	if (!frm_dev || context == parent_layout_input_ctx)
		return;

	parent_layout_input_ctx = context;

	frm_dev->SetParentInputContext(parent_layout_input_ctx ? parent_layout_input_ctx : parent_frm_doc->GetVisualDevice(), TRUE);
}

BOOL FramesDocElm::CheckForUnloadTarget()
{
	BOOL has_unload_handler = GetHasUnloadHandler();
	if (!GetCheckedUnloadHandler())
	{
		has_unload_handler = FALSE;

		FramesDocElm *fde = this;
		while (FramesDocElm *lc = fde->LastChild())
			fde = lc;

		while (!has_unload_handler)
		{
			if (fde->GetCheckedUnloadHandler() && fde->GetHasUnloadHandler())
				has_unload_handler = TRUE;
			else if (FramesDocument *frames_doc = fde->GetCurrentDoc())
			{
				if (DOM_Environment *environment = frames_doc->GetDOMEnvironment())
				{
					if (environment->HasWindowEventHandler(ONUNLOAD))
						has_unload_handler = TRUE;
				}
				else if (HTML_Element *fde_target = frames_doc->GetWindowEventTarget(ONUNLOAD))
					has_unload_handler = fde_target->HasEventHandlerAttribute(frames_doc, ONUNLOAD);

				fde->SetCheckedUnloadHandler(TRUE);
				fde->SetHasUnloadHandler(has_unload_handler);
			}

			if (fde == this)
				break;

			fde = fde->Prev();
		}
		SetCheckedUnloadHandler(TRUE);
		SetHasUnloadHandler(has_unload_handler);
	}

	return has_unload_handler;
}

void FramesDocElm::UpdateUnloadTarget(BOOL added)
{
	if (GetCheckedUnloadHandler() && added == GetHasUnloadHandler())
		return;

	/* If we add or remove an unload handler, the computed
	   information on ancestor elements must be recomputed.

	   Do not recompute here but simply clear the cached
	   flags on the ancestors. */
	FramesDocElm *fde = Parent();
	while (fde)
	{
		fde->SetCheckedUnloadHandler(FALSE);
		fde = fde->Parent();
	}

	/* The target of the unload addition/removal is unknown, it
	   may not have been for the element of this FramesDocElm.

	   Update the flag by doing a full check. */
	SetCheckedUnloadHandler(FALSE);
	CheckForUnloadTarget();
}

#ifdef SVG_SUPPORT
static BOOL
SVGShouldDisableMouseEvents(FramesDocElm* frame)
{
	HTML_Element* frame_elm = frame->GetHtmlElement();
	if (!frame_elm)
		return FALSE;

	if (frame_elm->GetNsType() != NS_HTML)
		return FALSE;

	switch (frame_elm->Type())
	{
	case Markup::HTE_IMG:
		return TRUE;
	case Markup::HTE_IFRAME:
		return (frame_elm->GetInserted() == HE_INSERTED_BY_SVG);
	case Markup::HTE_OBJECT:
		if (FramesDocument* doc = frame->GetCurrentDoc())
			if (LogicalDocument* logdoc = doc->GetLogicalDocument())
				if (SVGImage* svg = g_svg_manager->GetSVGImage(logdoc, logdoc->GetDocRoot()))
					return !svg->IsInteractive();
		// Fall through.
#ifdef ON_DEMAND_PLUGIN
	case Markup::HTE_EMBED:
	case Markup::HTE_APPLET:
		return frame_elm->IsPluginPlaceholder();
#endif // ON_DEMAND_PLUGIN
	default:
		return FALSE;
	}
}

void
FramesDocElm::SVGCheckWantMouseEvents()
{
	if (SVGShouldDisableMouseEvents(this))
	{
		CoreView *view = frm_dev->GetContainerView();
		OP_ASSERT(view || !"View not yet (re)initialized");
		if (view)
			view->SetWantMouseEvents(FALSE);
	}
}

#endif // SVG_SUPPORT
