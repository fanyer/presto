/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SVG_DOM_INTERFACES_H
#define SVG_DOM_INTERFACES_H

#ifdef SVG_DOM

#include "modules/svg/svg_external_types.h"
#include "modules/logdoc/namespace.h"
#include "modules/util/adt/opvector.h"

#ifndef SVG_DOCUMENT_CLASS
# define SVG_DOCUMENT_CLASS FramesDocument
#endif // !SVG_DOCUMENT_CLASS

class SVG_DOCUMENT_CLASS;
class DOM_Object;
class LogicalDocument;

/**
 *
 * These datatypes follows the SVG DOM tightly. There are some
 * exceptions, when the return value is OP_STATUS instead of the
 * parameter sent in. In those cases the DOM module is responsible for
 * sending back the correct return value upon success, or throw the
 * approriate exception upon error.
 *
 * All properties are changed to a getter and setter pair. This is
 * because changes to properties in the DOM interfaces can require
 * updates to other properties. This dependency code is kept inside
 * the SVG module.
 *
 * The naming is kept as close to the DOM specification as possible
 * without breaking the Opera coding standard. In short, parameter
 * names are changed from paramName to param_name, and method-names
 * from methodName() to MethodName().
 *
 */

class SVGDOMItem;
class SVGDOMNumber;
class SVGDOMLength;
class SVGDOMAngle;
class SVGDOMPoint;
class SVGDOMMatrix;
class SVGDOMRect;
class SVGDOMString;
class SVGDOMTransform;
class SVGDOMAnimatedValue;
class SVGDOMList;
class SVGDOMStringList;
class SVGDOMEnumeration;
class SVGDOMPathSeg;
class SVGDOMPreserveAspectRatio;
class SVGDOMPath;
class SVGDOMRGBColor;
class HTML_Element;
class DOM_Environment;

enum SVGDOMItemType
{
	SVG_DOM_ITEMTYPE_UNKNOWN = 0,
	SVG_DOM_ITEMTYPE_NUMBER,
	SVG_DOM_ITEMTYPE_MATRIX,
	SVG_DOM_ITEMTYPE_TRANSFORM,
	SVG_DOM_ITEMTYPE_LENGTH,
	SVG_DOM_ITEMTYPE_LIST,
	SVG_DOM_ITEMTYPE_STRING_LIST,
	SVG_DOM_ITEMTYPE_RECT,
	SVG_DOM_ITEMTYPE_STRING,
	SVG_DOM_ITEMTYPE_PATHSEG,
	SVG_DOM_ITEMTYPE_POINT,
	SVG_DOM_ITEMTYPE_CSSVALUE,
	SVG_DOM_ITEMTYPE_COLOR,
	SVG_DOM_ITEMTYPE_ANGLE,
	SVG_DOM_ITEMTYPE_ENUM,
	SVG_DOM_ITEMTYPE_BOOLEAN,
	SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_0,
	SVG_DOM_ITEMTYPE_NUMBER_OPTIONAL_NUMBER_1,
	SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_0,
	SVG_DOM_ITEMTYPE_INTEGER_OPTIONAL_INTEGER_1,
	SVG_DOM_ITEMTYPE_PAINT,
	SVG_DOM_ITEMTYPE_CSSPRIMITIVEVALUE,
	SVG_DOM_ITEMTYPE_CSSRGBCOLOR,
	SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO,
	SVG_DOM_ITEMTYPE_PATH, // 1.2 tiny type
	SVG_DOM_ITEMTYPE_RGBCOLOR // 1.2 tiny type
};

/**
 * Main interface between DOM and SVG.
 * Separates the implementation from creation.
 */
class SVGDOM
{
public:
	enum TextContentLengthAdjustType
	{
		SVG_DOM_TEXTCONTENT_LENGTHADJUST_UNKNOWN			= 0,
		SVG_DOM_TEXTCONTENT_LENGTHADJUST_SPACING			= 1,
		SVG_DOM_TEXTCONTENT_LENGTHADJUST_SPACINGANDGLYPHS   = 2
	};

	enum TextPathMethodType
	{
		SVG_DOM_TEXTPATH_METHODTYPE_UNKNOWN					= 0,
		SVG_DOM_TEXTPATH_METHODTYPE_ALIGN					= 1,
		SVG_DOM_TEXTPATH_METHODTYPE_STRETCH					= 2
	};

	enum TextPathSpacingType
	{
		SVG_DOM_TEXTPATH_SPACINGTYPE_UNKNOWN				= 0,
		SVG_DOM_TEXTPATH_SPACINGTYPE_AUTO					= 1,
		SVG_DOM_TEXTPATH_SPACINGTYPE_EXACT					= 2
	};

	enum SVGUnitType
	{
		SVG_DOM_SVG_UNIT_TYPE_UNKNOWN           = 0,
		SVG_DOM_SVG_UNIT_TYPE_USERSPACEONUSE    = 1,
		SVG_DOM_SVG_UNIT_TYPE_OBJECTBOUNDINGBOX = 2
	};

	enum SVGZoomAndPanType
	{
		SVG_DOM_SVG_ZOOMANDPAN_UNKNOWN = 0,
		SVG_DOM_SVG_ZOOMANDPAN_DISABLE = 1,
		SVG_DOM_SVG_ZOOMANDPAN_MAGNIFY = 2
	};

	enum SVGMarkerUnitType
	{
		SVG_DOM_SVG_MARKERUNITS_UNKNOWN		   = 0,
		SVG_DOM_SVG_MARKERUNITS_USERSPACEONUSE = 1,
		SVG_DOM_SVG_MARKERUNITS_STROKEWIDTH	   = 2
	};

	enum SVGMarkerOrientType
	{
		SVG_DOM_SVG_MARKER_ORIENT_UNKNOWN = 0,
		SVG_DOM_SVG_MARKER_ORIENT_AUTO 	  = 1,
		SVG_DOM_SVG_MARKER_ORIENT_ANGLE   = 2
	};

	enum SVGSpreadMethod
	{
		SVG_DOM_SVG_SPREADMETHOD_UNKNOWN = 0,
		SVG_DOM_SVG_SPREADMETHOD_PAD     = 1,
		SVG_DOM_SVG_SPREADMETHOD_REFLECT = 2,
		SVG_DOM_SVG_SPREADMETHOD_REPEAT  = 3
	};

	enum SVGFEBlendMode
	{
		SVG_DOM_SVG_FEBLEND_MODE_UNKNOWN  = 0,
		SVG_DOM_SVG_FEBLEND_MODE_NORMAL   = 1,
		SVG_DOM_SVG_FEBLEND_MODE_MULTIPLY = 2,
		SVG_DOM_SVG_FEBLEND_MODE_SCREEN   = 3,
		SVG_DOM_SVG_FEBLEND_MODE_DARKEN   = 4,
		SVG_DOM_SVG_FEBLEND_MODE_LIGHTEN  = 5
	};

	enum SVGFEColorMatrixType
	{
		SVG_DOM_SVG_FECOLORMATRIX_TYPE_UNKNOWN          = 0,
		SVG_DOM_SVG_FECOLORMATRIX_TYPE_MATRIX           = 1,
		SVG_DOM_SVG_FECOLORMATRIX_TYPE_SATURATE         = 2,
		SVG_DOM_SVG_FECOLORMATRIX_TYPE_HUEROTATE        = 3,
		SVG_DOM_SVG_FECOLORMATRIX_TYPE_LUMINANCETOALPHA = 4
	};

	enum SVGComponentTransferFunctionType
	{
		SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_UNKNOWN  = 0,
		SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_IDENTITY = 1,
		SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_TABLE    = 2,
		SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_DISCRETE = 3,
		SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_LINEAR   = 4,
		SVG_DOM_SVG_FECOMPONENTTRANSFER_TYPE_GAMMA    = 5
	};

	enum SVGFECompositeOperator
	{
		SVG_DOM_SVG_FECOMPOSITE_OPERATOR_UNKNOWN    = 0,
		SVG_DOM_SVG_FECOMPOSITE_OPERATOR_OVER       = 1,
		SVG_DOM_SVG_FECOMPOSITE_OPERATOR_IN         = 2,
		SVG_DOM_SVG_FECOMPOSITE_OPERATOR_OUT        = 3,
		SVG_DOM_SVG_FECOMPOSITE_OPERATOR_ATOP       = 4,
		SVG_DOM_SVG_FECOMPOSITE_OPERATOR_XOR        = 5,
		SVG_DOM_SVG_FECOMPOSITE_OPERATOR_ARITHMETIC = 6
	};

	enum SVGFEConvolveMatrixEdgeMode
	{
		SVG_DOM_SVG_EDGEMODE_UNKNOWN   = 0,
		SVG_DOM_SVG_EDGEMODE_DUPLICATE = 1,
		SVG_DOM_SVG_EDGEMODE_WRAP      = 2,
		SVG_DOM_SVG_EDGEMODE_NONE      = 3
	};

	enum SVGChannelSelector
	{
		SVG_DOM_SVG_CHANNEL_UNKNOWN = 0,
		SVG_DOM_SVG_CHANNEL_R       = 1,
		SVG_DOM_SVG_CHANNEL_G       = 2,
		SVG_DOM_SVG_CHANNEL_B       = 3,
		SVG_DOM_SVG_CHANNEL_A       = 4
	};

	enum SVGFEMorphologyOperator
	{
		SVG_DOM_SVG_MORPHOLOGY_OPERATOR_UNKNOWN = 0,
		SVG_DOM_SVG_MORPHOLOGY_OPERATOR_ERODE   = 1,
		SVG_DOM_SVG_MORPHOLOGY_OPERATOR_DILATE  = 2
	};

	enum SVGFETurbulenceType
	{
		SVG_DOM_SVG_TURBULENCE_TYPE_UNKNOWN      = 0,
		SVG_DOM_SVG_TURBULENCE_TYPE_FRACTALNOISE = 1,
		SVG_DOM_SVG_TURBULENCE_TYPE_TURBULENCE   = 2
	};

	enum SVGFETurbulenceStitchType
	{
		SVG_DOM_SVG_STITCHTYPE_UNKNOWN  = 0,
		SVG_DOM_SVG_STITCHTYPE_STITCH   = 1,
		SVG_DOM_SVG_STITCHTYPE_NOSTITCH = 2
	};

	enum SVGExceptionCode
	{
		SVG_WRONG_TYPE_ERR = 0,
		SVG_INVALID_VALUE_ERR = 1,
		SVG_MATRIX_NOT_INVERTABLE = 2
	};

	enum SVGNavigationType
	{
		SVG_DOM_NAV_AUTO           = 1,
		SVG_DOM_NAV_NEXT           = 2,
		SVG_DOM_NAV_PREV           = 3,
		SVG_DOM_NAV_UP             = 4,
		SVG_DOM_NAV_UP_RIGHT       = 5,
		SVG_DOM_NAV_RIGHT          = 6,
		SVG_DOM_NAV_DOWN_RIGHT     = 7,
		SVG_DOM_NAV_DOWN           = 8,
		SVG_DOM_NAV_DOWN_LEFT      = 9,
		SVG_DOM_NAV_LEFT           = 10,
		SVG_DOM_NAV_UP_LEFT        = 11
	};

	enum SVGBBoxType
	{
		SVG_BBOX_NORMAL = 0,
		SVG_BBOX_SCREEN = 1,
		SVG_BBOX_COMPUTED = 2
	};

	/**
	 * Creates an SVGDOMItem object outside of any document trees.
	 * If an object of the type object_type can't be created,
	 * OpStatus::ERR_NOT_SUPPORTED is returned. If there was an OOM
	 * condition OpStatus::ERR_NO_MEMORY is returned. In the unlikely
	 * event that an error occurs that is not covered by these two
	 * the returned value will be OpStatus::ERR.
	 *
	 * @param object_type The type of object that should be allocated.
	 * @param obj The newly allocated object is returned here, NULL on OOM.
	 */
	static OP_STATUS CreateSVGDOMItem(SVGDOMItemType object_type, SVGDOMItem*& obj);

	/**
	 * Creates an SVGPathSeg object outside of any document trees.
	 * The object is initialized with 0 values and is given the type
	 * that's passed in.
	 *
	 * @param type The type of path segment (for valid values see SVGDOMPathSeg::PathSegType)
	 * @return A pointer to the newly created object, or NULL on OOM.
	 */
	static SVGDOMPathSeg*            CreateSVGDOMPathSeg(int type);

	/**
	 * Gets a trait. This is for SVG Tiny 1.2 TraitAccess.
	 *
	 * Note that only one of the result-parameters should be set to non-NULL.
	 *
	 * @param environment The DOM_Environment
	 * @param elm The element to get trait from
	 * @param attr The attribute to get
	 * @param name The name of the attribute
	 * @param ns_idx The namespace index of the attribute
	 * @param presentation If TRUE then the presentation value (animVal) will be returned, otherwise it will be baseVal
	 * @param itemType The type of item expected back in the result parameter
	 * @param result If non-NULL an object will be returned here
	 * @param resultstr If non-NULL the string will be returned in the buffer
	 * @param resultnum If non-NULL the number will be returned here
	 */
	static OP_STATUS				 GetTrait(DOM_Environment *environment, HTML_Element* elm, Markup::AttrType attr, const uni_char *name, int ns_idx, BOOL presentation,
											  SVGDOMItemType itemType, SVGDOMItem** result, TempBuffer* resultstr = NULL, double* resultnum = NULL);

	/**
	 * Sets a trait. This is for SVG Tiny 1.2 TraitAccess.
	 *
	 * Note that only one of the value-parameters should be set to non-NULL.
	 *
	 * @param environment The DOM_Environment
	 * @param elm The element to set trait on
	 * @param attr The attribute to set
	 * @param name The name of the attribute
	 * @param ns_idx The namespace index of the attribute
	 * @param value If non-NULL this is the string that will be set
	 * @param value_object If non-NULL the object will be set
	 * @param value_num If non-NULL the number will be set
	 */
	static OP_STATUS				 SetTrait(DOM_Environment *environment, HTML_Element* elm, Markup::AttrType attr, const uni_char *name, int ns_idx,
											  const uni_char *value, SVGDOMItem* value_object = NULL, double* value_num = NULL);

	/**
	 * Get an animated value from a attribute
	 *
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY
	 */
	static OP_STATUS				GetAnimatedValue(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc,
													 SVGDOMItemType itemType, SVGDOMAnimatedValue*& values,
													 Markup::AttrType attr_name, NS_Type ns);

	/**
	 * Get an animated list from a attribute
	 *
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY
	 */
	static OP_STATUS				GetAnimatedList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMItemType listItemType, SVGDOMAnimatedValue*& value, Markup::AttrType attr_name);

	/**
	 * Get part of a number-optional-number from an attribute
	 *
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY
	 */
	static OP_STATUS				GetAnimatedNumberOptionalNumber(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMAnimatedValue*& value, Markup::AttrType attr_name, UINT32 idx);

	/**
	 * Get the viewport of a HTML_Element. The element should be a SVG root node.
	 *
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY
	 */
	static OP_STATUS				GetViewPort(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect*& rect);

	/**
	 * Get the string list from a element/attribute pair
	 *
	 * The base value is always returned since none of the attributes
	 * holding SVGStringLists in DOM are animatable. This is the
	 * attributes: requireFeatures, requiredExtensions, systemLanguage
	 * and viewTarget of View elements.
	 *
	 * @return OpStatus::OK, OpStatus::ERR, OpStatus::ERR_NO_MEMORY
	 */
	static OP_STATUS				GetStringList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, Markup::AttrType attr_name, SVGDOMStringList*& list);

	/**
	 * Get/Set the zoom and pan value of the HTML_Element.
	 *
	 * @return OpStatus::OK, OpStatus::ERR, OpStatus::ERR_NO_MEMORY
	 */
	static OP_STATUS				AccessZoomAndPan(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGZoomAndPanType& domzap, BOOL write);

	/**
	 * Get the viewport element for a HTML_Element
	 */
	static OP_STATUS				GetViewportElement(HTML_Element* elm, BOOL nearest, BOOL svg_only, HTML_Element*& viewport_element);

	/**
	 * Get a PathSegList from an attribute.
	 *
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY
	 */
	static OP_STATUS				GetPathSegList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMItem*& value, Markup::AttrType attr_type, BOOL base, BOOL normalized);

	/**
	 * Get the root of a shadow tree, be it animated or not.
	 */
	static OP_STATUS				GetInstanceRoot(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, BOOL animated, HTML_Element*& root);

	/**
	 * Size of a pixel units (as defined by CSS2) along the x-axis of
	 * the viewport, which represents a unit somewhere in the range of
	 * 70dpi to 120dpi, and, on systems that support this, might
	 * actually match the characteristics of the target medium. On
	 * systems where it is impossible to know the size of a pixel, a
	 * suitable default pixel size is provided.
	 */
	static double					PixelUnitToMillimeterX(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc);

	/**
	 * Corresponding size of a pixel unit along the y-axis of the viewport.
	 */
	static double					PixelUnitToMillimeterY(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc);

	/**
	 * User interface (UI) events in DOM Level 2 indicate the screen
	 * positions at which the given UI event occurred. When the user
	 * agent actually knows the physical size of a "screen unit", this
	 * attribute will express that information; otherwise, user agents
	 * will provide a suitable default value such as .28mm.
	 */
	static double					ScreenPixelToMillimeterX(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc);

	/**
	 * Corresponding size of a screen pixel along the y-axis of the
	 * viewport.
	 */
	static double					ScreenPixelToMillimeterY(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc);

	/**
	 * The initial view (i.e., before magnification and panning) of
	 * the current innermost SVG document fragment can be either the
	 * "standard" view (i.e., based on attributes on the 'svg' element
	 * such as fitBoxToViewport) or to a "custom" view (i.e., a
	 * hyperlink into a particular 'view' or other element - see
	 * Linking into SVG content: URI fragments and SVG views). If the
	 * initial view is the "standard" view, then this attribute is
	 * false. If the initial view is a "custom" view, then this
	 * attribute is true.
	 */
	static BOOL						UseCurrentView(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc);

	/**
	 * This attribute indicates the current scale factor relative to
	 * the initial view to take into account user magnification and
	 * panning operations, as described under Magnification and
	 * panning. DOM attributes currentScale and currentTranslate are
	 * equivalent to the 2x3 matrix [a b c d e f] = [currentScale 0 0
	 * currentScale currentTranslate.x currentTranslate.y]. If
	 * "magnification" is enabled (i.e., zoomAndPan="magnify"), then
	 * the effect is as if an extra transformation were placed at the
	 * outermost level on the SVG document fragment (i.e., outside the
	 * outermost 'svg' element).
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK.
	 */
	static OP_STATUS				GetCurrentScale(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double& scale);

	/**
	 * Set current scale. @see GetCurrentScale
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK.
	 */
	static OP_STATUS				SetCurrentScale(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double scale);

	/**
	 * Get a translation for a SVG root element. Returns default value
	 * if the element isn't a SVG root.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS			    GetCurrentTranslate(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMPoint*& point);

	/**
	 * Notification that currentTranslate was modified through the DOM.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK.
	 */
	static OP_STATUS				OnCurrentTranslateChange(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMPoint* point);

	/**
	 * Get the current rotation for a SVG root element. Similar to currentScale but instead it is the user rotation of the svg.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS			    GetCurrentRotate(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double& rotate);

	/**
	 * Set current rotate. @see GetCurrentRotate
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK.
	 */
	static OP_STATUS				SetCurrentRotate(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double rotate);

	/**
	 * Takes a time-out value which indicates that redraw shall not
	 * occur until: (a) the corresponding
	 * unsuspendRedraw(suspend_handle_id) call has been made, (b) an
	 * unsuspendRedrawAll() call has been made, or (c) its timer has
	 * timed out. In environments that do not support interactivity
	 * (e.g., print media), then redraw shall not be
	 * suspended. suspend_handle_id =
	 * suspendRedraw(max_wait_milliseconds) and
	 * unsuspendRedraw(suspend_handle_id) must be packaged as balanced
	 * pairs. When you want to suspend redraw actions as a collection
	 * of SVG DOM changes occur, then precede the changes to the SVG
	 * DOM with a method call similar to suspend_handle_id =
	 * suspendRedraw(max_wait_milliseconds) and follow the changes
	 * with a method call similar to
	 * unsuspendRedraw(suspend_handle_id). Note that multiple
	 * suspendRedraw calls can be used at once and that each such
	 * method call is treated independently of the other suspendRedraw
	 * method calls.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done and handle set to 0.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param ms The number of milliseconds to suspend the redraw (The
	 * value is clamped to 0-60000 ms, in other words the maximum
	 * suspension is 60s)
	 *
	 * @param suspend_handle_id The unique handle id is saved here.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				SuspendRedraw(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, int ms, int& suspend_handle_id);

	/**
	 * Cancels a specified suspendRedraw() by providing a unique
	 * suspend_handle_id.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param suspend_handle_id The unique handle id of the suspension.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpBoolean::IS_TRUE on
	 * unsuspension. OpBoolean::IS_FALSE if the handle wasn't found or
	 * wasn't locked.
	 */
	static OP_BOOLEAN				UnsuspendRedraw(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, int handle);

	/**
	 * Cancels all currently active suspendRedraw() method calls. This
	 * method is most useful at the very end of a set of SVG DOM calls
	 * to ensure that all pending suspendRedraw() method calls have
	 * been cancelled.
	 */
	static OP_STATUS				UnsuspendRedrawAll(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc);

	/**
	 * In rendering environments supporting interactivity, forces the
	 * user agent to immediately redraw all regions of the viewport
	 * that require updating.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				ForceRedraw(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc);

	/**
	 * Suspends (i.e., pauses) all currently running animations that
	 * are defined within the SVG document fragment corresponding to
	 * this 'svg' element, causing the animation clock corresponding
	 * to this document fragment to stand still until it is unpaused.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				PauseAnimations(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc);

	/**
	 * Unsuspends (i.e., unpauses) currently running animations that
	 * are defined within the SVG document fragment, causing the
	 * animation clock to continue from the time at which it was
	 * suspended.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				UnpauseAnimations(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc);

	/**
	 * Returns true if this SVG document fragment is in a paused state.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param paused A reference to a BOOL where the state is saved
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 *
	 */
	static OP_STATUS				AnimationsPaused(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, BOOL& paused);

	/**
	 * Returns the current time in seconds relative to the start time
	 * for the current SVG document fragment.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param current_time A reference to a double where the current time is saved
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetDocumentCurrentTime(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double& current_time);

	/**
	 * Adjusts the clock for this SVG document fragment, establishing
	 * a new current time.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param current_time The current time used to adjust the clock
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				SetDocumentCurrentTime(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double current_time);

	/**
	 * Returns the list of graphics elements whose rendered content
	 * intersects the supplied rectangle, honoring the
	 * 'pointer-events' property value on each candidate graphics
	 * element.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetIntersectionList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect* rect, HTML_Element* reference_element, OpVector<HTML_Element>& selected);

	/**
	 * Returns the list of graphics elements whose rendered content is
	 * entirely contained within the supplied rectangle, honoring the
	 * 'pointer-events' property value on each candidate graphics
	 * element.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetEnclosureList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect* rect, HTML_Element* reference_element, OpVector<HTML_Element>& selected);

	/**
	 * Returns true if the rendered content of the given element
	 * intersects the supplied rectangle, honoring the
	 * 'pointer-events' property value on each candidate graphics
	 * element.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param rect The rectangle
	 *
	 * @param test_element The SVG element to check. Must be a child of
	 * the root.
	 *
	 * @param intersects The result of the check is saved here
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				CheckIntersection(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect* rect, HTML_Element* test_element, BOOL& intersects);

	/**
	 * Returns true if the rendered content of the given element is
	 * entirely contained within the supplied rectangle, honoring the
	 * 'pointer-events' property value on each candidate graphics
	 * element.
	 *
	 * @param elm The SVG root element. If the element is NULL or not
	 * a SVG root element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param rect The rectangle
	 *
	 * @param test_element The SVG element to check. Must be a child of
	 * the root.
	 *
	 * @param intersects The result of the check is saved here
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				CheckEnclosure(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect* rect, HTML_Element* test_element, BOOL& intersects);

	/**
	 * Returns the number of characters in a text-class element.
	 *
	 * @param elm A text-class element. If the element is NULL or not
	 * a text-class element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param numChars Result is returned here, only valid if method returns with success status.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetNumberOfChars(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32& numChars);

	/**
	 * Returns the length (in user units) of the text in this text-class element.
	 *
	 * @param elm A text-class element. If the element is NULL or not
	 * a text-class element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param textlength Result is returned here, only valid if method returns with success status.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetComputedTextLength(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double& textlength);

	/**
	 * Returns the length (in user units) of a substring of the text in this text-class element.
	 *
	 * @param elm A text-class element. If the element is NULL or not
	 * a text-class element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param firstCharIndex The starting charindex of the substring
	 *
	 * @param numChars The number of characters in the substring
	 *
	 * @param textlength Result is returned here, only valid if method returns with success status.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetSubStringLength(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 firstCharIndex, UINT32 numChars, double& textlength);

	/**
	 * Returns the startposition of a specific character in this text-class element.
	 *
	 * @param elm A text-class element. If the element is NULL or not
	 * a text-class element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param charIndex The charindex
	 *
	 * @param startPos Result is returned here, only valid if method returns with success status.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetStartPositionOfChar(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 charIndex, SVGDOMPoint*& startPos);

	/**
	 * Returns the endposition of a specific character in this text-class element.
	 *
	 * @param elm A text-class element. If the element is NULL or not
	 * a text-class element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param charIndex The charindex
	 *
	 * @param endPos Result is returned here, only valid if method returns with success status.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetEndPositionOfChar(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 charIndex, SVGDOMPoint*& endPos);

	/**
	 * Returns the extent of a character in this text-class element.
	 *
	 * @param elm A text-class element. If the element is NULL or not
	 * a text-class element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param charIndex The starting charindex of the substring
	 *
	 * @param extent Result is returned here, only valid if method returns with success status.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetExtentOfChar(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 charIndex, SVGDOMRect*& extent);

	/**
	 * Returns the rotation of a character in this text-class element.
	 *
	 * @param elm A text-class element. If the element is NULL or not
	 * a text-class element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param charIndex The charindex
	 *
	 * @param rotation Result is returned here, only valid if method returns with success status.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetRotationOfChar(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 charIndex, double& rotation);

	/**
	 * Returns the characterindex of a specific position.
	 *
	 * @param elm A text-class element. If the element is NULL or not
	 * a text-class element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param pos The position to get index for
	 *
	 * @param charIndex Result is returned here, only valid if method returns with success status.
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetCharNumAtPosition(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMPoint* pos, long& charIndex);

	/**
	 * Selects a substring of the text in this text-class element, just as if the user had
	 * selected the substring interactively.
	 *
	 * @param elm A text-class element. If the element is NULL or not
	 * a text-class element, nothing is done.
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param startCharIndex The starting charindex of the substring
	 *
	 * @param numChars The number of characters in the substring
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				SelectSubString(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, UINT32 startCharIndex, UINT32 numChars);

	/**
	* Gets the length of a path. The path must be set as the 'd'-attribute on the element.
	*
	* @param elm A path element
	*
	* @param frm_doc The frames document of the root element
	*
	* @param length The length will be returned here
	*
	* @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	*/
	static OP_STATUS				GetTotalLength(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double& length);

	/**
	* Gets a point on the path at a specific distance from the start.
	* The path must be set as the 'd'-attribute on the element.
	*
	* @param elm A path element
	*
	* @param frm_doc The frames document of the root element
	*
	* @param length The length to walk along the curve
	*
	* @param point The point on the path will be returned here
	*
	* @return OpStatus::ERR_NO_MEMORY, OpStatus::OK, OpStatus::ERR (What does OpStatus::ERR mean? If the operation could fail in a legal case use OP_BOOLEAN instead)
	*/
	static OP_STATUS				GetPointAtLength(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double length, SVGDOMPoint*& point);

	/**
	* Get the index in the pathseglist for a specific distance along the path.
	* The path must be set as the 'd'-attribute on the element.
	*
	* @param elm A path element
	*
	* @param frm_doc The frames document of the root element
	*
	* @param length The length to walk along the curve
	*
	* @param index The index will be returned here
	*
	* @return OpStatus::ERR_NO_MEMORY, OpStatus::OK, OpStatus::ERR (What does OpStatus::ERR mean? If the operation could fail in a legal case use OP_BOOLEAN instead)
	*/
	static OP_STATUS				GetPathSegAtLength(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double length, UINT32& index);

	/**
	 * Get a point list
	 *
	 * @param elm A polygon or polyline element
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::OK
	 */
	static OP_STATUS				GetPointList(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMItem*& value, Markup::AttrType attr_name, BOOL base);

	/**
	 * Returns the tight bounding box in current user space (i.e.,
	 * after application of the transform attribute, if any) on the
	 * geometry of all contained graphics elements, exclusive of
	 * stroke-width and filter effects).
	 *
	 * If the element has a bounding box, a pointer to it is save in
	 * SVGDOMRect*& and OpBoolean::IS_TRUE is returned. If the element
	 * has no bounding box, OpBoolean::IS_FALSE is returned and
	 * SVGDOMRect*& is set to NULL.
	 *
	 * @param elm A SVG element
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param rect A pointer to the SVGDOMRect is saved here
	 *
	 * @param type The type of boundingbox, see
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpBoolean::IS_TRUE, OpBoolean::IS_FALSE
	 */
	static OP_BOOLEAN				GetBoundingBox(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMRect*& rect, int type = SVG_BBOX_NORMAL);

	/**
	 * Returns the transformation matrix from current user units
	 * (i.e., after application of the transform attribute, if any) to
	 * the viewport coordinate system for the nearestViewportElement.
	 *
	 * If the element has a transform matrix, a pointer to it is save
	 * in SVGDOMMatrix*& and OpBoolean::IS_TRUE is returned. If the
	 * element has no transform matrix, OpBoolean::IS_FALSE is
	 * returned and SVGDOMMatrix*& is set to NULL.
	 *
	 * @param elm A SVG element
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param matrix A pointer to the SVGDOMMatrix is saved here
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpBoolean::IS_TRUE, OpBoolean::IS_FALSE
	 */
	static OP_BOOLEAN				GetCTM(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMMatrix*& matrix);

	/**
	 * Returns the transformation matrix from current user units
	 * (i.e., after application of the transform attribute, if any) to
	 * the parent user agent's notice of a "pixel". For display
	 * devices, ideally this represents a physical screen pixel.
	 *
	 * If the element has a transform matrix, a pointer to it is save
	 * in SVGDOMMatrix*& and OpBoolean::IS_TRUE is returned. If the
	 * element has no transform matrix, OpBoolean::IS_FALSE is
	 * returned and SVGDOMMatrix*& is set to NULL.
	 *
	 * @param elm A SVG element
	 *
	 * @param frm_doc The frames document of the root element
	 *
	 * @param matrix A pointer to the SVGDOMMatrix is saved here
	 *
	 * @return OpStatus::ERR_NO_MEMORY, OpBoolean::IS_TRUE, OpBoolean::IS_FALSE
	 */
	static OP_BOOLEAN				GetScreenCTM(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMMatrix*& matrix);

	/**
	 * Returns the transformation matrix from the user coordinate system
	 * on the current element (after application of the transform
	 * attribute, if any) to the user coordinate system on parameter
	 * element (after application of its transform attribute, if any).
	 *
	 * If the element has a transform matrix, a pointer to it is save
	 * in SVGDOMMatrix*& and OpBoolean::IS_TRUE is returned. If the
	 * element has no transform matrix, OpBoolean::IS_FALSE is
	 * returned and SVGDOMMatrix*& is set to NULL.
	 *
	 * @param elm A SVG element
	 * @param frm_doc The frames document of the root element
	 * @param target_element The element we want to get the transform to
	 * @param matrix A pointer to the SVGDOMMatrix is saved here
	 * @return OpStatus::ERR_NO_MEMORY, OpBoolean::IS_TRUE, OpBoolean::IS_FALSE
	 *
	 */
	static OP_BOOLEAN				GetTransformToElement(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, HTML_Element* target_element, SVGDOMMatrix*& matrix);

	/**
	 * Returns the presentation attribute from a HTML_Elements
	 * attribute. This attribute could from an xml attribute or from a
	 * css declaration.
	 *
	 * @param elm A SVG element
	 * @param frm_doc The frames document of the root element
	 * @param attr_type The presentation attribute to fetch
	 * @return OpBoolean::IS_FALSE if the attribute types isn't a
	 * presentation attribute. OpBoolean::IS_TRUE if a reference to a
	 * presentation attribute has been returned. This doesn't mean
	 * that the attribute currently exists. OpStatus::ERR_NO_MEMORY on
	 * OOM.
	 *
	 */
	static OP_BOOLEAN				GetPresentationAttribute(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, Markup::AttrType attr_name, SVGDOMItem*& item);

	/**
	 * Part of the The ElementTimeControl interface, which is part of the
	 * org.w3c.dom.smil module. Only valid for animation elements.
	 */
	static OP_STATUS				BeginElement(HTML_Element *elm, SVG_DOCUMENT_CLASS *doc, double offset_ms);

	/**
	 * Part of the The ElementTimeControl interface, which is part of the
	 * org.w3c.dom.smil module. Only valid for animation elements.
	 */
	static OP_STATUS				EndElement(HTML_Element *elm, SVG_DOCUMENT_CLASS *doc, double offset);

	/**
	 * Part of the SVGFilterElement interface, sets the resolution used when
	 * rendering the filter.
	 */
	static OP_STATUS				SetFilterRes(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double filter_res_x, double filter_res_y);

	/**
	 * Part of the SVGFEGaussianBlurElement interface, sets the standard deviation for the
	 * gaussian blur effect.
	 */
	static OP_STATUS				SetStdDeviation(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, double std_dev_x, double std_dev_y);

	/**
	 * hasFeature in DOM, for svg features.
	 */
	static OP_STATUS				HasFeature(const uni_char* featurestring, const uni_char*, BOOL& supported);

	/**
	 * setOrientTo{Auto,Angle} in DOM, used for SVGMarkerElement
	 *
	 * @param angle If non-NULL, the angle to set, otherwise set 'auto'.
	 */
	static OP_STATUS				SetOrient(HTML_Element* elm, SVG_DOCUMENT_CLASS *doc, SVGDOMAngle* angle);

	/**
	 * Returns OpBoolean::IS_TRUE if the user agent supports the given
	 * extension, specified by a URI.
	 */
	static OP_BOOLEAN				HasExtension(const uni_char* uri);

	/**
	 * Retreive serial for an attribute
	 */
	static UINT32					Serial(HTML_Element* elm, Markup::AttrType attr, NS_Type ns);

	/**
	 * Get animation target for an animation element
	 *
	 * @param anim_elm The animation element to get target for
	 * @param target_elm The target is saved here. NULL is no target is found.
	 */
	static OP_STATUS				GetAnimationTargetElement(HTML_Element* anim_elm,
															  SVG_DOCUMENT_CLASS *doc,
															  HTML_Element*& target_elm);

	/**
	 * Get animation start time for an animation element.
	 *
	 * @param anim_elm The animation element to get target for
	 * @param start_time The start time is saved here
	 */
	static OP_STATUS				GetAnimationStartTime(HTML_Element* anim_elm,
														  SVG_DOCUMENT_CLASS *doc,
														  double& start_time);

	/**
	 * Get current time for an animation element
	 *
	 * @param anim_elm The animation element to get target for
	 * @param current_time The current time is saved here
	 */
	static OP_STATUS				GetAnimationCurrentTime(HTML_Element* anim_elm,
															SVG_DOCUMENT_CLASS *doc,
															double& current_time);

	/**
	 * Get simple duration for a animation element
	 *
	 * @param anim_elm The animation element to get target for
	 * @param current_time The current time is saved here
	 */
	static OP_STATUS				GetAnimationSimpleDuration(HTML_Element* anim_elm,
															   SVG_DOCUMENT_CLASS *doc,
															   double& duration);

	/**
	 * Get bounding client rect
	 */
	static OP_STATUS				GetBoundingClientRect(HTML_Element *element, OpRect& rect);
};

/**
 * This is an internal SVG class so it is only forward declarated
 * here.
 */
class SVGObject;

/**
 * This is the base class for all svg objects exposed to dom.
 */
class SVGDOMItem
{
public:
	SVGDOMItem() {}
	virtual ~SVGDOMItem() {}

	/**
	 * Get the typename of this item.
	 */
	virtual const char* GetDOMName() = 0;

	/**
	 * Only used in the SVG module.
	 */
	virtual SVGObject* GetSVGObject() = 0;

	/**
	 * Check if the item is of a certain type.
	 * @param type The type to check for
	 * @return TRUE if this item is of the specified type, FALSE otherwise.
	 */
	virtual BOOL IsA(SVGDOMItemType type) { return FALSE; }
};

/**
 * This class is here since we represent SVG strings as SVGStrings.
 * Note! There are exceptions to this rule: Markup::SVGA_CLASS and Markup::SVGA_ID, which
 *       are stored as regular strings.
 *
 * This class is deprecated and will be removed as soon as the dom
 * module does not depend on it anymore (in domsvgelement.cpp)
 *
 * In the SVGDOM spec the strings are seen as DOMString.
 */
class SVGDOMString : public SVGDOMItem
{
protected:
	SVGDOMString() {}
public:
	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_STRING || SVGDOMItem::IsA(type); }
	virtual OP_BOOLEAN		SetValue(const uni_char* str) = 0;
	virtual const uni_char* GetValue() = 0;
};

/**
 * Captures the behavior of the SVGNumber interface.
 */
class SVGDOMNumber : public SVGDOMItem
{
public:
	SVGDOMNumber() {}
	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_NUMBER || SVGDOMItem::IsA(type); }

	virtual OP_BOOLEAN	SetValue(double value) = 0;
	virtual double		GetValue() = 0;
};

/**
 * SVGPreserveAspectRatio interface
 */
class SVGDOMPreserveAspectRatio : public SVGDOMItem
{
public:
	SVGDOMPreserveAspectRatio() {}

	enum AlignType
	{
		SVG_PRESERVEASPECTRATIO_UNKNOWN  = 0,
		SVG_PRESERVEASPECTRATIO_NONE     = 1,
		SVG_PRESERVEASPECTRATIO_XMINYMIN = 2,
		SVG_PRESERVEASPECTRATIO_XMIDYMIN = 3,
		SVG_PRESERVEASPECTRATIO_XMAXYMIN = 4,
		SVG_PRESERVEASPECTRATIO_XMINYMID = 5,
		SVG_PRESERVEASPECTRATIO_XMIDYMID = 6,
		SVG_PRESERVEASPECTRATIO_XMAXYMID = 7,
		SVG_PRESERVEASPECTRATIO_XMINYMAX = 8,
		SVG_PRESERVEASPECTRATIO_XMIDYMAX = 9,
		SVG_PRESERVEASPECTRATIO_XMAXYMAX = 10
	};

	enum MeetOrSliceType
	{
		SVG_MEETORSLICE_UNKNOWN = 0,
		SVG_MEETORSLICE_MEET  	= 1,
		SVG_MEETORSLICE_SLICE 	= 2
	};

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_PRESERVE_ASPECT_RATIO || SVGDOMItem::IsA(type); }

	virtual OP_BOOLEAN SetAlign(unsigned short value) = 0;
	virtual unsigned short GetAlign() = 0;

	virtual OP_BOOLEAN SetMeetOrSlice(unsigned short value) = 0;
	virtual unsigned short GetMeetOrSlice() = 0;
};

/**
 * Use for Enumerations (as in SVGAnimatedEnumeration)
 *
 * This class is deprecated and will be removed as soon as the dom
 * module does not depend on it anymore (in domsvgelement.cpp)
 */
class SVGDOMEnumeration : public SVGDOMItem
{
protected:
	SVGDOMEnumeration() {}
public:
	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_ENUM || SVGDOMItem::IsA(type); }

	virtual OP_BOOLEAN  SetValue(unsigned short value) = 0;
	virtual unsigned short GetValue() = 0;
};

class SVGDOMLength : public SVGDOMItem
{
public:
	SVGDOMLength() {}
	// Length Unit Types
	enum UnitType
	{
		/**
		 * The unit type is not one of predefined unit types. It is
		 * invalid to attempt to define a new value of this type or to
		 * attempt to switch an existing value to this type.
		 */
		SVG_DOM_LENGTHTYPE_UNKNOWN    = 0,

		/**
		 * No unit type was provided (i.e., a unitless value was
		 * 	specified), which indicates a value in user units.
		 */
		SVG_DOM_LENGTHTYPE_NUMBER     = 1,

		/**
		 * A percentage value was specified.
		 */
		SVG_DOM_LENGTHTYPE_PERCENTAGE = 2,

		/**
		 * A value was specified using the "em" units defined in CSS2.
		 */
		SVG_DOM_LENGTHTYPE_EMS        = 3,

		/**
		 * A value was specified using the "ex" units defined in CSS2.
		 */
		SVG_DOM_LENGTHTYPE_EXS        = 4,

		/**
		 * A value was specified using the "px" units defined in CSS2.
		 */
		SVG_DOM_LENGTHTYPE_PX         = 5,

		/**
		 * A value was specified using the "cm" units defined in CSS2.
		 */
		SVG_DOM_LENGTHTYPE_CM         = 6,

		/**
		 * A value was specified using the "mm" units defined in CSS2.
		 */
		SVG_DOM_LENGTHTYPE_MM         = 7,

		/**
		 * A value was specified using the "in" units defined in CSS2.
		 */
		SVG_DOM_LENGTHTYPE_IN         = 8,

		/**
		 * A value was specified using the "pt" units defined in CSS2.
		 */
		SVG_DOM_LENGTHTYPE_PT         = 9,

		/**
		 * A value was specified using the "pc" units defined in CSS2.
		 */
		SVG_DOM_LENGTHTYPE_PC         = 10,

		/**
		 * A value was specified using the "rem" units defined in CSS3.
		 */
		SVG_DOM_LENGTHTYPE_REMS       = 11
	};

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_LENGTH || SVGDOMItem::IsA(type); }

	/**
	 * The type of the value as specified by one of the constants
	 * specified above.
	 */
	virtual UnitType		GetUnitType() = 0;

	/**
	 * The value as an doubleing point value, in user units. Setting
	 * this attribute will cause valueInSpecifiedUnits and
	 * valueAsString to be updated automatically to reflect this
	 * setting. */
	virtual OP_BOOLEAN		SetValue(double v) = 0;

	/**
	 * The value as an doubleing point value, in user units.
	 */
	virtual double			GetValue(HTML_Element* elm, Markup::AttrType attr_name, NS_Type ns) = 0;

	/**
	 * The angle value as a doubleing point value, in the units
	 * expressed by unitType. Setting this attribute will cause value
	 * and valueAsString to be updated automatically to reflect this
	 * setting.
	 * raises DOMException on setting.
	 */
	virtual OP_BOOLEAN		SetValueInSpecifiedUnits(double v) = 0;

	/**
	 * The angle value as a doubleing point value, in the units
	 * expressed by unitType.
	 */
	virtual double			GetValueInSpecifiedUnits() = 0;

	/**
	 * Set the length value as a string value.
	 *
	 * @param v The string that describes the length
	 *
	 * @return OpBoolean::IS_TRUE if the value was set
	 * correctly. OpBoolean::IS_FALSE if the value could not be parsed
	 * to a length. OpStatus::ERR_NO_MEMORY if OOM.
	 *
	 */
	virtual OP_BOOLEAN		SetValueAsString(const uni_char* v) = 0;

	/**
	 * The length value as a string value, in the units expressed by
	 * unitType.
	 */
	virtual const uni_char* GetValueAsString() = 0;

	/**
	 * Reset the value as a number with an associated unitType,
	 * thereby replacing the values for all of the attributes on the
	 * object.
	 *
	 * raises DOMException on setting
	 */
	virtual OP_BOOLEAN		NewValueSpecifiedUnits(UnitType unitType, double valueInSpecifiedUnits) = 0;

	/**
	 * Preserve the same underlying stored value, but reset the stored
	 * unit identifier to the given unitType. Object attributes
	 * unitType, valueAsSpecified and valueAsString might be modified
	 * as a result of this method. For example, if the original value
	 * were "0.5cm" and the method was invoked to convert to
	 * millimeters, then the unitType would be changed to
	 * SVG_LENGTHTYPE_MM, valueAsSpecified would be changed to the
	 * numeric value 5 and valueAsString would be changed to "5mm".
	 *
	 * raises DOMException on setting
	 */
	virtual OP_BOOLEAN		ConvertToSpecifiedUnits(UnitType unitType, HTML_Element* elm, Markup::AttrType attr_name, NS_Type ns) = 0;
};

class SVGDOMPrimitiveValue
{
public:
	enum ValueType
	{
		VALUE_ITEM,
		VALUE_STRING,
		VALUE_NUMBER,
		VALUE_BOOLEAN,
		VALUE_NONE
	} type;

	union
	{
		SVGDOMItem *item;
		double number;
		const uni_char *str;
		bool boolean;
	} value;

	SVGDOMPrimitiveValue(SVGDOMItem *item) : type(VALUE_ITEM) {
		value.item = item;
	}

	SVGDOMPrimitiveValue(double v) : type(VALUE_NUMBER) {
		value.number = v;
	}

	SVGDOMPrimitiveValue(const uni_char *str) : type(VALUE_STRING) {
		value.str = str;
	}

	SVGDOMPrimitiveValue() : type(VALUE_NONE) { value.item = NULL; }
};

class SVGDOMAnimatedValue
{
public:
	virtual ~SVGDOMAnimatedValue() {}

	enum Field
	{
		FIELD_BASE,
		FIELD_ANIM
	};

	/**
	 * Get value for reading
	 */
	virtual OP_BOOLEAN GetPrimitiveValue(SVGDOMPrimitiveValue &value, Field field) = 0;

	/**
	 * Can the base value be set using a number?
	 */
	virtual BOOL IsSetByNumber() = 0;

	/**
	 * Set base value by number
	 */
	virtual OP_BOOLEAN SetNumber(double number) = 0;

	/**
	 * Can the base value be set using a string?
	 */
	virtual BOOL IsSetByString() = 0;

	/**
	 * Set base value by string
	 *
	 * @param str The string to set.
	 * @param logdoc The logical document that the svg element that this value
	 *				 is connected to, belongs to. Used for getting class references
	 *				 from the correct hash table when setting class attribute strings.
	 *				 May be NULL.
	 */
	virtual OP_BOOLEAN SetString(const uni_char *str, LogicalDocument* logdoc) = 0;

	virtual const char* GetDOMName() = 0;
};

class SVGDOMAngle : public SVGDOMItem
{
public:
	// Angle Unit Types
	enum UnitType
	{
		/**
		 * The unit type is not one of predefined unit types. It is
		 * invalid to attempt to define a new value of this type or to
		 * attempt to switch an existing value to this type.
		 */
		SVG_ANGLETYPE_UNKNOWN     = 0,

		/**
		 * No unit type was provided (i.e., a unitless value was
		 * specified). For angles, a unitless value is treated the
		 * same as if degrees were specified.
		 */
		SVG_ANGLETYPE_UNSPECIFIED = 1,

		/**
		 * The unit type was explicitly set to degrees.
		 */
		SVG_ANGLETYPE_DEG         = 2,

		/**
		 * The unit type is radians.
		 */
		SVG_ANGLETYPE_RAD         = 3,

		/**
		 * The unit type is grads.
		 */
		SVG_ANGLETYPE_GRAD        = 4
	};

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_ANGLE || SVGDOMItem::IsA(type); }

	/**
	 * The type of the value as specified by one of the constants
	 * specified above. Readonly attribute.
	 */
	virtual UnitType		GetUnitType() = 0;

	/**
	 * The angle value as a doubleing point value, in degrees. Setting
	 * this attribute will cause valueInSpecifiedUnits and
	 * valueAsString to be updated automatically to reflect this
	 * setting.
	 */
	virtual OP_BOOLEAN		SetValue(double v) = 0;

	/**
	 * The angle value as a doubleing point value, in degrees.
	 */
	virtual double			GetValue() = 0;

	/**
	 * The angle value as a doubleing point value, in the units
	 * expressed by unitType. Setting this attribute will cause value
	 * and valueAsString to be updated automatically to reflect this
	 * setting.
	 * raises DOMException on setting. attribute
	 */
	virtual OP_BOOLEAN		SetValueInSpecifiedUnits(double v) = 0;

	/**
	 * The angle value as a doubleing point value, in the units
	 * expressed by unitType.
	 */
	virtual double			GetValueInSpecifiedUnits() = 0;

	/**
	 * The angle value as a string value, in the units expressed by
	 * unitType. Setting this attribute will cause value and
	 * valueInSpecifiedUnits to be updated automatically to reflect
	 * this setting.
	 */
	virtual OP_BOOLEAN		SetValueAsString(const uni_char* v) = 0;

	/**
	 * The angle value as a string value, in the units expressed by
	 * unitType.
	 */
	virtual const uni_char* GetValueAsString() = 0;

	/**
	 * Reset the value as a number with an associated unitType,
	 * thereby replacing the values for all of the attributes on the
	 * object.
	 * raises DOMException on setting
	 */
	virtual OP_BOOLEAN		NewValueSpecifiedUnits(UnitType unit_type, double value_in_specified_units) = 0;

	/**
	 * Preserve the same underlying stored value, but reset the stored
	 * unit identifier to the given unitType. Object attributes
	 * unit_type, value_as_specified and value_as_string might be modified
	 * as a result of this method. Raises DOMException on setting
	 *
	 * @param unit_type The specified unit to convert to
	 */
	virtual OP_BOOLEAN		ConvertToSpecifiedUnits(UnitType unit_type) = 0;
};

/**
 * Many of the SVG DOM interfaces refer to objects of class
 * SVGPoint. An SVGPoint is an (x,y) coordinate pair. When used in
 * matrix operations, an SVGPoint is treated as a vector of the form:
 * [x]
 * [y]
 * [1]
 *
 */
class SVGDOMPoint : public SVGDOMItem
{
public:
	SVGDOMPoint() {}

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_POINT || SVGDOMItem::IsA(type); }

	/** Set the x component of the point. */
	virtual OP_BOOLEAN	SetX(double x) = 0;
	/** Get the x component of the point. */
	virtual double 		GetX() = 0;

	/** Set the y component of the point. */
	virtual OP_BOOLEAN	SetY(double y) = 0;
	/** Get the y component of the point. */
	virtual double 		GetY() = 0;

	/** Transform a point with a matrix and save the result to another
	 * point */
	virtual OP_BOOLEAN  MatrixTransform(SVGDOMMatrix* matrix, SVGDOMPoint* target) = 0;
};

/**
 * The matrix is specified as the following:
 * [a c e]
 * [b d f]
 * [0 0 1]
 *
 * In index form (used in SetValue/GetValue):
 * a=0, b=1, c=2, d=3, e=4, f=5
 */
class SVGDOMMatrix : public SVGDOMItem
{
public:
	SVGDOMMatrix() {}

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_MATRIX || SVGDOMItem::IsA(type); }

	/** Set the 'idx' component of the matrix. */
	virtual OP_BOOLEAN	SetValue(int idx, double x) = 0;

	/** Get the 'idx' component of the matrix. */
	virtual double 		GetValue(int idx) = 0;

	/**
	 * Performs matrix multiplication. This matrix is post-multiplied
	 * by another matrix, returning the resulting new matrix.
	 */
	virtual void Multiply(SVGDOMMatrix* second_matrix, SVGDOMMatrix* target_matrix) = 0;

	/**
	 * Returns the inverse matrix.
	 */
	virtual OP_BOOLEAN Inverse(SVGDOMMatrix* target_matrix) = 0;

	/**
	 * Post-multiplies a translation transformation on the current
	 * matrix and returns the resulting matrix.
	 */
	virtual void Translate(double x, double y, SVGDOMMatrix* target) = 0;

	/**
	 * Post-multiplies a uniform scale transformation on the current
	 * matrix and returns the resulting matrix.
	 */
	virtual void Scale(double scale_factor, SVGDOMMatrix* target) = 0;

	/**
	 * Post-multiplies a non-uniform scale transformation on the
	 * current matrix and returns the resulting matrix.
	 */
	virtual void ScaleNonUniform(double scale_factor_x, double scale_factor_y,
								 SVGDOMMatrix* target) = 0;

	/**
	 * Post-multiplies a rotation transformation on the current matrix
	 * and returns the resulting matrix.
	 */
	virtual void Rotate(double angle, SVGDOMMatrix* target) = 0;

	/**
	 * Post-multiplies a rotation transformation on the current matrix
	 * and returns the resulting matrix. The rotation angle is
	 * determined by taking (+/-) atan(y/x). The direction of the
	 * vector (x,y) determines whether the positive or negative angle
	 * value is used.
	 */
	virtual OP_BOOLEAN RotateFromVector(double scale_factor_x, double scale_factor_y,
										SVGDOMMatrix* target) = 0;

	/**
	 * Post-multiplies the transformation [-1 0 0 1 0 0] and returns
	 * the resulting matrix.
	 */
    virtual void FlipX(SVGDOMMatrix* target) = 0;

	/**
	 * Post-multiplies the transformation [1 0 0 -1 0 0] and returns
	 * the resulting matrix.
	 */
    virtual void FlipY(SVGDOMMatrix* target) = 0;

	/**
	 * Post-multiplies a skewX transformation on the current matrix
	 * and returns the resulting matrix.
	 */
	virtual void SkewX(double angle, SVGDOMMatrix* target) = 0;

	/**
	 * Post-multiplies a skewY transformation on the current matrix
	 * and returns the resulting matrix.
	 */
    virtual void SkewY(double angle, SVGDOMMatrix* target) = 0;
};

class SVGDOMRect : public SVGDOMItem
{
public:
	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_RECT || SVGDOMItem::IsA(type); }

	/** Set the x component of the rect. */
	virtual OP_BOOLEAN	SetX(double x) = 0;
	/** Get the x component of the rect. */
	virtual double 		GetX() = 0;

	/** Set the y component of the matrix. */
	virtual OP_BOOLEAN  SetY(double y) = 0;
	/** Set the y component of the matrix. */
	virtual double		GetY() = 0;

	/** Set the width component of the matrix. */
	virtual OP_BOOLEAN  SetWidth(double width) = 0;
	/** Set the width component of the matrix. */
	virtual double		GetWidth() = 0;

	/** Set the height component of the matrix. */
	virtual OP_BOOLEAN  SetHeight(double height) = 0;
	/** Set the height component of the matrix. */
	virtual double		GetHeight() = 0;
};

class SVGDOMTransform : public SVGDOMItem
{
public:
	SVGDOMTransform() {}

	enum TransformType {
		SVG_TRANSFORM_UNKNOWN = 0,
		SVG_TRANSFORM_MATRIX = 1,
		SVG_TRANSFORM_TRANSLATE = 2,
		SVG_TRANSFORM_SCALE = 3,
		SVG_TRANSFORM_ROTATE = 4,
		SVG_TRANSFORM_SKEWX = 5,
		SVG_TRANSFORM_SKEWY = 6
	};

	virtual ~SVGDOMTransform() {}

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_TRANSFORM || SVGDOMItem::IsA(type); }

	/**
	 * Get the type of transform.
	 * @return The matrix type
	 */
	virtual TransformType GetType() const = 0;

	/**
	 * The matrix that represents this transformation.
	 *
	 * For SVG_TRANSFORM_MATRIX, the matrix contains the a, b, c, d,
	 * e, f values supplied by the user.
	 *
	 * For SVG_TRANSFORM_TRANSLATE, e and f represent the translation
	 * amounts (a=1,b=0,c=0,d=1).
	 *
	 * For SVG_TRANSFORM_SCALE, a and d represent the scale amounts
	 * (b=0,c=0,e=0,f=0).
	 *
	 * For SVG_TRANSFORM_ROTATE, SVG_TRANSFORM_SKEWX and
	 * SVG_TRANSFORM_SKEWY, a, b, c and d represent the matrix which
	 * will result in the given transformation (e=0,f=0).
	 *
	 * @param matrix The matrix is saved here
	 */
	virtual OP_STATUS	GetMatrix(SVGDOMMatrix*& matrix) = 0;

	/**
	 * Update a matrix to the current transform
	 */
	virtual void		UpdateMatrix(SVGDOMMatrix* matrix) = 0;

	/**
	 * Get the angle. A convenience attribute for
	 * SVG_TRANSFORM_ROTATE, SVG_TRANSFORM_SKEWX and
	 * SVG_TRANSFORM_SKEWY.
	 *
	 * It holds the angle that was specified.  For
	 * SVG_TRANSFORM_MATRIX, SVG_TRANSFORM_TRANSLATE and
	 * SVG_TRANSFORM_SCALE, angle will be zero.
	 *
	 * @return The angle
	 */
	virtual double		GetAngle() = 0;

	/**
	 * Sets the transform type to SVG_TRANSFORM_MATRIX, with parameter
	 * matrix defining the new transformation.
	 */
	virtual void 		SetMatrix(const SVGDOMMatrix* matrix) = 0;

	/**
	 * Sets the transform type to SVG_TRANSFORM_TRANSLATE, with
	 * parameters tx and ty defining the translation amounts.
	 *
	 * @param x The x parameter
	 * @param y The y parameter
	 */
	virtual void		SetTranslate(double x, double y) = 0;

	/**
	 * Sets the transform type to SVG_TRANSFORM_SCALE, with parameters
	 * sx and sy defining the scale amounts.
	 */
	virtual void        SetScale(double sx, double sy) = 0;

	/**
	 * Sets the transform type to SVG_TRANSFORM_ROTATE, with parameter
	 * angle defining the rotation angle and parameters cx and cy
	 * defining the optional centre of rotation.
	 */
	virtual void        SetRotate(double angle, double cx, double cy) = 0;

	/**
	 * Sets the transform type to SVG_TRANSFORM_SKEWX, with parameter
	 * angle defining the amount of skew.
	 */
	virtual void		SetSkewX(double angle) = 0;

	/**
	 * Sets the transform type to SVG_TRANSFORM_SKEWY, with parameter
	 * angle defining the amount of skew.
	 */
	virtual void		SetSkewY(double angle) = 0;

};

class SVGDOMList : public SVGDOMItem
{
public:
	SVGDOMList(SVGDOMItemType list_type) : list_type(list_type) {}

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_LIST || SVGDOMItem::IsA(type); }

	/**
	 * Clears all existing current items from the list, with the result being an empty list.
	 *
	 * raises( DOMException );
	 * DOMException NO_MODIFICATION_ALLOWED_ERR: Raised when the list cannot be modified.
	 */
	virtual void Clear() = 0;

	/**
	 * Clears all existing current items from the list and
	 * re-initializes the list to hold the single item specified by
	 * the parameter.
	 *
	 * DOMException NO_MODIFICATION_ALLOWED_ERR: Raised when the list
	 * cannot be modified.  SVGException SVG_WRONG_TYPE_ERR: Raised if
	 * parameter newItem is the wrong type of object for the given
	 * list.
	 */
	virtual OP_STATUS Initialize(SVGDOMItem* new_item) = 0;

	/**
	 * Returns the specified item from the list.
	 *
	 * DOMException INDEX_SIZE_ERR: Raised if the index number is
	 * negative or greater than or equal to numberOfItems.
	 *
	 * @return OpBoolean::IS_TRUE is the element is
	 * found. OpBoolean::IS_FALSE if the idx was
	 * out-of-bounds. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	virtual OP_BOOLEAN CreateItem(UINT32 idx, SVGDOMItem*& item) = 0;

	/**
	 * Inserts a new item into the list at the specified position. The
	 * first item is number 0. If newItem is already in a list, it is
	 * removed from its previous list before it is inserted into this
	 * list.
	 *
	 * raises( DOMException, SVGException );
	 */
	virtual OP_BOOLEAN InsertItemBefore(SVGDOMItem* new_item, UINT32 index) = 0;

	/**
	 * Removes an existing item from the list.
	 *
	 *  raises( DOMException );
	 *
	 * @param removed_item The item to remove
	 * @param index The index to remove
	 * @param old_item The removed item is saved here if the index was valid, otherwise NULL.
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS RemoveItem (UINT32 index) = 0;

	/**
	 * The number of items in the list.
	 */
	virtual UINT32 GetCount() = 0;

	/**
	 * Consolidate the list. This is only legal for transform lists,
	 * since it is defined to multiply all matrices together and store
	 * the resulting matrix as the only value in the list.
	 *
	 * @return OpBoolean::IS_FALSE if the operation wasn't legal for
	 * this type. OpBoolean::IS_TRUE if the operation was
	 * successful. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	virtual OP_BOOLEAN Consolidate() = 0;

	/**
	 * Apply a value to a certain place in the list.
	 *
	 * Called when an item has been replaced or appended to the list.
	 *
	 * @param idx The index of the item to copy item to
	 * @param item The value of item that will be used in copying.
	 */
	virtual OP_BOOLEAN ApplyChange(UINT32 idx, SVGDOMItem* item) = 0;

	/**
	 * Get a dom object
	 */
	virtual DOM_Object* GetDOMObject(UINT32 idx) = 0;

	/**
	 * Set a dom object within a list
	 */
	virtual OP_STATUS	SetDOMObject(SVGDOMItem* item, DOM_Object* obj) = 0;

	/**
	 * This method should be called when the DOM object isn't held by
	 * anyone else than this list. This means we can drop it now and
	 * create a new one if anyone ever asks for it. It can't be
	 * noticed that it isn't the same DOM object as it was before.
	 *
	 * This is mainly an memory and performance optimization, but
	 * objects must be released wen this functions is called because
	 * the pointer will be invalid at a later stage.
	 *
	 * @param item The item to be released. Can be NULL, and in that
	 * case nothing should be done.
	 */
	virtual void		ReleaseDOMObject(SVGDOMItem *item) = 0;

	SVGDOMItemType ListType() { return list_type; }

protected:
	SVGDOMItemType list_type;
};

class SVGDOMStringList : public SVGDOMItem
{
public:
	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_STRING_LIST || SVGDOMItem::IsA(type); }

	/**
	 * Clears all existing current items from the list, with the result being an empty list.
	 */
	virtual void Clear() = 0;

	/**
	 * Clears all existing current items from the list and
	 * re-initializes the list to hold the single item specified by
	 * the parameter.
	 */
	virtual OP_STATUS Initialize(const uni_char* new_item) = 0;

	/**
	 * Returns the specified item from the list.
	 *
	 * @param idx The index to fetch the item of
	 * @param item The item is saved here
	 * @return OpBoolean::IS_TRUE is the element is
	 * found. OpBoolean::IS_FALSE if the idx was
	 * out-of-bounds. OpStatus::ERR_NO_MEMORY if OOM.
	 */
	virtual OP_BOOLEAN GetItem(UINT32 index, const uni_char*& item) = 0;

	/**
	 * Inserts a new item into the list at the specified position. The
	 * first item is number 0.
	 *
	 * @param new_item The item to insert
	 * @param index The index to insert item at
	 */
	virtual OP_BOOLEAN InsertItemBefore(const uni_char* new_item, UINT32 index) = 0;

	/**
	 * Removes an existing item from the list.
	 *
	 * @param index The index to remove
	 * @param old_item The removed item is saved here if the index was valid, otherwise NULL.
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY
	 */
	virtual OP_STATUS RemoveItem (UINT32 index, const uni_char*& removed_item) = 0;

	/**
	 * The number of items in the list.
	 * @return The number of items in the list.
	 */
	virtual UINT32 GetCount() = 0;

	/**
	 * Apply a value to a certain place in the list.
	 *
	 * Called when an item has been replaced or appended to the list.
	 *
	 * @param idx The index of the item to copy item to
	 * @param item The value of item that will be used in copying.
	 * @return OpBoolean::IS_TRUE if
	 * successful. OpStatus::ERR_NO_MEMORY if OOM. OpBoolean::IS_FALSE
	 * if this item can't be applied.
	 */
	virtual OP_BOOLEAN ApplyChange(UINT32 idx, const uni_char* item) = 0;
};

class SVGDOMPathSeg : public SVGDOMItem
{
public:
	enum PathSegType
	{
		SVG_DOM_PATHSEG_UNKNOWN								= 0,
		SVG_DOM_PATHSEG_CLOSEPATH							= 1,
		SVG_DOM_PATHSEG_MOVETO_ABS							= 2,
		SVG_DOM_PATHSEG_MOVETO_REL							= 3,
		SVG_DOM_PATHSEG_LINETO_ABS							= 4,
		SVG_DOM_PATHSEG_LINETO_REL							= 5,
		SVG_DOM_PATHSEG_CURVETO_CUBIC_ABS					= 6,
		SVG_DOM_PATHSEG_CURVETO_CUBIC_REL					= 7,
		SVG_DOM_PATHSEG_CURVETO_QUADRATIC_ABS				= 8,
		SVG_DOM_PATHSEG_CURVETO_QUADRATIC_REL				= 9,
		SVG_DOM_PATHSEG_ARC_ABS								= 10,
		SVG_DOM_PATHSEG_ARC_REL								= 11,
		SVG_DOM_PATHSEG_LINETO_HORIZONTAL_ABS				= 12,
		SVG_DOM_PATHSEG_LINETO_HORIZONTAL_REL				= 13,
		SVG_DOM_PATHSEG_LINETO_VERTICAL_ABS					= 14,
		SVG_DOM_PATHSEG_LINETO_VERTICAL_REL					= 15,
		SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_ABS			= 16,
		SVG_DOM_PATHSEG_CURVETO_CUBIC_SMOOTH_REL			= 17,
		SVG_DOM_PATHSEG_CURVETO_QUADRATIC_SMOOTH_ABS		= 18,
		SVG_DOM_PATHSEG_CURVETO_QUADRATIC_SMOOTH_REL		= 19
	};

	SVGDOMPathSeg() {}
	virtual ~SVGDOMPathSeg() {}

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_PATHSEG || SVGDOMItem::IsA(type); }

	virtual double GetX() = 0;
	virtual double GetY() = 0;
	virtual double GetX1() = 0;
	virtual double GetY1() = 0;
	virtual double GetX2() = 0;
	virtual double GetY2() = 0;
	virtual double GetR1() = 0;
	virtual double GetR2() = 0;
	virtual double GetAngle() = 0;
	virtual BOOL GetLargeArcFlag() = 0;
	virtual BOOL GetSweepFlag() = 0;

	virtual OP_BOOLEAN SetX(double val) = 0;
	virtual OP_BOOLEAN SetY(double val) = 0;
	virtual OP_BOOLEAN SetX1(double val) = 0;
	virtual OP_BOOLEAN SetY1(double val) = 0;
	virtual OP_BOOLEAN SetX2(double val) = 0;
	virtual OP_BOOLEAN SetY2(double val) = 0;
	virtual OP_BOOLEAN SetR1(double val) = 0;
	virtual OP_BOOLEAN SetR2(double val) = 0;
	virtual OP_BOOLEAN SetAngle(double val) = 0;
	virtual OP_BOOLEAN SetLargeArcFlag(BOOL large) = 0;
	virtual OP_BOOLEAN SetSweepFlag(BOOL sweep) = 0;

	virtual int GetSegType() const = 0;
	virtual uni_char GetSegTypeAsLetter() const = 0;

	virtual BOOL IsValid(UINT32 idx) = 0;
};

class SVGDOMPath : public SVGDOMItem
{
public:
	enum SVGPathCodes
	{
		SVGPATH_MOVETO = 77,
		SVGPATH_LINETO = 76,
		SVGPATH_CURVETO = 67,
		SVGPATH_QUADTO = 81,
		SVGPATH_CLOSE = 90
	};

	SVGDOMPath() {}
	virtual ~SVGDOMPath() {}

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_PATH || SVGDOMItem::IsA(type); }

	virtual OP_STATUS GetSegment(unsigned long cmdIndex, short& outsegment) = 0;
	virtual OP_STATUS GetSegmentParam(unsigned long cmdIndex, unsigned long paramIndex, double& outparam) = 0;
	virtual UINT32 GetCount() = 0;

	virtual SVGPath* GetPath() = 0;
};

class SVGDOMRGBColor : public SVGDOMItem
{
public:
	SVGDOMRGBColor() {}
	virtual ~SVGDOMRGBColor() {}

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_RGBCOLOR || SVGDOMItem::IsA(type); }

	virtual OP_STATUS GetComponent(int type, double& val) = 0;
	virtual OP_STATUS SetComponent(int type, double val) = 0;
};

class SVGDOMCSSValue : public SVGDOMItem
{
public:
	enum UnitType
	{
		SVG_DOM_CSS_INHERIT                    = 0,
		SVG_DOM_CSS_PRIMITIVE_VALUE            = 1,
		SVG_DOM_CSS_VALUE_LIST                 = 2,
		SVG_DOM_CSS_CUSTOM                     = 3
	};

	SVGDOMCSSValue() {}
	virtual ~SVGDOMCSSValue() {}

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_CSSVALUE || SVGDOMItem::IsA(type); }

	virtual OP_STATUS GetCSSText(TempBuffer *buffer) = 0;
	virtual OP_STATUS SetCSSText(uni_char* buffer) = 0;

	virtual UnitType GetCSSValueType() = 0;
};

/**
 * SVGDOMCSSPrimitiveValue is only implemented because we need it for
 * css::RGBColor. Therefor only CSS_NUMBER is supported.
 */
class SVGDOMCSSPrimitiveValue : public SVGDOMCSSValue
{
public:
	virtual ~SVGDOMCSSPrimitiveValue() {}

	enum UnitType {
		SVG_DOM_CSS_UNKNOWN                    = 0,
		SVG_DOM_CSS_NUMBER                     = 1,
		SVG_DOM_CSS_PERCENTAGE                 = 2,
		SVG_DOM_CSS_EMS                        = 3,
		SVG_DOM_CSS_EXS                        = 4,
		SVG_DOM_CSS_PX                         = 5,
		SVG_DOM_CSS_CM                         = 6,
		SVG_DOM_CSS_MM                         = 7,
		SVG_DOM_CSS_IN                         = 8,
		SVG_DOM_CSS_PT                         = 9,
		SVG_DOM_CSS_PC                         = 10,
		SVG_DOM_CSS_DEG                        = 11,
		SVG_DOM_CSS_RAD                        = 12,
		SVG_DOM_CSS_GRAD                       = 13,
		SVG_DOM_CSS_MS                         = 14,
		SVG_DOM_CSS_S                          = 15,
		SVG_DOM_CSS_HZ                         = 16,
		SVG_DOM_CSS_KHZ                        = 17,
		SVG_DOM_CSS_DIMENSION                  = 18,
		SVG_DOM_CSS_STRING                     = 19,
		SVG_DOM_CSS_URI                        = 20,
		SVG_DOM_CSS_IDENT                      = 21,
		SVG_DOM_CSS_ATTR                       = 22,
		SVG_DOM_CSS_COUNTER                    = 23,
		SVG_DOM_CSS_RECT                       = 24,
		SVG_DOM_CSS_RGBCOLOR                   = 25
	};

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_CSSPRIMITIVEVALUE || SVGDOMCSSValue::IsA(type); }

	/**
	 * The type of the value as defined by the constants specified
	 * above. Read-only.
	 *
	 * We only support CSS_NUMBER
	 */
	virtual UnitType		GetPrimitiveType() = 0;

	/**
	 * A method to set the float value with a specified unit. If the
	 * property attached with this value can not accept the specified
	 * unit or the float value, the value will be unchanged and a
	 * DOMException will be raised.
	 *
	 * We only support CSS_NUMBER
	 */
	virtual OP_BOOLEAN		SetFloatValue(UnitType unit_type, double float_value) = 0;

	/**
	 * This method is used to get a float value in a specified
	 * unit. If this CSS value doesn't contain a float value or can't
	 * be converted into the specified unit, a DOMException is raised.
	 *
	 * We only support CSS_NUMBER
	 */
	virtual OP_BOOLEAN		GetFloatValue(UnitType unit_type, double& float_value) = 0;

	/**
	 * A method to set the string value with the specified unit. If
	 * the property attached to this value can't accept the specified
	 * unit or the string value, the value will be unchanged and a
	 * DOMException will be raised.
	 *
	 * Not supported
	 */
	/* virtual void			SetStringValue(UnitType unit_type,
	   const char* string_value) = 0; */

	/**
	 * This method is used to get the string value. If the CSS value
	 * doesn't contain a string value, a DOMException is raised.
	 *
	 * Not supported
	 */
	/* virtual OP_BOOLEAN		GetStringValue(const char*& str) = 0; */

	/**
	 * Not supported
	 */
	/* Counter            getCounterValue() */

	/**
	 * Not supported
	 */
	/* Rect               getRectValue() */

	/**
	 * Not supported
	 */
	/* RGBColor           getRGBColorValue() */
};

class SVGDOMCSSRGBColor : public SVGDOMItem
{
public:
	virtual ~SVGDOMCSSRGBColor() {}

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_CSSRGBCOLOR || SVGDOMItem::IsA(type); }

	virtual SVGDOMCSSPrimitiveValue* GetRed() = 0;
	virtual SVGDOMCSSPrimitiveValue* GetGreen() = 0;
	virtual SVGDOMCSSPrimitiveValue* GetBlue() = 0;
};

class SVGDOMColor : public SVGDOMCSSValue
{
public:
	enum ColorType
	{
		/**
		 * The color type is not one of predefined types. It is invalid
		 * to attempt to define a new value of this type or to attempt
		 * to switch an existing value to this type.
		 */
		SVG_COLORTYPE_UNKNOWN = 0,

		/**
		 * An sRGB color has been specified without an alternative ICC
		 * color specification.
		 */
		SVG_COLORTYPE_RGBCOLOR = 1,

		/**
		 * An sRGB color has been specified along with an alternative
		 * ICC color specification.
		 */
		SVG_COLORTYPE_RGBCOLOR_ICCCOLOR = 2,

		/**
		 * Corresponds to when keyword 'currentColor' has been
		 * specified.
		 */
		SVG_COLORTYPE_CURRENTCOLOR = 3
	};

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_COLOR || SVGDOMItem::IsA(type); }

	/**
	 * The type of the value as specified by one of the constants
	 * specified above.
	 */
	virtual ColorType GetColorType() = 0;

	/**
	 * The color specified in the sRGB color space.
	 */
	virtual OP_BOOLEAN		GetRGBColor(SVGDOMCSSRGBColor*& rgb_color) = 0;

	/**
	 * Modifies the color value to be the specified sRGB color without
	 * an alternate ICC color specification.
	 *
	 * @param rgb_color The new color value
	 * @return OpBoolean::IS_TRUE on success. OpBoolean::IS_FALSE if
	 * rgb_color is an illegal value.
	 */
	virtual OP_BOOLEAN SetRGBColor(const uni_char* rgb_color) = 0;

	/**
	 * Not supported
	 */
	/* virtual GetICCColor() = 0 ; */

	/**
	 * Not supported
	 */
	/* virtual SetRGBColorICCColor() = 0 ; */

	/**
	 *
	 * SVG_COLORTYPE_RGBCOLOR_ICCCOLOR only sets rgbcolor
	 *
	 * @param icc_color is ignored.
	 */
	virtual OP_BOOLEAN SetColor(ColorType color_type, const uni_char* rgb_color, uni_char* icc_color) = 0;
};

class SVGDOMPaint : public SVGDOMColor
{
public:
	enum PaintType
	{
		/**
		 * The paint type is not one of predefined types. It is
		 * invalid to attempt to define a new value of this type or to
		 * attempt to switch an existing value to this type.
		 */
		SVG_PAINTTYPE_UNKNOWN = SVGPaint::UNKNOWN,

		/**
		 * An sRGB color has been specified without an alternative ICC
		 * color specification.
		 */
		SVG_PAINTTYPE_RGBCOLOR = SVGPaint::RGBCOLOR,

		/**
		 * An sRGB color has been specified along with an alternative
		 * ICC color specification.
		 */
		SVG_PAINTTYPE_RGBCOLOR_ICCCOLOR = SVGPaint::RGBCOLOR_ICCCOLOR,

		/**
		 * Corresponds to a 'none' value on a <paint> specification.
		 */
		SVG_PAINTTYPE_NONE = SVGPaint::NONE,

		/**
		 * Corresponds to a 'currentColor' value on a <paint>
		 * specification.
		 */
		SVG_PAINTTYPE_CURRENTCOLOR = SVGPaint::CURRENT_COLOR,

		/**
		 * A URI has been specified, along with an explicit 'none' as
		 * the backup paint method in case the URI is unavailable or
		 * invalid.
		 */
		SVG_PAINTTYPE_URI_NONE = SVGPaint::URI_NONE,

		/**
		 * A URI has been specified, along with 'currentColor' as the
		 * backup paint method in case the URI is unavailable or
		 * invalid.
		 */
		SVG_PAINTTYPE_URI_CURRENTCOLOR = SVGPaint::URI_CURRENT_COLOR,

		/**
		 * A URI has been specified, along with an sRGB color as the
		 * backup paint method in case the URI is unavailable or
		 * invalid.
		 */
		SVG_PAINTTYPE_URI_RGBCOLOR = SVGPaint::URI_RGBCOLOR,

		/**
		 * A URI has been specified, along with both an sRGB color and
		 * alternate ICC color as the backup paint method in case the
		 * URI is unavailable or invalid.
		 */
		SVG_PAINTTYPE_URI_RGBCOLOR_ICCCOLOR = SVGPaint::URI_RGBCOLOR_ICCCOLOR,

		/**
		 * Only a URI has been specified.
		 */
		SVG_PAINTTYPE_URI = SVGPaint::URI
	};

	virtual BOOL IsA(SVGDOMItemType type) { return type == SVG_DOM_ITEMTYPE_PAINT || SVGDOMColor::IsA(type); }

	/**
	 * Get paint type
	 * @return The type of paint, identified by one of the constants above.
	 */
	virtual PaintType GetPaintType() = 0;

	/**
	 * Get URI
	 * @return When the paintType specifies a URI, this function returns the
	 * URI string. When the paintType does not specify a URI, it returns NULL.
	 */
	virtual const uni_char* GetURI() = 0;

	/**
	 * Set URI. Sets the paintType to SVG_PAINTTYPE_URI_NONE and sets uri to
	 * the specified value.
	 * @param uri The uri to set. Must not be NULL.
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY.
	 */
	virtual OP_STATUS SetURI(const uni_char* uri) = 0;

	/**
	 * Sets the paint type as specified by the parameters.
	 *
	 * @param type The paint type to be set
	 * @param uri If 'type' requires a URI, then uri must be non-NULL
	 * and a valid string; otherwise, uri must be NULL.
	 * @param rgb_color If 'type' requires an RGBColor,
	 * then rgb_color must be a valid RGBColor object; otherwise,
	 * rgbColor must be NULL.
	 * @param icc_color If 'type' requires an SVGICCColor,
	 * then icc_color must be a valid SVGICCColor object; otherwise,
	 * icc_color must be null.
	 * @return OpBoolean::IS_TRUE if OK OpBoolean::IS_FALSE if one or
	 * more values are invalid, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	virtual OP_BOOLEAN SetPaint(PaintType type, const uni_char* uri,
								const uni_char* rgb_color, const uni_char* icc_color) = 0;

};

#endif // SVG_DOM
#endif // SVG_DOM_INTERFACES_H
