#include "core/pch.h"
#include "OpWidgetStringDrawer.h"
#include "optextfragment.h"

OpWidgetStringDrawer::OpWidgetStringDrawer()
    : selection_highlight_type(VD_TEXT_HIGHLIGHT_TYPE_SELECTION),
		caret_pos(-1),
		ellipsis_position(ELLIPSIS_NONE),
		underline(FALSE),
		x_scroll(0),
		caret_snap_forward(FALSE),
		only_text(FALSE)
{
}

extern void DrawFragment(VisualDevice* vis_dev, int x, int y,
	int sel_start, int sel_stop, int ime_start, int ime_stop,
	const uni_char* str, OP_TEXT_FRAGMENT* frag,
	UINT32 color, UINT32 fcol_sel, UINT32 bcol_sel, VD_TEXT_HIGHLIGHT_TYPE selection_highlight_type,
	int height, BOOL use_accurate_font_size);

void OpWidgetStringDrawer::Draw(OpWidgetString* widget_string, const OpRect& rect, VisualDevice* vis_dev, UINT32 color)
{
	OpWidget* widget = widget_string->GetWidget();
	const uni_char* str = widget_string->GetStr();
	INT32 height = widget_string->GetHeight();

	if (!widget || !widget->font_info.font_info)
	{
		return;
	}

#ifdef SKIN_SUPPORT
	if (!only_text)
	{
		if (OpSkinElement *skin_element = widget->GetBorderSkin()->GetSkinElement())
		{
			const OpSkinTextShadow* shadow;
			if (OpStatus::IsSuccess(skin_element->GetTextShadow(shadow, widget->GetBorderSkin()->GetState())) &&
				(shadow->ofs_x || shadow->ofs_y))
			{
				OpRect shadow_rect = rect;
				shadow_rect.OffsetBy(shadow->ofs_x, shadow->ofs_y);

				OpWidgetStringDrawer drawer;
				drawer.SetSelectionHighlightType(selection_highlight_type);
				drawer.SetEllipsisPosition(ellipsis_position);
				drawer.SetOnlyText(TRUE);
				drawer.Draw(widget_string, shadow_rect, vis_dev, shadow->color);
			}
		}
	}
#endif

	// No scrolling if there is ellipses making the text fit
	if (ellipsis_position != ELLIPSIS_NONE)
		x_scroll = 0;

	OpStatus::Ignore(widget_string->Update(vis_dev));//FIXME:OOM

	widget_string->UpdateVisualDevice(vis_dev);

	// Fix for widgets in UI which hasn't ever set the string. (So height is still 0)
	if (height == 0)
		height = vis_dev->GetFontHeight();
	const UINT32 base_ascent = vis_dev->GetFontAscent();

	vis_dev->SetColor32(color);

	BOOL use_focused_colors = widget->IsFocused()
		|| (selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT);
	BOOL use_search_hit_colors = (selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_SEARCH_HIT)
		|| (selection_highlight_type == VD_TEXT_HIGHLIGHT_TYPE_ACTIVE_SEARCH_HIT);

	OP_SYSTEM_COLOR fg_col_id = OpWidgetString::GetSelectionTextColor(use_search_hit_colors, use_focused_colors);
	OP_SYSTEM_COLOR bg_col_id = OpWidgetString::GetSelectionBackgroundColor(use_search_hit_colors, use_focused_colors);

	UINT32 fcol_sel = widget->GetInfo()->GetSystemColor(fg_col_id);
	UINT32 bcol_sel = widget->GetInfo()->GetSystemColor(bg_col_id);

	INT32 sel_start = widget_string->sel_start;
	INT32 sel_stop = widget_string->sel_stop;
	if (sel_start == sel_stop)
	{
		sel_start = -1;
		sel_stop = -1;
	}
	else if (sel_start > sel_stop)
	{
		INT32 tmp = sel_start;
		sel_start = sel_stop;
		sel_stop = tmp;
	}

	OpPoint p(widget_string->GetJustifyAndIndent(rect), rect.y + rect.height/2 - height/2);

#ifdef WIDGETS_IME_SUPPORT
	// If we have an IME candidateword.. draw the selection on it.
	IME_INFO* ime_info = widget_string->m_ime_info;
	if (ime_info && ime_info->can_start != ime_info->can_stop)
	{
		sel_start = ime_info->can_start;
		sel_stop = ime_info->can_stop;
	}
#endif // WIDGETS_IME_SUPPORT

	// since quick assumes text can be drawn outside of padding, we don't clip for ui widgets
	// Also, we have to not clip buttons because there will very likely be much padding inside the button and padding on buttons is included in size.
	BOOL should_clip = FALSE;
	if (widget->IsForm() && widget->GetType() != OpTypedObject::WIDGET_TYPE_BUTTON)
		should_clip = TRUE;

	if (should_clip)
		vis_dev->BeginClipping(rect);

	if (x_scroll)
		vis_dev->Translate(-x_scroll, 0);

	int x_pos = p.x;

	int ime_start = 0;
	int ime_stop = 0;

	BOOL show_caret = TRUE;
#ifdef WIDGETS_IME_SUPPORT
	BOOL show_underline = TRUE;
	if (ime_info)
	{
		show_caret = ime_info->string->GetShowCaret();
		show_underline = ime_info->string->GetShowUnderline();
	}
#ifdef WIDGETS_IME_DISABLE_UNDERLINE
	show_underline = FALSE;
#endif // WIDGETS_IME_DISABLE_UNDERLINE
	if (ime_info && show_underline)
	{
		ime_start = ime_info->start;
		ime_stop = ime_info->stop;
	}
#endif

	if (only_text)
	{
		sel_start = -1;
		sel_stop = -1;
		ime_start = 0;
		ime_stop = 0;
		show_caret = FALSE;
#ifdef WIDGETS_IME_SUPPORT
		show_underline = FALSE;
#endif // WIDGETS_IME_SUPPORT
		underline = FALSE;
	}

#if !defined(WIDGETS_IME_SUPPORT) || !defined(SUPPORT_IME_PASSWORD_INPUT)
	if (widget_string->GetPasswordMode())
	{
		// If we have a selection, draw it and set the selectioncolor
		if (sel_start != sel_stop)
		{
			INT32 start_x = widget_string->GetCaretX(rect, sel_start, caret_snap_forward);
			INT32 stop_x = widget_string->GetCaretX(rect, sel_stop, caret_snap_forward);
			vis_dev->SetColor32(bcol_sel);
			vis_dev->FillRect(OpRect(start_x, 1 + p.y, stop_x - start_x, height - 1));
		}

		int orgiginal_fontnr = widget_string->GetFontNumber();
		vis_dev->SetFont(orgiginal_fontnr);
		INT32 char_w = vis_dev->GetTxtExtent(g_widget_globals->passwd_char, 1);
		INT32 i, x_pos = p.x;

		int len = uni_strlen(str);
		for(i = 0; i < len; i++)
		{
			if (i >= sel_start && i < sel_stop)
				vis_dev->SetColor32(fcol_sel);
			else
				vis_dev->SetColor32(color);
			vis_dev->TxtOut(x_pos, p.y, g_widget_globals->passwd_char, 1);
			x_pos += char_w;
		}
	}
	else
#endif
	{
#ifdef WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
		UINT32 non_server_color = color;
		BOOL use_server_highlighting = widget_string->GetHighlightType() != OpWidgetString::None && widget_string->m_highlight_ofs != -1 && widget_string->m_highlight_len != -1
									   && !only_text;  //only_text is actually "painting text shadow", which we should not change color for.
		if (use_server_highlighting)
		{
			// Calculate a brighter color if the set color is dark, and darker if it is bright.
			float f = 0.5; // Difference factor
			int r = GetRValue(color);
			int g = GetGValue(color);
			int b = GetBValue(color);
			r = static_cast<int>(r * (1.f - f) + ((255 - r)) * f);
			g = static_cast<int>(g * (1.f - f) + ((255 - g)) * f);
			b = static_cast<int>(b * (1.f - f) + ((255 - b)) * f);
			non_server_color =  OP_RGB(r, g, b);
		}
#endif // WIDGETS_ADDRESS_SERVER_HIGHLIGHTING

		int used_space = 0;
		const int original_fontnr = vis_dev->GetCurrentFontNumber();
		const BOOL reverse_fragments = widget_string->ShouldDrawFragmentsReversed(rect, ellipsis_position);

		OpTextFragmentList* fragments = widget_string->GetFragments();
		for (UINT32 i = 0; i < fragments->GetNumFragments(); i++)
		{
			const int idx = reverse_fragments ? fragments->GetNumFragments() - i - 1 : i;
			OP_TEXT_FRAGMENT* frag = fragments->GetByVisualOrder(idx);
			vis_dev->SetFont(frag->wi.GetFontNumber());

			int py = p.y + base_ascent - vis_dev->GetFontAscent();

			UINT32 current_color = color;
#ifdef WIDGETS_ADDRESS_SERVER_HIGHLIGHTING
			if (use_server_highlighting)
			{
				BOOL in_server = frag->start >= widget_string->m_highlight_ofs
					&& (INT32)(frag->start + frag->wi.GetLength()) <= widget_string->m_highlight_ofs + widget_string->m_highlight_len;
				if (in_server && (str[frag->start] == '/' || str[frag->start] == ':'))
				{
					use_server_highlighting = FALSE;
					in_server = FALSE;
					color = non_server_color;
				}
				if (!in_server)
					current_color = non_server_color;
			}
#endif // WIDGETS_ADDRESS_SERVER_HIGHLIGHTING

			BOOL painted = FALSE;
#ifdef WIDGETS_ELLIPSIS_SUPPORT
			if (ellipsis_position != ELLIPSIS_NONE || widget_string->GetConvertAmpersand() && frag->wi.GetLength() == 1 && str[frag->start] == '&')
			{
				vis_dev->SetColor32(current_color);
				painted = widget_string->DrawFragmentSpecial(vis_dev, x_pos, py, frag, ellipsis_position, used_space, rect);
			}
#endif // WIDGETS_ELLIPSIS_SUPPORT

			if (!painted)
			{
				int x = x_pos;
				if (reverse_fragments)
					x = MAX(rect.Right() - (x_pos - rect.x) - frag->wi.GetWidth(), p.x);
				DrawFragment(vis_dev, x, py, sel_start, sel_stop, ime_start, ime_stop, str, frag,
					current_color, fcol_sel, bcol_sel, selection_highlight_type,
					height, widget_string->UseAccurateFontSize());
				x_pos += frag->wi.GetWidth();
				used_space += frag->wi.GetWidth();
			}

			if (x_pos - x_scroll > rect.x + rect.width)
				// We reached the end of the visible rectangle, so we're done
				break;
		}
		vis_dev->SetFont(original_fontnr);
	}

	if (underline)
		vis_dev->DrawLine(OpPoint(p.x, p.y + height - 1), x_pos - p.x, TRUE, 1);

	if (widget_string->GetTextDecoration() & WIDGET_LINE_THROUGH)
		vis_dev->DrawLine(OpPoint(p.x, p.y + height/2), x_pos - p.x, TRUE, 1);

	if (should_clip)
		vis_dev->EndClipping();

	// Draw caret
	if (caret_pos != -1 && show_caret)
	{
		INT32 caret_x = widget_string->GetCaretX(rect, caret_pos, caret_snap_forward);
#ifdef DISPLAY_INVERT_CARET
		if( sel_start != sel_stop && caret_pos == sel_stop )
		{
			caret_x --; // Otherwise we cannot see cursor when at at end of selection
		}
#endif

		vis_dev->DrawCaret(OpRect(caret_x, 1 + p.y, widget_string->GetOverstrike() ? 3 : g_op_ui_info->GetCaretWidth(), height - 1));
	}

	if (x_scroll)
		vis_dev->Translate(x_scroll, 0);
}

void OpWidgetStringDrawer::Draw(OpWidgetString* widget_string, const OpRect& rect, VisualDevice* vis_dev, UINT32 color,
	ELLIPSIS_POSITION ellipsis_position, INT32 x_scroll)
{
	this->ellipsis_position = ellipsis_position;
	this->x_scroll = x_scroll;
	this->Draw(widget_string, rect, vis_dev, color);
}
