/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "adjunct/quick/widgets/OpResizeSearchDropDown.h"

#include "adjunct/quick/widgets/OpSearchDropDown.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/dragdrop/dragdrop_manager.h"

#ifdef VEGA_OPPAINTER_SUPPORT
#include "adjunct/quick/managers/AnimationManager.h"
#endif

namespace
{
	const int MIN_ADDRESS_SEARCH_DROP_DOWN_WIDTH = 180;
	// Weighted width in percents is unpractical (It result in jerky resizing).
	// Prefs files can't store floats, so weight is now in between 0 and 10000.
	const int MIN_ADDRESS_SEARCH_DROP_DOWN_WEIGHTED_WIDTH = 1500;
}


/**
 * A bidirectional iterator over OpResizeSearchDropDown's siblings.
 *
 * Makes sure Left() and Right() always mean left and right, regardles of UI
 * direction.
 */
class OpResizeSearchDropDown::SiblingIterator
{
public:
	/**
	 * @param current determines the iterator's initial position
	 */
	explicit SiblingIterator(OpWidget* current)
		: m_parent(GetParent(current))
		, m_pos(m_parent != NULL ? m_parent->FindWidget(current) : -1)
	{
	}

	OpWidget* Get() const { return m_pos >= 0 ? m_parent->GetWidget(m_pos) : NULL; }
	void Left() { if (m_pos >= 0) { m_pos += m_parent->GetRTL() ? 1 : -1; } }
	void Right() { if (m_pos >= 0) { m_pos += m_parent->GetRTL() ? -1 : 1; } }

	operator bool() const { return Get() != NULL; }
	OpWidget* operator->() const { return Get(); }

private:
	static OpToolbar* GetParent(OpWidget* widget)
	{
		if (widget != NULL && widget->GetParent() != NULL
				&& widget->GetParent()->IsOfType(OpTypedObject::WIDGET_TYPE_TOOLBAR))
		{
			return static_cast<OpToolbar*>(widget->GetParent());
		}
		return NULL;
	}

	OpToolbar* m_parent;
	int m_pos;
};


/***********************************************************************************
 **
 **	OpResizeSearchDropDown
 **
 ** Note: UserData in the menu item is set to the index of the search
 **
 **
 ***********************************************************************************/

DEFINE_CONSTRUCT(OpResizeSearchDropDown)

////////////////////////////////////////////////////////////////////////////////////////

OpResizeSearchDropDown::OpResizeSearchDropDown() :
	m_knob(NULL),
	m_search_drop_down(NULL),
	m_dragging(FALSE),
	m_max_size(-1),
	m_weighted_width(-1),
	m_width_auto_resize(-1),
	m_width_auto_resize_old(-1),
	m_requested_width(-1),
	m_animation_position(1),
	m_knob_on_left(TRUE),
	m_grow_to_left(m_knob_on_left),
	m_grow_to_right(!m_knob_on_left),
	m_delayed_layout_in_progress(FALSE),
	m_parent_available_width(0),
	m_drag_left_border(0),
	m_drag_right_border(0)
{
	// Add the knob
	init_status = OpWidget::Construct(&m_knob);
	if (OpStatus::IsError(init_status))
		return;

	SetMinWidth(90);

	SetSkinned(TRUE);
	GetBorderSkin()->SetImage("Dropdown Search Resize Skin");

	m_knob->SetSkinned(TRUE);
	m_knob->GetBorderSkin()->SetImage("Resize Knob");
	// Ignore mouse in the knob as it's all handled in this control
	m_knob->SetIgnoresMouse(TRUE);
	AddChild(m_knob, TRUE);

	// Add the search drop down
	init_status = OpSearchDropDownSpecial::Construct(&m_search_drop_down);
	if (OpStatus::IsError(init_status))
		return;
		
	AddChild(m_search_drop_down, TRUE);

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT
	AccessibilitySkipsMe();
#endif //ACCESSIBILITY_EXTENSION_SUPPORT
	}

void OpResizeSearchDropDown::OnDeleted()
{
	if (m_search_drop_down)
		m_search_drop_down->GetDropdownButton()->SetListener(NULL);
}

OpResizeSearchDropDown::~OpResizeSearchDropDown()
{
}
////////////////////////////////////////////////////////////////////////////////////////

void OpResizeSearchDropDown::OnContentSizeChanged()
{
	if (!m_search_drop_down || !GetParent())
		return;

	OpRect parent_rect = GetParent()->GetRect();
	OpRect text_rect = m_search_drop_down->GetTextRect();
	int extra_stuff = GetBounds().width - text_rect.width;

	int curr_wanted_auto_width = CALC_WIDTH_FROM_WEIGHTED_WIDTH(m_weighted_width, parent_rect.width);

	int overflow = m_search_drop_down->GetStringWidth() + extra_stuff - curr_wanted_auto_width;
	if (overflow > 0)
	{
		int leap = 60;
		curr_wanted_auto_width += (overflow + leap - 1) / leap * leap;

		// We shouldn't grow too much
		int grow_max = GetParent()->GetBounds().width / 2;
		curr_wanted_auto_width = MIN(grow_max, curr_wanted_auto_width);
	}
	if (m_width_auto_resize != curr_wanted_auto_width)
	{
		m_width_auto_resize_old = m_width_auto_resize;
		m_width_auto_resize = curr_wanted_auto_width;
		ResetRequiredSize();
		GetParent()->Relayout();

#ifdef VEGA_OPPAINTER_SUPPORT
		if (g_animation_manager->GetEnabled())
			g_animation_manager->startAnimation(this, ANIM_CURVE_BEZIER, 300, TRUE);
#endif
	}
}

#ifdef VEGA_OPPAINTER_SUPPORT

void OpResizeSearchDropDown::OnAnimationStart()
{
	m_animation_position = 0;
}

void OpResizeSearchDropDown::OnAnimationUpdate(double position)
{
	m_animation_position = position;
	ResetRequiredSize();
	GetParent()->Relayout();
}

void OpResizeSearchDropDown::OnAnimationComplete() { }

#endif // VEGA_OPPAINTER_SUPPORT

////////////////////////////////////////////////////////////////////////////////////////

void OpResizeSearchDropDown::OnLayout()
{
	DetectGrowableSiblings();

	OpRect control_rect, knob_rect, search_rect;
	int knob_width = 0;
	int width = 0;

	control_rect = GetBounds();

	// Very hacky, calculate the width until the pref is set, I don't like this at all :)
	if (m_weighted_width < 0)
	{
		m_weighted_width = MIN_ADDRESS_SEARCH_DROP_DOWN_WEIGHTED_WIDTH;

		if (GetParent() && GetParent()->IsNamed("Document Toolbar"))
		{
			OpRect toolbar_rect = GetParent()->GetRect();
			INT32  left, top, right, bottom;

			// Adjust the width for the padding on the toolbar
			GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
			toolbar_rect.width -= (left + right);

			// get a base minimum width on first run
			width = toolbar_rect.width / 4;

			// find the weighted width
			width = max(width, CALC_WIDTH_FROM_WEIGHTED_WIDTH(m_weighted_width, toolbar_rect.width));

			// Make sure this isn't smaller than the minimum
			if (width < MIN_ADDRESS_SEARCH_DROP_DOWN_WIDTH)
			{
				width = MIN_ADDRESS_SEARCH_DROP_DOWN_WIDTH;
			}
			m_weighted_width = max(MIN_ADDRESS_SEARCH_DROP_DOWN_WEIGHTED_WIDTH, CALC_WEIGHTED_WIDTH_FROM_WIDTH(width, toolbar_rect.width));
			// We have to save the size now so that "funny" resizing doesn't happen on restart if
			// the size of the main window is saved
			TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::AddressSearchDropDownWeightedWidth, m_weighted_width));
		}
		else
		{
			width = MIN_ADDRESS_SEARCH_DROP_DOWN_WIDTH;
		}
	}
	else
	{
		OpRect toolbar_rect = GetParent()->GetRect();
		INT32  left, top, right, bottom;

		// Adjust the width for the padding on the toolbar
		GetBorderSkin()->GetPadding(&left, &top, &right, &bottom);
		toolbar_rect.width -= (left + right);

		if(m_weighted_width < MIN_ADDRESS_SEARCH_DROP_DOWN_WEIGHTED_WIDTH)
		{
			m_weighted_width = MIN_ADDRESS_SEARCH_DROP_DOWN_WEIGHTED_WIDTH;
		}
		else if(m_weighted_width > 7000)
		{
			m_weighted_width = 7000;
		}
		width = CALC_WIDTH_FROM_WEIGHTED_WIDTH(m_weighted_width, toolbar_rect.width);
	}
	if(m_requested_width != width)
	{
		m_width_auto_resize = m_requested_width = width;

		ResetRequiredSize();
	}
	if (m_knob)
	{
		INT32 h;
		m_knob->GetBorderSkin()->GetSize(&knob_width, &h);
		
		knob_rect = m_knob->GetRect();
		knob_rect.x = control_rect.x + m_knob_on_left ? 0 : (control_rect.width - knob_width);
		knob_rect.y = control_rect.y;
		knob_rect.height = control_rect.height;
		knob_rect.width = knob_width;

		m_knob->SetRect(knob_rect, !m_delayed_layout_in_progress);
	}

	if (m_search_drop_down)
	{
		search_rect = m_search_drop_down->GetRect();
		search_rect.x = control_rect.x + m_knob_on_left ? knob_width : 0;
		search_rect.y = control_rect.y;
		search_rect.height = control_rect.height;
		search_rect.width = control_rect.width - knob_width;

		m_search_drop_down->SetRect(search_rect, !m_delayed_layout_in_progress);
	}
	OpWidget::OnLayout();
}

////////////////////////////////////////////////////////////////////////////////////////

void OpResizeSearchDropDown::OnAdded() 
{
	INT32 width = -1;

	// If this is on the address bar then get the default size from the pref
	if (GetParent() && GetParent()->IsNamed("Document Toolbar"))
	{
		// Get the default size from the pref
		m_weighted_width = g_pcui->GetIntegerPref(PrefsCollectionUI::AddressSearchDropDownWeightedWidth);
	}
	else if (GetParent() && GetParent()->IsNamed("Customize Toolbar Search"))
	{
		m_weighted_width = CALC_WEIGHTED_WIDTH_FROM_WIDTH(40, 100);	// Needs a reasonable default size for use in the Appearance dialog
	}
	else
	{
		width = MIN_ADDRESS_SEARCH_DROP_DOWN_WIDTH;

		if(GetParent())
		{
			OpRect toolbar_rect = GetParent()->GetRect();
			if (toolbar_rect.width > 0)
				m_weighted_width = CALC_WEIGHTED_WIDTH_FROM_WIDTH(width, toolbar_rect.width);
		}
	}

	OpToolbar *toolbar = static_cast<OpToolbar*>(GetParent());
	if (toolbar)
	{
		m_search_drop_down->GetDropdownButton()->SetListener(toolbar);
	}

	DetectGrowableSiblings();

}

////////////////////////////////////////////////////////////////////////////////////////

void OpResizeSearchDropDown::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	if (m_search_drop_down)
	{
		m_search_drop_down->GetPreferedSize(w, h, cols, rows);

		int auto_width = m_width_auto_resize;

#ifdef VEGA_OPPAINTER_SUPPORT
		if (IsAnimating())
			auto_width = m_width_auto_resize_old + (m_width_auto_resize - m_width_auto_resize_old) * m_animation_position;
#endif
		if(m_parent_available_width != m_requested_width)
		{
			auto_width = m_width_auto_resize = m_requested_width = CALC_WIDTH_FROM_WEIGHTED_WIDTH(m_weighted_width, m_parent_available_width);
		}
		*w = MAX(m_requested_width, auto_width);
	}
	else
		OpWidget::GetPreferedSize(w, h, cols, rows);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpResizeSearchDropDown::OnSetCursor(const OpPoint &point)
{
	if (!IsCustomizing() && (m_grow_to_left || m_grow_to_right))
	{
		// Change the cursor if we are over the knob
		if (m_knob)
		{
			OpRect knob_rect = m_knob->GetRect();
			if (knob_rect.Contains(point))
			{
				SetCursor(CURSOR_VERT_SPLITTER);
				return;
			}
		}
		if (m_dragging)
		{
			SetCursor(CURSOR_VERT_SPLITTER);
			return;
		}
	}
	OpWidget::OnSetCursor(point);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpResizeSearchDropDown::OnMouseMove(const OpPoint &point)
{
	if (!IsCustomizing())
	{
		if (m_dragging)
		{
			OpWidget* left_sibling = m_growable_left_sibling;
			OpWidget* right_sibling = m_growable_right_sibling;

			int width = MIN_ADDRESS_SEARCH_DROP_DOWN_WIDTH;
			OpRect old_rect = GetRect();

			// Count how much and what direction we want to resize
			INT32 move = m_drag_start_point.x - point.x;

			OpRect control_rect = m_drag_start_rect;

			if (m_grow_to_left)
			{
				control_rect = GetRect();
				control_rect.x -= move;
				control_rect.width += move;
			}
			else if (m_grow_to_right)
			{
				control_rect = GetRect();
				control_rect.width -= move;
			}

			INT32 left_border = m_drag_left_border;
			INT32 right_border = m_drag_right_border;

			// Make sure we don't make this too small
			// or too large
			if (control_rect.width < MIN_ADDRESS_SEARCH_DROP_DOWN_WIDTH ||
				control_rect.x < left_border ||
				control_rect.x+control_rect.width > right_border)
			{
				control_rect = old_rect;
				// Prevent moving of widget
				move = 0;
			}
			else if (!m_knob_on_left)
			{
				// This line is small workaround: if not used, then
				// dragging using knob on right side
				// would result in unpredictable 'move' values
				m_drag_start_point = point;
			}

			//printf("move: %d\n", move);
			
			// Set the new rect
			SetRect(control_rect);

			// We must reset the size since this is dynamic!
			ResetRequiredSize();
			
			width = control_rect.width;
			// Store the new width so it's correct for the next relayout
			if(GetParent())
			{
				OpRect toolbar_rect = GetParent()->GetRect();
				// take the maximum of values. Calculated weighted value may be less than minimum.
				m_weighted_width = MAX(
					MIN_ADDRESS_SEARCH_DROP_DOWN_WEIGHTED_WIDTH,
					CALC_WEIGHTED_WIDTH_FROM_WIDTH(width, toolbar_rect.width)
				);
			}
			// Reset automatic resize to respect users choice. It will kick in if the text is changed again.
			m_width_auto_resize = width;

			// Relayout the toolbar
			// We do this manually; alternatively, we could try GetParent()->Relayout(), but it results
			// in jerky motion, high cpu usage, and breaks position of widgets to the right side of
			// searchfield.
			//
			// Turns out that GetNextSibling() and GetPreviousSibling() cannot be depended on
			// at this point any more: widgets that are between two resizable widgets
			// disappear from their positions in linked list after being clicked
			// that's why we have to ask parent toolbar to iterate through widgets :/

			OP_ASSERT((m_grow_to_left)  ? left_sibling != NULL : TRUE);
			OP_ASSERT((m_grow_to_right) ? right_sibling != NULL : TRUE);

			if (m_grow_to_left)
			{
				// resize resizable widget to the left
				OpRect left_rect = left_sibling->GetRect();
				left_rect.width -= move;
				left_sibling->SetRect(left_rect);

				// reposition every widget in between
				SiblingIterator it(left_sibling);
				for (it.Right(); it && it.Get() != this; it.Right())
				{
					OpRect rect = it->GetRect();
					rect.x -= move;
					it->SetRect(rect);
				}
			}
			if (m_grow_to_right)
			{
				// resize resizable widget to the right
				OpRect right_rect = right_sibling->GetRect();
				right_rect.x -= move;
				right_rect.width += move;
				right_sibling->SetRect(right_rect);

				// reposition every widget in between
				SiblingIterator it(right_sibling);
				for (it.Left(); it && it.Get() != this; it.Left())
				{
					OpRect rect = it->GetRect();
					rect.x -= move;
					it->SetRect(rect);
				}
			}
		}
	}
	OpWidget::OnMouseMove(point);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpResizeSearchDropDown::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (!IsCustomizing())
	{
		if (button == MOUSE_BUTTON_1 && nclicks == 1 && (m_grow_to_left || m_grow_to_right))
		{
			m_drag_start_rect = GetRect();
			m_drag_start_point = point;
			m_dragging = TRUE; 

			DetectGrowableSiblings();
			DetectLeftRightDraggingBorders();
		}
	}
	OpWidget::OnMouseDown(point, button, nclicks);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpResizeSearchDropDown::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (!IsCustomizing())
	{
		// If we are currently dragging the save the new width into the toolbar
		if (m_dragging)
		{
			// Make sure we have a parent
			if (GetParent())
			{
				// If we would like to assure, that resizing will result in proper toolbar
				// layout, we could try:
				// (it's commented-out because it's only useful for testing)
				//GetParent()->Relayout(); 

				// If this button is on the Address bar save the width in the preference
				// Don't save this into the toolbar as then the address bar will ALWAYS be customised
				if (GetParent()->IsNamed("Document Toolbar"))
				{
					// Make sure we don't save a stupid small value
					if (m_weighted_width >= DEFAULT_ADDRESS_SEARCH_DROP_DOWN_WIDTH)
					{
						// Set the default size from the pref
						g_pcui->WriteIntegerL(PrefsCollectionUI::AddressSearchDropDownWeightedWidth, m_weighted_width);
					}
				}
				else
				{
					// Any other toolbar just save the new width
					if (GetParent()->GetType() == WIDGET_TYPE_TOOLBAR)
					{
						static_cast<OpToolbar*>(GetParent())->WriteContent();
					}
				}
			}
		}

		m_dragging = FALSE;
		m_drag_start_point = point;

		GenerateOnSetCursor(GetMousePos());

		// Reset the max size
		m_max_size = -1;

	}
	OpWidget::OnMouseUp(point, button, nclicks);
}

////////////////////////////////////////////////////////////////////////////////////////

void OpResizeSearchDropDown::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget->GetType() == WIDGET_TYPE_SEARCH_DROPDOWN)
	{
		// Pass on the call to the toolbar to make the menu selction work
		((OpWidgetListener*)GetParent())->OnMouseEvent(this, pos, x, y, button, down, nclicks);
	}
	else
	{
		OpWidget::OnMouseEvent(widget, pos, x, y, button, down, nclicks);
	}
}

////////////////////////////////////////////////////////////////////////////////////////

void OpResizeSearchDropDown::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (!g_application->IsDragCustomizingAllowed())
		return;

	DesktopDragObject* drag_object = GetDragObject(OpTypedObject::DRAG_TYPE_RESIZE_SEARCH_DROPDOWN, x, y);

	if (drag_object)
	{
		drag_object->SetObject(this);
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpResizeSearchDropDown::GetText(OpString& text) 
{
	return m_search_drop_down ? m_search_drop_down->GetText(text) : OpStatus::ERR;
}

////////////////////////////////////////////////////////////////////////////////////////

BOOL OpResizeSearchDropDown::OnInputAction(OpInputAction* action)
{ 
	switch (action->GetAction())
	{
		// ACTION_GO is invoked in OpToolbar on the closest widget of Go button, OpResizeSearchDropDown has to delegate this action to m_search_drop_down
		case OpInputAction::ACTION_GO:
		{
			if(m_search_drop_down) 
				return m_search_drop_down->OnInputAction(action);
		}
		default:
			break;
	}

	return OpWidget::OnInputAction(action);
}

void OpResizeSearchDropDown::DetectGrowableSiblings()
{
	// Detect in which direction we can grow and place knob accordingly
	// If we can't grow, knob is still visible (on left)
	
	m_growable_left_sibling = NULL;
	m_growable_right_sibling = NULL;

	m_grow_to_left  = FALSE;
	m_grow_to_right = FALSE;

	SiblingIterator it(this);
	for (it.Left(); it; it.Left())
	{
		if (it->GetGrowValue() > 0)
		{
			m_growable_left_sibling = it.Get();
			m_grow_to_left = TRUE;
			break;
		}
	}
	if (!m_grow_to_left)
	{
		SiblingIterator it(this);
		for (it.Right(); it; it.Right())
		{
			if (it->GetGrowValue() > 0)
			{
				m_growable_right_sibling = it.Get();
				m_grow_to_right = TRUE;
			}
			if (!it->IsVisible() || m_grow_to_right)
				break;
		}
	}
	m_knob_on_left = m_grow_to_left;
}

void OpResizeSearchDropDown::DetectLeftRightDraggingBorders()
{
	OpWidget* left_sibling = m_growable_left_sibling;
	OpWidget* right_sibling = m_growable_right_sibling;

	// Don't allow to resize to non-resizable area of neighbouring widgets
	// these two are 'virtual' borders that block resizing to left and right
	INT32 left_border = 0;
	INT32 right_border = GetParent()->GetRect().width;

	// these are margins between widgets
	INT32 m_l, m_t, m_r, m_b;
	INT32 margin_left, margin_right;
	GetBorderSkin()->GetMargin(&margin_left,&m_t,&margin_right,&m_b);

	// count position of left border
	if (m_grow_to_left && left_sibling)
	{
		OpRect left_rect = left_sibling->GetRect();
		INT32 w, h; // minimum size required by sibling
		left_sibling->GetRequiredSize(w,h);

		left_border = left_rect.x+w;

		// add sizes of all non-resizable widgets between
		SiblingIterator it(left_sibling);
		for (it.Right(); it && it.Get() != this; it.Right())
		{
			OpRect rect = it->GetRect();
			it->GetBorderSkin()->GetMargin(&m_l, &m_t, &m_r, &m_b);
			left_border += rect.width + m_l + m_r;
		}
		left_border += margin_left;
	}
	// same thing as above, for right border:
	if (m_grow_to_right && right_sibling)
	{
		OpRect right_rect = right_sibling->GetRect();
		INT32 w, h; // minimum size required by sibling
		right_sibling->GetRequiredSize(w,h);
		right_border = right_rect.Right() - w;

		SiblingIterator it(right_sibling);
		for (it.Left(); it && it.Get() != this; it.Left())
		{
			OpRect rect = it->GetRect();
			it->GetBorderSkin()->GetMargin(&m_l, &m_t, &m_r, &m_b);
			right_border -= rect.width + m_l + m_r;
		}
		right_border -= margin_right;
	}

	m_drag_left_border = left_border;
	m_drag_right_border = right_border;
}

