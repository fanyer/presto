/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_ELEMENT_STATE_CONTEXT
#define SVG_ELEMENT_STATE_CONTEXT

#ifdef SVG_SUPPORT

#include "modules/svg/SVGManager.h"
#include "modules/svg/SVGContext.h"

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGValue.h"
#include "modules/svg/src/SVGRect.h"
#include "modules/svg/src/SVGUtils.h"

class SVGTraversalObject;
class SVGAnimationTarget;
class SVGFontElement;
class SVGXMLFontData;
class SVGTimeObject;
class SVGTextCache;
struct SVGElementInfo;
class SVGPaintNode;

/**
 * State for SVG elements.
 *
 * Describes if and how an SVG element needs to be updated. The state
 * is bound to the tree, and the state of one element alone does not
 * necessarily reflect the overall state of the element. To find the
 * actual state for an element, its ancestors - and their state(s) -
 * have to be considered. For instance if an element is NOT_INVALID,
 * it can still have an ancestor with INVALID_ADDED. This would
 * indicate that the element is part of subtree that has been added
 * since the last layout pass. Ancestor state is propagated during
 * traversals, but may also need to be propagated manually - for
 * instance by using methods like SVGUtils::PropagateInvalidState.
 *
 * The states are supposed to be 'inclusive', so a state with a higher
 * enumeration value _should_ be considered to include all of the
 * previous states.
 */
enum SVGElementInvalidState
{
	NOT_INVALID = 0,

	INVALID_SUBTREE,
	/**< Some element in the subtree is invalid. */

	INVALID_STRUCTURE,
	/**< The structure of the elements children has changed (via
	 * addition, removal or changes to the conditional predicates). */

	INVALID_FONT_METRICS,
	/**< The font metrics (or anything affecting font metrics) of the
	 * subtree has changed. */

	INVALID_ADDED
	/**< The subtree has been added. Currently also used to signal
	 * attribute changes. */

	/* NOTE: When extending this enumeration, make sure to sync with
	 * the SVGElementContext::packed.m_invalidation_state field - and
	 * make sure to leave headroom for a sign bit. */
};

/**
 * This class is used to store information about a modified element at
 * the element that for example is being animated. Several animation
 * elements then can share this information.
 *
 * This class could use a (better) mapping data structure to let go of
 * the two lists that are implementing the mapping now. This gives
 * linear complexity and could lead to performance problems when many
 * many attributes of the same object is animated.
 *
 */
class SVGElementContext : public SVGContext, public ListElement<SVGElementContext>
{
private:

	/**
	 * This is the boundingbox for this element, in userspace coordinates.
	 *
	 * NOTE: Option to consider - Store this elsewhere (when needed at all).
	 */
	SVGBoundingBox	m_bbox;  // 16-32 bytes

	/**
	 * This is a masqueraded pointer to the HTML_Element this context
	 * belongs to. The LSB is used to indicate if the element is an
	 * instance of a shadow-tree.
	 */
	UINTPTR			m_element_data;

	/**
	 * If the element this context represents is/has been animated,
	 * this will denote the structure containing animation state.
	 */
	SVGAnimationTarget *m_animation_target;

	/**
	 * The paint node for this element.
	 */
	SVGPaintNode*	m_paint_node;

protected:
	// Sub classes borrow this area for some extra bits
	union {
		struct {
			unsigned int	bbox_is_valid : 1;
			unsigned int	need_css_reload : 1;
			unsigned int	is_filtered_by_requirements : 1;
			unsigned int	has_calculated_requirements : 1;

			// NOTE: When enlarging this field, you need to consider
			// that on certain platforms, enumeration type can be
			// signed. Hence, one bit should be left as headroom.
			SVGElementInvalidState m_invalidation_state : 4;

			// For buffered-rendering:
			// Should this context bother to try and buffer
			// something. Can have three different values:
			//
			//   0 - Undecided, will re-evaluate during next traverse
			//   1 - The context should buffer
			//       (usually [always?] implies buffered-rendering='static')
			//   3 - The context should not buffer ('buffering disabled')
			//
			// (in practice this means that bit 0 means 'undecided',
			// and bit 1 is 'disabled')
			unsigned int	should_buffer : 2;

			unsigned int	has_presentation_attributes : 1;
			unsigned int	has_trustworthy_pres_attrs : 1;

			/** 12 bits used **/
		} packed;
		UINT16 packed_init;
	};

	SVGElementContext(HTML_Element* element);

	static void UnpackAttributeInfo(INT32 packed, Markup::AttrType &attribute_name, int &ns_idx,
									SVGAttributeType &attribute_type);
	static INT32 PackAttributeInfo(Markup::AttrType attribute_name, int ns_idx, SVGAttributeType attribute_type);

	static BOOL NeedsResources(HTML_Element* element);
	static BOOL FailingRequirements(HTML_Element* element);

public:
	/**
	 * Creates the element and stores it in the element.
	 */
	static SVGElementContext* Create(HTML_Element* svg_elm);

	virtual ~SVGElementContext();

	// Traversal interface
	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info) { return OpStatus::OK; }

	virtual SVGElementContext* FirstChild() { return NULL; }
	virtual void RebuildChildList(SVGElementInfo& info) {}

	virtual BOOL EvaluateChild(HTML_Element* child) { return FALSE; }

	HTML_Element* GetHtmlElement() { return reinterpret_cast<HTML_Element*>(m_element_data & ~(UINTPTR)0x1); }
	BOOL IsShadowNode() const { return (m_element_data & 0x1) != 0; }
	HTML_Element* GetLayoutedElement() { return SVGUtils::GetElementToLayout(GetHtmlElement()); }

	SVGAnimationTarget *GetAnimationTarget() { return m_animation_target; }
	SVGAnimationTarget *AssertAnimationTarget(HTML_Element *target_element);

	void ClearInvalidState() { packed.m_invalidation_state = NOT_INVALID; }
	virtual void AddInvalidState(SVGElementInvalidState state);
	void UpgradeInvalidState(SVGElementInvalidState state) { packed.m_invalidation_state = MAX(packed.m_invalidation_state, state); }
	SVGElementInvalidState GetInvalidState() const { return packed.m_invalidation_state; }
	/**
	 * Computes the actual invalid state for the the given element.
	 * This thus includes the state of the parent(s), and is the
	 * "real" invalid state for this context.
	 */
	static SVGElementInvalidState ComputeInvalidState(HTML_Element* elm);

	void ClearNeedCSSReloadFlag() { packed.need_css_reload = FALSE; }
	BOOL IsCSSReloadNeeded() { return packed.need_css_reload; }
	BOOL HasSubTreeChanged() { return packed.m_invalidation_state >= INVALID_SUBTREE; }
	BOOL IsNewPosInvalid() { return packed.m_invalidation_state > INVALID_SUBTREE; }
//	BOOL IsOldPosInvalid() { return packed.m_invalidation_state >= INVALID_MOVED; }

	BOOL3 HasPresentationAttributes() { return (packed.has_trustworthy_pres_attrs == 1 ? (packed.has_presentation_attributes ? YES : NO) : MAYBE); }
	void SetHasTrustworthyPresAttrs(BOOL val) { packed.has_trustworthy_pres_attrs = val; }
	void SetHasPresentationAttributes(BOOL val) { packed.has_presentation_attributes = val; }

	/**
	 * Evaluates the 'requiredFeatures', 'requiredExtensions' and 'systemLanguage' attributes
	 * on the element, and return whether it meets the requirements and thus should be processed further.
	 *
	 * @return FALSE if element meets requirements, TRUE if not
	 */
	BOOL IsFilteredByRequirements()
	{
		if (!packed.has_calculated_requirements)
		{
			packed.is_filtered_by_requirements = !SVGUtils::ShouldProcessElement(GetLayoutedElement());
			packed.has_calculated_requirements = TRUE;
		}
		return packed.is_filtered_by_requirements;
	}

	/**
	 * Call this if any of the attributes controlling the requirements are changed on the element.
	 */
	void InvalidateRequirementsCalculation() { packed.has_calculated_requirements = FALSE; }

	/**
	 * Set before the element can safely be discarded.
	 */
	void SetDiscarded() { packed.has_calculated_requirements = 1; packed.is_filtered_by_requirements = 1; }

	/**
	 * Boundingbox cache methods
	 */
	BOOL IsBBoxValid() const { return packed.bbox_is_valid; }
	void SetBBoxIsValid(BOOL valid)
	{
		packed.bbox_is_valid = valid;
		if (!valid)
			m_bbox.Clear();
	}

	SVGPaintNode* GetPaintNode() { return m_paint_node; }
	void SetPaintNode(SVGPaintNode* paint_node) { m_paint_node = paint_node; }
	BOOL HasAttachedPaintNode() const;

	OpRect GetScreenExtents() const;

	const SVGBoundingBox& GetBBox() const { return m_bbox; }
	void SetBBox(const SVGBoundingBox& bbox) { m_bbox = bbox; }
	void AddBBox(const SVGBoundingBox& childbox) { m_bbox.UnionWith(childbox); }

	BOOL ShouldBuffer(const SvgProperties* svg_props);
	void DisableBuffering() { packed.should_buffer = 3; }
	void RevalidateBuffering() { packed.should_buffer = 0; }

	void DetachBuffer();

	/**
	 * This is our own simple event handlers for when scripting is
	 * disabled.
	 */
    BOOL ListensToEvent(DOM_EventType type);
    BOOL HandleEvent(HTML_Element* elm, const SVGManager::EventData& data);

	OP_STATUS RegisterListener(HTML_Element *element, SVGTimeObject* etv);
	OP_STATUS UnregisterListener(SVGTimeObject* etv);

	/**
	 * Virtual casts
	 */
	virtual class SVGDocumentContext* GetAsDocumentContext() { return NULL; }
	virtual class SVGFontElement* GetAsFontElement() { return NULL; }
	virtual class SVGTextRootContainer* GetAsTextRootContainer() { return NULL; }

	/**
	 * Virtual casts for timing / animation
	 */
	virtual class SVGTimingInterface* GetTimingInterface() { return NULL; }
	virtual class SVGAnimationInterface* GetAnimationInterface() { return NULL; }

	virtual SVGTextCache* GetTextCache() { return NULL; }
};

//
// The behavioral hierarchy (leaving the abstract root SVGContext out):
//
//    SVGElementContext
//       SVGInvisibleElement
//          SVGFEPrimitive						<fe*>
//          SVGInvisibleTimedElement			<discard>, <audio>
//          SVGAnimationElement					<animate>, <set>, et.c
//       SVGGraphicsElement						Basic shapes
//          SVGExternalContentElement			<image>, <foreignObject>
//             SVGTimedExternalContentElement	<video>, <animation>
//       SVGContainer							<g>
//			SVGResourceContainer				<mask>, <marker>, <pattern>
//             SVGClipPathElement				<clipPath>
//             SVGFilterElement					<filter>
//          SVGSVGElement						<svg>
//			SVGUseElement						<use>
//          SVGSymbolInstanceElement			<symbol> (when <use>:d)
//          SVGTextContainer					<tspan>, <textPath>, <tref>
//             SVGAltGlyphElement				<altGlyph>
//             SVGTextElement					<text>
//             SVGTextAreaElement				<textArea>
//             SVGTransparentContainer
//                SVGAElement					<a>
//                SVGSwitchElement				<switch>
//                SVGTextGroupElement			text group nodes (collection of text nodes for very long texts)
//       SVGTBreakElement						<tbreak>
//       SVGTextNode							text nodes
//       SVGFontElement							<font>, <font-face>
//

/** Base class for elements lacking a graphical representation */
class SVGInvisibleElement : public SVGElementContext
{
public:
	SVGInvisibleElement(HTML_Element* element) : SVGElementContext(element) {}

	virtual BOOL EvaluateChild(HTML_Element* child);
};

#ifdef SVG_SUPPORT_FILTERS
class SVGFEPrimitive : public SVGInvisibleElement
{
public:
	SVGFEPrimitive(HTML_Element* element) : SVGInvisibleElement(element) {}

	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info);

	virtual BOOL EvaluateChild(HTML_Element* child) { return FALSE; }
};
#endif // SVG_SUPPORT_FILTERS

/** Base class for elements with a graphical representation */
class SVGGraphicsElement : public SVGElementContext
{
public:
	SVGGraphicsElement(HTML_Element* element) : SVGElementContext(element) {}

	// Traversal interface
	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);
};

/** Base for <video>, <audio>, <animation>. Concrete for <image> and <foreignObject> */
class SVGExternalContentElement : public SVGGraphicsElement
{
public:
	SVGExternalContentElement(HTML_Element* element) : SVGGraphicsElement(element) {}

	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info);
};

/** Base class for elements with children */
class SVGContainer : public SVGElementContext
{
public:
	SVGContainer(HTML_Element* element) : SVGElementContext(element) {}

	virtual ~SVGContainer() { m_children.RemoveAll(); }

	virtual void AddInvalidState(SVGElementInvalidState state);

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);

	virtual SVGElementContext* FirstChild() { return m_children.First(); }
	virtual void RebuildChildList(SVGElementInfo& info) { SelectChildren(info); }

protected:
	virtual BOOL EvaluateChild(HTML_Element* child);
	virtual void AppendChild(HTML_Element* child, List<SVGElementContext>* childlist);

	void SelectChildren(SVGElementInfo& info);

	List<SVGElementContext> m_children;	///< Ordered list of children
};

class SVGSVGElement : public SVGContainer
{
public:
	SVGSVGElement(HTML_Element* element) : SVGContainer(element) {}

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);
};

class SVGSymbolInstanceElement : public SVGContainer
{
public:
	SVGSymbolInstanceElement(HTML_Element* element) : SVGContainer(element) {}

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);
};

/** Action-less container */
class SVGResourceContainer : public SVGContainer
{
public:
	SVGResourceContainer(HTML_Element* element) : SVGContainer(element) {}

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS HandleContent(SVGTraversalObject* traversal_object, SVGElementInfo& info) { return OpStatus::OK; }
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info) { return OpStatus::OK; }

protected:
	virtual BOOL EvaluateChild(HTML_Element* child);
};

#ifdef SVG_SUPPORT_STENCIL
class SVGClipPathElement : public SVGResourceContainer
{
public:
	SVGClipPathElement(HTML_Element* element) : SVGResourceContainer(element) {}

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);

	virtual BOOL EvaluateChild(HTML_Element* child);
};

class SVGMaskElement : public SVGResourceContainer
{
public:
	SVGMaskElement(HTML_Element* element) : SVGResourceContainer(element) {}

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);
};
#endif // SVG_SUPPORT_STENCIL

#ifdef SVG_SUPPORT_FILTERS
class SVGFilterElement : public SVGResourceContainer
{
public:
	SVGFilterElement(HTML_Element* element) : SVGResourceContainer(element) {}

	virtual BOOL EvaluateChild(HTML_Element* child);
};
#endif // SVG_SUPPORT_FILTERS

class SVGUseElement : public SVGContainer
{
public:
	SVGUseElement(HTML_Element* element) : SVGContainer(element) {}

	virtual OP_STATUS Enter(SVGTraversalObject* traversal_object, SVGElementInfo& info);
	virtual OP_STATUS Leave(SVGTraversalObject* traversal_object, SVGElementInfo& info);
};

class SVGFontImpl;

/**
 * This class owns the font rooted in the context-owning element
 */
class SVGFontElement : public SVGInvisibleElement
{
public:
	SVGFontElement(HTML_Element* element)
		: SVGInvisibleElement(element), m_fontdata(NULL) {}
	virtual ~SVGFontElement();

	SVGFontImpl* CreateFontInstance(UINT32 size);

	OP_STATUS MarkFontDataDirty();
	void NotifyFontDataChange();

	virtual SVGFontElement* GetAsFontElement() { return this; }

	virtual BOOL EvaluateChild(HTML_Element* child) { return FALSE; }

private:
	OP_STATUS CreateFontData();

	Head m_font_instances;
	SVGXMLFontData* m_fontdata;
};

#endif // SVG_SUPPORT
#endif // SVG_ELEMENT_STATE_CONTEXT
