/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpColorField.h"
#include "modules/forms/piforms.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/forms/formsuggestion.h"
#include "modules/display/vis_dev.h"
#include "modules/style/css.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/widgets/CssWidgetPainter.h"
#include "modules/widgets/WidgetWindow.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/windowcommander/src/WindowCommander.h"

// == Utilities ====================================================

OP_STATUS ParseColor(const uni_char *text, COLORREF &out_color)
{
	if (!text || *text != '#' || !ParseColor(text, uni_strlen(text), out_color))
		return OpStatus::ERR;
	// Workaround for something that might be a bug in ParseColor. It returns a COLORREF that has alpha calculated like a UINT32 color.
	// COLORREF is 7bit alpha and the full alpha value means it's a named color which it's not in this case.
	out_color = OP_RGB(GetRValue(out_color), GetGValue(out_color), GetBValue(out_color));
	return OpStatus::OK;
}

// == OpColorMatrix ================================================

DEFINE_CONSTRUCT(OpColorMatrix)

#define MAX_CELLS_PER_ROW 10
#define DEFAULT_CELL_SIZE 20

OpColorMatrix::OpColorMatrix()
{
	m_active_cell_index = -1;
	m_cell_size = DEFAULT_CELL_SIZE;
	m_colors = m_default_colors;
	m_num_colors = 20;
	m_default_colors[0] = OP_RGB(0, 0, 0);
	m_default_colors[1] = OP_RGB(127, 127, 127);
	m_default_colors[2] = OP_RGB(136, 0, 21);
	m_default_colors[3] = OP_RGB(237, 28, 36);
	m_default_colors[4] = OP_RGB(255, 127, 39);
	m_default_colors[5] = OP_RGB(255, 242, 0);
	m_default_colors[6] = OP_RGB(34, 177, 76);
	m_default_colors[7] = OP_RGB(0, 162, 232);
	m_default_colors[8] = OP_RGB(63, 72, 204);
	m_default_colors[9] = OP_RGB(163, 73, 164);

	m_default_colors[10] = OP_RGB(255, 255, 255);
	m_default_colors[11] = OP_RGB(195, 195, 195);
	m_default_colors[12] = OP_RGB(185, 122, 87);
	m_default_colors[13] = OP_RGB(255, 174, 201);
	m_default_colors[14] = OP_RGB(255, 201, 14);
	m_default_colors[15] = OP_RGB(239, 228, 176);
	m_default_colors[16] = OP_RGB(181, 230, 29);
	m_default_colors[17] = OP_RGB(153, 217, 234);
	m_default_colors[18] = OP_RGB(112, 146, 190);
	m_default_colors[19] = OP_RGB(200, 191, 231);

	m_picked_color = m_default_colors[0];

	SetTabStop(TRUE);
}

OpColorMatrix::~OpColorMatrix()
{
	if (m_colors != m_default_colors)
		OP_DELETEA(m_colors);
}

void OpColorMatrix::SetPickedColor(COLORREF color)
{
	if (color != m_picked_color)
	{
		m_picked_color = color;
		InvalidateAll();
	}
}

OP_STATUS OpColorMatrix::SetColors(COLORREF *colors, int num_colors)
{
	// Create new array and copy data to it
	COLORREF *new_colors = OP_NEWA(COLORREF, num_colors);
	if (!new_colors)
		return OpStatus::ERR_NO_MEMORY;
	op_memcpy(new_colors, colors, num_colors * sizeof(COLORREF));

	// Unallocate old array
	if (m_colors != m_default_colors)
		OP_DELETEA(m_colors);

	// We're done
	m_active_cell_index = -1;
	m_colors = new_colors;
	m_num_colors = num_colors;
	InvalidateAll();
	return OpStatus::OK;
}

int OpColorMatrix::CalculateWidth()
{
	int row_count = (m_num_colors + MAX_CELLS_PER_ROW - 1) / MAX_CELLS_PER_ROW;
	int width = row_count > 1 ? MAX_CELLS_PER_ROW * m_cell_size : m_num_colors * m_cell_size;
	return width + 2; // Border
}

int OpColorMatrix::CalculateHeight()
{
	int row_count = (m_num_colors + MAX_CELLS_PER_ROW - 1) / MAX_CELLS_PER_ROW;
	int height = row_count * m_cell_size;
	return height + 2; // Border
}

void OpColorMatrix::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	OpRect view_rect = GetBounds();
	VisualDevice *vd = GetVisualDevice();
	vd->SetColor(GetColor().use_default_background_color ? OP_RGB(255, 255, 255) : GetColor().background_color);
	vd->FillRect(view_rect);
	vd->SetColor(0, 0, 0);
	vd->DrawRect(view_rect);

	view_rect = view_rect.InsetBy(1); // Border

	BOOL is_focused = IsFocused();

	int x = 0, y = 0;
	for (int i = 0; i < m_num_colors; i++)
	{
		OpRect cell_rect(view_rect.x + x, view_rect.y + y, m_cell_size, m_cell_size);
		cell_rect = cell_rect.InsetBy(1);

		vd->SetColor(m_colors[i]);
		vd->FillRect(cell_rect);

		if (m_colors[i] == m_picked_color)
		{
			m_active_cell_index = i;
			vd->SetColor(OP_RGB(255, 255, 255));
			vd->DrawRect(cell_rect);
			if (is_focused)
				vd->DrawFocusRect(cell_rect);
			cell_rect = cell_rect.InsetBy(-1);
			vd->SetColor(OP_RGB(0, 0, 0));
			vd->DrawRect(cell_rect);
		}

		// Move to next grid position
		x += m_cell_size;
		if (x >= view_rect.width)
		{
			x = 0;
			y += m_cell_size;
		}
	}
}

int OpColorMatrix::GetColorIndexFromPoint(const OpPoint& point)
{
	// Calculate picked color index. (-1 for border)
	int cellx = (point.x - 1) / m_cell_size;
	int celly = (point.y - 1) / m_cell_size;
	if (cellx >= 0 && cellx < MAX_CELLS_PER_ROW)
	{
		int cell_index = cellx + celly * MAX_CELLS_PER_ROW;
		if (cell_index >= 0 && cell_index < m_num_colors)
			return cell_index;
	}
	return -1;
}

#ifndef MOUSELESS
void OpColorMatrix::OnMouseMove(const OpPoint &point)
{
	if (hooked_widget == this)
	{
		int cell_index = GetColorIndexFromPoint(point);
		if (cell_index != -1)
			SetPickedColor(m_colors[cell_index]);
	}
}

void OpColorMatrix::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	OnMouseMove(point);
}

void OpColorMatrix::OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (button == MOUSE_BUTTON_1 && GetColorIndexFromPoint(point) != -1)
	{
		if (listener)
			listener->OnClick(this);
	}
}
#endif // !MOUSELESS

void OpColorMatrix::PopulateFromDatalist(OpWidget *widget_with_form)
{
	FormObject *form_obj = widget_with_form->GetFormObject(TRUE);
	if (!form_obj)
		return;
	HTML_Element *he = form_obj->GetHTML_Element();
	if (he->HasAttr(ATTR_LIST))
	{
		// Get all items from the list in to a array
		FormSuggestion form_suggestion(form_obj->GetDocument(), he, FormSuggestion::AUTOMATIC);
		BOOL contains_private_data = TRUE; // Play it safe
		uni_char **items;
		int num_items = 0;
		int num_columns = 0;
		OP_STATUS ret_val = form_suggestion.GetItems(NULL, &items, &num_items, &num_columns, &contains_private_data);
		if (OpStatus::IsSuccess(ret_val))
		{
			// Parse values as colors and feed the successful ones into a new color array
			COLORREF *colors = OP_NEWA(COLORREF, num_items);
			if (colors)
			{
				int num_colors = 0;
				for(int i = 0; i < num_items; i++)
				{
					COLORREF c;
					if (OpStatus::IsSuccess(ParseColor(items[i * num_columns], c)))
						colors[num_colors++] = c;
				}
				// We're done. Set it.
				SetColors(colors, num_colors);
				OP_DELETEA(colors);
			}
			// Clean up
			for(int i = 0; i < num_items * num_columns; i++)
				OP_DELETEA(items[i]);
			OP_DELETEA(items);
		}
	}
}

BOOL OpColorMatrix::IsParentInputContextAvailabilityRequired()
{
	// Normally we don't allow focus to move to a different window, but
	// we really need focus in this one to make it usable.
	return CanHaveFocusInPopup() ? FALSE : TRUE;
}

BOOL OpColorMatrix::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_PREVIOUS_ITEM:
		case OpInputAction::ACTION_NEXT_ITEM:
			{
				int index = m_active_cell_index;
				if (index == -1)
					index = 0;
				else if (action->GetAction() == OpInputAction::ACTION_PREVIOUS_ITEM)
					index--;
				else if (action->GetAction() == OpInputAction::ACTION_NEXT_ITEM)
					index++;
				if (index >= 0 && index < m_num_colors)
					SetPickedColor(m_colors[index]);
				return TRUE;
			}
			break;
		case OpInputAction::ACTION_SELECT_ITEM:
			{
				if (listener)
				{
					listener->OnClick(this);
					return TRUE;
				}
			}
			break;
	}
	return FALSE;
}

// == OpColorBoxWindow =============================================

class OpColorBoxWindow : public WidgetWindow
{
public:
	OpColorBoxWindow(WidgetWindowHandler *handler) : WidgetWindow(handler), matrix(NULL), edit(NULL), button(NULL) {}

	OpColorMatrix *matrix;
	OpEdit *edit;
	OpButton *button;
};

// == OpColorBoxColorSelectionCallback =============================
// Listen for the response from the platform/UI color picker.

#ifdef WIDGETS_USE_NATIVECOLORSELECTOR

class OpColorBoxColorSelectionCallback : public OpColorSelectionListener::ColorSelectionCallback
{
public:
	OpColorBoxColorSelectionCallback(OpColorBox *colorbox, COLORREF initial_color)
		: m_colorbox(colorbox)
		, m_initial_color(initial_color)
	{
	}
	~OpColorBoxColorSelectionCallback()
	{
		m_colorbox->ResetColorSelectionCallback();
	}
	void OnColorSelected(COLORREF color)
	{
		m_colorbox->SetColor(color, FALSE);
		OP_DELETE(this);
	}
	void OnCancel()
	{
		OP_DELETE(this);
	}
	COLORREF GetInitialColor()
	{
		return m_initial_color;
	}
	OpColorBox *m_colorbox;
	COLORREF m_initial_color;
};
#endif // WIDGETS_USE_NATIVECOLORSELECTOR

// == OpColorBox ===================================================

DEFINE_CONSTRUCT(OpColorBox)

OpColorBox::OpColorBox()
	: m_color(OP_RGB(0, 0, 0))
	, m_is_readonly(FALSE)
	, m_is_hovering_button(FALSE)
#ifdef WIDGETS_USE_NATIVECOLORSELECTOR
	, m_callback(NULL)
#endif
{
#ifdef SKIN_SUPPORT
	SetSkinned(TRUE);
	GetBorderSkin()->SetImage("Dropdown Skin");
#endif
}

void OpColorBox::OnRemoving()
{
#ifdef WIDGETS_USE_NATIVECOLORSELECTOR
	if (m_callback)
	{
		// Tell the UI that we're going down and it should cancel dialog
		// or just unreference the callback.
		WindowCommander* wc = vis_dev->GetWindow()->GetWindowCommander();
		OP_ASSERT(wc);
		if (wc)
			wc->GetColorSelectionListener()->OnColorSelectionCancel(wc);
		OP_DELETE(m_callback);
	}
#endif
}

void OpColorBox::OnDeleted()
{
	m_window_handler.OnOwnerDeleted();
}

void OpColorBox::SetReadOnly(BOOL readonly)
{
	if (m_is_readonly != readonly)
	{
		m_is_readonly = readonly;
		InvalidateAll();
	}
}

void OpColorBox::SetColor(COLORREF color, bool force_no_onchange)
{
	if (color != m_color)
	{
		m_color = color;
		InvalidateAll();

		if (listener && !force_no_onchange)
			listener->OnChange(this);
	}
}

OP_STATUS OpColorBox::GetText(OpString &str)
{
	if (!str.Reserve(10))
		return OpStatus::ERR_NO_MEMORY;
	HTM_Lex::GetRGBStringFromVal(m_color, str.CStr(), TRUE);
	return OpStatus::OK;
}

OP_STATUS OpColorBox::SetText(const uni_char* text)
{
	COLORREF c;
	RETURN_IF_ERROR(ParseColor(text, c));
	SetColor(c, TRUE);
	return UpdateEditField();
}

void OpColorBox::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	widget_painter->DrawPopupableString(GetBounds(), m_is_hovering_button);

	UpdateWindow();
}

#ifndef MOUSELESS
void OpColorBox::OnMouseMove(const OpPoint &point)
{
	BOOL is_inside = GetBounds().Contains(point) && !IsDead();
	if (is_inside != m_is_hovering_button)
	{
		m_is_hovering_button = is_inside;
		InvalidateAll();
	}
}

void OpColorBox::OnMouseLeave()
{
	m_is_hovering_button = FALSE;
	InvalidateAll();
}

void OpColorBox::OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks)
{
	if (!m_is_readonly && IsEnabled())
	{
		if (m_window_handler.GetWindow() || m_window_handler.IsClosingWindow())
			CloseWindow();
		else
			OpenWindow();
	}
}
#endif // !MOUSELESS

void OpColorBox::OpenWindow()
{
	if (!m_window_handler.GetWindow())
	{
		// Create window for color matrix
		if (OpColorBoxWindow *window = OP_NEW(OpColorBoxWindow, (&m_window_handler)))
		{
			OpWindow* parent_window = GetParentOpWindow();
			OP_STATUS status = window->Init(OpWindow::STYLE_POPUP, parent_window);
			if (OpStatus::IsError(status))
			{
				OP_DELETE(window);
				if (OpStatus::IsMemoryError(status))
					ReportOOM();
			}
			else
			{
				// Create color matrix and position it in the window.
				if (OpStatus::IsError(OpColorMatrix::Construct(&window->matrix)))
					OP_DELETE(window);
				else
				{
					OpWidget *root = window->GetWidgetContainer()->GetRoot();
					root->SetParentInputContext(this);
					window->matrix->SetCanHaveFocusInPopup(TRUE);

					// Setup the color matrix
					window->matrix->PopulateFromDatalist(this);
					window->matrix->SetPickedColor(m_color);
					window->matrix->SetListener(this);
					window->GetWidgetContainer()->GetRoot()->AddChild(window->matrix, TRUE);

					// Setup edit field
					if (OpStatus::IsSuccess(OpEdit::Construct(&window->edit)))
					{
						window->edit->SetCanHaveFocusInPopup(TRUE);
						OpStatus::Ignore(window->edit->SetPattern(UNI_L("#      ")));
						OpStatus::Ignore(window->edit->SetAllowedChars("0123456789abcdef"));
						window->edit->SetHasCssBackground(TRUE);
						window->edit->SetHasCssBorder(TRUE);
						window->edit->SetListener(this);
						window->matrix->AddChild(window->edit, FALSE);
						window->matrix->SetFocus(FOCUS_REASON_OTHER);
					}
					// Setup button for native color picker (if available)
#ifdef WIDGETS_USE_NATIVECOLORSELECTOR
					if (OpStatus::IsSuccess(OpButton::Construct(&window->button)))
					{
						window->button->SetListener(this);
						OpString str;
						TRAPD(status, g_languageManager->GetStringL(Str::S_COLORPICKER_OTHER, str));
						OpStatus::Ignore(status);
						OpStatus::Ignore(window->button->SetText(str.CStr()));
						window->matrix->AddChild(window->button, TRUE);
					}
#endif // WIDGETS_USE_NATIVECOLORSELECTOR

					// Set background/foreground colors on matrix and editfield
					if (OpWidget::GetColor().use_default_foreground_color == FALSE)
						window->matrix->SetForegroundColor(OpWidget::GetColor().foreground_color);
					if (OpWidget::GetColor().use_default_background_color == FALSE)
						window->matrix->SetBackgroundColor(OpWidget::GetColor().background_color);

					m_window_handler.SetWindow(window);

					// Show window
					OpStatus::Ignore(UpdateEditField());
					UpdateWindow();
				}
			}
		}
	}
}

void OpColorBox::OnMove()
{
	UpdateWindow();
}

void OpColorBox::GetPreferedSize(INT32* w, INT32* h, INT32 cols, INT32 rows)
{
	*w = GetVisualDevice()->GetFontAveCharWidth() * 2 + GetInfo()->GetDropdownButtonWidth(this);
	*w += GetPaddingLeft() + GetPaddingRight();
	if (!HasCssBorder())
		*w += 4;
}

void OpColorBox::UpdateWindow()
{
	OpColorBoxWindow *window = (OpColorBoxWindow *) m_window_handler.GetWindow();
	if (!window)
		return;

	// Zoom cell size - based on font size and current zoom, but never smaller than DEFAULT_CELL_SIZE
	OpColorMatrix *matrix = window->matrix;
	matrix->SetCellSize(MAX(DEFAULT_CELL_SIZE, GetVisualDevice()->ScaleToScreen(font_info.size) + 4));

	INT32 matrix_width = matrix->CalculateWidth();
	INT32 matrix_height = matrix->CalculateHeight();
	// Position editfield
	if (window->edit)
	{
		window->edit->SetFontInfo(font_info);
		window->edit->SetJustify(JUSTIFY_CENTER, FALSE);

		INT32 edit_width = 80, edit_height = 20;
		window->edit->GetPreferedSize(&edit_width, &edit_height, 0, 0);

		// Make sure there's room for it.
		matrix_width = MAX(matrix_width, edit_width);

		window->edit->SetRect(OpRect(1, matrix_height - 1, matrix_width - 2, edit_height));
		// The edit lives in the matrix, extend its height:
		matrix_height += edit_height;
	}
	// Position button
#ifdef WIDGETS_USE_NATIVECOLORSELECTOR
	if (window->button)
	{
		INT32 button_width, button_height = 20;
		window->button->GetPreferedSize(&button_width, &button_height, 0, 0);

		// Make sure there's room for the button.
		matrix_width = MAX(matrix_width, button_width);

		window->button->SetRect(OpRect(1, matrix_height - 1, matrix_width - 2, button_height));
		// The button lives in the matrix, extend its height:
		matrix_height += button_height;
	}
#endif
	// Position the matrix and window
	OpRect rect = AutoCompleteWindow::GetBestDropdownPosition(this, matrix_width, matrix_height);
	matrix->SetRect(OpRect(0, 0, matrix_width, matrix_height));
	window->Show(TRUE, &rect);
}

OP_STATUS OpColorBox::UpdateEditField()
{
	OpColorBoxWindow *window = (OpColorBoxWindow *) m_window_handler.GetWindow();
	if (!window)
		return OpStatus::OK;
	OpString text;
	RETURN_IF_ERROR(GetText(text));
	return window->edit->SetText(text, TRUE);
}

void OpColorBox::OnClick(OpWidget *widget, UINT32 id)
{
	if (widget->GetType() == WIDGET_TYPE_COLOR_MATRIX)
	{
		OpColorMatrix *matrix = (OpColorMatrix *) widget;
		SetColor(matrix->GetPickedColor(), FALSE);

		m_window_handler.Close();
	}
#ifdef WIDGETS_USE_NATIVECOLORSELECTOR
	if (widget->GetType() == WIDGET_TYPE_BUTTON && !m_callback)
	{
		m_window_handler.Close();

		// Request a color from the platform/UI
		WindowCommander* wc = vis_dev->GetWindow()->GetWindowCommander();
		if (wc)
		{
			m_callback = OP_NEW(OpColorBoxColorSelectionCallback, (this, m_color));
			if (m_callback)
			{
				wc->GetColorSelectionListener()->OnColorSelectionRequest(wc, m_callback);
			}
		}
	}
#endif // WIDGETS_USE_NATIVECOLORSELECTOR
}

void OpColorBox::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	OpColorBoxWindow *window = (OpColorBoxWindow *) m_window_handler.GetWindow();
	if (!window)
		return;

	if (widget == window->edit)
	{
		OpStatus::Ignore(SetText(window->edit->string.Get()));
		window->matrix->SetPickedColor(m_color);
	}
}

BOOL OpColorBox::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_PREVIOUS_ITEM:
		case OpInputAction::ACTION_NEXT_ITEM:
		case OpInputAction::ACTION_SELECT_ITEM:
			if (m_window_handler.GetWindow())
			{
				OpColorBoxWindow *window = (OpColorBoxWindow *) m_window_handler.GetWindow();
				return window->matrix->OnInputAction(action);
			}
			break;
		case OpInputAction::ACTION_FOCUS_NEXT_WIDGET:
		case OpInputAction::ACTION_FOCUS_PREVIOUS_WIDGET:
			if (m_window_handler.GetWindow())
			{
				OpColorBoxWindow *window = (OpColorBoxWindow *) m_window_handler.GetWindow();
				return window->GetWidgetContainer()->OnInputAction(action);
			}
			break;
		case OpInputAction::ACTION_CLOSE_DROPDOWN:
		{
			if (m_window_handler.GetWindow())
			{
				CloseWindow();
				return TRUE;
			}
			return FALSE;
		}

		case OpInputAction::ACTION_SHOW_DROPDOWN:
		{
			if (!m_window_handler.GetWindow())
			{
				OpenWindow();
				return TRUE;
			}
			break;
		}
#ifdef _SPAT_NAV_SUPPORT_
		case OpInputAction::ACTION_UNFOCUS_FORM:
			return g_input_manager->InvokeAction(action, GetParentInputContext());
#endif // _SPAT_NAV_SUPPORT_
	}
	return FALSE;
}

void OpColorBox::OnFocus(BOOL focus,FOCUS_REASON reason)
{
}

void OpColorBox::OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason)
{
	if (!new_input_context || !new_input_context->IsChildInputContextOf(this))
		CloseWindow();
	InvalidateAll();
}
