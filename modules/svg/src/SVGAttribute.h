/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SVG_ATTRIBUTE_H
#define SVG_ATTRIBUTE_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGObject.h"
#include "modules/logdoc/htm_elm.h"

enum SVGAttributeField
{
	SVG_ATTRFIELD_DEFAULT = 0,
	SVG_ATTRFIELD_BASE,
	SVG_ATTRFIELD_ANIM,
	SVG_ATTRFIELD_CSS
};

/**
 * Matches SVGENUM_ATTRIBUTE_TYPE
 */
enum SVGAttributeType
{
	SVGATTRIBUTE_CSS,
	SVGATTRIBUTE_XML,
	SVGATTRIBUTE_AUTO,
	SVGATTRIBUTE_UNKNOWN
};

enum
{
	SVG_T_SVGATTR = 4637643 // Must be unique
};

class SVGAttribute : public ComplexAttr
{
public:
	SVGAttribute(SVGObject* base);

	virtual ~SVGAttribute();

	/**
	 * Used to check the type of the ComplexAttr during runtime
	 */
	virtual BOOL IsA(int type) { return type == SVG_T_SVGATTR; }

	/**
	 * Used to clone HTML Elements. Returning OpStatus::ERR_NOT_SUPPORTED here
	 * will prevent the attribute from being cloned with the html element.
	 */
	virtual OP_STATUS CreateCopy(ComplexAttr **copy_to);

	virtual OP_STATUS ToString(TempBuffer *buffer);

	virtual BOOL MoveToOtherDocument(FramesDocument *old_document, FramesDocument *new_document);

	virtual BOOL Equals(ComplexAttr *other);

	void ReplaceBaseObject(SVGObject* obj);
	OP_STATUS SetAnimationObject(SVGAttributeField field, SVGObject* obj);
	BOOL ActivateAnimationObject(SVGAttributeField field);
	BOOL DeactivateAnimation();

	BOOL HasSVGObject(SVGAttributeField field_type = SVG_ATTRFIELD_DEFAULT,
					  SVGAttributeType anim_attribute_type = SVGATTRIBUTE_AUTO)
	{
		return GetSVGObject(field_type, anim_attribute_type) != NULL;
	}

	SVGObject* GetSVGObject(SVGAttributeField field_type = SVG_ATTRFIELD_DEFAULT,
							SVGAttributeType anim_attribute_type = SVGATTRIBUTE_AUTO);

	/**
	 * When a value is created implicitly (from DOM for example), an
	 * UNINITIALIZED flag is set. The reason for this is that we
	 * cannot in all situations give the attribute a default value that
	 * would not affect the result of the SVG fragment. This method
	 * clears that flag, and should be called when an attribute change
	 * has occured at this attribute.
	 */
	void MarkAsInitialized();

	/**
	 * This method is used from the animation code through
	 * AttrValueStore and is used for fetching the current animation
	 * object, regardless of if it is the animation is active or not
	 * at the attribute.
	 */
	SVGObject *GetAnimationSVGObject(SVGAttributeType anim_attribute_type);

	UINT32 GetSerial() { return serial; }

	OP_STATUS SetOverrideString(const uni_char *override_string, unsigned override_string_length);
	void ClearOverrideString();

protected:
	/**
	 * Helper method.
	 */
	static OP_STATUS SetTempBufferFromUTF8(TempBuffer* buffer, const char* utf_8_str);

private:

	struct AnimationData
	{
		AnimationData();
		~AnimationData();
		SVGObject* anim;
		SVGObject* css;

		union {
			struct {
				unsigned int is_anim_active:1;
				unsigned int is_css_active:1;
			} info;
			unsigned char info_packed;
		};
	};

	OP_STATUS AssertAnimationData();

	UINT32 serial;
	SVGObject* base;	///< Base property

	/**
	 * If the originating string doesn't match string representation
	 * we can generate, we save a copy of the string here. We do this
	 * for all object types, except OpBpath which have its own
	 * handling of the string data (in a compressed form).
	 *
	 * The string is stored in UTF-8 encoding.
	 *
	 * May be NULL.
	 */
	char *string_rep;

	AnimationData* anim_data;
};

class SVGAttributeIterator : public HTML_ImmutableAttrIterator
{
private:
	SVGAttributeField m_field_type;
	SVGAttributeType m_anim_attr_type;

public:
	SVGAttributeIterator(HTML_Element* element,
						 SVGAttributeField field_type = SVG_ATTRFIELD_DEFAULT,
						 SVGAttributeType anim_attribute_type = SVGATTRIBUTE_AUTO) :
			HTML_ImmutableAttrIterator(element),
			m_field_type(field_type),
			m_anim_attr_type(anim_attribute_type) {}

	BOOL GetNext(int& attr_name, int& ns_idx, BOOL& is_special, SVGObject*& obj);
};

#endif // SVG_SUPPORT
#endif // !SVG_ATTRIBUTE_H
