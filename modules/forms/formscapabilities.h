/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2003-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef FORMS_CAPABILITIES_H
#define FORMS_CAPABILITIES_H

/**
 * The forms module has internal tab stops in FormObject. Added spring
 * 2004 in core-2.
 */
#define FORMS_CAP_HAS_INTERNAL_TAB_STOP

/**
 * The forms module has split the FormObject into one "content"
 * object, the FormValue and one layout object, the FormObject.
 * Added summer 2004 in core-2.
 */
#define FORMS_CAP_FORM_VALUE

/**
 * The piforms used to contain a public member variable that
 * told if the construction succeeded. That wasted memory and
 * made memory tools fail so it was removed and replaced
 * by traditional factory methods that returned OP_STATUS.
 * Added summer 2004 in core-2.
 */
#define FORMS_CAP_NO_INITSTATUS

/**
 * The forms module is update to the call-for-comments version of
 * the WebForms2 specification. Added summer 2004 in core-2.
 */
#define FORMS_CAP_WF2CC

/**
 * FormValueOutput has special methods to handle element setting and
 * resetting that takes FramesDocument. Added autumn 2004 in core-2.
 */
#define FORMS_CAP_SPECIAL_OUTPUT_RESET

/**
 * FormValue can calculate the pseudo classes for a form. This moves
 * common code from doc and logdoc here. Added autumn 2004 in core-2.
 */
#define FORMS_CAP_CALCULATE_PSEUDO_CLASS

/**
 * There is a DaySpec::GetWeek function. Added November 2004 in
 * core-2.
 */
#define FORMS_CAP_GET_WEEK

/**
 * There is a FormValue::IsChangedFromOriginal() for Wand. Added
 * December 2004 in core-2.
 */
#define FORMS_CAP_IS_CHANGED_FROM_ORIGINAL

/**
 * The new ResetDefaultButton function has one argument and is static. This
 * replaces an old ResetDefaultButton that was a member function. That function
 * will be included a little while longer for compatibility reasons.
 *
 * Added December 2004 in core-2.
 */
#define FORMS_CAP_STATIC_RESETDEFAULTBUTTON

/**
 * There is a static HasSpecifiedBorder in CSS that can be used to check if
 * a certain CSS will trigger a custom border or if the widget border will
 * be enabled. This is merged from core-1 and should be used in core-2 too.
 *
 * Added December 2004 in core-2.
 */
#define FORMS_CAP_HAS_SPECIFIED_BORDER

/**
 * There is GetSelection, SetSelection, GetCaretOffset and SetCaretOffset methods in 
 * FormValue. They're also in FormObject but this shouldn't be used externally. Added
 * January 2005 in core-2.
 */
#define FORMS_CAP_HAS_SELECTION_AND_CARETOFFSET

/**
 * There is a FormValue::GetFormValueTypeString to be used when trying to
 * restore the type attribute when we fail to replace a form value when
 * the type attribute has changed.
 *
 * Added February 2005 in core-2.
 */
#define FORMS_CAP_HAS_GETTYPESTRING

/**
 * The FormValue class inherits from ComplexAttr in logdoc to make it easier to use for logdoc.
 *
 * Added May 2005 in core-2.
 */
#define FORMS_CAP_COMPLEX_FORM_VALUE

/**
 * Some special attributes used in forms have the SpecialNs::NS_FORMS namespace
 *
 * Added July 2005 in core-2.
 */
#define FORMS_CAP_HAS_SPECIAL_ATTRS

/**
 * Has SelectionObject::RemoveGroup
 *
 * Added July 2005 in core-2.
 */

#define FORMS_CAP_HAS_REMOVEGROUP

/**
 * Pass HTML_Element to SelectionObject::ChangeElement.
 *
 * Added summer 2005 in core-2.
 */
#define FORMS_CAP_OPTION_ELEMENT

/**
 * Form::GetDisplayURL() exists. Added August 2005 in core-2.
 */
#define FORMS_CAP_DISPLAY_URL

/**
 * There is a FormValidator::WillValidate method.
 *
 * Added September 2005 in core-2.
 */
#define FORMS_CAP_HAS_WILL_VALIDATE

/**
 * There is a TemplateHandler::IsRepeatBlockBelongingToTemplate method.
 *
 * Added October 2005 in core-2.
 */
#define FORMS_CAP_HAS_REPEAT_BLOCK_CONNECT

/**
 * FormValue supports the functions GetSerialNr, UpdateSerialNr,
 * SignalValueRestored and DoNotSignalValueRestored.
 *
 * Added November 2005 in core-2.
 */
#define FORMS_CAP_FORMVALUE_SERIAL_NR

/**
 * There is support for the oninput, and onforminput events.
 *
 * Added November 2005 in core-2.
 */
#define FORMS_CAP_HAS_ONINPUT

/**
 * There is a Form::GetQueryString method.
 *
 * Added December 2005 in core-2.
 */
#define FORMS_CAP_HAS_GETQUERYSTRING

/**
 * There is a FormSuggestion class to be used for autocomplete.
 *
 * Added December 2005 in core-2.
 */
#define FORMS_CAP_FORMSUGGESTION

/**
 * There are TemplateHandler methods to handle template index from DOM.
 *
 * Added January 2006 in core-2.
 */
#define FORMS_CAP_HAS_TEMPLATE_INDEX

/**
 * Allows opera:config to prefill a file path in a file selector.
 *
 * Added January 2006 in core-2.
 */
#define FORMS_CAP_OPERACONFIG_FILE_PATH

/**
 * There is a FormValueListener::HandleValueChanged() method.
 *
 * Added January 2006 in core-2.
 */
#define FORMS_CAP_FORMVALUELISTENER

/**
 * There is a TimeSpec::GetFraction() and
 * TimeSpec::GetFractionDigitCount() that should be used instead of
 * accessing the m_hundreths member to prepare for renaming and
 * changing the meaning of that member.
 *
 * Added January 2006 in core-2.
 */
#define FORMS_CAP_AS_TIME_T

/**
 * Widgets don't send the ONCLICK DOM event themselves anymore. It
 * should be done as the default action of the mouseup event. This
 * also includes SaveStateBeforeOnClick and RestoreStateAfterOnClick
 * in FormValueRadioCheck.
 *
 * Added January 2006 in core-2
 */
#define FORMS_CAP_STOPPED_SENDING_ONCLICK

/**
 * The method FormValue::IsChangedFromOriginal is always
 * exported. Used to be exported only for WAND_SUPPORT.
 *
 * Added January 2006 in core-2
 */
#define FORMS_CAP_CHANGED_FROM_ORIGINAL

/**
 * New WebForms2Number methods. StepNumber, GetNumberRestrictions....
 *
 * Added February 2006 in core-2
 */
#define FORMS_CAP_STEPNUMBER

/**
 * The FormManager exists.
 *
 * Added February 2006 in core-2
 */
#define FORMS_CAP_FORMMANAGER

/**
 * FormValueKeyGen::Externalize inserts the values when needed.
 *
 * Added March 2006 in core-2
 */
#define FORMS_CAP_KEYGEN_VALUES_INSERTED

/**
 * FormValueRadioCheck::SetIsChecked takes an (right now optional)
 * document and BOOL syaing if it should process the whole radiogroup.
 *
 * Added May 2006 in core-2
 */
#define FORMS_CAP_SET_CHECKED_RADIOGROUP

/**
 * FormValue::Clone takes an optional HTML_Element* argument (and
 * works on externalized values if the argument is supplied.)
 *
 * Added May 2006 in core-2
 */
#define FORMS_CAP_CLONE_ELEMENT_ARGUMENT

/**
 * FormSuggestion::GetItems takes an optional BOOL parameter where
 * the method can tell if the returned data contains data that might
 * be private and shouldn't be made available to scripts.
 *
 * Added May 2006 in core-2
 */
#define FORMS_CAP_FORMSUGGESTION_PRIVACY

/**
 * FormManager::ValidateWmlInputData() has a BOOL parameter indicating
 * whether we are validating while the user is typing in form values or not
 *
 * Added May 2006 in core-2
 */
#define FORMS_CAP_VALIDATE_WHILE_TYPING

/**
 * FormManager::ValidateWmlInputData() is called in the FormValueListener
 *
 * Added June 2006 in core-2
 */
#define FORMS_CAP_VALIDATES_WML

/**
 * FormManager::HandleChangedRadioGroup exists.
 *
 * Added June 2006 in core-2.
 */
#define FORMS_CAP_HANDLE_CHANGED_RADIO_GROUP

/**
 * FormValueListener::HandleValueChanged can handle nodes outside the
 * tree and nodes being processed by XSLT.
 *
 * Added June 2006 in core-2.
 */
#define FORMS_CAP_SMARTER_HANDLEVALUECHANGED

/**
 * FormValidator::ValidateForm and FormValidator::SendOnInvalidEvent
 * has an extra thread parameter.
 *
 * Added June 2006 in core-2.
 */
#define FORMS_CAP_THREAD_IN_VALIDATION

/**
 * There is a minimal functionality FileUploadObject even if
 * _FILE_UPLOAD_SUPPORT_ is not set.
 *
 * Added July 2006 in core-2.
 */
#define FORMS_CAP_MINIMAL_FILE_UPLOAD

/**
 * DaySpec (the more advanced methods in it) works beyond
 * the earlier limit of 1900-2099.
 *
 * Added August 2006 in core-2
 */
#define FORMS_CAP_UNIVERSAL_DAY_OF_WEEK

/**
 * FormValueList has a method (HandleOptionListChanged) for updating
 * the widget when dom changes.
 *
 * Added September 2006 in core-2
 */
#define FORMS_CAP_HANDLE_OPTION_LIST_CHANGED

/**
 * FormValue::IsChangedFromOriginalByUser is supported.
 *
 * Added September 2006 in core-2.
 */
#define FORMS_CAP_CHANGED_FROM_ORIGINAL_BY_USER

/**
 * FormValueListener::HandleValueChanged requires a thread parameter
 * and so does FormValueList::SetIsChecked()
 *
 * Added September 2006 in core-2.
 */
#define FORMS_CAP_THREAD_IN_HANDLE_VALUE_CHANGED

/**
 * TempBuffer8 is made a public API and tempbuffer8.h is moved.
 *
 * Added November 2006 in core-2.
 */
#define FORMS_CAP_PUBLIC_TEMPBUFFER8

/**
 * FormObject::OnMouseUp has an extra parameter nclicks.
 *
 * Added December 2006 in core-2
 */
#define FORMS_CAP_MOUSE_UP_HAS_COUNT

/**
 * FormRadioGroups/FormRadioGroup exists.
 *
 * Added December 2006 in core-2.
 */
#define FORMS_CAP_FORMRADIOGROUP

/**
 * SelectionObject::GetDocumentAtPos exists and if
 * WIDGETS_CAP_LISTBOX_FIND_ITEM is set it actually works as well.
 *
 * Added December 2006 in core-2.
 */
#define FORMS_CAP_LISTBOX_TO_OPTION

/**
 * The API API_FORMS_VALIDATEEMAIL is available which adds the method FormsManager::IsValidEmailAddress.
 *
 * Added January 2007 in core-2
 */
#define FORMS_CAP_VALIDATEEMAIL_API

/**
 * FormObject has HasSpecifiedBackgroundImage method.
 *
 * Added January 2007 in core-2
 */
#define FORMS_CAP_HAS_SPEC_BG_IMAGE

/**
 * Has stopped using HTML_Element::GetSelectTextValue
 *
 * Added January 2007 in core-2
 */
#define FORMS_CAP_NO_GETSELECTTEXTVALUE

/**
 * FormObject::SetSize() returned preferred width of object
 * if intrinsic width is needed.
 *
 * Added February 2007 in core-2
 */
#define FORMS_CAP_SETSIZE_RETURNS_PREFWIDTH

/**
 * FormManager::ValidateWMLForm exists.
 *
 * Added February 2007 in core-2
 */
#define FORMS_CAP_VALIDATEWMLFORM

/**
 * FormObject (SelectionObject) handles ATTR_MULTIPLE internally and
 * the value used in other methods is not needed.
 *
 * Added March 2007 in core-2
 */
#define FORMS_CAP_OWN_MULTIPLE_HANDLING

/**
 * The module can handle overflow_x, overflow_y instead of overflow
 * in HTMLayoutProperties.
 *
 * Added March 2007 in core-2
 */
#define FORMS_CAP_LAYOUT_OVERFLOW_XY

/**
 * FormObject::FocusInternalEdge exists.
 *
 * Added May 2007 in core-2
 */
#define FORMS_CAP_FOCUS_EDGE

/**
 * TextAreaObject::GetScrollableSize exists.
 *
 * Added May 2007 in core-2
 */
#define FORMS_CAP_SCROLLABLE_SIZE

/**
 * FormObject::SetSize(lots of arguments) is
 * gone. FormObject::GetPreferredSize exists. Code rewritten by emil
 * to fix lots of small form sizing problems. Requires a new layout
 * module and a new widgets module.
 *
 * Added May 2007 in core-2
 */
#define FORMS_CAP_GET_PREFERRED_SIZE

/**
 * The fileupload has correct handling of borders, size and padding.
 * Was left out when fixing FORMS_CAP_GET_PREFERRED_SIZE.
 *
 * Added June 2007 in core-2
 */
#define FORMS_CAP_FILEUPLOAD_CSS_FIX

/**
 * FormValueList::DOMIsFreeOptionSelected and
 * FormValueList::DOMSetFreeOptionSelected exists.
 *
 * Added June 2007 in core-2
 */
#define FORMS_CAP_DOM_FREE_OPTIONS

/**
 * FormValueText::GetCopyOfInternalText exists.
 *
 * Added July 2007 in core-2
 */
#define FORMS_CAP_GET_COPY_OF_INTERNAL_TEXT

/**
 * This module includes layout/cssprops.h and layout/layout_workplace.h where needed,
 * instead of relying on header files in other modules doing it.
 *
 * Added late 2007 in core-2
 */
#define FORMS_CAP_INCLUDE_CSSPROPS

/**
 * SelectionObject::ChangeElement takes a HTMLayoutProperties so that we can style the options and there
 * is a SelectionObject::ChangeElement without any HTML_Element at all and only a props.
 *
 * Added January 2008 in core-2.1
 */
#define FORMS_CAP_PROPS_IN_CHANGEELEMENT

/**
 * Sometimes the checked attribute is allowed to change the
 * form state. Sometimes it isn't. This adds methods to
 * check such things.
 */
#define FORMS_CAP_IS_CHECKED_ATTR_CHECKING

/**
 * FormObject will set the IMStyle on OpEdit and OpMultiEdit
 * if the apis for it are available.
 *
 * Added March 2008 in core-2.2
 */
#define FORMS_CAP_SET_IM_STYLE

/**
 * TempBuffer8::AppendURLEncoded
 * takes a parameter to decide if space should be encoded as plus or %20
 *
 * Added June 2008 in core-2.2
 */
#define FORMS_CAP_URL_ENCODE_SANS_PLUS

/**
 * FormObject::OnMouseDown takes a cancelled_by_script parameter.
 *
 * Added June 2008 in core-2.2 and core-2.1.1
 */
#define FORMS_CAP_CANCELLED_ONMOUSEDOWN

/**
 * SelectionObject::BeginGroup takes a HTMLayoutProperties so that we can style the options and there
 * is a SelectionObject::ChangeElement without any HTML_Element at all and only a props.
 *
 * Added June 2008 in core-2.2
 */
#define FORMS_CAP_PROPS_IN_BEGINGROUP

/**
 * Form::AddUploadFileNamesL won't escape filenames sent to upload anymore if
 * UPLOAD_CAP_CAN_HANDLE_NAMES_WITH_PERCENT is defined
 *
 * Added June 2008 in core-2.2
 */
#define FORMS_CAP_NOT_ESCAPE_UPLOAD_NAMES

/**
 * FormManager::AbortSubmitsForDocument exists so that pointers can be
 * reset when a document is going away.
 */
#define FORMS_CAP_ABORT_SUBMITS

/**
 * Supports ASCII key strings for actions/menus/dialogs/skin.
 *
 * Added Novemeber 2008 in core-2.3
 */
#define FORMS_CAP_INI_KEYS_ASCII

/**
 * If FormValue::IsUserEditableText() exists.
 *
 * Added November 2008 for core-2.3
 */
#define FORMS_CAP_IS_EDITABLE

/**
 * If FormValue::GetSpellSessionId() exists if
 * INTERNAL_SPELLCHECK_SUPPORT is defined.
 *
 * Added November 2008 for core-2.3
 */
#define FORMS_CAP_SPELLCHECKING

/**
 * The forms module understands that background properties has moved
 * from logdoc to layout.
 *
 * Added December 2008 in core-2.3
 */
#define FORMS_CAP_CSS3_BACKGROUND

/**
 * Has FormObject::GetCaretPosInDocument.
 *
 * Added December 2008 in core-2.3
 */
#define FORMS_CAP_GET_CARET_POS_IN_DOCUMENT

/**
 * SelectionObject has ApplyProps
 *
 * Added January 2009 in core-2.3
 */
#define FORMS_CAP_HAS_APPLYPROPS

/**
 * Will not call GetCssColorFromStyleAttr and similar if the right defines are set.
 *
 * Added January 2009 in core-2.3.
 */
#define FORMS_CAP_NO_GETCOLOUR

/**
 * FormObject has SetWidgetPosition
 *
 * Added February 2009 in core-2.3
 */
#define FORMS_CAP_HAS_SETWIDGETPOSITION

/**
 * If all the CreateSpellSessionId():s take an optional OpPoint argument.
 *
 * Added February 2009 in core-2.3
 */
#define FORMS_CAP_CREATESPELLSESSION_HAS_POINT_ARG

/**
 * Now the TemplateHandler and all the repeat code is going away.
 *
 * Added February 2009 in core-2.3
 */
#define FORMS_CAP_NO_MORE_REPEAT_TEMPLATES

/**
 * SelectionObject::ApplyProps taking LayoutProps
 * 
 * Added February 2009 in core-2.3
 */
#define FORMS_CAP_APPLYPROPS_LAYOUTPROPS

/**
 * Understands that HTMLayoutProperties::current_script may be an enum
 *
 * Added April 2009 in core-2.4
 */
#define FORMS_CAP_WRITINGSYSTEM_ENUM

/**
 * Isn't using the IMS functions in widgets.
 *
 * Added June 2009 in core-2.4
 */
#define FORMS_CAP_NOT_CHECKING_IMS

/**
 * The form seeding code is removed.
 *
 * Added July 2009 in core-2.4
 */
#define FORMS_CAP_REMOVED_FORM_SEEDING

/**
 * Form::GetDisplayURL() takes a FramesDocument parameter
 *
 * Added July 2009 in core-2.4
 */
#define FORMS_CAP_GETDISPLAYURL_TAKES_DOC

/**
 * Stopped using DOMCONTROLVALUECHANGED.
 *
 * Added July 2009 in core-2.4
 */
#define FORMS_CAP_NO_DOMCONTROLVALUECHANGED

#endif // FORMS_CAPABILITIES_H
