/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_VIEWPORT_H
#define CSS_VIEWPORT_H

#ifdef CSS_VIEWPORT_SUPPORT

#include "modules/util/gen_math.h"
#include "modules/style/css_device_properties.h"
#include "modules/style/css_media.h"

/** 'auto' value for zoom, min-zoom and max-zoom. */
#define CSS_VIEWPORT_ZOOM_AUTO DBL_MAX

/** Length class for representing <viewport-length> values. */
class CSS_ViewportLength
{
public:

	/** Length value type. Keyword or length unit. */
	enum Type
	{
		AUTO,
		DEVICE_WIDTH,
		DEVICE_HEIGHT,
		DESKTOP_WIDTH,
		PERCENT,
		EM,
		REM,
		EX,
		PX,
		CM,
		MM,
		INCH,
		PT,
		PC
	};

	/** Default constructor. Construct an auto length. */

	CSS_ViewportLength() : m_length(0), m_type(AUTO) {}

	/** Construct a keyword length. */

	CSS_ViewportLength(Type type) : m_length(0), m_type(type) { OP_ASSERT(m_type < PERCENT); }

	/** Construct a scalar length with unit. */

	CSS_ViewportLength(double length, Type type) : m_type(type)
	{
		m_length = length;
		OP_ASSERT(m_type >= PERCENT);
	}

	/** Clamp pixel length within given range. */

	void Clamp(double range_min, double range_max)
	{
		OP_ASSERT(m_type == PX);

		if (m_length < range_min)
			m_length = range_min;
		else if (m_length > range_max)
			m_length = range_max;
	}

	/** Convert a viewport length value to a px length. 'auto' stays 'auto'. */

	const CSS_ViewportLength ToPixels(const CSS_DeviceProperties& device_props, BOOL horizontal) const;

	/** Returns true if this is an 'auto' length. */

	BOOL IsAuto() const { return m_type == AUTO; }

	/** Assignment operator for setting px length from double. */

	const CSS_ViewportLength& operator=(double length) { m_length = length; m_type = PX; return *this; }

	/** Assignment operator for CSS_ViewportLengths. */

	const CSS_ViewportLength& operator=(const CSS_ViewportLength& length) { m_length = length.m_length; m_type = length.m_type; return *this; }

	/** Cast operator to convert px length to double. */

	operator double() const { OP_ASSERT(m_type == PX); return m_length; }

	/** Cast operator to convert px length to unsigned int. */

	operator unsigned int() const { OP_ASSERT(m_type == PX); return static_cast<unsigned int>(OpRound(m_length)); }

private:

	/** The length value if m_type is a length unit. */
	double m_length;

	/** The unit of m_length or a length keyword. */
	Type m_type;
};

inline const CSS_ViewportLength& Min(const CSS_ViewportLength& a, const CSS_ViewportLength& b) { if (a.IsAuto() || !b.IsAuto() && double(b) < double(a)) return b; else return a; }
inline const CSS_ViewportLength& Max(const CSS_ViewportLength& a, const CSS_ViewportLength& b) { if (a.IsAuto() || !b.IsAuto() && double(a) < double(b)) return b; else return a; }

/** Class for keeping computed and constrained values for the CSS viewport
	from @viewport and viewport meta declarations. */

class CSS_Viewport
{
public:

	/** @viewport orientation */
	enum Orientation
	{
		Auto,
		Portrait,
		Landscape,
	};

	/** Constructor. Resets to initial values. */
	CSS_Viewport()
		: m_zoom(CSS_VIEWPORT_ZOOM_AUTO),
		  m_min_zoom(CSS_VIEWPORT_ZOOM_AUTO),
		  m_max_zoom(CSS_VIEWPORT_ZOOM_AUTO),
		  m_zoomable(TRUE),
		  m_orientation(Auto),
		  m_actual_orientation(Auto),
		  m_actual_width(0),
		  m_actual_height(0),
		  m_actual_zoom(CSS_VIEWPORT_ZOOM_AUTO),
		  m_actual_min_zoom(CSS_VIEWPORT_ZOOM_AUTO),
		  m_actual_max_zoom(CSS_VIEWPORT_ZOOM_AUTO),
		  m_actual_zoomable(TRUE) { m_packed_init = 0; }

	/** Mark properties for re-cascading. */
	void MarkDirty() { m_packed.dirty = 1; }

	/** @return TRUE if properties need to be re-cascaded. */
	BOOL IsDirty() { return m_packed.dirty == 1; }

	/** Reset computed values to initial values. */
	void ResetComputedValues();

	/** @return TRUE if the cascading of viewport properties after last
				Reset() added any of the user or author properties,
				otherwise FALSE. */
	BOOL HasProperties() const { return m_packed_init != 0; }

	/** Run the constraining procedure specified in the CSS Device Adaptation spec.

		@param device_props Device properties used as input to the constraing procedure. */
	void Constrain(const CSS_DeviceProperties& device_props);

	/** Set the UA stylesheet viewport properties. */
	void SetDefaultProperties(BOOL has_fullscreen_element)
	{
		// UA default stylesheet for Opera:
		//
		//  @viewport {
		//    min-zoom: 0.25; /* tweakable */
		//    max-zoom: 5; /* tweakable */
		//    width: desktop-width; /* internal keyword, desktop-width may vary with flex-root at some point. */
		//  }
		//
		//  /* Not entirely correct. Will only match (view-mode: fullscreen) if
		//     a fullscreen element is set. */
		//  @media (view-mode: fullscreen) {
		//    @viewport {
		//      width: auto;
		//      height: auto;
		//      zoom: 1;
		//      user-zoom: fixed;
		//    }
		//  }
		//
		// The m_packed.*_set bits are not set below on purpose.
		// HasProperties() should return FALSE if only UA style is present.
		// That is because Mobile Rendering Modes are affected by the presence
		// of author specified viewport (meta viewport or @viewport), and that
		// presence is checked with HasProperties().
		// The exception is for fullscreen styling. The effect is that
		// flex-root will not work for fullscreen'ed elements, media handheld
		// triggering will not happen for fullscreen'ed elements, etc.
		// It's most likely for the good, this needs to be cleaned
		// up at some point. Spec and implementation-wise.

		if (!m_packed.min_zoom_set)
			m_min_zoom = CSS_VIEWPORT_DEFAULT_MIN_ZOOM;
		if (!m_packed.max_zoom_set)
			m_max_zoom = CSS_VIEWPORT_DEFAULT_MAX_ZOOM;

#ifdef DOM_FULLSCREEN_MODE
		if (has_fullscreen_element)
		{
			if (!m_packed.zoom_set)
			{
				m_packed.zoom_set = 1;
				m_zoom = 1.0;
			}
			if (!m_packed.zoomable_set)
			{
				m_packed.zoomable_set = 1;
				m_zoomable = FALSE;
			}
		}
		else
#endif // DOM_FULLSCREEN_MODE
		{
			CSS_ViewportLength desktop_width_len(CSS_ViewportLength::DESKTOP_WIDTH);

			if (!m_packed.min_w_set)
				m_min_width = desktop_width_len;
			if (!m_packed.max_w_set)
				m_max_width = desktop_width_len;
		}
	}

	/** Set the min-width property. Used during cascading. */
	void SetMinWidth(const CSS_ViewportLength& min_width, BOOL user, BOOL important)
	{
		if (!m_packed.min_w_set || important && (!m_packed.min_w_imp || user && !m_packed.min_w_usr))
		{
			m_min_width = min_width;
			m_packed.min_w_set = 1;
			m_packed.min_w_imp = important ? 1 : 0;
			m_packed.min_w_usr = user ? 1 : 0;
		}
	}

	/** Set the max-width property. Used during cascading. */
	void SetMaxWidth(const CSS_ViewportLength& max_width, BOOL user, BOOL important)
	{
		if (!m_packed.max_w_set || important && (!m_packed.max_w_imp || user && !m_packed.max_w_usr))
		{
			m_max_width = max_width;
			m_packed.max_w_set = 1;
			m_packed.max_w_imp = important ? 1 : 0;
			m_packed.max_w_usr = user ? 1 : 0;
		}
	}

	/** Set the min-height property. Used during cascading. */
	void SetMinHeight(const CSS_ViewportLength& min_height, BOOL user, BOOL important)
	{
		if (!m_packed.min_h_set || important && (!m_packed.min_h_imp || user && !m_packed.min_h_usr))
		{
			m_min_height = min_height;
			m_packed.min_h_set = 1;
			m_packed.min_h_imp = important ? 1 : 0;
			m_packed.min_h_usr = user ? 1 : 0;
		}
	}

	/** Set the max-height property. Used during cascading. */
	void SetMaxHeight(const CSS_ViewportLength& max_height, BOOL user, BOOL important)
	{
		if (!m_packed.max_h_set || important && (!m_packed.max_h_imp || user && !m_packed.max_h_usr))
		{
			m_max_height = max_height;
			m_packed.max_h_set = 1;
			m_packed.max_h_imp = important ? 1 : 0;
			m_packed.max_h_usr = user ? 1 : 0;
		}
	}

	/** Set the zoom property. Used during cascading. */
	void SetZoom(double zoom, BOOL user, BOOL important)
	{
		if (!m_packed.zoom_set || important && (!m_packed.zoom_imp || user && !m_packed.zoom_usr))
		{
			m_zoom = zoom;
			m_packed.zoom_set = 1;
			m_packed.zoom_imp = important ? 1 : 0;
			m_packed.zoom_usr = user ? 1 : 0;
		}
	}

	/** Set the min-zoom property. Used during cascading. */
	void SetMinZoom(double min_zoom, BOOL user, BOOL important)
	{
		if (!m_packed.min_zoom_set || important && (!m_packed.min_zoom_imp || user && !m_packed.min_zoom_usr))
		{
			m_min_zoom = min_zoom;
			m_packed.min_zoom_set = 1;
			m_packed.min_zoom_imp = important ? 1 : 0;
			m_packed.min_zoom_usr = user ? 1 : 0;
		}
	}

	/** Set the max-zoom property. Used during cascading. */
	void SetMaxZoom(double max_zoom, BOOL user, BOOL important)
	{
		if (!m_packed.max_zoom_set || important && (!m_packed.max_zoom_imp || user && !m_packed.max_zoom_usr))
		{
			m_max_zoom = max_zoom;
			m_packed.max_zoom_set = 1;
			m_packed.max_zoom_imp = important ? 1 : 0;
			m_packed.max_zoom_usr = user ? 1 : 0;
		}
	}

	/** Set the user-zoom property. Used during cascading. */
	void SetUserZoom(BOOL zoomable, BOOL user, BOOL important)
	{
		if (!m_packed.zoomable_set || important && (!m_packed.zoomable_imp || user && !m_packed.zoomable_usr))
		{
			m_zoomable = zoomable;
			m_packed.zoomable_set = 1;
			m_packed.zoomable_imp = important ? 1 : 0;
			m_packed.zoomable_usr = user ? 1 : 0;
		}
	}

	/** Set the orientation property. Used during cascading. */
	void SetOrientation(Orientation orientation, BOOL user, BOOL important)
	{
		if (!m_packed.orient_set || important && (!m_packed.orient_imp || user && !m_packed.orient_usr))
		{
			m_orientation = orientation;
			m_packed.orient_set = 1;
			m_packed.orient_imp = important ? 1 : 0;
			m_packed.orient_usr = user ? 1 : 0;
		}
	}

	/** @return the constrained value of min-zoom. */
	double GetMinZoom() const { return m_actual_min_zoom; }

	/** @return the constrained value of max-zoom. */
	double GetMaxZoom() const { return m_actual_max_zoom; }

	/** @return TRUE if user-zoom is 'zoom', FALSE if 'fixed'. */
	BOOL GetUserZoom() const { return m_actual_zoomable; }

	/** @return constrained value of zoom. */
	double GetZoom() const { return m_actual_zoom; }

	/** @return the constrained viewport width. */
	unsigned int GetWidth() const { return m_actual_width; }

	/** @return the constrained viewport height. */
	unsigned int GetHeight() const { return m_actual_height; }

	/** @return the computed value of orientation. */
	Orientation GetOrientation() const { return m_actual_orientation; }

private:
	/** The computed value of the min-width property. */
	CSS_ViewportLength m_min_width;

	/** The computed value of the max-width property. */
	CSS_ViewportLength m_max_width;

	/** The computed value of the min-height property. */
	CSS_ViewportLength m_min_height;

	/** The computed value of the max-height property. */
	CSS_ViewportLength m_max_height;

	/** The computed value of the zoom property. */
	double m_zoom;

	/** The computed value of the min-zoom property. */
	double m_min_zoom;

	/** The computed value of the max-zoom property. */
	double m_max_zoom;

	/** The computed value of the user-zoom property.
		TRUE is 'zoom', FALSE is 'fixed'. */
	BOOL m_zoomable;

	/** The computed value of the orientation property. */
	Orientation m_orientation;

	/** The constrained value of the orientation property. */
	Orientation m_actual_orientation;

	/** Bitset for dirtyness, if properties have been set,
		which properties come from user style, and which are
		!important. */
	union
	{
		struct
		{
			/* 1 if viewport properties need to be re-cascaded. */
			unsigned int dirty:1;
			/* value-set bits. */
			unsigned int min_w_set:1;
			unsigned int max_w_set:1;
			unsigned int min_h_set:1;
			unsigned int max_h_set:1;
			unsigned int zoom_set:1;
			unsigned int min_zoom_set:1;
			unsigned int max_zoom_set:1;
			unsigned int zoomable_set:1;
			unsigned int orient_set:1;
			/* value-important bits. */
			unsigned int min_w_imp:1;
			unsigned int max_w_imp:1;
			unsigned int min_h_imp:1;
			unsigned int max_h_imp:1;
			unsigned int zoom_imp:1;
			unsigned int min_zoom_imp:1;
			unsigned int max_zoom_imp:1;
			unsigned int zoomable_imp:1;
			unsigned int orient_imp:1;
			/* value-user bits. */
			unsigned int min_w_usr:1;
			unsigned int max_w_usr:1;
			unsigned int min_h_usr:1;
			unsigned int max_h_usr:1;
			unsigned int zoom_usr:1;
			unsigned int min_zoom_usr:1;
			unsigned int max_zoom_usr:1;
			unsigned int zoomable_usr:1;
			unsigned int orient_usr:1;

			/* 28 bits in use. */
		} m_packed;
		unsigned int m_packed_init;
	};

	/** The constrained viewport width value in pixels. */
	unsigned int m_actual_width;

	/** The constrained viewport height value in pixels. */
	unsigned int m_actual_height;

	/** The constrained zoom value. */
	double m_actual_zoom;

	/** The constrained min-zoom value. */
	double m_actual_min_zoom;

	/** The constrained max-zoom value. */
	double m_actual_max_zoom;

	/** The constrained user-zoom value. */
	BOOL m_actual_zoomable;
};


/** An interface for classes that store viewport properties. */

class CSS_ViewportDefinition
{
public:

	/** Add the properties to the viewport object during cascading of
		viewport properties.

		@param viewport The viewport object to add properties to. */

	virtual void AddViewportProperties(CSS_Viewport* viewport) = 0;
};

#endif // CSS_VIEWPORT_SUPPORT

#endif // CSS_VIEWPORT_H

