/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef PIXELSCALER_H
#define PIXELSCALER_H

#ifdef PIXEL_SCALE_RENDERING_SUPPORT

class PixelScaler
{
public:
	int m_multiplier;
	int m_divider;

	PixelScaler() : m_multiplier(1), m_divider(1) {}
	PixelScaler(int scale) : m_multiplier(1), m_divider(1) { SetScale(scale); }

	int SetScale(int scale)
	{
		int r;
		int b = 100;
		int common_divisor = scale;

		while (b)
		{
			r = common_divisor % b;
			common_divisor = b;
			b = r;
		}

		int oldscale = GetScale();
		m_multiplier = scale / common_divisor;
		m_divider = 100 / common_divisor;
		OP_ASSERT(m_multiplier * 100 / m_divider == scale);

		return oldscale;
	}

	int GetScale() const
	{
		if (m_multiplier == m_divider)
			return 100;

		return 100 * m_multiplier / m_divider;
	}

	int ToDevicePixel(int src) const
	{
		if (m_multiplier == m_divider)
			return src;

		return src * m_multiplier / m_divider;
	}

	unsigned ToDevicePixel(unsigned src) const
	{
		if (m_multiplier == m_divider)
			return src;

		return src * m_multiplier / m_divider;
	}

	long ToDevicePixel(long src) const
	{
		if (m_multiplier == m_divider)
			return src;

		return src * m_multiplier / m_divider;
	}

	float ToDevicePixel(float src) const
	{
		if (m_multiplier == m_divider)
			return src;

		return src * float(m_multiplier) / m_divider;
	}

	OpPoint ToDevicePixel(const OpPoint& src) const
	{
		if (m_multiplier == m_divider)
			return src;

		return OpPoint(ToDevicePixel(src.x), ToDevicePixel(src.y));
	}

	OpRect ToDevicePixel(const OpRect& src) const
	{
		if (m_multiplier == m_divider)
			return src;

		int left = ToDevicePixel(src.x);
		int top = ToDevicePixel(src.y);
		int right = ToDevicePixel(src.Right());
		int bottom = ToDevicePixel(src.Bottom());

		return OpRect(left, top, right - left, bottom - top);
	}

	OpRect* ToDevicePixel(OpRect* src) const
	{
		if (m_multiplier == m_divider)
			return src;

		int left = ToDevicePixel(src->x);
		int top = ToDevicePixel(src->y);
		int right = ToDevicePixel(src->Right());
		int bottom = ToDevicePixel(src->Bottom());

		src->x = left;
		src->y = top;
		src->width = right - left;
		src->height = bottom - top;

		return src;
	}

	int FromDevicePixel(int src) const
	{
		if (m_multiplier == m_divider)
			return src;

		return (src * m_divider + m_multiplier - 1) / m_multiplier;
	}

	unsigned FromDevicePixel(unsigned src) const
	{
		if (m_multiplier == m_divider)
			return src;

		return (src * m_divider + m_multiplier - 1) / m_multiplier;
	}

	long FromDevicePixel(long src) const
	{
		if (m_multiplier == m_divider)
			return src;

		return (src * m_divider + m_multiplier - 1) / m_multiplier;
	}

	float FromDevicePixel(float src) const
	{
		if (m_multiplier == m_divider)
			return src;

		return src * float(m_divider) / m_multiplier;
	}

	OpPoint FromDevicePixel(const OpPoint& src) const
	{
		if (m_multiplier == m_divider)
			return src;

		return OpPoint(FromDevicePixel(src.x), FromDevicePixel(src.y));
	}

	OpRect FromDevicePixel(const OpRect& src) const
	{
		if (m_multiplier == m_divider)
			return src;

		int left = FromDevicePixel(src.x);
		int top = FromDevicePixel(src.y);
		int right = FromDevicePixel(src.Right());
		int bottom = FromDevicePixel(src.Bottom());

		return OpRect(left, top, right - left, bottom - top);
	}

	OpRect* FromDevicePixel(OpRect* src) const
	{
		if (m_multiplier == m_divider)
			return src;

		int left = FromDevicePixel(src->x);
		int top = FromDevicePixel(src->y);
		int right = FromDevicePixel(src->Right());
		int bottom = FromDevicePixel(src->Bottom());

		src->x = left;
		src->y = top;
		src->width = right - left;
		src->height = bottom - top;

		return src;
	}
};

#endif // PIXEL_SCALE_RENDERING_SUPPORT


#ifdef PIXEL_SCALE_RENDERING_SUPPORT

#define TO_DEVICE_PIXEL(scaler, value)	(scaler.ToDevicePixel(value))
#define FROM_DEVICE_PIXEL(scaler, value)	(scaler.FromDevicePixel(value))

#else

#define TO_DEVICE_PIXEL(scaler, value)	(value)
#define FROM_DEVICE_PIXEL(scaler, value)	(value)

#endif // PIXEL_SCALE_RENDERING_SUPPORT

#endif // PIXELSCALER_H
