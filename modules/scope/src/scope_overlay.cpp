/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_OVERLAY

#include "modules/scope/src/scope_overlay.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/winman.h"

/* virtual */ OP_STATUS
OpScopeOverlay::OnServiceDisabled()
{
	for (Window *w = g_windowManager->FirstWindow() ; w ; w = w->Suc())
		UnregisterOverlay(w, 0);

	return OpStatus::OK;
}

/* static */ COLORREF
OpScopeOverlay::ToCOLORREF(const Color &c)
{
	return OP_RGBA(c.GetR(), c.GetG(), c.GetB(), c.GetA());
}

/* static */ VisualDevice::InsertionMethod
OpScopeOverlay::ConvertInsertionMethodType(const InsertionMethod method)
{
	switch (method)
	{
	default:
		OP_ASSERT(!"Unknown method!");
		// fall through
	case INSERTIONMETHOD_FRONT:
		return VisualDevice::IN_FRONT;

	case INSERTIONMETHOD_BACK:
		return VisualDevice::IN_BACK;

	case INSERTIONMETHOD_ABOVE_TARGET:
		return VisualDevice::ABOVE_TARGET;

	case INSERTIONMETHOD_BELOW_TARGET:
		return VisualDevice::BELOW_TARGET;
	}
}

OP_STATUS
OpScopeOverlay::FindWindow(unsigned int id, Window *&window)
{
	Window *w = g_windowManager->FirstWindow();
	for (; w; w = w->Suc())
		if (w->Id() == id)
			break;

	return (window = w) ? OpStatus::OK : SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Window not found"));
}

/* virtual */ OP_STATUS
OpScopeOverlay::DoCreateOverlay(const CreateOverlayArg &in, OverlayID &out)
{
	if (in.GetOverlayType() == OVERLAYTYPE_AREA)
	{
		if (!in.HasAreaOverlay())
			return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("The area must be specified for the AREA overlay"));

		Window *window;
		RETURN_IF_ERROR(FindWindow(in.GetWindowID(), window));

		FramesDocument *frm_doc = window->GetCurrentDoc();
		RETURN_VALUE_IF_NULL(frm_doc, OpStatus::ERR_NULL_POINTER);
		VisualDevice *vis_dev = frm_doc->GetDocManager()->GetVisualDevice();
		RETURN_VALUE_IF_NULL(vis_dev, OpStatus::ERR_NULL_POINTER);

		OpAutoPtr<VDSpotlight> spotlight(OP_NEW(VDSpotlight, ()));
		RETURN_OOM_IF_NULL(spotlight.get());

		VDSpotlightInfo *spotlight_area = &spotlight->content;
		AreaOverlay &overlay = *in.GetAreaOverlay();

		/* Set area. */
		Area &area = overlay.GetAreaRef();
		spotlight_area->SetRect(OpRect(area.GetX(), area.GetY(), area.GetW(), area.GetH()));

		/* Set background color. */
		spotlight_area->EnableFill(ToCOLORREF(overlay.GetBackgroundColorRef()));

		/* Set border color. */
		if (overlay.HasBorderColor())
			spotlight_area->EnableInnerFrame(ToCOLORREF(*overlay.GetBorderColor()));

		/* Set grid color. */
		if (overlay.HasGridColor())
			spotlight_area->EnableGrid(ToCOLORREF(*overlay.GetGridColor()));

		const Insertion &insertion = in.GetInsertion();
		InsertionMethod method = insertion.GetMethod();
		VDSpotlight *target = NULL;

		if (method == INSERTIONMETHOD_ABOVE_TARGET || method == INSERTIONMETHOD_BELOW_TARGET)
		{
			if (!insertion.HasOverlayID() || insertion.GetOverlayID() == 0)
				return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("The overlay ID must be specified for given insertion method"));

			if ((target = vis_dev->GetSpotlight(insertion.GetOverlayID())) == NULL)
				return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("The overlay with specified ID does not exist"));
		}

		unsigned int overlay_id;
		RETURN_IF_ERROR(RegisterOverlay(window, spotlight.get(), overlay_id));
		RETURN_IF_ERROR(vis_dev->AddSpotlight(spotlight.get(), NULL, ConvertInsertionMethodType(method), target));
		spotlight.release();
		out.SetOverlayID(overlay_id);

		return OpStatus::OK;
	}

	return SetCommandError(OpScopeTPHeader::BadRequest, UNI_L("Unsupported overlay type"));
}

/* virtual */ OP_STATUS
OpScopeOverlay::DoRemoveOverlay(const RemoveOverlayArg &in)
{
	Window *window;
	RETURN_IF_ERROR(FindWindow(in.GetWindowID(), window));
	RETURN_IF_ERROR(UnregisterOverlay(window, in.GetOverlayID()));

	return OpStatus::OK;
}

OP_STATUS
OpScopeOverlay::RegisterOverlay(Window *window, VDSpotlight *spotlight, unsigned int &overlay_id)
{
	OtlList<unsigned int> *spotlight_list = NULL;
	if (OpStatus::IsError(m_window_to_spotlight_list.GetData(window, &spotlight_list)))
	{
		RETURN_OOM_IF_NULL(spotlight_list = OP_NEW(OtlList<unsigned int>, ()));
		RETURN_IF_ERROR(m_window_to_spotlight_list.Add(window, spotlight_list));
	}

	/* Make sure ID is never 0 (in unreal case when it overflows). */
	overlay_id = ++m_last_spotlight_id ? m_last_spotlight_id : ++m_last_spotlight_id;

	spotlight->SetID(overlay_id);
	return spotlight_list->Append(overlay_id);
}

OP_STATUS
OpScopeOverlay::UnregisterOverlay(Window *window, unsigned int overlay_id)
{
	FramesDocument *frm_doc = window->GetCurrentDoc();
	RETURN_VALUE_IF_NULL(frm_doc, OpStatus::ERR_NULL_POINTER);
	VisualDevice *vis_dev = frm_doc->GetDocManager()->GetVisualDevice();
	RETURN_VALUE_IF_NULL(vis_dev, OpStatus::ERR_NULL_POINTER);

	if (overlay_id == 0)
	{
		/* Remove all overlays registered for the given window. */
		OtlList<unsigned int> *spotlight_list;
		if (OpStatus::IsSuccess(m_window_to_spotlight_list.GetData(window, &spotlight_list)))
		{
			OtlList<unsigned int>::Iterator iter;
			for (iter = spotlight_list->Begin(); iter != spotlight_list->End(); iter++)
				vis_dev->RemoveSpotlight(*iter);

			spotlight_list->Clear();
		}
	}
	else
	{
		/* Remove overlay with a specific ID. */
		OtlList<unsigned int> *spotlight_list;
		if (OpStatus::IsSuccess(m_window_to_spotlight_list.GetData(window, &spotlight_list)))
			if (spotlight_list->RemoveItem(overlay_id))
				vis_dev->RemoveSpotlight(overlay_id);
	}

	vis_dev->UpdateAll();

	return OpStatus::OK;
}

#endif // SCOPE_OVERLAY
