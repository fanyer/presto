/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick_toolkit/widgets/OpRichTextLabel.h"

#include "adjunct/desktop_scope/src/scope_widget_info.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/quick-widget-names.h"
#include "modules/widgets/OpMultiEdit.h"

#include "modules/display/vis_dev.h"
#include "modules/style/css.h"
#include "modules/skin/OpSkinManager.h"
#include "modules/skin/skin_module.h"

#include "adjunct/desktop_scope/src/scope_desktop_window_manager.h"

class OpRichTextLabel::ScopeWidgetInfo : public OpScopeWidgetInfo
{
public:
	ScopeWidgetInfo(OpRichTextLabel& label) : m_label(label) {}
	virtual OP_STATUS AddQuickWidgetInfoItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, BOOL include_nonhoverable, BOOL include_invisible);
private:
	OpRichTextLabel& m_label;
};

/***********************************************************************************
 **
 **	OpRichTextLabel
 **
 ***********************************************************************************/

DEFINE_CONSTRUCT(OpRichTextLabel)

////////////////////////////////////////////////////////////////////////////////////////////////////////////

OpRichTextLabel::OpRichTextLabel() :
	OpRichtextEdit()
{
	SetSkinned(TRUE);
	SetLabelMode();	
	SetReadOnly(TRUE);
	
	GetBorderSkin()->SetImage("Label Skin");
	UpdateSkinPadding();
	
	// Set proper link color taken from skin
	UINT32 link_color;
	OpSkinElement* skin_elm = g_skin_manager->GetSkinElement("Rich Text Label Skin");
		
	if (skin_elm && OpStatus::IsSuccess(skin_elm->GetLinkColor(&link_color, GetBorderSkin()->GetState())))
	{
		SetLinkColor(link_color);
	}

    SetName(WIDGET_NAME_RICHTEXT_LABEL);
    
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpRichTextLabel::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1 && m_pointed_link)
	{
		OpStringC str(m_pointed_link);
		
		// Link contains an opera action
		if (str.Length() > 14 && str.Compare("opera:/action/", 14) == 0)
		{
			OpInputAction* action = NULL;
			if (OpStatus::IsSuccess(OpInputAction::CreateInputActionsFromString(str.CStr() + 14, action)))
				g_input_manager->InvokeAction(action, this);
		}
		// Link is to a web page. Open it up.
		else
			g_application->GoToPage(str);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT32 OpRichTextLabel::GetTextWidth()
{
	INT32 w = 0, top, left, right, bottom;
	
	GetPadding(&left, &top, &right, &bottom);
	
	w = GetMaxBlockWidth();
	
	w += (left + right);

	return w;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

INT32 OpRichTextLabel::GetTextHeight()
{
	INT32 h = 0, top, left, right, bottom;
	
	GetPadding(&left, &top, &right, &bottom);
	
	h = GetContentHeight();
	
	h += (top + bottom);
	
	return h;
} 


////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpRichTextLabel::OnResize(INT32* new_w, INT32* new_h)
{
	ResetRequiredSize();
	
	OpRichtextEdit::OnResize(new_w, new_h);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void OpRichTextLabel::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	UpdateSkinPadding();
	
	*w = GetTextWidth();
	*h = GetTextHeight();
}


OpScopeWidgetInfo* OpRichTextLabel::CreateScopeWidgetInfo()
{
	return OP_NEW(ScopeWidgetInfo, (*this));
}

OP_STATUS OpRichTextLabel::ScopeWidgetInfo::AddQuickWidgetInfoItems(OpScopeDesktopWindowManager_SI::QuickWidgetInfoList &list, BOOL include_nonhoverable, BOOL include_invisible)
{
    OpVector<OpRect> rects;
    
    RETURN_IF_ERROR(m_label.GetLinkPositions(rects));
    
    // If there is a list this is the automated UI collecting information about each of the cells
    UINT32 counter = 0;
    OpRect  label_rect = m_label.GetScreenRect();
    for (counter = 0; counter < rects.GetCount(); counter++)
    {
        // Make a treeitem for this
        OpAutoPtr<OpScopeDesktopWindowManager_SI::QuickWidgetInfo> info(OP_NEW(OpScopeDesktopWindowManager_SI::QuickWidgetInfo, ()));
        if (info.get() == NULL)
            return OpStatus::ERR;
    
        OpString name;
        name.AppendFormat(UNI_L("Link_%d"), counter);
        RETURN_IF_ERROR(info.get()->SetName(name));
	
        // Set the info
        info.get()->SetType(OpScopeDesktopWindowManager_SI::QuickWidgetInfo::QUICKWIDGETTYPE_BUTTON);
    
        OpString text_value;
        m_label.GetText(text_value);
        RETURN_IF_ERROR(info.get()->SetText(text_value.CStr()));
    
        // Should only get here for the visible items
        info.get()->SetVisible(m_label.IsVisible());
	
        // Set the enabled flag based on the main control
        info.get()->SetEnabled(m_label.IsEnabled());
        
        // Set the label as the parent
        if (m_label.GetName().HasContent())
        {
            OpString name;
            name.Set(m_label.GetName());
            info.get()->SetParent(name);
        }
	
        // Set the rect

        OpRect* link_rect = rects.Get(counter);
        info.get()->GetRectRef().SetX(label_rect.x + link_rect->x);
        info.get()->GetRectRef().SetY(label_rect.y + link_rect->y);
        info.get()->GetRectRef().SetWidth(link_rect->width);
        info.get()->GetRectRef().SetHeight(link_rect->height);
		
	
        RETURN_IF_ERROR(list.GetQuickwidgetListRef().Add(info.get()));
        info.release();
	
	}
    
    return OpStatus::OK;
}
