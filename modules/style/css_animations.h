/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_ANIMATIONS_H
#define CSS_ANIMATIONS_H

#ifdef CSS_ANIMATIONS

#include "modules/logdoc/elementref.h"
#include "modules/util/OpHashTable.h"

class CSSCollection;
class HTML_Element;
class CSS_Properties;
class CSS_Animation;
class CSS_ElementAnimations;
class CSS_AnimationManager;
class LayoutProperties;
struct TransitionDescription;

/** Represents the list of animations applied to one single element.
	Owned by the animation manager. */

class CSS_ElementAnimations : public ElementRef
{
public:
	CSS_ElementAnimations(FramesDocument* doc, HTML_Element* element) : ElementRef(element), m_doc(doc) {}
	~CSS_ElementAnimations() { m_animations.Clear(); }

	/** Search this list of animations for an untraced animation
		matching 'name'. If such an animation exists, that animation
		is traced and returned. Used to "garbage-collect" animations
		that should no longer apply. */

	CSS_Animation* TraceAnimation(const uni_char* name);

	/** Returns TRUE if one or more animations were removed */

	BOOL Sweep();

	/** Append an animation to the list for animation for this
		element. */

	void Append(CSS_Animation* animation);

	/** Apply current state of animations for this element to cssprops
		and the animation manager. */

	void Apply(CSS_AnimationManager* animation_manager, double runtime_ms, CSS_Properties* cssprops, BOOL apply_partially);

	/** An animation-delay has timed out for this element. Reload
		style. */

	void OnAnimationDelayTimeout();

	/** Element removed from document, stop animations. */

	virtual void OnRemove(FramesDocument* document);

	/** Element deleted, stop animations and delete this. */

	virtual void OnDelete(FramesDocument* document);

private:
	List<CSS_Animation> m_animations;

	/** The document of the animated element to which events are sent
		to. */

	FramesDocument* m_doc;
};

/** The animation manager is a prolonged part of a CSSCollection,
	specifically responsible for the current set of animations within
	that CSSCollection.

	It owns one hash table that maps elements to animations, one set
	of animations for each element.

	It also owns a list of transition descriptions, populated by
	animations when they request interpolation between two CSS
	values. */

class CSS_AnimationManager
{
public:
	CSS_AnimationManager(CSSCollection* css_collection) : m_css_collection(css_collection) {}
	~CSS_AnimationManager() { m_animations.DeleteAll(); }

	/** Retrieve the current document. */

	FramesDocument* GetFramesDocument() const;

	/** Apply the current state of all animations. Called from the css
		collection when reloading CSS properties. */

	void ApplyAnimations(HTML_Element* elm, double runtime_ms, CSS_Properties* cssprops, BOOL apply_partially);

	/** Start and update animations according to what is defined by
		the animation properties in 'cascade'. Called during property
		reload, but after we have calculated computed style. */

	OP_STATUS StartAnimations(LayoutProperties* cascade, double runtime_ms);

	/** Register an property as currently animating. Called from the
		individual animations. */

	void RegisterAnimatedProperty(TransitionDescription* description);

	/** Extract the current set of transitions, as created by the
		current set of animations. This list may be used as input when
		updating transitions. */

	Head& CurrentTransitionList() { return m_current_transitions; }

	/** Remove animations for a specific element. Used when cleaning
		up after an element that has been removed from the tree. */

	void RemoveAnimations(HTML_Element* element);

private:

	CSS_ElementAnimations* GetAnimations(HTML_Element* element);

	OP_STATUS SetupAnimation(HTML_Element* element, CSS_ElementAnimations*& animations);

	OpPointerHashTable<const HTML_Element, CSS_ElementAnimations> m_animations;

	List<TransitionDescription> m_current_transitions;

	CSSCollection* m_css_collection;
};

#endif // CSS_ANIMATIONS
#endif // CSS_ANIMATIONS_H
