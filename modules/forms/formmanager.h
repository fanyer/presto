/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
x *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 */

#ifndef FORMMANAGER_H
#define FORMMANAGER_H

#include "modules/url/url2.h"
#include "modules/logdoc/logdocenum.h"
#include "modules/util/adt/opvector.h"
#include "modules/hardcore/keys/opkeys.h"

class FramesDocument;
class HTML_Document;
class HTML_Element;
class ES_Thread;
class UniParameterList;
class LogicalDocument;

class FormManager
{
private:
	FormManager(); // No implementation to prevent accidential creation.

public:
	/**
	 * Restores a form to its original state and sends appropriate
	 * DOM events.
	 *
	 * @param frames_doc The document with the form in it.
	 * @param form_element The form element of the form.
	 *
	 * @return The status of the operation. OpStatus::OK if it
	 * completed successfully, OpStatus::ERR_NO_MEMORY if we ran out
	 * of memory while doing the operation.
	 */
	static OP_STATUS ResetForm(FramesDocument* frames_doc,
							   HTML_Element* form_element);
	/**
	 * Initiates a submit process.
	 *
	 * @param frames_doc The document with the form in it.
	 * @param form_element The form element of the form.
	 * @param submit_element The element that initiated the
	 * submit (normally a submit button or a submit image)
	 * or NULL if not submitted from a submit element.
	 * @param offset_x The x coordinate of the click on the
	 * submit image or irrelevant if the submit_elmement
	 * wasn't an image.
	 * @param offset_y The y coordinate of the click on the
	 * submit image or irrelevant if the submit_elmement
	 * wasn't an image.
	 * @param thread The ecmascript thread initiating thise submit.
	 * @param synthetic TRUE if there were scripts involved.
	 * @param modifiers What modifier keys were pressed when the submit
	 * was triggered.
	 *
	 * @return The status of the operation. OpStatus::OK if it
	 * completed successfully, OpStatus::ERR_NO_MEMORY if we ran out
	 * of memory while doing the operation.
	 */
	static OP_STATUS SubmitForm(FramesDocument* frames_doc,
								HTML_Element* form_element,
								HTML_Element* submit_element,
							    int offset_x, int offset_y,
								ES_Thread* thread,
								BOOL synthetic,
							    ShiftKeyState modifiers);

	/**
	 * Submits with the default action when started from
	 * a text field. Will trigger wand if wand is enabled.
	 *
	 * @param frames_doc The document with the form in it.
	 * @param input_field The input field. Right now only
	 * text fields and password fields are supported.
	 * @param modifiers What modifier keys were pressed when the submit
	 * was triggered.
	 *
	 * @return The status of the operation. OpStatus::OK if it
	 * completed successfully, OpStatus::ERR_NO_MEMORY if we ran out
	 * of memory while doing the operation.
	 */
	static OP_STATUS SubmitFormFromInputField(FramesDocument* frames_doc,
											  HTML_Element* input_field,
											  ShiftKeyState modifiers);

	/**
	 * Takes appropriate actions after a click
	 * on a submit button (or image) in a form.
	 */
	static OP_STATUS HandleSubmitButtonClick(FramesDocument* frames_doc,
	                                         HTML_Element* form_element,
	                                         HTML_Element* submit_element,
	                                         int offset_x, long offset_y,
	                                         int document_x, long document_y,
	                                         int button_or_key_or_delta,
	                                         ShiftKeyState modifiers,
	                                         ES_Thread *origining_thread);

	/**
	 * Called when someone does .submit on a form element.
	 */
	static OP_STATUS DOMSubmit(FramesDocument* frames_doc,
	                           HTML_Element* form, ES_Thread* origining_thread
#ifdef WAND_SUPPORT
	                           , BOOL is_from_submit_event
#endif// WAND_SUPPORT
	                           );

#ifdef _WML_SUPPORT_
	/**
	 * Determine if an HE_OPTION is default selected by wml value/ivalue.
	 */
	static BOOL		IsWmlSelected(HTML_Element* elm, INT32 index);

	/**
	 * Validate the content in HE_INPUT according to format/emptyok attributes.
	 */
	static BOOL		ValidateWmlInputData(HTML_Element* elm,
										 const uni_char* data,
										 BOOL is_typing = FALSE);

	/**
	 * Validate all the inputs in the document.
	 * @param frames_doc The document with the elements in.
	 */
	static BOOL		ValidateWmlInputData(FramesDocument *doc,
										 HTML_Element* elm,
										 BOOL is_typing = FALSE);

	/**
	 * @param frames_doc The document with the form in it.
	 *
	 * @param elm The element to get the default value for.
	 */
	static const uni_char* GetWmlDefaultValue(FramesDocument *doc,
											  HTML_Element* elm);

	/**
	 * Checks if a wml document's form data is valid.
	 *
	 * @param frames_doc The document.
	 *
	 * @returns FALSE if the form wasn't valid. TRUE if it was valid.
	 */
	static BOOL ValidateWMLForm(FramesDocument* frames_doc);

#endif // _WML_SUPPORT_

#ifdef _PLUGIN_SUPPORT_
	/**
	 * Help method that fetches the value from the plugin into the
	 * string or return OpStatus:ERR if there was no plugin, if the
	 * plugin wasn't a form plugin or if the plugin had no value.
	 */
	static OP_STATUS GetPluginFormValue(HTML_Element* elm, OpString& output);
#endif // _PLUGIN_SUPPORT

	/**
	 * Consider using FormIterator instead since that is much more
	 * efficient.
	 *
	 * @param frames_doc The document with the elements in.
	 * @param form_element The form (HE_FORM) that we are iterating.
	 * @param current_form_elm The form control whose successor
	 * should be returned.
	 * @param include_option Normally HE_OPTION:s aren't
	 * included, but if this parameter is used with the
	 * value TRUE, they are.
	 * @param include_images Normally images aren't
	 * included, but if this parameter is used with the
	 * value TRUE, they are.
	 *
	 * @return The next form control in the form, or NULL if
	 * current_form_elm is the last element.
	 *
	 */
	static HTML_Element* NextFormElm(FramesDocument* frames_doc,
									 HTML_Element* form_element,
									 HTML_Element* current_form_elm,
									 BOOL include_option = FALSE,
									 BOOL include_images = FALSE);

	/**
	 * Returns the form element for a form control.
	 *
	 * @param frames_doc The document with the elements in.
	 * @param form_control The element whose form should sought after.
	 *
	 * @return NULL if the form_control wasn't in a form. The
	 * form element otherwise.
	 */
	static HTML_Element* FindFormElm(FramesDocument* frames_doc,
									 HTML_Element* form_control);

	/**
	 * Checks if |form_control| belongs to the form |form|.
	 *
	 * @param frames_doc The document with the elements in.
	 * @param form The form element. Must not be NULL.
	 * @param form_control The form_control to check.
	 *
	 * @returns TRUE if the form_control is a part of form's form.
	 */
	static BOOL BelongsToForm(FramesDocument* frames_doc,
							  HTML_Element* form,
							  HTML_Element* form_control);

	/**
	 * Checks if there exists a selected radio button in the same
	 * group as the supplied radion button, excluding the radio button
	 * itself.
	 *
	 * @param html_doc The document with the radio button. Must not be
	 * NULL.
	 * @param radio_button The radio button. Must not be NULL.
	 *
	 * @return TRUE if there is a selected radio button in the group.
	 */
	static BOOL IsSelectedRadioInGroup(HTML_Document *html_doc,
									   HTML_Element* radio_button);

	/**
	 * If the element is a "true" form element, so that
	 * it for instance has a FormValue. If this returns TRUE
	 * it's ok to call GetFormValue().
	 *
	 * @param elm The element to check. Must not be NULL.
	 *
	 * @return TRUE if the element is classified as a Form Element.
	 */
	static BOOL IsFormElement(HTML_Element* elm);

	/**
	 * Returns the relevant form id string for the element. This string
	 * might be inherited from a fieldset.
	 */
	static const uni_char* GetFormIdString(HTML_Element* form_control);

	/**
	 * Given a string "foo bar fum", looks up the elements with that
	 * id and returns the first match.
	 *
	 * @param frames_doc The document.
	 *
	 * @param form_control The form (HE_FORM).
	 *
	 * @param form_id The id string.
	 *
	 * @returns The element found or NULL.
	 */
	static
	HTML_Element* FindElementFromIdString(FramesDocument* frames_doc,
											   HTML_Element* form_control,
											   const uni_char* form_id);

	/**
	 * Determines if a form element is disabled by taking disabled
	 * fieldset parents into consideration. Normally this shouldn't be
	 * used. Use HTML_Element::IsDisabled instead.
	 *
	 * @param form_control_element The form element that might be disabled.
	 *
	 * @returns TRUE if the element is disabled. FALSE if it's not.
	 */
	static BOOL IsInheritedDisabled(const HTML_Element* form_control_element);

	/**
	 * Checks if a certain option is selected.
	 *
	 * @param option The option to check,
	 *
	 * @returns TRUE if it was selected, false otherwise,
	 */
	static BOOL IsSelectedOption(HTML_Element* option);

	/**
	 * Determines if the control is successful (submittable). For instance
	 * form controls in templates aren't submitted.
	 *
	 * @param frames_doc The document.
	 *
	 * @param form_control_element The element to check.
	 *
	 * @param submit_element The element used to submit or NULL if not
	 * a submit. This is needed since the submit element _is_ included while
	 * buttons normally isn't.
	 *
	 * @param form_element_check In order for a form control element to be
	 *                           included in a submit it must be associated
	 *                           with a form element. This check is expensive
	 *                           and can be skipped by setting this parameter
	 *                           to FALSE. This should only be used when the
	 *                           caller already knows that the form control
	 *                           element is associated with a form.
	 */
	static BOOL IsIncludedInSubmit(FramesDocument* frames_doc,
								   HTML_Element* form_control_element,
								   HTML_Element* submit_element,
								   BOOL form_element_check = TRUE);

	/**
	 * Configures and initializes the UniParameterList for
	 * file name iteration.
	 *
	 * @param file_list The list to configure and initialize.
	 * @param file_value_str The string with filenames.
	 */
	static OP_STATUS ConfigureForFileSplit(UniParameterList& file_list,
										   const uni_char* file_value_str);


	/**
	 * Returns TRUE is elm is a HE_BUTTON or a HE_INPUT
	 * of types INPUT_BUTTON, INPUT_SUBMIT, INPUT_RESET.
	 * Otherwise it returns FALSE. elm must not be NULL.
	 *
	 * @param elm The element.
	 */
	static BOOL IsButton(HTML_Element* elm);

	/**
	 * Returns TRUE is input_type is
	 * of types INPUT_BUTTON, INPUT_SUBMIT, INPUT_RESET.
	 * Otherwise it returns FALSE.
	 *
	 * @param input_type the InputType.
	 */
	static BOOL IsButton(InputType input_type);

	/**
	 * To be called when a bunch of radio buttons are added or removed
	 * from a tree so that the :indeterminate can be updated.
	 *
	 * @param doc The document that containes the radio buttons.
	 * @param radio_buttons The radio buttons that have changed.
	 *
	 * @param added_removed_or_name_change 1=added, 0=removed, -1=name_change
	 */
	static OP_STATUS HandleChangedRadioGroups(FramesDocument* doc,
											  const OpVector<HTML_Element>& radio_buttons,
											  int added_removed_or_name_change);
#ifdef FORMS_VALIDATE_EMAIL // exposed API, see module.export
	/**
	 * Checks if a string (not NULL) matches the definition of an
	 * email address in RFC2822. Not that this method is quite slow
	 * and should not be called unless it's very likely that the
	 * string actually is an email address.  This is used by WebForms2
	 * validation of email addresses.
	 *
	 * @param string The string to check. Must not be NULL.
	 * @returns TRUE if the string matches the RFC2822 definition of an email address, FALSE otherwise.
	 */
	static BOOL IsValidEmailAddress(const uni_char* string);
#endif // FORMS_VALIDATE_EMAIL

	/**
	 * To be called before the document is deleted so
	 * that pointers in existing FormSubmits can be
	 * cleared and the submit ignored.
	 *
	 * @param[in] doc The FramesDocument that is being deleted.
	 */
	static void AbortSubmitsForDocument(FramesDocument* doc);

	/**
	 * Returns the default submit button element for a given form.
	 * @param[in] doc The FramesDocument to search in.
	 * @param[in] form The form (HE_FORM) element.
	 * @return A HE_INPUT/HE_BUTTON element or NULL if none is found.
	 */
	static HTML_Element* FindDefaultSubmitButton(FramesDocument* doc, HTML_Element* form);
};

#endif // FORMMANAGER_H
