/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef CSS_VIEWPORT_SUPPORT

#include "modules/style/css_viewport_meta.h"

static inline BOOL vpmeta_isspace(uni_char c) { return (c == 0x09 || c == 0x0a || c == 0x0d || c == 0x20); }

void
CSS_ViewportMeta::ParseContent(const uni_char* content)
{
	Reset();

	const uni_char* tmp = content;

	while (tmp && *tmp)
	{
		while (vpmeta_isspace(*tmp) || *tmp == ',' || *tmp == ';' || *tmp == '=')
			tmp++;

		if (*tmp)
			ParseProperty(tmp);
	}

	/* "For a viewport META element that translates into an @viewport rule with no max-zoom declaration
		and a non-'auto' min-zoom value that is larger than the max-zoom value of the UA stylesheet,
		the min-zoom declaration value is clamped to the UA stylesheet max-zoom value." */
	if (m_min_scale != CSS_VIEWPORT_ZOOM_AUTO &&
		m_max_scale == CSS_VIEWPORT_ZOOM_AUTO &&
		m_min_scale > CSS_VIEWPORT_DEFAULT_MAX_ZOOM)
		m_min_scale = CSS_VIEWPORT_DEFAULT_MAX_ZOOM;
}

void CSS_ViewportMeta::ParseProperty(const uni_char*& content)
{
	typedef enum {
		UnknownProperty,
		ViewportWidth,
		ViewportHeight,
		ViewportMinScale,
		ViewportMaxScale,
		ViewportInitScale,
		ViewportUserScale,
	} ViewportProperty;

	typedef enum {
		UnknownIdent,
		Yes,
		No,
		DeviceWidth,
		DeviceHeight
	} ViewportIdent;

	ViewportProperty prop(UnknownProperty);

	const uni_char* prop_start = content;

	// Find end of property name
	while (*content && *content != '=' && *content != ',' && *content != ';' && !vpmeta_isspace(*content))
		content++;

	if (*content == 0 || *content == ',' || *content == ';')
		return;

	// Find property name length
	int prop_len = content - prop_start;

	// Match property names
	if (prop_len == 5 && uni_strnicmp(prop_start, "WIDTH", 5) == 0)
		prop = ViewportWidth;
	else if (prop_len == 6 && uni_strnicmp(prop_start, "HEIGHT", 6) == 0)
		prop = ViewportHeight;
	else if (prop_len == 13)
	{
		if (uni_strnicmp(prop_start, "MINIMUM-SCALE", 13) == 0)
			prop = ViewportMinScale;
		else if (uni_strnicmp(prop_start, "MAXIMUM-SCALE", 13) == 0)
			prop = ViewportMaxScale;
		else if (uni_strnicmp(prop_start, "INITIAL-SCALE", 13) == 0)
			prop = ViewportInitScale;
		else if (uni_strnicmp(prop_start, "USER-SCALABLE", 13) == 0)
			prop = ViewportUserScale;
	}

	// Find equals sign.
	content = uni_strpbrk(content, UNI_L("=,;"));

	// End of attribute, or next property.
	if (!content || *content == ',' || *content == ';')
		return;

	// Skip whitespaces _and_ equals signs before value.
	while (*content == '=' || vpmeta_isspace(*content))
		content++;

	const uni_char* val_start = content;

	// Find end of the value.
	while (*content && *content != '=' && *content != ',' && *content != ';' && !vpmeta_isspace(*content))
		content++;

	// Find value length.
	int val_len = content - val_start;

	// Empty value.
	if (val_len == 0)
		return;

	// Parse the value.
	double num(0);
	ViewportIdent ident(UnknownIdent);

	uni_char* end;
	num = uni_strtod(val_start, &end);
	BOOL is_num = end != val_start;

	if (!is_num)
	{
		if (val_len == 2 && uni_strnicmp(val_start, "NO", 2) == 0)
			ident = No;
		else if (val_len == 3 && uni_strnicmp(val_start, "YES", 3) == 0)
			ident = Yes;
		else if (val_len == 12 && uni_strnicmp(val_start, "DEVICE-WIDTH", 12) == 0)
			ident = DeviceWidth;
		else if (val_len == 13 && uni_strnicmp(val_start, "DEVICE-HEIGHT", 13) == 0)
			ident = DeviceHeight;
	}

	// Store parsed value.
	switch (prop)
	{
	case ViewportWidth:
		if (is_num)
		{
			if (num >= 0)
			{
				m_width = CSS_ViewportLength(num, CSS_ViewportLength::PX);
				m_width.Clamp(VIEWPORT_META_LENGTH_MIN, VIEWPORT_META_LENGTH_MAX);
			}
		}
		else if (ident == DeviceWidth)
			m_width = CSS_ViewportLength(CSS_ViewportLength::DEVICE_WIDTH);
		else if (ident == DeviceHeight)
			m_width = CSS_ViewportLength(CSS_ViewportLength::DEVICE_HEIGHT);
		else
			m_width = CSS_ViewportLength(0, CSS_ViewportLength::PX);
		break;

	case ViewportHeight:
		if (is_num)
		{
			if (num >= 0)
			{
				m_height = CSS_ViewportLength(num, CSS_ViewportLength::PX);
				m_height.Clamp(VIEWPORT_META_LENGTH_MIN, VIEWPORT_META_LENGTH_MAX);
			}
		}
		else if (ident == DeviceWidth)
			m_height = CSS_ViewportLength(CSS_ViewportLength::DEVICE_WIDTH);
		else if (ident == DeviceHeight)
			m_height = CSS_ViewportLength(CSS_ViewportLength::DEVICE_HEIGHT);
		else
			m_height = CSS_ViewportLength(0, CSS_ViewportLength::PX);
		break;

	case ViewportMinScale:
		if (is_num)
		{
			if (num >= 0)
				m_min_scale = MAX(VIEWPORT_META_SCALE_MIN, MIN(VIEWPORT_META_SCALE_MAX, num));
		}
		else if (ident == Yes)
			m_min_scale = 1;
		else if (ident == DeviceWidth || ident == DeviceHeight)
			m_min_scale = 10;
		else
			m_min_scale = 0;
		break;

	case ViewportMaxScale:
		if (is_num)
		{
			if (num >= 0)
				m_max_scale = MAX(VIEWPORT_META_SCALE_MIN, MIN(VIEWPORT_META_SCALE_MAX, num));
		}
		else if (ident == Yes)
			m_max_scale = 1;
		else if (ident == DeviceWidth || ident == DeviceHeight)
			m_max_scale = 10;
		else
			m_max_scale = 0;
		break;

	case ViewportInitScale:
		if (is_num)
		{
			if (num >= 0)
				m_init_scale = MAX(VIEWPORT_META_SCALE_MIN, MIN(VIEWPORT_META_SCALE_MAX, num));
		}
		else if (ident == Yes)
			m_init_scale = 1;
		else if (ident == DeviceWidth || ident == DeviceHeight)
			m_init_scale = 10;
		else
			m_init_scale = 0;
		break;

	case ViewportUserScale:
		if (is_num && (num >= 1 || num <= -1) || !is_num && (ident == Yes || ident == DeviceWidth || ident == DeviceHeight))
			m_user_scalable = CSS_ViewportMeta::Yes;
		else
			m_user_scalable = CSS_ViewportMeta::No;
		break;
	}
}

/* virtual */ void
CSS_ViewportMeta::AddViewportProperties(CSS_Viewport* viewport)
{
	if (!m_width.IsAuto() || m_init_scale != CSS_VIEWPORT_ZOOM_AUTO)
	{
		viewport->SetMinWidth(m_width, FALSE, FALSE);
		viewport->SetMaxWidth(m_width, FALSE, FALSE);
	}

	if (!m_height.IsAuto())
	{
		viewport->SetMinHeight(m_height, FALSE, FALSE);
		viewport->SetMaxHeight(m_height, FALSE, FALSE);
	}

	if (m_init_scale != CSS_VIEWPORT_ZOOM_AUTO)
		viewport->SetZoom(m_init_scale, FALSE, FALSE);

	if (m_min_scale != CSS_VIEWPORT_ZOOM_AUTO)
		viewport->SetMinZoom(m_min_scale, FALSE, FALSE);

	if (m_max_scale != CSS_VIEWPORT_ZOOM_AUTO)
		viewport->SetMaxZoom(m_max_scale, FALSE, FALSE);

	if (m_user_scalable != CSS_ViewportMeta::Unset)
		viewport->SetUserZoom(m_user_scalable == CSS_ViewportMeta::Yes, FALSE, FALSE);
}

/* virtual */ OP_STATUS
CSS_ViewportMeta::CreateCopy(ComplexAttr** copy_to)
{
	*copy_to = OP_NEW(CSS_ViewportMeta, (*this));
	return *copy_to ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

#endif // CSS_VIEWPORT_SUPPORT
