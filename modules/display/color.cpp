/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/display/color.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/style/css_types.h"

COLORREF CheckColorContrast(COLORREF font_color, COLORREF bg_color, int minimum_contrast, COLORREF light_font_color, COLORREF dark_font_color)
{
	font_color = HTM_Lex::GetColValByIndex(font_color);
	bg_color = HTM_Lex::GetColValByIndex(bg_color);

	int font_brightness = (OP_GET_R_VALUE(font_color) * 299 + OP_GET_G_VALUE(font_color) * 587 + OP_GET_B_VALUE(font_color) * 114) / 1000;
	int bg_brightness = (OP_GET_R_VALUE(bg_color) * 299 + OP_GET_G_VALUE(bg_color) * 587 + OP_GET_B_VALUE(bg_color) * 114) / 1000;

	if (op_abs(font_brightness - bg_brightness) < minimum_contrast)
		return bg_brightness < 125 ? light_font_color : dark_font_color;
	else
		return font_color;
}

COLORREF GetRGBA(COLORREF col_ref)
{
	if (COLOR_is_rgba(col_ref))
		return col_ref;
	else if (COLOR_is_indexed(col_ref))
		return HTM_Lex::GetColValByIndex(col_ref);
	else if (col_ref == CSS_COLOR_transparent)
		return OP_RGBA(0,0,0,0);
	else
	{
		OP_ASSERT(col_ref == CSS_COLOR_invert ||
				  col_ref == CSS_COLOR_inherit ||
				  col_ref == CSS_COLOR_current_color ||
				  col_ref == USE_DEFAULT_COLOR);
		return USE_DEFAULT_COLOR;
	}
}

COLORREF InterpolatePremultipliedRGBA(COLORREF from, COLORREF to, double point)
{
	int from_r = OP_GET_R_VALUE(from);
	int from_g = OP_GET_G_VALUE(from);
	int from_b = OP_GET_B_VALUE(from);
	int from_a = OP_GET_A_VALUE(from);

	int to_r = OP_GET_R_VALUE(to);
	int to_g = OP_GET_G_VALUE(to);
	int to_b = OP_GET_B_VALUE(to);
	int to_a = OP_GET_A_VALUE(to);

	/* In premultiplied space, the point |t| is:
	   rgba( ((1-t)r1*a1 + t*r2*a2) / a3,
			 ((1-t)g1*a1 + t*g2*a2) / a3,
			 ((1-t)b1*a1 + t*b2*a2) / a3,
			 a3)

		where a3 = (1-t)*a1 + t*a2. */

	double mult_1 = (1-point) * from_a;
	double mult_2 = point * to_a;
	double a3 = mult_1 + mult_2;

	to_r = a3 == 0 ? 0 : static_cast<int>(OpRound((mult_1*from_r + mult_2*to_r) / a3));
	to_g = a3 == 0 ? 0 : static_cast<int>(OpRound((mult_1*from_g + mult_2*to_g) / a3));
	to_b = a3 == 0 ? 0 : static_cast<int>(OpRound((mult_1*from_b + mult_2*to_b) / a3));
	to_a = static_cast<int>(OpRound(a3));

	return OP_RGBA(to_r, to_g, to_b, to_a);
}
