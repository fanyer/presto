/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2003 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domhtml/htmlelem.h"

/* Along with the atom for a reflected numeric property, we also need to
   specify its type and if the property has a default value in case the
   attribute is absent or outside of range. See DOM_HTMLElement's
   declaration for details on the different type kinds.

   The following convenience macros can be used to construct numeric
   property 'type descriptors'. */

/** A reflected numeric property for 'atom' at the given type. No explicit
    default, meaning 0 will be used. */
#define NUMERIC_TYPE(atom, type) (((unsigned)atom << 4) | ((unsigned)type << 1))

/** A reflected numeric property for 'atom' at the given 'type' and with
    a default value. */
#define NUMERIC_TYPE_DEFAULT(atom, type, value) (((unsigned)atom << 4) | ((unsigned)type << 1) | 1), (unsigned)value

/** A no-default 'long' numeric property. */
#define INT_TYPE(atom) NUMERIC_TYPE(atom, DOM_HTMLElement::TYPE_LONG)

/** A no-default 'unsigned long' numeric property. */
#define UINT_TYPE(atom) NUMERIC_TYPE(atom, DOM_HTMLElement::TYPE_ULONG)

/* For string properties, we distinguish the different kinds
   of behaviour they have for null (corresponding to the
   TreatNullAs WebIDL extended attribute.) A string can either
   have TreatNullAs=EmptyString or not. The latter meaning that
   a DOMString-valued property should convert the (null) value
   to "null" first. The former, that it instead will be "". */

#define STRING_TYPE(atom) (static_cast<unsigned>(atom))
#define STRING_TYPE_NULL_AS_EMPTY(atom) ((static_cast<unsigned>(atom)) | 0x80000000)

static const unsigned string_property_type[] =
{
	STRING_TYPE(OP_ATOM_type),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned string_property_align[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned string_property_name[] =
{
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned numeric_property_tabIndex[] =
{
	INT_TYPE(OP_ATOM_tabIndex),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom boolean_property_disabled[] =
{
	OP_ATOM_disabled,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const OpAtom boolean_property_compact[] =
{
	OP_ATOM_compact,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};


static const unsigned html_string_properties[] =
{
	STRING_TYPE(OP_ATOM_version),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned link_string_properties[] =
{
	STRING_TYPE(OP_ATOM_charset),
	STRING_TYPE(OP_ATOM_href),
	STRING_TYPE(OP_ATOM_hreflang),
	STRING_TYPE(OP_ATOM_media),
	STRING_TYPE(OP_ATOM_rel),
	STRING_TYPE(OP_ATOM_rev),
	STRING_TYPE(OP_ATOM_target),
	STRING_TYPE(OP_ATOM_type),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned meta_string_properties[] =
{
	STRING_TYPE(OP_ATOM_content),
	STRING_TYPE(OP_ATOM_httpEquiv),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_scheme),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned base_string_properties[] =
{
	STRING_TYPE(OP_ATOM_href),
	STRING_TYPE(OP_ATOM_target),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned isindex_string_properties[] =
{
	STRING_TYPE(OP_ATOM_prompt),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned style_string_properties[] =
{
	STRING_TYPE(OP_ATOM_media),
	STRING_TYPE(OP_ATOM_type),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned body_string_properties[] =
{
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_aLink),
	STRING_TYPE(OP_ATOM_background),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_bgColor),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_link),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_text),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_vLink),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned form_string_properties[] =
{
	STRING_TYPE(OP_ATOM_acceptCharset),
	STRING_TYPE(OP_ATOM_action),
	STRING_TYPE(OP_ATOM_encoding),
	STRING_TYPE(OP_ATOM_enctype),
	STRING_TYPE(OP_ATOM_method),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_target),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom form_boolean_properties[] =
{
	OP_ATOM_noValidate,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};


static const unsigned select_string_properties[] =
{
	STRING_TYPE(OP_ATOM_autocomplete),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned select_numeric_properties[] =
{
	UINT_TYPE(OP_ATOM_size),
	INT_TYPE(OP_ATOM_tabIndex),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom select_boolean_properties[] =
{
	OP_ATOM_autofocus,
	OP_ATOM_disabled,
	OP_ATOM_multiple,
	OP_ATOM_required,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned optgroup_string_properties[] =
{
	STRING_TYPE(OP_ATOM_label),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned option_string_properties[] =
{
	STRING_TYPE(OP_ATOM_label),
	STRING_TYPE(OP_ATOM_value),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom option_boolean_properties[] =
{
	OP_ATOM_defaultSelected,
	OP_ATOM_disabled,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned input_string_properties[] =
{
	STRING_TYPE(OP_ATOM_accept),
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_alt),
	STRING_TYPE(OP_ATOM_autocomplete),
	STRING_TYPE(OP_ATOM_defaultValue),
	STRING_TYPE(OP_ATOM_dirName),
	STRING_TYPE(OP_ATOM_formAction),
	STRING_TYPE(OP_ATOM_formEnctype),
	STRING_TYPE(OP_ATOM_formMethod),
	STRING_TYPE(OP_ATOM_formTarget),
	STRING_TYPE(OP_ATOM_inputmode),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_max),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_min),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_pattern),
	STRING_TYPE(OP_ATOM_placeholder),
	STRING_TYPE(OP_ATOM_src),
	STRING_TYPE(OP_ATOM_step),
	STRING_TYPE(OP_ATOM_type),
	STRING_TYPE(OP_ATOM_useMap),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned input_numeric_properties[] =
{
	INT_TYPE(OP_ATOM_height),
	NUMERIC_TYPE_DEFAULT(OP_ATOM_maxLength, DOM_HTMLElement::TYPE_LONG_NON_NEGATIVE, -1),
	NUMERIC_TYPE_DEFAULT(OP_ATOM_size, DOM_HTMLElement::TYPE_ULONG_NON_ZERO, 20),
	INT_TYPE(OP_ATOM_tabIndex),
	INT_TYPE(OP_ATOM_width),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom input_boolean_properties[] =
{
	OP_ATOM_autofocus,
	OP_ATOM_checked,
	OP_ATOM_defaultChecked,
	OP_ATOM_disabled,
	OP_ATOM_formNoValidate,
	OP_ATOM_indeterminate,
	OP_ATOM_multiple,
	OP_ATOM_readOnly,
	OP_ATOM_required,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned textarea_string_properties[] =
{
	STRING_TYPE(OP_ATOM_dirName),
	STRING_TYPE(OP_ATOM_inputmode),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_pattern),
	STRING_TYPE(OP_ATOM_placeholder),
	STRING_TYPE(OP_ATOM_wrap),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned textarea_numeric_properties[] =
{
	NUMERIC_TYPE_DEFAULT(OP_ATOM_cols, DOM_HTMLElement::TYPE_ULONG_NON_ZERO, 20),
	NUMERIC_TYPE_DEFAULT(OP_ATOM_maxLength, DOM_HTMLElement::TYPE_LONG_NON_NEGATIVE, -1),
	NUMERIC_TYPE_DEFAULT(OP_ATOM_rows, DOM_HTMLElement::TYPE_ULONG_NON_ZERO, 2),
	INT_TYPE(OP_ATOM_tabIndex),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom textarea_boolean_properties[] =
{
	OP_ATOM_autofocus,
	OP_ATOM_disabled,
	OP_ATOM_readOnly,
	OP_ATOM_required,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned button_string_properties[] =
{
	STRING_TYPE(OP_ATOM_formAction),
	STRING_TYPE(OP_ATOM_formEnctype),
	STRING_TYPE(OP_ATOM_formMethod),
	STRING_TYPE(OP_ATOM_formTarget),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_type),
	STRING_TYPE(OP_ATOM_value),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom button_boolean_properties[] =
{
	OP_ATOM_autofocus,
	OP_ATOM_disabled,
	OP_ATOM_formNoValidate,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned label_string_properties[] =
{
	STRING_TYPE(OP_ATOM_htmlFor),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned legend_string_properties[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned output_string_properties[] =
{
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned progress_numeric_properties[] =
{
	NUMERIC_TYPE_DEFAULT(OP_ATOM_max, DOM_HTMLElement::TYPE_DOUBLE, 1),
	NUMERIC_TYPE(OP_ATOM_value, DOM_HTMLElement::TYPE_DOUBLE),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned meter_numeric_properties[] =
{
	NUMERIC_TYPE(OP_ATOM_high, DOM_HTMLElement::TYPE_DOUBLE),
	NUMERIC_TYPE(OP_ATOM_low, DOM_HTMLElement::TYPE_DOUBLE),
	NUMERIC_TYPE(OP_ATOM_max, DOM_HTMLElement::TYPE_DOUBLE),
	NUMERIC_TYPE(OP_ATOM_min, DOM_HTMLElement::TYPE_DOUBLE),
	NUMERIC_TYPE(OP_ATOM_optimum, DOM_HTMLElement::TYPE_DOUBLE),
	NUMERIC_TYPE(OP_ATOM_value, DOM_HTMLElement::TYPE_DOUBLE),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom keygen_boolean_properties[] =
{
	OP_ATOM_autofocus,
	OP_ATOM_disabled,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned keygen_string_properties[] =
{
	STRING_TYPE(OP_ATOM_challenge),
	STRING_TYPE(OP_ATOM_keytype),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned olist_numeric_properties[] =
{
	INT_TYPE(OP_ATOM_start),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom olist_boolean_properties[] =
{
	OP_ATOM_compact,
	OP_ATOM_reversed,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned li_numeric_properties[] =
{
	INT_TYPE(OP_ATOM_value),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned quote_string_properties[] =
{
	STRING_TYPE(OP_ATOM_cite),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned pre_numeric_properties[] =
{
	UINT_TYPE(OP_ATOM_width),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned br_string_properties[] =
{
	STRING_TYPE(OP_ATOM_clear),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned font_string_properties[] =
{
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_color),
	STRING_TYPE(OP_ATOM_face),
	STRING_TYPE(OP_ATOM_size),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned hr_string_properties[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_size),
	STRING_TYPE(OP_ATOM_width),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom hr_boolean_properties[] =
{
	OP_ATOM_noShade,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned mod_string_properties[] =
{
	STRING_TYPE(OP_ATOM_cite),
	STRING_TYPE(OP_ATOM_dateTime),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned anchor_string_properties[] =
{
	STRING_TYPE(OP_ATOM_charset),
	STRING_TYPE(OP_ATOM_coords),
	STRING_TYPE(OP_ATOM_href),
	STRING_TYPE(OP_ATOM_hreflang),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_rel),
	STRING_TYPE(OP_ATOM_rev),
	STRING_TYPE(OP_ATOM_shape),
	STRING_TYPE(OP_ATOM_target),
	STRING_TYPE(OP_ATOM_type),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned image_string_properties[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_alt),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_border),
	STRING_TYPE(OP_ATOM_crossOrigin),
	STRING_TYPE(OP_ATOM_longDesc),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_src),
	STRING_TYPE(OP_ATOM_useMap),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static unsigned image_numeric_properties[] =
{
	UINT_TYPE(OP_ATOM_height),
	UINT_TYPE(OP_ATOM_hspace),
	UINT_TYPE(OP_ATOM_vspace),
	UINT_TYPE(OP_ATOM_width),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom image_boolean_properties[] =
{
	OP_ATOM_isMap,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned object_string_properties[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_archive),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_border),
	STRING_TYPE(OP_ATOM_classId),
	STRING_TYPE(OP_ATOM_code),
	STRING_TYPE(OP_ATOM_codeBase),
	STRING_TYPE(OP_ATOM_codeType),
	STRING_TYPE(OP_ATOM_data),
	STRING_TYPE(OP_ATOM_height),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_standby),
	STRING_TYPE(OP_ATOM_type),
	STRING_TYPE(OP_ATOM_useMap),
	STRING_TYPE(OP_ATOM_width),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned object_numeric_properties[] =
{
	UINT_TYPE(OP_ATOM_hspace),
	INT_TYPE(OP_ATOM_tabIndex),
	UINT_TYPE(OP_ATOM_vspace),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom object_boolean_properties[] =
{
	OP_ATOM_declare,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned param_string_properties[] =
{
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_type),
	STRING_TYPE(OP_ATOM_value),
	STRING_TYPE(OP_ATOM_valueType),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned applet_string_properties[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_alt),
	STRING_TYPE(OP_ATOM_archive),
	STRING_TYPE(OP_ATOM_code),
	STRING_TYPE(OP_ATOM_codeBase),
	STRING_TYPE(OP_ATOM_height),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_object),
	STRING_TYPE(OP_ATOM_width),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned applet_numeric_properties[] =
{
	UINT_TYPE(OP_ATOM_hspace),
	UINT_TYPE(OP_ATOM_vspace),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned area_string_properties[] =
{
	STRING_TYPE(OP_ATOM_alt),
	STRING_TYPE(OP_ATOM_coords),
	STRING_TYPE(OP_ATOM_href),
	STRING_TYPE(OP_ATOM_shape),
	STRING_TYPE(OP_ATOM_target),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom area_boolean_properties[] =
{
	OP_ATOM_noHref,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned script_string_properties[] =
{
	STRING_TYPE(OP_ATOM_charset),
	STRING_TYPE(OP_ATOM_src),
	STRING_TYPE(OP_ATOM_type),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom script_boolean_properties[] =
{
	OP_ATOM_defer,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned table_string_properties[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_bgColor),
	STRING_TYPE(OP_ATOM_border),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_cellPadding),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_cellSpacing),
	STRING_TYPE(OP_ATOM_frame),
	STRING_TYPE(OP_ATOM_rules),
	STRING_TYPE(OP_ATOM_summary),
	STRING_TYPE(OP_ATOM_width),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned tablecol_string_properties[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_ch),
	STRING_TYPE(OP_ATOM_chOff),
	STRING_TYPE(OP_ATOM_vAlign),
	STRING_TYPE(OP_ATOM_width),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned tablecol_numeric_properties[] =
{
	UINT_TYPE(OP_ATOM_span),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned tablesection_string_properties[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_ch),
	STRING_TYPE(OP_ATOM_chOff),
	STRING_TYPE(OP_ATOM_vAlign),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned tablerow_string_properties[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_bgColor),
	STRING_TYPE(OP_ATOM_ch),
	STRING_TYPE(OP_ATOM_chOff),
	STRING_TYPE(OP_ATOM_height),
	STRING_TYPE(OP_ATOM_vAlign),
	STRING_TYPE(OP_ATOM_width),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned tablecell_string_properties[] =
{
	STRING_TYPE(OP_ATOM_abbr),
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_axis),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_bgColor),
	STRING_TYPE(OP_ATOM_ch),
	STRING_TYPE(OP_ATOM_chOff),
	STRING_TYPE(OP_ATOM_headers),
	STRING_TYPE(OP_ATOM_height),
	STRING_TYPE(OP_ATOM_scope),
	STRING_TYPE(OP_ATOM_vAlign),
	STRING_TYPE(OP_ATOM_width),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned tablecell_numeric_properties[] =
{
	/* colSpan is currently specified as being limited to non-zero values,
	   and must consequently throw on setting 0. No one implements that
	   restriction, so choose to follow that behaviour here. */
	NUMERIC_TYPE(OP_ATOM_colSpan, DOM_HTMLElement::TYPE_ULONG),
	NUMERIC_TYPE_DEFAULT(OP_ATOM_rowSpan, DOM_HTMLElement::TYPE_ULONG, 1),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom tablecell_boolean_properties[] =
{
	OP_ATOM_noWrap,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned frameset_string_properties[] =
{
	STRING_TYPE(OP_ATOM_cols),
	STRING_TYPE(OP_ATOM_rows),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned frame_string_properties[] =
{
	STRING_TYPE(OP_ATOM_frameBorder),
	STRING_TYPE(OP_ATOM_longDesc),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_marginHeight),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_marginWidth),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_scrolling),
	STRING_TYPE(OP_ATOM_src),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom frame_boolean_properties[] =
{
	OP_ATOM_noResize,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned iframe_string_properties[] =
{
	STRING_TYPE(OP_ATOM_align),
	STRING_TYPE(OP_ATOM_frameBorder),
	STRING_TYPE(OP_ATOM_height),
	STRING_TYPE(OP_ATOM_longDesc),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_marginHeight),
	STRING_TYPE_NULL_AS_EMPTY(OP_ATOM_marginWidth),
	STRING_TYPE(OP_ATOM_name),
	STRING_TYPE(OP_ATOM_scrolling),
	STRING_TYPE(OP_ATOM_src),
	STRING_TYPE(OP_ATOM_width),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

#ifdef MEDIA_HTML_SUPPORT
static const OpAtom audio_boolean_properties[] =
{
	OP_ATOM_autoplay,
	OP_ATOM_controls,
	OP_ATOM_defaultMuted,
	OP_ATOM_loop,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned audio_string_properties[] =
{
	STRING_TYPE(OP_ATOM_preload),
	STRING_TYPE(OP_ATOM_src),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned video_numeric_properties[] =
{
	UINT_TYPE(OP_ATOM_height),
	UINT_TYPE(OP_ATOM_width),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom video_boolean_properties[] =
{
	OP_ATOM_autoplay,
	OP_ATOM_controls,
	OP_ATOM_defaultMuted,
	OP_ATOM_loop,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned video_string_properties[] =
{
	STRING_TYPE(OP_ATOM_poster),
	STRING_TYPE(OP_ATOM_preload),
	STRING_TYPE(OP_ATOM_src),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned source_string_properties[] =
{
	STRING_TYPE(OP_ATOM_media),
	STRING_TYPE(OP_ATOM_src),
	STRING_TYPE(OP_ATOM_type),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom track_boolean_properties[] =
{
	OP_ATOM_default,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

static const unsigned track_string_properties[] =
{
	STRING_TYPE(OP_ATOM_kind),
	STRING_TYPE(OP_ATOM_label),
	STRING_TYPE(OP_ATOM_src),
	STRING_TYPE(OP_ATOM_srclang),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};
#endif //MEDIA_HTML_SUPPORT


#ifdef CANVAS_SUPPORT
static const unsigned canvas_numeric_properties[] =
{
	UINT_TYPE(OP_ATOM_height),
	UINT_TYPE(OP_ATOM_width),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};
#endif // CANVAS_SUPPORT

static const unsigned marquee_string_properties[] =
{
	STRING_TYPE(OP_ATOM_behavior),
	STRING_TYPE(OP_ATOM_direction),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned marquee_numeric_properties[] =
{
	UINT_TYPE(OP_ATOM_height),
	UINT_TYPE(OP_ATOM_hspace),
	INT_TYPE(OP_ATOM_loop),
	UINT_TYPE(OP_ATOM_scrollAmount),
	UINT_TYPE(OP_ATOM_scrollDelay),
	UINT_TYPE(OP_ATOM_vspace),
	UINT_TYPE(OP_ATOM_width),
	INT_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const unsigned time_string_properties[] =
{
	STRING_TYPE(OP_ATOM_dateTime),
	STRING_TYPE(OP_ATOM_ABSOLUTELY_LAST_ENUM)
};

static const OpAtom time_boolean_properties[] =
{
	OP_ATOM_pubDate,
	OP_ATOM_ABSOLUTELY_LAST_ENUM
};

#undef STRING_TYPE
#undef STRING_TYPE_NULL_AS_EMPTY
#undef NUMERIC_TYPE
#undef NUMERIC_TYPE_DEFAULT
#undef INT_TYPE
#undef UINT_TYPE

/* static */ const unsigned *
DOM_HTMLElement::GetTabIndexTypeDescriptor()
{
    return numeric_property_tabIndex;
}

#ifdef DOM_NO_COMPLEX_GLOBALS
# define DOM_HTML_PROPERTIES_START() void DOM_htmlProperties_Init(DOM_GlobalData *global_data) { DOM_HTMLElement::SimpleProperties *properties = global_data->htmlProperties;
# define DOM_HTML_PROPERTIES_ITEM(string_, numeric_, boolean_) properties->string = string_; properties->boolean = boolean_; properties->numeric = numeric_; ++properties;
# define DOM_HTML_PROPERTIES_ITEM_LAST(string_, numeric_, boolean_) properties->string = string_; properties->boolean = boolean_; properties->numeric = numeric_;
# define DOM_HTML_PROPERTIES_END() }
#else // DOM_NO_COMPLEX_GLOBALS
# define DOM_HTML_PROPERTIES_START() const DOM_HTMLElement::SimpleProperties g_DOM_htmlProperties[] = {
# define DOM_HTML_PROPERTIES_ITEM(string, numeric, boolean) { string, boolean, numeric },
# define DOM_HTML_PROPERTIES_ITEM_LAST(string, numeric, boolean) { string, boolean, numeric }
# define DOM_HTML_PROPERTIES_END() };
#endif // DOM_NO_COMPLEX_GLOBALS


/* Important: this array must be kept in sync with the enumeration
   DOM_Runtime::HTMLElementPrototype.  See its definition for what
   else it needs to be in sync with. */
DOM_HTML_PROPERTIES_START()
	// Unknown
	DOM_HTML_PROPERTIES_ITEM(0, 0, 0)
	// Html
	DOM_HTML_PROPERTIES_ITEM(html_string_properties, 0, 0)
	// Head
	DOM_HTML_PROPERTIES_ITEM(0, 0, 0)
	// Link
	DOM_HTML_PROPERTIES_ITEM(link_string_properties, 0, 0)
	// Title
	DOM_HTML_PROPERTIES_ITEM(0, 0, 0)
	// Meta
	DOM_HTML_PROPERTIES_ITEM(meta_string_properties, 0, 0)
	// Base
	DOM_HTML_PROPERTIES_ITEM(base_string_properties, 0, 0)
	// IsIndex
	DOM_HTML_PROPERTIES_ITEM(isindex_string_properties, 0, 0)
	// Style
	DOM_HTML_PROPERTIES_ITEM(style_string_properties, 0, 0)
	// Body
	DOM_HTML_PROPERTIES_ITEM(body_string_properties, 0, 0)
	// Form
	DOM_HTML_PROPERTIES_ITEM(form_string_properties, 0, form_boolean_properties)
	// Select
	DOM_HTML_PROPERTIES_ITEM(select_string_properties, select_numeric_properties, select_boolean_properties)
	// OptGroup
	DOM_HTML_PROPERTIES_ITEM(optgroup_string_properties, 0, boolean_property_disabled)
	// Option
	DOM_HTML_PROPERTIES_ITEM(option_string_properties, 0, option_boolean_properties)
	// Input
	DOM_HTML_PROPERTIES_ITEM(input_string_properties, input_numeric_properties, input_boolean_properties)
	// TextArea
	DOM_HTML_PROPERTIES_ITEM(textarea_string_properties, textarea_numeric_properties, textarea_boolean_properties)
	// Button
	DOM_HTML_PROPERTIES_ITEM(button_string_properties, numeric_property_tabIndex, button_boolean_properties)
	// Label
	DOM_HTML_PROPERTIES_ITEM(label_string_properties, 0, 0)
	// FieldSet
	DOM_HTML_PROPERTIES_ITEM(0, 0, boolean_property_disabled)
	// Legend
	DOM_HTML_PROPERTIES_ITEM(legend_string_properties, 0, 0)
	// Output
	DOM_HTML_PROPERTIES_ITEM(output_string_properties, 0, 0)
	// DataList
	DOM_HTML_PROPERTIES_ITEM(0, 0, 0)
	// Progress
	DOM_HTML_PROPERTIES_ITEM(0, progress_numeric_properties, 0)
	// Meter
	DOM_HTML_PROPERTIES_ITEM(0, meter_numeric_properties, 0)
	// Keygen
	DOM_HTML_PROPERTIES_ITEM(keygen_string_properties, 0, keygen_boolean_properties)
	// UList
	DOM_HTML_PROPERTIES_ITEM(string_property_type, 0, boolean_property_compact)
	// OList
	DOM_HTML_PROPERTIES_ITEM(string_property_type, olist_numeric_properties, olist_boolean_properties)
	// DList
	DOM_HTML_PROPERTIES_ITEM(0, 0, boolean_property_compact)
	// Directory
	DOM_HTML_PROPERTIES_ITEM(0, 0, boolean_property_compact)
	// Menu
	DOM_HTML_PROPERTIES_ITEM(0, 0, boolean_property_compact)
	// LI
	DOM_HTML_PROPERTIES_ITEM(string_property_type, li_numeric_properties, 0)
	// Div
	DOM_HTML_PROPERTIES_ITEM(string_property_align, 0, 0)
	// Paragraph
	DOM_HTML_PROPERTIES_ITEM(string_property_align, 0, 0)
	// Heading
	DOM_HTML_PROPERTIES_ITEM(string_property_align, 0, 0)
	// Quote
	DOM_HTML_PROPERTIES_ITEM(quote_string_properties, 0, 0)
	// Pre
	DOM_HTML_PROPERTIES_ITEM(0, pre_numeric_properties, 0)
	// Br
	DOM_HTML_PROPERTIES_ITEM(br_string_properties, 0, 0)
	// Font
	DOM_HTML_PROPERTIES_ITEM(font_string_properties, 0, 0)
	// HR
	DOM_HTML_PROPERTIES_ITEM(hr_string_properties, 0, hr_boolean_properties)
	// Mod
	DOM_HTML_PROPERTIES_ITEM(mod_string_properties, 0, 0)
#ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
	// Xml
	DOM_HTML_PROPERTIES_ITEM(0, 0, 0)
#endif // #ifdef PARTIAL_XMLELEMENT_SUPPORT // See bug 286692
	// Anchor
	DOM_HTML_PROPERTIES_ITEM(anchor_string_properties, numeric_property_tabIndex, 0)
	// Image
	DOM_HTML_PROPERTIES_ITEM(image_string_properties, image_numeric_properties, image_boolean_properties)
	// Object
	DOM_HTML_PROPERTIES_ITEM(object_string_properties, object_numeric_properties, object_boolean_properties)
	// Param
	DOM_HTML_PROPERTIES_ITEM(param_string_properties, 0, 0)
	// Applet
	DOM_HTML_PROPERTIES_ITEM(applet_string_properties, applet_numeric_properties, 0)
	// Embed
	DOM_HTML_PROPERTIES_ITEM(0, 0, 0)
	// Map
	DOM_HTML_PROPERTIES_ITEM(string_property_name, 0, 0)
	// Area
	DOM_HTML_PROPERTIES_ITEM(area_string_properties, numeric_property_tabIndex, area_boolean_properties)
	// Script
	DOM_HTML_PROPERTIES_ITEM(script_string_properties, 0, script_boolean_properties)
	// Table
	DOM_HTML_PROPERTIES_ITEM(table_string_properties, 0, 0)
	// TableCaption
	DOM_HTML_PROPERTIES_ITEM(string_property_align, 0, 0)
	// TableCol
	DOM_HTML_PROPERTIES_ITEM(tablecol_string_properties, tablecol_numeric_properties, 0)
	// TableSection
	DOM_HTML_PROPERTIES_ITEM(tablesection_string_properties, 0, 0)
	// TableRow
	DOM_HTML_PROPERTIES_ITEM(tablerow_string_properties, 0, 0)
	// TableCell
	DOM_HTML_PROPERTIES_ITEM(tablecell_string_properties, tablecell_numeric_properties, tablecell_boolean_properties)
	// FrameSet
	DOM_HTML_PROPERTIES_ITEM(frameset_string_properties, 0, 0)
	// Frame
	DOM_HTML_PROPERTIES_ITEM(frame_string_properties, 0, frame_boolean_properties)
	// IFrame
	DOM_HTML_PROPERTIES_ITEM(iframe_string_properties, 0, frame_boolean_properties)
#ifdef MEDIA_HTML_SUPPORT
	// Media
	DOM_HTML_PROPERTIES_ITEM(0, 0, 0)
	// Audio
	DOM_HTML_PROPERTIES_ITEM(audio_string_properties, 0, audio_boolean_properties)
	// Video
	DOM_HTML_PROPERTIES_ITEM(video_string_properties, video_numeric_properties, video_boolean_properties)
	// Source
	DOM_HTML_PROPERTIES_ITEM(source_string_properties, 0, 0)
	// Track
	DOM_HTML_PROPERTIES_ITEM(track_string_properties, 0, track_boolean_properties)
#endif // MEDIA_HTML_SUPPORT
#ifdef CANVAS_SUPPORT
	// Canvas
	DOM_HTML_PROPERTIES_ITEM(0, canvas_numeric_properties, 0)
#endif // CANVAS_SUPPORT
	// Marquee
	DOM_HTML_PROPERTIES_ITEM(marquee_string_properties, marquee_numeric_properties, 0)
	// Time
	DOM_HTML_PROPERTIES_ITEM_LAST(time_string_properties, 0, time_boolean_properties)
DOM_HTML_PROPERTIES_END()
