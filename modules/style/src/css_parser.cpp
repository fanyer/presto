/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/display/vis_dev.h"
#include "modules/layout/cssprops.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/xmlenum.h"
#include "modules/display/styl_man.h"
#include "modules/style/css.h"
#include "modules/style/css_media.h"
#include "modules/style/src/css_parser.h"
#include "modules/style/src/css_import_rule.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/util/gen_math.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/windowcommander/OpWindowCommander.h"

#include "modules/style/src/css_aliases.h"

#ifdef SCOPE_SUPPORT
# include "modules/probetools/probetimeline.h"
#endif // SCOPE_SUPPORT

#define CSS_MAX_ARR_SIZE 32

/** Anchors a bunch of heap arrays or strings to the stack. The anchor will take responsibility
    for deleting objects that are added to it.

    @author rune */
template<class T> class CSS_anchored_heap_arrays
{
private:
	T* m_initial_objects[CSS_MAX_ARR_SIZE];
	T** m_objects;
	int m_objects_size;
	int m_count;

	/** Check that there's room for adding another array */
	void CheckSizeL()
	{
		if (m_count + 1 == m_objects_size)
		{
			int new_size = m_objects_size * 2;
			T** new_array = OP_NEWA_L(T*, new_size);

			for (int i=0; i<m_count; i++)
				new_array[i] = m_objects[i];

			if (m_objects != m_initial_objects)
				OP_DELETEA(m_objects);

			m_objects_size = new_size;
			m_objects = new_array;
		}
	}

public:

	CSS_anchored_heap_arrays() : m_objects(m_initial_objects), m_objects_size(CSS_MAX_ARR_SIZE), m_count(0) {}

	/** Deletes all anchored arrays. */
	~CSS_anchored_heap_arrays()
	{
		for (int i=0; i<m_count; i++)
			OP_DELETEA(m_objects[i]);
		if (m_objects != m_initial_objects)
			OP_DELETEA(m_objects);
	}

	/** Add object to list of anchored objects.

		@param obj Heap allocated T* array. This anchor takes over
				   the responsibility for deleting the array - even if this
				   method should leave.

		@return Leaves on OOM. */

	void AddArrayL(T* obj)
	{
		// Anchor the object using OpHeapArrayAnchor while doing CheckSizeL in case it leaves
		ANCHOR_ARRAY(T, obj);
		CheckSizeL();
		ANCHOR_ARRAY_RELEASE(obj); // Now we're insured against OOM, so release the other anchor

		m_objects[m_count++] = obj;
	}
};

/** Anchors a bunch of heap objects to the stack. The anchor will take responsibility
    for deleting objects that are added to it.

    @author lstorset */
template<class T> class CSS_anchored_heap_objects
{
private:
	T* m_initial_objects[CSS_MAX_ARR_SIZE];
	T** m_objects;
	int m_objects_size;
	int m_count;

	/** Check that there's room for adding another object */
	void CheckSizeL()
	{
		if (m_count + 1 == m_objects_size)
		{
			int new_size = m_objects_size * 2;
			T** new_array = OP_NEWA_L(T*, new_size);

			for (int i=0; i<m_count; i++)
				new_array[i] = m_objects[i];

			if (m_objects != m_initial_objects)
				OP_DELETEA(m_objects);

			m_objects_size = new_size;
			m_objects = new_array;
		}
	}

public:

	CSS_anchored_heap_objects() : m_objects(m_initial_objects), m_objects_size(CSS_MAX_ARR_SIZE), m_count(0) {}

	/** Deletes all anchored objects. */
	~CSS_anchored_heap_objects()
	{
		for (int i=0; i<m_count; i++)
			OP_DELETE(m_objects[i]);
		if (m_objects != m_initial_objects)
			OP_DELETEA(m_objects);
	}

	/** Add object to list of anchored objects.

		@param obj Heap allocated T* object. This anchor takes over
				   the responsibility for deleting the object - even if this
				   method should leave.

	    @return Leaves on OOM. */

	void AddObjectL(T* obj)
	{
		// Anchor the object using OpHeapAnchor while doing CheckSizeL in case it leaves
		ANCHOR_PTR(T, obj);
		CheckSizeL();
		ANCHOR_PTR_RELEASE(obj); // Now we're insured against OOM, so release the other anchor

		m_objects[m_count++] = obj;
	}
};

class CSS_TransformFunctionInfo
{
public:
	CSS_TransformFunctionInfo(CSSValue t) : type(t), n_args(1), allow_short_form(FALSE)
	{
		switch (type)
		{
		case CSS_VALUE_matrix:
			n_args = 6;
			break;
		case CSS_VALUE_scale:
		case CSS_VALUE_translate:
		case CSS_VALUE_skew:
			n_args = 2;
			allow_short_form = TRUE;
			break;
		}
	}

	BOOL IsValidUnit(BOOL allow_implicit_unit, short& unit)
	{
		switch (type)
		{
		case CSS_VALUE_matrix:
		case CSS_VALUE_scale:
		case CSS_VALUE_scaleX:
		case CSS_VALUE_scaleY:
			return unit == CSS_NUMBER;

		case CSS_VALUE_translate:
		case CSS_VALUE_translateX:
		case CSS_VALUE_translateY:

			if (unit == CSS_NUMBER && allow_implicit_unit)
			{
				unit = CSS_PX;
				return TRUE;
			}
			else
				return CSS_is_length_number_ext(unit) || unit == CSS_PERCENTAGE;

		case CSS_VALUE_skew:
		case CSS_VALUE_skewX:
		case CSS_VALUE_skewY:
		case CSS_VALUE_rotate:
			return unit == CSS_RAD;
		default:
			OP_ASSERT(!"Not reached");
			return FALSE;
		}
	}

	int MaxArgCount() { return n_args; }
	BOOL ValidArgCount(int i) { return i == n_args || allow_short_form && i == 1; }

private:

	CSSValue type;
	int n_args;
	BOOL allow_short_form;
};

/** Helper class for parsing a background shorthand declaration. */
class CSS_BackgroundShorthandInfo
{
public:
	CSS_BackgroundShorthandInfo(const CSS_Parser& p, CSS_anchored_heap_arrays<uni_char>& s
#ifdef CSS_GRADIENT_SUPPORT
	, CSS_anchored_heap_objects<CSS_Gradient>& g
#endif // CSS_GRADIENT_SUPPORT
	);

	/** Does the value array represent 'inherit'? */
	BOOL IsInherit() const;

	/** After ExtractLayerL() has returned FALSE for the first time,
		this function returns whether there was any error detected
		during parsing.  Before ExtractLayerL() has returned FALSE, this
		function always returns FALSE. */
	BOOL IsInvalid() const { return !m_is_valid; }

	/** Extract next layer from the value array into separate
		arrays. Returns FALSE when there are no more layers to
		extract. */
	BOOL ExtractLayerL(CSS_generic_value_list& bg_attachment_arr,
					   CSS_generic_value_list& bg_repeat_arr,
					   CSS_generic_value_list& bg_image_arr,
					   CSS_generic_value_list& bg_pos_arr,
					   CSS_generic_value_list& bg_size_arr,
					   CSS_generic_value_list& bg_origin_arr,
					   CSS_generic_value_list& bg_clip_arr,
					   COLORREF& bg_color_value,
					   BOOL& bg_color_is_keyword,
					   CSSValue& bg_color_keyword);

private:

	struct Layer
	{
		void Reset();

		CSSValue bg_attachment;
		BOOL bg_attachment_set;

		CSSValue bg_repeat[2];
		int n_bg_repeat;

		const uni_char* bg_image;
#ifdef CSS_GRADIENT_SUPPORT
		CSS_Gradient* bg_gradient;
#endif // CSS_GRADIENT_SUPPORT
		BOOL bg_image_set;
		BOOL bg_image_is_skin;

		CSS_generic_value bg_size[2];
		int n_bg_size;

		COLORREF bg_color_value;
		CSSValue bg_color_keyword;
		BOOL bg_color_set;
		BOOL bg_color_is_keyword;

		float bg_pos[2];
		CSSValueType bg_pos_type[2];
		CSSValue bg_pos_ref[2];
		BOOL bg_pos_set;
		BOOL bg_pos_has_ref_point;

		CSSValue bg_origin;
		BOOL bg_origin_and_clip_set;
		CSSValue bg_clip;
		BOOL bg_clip_set;

		BOOL last_layer;
	};

	BOOL ParseLayer(Layer& layer);

	BOOL Invalid() { m_is_valid = FALSE; return FALSE; }

	const CSS_Parser& m_parser;
	CSS_anchored_heap_arrays<uni_char>& m_strings;
#ifdef CSS_GRADIENT_SUPPORT
	CSS_anchored_heap_objects<CSS_Gradient>& m_gradients;
#endif //CSS_GRADIENT_SUPPORT
	const CSS_Parser::ValueArray& m_val_array;
	CSS_Buffer* m_input_buffer;

	BOOL m_is_valid;
	int m_i;
};

CSS_BackgroundShorthandInfo::CSS_BackgroundShorthandInfo(const CSS_Parser& p, CSS_anchored_heap_arrays<uni_char>& s
#ifdef CSS_GRADIENT_SUPPORT
, CSS_anchored_heap_objects<CSS_Gradient>& g
#endif //CSS_GRADIENT_SUPPORT
) :
	m_parser(p),
	m_strings(s),
#ifdef CSS_GRADIENT_SUPPORT
	m_gradients(g),
#endif //CSS_GRADIENT_SUPPORT
	m_val_array(p.m_val_array),
	m_input_buffer(p.m_input_buffer),
	m_is_valid(TRUE),
	m_i(0)
{
}

void CSS_BackgroundShorthandInfo::Layer::Reset()
{
	bg_attachment = CSS_VALUE_scroll;
	bg_attachment_set = FALSE;

	bg_repeat[0] = CSS_VALUE_repeat;
	bg_repeat[1] = CSS_VALUE_repeat;
	n_bg_repeat = 0;

	bg_image = NULL;
#ifdef CSS_GRADIENT_SUPPORT
	bg_gradient = NULL;
#endif //CSS_GRADIENT_SUPPORT
	bg_image_set = FALSE;
	bg_image_is_skin = FALSE;

	bg_size[0].SetValueType(CSS_IDENT);
	bg_size[0].SetType(CSS_VALUE_auto);
	bg_size[1].SetValueType(CSS_IDENT);
	bg_size[1].SetType(CSS_VALUE_auto);
	n_bg_size = 0;

	bg_color_value = USE_DEFAULT_COLOR;
	bg_color_keyword = CSS_VALUE_transparent;
	bg_color_set = FALSE;
	bg_color_is_keyword = FALSE;

	bg_pos[0] = 0;
	bg_pos[1] = 0;
	bg_pos_type[0] = CSS_PERCENTAGE;
	bg_pos_type[1] = CSS_PERCENTAGE;
	bg_pos_set = FALSE;
	bg_pos_ref[0] = CSS_VALUE_left;
	bg_pos_ref[1] = CSS_VALUE_top;
	bg_pos_has_ref_point = FALSE;

	bg_origin = CSS_VALUE_padding_box;
	bg_origin_and_clip_set = FALSE;
	bg_clip = CSS_VALUE_border_box;
	bg_clip_set = FALSE;

	last_layer = TRUE;
}

BOOL
CSS_BackgroundShorthandInfo::IsInherit() const
{
	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos,
													   m_val_array[0].value.str.str_len);
		return (keyword == CSS_VALUE_inherit);
	}
	else
		return FALSE;
}

BOOL
CSS_BackgroundShorthandInfo::ParseLayer(CSS_BackgroundShorthandInfo::Layer& layer)
{
	if (m_i >= m_val_array.GetCount())
		return FALSE;

	BOOL quirks_mode = m_parser.m_hld_prof && !m_parser.m_hld_prof->IsInStrictMode();

	enum {
		STATE_START,
		STATE_REPEAT,
		STATE_AFTER_POSITION
	} state = STATE_START;

	layer.Reset();

	while (m_i < m_val_array.GetCount())
	{
		if (m_val_array[m_i].token == CSS_IDENT)
		{
			// attachment, color, position, repeat
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[m_i].value.str.start_pos,
															  m_val_array[m_i].value.str.str_len);

			if (CSS_is_bg_attachment_val(keyword))
			{
				if (layer.bg_attachment_set)
					return Invalid();;

				layer.bg_attachment = keyword;
				layer.bg_attachment_set = TRUE;
				state = STATE_START;
			}
			else if (CSS_is_bg_repeat_val(keyword))
			{
				if (layer.n_bg_repeat == 0)
				{
					layer.bg_repeat[layer.n_bg_repeat++] = keyword;

					if (keyword != CSS_VALUE_repeat_x && keyword != CSS_VALUE_repeat_y)
						state = STATE_REPEAT;
				}
				else if (layer.n_bg_repeat == 1 && state == STATE_REPEAT &&
						 (keyword != CSS_VALUE_repeat_x && keyword != CSS_VALUE_repeat_y))
				{
					layer.bg_repeat[layer.n_bg_repeat++] = keyword;
					state = STATE_START;
				}
				else
					return Invalid();
			}
			else if (CSS_is_position_val(keyword))
			{
				if (layer.bg_pos_set)
					return Invalid();

				CSS_Parser::DeclStatus ret = m_parser.SetPosition(m_i, layer.bg_pos, layer.bg_pos_type, TRUE, layer.bg_pos_ref, layer.bg_pos_has_ref_point);
				if (ret != CSS_Parser::OK)
					return Invalid();
				else
					layer.bg_pos_set = TRUE;

				--m_i; // already incremented in SetPosition

				state = STATE_AFTER_POSITION;
			}
			else if (keyword >= 0 && CSS_is_color_val(keyword) ||
					 m_input_buffer->GetNamedColorIndex(m_val_array[m_i].value.str.start_pos,
														m_val_array[m_i].value.str.str_len) != USE_DEFAULT_COLOR)
			{
				if (layer.bg_color_set)
					return Invalid();
				if (CSS_is_ui_color_val(keyword))
				{
					layer.bg_color_value = keyword | CSS_COLOR_KEYWORD_TYPE_ui_color;
					layer.bg_color_is_keyword = TRUE;
				}
				else
				{
					layer.bg_color_value = m_input_buffer->GetNamedColorIndex(m_val_array[m_i].value.str.start_pos,
																			  m_val_array[m_i].value.str.str_len);
					layer.bg_color_is_keyword = TRUE;
				}
				layer.bg_color_set = TRUE;
				state = STATE_START;
			}
			else if (keyword == CSS_VALUE_transparent || keyword == CSS_VALUE_currentColor)
			{
				if (layer.bg_color_set)
					return Invalid();
				layer.bg_color_keyword = keyword;
				layer.bg_color_set = TRUE;
				state = STATE_START;
			}
			else if (keyword == CSS_VALUE_none)
			{
				if (layer.bg_image_set)
					return Invalid();
				layer.bg_image_set = TRUE;
				layer.bg_image = NULL;
				state = STATE_START;
			}
			else if (CSS_is_background_clip_or_origin_val(keyword))
			{
				if (!layer.bg_origin_and_clip_set)
				{
					layer.bg_origin = keyword;
					layer.bg_clip = keyword;
					layer.bg_origin_and_clip_set = TRUE;
				}
				else
					if (!layer.bg_clip_set)
					{
						layer.bg_clip = keyword;
						layer.bg_clip_set = TRUE;
					}
					else
						return Invalid();

				state = STATE_START;
			}
			else if (keyword < 0 && quirks_mode)
			{
				if (layer.bg_color_set)
					return Invalid();
				COLORREF col = m_input_buffer->GetColor(m_val_array[m_i].value.str.start_pos,
														m_val_array[m_i].value.str.str_len);
				if (col == USE_DEFAULT_COLOR)
					return Invalid();
				layer.bg_color_value = col;
				layer.bg_color_set = TRUE;
				state = STATE_START;
			}
			else
				return Invalid();
		}
		else if (state == STATE_AFTER_POSITION && m_val_array[m_i].token == '/')
		{
			/* bg-size */

			BOOL done = FALSE;
			OP_ASSERT(layer.n_bg_size == 0);

			if (++m_i >= m_val_array.GetCount())
				return Invalid();

			if (m_val_array[m_i].token == CSS_IDENT)
			{
				CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[m_i].value.str.start_pos,
																  m_val_array[m_i].value.str.str_len);

				if (keyword == CSS_VALUE_cover || keyword == CSS_VALUE_contain || keyword == CSS_VALUE_auto)
				{
					layer.bg_size[layer.n_bg_size].SetValueType(CSS_IDENT);
					layer.bg_size[layer.n_bg_size++].SetType(keyword);

					if (keyword == CSS_VALUE_cover || keyword == CSS_VALUE_contain)
						done = TRUE;
				}
				else
					return Invalid();
			}
			else if (m_val_array[m_i].token == CSS_NUMBER ||
					 m_val_array[m_i].token == CSS_PERCENTAGE ||
					 CSS_is_length_number_ext(m_val_array[m_i].token))
			{
				short type = m_val_array[m_i].token;
				if (type == CSS_NUMBER)
					type = CSS_PX;
				layer.bg_size[layer.n_bg_size].SetValueType(type);
				layer.bg_size[layer.n_bg_size++].SetReal(float(m_val_array[m_i].value.number.number));
			}
			else
				return Invalid();

			if (!done && (m_i+1) < m_val_array.GetCount())
			{
				// Try next token

				m_i++;

				if (m_val_array[m_i].token == CSS_IDENT)
				{
					CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[m_i].value.str.start_pos,
																	  m_val_array[m_i].value.str.str_len);

					if (keyword == CSS_VALUE_auto)
					{
						layer.bg_size[layer.n_bg_size].SetValueType(CSS_IDENT);
						layer.bg_size[layer.n_bg_size++].SetType(keyword);
					}
					else
						--m_i;
				}
				else if (m_val_array[m_i].token == CSS_NUMBER ||
						 m_val_array[m_i].token == CSS_PERCENTAGE ||
						 CSS_is_length_number_ext(m_val_array[m_i].token))
				{
					short type = m_val_array[m_i].token;
					if (type == CSS_NUMBER)
						type = CSS_PX;
					layer.bg_size[layer.n_bg_size].SetValueType(type);
					layer.bg_size[layer.n_bg_size++].SetReal(float(m_val_array[m_i].value.number.number));
				}
				else
					--m_i;
			}

			state = STATE_START;
		}
		else if (m_val_array[m_i].token == CSS_HASH)
		{
			/* bg-color */

			layer.bg_color_value = m_input_buffer->GetColor(m_val_array[m_i].value.str.start_pos,
															m_val_array[m_i].value.str.str_len);
			if (layer.bg_color_set || layer.bg_color_value == USE_DEFAULT_COLOR)
				return Invalid();
			layer.bg_color_set = TRUE;
			state = STATE_START;
		}
		else if (m_val_array[m_i].token == CSS_FUNCTION_RGB ||
				 m_parser.SupportsAlpha() && m_val_array[m_i].token == CSS_FUNCTION_RGBA)
		{
			/* bg-color */

			layer.bg_color_value = m_val_array[m_i].value.color;
			layer.bg_color_set = TRUE;
			state = STATE_START;
		}
		else if (m_val_array[m_i].token == CSS_FUNCTION_URL)
		{
			/* bg-image */

			if (layer.bg_image_set)
				return Invalid();
			layer.bg_image = m_input_buffer->GetURLL(m_parser.m_base_url,
													 m_val_array[m_i].value.str.start_pos,
													 m_val_array[m_i].value.str.str_len).GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
			layer.bg_image_set = TRUE;
			state = STATE_START;
		}
#ifdef SKIN_SUPPORT
		else if (m_val_array[m_i].token == CSS_FUNCTION_SKIN)
		{
			if (layer.bg_image_set)
				return Invalid();
			uni_char* skin_image = m_input_buffer->GetSkinString(m_val_array[m_i].value.str.start_pos,
																 m_val_array[m_i].value.str.str_len);
			layer.bg_image = skin_image;
			if (!layer.bg_image)
				LEAVE(OpStatus::ERR_NO_MEMORY);
			m_strings.AddArrayL(skin_image);

			layer.bg_image_set = TRUE;
			layer.bg_image_is_skin = TRUE;
			state = STATE_START;
		}
#endif // SKIN_SUPPORT
#ifdef CSS_GRADIENT_SUPPORT
		else if (CSS_is_gradient(m_val_array[m_i].token))
		{
			if (layer.bg_image_set)
				return Invalid();
			CSSValueType type = (CSSValueType) m_val_array[m_i].token;
			CSS_Gradient* gradient = m_parser.SetGradientL(/* inout */ m_i, /* inout */ type);
			if (!gradient)
				return Invalid();
			m_gradients.AddObjectL(gradient);

			layer.bg_gradient = gradient;
			layer.bg_image_set = TRUE;
		}
#endif // CSS_GRADIENT_SUPPORT
		else if (CSS_is_number(m_val_array[m_i].token))
		{
			CSS_Parser::DeclStatus ret = layer.bg_pos_set ? CSS_Parser::INVALID : m_parser.SetPosition(m_i, layer.bg_pos, layer.bg_pos_type, TRUE, layer.bg_pos_ref, layer.bg_pos_has_ref_point);
			if (ret == CSS_Parser::INVALID)
			{
				if (quirks_mode && !layer.bg_color_set)
				{
					COLORREF col = m_input_buffer->GetColor(m_val_array[m_i].value.number.start_pos,
															m_val_array[m_i].value.number.str_len);
					if (col == USE_DEFAULT_COLOR)
						return Invalid();
					layer.bg_color_value = col;
					layer.bg_color_set = TRUE;
				}
				else
					return Invalid();
			}
			else
			{
				--m_i; // already incremented in SetPosition
				layer.bg_pos_set = TRUE;
			}

			state = STATE_AFTER_POSITION;
		}
		else if (m_val_array[m_i].token == CSS_COMMA)
		{
			m_i++;
			layer.last_layer = FALSE;
			break;
		}
		else
		{
			return Invalid();
		}

		m_i++;
	}

	if (layer.bg_color_set && !layer.last_layer)
		/* background color only allowed on the last layer */

		return Invalid();

	return TRUE;
}

BOOL
CSS_BackgroundShorthandInfo::ExtractLayerL(CSS_generic_value_list& bg_attachment_arr,
										   CSS_generic_value_list& bg_repeat_arr,
										   CSS_generic_value_list& bg_image_arr,
										   CSS_generic_value_list& bg_pos_arr,
										   CSS_generic_value_list& bg_size_arr,
										   CSS_generic_value_list& bg_origin_arr,
										   CSS_generic_value_list& bg_clip_arr,
										   COLORREF& bg_color_value,
										   BOOL& bg_color_is_keyword,
										   CSSValue& bg_color_keyword)
{
	Layer layer;

	if (ParseLayer(layer))
	{
		/* background-attachment */

		bg_attachment_arr.PushIdentL(layer.bg_attachment);

		/* background-repeat */

		bg_repeat_arr.PushIdentL(layer.bg_repeat[0]);
		if (layer.n_bg_repeat > 1)
			bg_repeat_arr.PushIdentL(layer.bg_repeat[1]);
		if (!layer.last_layer)
			bg_repeat_arr.PushValueTypeL(CSS_COMMA);

		/* background-image */

		if (layer.bg_image)
		{
			uni_char* url_str = OP_NEWA_L(uni_char, uni_strlen(layer.bg_image)+1);
			m_strings.AddArrayL(url_str);
			uni_strcpy(url_str, layer.bg_image);

			bg_image_arr.PushStringL(layer.bg_image_is_skin ? CSS_FUNCTION_SKIN : CSS_FUNCTION_URL, url_str);
		}
#ifdef CSS_GRADIENT_SUPPORT
		else if (layer.bg_gradient)
		{
			CSS_Gradient* gradient = layer.bg_gradient->CopyL();
			m_gradients.AddObjectL(gradient);

			bg_image_arr.PushGradientL(gradient->GetCSSValueType(), gradient);
		}
#endif // CSS_GRADIENT_SUPPORT
		else
			bg_image_arr.PushIdentL(CSS_VALUE_none);

		/* background-position */

		if (layer.bg_pos_has_ref_point)
			bg_pos_arr.PushIdentL(layer.bg_pos_ref[0]);
		bg_pos_arr.PushNumberL(layer.bg_pos_type[0], layer.bg_pos[0]);

		if (layer.bg_pos_has_ref_point)
			bg_pos_arr.PushIdentL(layer.bg_pos_ref[1]);
		bg_pos_arr.PushNumberL(layer.bg_pos_type[1], layer.bg_pos[1]);

		if (!layer.last_layer)
			bg_pos_arr.PushValueTypeL(CSS_COMMA);

		/* background-size */

		bg_size_arr.PushGenericValueL(layer.bg_size[0]);
		if (layer.n_bg_size > 1)
			bg_size_arr.PushGenericValueL(layer.bg_size[1]);
		if (!layer.last_layer)
			bg_size_arr.PushValueTypeL(CSS_COMMA);

		if (layer.bg_color_set)
		{
			if (!layer.last_layer)
				return Invalid();

			bg_color_value = layer.bg_color_value;
			bg_color_is_keyword = layer.bg_color_is_keyword;
			bg_color_keyword = layer.bg_color_keyword;
		}

		/* background-origin */

		bg_origin_arr.PushIdentL(layer.bg_origin);

		/* background-clip */

		bg_clip_arr.PushIdentL(layer.bg_clip);

		return TRUE;
	}

	return FALSE;
}

void CSS_Parser::ValueArray::CommitToL(ValueArray& dest)
{
	for (int i = 0; i < m_val_count; i++)
		dest.AddValueL(m_val_array[i]);

	ResetValCount();
}


extern int cssparse(void* parm);

CSS_Parser::CSS_Parser(CSS* css, CSS_Buffer* buf, const URL& base_url, HLDocProfile* hld_profile, unsigned start_line_number) :
	m_lexer(buf),
	m_stylesheet(css),
	m_input_buffer(buf),
	m_hld_prof(hld_profile),
	m_val_array(CSS_VAL_ARRAY_SIZE, m_default_val_array),
	m_current_props(0),
	m_base_url(base_url),
	m_allow_min(ALLOW_CHARSET),
	m_allow_max(ALLOW_STYLE),
	m_decl_type(DECL_RULESET),
	m_next_token(0),
#ifdef MEDIA_HTML_SUPPORT
	m_saved_current_selector(0),
	m_in_cue(FALSE),
#endif // MEDIA_HTML_SUPPORT
	m_current_selector(0),
#ifdef CSS_ANIMATIONS
	m_current_keyframes_rule(NULL),
#endif
	m_current_conditional_rule(0),
	m_current_media_object(0),
	m_current_media_query(0),
	m_replace_rule(FALSE),
	m_dom_rule(0),
	m_dom_property(-1),
	m_current_ns_idx(NS_IDX_ANY_NAMESPACE),
	m_error_occurred(CSSParseStatus::OK),
	m_last_decl(NULL),
	m_document_line(start_line_number),
	m_condition_list(NULL)
{
	m_yystack[0] = m_yystack[1] = NULL;
	m_user = css && css->GetUserDefined();
#ifdef SCOPE_CSS_RULE_ORIGIN
	m_rule_line_no = 0;
#endif // SCOPE_CSS_RULE_ORIGIN
}

CSS_Parser::~CSS_Parser()
{
	m_selector_list.Clear();
#ifdef MEDIA_HTML_SUPPORT
	m_cue_selector_list.Clear();
#endif // MEDIA_HTML_SUPPORT
	m_simple_selector_stack.Clear();
	if (m_current_props)
		m_current_props->Unref();
	OP_DELETE(m_current_selector);
	OP_DELETE(m_current_media_object);
	OP_DELETE(m_current_media_query);
	if (m_yystack[0])
		op_free(m_yystack[0]);
	if (m_yystack[1])
		op_free(m_yystack[1]);
	OP_ASSERT(!m_condition_list);
}

// From yyexhaustedlab in css_grammar.cpp
#define YY_OOM_VAL 2

void
CSS_Parser::ParseL()
{
#ifdef SCOPE_PROFILER
	OpPropertyProbe probe;
	ANCHOR(OpPropertyProbe, probe);

	if (HLDProfile() && HLDProfile()->GetFramesDocument()->GetTimeline())
	{
		OpProbeTimeline* t = HLDProfile()->GetFramesDocument()->GetTimeline();
		const URL& url = GetBaseURL();

		OpPropertyProbe::Activator<1> act(probe);
		ANCHOR(OpPropertyProbe::Activator<1>, act);

		LEAVE_IF_FATAL(act.Activate(t, PROBE_EVENT_CSS_PARSING, url.GetRep()));
		LEAVE_IF_FATAL(act.AddURL("url", const_cast<URL*>(&url)));
	}
#endif // SCOPE_PROFILER

	int ret = cssparse(this);

#ifdef CSS_ERROR_SUPPORT
	if (m_lexer.GetTruncationBits() != CSS_Lexer::TRUNC_NONE)
		EmitErrorL(UNI_L("Unexpected end of file"), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT

	if (ret == 0 && m_error_occurred == CSSParseStatus::OK)
		return;
	else if (ret == YY_OOM_VAL)
		LEAVE(CSSParseStatus::ERR_NO_MEMORY);
	else if (m_error_occurred != CSSParseStatus::OK)
		LEAVE(m_error_occurred);
	else
		LEAVE(CSSParseStatus::SYNTAX_ERROR);
}

int
CSS_Parser::Lex(YYSTYPE* value)
{
	if (m_next_token)
	{
		int ret = m_next_token;
		m_next_token = 0;
		if (ret == CSS_TOK_DOM_MEDIA_LIST || ret == CSS_TOK_DOM_MEDIUM)
			m_lexer.ForceInMediaRule();
		return ret;
	}
	return m_lexer.Lex(value);
}

static short ResolveFontWeight(CSSValue keyword)
{
	short fweight = 0;
	if (keyword == CSS_VALUE_normal)
		fweight = 4;
	else if (keyword == CSS_VALUE_bold)
		fweight = 7;
	else if (keyword != CSS_VALUE_bolder && keyword != CSS_VALUE_lighter && keyword != CSS_VALUE_inherit)
		fweight = -1;

	return fweight;
}

/** Return TRUE if the value is a valid value for the flex-direction property. */

static BOOL IsFlexDirectionValue(CSSValue value)
{
	switch (value)
	{
	case CSS_VALUE_inherit:
	case CSS_VALUE_row:
	case CSS_VALUE_row_reverse:
	case CSS_VALUE_column:
	case CSS_VALUE_column_reverse:
		return TRUE;
	default:
		return FALSE;
	}
}

/** Return TRUE if the value is a valid value for the flex-wrap property. */

static BOOL IsFlexWrapValue(CSSValue value)
{
	switch (value)
	{
	case CSS_VALUE_inherit:
	case CSS_VALUE_nowrap:
	case CSS_VALUE_wrap:
	case CSS_VALUE_wrap_reverse:
		return TRUE;
	default:
		return FALSE;
	}
}

CSS_Parser::ColorValueType
CSS_Parser::SetColorL(COLORREF& color, CSSValue& keyword, uni_char*& skin_color, const PropertyValue& color_value) const
{
	color = USE_DEFAULT_COLOR;
	if (color_value.token == CSS_IDENT)
	{
		keyword = m_input_buffer->GetValueSymbol(color_value.value.str.start_pos, color_value.value.str.str_len);
		if (keyword == CSS_VALUE_transparent
			|| keyword == CSS_VALUE_inherit
			|| keyword == CSS_VALUE_invert
			|| keyword == CSS_VALUE_currentColor)
			return COLOR_KEYWORD;
	}

#ifdef SKIN_SUPPORT
	if (color_value.token == CSS_FUNCTION_SKIN)
	{
		skin_color = m_input_buffer->GetSkinString(color_value.value.str.start_pos, color_value.value.str.str_len);
		if (skin_color)
		{
			return COLOR_SKIN;
		}
		else
			LEAVE(OpStatus::ERR_NO_MEMORY);
	}
#endif // SKIN_SUPPORT

	// We found no special keyword and no skin color; look for a color
	if (color_value.token == CSS_HASH)
	{
		color = m_input_buffer->GetColor(color_value.value.str.start_pos, color_value.value.str.str_len);
	}
	else if (color_value.token == CSS_FUNCTION_RGB || SupportsAlpha() && color_value.token == CSS_FUNCTION_RGBA)
	{
		color = color_value.value.color;
	}
	else if (color_value.token == CSS_IDENT && CSS_is_ui_color_val(keyword))
	{
		color = keyword | CSS_COLOR_KEYWORD_TYPE_ui_color;
		return COLOR_NAMED;
	}
	else if (color_value.token == CSS_IDENT)
	{
		color = m_input_buffer->GetNamedColorIndex(color_value.value.str.start_pos, color_value.value.str.str_len);
		if (color == USE_DEFAULT_COLOR)
		{
			if (m_hld_prof && !m_hld_prof->IsInStrictMode())
				color = m_input_buffer->GetColor(color_value.value.str.start_pos, color_value.value.str.str_len);
		}
		else
			return COLOR_NAMED;
	}
	else if (CSS_is_number(color_value.token) && m_hld_prof && !m_hld_prof->IsInStrictMode())
	{
		color = m_input_buffer->GetColor(color_value.value.number.start_pos, color_value.value.number.str_len);
	}

	if (color == USE_DEFAULT_COLOR)
		return COLOR_INVALID;

	// We found no keyword (whether a named color or a keyword); 'color' is set
	return COLOR_RGBA;
}

#ifdef CSS_GRADIENT_SUPPORT
CSS_Gradient*
CSS_Parser::SetGradientL(int& pos, CSSValueType& type) const
{
	CSS_Gradient* gradient;

	switch (type)
	{
	case CSS_FUNCTION_LINEAR_GRADIENT:
	case CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT:
	case CSS_FUNCTION_O_LINEAR_GRADIENT:
	case CSS_FUNCTION_REPEATING_LINEAR_GRADIENT:
		gradient = SetLinearGradientL(/* inout */ pos,
			type == CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT
			|| type == CSS_FUNCTION_O_LINEAR_GRADIENT
			);
		break;

	case CSS_FUNCTION_RADIAL_GRADIENT:
	case CSS_FUNCTION_REPEATING_RADIAL_GRADIENT:
		gradient = SetRadialGradientL(/* inout */ pos);
		break;

	case CSS_FUNCTION_DOUBLE_RAINBOW:
		gradient = SetDoubleRainbowL(/* inout */ pos);
		type = CSS_FUNCTION_RADIAL_GRADIENT;
		break;

	default:
		OP_ASSERT(!"Invalid gradient type");
		return NULL;
	}

	if (gradient)
	{
		if (type == CSS_FUNCTION_REPEATING_LINEAR_GRADIENT ||
			type == CSS_FUNCTION_REPEATING_RADIAL_GRADIENT)
			gradient->repeat = TRUE;

		if (type == CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT)
			gradient->webkit_prefix = TRUE;
		else if (type == CSS_FUNCTION_O_LINEAR_GRADIENT)
			gradient->o_prefix = TRUE;
	}

	return gradient;
}

CSS_LinearGradient*
CSS_Parser::SetLinearGradientL(int& pos, BOOL legacy_syntax) const
{
	pos++;

	CSS_LinearGradient gradient;
	BOOL expect_comma = FALSE;

	if (m_val_array[pos].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len);

		if (!legacy_syntax)
		{
			if ( keyword == CSS_VALUE_to)
			{
				gradient.line.to = TRUE;

				if (m_val_array[++pos].token == CSS_IDENT)
					keyword = m_input_buffer->GetValueSymbol(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len);
				else
					return NULL;
			}
			else
				goto parse_color_stops;
		}
		else if (legacy_syntax && keyword == CSS_VALUE_to)
			return NULL;

		switch (keyword)
		{
			case CSS_VALUE_top:
			case CSS_VALUE_bottom:
				gradient.line.y = keyword;
				gradient.line.end_set = TRUE;
				expect_comma = TRUE;

				if (m_val_array[++pos].token == CSS_IDENT)
				{
					// We have two identifiers
					CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len);

					switch (keyword)
					{
						case CSS_VALUE_left:
						case CSS_VALUE_right:
							gradient.line.x = keyword;
							break;

						default:
							// But the second keyword does not refer to an edge
							return NULL;
					}
					pos++;
				}
				else
					gradient.line.x = CSS_VALUE_center;
				break;

			case CSS_VALUE_left:
			case CSS_VALUE_right:
				gradient.line.x = keyword;
				gradient.line.end_set = TRUE;
				expect_comma = TRUE;

				if (m_val_array[++pos].token == CSS_IDENT)
				{
					// We have two identifiers
					CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len);

					switch (keyword)
					{
						case CSS_VALUE_top:
						case CSS_VALUE_bottom:
							gradient.line.y = keyword;
							break;

						default:
							// But the second keyword does not refer to an edge
							return NULL;
					}
					pos++;
				}
				else
					gradient.line.y = CSS_VALUE_center;
				break;

			// default: eat nothing, try reparsing as color (below)
		}

	}
	// Angle
	else if (m_val_array[pos].token == CSS_RAD ||
			 m_val_array[pos].token == CSS_NUMBER && m_val_array[pos].value.number.number == 0)
	{
		gradient.line.angle_set = TRUE;
		gradient.line.angle = float(m_val_array[pos].value.number.number);

		pos++;
		expect_comma = TRUE;
	}

parse_color_stops:
	// Use default line ('to bottom') if neither edge nor angle is set.
	if (!gradient.line.end_set && !gradient.line.angle_set)
	{
		gradient.line.end_set = TRUE;
		gradient.line.x = CSS_VALUE_center;
		gradient.line.y = CSS_VALUE_bottom;
		gradient.line.to = TRUE;
	}

	// Keyword(s) or angle must be followed by a comma
	if (expect_comma && m_val_array[pos++].token != CSS_COMMA)
		return NULL;

	if (!SetColorStops(pos, gradient))
		return NULL;

	return gradient.CopyL();
}

CSS_RadialGradient*
CSS_Parser::SetDoubleRainbowL(int& pos) const
{
	pos++;

	CSS_RadialGradient gradient;

	BOOL has_ref;
	// <position> (required)
	if (SetPosition(pos, gradient.pos.pos, gradient.pos.pos_unit, TRUE, gradient.pos.ref, has_ref) == OK)
	{
		gradient.pos.is_set = TRUE;
		gradient.pos.has_ref = !!has_ref;
	}
	else
		return NULL;

	// <length> | <percentage> | <size>
	// defaults
	gradient.form.shape = CSS_VALUE_circle;
	gradient.form.size = CSS_VALUE_farthest_side;

	if (m_val_array[pos].token == CSS_COMMA)
	{
		pos++;
		// <size>
		if (m_val_array[pos].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(
				m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len);
			switch (keyword)
			{
			case CSS_VALUE_contain:
				keyword = CSS_VALUE_closest_side;
				break;
			case CSS_VALUE_cover:
				keyword = CSS_VALUE_farthest_corner;
				break;
			}

			switch (keyword)
			{
			case CSS_VALUE_closest_side:
			case CSS_VALUE_closest_corner:
			case CSS_VALUE_farthest_side:
			case CSS_VALUE_farthest_corner:
				gradient.form.shape = keyword;
				pos++;
				break;

			// default (INVALID): try parsing the rest. If there is an invalid <size> it will quickly be caught when parsing color stops.
			}
		}
		// <length> | <percentage>
		else if ((CSS_is_length_number_ext(m_val_array[pos].token)
			|| m_val_array[pos].token == CSS_PERCENTAGE)
			&& m_val_array[pos].value.number.number > 0)
		{
			gradient.form.has_explicit_size = TRUE;
			gradient.form.explicit_size[0]      = gradient.form.explicit_size[1]      = float(m_val_array[pos].value.number.number);
			gradient.form.explicit_size_unit[0] = gradient.form.explicit_size_unit[1] = static_cast<CSSValueType>(CSS_get_number_ext(m_val_array[pos].token));
			if (m_val_array[pos].token == CSS_PERCENTAGE)
				// Special signal to CSS_RadialGradient::CalculateShape to maintain circle shape
				gradient.form.explicit_size[1] = -1.0;
			pos++;
		}
	}

	// Function must contain no more arguments
	if (m_val_array[pos].token != CSS_FUNCTION_END)
		return NULL;

	// Color stops
	const int stop_count = 19;
	gradient.stop_count = stop_count;

	COLORREF stops[stop_count] =
	{
		static_cast<COLORREF>(CSS_COLOR_transparent),
		// 50% opacity
		0x20d2fafa, // lightgoldenrodyellow 63%
		0x20d30094, // darkviolet
		0x20800000, // navy
		0x20ff0000, // blue
		0x20008000, // green
		0x2000ffff, // yellow
		0x2000a5ff, // orange
		0x200000ff, // red
		static_cast<COLORREF>(CSS_COLOR_transparent), // 67%
		static_cast<COLORREF>(CSS_COLOR_transparent), // 90%
		// 25% opacity
		0x100000ff, // red
		0x1000a5ff, // orange
		0x1000ffff, // yellow
		0x10008000, // green
		0x10ff0000, // blue
		0x10800000, // navy
		0x10d30094, // darkviolet
		static_cast<COLORREF>(CSS_COLOR_transparent) // 100%
	};

	gradient.stops = OP_NEWA_L(CSS_Gradient::ColorStop, gradient.stop_count);

	for (int i = 0; i < gradient.stop_count; i++)
	{
		gradient.stops[i].color = stops[i];
		if (i == 1
			|| i == 9
			|| i == 10
			|| i == 18)
		{
			gradient.stops[i].has_length = TRUE;
			gradient.stops[i].unit = CSS_PERCENTAGE;
			switch (i)
			{
			case 1:
				gradient.stops[i].length = 63;
				break;
			case 9:
				gradient.stops[i].length = 67;
				break;
			case 10:
				gradient.stops[i].length = 90;
				break;
			case 18:
				gradient.stops[i].length = 100;
				break;
			}
		}
	}

	// Epilogue
	return gradient.CopyL();
}

BOOL CSS_Parser::SetRadialGradientPos(int& pos, CSS_RadialGradient& gradient) const
{
	if (m_val_array[pos].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len);
		if (keyword == CSS_VALUE_at)
		{
			pos++;

			BOOL has_ref;

			DeclStatus res = SetPosition(pos, gradient.pos.pos, gradient.pos.pos_unit, TRUE, gradient.pos.ref, has_ref);
			gradient.pos.is_set = TRUE;
			gradient.pos.has_ref = !!has_ref;

			return res == OK;
		}
	}
	return TRUE;
}

CSS_RadialGradient*
CSS_Parser::SetRadialGradientL(int& pos) const
{
	pos++;
	BOOL expect_comma = FALSE;

	CSS_RadialGradient gradient;

	if (m_val_array[pos].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len);
		switch (keyword)
		{
		case CSS_VALUE_at:
			expect_comma = TRUE;

			if (!SetRadialGradientPos(pos, gradient))
				return NULL;
			break;

		case CSS_VALUE_circle:
		case CSS_VALUE_ellipse:
			expect_comma = TRUE;
			pos++;

			gradient.form.shape = keyword;

			if (m_val_array[pos].token == CSS_IDENT)
			{
				CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len);
				if (keyword == CSS_VALUE_closest_side ||
				    keyword == CSS_VALUE_closest_corner ||
				    keyword == CSS_VALUE_farthest_side ||
				    keyword == CSS_VALUE_farthest_corner)
				{
					pos++;
					gradient.form.size = keyword;
				}
			}
			// ellipse and circle can be followed by a length or 0, and ellipse
			// (but not circle) can also be followed by a percentage
			else if ((CSS_is_length_number_ext(m_val_array[pos].token) ||
			          (m_val_array[pos].token == CSS_PERCENTAGE &&
			           gradient.form.shape == CSS_VALUE_ellipse)) &&
			         m_val_array[pos].value.number.number >= 0 ||
			         (m_val_array[pos].token == CSS_NUMBER && m_val_array[pos].value.number.number == 0))
			{
				gradient.form.has_explicit_size = TRUE;
				gradient.form.explicit_size[0] = float(m_val_array[pos].value.number.number);
				gradient.form.explicit_size_unit[0] = static_cast<CSSValueType>(CSS_get_number_ext(m_val_array[pos].token));

				pos++;
				// an ellipse must have a second length percentage or 0
				if (gradient.form.shape == CSS_VALUE_ellipse)
				{
					if ((CSS_is_length_number_ext(m_val_array[pos].token) || m_val_array[pos].token == CSS_PERCENTAGE) &&
					    m_val_array[pos].value.number.number >= 0 ||
						(m_val_array[pos].token == CSS_NUMBER && m_val_array[pos].value.number.number == 0))
					{
						gradient.form.explicit_size[1] = float(m_val_array[pos].value.number.number);
						gradient.form.explicit_size_unit[1] = static_cast<CSSValueType>(CSS_get_number_ext(m_val_array[pos].token));

						pos++;
					}
					else
						return NULL;
				}
				else
					// Special signal to CSS_RadialGradient::CalculateShape to maintain circle shape
					gradient.form.explicit_size[1] = -1.0;
			}

			if (!SetRadialGradientPos(pos, gradient))
				return NULL;
			break;

		case CSS_VALUE_closest_side:
		case CSS_VALUE_closest_corner:
		case CSS_VALUE_farthest_side:
		case CSS_VALUE_farthest_corner:
			expect_comma = TRUE;
			pos++;

			gradient.form.size = keyword;

			if (m_val_array[pos].token == CSS_IDENT)
			{
				CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len);
				if (keyword == CSS_VALUE_circle ||
				    keyword == CSS_VALUE_ellipse)
				{
					pos++;
					gradient.form.shape = keyword;
				}

				if (!SetRadialGradientPos(pos, gradient))
					return NULL;
			}
			break;
		}
	}
	// Explicit size begins with a length or a percentage or 0
	else if ((CSS_is_length_number_ext(m_val_array[pos].token) || m_val_array[pos].token == CSS_PERCENTAGE)
			&& m_val_array[pos].value.number.number >= 0
			|| (m_val_array[pos].token == CSS_NUMBER && m_val_array[pos].value.number.number == 0)
			)
	{
		expect_comma = TRUE;

		gradient.form.has_explicit_size = TRUE;
		gradient.form.explicit_size[0] = float(m_val_array[pos].value.number.number);
		gradient.form.explicit_size_unit[0] = static_cast<CSSValueType>(CSS_get_number_ext(m_val_array[pos].token));

		pos++;

		// Another length or percentage or 0 means an ellipse
		if ((CSS_is_length_number_ext(m_val_array[pos].token) || m_val_array[pos].token == CSS_PERCENTAGE)
				&& m_val_array[pos].value.number.number >= 0
				|| (m_val_array[pos].token == CSS_NUMBER && m_val_array[pos].value.number.number == 0)
		   )
		{
			gradient.form.explicit_size[1] = float(m_val_array[pos].value.number.number);
			gradient.form.explicit_size_unit[1] = static_cast<CSSValueType>(CSS_get_number_ext(m_val_array[pos].token));

			pos++;

			// The shape is optional, but if it is mentioned it must be ellipse
			if (m_val_array[pos].token == CSS_IDENT &&
			    m_input_buffer->GetValueSymbol(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len) == CSS_VALUE_ellipse)
				pos++;
		}
		// A single non percentage length is allowed, and means circle
		else if (gradient.form.explicit_size_unit[0] != CSS_PERCENTAGE)
		{
			// Special signal to CSS_RadialGradient::CalculateShape to maintain circle shape
			gradient.form.explicit_size[1] = -1.0;

			// The shape is optional, but if it is mentioned it must be circle
			if (m_val_array[pos].token == CSS_IDENT &&
			    m_input_buffer->GetValueSymbol(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len) == CSS_VALUE_circle)
				pos++;
		}
		else
			return NULL;

		if (!SetRadialGradientPos(pos, gradient))
			return NULL;
	}

	if (expect_comma && m_val_array[pos++].token != CSS_COMMA)
		return NULL;

	if (!SetColorStops(pos, gradient))
		return NULL;

	return gradient.CopyL();
}

BOOL
CSS_Parser::SetColorStops(int& pos, CSS_Gradient& gradient) const
{
	// Find the end of the gradient function and count color stops
	int stop_count = 0;
	for (int i = pos; i != CSS_FUNCTION_END; i++)
	{
		OP_ASSERT(i < m_val_array.GetCount()); // The parser should have inserted CSS_FUNCTION_END at the end.
		if (m_val_array[i].token == CSS_COMMA || m_val_array[i].token == CSS_FUNCTION_END)
		{
			stop_count++;
			if (m_val_array[i].token == CSS_FUNCTION_END)
				break;
		}
	}

	if (stop_count < 2)
		return FALSE;

	gradient.stop_count = stop_count;
	gradient.stops = OP_NEWA_L(CSS_Gradient::ColorStop, stop_count);

	for (int i = 0; i < stop_count; i++)
	{
		OP_ASSERT(pos < m_val_array.GetCount());
		if (m_val_array[pos].token == CSS_FUNCTION_END)
			return FALSE;

		// Color
		COLORREF color;
		CSSValue keyword;
		uni_char* dummy = NULL; // Skin colors are not supported.
		switch (SetColorL(color, keyword, dummy, m_val_array[pos]))
		{
		case COLOR_KEYWORD:
			if (keyword == CSS_VALUE_transparent)
				gradient.stops[i].color = COLORREF(CSS_COLOR_transparent);
			else if (keyword == CSS_VALUE_currentColor)
				gradient.stops[i].color = COLORREF(CSS_COLOR_current_color);
			else
				return FALSE;
			break;
		case COLOR_RGBA:
		case COLOR_NAMED:
			gradient.stops[i].color = color;
			break;
		case COLOR_SKIN:
		case COLOR_INVALID:
			return FALSE;
		}
		pos++;

		if (m_val_array[pos].token == CSS_COMMA)
			// Comma
			pos++;
		else if (m_val_array[pos].token == CSS_FUNCTION_END)
			// End
			;
		else if (CSS_is_length_number_ext(m_val_array[pos].token) ||
				 m_val_array[pos].token == CSS_PERCENTAGE ||
				 m_val_array[pos].token == CSS_NUMBER && m_val_array[pos].value.number.number == 0)
		{
			// Length
			gradient.stops[i].has_length = TRUE;
			gradient.stops[i].length = float(m_val_array[pos].value.number.number);
			gradient.stops[i].unit = CSS_get_number_ext(m_val_array[pos].token);
			pos++;
			if (m_val_array[pos].token == CSS_COMMA)
				pos++;
			// else: End
		}
		else
			// Invalid
			return FALSE;
	}

	// End up at the end of this item so caller can increment pos.
	if (m_val_array[pos].token != CSS_FUNCTION_END)
		return FALSE;

	return TRUE;
}
#endif // CSS_GRADIENT_SUPPORT

CSS_Parser::DeclStatus
CSS_Parser::AddFontfaceDeclL(short prop)
{
	CSS_property_list* prop_list = m_current_props;
	int i = 0;

	switch (prop)
	{
	case CSS_PROPERTY_font_family:
		{
			if (m_val_array.GetCount() > 0)
			{
				CSS_generic_value value;
				if (SetFamilyNameL(value, i, FALSE, FALSE) == OK)
				{
					uni_char* family_name = value.GetString();
					OP_ASSERT(value.GetValueType() == CSS_STRING_LITERAL && family_name);
					if (i == m_val_array.GetCount())
						prop_list->AddDeclL(CSS_PROPERTY_font_family, family_name, FALSE, GetCurrentOrigin(), CSS_string_decl::StringDeclString, m_hld_prof == NULL);
					else
					{
						OP_DELETEA(family_name);
						return INVALID;
					}
				}
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		break;

	case CSS_PROPERTY_font_style:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (!CSS_is_font_style_val(keyword) && keyword != CSS_VALUE_inherit)
				return INVALID;
			prop_list->AddTypeDeclL(prop, keyword, FALSE, GetCurrentOrigin());
		}
		break;

	case CSS_PROPERTY_font_weight:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				short fweight = 0;
				CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				fweight = ResolveFontWeight(keyword);
				if (fweight > 0)
					prop_list->AddDeclL(prop, fweight, CSS_NUMBER, FALSE, GetCurrentOrigin());
				else if (fweight == 0)
					prop_list->AddTypeDeclL(prop, keyword, FALSE, GetCurrentOrigin());
				else
					return INVALID;
			}
			else if (CSS_is_number(m_val_array[0].token) && CSS_get_number_ext(m_val_array[0].token) == CSS_NUMBER)
			{
				double fval = m_val_array[0].value.number.number;
				if (fval >= 100 && fval <= 900)
				{
					int fweight = (int)fval;
					if ((fweight/100)*100 == fval)
						prop_list->AddDeclL(prop, (short) (fweight/100), CSS_NUMBER, FALSE, GetCurrentOrigin());
					else
						return INVALID;
				}
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_src:
		{
			CSS_generic_value_list gen_arr;
			ANCHOR(CSS_generic_value_list, gen_arr);
			CSS_anchored_heap_arrays<uni_char> strings;
			ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);

			while (i < m_val_array.GetCount())
			{
				if (m_val_array[i].token == CSS_FUNCTION_URL)
				{
					int url_pos = i++;
					CSS_WebFont::Format format = CSS_WebFont::FORMAT_UNKNOWN;
					if (i < m_val_array.GetCount() && m_val_array[i].token == CSS_FUNCTION_FORMAT)
						format = static_cast<CSS_WebFont::Format>(m_val_array[i++].value.shortint);

					if (format != CSS_WebFont::FORMAT_UNSUPPORTED)
					{
						if (!gen_arr.Empty())
							gen_arr.PushValueTypeL(CSS_COMMA);

						URL url = m_input_buffer->GetURLL(m_base_url, m_val_array[url_pos].value.str.start_pos, m_val_array[url_pos].value.str.str_len);
						ANCHOR(URL, url);
						const uni_char* url_name = url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI).CStr();
						uni_char* url_str = OP_NEWA_L(uni_char, uni_strlen(url_name)+1);
						strings.AddArrayL(url_str);
						uni_strcpy(url_str, url_name);
						gen_arr.PushStringL(CSS_FUNCTION_URL, url_str);
						if (format != CSS_WebFont::FORMAT_UNKNOWN)
							gen_arr.PushIntegerL(CSS_FUNCTION_FORMAT, format);
					}
				}
				else if (m_val_array[i++].token == CSS_FUNCTION_LOCAL_START)
				{
					CSS_generic_value value;
					if (i < m_val_array.GetCount() && SetFamilyNameL(value, i, FALSE, FALSE) == OK)
					{
						OP_ASSERT(value.GetValueType() == CSS_STRING_LITERAL);
						strings.AddArrayL(value.GetString());

						if (i < m_val_array.GetCount() && m_val_array[i].token == CSS_FUNCTION_END)
						{
							i++;
							if (!gen_arr.Empty())
								gen_arr.PushValueTypeL(CSS_COMMA);
							gen_arr.PushStringL(CSS_FUNCTION_LOCAL, value.GetString());
						}
						else
							return INVALID;
					}
					else
						return INVALID;
				}
				else
					return INVALID;

				/* We're either finished, or there has to be a comma followed by
				   something that's presumably the next source. */
				if (i < m_val_array.GetCount() && (m_val_array[i++].token != CSS_COMMA || i == m_val_array.GetCount()))
					return INVALID;
			}

			if (!gen_arr.Empty())
				prop_list->AddDeclL(prop, gen_arr, 0, FALSE, GetCurrentOrigin());
		}
		break;

	default:
		return INVALID;
	}

	return OK;
}

CSS_Parser::DeclStatus
CSS_Parser::AddViewportDeclL(short prop, BOOL important)
{
	CSS_property_list* prop_list = m_current_props;
	int val_count = m_val_array.GetCount();

	switch (prop)
	{
	case CSS_PROPERTY_height:
	case CSS_PROPERTY_width:
		if (val_count == 1 || val_count == 2)
		{
			short shorthand_props[2]; /* ARRAY OK 2010-12-10 rune */
			CSSValue shorthand_keywords[2] = { CSS_VALUE_UNSPECIFIED, CSS_VALUE_UNSPECIFIED }; /* ARRAY OK 2010-12-10 rune */
			short shorthand_units[2] = { CSS_NUMBER, CSS_NUMBER }; /* ARRAY OK 2010-12-10 rune */
			float shorthand_numbers[2] = { 0, 0 }; /* ARRAY OK 2010-12-10 rune */

			if (prop == CSS_PROPERTY_width)
			{
				shorthand_props[0] = CSS_PROPERTY_min_width;
				shorthand_props[1] = CSS_PROPERTY_max_width;
			}
			else
			{
				shorthand_props[0] = CSS_PROPERTY_min_height;
				shorthand_props[1] = CSS_PROPERTY_max_height;
			}

			int j;

			for (j = 0; j < 2; j++)
			{
				int i = val_count == 2 ? j : 0;

				if (m_val_array[i].token == CSS_IDENT)
				{
					shorthand_keywords[j] = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (!CSS_is_viewport_length_val(shorthand_keywords[j]))
						return INVALID;
				}
				else if ((CSS_is_length_number_ext(m_val_array[i].token) || m_val_array[i].token == CSS_PERCENTAGE)
						 && m_val_array[i].value.number.number > 0)
				{
					shorthand_numbers[j] = float(m_val_array[i].value.number.number);
					shorthand_units[j] = m_val_array[i].token;
				}
				else
					return INVALID;
			}

			for (j = 0; j < 2; j++)
			{
				if (shorthand_keywords[j] != CSS_VALUE_UNSPECIFIED)
					prop_list->AddTypeDeclL(shorthand_props[j], shorthand_keywords[j], important, GetCurrentOrigin());
				else
					prop_list->AddDeclL(shorthand_props[j], shorthand_numbers[j], shorthand_units[j], important, GetCurrentOrigin());
			}
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_max_width:
	case CSS_PROPERTY_min_width:
	case CSS_PROPERTY_max_height:
	case CSS_PROPERTY_min_height:
		if (val_count == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (CSS_is_viewport_length_val(keyword))
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else if ((CSS_is_length_number_ext(m_val_array[0].token) || m_val_array[0].token == CSS_PERCENTAGE)
					 && m_val_array[0].value.number.number > 0)
			{
				short length_type = m_val_array[0].token;
				prop_list->AddDeclL(prop, float(m_val_array[0].value.number.number), length_type, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_zoom:
	case CSS_PROPERTY_min_zoom:
	case CSS_PROPERTY_max_zoom:
		if (m_val_array.GetCount() == 1)
		{
			if ((m_val_array[0].token == CSS_NUMBER || m_val_array[0].token == CSS_PERCENTAGE) && m_val_array[0].value.number.number > 0)
				prop_list->AddDeclL(prop, float(m_val_array[0].value.number.number), m_val_array[0].token, important, GetCurrentOrigin());
			else if (m_val_array[0].token == CSS_IDENT && m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len) == CSS_VALUE_auto)
				prop_list->AddTypeDeclL(prop, CSS_VALUE_auto, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_user_zoom:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (CSS_is_user_zoom_val(keyword))
				prop_list->AddTypeDeclL(CSS_PROPERTY_user_zoom, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_orientation:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_auto || keyword == CSS_VALUE_portrait || keyword == CSS_VALUE_landscape)
				prop_list->AddTypeDeclL(CSS_PROPERTY_orientation, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	default:
		return INVALID;
	}

	return OK;
}

#ifdef CSS_ANIMATIONS
void
CSS_Parser::BeginKeyframesL(uni_char* name)
{
	if (m_replace_rule)
	{
		OP_ASSERT(m_current_keyframes_rule);
		m_current_keyframes_rule->SetName(name);
		m_current_keyframes_rule->ClearRules();
	}
	else
	{
		OP_ASSERT(m_decl_type == DECL_RULESET);
		m_current_keyframes_rule = OP_NEW_L(CSS_KeyframesRule, (name));
		AddRuleL(m_current_keyframes_rule);

		m_current_keyframe_selectors.Clear();
	}

	BeginKeyframes();
}

void
CSS_Parser::AddKeyframeRuleL()
{
	OP_ASSERT(m_current_keyframes_rule);

	if (m_current_props)
		m_current_props->PostProcess(TRUE, FALSE);

	if (m_replace_rule && m_dom_rule->GetType() == CSS_Rule::KEYFRAME)
	{
		CSS_KeyframeRule* keyframe_rule = static_cast<CSS_KeyframeRule*>(m_dom_rule);

		keyframe_rule->Replace(m_current_keyframe_selectors, m_current_props);
	}
	else
	{
		CSS_KeyframeRule* keyframe_rule = OP_NEW_L(CSS_KeyframeRule, (m_current_keyframes_rule, m_current_keyframe_selectors, m_current_props));
		m_current_keyframes_rule->Add(keyframe_rule);
	}
}

#endif // CSS_ANIMATIONS

CSS_Parser::DeclStatus
CSS_Parser::AddPageDeclL(short prop, BOOL important)
{
	switch (prop)
	{
	case CSS_PROPERTY_size:
	case CSS_PROPERTY_margin:
	case CSS_PROPERTY_margin_top:
	case CSS_PROPERTY_margin_left:
	case CSS_PROPERTY_margin_right:
	case CSS_PROPERTY_margin_bottom:
	case CSS_PROPERTY_width:
	case CSS_PROPERTY_height:
	case CSS_PROPERTY_max_width:
	case CSS_PROPERTY_max_height:
	case CSS_PROPERTY_min_width:
	case CSS_PROPERTY_min_height:
		return AddRulesetDeclL(prop, important);
	default:
		return INVALID;
	}
}

CSS_Parser::DeclStatus
CSS_Parser::AddRulesetDeclL(short prop, BOOL important)
{
	CSS_property_list* prop_list = m_current_props;
	int i = 0;
	int level = 0;
	CSSValue keyword = CSS_VALUE_UNKNOWN;

#ifdef CSS_GRADIENT_SUPPORT
	CSS_anchored_heap_objects<CSS_Gradient> gradients;
	ANCHOR(CSS_anchored_heap_objects<CSS_Gradient>, gradients);
#endif // CSS_GRADIENT_SUPPORT

	BOOL auto_allowed = FALSE;

	prop = GetAliasedProperty(prop);
	switch (prop)
	{
	case CSS_PROPERTY_top:
	case CSS_PROPERTY_left:
	case CSS_PROPERTY_right:
	case CSS_PROPERTY_bottom:
	case CSS_PROPERTY_width:
	case CSS_PROPERTY_height:
	case CSS_PROPERTY_margin_top:
	case CSS_PROPERTY_margin_left:
	case CSS_PROPERTY_margin_right:
	case CSS_PROPERTY_margin_bottom:
	case CSS_PROPERTY_column_width:
	case CSS_PROPERTY_column_gap:
	case CSS_PROPERTY_min_width:
	case CSS_PROPERTY_min_height:
		auto_allowed = TRUE;
		// fall-through is intended
	case CSS_PROPERTY_max_width:
	case CSS_PROPERTY_max_height:
	case CSS_PROPERTY_padding_top:
	case CSS_PROPERTY_padding_left:
	case CSS_PROPERTY_padding_right:
	case CSS_PROPERTY_padding_bottom:
	case CSS_PROPERTY_line_height:
	case CSS_PROPERTY_font_size:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword < 0)
					return INVALID;

				if (keyword == CSS_VALUE_inherit || (keyword == CSS_VALUE_auto && auto_allowed) ||
					(prop == CSS_PROPERTY_font_size && CSS_is_fontsize_val(keyword)) ||
					(keyword == CSS_VALUE_none && (prop == CSS_PROPERTY_max_width || prop == CSS_PROPERTY_max_height)) ||
					(prop == CSS_PROPERTY_column_gap && keyword == CSS_VALUE_normal))
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else if (keyword == CSS_VALUE_normal && prop == CSS_PROPERTY_line_height)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else if ((prop == CSS_PROPERTY_height || prop == CSS_PROPERTY_width) && (keyword == CSS_VALUE__o_skin || keyword == CSS_VALUE__o_content_size))
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
					if (keyword == CSS_VALUE__o_skin || keyword == CSS_VALUE__o_content_size)
						prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				}
				else
					return INVALID;
			}
			else if (CSS_is_number(m_val_array[0].token) && m_val_array[0].token != CSS_DIMEN)
			{
				float length = float(m_val_array[0].value.number.number);
				int length_type = CSS_get_number_ext(m_val_array[0].token);
				if (length <= 0)
				{
					switch (prop)
					{
					case CSS_PROPERTY_font_size:
					case CSS_PROPERTY_line_height:
					case CSS_PROPERTY_padding_top:
					case CSS_PROPERTY_padding_left:
					case CSS_PROPERTY_padding_right:
					case CSS_PROPERTY_padding_bottom:
					case CSS_PROPERTY_width:
					case CSS_PROPERTY_height:
					case CSS_PROPERTY_min_width:
					case CSS_PROPERTY_max_width:
					case CSS_PROPERTY_max_height:
					case CSS_PROPERTY_min_height:
					case CSS_PROPERTY_column_gap:
						if (length < 0)
							return INVALID;
						break;
					case CSS_PROPERTY_column_width:
						return INVALID;
					default:
						break;
					}
				}
				if (length_type == CSS_PERCENTAGE && (prop == CSS_PROPERTY_column_width || prop == CSS_PROPERTY_column_gap))
					return INVALID;
				if (prop == CSS_PROPERTY_line_height && length_type != CSS_PERCENTAGE && length_type != CSS_NUMBER && !CSS_is_length_number_ext(length_type))
					return INVALID;
				else if (prop != CSS_PROPERTY_line_height && length_type != CSS_PERCENTAGE && !CSS_is_length_number_ext(length_type))
				{
					if (length_type == CSS_NUMBER && (length == 0 || (m_hld_prof && !m_hld_prof->IsInStrictMode())))
						length_type = CSS_PX;
					else
						return INVALID;
				}
				prop_list->AddDeclL(prop, length, length_type, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_border_top_right_radius:
	case CSS_PROPERTY_border_bottom_right_radius:
	case CSS_PROPERTY_border_bottom_left_radius:
	case CSS_PROPERTY_border_top_left_radius:
		return SetIndividualBorderRadiusL(prop_list, prop, important);

	case CSS_PROPERTY_border_radius:
		return SetShorthandBorderRadiusL(prop_list, important);

	case CSS_PROPERTY_border_style:
	case CSS_PROPERTY_border_width:
	case CSS_PROPERTY_padding:
	case CSS_PROPERTY_margin:
	case CSS_PROPERTY_border_color:
		if (m_val_array.GetCount() <= 4)
		{
			BOOL is_keyword[4] = { FALSE, FALSE, FALSE, FALSE };
			BOOL is_color[4] = { FALSE, FALSE, FALSE, FALSE };
			COLORREF value_col[4] = { USE_DEFAULT_COLOR, USE_DEFAULT_COLOR, USE_DEFAULT_COLOR, USE_DEFAULT_COLOR };
			float value[4];
			int value_type[4];
			CSSValue value_keyword[4];
			short property[4];
			if (prop == CSS_PROPERTY_margin)
			{
				property[0] = CSS_PROPERTY_margin_top;
				property[1] = CSS_PROPERTY_margin_right;
				property[2] = CSS_PROPERTY_margin_bottom;
				property[3] = CSS_PROPERTY_margin_left;
			}
			else if (prop == CSS_PROPERTY_padding)
			{
				property[0] = CSS_PROPERTY_padding_top;
				property[1] = CSS_PROPERTY_padding_right;
				property[2] = CSS_PROPERTY_padding_bottom;
				property[3] = CSS_PROPERTY_padding_left;
			}
			else if (prop == CSS_PROPERTY_border_width)
			{
				property[0] = CSS_PROPERTY_border_top_width;
				property[1] = CSS_PROPERTY_border_right_width;
				property[2] = CSS_PROPERTY_border_bottom_width;
				property[3] = CSS_PROPERTY_border_left_width;
			}
			else if (prop == CSS_PROPERTY_border_color)
			{
				property[0] = CSS_PROPERTY_border_top_color;
				property[1] = CSS_PROPERTY_border_right_color;
				property[2] = CSS_PROPERTY_border_bottom_color;
				property[3] = CSS_PROPERTY_border_left_color;
			}
			else
			{
				property[0] = CSS_PROPERTY_border_top_style;
				property[1] = CSS_PROPERTY_border_right_style;
				property[2] = CSS_PROPERTY_border_bottom_style;
				property[3] = CSS_PROPERTY_border_left_style;
			}

			while (i < m_val_array.GetCount())
			{
				if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (prop == CSS_PROPERTY_border_color && (keyword >= 0 && CSS_is_color_val(keyword) || m_input_buffer->GetNamedColorIndex(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len) != USE_DEFAULT_COLOR))
					{
						is_color[i] = TRUE;
						is_keyword[i] = TRUE;

						if (CSS_is_ui_color_val(keyword))
							value_col[i] = CSS_COLOR_KEYWORD_TYPE_ui_color | keyword;
						else
							value_col[i] = m_input_buffer->GetNamedColorIndex(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					}
					else if (	(m_val_array.GetCount() == 1 && keyword == CSS_VALUE_inherit)
							||	(prop == CSS_PROPERTY_border_color &&
										(keyword == CSS_VALUE_transparent ||
											keyword == CSS_VALUE_currentColor))
							||	(keyword == CSS_VALUE_auto && prop == CSS_PROPERTY_margin)
							)
					{
						value_keyword[i] = keyword;
						is_keyword[i] = TRUE;
					}
					else if (prop == CSS_PROPERTY_border_style && (CSS_is_border_style_val(keyword) || keyword == CSS_VALUE_none))
					{
						value_keyword[i] = keyword;
						is_keyword[i] = TRUE;
					}
					else if (prop == CSS_PROPERTY_border_width && CSS_is_border_width_val(keyword))
					{
						value_keyword[i] = keyword;
						is_keyword[i] = TRUE;
					}
					else if (prop == CSS_PROPERTY_border_color && m_hld_prof && !m_hld_prof->IsInStrictMode())
					{
						value_col[i] = m_input_buffer->GetColor(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
						if (value_col[i] == USE_DEFAULT_COLOR)
							return INVALID;
						is_color[i] = TRUE;
					}
					else
						return INVALID;
				}
				else if (prop == CSS_PROPERTY_border_color)
				{
					is_color[i] = TRUE;
					if (m_val_array[i].token == CSS_FUNCTION_RGB || SupportsAlpha() && m_val_array[i].token == CSS_FUNCTION_RGBA)
						value_col[i] = m_val_array[i].value.color;
					else if (m_val_array[i].token == CSS_HASH)
						value_col[i] = m_input_buffer->GetColor(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					else if (CSS_is_number(m_val_array[i].token) && m_hld_prof && !m_hld_prof->IsInStrictMode())
					{
						if (m_val_array[i].value.number.str_len == 3 || m_val_array[i].value.number.str_len == 6)
							value_col[i] = m_input_buffer->GetColor(m_val_array[i].value.number.start_pos, m_val_array[i].value.number.str_len);
					}

					if (value_col[i] == USE_DEFAULT_COLOR)
						return INVALID;
				}
				else if (CSS_is_number(m_val_array[i].token) && prop != CSS_PROPERTY_border_style)
				{
					if (m_val_array[i].token == CSS_PERCENTAGE && prop == CSS_PROPERTY_border_width)
						return INVALID;

					value[i] = float(m_val_array[i].value.number.number);
					value_type[i] = CSS_get_number_ext(m_val_array[i].token);

					if (value[i] < 0)
					{
						switch (prop)
						{
						case CSS_PROPERTY_border_width:
						case CSS_PROPERTY_padding:
							return INVALID;
						default:
							break;
						}
					}

					//units are required if not 0
					if (value_type[i] == CSS_NUMBER)
					{
						// Accept unspecified unit for borders since i have seen a lot of pages which get the default
						// borderwidth (3) instead of the given value. (emil@opera.com)
						// Added margin and padding. (rune@opera.com)
						// Added check for strict mode.
						if (value[i] == 0 || (m_hld_prof && !m_hld_prof->IsInStrictMode() && (prop == CSS_PROPERTY_border_width || prop == CSS_PROPERTY_margin || prop == CSS_PROPERTY_padding)))
							value_type[i] = CSS_PX;
						else
							return INVALID;
					}

					if (value_type[i] == CSS_DIMEN)
					{
						switch (prop)
						{
							case CSS_PROPERTY_border_width:
							case CSS_PROPERTY_padding:
							case CSS_PROPERTY_margin:
								return INVALID; // we encountered a unit that we didn't recognize, for example "border-width: 50zu;"
							default:
								break;
						}
					}
				}
				else
					return INVALID;

				i++;
			}

			int right_idx = 0;
			int bottom_idx = 0;
			int left_idx = 0;
			if (m_val_array.GetCount() == 2)
			{
				left_idx = right_idx = 1;
			}
			else if (m_val_array.GetCount() == 3)
			{
				left_idx = right_idx = 1;
				bottom_idx = 2;
			}
			else if (m_val_array.GetCount() == 4)
			{
				right_idx = 1;
				bottom_idx = 2;
				left_idx = 3;
			}

			if (is_color[0])
			{
				if (is_keyword[0])
					prop_list->AddColorDeclL(property[0], value_col[0], important, GetCurrentOrigin());
				else
					prop_list->AddLongDeclL(property[0], (long)value_col[0], important, GetCurrentOrigin());
			}
			else if (is_keyword[0])
			{
				prop_list->AddTypeDeclL(property[0], value_keyword[0], important, GetCurrentOrigin());
			}
			else
			{
				prop_list->AddDeclL(property[0], value[0], value_type[0], important, GetCurrentOrigin());
			}

			if (is_color[right_idx])
			{
				if (is_keyword[right_idx])
					prop_list->AddColorDeclL(property[1], value_col[right_idx], important, GetCurrentOrigin());
				else
					prop_list->AddLongDeclL(property[1], (long)value_col[right_idx], important, GetCurrentOrigin());
			}
			else if (is_keyword[right_idx])
			{
				prop_list->AddTypeDeclL(property[1], value_keyword[right_idx], important, GetCurrentOrigin());
			}
			else
			{
				prop_list->AddDeclL(property[1], value[right_idx], value_type[right_idx], important, GetCurrentOrigin());
			}

			if (is_color[bottom_idx])
			{
				if (is_keyword[bottom_idx])
					prop_list->AddColorDeclL(property[2], value_col[bottom_idx], important, GetCurrentOrigin());
				else
					prop_list->AddLongDeclL(property[2], (long)value_col[bottom_idx], important, GetCurrentOrigin());
			}
			else if (is_keyword[bottom_idx])
			{
				prop_list->AddTypeDeclL(property[2], value_keyword[bottom_idx], important, GetCurrentOrigin());
			}
			else
			{
				prop_list->AddDeclL(property[2], value[bottom_idx], value_type[bottom_idx], important, GetCurrentOrigin());
			}

			if (is_color[left_idx])
			{
				if (is_keyword[left_idx])
					prop_list->AddColorDeclL(property[3], value_col[left_idx], important, GetCurrentOrigin());
				else
					prop_list->AddLongDeclL(property[3], (long)value_col[left_idx], important, GetCurrentOrigin());
			}
			else if (is_keyword[left_idx])
			{
				prop_list->AddTypeDeclL(property[3], value_keyword[left_idx], important, GetCurrentOrigin());
			}
			else
			{
				prop_list->AddDeclL(property[3], value[left_idx], value_type[left_idx], important, GetCurrentOrigin());
			}
			return OK;
		}
		break;

	case CSS_PROPERTY_font_family:
		return SetFontFamilyL(prop_list, 0, important, m_val_array.GetCount() == 1);

	case CSS_PROPERTY_font_size_adjust:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword != CSS_VALUE_none && keyword != CSS_VALUE_inherit)
					return INVALID;
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			}
			else if (CSS_is_number(m_val_array[0].token))
				prop_list->AddDeclL(prop, float(m_val_array[0].value.number.number), CSS_get_number_ext(m_val_array[0].token), important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_font_style:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (!CSS_is_font_style_val(keyword) && keyword != CSS_VALUE_inherit)
				return INVALID;
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_font_variant:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword != CSS_VALUE_normal && keyword != CSS_VALUE_small_caps && keyword != CSS_VALUE_inherit)
				return INVALID;
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_font_weight:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				short fweight = 0;
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				fweight = ResolveFontWeight(keyword);
				if (fweight > 0)
					prop_list->AddDeclL(prop, fweight, CSS_NUMBER, important, GetCurrentOrigin());
				else if (fweight == 0)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else if (CSS_is_number(m_val_array[0].token) && CSS_get_number_ext(m_val_array[0].token) == CSS_NUMBER)
			{
				double fval = m_val_array[0].value.number.number;
				if (fval >= 100 && fval <= 900)
				{
					int fweight = (int)fval;
					if ((fweight/100)*100 == fval)
						prop_list->AddDeclL(prop, (short) (fweight/100), CSS_NUMBER, important, GetCurrentOrigin());
					else
						return INVALID;
				}
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_font:
		{
			CSS_Parser::DeclStatus font_family = INVALID;
			CSSValue font_style = CSS_VALUE_UNSPECIFIED;
			short font_weight = 0;
			CSSValue font_variant = CSS_VALUE_UNSPECIFIED;
			CSSValue font_size = CSS_VALUE_UNSPECIFIED;
			float font_size_val = -1;
			short font_size_val_type = 0;
			CSSValue line_height_keyword = CSS_VALUE_UNSPECIFIED;
			float line_height = -1;
			short line_height_type = 0;
			BOOL next_is_line_height = FALSE;
			while (i < m_val_array.GetCount())
			{
				if (m_val_array[i].token == CSS_IDENT && !next_is_line_height)
				{
					BOOL value_used = FALSE;

					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (keyword >= 0)
					{
						value_used = TRUE;

						if (m_val_array.GetCount() == 1 && (CSS_is_font_system_val(keyword) || keyword == CSS_VALUE_inherit))
						{
							if (keyword == CSS_VALUE_Menu)
								keyword = CSS_VALUE_menu;

							prop_list->AddTypeDeclL(CSS_PROPERTY_font_style, keyword, important, GetCurrentOrigin());
							prop_list->AddTypeDeclL(CSS_PROPERTY_font_variant, keyword, important, GetCurrentOrigin());
							prop_list->AddTypeDeclL(CSS_PROPERTY_font_weight, keyword, important, GetCurrentOrigin());
							prop_list->AddTypeDeclL(CSS_PROPERTY_font_size, keyword, important, GetCurrentOrigin());
							prop_list->AddTypeDeclL(CSS_PROPERTY_line_height, keyword, important, GetCurrentOrigin());
							prop_list->AddTypeDeclL(CSS_PROPERTY_font_family, keyword, important, GetCurrentOrigin());
							return OK;
						}

						if (font_size || font_size_val >= 0)
						{
							value_used = FALSE; // let the font-family code below handle the value (only font-family is valid after font-size)
						}
						else if (!font_style && (keyword == CSS_VALUE_normal || keyword == CSS_VALUE_italic || keyword == CSS_VALUE_oblique || keyword == CSS_VALUE_inherit))
						{
							font_style = keyword;
						}
						else if (font_style == CSS_VALUE_normal && (!font_variant || !font_weight) && (keyword == CSS_VALUE_italic || keyword == CSS_VALUE_oblique))
						{
							if (!font_variant)
								font_variant = CSS_VALUE_normal;
							else
								font_weight = CSS_VALUE_normal;
							font_style = keyword;
						}
						else if (!font_variant && (keyword == CSS_VALUE_normal || keyword == CSS_VALUE_small_caps || keyword == CSS_VALUE_inherit))
						{
							font_variant = keyword;
						}
						else if (font_variant == CSS_VALUE_normal && (!font_style || !font_weight) && keyword == CSS_VALUE_small_caps)
						{
							if (!font_style)
								font_style = CSS_VALUE_normal;
							else
								font_weight = CSS_VALUE_normal;
							font_variant = keyword;
						}
						else if (!font_weight && (keyword == CSS_VALUE_normal || keyword == CSS_VALUE_bold || keyword == CSS_VALUE_bolder || keyword == CSS_VALUE_lighter || keyword == CSS_VALUE_inherit))
						{
							/* Relevant keywords have values greater than 9, the largest legal
							   numeric value. */

							font_weight = keyword;
							if (keyword == CSS_VALUE_normal)
								font_weight = 4;
							else if (keyword == CSS_VALUE_bold)
								font_weight = 7;
						}
						else if (font_weight == CSS_VALUE_normal && (!font_style || !font_variant) && (keyword == CSS_VALUE_bold || keyword == CSS_VALUE_bolder || keyword == CSS_VALUE_lighter))
						{
							if (!font_style)
								font_style = CSS_VALUE_normal;
							else
								font_variant = CSS_VALUE_normal;

							/* Relevant keywords have values greater than 9, the largest legal
							   numeric value. */

							font_weight = keyword;
							if (keyword == CSS_VALUE_normal)
								font_weight = 4;
							else if (keyword == CSS_VALUE_bold)
								font_weight = 7;
						}
						else if (!font_size && font_size_val < 0 && CSS_is_fontsize_val(keyword))
						{
							font_size = keyword;
						}
						else
							value_used = FALSE;
					}

					if (!value_used && (font_size || font_size_val >= 0))
					{
						font_family = SetFontFamilyL(prop_list, i, important, FALSE);
						break; // font family is always last
					}
					else if (!value_used)
						return INVALID;
				}
				else if (CSS_is_number(m_val_array[i].token))
				{
					double fval = m_val_array[i].value.number.number;
					int ftype = CSS_get_number_ext(m_val_array[i].token);
					if (next_is_line_height)
					{
						next_is_line_height = FALSE;
						line_height = float(fval);
						line_height_type = ftype;
						if (line_height < 0)
							return INVALID;
					}
					else if (!font_weight &&
								CSS_get_number_ext(m_val_array[i].token) == CSS_NUMBER &&
								fval >= 100 &&
								fval <= 900 &&
								((int(fval)/100)*100 == fval))
					{
						int fweight = (int) fval;
						font_weight = (short) (fweight/100);
					}
					else if (!font_size && font_size_val < 0)
					{
						if (ftype == CSS_NUMBER)
						{
							if ((!m_hld_prof || m_hld_prof->IsInStrictMode()) && fval != 0)
								return INVALID;
							else
								ftype = CSS_PX;
						}
						if (ftype == CSS_PERCENTAGE || CSS_is_length_number_ext(ftype))
						{
							font_size_val = float(fval);
							font_size_val_type = ftype;
						}
						else
							return INVALID;
					}
					else
						return INVALID;
				}
				else if (m_val_array[i].token == CSS_SLASH && !next_is_line_height)
				{
					next_is_line_height = TRUE;
				}
				else if ((font_size || font_size_val >= 0) && m_val_array[i].token == CSS_STRING_LITERAL)
				{
					font_family = SetFontFamilyL(prop_list, i, important, FALSE);
					break; // font family is always last
				}
				else if (m_val_array[i].token == CSS_IDENT && next_is_line_height)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (keyword < 0)
						return INVALID;
					if (keyword == CSS_VALUE_inherit)
						line_height_keyword = keyword;
					else if (keyword == CSS_VALUE_normal)
						line_height_keyword = keyword;
					else
						return INVALID;
					next_is_line_height = FALSE;
				}
				else
					return INVALID;
				i++;
			}

			if ((font_family == OK || m_hld_prof && !m_hld_prof->IsInStrictMode()) && (font_size || font_size_val >= 0))
			{
				if (!font_style)
					font_style = CSS_VALUE_normal;
				prop_list->AddTypeDeclL(CSS_PROPERTY_font_style, font_style, important, GetCurrentOrigin());

				if (!font_variant)
					font_variant = CSS_VALUE_normal;
				prop_list->AddTypeDeclL(CSS_PROPERTY_font_variant, font_variant, important, GetCurrentOrigin());

				if (!font_weight)
					font_weight = 4;
				if (font_weight <= 9)
					prop_list->AddDeclL(CSS_PROPERTY_font_weight, font_weight, CSS_NUMBER, important, GetCurrentOrigin());
				else
					/* Relevant keywords have values greater than 9, the largest legal numeric
					   value. */

					prop_list->AddTypeDeclL(CSS_PROPERTY_font_weight, CSSValue(font_weight), important, GetCurrentOrigin());

				if (font_size)
					prop_list->AddTypeDeclL(CSS_PROPERTY_font_size, font_size, important, GetCurrentOrigin());
				else
					prop_list->AddDeclL(CSS_PROPERTY_font_size, font_size_val, font_size_val_type, important, GetCurrentOrigin());

				if (line_height_keyword)
					prop_list->AddTypeDeclL(CSS_PROPERTY_line_height, line_height_keyword, important, GetCurrentOrigin());
				else if (line_height < 0)
					prop_list->AddTypeDeclL(CSS_PROPERTY_line_height, CSS_VALUE_normal, important, GetCurrentOrigin());
				else
					prop_list->AddDeclL(CSS_PROPERTY_line_height, line_height, line_height_type, important, GetCurrentOrigin());

				return OK;
			}
			else
				return INVALID;
		}
		break;

	case CSS_PROPERTY_caption_side:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_top || keyword == CSS_VALUE_left || keyword == CSS_VALUE_right || keyword == CSS_VALUE_bottom || keyword == CSS_VALUE_inherit)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_visibility:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_visible || keyword == CSS_VALUE_hidden || keyword == CSS_VALUE_collapse || keyword == CSS_VALUE_inherit)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_box_decoration_break:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_clone || keyword == CSS_VALUE_slice || keyword == CSS_VALUE_inherit)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_clear:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_left || keyword == CSS_VALUE_right || keyword == CSS_VALUE_none || keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_both)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_float:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);

				switch (keyword)
				{
				case CSS_VALUE_none:
				case CSS_VALUE_inherit:
				case CSS_VALUE_left:
				case CSS_VALUE_right:
				case CSS_VALUE__o_top:
				case CSS_VALUE__o_bottom:
				case CSS_VALUE__o_top_corner:
				case CSS_VALUE__o_bottom_corner:
				case CSS_VALUE__o_top_next_page:
				case CSS_VALUE__o_bottom_next_page:
				case CSS_VALUE__o_top_corner_next_page:
				case CSS_VALUE__o_bottom_corner_next_page:
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
					break;
				default:
					return INVALID;
				}
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_display:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (CSS_is_display_val(keyword) || keyword == CSS_VALUE_none || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_text_align:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (CSS_is_text_align_val(keyword) || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_text_transform:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (CSS_is_text_transform_val(keyword) || keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_none)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_text_decoration:
		{
			short value = 0;
			while (i < m_val_array.GetCount())
			{
				if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);

					switch (keyword)
					{
					case CSS_VALUE_underline:
						value |= CSS_TEXT_DECORATION_underline;
						break;

					case CSS_VALUE_overline:
						value |= CSS_TEXT_DECORATION_overline;
						break;

					case CSS_VALUE_line_through:
						value |= CSS_TEXT_DECORATION_line_through;
						break;

					case CSS_VALUE_blink:
						value |= CSS_TEXT_DECORATION_blink;
						break;

					case CSS_VALUE_inherit:
					case CSS_VALUE_none:
						if (m_val_array.GetCount() == 1)
						{
							prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
							return OK;
						}
						// fall-through is intended

					default:
						return INVALID;
					}
				}
				else
					return INVALID;
				i++;
			}

			if (value)
			{
				/* Text decoration flags are stored as CSSValue, carefully chosen not to
				   collide with other valid values for text-decoration. */

				prop_list->AddTypeDeclL(prop, CSSValue(value), important, GetCurrentOrigin());
				return OK;
			}
		}
		break;

	case CSS_PROPERTY_text_shadow:
	case CSS_PROPERTY_box_shadow:
		return SetShadowL(prop_list, prop, important);

	case CSS_PROPERTY__o_border_image:
		return SetBorderImageL(prop_list, important);

	case CSS_PROPERTY_vertical_align:
	case CSS_PROPERTY_text_indent:
	case CSS_PROPERTY_word_spacing:
	case CSS_PROPERTY_letter_spacing:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);

				if (keyword == CSS_VALUE_inherit || (prop == CSS_PROPERTY_vertical_align && CSS_is_vertical_align_val(keyword)) ||
					(keyword == CSS_VALUE_normal && (prop == CSS_PROPERTY_letter_spacing || prop == CSS_PROPERTY_word_spacing)))
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else if (CSS_is_number(m_val_array[0].token) && m_val_array[0].token != CSS_DIMEN)
			{
				float length = float(m_val_array[0].value.number.number);
				int length_type = CSS_get_number_ext(m_val_array[0].token);
				if ((prop != CSS_PROPERTY_letter_spacing && prop != CSS_PROPERTY_word_spacing) || length_type != CSS_PERCENTAGE)
				{
					if (length_type == CSS_NUMBER && (length == 0 || (m_hld_prof && !m_hld_prof->IsInStrictMode())))
						length_type = CSS_PX;

					if (length_type == CSS_PERCENTAGE || CSS_is_length_number_ext(length_type))
						prop_list->AddDeclL(prop, length, length_type, important, GetCurrentOrigin());
					else
						return INVALID;
				}
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_list_style_position:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inside || keyword == CSS_VALUE_outside || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_list_style_type:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (CSS_is_list_style_type_val(keyword) || keyword == CSS_VALUE_none || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_list_style_image:
#ifdef CSS_GRADIENT_SUPPORT
		if (CSS_is_gradient(m_val_array[i].token))
		{
			CSS_generic_value gen_value;

			CSSValueType type = (CSSValueType) m_val_array[i].token;
			CSS_Gradient* gradient = SetGradientL(/* inout */ i, /* inout */ type);

			if (!gradient)
				return INVALID;

			gradients.AddObjectL(gradient);

			gen_value.SetValueType(type);
			gen_value.SetGradient(gradient);

			prop_list->AddDeclL(prop, &gen_value, 1, 0, important, GetCurrentOrigin());
		}
		else
#endif // CSS_GRADIENT_SUPPORT
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_none || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else if (m_val_array[0].token == CSS_FUNCTION_URL)
			{
				// image
				const uni_char* url_name = m_input_buffer->GetURLL(m_base_url,
					m_val_array[0].value.str.start_pos,
					m_val_array[0].value.str.str_len).GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
				if (url_name)
					prop_list->AddDeclL(prop, url_name, uni_strlen(url_name), important, GetCurrentOrigin(), CSS_string_decl::StringDeclUrl, m_hld_prof == NULL);
			}
#ifdef SKIN_SUPPORT
			else if (m_val_array[0].token == CSS_FUNCTION_SKIN)
			{
				uni_char* skin_img = m_input_buffer->GetSkinString(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (skin_img)
				{
					prop_list->AddDeclL(prop, skin_img, important, GetCurrentOrigin(), CSS_string_decl::StringDeclSkin, m_hld_prof == NULL);
				}
				else
					LEAVE(OpStatus::ERR_NO_MEMORY);
			}
#endif // SKIN_SUPPORT
			else
				return INVALID;
		}
		else
			return INVALID;

		break;

	case CSS_PROPERTY_background_image:
		return SetBackgroundImageL(prop_list, important);

	case CSS_PROPERTY_list_style:
		{
			CSSValue list_style_pos = CSS_VALUE_UNSPECIFIED;
			CSSValue list_style_type = CSS_VALUE_UNSPECIFIED;
			const uni_char* list_style_image = 0;

			BOOL list_style_image_is_gradient = FALSE;

#ifdef CSS_GRADIENT_SUPPORT
			CSS_generic_value gradient_value;
#endif // CSS_GRADIENT_SUPPORT

			uni_char* heap_list_img = 0;
			ANCHOR_ARRAY(uni_char, heap_list_img); // only used for skin image strings.
			CSS_string_decl::StringType img_type = CSS_string_decl::StringDeclUrl;
			BOOL inherit_list_style_image = FALSE;
			int seen_none = 0;

			while (i < m_val_array.GetCount())
			{
				if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (keyword == CSS_VALUE_inherit && m_val_array.GetCount() == 1)
					{
						list_style_pos = CSS_VALUE_inherit;
						list_style_type = CSS_VALUE_inherit;
						inherit_list_style_image = TRUE;
					}
					else if (!list_style_pos && (keyword == CSS_VALUE_inside || keyword == CSS_VALUE_outside))
						list_style_pos = keyword;
					else if (!list_style_type && CSS_is_list_style_type_val(keyword))
						list_style_type = keyword;
					else if (keyword == CSS_VALUE_none)
						seen_none++;
					else
						return INVALID;
				}
				else if (m_val_array[i].token == CSS_FUNCTION_URL && !list_style_image && !list_style_image_is_gradient)
				{
					// image
					list_style_image = m_input_buffer->GetURLL(m_base_url,
						m_val_array[i].value.str.start_pos,
						m_val_array[i].value.str.str_len).GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
					if (!list_style_image)
						return INVALID;
				}
#ifdef SKIN_SUPPORT
				else if (m_val_array[i].token == CSS_FUNCTION_SKIN && !list_style_image && !list_style_image_is_gradient)
				{
					heap_list_img = m_input_buffer->GetSkinString(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
					if (heap_list_img)
					{
						ANCHOR_ARRAY_RESET(heap_list_img);
						list_style_image = heap_list_img;
						img_type = CSS_string_decl::StringDeclSkin;
					}
					else
						LEAVE(OpStatus::ERR_NO_MEMORY);
				}
#endif // SKIN_SUPPORT
#ifdef CSS_GRADIENT_SUPPORT
				else if (CSS_is_gradient(m_val_array[i].token) && !list_style_image && !list_style_image_is_gradient)
				{
					list_style_image_is_gradient = TRUE;

					CSSValueType type = (CSSValueType) m_val_array[i].token;
					CSS_Gradient* gradient = SetGradientL(/* inout */ i, /* inout */ type);

					if (!gradient)
						return INVALID;

					gradients.AddObjectL(gradient);

					gradient_value.SetValueType(type);
					gradient_value.SetGradient(gradient);
				}
#endif // CSS_GRADIENT_SUPPORT
				else
					return INVALID;
				i++;
			}

			if (seen_none)
			{
				if (!list_style_type)
				{
					list_style_type = CSS_VALUE_none;
					--seen_none;
				}
				if (!list_style_image && !list_style_image_is_gradient)
					--seen_none;
				if (seen_none > 0)
					return INVALID;
			}

			if (list_style_pos || list_style_type || list_style_image || list_style_image_is_gradient || inherit_list_style_image)
			{
				if (list_style_pos)
					prop_list->AddTypeDeclL(CSS_PROPERTY_list_style_position, list_style_pos, important, GetCurrentOrigin());
				else
					prop_list->AddTypeDeclL(CSS_PROPERTY_list_style_position, CSS_VALUE_outside, important, GetCurrentOrigin());

				if (list_style_type)
					prop_list->AddTypeDeclL(CSS_PROPERTY_list_style_type, list_style_type, important, GetCurrentOrigin());
				else
					prop_list->AddTypeDeclL(CSS_PROPERTY_list_style_type, CSS_VALUE_disc, important, GetCurrentOrigin());

				if (list_style_image)
					prop_list->AddDeclL(CSS_PROPERTY_list_style_image, list_style_image, uni_strlen(list_style_image), important, GetCurrentOrigin(), img_type, m_hld_prof == NULL);
#ifdef CSS_GRADIENT_SUPPORT
				else if (list_style_image_is_gradient)
					prop_list->AddDeclL(CSS_PROPERTY_list_style_image, &gradient_value, 1, 0, important, GetCurrentOrigin());
#endif // CSS_GRADIENT_SUPPORT
				else
					prop_list->AddTypeDeclL(CSS_PROPERTY_list_style_image, (inherit_list_style_image ? CSS_VALUE_inherit : CSS_VALUE_none), important, GetCurrentOrigin());

				return OK;
			}
			else
				return INVALID;
		}
		break;

	case CSS_PROPERTY_page:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_auto)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
			{
				uni_char* str = m_input_buffer->GetString(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (str)
				{
					prop_list->AddDeclL(prop, str, important, GetCurrentOrigin(), CSS_string_decl::StringDeclString, m_hld_prof == NULL);
				}
				else
					LEAVE(OpStatus::ERR_NO_MEMORY);
			}
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_size:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_auto || keyword == CSS_VALUE_portrait || keyword == CSS_VALUE_landscape || keyword == CSS_VALUE_inherit)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else if (m_val_array.GetCount() == 2 && CSS_is_number(m_val_array[0].token) && CSS_is_number(m_val_array[1].token))
			prop_list->AddDeclL(prop, float(m_val_array[0].value.number.number), float(m_val_array[1].value.number.number), CSS_get_number_ext(m_val_array[0].token), CSS_get_number_ext(m_val_array[1].token), important, GetCurrentOrigin());
		else if (m_val_array.GetCount() == 1 && CSS_is_number(m_val_array[0].token))
			prop_list->AddDeclL(prop, float(m_val_array[0].value.number.number), float(m_val_array[0].value.number.number), CSS_get_number_ext(m_val_array[0].token), CSS_get_number_ext(m_val_array[0].token), important, GetCurrentOrigin());
		else
			return INVALID;
		break;

	case CSS_PROPERTY_clip:
		if (m_val_array.GetCount() > 0)
		{
			if (m_val_array.GetCount() == 6 && m_val_array[0].token == CSS_FUNCTION_RECT && m_val_array[5].token == CSS_FUNCTION_RECT)
			{
				short clip_rect_typ[4];
				float clip_rect_val[4];

				while (++i < m_val_array.GetCount()-1)
				{
					if (m_val_array[i].token == CSS_IDENT)
					{
						keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
						if (keyword == CSS_VALUE_auto)
						{
							clip_rect_typ[i-1] = CSS_VALUE_auto;
						}
						else
							return INVALID;
					}
					else if (CSS_is_number(m_val_array[i].token))
					{
						if (!CSS_is_length_number_ext(m_val_array[i].token))
						{
							if ((m_val_array[i].token == CSS_NUMBER && m_hld_prof && !m_hld_prof->IsInStrictMode()) || m_val_array[i].value.number.number == 0)
								clip_rect_typ[i-1] = CSS_PX;
							else
								return INVALID;
						}
						else
							clip_rect_typ[i-1] = m_val_array[i].token;
						clip_rect_val[i-1] = float(m_val_array[i].value.number.number);
					}
					else
						return INVALID;
				}
				prop_list->AddDeclL(prop, clip_rect_typ, clip_rect_val, important, GetCurrentOrigin());
			}
			else if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_auto || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_cursor:
		if (m_val_array.GetCount() > 0 && m_val_array.GetCount()%2 == 1)
		{
			if (m_val_array[m_val_array.GetCount()-1].token != CSS_IDENT)
				return INVALID;

			for (int j=0; j<m_val_array.GetCount()-1; j++)
				if (j%2 == 0 && m_val_array[j].token != CSS_FUNCTION_URL ||
					j%2 == 1 && m_val_array[j].token != CSS_COMMA)
						return INVALID;

			keyword = m_input_buffer->GetValueSymbol(m_val_array[m_val_array.GetCount()-1].value.str.start_pos, m_val_array[m_val_array.GetCount()-1].value.str.str_len);
			if (keyword < 0)
			{
				if (m_val_array[m_val_array.GetCount()-1].value.str.str_len == 4)
				{
					uni_char cursor_str[5]; /* ARRAY OK 2009-02-12 rune */
					uni_char* string = m_input_buffer->GetString(cursor_str, m_val_array[m_val_array.GetCount()-1].value.str.start_pos, m_val_array[m_val_array.GetCount()-1].value.str.str_len);
					if (string)
					{
						if (uni_strni_eq_upper(string, "HAND", m_val_array[m_val_array.GetCount()-1].value.str.str_len))
							keyword = CSS_VALUE_pointer;
						else
							return INVALID;
					}
					else
						return INVALID;
				}
				else
					return INVALID;
			}
			else if (!CSS_is_cursor_val(keyword) && keyword != CSS_VALUE_inherit)
				return INVALID;

			if (m_val_array.GetCount() == 1)
			{
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				break;
			}
			else if (keyword != CSS_VALUE_inherit)
			{
				CSS_generic_value gen_arr[CSS_MAX_ARR_SIZE];
				CSS_anchored_heap_arrays<uni_char> strings;
				ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);
				int j=0;
				for (; i < m_val_array.GetCount()-1 && i < CSS_MAX_ARR_SIZE-1; i++)
				{
					if ((i % 2) == 1)
						continue;

					const uni_char* url_name = m_input_buffer->GetURLL(m_base_url,
						m_val_array[i].value.str.start_pos,
						m_val_array[i].value.str.str_len).GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();

					gen_arr[j].value_type = CSS_FUNCTION_URL;
					gen_arr[j].value.string = OP_NEWA_L(uni_char, uni_strlen(url_name)+1);
					strings.AddArrayL(gen_arr[j].value.string);
					uni_strcpy(gen_arr[j++].value.string, url_name);
				}

				gen_arr[j].value_type = CSS_IDENT;
				gen_arr[j++].value.type = keyword;

				prop_list->AddDeclL(prop, gen_arr, j, 0, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_direction:
#ifdef SUPPORT_TEXT_DIRECTION
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_rtl || keyword == CSS_VALUE_ltr)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
#endif
		break;

	case CSS_PROPERTY_unicode_bidi:
#ifdef SUPPORT_TEXT_DIRECTION
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_normal || keyword == CSS_VALUE_embed || keyword == CSS_VALUE_bidi_override)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
#endif
		break;

	case CSS_PROPERTY_overflow:
		level++;
		// fall-through is intended
	case CSS_PROPERTY_overflow_x:
	case CSS_PROPERTY_overflow_y:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);

			switch (keyword)
			{
			case CSS_VALUE_inherit:
			case CSS_VALUE_auto:
			case CSS_VALUE_hidden:
			case CSS_VALUE_visible:
			case CSS_VALUE_scroll:
#ifdef PAGED_MEDIA_SUPPORT
			case CSS_VALUE__o_paged_x_controls:
			case CSS_VALUE__o_paged_x:
			case CSS_VALUE__o_paged_y_controls:
			case CSS_VALUE__o_paged_y:
#endif // PAGED_MEDIA_SUPPORT
				if (level)
				{
					prop_list->AddTypeDeclL(CSS_PROPERTY_overflow_x, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_overflow_y, keyword, important, GetCurrentOrigin());
				}
				else
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				break;
			default:
				return INVALID;
			}
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_counter_reset:
	case CSS_PROPERTY_counter_increment:
	case CSS_PROPERTY_quotes:
	case CSS_PROPERTY_content:
		{
			CSS_generic_value_list gen_arr;
			ANCHOR(CSS_generic_value_list, gen_arr);
			CSS_anchored_heap_arrays<uni_char> strings;
			ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);
			while (i < m_val_array.GetCount())
			{
				if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (prop == CSS_PROPERTY_content && keyword >= 0 && CSS_is_quote_val(keyword))
						gen_arr.PushIdentL(keyword);
					else if (i == 0 && m_val_array.GetCount() == 1 &&
						(keyword == CSS_VALUE_inherit || (prop == CSS_PROPERTY_content && keyword == CSS_VALUE_normal) || keyword == CSS_VALUE_none))
					{
						prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
						return OK;
					}
					else if (prop == CSS_PROPERTY_counter_reset || prop == CSS_PROPERTY_counter_increment)
					{
						uni_char* string = m_input_buffer->GetString(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
						if (!string)
							LEAVE(OpStatus::ERR_NO_MEMORY);
						strings.AddArrayL(string);
						gen_arr.PushStringL(CSS_STRING_LITERAL, string);
					}
					else
						return INVALID;
				}
				else if (prop == CSS_PROPERTY_quotes && m_val_array[i].token == CSS_STRING_LITERAL ||
					     (prop == CSS_PROPERTY_content && (
						   m_val_array[i].token == CSS_STRING_LITERAL ||
						   m_val_array[i].token == CSS_FUNCTION_COUNTER ||
						   m_val_array[i].token == CSS_FUNCTION_COUNTERS ||
						   m_val_array[i].token == CSS_FUNCTION_ATTR)))
				{
					uni_char* string = m_input_buffer->GetString(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (!string)
						LEAVE(OpStatus::ERR_NO_MEMORY);
					strings.AddArrayL(string);
					gen_arr.PushStringL(CSSValueType(m_val_array[i].token), string);
				}
				else if (m_val_array[i].token == CSS_NUMBER && (prop == CSS_PROPERTY_counter_reset || prop == CSS_PROPERTY_counter_increment))
				{
					double num_val = m_val_array[i].value.number.number;
					if (gen_arr.Last() && gen_arr.Last()->value.GetValueType() == CSS_STRING_LITERAL && num_val == op_floor(num_val))
					{
						int int_val;
						if (num_val >= INT_MAX)
							int_val = INT_MAX;
						else if (num_val <= INT_MIN)
							int_val = INT_MIN;
						else
							int_val = int(num_val);
						gen_arr.PushIntegerL(CSS_INT_NUMBER, int_val);
					}
					else
						return INVALID;
				}
				else if (m_val_array[i].token == CSS_FUNCTION_URL && prop == CSS_PROPERTY_content)
				{
					const uni_char* url_name =
						m_input_buffer->GetURLL(m_base_url,
												m_val_array[i].value.str.start_pos,
												m_val_array[i].value.str.str_len).GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
					if (url_name != NULL)
					{
						uni_char* string = OP_NEWA_L(uni_char, uni_strlen(url_name)+1);
						uni_strcpy(string, url_name);
						strings.AddArrayL(string);
						gen_arr.PushStringL(CSS_FUNCTION_URL, string);
					}
				}
#ifdef SKIN_SUPPORT
				else if (m_val_array[i].token == CSS_FUNCTION_SKIN && prop == CSS_PROPERTY_content)
				{
					uni_char* skin_img = m_input_buffer->GetSkinString(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (!skin_img)
						LEAVE(OpStatus::ERR_NO_MEMORY);
					strings.AddArrayL(skin_img);
					gen_arr.PushStringL(CSS_FUNCTION_SKIN, skin_img);
				}
#endif // SKIN_SUPPORT
				else if (m_val_array[i].token == CSS_FUNCTION_LANGUAGE_STRING
					&& prop == CSS_PROPERTY_content)
				{
					gen_arr.PushIntegerL(CSS_FUNCTION_LANGUAGE_STRING, m_val_array[i].value.integer.integer);
				}
				else
					return INVALID;

				i++;
			}

			if (gen_arr.First() && (prop != CSS_PROPERTY_quotes || gen_arr.Cardinal() % 2 == 0))
			{
				prop_list->AddDeclL(prop, gen_arr, 0, important, GetCurrentOrigin());
				return OK;
			}
			else
				return INVALID;
		}
		break;

#ifdef _CSS_LINK_SUPPORT_
	case CSS_PROPERTY__o_link:
		if (i < m_val_array.GetCount())
		{
			if (m_val_array[i].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
				if (keyword == CSS_VALUE_none)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else if (m_val_array[i].token == CSS_FUNCTION_URL)
			{
				const uni_char* url_name = m_input_buffer->GetURLL(m_base_url,
					m_val_array[i].value.str.start_pos,
					m_val_array[i].value.str.str_len).GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
				if (url_name)
					prop_list->AddDeclL(prop, url_name, uni_strlen(url_name), important, GetCurrentOrigin(), CSS_string_decl::StringDeclUrl, m_hld_prof == NULL);
			}
			else if (m_val_array[i].token == CSS_STRING_LITERAL || m_val_array[i].token == CSS_FUNCTION_ATTR)
			{
				CSS_generic_value gen_arr[1];
				gen_arr[0].value_type = m_val_array[i].token;
				gen_arr[0].value.string = m_input_buffer->GetString(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
				if (!gen_arr[0].value.string)
					LEAVE(OpStatus::ERR_NO_MEMORY);
				uni_char* str = gen_arr[0].value.string;
				ANCHOR_ARRAY(uni_char, str);
				prop_list->AddDeclL(prop, gen_arr, 1, 0, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		break;

	case CSS_PROPERTY__o_link_source:
		if (i < m_val_array.GetCount() && m_val_array[i].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
			if (keyword != CSS_VALUE_none && keyword != CSS_VALUE_current && keyword != CSS_VALUE_next)
				return INVALID;
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		}
		break;
#endif // _CSS_LINK_SUPPORT_

	case CSS_PROPERTY_white_space:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit || CSS_is_whitespace_value(keyword))
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_table_layout:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_fixed || keyword == CSS_VALUE_auto)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_border_collapse:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_collapse || keyword == CSS_VALUE_separate)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_border_spacing:
		if (m_val_array.GetCount() < 1 || m_val_array.GetCount() > 2)
			return INVALID;
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else if (CSS_is_length_number_ext(m_val_array[0].token) || m_val_array[0].token == CSS_NUMBER)
		{
			float length_val_1 = float(m_val_array[0].value.number.number);
			int length_type_1 = CSS_get_number_ext(m_val_array[0].token);
			if (length_val_1 < 0)
				return INVALID;
			if (length_type_1 == CSS_NUMBER)
			{
				if (length_val_1 == 0 || m_hld_prof && !m_hld_prof->IsInStrictMode())
					length_type_1 = CSS_PX;
				else
					return INVALID;
			}
			if (m_val_array.GetCount() == 2)
			{
				if (CSS_is_length_number_ext(m_val_array[1].token) || m_val_array[1].token == CSS_NUMBER)
				{
					float length_val_2 = float(m_val_array[1].value.number.number);
					int length_type_2 = CSS_get_number_ext(m_val_array[1].token);
					if (length_val_2 < 0)
						return INVALID;
					if (length_type_2 == CSS_NUMBER && (length_val_2 == 0 || m_hld_prof && !m_hld_prof->IsInStrictMode()))
					{
						if (length_val_2 == 0 || m_hld_prof && !m_hld_prof->IsInStrictMode())
							length_type_2 = CSS_PX;
						else
							return INVALID;
					}
					prop_list->AddDeclL(prop, length_val_1, length_val_2, length_type_1, length_type_2, important, GetCurrentOrigin());
				}
				else
					return INVALID;
			}
			else
				prop_list->AddDeclL(prop, length_val_1, length_type_1, important, GetCurrentOrigin());
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_empty_cells:
		if (m_val_array.GetCount() == 1 && m_val_array[i].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_show || keyword == CSS_VALUE_hide)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_z_index:
	case CSS_PROPERTY_order:
#ifdef WEBKIT_OLD_FLEXBOX
	case CSS_PROPERTY__webkit_box_ordinal_group:
#endif // WEBKIT_OLD_FLEXBOX
		if (m_val_array.GetCount() != 1)
			return INVALID;
		if (m_val_array[0].token == CSS_IDENT)
		{
			keyword = GetKeyword(0);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_auto && prop == CSS_PROPERTY_z_index)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else if (CSS_is_number(m_val_array[0].token) && CSS_get_number_ext(m_val_array[0].token) == CSS_NUMBER)
		{
			int num;
			if (m_val_array[0].value.number.number >= INT_MAX)
				num = INT_MAX;
			else if (m_val_array[0].value.number.number <= INT_MIN)
				num = INT_MIN;
			else
				num = int(m_val_array[0].value.number.number);
			prop_list->AddLongDeclL(prop, (long)num, important, GetCurrentOrigin());
		}
		else
			return INVALID;

		break;

	case CSS_PROPERTY_justify_content:
	case CSS_PROPERTY_align_content:
		if (m_val_array.GetCount() != 1 || m_val_array[0].token != CSS_IDENT)
			return INVALID;

		keyword = GetKeyword(0);
		switch (keyword)
		{
		case CSS_VALUE_stretch:
			if (prop != CSS_PROPERTY_align_content)
				return INVALID;

			// fall-through

		case CSS_VALUE_inherit:
		case CSS_VALUE_flex_start:
		case CSS_VALUE_flex_end:
		case CSS_VALUE_center:
		case CSS_VALUE_space_between:
		case CSS_VALUE_space_around:
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			break;
		default:
			return INVALID;
		}
		break;

	case CSS_PROPERTY_align_items:
	case CSS_PROPERTY_align_self:
		if (m_val_array.GetCount() != 1 || m_val_array[0].token != CSS_IDENT)
			return INVALID;

		keyword = GetKeyword(0);
		switch (keyword)
		{
		case CSS_VALUE_auto:
			if (prop != CSS_PROPERTY_align_self)
				return INVALID;
			// fall-through
		case CSS_VALUE_inherit:
		case CSS_VALUE_flex_start:
		case CSS_VALUE_flex_end:
		case CSS_VALUE_center:
		case CSS_VALUE_baseline:
		case CSS_VALUE_stretch:
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			break;
		default:
			return INVALID;
		}
		break;

#ifdef WEBKIT_OLD_FLEXBOX
	case CSS_PROPERTY__webkit_box_flex:
#endif // WEBKIT_OLD_FLEXBOX
	case CSS_PROPERTY_flex_grow:
	case CSS_PROPERTY_flex_shrink:
		if (m_val_array.GetCount() != 1)
			return INVALID;
		if (m_val_array[0].token == CSS_IDENT)
		{
			keyword = GetKeyword(0);
			if (keyword == CSS_VALUE_inherit ||
				keyword == CSS_VALUE_initial && (prop == CSS_PROPERTY_flex_grow || prop == CSS_PROPERTY_flex_shrink))
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else if (CSS_is_number(m_val_array[0].token) && CSS_get_number_ext(m_val_array[0].token) == CSS_NUMBER)
		{
			float value = float(m_val_array[0].value.number.number);

			if (value < 0.0)
				return INVALID;

			prop_list->AddDeclL(prop, value, CSS_NUMBER, important, GetCurrentOrigin());
		}
		else
			return INVALID;

		break;

	case CSS_PROPERTY_flex_flow:
		{
			if (m_val_array.GetCount() < 1 || m_val_array.GetCount() > 2)
				return INVALID;

			CSSValue direction = CSS_VALUE_UNSPECIFIED;
			CSSValue wrap = CSS_VALUE_UNSPECIFIED;

			// First verify that the declaration is valid.

			for (int i = 0; i < m_val_array.GetCount(); i++)
				if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = GetKeyword(i);
					if (keyword == CSS_VALUE_inherit)
					{
						if (m_val_array.GetCount() == 1)
						{
							// Value is inherit. Add it right away and return.
							prop_list->AddTypeDeclL(CSS_PROPERTY_flex_direction, keyword, important, GetCurrentOrigin());
							prop_list->AddTypeDeclL(CSS_PROPERTY_flex_wrap, keyword, important, GetCurrentOrigin());
							return OK;
						}
						else
							return INVALID;
					}
					else
						if (IsFlexDirectionValue(CSSValue(keyword)))
							if (direction == CSS_VALUE_UNSPECIFIED)
								direction = CSSValue(keyword);
							else
								return INVALID;
						else
							if (IsFlexWrapValue(CSSValue(keyword)))
								if (wrap == CSS_VALUE_UNSPECIFIED)
									wrap = CSSValue(keyword);
								else
									return INVALID;
							else
								return INVALID;
				}
				else
					return INVALID;

			// All OK. Add declarations.

			if (direction == CSS_VALUE_UNSPECIFIED)
				direction = CSS_VALUE_row;

			if (wrap == CSS_VALUE_UNSPECIFIED)
				wrap = CSS_VALUE_nowrap;

			prop_list->AddTypeDeclL(CSS_PROPERTY_flex_direction, direction, important, GetCurrentOrigin());
			prop_list->AddTypeDeclL(CSS_PROPERTY_flex_wrap, wrap, important, GetCurrentOrigin());
		}
		break;

	case CSS_PROPERTY_flex_direction:
		if (m_val_array.GetCount() != 1 || m_val_array[0].token != CSS_IDENT)
			return INVALID;

		keyword = GetKeyword(0);

		if (IsFlexDirectionValue(CSSValue(keyword)))
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		else
			return INVALID;
		break;

	case CSS_PROPERTY_flex_wrap:
		if (m_val_array.GetCount() != 1 || m_val_array[0].token != CSS_IDENT)
			return INVALID;

		keyword = GetKeyword(0);

		if (IsFlexWrapValue(CSSValue(keyword)))
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		else
			return INVALID;
		break;

	case CSS_PROPERTY_flex_basis:
		{
			CSS_generic_value preferred_size;

			if (CSS_is_number(m_val_array[i].token))
			{
				if (m_val_array[i].token == CSS_DIMEN)
					return INVALID;

				preferred_size.value_type = m_val_array[i].token;
				preferred_size.value.real = float(m_val_array[i].value.number.number);
			}
			else
				if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = GetKeyword(i);
					if (keyword != CSS_VALUE_inherit && keyword != CSS_VALUE_initial && keyword != CSS_VALUE_auto && keyword != CSS_VALUE__o_skin && keyword != CSS_VALUE__o_content_size)
						return INVALID;

					preferred_size.value_type = CSS_IDENT;
					preferred_size.value.type = keyword;
				}
				else
					return INVALID;

			if (preferred_size.value_type == CSS_IDENT)
				prop_list->AddTypeDeclL(CSS_PROPERTY_flex_basis, CSSValue(preferred_size.value.type), important, GetCurrentOrigin());
			else
				prop_list->AddDeclL(CSS_PROPERTY_flex_basis, preferred_size.value.real, preferred_size.value_type, important, GetCurrentOrigin());
			break;
		}

	case CSS_PROPERTY_flex:
		{
			int val_count = m_val_array.GetCount();

			if (val_count < 1 || val_count > 3)
				return INVALID;

			if (val_count == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = GetKeyword(0);
				if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_initial)
				{
					prop_list->AddTypeDeclL(CSS_PROPERTY_flex_grow, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_flex_shrink, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_flex_basis, keyword, important, GetCurrentOrigin());
					break;
				}
				else if (keyword == CSS_VALUE_none)
				{
					prop_list->AddDeclL(CSS_PROPERTY_flex_grow, float(0.0), CSS_NUMBER, important, GetCurrentOrigin());
					prop_list->AddDeclL(CSS_PROPERTY_flex_shrink, float(0.0), CSS_NUMBER, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_flex_basis, CSS_VALUE_auto, important, GetCurrentOrigin());
					break;
				}

				// Other keywords than the above will be attempted parsed as <flex-basis>.
			}

			int flexcount = 0;
			float flex_grow = 1.0;
			float flex_shrink = 1.0;
			CSS_generic_value preferred_size;
			BOOL has_preferred_size = FALSE;

			preferred_size.value_type = CSS_PX;
			preferred_size.value.real = 0.0;

			for (int i = 0; i < val_count; i++)
				if (m_val_array[i].token == CSS_NUMBER)
				{
					/* <flex-grow> or <flex-shrink>, or zero <flex-basis>
					   preceded by both <flex-grow> and <flex-shrink> */

					float number = float(m_val_array[i].value.number.number);

					if (number < 0.0)
						return INVALID;

					if (flexcount >= 2)
					{
						/* This is a unit-less number, but two have already been supplied. That
						   means that this one has be treated as flex-basis, i.e. a length. The
						   only valid unit-less length is 0. */

						if (number == 0.0)
						{
							preferred_size.value_type = CSS_PX;
							preferred_size.value.real = number;
							has_preferred_size = TRUE;
						}
						else
							return INVALID;
					}
					else
						if (flexcount++ == 0)
							flex_grow = number;
						else
							flex_shrink = number;
				}
				else
				{
					// <flex-basis> (or something illegal)

					if (flexcount == 1)
						/* We have parsed <flex-grow>, but not <flex-shrink>. <flex-grow>
						   and <flex-shrink> must be specified next to each other (and in
						   that order) if one wants to specify both. This means that the
						   opportunity to specify <flex-shrink> was waived. */

						flexcount = 2;

					if (has_preferred_size)
						return INVALID;

					if (CSS_is_number(m_val_array[i].token))
					{
						if (m_val_array[i].token == CSS_DIMEN)
							return INVALID;

						preferred_size.value_type = m_val_array[i].token;
						preferred_size.value.real = float(m_val_array[i].value.number.number);
					}
					else
						if (m_val_array[i].token == CSS_IDENT)
						{
							keyword = GetKeyword(i);
							if (keyword != CSS_VALUE_auto && keyword != CSS_VALUE__o_skin && keyword != CSS_VALUE__o_content_size)
								return INVALID;

							preferred_size.value_type = CSS_IDENT;
							preferred_size.value.type = keyword;
						}
						else
							return INVALID;

					has_preferred_size = TRUE;
				}

			prop_list->AddDeclL(CSS_PROPERTY_flex_grow, flex_grow, CSS_NUMBER, important, GetCurrentOrigin());
			prop_list->AddDeclL(CSS_PROPERTY_flex_shrink, flex_shrink, CSS_NUMBER, important, GetCurrentOrigin());

			if (preferred_size.value_type == CSS_IDENT)
				prop_list->AddTypeDeclL(CSS_PROPERTY_flex_basis, CSSValue(preferred_size.value.type), important, GetCurrentOrigin());
			else
				prop_list->AddDeclL(CSS_PROPERTY_flex_basis, preferred_size.value.real, preferred_size.value_type, important, GetCurrentOrigin());

			break;
		}

#ifdef WEBKIT_OLD_FLEXBOX
	case CSS_PROPERTY__webkit_box_pack:
		if (m_val_array.GetCount() != 1 || m_val_array[0].token != CSS_IDENT)
			return INVALID;

		keyword = GetKeyword(0);

		if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_start || keyword == CSS_VALUE_end || keyword == CSS_VALUE_center || keyword == CSS_VALUE_justify)
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		else
			return INVALID;
		break;

	case CSS_PROPERTY__webkit_box_align:
		if (m_val_array.GetCount() != 1 || m_val_array[0].token != CSS_IDENT)
			return INVALID;

		keyword = GetKeyword(0);

		if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_start || keyword == CSS_VALUE_end || keyword == CSS_VALUE_center || keyword ==  CSS_VALUE_baseline || keyword == CSS_VALUE_stretch)
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		else
			return INVALID;
		break;

	case CSS_PROPERTY__webkit_box_orient:
		if (m_val_array.GetCount() != 1 || m_val_array[0].token != CSS_IDENT)
			return INVALID;

		keyword = GetKeyword(0);

		if (keyword == CSS_VALUE_horizontal || keyword == CSS_VALUE_vertical || keyword == CSS_VALUE_inline_axis || keyword == CSS_VALUE_block_axis || keyword == CSS_VALUE_inherit)
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		else
			return INVALID;
		break;

	case CSS_PROPERTY__webkit_box_direction:
		if (m_val_array.GetCount() != 1 || m_val_array[0].token != CSS_IDENT)
			return INVALID;

		keyword = GetKeyword(0);

		if (keyword == CSS_VALUE_normal || keyword == CSS_VALUE_reverse || keyword == CSS_VALUE_inherit)
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		else
			return INVALID;
		break;

	case CSS_PROPERTY__webkit_box_lines:
		if (m_val_array.GetCount() != 1 || m_val_array[0].token != CSS_IDENT)
			return INVALID;

		keyword = GetKeyword(0);

		if (keyword == CSS_VALUE_single || keyword == CSS_VALUE_multiple || keyword == CSS_VALUE_inherit)
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		else
			return INVALID;
		break;
#endif // WEBKIT_OLD_FLEXBOX

	case CSS_PROPERTY_columns:
		{
			if (m_val_array.GetCount() < 1 || m_val_array.GetCount() > 2)
				return INVALID;

			long column_count = LONG_MIN;
			float column_width = -1.0;
			short column_width_ext = 0;

			// First verify that the declaration is valid.

			for (int i = 0; i < m_val_array.GetCount(); i++)
			{
				if (CSS_is_number(m_val_array[i].token) && CSS_get_number_ext(m_val_array[i].token) == CSS_NUMBER && m_val_array[i].value.number.number > 0)
					if (column_count > 0)
						return INVALID;
					else
						column_count = (long)MIN(m_val_array[i].value.number.number, SHRT_MAX);
				else if (CSS_is_length_number_ext(m_val_array[i].token) && m_val_array[i].value.number.number > 0 && CSS_get_number_ext(m_val_array[i].token) != CSS_PERCENTAGE)
					if (column_width > 0)
						return INVALID;
					else
					{
						column_width = (float)m_val_array[i].value.number.number;
						column_width_ext = CSS_get_number_ext(m_val_array[i].token);
					}
				else if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (keyword == CSS_VALUE_inherit)
						if (m_val_array.GetCount() == 1)
						{
							// Value is inherit. Add it right away and return.
							prop_list->AddTypeDeclL(CSS_PROPERTY_column_width, keyword, important, GetCurrentOrigin());
							prop_list->AddTypeDeclL(CSS_PROPERTY_column_count, keyword, important, GetCurrentOrigin());
							return OK;
						}
						else
							return INVALID;
					else if (keyword != CSS_VALUE_auto)
						return INVALID;
				}
				else
					return INVALID;
			}

			// All OK. Add declaration(s).

			if (column_width > 0)
				prop_list->AddDeclL(CSS_PROPERTY_column_width, column_width, column_width_ext, important, GetCurrentOrigin());
			else
				prop_list->AddTypeDeclL(CSS_PROPERTY_column_width, CSS_VALUE_auto, important, GetCurrentOrigin());

			if (column_count > 0)
				prop_list->AddLongDeclL(CSS_PROPERTY_column_count, column_count, important, GetCurrentOrigin());
			else
				prop_list->AddTypeDeclL(CSS_PROPERTY_column_count, CSS_VALUE_auto, important, GetCurrentOrigin());

		}
		break;

	case CSS_PROPERTY_column_count:
		if (m_val_array.GetCount() != 1)
			return INVALID;
		if (m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_auto)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else if (CSS_is_number(m_val_array[0].token) && CSS_get_number_ext(m_val_array[0].token) == CSS_NUMBER && m_val_array[0].value.number.number > 0)
		{
			int num = int(m_val_array[0].value.number.number);
			prop_list->AddLongDeclL(prop, (long)MIN(num, SHRT_MAX), important, GetCurrentOrigin());
		}
		else
			return INVALID;

		break;

	case CSS_PROPERTY_column_fill:
		if (m_val_array.GetCount() != 1)
			return INVALID;
		if (m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_auto || keyword == CSS_VALUE_balance)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;

		break;

	case CSS_PROPERTY_column_span:
		if (m_val_array.GetCount() != 1)
			return INVALID;
		if (m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_all || keyword == CSS_VALUE_none)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else if (m_val_array[0].token == CSS_FUNCTION_INTEGER && m_val_array[0].value.integer.integer > 0)
			prop_list->AddLongDeclL(prop, (long)MIN(m_val_array[0].value.integer.integer, SHRT_MAX), important, GetCurrentOrigin());
		else
			return INVALID;

		break;

	case CSS_PROPERTY_position:
		if (m_val_array.GetCount() != 1)
			return INVALID;
		if (m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_static || keyword == CSS_VALUE_absolute
				|| keyword == CSS_VALUE_relative || keyword == CSS_VALUE_fixed)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		break;

	case CSS_PROPERTY_widows:
	case CSS_PROPERTY_orphans:
		if (m_val_array.GetCount() != 1)
			return INVALID;
		if (m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else if (CSS_is_number(m_val_array[0].token) && CSS_get_number_ext(m_val_array[0].token) == CSS_NUMBER)
		{
			int num = (int)m_val_array[0].value.number.number;
			if (num >= 0)
				prop_list->AddLongDeclL(prop, (long)num, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;

		break;

	case CSS_PROPERTY_page_break_after:
	case CSS_PROPERTY_page_break_before:
	case CSS_PROPERTY_page_break_inside:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_avoid || keyword == CSS_VALUE_auto || keyword == CSS_VALUE_inherit
				|| (prop != CSS_PROPERTY_page_break_inside && (keyword == CSS_VALUE_always || keyword == CSS_VALUE_left || keyword == CSS_VALUE_right)))
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_break_after:
	case CSS_PROPERTY_break_before:
	case CSS_PROPERTY_break_inside:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);

			switch (keyword)
			{
			case CSS_VALUE_always:
			case CSS_VALUE_left:
			case CSS_VALUE_right:
			case CSS_VALUE_page:
			case CSS_VALUE_column:
				if (prop == CSS_PROPERTY_break_inside)
					return INVALID;

				// fall-through
			case CSS_VALUE_avoid:
			case CSS_VALUE_avoid_page:
			case CSS_VALUE_avoid_column:
			case CSS_VALUE_auto:
			case CSS_VALUE_inherit:
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				break;
			default:
				return INVALID;
			}
		}
		else
			return INVALID;
		break;

#ifdef SVG_SUPPORT
	case CSS_PROPERTY_fill:
	case CSS_PROPERTY_stroke:
		if (m_val_array.GetCount() >= 1 && m_val_array[0].token == CSS_FUNCTION_URL)
			return SetPaintUriL(prop_list, prop, important);
		// fall-through is intended
	case CSS_PROPERTY_stop_color:
	case CSS_PROPERTY_flood_color:
	case CSS_PROPERTY_lighting_color:
	case CSS_PROPERTY_solid_color:
	case CSS_PROPERTY_viewport_fill:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos,
													 m_val_array[0].value.str.str_len);

			if (keyword == CSS_VALUE_inherit ||
				keyword == CSS_VALUE_none ||
				keyword == CSS_VALUE_currentColor)
			{
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				return OK;
			}
		}
		// fall-through is intended
#endif
	case CSS_PROPERTY_color:
	case CSS_PROPERTY_outline_color:
	case CSS_PROPERTY_column_rule_color:
	case CSS_PROPERTY_background_color:
	case CSS_PROPERTY_border_top_color:
	case CSS_PROPERTY_border_left_color:
	case CSS_PROPERTY_border_right_color:
	case CSS_PROPERTY_border_bottom_color:
	case CSS_PROPERTY_scrollbar_base_color:
	case CSS_PROPERTY_scrollbar_face_color:
	case CSS_PROPERTY_scrollbar_arrow_color:
	case CSS_PROPERTY_scrollbar_track_color:
	case CSS_PROPERTY_scrollbar_shadow_color:
	case CSS_PROPERTY_scrollbar_3dlight_color:
	case CSS_PROPERTY_scrollbar_highlight_color:
	case CSS_PROPERTY_scrollbar_darkshadow_color:
		if (m_val_array.GetCount() == 1)
		{
			COLORREF color;
			uni_char* skin_color = NULL;
			switch (SetColorL(color, keyword, skin_color, m_val_array[0]))
			{
			case COLOR_KEYWORD:
				if (keyword == CSS_VALUE_transparent
					|| (keyword == CSS_VALUE_invert && prop == CSS_PROPERTY_outline_color)
					|| keyword == CSS_VALUE_inherit
					|| keyword == CSS_VALUE_currentColor)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				return OK;
			case COLOR_RGBA:
				prop_list->AddLongDeclL(prop, (long)color, important, GetCurrentOrigin());
				return OK;
			case COLOR_NAMED:
				prop_list->AddColorDeclL(prop, color, important, GetCurrentOrigin());
				return OK;
#ifdef SKIN_SUPPORT
			case COLOR_SKIN:
				prop_list->AddDeclL(prop, skin_color, important, GetCurrentOrigin(), CSS_string_decl::StringDeclSkin, FALSE);
				return OK;
#endif // SKIN_SUPPORT
			case COLOR_INVALID:
				return INVALID;
			}
		}
		return INVALID;

	case CSS_PROPERTY_background_position:
		return SetPositionL(prop_list, important, prop);

	case CSS_PROPERTY_background_repeat:
		return SetBackgroundRepeatL(prop_list, important);

	case CSS_PROPERTY_background_attachment:
	case CSS_PROPERTY_background_origin:
	case CSS_PROPERTY_background_clip:
		return SetBackgroundListL(prop_list, prop, important);

	case CSS_PROPERTY_background:
		return SetBackgroundShorthandL(prop_list, important);

	case CSS_PROPERTY_background_size:
		return SetBackgroundSizeL(prop_list, important);

	case CSS_PROPERTY__o_object_fit:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (CSS_is_object_fit_val(keyword) || keyword == CSS_VALUE_inherit)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY__o_object_position:
		return SetPositionL(prop_list, important, prop);

	case CSS_PROPERTY_border:
	case CSS_PROPERTY_outline:
	case CSS_PROPERTY_column_rule:
		level++;
		// fall-through is intended
	case CSS_PROPERTY_border_top:
	case CSS_PROPERTY_border_left:
	case CSS_PROPERTY_border_right:
	case CSS_PROPERTY_border_bottom:
		level++;
		// fall-through is intended
	case CSS_PROPERTY_outline_style:
	case CSS_PROPERTY_column_rule_style:
	case CSS_PROPERTY_border_top_style:
	case CSS_PROPERTY_border_left_style:
	case CSS_PROPERTY_border_right_style:
	case CSS_PROPERTY_border_bottom_style:
	case CSS_PROPERTY_outline_width:
	case CSS_PROPERTY_column_rule_width:
	case CSS_PROPERTY_border_top_width:
	case CSS_PROPERTY_border_left_width:
	case CSS_PROPERTY_border_right_width:
	case CSS_PROPERTY_border_bottom_width:
	case CSS_PROPERTY_outline_offset:
		{
			CSSValue style_value = CSS_VALUE_UNSPECIFIED;
			CSSValue invert_keyword = CSS_VALUE_none;

			COLORREF col = USE_DEFAULT_COLOR;
			BOOL set_color = FALSE;

			float width = 0.0f;
			int width_type = CSS_DIM_TYPE;
			BOOL set_width = FALSE;
			BOOL set_keyword_width = TRUE;
			CSSValue width_keyword = CSS_VALUE_medium;
			BOOL is_color_keyword = FALSE;
			CSSValue transparent_keyword = CSS_VALUE_none;

			while (i < m_val_array.GetCount())
			{
				if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (keyword < 0)
					{
						if (!level)
							return INVALID;

						col = m_input_buffer->GetNamedColorIndex(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
						if (col != USE_DEFAULT_COLOR)
							set_color = TRUE;
						else if (m_hld_prof && !m_hld_prof->IsInStrictMode())
						{
							col = m_input_buffer->GetNamedColorIndex(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
							if (col == USE_DEFAULT_COLOR)
								col = m_input_buffer->GetColor(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
							if (col == USE_DEFAULT_COLOR)
								return INVALID;
							set_color = TRUE;
						}
						else
							return INVALID;
					}
					else if (keyword == CSS_VALUE_inherit && m_val_array.GetCount() == 1)
					{
						if (!level)
						{
							prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
						}
						else
						{
							if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_top)
							{
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_top_color, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_top_style, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_top_width, keyword, important, GetCurrentOrigin());
							}
							if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_left)
							{
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_left_color, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_left_style, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_left_width, keyword, important, GetCurrentOrigin());
							}
							if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_right)
							{
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_right_color, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_right_style, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_right_width, keyword, important, GetCurrentOrigin());
							}
							if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_bottom)
							{
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_bottom_color, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_bottom_style, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_border_bottom_width, keyword, important, GetCurrentOrigin());
							}
							if (prop == CSS_PROPERTY_outline)
							{
								prop_list->AddTypeDeclL(CSS_PROPERTY_outline_color, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_outline_style, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_outline_width, keyword, important, GetCurrentOrigin());
							}
							if (prop == CSS_PROPERTY_column_rule)
							{
								prop_list->AddTypeDeclL(CSS_PROPERTY_column_rule_color, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_column_rule_style, keyword, important, GetCurrentOrigin());
								prop_list->AddTypeDeclL(CSS_PROPERTY_column_rule_width, keyword, important, GetCurrentOrigin());
							}
						}
						return OK;
					}
					else if (CSS_is_border_style_val(keyword) || keyword == CSS_VALUE_none)
					{
						if (style_value || (prop == CSS_PROPERTY_outline_style && keyword == CSS_VALUE_hidden))
							return INVALID;

						if (!level && prop != CSS_PROPERTY_border_top_style && prop != CSS_PROPERTY_border_left_style &&
							prop != CSS_PROPERTY_border_right_style && prop != CSS_PROPERTY_border_bottom_style &&
							prop != CSS_PROPERTY_outline_style && prop != CSS_PROPERTY_column_rule_style)
							return INVALID;

						style_value = keyword;
					}
					else if (CSS_is_color_val(keyword))
					{
						if (set_color || !level)
							return INVALID;

						if (CSS_is_ui_color_val(keyword))
							col = keyword | CSS_COLOR_KEYWORD_TYPE_ui_color;
						else
							col = m_input_buffer->GetNamedColorIndex(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);

						set_color = TRUE;
						is_color_keyword = TRUE;
					}
					else if (CSS_is_border_width_val(keyword) && prop != CSS_PROPERTY_outline_offset)
					{
						if (set_width)
							return INVALID;

						set_width = TRUE;
						set_keyword_width = TRUE;
						width_keyword = keyword;
					}
					else if (keyword == CSS_VALUE_invert)
					{
						if (set_color || !level)
							return INVALID;

						set_color = TRUE;
						invert_keyword = keyword;
					}
					else if (keyword == CSS_VALUE_transparent || keyword == CSS_VALUE_currentColor)
					{
						if (set_color || !level)
							return INVALID;

						set_color = TRUE;
						transparent_keyword = keyword;
					}
					else
						return INVALID;
				}
				else if (m_val_array[i].token == CSS_FUNCTION_RGB || SupportsAlpha() && m_val_array[i].token == CSS_FUNCTION_RGBA)
				{
					// color
					if (set_color || !level)
						return INVALID;
					col = m_val_array[i].value.color;
					set_color = TRUE;
				}
				else if (m_val_array[i].token == CSS_HASH)
				{
					col = m_input_buffer->GetColor(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (set_color || !level || col == USE_DEFAULT_COLOR)
						return INVALID;
					set_color = TRUE;
				}
				else if (m_val_array[i].token == CSS_DIMEN)
				{
					// hack for supporting hex-colors without #
					if (set_color || !level || !m_hld_prof || m_hld_prof->IsInStrictMode())
						return INVALID;

					if (m_val_array[i].value.number.str_len == 3 || m_val_array[i].value.number.str_len == 6)
						col = m_input_buffer->GetColor(m_val_array[i].value.number.start_pos, m_val_array[i].value.number.str_len);
					if (col == USE_DEFAULT_COLOR)
						return INVALID;
					set_color = TRUE;
				}
				else if (CSS_is_length_number_ext(m_val_array[i].token) || m_val_array[i].token == CSS_NUMBER)
				{
					width = float(m_val_array[i].value.number.number);
					width_type = CSS_get_number_ext(m_val_array[i].token);
					if (set_width || width < 0 && prop != CSS_PROPERTY_outline_offset || width > 0 && width_type == CSS_NUMBER && (!m_hld_prof || m_hld_prof->IsInStrictMode()))
						return INVALID;
					//units are required if not 0
					if (width_type == CSS_NUMBER)
						width_type = CSS_PX;
					set_width = TRUE;
					set_keyword_width = FALSE;
				}
				else
					return INVALID;

				i++;
			}

			if (set_width && !level && prop != CSS_PROPERTY_border_top_width && prop != CSS_PROPERTY_border_left_width &&
				prop != CSS_PROPERTY_border_right_width && prop != CSS_PROPERTY_border_bottom_width &&
				prop != CSS_PROPERTY_outline_width && prop != CSS_PROPERTY_outline_offset &&
				prop != CSS_PROPERTY_column_rule_width)
				return INVALID;

			// Don't return INVALID below this point for these cases.

			if (set_color || level)
			{
				if (invert_keyword != CSS_VALUE_none)
				{
					prop_list->AddTypeDeclL(CSS_PROPERTY_outline_color, invert_keyword, important, GetCurrentOrigin());
				}
				else if (transparent_keyword != CSS_VALUE_none || !is_color_keyword && col == USE_DEFAULT_COLOR)
				{
					if (transparent_keyword == CSS_VALUE_none)
						transparent_keyword = CSS_VALUE_currentColor;

					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_top)
						prop_list->AddTypeDeclL(CSS_PROPERTY_border_top_color, transparent_keyword, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_left)
						prop_list->AddTypeDeclL(CSS_PROPERTY_border_left_color, transparent_keyword, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_right)
						prop_list->AddTypeDeclL(CSS_PROPERTY_border_right_color, transparent_keyword, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_bottom)
						prop_list->AddTypeDeclL(CSS_PROPERTY_border_bottom_color, transparent_keyword, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_outline)
						prop_list->AddTypeDeclL(CSS_PROPERTY_outline_color, transparent_keyword, important, GetCurrentOrigin());
				}
				else if (is_color_keyword)
				{
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_top)
						prop_list->AddColorDeclL(CSS_PROPERTY_border_top_color, col, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_left)
						prop_list->AddColorDeclL(CSS_PROPERTY_border_left_color, col, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_right)
						prop_list->AddColorDeclL(CSS_PROPERTY_border_right_color, col, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_bottom)
						prop_list->AddColorDeclL(CSS_PROPERTY_border_bottom_color, col, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_outline)
						prop_list->AddColorDeclL(CSS_PROPERTY_outline_color, col, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_column_rule)
						prop_list->AddColorDeclL(CSS_PROPERTY_column_rule_color, col, important, GetCurrentOrigin());
				}
				else
				{
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_top)
						prop_list->AddLongDeclL(CSS_PROPERTY_border_top_color, (long)col, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_left)
						prop_list->AddLongDeclL(CSS_PROPERTY_border_left_color, (long)col, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_right)
						prop_list->AddLongDeclL(CSS_PROPERTY_border_right_color, (long)col, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_bottom)
						prop_list->AddLongDeclL(CSS_PROPERTY_border_bottom_color, (long)col, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_outline)
						prop_list->AddLongDeclL(CSS_PROPERTY_outline_color, (long)col, important, GetCurrentOrigin());
					if (prop == CSS_PROPERTY_column_rule)
						prop_list->AddLongDeclL(CSS_PROPERTY_column_rule_color, (long)col, important, GetCurrentOrigin());
				}
			}

			if (set_width || level)
			{
				if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_top || prop == CSS_PROPERTY_border_top_width)
				{
					if (set_keyword_width)
						prop_list->AddTypeDeclL(CSS_PROPERTY_border_top_width, width_keyword, important, GetCurrentOrigin());
					else
						prop_list->AddDeclL(CSS_PROPERTY_border_top_width, width, width_type, important, GetCurrentOrigin());
				}
				if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_left || prop == CSS_PROPERTY_border_left_width)
				{
					if (set_keyword_width)
						prop_list->AddTypeDeclL(CSS_PROPERTY_border_left_width, width_keyword, important, GetCurrentOrigin());
					else
						prop_list->AddDeclL(CSS_PROPERTY_border_left_width, width, width_type, important, GetCurrentOrigin());
				}
				if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_right || prop == CSS_PROPERTY_border_right_width)
				{
					if (set_keyword_width)
						prop_list->AddTypeDeclL(CSS_PROPERTY_border_right_width, width_keyword, important, GetCurrentOrigin());
					else
						prop_list->AddDeclL(CSS_PROPERTY_border_right_width, width, width_type, important, GetCurrentOrigin());
				}
				if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_bottom || prop == CSS_PROPERTY_border_bottom_width)
				{
					if (set_keyword_width)
						prop_list->AddTypeDeclL(CSS_PROPERTY_border_bottom_width, width_keyword, important, GetCurrentOrigin());
					else
						prop_list->AddDeclL(CSS_PROPERTY_border_bottom_width, width, width_type, important, GetCurrentOrigin());
				}
				if (prop == CSS_PROPERTY_outline || prop == CSS_PROPERTY_outline_width)
				{
					if (set_keyword_width)
						prop_list->AddTypeDeclL(CSS_PROPERTY_outline_width, width_keyword, important, GetCurrentOrigin());
					else
						prop_list->AddDeclL(CSS_PROPERTY_outline_width, width, width_type, important, GetCurrentOrigin());
				}
				else if (prop == CSS_PROPERTY_outline_offset)
					prop_list->AddDeclL(CSS_PROPERTY_outline_offset, width, width_type, important, GetCurrentOrigin());
				if (prop == CSS_PROPERTY_column_rule || prop == CSS_PROPERTY_column_rule_width)
				{
					if (set_keyword_width)
						prop_list->AddTypeDeclL(CSS_PROPERTY_column_rule_width, width_keyword, important, GetCurrentOrigin());
					else
						prop_list->AddDeclL(CSS_PROPERTY_column_rule_width, width, width_type, important, GetCurrentOrigin());
				}
			}

			if (!style_value && level)
				style_value = CSS_VALUE_none;

			if (style_value)
			{
				if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_top || prop == CSS_PROPERTY_border_top_style)
					prop_list->AddTypeDeclL(CSS_PROPERTY_border_top_style, style_value, important, GetCurrentOrigin());
				if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_left || prop == CSS_PROPERTY_border_left_style)
					prop_list->AddTypeDeclL(CSS_PROPERTY_border_left_style, style_value, important, GetCurrentOrigin());
				if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_right || prop == CSS_PROPERTY_border_right_style)
					prop_list->AddTypeDeclL(CSS_PROPERTY_border_right_style, style_value, important, GetCurrentOrigin());
				if (prop == CSS_PROPERTY_border || prop == CSS_PROPERTY_border_bottom || prop == CSS_PROPERTY_border_bottom_style)
					prop_list->AddTypeDeclL(CSS_PROPERTY_border_bottom_style, style_value, important, GetCurrentOrigin());
				if (prop == CSS_PROPERTY_outline || prop == CSS_PROPERTY_outline_style)
					prop_list->AddTypeDeclL(CSS_PROPERTY_outline_style, style_value, important, GetCurrentOrigin());
				if (prop == CSS_PROPERTY_column_rule || prop == CSS_PROPERTY_column_rule_style)
					prop_list->AddTypeDeclL(CSS_PROPERTY_column_rule_style, style_value, important, GetCurrentOrigin());
			}
		}
		break;

	case CSS_PROPERTY_box_sizing:
		{
			if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				switch (keyword)
				{
				case CSS_VALUE_content_box:
				case CSS_VALUE_border_box:
				case CSS_VALUE_inherit:
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
					break;

				default:
					return INVALID;
				}
			}
			else
				return INVALID;
		}
		break;

#ifdef SVG_SUPPORT
	case CSS_PROPERTY_stop_opacity:
	case CSS_PROPERTY_stroke_opacity:
	case CSS_PROPERTY_fill_opacity:
	case CSS_PROPERTY_flood_opacity:
	case CSS_PROPERTY_viewport_fill_opacity:
	case CSS_PROPERTY_solid_opacity:
#endif
	case CSS_PROPERTY_opacity:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				}
				else
					return INVALID;
			}
			else if (m_val_array[0].token == CSS_NUMBER)
			{
				float opacity = float(m_val_array[0].value.number.number);
				if (opacity < 0.0f)
					opacity = 0.0f;
				else if (opacity > 1.0f)
					opacity = 1.0f;
				prop_list->AddDeclL(prop, opacity, CSS_NUMBER, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY__wap_accesskey:
		{
			int gi = 0;
			CSS_generic_value gen_arr[CSS_MAX_ARR_SIZE];
			CSS_anchored_heap_arrays<uni_char> strings;
			ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);

			while (i < m_val_array.GetCount() && gi < CSS_MAX_ARR_SIZE)
			{
				if (m_val_array[i].token == CSS_IDENT)
				{
					if (m_val_array.GetCount() == 1)
					{
						keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
						if (keyword == CSS_VALUE_none || keyword == CSS_VALUE_inherit)
						{
							prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
							return OK;
						}
					}

					uni_char* access_key = m_input_buffer->GetString(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					strings.AddArrayL(access_key);
					// *** Handle accesskey parsing here ***
					gen_arr[gi].value_type = CSS_STRING_LITERAL;
					gen_arr[gi++].value.string = access_key;
				}
				else if (m_val_array[i].token == CSS_NUMBER)
				{
					int start_pos = m_val_array[i].value.number.start_pos;
					int str_len = 0;
					float number_value = float(m_val_array[i].value.number.number);
					BOOL valid = number_value >= 0.0
						&& number_value < 10.0
						&& number_value == op_floor(number_value);

					while (i+1 < m_val_array.GetCount() && (m_val_array[i+1].token == CSS_NUMBER) && (m_val_array[i+1].value.number.number < 0))
					{
						if (m_val_array[i+1].value.number.number <= -10.0
							|| m_val_array[i+1].value.number.number != op_floor(m_val_array[i+1].value.number.number))
							valid = FALSE;

						i++;
					}

					if (valid)
					{
						str_len = m_val_array[i].value.number.start_pos - start_pos + 1;

						uni_char* access_key = m_input_buffer->GetString(start_pos, str_len);
						strings.AddArrayL(access_key);
						// *** Handle accesskey parsing here ***
						gen_arr[gi].value_type = CSS_STRING_LITERAL;
						gen_arr[gi++].value.string = access_key;
					}
				}
				else if (m_val_array[i].token == CSS_HASH_CHAR || m_val_array[i].token == CSS_ASTERISK_CHAR)
				{
					uni_char* access_key = OP_NEWA_L(uni_char, 2);
					access_key[0] = m_val_array[i].token;
					access_key[1] = 0;
					strings.AddArrayL(access_key);
					gen_arr[gi].value_type = CSS_STRING_LITERAL;
					gen_arr[gi++].value.string = access_key;
				}
				else if (m_val_array[i].token == CSS_COMMA)
					gen_arr[gi++].value_type = CSS_COMMA;

				i++;
			}

			if (gi > 0)
			{
				prop_list->AddDeclL(prop, gen_arr, gi, 0, important, GetCurrentOrigin());
				return OK;
			}
			else
				return INVALID;
		}
		break;

	case CSS_PROPERTY__wap_input_format:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_STRING_LITERAL)
		{
			uni_char* input_format = m_input_buffer->GetString(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (!input_format)
				LEAVE(OpStatus::ERR_NO_MEMORY);
			prop_list->AddDeclL(prop, input_format, important, GetCurrentOrigin(), CSS_string_decl::StringDeclString, m_hld_prof == NULL);
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY__wap_input_required:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_true || keyword == CSS_VALUE_false)
			{
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY__wap_marquee_dir:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_rtl || keyword == CSS_VALUE_ltr)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				}
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY__wap_marquee_loop:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_infinite)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				}
				else
					return INVALID;
			}
			else if (m_val_array[0].token == CSS_NUMBER)
			{
				prop_list->AddDeclL(prop, float(m_val_array[i].value.number.number), CSS_NUMBER, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY__wap_marquee_speed:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_slow || keyword == CSS_VALUE_normal || keyword == CSS_VALUE_fast)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				}
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY__wap_marquee_style:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_scroll || keyword == CSS_VALUE_slide || keyword == CSS_VALUE_alternate)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				}
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

#ifdef CSS_MINI_EXTENSIONS
	// -o-mini-fold
	case CSS_PROPERTY__o_mini_fold:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword != CSS_VALUE_none && keyword != CSS_VALUE_folded && keyword != CSS_VALUE_unfolded)
				return INVALID;
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
		}
		else
			return INVALID;
		break;

	// -o-focus-opacity
	case CSS_PROPERTY__o_focus_opacity:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_NUMBER)
		{
			float opacity = static_cast<float>(m_val_array[0].value.number.number);
			if (opacity < 0.0f)
				opacity = 0.0f;
			else if (opacity > 1.0f)
				opacity = 1.0f;
			prop_list->AddDeclL(prop, opacity, CSS_NUMBER, important, GetCurrentOrigin());
		}
		else
			return INVALID;
		break;
#endif // CSS_MINI_EXTENSIONS

#ifdef CSS_CHARACTER_TYPE_SUPPORT
	case CSS_PROPERTY_character_type:
#endif // CSS_CHARACTER_TYPE_SUPPORT
	case CSS_PROPERTY_input_format:
		if (m_val_array.GetCount() == 1 && (m_val_array[0].token == CSS_STRING_LITERAL || m_val_array[0].token == CSS_IDENT))
		{
			uni_char* input_format = m_input_buffer->GetString(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (!input_format)
				LEAVE(OpStatus::ERR_NO_MEMORY);
			prop_list->AddDeclL(prop, input_format, important, GetCurrentOrigin(), CSS_string_decl::StringDeclString, m_hld_prof == NULL);
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_text_overflow:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			CSSValue val = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (val == CSS_VALUE_clip || val == CSS_VALUE_ellipsis || val == CSS_VALUE__o_ellipsis_lastline || val == CSS_VALUE_inherit)
			{
				prop_list->AddTypeDeclL(prop, val, important, GetCurrentOrigin());
				break;
			}
		}
		return INVALID;

	case CSS_PROPERTY_overflow_wrap:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			CSSValue val = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);

			if (val == CSS_VALUE_break_word || val == CSS_VALUE_normal || val == CSS_VALUE_inherit)
				prop_list->AddTypeDeclL(prop, val, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;

		break;

	case CSS_PROPERTY_nav_up:
	case CSS_PROPERTY_nav_down:
	case CSS_PROPERTY_nav_left:
	case CSS_PROPERTY_nav_right:
		{
			if (m_val_array.GetCount() > 0)
			{
				if (m_val_array[0].token == CSS_IDENT && m_val_array.GetCount() == 1)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
					if (keyword == CSS_VALUE_auto || keyword == CSS_VALUE_inherit)
						prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
					else
						return INVALID;
				}
				else if (m_val_array[0].token == CSS_HASH && m_val_array.GetCount() <= 2)
				{
					CSS_anchored_heap_arrays<uni_char> strings;
					ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);
					CSS_generic_value gen_arr[2];
					if (m_val_array.GetCount() == 2)
					{
						if (m_val_array[1].token == CSS_STRING_LITERAL)
						{
							gen_arr[1].value_type = CSS_STRING_LITERAL;
							gen_arr[1].value.string = m_input_buffer->GetString(m_val_array[1].value.str.start_pos, m_val_array[1].value.str.str_len);
							if (!gen_arr[1].value.string)
								LEAVE(OpStatus::ERR_NO_MEMORY);
							strings.AddArrayL(gen_arr[1].value.string);
						}
						else if (m_val_array[1].token == CSS_IDENT)
						{
							keyword = m_input_buffer->GetValueSymbol(m_val_array[1].value.str.start_pos, m_val_array[1].value.str.str_len);
							if (keyword == CSS_VALUE_current || keyword == CSS_VALUE_root)
							{
								gen_arr[1].value_type = CSS_IDENT;
								gen_arr[1].value.type = keyword;
							}
							else
								return INVALID;
						}
						else
							return INVALID;
					}

					gen_arr[0].value_type = CSS_HASH;
					gen_arr[0].value.string = m_input_buffer->GetString(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
					if (!gen_arr[0].value.string)
						LEAVE(OpStatus::ERR_NO_MEMORY);

					strings.AddArrayL(gen_arr[0].value.string);

					prop_list->AddDeclL(prop, gen_arr, m_val_array.GetCount(), 0, important, GetCurrentOrigin());
				}
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		break;

	case CSS_PROPERTY_nav_index:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_auto || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			}
			else if (m_val_array[0].token == CSS_NUMBER && m_val_array[0].value.number.number > 0)
				prop_list->AddDeclL(prop, float(m_val_array[0].value.number.number), CSS_NUMBER, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY__o_table_baseline:
		if (m_val_array.GetCount() != 1)
			return INVALID;
		if (m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else if (m_val_array[0].token == CSS_NUMBER)
			prop_list->AddLongDeclL(prop, (long)m_val_array[0].value.number.number, important, GetCurrentOrigin());
		else
			return INVALID;
		break;

	case CSS_PROPERTY__o_tab_size:
		if (m_val_array.GetCount() != 1)
			return INVALID;
		if (m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_auto)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else if (CSS_is_number(m_val_array[0].token) && CSS_get_number_ext(m_val_array[0].token) == CSS_NUMBER)
		{
			if (m_val_array[0].value.number.number < 0)
				return INVALID;

			int num;
			if (m_val_array[0].value.number.number >= INT_MAX)
				num = INT_MAX;
			else
				num = int(m_val_array[0].value.number.number);
			prop_list->AddLongDeclL(prop, (long)num, important, GetCurrentOrigin());
		}
		else
			return INVALID;
		break;

#ifdef SVG_SUPPORT
	case CSS_PROPERTY_alignment_baseline:
	case CSS_PROPERTY_clip_rule:
	case CSS_PROPERTY_fill_rule:
	case CSS_PROPERTY_color_interpolation:
	case CSS_PROPERTY_color_interpolation_filters:
	case CSS_PROPERTY_color_rendering:
	case CSS_PROPERTY_image_rendering:
	case CSS_PROPERTY_dominant_baseline:
	case CSS_PROPERTY_pointer_events:
	case CSS_PROPERTY_shape_rendering:
	case CSS_PROPERTY_stroke_linejoin:
	case CSS_PROPERTY_stroke_linecap:
	case CSS_PROPERTY_text_anchor:
	case CSS_PROPERTY_text_rendering:
	case CSS_PROPERTY_vector_effect:
	case CSS_PROPERTY_writing_mode:
	case CSS_PROPERTY_display_align:
	case CSS_PROPERTY_buffered_rendering:
		if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);

			BOOL goodKeyword = (keyword == CSS_VALUE_inherit);
			if (!goodKeyword)
			{
				switch (prop)
				{
				case CSS_PROPERTY_buffered_rendering:
					goodKeyword = CSS_is_bufferedrendering_val(keyword);
					break;
				case CSS_PROPERTY_alignment_baseline:
					goodKeyword = CSS_is_alignmentbaseline_val(keyword);
					break;
				case CSS_PROPERTY_clip_rule: // fall-through is intended
				case CSS_PROPERTY_fill_rule:
					goodKeyword = CSS_is_fillrule_val(keyword);
					break;
				case CSS_PROPERTY_color_interpolation: // fall-through is intended
				case CSS_PROPERTY_color_interpolation_filters:
					goodKeyword = CSS_is_colorinterpolation_val(keyword);
					break;
				case CSS_PROPERTY_color_rendering:
					goodKeyword = CSS_is_colorrendering_val(keyword);
					break;
				case CSS_PROPERTY_image_rendering:
					goodKeyword = CSS_is_imagerendering_val(keyword);
					break;
				case CSS_PROPERTY_dominant_baseline:
					goodKeyword = CSS_is_dominantbaseline_val(keyword);
					break;
				case CSS_PROPERTY_pointer_events:
					goodKeyword = CSS_is_pointerevents_val(keyword);
					break;
				case CSS_PROPERTY_shape_rendering:
					goodKeyword = CSS_is_shaperendering_val(keyword);
					break;
				case CSS_PROPERTY_stroke_linecap:
					goodKeyword = CSS_is_strokelinecap_val(keyword);
					break;
				case CSS_PROPERTY_stroke_linejoin:
					goodKeyword = CSS_is_strokelinejoin_val(keyword);
					break;
				case CSS_PROPERTY_text_anchor:
					goodKeyword = CSS_is_textanchor_val(keyword);
					break;
				case CSS_PROPERTY_text_rendering:
					goodKeyword = CSS_is_textrendering_val(keyword);
					break;
				case CSS_PROPERTY_vector_effect:
					goodKeyword = CSS_is_vectoreffect_val(keyword);
					break;
				case CSS_PROPERTY_writing_mode:
					goodKeyword = CSS_is_writingmode_val(keyword);
					break;
				case CSS_PROPERTY_display_align:
					goodKeyword = CSS_is_displayalign_val(keyword);
					break;
				}
			}

			if (goodKeyword)
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		else
			return INVALID;
		break;
	case CSS_PROPERTY_color_profile:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (CSS_VALUE_sRGB == keyword || CSS_VALUE_auto == keyword || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());

				return OK;
			}
			else if (m_val_array[0].token == CSS_FUNCTION_URL)
			{
				URL url = m_input_buffer->GetURLL(m_base_url,
												m_val_array[0].value.str.start_pos,
												m_val_array[0].value.str.str_len);
				ANCHOR(URL, url);
				const uni_char* url_name = url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI).CStr();

				if (url_name)
					prop_list->AddDeclL(prop, url_name, uni_strlen(url_name), important, GetCurrentOrigin(), CSS_string_decl::StringDeclUrl, m_hld_prof == NULL);
				else
					return INVALID;

				return OK;
			}
			else if (m_val_array[0].token == CSS_STRING_LITERAL)
			{
				uni_char* str = m_input_buffer->GetString(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
				if (str)
				{
					prop_list->AddDeclL(prop, str, important, GetCurrentOrigin(), CSS_string_decl::StringDeclString, m_hld_prof == NULL);
				}
				else
					LEAVE(OpStatus::ERR_NO_MEMORY);

				return OK;
			}

			return INVALID;
		}
		else
			return INVALID;
		break;
	case CSS_PROPERTY_glyph_orientation_horizontal: // fall-through is intended
	case CSS_PROPERTY_glyph_orientation_vertical:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if ((prop == CSS_PROPERTY_glyph_orientation_vertical && CSS_VALUE_auto == keyword) || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else if (m_val_array[0].token == CSS_DEG ||
						m_val_array[0].token == CSS_RAD ||
						m_val_array[0].token == CSS_GRAD ||
						m_val_array[0].token == CSS_NUMBER)
			{
				prop_list->AddDeclL(prop, float(m_val_array[0].value.number.number), m_val_array[0].token, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;
	case CSS_PROPERTY_baseline_shift:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (CSS_is_baselineshift_val(keyword) || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else if (CSS_is_number(m_val_array[0].token))
			{
				float length = float(m_val_array[0].value.number.number);
				int length_type = CSS_get_number_ext(m_val_array[0].token);

				if (length && !CSS_is_length_number_ext(length_type) && (length_type != CSS_PERCENTAGE))
				{
					length_type = CSS_PX;
				}
				prop_list->AddDeclL(prop, length, length_type, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_clip_path:
	case CSS_PROPERTY_filter:
	case CSS_PROPERTY_marker:
	case CSS_PROPERTY_marker_end:
	case CSS_PROPERTY_marker_mid:
	case CSS_PROPERTY_marker_start:
	case CSS_PROPERTY_mask:
		// <uri> | none | inherit
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_none || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else if (m_val_array[0].token == CSS_FUNCTION_URL)
			{
				URL url = m_input_buffer->GetURLL(m_base_url,
												m_val_array[0].value.str.start_pos,
												m_val_array[0].value.str.str_len);
				ANCHOR(URL, url);
				const uni_char* url_name = url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI).CStr();

				if (url_name)
					prop_list->AddDeclL(prop, url_name, uni_strlen(url_name), important, GetCurrentOrigin(), CSS_string_decl::StringDeclUrl, m_hld_prof == NULL);
				else
					return INVALID;

				return OK;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;
	case CSS_PROPERTY_enable_background:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_accumulate || keyword == CSS_VALUE_new || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
		}
		else if (m_val_array.GetCount() == 5)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword != CSS_VALUE_new)
				{
					return INVALID;
				}
			}
			else
				return INVALID;

			int gi;
			CSS_generic_value gen_arr[CSS_MAX_ARR_SIZE];

			for (gi = 0; gi < m_val_array.GetCount(); gi++)
			{
				if (CSS_is_number(m_val_array[gi].token))
				{
					float length = float(m_val_array[gi].value.number.number);

					int length_type = CSS_get_number_ext(m_val_array[gi].token);

					if (length && !CSS_is_length_number_ext(length_type) && (length_type != CSS_PERCENTAGE))
					{
						length_type = CSS_PX;
					}

					gen_arr[gi].value_type = length_type;
					gen_arr[gi].value.real = length;
				}
				else
				{
					return INVALID;
				}
			}

			prop_list->AddDeclL(prop, gen_arr, gi, 0, important, GetCurrentOrigin());
		}
		else
			return INVALID;
		break;

	case CSS_PROPERTY_kerning:
	case CSS_PROPERTY_stroke_width:
	case CSS_PROPERTY_stroke_miterlimit:
	case CSS_PROPERTY_stroke_dashoffset:
	case CSS_PROPERTY_audio_level:
	case CSS_PROPERTY_line_increment:
		if (m_val_array.GetCount() == 1)
		{
			if (CSS_is_number(m_val_array[0].token))
			{
				float length = float(m_val_array[0].value.number.number);
				int length_type = CSS_get_number_ext(m_val_array[0].token);

				if (length && !CSS_is_length_number_ext(length_type) && (length_type != CSS_PERCENTAGE))
				{
					length_type = CSS_PX;
				}
				prop_list->AddDeclL(prop, length, length_type, important, GetCurrentOrigin());
			}
			else if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit ||
					((prop == CSS_PROPERTY_kerning || prop == CSS_PROPERTY_line_increment) && keyword == CSS_VALUE_auto))
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		break;

	case CSS_PROPERTY_stroke_dasharray:
		if (m_val_array.GetCount() > 0)
		{
			if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_none || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
			{
				int gi = 0;
				CSS_generic_value gen_arr[CSS_MAX_ARR_SIZE];

				for (int i = 0; i < m_val_array.GetCount(); i++)
				{
					if (CSS_is_number(m_val_array[i].token))
					{
						float length = float(m_val_array[i].value.number.number);

						int length_type = CSS_get_number_ext(m_val_array[i].token);

						if (length && !CSS_is_length_number_ext(length_type) && (length_type != CSS_PERCENTAGE))
						{
							length_type = CSS_PX;
						}

						gen_arr[gi].value_type = length_type;
						gen_arr[gi].value.real = length;
						gi++;
					}
					else if (m_val_array[i].token != CSS_COMMA)
					{
						return INVALID;
					}
				}

				prop_list->AddDeclL(prop, gen_arr, gi, 0, important, GetCurrentOrigin());
			}
		}
		else
			return INVALID;
		break;
#endif // SVG_SUPPORT
#ifdef GADGET_SUPPORT
	case CSS_PROPERTY__apple_dashboard_region:
		if (m_val_array.GetCount() == 7
			&& m_val_array[0].token == CSS_FUNCTION_DASHBOARD_REGION
			&& m_val_array[5].token == CSS_IDENT
			&& m_input_buffer->GetValueSymbol(m_val_array[5].value.str.start_pos, m_val_array[5].value.str.str_len) == CSS_VALUE_control
			&& m_val_array[6].token == CSS_IDENT)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[6].value.str.start_pos, m_val_array[6].value.str.str_len);
			if (keyword == CSS_VALUE_circle || keyword == CSS_VALUE_rectangle)
			{
				CSS_generic_value gen_arr[5];
				int gi=0;
				gen_arr[gi].value_type = CSS_IDENT;
				gen_arr[gi++].value.type = keyword;
				for (int i = 1; i < 5; i++)
				{
					float length = float(m_val_array[i].value.number.number);
					int length_type = CSS_get_number_ext(m_val_array[i].token);
					if (m_val_array[i].token == CSS_NUMBER && length == 0
						|| length >= 0 && CSS_is_length_number_ext(length_type))
					{
						if (length_type == CSS_NUMBER)
							length_type = CSS_PX;
						gen_arr[gi].value_type = length_type;
						gen_arr[gi].value.real = length;
						gi++;
					}
					else
						return INVALID;
				}
				prop_list->AddDeclL(prop, gen_arr, 5, 0, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;
#endif // GADGET_SUPPORT

#ifdef CSS_TRANSITIONS
	case CSS_PROPERTY_transition:
		{
			if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit)
				{
					prop_list->AddTypeDeclL(CSS_PROPERTY_transition_delay, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_transition_duration, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_transition_property, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_transition_timing_function, keyword, important, GetCurrentOrigin());
					return OK;
				}
			}

			int i=0;
			int gi=0;
			short prop_arr[CSS_MAX_ARR_SIZE];
			CSSValue property_keyword = CSS_VALUE_UNSPECIFIED;

			CSS_generic_value_list dur_arr;
			ANCHOR(CSS_generic_value_list, dur_arr);

			CSS_generic_value_list del_arr;
			ANCHOR(CSS_generic_value_list, del_arr);

			CSS_generic_value_list time_arr;
			ANCHOR(CSS_generic_value_list, time_arr);

			while (i < m_val_array.GetCount())
			{
				BOOL property_set = FALSE;
				BOOL delay_set = FALSE;
				BOOL duration_set = FALSE;
				BOOL timing_set = FALSE;
				int start_i = i;
				while (i < m_val_array.GetCount() && m_val_array[i].token != CSS_COMMA)
				{
					short tok = m_val_array[i].token;
					if (tok == CSS_SECOND || tok == CSS_NUMBER && m_val_array[i].value.number.number == 0)
					{
						if (!duration_set)
						{
							duration_set = TRUE;
							dur_arr.PushNumberL(CSS_SECOND, float(m_val_array[i].value.number.number));
						}
						else if (!delay_set)
						{
							delay_set = TRUE;
							del_arr.PushNumberL(CSS_SECOND, float(m_val_array[i].value.number.number));
						}
						else
							return INVALID;
					}
					else if (tok == CSS_IDENT)
					{
						keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
						if (CSS_is_timing_func_val(keyword) && !timing_set)
						{
							timing_set = TRUE;
							SetTimingKeywordL(keyword, time_arr);
						}
						else if (!property_set && (keyword == CSS_VALUE_all || keyword == CSS_VALUE_none))
						{
							// Not allowed to set property to "all" or "none" after other properties
							if (gi > 0)
								return INVALID;

							property_keyword = keyword;
							property_set = TRUE;
							prop_arr[0] = -1;
						}
						else if (!property_set)
						{
							// Not allowed to set property after "all" or "none"
							if (gi > 0 && prop_arr[0] == -1)
								return INVALID;

							short val_prop = m_input_buffer->GetPropertySymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
							if (val_prop != -1)
							{
								property_set = TRUE;
								if (gi < CSS_MAX_ARR_SIZE)
								{
									prop_arr[gi] = val_prop;
								}
							}
							else
								return INVALID;
						}
						else
							return INVALID;
					}
					else if (tok == CSS_FUNCTION_CUBIC_BEZ && !timing_set)
					{
						timing_set = TRUE;
						if (i+4 >= m_val_array.GetCount())
						{
							OP_ASSERT(FALSE); // If this happens, the grammar outputs wrong values for cubic-bezier functions.
							return INVALID;
						}
						for (int j=0; j<4; j++)
						{
							++i;
							OP_ASSERT(m_val_array[i].token == CSS_NUMBER);

							float param = float(m_val_array[i].value.number.number);
							if ((j % 2) == 0 || param >= 0 && param <= 1.0) // even values are y-values and can exceed [0,1]. x-values cannot.
								time_arr.PushNumberL(CSS_NUMBER, param);
							else
								return INVALID;
						}
					}
					else if (!timing_set && tok == CSS_FUNCTION_STEPS && m_val_array[i].value.steps.steps > 0)
					{
						timing_set = TRUE;
						time_arr.PushIntegerL(CSS_INT_NUMBER, m_val_array[i].value.steps.steps);
						time_arr.PushIdentL(m_val_array[i].value.steps.start ? CSS_VALUE_start : CSS_VALUE_end);
						time_arr.PushValueTypeL(CSS_IDENT); // dummy
						time_arr.PushValueTypeL(CSS_IDENT); // dummy
					}
					else
						return INVALID;
					i++;
				}
				if (start_i == i || ++i == m_val_array.GetCount())
					return INVALID;

				if (gi < CSS_MAX_ARR_SIZE)
				{
					if (!duration_set)
						dur_arr.PushNumberL(CSS_NUMBER, 0);

					if (!delay_set)
						del_arr.PushNumberL(CSS_NUMBER, 0);

					if (!property_set)
					{
						if (gi > 0 || i < m_val_array.GetCount())
							return INVALID;

						property_keyword = CSS_VALUE_all;
						prop_arr[gi] = -1;
					}

					gi++;
				}

				if (!timing_set)
				{
					time_arr.PushNumberL(CSS_NUMBER, 0.25f);
					time_arr.PushNumberL(CSS_NUMBER, 0.1f);
					time_arr.PushNumberL(CSS_NUMBER, 0.25f);
					time_arr.PushNumberL(CSS_NUMBER, 1.0f);
				}
			}
			prop_list->AddDeclL(CSS_PROPERTY_transition_delay, del_arr, 0, important, GetCurrentOrigin());
			prop_list->AddDeclL(CSS_PROPERTY_transition_duration, dur_arr, 0, important, GetCurrentOrigin());
			if (gi == 1 && prop_arr[0] == -1)
			{
				OP_ASSERT(property_keyword != CSS_VALUE_UNSPECIFIED);
				prop_list->AddTypeDeclL(CSS_PROPERTY_transition_property, property_keyword, important, GetCurrentOrigin());
			}
			else
				prop_list->AddDeclL(CSS_PROPERTY_transition_property, prop_arr, gi, important, GetCurrentOrigin());
			prop_list->AddDeclL(CSS_PROPERTY_transition_timing_function, time_arr, 0, important, GetCurrentOrigin());
		}
		break;

	case CSS_PROPERTY_transition_delay:
	case CSS_PROPERTY_transition_duration:
#ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_delay:
	case CSS_PROPERTY_animation_duration:
#endif // CSS_ANIMATIONS
		{
			if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
					return OK;
				}
			}

			if (m_val_array.GetCount() > 0 && m_val_array.GetCount()%2 == 1)
			{
				CSS_generic_value gen_arr[CSS_MAX_ARR_SIZE];
				int gi=0;

				for (int i=0; i<m_val_array.GetCount(); i++)
				{
					short tok = m_val_array[i].token;
					if ((i%2) == 1)
					{
						if (tok == CSS_COMMA)
							continue;
						else
							return INVALID;
					}
					else if (tok == CSS_SECOND || tok == CSS_NUMBER && m_val_array[i].value.number.number == 0)
					{
						if (gi < CSS_MAX_ARR_SIZE)
						{
							gen_arr[gi].SetValueType(CSS_SECOND);
							gen_arr[gi++].SetReal(float(m_val_array[i].value.number.number));
						}
					}
					else
						return INVALID;
				}
				prop_list->AddDeclL(prop, gen_arr, gi, 0, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		break;

	case CSS_PROPERTY_transition_property:
		{
			if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
					return OK;
				}
			}

			if (m_val_array.GetCount() > 0 && m_val_array.GetCount()%2 == 1)
			{
				short prop_array[CSS_MAX_ARR_SIZE];
				int ai=0;

				for (int i=0; i<m_val_array.GetCount(); i++)
				{
					short tok = m_val_array[i].token;
					if ((i%2) == 1)
					{
						if (tok == CSS_COMMA)
							continue;
						else
							return INVALID;
					}

					if (tok != CSS_IDENT)
						return INVALID;

					if (i == 0 && m_val_array.GetCount() == 1)
					{
						keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
						if (keyword == CSS_VALUE_all || keyword == CSS_VALUE_none)
						{
							prop_list->AddTypeDeclL(CSS_PROPERTY_transition_property, keyword, important, GetCurrentOrigin());
							return OK;
						}
					}

					short val_prop = m_input_buffer->GetPropertySymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (val_prop == -1)
						return INVALID;
					else if (ai < CSS_MAX_ARR_SIZE)
						prop_array[ai++] = val_prop;
				}
				prop_list->AddDeclL(CSS_PROPERTY_transition_property, prop_array, ai, important, GetCurrentOrigin());
			}
			else
				return INVALID;
		}
		break;

#ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_timing_function:
#endif // CSS_ANIMATIONS
	case CSS_PROPERTY_transition_timing_function:
		{

			if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
					return OK;
				}
			}

			CSS_generic_value_list gen_arr;
			ANCHOR(CSS_generic_value_list, gen_arr);
			BOOL expect_comma = FALSE;
			for (int i=0; i<m_val_array.GetCount(); i++)
			{
				if (expect_comma && m_val_array[i].token == CSS_COMMA)
				{
					expect_comma = FALSE;
					continue;
				}

				if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (CSS_is_timing_func_val(keyword))
						SetTimingKeywordL(keyword, gen_arr);
					else
						return INVALID;
				}
				else if (m_val_array[i].token == CSS_FUNCTION_CUBIC_BEZ)
				{
					OP_ASSERT(i+4 < m_val_array.GetCount());
					i++;
					for (int j=0; j<4; j++, i++)
					{
						OP_ASSERT(m_val_array[i].token == CSS_NUMBER);

						float param = float(m_val_array[i].value.number.number);
						if ((i % 2) == 0 || param >= 0 && param <= 1.0) // even values are y-values and can exceed [0,1]. x-values cannot.
							gen_arr.PushNumberL(CSS_NUMBER, param);
						else
							return INVALID;
					}
				}
				else if (m_val_array[i].token == CSS_FUNCTION_STEPS && m_val_array[i].value.steps.steps > 0)
				{
					gen_arr.PushIntegerL(CSS_INT_NUMBER, m_val_array[i].value.steps.steps);
					gen_arr.PushIdentL(m_val_array[i].value.steps.start ? CSS_VALUE_start : CSS_VALUE_end);
					gen_arr.PushValueTypeL(CSS_IDENT); // dummy
					gen_arr.PushValueTypeL(CSS_IDENT); // dummy
				}
				else
					return INVALID;

				expect_comma = TRUE;
			}

			if (expect_comma)
				prop_list->AddDeclL(prop, gen_arr, 0, important, GetCurrentOrigin());
			else
				return INVALID;
		}
		break;
#endif // CSS_TRANSITIONS
#ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation:
		{
			if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit)
				{
					prop_list->AddTypeDeclL(CSS_PROPERTY_animation_name, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_animation_duration, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_animation_timing_function, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_animation_iteration_count, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_animation_direction, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_animation_delay, keyword, important, GetCurrentOrigin());
					prop_list->AddTypeDeclL(CSS_PROPERTY_animation_fill_mode, keyword, important, GetCurrentOrigin());
					return OK;
				}
			}

			CSS_generic_value_list animation_name;
			ANCHOR(CSS_generic_value_list, animation_name);

			CSS_generic_value_list animation_duration;
			ANCHOR(CSS_generic_value_list, animation_duration);

			CSS_generic_value_list animation_timing;
			ANCHOR(CSS_generic_value_list, animation_timing);

			CSS_generic_value_list animation_iteration_count;
			ANCHOR(CSS_generic_value_list, animation_iteration_count);

			CSS_generic_value_list animation_direction;
			ANCHOR(CSS_generic_value_list, animation_direction);

			CSS_generic_value_list animation_delay;
			ANCHOR(CSS_generic_value_list, animation_delay);

			CSS_generic_value_list animation_fill_mode;
			ANCHOR(CSS_generic_value_list, animation_fill_mode);

			CSS_anchored_heap_arrays<uni_char> strings;
			ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);

			const uni_char* default_name = UNI_L("none");
			int start_i = i;

			while (i < m_val_array.GetCount())
			{
				BOOL name_set = FALSE;
				BOOL delay_set = FALSE;
				BOOL duration_set = FALSE;
				BOOL timing_set = FALSE;
				BOOL direction_set = FALSE;
				BOOL iteration_count_set = FALSE;
				BOOL fill_mode_set = FALSE;

				for (;i < m_val_array.GetCount() && m_val_array[i].token != CSS_COMMA; i++)
				{
					short tok = m_val_array[i].token;

					if (m_val_array[i].token == CSS_IDENT)
					{
						keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos,
																 m_val_array[i].value.str.str_len);

						if (!timing_set && CSS_is_timing_func_val(keyword))
						{
							timing_set = TRUE;
							SetTimingKeywordL(keyword, animation_timing);
						}
						else if (!direction_set && CSS_is_animation_direction_val(keyword))
						{
							direction_set = TRUE;
							animation_direction.PushIdentL(keyword);
						}
						else if (!fill_mode_set && CSS_is_animation_fill_mode_value(keyword))
						{
							fill_mode_set = TRUE;
							animation_fill_mode.PushIdentL(keyword);
						}
						else if (!iteration_count_set && keyword == CSS_VALUE_infinite)
						{
							iteration_count_set = TRUE;
							animation_iteration_count.PushNumberL(CSS_NUMBER, -1);
						}
						else if (!name_set)
						{
							if (uni_char* str = m_input_buffer->GetString(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len))
							{
								name_set = TRUE;
								animation_name.PushStringL(CSS_STRING_LITERAL, str);
								strings.AddArrayL(str);
							}
							else
								LEAVE(OpStatus::ERR_NO_MEMORY);
						}
						else
							return INVALID;
					}
					else if (!duration_set && tok == CSS_SECOND || tok == CSS_NUMBER && m_val_array[i].value.number.number == 0)
					{
						duration_set = TRUE;
						animation_duration.PushNumberL(CSS_SECOND, float(m_val_array[i].value.number.number));
					}
					else if (!iteration_count_set && tok == CSS_NUMBER)
					{
						iteration_count_set = TRUE;
						animation_iteration_count.PushNumberL(CSS_NUMBER, float(m_val_array[i].value.number.number));
					}
					else if (!delay_set && tok == CSS_SECOND || tok == CSS_NUMBER && m_val_array[i].value.number.number == 0)
					{
						delay_set = TRUE;
						animation_delay.PushNumberL(CSS_SECOND, float(m_val_array[i].value.number.number));
					}
					else if (tok == CSS_FUNCTION_CUBIC_BEZ && !timing_set)
					{
						timing_set = TRUE;
						if (i+4 >= m_val_array.GetCount())
						{
							OP_ASSERT(FALSE); // If this happens, the grammar outputs wrong values for cubic-bezier functions.
							return INVALID;
						}
						for (int j=0; j<4; j++)
						{
							++i;
							OP_ASSERT(m_val_array[i].token == CSS_NUMBER);

							float param = float(m_val_array[i].value.number.number);
							if ((j % 2) == 0 || param >= 0 && param <= 1.0) // even values are y-values and can exceed [0,1]. x-values cannot.
								animation_timing.PushNumberL(CSS_NUMBER, param);
							else
								return INVALID;
						}
					}
					else if (!timing_set && tok == CSS_FUNCTION_STEPS && m_val_array[i].value.steps.steps > 0)
					{
						timing_set = TRUE;
						animation_timing.PushIntegerL(CSS_INT_NUMBER, m_val_array[i].value.steps.steps);
						animation_timing.PushIdentL(m_val_array[i].value.steps.start ? CSS_VALUE_start : CSS_VALUE_end);
						animation_timing.PushValueTypeL(CSS_IDENT); // dummy
						animation_timing.PushValueTypeL(CSS_IDENT); // dummy
					}
					else
					{
						return INVALID;
					}
				}
				if (start_i == i || ++i == m_val_array.GetCount())
					return INVALID;

				if (!name_set)
					animation_name.PushStringL(CSS_STRING_LITERAL, const_cast<uni_char*>(default_name));

				if (!delay_set)
					animation_delay.PushNumberL(CSS_SECOND, 0.0);

				if (!duration_set)
					animation_duration.PushNumberL(CSS_SECOND, 0.0);

				if (!iteration_count_set)
					animation_iteration_count.PushNumberL(CSS_NUMBER, 1.0);

				if (!fill_mode_set)
					animation_fill_mode.PushIdentL(CSS_VALUE_none);

				if (!direction_set)
					animation_direction.PushIdentL(CSS_VALUE_normal);

				if (!timing_set)
				{
					animation_timing.PushNumberL(CSS_NUMBER, 0.25f);
					animation_timing.PushNumberL(CSS_NUMBER, 0.1f);
					animation_timing.PushNumberL(CSS_NUMBER, 0.25f);
					animation_timing.PushNumberL(CSS_NUMBER, 1.0f);
				}
			}

			if (i > 0)
			{
				prop_list->AddDeclL(CSS_PROPERTY_animation_name, animation_name, 0, important, GetCurrentOrigin());
				prop_list->AddDeclL(CSS_PROPERTY_animation_duration, animation_duration, 0, important, GetCurrentOrigin());
				prop_list->AddDeclL(CSS_PROPERTY_animation_timing_function, animation_timing, 0, important, GetCurrentOrigin());
				prop_list->AddDeclL(CSS_PROPERTY_animation_iteration_count, animation_iteration_count, 0, important, GetCurrentOrigin());
				prop_list->AddDeclL(CSS_PROPERTY_animation_direction, animation_direction, 0, important, GetCurrentOrigin());
				prop_list->AddDeclL(CSS_PROPERTY_animation_delay, animation_delay, 0, important, GetCurrentOrigin());
				prop_list->AddDeclL(CSS_PROPERTY_animation_fill_mode, animation_fill_mode, 0, important, GetCurrentOrigin());
				return OK;
			}
			else
				return INVALID;
		}
		break;
	case CSS_PROPERTY_animation_name:
		{
			if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
					return OK;
				}
			}

			CSS_generic_value_list gen_arr;
			ANCHOR(CSS_generic_value_list, gen_arr);
			CSS_anchored_heap_arrays<uni_char> strings;
			ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);
			for (i=0; i < m_val_array.GetCount(); i++)
			{
				short tok = m_val_array[i].token;
				if ((i%2) == 1)
				{
					if (tok == CSS_COMMA)
						continue;
					else
						return INVALID;
				}
				else if (m_val_array[i].token == CSS_IDENT)
				{
					uni_char* str = m_input_buffer->GetString(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					if (str)
					{
						gen_arr.PushStringL(CSS_STRING_LITERAL, str);
						strings.AddArrayL(str);
					}
					else
						LEAVE(OpStatus::ERR_NO_MEMORY);
				}
			}

			if (!gen_arr.Empty())
			{
				prop_list->AddDeclL(prop, gen_arr, 0, important, GetCurrentOrigin());
				return OK;
			}
			else
				return INVALID;
		}
		break;
	case CSS_PROPERTY_animation_iteration_count:
		{
			if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
					return OK;
				}
			}

			CSS_generic_value_list gen_arr;
			ANCHOR(CSS_generic_value_list, gen_arr);

			for (i=0; i < m_val_array.GetCount(); i++)
			{
				short tok = m_val_array[i].token;
				if ((i%2) == 1)
				{
					if (tok == CSS_COMMA)
						continue;
					else
						return INVALID;
				}
				else if (m_val_array[i].token == CSS_NUMBER)
					gen_arr.PushNumberL(CSS_NUMBER, float(m_val_array[i].value.number.number));
				else if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos,
															 m_val_array[i].value.str.str_len);

					if (keyword == CSS_VALUE_infinite)
						gen_arr.PushNumberL(CSS_NUMBER, -1);
					else
						return INVALID;
				}
			}

			if (!gen_arr.Empty())
			{
				prop_list->AddDeclL(prop, gen_arr, 0, important, GetCurrentOrigin());
				return OK;
			}
			else
				return INVALID;
		}
		break;
	case CSS_PROPERTY_animation_direction:
	case CSS_PROPERTY_animation_fill_mode:
	case CSS_PROPERTY_animation_play_state:
		{
			if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (keyword == CSS_VALUE_inherit)
				{
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
					return OK;
				}
			}

			CSS_generic_value_list gen_arr;
			ANCHOR(CSS_generic_value_list, gen_arr);

			for (i=0; i < m_val_array.GetCount(); i++)
			{
				short tok = m_val_array[i].token;
				if ((i%2) == 1)
				{
					if (tok == CSS_COMMA)
						continue;
					else
						return INVALID;
				}
				else if (m_val_array[i].token == CSS_IDENT)
				{
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos,
															 m_val_array[i].value.str.str_len);

					BOOL valid_value =
						(prop == CSS_PROPERTY_animation_direction &&
						 CSS_is_animation_direction_val(keyword)) ||
						(prop == CSS_PROPERTY_animation_fill_mode &&
						 (keyword == CSS_VALUE_none || keyword == CSS_VALUE_forwards || keyword == CSS_VALUE_backwards ||
						  keyword == CSS_VALUE_both)) ||
						(prop == CSS_PROPERTY_animation_play_state &&
						 (keyword == CSS_VALUE_running || keyword == CSS_VALUE_paused));

					if (valid_value)
						gen_arr.PushIdentL(keyword);
					else
						return INVALID;
				}
			}

			if (!gen_arr.Empty())
			{
				prop_list->AddDeclL(prop, gen_arr, 0, important, GetCurrentOrigin());
				return OK;
			}
			else
				return INVALID;
		}
		break;
#endif // CSS_ANIMATIONS

#ifdef CSS_TRANSFORMS
	case CSS_PROPERTY_transform:
		return SetTransformListL(prop_list, important);
	case CSS_PROPERTY_transform_origin:
		return SetTransformOriginL(prop_list, important);
#endif

	case CSS_PROPERTY_resize:
		if (m_val_array.GetCount() == 1)
		{
			if (m_val_array[0].token == CSS_IDENT)
			{
				keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
				if (CSS_is_resize_val(keyword) || keyword == CSS_VALUE_inherit)
					prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				else
					return INVALID;
			}
			else
				return INVALID;
		}
		else
			return INVALID;
		break;

	default:
		return INVALID;
	}

	return OK;
}

#ifdef SVG_SUPPORT

CSS_Parser::DeclStatus
CSS_Parser::SetPaintUriL(CSS_property_list* prop_list, short prop, BOOL important)
{
	OP_ASSERT(prop == CSS_PROPERTY_fill || prop == CSS_PROPERTY_stroke);
	OP_ASSERT(m_val_array.GetCount() >= 1);
	OP_ASSERT(m_val_array[0].token == CSS_FUNCTION_URL);

	if (m_val_array.GetCount() > 2)
		return INVALID;

	// <uri> [none | currentColor | <color> ]
	URL url = m_input_buffer->GetURLL(m_base_url,
									  m_val_array[0].value.str.start_pos,
									  m_val_array[0].value.str.str_len);
	ANCHOR(URL, url);
	const uni_char* url_name = url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI).CStr();
	if (!url_name)
		return INVALID;

	if (m_val_array.GetCount() == 2)
	{
		CSS_generic_value gen_arr[2];
		BOOL recognized_keyword = FALSE;

		if (m_val_array[1].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[1].value.str.start_pos,
															  m_val_array[1].value.str.str_len);
			if (keyword == CSS_VALUE_none || keyword == CSS_VALUE_currentColor)
			{
				gen_arr[1].SetValueType(CSS_IDENT);
				gen_arr[1].SetType(keyword);

				recognized_keyword = TRUE;
			}
		}

		if (!recognized_keyword)
		{
			COLORREF color;
			CSSValue keyword;
			uni_char* dummy = NULL;
			switch (SetColorL(color, keyword, dummy, m_val_array[1]))
			{
			case COLOR_KEYWORD:
			case COLOR_SKIN:
			case COLOR_INVALID:
				return INVALID;

			case COLOR_RGBA:
			case COLOR_NAMED:
				break;
			}

			gen_arr[1].SetValueType(CSS_DECL_COLOR);
			gen_arr[1].value.color = color;
		}

		uni_char* url = OP_NEWA_L(uni_char, uni_strlen(url_name)+1);

		CSS_anchored_heap_arrays<uni_char> strings;
		ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);
		strings.AddArrayL(url);
		uni_strcpy(url, url_name);

		gen_arr[0].SetValueType(CSS_FUNCTION_URL);
		gen_arr[0].SetString(url);

		prop_list->AddDeclL(prop, gen_arr, 2, 0, important, GetCurrentOrigin());
	}
	else
	{
		prop_list->AddDeclL(prop, url_name, uni_strlen(url_name), important, GetCurrentOrigin(), CSS_string_decl::StringDeclUrl, m_hld_prof == NULL);
	}
	return OK;
}

#endif

#ifdef CSS_TRANSFORMS

CSS_Parser::DeclStatus
CSS_Parser::SetTransformOriginL(CSS_property_list* prop_list, BOOL important)
{
	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
		if (keyword == CSS_VALUE_inherit)
		{
			prop_list->AddTypeDeclL(CSS_PROPERTY_transform_origin, keyword, important, GetCurrentOrigin());
			return OK;
		}
	}

	int i = 0;
	float pos[2];
	CSSValueType pos_type[2];
	CSSValue dummy[2];
	BOOL dummy2;

	DeclStatus ret = SetPosition(i, pos, pos_type, FALSE, dummy, dummy2);

	if (ret == INVALID)
		return INVALID;

	if (i != m_val_array.GetCount())
		return INVALID;

	if (ret == OK)
		prop_list->AddDeclL(CSS_PROPERTY_transform_origin, pos[0], pos[1], pos_type[0], pos_type[1], important, GetCurrentOrigin());

	return ret;
}

CSS_Parser::DeclStatus
CSS_Parser::SetTransformListL(CSS_property_list* prop_list, BOOL important)
{
	CSSValue keyword;

	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
		if (keyword == CSS_VALUE_inherit || keyword == CSS_VALUE_none)
		{
			prop_list->AddTypeDeclL(CSS_PROPERTY_transform, keyword, important, GetCurrentOrigin());
			return OK;
		}
		else
			return INVALID;
	}
	else if (m_val_array.GetCount() > 0)
	{
		int i = 0;

		CSS_transform_list* transform_list = OP_NEW_L(CSS_transform_list, (CSS_PROPERTY_transform));

		for (i=0; i < m_val_array.GetCount(); i++)
		{
			keyword = CSSValue(m_val_array[i].token);

			if (!CSS_is_transform_function(keyword))
			{
				OP_DELETE(transform_list);
				return INVALID;
			}

			CSS_TransformFunctionInfo transform_info(keyword);

			i++;
			if (OpStatus::IsMemoryError(transform_list->StartTransformFunction(keyword)))
			{
				OP_DELETE(transform_list);
				LEAVE(OpStatus::ERR_NO_MEMORY);
			}

			int j = 0;

			for (;i < m_val_array.GetCount(); j++, i++)
			{
				if (CSS_is_transform_function(m_val_array[i].token))
				{
					i--;
					break;
				}

				if (!CSS_is_number(m_val_array[i].token))
				{
					OP_DELETE(transform_list);
					return INVALID;
				}

				float length = float(m_val_array[i].value.number.number);
				short length_type = CSS_get_number_ext(m_val_array[i].token);

				BOOL allow_implicit_unit = (m_hld_prof && !m_hld_prof->IsInStrictMode());

				if (length != 0 &&
					!transform_info.IsValidUnit(allow_implicit_unit, length_type))
				{
					OP_DELETE(transform_list);
					return INVALID;
				}

				transform_list->AppendValue(length, length_type);
			}

			if (!transform_info.ValidArgCount(j))
			{
				OP_DELETE(transform_list);
				return INVALID;
			}
		}

		prop_list->AddDecl(transform_list, important, GetCurrentOrigin());
		return OK;
	}
	else
		return INVALID;
}

#endif

/** Helper for handling a CSS_generic_value array of arbitrary size */
class CSS_generic_value_handler
{
public:
	/** First phase constructor. Initializes an empty handler. Use
	    ConstructL() as a second step. */
	CSS_generic_value_handler() : generic_value_array(NULL) {}

	/** Destructor */
	~CSS_generic_value_handler()
	{
		if (static_generic_value != generic_value_array)
			OP_DELETEA(generic_value_array);
	}

	/** Read/write operator. */
	CSS_generic_value* operator()() { return generic_value_array; }

	/** Read-only index operator */
	CSS_generic_value operator[](int index) const { return generic_value_array[index]; }

	/** Read/write index operator */
	CSS_generic_value& operator[](int index) { return generic_value_array[index]; }

	/** Second phase constructor.  Specifies maximum size.  Leaves on
	    out-of-memory. */
	void ConstructL(unsigned int max_size);

private:
	CSS_generic_value static_generic_value[CSS_MAX_ARR_SIZE];
	CSS_generic_value* generic_value_array;
};

void
CSS_generic_value_handler::ConstructL(unsigned int max_size)
{
	CSS_generic_value* p;

	if (max_size < CSS_MAX_ARR_SIZE)
		p = static_generic_value;
	else
		p = OP_NEWA_L(CSS_generic_value, max_size);

	generic_value_array = p;
}

CSS_Parser::DeclStatus
CSS_Parser::SetBackgroundListL(CSS_property_list* prop_list, short prop, BOOL important)
{
	int i = 0, j = 0;

	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
		if (keyword == CSS_VALUE_inherit)
		{
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			return OK;
		}
	}

	CSS_generic_value_handler gen_arr;
	ANCHOR(CSS_generic_value_handler, gen_arr);
	gen_arr.ConstructL(m_val_array.GetCount());

	for (i=0; i < m_val_array.GetCount(); i++)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
		if (keyword >= 0)
			if (((prop == CSS_PROPERTY_background_clip || prop == CSS_PROPERTY_background_origin) &&
				 CSS_is_background_clip_or_origin_val(keyword)) ||
				(prop == CSS_PROPERTY_background_attachment && CSS_is_bg_attachment_val(keyword)))
			{
				gen_arr[j].SetValueType(CSS_IDENT);
				gen_arr[j++].SetType(keyword);
			}
	}

	if (j > 0)
	{
		prop_list->AddDeclL(prop, gen_arr(), j, j, important, GetCurrentOrigin());
		return OK;
	}
	else
		return INVALID;
}

CSS_Parser::DeclStatus
CSS_Parser::SetPositionL(CSS_property_list* prop_list, BOOL important, short prop)
{
	OP_ASSERT(prop == CSS_PROPERTY_background_position
			|| prop == CSS_PROPERTY__o_object_position);

	// Inheritance
	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
		if (keyword == CSS_VALUE_inherit)
		{
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			return OK;
		}
	}

	int i = 0;

	CSS_generic_value_list gen_arr;
	ANCHOR(CSS_generic_value_list, gen_arr);

	int layers = 0;

	for (;i < m_val_array.GetCount(); i++)
	{
		float pos[2];
		CSSValueType pos_type[2];
		CSSValue reference_point[2];
		BOOL has_reference_point;

		DeclStatus ret = SetPosition(i, pos, pos_type, TRUE, reference_point, has_reference_point);

		if (ret == INVALID)
			return INVALID;

		if (has_reference_point)
			gen_arr.PushIdentL(reference_point[0]);
		gen_arr.PushNumberL(pos_type[0], pos[0]);

		if (has_reference_point)
			gen_arr.PushIdentL(reference_point[1]);
		gen_arr.PushNumberL(pos_type[1], pos[1]);

		layers++;

		if (i == m_val_array.GetCount())
			break;

		if (m_val_array[i].token == CSS_COMMA)
			gen_arr.PushValueTypeL(CSS_COMMA);
		else
			return INVALID;
	}

	if (layers > 1 && prop == CSS_PROPERTY__o_object_position)
		// Only a single item for object-position
		return INVALID;

	if (layers > 0)
	{
		prop_list->AddDeclL(prop, gen_arr, layers, important, GetCurrentOrigin());
		return OK;
	}
	else
		return INVALID;
}

CSS_Parser::DeclStatus
CSS_Parser::SetBackgroundShorthandL(CSS_property_list* prop_list, BOOL important)
{
	/* Memory management note:
	   m_{strings,gradients} holds the ownership of the data during parsing and cleans up
	   afterwards.  The data is also held in CSS_generic_value_list structures, but without
	   ownership. If parsing goes well, that data is then copied to the property list when
	   the CSS_generic_value_list is added. See CSS_property_list::AddDeclL(...) */
	CSS_anchored_heap_arrays<uni_char> strings;
	ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);

#ifdef CSS_GRADIENT_SUPPORT
	CSS_anchored_heap_objects<CSS_Gradient> gradients;
	ANCHOR(CSS_anchored_heap_objects<CSS_Gradient>, gradients);
#endif // CSS_GRADIENT_SUPPORT

	CSS_BackgroundShorthandInfo	shorthand_info(*this, strings
#ifdef CSS_GRADIENT_SUPPORT
	, gradients
#endif // CSS_GRADIENT_SUPPORT
	);

	if (shorthand_info.IsInherit())
	{
		prop_list->AddTypeDeclL(CSS_PROPERTY_background_attachment, CSS_VALUE_inherit, important, GetCurrentOrigin());
		prop_list->AddTypeDeclL(CSS_PROPERTY_background_repeat, CSS_VALUE_inherit, important, GetCurrentOrigin());
		prop_list->AddTypeDeclL(CSS_PROPERTY_background_image, CSS_VALUE_inherit, important, GetCurrentOrigin());
		prop_list->AddTypeDeclL(CSS_PROPERTY_background_position, CSS_VALUE_inherit, important, GetCurrentOrigin());
		prop_list->AddTypeDeclL(CSS_PROPERTY_background_size, CSS_VALUE_inherit, important, GetCurrentOrigin());
		prop_list->AddTypeDeclL(CSS_PROPERTY_background_color, CSS_VALUE_inherit, important, GetCurrentOrigin());
		prop_list->AddTypeDeclL(CSS_PROPERTY_background_origin, CSS_VALUE_inherit, important, GetCurrentOrigin());
		prop_list->AddTypeDeclL(CSS_PROPERTY_background_clip, CSS_VALUE_inherit, important, GetCurrentOrigin());

		return OK;
	}
	else
	{
		CSS_generic_value_list bg_attachment_list;
		ANCHOR(CSS_generic_value_list, bg_attachment_list);

		CSS_generic_value_list bg_repeat_list;
		ANCHOR(CSS_generic_value_list, bg_repeat_list);

		CSS_generic_value_list bg_image_list;
		ANCHOR(CSS_generic_value_list, bg_image_list);

		CSS_generic_value_list bg_pos_list;
		ANCHOR(CSS_generic_value_list, bg_pos_list);

		CSS_generic_value_list bg_size_list;
		ANCHOR(CSS_generic_value_list, bg_size_list);

		CSS_generic_value_list bg_origin_list;
		ANCHOR(CSS_generic_value_list, bg_origin_list);

		CSS_generic_value_list bg_clip_list;
		ANCHOR(CSS_generic_value_list, bg_clip_list);

		COLORREF bg_color_value = USE_DEFAULT_COLOR;
		BOOL bg_color_is_keyword = FALSE;
		CSSValue bg_color_keyword = CSS_VALUE_transparent;

		int nlayers = 0;
		while (shorthand_info.ExtractLayerL(bg_attachment_list, bg_repeat_list,
											bg_image_list, bg_pos_list, bg_size_list,
											bg_origin_list, bg_clip_list,
											bg_color_value, bg_color_is_keyword, bg_color_keyword))
			nlayers++;

		if (shorthand_info.IsInvalid())
			return INVALID;

		prop_list->AddDeclL(CSS_PROPERTY_background_attachment, bg_attachment_list, nlayers, important, GetCurrentOrigin());
		prop_list->AddDeclL(CSS_PROPERTY_background_repeat, bg_repeat_list, nlayers, important, GetCurrentOrigin());
		prop_list->AddDeclL(CSS_PROPERTY_background_image, bg_image_list, nlayers, important, GetCurrentOrigin());
		prop_list->AddDeclL(CSS_PROPERTY_background_position, bg_pos_list, nlayers, important, GetCurrentOrigin());
		prop_list->AddDeclL(CSS_PROPERTY_background_size, bg_size_list, nlayers, important, GetCurrentOrigin());
		prop_list->AddDeclL(CSS_PROPERTY_background_origin, bg_origin_list, nlayers, important, GetCurrentOrigin());
		prop_list->AddDeclL(CSS_PROPERTY_background_clip, bg_clip_list, nlayers, important, GetCurrentOrigin());

		if (bg_color_value != USE_DEFAULT_COLOR)
		{
			if (bg_color_is_keyword)
				prop_list->AddColorDeclL(CSS_PROPERTY_background_color, bg_color_value, important, GetCurrentOrigin());
			else
				prop_list->AddLongDeclL(CSS_PROPERTY_background_color, (long)bg_color_value, important, GetCurrentOrigin());
		}
		else if (bg_color_keyword >= 0)
			prop_list->AddTypeDeclL(CSS_PROPERTY_background_color, bg_color_keyword, important, GetCurrentOrigin());

		return OK;
	}
}

CSS_Parser::DeclStatus
CSS_Parser::SetBackgroundSizeL(CSS_property_list* prop_list, BOOL important)
{
	int i = 0, j = 0;
	int item_count = 1;

	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
		if (keyword == CSS_VALUE_inherit)
		{
			prop_list->AddTypeDeclL(CSS_PROPERTY_background_size, keyword, important, GetCurrentOrigin());
			return OK;
		}
	}

	CSS_generic_value_handler gen_arr;
	ANCHOR(CSS_generic_value_handler, gen_arr);
	gen_arr.ConstructL(m_val_array.GetCount());

	enum {
		EXPECT_NEXT,
		EXPECT_SECOND_LENGTH_OR_COMMA,
		EXPECT_COMMA
	} state = EXPECT_NEXT;

	for (i=0; i < m_val_array.GetCount(); i++)
	{
		if (m_val_array[i].token == CSS_IDENT && (state == EXPECT_NEXT || state == EXPECT_SECOND_LENGTH_OR_COMMA))
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);

			if (keyword == CSS_VALUE_cover || keyword == CSS_VALUE_contain)
			{
				gen_arr[j].SetValueType(CSS_IDENT);
				gen_arr[j++].SetType(keyword);

				state = (state == EXPECT_NEXT) ? EXPECT_SECOND_LENGTH_OR_COMMA : EXPECT_COMMA;
			}
			else if (keyword == CSS_VALUE_auto)
			{
				gen_arr[j].SetValueType(CSS_IDENT);
				gen_arr[j++].SetType(keyword);

				state = (state == EXPECT_NEXT) ? EXPECT_SECOND_LENGTH_OR_COMMA : EXPECT_COMMA;
			}
			else
				return INVALID;
		}
		else if (m_val_array[i].token == CSS_NUMBER ||
				 m_val_array[i].token == CSS_PERCENTAGE ||
				 CSS_is_length_number_ext(m_val_array[i].token) && (state == EXPECT_NEXT || state == EXPECT_SECOND_LENGTH_OR_COMMA))
		{
			short type = m_val_array[i].token;
			if (type == CSS_NUMBER)
				type = CSS_PX;
			gen_arr[j].SetValueType(type);
			gen_arr[j++].SetReal(float(m_val_array[i].value.number.number));

			state = (state == EXPECT_NEXT) ? EXPECT_SECOND_LENGTH_OR_COMMA : EXPECT_COMMA;
		}
		else if (m_val_array[i].token == CSS_COMMA && (state == EXPECT_SECOND_LENGTH_OR_COMMA || state == EXPECT_COMMA))
		{
			gen_arr[j++].SetValueType(CSS_COMMA);

			state = EXPECT_NEXT;
			item_count++;
		}
		else
			return INVALID;
	}

	if (state == EXPECT_COMMA || state == EXPECT_SECOND_LENGTH_OR_COMMA)
	{
		prop_list->AddDeclL(CSS_PROPERTY_background_size, gen_arr(), j, item_count, important, GetCurrentOrigin());
		return OK;
	}
	else
		return INVALID;
}

CSS_Parser::DeclStatus
CSS_Parser::SetBackgroundRepeatL(CSS_property_list* prop_list, BOOL important)
{
	int i = 0, j = 0;

	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
		if (keyword == CSS_VALUE_inherit)
		{
			prop_list->AddTypeDeclL(CSS_PROPERTY_background_repeat, keyword, important, GetCurrentOrigin());
			return OK;
		}
	}

	CSS_generic_value_handler gen_arr;
	ANCHOR(CSS_generic_value_handler, gen_arr);
	gen_arr.ConstructL(m_val_array.GetCount());

	int item_count = 1;

	enum {
		EXPECT_FIRST,
		EXPECT_SECOND_OR_COMMA,
		EXPECT_COMMA
	} state = EXPECT_FIRST;

	for (i=0; i < m_val_array.GetCount(); i++)
	{
		if (m_val_array[i].token == CSS_IDENT && (state == EXPECT_FIRST || state == EXPECT_SECOND_OR_COMMA))
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
			if (CSS_is_bg_repeat_val(keyword) &&
				(state == EXPECT_FIRST || (keyword != CSS_VALUE_repeat_x && keyword != CSS_VALUE_repeat_y)))
			{
				gen_arr[j].SetValueType(CSS_IDENT);
				gen_arr[j++].SetType(keyword);

				if (state == EXPECT_FIRST && keyword != CSS_VALUE_repeat_x && keyword != CSS_VALUE_repeat_y)
					state = EXPECT_SECOND_OR_COMMA;
				else
					state = EXPECT_COMMA;
			}
			else
				return INVALID;
		}
		else if (m_val_array[i].token == CSS_COMMA && (state == EXPECT_SECOND_OR_COMMA || state == EXPECT_COMMA))
		{
			gen_arr[j++].SetValueType(CSS_COMMA);

			state = EXPECT_FIRST;
			item_count++;
		}
		else
			return INVALID;
	}

	if (state == EXPECT_COMMA || state == EXPECT_SECOND_OR_COMMA)
	{
		prop_list->AddDeclL(CSS_PROPERTY_background_repeat, gen_arr(), j, item_count, important, GetCurrentOrigin());
		return OK;
	}
	else
		return INVALID;
}

CSS_Parser::DeclStatus
CSS_Parser::SetBackgroundImageL(CSS_property_list* prop_list, BOOL important)
{
	CSSValue keyword = CSS_VALUE_UNKNOWN;
	int i = 0, j = 0;
	BOOL expect_comma = FALSE;

	int item_count = 1;

	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
		if (keyword == CSS_VALUE_inherit)
		{
			prop_list->AddTypeDeclL(CSS_PROPERTY_background_image, CSS_VALUE_inherit, important, GetCurrentOrigin());
			return OK;
		}
	}

	CSS_generic_value_handler gen_arr;
	ANCHOR(CSS_generic_value_handler, gen_arr);
	gen_arr.ConstructL(m_val_array.GetCount());

	// Anchor new objects to these when adding them to gen_arr.
	// AddDeclL at the end of this method will copy them elsewhere before the anchor is destroyed.
	CSS_anchored_heap_arrays<uni_char> strings;
	ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);

#ifdef CSS_GRADIENT_SUPPORT
	CSS_anchored_heap_objects<CSS_Gradient> gradients;
	ANCHOR(CSS_anchored_heap_objects<CSS_Gradient>, gradients);
#endif // CSS_GRADIENT_SUPPORT

	for (i=0; i < m_val_array.GetCount(); i++)
	{
		if (m_val_array[i].token == CSS_IDENT && !expect_comma)
		{
			keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
			if (keyword == CSS_VALUE_none)
			{
				gen_arr[j].value_type = CSS_IDENT;
				gen_arr[j++].value.type = keyword;
				expect_comma = TRUE;
			}
			else
				return INVALID;
		}
		else if (m_val_array[i].token == CSS_FUNCTION_URL && !expect_comma)
		{
			// image
			const uni_char* url_name = m_input_buffer->GetURLL(m_base_url,
															   m_val_array[i].value.str.start_pos,
															   m_val_array[i].value.str.str_len).GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI).CStr();
			if (!url_name)
				return INVALID;

			uni_char* url_str = OP_NEWA_L(uni_char, uni_strlen(url_name)+1);
			strings.AddArrayL(url_str);
			uni_strcpy(url_str, url_name);

			gen_arr[j].value_type = CSS_FUNCTION_URL;
			gen_arr[j++].value.string = url_str;
			expect_comma = TRUE;
		}
#ifdef CSS_GRADIENT_SUPPORT
		else if (CSS_is_gradient(m_val_array[i].token) && !expect_comma)
		{
			CSSValueType type = (CSSValueType) m_val_array[i].token;
			CSS_Gradient* gradient = SetGradientL(/* inout */ i, /* inout */ type);
			if (!gradient)
				return INVALID;

			gradients.AddObjectL(gradient);

			gen_arr[j].SetValueType(type);
			gen_arr[j++].SetGradient(gradient);
			expect_comma = TRUE;
		}
#endif // CSS_GRADIENT_SUPPORT
#ifdef SKIN_SUPPORT
		else if (m_val_array[i].token == CSS_FUNCTION_SKIN && !expect_comma)
		{
			uni_char* skin_name = m_input_buffer->GetSkinString(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
			if (!skin_name)
				return INVALID;

			strings.AddArrayL(skin_name);
			gen_arr[j].value_type = CSS_FUNCTION_SKIN;
			gen_arr[j++].value.string = skin_name;
			expect_comma = TRUE;
		}
#endif // SKIN_SUPPORT
		else if (m_val_array[i].token == CSS_COMMA && expect_comma)
		{
			expect_comma = FALSE;
			item_count++;
		}
		else
			return INVALID;
	}

	if (j > 0 && expect_comma)
	{
		prop_list->AddDeclL(CSS_PROPERTY_background_image, gen_arr(), j, item_count, important, GetCurrentOrigin());
		return OK;
	}
	else
		return INVALID;
}

void
CSS_Parser::FindPositionValues(int i, CSS_generic_value values[4], int& n_values) const
{
	int j = i;
	for (; j < i+4 && j < m_val_array.GetCount(); j++)
	{
		// If the CSS value is an identifier, retrieve the position values.
		if (m_val_array[j].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[j].value.str.start_pos,
														   m_val_array[j].value.str.str_len);
			if (CSS_is_position_val(keyword))
			{
				values[j-i].SetValueType(CSS_IDENT);
				values[j-i].SetType(keyword);
			}
			else
				break;
		}
		else if (CSS_is_length_number_ext(m_val_array[j].token) ||
				 m_val_array[j].token == CSS_PERCENTAGE ||
				 (m_val_array[j].token == CSS_NUMBER && (m_val_array[j].value.number.number == 0 || m_hld_prof && !m_hld_prof->IsInStrictMode())))
		{
			/* If the CSS value is a percentage or a length, set the positions to the given value.
			   Also, if we are in quirks mode on an author stylesheet, convert raw numbers (including 0) to pixel lengths. */
			short ext = CSS_get_number_ext(m_val_array[j].token);
			if (ext == CSS_NUMBER)
				ext = CSS_PX;
			values[j-i].SetValueType(ext);
			values[j-i].SetReal(float(m_val_array[j].value.number.number));
		}
		else
			break;
	}

	n_values = j - i;
}

CSS_Parser::DeclStatus
CSS_Parser::SetPosition(int& i, float pos[2], CSSValueType pos_type[2], BOOL allow_reference_point, CSSValue reference_point[2], BOOL& has_reference_point) const
{
	pos[0] = pos[1] = 0;
	pos_type[0] = pos_type[1] = CSS_PERCENTAGE;

	reference_point[0] = CSS_VALUE_left;
	reference_point[1] = CSS_VALUE_top;
	has_reference_point = FALSE;

	int n_values;
	CSS_generic_value values[4];
	FindPositionValues(i, values, n_values);

	if (n_values == 0)
		// No accepted values found: invalid CSS.
		return INVALID;
	else if (n_values == 1)
	{
		// Only one value found: assume 'center'/50% for the other.
		if (values[0].GetValueType() == CSS_IDENT)
		{
			CSSValue keyword = values[0].GetType();

			// A single keyword
			switch (keyword)
			{
			case CSS_VALUE_top:
				pos[0] = 50;
				pos[1] = 0;
				break;

			case CSS_VALUE_left:
				pos[0] = 0;
				pos[1] = 50;
				break;

			case CSS_VALUE_center:
				pos[0] = 50;
				pos[1] = 50;
				break;

			case CSS_VALUE_right:
				pos[0] = 100;
				pos[1] = 50;
				break;

			case CSS_VALUE_bottom:
				pos[0] = 50;
				pos[1] = 100;
				break;

			default:
				OP_ASSERT(!"Invalid keyword found.");
			}
		}
		else
		{
			// A single value.
			pos[0] = values[0].GetReal();
			pos_type[0] = (CSSValueType)values[0].GetValueType();

			pos[1] = 50;
		}
	}
	else if (n_values == 2)
	{
		// Two values found.
		if (values[0].GetValueType() == CSS_IDENT && values[1].GetValueType() == CSS_IDENT)
		{
			// Both values are keywords. Set percentage values.
			CSSValue keywords[2] = { values[0].GetType(), values[1].GetType() };

			if ((keywords[0] == CSS_VALUE_left && keywords[1] == CSS_VALUE_top) ||
				 (keywords[1] == CSS_VALUE_left && keywords[0] == CSS_VALUE_top))
			{
				pos[0] = pos[1] = 0;
			}
			else if ((keywords[0] == CSS_VALUE_center && keywords[1] == CSS_VALUE_top) ||
					 (keywords[1] == CSS_VALUE_center && keywords[0] == CSS_VALUE_top))
			{
				pos[0] = 50; pos[1] = 0;
			}
			else if ((keywords[0] == CSS_VALUE_right && keywords[1] == CSS_VALUE_top) ||
					 (keywords[1] == CSS_VALUE_right && keywords[0] == CSS_VALUE_top))
			{
				pos[0] = 100; pos[1] = 0;
			}
			else if ((keywords[0] == CSS_VALUE_center && keywords[1] == CSS_VALUE_left) ||
					 (keywords[1] == CSS_VALUE_center && keywords[0] == CSS_VALUE_left))
			{
				pos[0] = 0; pos[1] = 50;
			}
			else if (keywords[0] == CSS_VALUE_center && keywords[1] == CSS_VALUE_center)
			{
				pos[0] = 50; pos[1] = 50;
			}
			else if ((keywords[0] == CSS_VALUE_center && keywords[1] == CSS_VALUE_right) ||
					 (keywords[1] == CSS_VALUE_center && keywords[0] == CSS_VALUE_right))
			{
				pos[0] = 100; pos[1] = 50;
			}
			else if ((keywords[0] == CSS_VALUE_bottom && keywords[1] == CSS_VALUE_left) ||
					 (keywords[1] == CSS_VALUE_bottom && keywords[0] == CSS_VALUE_left))
			{
				 pos[0] = 0; pos[1] = 100;
			}
			else if ((keywords[0] == CSS_VALUE_bottom && keywords[1] == CSS_VALUE_center) ||
				(keywords[1] == CSS_VALUE_bottom && keywords[0] == CSS_VALUE_center))
			{
				pos[0] = 50; pos[1] = 100;
			}
			else if ((keywords[0] == CSS_VALUE_bottom && keywords[1] == CSS_VALUE_right) ||
					 (keywords[1] == CSS_VALUE_bottom && keywords[0] == CSS_VALUE_right))
			{
				pos[0] = 100; pos[1] = 100;
			}
			else
				return INVALID;
		}
		else if (values[0].GetValueType() == CSS_IDENT)
		{
			// The first value is a keyword. Convert it to an x position.
			CSSValue keyword = values[0].GetType();

			if (keyword == CSS_VALUE_center)
				pos[0] = 50;
			else if (keyword == CSS_VALUE_right)
				pos[0] = 100;
			else if (keyword != CSS_VALUE_left)
				return INVALID;

			pos[1] = values[1].GetReal();
			pos_type[1] = (CSSValueType)values[1].GetValueType();
		}
		else if (values[1].GetValueType() == CSS_IDENT)
		{
			// The second value is a keyword. Convert it to a y position.
			CSSValue keyword = values[1].GetType();

			if (keyword == CSS_VALUE_center)
				pos[1] = 50;
			else if (keyword == CSS_VALUE_bottom)
				pos[1] = 100;
			else if (keyword != CSS_VALUE_top)
				return INVALID;

			pos[0] = values[0].GetReal();
			pos_type[0] = (CSSValueType)values[0].GetValueType();
		}
		else
		{
			pos[0] = values[0].GetReal();
			pos_type[0] = (CSSValueType)values[0].GetValueType();
			pos[1] = values[1].GetReal();
			pos_type[1] = (CSSValueType)values[1].GetValueType();
		}
	}
	else if (allow_reference_point)
	{
		// three or four values

		OP_ASSERT(n_values == 3 || n_values == 4);

		float tmp_pos[2] = { 0, 0 };
		CSSValueType tmp_pos_type[2] = { CSS_PERCENTAGE, CSS_PERCENTAGE };
		CSSValue keywords[2];

		if (values[0].GetValueType() != CSS_IDENT)
			return INVALID;

		keywords[0] = (CSSValue)values[0].GetType();

		int j = 1;
		if (keywords[0] != CSS_VALUE_center)
			if (values[j].GetValueType() != CSS_IDENT)
			{
				tmp_pos[0] = values[j].GetReal();
				tmp_pos_type[0] = (CSSValueType)values[j++].GetValueType();
			}

		if (values[j].GetValueType() != CSS_IDENT)
			return INVALID;

		keywords[1] = (CSSValue)values[j++].GetType();

		if (keywords[1] != CSS_VALUE_center)
			if (j < n_values && values[j].GetValueType() != CSS_IDENT)
			{
				tmp_pos[1] = values[j].GetReal();
				tmp_pos_type[1] = (CSSValueType)values[j++].GetValueType();
			}

		if (j < n_values)
			return INVALID;

		if (keywords[0] == CSS_VALUE_left || keywords[0] == CSS_VALUE_right)
		{
			reference_point[0] = keywords[0];
			pos[0] = tmp_pos[0];
			pos_type[0] = tmp_pos_type[0];

			if (keywords[1] == CSS_VALUE_top || keywords[1] == CSS_VALUE_bottom)
			{
				reference_point[1] = keywords[1];
				pos[1] = tmp_pos[1];
				pos_type[1] = tmp_pos_type[1];
			}
			else if (keywords[1] == CSS_VALUE_center)
			{
				reference_point[1] = CSS_VALUE_top;
				pos[1] = 50;
			}
			else
				return INVALID;
		}
		else
			if (keywords[0] == CSS_VALUE_top || keywords[0] == CSS_VALUE_bottom)
			{
				reference_point[1] = keywords[0];
				pos[1] = tmp_pos[0];
				pos_type[1] = tmp_pos_type[0];

				if (keywords[1] == CSS_VALUE_left || keywords[1] == CSS_VALUE_right)
				{
					reference_point[0] = keywords[1];
					pos[0] = tmp_pos[1];
					pos_type[0] = tmp_pos_type[1];
				}
				else if (keywords[1] == CSS_VALUE_center)
				{
					reference_point[0] = CSS_VALUE_left;
					pos[0] = 50;
				}
				else
					return INVALID;
			}
			else
				if (keywords[0] == CSS_VALUE_center)
				{
					if (keywords[1] == CSS_VALUE_left || keywords[1] == CSS_VALUE_right)
					{
						reference_point[1] = CSS_VALUE_top;
						pos[1] = 50;

						reference_point[0] = keywords[1];
						pos[0] = tmp_pos[1];
						pos_type[0] = tmp_pos_type[1];
					}
					else if (keywords[1] == CSS_VALUE_top || keywords[1] == CSS_VALUE_bottom)
					{
						reference_point[0] = CSS_VALUE_left;
						pos[0] = 50;

						reference_point[1] = keywords[1];
						pos[1] = tmp_pos[1];
						pos_type[1] = tmp_pos_type[1];
					}
					else
						return INVALID;
				}
				else
					return INVALID;

		has_reference_point = TRUE;
	}
	else
		return INVALID;

	i += n_values;
	return OK;
}

void
CSS_Parser::SetTimingKeywordL(CSSValue keyword, CSS_generic_value_list& time_arr)
{
	BOOL steps = FALSE;
	BOOL step_start = FALSE;

	float preset[4] = { 0, 0, 1, 1 };
	switch (keyword)
	{
	case CSS_VALUE_ease:
		preset[0] = 0.25f;
		preset[1] = 0.1f;
		preset[2] = 0.25f;
		break;
	case CSS_VALUE_ease_in:
	case CSS_VALUE_ease_in_out:
		preset[0] = 0.42f;
		if (keyword == CSS_VALUE_ease_in)
			break;
	case CSS_VALUE_ease_out:
		preset[2] = 0.58f;
		break;
	case CSS_VALUE_step_start:
		step_start = TRUE;
		/* fall-through */
	case CSS_VALUE_step_end:
		steps = TRUE;
		break;
	}

	if (steps)
	{
		time_arr.PushIntegerL(CSS_INT_NUMBER, 1);
		time_arr.PushIdentL(step_start ? CSS_VALUE_start : CSS_VALUE_end);
		time_arr.PushValueTypeL(CSS_IDENT); // dummy
		time_arr.PushValueTypeL(CSS_IDENT); // dummy
	}
	else
		for (int j=0; j<4; j++)
			time_arr.PushNumberL(CSS_NUMBER, preset[j]);
}

CSS_Parser::DeclStatus
CSS_Parser::SetFamilyNameL(CSS_generic_value& value,
						   int& pos,
						   BOOL allow_inherit,
						   BOOL allow_generic)
{
	if (m_val_array[pos].token == CSS_STRING_LITERAL)
	{
		uni_char* family_str = m_input_buffer->GetString(m_val_array[pos].value.str.start_pos, m_val_array[pos].value.str.str_len);

		if (family_str)
		{
			value.SetValueType(CSS_STRING_LITERAL);
			value.SetString(family_str);
			pos++;
		}
		else
			LEAVE(OpStatus::ERR_NO_MEMORY);
	}
	else
	{
		int start = pos;
		int total_len = 0;

		while (pos < m_val_array.GetCount() && m_val_array[pos].token == CSS_IDENT)
		{
			/* Add extra character for space separator or null-termination. */
			total_len += m_val_array[pos].value.str.str_len + 1;
			pos++;
		}

		int ident_count = pos - start;

		if (ident_count > 0)
		{
			if (ident_count == 1)
			{
				CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[start].value.str.start_pos, m_val_array[start].value.str.str_len);
				if (keyword > CSS_VALUE_UNSPECIFIED)
				{
					if (allow_inherit && keyword == CSS_VALUE_inherit ||
						allow_generic && CSS_is_font_generic_val(keyword))
					{
						int font_type = keyword;
						if (font_type != CSS_VALUE_inherit)
							font_type |= CSS_GENERIC_FONT_FAMILY;

						/* 'font_type' is a CSSValue, but tagged with CSS_GENERIC_FONT_FAMILY to
						   separate it from plain font numbers. The receiver of this value must
						   check for CSS_GENERIC_FONT_FAMILY and mask with
						   CSS_GENERIC_FONT_FAMILY_mask to access the CSSValue. */

						value.SetValueType(CSS_IDENT);
						value.SetType(CSSValue(font_type));
						return OK;
					}
					else
						if (keyword == CSS_VALUE_inherit || CSS_is_font_generic_val(keyword))
							return INVALID;
				}
			}

			uni_char* family_str = OP_NEWA_L(uni_char, total_len);
			uni_char* dst = family_str;

			for (int i=start; i<start+ident_count; i++)
			{
				dst = m_input_buffer->GetString(dst, m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
				*dst++ = ' ';
			}
			*--dst = 0;
			value.SetValueType(CSS_STRING_LITERAL);
			value.SetString(family_str);
		}
		else
			return INVALID;
	}

	return OK;
}

CSS_Parser::DeclStatus
CSS_Parser::SetFontFamilyL(CSS_property_list* prop_list, int pos, BOOL important, BOOL allow_inherit)
{
	CSS_anchored_heap_arrays<uni_char> strings;
	ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);
	CSS_generic_value_list gen_arr;
	ANCHOR(CSS_generic_value_list, gen_arr);

	BOOL expect_comma = FALSE;
	while (pos < m_val_array.GetCount())
	{
		if (expect_comma)
		{
			if (m_val_array[pos++].token != CSS_COMMA)
				return INVALID;
			expect_comma = FALSE;
		}
		else
		{
			CSS_generic_value value;
			if (SetFamilyNameL(value, pos, allow_inherit, TRUE) == OK)
			{
				if (value.GetValueType() == CSS_IDENT && value.GetType() == CSS_VALUE_inherit)
				{
					OP_ASSERT(pos == m_val_array.GetCount());
					prop_list->AddTypeDeclL(CSS_PROPERTY_font_family, CSS_VALUE_inherit, important, GetCurrentOrigin());
					return OK;
				}

				if (value.GetValueType() == CSS_STRING_LITERAL)
				{
					strings.AddArrayL(value.GetString());
					if (m_hld_prof)
						m_hld_prof->GetCSSCollection()->AddFontPrefetchCandidate(value.GetString());
				}
				gen_arr.PushGenericValueL(value);
				expect_comma = TRUE;
			}
			else
				return INVALID;
		}
	}

	if (gen_arr.Empty())
		return INVALID;

	prop_list->AddDeclL(CSS_PROPERTY_font_family, gen_arr, 0, important, GetCurrentOrigin());
	return OK;
}

CSS_Parser::DeclStatus
CSS_Parser::SetShadowL(CSS_property_list* prop_list, short prop, BOOL important)
{
	const int max_length_count = (prop == CSS_PROPERTY_box_shadow) ? 4 : 3;

	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		// Check for none/inherit
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
		if (keyword == CSS_VALUE_none || keyword == CSS_VALUE_inherit)
		{
			prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
			return OK;
		}
		else
			return INVALID;
	}
	else if (m_val_array.GetCount() > 1)
	{
		int i=0, j=0;
		CSS_generic_value_handler gen_arr;
		ANCHOR(CSS_generic_value_handler, gen_arr);
		gen_arr.ConstructL(m_val_array.GetCount());

		while (i < m_val_array.GetCount())
		{
			COLORREF color = USE_DEFAULT_COLOR;
			int length_count = 0;

			// The value is a list of shadow lengths with colors.
			while (i < m_val_array.GetCount())
			{
				short tok = m_val_array[i].token;
				CSSValue keyword = CSS_VALUE_UNKNOWN;

				if (tok == CSS_NUMBER && (m_val_array[i].value.number.number == 0 || m_hld_prof && !m_hld_prof->IsInStrictMode()))
					tok = CSS_PX;

				if (tok == CSS_IDENT)
					keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);

				if (CSS_is_length_number_ext(tok) && length_count > -1 && length_count < max_length_count)
				{
					float num = float(m_val_array[i].value.number.number);

					if (length_count == 2 && num < 0)
						/* "The third length is a blur radius. Negative values are not allowed." */

						return INVALID;

					gen_arr[j].value_type = tok;
					gen_arr[j++].value.real = num;
					length_count++;
				}
				else if (tok == CSS_COMMA)
				{
					if (length_count != 0 && length_count != 1 && i+1 < m_val_array.GetCount())
					{
						gen_arr[j++].value_type = tok;
						break;
					}
					else
						return INVALID;
				}
				else if (prop == CSS_PROPERTY_box_shadow && tok == CSS_IDENT && keyword == CSS_VALUE_inset)
				{
					gen_arr[j].value_type = CSS_IDENT;
					gen_arr[j++].SetType(CSS_VALUE_inset);
				}
				else if (color == USE_DEFAULT_COLOR && length_count != 1)
				{
					if (tok == CSS_FUNCTION_RGB || SupportsAlpha() && tok == CSS_FUNCTION_RGBA)
						color = m_val_array[i].value.color;
					else if (tok == CSS_HASH)
					{
						color = m_input_buffer->GetColor(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					}
					else if (tok == CSS_IDENT)
					{
						if (CSS_is_ui_color_val(keyword))
							color = keyword | CSS_COLOR_KEYWORD_TYPE_ui_color;
						else if (keyword == CSS_VALUE_currentColor)
							color = COLORREF(CSS_COLOR_current_color);
						else if (keyword == CSS_VALUE_transparent)
							color = COLORREF(CSS_COLOR_transparent);
						else
							color = m_input_buffer->GetNamedColorIndex(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					}

					if (color == USE_DEFAULT_COLOR)
						return INVALID;

					if (length_count > 0)
						length_count = -1;

					gen_arr[j].value_type = CSS_DECL_COLOR;
					gen_arr[j++].value.color = color;
				}
				else
					return INVALID;

				i++;
			}

			if (length_count == 0 || length_count == 1)
				return INVALID;

			i++;
		}

		prop_list->AddDeclL(prop, gen_arr(), j, 0, important, GetCurrentOrigin());
		return OK;
	}
	return INVALID;
}

CSS_Parser::DeclStatus
CSS_Parser::SetIndividualBorderRadiusL(CSS_property_list* prop_list, short prop, BOOL important)
{
	if (m_val_array.GetCount() == 1)
	{
		if (CSS_is_number(m_val_array[0].token) && m_val_array[0].token != CSS_DIMEN)
		{
			float length = float(m_val_array[0].value.number.number);
			int length_type = CSS_get_number_ext(m_val_array[0].token);

			if (!CSS_is_length_number_ext(length_type) && (length_type != CSS_PERCENTAGE))
				length_type = CSS_PX;

			if (length >= 0)
			{
				prop_list->AddDeclL(prop, length, length, length_type, length_type, important, GetCurrentOrigin());
				return OK;
			}
		}
		else if (m_val_array[0].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_inherit)
			{
				prop_list->AddTypeDeclL(prop, keyword, important, GetCurrentOrigin());
				return OK;
			}
		}
	}
	else if (m_val_array.GetCount() == 2)
	{
		if (CSS_is_number(m_val_array[0].token) && m_val_array[0].token != CSS_DIMEN &&
			CSS_is_number(m_val_array[1].token) && m_val_array[1].token != CSS_DIMEN)
		{
			float length1 = float(m_val_array[0].value.number.number);
			int length1_type = CSS_get_number_ext(m_val_array[0].token);

			float length2 = float(m_val_array[1].value.number.number);
			int length2_type = CSS_get_number_ext(m_val_array[1].token);

			if (!CSS_is_length_number_ext(length1_type) && (length1_type != CSS_PERCENTAGE))
				length1_type = CSS_PX;

			if (!CSS_is_length_number_ext(length2_type) && (length2_type != CSS_PERCENTAGE))
				length2_type = CSS_PX;

			if (length1 >= 0 && length2 >= 0)
			{
				prop_list->AddDeclL(prop, length1, length2, length1_type, length2_type, important, GetCurrentOrigin());
				return OK;
			}
		}
	}

	return INVALID;
}

static void CompleteBorderRadiusArray(int code, float* radius, int* type)
{
	if (code < 2)
	{
		radius[1] = radius[0];
		type[1] = type[0];
	}

	if (code < 3)
	{
		radius[2] = radius[0];
		type[2] = type[0];
	}

	if (code < 4)
	{
		radius[3] = radius[1];
		type[3] = type[1];
	}
}

CSS_Parser::DeclStatus
CSS_Parser::SetShorthandBorderRadiusL(CSS_property_list* prop_list, BOOL important)
{
	int i = 0, j = 0;

	float horiz_radius[4];
	int horiz_radius_type[4];

	float vert_radius[4];
	int vert_radius_type[4];

	short props[4] = {CSS_PROPERTY_border_top_left_radius,
					  CSS_PROPERTY_border_top_right_radius,
					  CSS_PROPERTY_border_bottom_right_radius,
					  CSS_PROPERTY_border_bottom_left_radius};

	// Check for 'inherit'.
	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
		if (keyword == CSS_VALUE_inherit)
		{
			for (; i < 4; i++)
				prop_list->AddTypeDeclL(props[i], keyword, important, GetCurrentOrigin());

			return OK;
		}
		else
			return INVALID;
	}

	while (i < m_val_array.GetCount() && CSS_is_number(m_val_array[i].token) && m_val_array[i].token != CSS_DIMEN && j < 4)
	{
		horiz_radius[j] = float(m_val_array[i].value.number.number);
		horiz_radius_type[j] = CSS_get_number_ext(m_val_array[i].token);

		if (!CSS_is_length_number_ext(horiz_radius_type[j]) && (horiz_radius_type[j] != CSS_PERCENTAGE))
			horiz_radius_type[j] = CSS_PX;

		i++; j++;
	}

	CompleteBorderRadiusArray(j, horiz_radius, horiz_radius_type);

	if (i < m_val_array.GetCount() && m_val_array[i].token == CSS_SLASH)
	{
		j = 0;
		i++;

		while (i < m_val_array.GetCount() && CSS_is_number(m_val_array[i].token) && m_val_array[i].token != CSS_DIMEN && j < 4)
		{
			vert_radius[j] = float(m_val_array[i].value.number.number);
			vert_radius_type[j] = CSS_get_number_ext(m_val_array[i].token);

			if (!CSS_is_length_number_ext(vert_radius_type[j]) && (horiz_radius_type[j] != CSS_PERCENTAGE))
				vert_radius_type[j] = CSS_PX;

			i++; j++;
		}

		CompleteBorderRadiusArray(j, vert_radius, vert_radius_type);
	}
	else
		for (j=0;j<4;j++)
		{
			vert_radius[j] = horiz_radius[j];
			vert_radius_type[j] = horiz_radius_type[j];
		}

	if (i == m_val_array.GetCount())
	{
		// Check that no values are negative
		for (j=0; j<4 && horiz_radius[j] >= 0 && vert_radius[j] >= 0; j++) {}
		if (j < 4)
			return INVALID;

		for (j=0; j<4; j++)
			prop_list->AddDeclL(props[j],
								horiz_radius[j], vert_radius[j],
								horiz_radius_type[j], vert_radius_type[j], important, GetCurrentOrigin());

		return OK;
	}
	else
		return INVALID;
}

/* Parse

  `none | <image> [ <number> | <percentage> ]{1,4} [ / <border-width>{1,4} ]? [ stretch | repeat | round ]{0,2}'

  into a corresponding list of CSS_generic_value's. */

CSS_Parser::DeclStatus
CSS_Parser::SetBorderImageL(CSS_property_list* prop_list, BOOL important)
{
	CSS_anchored_heap_arrays<uni_char> strings;
	ANCHOR(CSS_anchored_heap_arrays<uni_char>, strings);

#ifdef CSS_GRADIENT_SUPPORT
	CSS_anchored_heap_objects<CSS_Gradient> gradients;
	ANCHOR(CSS_anchored_heap_objects<CSS_Gradient>, gradients);
#endif // CSS_GRADIENT_SUPPORT

	if (m_val_array.GetCount() == 0)
		return INVALID;

	/* Check for `none' and `inherit' */

	if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
	{
		CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
		if (keyword == CSS_VALUE_none || keyword == CSS_VALUE_inherit)
		{
			prop_list->AddTypeDeclL(CSS_PROPERTY__o_border_image, keyword, important, GetCurrentOrigin());
			return OK;
		}
		else
			return INVALID;
	}

	enum {
		STATE_IMAGE,
		STATE_CUTS,
		STATE_WIDTHS,
		STATE_SCALE,

		STATE_INVALID,
		STATE_DONE
	} state = STATE_IMAGE;

	CSS_generic_value gen_arr[CSS_MAX_ARR_SIZE];
	int j = 0, i = 0;

	if (state == STATE_IMAGE)
	{
		/* <image> is really an <uri> */

		if (m_val_array[i].token == CSS_FUNCTION_URL)
		{
			const uni_char* border_image =
				m_input_buffer->GetURLL(m_base_url,
										m_val_array[0].value.str.start_pos,
										m_val_array[0].value.str.str_len).GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
			OP_ASSERT(border_image != NULL);

			uni_char* url = OP_NEWA_L(uni_char, uni_strlen(border_image)+1);
			strings.AddArrayL(url);
			uni_strcpy(url, border_image);
			gen_arr[j].SetValueType(CSS_FUNCTION_URL);
			gen_arr[j++].SetString(url);

			i++;
			state = STATE_CUTS;
		}
#ifdef CSS_GRADIENT_SUPPORT
		else if (CSS_is_gradient(m_val_array[i].token))
		{
			CSSValueType type = (CSSValueType) m_val_array[i].token;
			CSS_Gradient* gradient = SetGradientL(/* inout */ i, /* inout */ type);
			if (!gradient)
				return INVALID;

			gradients.AddObjectL(gradient);

			gen_arr[j].SetValueType(type);
			gen_arr[j++].SetGradient(gradient);
			i++;
			state = STATE_CUTS;
		}
#endif
		else
			state = STATE_INVALID;
	}

	if (state == STATE_CUTS)
	{
		const int cuts_start = i;

		while (i - cuts_start < 4 &&
			   i < m_val_array.GetCount() &&
			   CSS_is_number(m_val_array[i].token)
			   && state == STATE_CUTS)
		{
			int length_type = CSS_get_number_ext(m_val_array[i].token);
			if (length_type == CSS_PERCENTAGE || length_type == CSS_NUMBER)
			{
				gen_arr[j].SetValueType(length_type);
				gen_arr[j++].SetReal(float(m_val_array[i].value.number.number));

				i++;
			}
			else
				state = STATE_INVALID;
		}

		if (i - cuts_start)
			state = i < m_val_array.GetCount() ? STATE_WIDTHS : STATE_DONE;
		else
			state = STATE_INVALID;
	}

	if (state == STATE_WIDTHS)
	{
		if (m_val_array[i].token == CSS_SLASH)
		{
			gen_arr[j++].SetValueType(CSS_SLASH);
			i++;

			const int widths_start = i;

			while (i - widths_start < 4 &&
				   i < m_val_array.GetCount() &&
				   (CSS_is_number(m_val_array[i].token) || m_val_array[i].token == CSS_IDENT)
				   && state == STATE_WIDTHS)
			{
				if (m_val_array[i].token == CSS_IDENT)
				{
					CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
					gen_arr[j].SetValueType(CSS_IDENT);

					if (keyword == CSS_VALUE_thin || keyword == CSS_VALUE_medium || keyword == CSS_VALUE_thick)
						gen_arr[j++].SetType(keyword);
					else
						break;
				}
				else if (CSS_is_number(m_val_array[i].token) && m_val_array[i].token != CSS_DIMEN)
				{
					int length_type = CSS_get_number_ext(m_val_array[i].token);
					gen_arr[j].SetValueType(length_type);
					gen_arr[j++].SetReal(float(m_val_array[i].value.number.number));
				}
				else
					state = STATE_INVALID;

				i++;
			}

			if (state != STATE_INVALID)
			{
				if (i - widths_start)
					state = i < m_val_array.GetCount() ? STATE_SCALE : STATE_DONE;
				else
					state = STATE_INVALID;
			}
		}
		else
			state = STATE_SCALE;
	}

	if (state == STATE_SCALE)
	{
		int scale_start = i;

		while (i - scale_start < 2 &&
			   i < m_val_array.GetCount() &&
			   state == STATE_SCALE)
		{
			if (m_val_array[i].token == CSS_IDENT)
			{
				CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[i].value.str.start_pos, m_val_array[i].value.str.str_len);
				if (CSS_is_scale_val(keyword))
				{
					gen_arr[j].SetValueType(CSS_IDENT);
					gen_arr[j++].SetType(keyword);
				}
				else
				{
					state = STATE_INVALID;
				}
			}
			else
				state = STATE_INVALID;

			i++;
		}

		if (state != STATE_INVALID)
			state = STATE_DONE;
	}

	if (state == STATE_DONE)
	{
		prop_list->AddDeclL(CSS_PROPERTY__o_border_image, gen_arr, j, 1, important, GetCurrentOrigin());
		return OK;
	}
	else
		return INVALID;
}

bool CSS_Parser::AddCurrentSimpleSelector(short combinator)
{
	if (m_simple_selector_stack.Last() && m_current_selector)
	{
		BOOL is_last = (m_current_selector->LastSelector() == NULL);

		CSS_SimpleSelector* sel = (CSS_SimpleSelector*)m_simple_selector_stack.Last();
		sel->Out();
		m_current_selector->AddSimpleSelector(sel, combinator);

		if (!is_last)
		{
			CSS_SelectorAttribute* last_attr = sel->GetLastAttr();
			if (	last_attr
				&&	last_attr->GetType() == CSS_SEL_ATTR_TYPE_PSEUDO_CLASS
				&&	IsPseudoElement(last_attr->GetPseudoClass()))
			{
#ifdef CSS_ERROR_SUPPORT
				EmitErrorL(UNI_L("A pseudo element must be the last part of the selector."), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
				return false;
			}
		}
	}
	return true;
}

void CSS_Parser::DiscardRuleset()
{
	m_selector_list.Clear();
#ifdef MEDIA_HTML_SUPPORT
	m_cue_selector_list.Clear();
#endif // MEDIA_HTML_SUPPORT
	m_simple_selector_stack.Clear();
#ifdef CSS_ANIMATIONS
	m_current_keyframe_selectors.Clear();
#endif // CSS_ANIMATIONS
}

bool CSS_Parser::AddSelectorAttributeL(CSS_SelectorAttribute* sel_attr)
{
	CSS_SimpleSelector* cur_simple = GetCurrentSimpleSelector();
	OP_ASSERT(cur_simple);
	CSS_SelectorAttribute* prev_sel_attr = cur_simple->GetLastAttr();
	cur_simple->AddAttribute(sel_attr);

	if (	prev_sel_attr
		&&	prev_sel_attr->GetType() == CSS_SEL_ATTR_TYPE_PSEUDO_CLASS
		&&	IsPseudoElement(prev_sel_attr->GetPseudoClass()))
	{
#ifdef CSS_ERROR_SUPPORT
		EmitErrorL(UNI_L("A pseudo element must be the last part of the selector."), OpConsoleEngine::Error);
#endif // CSS_ERROR_SUPPORT
		return false;
	}
	else
		return true;
}

bool CSS_Parser::AddClassSelectorL(int start_pos, int length)
{
	CSS_SelectorAttribute* sel_attr = NULL;
	uni_char* text = m_input_buffer->GetString(start_pos, length);
	if (text)
	{
		if (m_hld_prof)
		{
			ReferencedHTMLClass* class_ref = m_hld_prof->GetLogicalDocument()->GetClassReference(text);
			if (class_ref)
			{
				sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_CLASS, class_ref));
				if (!sel_attr)
					class_ref->UnRef();
			}
		}
		else
		{
			sel_attr = OP_NEW(CSS_SelectorAttribute, (CSS_SEL_ATTR_TYPE_CLASS, 0, text));
			if (!sel_attr)
				OP_DELETEA(text);
		}
	}

	if (!sel_attr)
		LEAVE(OpStatus::ERR_NO_MEMORY);

	return AddSelectorAttributeL(sel_attr);
}

void CSS_Parser::AddCurrentSelectorL()
{
	OP_ASSERT(m_current_selector);

#ifdef MEDIA_HTML_SUPPORT
	if (m_in_cue)
	{
		m_current_selector->Into(&m_cue_selector_list);
		m_current_selector = NULL;
		return;
	}

	// If we have parsed a ::cue sub-selector:
	// 1) Rewrite the sub-selector(s),
	// 2) expand them into N selectors, and
	// 3) add them to the selector list.
	if (!m_cue_selector_list.Empty())
	{
		LEAVE_IF_ERROR(ApplyCueSelectors());
		return;
	}

	// If the current selector ends with a ::cue, we need to add an
	// additional simple selector to make it match the actual root
	// element.
	if (CSS_SimpleSelector* cur_simple = m_current_selector->LastSelector())
		if (CSS_SelectorAttribute* sel_attr = cur_simple->GetLastAttr())
			if (sel_attr->GetType() == CSS_SEL_ATTR_TYPE_PSEUDO_CLASS &&
				sel_attr->GetPseudoClass() == PSEUDO_CLASS_CUE)
				AddCueRootSelectorL();
#endif // MEDIA_HTML_SUPPORT

	m_current_selector->CalculateSpecificity();
	m_current_selector->Into(&m_selector_list);
	m_current_selector = NULL;
#ifdef SCOPE_CSS_RULE_ORIGIN
	m_rule_line_no = m_lexer.GetCurrentLineNo();
#endif // SCOPE_CSS_RULE_ORIGIN
}

bool CSS_Parser::AddMediaFeatureExprL(CSS_MediaFeature feature)
{
	CSS_MediaFeatureExpr* feature_expr = NULL;

	switch (feature)
	{
	case MEDIA_FEATURE_width:
	case MEDIA_FEATURE_height:
	case MEDIA_FEATURE_device_width:
	case MEDIA_FEATURE_device_height:
		if (m_val_array.GetCount() == 0)
		{
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature));
			break;
		}
		/* Fall-through */
	case MEDIA_FEATURE_max_width:
	case MEDIA_FEATURE_min_width:
	case MEDIA_FEATURE_max_height:
	case MEDIA_FEATURE_min_height:
	case MEDIA_FEATURE_max_device_width:
	case MEDIA_FEATURE_min_device_width:
	case MEDIA_FEATURE_max_device_height:
	case MEDIA_FEATURE_min_device_height:
		if (m_val_array.GetCount() == 1)
		{
			// non-negative <length>

			float length = 0;
			short unit = CSS_PX;

			if (CSS_is_length_number_ext(m_val_array[0].token) && m_val_array[0].value.number.number >= 0)
			{
				unit = m_val_array[0].token;
				length = static_cast<float>(m_val_array[0].value.number.number);
			}
			else
				if (!(m_val_array[0].token == CSS_NUMBER && m_val_array[0].value.number.number == 0 ||
					  m_val_array[0].token == CSS_INT_NUMBER && m_val_array[0].value.integer.integer == 0))
					break;

			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature, unit, length));
		}
		break;

	case MEDIA_FEATURE_grid:
	case MEDIA_FEATURE_color:
	case MEDIA_FEATURE_monochrome:
	case MEDIA_FEATURE_color_index:
	case MEDIA_FEATURE__o_paged:
		if (m_val_array.GetCount() == 0)
		{
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature));
			break;
		}
		/* Fall-through */
	case MEDIA_FEATURE_max_color:
	case MEDIA_FEATURE_min_color:
	case MEDIA_FEATURE_max_monochrome:
	case MEDIA_FEATURE_min_monochrome:
	case MEDIA_FEATURE_max_color_index:
	case MEDIA_FEATURE_min_color_index:
		if (m_val_array.GetCount() == 1 &&
			m_val_array[0].token == CSS_INT_NUMBER &&
			m_val_array[0].value.integer.integer >= 0 &&
			(feature != MEDIA_FEATURE_grid || m_val_array[0].value.integer.integer <= 1))
		{
			// <integer>
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature, m_val_array[0].value.integer.integer));
		}
		break;

	case MEDIA_FEATURE_scan:
		if (m_val_array.GetCount() == 0)
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature));
		else if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_progressive || keyword == CSS_VALUE_interlace)
				feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature, CSS_IDENT, keyword));
		}
		break;

	case MEDIA_FEATURE_view_mode:
		if (m_val_array.GetCount() == 0)
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature));
		else if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (CSS_is_viewmode_val(keyword))
			{
				feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature, CSS_IDENT, keyword));
				if (HLDProfile())
					HLDProfile()->GetFramesDocument()->SetViewModeSupported(static_cast<WindowViewMode>(CSS_ValueToWindowViewMode(keyword)));
			}
		}
		break;

	case MEDIA_FEATURE_orientation:
		if (m_val_array.GetCount() == 0)
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature));
		else if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (keyword == CSS_VALUE_portrait || keyword == CSS_VALUE_landscape)
				feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature, CSS_IDENT, keyword));
		}
		break;

	case MEDIA_FEATURE__o_widget_mode:
		if (m_val_array.GetCount() == 0)
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature));
		else if (m_val_array.GetCount() == 1 && m_val_array[0].token == CSS_IDENT)
		{
			CSSValue keyword = m_input_buffer->GetValueSymbol(m_val_array[0].value.str.start_pos, m_val_array[0].value.str.str_len);
			if (CSS_is_widgetmode_val(keyword))
				feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature, CSS_IDENT, keyword));
		}
		break;

	case MEDIA_FEATURE_resolution:
		if (m_val_array.GetCount() == 0)
		{
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature));
			break;
		}
		/* Fall-through */
	case MEDIA_FEATURE_max_resolution:
	case MEDIA_FEATURE_min_resolution:
		if (m_val_array.GetCount() == 1 &&
			CSS_is_resolution_ext(m_val_array[0].token) &&
			m_val_array[0].value.number.number > 0)
		{
			// <resolution>
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature, m_val_array[0].token, static_cast<float>(m_val_array[0].value.number.number)));
		}
		break;

	case MEDIA_FEATURE_aspect_ratio:
	case MEDIA_FEATURE_device_aspect_ratio:
	case MEDIA_FEATURE__o_device_pixel_ratio:
		if (m_val_array.GetCount() == 0)
		{
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature));
			break;
		}
		/* Fall-through */
	case MEDIA_FEATURE_max_aspect_ratio:
	case MEDIA_FEATURE_min_aspect_ratio:
	case MEDIA_FEATURE_max_device_aspect_ratio:
	case MEDIA_FEATURE_min_device_aspect_ratio:
	case MEDIA_FEATURE__o_max_device_pixel_ratio:
	case MEDIA_FEATURE__o_min_device_pixel_ratio:
		if (m_val_array.GetCount() == 3 &&
			m_val_array[0].token == CSS_INT_NUMBER && m_val_array[0].value.integer.integer > 0 &&
			m_val_array[1].token == CSS_SLASH &&
			m_val_array[2].token == CSS_INT_NUMBER && m_val_array[2].value.integer.integer > 0)
		{
			feature_expr = OP_NEW_L(CSS_MediaFeatureExpr, (feature,
														   CSS_RATIO,
														   MIN(m_val_array[0].value.integer.integer, SHRT_MAX),
														   MIN(m_val_array[2].value.integer.integer, SHRT_MAX)));
		}
		break;

	default:
		break;
	}

	if (feature_expr)
	{
		m_current_media_query->AddFeatureExpr(feature_expr);
		return true;
	}
	else
		return false;
}

void CSS_Parser::AddRuleL(CSS_Rule* rule)
{
	if (m_dom_rule || m_current_conditional_rule && m_current_conditional_rule->Suc())
	{
		if (m_replace_rule && m_dom_rule)
			m_stylesheet->ReplaceRule(m_hld_prof, m_dom_rule, rule, m_current_conditional_rule);
		else
			m_stylesheet->InsertRule(m_hld_prof, m_dom_rule, rule, m_current_conditional_rule);
	}
	else
	{
		LEAVE_IF_ERROR(m_stylesheet->AddRule(m_hld_prof, rule, m_current_conditional_rule));
	}

	if (rule->GetType() == CSS_Rule::MEDIA)
	{
		m_dom_rule = NULL;
		m_current_conditional_rule = static_cast<CSS_ConditionalRule*>(rule);
		if (m_hld_prof)
		{
			m_hld_prof->SetHasMediaStyle(static_cast<CSS_MediaRule*>(rule)->GetMediaTypes());
		}
	}
	else if (rule->GetType() == CSS_Rule::SUPPORTS)
	{
		m_dom_rule = NULL;
		m_current_conditional_rule = static_cast<CSS_ConditionalRule*>(rule);
	}
}

void CSS_Parser::AddImportL(const uni_char* url_str)
{
	OpStackAutoPtr<CSS_MediaObject> media_obj(m_current_media_object);
	m_current_media_object = NULL;

	URL url = g_url_api->GetURL(m_base_url, url_str);

	if (url.IsEmpty()
		|| GetBaseURL().Id(TRUE) == url.Id(TRUE) /* Avoid recursive imports */
		|| !m_hld_prof && url.Type() != URL_FILE /* Only allow file imports for user stylesheets */)
		return;

	const uni_char* base_url_str = m_base_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr();
	const uni_char* url_attr_str = base_url_str ? url_str : NULL;

	if (!url_attr_str)
		return;

	/* Handle ERA SSR where stylesheets should not be loaded unless they have handheld media. */
	BOOL is_ssr = m_hld_prof && m_hld_prof->GetFramesDocument()->GetWindow()->GetLayoutMode() == LAYOUT_SSR;

	if (is_ssr && m_hld_prof && (m_hld_prof->GetCSSMode() == CSS_FULL) &&
		!m_hld_prof->HasMediaStyle(CSS_MEDIA_TYPE_HANDHELD))
	{
		if (media_obj.get() && media_obj->GetMediaTypes() & CSS_MEDIA_TYPE_HANDHELD)
			m_hld_prof->SetHasMediaStyle(CSS_MEDIA_TYPE_HANDHELD);
		return;
	}

	/* Create inserted LINK element for the import rule. */
	HTML_Element* new_elm = NULL;
	HTML_Element* src_html_elm = m_stylesheet->GetHtmlElement();

	if (m_hld_prof)
	{
		HtmlAttrEntry ha_list[6];
		ha_list[0].ns_idx = NS_IDX_DEFAULT;
		ha_list[0].attr = Markup::HA_HREF;
		ha_list[0].value = const_cast<uni_char*>(url_attr_str);
		ha_list[0].value_len = uni_strlen(url_attr_str);

		int idx = 1;

		ha_list[idx].ns_idx = NS_IDX_XML;
		ha_list[idx].attr = XMLA_BASE;
		ha_list[idx].value = base_url_str;
		ha_list[idx++].value_len = uni_strlen(base_url_str);

		const uni_char* rel_val = src_html_elm->GetStringAttr(Markup::HA_REL);
		if (rel_val == NULL)
			rel_val = UNI_L("stylesheet");

		ha_list[idx].ns_idx = NS_IDX_DEFAULT;
		ha_list[idx].attr = Markup::HA_REL;
		ha_list[idx].value = rel_val;
		ha_list[idx++].value_len = uni_strlen(rel_val);

		const uni_char* title_val = src_html_elm->GetStringAttr(Markup::HA_TITLE);
		if (title_val)
		{
			ha_list[idx].ns_idx = NS_IDX_DEFAULT;
			ha_list[idx].attr = Markup::HA_TITLE;
			ha_list[idx].value = title_val;
			ha_list[idx++].value_len = uni_strlen(title_val);
		}

		ha_list[idx].attr = Markup::HA_NULL;

		new_elm = NEW_HTML_Element();

		if (!new_elm || OpStatus::IsMemoryError(new_elm->Construct(m_hld_prof, NS_IDX_HTML, Markup::HTE_LINK, ha_list, HE_INSERTED_BY_CSS_IMPORT)))
		{
			DELETE_HTML_Element(new_elm);
			LEAVE(OpStatus::ERR_NO_MEMORY);
		}
	}
	else
	{
		/* Local style sheet. */
		new_elm = g_cssManager->NewLinkElementL(url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI).CStr());
	}

	/* Set media from @import rule as a media attribute. */
	if (media_obj.get())
		new_elm->SetLinkStyleMediaObject(media_obj.release());

	/* Add the import link element into the dom tree under the parent stylesheet element. */
	new_elm->Under(src_html_elm);

	/* Insert the import rule into the stylesheet. */
	CSS_ImportRule* import_rule = OP_NEW_L(CSS_ImportRule, (new_elm));
	AddRuleL(import_rule);

#ifdef SCOPE_CSS_RULE_ORIGIN
	import_rule->SetLineNumber(m_lexer.GetCurrentLineNo());
#endif // SCOPE_CSS_RULE_ORIGIN

	/* No charset rules allowed after imports. */
	SetAllowLevel(ALLOW_IMPORT);

	/* Start loading the stylesheet. */
	if (m_hld_prof)
		LEAVE_IF_FATAL(m_hld_prof->HandleLink(new_elm));
	else
		LEAVE_IF_FATAL(g_cssManager->LoadCSS_URL(new_elm, m_stylesheet->GetUserDefined()));
}

void CSS_Parser::EndRulesetL()
{
	OP_ASSERT(m_current_props);

	CSS_StyleRule* style_rule = OP_NEW_L(CSS_StyleRule, ());

#ifdef SCOPE_CSS_RULE_ORIGIN
	style_rule->SetLineNumber(m_rule_line_no);
#endif // SCOPE_CSS_RULE_ORIGIN

	CSS_Selector* sel;
	while ((sel = static_cast<CSS_Selector*>(m_selector_list.First())))
	{
		sel->Out();
		style_rule->AddSelector(sel);
	}

	style_rule->SetPropertyList(m_hld_prof, m_current_props, TRUE);
	AddRuleL(style_rule);

	SetAllowLevel(ALLOW_STYLE);
}

void CSS_Parser::EndPageRulesetL(uni_char* name)
{
	OP_ASSERT(m_current_props);

	CSS_PageRule* page_rule = OP_NEW(CSS_PageRule, ());
	if (!page_rule)
	{
		OP_DELETEA(name);
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	page_rule->SetName(name);

	CSS_Selector* sel;
	while ((sel = static_cast<CSS_Selector*>(m_selector_list.First())))
	{
		sel->Out();
		page_rule->AddSelector(sel);
	}

	page_rule->SetPropertyList(m_hld_prof, m_current_props, TRUE);
	AddRuleL(page_rule);

	SetAllowLevel(ALLOW_STYLE);
}

void CSS_Parser::AddSupportsRuleL()
{
	OP_ASSERT(m_condition_list && !m_condition_list->next);
	CSS_SupportsRule* rule = OP_NEW_L(CSS_SupportsRule, (m_condition_list));
	m_condition_list = NULL;
	AddRuleL(rule);

	SetAllowLevel(ALLOW_STYLE);
}

void CSS_Parser::AddMediaRuleL()
{
	CSS_MediaRule* rule = OP_NEW_L(CSS_MediaRule, (m_current_media_object));
	m_current_media_object = NULL;
	AddRuleL(rule);

	SetAllowLevel(ALLOW_STYLE);
}

void CSS_Parser::TerminateLastDecl()
{
	if (m_last_decl && m_current_props)
	{
		CSS_decl* decl;
		while ((decl = m_current_props->GetLastDecl()) && decl != m_last_decl)
		{
			decl->Out();
			OP_DELETE(decl);
		}
	}
}

void CSS_Parser::EndSelectorL(uni_char* page_name)
{
	OP_ASSERT(m_dom_rule);

	CSS_Rule::Type type = m_dom_rule->GetType();

	OP_ASSERT(type == CSS_Rule::STYLE || type == CSS_Rule::PAGE);

	CSS_StyleRule* rule = static_cast<CSS_StyleRule*>(m_dom_rule);

	if (m_stylesheet)
		m_stylesheet->RuleRemoved(m_hld_prof, rule);

	rule->DeleteSelectors();

	CSS_Selector* sel;
	CSS_property_list* pl = rule->GetPropertyList();

	unsigned int flags = CSS_property_list::HAS_NONE;

	if (pl && type == CSS_Rule::STYLE)
		flags = pl->PostProcess(FALSE, TRUE);

	while ((sel = static_cast<CSS_Selector*>(m_selector_list.First())))
	{
		sel->Out();
		if (flags & CSS_property_list::HAS_CONTENT)
			sel->SetHasContentProperty(TRUE);
		if (flags & CSS_property_list::HAS_URL_PROPERTY)
			sel->SetMatchPrefetch(TRUE);
		rule->AddSelector(sel);
	}

	if (page_name)
	{
		OP_ASSERT(rule->GetType() == CSS_Rule::PAGE);
		static_cast<CSS_PageRule*>(rule)->SetName(page_name);
	}

	if (m_stylesheet)
		LEAVE_IF_ERROR(m_stylesheet->RuleInserted(m_hld_prof, rule));
}

#ifdef MEDIA_HTML_SUPPORT
void CSS_Parser::BeginCueSelector()
{
	OP_ASSERT(m_cue_selector_list.Empty());

	m_saved_current_selector = m_current_selector;
	m_current_selector = NULL;
	m_in_cue = TRUE;
}

void CSS_Parser::EndCueSelector()
{
	OP_ASSERT(m_in_cue);

	m_current_selector = m_saved_current_selector;
	m_saved_current_selector = NULL;
	m_in_cue = FALSE;
}

void CSS_Parser::AddCueRootSelectorL()
{
	OP_ASSERT(m_current_selector);

	CSS_SimpleSelector* simple_sel = OP_NEW_L(CSS_SimpleSelector, (Markup::CUEE_ROOT));
	simple_sel->SetNameSpaceIdx(NS_IDX_CUE);
	simple_sel->SetSerialize(FALSE);

	m_current_selector->PrependSimpleSelector(simple_sel);
}

static bool IsValidCueElementType(Markup::Type elm_type)
{
	switch (elm_type)
	{
	case Markup::CUEE_B:
	case Markup::CUEE_C:
	case Markup::CUEE_I:
	case Markup::CUEE_U:
	case Markup::CUEE_V:
		return true;

	case Markup::HTE_ANY:
		return true;
	}
	return false;
}

static bool RewriteCueRoot(CSS_SimpleSelector* simple_sel)
{
	OP_ASSERT(simple_sel && simple_sel->Pred() == NULL);

	// Does this simple selector match '*|*:root'?
	if (simple_sel->Suc() != NULL ||
		simple_sel->GetElm() != Markup::HTE_ANY ||
		simple_sel->GetNameSpaceIdx() != NS_IDX_ANY_NAMESPACE)
		return false;

	CSS_SelectorAttribute* sel_attr = simple_sel->GetFirstAttr();
	if (!sel_attr || sel_attr->Suc() != NULL)
		return false;

	if (sel_attr->GetType() != CSS_SEL_ATTR_TYPE_PSEUDO_CLASS ||
		sel_attr->GetPseudoClass() != PSEUDO_CLASS_ROOT)
		return false;

	// It matched, so rewrite it.
	if (sel_attr->IsNegated())
	{
		sel_attr->SetType(CSS_SEL_ATTR_TYPE_ELM);
		sel_attr->SetAttr(Markup::CUEE_ROOT);
		sel_attr->SetNsIdx(NS_IDX_CUE);
	}
	else
	{
		simple_sel->RemoveAttrs();
		simple_sel->SetElm(Markup::CUEE_ROOT);
	}

	simple_sel->SetNameSpaceIdx(NS_IDX_CUE);
	simple_sel->SetSerialize(FALSE);
	return true;
}

static void RewriteCueSelector(CSS_Selector* selector)
{
	CSS_SimpleSelector* simple_sel = selector->FirstSelector();

	// Match and rewrite :root to 'cue'|'root' (the 'root' element in
	// the CUE namespace).
	if (RewriteCueRoot(simple_sel))
		return;

	while (simple_sel)
	{
		// Force matching in the cue namespace and filter out any
		// invalid element references.
		if ((simple_sel->GetNameSpaceIdx() == NS_IDX_ANY_NAMESPACE ||
			 simple_sel->GetNameSpaceIdx() == NS_IDX_DEFAULT) &&
			IsValidCueElementType(simple_sel->GetElm()))
			simple_sel->SetNameSpaceIdx(NS_IDX_CUE);
		else
			simple_sel->SetNameSpaceIdx(NS_IDX_NOT_FOUND);

		// Disable serialization.
		simple_sel->SetSerialize(FALSE);

		simple_sel = simple_sel->Suc();
	}
}

// Clone all the simple selectors sel and put them in the cloned_simple_sels.
static OP_STATUS CloneSimpleSelectors(CSS_Selector* sel, Head& cloned_simple_sels)
{
	CSS_SimpleSelector* simple_sel = sel->FirstSelector();
	while (simple_sel)
	{
		CSS_SimpleSelector* simple_sel_clone = simple_sel->Clone();
		if (!simple_sel_clone)
			return OpStatus::ERR_NO_MEMORY;

		simple_sel_clone->Into(&cloned_simple_sels);

		simple_sel = simple_sel->Suc();
	}
	return OpStatus::OK;
}

OP_STATUS CSS_Parser::ApplyCueSelectors()
{
	OP_ASSERT(m_current_selector);

	OpAutoPtr<CSS_Selector> selector_cleaner(m_current_selector);
	CSS_Selector* cue_prefix = m_current_selector;
	m_current_selector = NULL;

	AutoDeleteHead cue_selector_list;
	cue_selector_list.Append(&m_cue_selector_list);

	CSS_SimpleSelector* cue_sel = NULL;
	BOOL is_first_selector = TRUE;
	TempBuffer serialized_selector;

	// "Prefix" (using the descendant combinator) the selectors with
	// the cue prefix selector:
	//
	// <prefix>::cue(a, b) => <prefix> a, <prefix> b
	//
	while (CSS_Selector* sel = static_cast<CSS_Selector*>(cue_selector_list.First()))
	{
		if (!is_first_selector)
			RETURN_IF_ERROR(serialized_selector.Append(", "));

		RETURN_IF_ERROR(sel->GetSelectorText(m_stylesheet, &serialized_selector));

		RewriteCueSelector(sel);

		AutoDeleteHead cloned_simple_sels;
		RETURN_IF_ERROR(CloneSimpleSelectors(cue_prefix, cloned_simple_sels));

		if (is_first_selector)
			cue_sel = static_cast<CSS_SimpleSelector*>(cloned_simple_sels.Last());

		sel->Out();

		// Prepend the prefix.
		sel->Prepend(cloned_simple_sels);

		// Only the first selector of the resulting N should be
		// serialized.
		sel->SetSerialize(is_first_selector);

		if (is_first_selector)
			is_first_selector = FALSE;

		sel->CalculateSpecificity();
		sel->Into(&m_selector_list);
	}

	// Set the serialization of the cue selectors on the ::cue
	// CSS_SelectorAttribute.
	uni_char* serialized_cue_str = UniSetNewStr(serialized_selector.GetStorage());
	cue_sel->GetLastAttr()->SetCueString(serialized_cue_str);
	return OpStatus::OK;
}
#endif // MEDIA_HTML_SUPPORT

#if defined(CSS_ERROR_SUPPORT)
void CSS_Parser::EmitErrorL(const uni_char* message, OpConsoleEngine::Severity severity)
{
	if (!g_console->IsLogging())
		return;

	// Maximum length of source text in error message
	const int max_src_length = 80;
	const int max_css_errors = 100;

	OpConsoleEngine::Message cmessage(OpConsoleEngine::CSS, severity);
	ANCHOR(OpConsoleEngine::Message, cmessage);

	// Set the window id.
	if (m_hld_prof && m_hld_prof->GetFramesDocument() && m_hld_prof->GetFramesDocument()->GetWindow())
		cmessage.window = m_hld_prof->GetFramesDocument()->GetWindow()->Id();

	HTML_Element* src_elm = m_stylesheet ? m_stylesheet->GetHtmlElement() : NULL;
	const URL* sheet_url = NULL;

	// Set the context.
	if (!m_stylesheet)
	{
		if (m_hld_prof)
		{
			if (m_hld_prof->GetCssErrorCount() >= max_css_errors)
				return;
			m_hld_prof->IncCssErrorCount();
		}

		if (GetDOMProperty() == -1)
			cmessage.context.SetL("HTML style attribute");
		else
			cmessage.context.SetL("DOM style property");
	}
	else if (!src_elm || src_elm->IsStyleElement())
	{
		cmessage.context.SetL("Inlined stylesheet");
	}
	else if (src_elm->IsLinkElement())
	{
		cmessage.context.SetL("Linked-in stylesheet");
	}

	// Set the url of the stylesheet. Use the document url if inlined.
	if (!src_elm || src_elm->IsStyleElement())
	{
		sheet_url = &m_base_url;
	}
	else if (src_elm->IsLinkElement())
		sheet_url = m_stylesheet->GetHRef(m_hld_prof ? m_hld_prof->GetLogicalDocument() : NULL);

	if (sheet_url)
		sheet_url->GetAttribute(URL::KUniName, cmessage.url);

	// Set the error message itself.
	cmessage.message.SetL(message);

	LEAVE_IF_ERROR(cmessage.message.AppendFormat(UNI_L("\n\nLine %d:\n  "), m_document_line + m_lexer.GetCurrentLineNo() - 1));
	int line_pos = m_lexer.GetCurrentLinePos();
	if (line_pos > max_src_length)
		line_pos = max_src_length;
	uni_char* current_line = m_lexer.GetCurrentLineString(max_src_length);
	if (!current_line)
		LEAVE(OpStatus::ERR_NO_MEMORY);
	uni_char* tmp = current_line;
	while (*tmp)
		if (*tmp == '\t')
			*tmp++ = ' ';
		else
			tmp++;
	ANCHOR_ARRAY(uni_char, current_line);
	cmessage.message.AppendL(current_line);
	ANCHOR_ARRAY_DELETE(current_line);
	cmessage.message.AppendL("\n  ");
	cmessage.message.ReserveL(cmessage.message.Length()+line_pos+1);
	for (int i=0; i<line_pos; i++)
	{
		cmessage.message.AppendL("-");
	}
	cmessage.message.AppendL("^");
	g_console->PostMessageL(&cmessage);
	if (!m_stylesheet && m_hld_prof && m_hld_prof->GetCssErrorCount() >= max_css_errors)
	{
		cmessage.message.SetL("Too many CSS errors... bailing out.");
		g_console->PostMessageL(&cmessage);
	}
}

void CSS_Parser::InvalidDeclarationL(DeclStatus error, short property)
{
	const char* prop_name;
	prop_name = GetCSS_PropertyName(property);

	OP_ASSERT(prop_name);

	OpString msg;
	ANCHOR(OpString, msg);
	msg.SetL(prop_name);
	msg.MakeLower();
	LEAVE_IF_ERROR(msg.Insert(0, "Invalid value for property: "));

	EmitErrorL(msg.CStr(), OpConsoleEngine::Error);
}

#endif // CSS_ERROR_SUPPORT

void CSS_Parser::InvalidBlockHit()
{
	if (m_stylesheet && !m_stylesheet->HasRules())
		m_stylesheet->SetHasSyntacticallyValidCSSHeader(FALSE);
}

CSS_decl::DeclarationOrigin
CSS_Parser::GetCurrentOrigin() const
{
#ifdef CSS_ANIMATIONS
	if (m_decl_type == DECL_KEYFRAMES)
		return CSS_decl::ORIGIN_ANIMATIONS;
	else
#endif
		if (m_user)
			return CSS_decl::ORIGIN_USER;
		else
			return CSS_decl::ORIGIN_AUTHOR;
}
