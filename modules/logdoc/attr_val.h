/// Numeric codes stored for enumerated HTML attributes

#ifndef _ATTR_VAL_H_
#define _ATTR_VAL_H_

enum
{
	ATTR_VALUE_yes = 0x01,
	ATTR_VALUE_no = 0x02,
	ATTR_VALUE_disc = 0x03,
	ATTR_VALUE_square = 0x04,
	ATTR_VALUE_put = 0x05,
	ATTR_VALUE_post = 0x06,
	ATTR_VALUE_get = 0x07,
	ATTR_VALUE_mouseover = 0x08,
	ATTR_VALUE_fileopen = 0x09,
	ATTR_VALUE_silent = 0x0a,
	ATTR_VALUE_infinite = 0x0b,
	ATTR_VALUE_false = 0x0c,
	ATTR_VALUE_en = 0x0d,
	ATTR_VALUE_justify = 0x0e,
	ATTR_VALUE_relative = 0x0f,
	ATTR_VALUE_pixels = 0x10,
	ATTR_VALUE_all = 0x11,
	ATTR_VALUE_absmiddle = 0x12,
	ATTR_VALUE_middle = 0x13,
	ATTR_VALUE_top = 0x14,
	ATTR_VALUE_absbottom = 0xa4,
	ATTR_VALUE_bottom = 0x15,
	ATTR_VALUE_baseline = 0x16,
	ATTR_VALUE_auto = 0x17,
	ATTR_VALUE_both = 0x18,
	
	ATTR_VALUE_text = 0x20,
	ATTR_VALUE_checkbox = 0x21,
	ATTR_VALUE_radio = 0x22,
	ATTR_VALUE_submit = 0x23,
	ATTR_VALUE_reset = 0x24,
	ATTR_VALUE_hidden = 0x25,
	ATTR_VALUE_image = 0x26,
	ATTR_VALUE_password = 0x27,
	ATTR_VALUE_button = 0x28,

	ATTR_VALUE_file = 0x2a,
	ATTR_VALUE_url = 0x2b,
	ATTR_VALUE_date = 0x2c,
	ATTR_VALUE_week = 0x2d,
	ATTR_VALUE_time = 0x2e,
	ATTR_VALUE_email = 0x2f,
	ATTR_VALUE_number = 0x30,
	ATTR_VALUE_range = 0x31,
	ATTR_VALUE_month = 0x32,
	ATTR_VALUE_datetime = 0x33,
	ATTR_VALUE_datetime_local = 0x34,
	ATTR_VALUE_search = 0x35,
	ATTR_VALUE_tel = 0x36,
	ATTR_VALUE_color = 0x37,
	
	ATTR_VALUE_ltr = 0x40,
	ATTR_VALUE_rtl = 0x41,
	
	ATTR_VALUE_up = 0xa4,
	ATTR_VALUE_down = 0xa5,
	
	ATTR_VALUE_circle = 0x43,
	ATTR_VALUE_circ = 0x44,
	ATTR_VALUE_poly = 0x45,
	ATTR_VALUE_polygon = 0x46,
	ATTR_VALUE_default = 0x47,
	
	ATTR_VALUE_left = 0x80,
	ATTR_VALUE_right = 0x81,
	ATTR_VALUE_center = 0x82,
	ATTR_VALUE_hide = 0x83,
	ATTR_VALUE_show = 0x84,
	ATTR_VALUE_inherit = 0x85,
	
	ATTR_VALUE_slide = 0x88,
	ATTR_VALUE_scroll = 0x89,
	ATTR_VALUE_alternate = 0x8a,
	
	ATTR_VALUE_box = 0x90,
	ATTR_VALUE_lhs = 0x91,
	ATTR_VALUE_rhs = 0x92,
	ATTR_VALUE_void = 0x93,
	ATTR_VALUE_above = 0x94,
	ATTR_VALUE_below = 0x95,
	ATTR_VALUE_hsides = 0x96,
	ATTR_VALUE_vsides = 0x97,
	ATTR_VALUE_border = 0x98,
	
	ATTR_VALUE_none = 0xa0,
	ATTR_VALUE_rows = 0xa1,
	ATTR_VALUE_cols = 0xa2,
	ATTR_VALUE_groups = 0xa3
};

///\define ATTR_is_inputtype_val Returns TRUE if v is an input type code.
#define ATTR_is_inputtype_val(v)	((v) >= ATTR_VALUE_text && (v) <= ATTR_VALUE_color)
#define ATTR_is_frame_val(v)	((v) >= ATTR_VALUE_box && (v) <= ATTR_VALUE_border)
#define ATTR_is_rules_val(v)	(((v) >= ATTR_VALUE_none && (v) <= ATTR_VALUE_groups) || (v) == ATTR_VALUE_box)

#endif // _ATTR_VAL_H_
