#ifndef _SVG_PCH_H_
#define _SVG_PCH_H_

#ifdef SVG_SUPPORT

// The following macro truncates a size_t to an int, with possible loss of precision
#define SIZE_T_TO_INT(x)	(int)MIN(x,INT_MAX)

// Helpers for color component extraction
#define GetSVGColorRValue(x) OP_GET_R_VALUE(x)
#define GetSVGColorGValue(x) OP_GET_G_VALUE(x)
#define GetSVGColorBValue(x) OP_GET_B_VALUE(x)
#define GetSVGColorAValue(x) ((BYTE) ((x) >> 24))

#if !defined(HAS_COMPLEX_GLOBALS)
# define SVG_CONST_ARRAY_INIT(name) init_##name()
#else
# define SVG_CONST_ARRAY_INIT(name) ((void)0)
#endif // !HAS_COMPLEX_GLOBALS

// This makes sure we save original strings for attributes
#define SVG_11_DOM_COMPATIBLE

// Adobe (and Ikivo since their content is affected?) applies the
// transform in this order:
//
// CTM = base_transforms * animate_motion_transform * animate_transform_transforms
//
// The spec seems to indicate this behavior:
//
// CTM = base_transforms * animate_transform_transforms * animate_motion_transform
//
// The first formula is used if the define is set, otherwise the latter is used.
#define SVG_APPLY_ANIMATE_MOTION_IN_BETWEEN

#ifdef SVG_LOG_ERRORS
# define SVG_LOG_ERRORS_PARSEATTRIBUTE
#endif // SVG_LOG_ERRORS

#if 0
#define SVG_DEBUG_OPBPATH_TESTCOMPRESSION
#endif // 0

//#define SVG_DEBUG_SHOW_FONTNAMES
#ifdef SVG_FULL_11
# define SVG_SUPPORT_MARKERS
#endif // SVG_FULL_11

#if defined(SVG_SUPPORT_GRADIENTS) || defined(SVG_SUPPORT_PATTERNS)
# define SVG_SUPPORT_PAINTSERVERS
#endif // SVG_SUPPORT_GRADIENTS || SVG_SUPPORT_PATTERNS

#ifndef SVG_DISABLE_SVG_DRAGGING
#define SVG_SUPPORT_PANNING
#endif // SVG_DISABLE_SVG_DRAGGING

// Enables external references in 'use' elements
# define SVG_SUPPORT_EXTERNAL_USE

// Quadratic curves are kept as quadratic through normalization, they're not normalized to cubic curves
#define SVG_KEEP_QUADRATIC_CURVES

// Allow negative startOffset attribute values (disallowed in 1.1 full)
#define SVG_ALLOW_NEGATIVE_STARTOFFSET

// Enables text selection support
#ifndef HAS_NOTEXTSELECTION
# define SVG_SUPPORT_TEXTSELECTION
// Enables support for 'editable'
# define SVG_SUPPORT_EDITABLE
#endif // !HAS_NOTEXTSELECTION

// Avoid switching font in some cases to be able to support svgfont ligatures - fixes Acid3 (at the expense of fontswitching)
#define SVG_GET_NEXT_TEXTFRAGMENT_LIGATURE_HACK

// Ericsson TV portal used space in some xlink:href attributes and expected them to be treated the same as an empty string
#define SVG_TREAT_WHITESPACE_AS_EMPTY_XLINK_HREF

// Does several rendering pass to try to paint a couple of small areas
// instead of a huge union of the areas. This can be useful when things
// are being animated at different places in the image with lots a static
// content in between. The downside is the overhead, and when the areas
// are covered by the same objects so that the same job is done several times.
#define SVG_OPTIMIZE_RENDER_MULTI_PASS

#if SVG_RENDERING_TIMESLICE_MS > 0
# define SVG_INTERRUPTABLE_RENDERING
#endif // SVG_RENDERING_TIMESLICE_MS

#if !defined(SVG_THROTTLING_FPS_LOW)
#define SVG_THROTTLING_FPS_LOW SVG_THROTTLING_FPS_HIGH
#else // ! SVG_THROTTLING_FPS_LOW
#if SVG_THROTTLING_FPS_LOW <= 0
#undef SVG_THROTTLING_FPS_LOW
#define SVG_THROTTLING_FPS_LOW 1
#endif // SVG_THROTTLING_FPS_LOW <= 0
#endif // ! SVG_THROTTLING_FPS_LOW
#if SVG_THROTTLING_FPS_HIGH > 0
# define SVG_THROTTLING
#endif // SVG_THROTTLING_FPS_HIGH > 0

// Enables extended navigation code (1.2 Tiny-style nav-*/focusHighlight et.c)
#define SVG_SUPPORT_NAVIGATION

// Enables support for svg:image with xlink:href="foo.svg"
#define SVG_SUPPORT_SVG_IN_IMAGE

// Enables support for svg:video and svg:audio
#if defined(SVG_TINY_12) && defined(MEDIA_PLAYER_SUPPORT)
# define SVG_SUPPORT_MEDIA

// Enables support for 'transformBehavior="geometric"'
# define SVG_SUPPORT_TRANSFORMED_VIDEO
// Enables support for 'overlay="none"'
# define SVG_SUPPORT_COMPOSED_VIDEO
// Should the above be inverted? (i.e. 'Disables ...')
#endif // SVG_TINY_12 && MEDIA_PLAYER_SUPPORT

#ifdef _DEBUG
// Dump information about screenboxes
//# define SVG_DEBUG_DIRTYRECTS

// Draws purple borders around areas that are rendered in an SVG
//# define SVG_DEBUG_RENDER_RECTS

// Draws thin orange borders around every screen box in the system
//# define SVG_DEBUG_SCREEN_BOXES

// Does a simple consistency check of the dependency graph (on changes)
//# define SVG_DEBUG_DEPENDENCY_GRAPH
#endif // _DEBUG

#ifdef _DEBUG
#include "modules/pi/OpSystemInfo.h"
#define SVG_PROBE_START(name)								\
	OP_NEW_DBG(name, "svg_probe");							\
	double start_clock = g_op_time_info->GetRuntimeMS();
#define SVG_PROBE_END()													\
	double passed_time = (g_op_time_info->GetRuntimeMS()				\
						  - start_clock) / 1000;				\
	OP_DBG(("Measured time: %g", passed_time));
#else
#define SVG_PROBE_START(name)
#define SVG_PROBE_END()
#endif // _DEBUG

// Helper macros to make the code more readable
#define NUM_AS_ATTR_VAL(x) reinterpret_cast<void*>(static_cast<INTPTR>(x))
#define ATTR_VAL_AS_NUM(x) reinterpret_cast<INTPTR>(x)

// Include debug settings here so it is available everywhere inside
// the svg module
#include "modules/svg/src/SVGDebugSettings.h"

#include "modules/svg/svg_number.h"

#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131e-016
#endif

#ifdef PLATFORM_FONTSWITCHING
#define SVG_DECLARE_ELLIPSIS								\
	const uni_char ellipsis_codepoint = 0x2026;				\
	const uni_char* ellipsis_string = &ellipsis_codepoint;	\
	unsigned ellipsis_length = 1
#else
#define SVG_DECLARE_ELLIPSIS											\
	const uni_char ellipsis_string[3] = {'.', '.', '.'}; /* ARRAY OK 2011-04-05 fs */ \
	unsigned ellipsis_length = 3
#endif // PLATFORM_FONTSWITCHING

#define RETURN_FALSE_IF(cond)\
	if (cond) return OpBoolean::IS_FALSE;

//
// The meaning of life^Werror codes:
//
// WRONG_NUMBER_OF_ARGUMENTS - Unused?
// INVALID_ARGUMENT - ?
// BAD_PARAMETER - ?
// MISSING_REQ_ARGUMENT - An attribute that is required was missing
// ATTRIBUTE_ERROR - An attribute was invalid due do malformed syntax or out-of-bounds values
// DATA_NOT_LOADED_ERROR - Required data has not (yet) been loaded
// NOT_SUPPORTED_IN_TINY - Used when certain features (gradients) are turned off,
//						   to indicate lack of support
// NOT_YET_IMPLEMENTED - Indicates that something is not yet implemented (internal?)
// GENERAL_DOCUMENT_ERROR - Indicates that something was severly broken in the document
// INVALID_ANIMATION - Animation could not be performed due to incorrect specifications
// TYPE_ERROR - An object had the wrong type
// INTERNAL_ERROR - Some went terribly wrong
//
// Error codes used by the traverser:
// ELEMENT_IS_INVISIBLE - Returned from visualtraverse when an element is invisible, then it's translated
//						  into another error depending on traversemode. The switch-traversal needs to separate
//						  invisible elements from elements that didn't fulfill the test-attributes' requirements.
//
// SKIP_SUBTREE - Skips the rest of the children in the current container
//				  Used mostly in 'switch' processing
//
// SKIP_ELEMENT - Skips processing of the current element
//				  will skip the following steps in the traversal
//
// SKIP_CHILDREN - Skip processing of the children of this element (if any)
//

#if defined(_DEBUG) || defined(SVG_LOG_ERRORS)
extern const uni_char* const g_svg_errorstrings[];
#endif // _DEBUG || SVG_LOG_ERRORS

class OpSVGStatus : public OpStatus
{
public:
	enum
	{
		// Success values
		COLOR_IS_NONE = USER_SUCCESS,
		COLOR_IS_CURRENTCOLOR,
		INHERIT_VALUE,
		VALUE_IS_PERCENT,
		VALUE_IS_EMPTY,
		VALUE_IS_INDEFINITE,
		DUPLICATE_ENTRY,

		// Error values
		// NOTE: If any error value is added after TIMED_OUT the m_svg_errorstrings count in svg_module.h needs to be adjusted as well
		WRONG_NUMBER_OF_ARGUMENTS = USER_ERROR+1,
		INVALID_ARGUMENT,
		BAD_PARAMETER,
		MISSING_REQ_ARGUMENT,
		ATTRIBUTE_ERROR,
		DATA_NOT_LOADED_ERROR,
		NOT_SUPPORTED_IN_TINY,
		NOT_YET_IMPLEMENTED,
		GENERAL_DOCUMENT_ERROR,
		INVALID_ANIMATION,
		TYPE_ERROR,
		INTERNAL_ERROR,

		// Pseudo-errors, used internally to raise various conditions.
		ELEMENT_IS_INVISIBLE,
		SKIP_SUBTREE,
		SKIP_ELEMENT,
		SKIP_CHILDREN,
		TIMED_OUT
	};

#if defined(_DEBUG) || defined(SVG_LOG_ERRORS)
public:
        static const uni_char* GetErrorString(OP_STATUS error);
#endif // _DEBUG || SVG_LOG_ERRORS
};

#ifndef SVG_DOM
#define SVGLOAD   DOM_EVENT_NONE
#define	SVGUNLOAD DOM_EVENT_NONE
#define	SVGABORT  DOM_EVENT_NONE
#define	SVGERROR  DOM_EVENT_NONE
#define	SVGRESIZE DOM_EVENT_NONE
#define	SVGSCROLL DOM_EVENT_NONE
#define	SVGZOOM   DOM_EVENT_NONE
#define BEGINEVENT	DOM_EVENT_NONE
#define ENDEVENT	DOM_EVENT_NONE
#define REPEATEVENT	DOM_EVENT_NONE
#endif // SVG_DOM

// Note: These macros were intended to ease a transition to core-3
// that never happened and should at some point be cleaned up.

#ifndef SVG_DOCUMENT_CLASS
# define SVG_DOCUMENT_CLASS FramesDocument
#endif

// Some HTML_Element methods take a LogicalDocument on core-3, where
// they took a FramesDocument on core-2. This macro can be used in
// thoses places. Example HTML_Element::MarkExtraDirty
#define LOGDOC_DOC(X) (X)

// LayoutProperties::CreateCascade took a HLDocProfile on core-3, but
// a layout workplace on core-3. Use this macro in the transition.
#define LAYOUT_WORKPLACE(X) (X)

// On core-3, there are methods in logdoc that return reference to a
// URL instead of a pointer to it.
#define LOGDOC_GETURL(X) *(X)

// On core-3, there SetUrlAttr in docutil takes a reference to a URL
// instead of a pointer to it.
#define DOCUTIL_SETURLATTR(X) (X)

#ifdef SVG_DOUBLE_PRECISION_FLOATS
typedef double SVG_ANIMATION_INTERVAL_POSITION;
#else // !SVG_DOUBLE_PRECISION_FLOATS
typedef float SVG_ANIMATION_INTERVAL_POSITION;
#endif // !SVG_DOUBLE_PRECISION_FLOATS

#endif // SVG_SUPPORT
#endif // _SVG_PCH_H_
