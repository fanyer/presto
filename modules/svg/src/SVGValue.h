/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_VALUE_H
#define SVG_VALUE_H

#ifdef SVG_SUPPORT

#include "modules/svg/svg_external_types.h" // for SVGColor, SVGEnableBackground

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGObject.h"
#include "modules/svg/src/SVGLength.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGInternalEnum.h"

#include "modules/url/url2.h" // SVGURL has a URL
#include "modules/util/opstring.h"

class HTML_Element;
class TempBuffer;

class SVGNumberObject : public SVGObject
{
public:
	SVGNumberObject(SVGNumber value = 0);

	virtual BOOL IsEqual(const SVGObject& obj) const;
	virtual SVGObject *Clone() const { return OP_NEW(SVGNumberObject, (value)); }
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	SVGNumber value;
};

class SVGEnum : public SVGObject
{
private:
	INT32 m_enum_value;
	SVGEnumType m_enum_type;
public:
	SVGEnum(SVGEnumType enum_type, UINT32 value = 0) : SVGObject(SVGOBJECT_ENUM), m_enum_value(value), m_enum_type(enum_type) {}
    virtual ~SVGEnum() {}

	virtual BOOL IsEqual(const SVGObject& other) const;

	virtual SVGObject *Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	void Copy(const SVGEnum& other);

    INT32 EnumValue() const { return m_enum_value; }
    INT32 EnumType() const { return m_enum_type; }

	void SetEnumValue(UINT32 enum_value) { m_enum_value = enum_value; }
	void SetEnumType(SVGEnumType enum_type) { m_enum_type = enum_type; }
};

class SVGColorObject : public SVGObject
{
public:
	SVGColorObject(SVGColor::ColorType type = SVGColor::SVGCOLOR_RGBCOLOR, UINT32 color = 0x0, UINT32 icccolor = 0x0) :
			SVGObject(SVGOBJECT_COLOR),
			m_color(type, color, icccolor)
		{}

    virtual ~SVGColorObject() {}

	virtual BOOL IsEqual(const SVGObject& obj) const;
	virtual SVGObject *Clone() const
	{
		SVGColorObject* nc = OP_NEW(SVGColorObject, (m_color.GetColorType(),
												m_color.GetColorRef(),
												m_color.GetICCColor()));
		if (nc)
			nc->CopyFlags(*this);
		return nc;
	}
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	SVGColor& GetColor() { return m_color; }
	const SVGColor& GetColor() const { return m_color; }

private:
	SVGColor m_color;
};

enum SVGOrientType
{
	SVGORIENT_UNKNOWN,
	SVGORIENT_AUTO,
	SVGORIENT_ANGLE
};

enum SVGRotateType
{
	SVGROTATE_AUTO,
	SVGROTATE_AUTOREVERSE,
	SVGROTATE_ANGLE,
	SVGROTATE_UNKNOWN
};

enum SVGAngleType
{
	SVGANGLE_UNSPECIFIED = 1,
	SVGANGLE_DEG		 = 2,
	SVGANGLE_RAD		 = 3,
	SVGANGLE_GRAD		 = 4
};

/* Helper object for SVGRotate/SVGOrient */
class SVGAngle : public SVGObject
{
private:
	SVGAngleType m_unit;
	SVGNumber m_angle;

public:
	SVGAngle()
		: SVGObject(SVGOBJECT_ANGLE), m_unit(SVGANGLE_UNSPECIFIED), m_angle(0) { }
	SVGAngle(SVGNumber angle, SVGAngleType unit = SVGANGLE_UNSPECIFIED)
		: SVGObject(SVGOBJECT_ANGLE), m_unit(unit), m_angle(angle) { }

	void SetAngle(SVGNumber angle, SVGAngleType unit);
	void SetAngleValue(SVGNumber angle_value) { m_angle = angle_value; }

	SVGNumber GetAngleInUnits(SVGAngleType desiredunit) const;
	SVGNumber GetAngleValue() const { return m_angle; }
	SVGAngleType GetAngleType() const { return m_unit; }

	void Copy(const SVGAngle& angle);

	/* Quantize an angle (in degrees) into a quadrant value 0-3 */
	static unsigned QuantizeAngle90Deg(float angle);

	virtual BOOL IsEqual(const SVGObject &other) const;
	virtual SVGObject *Clone() const { return OP_NEW(SVGAngle, (m_angle, m_unit)); }
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
};

class SVGRotate : public SVGObject
{
private:
	SVGRotateType 	m_type;
	SVGAngle* 		m_angle;

public:
	SVGRotate(SVGRotateType t = SVGROTATE_UNKNOWN);
	SVGRotate(SVGAngle* a);
	virtual ~SVGRotate();

	virtual BOOL IsEqual(const SVGObject &other) const;
	virtual SVGObject *Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	SVGRotateType	GetRotateType() { return m_type; }
	SVGAngle*		GetAngle() { return m_angle; }
};

class SVGOrient : public SVGObject
{
private:
	SVGEnum* m_type;
	SVGAngle* m_angle;

public:
	virtual ~SVGOrient();

	static OP_STATUS Make(SVGOrient*& orient, SVGOrientType t, SVGAngle* a = NULL);

	virtual SVGObject *Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
	virtual BOOL IsEqual(const SVGObject &other) const;
	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	void SetOrientType(SVGOrientType type) { m_type->SetEnumValue((UINT32)type); }
	OP_STATUS SetAngle(const SVGAngle &angle);
	OP_STATUS SetToAngleInDeg(SVGNumber deg);
	OP_STATUS Copy(const SVGOrient &other);

	SVGOrientType	GetOrientType() const { return (SVGOrientType)m_type->EnumValue(); }
	SVGEnum*		GetEnum() { return m_type; }
	SVGAngle*		GetAngle() { return m_angle; }
	const SVGAngle*	GetAngle() const { return m_angle; }

private:
	SVGOrient(SVGAngle* a, SVGEnum* t);
};

class SVGString : public SVGObject
{
private:
	uni_char* m_value;
	unsigned m_strlen;
public:
	SVGString() : SVGObject(SVGOBJECT_STRING), m_value(NULL), m_strlen(0) {}
	virtual ~SVGString() { if (m_value) OP_DELETEA(m_value); }

	OP_STATUS SetString(const uni_char* str, unsigned strlen);
	const uni_char* GetString() const { return m_value; }
	unsigned GetLength() const { return m_strlen; }

	virtual SVGObject *Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
	virtual BOOL IsEqual(const SVGObject &other) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);
};

enum SVGAlignType
{
	SVGALIGN_UNKNOWN  = 0,
	SVGALIGN_NONE 	  = 1,
	SVGALIGN_XMINYMIN = 2,
	SVGALIGN_XMIDYMIN = 3,
	SVGALIGN_XMAXYMIN = 4,
	SVGALIGN_XMINYMID = 5,
	SVGALIGN_XMIDYMID = 6,
	SVGALIGN_XMAXYMID = 7,
	SVGALIGN_XMINYMAX = 8,
	SVGALIGN_XMIDYMAX = 9,
	SVGALIGN_XMAXYMAX = 10
};

enum SVGMeetOrSliceType
{
	SVGMOS_UNKNOWN = 0,
	SVGMOS_MEET    = 1,
	SVGMOS_SLICE   = 2
};

class SVGAspectRatio : public SVGObject
{
public:
    SVGAspectRatio() : SVGObject(SVGOBJECT_ASPECT_RATIO), align(SVGALIGN_XMIDYMID), mos(SVGMOS_MEET), defer(FALSE) { }
    SVGAspectRatio(BOOL d, SVGAlignType a, SVGMeetOrSliceType m) : SVGObject(SVGOBJECT_ASPECT_RATIO), align(a), mos(m), defer(d) { }
	virtual ~SVGAspectRatio() {}

	virtual BOOL IsEqual(const SVGObject &other) const;
	virtual SVGObject *Clone() const { return OP_NEW(SVGAspectRatio, (defer, align, mos)); }
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	void Copy(const SVGAspectRatio& src);
	SVGAlignType align;
	SVGMeetOrSliceType mos;
	BOOL defer;
};

class SVGURL : public SVGObject
{
private:
	URL m_last_resolved_url;
	OpString m_attr_string;

public:
	SVGURL() : SVGObject(SVGOBJECT_URL) {}

	// FIXME: Move setting of string to function that can return error code
	SVGURL(const SVGURLReference& urlref, URL resolved_url)
		: SVGObject(SVGOBJECT_URL)
	{
		m_attr_string.Set(urlref.url_str, urlref.info.url_str_len);
		m_last_resolved_url = resolved_url;
	}
	SVGURL(const uni_char* attr_string, unsigned str_len, URL resolved_url)
		: SVGObject(SVGOBJECT_URL)
	{
		m_attr_string.Set(attr_string, str_len);
		m_last_resolved_url = resolved_url;
	}
    virtual ~SVGURL() {}

	virtual BOOL IsEqual(const SVGObject& obj) const;
	virtual SVGObject *Clone() const
	{
		SVGURL* nu = OP_NEW(SVGURL, (m_attr_string.CStr(), m_attr_string.Length(),
								m_last_resolved_url));
		if (nu)
			nu->CopyFlags(*this);
		return nu;
	}
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	OP_STATUS Copy(const SVGURL& other);

	OP_STATUS SetURL(const uni_char* attr_string, URL url);
	URL GetURL(URL root_url, HTML_Element* elm);
	URL* GetURLPtr(URL root_url, HTML_Element* elm);
	URL* GetUnreliableCachedURLPtr();

	const uni_char* GetAttrString() { return m_attr_string.CStr(); }
	unsigned GetAttrStringLength() { return m_attr_string.Length(); }
};

#ifdef SVG_SUPPORT_FILTERS
class SVGEnableBackgroundObject : public SVGObject
{
public:
	SVGEnableBackgroundObject(SVGEnableBackground::EnableBackgroundType t = SVGEnableBackground::SVGENABLEBG_ACCUMULATE);
	SVGEnableBackgroundObject(SVGEnableBackground::EnableBackgroundType t, SVGNumber x, SVGNumber y, SVGNumber width, SVGNumber height);
	SVGEnableBackgroundObject(SVGEnableBackground& other);
	virtual ~SVGEnableBackgroundObject() {}

	virtual BOOL IsEqual(const SVGObject &other) const;
	virtual SVGObject *Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;

	SVGEnableBackground::EnableBackgroundType GetType() const { return m_enable_bg.GetType(); }
	BOOL IsRectValid() const { return m_enable_bg.IsRectValid(); }
	SVGRect GetRect() { return SVGRect(m_enable_bg.GetX(), m_enable_bg.GetY(), m_enable_bg.GetWidth(), m_enable_bg.GetHeight()); }
	SVGEnableBackground& GetEnableBackground() { return m_enable_bg; }
private:
	SVGEnableBackground m_enable_bg;
};
#endif // SVG_SUPPORT_FILTERS

class SVGBaselineShiftObject : public SVGObject
{
public:
	SVGBaselineShiftObject(SVGBaselineShift::BaselineShiftType bls_type = SVGBaselineShift::SVGBASELINESHIFT_BASELINE)
			: SVGObject(SVGOBJECT_BASELINE_SHIFT), m_shift(bls_type) {}
	SVGBaselineShiftObject(SVGBaselineShift::BaselineShiftType bls_type, SVGLength value)
			: SVGObject(SVGOBJECT_BASELINE_SHIFT), m_shift(bls_type, value) {}
	SVGBaselineShiftObject(const SVGBaselineShift& ref)
			: SVGObject(SVGOBJECT_BASELINE_SHIFT), m_shift(ref) {}
	virtual ~SVGBaselineShiftObject() {}
	virtual SVGObject *Clone() const
	{
		SVGBaselineShiftObject* nbls = OP_NEW(SVGBaselineShiftObject, (m_shift));
		if (nbls)
			nbls->CopyFlags(*this);
		return nbls;
	}
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
	virtual BOOL IsEqual(const SVGObject &other) const;

	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);

	SVGBaselineShift& GetBaselineShift() { return m_shift; }
	SVGLength GetValue() const { return m_shift.GetValue(); }
	SVGBaselineShift::BaselineShiftType GetShiftType() const { return m_shift.GetShiftType(); }
private:
	SVGBaselineShift m_shift;
};

class SVGNavRef : public SVGObject
{
public:
	enum NavRefType
	{
		AUTO,
		SELF,
		URI
	};

	SVGNavRef()
		: SVGObject(SVGOBJECT_NAVREF), m_uri(NULL), m_navtype(AUTO) {}
	SVGNavRef(NavRefType type)
		: SVGObject(SVGOBJECT_NAVREF), m_uri(NULL), m_navtype(type) {}

	virtual ~SVGNavRef() { OP_DELETEA(m_uri); }
	virtual SVGObject *Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
	virtual BOOL IsEqual(const SVGObject &other) const;

	OP_STATUS Copy(const SVGNavRef& navref_object);

	/**
	 * Set the URI of the navigation reference.
	 *
	 * @param uri A pointer to a string to copy the URI from. If the
	 * pointer is NULL the URI of the paint is also set to NULL.
	 * @param strlen The length of the string. Set to zero if uri is NULL.
	 * @param add_hashsep
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS SetURI(const uni_char* uri, int strlen, BOOL add_hashsep = FALSE);

	const uni_char* GetURI() const { return m_uri; }
	NavRefType GetNavType() const { return m_navtype; }

private:
	uni_char*		m_uri;
	NavRefType		m_navtype;
};

class SVGProxyObject : public SVGObject
{
public:
	SVGProxyObject() : SVGObject(SVGOBJECT_PROXY), m_real_obj(NULL) {}

	virtual ~SVGProxyObject() { SVGObject::DecRef(m_real_obj); }

	virtual SVGObject* Clone() const
	{
		SVGProxyObject* proxy_clone = OP_NEW(SVGProxyObject, ());
		if (proxy_clone)
		{
			SVGObject* real_clone = m_real_obj ? m_real_obj->Clone() : NULL;
			if (real_clone || !m_real_obj)
			{
				proxy_clone->SetRealObject(real_clone);
				return proxy_clone;
			}
			OP_DELETE(proxy_clone);
		}
		return NULL;
	}
	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value)
	{
		return m_real_obj && m_real_obj->InitializeAnimationValue(animation_value);
	}
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const
	{
		return m_real_obj ? m_real_obj->LowLevelGetStringRepresentation(buffer) : OpStatus::ERR;
	}
	virtual BOOL IsEqual(const SVGObject &other) const
	{
		return m_real_obj && m_real_obj->IsEqual(other);
	}

	void SetRealObject(SVGObject* real_obj)
	{
		SVGObject::DecRef(m_real_obj);
		m_real_obj = SVGObject::IncRef(real_obj);
	}
	SVGObject* GetRealObject() const { return m_real_obj; }

private:
	SVGObject* m_real_obj;
};

class SVGClassObject : public SVGObject
{
private:
	ClassAttribute* m_class_attr;

public:
	SVGClassObject() : SVGObject(SVGOBJECT_CLASSOBJECT), m_class_attr(NULL) {}
	virtual ~SVGClassObject() { if (m_class_attr) OP_DELETE(m_class_attr); }

	void SetClassAttribute(ClassAttribute* class_attr);
	ClassAttribute* GetClassAttribute() const { return m_class_attr; }

	OP_STATUS Copy(const SVGClassObject& other);

	virtual SVGObject* Clone() const;
	virtual OP_STATUS LowLevelGetStringRepresentation(TempBuffer* buffer) const;
	virtual BOOL IsEqual(const SVGObject &other) const;
	virtual BOOL InitializeAnimationValue(SVGAnimationValue &animation_value);
};

#endif // SVG_SUPPORT
#endif // SVG_VALUE_H
