/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_GRADIENT_H
#define CSS_GRADIENT_H

#include "modules/style/css_value_types.h"

#ifdef VEGA_OPPAINTER_SUPPORT
# define CSS_GRADIENT_SUPPORT
#endif // VEGA_OPPAINTER_SUPPORT

#ifdef CSS_GRADIENT_SUPPORT

class TempBuffer;
class OpDoublePoint;
class VisualDevice;
class VEGAOpPainter;
class VEGAFill;
class VEGATransform;
class CSSLengthResolver;

/**
	This implementation follows the specification at http://www.w3.org/TR/2011/WD-css3-images-20110908/.

	@author lstorset
*/
class CSS_Gradient
{
public:
	struct ColorStop
	{
		BOOL     has_length;
		COLORREF color;

		float    length;
		short    unit;

		ColorStop() :
			has_length(FALSE),
			color(USE_DEFAULT_COLOR)
		{}

		inline void ToStringL(TempBuffer* buf) const;
	};

	ColorStop* stops;
	short stop_count;
	BOOL repeat:1;
	BOOL webkit_prefix:1;
	BOOL o_prefix:1;

	CSS_Gradient() :
		stops(NULL), stop_count(0), repeat(FALSE), webkit_prefix(FALSE), o_prefix(FALSE), offsets(NULL), colors(NULL), use_average_color(FALSE) {};

	enum Type
	{
		LINEAR,
		RADIAL
	};

	virtual Type GetType() const = 0;

	/** Convenience function. */
	CSSValueType GetCSSValueType() const;

protected:
	/** @name Rendering details

		These mutable fields contain concrete rendering information based on the abstract gradient.
		CalculateShape() will populate them.

		@{ */
	/** The start of the gradient. For linear gradients, it's the start of the gradient line;
		for radial gradients, the center point. */
	mutable OpDoublePoint start;

	/** Length of the line used to place color stops. For radial gradients, this means
		the horizontal radius; for linear gradients, it means the gradient line. */
	mutable double length;

	/** Color-stop offsets as sent to VEGA. These are relative to the start and end of the gradient line,
		or the center and edge for radial gradients.  */
	mutable VEGA_FIX* offsets;

	/** Stop colors as sent to VEGA. The COLORREFS are transformed into UINT32s. */

	mutable UINT32* colors;

	/** A signal to MakeVEGAGradient that the averaging algorithm has been invoked, and the gradient
		must be drawn as simply as possible. */
	mutable BOOL use_average_color;

	/** @} */

	/** Given a specific VisualDevice and rendering rectangle, calculate the shape of this gradient.

		This will populate the mutable "rendering details" fields.

		@param      vd       the VisualDevice for which to calculate the gradient.
		@param      rect     the rendering area for the gradient.
	*/
	virtual void CalculateShape(VisualDevice* vd, const OpRect& rect) const = 0;

	/** Converts color stops to use VEGA_FIX percentages only. Applies the rules found in the spec
		under "Color-stop syntax":

		-# If the first color-stop does not have a position, its position defaults to 0%. If the last
		   color-stop does not have a position, its position defaults to 100%.
		-# If a color-stop has a position that is less than the specified position of any color-stop
		   before it in the list, its position is changed to be equal to the largest specified position
		   of any color-stop before it.
		-# If any color-stop still does not have a position, then, for each run of adjacent color-stops
		   without positions, set their positions so that they are evenly spaced between the preceding
		   and following color-stops with positions.

		This method modifies mutable fields.

		@param vd the VisualDevice whose metrics to use.
		@param current_color the value of currentColor.

		@return TRUE if all went well, or FALSE if OOM.
	*/
	BOOL CalculateStops(VisualDevice* vd, COLORREF current_color) const;

	/** Change all stops so their positions are relative to the first and last stop.
		This is a helper for ModifyStops().

		@param old_length the length of the gradient line before it was modified.
	*/
	virtual void NormalizeStops(double old_length) const = 0;

	/** Modifies stops so that the gradient can be converted to a VEGA Gradient. */
	virtual void ModifyStops() const = 0;

	/** @name Pixel accessors

		Helpers for CSS_RadialGradient::InterpolatePremultiplied and InterpolateForAverage().

		@{ */
	static inline unsigned char UnpackAlpha(UINT32 color) { return color >> 24       ; }
	static inline unsigned char UnpackRed  (UINT32 color) { return color >> 16 & 0xff; }
	static inline unsigned char UnpackGreen(UINT32 color) { return color >> 8  & 0xff; }
	static inline unsigned char UnpackBlue (UINT32 color) { return color       & 0xff; }
	static inline void Unpremultiply(double& r, double& g, double& b, double a)
	{
		if (a == 0)
			r = g = b = 0;
		else
		{
			r /= a / 255;
			g /= a / 255;
			b /= a / 255;
		}
	}

	static inline UINT32 PackARGB(double red, double green, double blue, double alpha)
	{
		return
			  unsigned (alpha + 0.5) << 24
			| unsigned (red   + 0.5) << 16
			| unsigned (green + 0.5) << 8
			| unsigned (blue  + 0.5);
	}
	/** @} */

	/** Finds the gradient's average color, for use when the gradient-line is close to 0 length.

		@param preserve_offsets whether to preserve the offsets, which happens in the second
		                        version of the specified algorithm, which is used when the
								gradient line is long enough but the radial gradient's height
								is tiny.
	*/
	void AveragePremultiplied(BOOL preserve_offsets) const;

	/** Helper for AveragePremultiplied(). Repurposes the interpolation logic to find the
		average of two stops, weighted by the provided parameter.

		@param prev   the previous stop's color.
		@param next   the next stop's color.
		@param weight the weight of this pair of stops. Note that the specified algorithm talks about the weight
		              of the individual stops, so this will be double that.
		@param[out] r the resulting premultiplied red value.
		@param[out] g the resulting premultiplied green value.
		@param[out] b the resulting premultiplied blue value.
		@param[out] a the resulting alpha value.
	*/
	static inline void InterpolateForAverage(UINT32 prev, UINT32 next, double weight, double& r, double& g, double& b, double& a);

	/** Common functionality for CSS_RadialGradient::InterpolateStop() and InterpolateForAverage().
		Taken from http://lists.w3.org/Archives/Public/www-style/2010Aug/0589.html.
		See also http://keithp.com/~keithp/porterduff/.

		@param prev   the previous stop.
		@param prev   the next stop.
		@param pos    position whose color to calculate.
		@param weight weight by which to scale the result.
		@param[out] r premultiplied red result.
		@param[out] g premultiplied green result.
		@param[out] b premultiplied blue result.
		@param[out] a alpha result.
	*/
	static inline void InterpolatePremultiplied(UINT32 prev, UINT32 next, double pos, double weight, double& r, double& g, double& b, double& a);


private:
	/** Copies all the stops in s to this CSS_Gradient object.

	    @param s the stops to copy.
	    @param n the number of stops.
	*/
	OP_STATUS CopyStops(ColorStop* s, int n)
	{
		OP_DELETEA(stops);
		stops = OP_NEWA(ColorStop, (stop_count = n));
		if (stops)
		{
			for (int i = 0; i < n; i++)
				stops[i] = s[i];
			return OpStatus::OK;
		}
		else
			return OpStatus::ERR_NO_MEMORY;
	}

	/* Use ::CopyTo instead. It will perform memory allocation. */
	DEPRECATED(CSS_Gradient(const CSS_Gradient& other))
	{
		OP_ASSERT(!"CSS_Gradient copy constructor is invalid.");
	}

	/* Use ::CopyTo instead. It will perform memory allocation. */
	DEPRECATED(CSS_Gradient& operator=(const CSS_Gradient& other))
	{
		OP_ASSERT(!"CSS_Gradient copy assignment is invalid.");
		return *this;
	}

protected:
	void ColorStopsToStringL(TempBuffer* buf) const;

	OP_STATUS CopyTo(CSS_Gradient& copy) const;

public:

	/** Convenience function that returns a pointer to a newly-constructed copy of this gradient. */
	virtual CSS_Gradient* Copy() const = 0;

	/** Convenience function that returns a pointer to a newly-constructed copy of this gradient. */
	virtual CSS_Gradient* CopyL() const = 0;

	/** Allocates a VEGAGradient, converts this gradient and returns a pointer.
		@param      rect          the rendering area for the gradient.
		@param      current_color the value of currentColor.
		@param[out] adjustments   filled with necessary adjustments to the fill transform.

		@return see VegaOpPainter::CreateLinearGradient and VegaOpPainter::CreateRadialGradient.
		        Notably, NULL on OOM.
	*/
	virtual VEGAFill* MakeVEGAGradient(VisualDevice* vd, VEGAOpPainter* painter, const OpRect& rect,
	                                   COLORREF current_color, VEGATransform& adjustments) const = 0;

	virtual ~CSS_Gradient()
	{
		OP_DELETEA(stops);
		OP_DELETEA(offsets);
		OP_DELETEA(colors);
	}

	BOOL IsOpaque() const;

	virtual void ToStringL(TempBuffer* buf) const = 0;
};

class CSS_LinearGradient : public CSS_Gradient
{
public:
	struct GradientLine
	{
		/** The ending point of the gradient line. */
		CSSValue x, y;
		/** The angle of the gradient line. */
		float angle;
		bool end_set:1;
		bool angle_set:1;

		// Whether the new 'to' syntax is used. Remove when -o- prefix is removed.
		bool to:1;
	} line;

protected:

	/** @name Rendering details

		These mutable fields contain concrete rendering information based on the abstract gradient.
		CalculateShape() will populate them.
		@{ */
	/** The end of the gradient line. */
	mutable OpDoublePoint end;

	/** The angle of the gradient. */
	mutable double angle;
	/** @} */

	/** Find the angle and length of the gradient (the "gradient line").

		- If a box side is supplied, the gradient is simply drawn from there to the opposite side.
		- If a box corner is supplied, an angle perpendicular to the angle to the neighboring corner is passed to
		  CalculateShape(double, const OpRect&) const.
		- If an angle is supplied, CalculateShape(double, const OpRect&) const is called.

		This method will populate the following fields:

		- \c start    the start of the gradient line, relative to @c rect.
		- \c end      the end of the gradient line, relative to @c rect.
		- \c length   the length of the gradient line in pixels.
		- \c angle    the angle of the gradient line in radians, where 0rad is to the right and angles increase counterclockwise.

		@param      vd       the VisualDevice for which to calculate the gradient.
		@param      rect     the rendering area for the gradient.
	*/
	void CalculateShape(VisualDevice* vd, const OpRect& rect) const;

private:
	/** Helper for CalculateShape(const VisualDevice*, const OpRect&) const.

		Calculates the angle using the following algorithm (see figure):
		\image html CalculateGradientLine.svg

		Given a triangle ABC where:
		<ul>
			<li>A is the center of the box
			<li>B is the corner closest to the gradient line in either direction
			<li>C is the point on the gradient line where a line drawn perpendicular to the gradient line
				would intersect B
		</ul>

		<ol>
			<li>Find the distance c = AB using the Pythagorean theorem.
			<li>Find the angle c_angle of AB, where 0deg points to the right and angles increase counter-clockwise.
			<li>Given the gradient angle t, the angles of ABC are: <pre>
				A = abs(t - c_angle)
				B = 90deg - A
				C = 90deg </pre>
			<li>Use the sine rule to find the distance b = AC: <pre>
				sin(B) / b = sin(C) / c
				b = c*sin(B) </pre>
			<li>Now the relative position of the end of the gradient line in terms of x and y can be found
				by <code>x = b*cos(t)</code> and <code>y = b*sin(t)</code>.
		</ol>
	*/
	void CalculateShape(double angle, const OpRect& rect) const;

protected:

	/** Finds the position of the first and last stop and adjusts the position of all color stops to be
		relative to these stops, and size the gradient so that its edges coincide with them. This way we
		can simply tell VEGAGradient to SPREAD_REPEAT.

		Also averages colors of repeating gradients shorter than 1px.

		This method modifies mutable fields.
	*/
	void ModifyStops() const;

	virtual void NormalizeStops(double old_length) const;

	OP_STATUS CopyToLinear(CSS_LinearGradient& copy) const;

public:

	CSS_LinearGradient() : CSS_Gradient()
	{
		line.x = CSS_VALUE_center;
		line.y = CSS_VALUE_bottom;
		line.end_set = line.angle_set = FALSE;
		line.to = FALSE;
	}

	Type GetType() const { return LINEAR; }

	CSS_LinearGradient* CopyL() const;

	CSS_LinearGradient* Copy() const;

	VEGAFill* MakeVEGAGradient(VisualDevice* vd, VEGAOpPainter* painter, const OpRect& rect,
	                           COLORREF current_color, VEGATransform&) const;

	void ToStringL(TempBuffer* buf) const;
};

class CSS_RadialGradient : public CSS_Gradient
{
public:
	struct Position
	{
		bool is_set:1;
		bool has_ref:1;

		float pos[2];
		CSSValueType pos_unit[2];
		CSSValue ref[2];

		Position() : is_set(FALSE), has_ref(FALSE) {}

		/** Turn this Position into concrete coordinates using a given rect. */
		void Calculate(VisualDevice* vd, const OpRect& rect, OpDoublePoint& pos, CSSLengthResolver& length_resolver) const;

		void ToStringL(TempBuffer* buf) const;
	} pos;

	struct ShapeSize
	{
		CSSValue shape;
		CSSValue size;

		BOOL has_explicit_size;
		/** explicit_size[0] is the horizontal dimension and
			explicit_size[1] the vertical one.

			If explicit_size[1] is set to -1.0, it indicates that the shape is
			a circle whose radius is in explicit_size[0]. */
		float explicit_size[2];
		CSSValueType explicit_size_unit[2];

		ShapeSize() : shape(CSS_VALUE_ellipse), size(CSS_VALUE_farthest_corner), has_explicit_size(FALSE) {};
	} form;

protected:
	/** @name Rendering details

		These mutable fields contain concrete rendering information based on the abstract gradient.
		CalculateShape() will populate them.
		@{ */
	/** The vertical radius of the ellipse. */
	mutable double vrad;

	/** Repeating radial gradients often need to duplicate stops; this is the actual count. */
	mutable int actual_stop_count;
	/** @} */

	/** Find the radius or radii of the gradient.

		This method will populate the following fields:

		- \c length   the radius of the gradient.
		- \c start    the center of the gradient.

		@param      vd       the VisualDevice for which to calculate the gradient.
		@param      rect     the rendering area for the gradient.
	*/
	void CalculateShape(VisualDevice* vd, const OpRect& rect) const;

private:
	/** Helper for CalculateShape() when <shape> is 'ellipse closest-corner'.

		@param end The position of the ellipse corner to reach for.
	*/
	void CalculateEllipseCorner(const OpDoublePoint& end) const;

	/** Helper for ModifyStops() used when synthesizing stops for repeating radial gradients.

		@param prev the previous color.
		@param next the next color.
		@param pos  the position of the interleaved stop.

		@return the unpremultiplied color representation.
	*/
	static inline UINT32 InterpolateStop(UINT32 prev, UINT32 next, double pos);

protected:

	/**	Modifying the stops for repeat is simple if the first stop is still at 0, but very involved
		if it isn't.

		Modification is necessary because repeating VEGA gradients have different semantics than CSS
		Gradients when they repeat, and they don't support first and last stops very well if they
		aren't positioned at 0% or 100%.

		If the the first stop is at 0% and the last stop is at 100%, nothing needs to be done. If the
		last is not at 100% but the first stop is still at 0%, the gradient can simply be resized so
		that the last stop is at 100%.

		If the first stop is not at 0%, a number of transformations are employed to turn, for instance,

		<pre>repeating-radial-gradient(red 60px, lime 70px, blue 110px)</pre>

		into a VEGA gradient with length 50px and color stops corresponding to

		<pre>rgb(0, 64, 192) 0%, blue 10px, red 10px, lime 20px, rgb(0, 64, 192) 100%</pre>

		using the following algorithm:

		-# Find the length of the gradient line and use it as the actual radius.
		-# Transpose all stops by the gradient-line length so that the first stop is within the
		   actual radius.
		-# Take stops that are outside the actual radius and transpose them left by the gradient-line
		   length. They will end up before the formerly first stop. (This is referred to as 'rotation'
		   in the code.)
		-# Create synthetic stops at 0 and at the actual radius. The stops' colors are determined
		   using premultiplied interpolation between the actual first and last non-synthetic stops,
		   and are intended to be indistinguishable from what VEGA would draw in gradient rendering.
		-# Apply the actual radius and normalize the stops to match it.

		Also averages repeating gradients that are shorter than 1px.
	*/
	void ModifyStops() const;

	virtual void NormalizeStops(double old_length) const;

	OP_STATUS CopyToRadial(CSS_RadialGradient& copy) const;

public:

	CSS_RadialGradient() : CSS_Gradient(), actual_stop_count(-1) {};

	Type GetType() const { return RADIAL; }

	CSS_RadialGradient* CopyL() const;

	CSS_RadialGradient* Copy() const;

	VEGAFill* MakeVEGAGradient(VisualDevice* vd, VEGAOpPainter* painter, const OpRect& rect,
	                           COLORREF current_color, VEGATransform& adjustments) const;

	void ToStringL(TempBuffer* buf) const;
};
#endif // CSS_GRADIENT_SUPPORT

#endif // CSS_GRADIENT_H
