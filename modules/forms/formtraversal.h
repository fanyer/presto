/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef FORMS_FORMTRAVERSAL_H_
#define FORMS_FORMTRAVERSAL_H_

/** Interfaces for traversing a form in tree order and synthesize a form data set
    from it. */
class FramesDocument;
class HTML_Element;
class TempBuffer8;


/** When traversing a form, the context object that's propagated along. */
class FormTraversalContext
{
public:
	FormTraversalContext(FramesDocument *frames_doc, HTML_Element *submit_elm, HTML_Element *search_field_elm = NULL)
		: frames_doc(frames_doc)
		, submit_elm(submit_elm)
		, search_field_elm(search_field_elm)
		, force_name(NULL)
		, force_value(NULL)
		, output_buffer(NULL)
		, xpos(0)
		, ypos(0)
	{
	}

	/**
	 * The frames document the form belongs to.
	 */
	FramesDocument *frames_doc;
	/**
	 * The HTML Element used to submit. A submit button or image.
	 */
	HTML_Element *submit_elm;
	/**
	 * Optional search field element, whose value will be replaced by
	 * "%s" in the submit data.
	 */
	HTML_Element *search_field_elm;
	/**
	 * Optional overriding name to use when processing the next
	 * element.
	 */
	const uni_char *force_name;
	/**
	 * Optional overriding value to use when processing the next
	 * element.
	 */
	const uni_char *force_value;
	/**
	 * Optional output buffer to use when traversing; some traversal
	 * object won't consider this.
	 */
	TempBuffer8 *output_buffer;
	/**
	 * The position of the click (x coordinate) that caused the submit.
	 * This is only used when the submit was through an image.
	 */
	int xpos;
	/**
	 * The position of the click (y coordinate) that caused the submit.
	 * This is only used when the submit was through an image.
	 */
	int ypos;
};

/** The abstract interface a form traverser needs to supply to
 *  walk over a form. The generic traversal method will call upon
 *  methods of this interface to process name and value/filenames
 *  of the elements it encounters.
 */
class FormTraverser
{
public:
	/** Convert the given element name or value into its encoded
	 *  representation. Details are traversal specific, but most
	 *  likely UTF-8 encoded.
	 *
	 * @param [out] name The encoded result.
	 * @param raw_content The form element's name or value.
	 *
	 * The implementation may leave if the conversion isn't possible
	 * or an out of memory condition is encountered. The overall
	 * traversal will not intercept any exceptions.
	 *
	 */
	virtual void ToFieldValueL(OpString8 &name, const uni_char *raw_content) = 0;

	/**
	 * Add a name value pair to the traversal set, recording the named element
	 * along with its submittable value.
	 *
	 * @param context The traversal context.
	 * @param he The form element.
	 * @param name The name to use.
	 * @param value The value to use.
	 * @param verbatim If TRUE, the value should be added without escaping.
	 *
	 * The implementation may leave if the conversion isn't possible
	 * or an out of memory condition is encountered. The overall
	 * traversal will not intercept any exceptions.
	 *
	 */
	virtual void AppendNameValuePairL(FormTraversalContext &context, HTML_Element *he, const char *name, const char *value, BOOL verbatim = FALSE) = 0;

	/**
	 * Adds a named file to the traversal set.
	 *
	 * @param context The traversal context.
	 * @param he The form element.
	 * @param name The name of the form element.
	 * @param filename The file name.
	 * @param encoded_value A charset-converted file name.
	 *
	 * The implementation may leave if the conversion isn't possible
	 * or an out of memory condition is encountered. The overall
	 * traversal will not intercept any exceptions.
	 *
	 */
	virtual void AppendUploadFilenameL(FormTraversalContext &context, HTML_Element *he, const char *name, const uni_char *filename, const char *encoded_filename) = 0;
};

/** A general interface for traversing Forms, parameterised over the
 *  the object that handles the addition of the individual elements
 *  to the form data set, or whatever operations it needs to perform
 *  for each form element.
 */
class FormTraversal
{
public:
	/**
	 * "Traverse" a single element.
	 *
	 * @param context The traversal context.
	 * @param traverser The traversal object.
	 * @param he The HTML_Element for the value.
	 *
	 * Leaves on OOM.
	 */
	static void	TraverseElementL(FormTraversalContext &context, FormTraverser *traverser, HTML_Element* he);

	/**
	 * "Traverse" the given form, invoking the traversal object
	 * for each submittable and eligible element.
	 *
	 * @param context The traversal context.
	 * @param traverser The traversal object.
	 * @param he The HTML_Element for the value.
	 * @param [out]have_password TRUE if the form contained
	 *        a password element.
	 *
	 * Leaves on OOM.
	 */
	static void TraverseL(FormTraversalContext &context, FormTraverser *traverser, HTML_Element *he, BOOL *have_password = NULL);
};

#endif // FORMS_FORMTRAVERSAL_H_
