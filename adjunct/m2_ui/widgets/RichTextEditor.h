// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
/** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file			 RichTextEditor.h
 * @author Owner:    Alexander Remen (alexr)
 */

#ifndef RICHTEXTEDITOR_H
#define RICHTEXTEDITOR_H

#include "adjunct/desktop_pi/DesktopColorChooser.h"
#include "adjunct/desktop_pi/DesktopFontChooser.h"
#include "adjunct/m2_ui/util/ComposeFontList.h"

#include "adjunct/m2_ui/widgets/RichTextEditorListener.h"
#include "modules/documentedit/OpDocumentEdit.h"

#include "adjunct/quick_toolkit/widgets/QuickOpWidgetWrapper.h"

class OpToolbar;
class OpBrowserView;
class DesktopWindow;
class DesktopFileChooser;
class Upload_Multipart;
class HTML_Element;


class RichTextEditor : public OpWidget
					 , public OpColorChooserListener
					 , public OpDocumentEditListener
					 , public OpFontChooserListener
					 , public OpLoadingListener
					 , public OpMailClientListener
{
public:

									/** Public static constructor 
									 */
		static OP_STATUS			Construct(RichTextEditor** obj);

									/** Initialize the rich text editor
									 * @param Parent DesktopWindow*
									 * @param is_HTML - whether the content is HTML or plain text (when starting)
									 * @param RichTextEditorListener
									 * @param message_id - for creating unique URLs
									 */
		OP_STATUS					Init(DesktopWindow* parent_window, BOOL is_HTML, RichTextEditorListener* listener, UINT32 message_id);

									/** Initializes the font dropdown in the richt text editor
									 *  call this when the toolbar has been reset, otherwise it will be empty
									 */
		OP_STATUS					InitFontDropdown();

									// OpInputContext
		virtual BOOL				OnInputAction(OpInputAction* action);
		virtual const char*			GetInputContextName() {return "Rich Text Editor";}
		
		virtual Type				GetType() {return WIDGET_TYPE_RICH_TEXT_EDITOR;}
		
									//OpColorChooserListener
		virtual void				OnColorSelected(COLORREF color);

									//OpDocumentEditListener
		virtual void				OnCaretMoved() { g_input_manager->UpdateAllInputStates(); UpdateFontDropdownSelection(); }
		virtual void				OnTextChanged() { m_listener->OnTextChanged(); }
		virtual void				OnTextDirectionChanged (BOOL to_rtl) { m_is_rtl = to_rtl; if (m_listener) m_listener->OnTextDirectionChanged(to_rtl);}

									// OpMailClientListener
		virtual	void				GetExternalEmbedPermission(OpWindowCommander* commander, BOOL& has_permission) { has_permission = TRUE; }
		virtual void				OnExternalEmbendBlocked(OpWindowCommander* commander) {}


									//DesktopFontChooser
		virtual void				OnFontSelected(FontAtt& font, COLORREF color);

									/** Gets whether the current formatting is plain text or HTML
									 * @return TRUE if HTML formatting, FALSE if plaintext
									 */
		BOOL						IsHTML() const { return m_is_HTML_message; }
		
									/** Get the multipart/related of the current document in the rich text editor
									 * @param - returns the multipart related with the HTML part
									 * @param - the charset you want to use
									 */
		OP_STATUS					GetEncodedHTMLPart(OpAutoPtr<Upload_Multipart>& html_multipart,  const OpStringC8 &charset_to_use);
									
									/** Get the HTML source of the current document in the rich text editor
									 * @param content - returns the HTML source code for the 
									 * @param body_only - whether to include the head section or not
									 */
		OP_STATUS					GetHTMLSource(OpString &content, BOOL body_only = FALSE) const;
									
									/** Get the plain text equivalent of the current document in the rich text editor
									 * @param equivalent - returns the plain text equivalent 
									 */
		OP_STATUS					GetTextEquivalent(OpString& equivalent) const;

									/** Insert some HTML in the documentedit
									 * @param source_code - the raw HTML to insert into the documentedit
									 */
		void						InsertHTML(OpString& source_code);

									/** Insert a link in the documentedit
									 * @param title - the text that should be linkified
									 * @param address - the URL address
									 */
		void						InsertLink(OpString& title, OpString& address);

									/** Insert an image to the documentedit
									 * @param image_url - the url to the image to insert into the documentedit
									 */
		void						InsertImage(OpString& image_url);

									/** Sets the focus to the documentedit
									 */
		void						SetFocusToBody();
									
									/** Sets the content in the documentedit and updates the view 
									 * @param content - the content to start with in the rich text editor
									 * @param body_only - whether this content is only the body or has some styling in the head section
									 * @param context_id - if there are images, you need to specify the context_id of the images
									 */
		OP_STATUS					SetMailContentAndUpdateView(const OpStringC &content, BOOL body_only, URL_CONTEXT_ID context_id = 0);
									
									/** Load some plaintext in the documentedit
									 * @param body - the plain text to load
									 */
		OP_STATUS					SetPlainTextBody(const OpStringC& body);
									
									/** Set the text direction in the rich text editor
									 * @param direction - AccountTypes::DIR_AUTOMATIC, DIR_LEFT_TO_RIGHT or DIR_RIGHT_TO_LEFT
									 * @param update_body - whether to update the window or not (eg. not necessary if updated a bit later)
									 * @param force_keep_auto_detecting - changing the direction would normally turn off autodetection, this overrides that
									 */
		void						SetDirection(int direction, BOOL update_body, BOOL force_keep_auto_detecting = FALSE);

									/** Append the signature to the text. If this function has already been called 
									  * - ie. the signature is already been set, it will try to find it first in the logical tree and replace it
									  * @param signature in HTML or plain text
									  * @param is_HTML_signature whether the signature is in HTML or not
									  * @param update_view should be FALSE if you call SetMailContentAndUpdateView right after with append_signature = TRUE
									  */
		OP_STATUS					SetSignature(const OpStringC signature, BOOL is_HTML_signature, BOOL update_view = TRUE);

									/** Toggle between HTML and plain text
									  * @param update_view whether we should update the view or it will be done shortly after by something else
									  */
		void						SwitchHTML(BOOL update_view = 1);

									
									/** The rich text editor will append m_signature the next time it updates the content
									  */
		void						UpdateSignature() { m_update_signature = TRUE; }
									
									/** OnLayout
									 */
		virtual void				OnLayout();

									/** To make DesktopWindowSpy::GetTargetBrowserView work
									 */
		OpBrowserView*				GetBrowserView() {return m_HTML_compose_edit;}

									/** Changes the skin to match dialog skin, and forces the formatting toolbar to be visible all the time
									 */
		void						SetEmbeddedInDialog();

									/** Returns the preferred width of this widget which is when the HTML formatting toolbar doesn't wrap
									  */
		unsigned					GetPreferredWidth();

									/** Functions that implements OpLoadingListener 
									 *	Only used to do stuff when the first reflow is done in OnLoadingFinished()
									 */
		virtual void				OnLoadingFinished(OpWindowCommander* commander, LoadingFinishStatus status);
		virtual void				OnUrlChanged(OpWindowCommander* commander, URL& url) {} // NOTE: This will be removed from OpLoadingListener soon (CORE-35579), don't add anything here!
		virtual void				OnUrlChanged(OpWindowCommander* commander, const uni_char* url) {}
		virtual void				OnStartLoading(OpWindowCommander* commander) {}
		virtual void				OnLoadingProgress(OpWindowCommander* commander, const LoadingInformation* info) {}
		virtual void				OnAuthenticationRequired(OpWindowCommander* commander, OpAuthenticationCallback* callback) {}
		virtual void				OnAuthenticationCancelled(OpWindowCommander* commander, URL_ID authid) {}
		virtual void				OnStartUploading(OpWindowCommander* commander) {}
		virtual void				OnUploadingFinished(OpWindowCommander* commander, LoadingFinishStatus status) {}
		virtual void				OnLoadingCreated(OpWindowCommander* commander) {}
		virtual void				OnUndisplay(OpWindowCommander* commander) {}
		virtual BOOL				OnLoadingFailed(OpWindowCommander* commander, int msg_id, const uni_char* url) { return FALSE; }
	    virtual void                OnXmlParseFailure(OpWindowCommander*) {}
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
		virtual void 				OnSearchSuggestionsRequested(OpWindowCommander* commander, const uni_char* url, OpSearchSuggestionsCallback* callback) {}
#endif
private:									

									/** Sets the mail content to a URL, after having created valid HTML
									 * @param content - the mail content to use (has to be plain text if m_is_HTML_message is FALSE)
									 * @param url - the url where the content will be written to
									 * @param body_only - if the function should add a <head> and a <style> section or not
									 * @param charset - the charset to use when writing the 8bit content
									 */
		OP_STATUS					SetMailContentToURL(const OpStringC &content, URL& url, BOOL body_only, const OpStringC8 &charset);

		OP_STATUS					GetDefaultFont(OpString& default_font, INT32 &font_size) const;

		OP_STATUS					UpdateFontDropdownSelection();

		// Private constructor
		RichTextEditor()
		: m_parent_window(NULL)
		, m_is_HTML_message(FALSE)
		, m_update_signature(FALSE)
		, m_listener(NULL)
		, m_HTML_formatting_toolbar(NULL)
		, m_HTML_compose_edit(NULL)
		, m_draft_number(0)
		, m_message_id(0)
		, m_context_id(0)
		, m_document_edit(NULL)	
		, m_is_rtl(FALSE)
		, m_autodetect_direction(TRUE)
		{};

		DesktopWindow*				m_parent_window;
		BOOL						m_is_HTML_message;

		OpString					m_signature;
		BOOL						m_update_signature;

		RichTextEditorListener*		m_listener;
		OpToolbar*					m_HTML_formatting_toolbar;
		OpBrowserView*				m_HTML_compose_edit;

		ComposeFontList				m_composefont_list;

		// these 2 are required to make unique draft urls:
		UINT32						m_draft_number;
		UINT32						m_message_id;
		
		URL_CONTEXT_ID				m_context_id;
		OpDocumentEdit*             m_document_edit;

		BOOL						m_is_rtl;
		BOOL						m_autodetect_direction;
};

#endif //RICHTEXTEDITOR_H
