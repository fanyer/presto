/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2005-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SVG_UTILS_H
#define SVG_UTILS_H

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/SVGLength.h"
#include "modules/svg/src/SVGVector.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGElementResolverContext.h"

class HTML_Element;
class SVG_DOCUMENT_CLASS;
class OpBpath;
class VisualDevice;
class SVGCanvas;
class SVGCanvasState;
class SVGTextArguments;
class TempBuffer;
class URL;
class SVGRect;
class HTMLayoutProperties;
class SVGDocumentContext;
struct SVGTextChunk;

struct SVGValueContext
{
	/**
	 * Serves as default ctor also. Uses some fallback magic numbers
	 * as default params.
	 */
	SVGValueContext(const SVGNumber& viewport_width = 0,
					const SVGNumber& viewport_height = 0,
					const SVGNumber& font_size = 10,
					const SVGNumber& ex_height = 5,
					const SVGNumber& root_font_size = 10)
		: fontsize(font_size)
		, ex_height(ex_height)
		, root_font_size(root_font_size)
		, viewport_width(viewport_width)
		, viewport_height(viewport_height) {}

	SVGNumber fontsize;
	/** For the referred font:
		The height of 'x' glyph part that is above the baseline. */
	SVGNumber ex_height;
	/** The size of the font of the root element of the document in which
		the element, that is referred by this SVGValueContext, is located. */
	SVGNumber root_font_size;

	SVGNumber viewport_width;
	SVGNumber viewport_height;
};

struct SVGTextRange
{
	SVGTextRange(int idx, int len) : index(idx), length(len) {}

	BOOL IsEmpty() const { return length <= 0; }
	BOOL Contains(int idx) const
	{
		return idx >= index && idx < index + length;
	}
	BOOL Intersecting(int idx, int len)
	{
		return index < idx + len && idx < index + length;
	}
	void IntersectWith(int idx, int len)
	{
		index = MAX(index, idx);
		length = MIN(index + length, idx + len) - index;
	}

	int index;
	int length;
};

class SVGTextData
{
public:
	enum SVGTextDataEnum
	{
		EXTENT				= 0x01,		///< Get the visual length (in user units)
		ENDPOSITION			= 0x02,		///< Get the endposition (in user units)
		ANGLE				= 0x04,		///< Get the rotation-angle (in degrees)
		BBOX				= 0x08,		///< Get the boundingbox (in user units)
		NUMCHARS			= 0x10,		///< Get the number of chars that would be drawn
		CHARATPOS			= 0x20,		///< Get the index of the character at a position, or -1 for not found
		SELECTEDSTRING		= 0x40,		///< Get the unicode string that is currently selected
		STARTPOSITION		= 0x80,		///< Get the startposition (in user units)
		SELECTIONRECTLIST	= 0x100		///< Get the list of rectangles enclosing the selected text (in enclosing document coordinates).
	};

	/**
	* Provide OR:ed together values from the enum above, but don't mix rendermode and non-rendermode enums.
	*/
	SVGTextData(unsigned int desiredinfo) 
		: chunk_list(NULL), extent(0), startpos(0,0), endpos(0,0),
		  angle(0), range(-1, 0), numchars(0), buffer(NULL), selection_rect_list(NULL), desiredinfo_init(0) 
		{
			desired_info.enums = desiredinfo;
			if(desired_info.enums & (BBOX | CHARATPOS | ANGLE | SELECTEDSTRING | SELECTIONRECTLIST))
			{
				desired_info.needs_render_mode = 1;
			}
		}

	BOOL			SetExtent() { return (desired_info.enums & EXTENT) != 0; }
	BOOL			SetEndPosition() { return (desired_info.enums & ENDPOSITION) != 0; }
	BOOL			SetStartPosition() { return (desired_info.enums & STARTPOSITION) != 0; }
	BOOL			SetAngle() { return (desired_info.enums & ANGLE) != 0; }
	BOOL			SetBBox() { return (desired_info.enums & BBOX) != 0; }
	BOOL			SetNumChars() { return (desired_info.enums & NUMCHARS) != 0; }
	BOOL			SetCharNumAtPosition() { return (desired_info.enums & CHARATPOS) != 0; }
	BOOL			SetString() { return (desired_info.enums & SELECTEDSTRING) != 0; }
	BOOL			SetSelectionRectList() { return (desired_info.enums & SELECTIONRECTLIST) != 0; }
	
	BOOL			NeedsRenderMode() { return desired_info.needs_render_mode; }

	OpVector<SVGTextChunk>* chunk_list;
	SVGNumber		extent;
	SVGNumberPair	startpos;
	SVGNumberPair	endpos;
	SVGNumber		angle;
	SVGBoundingBox	bbox;
	SVGTextRange	range;
	unsigned int	numchars;
	TempBuffer*		buffer;
	OpVector<SVGRect>* selection_rect_list;

private:
	union
	{
		struct
		{
			unsigned int needs_render_mode:1;
			unsigned int enums:9; ///< SVGTextDataEnum
			/* 10 bits */
		} desired_info;

		unsigned int desiredinfo_init;
	} ;
};

template <class T>
class SVGStack : protected OpVector<T>
{
public:
	OP_STATUS Push(T* node) { return OpVector<T>::Add(node); }
	T* Top() { return OpVector<T>::Get(OpVector<T>::GetCount() - 1); }
	T* Pop() { return OpVector<T>::Remove(OpVector<T>::GetCount() - 1); }
	BOOL IsEmpty() const { return OpVector<T>::GetCount() == 0; }
	void Clear() { OpVector<T>::Clear(); }
};

class SVGUtils
{
public:
	/**
	* Find the element with matching id in the given document.
	* @param logdoc The document to search in
	* @param id The id we want to look for
	* @return The wanted element, or NULL if not found
	*/
	static HTML_Element* FindElementById(LogicalDocument* logdoc, const uni_char* id);

	/**
	 * Looks at xlink:href and finds the node in the document that
	 * href refers to, or NULL if it doesn't exist or isn't loaded
	 * yet.
	 *
	 * Does not start any loading and is safe to use from call sites
	 * where traversions and/or reflows may be on the stack.
	 *
	 * @param doc_ctx The SVGDocumentContext where the SVG lives.
	 *
	 * @param elm The element with the xlink:href attribute.
	 *
	 * @param svg_root The root of the SVG document (the svg:svg element)
	 *
	 * @param has_xlink_href optional argument in case the caller has
	 *    to know if the attribute existed. *has_xlink_href will in
	 *    that case be set to TRUE, otherwise FALSE. If this is
	 *    uninteresting, NULL can be sent in, which also is the
	 *    default value if not specified..
	 *
	 * @returns The element the xlink:href pointed at, if it was
	 * loaded and existed. NULL is returns otherwise.
	 */
	static HTML_Element* FindHrefReferredNode(SVGElementResolver* resolver, SVGDocumentContext* doc_ctx, HTML_Element* elm, BOOL* has_xlink_href = NULL);

	/**
	 * Attaches a FramesDocElm element to 'frame_element' and starts
	 * loading the URL specified in the xlink:href attribute of
	 * 'frame_element', which is assumed to be the same as the
	 * supplied 'url' (used for security checks).
	 */
	static void LoadExternalDocument(URL &url, FramesDocument *frm_doc, HTML_Element* frame_element);

private:
	/**
	 * Load external references/resources for an element. Calls
	 * LoadExternalDocument to do the heavy lifting.
	 *
	 * Called when a element is created and inserted into the tree.
	 *
	 * @param element The element to load external references for
	 */
	static void LoadExternalPaintServers(SVGDocumentContext *doc_ctx, const URL& doc_url, URL& base_url, HTML_Element *element, const SVGPaint *paint);
	static void LoadExternalResource(SVGDocumentContext *doc_ctx, const URL& doc_url, URL& base_url, HTML_Element *element, const uni_char* resource_uri_str);

public:

	/**
	 * Load external references for an element. Calls
	 * (Frames)Document::LoadInline and LoadExternalDocument and to do
	 * the heavy lifting.
	 *
	 * Called when a element is created and inserted into the tree.
	 */
	static void LoadExternalReferences(SVGDocumentContext* doc_ctx, HTML_Element *element);

	/**
	 * Load external references for an element, based on values in the
	 * cascade. Calls LoadExternalPaintServers and
	 * LoadExternalCSSReference respectively on the different
	 * properties.
	 *
	 * Called during layout.
	 */
	static void LoadExternalReferencesFromCascade(SVGDocumentContext* doc_ctx, HTML_Element *element, LayoutProperties* cascade);

	/**
	 * Finds a node that the relative part of an url refers to in the
	 * _local_ document specified by document. Supports the id version
	 * of xpointer (see the SVG specification) so that
	 * http://www.opera.com/#xpointer(id('hej')) is equivalent with
	 * http://www.opera.com/#hej, but note that it does not respect
	 * the xml:base attribute, that has to be done before using this
	 * function.
	 *
	 * @param locdoc The logical document to search in
	 *
	 * @param url_rel The id to search for
	 *
	 * @returns The element with the searched-for id, or NULL if the
	 *   element could not be found
	 */
	static HTML_Element* FindDocumentRelNode(LogicalDocument* logdoc, const uni_char* url_rel);

	/**
	 * Find a element referenced by an optional rel_name in an
	 * external document. If rel_name is NULL, you get the document
	 * root. Only returns elements in the SVG namespace.
	 *
	 * Usually it is SVGDocumentContext::GetElementByReference you
	 * want to use. It can handle both the local and external case.
	 *
	 * @param document The document of the frame element
	 *
	 * @param frame_element The frame element the external resource is
	 *   attached to.
	 *
	 * @param rel_name An optiona argument which specifies a relative
	 *   name into the external document.
	 *
	 * @returns The root element of the external document or the
	 *   element matching rel_name in the external document. NULL if
	 *   the element wasn't found.
	 */
	static HTML_Element* FindURLReferredNode(SVGElementResolver* resolver,
											 SVG_DOCUMENT_CLASS* document, HTML_Element *frame_element,
											 const uni_char *rel_name = NULL);

	/**
	 * Find a element referenced by an rel_name in an external
	 * document.
	 *
	 * Usually it is SVGDocumentContext::GetElementByReference you
	 * want to use. It can handle both the local and external case.
	 *
	 * @param document The document of the frame element
	 *
	 * @param frame_element The frame element the external resource is
	 *   attached to.
	 *
	 * @param url_rel An SVGURL that should have a rel-part, which may or
	 *   may not be in the current document. If the url has no rel-part
	 *   NULL will be returned.
	 *
	 * @return The root element of the external document or the
	 *   element matching rel_name in the external document. NULL if
	 *   the element wasn't found.
	 */
	static HTML_Element* FindURLRelReferredNode(SVGElementResolver* resolver,
												SVGDocumentContext* doc_ctx, HTML_Element *traversed_elm,
												SVGURL& url_rel);

	/**
	* Given a child of a SVG document, find the root element. If this is in a nested SVG,
	* then the closest SVG element will be returned.
	* @param child The child
	* @return The root element, or NULL if an error occurred
	*/
	static HTML_Element* GetRootSVGElement(HTML_Element *child);
	
	/**
	* Given a child of a SVG document, find the root element. If this is in a nested SVG,
	* then the topmost SVG element will be returned. This is supposed to return the 
	* svg element that contains the SVGDocumentContext for the child element, but 
	* right now it only returns the SVG element found highest up in the tree.
	* When we support foreign objects that can contain XHTML with another SVG in them,
	* this might have to change.
	*
	* @param child The child
	* @return The root element, or NULL if none were found. This shouldn't be possible in a legal SVG document.
	*/
	static HTML_Element* GetTopmostSVGRoot(HTML_Element *child);

	/**
	* Given the child of an SVG document, find the topmost document context.
	*
	* NOTE: This is _NOT_ the same as
	* GetSVGDocumentContext(GetTopmostSVGRoot(elm)), since this
	* function will cross namespace (and document) boundaries.
	*
	* @param elm The child
	* @return The topmost document context
	*/
	static SVGDocumentContext* GetTopmostSVGDocumentContext(HTML_Element* elm);

	/**
	* Get the viewport for a given element.
	* @param e The element
	* @param doc_ctx The document context
	* @param viewport The viewport will be returned in this parameter, it's valid only if the return value indicates success
	* @param viewport_xy The viewport offset, NULL means don't care about it
	*/
	static OP_STATUS GetViewportForElement(HTML_Element* e, SVGDocumentContext* doc_ctx, SVGNumberPair& viewport, SVGNumberPair* viewport_xy = NULL, SVGMatrix* outviewportTransform = NULL);

	/**
	* Checks if the given element is a graphic element (something that should be drawn).
	* @return TRUE if it is a graphic element, FALSE if not
	*/
	static BOOL	IsGraphicElement(HTML_Element *e);
	
	/**
	* Checks if the given element is a container element (something that should propagate attributes downwards).
	* @return TRUE if it is a container element, FALSE if not
	*/
	static BOOL	IsContainerElement(HTML_Element *e);

	/**
	 * Checks if the given type is a text element but not a text root.
	 */
	static BOOL IsTextChildType(Markup::Type type);

	/**
	 * Checks if the given type is a text-root type.
	 * @return TRUE if it is, FALSE if not
	 */
	static BOOL IsTextRootType(Markup::Type type)
	{
		return type == Markup::SVGE_TEXT || type == Markup::SVGE_TEXTAREA;
	}

	/**
	 * Checks if the given type is a textclass type.
	 * @return TRUE if it is, FALSE if not
	 */
	static BOOL IsTextClassType(Markup::Type type)
	{
		return IsTextRootType(type) || IsTextChildType(type);
	}

	/**
	 * Checks if an element is a potential root for text.
	 * @return TRUE if it is, FALSE if not
	 */
	static BOOL IsTextRootElement(HTML_Element* e);
	
	/**
	* Checks if the given element is an animation element.
	* @return TRUE if it is an animation element, FALSE if not
	*/
	static BOOL IsAnimationElement(HTML_Element *e);

	/**
	* Checks if the given element is an timing element.
	* @return TRUE if it is an timing element, FALSE if not
	*/
	static BOOL IsTimedElement(HTML_Element *e);

	/**
	 * Check if the given element has any children that are
	 * non-special and non-text.
	 */
	static inline BOOL HasChildren(HTML_Element *e)
	{
		HTML_Element* child = e->FirstChild();
		while (child && !Markup::IsRealElement(child->Type()))
			child = child->Suc();

		return !!child;
	}

	/**
	* Checks if the given elementtype is allowed as a child of a 'clipPath' element.
	* @return TRUE if allowed, FALSE otherwise
	*/
	static BOOL IsAllowedClipPathElementType(Markup::Type type);

	/**
	 * Checks if the given elementtype is allowed as a child of a 'mask' element.
	 * @return TRUE if allowed, FALSE otherwise
	 */
	static BOOL IsAllowedMaskElementType(Markup::Type type);

	/**
	 * Simple helper for determining if an element is a external proxy
	 * elements. External proxy elements are foreign object element
	 * marked with GetInserted() == HE_INSERTED_BY_SVG that are used
	 * for loading external resources like paint servers and SVG fonts.
	 */
	static BOOL IsExternalProxyElement(HTML_Element *e)
	{
		return (e->GetInserted() == HE_INSERTED_BY_SVG &&
				e->IsMatchingType(Markup::SVGE_FOREIGNOBJECT, NS_SVG));
	}

	/**
	 * Checks if the given attribute on the given element is stored as 
	 * ITEM_TYPE_STRING instead of as an SVGAttribute.
	 */
	static BOOL IsItemTypeTextAttribute(HTML_Element* e, int attr, NS_Type attr_ns);

	/**
	 * Checks if the given elementtype can have an xlink:href attribute pointing to an external resource.
	 * @return TRUE if permitted, FALSE otherwise
	 */
	static BOOL CanHaveExternalReference(Markup::Type type);

	/**
	* Evaluates the 'requiredFeatures', 'requiredExtensions' and 'systemLanguage' attributes
	* on the element, and return whether it meets the requirements and thus should be processed further.
	* @param e The element to check
	* @return TRUE if element meets requirements, FALSE if not
	*/
	static BOOL ShouldProcessElement(HTML_Element* e);
	
	/**
	* Checks if our implementation has support for all of the given features.
	* @param list A list of required features
	* @return TRUE if we support all of the features, FALSE otherwise
	*/
	static BOOL HasFeatures(SVGVector *list);

	/**
	 * Checks if our implementation has support for a given feature.
	 * @param feature A string representation of the features
	 * @param str_len The length of the feature string
	 *
	 * @return TRUE if we support all of the features, FALSE
	 * otherwise. We return FALSE on unknown features also.
	 *
	 */
	static BOOL HasFeature(const uni_char* feature, UINT32 str_len);
	
	/**
	* Checks if the user perference in Opera has listed any of the given languages.
	* @param list A list of required languages
	* @return TRUE if we prefer any of the given languages, FALSE otherwise
	*/
	static BOOL HasSystemLanguage(SVGVector* list);

	/**
	* Checks if our implementation has support for a given extension.
	* @param list A list of required extensions
	* @return TRUE if we support all of the extensions, FALSE
	* otherwise. We return FALSE on unknown extensions also.
	*/
	static BOOL HasRequiredExtensions(SVGVector *list);

#ifdef SVG_TINY_12
	/**
	* Checks if our implementation has support for all of the given formats.
	* @param list A list of required formats
	* @return TRUE if we support all of the formats, FALSE otherwise
	*/
	static BOOL HasFormats(SVGVector* list);

	/**
	* Checks if our implementation has support for all of the given fonts.
	* @param list A list of required fonts (can be both SVGFonts and systemfonts)
	* @return TRUE if we support all of the fonts, FALSE otherwise
	*/
	static BOOL HasFonts(SVGVector *list);
#endif // SVG_TINY_12

	/**
	 * Convert the length in user coordinates to a specific unit with access to
	 * external dependencies like dpi and fontsize.
	 *
	 * @param number The number to convert (in userunits)
	 * @param unit The unit to use
	 * @param type The type/orientation of length
	 * @param context The values to resolve relative values against
	 * @return The translated length in user coordinates.
	 */
	static SVGNumber ConvertToUnit(SVGNumber number,
								   int unit, SVGLength::LengthOrientation type,
								   const SVGValueContext& context);

	/**
	 * Calculates a new scalar value when unit changes from X to Y, preserving the actual length.
	 * @param obj The object to convert units for
	 * @param to_unit Convert to this CSS unit
	 * @param type The type of length
	 * @param context The values to resolve relative values against
	 */
	static OP_STATUS ConvertToUnit(SVGObject* obj,
								   int to_unit, SVGLength::LengthOrientation type,
								   const SVGValueContext& context);

	/**
	 * Gets the x-height of the given font with the given size.
	 * That is the height of the 'x' glyph part that is above the baseline.
	 * If the ex height couldn't be fetched from the font engine
	 * or the font engine claims it is zero, the ex height is assumed to be
	 * font_size / 2.
	 *
	 * Fetching ex height will fail in the following cases:
	 * - null or invalid param(s)
	 * - the font engine doesn't support getting glyph properties
	 * - some error inside the font engine when fetching glyph properties
	 *
	 * @param vd The VisualDevice that manages fonts. NULL is handled.
	 *			 The appropriate font data is set on it as a side effect.
	 * @param font_size The size of the font. Before passing to font engine, it
	 *		  is rounded to an integer.
	 * @param font_number The number of the font (together with font_size provides
	 *		  the needed data to get a desired ex height).
	 * @return The ex height according to the described rules. If the fetched ex height
	 *		   is non zero, it is always an integer (in mathematical way).
	 */
	static SVGNumber GetExHeight(VisualDevice* vd, const SVGNumber& font_size, int font_number);

	/**
	 * Calculate the length in user coordinates.
	 *
	 * @param len The length value and unit.
	 * @param type The length orientation (used to select the appropriate
			  base for percentage length).
	 * @param context The relative lengths context.
	 * @return The length in user coordinates.
	 */
	static SVGNumber ResolveLength(const SVGLength& len, SVGLength::LengthOrientation type,
								   const SVGValueContext& context);

	/**
	 * Get the object type for the combination (element type, attribute type, namespace)
	 */
	static SVGObjectType  GetObjectType(Markup::Type element_type, int attr_type, NS_Type ns);

	/**
	 * Used for reversal of enum value to strings
	 */
	static OP_STATUS GetEnumValueStringRepresentation(SVGEnumType enum_type, UINT32 enum_value, TempBuffer* buffer);

	static OP_STATUS GetRectValues(HTML_Element *e, SVGLengthObject** x, SVGLengthObject** y,
								   SVGLengthObject** width, SVGLengthObject** height,
								   SVGLengthObject** rx, SVGLengthObject** ry);

	static OP_STATUS GetImageValues(HTML_Element *e, const SVGValueContext& vcxt, SVGRect& image_rect);
	static OP_STATUS GetLineValues(HTML_Element *e, SVGLengthObject** x1, SVGLengthObject** y1, SVGLengthObject** x2, SVGLengthObject** y2);
	static void GetPolygonValues(HTML_Element *e, SVGVector*& list);
	static void GetPolylineValues(HTML_Element *e, SVGVector*& list);
	static OP_STATUS GetCircleValues(HTML_Element *e, SVGLengthObject** cx, SVGLengthObject** cy, SVGLengthObject **r);
	static void      AdjustRectValues(SVGNumber width, SVGNumber height, SVGNumber& rx, SVGNumber& ry);

	static OP_STATUS RescaleOutline(OpBpath* outline, SVGNumber scale_factor);

	/**
	 * Create a SVGVector
	 */
	static SVGVector* CreateSVGVector(SVGObjectType type,
									  Markup::Type element_type = Markup::HTE_UNKNOWN,
									  Markup::AttrType attr_type = Markup::HA_NULL);

	static SVGVectorSeparator GetVectorSeparator(Markup::Type element_type, Markup::AttrType attr_type);

	static SVGObjectType GetVectorObjectType(Markup::Type element_type, Markup::AttrType attr_type);

	static SVGEnumType GetEnumType(Markup::Type element_type, Markup::AttrType attr_type);
	static SVGObjectType GetProxyObjectType(Markup::Type element_type, Markup::AttrType attr_type);

	static OP_STATUS GetNumberOptionalNumber(HTML_Element* e, Markup::AttrType attr_type,
											 SVGNumberPair& numoptnum, SVGNumber def = 0);

	/**
	 * Resolve a length, while also considering what type of units the
	 * length is in.  If the units are objectBoundingBox and the
	 * length is a percentage, a fraction will be returned.
	 * Otherwise see ResolveLength.
	 *
	 * @param len The length to resolve, assumed non-NULL
	 * @param orient X/Y orientation of the length
	 * @param units Units (uSOU, oBB) to use if the length is a percentage
	 * @param vcxt The value context
	 * @return The resolved value of the length
	 */
	static SVGNumber ResolveLengthWithUnits(const SVGLengthObject* len,
											SVGLength::LengthOrientation orient,
											SVGUnitsType units, const SVGValueContext& vcxt);

	/**
	 * Same as ResolveLengthWithUnits, but fetches the length object from <elm, attr>
	 * v is not modified if no attribute-object exist.
	 *
	 * @return TRUE if 'attr' had an object and v has been set
	 */
	static BOOL GetResolvedLengthWithUnits(HTML_Element* elm, Markup::AttrType attr,
										   SVGLength::LengthOrientation orient,
										   SVGUnitsType units,
										   const SVGValueContext& vcxt,
										   SVGNumber& v);

	/**
	 * Get the viewbox transform and cliprect.
	 * This is not the same as the viewbox attribute, but we do take that as input.
	 * The resulting transform is affected by aspectratio, by viewport size and the viewbox if any.
	 *
	 * @param viewport The current viewport
	 * @param viewbox The viewbox if any, otherwise just pass NULL
	 * @param ar The aspectratio if any, otherwise just pass NULL
	 * @param tfm The resulting transform will be returned here
	 * @param clipRect The cliprect that can be used for clipping to viewport, in user coordinates (you should concat the out transform before using this)
	 * @return The status of the operation
	 */
	static OP_STATUS GetViewboxTransform(const SVGRect& viewport, const SVGRect* viewbox,
										  const SVGAspectRatio* ar, SVGMatrix& tfm,
										  SVGRect& clipRect);

	/**
	 * Needed because of use elements. When we traverse a tree under a use tag we should really 
	 * layout the tree the use tag refers to. If this is not under a use tag the traversed 
	 * element itself will be returned.
	 *
	 * GetRealNode does the unconditional fetching, while
	 * GetElementToLayout does conditional fetching.
	 */
	static HTML_Element* GetRealNode(HTML_Element* shadow_elm);
	static HTML_Element* GetElementToLayout(HTML_Element* traversed_elm)
	{
		if (IsShadowNode(traversed_elm))
			return GetRealNode(traversed_elm);

		return traversed_elm;
	}


	/**
	 * Check if a given attribute is a presentation attribute, with or 
	 * without the type of element the attribute is on.
	 */
	static BOOL IsPresentationAttribute(Markup::AttrType attr_name);
	static BOOL IsPresentationAttribute(Markup::AttrType attr_name, Markup::Type elm_type);

	/**
	 * Returns true if when the given attribute is changed it can affect this 
	 * element's (or any child element of this element) font metrics.
	 */
	static BOOL AttributeAffectsFontMetrics(Markup::AttrType attr_name, Markup::Type elm_type);

	/**
	 * Fetch the value of the xml:space attribute on this element or
	 * its nearest ancestor
	 */
	static BOOL GetPreserveSpaces(HTML_Element* element);

	/**
	 * Return the extent of the text within the range [startIndex..startIndex+numChars].
	 * The extent is provided in user units and is defined as the sum of the advance values.
	 *
	 * @param elm A textclass element (and thus also a non-shadow
	 *            element - a fact that may need reconsideration)
	 * @param doc_ctx The SVG Document we're in
	 * @param startIndex The startindex, count begins at 0 from the element given
	 * @param numChars The number of chars to get extent from
	 * @param initial_viewport The viewport that will be set on the traversal object
	 * @return OpStatus::OK on success; OpStatus::ERR if 'elm' had an incorrect element
	 *         type, or did not have a text root; OpStatus::ERR_OUT_OF_RANGE if 'startIndex'
	 *         is negative; or OpStatus::ERR_NO_MEMORY if we run out of memory.
	 */
	static OP_STATUS GetTextElementExtent(HTML_Element* elm, SVGDocumentContext* doc_ctx, 
										  int startIndex, int numChars, SVGTextData& info,
										  const SVGNumberPair& initial_viewport,
										  SVGCanvas* canvas = NULL, BOOL needs_extents = TRUE);

	/**
	 * Draw the bitmap, with the specified 'viewport', aspect ratio and quality
	 */
	static OP_STATUS DrawImage(OpBitmap* bitmap,
							   LayoutProperties* layprops, SVGCanvas* canvas,
							   const SVGRect& dst_rect, SVGAspectRatio* ar, int quality);

	/**
	 * Draw the image from the provided URL
	 */
	static OP_STATUS DrawImageFromURL(SVGDocumentContext* doc_ctx,
									  URL* imageURL, HTML_Element* layouted_elm,
									  LayoutProperties* layprops,
									  SVGCanvas* canvas, const SVGRect& dst_rect,
									  SVGAspectRatio* ar, int quality);

	/**
	 * Get the CTM (Current Transformation Matrix) for an element
	 *
	 * @param target The element to get CTM for
	 * @param doc_ctx The SVG Document we're in
	 * @param element_ctm The CTM will be returned here
	 * @param screen_ctm Whether the CTM to get should be the ScreenCTM or not
	 * @return Status of the operation
	 */
	static OP_STATUS GetElementCTM(HTML_Element* target, SVGDocumentContext* doc_ctx, SVGMatrix* element_ctm, BOOL screen_ctm = FALSE);

	/**
	 * Get the transform from one element to another
	 *
	 * @param from_elm Start at this element
	 * @param to_elm This element is the target
	 * @param doc_ctx Evaluate in this context
	 * @param elm_transform The resulting transform (if success)
	 * @return OpBoolean::IS_TRUE if successful, OpBoolean::IS_FALSE if unable to compute, other errors as appropriate.
	 */
	static OP_BOOLEAN GetTransformToElement(HTML_Element* from_elm, HTML_Element* to_elm, SVGDocumentContext* doc_ctx, SVGMatrix& elm_transform);

	/**
	 * Find the nearest/farthest viewport element.
	 *
	 * @param elm The element to start in
	 * @param find_nearest If TRUE then the closest parent viewport element will be returned, else the farthest
	 * @param svg_only Only consider svg elements
	 */
	static HTML_Element* GetViewportElement(HTML_Element* elm, BOOL find_nearest, BOOL svg_only = FALSE);

	/**
	 * Check if this element can define a viewport.
	 * @param type The elementtype, be sure to check NsType first!
	 */
	static BOOL IsViewportElement(Markup::Type type);

	/**
	 * Apply a possible 'clip' to the cliprect sent in
	 * @param cliprect Cliprect to adjust
	 */
	static OP_STATUS AdjustCliprect(SVGRect& cliprect, const HTMLayoutProperties& props);

	/**
	 * Adjust width and height (while keeping the aspect ratio) to fit inside a given
	 * a rectangle of a given size
	 */
	static OP_STATUS ShrinkToFit(int& width, int& height, int max_width, int max_height);

	/**
	 * Checks if a shadowtree has been built for the given element.
	 */
	static BOOL HasBuiltNormalShadowTree(HTML_Element* elm) { return HasBuiltShadowTree(elm, TRUE); }

	/**
	 * Checks if a shadowtree has been built for the given element.
	 */
	static BOOL HasBuiltBaseShadowTree(HTML_Element* elm) { return HasBuiltShadowTree(elm, FALSE); }

private:
	/**
	 * Checks if a shadowtree has been built for the given element.
	 */
	static BOOL HasBuiltShadowTree(HTML_Element* elm, BOOL animated);

public:

	static BOOL IsShadowType(Markup::Type type)
	{
		return (type == Markup::SVGE_SHADOW ||
				type == Markup::SVGE_ANIMATED_SHADOWROOT ||
				type == Markup::SVGE_BASE_SHADOWROOT);
	}

	/**
	 * Checks if the node is a shadow node, i.e. the virtual child 
	 * to a use tag. It can be either in the base tree (from the base 
	 * value of xlink:href) or the animated tree (from the 
	 * animated value of xlink:href).
	 */
	static BOOL IsShadowNode(HTML_Element* elm);

	/**
	 * Marks shadowtree as being built.
	 */
	static void MarkShadowTreeAsBuilt(HTML_Element* elm, BOOL is_built, BOOL animated);

	static OP_STATUS CloneToShadow(SVGDocumentContext* doc_ctx, HTML_Element* tree_to_clone, HTML_Element* parent_to_be, BOOL is_root_node, BOOL animated_tree);
	static OP_STATUS BuildShadowTreeForUseTag(SVGElementResolver* resolver, HTML_Element* traversed_elm, HTML_Element* layouted_elm, SVGDocumentContext* doc_ctx);
	static OP_STATUS LookupAndVerifyUseTarget(SVGElementResolver* resolver, SVGDocumentContext *use_elm_doc_ctx, HTML_Element* use_elm,
											  HTML_Element* traversed_elm, BOOL animated, HTML_Element*& target);
	static OP_STATUS CreateShadowRoot(SVGElementResolver* resolver, SVGDocumentContext* doc_ctx, HTML_Element* use_elm, HTML_Element* traversed_elm, BOOL animated);

	/**
	 * Get the opacity of this element.
	 * @param element the element.
	 * @param props cascaded CSS properties.
	 * @param is_topmost_svg_root TRUE if this is an <svg> element that is the root of the current context.
	 * @return the opacity level as an integer value between 0 and 255 where 255 is fully opaque.
	 */
	static UINT8 GetOpacity(HTML_Element* element, const HTMLayoutProperties& props, BOOL is_topmost_svg_root);

	/**
	 * Helper method to get the current active {letter,word} spacing value.
	 */
	static SVGNumber GetSpacing(HTML_Element* element, Markup::AttrType attr, const HTMLayoutProperties& props);

	static SVGLength::LengthOrientation GetLengthOrientation(Markup::AttrType attr_name, NS_Type ns);

	static UINT32 GetNextSerial() { return g_opera->svg_module.attr_serial++; }

	static BOOL IsEditable(HTML_Element* elm);

	/** 
	 * Gets the text root element of an element. Text root elements are decided by the IsTextRootElement method. 
	 * @return The textroot element or NULL if not found
	 */
	static HTML_Element* GetTextRootElement(HTML_Element* elm);

	static void LimitCanvasSize(SVG_DOCUMENT_CLASS* doc, VisualDevice* vis_dev, int& w, int& h);

	/**
	* Returns TRUE if the layouted element has display=none
	*/
	static BOOL IsDisplayNone(HTML_Element* layouted_elm, LayoutProperties* layout_props);

	/**
	 * Returns TRUE if the element never should be considered for visual presentation
	 */
	static BOOL IsAlwaysIrrelevantForDisplay(Markup::Type type);

	/**
	 * Returns TRUE if all required external resources have been loaded.
	 */
	static BOOL HasLoadedRequiredExternalResources(HTML_Element* container_elm);

	/**
	 * Returns TRUE if we're currently loading external svg font resources in the given element.
	 */
	static BOOL IsLoadingExternalFontResources(HTML_Element* font_elm);

	/**
	 * Returns TRUE if the element can contain svg font child elements.
	 */
	static BOOL IsFontElementContainer(Markup::Type type)
	{
		return (type == Markup::SVGE_FONT_FACE || type == Markup::SVGE_FONT || type == Markup::SVGE_FONT_FACE_SRC);
	}

	static OP_STATUS BuildSVGFontInfo(SVGDocumentContext* doc_ctx, HTML_Element* elm);

	static BOOL IsURLEqual(URL &url1, URL &url2);

	/**
	 * Resolve a base url for an element. Starts with the root url and
	 * applies xml:base on all elements from the root element and all
	 * the way down to the element 'elm'.
	 */
	static URL ResolveBaseURL(const URL& root_url, HTML_Element* elm);
	
#if defined(SVG_SUPPORT_EDITABLE) || defined(SVG_TREAT_WHITESPACE_AS_EMPTY_XLINK_HREF)
	static BOOL IsAllWhitespace(const uni_char* s, unsigned int s_len);
#endif // SVG_SUPPORT_EDITABLE || SVG_TREAT_WHITESPACE_AS_EMPTY_XLINK_HREF
};

#endif // SVG_SUPPORT
#endif // SVG_UTILS_H
