/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/desktop_util/widget_utils/VisualDeviceHandler.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpMultiEdit.h"

#include "modules/display/vis_dev.h"
#include "modules/style/css.h"

/***********************************************************************************
**
**	OpLabel
**
***********************************************************************************/

DEFINE_CONSTRUCT(OpLabel);

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OpLabel::OpLabel() :
	m_wrap(FALSE)
	, m_edit(NULL)
{
	// This MUST be first otherwise GetBorderSkin()->SetImage(UNI_L("Label Skin")); will crash below
	CreateEdit(FALSE);

	SetSkinned(TRUE);
	GetBorderSkin()->SetImage("Label Skin");
	m_delayed_trigger.SetDelayedTriggerListener(this);
	m_delayed_trigger.SetTriggerDelay(0, 50);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpLabel::CreateEdit(BOOL wrap)
{
	ELLIPSIS_POSITION	old_ellipsis = ELLIPSIS_END; // OpLabel's always default to ellipses at the end
	OpString			old_text;

	// Destroy the old control when changing types
	if (m_edit)
	{
		// Save the old text
		RETURN_IF_ERROR(m_edit->GetText(old_text));

		// Save the old ellipse position, but it can't be none, by defualt they are at the end
		old_ellipsis = m_edit->GetEllipsis();
		if (old_ellipsis == ELLIPSIS_NONE)
			old_ellipsis = ELLIPSIS_END;

		m_edit->Delete();
	}

	// Create the right type based on if you want wrapping or not
	if (wrap)
	{
		OpMultilineEdit *tmp;
		RETURN_IF_ERROR(OpMultilineEdit::Construct(&tmp));
		m_edit = tmp;

		// OpMultilineEdit needs to be flat!
		// This is a quick only function that makes it Flat and sets label mode
		tmp->SetLabelMode();

		// The sole purpose of this is removing the default padding
		tmp->GetBorderSkin()->SetImage("Label Edit Skin");
	}
	else
	{
		OpEdit *tmp;
		RETURN_IF_ERROR(OpEdit::Construct(&tmp));
		m_edit = tmp;

		// OpEdit needs to be flat!
		tmp->SetFlatMode();

		// Always at least has ellipses at the end by default
		tmp->SetEllipsis(old_ellipsis);

		// The sole purpose of this is removing the default padding
		tmp->GetBorderSkin()->SetImage("Label Edit Skin");
	}

	// ALWAYS make these fields read only
	m_edit->SetReadOnly(TRUE);

	// These fields ALWAYS ignore mouse clicks and let the OpLabel handle it
	m_edit->SetIgnoresMouse(TRUE);
	
	// The padding of the edit field needs to be set now since normally this is done when
	// a Relayout call is made. This can happen in a SetRect call for example, but until that
	// happens functions like GetTextWidth(), will return the wrong value since they will have
	// the padding from the constructor of OpEdit. Not the real one that is set in the
	// Relayout call. Therefore calling this early will get the padding correct as soon 
	// as the OpLabel is created!
	m_edit->UpdateSkinPadding();

	// Make this a child of the OpLabel
	AddChild(m_edit, TRUE);

	// Set the old text into the new control if it was found
	if (old_text.HasContent())
	{
		// SetText needs a visual device for MultiEdit
		if (wrap)
		{
			VisualDeviceHandler handler(m_edit);
			RETURN_IF_ERROR(m_edit->SetText(old_text.CStr()));
		}
		else
			RETURN_IF_ERROR(m_edit->SetText(old_text.CStr()));
	}

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpLabel::SetText(const uni_char* text)
{
	if (m_wrap)
	{
		// GetText()/SetText needs a visual device for MultiEdit
		VisualDeviceHandler handler(m_edit);

		// OpMultiedit doesn't test if the text has changed
		// so we need to do it until it does
		// [why? getting and comparing is slow on big texts arjanl 20101108]
		OpString current_text;
		RETURN_IF_ERROR(m_edit->GetText(current_text));

		// Simply return if the text hasn't changed
		if (current_text.Compare(text))
			RETURN_IF_ERROR(m_edit->SetText(text));
	}
	else
	{
		RETURN_IF_ERROR(m_edit->SetText(text));
	}

	m_delayed_trigger.InvokeTrigger();

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpLabel::GetText(OpString& text)
{
	return m_edit->GetText(text);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpLabel::SetScrollbarStatus(WIDGET_SCROLLBAR_STATUS status)
{
	if (m_wrap)
	{
		OpMultilineEdit* edit = static_cast<OpMultilineEdit*>(m_edit);
		edit->SetScrollbarStatus(status, status);
		edit->SetIgnoresMouse(status == SCROLLBAR_STATUS_OFF);
		SetIgnoresMouse(status == SCROLLBAR_STATUS_OFF);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpLabel::SetWrap(BOOL wrap)
{
	// Recreate the control if the type just changed
	if (m_wrap != wrap)
	{
		RETURN_IF_ERROR(CreateEdit(wrap));
	}

	// Store if we are using wrapping
	m_wrap = wrap;

	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpLabel::SetCenterAlign(BOOL center)
{
//	if (m_wrap)
//		((OpMultilineEdit *)m_edit)->SetCenterAlign(center);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpLabel::SetSelectable(BOOL selectable)
{
	SetIgnoresMouse(!selectable);
	m_edit->SetIgnoresMouse(!selectable);
	if (m_wrap)
	{
		((OpMultilineEdit *)m_edit)->SetLabelMode(selectable);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Deprecated REMOVE IT ASAP
//
OP_STATUS OpLabel::SetLabel(const uni_char* newlabel, BOOL center)
{
	SetJustify(center ? JUSTIFY_CENTER : JUSTIFY_LEFT, FALSE);

	return SetText(newlabel);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT32 OpLabel::GetTextLength()
{
	if (m_wrap)
		return ((OpMultilineEdit *)m_edit)->GetTextLength(FALSE);
	return ((OpEdit *)m_edit)->GetTextLength();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT32 OpLabel::GetTextWidth()
{
	INT32 w = 0, left, top, right, bottom;

	GetPadding(&left, &top, &right, &bottom);

	if (m_wrap)
		w = ((OpMultilineEdit *)m_edit)->GetMaxBlockWidth();
	else
		w = ((OpEdit *)m_edit)->string.GetWidth();
	w += (left + right); 

	return w;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT32 OpLabel::GetTextHeight()
{
	INT32 h = 0, left, top, right, bottom;

	GetPadding(&left, &top, &right, &bottom);

	if (m_wrap)
		h = ((OpMultilineEdit *)m_edit)->GetContentHeight();
	else
		h = ((OpEdit *)m_edit)->string.GetHeight();
	h += (top + bottom);

	return h;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT32 OpLabel::GetPreferedHeight(UINT32 rows)
{
	INT32 h = 0, left, top, right, bottom;
	GetPadding(&left, &top, &right, &bottom);
	if (m_wrap)
	{
		VisualDeviceHandler handler(m_edit);
		h  = ((OpMultilineEdit *)m_edit)->GetVisibleLineHeight() + 1; //Pixel compension
	}
	else
		h = ((OpEdit *)m_edit)->string.GetHeight();

	h =  rows * h + top + bottom;

	return h;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpLabel::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	m_edit->UpdateSkinPadding();	//julienp:	Otherwise, we mightg have the wrong padding here
									//			if this is called before a relayout
	*w = GetTextWidth();
	*h = GetTextHeight();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpLabel::SetEllipsis(ELLIPSIS_POSITION ellipsis)
{
	// Set the ellipsis in the OpLabel and in the edit control
	// Doing this will also mean we don't need to override the Get as the OpLabel
	// will be following the underlying edit control
	OpWidget::SetEllipsis(ellipsis);
	m_edit->SetEllipsis(ellipsis);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpLabel::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (listener)
	{
		listener->OnMouseEvent(this, -1, point.x, point.y, button, TRUE, nclicks);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpLabel::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (listener)
	{
		listener->OnMouseEvent(this, -1, point.x, point.y, button, FALSE, 1);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpLabel::OnJustifyChanged()
{
	m_edit->SetJustify(font_info.justify, FALSE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpLabel::OnFontChanged()
{
	m_edit->SetRelativeSystemFontSize(GetRelativeSystemFontSize());
	m_edit->SetSystemFontWeight(GetSystemFontWeight());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpLabel::OnResize(INT32* new_w, INT32* new_h)
{
	ResetRequiredSize();

	OpRect internal_rect(0,0,*new_w,*new_h);
	AddPadding(internal_rect);

	m_edit->SetRect(internal_rect);
}

void OpLabel::SetSkin(const char* skin)
{
	GetBorderSkin()->SetImage(skin);
}

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

OpAccessibleItem* OpLabel::GetAccessibleChildOrSelfAt(int x, int y)
{
	OpAccessibleItem* widget = OpWidget::GetAccessibleChildOrSelfAt(x, y);
	if (widget == NULL)
		return NULL;
	else
		return this;
}

OpAccessibleItem* OpLabel::GetAccessibleFocusedChildOrSelf()
{
	OpAccessibleItem* widget = OpWidget::GetAccessibleFocusedChildOrSelf();
	if (widget == NULL)
		return NULL;
	else
		return this;
}
#endif
