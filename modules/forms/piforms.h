/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef PIFORMS_H
#define PIFORMS_H

class HTML_Element;
class InputObject;
class VisualDevice;
class OpEdit;
class OpFont;
class FramesDocument;
class ES_Thread;
class WidgetListener;

#include "modules/logdoc/logdocenum.h"

#include "modules/widgets/OpWidget.h"

#include "modules/hardcore/mem/mem_man.h"

#include "modules/hardcore/timer/optimer.h"

class DocumentManager;
class HTMLayoutProperties;
class ValidationErrorWindow;

/**
 * This class represents the special information that connects an
 * HTML Element to something visual with data.
 */
class FormObject
 : public OpInputContext
{
	// Implement suitable operators new/delete that will do
	// pooling and/or memory accounting or neither depending
	// on configuration.
	OP_ALLOC_ACCOUNTED_POOLING
	OP_ALLOC_ACCOUNTED_POOLING_SMO_DOCUMENT

protected:
	/**
	 * The widget that is used to visualize the form element.
	 */
	OpWidget* opwidget;
	/**
	 * The document the control belongs to.
	 */
	FramesDocument* m_doc;
	/**
	 * The form control's visual device.
	 */
	VisualDevice*	m_vis_dev;
	/**
	 * The listener that is used to get information from the widget.
	 */
	WidgetListener* m_listener;

	/**
	 * The width of the form object in pixels.
	 */
	int			m_width;
	/**
	 * The height of the form object in pixels.
	 */
    int             m_height;
	/**
	 * If the form object is displayed.
	 */
	BOOL			m_displayed;
	/**
	 * The HTML Element that this form object represents.
	 */
    HTML_Element*	m_helm;
	/**
	 * Set when display properties should be updated.
	 */
	BOOL			m_update_properties;
	/**
	 * Used when a formobject is temporarily saved during
	 * reflow. Should not touch document etc when this is TRUE;
	 */
	BOOL			m_locked;

	/**
	 * The validation error window.
	 */
	ValidationErrorWindow* m_validation_error;

	/**
	 * If this form element has already processed its autofocus
	 * attribute (if any). Should this maybe be in FormValue instead?
	 */
	BOOL m_autofocus_attribute_processed;

	int				m_memsize;

public:

	/**
	 * Constructor. Base information about the form element.
	 * @param vd The VisualDevice for this FormObject.
	 * @param he The HTML_Element this object will be connected to.
	 * @param doc The document where this FormObject can be found.
	 */
					FormObject(VisualDevice *vd, HTML_Element* he, FramesDocument* doc);
	/**
	 * Destructor.
	 */
	virtual		~FormObject();

	/**
	 * Lock or unlock the form control.
	 * Locking is performed during the reflow when the form control's layout box is removed.
	 * After recreating the form control's layout box, the form control is unlocked.
	 * Note that when there's no need to recreate the form control's layout box (e.g. its style.display = 'none')
	 * the form control remains locked.
	 * Also note that this needs to be called in order to inform the widget connected
	 * with the form object about the lock's state as it may need
	 * to take some actions (e.g. hiding on lock and showing on unlock).
	 *
	 * @param status TRUE if the FormObject should locked, FALSE
	 * if it should be unlocked.
	 */
	void			SetLocked(BOOL status)
	{
		OP_ASSERT(opwidget);
		opwidget->SetLocked(status);
		m_locked = status;
	}

	/**
	 * The width of the form object in pixels.
	 *
	 * @return The width of the form object's widget.
	 */
	int		Width() { return m_width; }

	/**
	 * The height of the form object in pixels.
	 *
	 * @return the height of the form object's widget.
	 */
	int		Height() { return m_height; }

	/**
	 * Calculates position in document.
	 *
	 * @param doc_pos An out parameter that will contain the position.
	 */
	void			GetPosInDocument(AffinePos* doc_pos);

#ifdef OP_KEY_CONTEXT_MENU_ENABLED
	/**
	 * Used to determine the location of the caret so that a context menu can be shown at its
	 * position.
	 *
	 * @param[out] x The x coordinate of the caret.
	 *
	 * @param[out] y The y coordinate of the caret.
	 *
	 * @returns TRUE if there was a visible caret with a well defined position, FALSE otherwise.
	 */
	BOOL			GetCaretPosInDocument(INT32* x, INT32* y);
#endif // OP_KEY_CONTEXT_MENU_ENABLED

	/**
	 * Returns the rect for the object with CSS borders.
	 *
	 * @return the border.
	 */
	OpRect			GetBorderRect();

	/**
	 * Sets the widget position, calling both SetRect and SetPosInDocument. also sets visual device.
	 * @param vis_dev A VisualDevice
	 */
	void SetWidgetPosition(VisualDevice* vis_dev);

	/**
	 * Displays the form control by painting it.
	 *
	 * @param props Layout properties to use.
	 * @param vis_dev A VisualDevice.
	 *
	 * @return Status.
	 *
	 * @todo Check if the vis_dev parameter is necessary. It gets a
	 * VisualDevice in the constructor.
	 *
	 */
	OP_STATUS		Display(const HTMLayoutProperties& props, VisualDevice* vis_dev);

	/**
	 * Gets the default background color to be used for this form
	 * object.
	 */
	COLORREF		GetDefaultBackgroundColor() const;

	/**
	 * Selects the text in the form control if possible. Does nothing
	 * if it's not possible to select it.
	 */
	void            SelectText();

	/**
	 * Get current selection in the out varibles
	 * or 0, 0 if there is no selection or the selection.
	 * Any code outside of FormValue should call the same
	 * function on FormValue which will take care of special
	 * cases where FormObject doesn't exist.
	 *
	 * @param start_ofs The offset of the first selected character.
	 * @param stop_ofs The offset of the character after the last
	 * selected character. stop_ofs == start_ofs means that no
	 * character is selected.
	 * @param direction The direction of the selection
	 */
	virtual void GetSelection(INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction);

	/**
	 * Get current selection in the out varibles
	 * or 0, 0 if there is no selection or the selection.
	 * Any code outside of FormValue should call the same
	 * function on FormValue which will take care of special
	 * cases where FormObject doesn't exist.
	 *
	 * @param start_ofs The offset of the first selected character.
	 * @param stop_ofs The offset of the character after the last
	 * selected character. stop_ofs == start_ofs means that no
	 * character is selected.
	 */
	void GetSelection(INT32 &start_ofs, INT32 &stop_ofs) { SELECTION_DIRECTION direction; GetSelection(start_ofs, stop_ofs, direction); }

	/**
	 * Set current selection. Values incompatible with the current
	 * text contents will cause variables to be truncated to legal
	 * values before being used.  To clear selection use
	 * ClearSelection() unless you need it to be collapsed at a
	 * particular point.  Any code outside of FormValue should call
	 * the same function on FormValue which will take care of special
	 * cases where FormObject doesn't exist.
	 *
	 * @param start_ofs The offset of the first selected character.
	 * @param stop_ofs The offset of the character after the last selected
	 * character. stop_ofs == start_ofs means that no character is selected.
	 */
	virtual void SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction = SELECTION_DIRECTION_DEFAULT);

	/**
	 * Highlight the search result by selection.
	 * Values incompatible with the current
	 * text contents will cause variables to be truncated to legal
	 * values before being used.  To clear selection use
	 * ClearSelection() unless you need it to be collapsed at a
	 * particular point.  Any code outside of FormValue should call
	 * the same function on FormValue which will take care of special
	 * cases where FormObject doesn't exist.
	 *
	 * @param start_ofs The offset of the first selected character.
	 * @param stop_ofs The offset of the character after the last selected character. stop_ofs == start_ofs means that no character is selected.
	 * @param is_active_hit TRUE if this is the active search result.
	 */
	void SelectSearchHit(INT32 start_ofs, INT32 stop_ofs, BOOL is_active_hit);

	/**
	 * @return TRUE if current selection is a search hit.
	 */
	BOOL IsSearchHitSelected();
	/**
	 * @return TRUE if current selection is an active search hit.
	 */
	BOOL IsActiveSearchHitSelected();

	/**
	 * Removes any visible traces of the selection. There might still be a
	 * collapsed selection somewhere but it will not be visible. This operation
	 * will not affect any scroll positions.
	 */
	void ClearSelection();

	/**
	 * Returns the current position of the caret in the form value.
	 *
	 * Any code outside of FormValue should call the same
	 * function on FormValue which will take care of special
	 * cases where FormObject doesn't exist.
	 *
	 * @returns 0 if the caret is first in the field or
	 * if "caret position" has no meaning for the form value.
	 */
	virtual INT32 GetCaretOffset();

	/**
	 * Sets the current caret position.
	 * Values incompatible with the
	 * current text contents
	 * will cause variables to be truncated to legal values before being used.
	 *
	 * Any code outside of FormValue should call the same
	 * function on FormValue which will take care of special
	 * cases where FormObject doesn't exist.
	 *
	 * @param caret_ofs The number of characters before the caret. 0 is at
	 * the beginning of the field. 1 is after the first character.
	 */
	virtual void SetCaretOffset(INT32 caret_ofs);

	/**
	 * If the FormObject's widget can show partial content (like a textarea too small for its content)
	 * then this function scrolls it to make the active part (caret/selection) visible.
	 *
	 * @param[in] scroll_document_if_needed If the FormObject itself isn't visible and this parameter is TRUE,
	 * try making the FormObject visible as well.
	 */
	void EnsureActivePartVisible(BOOL scroll_document_if_needed) { GetWidget()->ScrollIfNeeded(scroll_document_if_needed); }

	/**
	 * Fills out_value with the value and returns a
	 * OpStatus::OK if it succedes. When
	 * allow_wrap is TRUE the textarea is allowed to insert hardbreaks
	 * if the WRAP attribute is HARD. It should be TRUE when
	 * f.ex. submitting the form content, and FALSE when caching the
	 * value for history or Wand.
	 *
	 * @param[out] out_value An empty OpString to be filled with the widget content.
	 */
	virtual OP_STATUS	GetFormWidgetValue(OpString& out_value, BOOL allow_wrap = TRUE) = 0;

	/**
	 * Set the value of the form control.
	 */
	virtual OP_STATUS SetFormWidgetValue(const uni_char* val) = 0;
	/**
	 * Set the value of the form control.
	 */
	virtual OP_STATUS SetFormWidgetValue(const uni_char * val, BOOL use_undo_stack){ return SetFormWidgetValue(val); }

	/**
	 * Returns the value as a integer (For button, checkbutton and radio)
	 *
	 * @return The value as an integer. Only works for buttons (state),
	 * checkbuttons and radio buttons.
	 */
	INT32			GetIntWidgetValue() { return opwidget->GetValue(); }

	/**
	 * Checks if there are internal tab stops to take care of after the current focused
	 * widget part.
	 *
	 * @param forward - TRUE if we should look forward. FALSE if we should look backwards.
	 *
	 * @return TRUE is focus is in the last available tab stop in the specified direction. Will only
	 * return FALSE if a TAB can be handled internally.
	 *
	 * @todo Remove fallback code when widget is released.
	 */
	BOOL IsAtLastInternalTabStop(BOOL forward)
	{
		return !opwidget || opwidget->IsAtLastInternalTabStop(forward);
	}

	/**
	 * Returns FALSE if there was no later internal tab stop.
	 *
	 * @param forward - TRUE if we should look forward. FALSE if we should look backwards.
	 *
	 * @returns TRUE if something was found and had SetFocus called on itself.
	 */
	BOOL FocusNextInternalTabStop(BOOL forward);

	/**
	 * Focus the last/first internal widget in the potentially nested widget.
	 *
	 * This is used when moving sequentially through the document so that you
	 * enter a widget from the right direction.
	 *
	 * @param[in] front If TRUE, focus the first internal widget, if FALSE, focuse the
	 * last internal widet.
	 */
	void FocusInternalEdge(BOOL front);

	/**
	 * Click it (for radio or checkbox) and when someone is clicking on a label for this element.
	 *
	 * @param thread If script generated, the thread that generated the click, otherwise NULL.
	 */
	virtual void	Click(ES_Thread* thread) {}

	/**
	 * Check it.
	 *
	 * @param radio_elm The radio element.
	 * @param widget The widget.
	 * @param form_obj A form object.
	 * @param send_onchange TRUE if the element should receive an ONCHANGE event (true for user initiated changes, false for scripted changes)
	 * @param thread The ECMAScript thread that caused this to happen. Needed to get events in the correct order.
	 *
	 * @todo Study this strange thing. Why should it have parameters that we already know? It isn't static.
	 */
	void			CheckRadio(HTML_Element *radio_elm, OpWidget* widget);

	/**
	 * Check next radio thing.
	 *
	 * @param forward TRUE if we should move the check forward one
	 * step. FALSE if it should be moved backwards.
	 */
	BOOL			CheckNextRadio(BOOL forward);

	/**
	 * @return TRUE if this is a radio button form object.
	 *
	 * @todo Check if it overlaps with GetInputType.
	 */
	virtual	BOOL	IsRadioButton() { return FALSE; }

	/**
	 * Which type is this?
	 *
	 * @return The type of the control.
	 */
	virtual InputType
					GetInputType() const { return INPUT_NOT_AN_INPUT_OBJECT; }

	/**
	 * Get the visual device.
	 */
	VisualDevice*	GetVisualDevice() { return m_vis_dev; }

	/**
	 * Get the HTML Element.
	 */
	HTML_Element*	GetHTML_Element() { return m_helm; }

	/**
	 * Get the widget. This will never return NULL.
	 */
	OpWidget*		GetWidget() { return opwidget; }

	/**
	 * Get the default element (if any?).
	 */
	HTML_Element*	GetDefaultElement();

	/**
	 * Mark default button.
	 */
	void			UpdateDefButton();

	/**
	 * Reset default button. This function shouldn't be used.
	 * Use the static function with an argument instead. It will not
	 * affect this FormObject, but the one that is currently default.
	 *
	 * @deprecated This is replaced by a static function.
	 */
	DEPRECATED(void ResetDefaultButton());

	/**
	 * Reset default button. This remove the current default button
	 * from that role.
	 */
	static void ResetDefaultButton(FramesDocument* doc);

	/**
	 * Returns TRUE if disabled.
	 */
	BOOL			IsDisabled()		const { return !opwidget->IsEnabled(); }

	/**
	 * Returns TRUE if displayed.
	 */
	BOOL			IsDisplayed()		const { return m_displayed; }

	/**
	 * Set enabled/disabled.
	 *
	 * @param enabled state to set the form object to.
	 * @param regain_focus if TRUE along with 'enabled', have form
	 * object reaquire input focus along with becoming (re-)enabled.
	 */
	void			SetEnabled(BOOL enabled, BOOL regain_focus = FALSE);

#ifdef INTERNAL_SPELLCHECK_SUPPORT
	/**
	 * Creates a spell session id for the widget or 0 if it
	 * isn't possible.
	 *
	 * @param point A point in the field which should be spell checked.
	 */
	int CreateSpellSessionId(OpPoint *point = NULL);
#endif // INTERNAL_SPELLCHECK_SUPPORT

	/**
	 * If the user can edit the widget. Used to control which context menu we present.
	 */
	BOOL IsUserEditableText();

	/**
	 * Set readonly/writeable. Only relevant for textareas and file upload controls.
	 */
	void			SetReadOnly(BOOL readonly);

	/**
	 * Set allow-multiple flag. Only relevant for file upload controls.
	 */
	void			SetMultiple(BOOL multiple);

	/**
	 * Set indeterminate status. Only relevant for checkboxes.
	 */
	void			SetIndeterminate(BOOL indeterminate);

	/**
	 * Set minimum and/or maximum values. Only relevant for type=range.
	 *
	 * @param new_min If non-NULL, the new minimum value for this
	 *        form object.
	 * @param new_max If non-NULL, the new maximum value for this
	 *        form object.
	 */
	void			SetMinMax(double *new_min, double *new_max);

	/**
	 * Initialise the widget.
	 */
	void			InitialiseWidget(const HTMLayoutProperties& props);

	/**
	 * Set that it's no longer displayed.
	 */
	void			SetUndisplayed() { m_displayed = FALSE; }

	/**
	 * Callback. When options collected.
	 */
	void			OnOptionsCollected();

	/**
	 * Updates the background-color, color, font etc.
	 */
	void			UpdateProperties() { m_update_properties = TRUE; }

	/**
	 * Get the document.
	 */
	FramesDocument*		GetDocument() { return m_doc; }

	/**
	 * Updates the position relative to view.
	 */
	void			UpdatePosition();

	/**
	 * Set size of the form control.
	 */
	virtual void	SetSize(int new_width, int new_height);

	/**
	 * Will update the widget with properties, and then calculate the
	 * preferred size (returned in preferred_width and preferred_height).
	 * The preferred size includes padding and formsborder. That means
	 * that if HasSpecifiedBorders is TRUE, the border will not be
	 * included as the widget doesn't have it itself.
	 */
	virtual void	GetPreferredSize(int &preferred_width, int &preferred_height, int html_cols, int html_rows, int html_size, const HTMLayoutProperties& props);

	/**
	 * Try to update the external representation of this text-editable
	 * value to be that of the input type of the given element.
	 * Used when the 'type' of a text-editable input element is dynamically
	 * updated, first attempting to in-place update the underlying
	 * widget into the new type.
	 *
	 * Returns TRUE if the conversion went ahead.
	 */
	virtual	BOOL	ChangeTypeInplace(HTML_Element *he) { return FALSE; }

	// == Hooks ==============================================

#ifndef MOUSELESS
	/**
	 * Called on mouse move.
	 */
	void OnMouseMove(const OpPoint &point);
	/**
	 * Should be called when mouse is pressed.
	 *
	 * @param script_cancelled If the mousedown was cancelled by scripts. That should prevent us from
	 * setting focus but still cause some other things to happen, like opening dropdowns.
	 *
	 * @returns FALSE if the mouse was pressed on a scrollbar.
	 */
	BOOL OnMouseDown(const OpPoint &point, MouseButton button, UINT8 nclicks, BOOL script_cancelled = FALSE);
	/**
	 * Called on mouse up.
	 */
	void OnMouseUp(const OpPoint &point, MouseButton button, UINT8 nclicks = 1);
	/**
	 Call OnMouseWheel on the form objects widget (or a child to it).
	 @param point the mouse position (in document coordinates)
	 @param delta the mouse wheel delta
	 @param vertical if TRUE, the mouse wheel delta is to be interpreted as vertical
	 @return TRUE if the wheel event was handled or if it typically handle scroll events (even though it didn't this time).
	 */
	BOOL OnMouseWheel(const OpPoint &point, INT32 delta, BOOL vertical);

	/**
	 * Called when cursor is positioned.
	 */
	void OnSetCursor(const OpPoint &point);

#endif // !MOUSELESS

	// Implementing OpInputContext interface
	/**
	 * Called when an input action happens.
	 */
	virtual BOOL			OnInputAction(OpInputAction* action);

	/**
	 * The name of the input context.
	 */
	virtual const char*		GetInputContextName() { return "Form"; }

	/**
	 * Set focus.
	 */
	virtual void			SetFocus(FOCUS_REASON reason);
	/**
	 * Called when focus is gained or lost.
	 */
	virtual	void			OnFocus(BOOL focus, FOCUS_REASON reason);

	/**
	 * Called when keyboard focus is moved to this object.
	 */
	virtual void			OnKeyboardInputGained(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

	/**
	 * Called when keyboard focus is moved away from this object.
	 */
	virtual void			OnKeyboardInputLost(OpInputContext* new_input_context, OpInputContext* old_input_context, FOCUS_REASON reason);

	/**
	 * Focus gained.
	 */
	void HandleFocusGained(FOCUS_REASON reason);
	/**
	 * Focus lost.
	 */
	void HandleFocusLost(FOCUS_REASON reason);


	/**
	 * Is the widget in RTL mode?
	 *
	 * @returns TRUE if the UI widget is in RTL (right to left) mode.
	 */
	BOOL IsInRTLMode() { return opwidget->GetRTL(); }

#ifdef SELFTEST
	/**
	 * Sets that the value in this FormValue was input in RTL (right-to-left) mode.
	 * The default is FALSE (ltr).
	 *
	 * @param set_to_rtl TRUE if RTL, FALSE if LTR.
	 */
	void SetInRTLMode(BOOL set_to_rtl) { opwidget->SetRTL(set_to_rtl); }
#endif // SELFTEST



	/**
	 * Inform the formobject/widget of what kind of borders to use.
	 * This is needed to prevent the widget from drawing an extra
	 * layer of borders and for it to be able to calculate its
	 * size accurately.
	 *
	 * @param props The CSS for this object.
	 */
	void SetCssBorders(const HTMLayoutProperties& props);

	/**
	 * Checks if the author has specified a border that should be
	 * drawn with CSS borders (or not drawn at all). When this
	 * returns FALSE we draw borders with the default skinnable
	 * widget borders.
	 *
	 * @param props The resolved CSS properties for the element.
	 *
	 * @param helm The HTML_Element that the CSS is placed on.
	 */
	static BOOL	HasSpecifiedBorders(const HTMLayoutProperties& props,
									HTML_Element* helm);

	/**
	 * Checks if the author has specified a background image that
	 * should be drawn.
	 *
	 * @param frm_doc The document this html element belongs to.
	 * @param props The resolved CSS properties for the element.
	 * @param helm The HTML_Element that the CSS is placed on.
	 */
	static BOOL HasSpecifiedBackgroundImage(FramesDocument* frm_doc,
											const HTMLayoutProperties& props,
											HTML_Element* helm);

	/**
	 * Stores a validation error window in the FormObject for tracking
	 * while displaying.
	 *
	 * @param error The window to set.
	 */
	void SetValidationError(ValidationErrorWindow* error) { m_validation_error = error; }

	/**
	 * Returns a window set with SetValidationError
	 *
	 * @returns The window or NULL if no window was set.
	 */
	ValidationErrorWindow* GetValidationError() { return m_validation_error; }

#ifdef WIDGETS_IME_SUPPORT
	/**
	 * Set the IME style of an OpEdit from the ISTYLE attribute or the
	 * input-format css property
	 */
	void UpdateIMStyle();

	void HandleFocusEventFinished();
#endif // WIDGETS_IME_SUPPORT

	/**
	 * Returns the element that is most relevant to mouse events at a
	 * certain position. Is most likely the formobject's own element
	 * but might be a HE_OPTION for instance.
	 *
	 * @returns an element, never NULL.
	 */
	HTML_Element* GetInnermostElement(int document_x, int document_y);

	/**
	 * Returns TRUE if the given input type has a textfield widget
	 * as external representation.
	 *
	 */
	static BOOL IsEditFieldType(InputType type);

public:
	/**
	 * The CSS border left width. This shouldn't be public and should
	 * have a m_ prefix.
	 */
	int css_border_left_width;

	/**
	 * The CSS border top width.
	 */
	int css_border_top_width;

	/**
	 * The CSS border rigth width.
	 */
	int css_border_right_width;

	/**
	 * The CSS border bottom width.
	 */
	int css_border_bottom_width;

protected:

	/**
	 * Sets widget properties by checking what the HTML says.
	 *
	 * @param set_start_value If it is allowed to change the value to some default value.
	 */
	virtual OP_STATUS ConfigureWidgetFromHTML(BOOL set_start_value);

	/**
	 * Helper that converts from document coordinates to coordinates relative to the widget
	 */
	static OpPoint ToWidgetCoords(OpWidget* widget, const OpPoint& doc_point);

private:
#ifdef WAND_HAS_MATCHES_WARNING
	/**
	 * Sometimes we don't want to let the user enter a
	 * text field until we have informed the user
	 * about the availability of wand data. This is
	 * the case for instance in the BREW version.
	 * In those cases this method will return TRUE
	 * and display a warning. Otherwise it
	 * will return FALSE and do nothing.
	 *
	 * @returns TRUE if the method displayed/initiated a warning.
	 */
	BOOL WarnIfWandHasData();
#endif // WAND_HAS_MATCHES_WARNING

	/**
	 * Paint borders special to this form object. F.ex border that indicates that the object
	 * has wand data.
	 *
	 * @param props Layout properties to use.
	 * @param vis_dev A VisualDevice.
	 */
	void PaintSpecialBorders(const HTMLayoutProperties& props, VisualDevice* vis_dev);
};


/**
 * Form object representing an input object.
 */
class InputObject : public FormObject
{
private:
	/**
	 * Constructor.
	 */
	InputObject(VisualDevice* vd,
				FramesDocument* doc,
				InputType type,
				HTML_Element* he,
				BOOL read_only);

	/**
	 * Second part of the constructor (check boxes and radio things). This may fail.
	 */
	OP_STATUS Construct(VisualDevice* vd,
						const HTMLayoutProperties& props,
						InputType type,
						HTML_Element* he);

	/**
	 * Second part of the constructor. This may fail.
	 */
	OP_STATUS Construct(VisualDevice* vd,
						const HTMLayoutProperties& props,
			InputType type,
						int maxlength,
						HTML_Element* he,
						const uni_char* label);

	/**
	 * The type of the object.
	 */
    InputType		object_type;

	/**
	 * Some flags.
	 */
	/**
	 * If it is checked.
	 */
	unsigned int m_is_checked:1;
	/**
	 * If it is checked by default.
	 */
	unsigned int m_is_checked_by_default:1;

	/**
	 * The default text of the control.
	 */
	uni_char*		m_default_text;
public:

	/**
	 * The way to construct objects. This takes care of the steps
	 * needed. This is for check boxes and radio things.
	 */
	static InputObject* Create(VisualDevice* vd,
								const HTMLayoutProperties& props,
								FramesDocument* doc,
								InputType type,
								HTML_Element* he,
								BOOL read_only);

	/**
	 * The way to construct objects. This takes care of the steps
	 * needed.
	 */
	static InputObject* Create(VisualDevice *vd,
								const HTMLayoutProperties& props,
								FramesDocument* doc,
				InputType type,
								HTML_Element* he,
				int maxlength,
								BOOL read_only,
								const uni_char* label);

	virtual OP_STATUS GetFormWidgetValue(OpString& out_value, BOOL allow_wrap = TRUE);
	virtual OP_STATUS SetFormWidgetValue(const uni_char* val);
	virtual OP_STATUS SetFormWidgetValue(const uni_char* val, BOOL use_undo_stack);

	virtual void	Click(ES_Thread* thread);

	/**
	 * @returns TRUE if this check box/radio button is checked by default.
	 */
	BOOL	GetDefaultCheck() const;
	/**
	 * Sets the value read by GetDefaultCheck.
	 * @param is_checked_default The new value.
	 * @returns TRUE if this check box/radio button is checked by default.
	 */
	void	SetDefaultCheck(BOOL is_checked_default);
	virtual BOOL	IsRadioButton() { return object_type == INPUT_RADIO; }

	/**
	 * The input type.
	 */
	virtual InputType GetInputType() const { return object_type; }

	// See base class documentation.
	virtual BOOL ChangeTypeInplace(HTML_Element *he);

protected:
	virtual OP_STATUS ConfigureWidgetFromHTML(BOOL set_start_value);

	/**
	 * Configure the underlying text edit widget as needed to
	 * to hold an input element corresponding to the given element.
	 *
	 * @param he The element with the text-editable input element.
	 * @param edit The edit widget to configure.
	 * @param placeholder If non-NULL, the placeholder text
	 *        to set.
	 */
	static void ConfigureEditWidgetFromHTML(HTML_Element *he, OpEdit *edit, const uni_char *placeholder);
};

/**
 * This object represents a select.
 */
class SelectionObject : public FormObject, private OpTimerListener
{
private:
	/**
	 * Number of visible elements.
	 */
	int				m_page_requested_size;
	/**
	 * Multi select control.
	 */
	BOOL			m_multi_selection;
	/**
	 * Min width in pixels.
	 */
	int				m_min_width;
	/**
	 * Max width in pixels.
	 */
	int				m_max_width;
	/**
	 * Cached value to make it possible to add elements outside of the layout engine.
	 */
	BOOL			m_last_set_need_intrinsic_width;
	/**
	 * Cached value to make it possible to add elements outside of the layout engine.
	 */
	BOOL			m_last_set_need_intrinsic_height;

	/**
	 * The timer used for asynchronous rebuild of the FormValueList.
	 * @see ScheduleFormValueRebuild()
	 */
	OpTimer			m_form_value_rebuild_timer;

	/**
	 * TRUE if a rebuild timer has been started but hasn't fired.
	 * @see ScheduleFormValueRebuild()
	 */
	BOOL			m_form_value_rebuild_timer_active;

	/**
	 * Init with layout information.
	 *
	 * @param props Layout information
	 * @param doc Used to look up site
	 *     specific layout preferences.
	 */
	void			Init(const HTMLayoutProperties& props, FramesDocument* doc);

	/**
	 * Constructor. Private.
	 *
	 * @param vd The visual device.
	 * @param doc The doc this form object belongs to
	 * @param multi If it's a listbox (TRUE) or a dropdown (FALSE).
	 */
	SelectionObject(VisualDevice* vd,
					FramesDocument* doc,
					BOOL multi,
					int size,
					int min_width,
					int max_width,
					HTML_Element* he);
public:
	~SelectionObject()
	{
		m_form_value_rebuild_timer.Stop();
	}


	/**
	 * Constructor. The right way to get a SelectionObject.
	 *
	 * @param vd The visual device.
	 * @param props The current look of this form thing.
	 * @param doc The doc this form object belongs to
	 * @param multi If it's a listbox (TRUE) or a dropdown (FALSE).
	 * @param size The expected size.
	 * @param form_width The width
	 * @param form_height The height
	 * @param min_width The minimum allowed width
	 * @param max_width The maximum allowed width
	 * @param he The HTML Element thing this is connected to.
	 * @param use_default_font
	 * @param created_selection_object Out parameter that will contain
	 * the constructed object if the method returns OpStatus::OK
	 *
	 */
	static OP_STATUS ConstructSelectionObject(VisualDevice* vd,
								 const HTMLayoutProperties& props,
								 FramesDocument* doc,
				 BOOL multi,
				 int size,
								 int form_width,
								 int form_height,
								 int min_width,
								 int max_width,
				 HTML_Element* he,
				 BOOL use_default_font,
								 FormObject*& created_selection_object);
	/**
	 * Constructor.
	 */
	OP_STATUS ConstructInternals(const HTMLayoutProperties& props,
								 int form_width,
								 int form_height,
								 BOOL use_default_font);

	/**
	 * Add a new element.
	 */
	OP_STATUS		AddElement(const uni_char* txt, BOOL selected, BOOL disabled, int idx = -1);

	/**
	 * Apply layout properties to an option element.
	 *
	 * @param[in] idx The index (zero-based) of the option. Has to exist in the SelectionObject.
	 *
	 * @param[in] props The CSS properties to apply to the option.
	 */
	void			ApplyProps(int idx, class LayoutProperties* props);

	/**
	 * Change an option element, giving it a new text.
	 */
	OP_STATUS		ChangeElement(const uni_char* txt, BOOL selected, BOOL disabled, int idx);

	/**
	 * Remove an element.
	 */
	void			RemoveElement(int idx);

	/**
	 * Remove all elements.
	 */
	void			RemoveAll();

	/**
	 * Begin an option group.
	 *
	 * @param[in] The HE_OPTGROUP element.
	 *
	 * @param[in] props The style for the option. If this is not NULL, then elm
	 * can be NULL. If this is NULL, then we will use elm to get a rough estimate.
	 */
	OP_STATUS		BeginGroup(HTML_Element* elm, const HTMLayoutProperties* props = NULL);
	/**
	 * End an option group.
	 *
	 * @param[in] The HE_OPTGROUP element.
	 *
	 */
	void			EndGroup(HTML_Element* elm);

	/**
	 * Remove an option group.
	 */
	void			RemoveGroup(int idx);

	/**
	 * Remove all option groups.
	 */
	void			RemoveAllGroups();

	/**
	 * Returns TRUE if the element at the index
	 * (counting all elements, including starts
	 * and stops) is a group stop.
	 */
	BOOL			ElementAtRealIndexIsOptGroupStop(unsigned int idx);

	/**
	 * Returns TRUE if the element at the index
	 * (counting all elements, including starts
	 * and stops) is a group start.
	 */
	BOOL			ElementAtRealIndexIsOptGroupStart(unsigned int idx);

	/**
	 * Changes the multi attribute. If multi is TRUE and it is a
	 * dropdown, it will switch to a listbox.  If it is a listbox and
	 * multi is FALSE and size is 1, it switches to a dropdown
	 */
//	OP_STATUS		ChangeMulti(BOOL multi);

	/**
	 * Overridden GetValue.
	 */
	virtual OP_STATUS	GetFormWidgetValue(OpString& out_value, BOOL allow_wrap = TRUE);

	/**
	 * Overridden SetValue.
	 */
	virtual OP_STATUS SetFormWidgetValue(const uni_char * szValue);

	/**
	 * Dispatches click event when clicking on a label for this element.
	 */
	virtual void	Click(ES_Thread* thread);

	/**
	 * Returns TRUE if the specified element is selected.
	 */
	BOOL			IsSelected(int idx);
	/**
	 * Returns TRUE if the specified element is disabled.
	 */
    BOOL			IsDisabled(int idx);
	/**
	 * Needed so that the base class' IsDisabled becomes reachable.
	 */
	BOOL            IsDisabled() { return FormObject::IsDisabled(); }

	/**
	 * TRUE if multi selection control.
	 */
	BOOL			IsMultiSelection() { return m_multi_selection; }

	/**
	 * TRUE if it's a list box.
	 */
	BOOL			IsListbox()
	{
#ifdef FORMS_SELECT_IGNORE_SIZE // tweak used by Opera Mini
		return m_multi_selection;
#else
		return m_page_requested_size > 1 || m_multi_selection;
#endif // FORMS_SELECT_IGNORE_SIZE
	}

	/**
	 * Select or unselect element.
	 */
    void			SetElement(int i, BOOL selected);
	/**
	 * Get count of elements. It doesn't include group start and group end elements.
	 */
	int				GetElementCount();

	/**
	 * Get count of elements. This does include group start and group end elements.
	 */
	unsigned int	GetFullElementCount();

	/**
	 * If it's a listbox and the selected element is outside of the
	 * currently visible are, it will be scrolled to display the
	 * selected element.
	 */
	void			ScrollIfNeeded();

	/**
	 * Get selected index (single selection).
	 */
	int				GetSelectedIndex();
	/**
	 * Set selected index (single selection).
	 */
	void			SetSelectedIndex(int index);

	/**
	 * Start add element.
	 */
	void			StartAddElement() {}
	/**
	 * End adding elements.
	 *
	 * @return The preferred width of the widget, or
	 * the current width if need_intrinsic_width is FALSE.
	 */
    INT32			EndAddElement(BOOL need_intrinsic_width, BOOL need_intrinsic_height);
	/**
	 * End adding elements.
	 *
	 * @return The preferred width of the widget, or the current width if
	 * need_intrinsic_width was FALSE in the last call to EndAddElement(BOOL, BOOL).
	 */
    INT32			EndAddElement();

	/**
	 * Recalculate the size and update the layout
	 */
	void			ChangeSizeIfNeeded();

	/**
	 * Tells the widget to recalculate their width.
	 */
	void            RecalculateWidestItem();

	/**
	 * The number of options ([start, stop[) that need be updated when
	 * SelectContent::Paint is called.  This is so that styles are
	 * applied to all relevant option elements in a select.  For a
	 * list box, only the options overlapped by area need be updated,
	 * since drawing always comes from layout. for a closed dropdown,
	 * only the selected option need be updated. for an open dropdown,
	 * all options need be updated, since drawing typically doesn't
	 * come from layout.
	 *
	 * @param[in] area The area in document coordinates.
	 *
	 * @param[out] start The index (zero-based) of the first option
	 * affected by a paint in the area.
	 *
	 * @param[out] stop The option after the last affected option. The
	 * same as start for an empty range.
	 */
	void            GetRelevantOptionRange(const OpRect& area, UINT32& start, UINT32& stop);

	/**
	 * Set min and max width.
	 */
	void			SetMinMaxWidth(int min_width, int max_width) { m_min_width = min_width; m_max_width = max_width; }

	/**
	 * Returns the scrollbar positions, or 0 if none.
	 */
	int GetWidgetScrollPosition();

	/**
	 * Sets the widget scroll positions. The values should come from an
	 * earlier call to GetWidgetScrollPosition.
	 */
	void SetWidgetScrollPosition(int scroll);

	/**
	 * Returns the option (HE_OPTION) at the document coordinates or
	 * NULL if no such option could be found. Is only relevant and may
	 * only be called if IsListBox returns TRUE.
	 */
	HTML_Element* GetOptionAtPos(int document_x, int document_y);

	/**
	 * Locks/unlocks the underlying widget's update.
	 * The behavior when locked may be different depending on widget's type e.g.
	 * for OpDropDown when it's locked when opened no changes are propagated
	 * to its internal OpListBox so it behaves as if it was closed.
	 */
	void LockWidgetUpdate(BOOL lock);

	/**
	 * Forces an update of the underlying widget e.g. widget's geometry,
	 * a position of the scrollbar, etc. The behavior may be different depending on widget's type.
	 * Note that the widget may refuse to update when locked.
	 * @see LockWidgetUpdate().
	 */
	void UpdateWidget();

	/**
	 * Schedules an asynchronous rebuild of the FormValueList.
	 * @see FormaValueList::Rebuild().
	 */
	void ScheduleFormValueRebuild();

	// OpTimerListener's interface.
	void OnTimeOut(OpTimer*);

	/**
	 * Returns TRUE if an async rebuild of the widget tree has been
	 * scheduled, but hasn't yet started.
	 */
	BOOL HasAsyncRebuildScheduled() { return m_form_value_rebuild_timer_active; }

protected:
	/**
	 * Sets widget properties by checking what the HTML says.
	 *
	 * @param set_start_value If it is allowed to change the value to some default value.
	 */
	virtual OP_STATUS ConfigureWidgetFromHTML(BOOL set_start_value);
};

/**
 * Form object representing a text area.
 */
class TextAreaObject : public FormObject
{
	/**
	 * Private constructor. Use the ConstructTextAreaObject method.
	 */
	TextAreaObject(VisualDevice* vd,
				   FramesDocument* doc,
				   int xsize,
				   int ysize,
				   BOOL read_only,
				   HTML_Element* he);

public:
	/**
	 * Constructor. The right way to get a TextAreaObject.
	 *
	 * @param vd The visual device.
	 * @param props The current look of this form thing.
	 * @param doc The doc this form object belongs to
	 * @param xsize The expected size.
	 * @param ysize The expected size.
	 * @param read_only Set to TRUE to get a read only control.
	 * @param value The text to put in the text area.
	 * @param form_width The width
	 * @param form_height The height
	 * @param he The HTML Element thing this is connected to.
	 * @param use_default_font
	 * @param created_textarea_object Out parameter that will contain
	 * the constructed object if the method returns OpStatus::OK
	 *
	 */
	static OP_STATUS ConstructTextAreaObject(VisualDevice* vd,
											 const HTMLayoutProperties& props,
											 FramesDocument* doc,
											 int xsize,
											 int ysize,
											 BOOL read_only,
											 const uni_char* value,
											 int form_width,
											 int form_height,
											 HTML_Element* he,
											 BOOL use_default_font,
											 FormObject*& created_textarea_object);

	/**
	 * The input type of the text area (INPUT_TEXTAREA).
	 */
	virtual InputType GetInputType() const { return INPUT_TEXTAREA; }

	/**
	 * Overridden GetValue.
	 */
	virtual OP_STATUS	GetFormWidgetValue(OpString& out_value, BOOL allow_wrap = TRUE);

	/**
	 * Overridden SetValue.
	 * @param szValue text to set
	 * @param use_undo_stack whether the undo buffer
	 *        should be used to store the previous value
	 *        so this SetText call is undoable
	 */
	virtual OP_STATUS SetFormWidgetValue(const uni_char * szValue);
	virtual OP_STATUS SetFormWidgetValue(const uni_char * szValue, BOOL use_undo_stack);

	// See base class documentation
	virtual void	Click(ES_Thread* thread);

	/**
	 * The number of rows.
	 */
	int				GetRows() const { return m_rows; }
	/**
	 * the number of cols.
	 */
	int				GetCols() const { return m_cols; }
	/**
	 * Get preferred height.
	 */
	int				GetPreferredHeight() const;

	/**
	 * This is used to make sure that we don't drop a document
	 * with user changes from the cache. This was added in core-1.
	 * Make sure the code calling this code is also merged from core-2.
	 */
	void            SetIsUserModified() { m_is_user_modified = TRUE; }

	/**
	 * This is used to make sure that we don't drop a document
	 * with user changes from the cache. This was added in core-1.
	 * Make sure the code calling this code is also merged from core-2.
	 */
	BOOL            IsUserModified() { return m_is_user_modified; }

	/**
	 * Returns the scrollbar positions, or 0 if none.
	 */
	void GetWidgetScrollPosition(int& out_scroll_x, int& out_scroll_y);

	/**
	 * Sets the widget scroll positions. The values should come from an
	 * earlier call to GetWidgetScrollPosition.
	 */
	void SetWidgetScrollPosition(int scroll_x, int scroll_y);

	/**
	 * Returns the size of the inner area in the widget. This
	 * changes when the user edits the text. Used by DOM code
	 * so that scripts can dynamically resize the textarea.
	 *
	 * @param[out] out_width The scrollable width of the widget
	 *
	 * @param[out] out_height The scrollable height of the widget
	 */
	void GetScrollableSize(int& out_width, int& out_height);

	// see base class documentation.
	virtual void GetSelection(INT32& start_ofs, INT32& stop_ofs, SELECTION_DIRECTION& direction);

	// see base class documentation.
	virtual void SetSelection(INT32 start_ofs, INT32 stop_ofs, SELECTION_DIRECTION direction = SELECTION_DIRECTION_DEFAULT);

	// see base class documentation.
	virtual INT32 GetCaretOffset();

	// see base class documentation.
	virtual void SetCaretOffset(INT32 caret_ofs);

protected:
	/**
	 * Sets widget properties by checking what the HTML says.
	 *
	 * @param set_start_value If it is allowed to change the value to some default value.
	 */
	virtual OP_STATUS ConfigureWidgetFromHTML(BOOL set_start_value);

private:
	/**
	 * Helper method.
	 *
	 * @param props Layout properties.
	 *
	 * @param form_width width.
	 *
	 * @param form_height height.
	 *
	 * @param use_default_font TRUE if a default font should be used.
	 */
	OP_STATUS ConstructInternals(const HTMLayoutProperties& props,
								 const uni_char* value,
								 int form_width,
								 int form_height,
								 BOOL use_default_font);

	/**
	 * count of rows.
	 */
    int			m_rows;
	/**
	 * Count of cols.
	 */
	int				m_cols;

	/**
	 * If the user has modified the text. We'll use this to prevent ourselves from
	 * dropping documents with user changed data from the cache.
	 */
	BOOL m_is_user_modified;
	/**
	 * Set text.
	 *
	 * @param value the text to set
	 * @param use_undo_stack whether the undo buffer
	 *        should be used to store the previous value
	 *        so this SetText call is undoable
	 */
    OP_STATUS		SetText(const uni_char* value, BOOL use_undo_stack = FALSE);
};

/**
 * Form object for a file upload field.
 */
class FileUploadObject : public FormObject
{
public:
	/**
	 * Private constructor. Use the ConstructFileUploadObject method.
	 */
	FileUploadObject(VisualDevice* vd,
					 FramesDocument* doc,
					 BOOL read_only,
					 HTML_Element* he);

	/**
	 * Constructor. The right way to get a FileUploadObject
	 *
	 * @param vd The visual device.
	 * @param props The current look of this form thing.
	 * @param doc The doc this form object belongs to
	 * @param read_only If it should be read only or not.
	 * @param szValue The value to put in the newly created file upload.
	 * @param he The HTML Element thing this is connected to.
	 * @param use_default_font
	 * @param created_file_upload_object Out parameter that will contain
	 * the constructed object if the method returns OpStatus::OK
	 *
	 */
	static OP_STATUS ConstructFileUploadObject(VisualDevice* vd,
											   const HTMLayoutProperties& props,
											   FramesDocument* doc,
											   BOOL read_only,
											   const uni_char* szValue,
											   HTML_Element* he,
											   BOOL use_default_font,
											   FormObject*& created_file_upload_object);

	OP_STATUS	ConstructInternals(const HTMLayoutProperties& props,
				   const uni_char* szValue,
				   BOOL use_default_font);

	/** Symbolic value representing the maximum number of files that
	    the underlying upload file widget will allow selection of.

	    The widget currently only checks to see if the value is greater
	    than 1, so any value higher than that suffices to provide multiple
	    file selection. */
	enum { MaxNumberOfUploadFiles = 10000 };

	/**
	 * Overridden GetValue.
	 */
	virtual OP_STATUS	GetFormWidgetValue(OpString& out_value, BOOL allow_wrap = TRUE);

	/**
	 * Overridden SetValue.
	 */
	virtual OP_STATUS	SetFormWidgetValue(const uni_char * szValue);

	/**
	 * Overridden GetInputType, returning INPUT_FILE.
	 */
	virtual InputType	GetInputType() const { return INPUT_FILE; }

	/**
	 * Overridden Click.
	 *
	 * @param thread If script generated, the thread that generated the click, otherwise NULL.
	 */
	virtual void		Click(ES_Thread* thread);
};

#endif // PIFORMS_H
