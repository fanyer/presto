#ifndef LAYOUT_INTERNAL_H
#define LAYOUT_INTERNAL_H

#include "modules/logdoc/logdocenum.h" // for ATTR_FIRST_ALLOWED

#define FONT_NUMBER_NOT_SET				-1

#define MARGIN_VALUE_AUTO				LAYOUT_COORD_MIN

#define BORDER_STYLE_NOT_SET			CSS_VALUE_UNSPECIFIED
#define BORDER_WIDTH_NOT_SET			-1

#define DEFAULT_BORDER_COLOR	OP_RGB(200,200,200)

// List item marker constants.

/* Multiplies the x by a fraction that specifies the offset above the baseline,
   where the bullet list marker's top edge is. */
#define MULT_BY_MARKER_OFFSET_RATIO(x) (x * 11 / 24)

/* Multiplies x by a fraction (1/3) that specifies the width of the bullet list marker. */
#define MULT_BY_MARKER_WIDTH_RATIO(x) (x / 3)

// A space between the list marker horizontal end and the start of the following inline box.
#define MARKER_INNER_OFFSET 8

// A space between the list marker horizontal end and the start of the following inline box (handheld mode).
#define MARKER_INNER_OFFSET_HANDHELD 6

#define NORMAL_LINE_HEIGHT_FACTOR 1.2

#endif // LAYOUT_INTERNAL_H
