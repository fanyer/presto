/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_DECL_H
#define CSS_DECL_H

#include "modules/style/css_value_types.h"
#include "modules/style/css_gradient.h"
#include "modules/util/simset.h"
#include "modules/hardcore/mem/mem_man.h"
#include "modules/style/src/css_properties.h"

typedef enum {
	CSS_DECL_TYPE,
	CSS_DECL_ARRAY,
	CSS_DECL_LONG,
	CSS_DECL_STRING,
	CSS_DECL_NUMBER,
	CSS_DECL_NUMBER2,
	CSS_DECL_GEN_ARRAY,
	CSS_DECL_NUMBER4,
	CSS_DECL_COLOR,
	CSS_DECL_TRANSFORM_LIST,
	CSS_DECL_PROXY
} CSSDeclType;

struct CSS_generic_value
{
	short			value_type;
	union {
		uni_char*           string;
		short               type;
		int                 number;
		float               real;
		COLORREF            color;
#ifdef CSS_GRADIENT_SUPPORT
		CSS_Gradient*	gradient;
#endif // CSS_GRADIENT_SUPPORT
	}				value;

				CSS_generic_value()
					: value_type(0) {}

	BOOL		IsSourceLocal() const { return FALSE; }

	void		SetValueType(short type) { value_type = type; }
	short		GetValueType() const { return value_type; }

	void		SetString(uni_char* string) { value.string = string; }
	uni_char*	GetString() const { return value.string; }

	void		SetType(CSSValue type) { value.type = type; }
	CSSValue	GetType() const { return CSSValue(value.type); }

	void		SetNumber(int number) { value.number = number; }
	int			GetNumber() const { return value.number; }

	void		SetReal(float real) { value.real = real; }
	float		GetReal() const { return value.real; }

#ifdef CSS_GRADIENT_SUPPORT
	void		SetGradient(CSS_Gradient* gradient) { value.gradient = gradient; }
	CSS_Gradient*	GetGradient() const { return value.gradient; }
#endif // CSS_GRADIENT_SUPPORT

};

#ifdef CSS_TRANSFORMS
class AffineTransform;
#endif

/** Stores a CSS declaration.
    Used throughout the style and layout code, and is (among many other things) consulted
    for getComputedStyle. */
class CSS_decl
: public Link
{
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT_L

protected:

	union {
		unsigned int info;
		struct {
			unsigned int property:9;
			unsigned int important:1;
			unsigned int origin:3; // DeclarationOrigin
			unsigned int unspecified:1;
			unsigned int prefetch:1;
			union {
				unsigned int type_value:10;
				unsigned int value_type:10;
			}		extra;
			/* 25 bits used, 7 bits free */
		}			data;
	}				info;

	/** Reference count */
	int ref_count;

public:

	enum DeclarationOrigin {
		ORIGIN_AUTHOR,
		ORIGIN_USER_AGENT,
		ORIGIN_USER,
		ORIGIN_ANIMATIONS,
		ORIGIN_USER_AGENT_FULLSCREEN
	};

	/** Used for sorting declarations in cascading order. See
		GetCascadingPoint(). */
	enum CascadingPoint {
		POINT_USER_AGENT,
		POINT_USER_NORMAL,
		POINT_AUTHOR_NORMAL,
		POINT_USER_AGENT_FULLSCREEN,
		POINT_AUTHOR_IMPORTANT,
		POINT_ANIMATIONS,
		POINT_USER_IMPORTANT
	};

					CSS_decl(short prop) :
						ref_count(0)
					{
						info.info = 0;
						info.data.property = prop;
					}

	virtual			~CSS_decl() { OP_ASSERT(ref_count == 0); }

	short			GetProperty() const { return info.data.property; }
	void			SetProperty(short prop) { info.data.property = prop; }
	BOOL			GetImportant() const { return info.data.important; }
	void			SetImportant() { info.data.important = 1; }
	BOOL			GetUnspecified() const { return info.data.unspecified; }
	void			SetUnspecified(BOOL val) { info.data.unspecified = val ? 1 : 0; }
	CascadingPoint  GetCascadingPoint() const
					{
						/* Modeled after 6.4.1 Cascading order, CSS 2.1 */
						switch(GetOrigin())
						{
						default:
							OP_ASSERT(!"Not reached");
							/* fall-through */
						case ORIGIN_AUTHOR:
							return info.data.important ? POINT_AUTHOR_IMPORTANT : POINT_AUTHOR_NORMAL;
						case ORIGIN_USER_AGENT:
							return POINT_USER_AGENT;
						case ORIGIN_USER:
							return info.data.important ? POINT_USER_IMPORTANT : POINT_USER_NORMAL;
#ifdef CSS_ANIMATIONS
						case ORIGIN_ANIMATIONS:
							return POINT_ANIMATIONS;
#endif
#ifdef DOM_FULLSCREEN_MODE
						case ORIGIN_USER_AGENT_FULLSCREEN:
							return POINT_USER_AGENT_FULLSCREEN;
#endif
						}
					}
	DeclarationOrigin
					GetOrigin() const { return DeclarationOrigin(info.data.origin); }
	BOOL			GetDefaultStyle() const { return info.data.origin == CSS_decl::ORIGIN_USER_AGENT || info.data.origin == CSS_decl::ORIGIN_USER_AGENT_FULLSCREEN; }

	void			SetOrigin(DeclarationOrigin origin) { info.data.origin = origin; }
	BOOL			GetUserDefined() const { return info.data.origin == CSS_decl::ORIGIN_USER; }
	void			SetDefaultStyle() { info.data.origin = CSS_decl::ORIGIN_USER_AGENT; }
#ifdef CSS_ANIMATIONS
	void			SetIsAnimation() { info.data.origin = CSS_decl::ORIGIN_ANIMATIONS; }
#endif // CSS_ANIMATIONS

	CSS_decl*		Suc() const { return (CSS_decl*)Link::Suc(); }
	CSS_decl*		Pred() const { return (CSS_decl*)Link::Pred(); }

	virtual CSS_decl* CreateCopy() const = 0;

	virtual CSSDeclType GetDeclType() const = 0;
	virtual const uni_char*
					StringValue() const { return 0; }
	virtual float	GetNumberValue(int i) const { return 0.0f; }
	virtual short	GetValueType(int i) const { return CSS_NUMBER; }
	virtual CSSValue
					TypeValue() const { return CSS_VALUE_UNSPECIFIED; }
	virtual long	LongValue() const { return 0; }
	virtual short	GetLongKeyword() const { return -1; }
	virtual short*	ArrayValue() const { return 0; }
	virtual int		ArrayValueLen() const { return 0; }
	virtual const CSS_generic_value*
					GenArrayValue() const { return 0; }

	/** Reference CSS declaration. */
	void			Ref() { ref_count++; }

	/** Dereference CSS declaration. */
	void			Unref() { OP_ASSERT(ref_count > 0); if (--ref_count == 0) OP_DELETE(this); }

	virtual BOOL    IsEqual(const CSS_decl* decl) const = 0;

	/** Start prefetching of url values for this declaration.
		@param doc The document to load the resource for.
		@return ERR_NO_MEMORY on OOM, otherwise OK. */

	virtual OP_STATUS PrefetchURLs(FramesDocument* doc) { return OpStatus::OK; }

	/** Reference CSS declaration.

		Convenience function you may send NULL to. */

	static void		Ref(CSS_decl* decl) { if (decl) decl->Ref(); }

	/** Dereference CSS declaration.

		Convenience function you may send NULL to. */

	static void		Unref(CSS_decl* decl) { if (decl) decl->Unref(); }

};

/** Helper classes to unreference a CSS declaration when going out of
	scope or leaving. */

class CSS_DeclAutoPtr : public OpAutoPtr<CSS_decl>
{
public:
	CSS_DeclAutoPtr(CSS_decl* decl = NULL) : OpAutoPtr<CSS_decl>(decl) {}
	~CSS_DeclAutoPtr() { CSS_decl::Unref(release()); }
};

class CSS_DeclStackAutoPtr : public OpStackAutoPtr<CSS_decl>
{
public:
	CSS_DeclStackAutoPtr(CSS_decl* decl = NULL) : OpStackAutoPtr<CSS_decl>(decl) {}
	~CSS_DeclStackAutoPtr() { CSS_decl::Unref(release()); }
};

/** A proxy declaration pointing out another declaration.

	Used when the declaration has to be put in a list, but the
	declaration is in another list already.  The receiver must of
	course be aware that proxy objects may be present.

	No ownership is transferred to the proxy object, so the life span
	of the proxy object must be shorter than the declaration it points
	to. */
class CSS_proxy_decl
	: public CSS_decl
{
private:
	CSS_decl* real_decl;

public:
	CSS_proxy_decl(short prop) : CSS_decl(prop), real_decl(NULL) { }

	/** Update the declaration this proxy object points to */
	void UpdateRealDecl(CSS_decl* decl) { real_decl = decl; }

	/** Get the declaration this proxy object points to. */
	CSS_decl* GetRealDecl() const { return real_decl; }

	/** Empty implementation. No copies should be made of proxy objects. */
	virtual CSS_decl* CreateCopy() const { OP_ASSERT(!"Copying not supported for proxy objects"); return NULL; }

	/** Get the declaration type */
	virtual CSSDeclType GetDeclType() const { return CSS_DECL_PROXY; }

	/** Empty implementation. Proxy object should not be compared. */
	virtual BOOL IsEqual(const CSS_decl* decl) const { OP_ASSERT(!"Equality tests not supported for proxy objects"); return FALSE; }
};

class CSS_type_decl
  : public CSS_decl
{
public:
					CSS_type_decl(short prop, CSSValue val);

	virtual CSS_decl* CreateCopy() const { CSS_decl* new_decl = OP_NEW(CSS_type_decl, (GetProperty(), TypeValue())); return new_decl; }
	virtual CSSDeclType
					GetDeclType() const { return CSS_DECL_TYPE; };
	virtual CSSValue
					TypeValue() const { return CSSValue(info.data.extra.type_value); }

	void			SetTypeValue(CSSValue val) { info.data.extra.type_value = val; }

    virtual BOOL    IsEqual(const CSS_decl* decl) const;
};

class CSS_array_decl
  : public CSS_decl
{
private:
	short*			value;
	int				array_len;

public:
	CSS_array_decl(short prop) : CSS_decl(prop), value(0), array_len(0) {}
	virtual ~CSS_array_decl();

	/** Second phase constructor. You must call this method prior to using the
		CSS_array_decl object, unless it was created using the Create() method.

		@return OP_STATUS Status of the construction, always check this. */
	OP_STATUS Construct(short* val_array, int val_array_len);

	/** OOM safe creation of a CSS_array_decl object.

		@return CSS_array_decl* The created object if successful and NULL otherwise. */
	static CSS_array_decl* Create(short prop, short* val_array, int val_array_len);

	virtual CSS_decl* CreateCopy() const { CSS_decl* new_decl = CSS_array_decl::Create(GetProperty(), value, array_len); return new_decl; }
	virtual CSSDeclType
					GetDeclType() const { return CSS_DECL_ARRAY; }
	virtual short*	ArrayValue() const { return value; }
	virtual int		ArrayValueLen() const { return array_len; }

    virtual BOOL    IsEqual(const CSS_decl* decl) const;
};

/** Base class for CSS declarations holding arrays of generic values.

	Choose one of the sub-classes depending on which memory handling
	you want. The declaration could either own its data or hold a
	pointer to data owned externally. For the case when it owns its
	data, it can either copy from an external array or adopt an
	existing array.

	See classes CSS_stack_gen_array_decl, CSS_heap_gen_array_decl and
	CSS_copy_gen_array_decl */

class CSS_gen_array_decl  : public CSS_decl
{
protected:
	int				array_len;

	/** Cached layer count. Used only for background declarations, to
		hold the number of layers within the array. This value is
		normally computed for free during parsing and we keep it to
		avoid traversing the array to compute the length each time we
		need it. */

	int				layer_count;

	/** Protected constructor. This class should not be used to
		instantiate objects directly. Use one of the base classes
		instead. */

					CSS_gen_array_decl(short prop, int val_array_len);

public:

	virtual			~CSS_gen_array_decl() {}

	/** Set layer count of the declaration. Only used for background
		properties. */

	void			SetLayerCount(int n) { layer_count = n; }

	/** Get layer count of the declaration. Only used for background
		properties. */

	int				GetLayerCount() const { return layer_count; }

	virtual CSS_decl*
					CreateCopy() const;
	virtual CSSDeclType
					GetDeclType() const { return CSS_DECL_GEN_ARRAY; }
	virtual int		ArrayValueLen() const { return array_len; }

	virtual BOOL	IsEqual(const CSS_decl* decl) const;
	virtual float	GetNumberValue(int i) const { if (i>=0 && i<ArrayValueLen()) return GenArrayValue()[i].GetReal(); else return 0.0f; }
	virtual short	GetValueType(int i) const { if (i>= 0 && i<ArrayValueLen()) return GenArrayValue()[i].GetValueType(); else return CSS_NUMBER; }
	virtual OP_STATUS
					PrefetchURLs(FramesDocument* doc);
};

/** CSS_stack_gen_array_decl wraps a constant array of
    CSS_generic_values as a CSS declaration. Useful when you want to
    re-use a declaration to point at different stack allocated arrays
    for brief periods of time. It doesn't hold ownership to the array
    and the array must have longer life span than the wrapper
    object.*/

class CSS_stack_gen_array_decl : public CSS_gen_array_decl
{
private:

	const CSS_generic_value*
					gen_values;

public:

					CSS_stack_gen_array_decl(short prop);

	void			Set(const CSS_generic_value* val_array, int val_array_len);

	virtual const CSS_generic_value*
					GenArrayValue() const { return gen_values; }

	virtual 		~CSS_stack_gen_array_decl() {}
};

/** CSS_heap_gen_array_decl owns an array of CSS_generic_values.

	Use this when you have created an array of generic values and want
	to put them inside a css declaration, transferring ownership in
	the process.

	Note: It does NOT make a copy of the supplied data! Use
	CSS_copy_gen_array_decl if you want to make a copy at creation. */

class CSS_heap_gen_array_decl : public CSS_gen_array_decl
{
protected:

	CSS_generic_value*
					gen_values;

	/** Protected constructor. Use the static Create method to
		instantiate objects. */

	CSS_heap_gen_array_decl(short prop) : CSS_gen_array_decl(prop, 0) {}

public:

	/** Create a generic array declaration by adopting an array.
		Returns NULL on OOM or if 'val_array' is NULL.

		Note that 'val_array' is deleted on OOM as a convenience
		measure. */

	static CSS_gen_array_decl*
					Create(short prop, CSS_generic_value* val_array, int val_array_len);

	virtual const CSS_generic_value*
					GenArrayValue() const { return gen_values; }

	CSS_generic_value*
					GenArrayValueRep() const { return gen_values; }

	virtual 		~CSS_heap_gen_array_decl();

#ifdef CSS_TRANSITIONS
	/** Transition */
	BOOL			Transition(CSS_decl* from, CSS_decl* to, float percentage);
#endif // CSS_TRANSITIONS
};

class CSS_generic_value_list;

/** CSS_copy_gen_array_decl, a helper class around
	CSS_heap_gen_array_decl, makes a deep copy of the supplied array
	on creation. All nested string, URL, etc. are duplicated in the
	new array. */

class CSS_copy_gen_array_decl : public CSS_heap_gen_array_decl
{
protected:

	/** Protected constructor. Use one of the two Create methods to
		instantiate objects. */

					CSS_copy_gen_array_decl(short prop);

public:

	/** Create a generic array declaration by doing a deep copy of the
		input array. Returns NULL on OOM. Don't use this on existing
		generic array declarations, it won't copy all necessary data.
		Use Copy() instead. */

	static CSS_copy_gen_array_decl*
					Create(short prop, const CSS_generic_value* val_array, int val_array_len);

	/** Create a generic array declaration by doing a deep copy of the
		input list. Returns NULL on OOM. */

	static CSS_copy_gen_array_decl*
					Create(short prop, const CSS_generic_value_list& val_list);

	/** Copy an existing generic array */

	static CSS_copy_gen_array_decl*
					Copy(short prop, const CSS_gen_array_decl* src);

	virtual 		~CSS_copy_gen_array_decl() {}
};

class CSS_generic_value_item : public Link
{
public:
	CSS_generic_value_item*
					Suc() const { return static_cast<CSS_generic_value_item*>(Link::Suc()); }

	CSS_generic_value
					value;
};

class CSS_generic_value_list : public Head
{
public:
					~CSS_generic_value_list() { Clear(); }
	CSS_generic_value_item*
					First() const { return static_cast<CSS_generic_value_item*>(Head::First()); }
	CSS_generic_value_item*
					Last() const { return static_cast<CSS_generic_value_item*>(Head::Last()); }

	void			PushIdentL(CSSValue value);
	void			PushIntegerL(CSSValueType value_type, int value);
	void			PushValueTypeL(CSSValueType value_type);

	/** Push string into the generic value list.  The ownership over
		the string is _not_ transferred to the list, so it must have a
		longer lifespan. */
	void			PushStringL(CSSValueType value_type, uni_char* str);
#ifdef CSS_GRADIENT_SUPPORT
	/** Push gradient into the generic value list.  The ownership of
		the gradient is _not_ transferred to the list, so it must have a
		longer lifespan. */
	void			PushGradientL(CSSValueType value_type, CSS_Gradient* gradient);
#endif // CSS_GRADIENT_SUPPORT
	void			PushNumberL(CSSValueType unit, float real_value);
	void			PushGenericValueL(const CSS_generic_value& value);
};

class CSS_long_decl
  : public CSS_decl
{
protected:

	long			value;

public:
					CSS_long_decl(short prop, long val);

	virtual CSS_decl* CreateCopy() const { CSS_decl* new_decl = OP_NEW(CSS_long_decl, (GetProperty(), value)); return new_decl; }
	virtual CSSDeclType
					GetDeclType() const { return CSS_DECL_LONG; }
	virtual long	LongValue() const { return value; }
	void			SetLongValue(long val) { value = val; }
    virtual BOOL    IsEqual(const CSS_decl* decl) const;
};

class CSS_color_decl
  : public CSS_long_decl
{
public:
					CSS_color_decl(short prop, long val);

	virtual CSS_decl* CreateCopy() const { CSS_decl* new_decl = OP_NEW(CSS_color_decl, (GetProperty(), value)); return new_decl; }
	virtual CSSDeclType
					GetDeclType() const { return CSS_DECL_COLOR; }

    virtual BOOL    IsEqual(const CSS_decl* decl) const;
};

class CSS_string_decl
  : public CSS_decl
{
public:

	typedef enum {
		StringDeclString,
		StringDeclUrl,
		StringDeclSkin
	} StringType;

private:

	const uni_char*	value;

	struct {
		/** The string declaration is a url. */
		unsigned int type:2;
		/** The string declaration is a -o-skin function. */
		unsigned int is_skin:1;
		/** The url stems from a local stylesheet. */
		unsigned int source_local:1;
		/** The string value should not be deleted. */
		unsigned int dont_delete:1;
	} packed;

public:
					/** @deprecated Use constructor with StringType parameter instead. */
					DEPRECATED(CSS_string_decl(short prop, BOOL is_url));
					CSS_string_decl(short prop, StringType type, BOOL source_local = FALSE, BOOL dont_delete = FALSE);
					CSS_string_decl(short prop, uni_char* value, StringType type, BOOL source_local = FALSE);
	virtual			~CSS_string_decl();

	OP_STATUS		CopyAndSetString(const uni_char* val, int val_len);
	void			SetString(const uni_char* val) { OP_ASSERT(packed.dont_delete); value = val; }

	virtual CSS_decl* CreateCopy() const;
	virtual CSSDeclType
					GetDeclType() const { return CSS_DECL_STRING; }

	/** Retrieve the string value for this declaration. May return NULL. */
	virtual const uni_char*
					StringValue() const { return value; }

	virtual BOOL    IsEqual(const CSS_decl* decl) const;

	StringType		GetStringType() const { return (StringType)packed.type; }
	BOOL			IsUrlString() const { return packed.type == StringDeclUrl; }
	BOOL			IsSkinString() const { return packed.type == StringDeclSkin; }
	BOOL			IsSourceLocal() const { return packed.source_local; }
	virtual OP_STATUS
					PrefetchURLs(FramesDocument* doc);
};

class CSS_number_decl
  : public CSS_decl
{
protected:

	float			value;

public:
					CSS_number_decl(short prop, float val, int val_type);

	virtual CSS_decl* CreateCopy() const { CSS_decl* new_decl = OP_NEW(CSS_number_decl, (GetProperty(), value, GetValueType(0))); return new_decl; }
	virtual CSSDeclType
					GetDeclType() const { return CSS_DECL_NUMBER; }
	virtual int		GetNumbers() const { return 1; }

    virtual float	GetNumberValue(int i) const { return value; }
    virtual short	GetValueType(int i) const { return short(info.data.extra.value_type | 0x0100); }

	virtual void	SetNumberValue(int i, float val, int val_type) { value = val; info.data.extra.value_type = val_type; }

	virtual BOOL    IsEqual(const CSS_decl* decl) const;
};

class CSS_number2_decl
  : public CSS_number_decl
{
private:

	float			value2;
	short			value2_type;

public:
					CSS_number2_decl(short prop, float val1, float val2, int val1_type, int val2_type);

	virtual CSS_decl* CreateCopy() const { CSS_decl* new_decl = OP_NEW(CSS_number2_decl, (GetProperty(), GetNumberValue(0), GetNumberValue(1), GetValueType(0), GetValueType(1))); return new_decl; }
	virtual CSSDeclType
					GetDeclType() const { return CSS_DECL_NUMBER2; }
	virtual int		GetNumbers() const { return 2; }

    virtual float	GetNumberValue(int i) const { return i == 0 ? value : value2; }
    virtual short	GetValueType(int i) const { return i == 0 ? short(info.data.extra.value_type | 0x0100) : value2_type; }

	virtual void	SetNumberValue(int i, float val, int val_type) { if (i == 0) CSS_number_decl::SetNumberValue(i, val, val_type); else { value2 = val; value2_type = val_type; } }

    virtual BOOL    IsEqual(const CSS_decl* decl) const;
};

class CSS_number4_decl
  : public CSS_decl
{
private:

	float			value[4];
	short			value_type[4];

public:
					CSS_number4_decl(short prop);

	virtual CSS_decl* CreateCopy() const;
	virtual CSSDeclType
					GetDeclType() const { return CSS_DECL_NUMBER4; }
	virtual int		GetNumbers() const { return 4; }

	void			SetValue(int i, short type, float val) { if (i>=0 && i<4) { value_type[i]=type; value[i]=val; } }
	virtual short	GetValueType(int i) const { if (i>= 0 && i<4) return value_type[i]; else return CSS_NUMBER; }
	virtual float	GetNumberValue(int i) const { if (i>=0 && i<4) return value[i]; else return 0.0f; }

    virtual BOOL    IsEqual(const CSS_decl* decl) const;
};

#ifdef CSS_TRANSFORMS

/** A CSS declaration representation of a transform list.

	Stores a linked list of transform operarations. */
class CSS_transform_list
  : public CSS_decl
{
public:

	/** One transform item in the linked list */
	class CSS_transform_item : public Link
	{
	public:
		/** A CSSValue fulfilling CSS_is_transform_function(v) */
		short type;

		/** The number of values and value types. */
		short n_values;

		/** The number values */
		float value[6];

		/** The number value types */
		short value_type[6];

		CSS_transform_item*
					Suc() { return static_cast<CSS_transform_item*>(Link::Suc()); }

		const CSS_transform_item*
					Suc() const { return static_cast<CSS_transform_item*>(Link::Suc()); }

		/** Make this item of CSS_VALUE_matrix with values from the AffineTransform
			parameter.

			@param at The AffineTransform to initialize the matrix values from. */
		void SetFromAffineTransform(const AffineTransform& at);

#ifdef CSS_TRANSITIONS
		/** Compute a transition between two transform items. */
		BOOL		Transition(const CSS_transform_item& item1, const CSS_transform_item& item2, float percentage);

		/** Resolve relative lengths. */
		void		ResolveRelativeLengths(const CSSLengthResolver& length_resolver);

		/** Construct a default value suitable for transitions. Used
			when transitioning from or to a empty value. */
		CSS_transform_item* MakeCompatibleZero(const CSS_transform_item* item);

		/** If the two items can be made compatible for transition by changing
			function types and/or units, do so and return TRUE, otherwise
			return FALSE. */
		static BOOL	MakeCompatible(CSS_transform_item& from, CSS_transform_item& to);

	private:
		/** Expand X/Y functions to two-component functions. */
		void ExpandFunction();

#endif // CSS_TRANSITIONS

	};

	/** Constructor */
    				CSS_transform_list(short prop);

	/** Deconstructor */
    virtual			~CSS_transform_list();

	/** Construct a CSS_transform_list containing a single matrix item
		with the values from an AffineTransform.  Returns NULL on
		OOM. */
	static CSS_transform_list*
					MakeFromAffineTransform(const AffineTransform& at);

	/** Start a new transform function.  Called from the CSS parser. */
    OP_STATUS		StartTransformFunction(short operation_type);

	/** Append value to the current transform function.  Called from the CSS parser. */
    void			AppendValue(float value, short value_type);

	/** Get the affine transform.  Get the product of the transform
		list as an AffineTransform.  Requires absolute lengths so it
		returns FALSE if there are lengths that are not absolute and
		the result should not be trusted. */
	BOOL			GetAffineTransform(AffineTransform& res) const;

#ifdef CSS_TRANSITIONS
	/** Convert transform lists into transforms compatible for transitioning. */
	static BOOL		MakeCompatible(CSS_transform_list& from, CSS_transform_list& to);

	/** Resolve relative lengths. */
	void			ResolveRelativeLengths(const CSSLengthResolver& length_resolver);

	/** Transition */
	BOOL			Transition(CSS_transform_list* from, CSS_transform_list* to, float percentage);

#endif // CSS_TRANSITIONS

	/** Create a copy of this transform list. */
	virtual CSS_decl*
					CreateCopy() const;

	/** Get the declaration type */
	virtual CSSDeclType
					GetDeclType() const { return CSS_DECL_TRANSFORM_LIST; }

	/** Is this transform list equal to another transform
		list. Requires exact "equalness", i.e. does not consider
		values like 'translate(10px)' and 'translate(10px, 0)') to be
		equal. */
	virtual BOOL    IsEqual(const CSS_decl* decl) const;

	CSS_transform_list*
					MakeCompatibleZeroList() const;

	const CSS_transform_item*
					First() const { return static_cast<CSS_transform_item*>(values.First()); }

private:
	Head values;
};

#endif // CSS_TRANSFORMS

#ifdef _DEBUG
Debug& operator<<(Debug& d, const CSS_decl& decl);
#endif // DEBUG

#endif // CSS_DECL_H
