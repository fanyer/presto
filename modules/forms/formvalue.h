/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMVALUE_H
#define FORMVALUE_H

#include "modules/util/adt/opvector.h"
#include "modules/logdoc/complex_attr.h"

class HTML_Element;
class FormObject;
class FramesDocument;
class HLDocProfile;

#include "modules/forms/datetime.h"
#include "modules/style/css.h"
#include "modules/widgets/OpEditCommon.h"

class FormValueList;
class FormValueRadioCheck;
class FormValueNoOwnValue;
class FormValueWF2DateTime;
class FormValueNumber;
#ifdef FORMS_KEYGEN_SUPPORT
class FormValueKeyGen;
#endif // FORMS_KEYGEN_SUPPORT

/**
 * Every form type html element has a form value. The form value can
 * be internal or external. If it's external the value is really
 * stored in a widget. The widget can only be reached through the
 * HTML_Element which is the reason many methods has a HTML_Element as
 * a parameter. We don't want to store that pointer in the FormValue
 * for two reasons. First it cost 4 bytes which is too much and second
 * we don't want to maintain that pointer.
 */
class FormValue : public ComplexAttr
{
public:
	/**
	 * Different types of FormValue. Every FormValue class should have it's
	 * own. The most interesting right now is VALUE_LIST_SELECTION and
	 * VALUE_RADIOCHECK which must be handled different from the rest that
	 * for most purposes are simple texts.
	 */
	enum FormValueType
	{
		VALUE_BEFORE_FIRST,

		/**
		 * Used by FormValueText.
		 */
		VALUE_TEXT,

		/**
		 * Used by FormValueTextArea.
		 */
		VALUE_TEXTAREA,

		/**
		 * Used by FormValueList.
		 */
		VALUE_LIST_SELECTION,

		/**
		 * Used by FormValueRadioCheck.
		 */
		VALUE_RADIOCHECK,

		/**
		 * Used by FormValueWF2DateTime (for types month, week, date, time, datetime and datetime-local)
		 */
		VALUE_DATETIMERELATED,
		/**
		 * Used by FormValueNumber (types number and range).
		 */
		VALUE_NUMBER,

		/**
		 * Used by FormValueOutput.
		 */
		VALUE_OUTPUT,

		 /**
		  * Used by FormValueColor.
		 */
		VALUE_COLOR,

#ifdef FORMS_KEYGEN_SUPPORT
		/**
		 * Used by FormValueKeyGen.
		 */
		VALUE_KEYGEN,
#endif // FORMS_KEYGEN_SUPPORT

		 /**
		 * Used by FormValueNoOwnValue which is for elements such as buttons.
		 */
		VALUE_NO_OWN_VALUE,

		 VALUE_AFTER_LAST
	};

protected:
	/**
	 * Constructor of the base class. Used to set the value type. The
	 * FormValue is initialized as being "internal". It also zeroes the
	 * char that is used by sub classes that wants to store some data.
	 *
	 * @param type The type this form value is.
	 */
	FormValue(FormValueType type)
	{
		op_memset(&members, 0, sizeof(members));

		members.m_type = type;
		m_serial_nr = -1;
	}

public:
	/**
	 * The one and only right way to create a new FormValue.
	 *
	 * @param he The form element the form value will belong to.
	 * @param hld_profile Document profile the element belongs to.
	 * This can be NULL, but if it's not NULL and the document is an
	 * internal document (opera:config) then some security checks
	 * will be disabled.
	 *
	 * @param out_value Pointer to the created FormValue.
	 */
	static OP_STATUS ConstructFormValue(HTML_Element* he,
										HLDocProfile* hld_profile,
										FormValue*& out_value);
	/**
	 * The destructor. Virtual since we will delete objects
	 * by deleting pointers to the base class.
	 */
	virtual ~FormValue() {}

	/**
	 * Creates a copy of this form value. XXX Externalized clones.
	 *
	 * Triggered by calls to DOM that clones HTML nodes and by
	 * WEBFORMS2's REPEAT elements.
	 */
	virtual FormValue* Clone(HTML_Element *he = NULL) = 0;

	/**
	 * From the ComplexAttr interface. See Clone.
	 */
	virtual OP_STATUS	CreateCopy(ComplexAttr **copy_to)
	{
		*copy_to = Clone();
		return *copy_to ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}

	/**
	 * The main way to get the value from a form element. This returns a
	 * string representing the entered value. Before calling this it's
	 * best to check HasMeaningfulTextRepresentation. For VALUE_RADIOCHECK
	 * this will return the value of the radio button or checkbox if it's set.
	 * If you want to see if it's set you should call IsChecked in FormValueRadioCheck.
	 *
	 *
	 * <code>
	 * FormValue* form_value = ...;
	 * BOOL is_checked = FormValueRadioCheck::GetAs(form_value)->IsChecked();
	 * </code>
	 *
	 * For VALUE_LIST_SELECTION this will not work at all. Use the methods
	 * in FormValueList.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param out_value The returned text.
	 */
	virtual OP_STATUS GetValueAsText(HTML_Element* he,
									 OpString& out_value) const = 0;
	/**
	 * Set the value of this form element. This is not the same as
	 * setting the attribute value in HTML. The attribute represents
	 * the default value, not the set value.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param in_value The new value to set. May be NULL if the current value
	 * should be emptied.
	 */
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
									   const uni_char* in_value) = 0;

	/**
	 * Overload of SetValueFromText but allows the callee to tell
	 * if the previous text should be stored in the undo/redo buffer.
	 * For sanity's sake, this is not ifdefed with WIDGETS_UNDO_REDO_SUPPORT
	 * although the BOOL argument will be ignored
	 *
	 * @param he             The HTML element this FormValue belongs to.
	 * @param in_value       The new value to set. May be NULL if the
	 *                       current value should be emptied.
	 * @param use_undo_stack Tells if the previous value should be stored
	 *                       in the undo/redo stack
	 *
	 */
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
	                                   const uni_char* in_value,
	                                   BOOL use_undo_stack)
	{ return SetValueFromText(he, in_value); };

	/**
	 * Return to the default content of the form element. Typically used
	 * when the user clicks a reset button or a script calls the reset method.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 */
	virtual OP_STATUS ResetToDefault(HTML_Element* he) = 0;

	/**
	 * Returns the type of FormValue.
	 *
	 * @see FormValueType
	 */
	FormValueType GetType() const { return (FormValueType)members.m_type; } // cast because some compilers have problem with enums in bit fields

	/**
	 * Returns TRUE if GetValueAsText can return text. FALSE if it's
	 * without meaning to call GetValueAsText. For instance a audio
	 * input would normally doesn't have a text representation.
	 */
	BOOL HasMeaningfulTextRepresentation() const
	{
		return GetType() != VALUE_LIST_SELECTION
#ifdef FORMS_KEYGEN_SUPPORT
			&& GetType() != VALUE_KEYGEN
#endif // FORMS_KEYGEN_SUPPORT
			;
	}

	/**
	 * Moves the value to a widget or something else. The value will
	 * be fetched from FormObject. This is to save memory while the
	 * value is stored somewhere else, and to be able to find the
	 * current value at all times.
	 *
	 * @returns TRUE if it was correct to call Externalize or FALSE
	 * if the call to Externalize shouldn't have been done. This is there
	 * to allow sub classes to abort the externalization.
	 */
	virtual BOOL Externalize(FormObject* form_object_to_seed);

	/**
	 * Moves the value back to the FormValue. The value will no longer
	 * be fetched from the FormObject.
	 */
	virtual void Unexternalize(FormObject* form_object_to_replace);

#ifdef _DEBUG
	/**
	 * Debug only method.
	 */
	void AssertNotExternal() const { OP_ASSERT(!IsValueExternally()); }
#endif // _DEBUG

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/**
	 * Returns a spell session id if the element can be spell checked
	 * or 0 if that isn't possible. See WindowCommander for how
	 * these ids can be used to control spellchecking.
	 *
	 * @param he The HTML Element this FormValue belongs to.
	 *
	 * @param point A point in the field which should be spell checked in document
	 * coordinates relative the upper left corner. It's not
	 * really optional but has a default argument until all callers
	 * have been fixed to include a coordinate.
	 *
	 * @returns 0 or a spell session id.
	 */
	virtual int CreateSpellSessionId(HTML_Element* he, OpPoint *point = NULL);
#endif // INTERNAL_SPELLCHECK_SUPPORT

	/**
	 * Returns if this form element contains text that can be
	 * edited by the user. Right now this is used to check
	 * what kind of context menu should be displayed.
	 *
	 * @param he The HTML Element this FormValue belongs to.
	 */
	virtual BOOL IsUserEditableText(HTML_Element* he);

#ifdef WAND_SUPPORT
	/**
	 * Set if this value has a suggestion in wand. The value defaults
	 * to 0.
	 *
	 * @exists Nonzero if there is a value in wand. (May be one or several of WAND_HAS_MATCH_IN_xxxx)
	 */
	void SetExistsInWand(int exists) { OP_ASSERT(exists >= 0 && exists <= 3); members.m_exists_in_wand = exists; }
	/**
	 * Returns one or several of WAND_HAS_MATCH_IN_xxxx if there is a value in wand for this form value, otherwise 0.
	 */
	int ExistsInWand() const { return members.m_exists_in_wand; }
#endif // WAND_SUPPORT

	/**
	 * The value has been changed by the user (or by a script). This
	 * basically checks that the current value differs from the value
	 * in the original HTML.
	 *
	 * @param he The HTML Element this FormValue belongs to.
	 *
	 * @returns TRUE is the value has changed, FALSE otherwise.
	 */
	BOOL IsChangedFromOriginal(HTML_Element* he) const { return members.m_is_changed_from_original; }

	/**
	 * Sets that the value in the FormValue has changed. This method is
	 * not included in the external API, and is only
	 * usable from the forms module.
	 *
	 * @param he The HTML Element this FormValue belongs to.
	 */
	void SetIsChangedFromOriginal(HTML_Element* he) { members.m_is_changed_from_original = TRUE; }

	/**
	 * The value has been changed by the user (not by a script). This
	 * basically checks that the current value differs from the value
	 * in the original HTML.
	 *
	 * @param he The HTML Element this FormValue belongs to.
	 *
	 * @returns TRUE is the value has changed, FALSE otherwise.
	 */
	BOOL IsChangedFromOriginalByUser(HTML_Element* he) const { return members.m_is_changed_from_original_by_user; }

	/**
	 * Sets that the value in the FormValue has been changed by the
	 * user. This method is not included in the external API, and is
	 * only usable from the forms module.
	 *
	 * @param he The HTML Element this FormValue belongs to.
	 */
	void SetIsChangedFromOriginalByUser(HTML_Element* he) { members.m_is_changed_from_original_by_user = TRUE; }

	/**
	 * Is the value input using RTL? That information may be forwarded to a
	 * server via the dirname attribute.
	 *
	 * @param he The HTML Element this FormValue belongs to.
	 */
	BOOL IsUserInputRTL(HTML_Element* he);

	/**
	 * Set so that layout can mark the widget.
	 */
	void SetMarkedValidationError(BOOL validation_error) { members.m_failed_validation = validation_error; }
	/**
	 * Is the value marked as validation error? This might be wrong so it should only
	 * be used for non functional decisions such as how to display a value.
	 */
	BOOL IsMarkedValidationError() const { return members.m_failed_validation; }
	/**
	 * @return OpStatus::ERR_NO_MEMORY, OpStatus::ERR_NOT_SUPPORTED,
	 * OpStatus::ERR_OUT_OF_RANGE or OpStatus::OK.
	 */
	virtual OP_STATUS StepUpDown(HTML_Element* he, int step_count) { return OpStatus::ERR_NOT_SUPPORTED; }

	void SetMarkedPseudoClasses(int pseudo_class_flags);
	int GetMarkedPseudoClasses();

	/**
	 * Calculates current form pseudo classes.
	 *
	 * @param he The he for this form value.
	 */
	int CalculateFormPseudoClasses(FramesDocument* frames_doc, HTML_Element* he);

	/**
	 * Get a mask that shows which pseudo class fields are calculated
	 * and stored by a FormValue.
	 */
	static int GetFormPseudoClassMask() { return CSS_PSEUDO_CLASS_DEFAULT |
			CSS_PSEUDO_CLASS_INVALID | CSS_PSEUDO_CLASS_VALID |
			CSS_PSEUDO_CLASS_IN_RANGE | CSS_PSEUDO_CLASS_OUT_OF_RANGE |
			CSS_PSEUDO_CLASS_REQUIRED | CSS_PSEUDO_CLASS_OPTIONAL |
			CSS_PSEUDO_CLASS_READ_ONLY | CSS_PSEUDO_CLASS_READ_WRITE |
			CSS_PSEUDO_CLASS_ENABLED | CSS_PSEUDO_CLASS_DISABLED |
			CSS_PSEUDO_CLASS_CHECKED | CSS_PSEUDO_CLASS_INDETERMINATE; }

	/**
	 * Used when a user changes type of the FormValue (by setting
	 * the type attribute through DOM. When that happens we create
	 * a new FormValue and then this method is used to transer necessary
	 * FormValue properties to this new FormValue.
	 *
	 * @param old_other The FormValue to fetch properties from.
	 * @param elm The HTML_Element old_other FormValue belongs to and this FormValue will belong to after replacement (shortly).
	 * NOTE: This means that HTML_Element won't be matching the old_other FormValue soon. It'll be compatible with this FormValue instead.
	 */
	void PrepareToReplace(const FormValue& old_other, HTML_Element* elm);

	/**
	 * Get current selection in the out varibles
	 * or 0, 0, SELECTION_DIRECTION_DEFAULT if there is no selection or the selection not relevant for this value.
	 *
	 * @param he The HTML Element this FormValue belongs
	 * to. Can not be null.
	 * @param start_ofs The offset of the first selected character.
	 * @param stop_ofs The offset of the character after the last
	 * selected character. stop_ofs == start_ofs means that no
	 * character is selected.
	 * @param direction The direction of the selection.
	 */
	virtual void GetSelection(HTML_Element* he, INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction) const { start_ofs = stop_ofs = 0; direction = SELECTION_DIRECTION_DEFAULT; }

	/**
	 * Get current selection in the out varibles
	 * or 0, 0 if there is no selection or the selection not relevant for this value.
	 *
	 * @param he The HTML Element this FormValue belongs
	 * to. Can not be null.
	 * @param start_ofs The offset of the first selected character.
	 * @param stop_ofs The offset of the character after the last
	 * selected character. stop_ofs == start_ofs means that no
	 * character is selected.
	 */
	void GetSelection(HTML_Element *he, INT32 &start_ofs, INT32 &stop_ofs) const { SELECTION_DIRECTION direction; GetSelection(he, start_ofs, stop_ofs, direction); }

	/**
	 * Set current selection.
	 * Values incompatible with the
	 * current text contents
	 * will cause variables to be truncated to legal values before being used.
	 *
	 * @param he The HTML Element this FormValue belongs to. Cannot be null.
	 * @param start_ofs The offset of the first selected character.
	 * @param stop_ofs The offset of the character after the last selected
	 * character.  stop_ofs == start_ofs means that no character is selected.
	 * @param direction The direction of the selection.
	 */
	virtual void SetSelection(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction) {}

	/**
	 * Set current selection.
	 * Values incompatible with the
	 * current text contents
	 * will cause variables to be truncated to legal values before being used.
	 *
	 * @param he The HTML Element this FormValue belongs to. Cannot be null.
	 * @param start_ofs The offset of the first selected character.
	 * @param stop_ofs The offset of the character after the last selected
	 * character.  stop_ofs == start_ofs means that no character is selected.
	 */
	void SetSelection(HTML_Element *he, INT32 start_ofs, INT32 stop_ofs) { SetSelection(he, start_ofs, stop_ofs, SELECTION_DIRECTION_DEFAULT); }

	/**
	 * Selects all text in the form control.
	 *
	 * @param he The HTML Element this FormValue belongs to. Cannot be null.
	 */
	virtual void SelectAllText(HTML_Element* he) {}

	/**
	 * Highlight the search result by selection.
	 * Values incompatible with the
	 * current text contents
	 * will cause variables to be truncated to legal values before being used.
	 *
	 * @param he The HTML Element this FormValue belongs to. Cannot be null.
	 * @param start_ofs The offset of the first selected character.
	 * @param stop_ofs The offset of the character after the last selected character.  stop_ofs == start_ofs means that no character is selected.
	 * @param is_active_hit TRUE if this is the active search result.
	 */
	virtual void SelectSearchHit(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit) { SetSelection(he, start_ofs, stop_ofs); }

	/**
	 * Removes any visible traces of the selection. There might still be a
	 * collapsed selection somewhere but it will not be visible. This operation
	 * will not affect any scroll positions.
	 */
	virtual void ClearSelection(HTML_Element* he) {}

	/**
	 * Returns the current position of the caret in the form value.
	 *
	 * @param he The HTML Element this FormValue belongs
	 * to. Can not be null.
	 *
	 * @returns 0 if the caret is first in the field or
	 * if "caret position" has no meaning for the form value.
	 */
	virtual INT32 GetCaretOffset(HTML_Element* he) const { return 0;}

	/**
	 * Sets the current caret position.
	 * Values incompatible with the
	 * current text contents
	 * will cause variables to be truncated to legal values before being used.
	 *
	 * @param he The HTML Element this FormValue belongs
	 * to. Can not be null.
	 * @param caret_ofs The number of characters before the caret. 0 is at
	 * the beginning of the field. 1 is after the first character.
	 */
	virtual void SetCaretOffset(HTML_Element* he, INT32 caret_ofs) {}

	/**
	 * Help function used in HTML_Element::HandleAttributeChange
	 * since the old InputType is lost there. This should
	 * normally not be used and will return strings that are
	 * different from the original. Also, it only works for
	 * HE_INPUT.
	 */
	const uni_char* GetFormValueTypeString(HTML_Element* he) const;

	/**
	 * Returns the form value's serial number.  This number is used to
	 * determine the order in which the different form value's in the
	 * document was last changed.
	 */
	int GetSerialNr() { return m_serial_nr; }

	/**
	 * Updates the form value's serial number.  This means fetching a
	 * new serial number from the document, which in turn updates the
	 * document's counter.
	 */
	void UpdateSerialNr(FramesDocument *doc);

	/**
	 * If TRUE, form value changes during form state restoration is to
	 * be signalled to script event handlers.
	 */
	BOOL SignalValueRestored() { return !members.m_do_not_signal_value_restored; }

	/**
	 * Called if an onchange event handler did something "bad", such as
	 * submitting the form.  This will make SignalValueRestored return
	 * FALSE.
	 */
	void DoNotSignalValueRestored() { members.m_do_not_signal_value_restored = 1; }

	/**
	 * Reset "is changed from original" flags when value is reset. forms module
	 * internal. Do not call from outside the forms module.
	 */
	void ResetChangedFromOriginal()
	{
		members.m_is_changed_from_original = members.m_is_changed_from_original_by_user = FALSE;
	}

	/**
	 * Converts all line endings to CRLF pairs.
	 * @param[in,out] text The original value which will be converted.
	 * @returns Normal OOM values
	 */
	static OP_STATUS FixupCRLF(OpString& text);

	/**
	 * Converts all line endings to LF.
	 * @param[in,out] text The original value which will be converted.
	 * @returns No result or error conditions.
	 */
	static void FixupLF(OpString& text);

protected:
	/**
	 * Helper method to get the form value from a FormObject and put
	 * it in an OpString.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param out_value The string where the text will be put if the
	 * method returns OpStatus::OK.
	 */
	OP_STATUS GetFormObjectValue(HTML_Element* he, OpString& out_value) const;

	/**
	 * Helper method to get the form value from a FormObject and put
	 * it in an OpString.
	 *
	 * @param form_object The FormObject to fetch the value from.
	 *
	 * @param out_value The string where the text will be put if the
	 * method returns OpStatus::OK.
	 */
	OP_STATUS GetFormObjectValue(FormObject* form_object,
								 OpString& out_value) const;
	/**
	 * Checks if the value is externally, i.e. in the HTML Element's
	 * FormObject.
	 */
	BOOL IsValueExternally() const { return members.m_is_value_externally; }
	friend class WidgetListener;

#ifdef _DEBUG
private:
	// Not implemented to prevent people from using it. Use the GetAs methods.
	operator FormValueList*();
	operator FormValueRadioCheck*();
	operator FormValueNoOwnValue*();
	operator FormValueWF2DateTime*();
	operator FormValueNumber*();
#if defined _SSL_SUPPORT_ && !defined _EXTERNAL_SSL_SUPPORT_ && !defined(SSL_DISABLE_CLIENT_CERTIFICATE_INSTALLATION)
	operator FormValueKeyGen*();
#endif // SSL && !EXTERNAL_SSL
#endif // _DEBUG

private:
	struct
	{
		/**
		* This is used by sub classes to save space. By using
		* these 8 bits many objects shrink by 4 bytes. That is
		* if we are not too small to care about anyway.
		*/
		unsigned char m_reserved_char_1;

		FormValueType m_type:5;
		unsigned int m_is_value_externally:1;
#ifdef WAND_SUPPORT
		unsigned int m_exists_in_wand:2;
#endif // WAND_SUPPORT
		/**
		* Used to signal that this FormValue doesn't contain the
		* original value.
		*/
		unsigned int m_is_changed_from_original:1;
		/**
		* Completents m_is_changed_from_original, saying that at least
		* part of the change was made by the user.
		*/
		unsigned int m_is_changed_from_original_by_user:1;
		/**
		* Used by layout to mark widgets that failed validation.
		*/
		unsigned int m_failed_validation:1;
		/**
		 * Used to record that the value was input in RTL (right-to-left) mode.
		 */
		unsigned int m_is_user_input_rtl:1;

		/**
		 * Determines whether form value changes during form state
		 * restoration is signalled to scripts on the page.  Usually
		 * it is, but if there is an onchange handler that submits
		 * the form (for instance) it is not.
		 */
		unsigned int m_do_not_signal_value_restored:1;

		struct {
			unsigned int m_marked_invalid_pseudo:1;
			// valid can be deduced from invalid
			//			unsigned int is_valid_form_element:1;
			unsigned int m_marked_default_pseudo:1;
			unsigned int m_marked_in_range_pseudo:1;
			unsigned int m_marked_out_of_range_pseudo:1;
			unsigned int m_marked_required_pseudo:1;
			unsigned int m_marked_optional_pseudo:1;
			unsigned int m_marked_read_only_pseudo:1;
			// read_write can be deduced from read_only (= !read_only)
			//			unsigned int m_marked_read_write_pseudo:1;
			unsigned int m_marked_disabled_pseudo:1;
			unsigned int m_marked_enabled_pseudo:1;
			unsigned int m_marked_checked_pseudo:1;
			unsigned int m_marked_indeterminate_pseudo:1;
		} pseudo;
		// 10 bits used for CSS3 UI pseudo classes, not counting the shared bits
	} members;

	/**
	 * Used by form restoration code to order form changes so that
	 * form data can be restored in the same order as set by the user.
	 */
	int m_serial_nr;

	protected:
	/**
	 * Use this to access the char in this base class that might be
	 * used by sub classes. It is zeroed by the constructor.
	 */
	unsigned char& GetReservedChar1() { return members.m_reserved_char_1; }

	/**
	 * Use this to access the char in this base class that might be
	 * used by sub classes. It is zeroed by the constructor.
	 */
	const unsigned char& GetReservedChar1() const { return members.m_reserved_char_1; }
};


/**
 * Represents an &lt;OUTPUT&gt; tag.
 *
 * @author Daniel Bratell
 */
class FormValueOutput : public FormValue
{
public:
	FormValueOutput() : FormValue(VALUE_OUTPUT) {};
	// Use base class documentation
	virtual FormValue* Clone(HTML_Element *he);

	/**
	 * "Cast" the FormValue to this class. This asserts in debug
	 * builds if done badly. This should be inlined and be as
	 * cheap as a regular cast. If it turns out it's not we
	 * might have to redo this design.
	 *
	 * @param val The FormValue pointer that really is a pointer to
	 * an object of this class.
	 */
	static FormValueOutput* GetAs(FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_OUTPUT);
		return static_cast<FormValueOutput*>(val);
	}

	/**
	 * This function shouldn't be used because FormValueOutput is
	 * the odd bird. Use ResetOutputToDefault insted.
	 */
	virtual OP_STATUS ResetToDefault(HTML_Element* he);

	/**
	 * Use this function rather than the normal ResetToDefault.
	 */
	OP_STATUS ResetOutputToDefault(HTML_Element* he, FramesDocument* frames_doc);

	// Use base class documentation
	virtual OP_STATUS GetValueAsText(HTML_Element* he, OpString& out_value) const;
	static OP_STATUS ConstructFormValueOutput(HTML_Element* he,
											FormValue*& out_value);
	// Use base class documentation
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
									   const uni_char* in_value);
	/**
	 * Use this function rather than the normal SetValueFromText.
	 * We must have a FramesDocument to be able to do this. :-(
	 */
	OP_STATUS SetOutputValueFromText(HTML_Element* he,
										FramesDocument* frames_doc,
										const uni_char* in_value);
	// Use base class documentation
	virtual BOOL Externalize(FormObject* form_object_to_seed);
	// Use base class documentation
	virtual void Unexternalize(FormObject* form_object_to_replace);

	/**
	 * Get the default value of this output tag.
	 */
	OP_STATUS GetDefaultValueAsText(HTML_Element* he, OpString& out_value);

	/**
	 * Set the new default value of the output tag. That might change the current value also.
	 */
	OP_STATUS SetDefaultValueFromText(HTML_Element* he, const uni_char* default_value);

private:
	/**
	 * Removes all current content in the output element and
	 * inserts the new text instead.
	 *
	 * @param new_value The new string. If NULL nothing will be inserted, but
	 * the old content will still be removed.
	 */
	OP_STATUS ReplaceTextContents(HTML_Element* he,
								FramesDocument* frames_doc,
								const uni_char* new_value);

	unsigned char& GetHasDefaultValueBool() { return GetReservedChar1(); }
	OpString m_default_value;
};



/**
 * Represents an input of type NUMBER or RANGE.
 *
 * @author Daniel Bratell
 */
class FormValueNumber : public FormValue
{
public:
	FormValueNumber() :
	  FormValue(VALUE_NUMBER),
	  m_selection_start(0),
	  m_selection_end(0),
	  m_selection_direction(SELECTION_DIRECTION_DEFAULT),
	  m_caret_position(0) {}

	// Use base class documentation
	virtual FormValue* Clone(HTML_Element *he);

	/**
	 * "Cast" the FormValue to this class. This asserts in debug
	 * builds if done badly. This should be inlined and be as
	 * cheap as a regular cast. If it turns out it's not we
	 * might have to redo this design.
	 *
	 * @param val The FormValue pointer that really is a pointer to
	 * an object of this class.
	 */
	static FormValueNumber* GetAs(FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_NUMBER);
		return static_cast<FormValueNumber*>(val);
	}
	// Use base class documentation
	virtual OP_STATUS ResetToDefault(HTML_Element* he);
	// Use base class documentation
	virtual OP_STATUS GetValueAsText(HTML_Element* he, OpString& out_value) const;
	static OP_STATUS ConstructFormValueNumber(HTML_Element* he,
											FormValue*& out_value);
	// Use base class documentation
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
									   const uni_char* in_value);
	// Use base class documentation
	virtual BOOL Externalize(FormObject* form_object_to_seed);
	// Use base class documentation
	virtual void Unexternalize(FormObject* form_object_to_replace);
	// Use base class documentation
	virtual OP_STATUS StepUpDown(HTML_Element* he, int step_count);
	// Use base class documentation
	virtual BOOL IsUserEditableText(HTML_Element* he);

	// Use base class documentation
	virtual void GetSelection(HTML_Element* he, INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction) const;

	// Use base class documentation
	virtual void SetSelection(HTML_Element* he, INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction);

	// Use base class documentation
	virtual void SelectAllText(HTML_Element* he);

	// Use base class documentation
	virtual void ClearSelection(HTML_Element* he);

	// Use base class documentation
	virtual INT32 GetCaretOffset(HTML_Element* he) const;

	// Use base class documentation
	void SetCaretOffset(HTML_Element* he, INT32 caret_ofs);

private:

	/**
	 * Sets the value of this FormValue by parsing the given string.
	 *
	 * @param he The input element
	 * @param in_value The string to set this FormValue's value from.
	 *
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS SetValueFromTextInternal(HTML_Element* he, const uni_char* in_value);

	/**
	 * Convert the string in_value to a valid number for this input element.
	 *
	 * @param he The input element.
	 * @param in_value The string that should be converted.
	 * @param val (out) The resulting number.
	 *
	 * @return TRUE on success or FALSE if the string couldn't be converted into
	 * a valid value.
	 */
	BOOL ValueFromText(HTML_Element* he, const uni_char* in_value, double& val);

	void ResetInternalSelection();

	double SnapToStepMinMax(double number, double min_val, double max_val, double step_base, double step_val);
	double CalculateRangeDefaultValue(HTML_Element* he);

	/**
	 * Incase we're not external, do we have a valid value?
	 */
	BOOL HasValue() { return !m_value.IsEmpty(); }

	/**
	 * Clear the internal value.
	 */
	void ClearValue() { m_value.Empty(); }

	/**
	 * The internal string representation of the current value, only valid when we're not external.
	 */
	OpString m_value;

	/**
	 * The internal number, only valid if we're not external and HasValue() returns TRUE.
	 */
	double m_number;

	/**
	 * The selection start if there is no widget to ask.
	 */
	INT32 m_selection_start;
	/**
	 * The selection end if there is no widget to ask.
	 */
	INT32 m_selection_end;
	/**
	 * The direction of the selection if there is no widget to ask.
	 */
	SELECTION_DIRECTION m_selection_direction;
	/**
	 * caret pos if there is no widget to ask. -1 means "at the end" and is the initial value.
	 */
	INT32 m_caret_position;
};

/**
 * This type of FormValue consists of nothing, it's used
 * for buttons and hidden elements.
 */
class FormValueNoOwnValue : public FormValue
{
private:
	/**
	 * Constructor.
	 */
	FormValueNoOwnValue() : FormValue(FormValue::VALUE_NO_OWN_VALUE)
	{
	}

	/**
	 * The default external value and the value also used in form
	 * submissions should the element not have a value attribute of its own.
	 */
	OpString m_default_value;

public:
	/**
	 * Factory method.
	 *
	 * @param he The element to construct the FormValueNoOwnValue for.
	 *
	 * @param out_value Out parameter that will receive the
	 * FormValueNoOwnValue if the method returns OpStatus::OK.
	 *
	 * @returns OpStatus::ERR_NO_MEMORY if OOM, OpStatus::OK
	 * otherwise.
	 */
	static OP_STATUS ConstructFormValueNoOwnValue(HTML_Element* he,
												  FormValue*& out_value)
	{
		FormValueNoOwnValue* val = OP_NEW(FormValueNoOwnValue, ());
		out_value = val;
		return val ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
	}

	/**
	 * The destructor.
	 */
	virtual ~FormValueNoOwnValue() {}

	// Use base class documentation
	virtual FormValue* Clone(HTML_Element *he);

	/**
	 * "Cast" the FormValue to this class. This asserts in debug
	 * builds if done badly. This should be inlined and be as
	 * cheap as a regular cast. If it turns out it's not we
	 * might have to redo this design.
	 *
	 * @param val The FormValue pointer that really is a pointer to
	 * an object of this class.
	 */
	static FormValueNoOwnValue* GetAs(FormValue* val)
	{
		OP_ASSERT(val->GetType() == VALUE_NO_OWN_VALUE);
		return static_cast<FormValueNoOwnValue*>(val);
	}

	// Use base class documentation
	virtual OP_STATUS GetValueAsText(HTML_Element* he,
									 OpString& out_value) const;

	// Use base class documentation
	virtual OP_STATUS SetValueFromText(HTML_Element* he,
									   const uni_char* in_value);

	/**
	 * Set the value as an attribute from text
	 *
	 * As opposed to SetValueFromText, this function will
	 * not set the attribute value on the element, since
	 * this function should be called from the place that
	 * sets the attribute value.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param in_value The new value to set. May be NULL if the current value
	 * should be emptied.
	 */
	void SetValueAttributeFromText(HTML_Element* he, const uni_char* in_value);

	/**
	 * Gets the effective value for this form element.
	 *
	 * Certain buttons have a default value besides their value
	 * attribute. These values are used in the UI if the
	 * element is without a value attribute and also as value
	 * on form submission.
	 *
	 * @param he The HTML element this FormValue belongs to.
	 *
	 * @param out_value The resulting string.
	 *
	 */
	OP_STATUS GetEffectiveValueAsText(HTML_Element *he, OpString &out_value);

	// Use base class documentation
	virtual OP_STATUS ResetToDefault(HTML_Element* he)
	{
		// Has no default. Just ignore.
		return OpStatus::OK; // dummy
	}

	virtual BOOL Externalize(FormObject* form_object_to_seed);
	// Not editable -> we don't need any Unexternalize
};

#endif // FORMVALUE_H
