/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/style/css_gradient.h"

#ifdef CSS_GRADIENT_SUPPORT
#include "modules/logdoc/htm_lex.h"
#include "modules/util/tempbuf.h"
#include "modules/style/css.h"
#include "modules/display/vis_dev.h"
#include "modules/libvega/src/oppainter/vegaoppainter.h"
#include "modules/layout/layoutprops.h"

CSSValueType
CSS_Gradient::GetCSSValueType() const
{
	if (GetType() == LINEAR)
	{
		if (repeat)
			return CSS_FUNCTION_REPEATING_LINEAR_GRADIENT;
		else
		{
			if (webkit_prefix)
				return CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT;
			else if (o_prefix)
				return CSS_FUNCTION_O_LINEAR_GRADIENT;
			else
				return CSS_FUNCTION_LINEAR_GRADIENT;
		}
	}
	else
	{
		if (repeat)
			return CSS_FUNCTION_REPEATING_RADIAL_GRADIENT;
		else
			return CSS_FUNCTION_RADIAL_GRADIENT;
	}
}

// Copying

OP_STATUS
CSS_Gradient::CopyTo(CSS_Gradient& copy) const
{
	if (this != &copy)
	{
		copy.repeat = repeat;
		copy.webkit_prefix = webkit_prefix;
		copy.o_prefix = o_prefix;
		return copy.CopyStops(stops, stop_count);
	}
	// Copy to self is weird but not an error.
	return OpStatus::OK;
}

OP_STATUS
CSS_LinearGradient::CopyToLinear(CSS_LinearGradient& copy) const
{
	if (this != &copy)
	{
		copy.line = line;
		return CSS_Gradient::CopyTo(copy);
	}
	// Copy to self is weird but not an error.
	return OpStatus::OK;
}

OP_STATUS
CSS_RadialGradient::CopyToRadial(CSS_RadialGradient& copy) const
{
	if (this != &copy)
	{
		copy.pos = pos;
		copy.form = form;
		return CSS_Gradient::CopyTo(copy);
	}
	// Copy to self is weird but not an error.
	return OpStatus::OK;
}

CSS_LinearGradient*
CSS_LinearGradient::Copy() const
{
	CSS_LinearGradient* copy = OP_NEW(CSS_LinearGradient, ());
	if (!copy || CopyToLinear(*copy) != OpStatus::OK)
	{
		OP_DELETE(copy);
		return NULL;
	}
	return copy;
}

CSS_LinearGradient*
CSS_LinearGradient::CopyL() const
{
	CSS_LinearGradient* copy = Copy();
	if (!copy)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return copy;
}

CSS_RadialGradient*
CSS_RadialGradient::Copy() const
{
	CSS_RadialGradient* copy = OP_NEW(CSS_RadialGradient, ());
	if (!copy || CopyToRadial(*copy) != OpStatus::OK)
	{
		OP_DELETE(copy);
		return NULL;
	}
	return copy;
}

CSS_RadialGradient*
CSS_RadialGradient::CopyL() const
{
	CSS_RadialGradient* copy = Copy();
	if (!copy)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	return copy;
}

// Misc.

BOOL
CSS_Gradient::IsOpaque() const
{
	for (int i = 0; i < stop_count; i++)
		if (stops[i].color == CSS_COLOR_transparent ||
			 OP_GET_A_VALUE(HTM_Lex::GetColValByIndex(stops[i].color)) < 255)
		{
			return false;
		}
	return true;
}

void
CSS_LinearGradient::CalculateShape(VisualDevice*, const OpRect& rect) const
{
	OP_ASSERT(line.end_set || line.angle_set); // Checked in CSS_Parser::SetLinearGradientL
	if (line.end_set)
	{
		if (line.to && line.x != CSS_VALUE_center && line.y != CSS_VALUE_center)
		{
			// "Magic corners"
			double corner_ccw = 0; // Angle of the neighboring corner in the counter-clockwise direction
			if      (line.x == CSS_VALUE_left  && line.y == CSS_VALUE_top   ) corner_ccw = op_atan2((double) -rect.height, -rect.width);
			else if (line.x == CSS_VALUE_right && line.y == CSS_VALUE_top   ) corner_ccw = op_atan2((double)  rect.height, -rect.width);
			else if (line.x == CSS_VALUE_right && line.y == CSS_VALUE_bottom) corner_ccw = op_atan2((double)  rect.height,  rect.width);
			else if (line.x == CSS_VALUE_left  && line.y == CSS_VALUE_bottom) corner_ccw = op_atan2((double) -rect.height,  rect.width);
			else OP_ASSERT(!"line.x can be only left, right or center.");
			CalculateShape(M_PI - corner_ccw, rect);
		}
		else
		{
			// Horizontal or vertical gradient line, or old syntax (no 'to': no magic corners)
			switch (line.x)
			{
			case CSS_VALUE_left:
				start.x = rect.width;
				break;
			case CSS_VALUE_right:
				start.x = 0;
				break;
			case CSS_VALUE_center:
				start.x = rect.width / 2.0;
				break;
			default:
				OP_ASSERT(!"line.x can be only left, right or center.");
			}
			switch (line.y)
			{
			case CSS_VALUE_top:
				start.y = rect.height;
				break;
			case CSS_VALUE_bottom:
				start.y = 0;
				break;
			case CSS_VALUE_center:
				start.y = rect.height / 2.0;
				break;
			default:
				OP_ASSERT(!"line.y can be only top, bottom or center.");
			}

			// This if can be removed once the prefixes are gone.
			if (!line.to)
			{
				start.x = rect.width - start.x;
				start.y = rect.height - start.y;
			}

			end.x = rect.width - start.x;
			end.y = rect.height - start.y;
		}
	}
	else if (line.angle_set)
	{
		CalculateShape(line.angle, rect);
	}

	double width = end.x - start.x;
	double height = end.y - start.y;
	length = op_sqrt(width*width + height*height);
	angle = op_atan2(height, width);
}

void
CSS_LinearGradient::CalculateShape(double t, const OpRect& rect) const
{
		// Convert the top-origin, clockwise angle into a right origin, counter-clockwise angle, for easier math
		// Don't do it when prefixed though, as the angle is that way.
		if (!webkit_prefix && !o_prefix)
			t = M_PI/2.0 - t;

		// Normalize the gradient angle to -M_PI <= t < M_PI to easily compare to c_angle
		t = op_fmod(t, 2*M_PI);
		if (t > 0)
		{
			if (t >= M_PI)
				t -= 2*M_PI;
		}
		else
			if (t < -M_PI)
				t += 2*M_PI;

		// Find the length of the corner line
		double c = op_sqrt((rect.width)*(rect.width)/4.0 + (rect.height)*(rect.height)/4.0);
		// Pick the right corner and find the angle there.
		double c_angle;
		if      (t >= -M_PI     && t < -M_PI/2.0) c_angle = op_atan2((double) -rect.height, -rect.width);
		else if (t >= -M_PI/2.0 && t < 0        ) c_angle = op_atan2((double) -rect.height,  rect.width);
		else if (t >= 0         && t < M_PI/2.0 ) c_angle = op_atan2((double)  rect.height,  rect.width);
		else                                      c_angle = op_atan2((double)  rect.height, -rect.width);

		double A = op_fabs(t - c_angle); // A: the angle between the corner line and the gradient_line.
		double B = M_PI/2.0 - A;         // B: the angle between the corner line and the line perpendicular to the gradient line.
		// Use the sine rule to find...
		double b = c * op_sin(B);        // b: the distance from the center of the box to the end of the gradient line
		// x, y: the end of the gradient line.
		double x = b * op_cos(t);
		double y = b * op_sin(t);

		start.x = rect.width / 2.0 - x;
		start.y = rect.height / 2.0 + y;
		end.x = rect.width / 2.0 + x;
		end.y = rect.height / 2.0 - y;
}

void
CSS_RadialGradient::CalculateEllipseCorner(const OpDoublePoint& end) const
{
	if (vrad == 0 || length == 0)
		// Degenerate; avoid divide by 0
		// MakeVEGAGradient will handle the 0-height case; CalculateShape handles 0-width
		return;

	// Find angle and distance to the corner
	double corner_dist = op_sqrt((start.x - end.x)*(start.x - end.x) + (start.y - end.y)*(start.y - end.y));
	double angle = op_atan2(end.x - start.x, end.y - start.y);

	// Current radius in the direction of the corner
	double current_rad = length * vrad / op_sqrt((length*op_cos(angle))*(length*op_cos(angle)) + ((vrad * op_sin(angle)) * (vrad * op_sin(angle))));

	// Scale the ellipse so that the circumference intersects the corner
	length = length * corner_dist / current_rad;
	vrad = vrad * corner_dist / current_rad;
}

void
CSS_RadialGradient::CalculateShape(VisualDevice* vd, const OpRect& rect) const
{
	CSSLengthResolver length_resolver(vd);
	length_resolver.FillFromVisualDevice();

	// Find the used center of the gradient.
	pos.Calculate(vd, rect, start, length_resolver);
	OpDoublePoint center = OpDoublePoint(rect.width / 2.0, rect.height / 2.0);

	if (form.has_explicit_size)
	{
		length = length_resolver.ChangePercentageBase(float(rect.width)).GetLengthInPixels(form.explicit_size[0], form.explicit_size_unit[0]);
		if (form.explicit_size[1] == -1.0)
			// Special handling, maintaining circle shape
			vrad = length;
		else
			vrad = length_resolver.ChangePercentageBase(float(rect.height)).GetLengthInPixels(form.explicit_size[1], form.explicit_size_unit[1]);
	}
	else
	{
		OpDoublePoint end;
		switch (form.shape)
		{
		case CSS_VALUE_circle:
			switch (form.size)
			{
			case CSS_VALUE_closest_side:
				vrad = length = op_fabs(MIN(MIN(start.x, rect.width - start.x), MIN(start.y, rect.height - start.y)));
				break;

			case CSS_VALUE_farthest_side:
				vrad = length = MAX(MAX(start.x, rect.width - start.x), MAX(start.y, rect.height - start.y));
				break;

			case CSS_VALUE_closest_corner:
				end.x = start.x < center.x ? 0 : rect.width;
				end.y = start.y < center.y ? 0 : rect.height;
				vrad = length = op_sqrt((start.x - end.x)*(start.x - end.x) + (start.y - end.y)*(start.y - end.y));
				break;

			case CSS_VALUE_farthest_corner:
				end.x = start.x > center.x ? 0 : rect.width;
				end.y = start.y > center.y ? 0 : rect.height;
				vrad = length = op_sqrt((start.x - end.x)*(start.x - end.x) + (start.y - end.y)*(start.y - end.y));
				break;
			}
			break;

		case CSS_VALUE_ellipse:
			switch (form.size)
			{
			case CSS_VALUE_closest_side:
			case CSS_VALUE_contain:
				length = op_fabs(MIN(start.x, rect.width - start.x));
				vrad = op_fabs(MIN(start.y, rect.height - start.y));
				break;

			case CSS_VALUE_farthest_side:
				length = MAX(start.x, rect.width - start.x);
				vrad = MAX(start.y, rect.height - start.y);
				break;

			case CSS_VALUE_closest_corner:
			{
				// Shape like for closest-side
				length = op_fabs(MIN(start.x, rect.width - start.x));
				vrad = op_fabs(MIN(start.y, rect.height - start.y));

				end.x = start.x < center.x ? 0 : rect.width;
				end.y = start.y < center.y ? 0 : rect.height;
				CalculateEllipseCorner(end);
				break;
			}

			case CSS_VALUE_farthest_corner:
			case CSS_VALUE_cover:
				// Shape like for farthest-side
				length = op_fabs(MAX(start.x, rect.width - start.x));
				vrad = op_fabs(MAX(start.y, rect.height - start.y));

				end.x = start.x > center.x ? 0 : rect.width;
				end.y = start.y > center.y ? 0 : rect.height;
				CalculateEllipseCorner(end);
				break;
			}
			break;
		}
	}

	// Degenerate: 0-width gradients are given infinite height and near-0 width
	// The 0-height case is handled in MakeVEGAGradient
	if (length <= 0)
	{
		vrad = VEGA_FIXTOFLT(VEGA_INFINITY);
		length = VEGA_FIXTOFLT(VEGA_EPSILON);
	}
}

// Serialization

void
CSS_LinearGradient::ToStringL(TempBuffer* buf) const
{
	if (line.end_set)
	{
		if (line.to)
			buf->AppendL("to ");

		if (line.y != CSS_VALUE_center)
			buf->AppendL(CSS_GetKeywordString(line.y));

		if (line.y != CSS_VALUE_center &&
			line.x != CSS_VALUE_center)
			buf->AppendL(" ");

		if (line.x != CSS_VALUE_center)
			buf->AppendL(CSS_GetKeywordString(line.x));
	}
	else if (line.angle_set)
	{
		CSS::FormatCssNumberL(line.angle, CSS_RAD, buf, FALSE);
	}

	buf->AppendL(", ");
	ColorStopsToStringL(buf);
}

void
CSS_RadialGradient::ToStringL(TempBuffer* buf) const
{
	if (form.has_explicit_size)
	{
		if(form.explicit_size[1] == -1.0)
		{
			buf->AppendL("circle");
			CSS::FormatCssNumberL(form.explicit_size[0], form.explicit_size_unit[0], buf, TRUE);
		}
		else
		{
			buf->AppendL("ellipse");
			CSS::FormatCssNumberL(form.explicit_size[0], form.explicit_size_unit[0], buf, TRUE);
			CSS::FormatCssNumberL(form.explicit_size[1], form.explicit_size_unit[1], buf, TRUE);
		}
	}
	else
	{
		buf->AppendL(CSS_GetKeywordString(form.shape));
		buf->AppendL(' ');
		buf->AppendL(CSS_GetKeywordString(form.size));
	}

	if (pos.is_set)
	{
		buf->AppendL(" at ");
		pos.ToStringL(buf);
	}
	else
	{
		buf->AppendL(" at center");
	}

	buf->AppendL(", ");

	ColorStopsToStringL(buf);
}

void
CSS_RadialGradient::Position::Calculate(VisualDevice* vd, const OpRect& rect, OpDoublePoint& out, CSSLengthResolver& length_resolver) const
{
	if (is_set)
	{
		if (has_ref)
		{
			switch (ref[0])
			{
			case CSS_VALUE_left:
				out.x = length_resolver.ChangePercentageBase(float(rect.width)).GetLengthInPixels(pos[0], pos_unit[0]);
				break;

			case CSS_VALUE_right:
				out.x = rect.width - length_resolver.ChangePercentageBase(float(rect.width)).GetLengthInPixels(pos[0], pos_unit[0]);
				break;

			default:
				OP_ASSERT(!"Unexpected <bg-position> keyword");
			}
			switch (ref[1])
			{
			case CSS_VALUE_top:
				out.y = length_resolver.ChangePercentageBase(float(rect.height)).GetLengthInPixels(pos[1], pos_unit[1]);
				break;

			case CSS_VALUE_bottom:
				out.y = rect.height - length_resolver.ChangePercentageBase(float(rect.height)).GetLengthInPixels(pos[1], pos_unit[1]);
				break;

			default:
				OP_ASSERT(!"Unexpected <bg-position> keyword");
			}
		}
		else
		{
			out.x = length_resolver.ChangePercentageBase(float(rect.width)).GetLengthInPixels(pos[0], pos_unit[0]);
			out.y = length_resolver.ChangePercentageBase(float(rect.height)).GetLengthInPixels(pos[1], pos_unit[1]);
		}
	}
	else
	{
		// Not set is equivalent to 'center'
		out.x = rect.width / 2.0;
		out.y = rect.height / 2.0;
	}
}

void
CSS_RadialGradient::Position::ToStringL(TempBuffer* buf) const
{
	if (is_set)
	{
		if (has_ref)
			buf->AppendL(CSS_GetKeywordString(ref[0]));
		CSS::FormatCssNumberL(pos[0], pos_unit[0], buf, has_ref);
		buf->AppendL(' ');

		if (has_ref)
			buf->AppendL(CSS_GetKeywordString(ref[1]));
		CSS::FormatCssNumberL(pos[1], pos_unit[1], buf, has_ref);
	}
}

void
CSS_Gradient::ColorStopsToStringL(TempBuffer* buf) const
{
	for (int i = 0; i < stop_count; i++)
	{
		if (i > 0)
			buf->AppendL(", ");
		stops[i].ToStringL(buf);
	}
}

void
CSS_Gradient::ColorStop::ToStringL(TempBuffer* buf) const
{
	/* Ideally, the CSS::FormatCssColorL should be used. We'd need to know
	   whether we are formatting for computed styles or style declarations though. */
	if (color == CSS_COLOR_transparent)
		buf->AppendL("transparent");
	else if (color == CSS_COLOR_current_color)
		buf->AppendL("currentColor");
	else
	{
		COLORREF colorval = HTM_Lex::GetColValByIndex(color);
		CSS::FormatCssValueL((void*)(INTPTR) colorval, CSS::CSS_VALUE_TYPE_color, buf, FALSE);
	}

	if (has_length)
		if (length == 0)
			buf->AppendL(" 0px");
		else
			CSS::FormatCssNumberL(length, unit, buf, TRUE);
}

// The following methods use a lot of graphics logic. Changes should be review by the GFX group.

/* static */ UINT32
CSS_RadialGradient::InterpolateStop(UINT32 prev, UINT32 next, double pos)
{
	double r, g, b, a;
	InterpolatePremultiplied(prev, next, pos, 1, r, g, b, a);
	Unpremultiply(r, g, b, a);
	return PackARGB(r, g, b, a);
}

/* static */ void
CSS_Gradient::InterpolateForAverage(UINT32 prev, UINT32 next, double weight, double& r, double& g, double& b, double& a)
{
	InterpolatePremultiplied(prev, next, 0.5, weight, r, g, b, a);
}

/* static */ void
CSS_Gradient::InterpolatePremultiplied(UINT32 prev, UINT32 next, double pos, double weight, double& r, double& g, double& b, double& a)
{
	const double alpha = (1 - pos) * UnpackAlpha(prev) + pos * UnpackAlpha(next);

	// Premultiply
	double r1 = UnpackRed  (prev) * UnpackAlpha(prev) / 255.0;
	double g1 = UnpackGreen(prev) * UnpackAlpha(prev) / 255.0;
	double b1 = UnpackBlue (prev) * UnpackAlpha(prev) / 255.0;

	double r2 = UnpackRed  (next) * UnpackAlpha(next) / 255.0;
	double g2 = UnpackGreen(next) * UnpackAlpha(next) / 255.0;
	double b2 = UnpackBlue (next) * UnpackAlpha(next) / 255.0;

	// Interpolate
	r = ((1 - pos) * r1 + pos * r2) * weight;
	g = ((1 - pos) * g1 + pos * g2) * weight;
	b = ((1 - pos) * b1 + pos * b2) * weight;
	a = (alpha) * weight;
}

void
CSS_LinearGradient::ModifyStops() const
{
	// For gradients too small to reproduce faithfully, use the 0-width averaging algorithm from the spec.
	if (repeat && VEGA_FIXTOFLT(offsets[stop_count - 1] - offsets[0]) * length < 1.0)
	{
		AveragePremultiplied(FALSE);
		return;
	}

	// Modification only necessary for repeating gradients
	if (repeat && length > 0)
	{
		double smallest = VEGA_FIXTOFLT(offsets[0]),
			   largest  = VEGA_FIXTOFLT(offsets[stop_count - 1]);

		OpDoublePoint line_start = start;

		start.x = line_start.x + smallest*length * op_cos(angle);
		start.y = line_start.y + smallest*length * op_sin(angle);
		end.x = line_start.x + largest*length * op_cos(angle);
		end.y = line_start.y + largest*length * op_sin(angle);

		double old_length = length;
		length = (largest - smallest) * length;

		NormalizeStops(old_length);
	}
}

void
CSS_RadialGradient::ModifyStops() const
{
	int last = stop_count - 1;

	if (!repeat && offsets[last] <= VEGA_INTTOFIX(1))
		return;

	// Radial gradients need additional massaging if first and last color stops are not at 0 and 1
	if ((offsets[0] == 0 || !repeat) && offsets[last] != VEGA_INTTOFIX(1))
	{
		// If the first stop is still at 0, or the gradient doesn't repeat, it's relatively easy
		double old_length = length;
		length = VEGA_FIXTOFLT(offsets[last]) * length;
		vrad = vrad * length / old_length;

		NormalizeStops(old_length);
	}
	else if (offsets[0] != 0 && repeat)
	{
		// If the first stop is not at 0 and the gradient repeats, we're in for a rough ride.

		/* 1. Find the length of the gradient line and use it as the actual radius. */
		double smallest = VEGA_FIXTOFLT(offsets[0]),
			   largest  = VEGA_FIXTOFLT(offsets[stop_count - 1]);

		double stop_distance = largest - smallest;

		/* 2. Transpose stops so that the gradient only repeats once within the new, actual radius.
		   This is to avoid duplicating more stops than absolutely necessary.
		   Transposition is only necessary if the specified stops are completely outside the new radius. */
		if (smallest >= stop_distance || smallest < 0)
		{
			int n = int(smallest / stop_distance);
			if (smallest < 0 && op_fmod(smallest, stop_distance) != 0)
				// When transposing from below zero, add an extra step to push it past 0.
				// Unless smallest is divisible by stop_distance, in which case we'll end up exactly at 0.
				n--;

			for(int i = 0; i < stop_count; i++)
				offsets[i] -= VEGA_FLTTOFIX(n * stop_distance);
		}

		/* 3. Rotate stops */
		// Find new first stop. It is the first stop whose offset is greater than the gradient length.
		int new_first = 0;
		OP_ASSERT(VEGA_FIXTOFLT(offsets[stop_count - 1]) > 0); // Loop invariant
		while (VEGA_FIXTOFLT(offsets[new_first]) - stop_distance < 0)
			new_first++;

		// CalculateStops() has already allocated the necessary space for actual_stop_count.
		actual_stop_count = stop_count + 2;

		/* Rotate */
		// First, shift array one to the right to make room for a synthetic stop.
		for (int i = stop_count; i > 0; i--)
		{
			offsets[i] = offsets[i - 1];
			colors [i] = colors [i - 1];

			// Update offsets
			if (i > new_first)
				offsets[i] -= VEGA_FLTTOFIX(stop_distance);
		}

		// Then, start shifting the stops.
		for (int i = 0; i < new_first; i++)
		{
			VEGA_FIX offset_temp = offsets[i];
			UINT32 color_temp  = colors [i];

			for (int j = 1; j < stop_count; j++)
			{
				offsets[j] = offsets[j+1];
				colors [j] = colors [j+1];
			}
			offsets[stop_count] = offset_temp;
			colors [stop_count] = color_temp;
		}

		/* 4. Create synthetic stops */
		offsets[0] = 0;
		offsets[actual_stop_count - 1] = VEGA_INTTOFIX(1);

		double delta = VEGA_FIXTOFLT(offsets[1]) + (stop_distance - VEGA_FIXTOFLT(offsets[actual_stop_count - 2]));
		// A 0-size gradient should prevent ModifyStops from being called. So delta will not be 0 and divide by 0 is prevented.
		OP_ASSERT(delta != 0);

		double pos = (delta - VEGA_FIXTOFLT(offsets[1])) / delta;
		colors[0] = colors[actual_stop_count - 1] = InterpolateStop(
			colors[actual_stop_count - 2], colors[1], pos);

		/* 5. Adjust radius */
		double old_length = length;
		length = stop_distance * length;
		vrad = vrad * length / old_length;

		for (int i = 1; i < actual_stop_count - 1; i++)
		{
			offsets[i] = VEGA_FLTTOFIX(VEGA_FIXTOFLT(offsets[i])*old_length/length);
		}
	}
	// else
	//   If the stops are already at 0 and 1, nothing needs to be done.

	// Clamp vrad at VEGA_INFINITY to avoid transform trouble
	if (vrad > VEGA_FIXTOFLT(VEGA_INFINITY))
		vrad = VEGA_FIXTOFLT(VEGA_INFINITY);

	if (actual_stop_count != -1)
		last = actual_stop_count - 1;

	// For gradients too small to reproduce faithfully, use the 0-width averaging algorithm from the spec.
	if (repeat)
	{
		// VEGA_EPSILON is used in case an exact 1.0 gets binary-rounded down or 0.0 gets rounded up
		// Less than or equals is used because length can be set to VEGA_EPSILON in CalculateShape
		if (length <= VEGA_FIXTOFLT(VEGA_EPSILON))
		{
			// Gradient line is zero; ignore stop offsets
			AveragePremultiplied(FALSE);
			return;
		}
		// The limit for vrad is smaller because it's handled using transforms rather than pixel coordinates
		else if (length < 1.0 - VEGA_FIXTOFLT(VEGA_EPSILON) || VEGA_FIXTOFLT(offsets[last] - offsets[0]) * length < 1.0 - VEGA_FIXTOFLT(VEGA_EPSILON)
			|| vrad < 0.005)
		{
			// Gradient line or vertical radius too short; average while preserving stop offsets
			AveragePremultiplied(TRUE);
			return;
		}
	}
}

BOOL
CSS_Gradient::CalculateStops(VisualDevice* vd, COLORREF current_color) const
{
	const int last = stop_count - 1;
	OP_ASSERT(stop_count >= 2); // Checked by SetGradient

	CSSLengthResolver length_resolver(vd);
	length_resolver.FillFromVisualDevice();
	VEGA_FIX largest = -VEGA_INFINITY;

	/* This should not happen, but just in case other code is changed and starts rendering
		the same gradient multiple times: */
	OP_DELETEA(offsets);
	OP_DELETEA(colors);

	// Allocate two extra stops, in case CSS_RadialGradient::ModifyStops needs to add and duplicate the 0 stop.
	colors = OP_NEWA(UINT32, stop_count + 2);
	offsets = OP_NEWA(VEGA_FIX, stop_count + 2);
	if (!colors || !offsets)
		return FALSE;

	// Stops with specified or default lengths
	for (int i = 0; i < stop_count; i++)
	{
		if (stops[i].has_length)
		{
			if (stops[i].unit == CSS_PERCENTAGE)
			{
				offsets[i] = VEGA_FLTTOFIX(stops[i].length)/100;
			}
			else
			{
				// Since VEGA color stops are normalized, absolute lengths must be converted.
				int stop_length = length_resolver.ChangePercentageBase(float(length)).GetLengthInPixels(stops[i].length, stops[i].unit);
				offsets[i] = VEGA_FLTTOFIX(stop_length/length);
			}

			// Rule 2: If a color-stop's position is less than any previous one, match it to the largest
			if (offsets[i] < largest)
				offsets[i] = largest;

			largest = offsets[i];
		}
		// Rule 1: Default positions for first and last stops, when no length is given or inferred.
		else if (i == 0)
			largest = offsets[0] = 0;
		else if (i == last)
			if (VEGA_INTTOFIX(1) < largest)
				offsets[last] = largest;
			else
				offsets[last] = VEGA_INTTOFIX(1);
	}

	int next_with_length = 0,
		last_with_length = 0;
	double next_length = 0,
		last_length = 0;

	// Stops without specified lengths
	// Rule 3: Spread lengthless stops evenly between stops with lengths
	for (int i = 0; i < stop_count; i++)
		if (stops[i].has_length || i == 0 || i == last)
		{
			// First and last always have lengths by now
			// Keep track of the last specified length
			last_length = VEGA_FIXTOFLT(offsets[i]);
			last_with_length = i;
		}
		else
		{
			// No length specified
			if (next_with_length < i)
			{
				// Find the next stop with a length (last one always has length)
				next_with_length = i + 1;
				while (!(stops[next_with_length].has_length || next_with_length == last))
					next_with_length++;

				next_length = VEGA_FIXTOFLT(offsets[next_with_length]);
			}
			// We're in a run of lengthless stops
			offsets[i] = VEGA_FLTTOFIX(last_length + (next_length - last_length) * (i - last_with_length) / (next_with_length - last_with_length));
		}

	for (int i = 0; i < stop_count; i++)
	{
		if (stops[i].color == CSS_COLOR_transparent)
			colors[i] = OP_COLORREF2VEGA(OP_RGBA(0,0,0,0));
		else if (stops[i].color == CSS_COLOR_current_color)
			colors[i] = OP_COLORREF2VEGA(HTM_Lex::GetColValByIndex(current_color));
		else
			colors[i] = OP_COLORREF2VEGA(HTM_Lex::GetColValByIndex(stops[i].color));
	}

	ModifyStops();

	return TRUE;
}

void
CSS_Gradient::AveragePremultiplied(BOOL preserve_offsets) const
{
	double r = 0,
		g = 0,
		b = 0,
		a = 0;

	for (int i = 0; i < stop_count - 1; i++) // 'stop_count - 1' because the stops are evaluated in pairs
	{
		double weight;
		if (preserve_offsets)
			weight = (offsets[i + 1] - offsets[i]);
		else
			weight = 1.0 / double(stop_count - 1);

		double new_r, new_b, new_g, new_a;
		InterpolateForAverage(colors[i], colors[i + 1], weight, new_r, new_g, new_b, new_a);
		r += new_r;
		g += new_g;
		b += new_b;
		a += new_a;
	}
	UINT32 average;
	Unpremultiply(r, g, b, a);
	average = PackARGB(r, g, b, a);

	use_average_color = TRUE;
	// use_average_color is a signal that there are only two stops.
	for (int i = 0; i < 2; i++)
		colors[i] = average;
}

VEGAFill*
CSS_LinearGradient::MakeVEGAGradient(VisualDevice* vd, VEGAOpPainter* vpainter, const OpRect& rect, COLORREF current_color, VEGATransform&) const
{
	use_average_color = FALSE;
	CalculateShape(vd, rect);
	if (!CalculateStops(vd, current_color))
		return NULL;

	// Handle average color
	if (use_average_color)
		return vpainter->CreateLinearGradient(
		0, 0, 0, 0,
		2,
		offsets, colors, TRUE /* premultiplied */);

	// Scale all rendering data to current zoom and transform
	start = vd->ToPainterExtentDbl(start);
	end = vd->ToPainterExtentDbl(end);

	// Create VEGAGradient
	VEGAFill* gradient_out = vpainter->CreateLinearGradient(
		VEGA_FLTTOFIX(start.x),
		VEGA_FLTTOFIX(start.y),
		VEGA_FLTTOFIX(end.x),
		VEGA_FLTTOFIX(end.y),
		stop_count,
		offsets, colors, TRUE /* premultiplied */);

	if (repeat)
		gradient_out->setSpread(VEGAFill::SPREAD_REPEAT);

	return gradient_out;
}

VEGAFill*
CSS_RadialGradient::MakeVEGAGradient(VisualDevice* vd, VEGAOpPainter* vpainter, const OpRect& rect, COLORREF current_color, VEGATransform& radial_adjust) const
{
	use_average_color = FALSE;
	CalculateShape(vd, rect);
	if (!CalculateStops(vd, current_color))
		return NULL;

	// Handle average color
	if (use_average_color)
		return vpainter->CreateRadialGradient(
			0, 0, 0, 0,
			0, 2,
			offsets, colors, FALSE /* premultiplied */);

	// Degenerate: If the radial gradient has 0 height, draw last color stop only.
	// The 0-width case is handled in CalculateShape
	if (vrad <= 0)
	{
		return vpainter->CreateRadialGradient(0, 0, 0, 0, 0, stop_count, offsets, colors, TRUE);
	}

	// Scale all rendering data to current zoom and transform
	start = vd->ToPainterExtentDbl(start);
	length = vd->ToPainterExtentDbl(length);
	vrad = vd->ToPainterExtentDbl(vrad);

	// Create VEGAGradient
	VEGAFill* gradient_out = vpainter->CreateRadialGradient(
		VEGA_FLTTOFIX(start.x),
		VEGA_FLTTOFIX(start.y),
		VEGA_FLTTOFIX(start.x),
		VEGA_FLTTOFIX(start.y),
		VEGA_FLTTOFIX(length),
		actual_stop_count == -1 ? stop_count : actual_stop_count,
		offsets, colors, TRUE /* premultiplied */);

	// VEGA only does circular gradients; if elliptic, adjust with fill-transform
	// Inaccuracy does little harm, so epsilon is not needed
	if (length != vrad)
	{
		VEGATransform tx;
		tx.loadTranslate(VEGA_FLTTOFIX(start.x), VEGA_FLTTOFIX(start.y));
		radial_adjust.multiply(tx);
		tx.loadScale(1, VEGA_FLTTOFIX(vrad / length));
		radial_adjust.multiply(tx);
		tx.loadTranslate(VEGA_FLTTOFIX(-start.x), VEGA_FLTTOFIX(-start.y));
		radial_adjust.multiply(tx);
	}

	if (repeat)
		gradient_out->setSpread(VEGAFill::SPREAD_REPEAT);

	return gradient_out;
}

void
CSS_LinearGradient::NormalizeStops(double old_length) const
{
	double smallest = VEGA_FIXTOFLT(offsets[0]);

	if (length != 0)
		for (int i = 0; i < stop_count; i++)
			offsets[i] = VEGA_FLTTOFIX((VEGA_FIXTOFLT(offsets[i]) - smallest) * old_length / length);
	else
		for (int i = 0; i < stop_count; i++)
			offsets[i] = 0;
}

void
CSS_RadialGradient::NormalizeStops(double old_length) const
{
	if (length != 0)
		for (int i = 0; i < stop_count; i++)
			offsets[i] = VEGA_FLTTOFIX((VEGA_FIXTOFLT(offsets[i])) * old_length / length);
	else
		for (int i = 0; i < stop_count; i++)
			offsets[i] = 0;
}
#endif // CSS_GRADIENT_SUPPORT
