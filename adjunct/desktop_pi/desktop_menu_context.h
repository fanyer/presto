/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DESKTOPCONTEXTMENU_H
# define DESKTOPCONTEXTMENU_H

class OpDocumentContext;
class OpWidget;
class URLInformation;
class DesktopWindow;
class OpPopupMenuRequestMessage;
class OpWindowCommander;

class DesktopMenuContext
{
public:
	DesktopMenuContext();
	~DesktopMenuContext();

	void		Init(URLInformation* file_url_info);
	OP_STATUS	Init(OpWidget * widget, const OpPoint & pos);
	OP_STATUS	Init(OpWindowCommander* commander, OpDocumentContext* context, const OpPopupMenuRequestMessage* message = NULL);

#ifdef INTERNAL_SPELLCHECK_SUPPORT 
	int			GetSpellSessionID();
#endif // INTERNAL_SPELLCHECK_SUPPORT 

	OpDocumentContext* GetDocumentContext() const { return m_context; }
	void SetContext(OpDocumentContext* context);

	URLInformation* GetFileUrlInfo() const { return m_file_url_info; }

	DesktopWindow* GetDesktopWindow() const { return m_desktop_window; }
	void SetDesktopWindow(DesktopWindow* window) { m_desktop_window = window; }

	OpPopupMenuRequestMessage* GetPopupMessage() const { return m_message; }

	OpWindowCommander* GetWindowCommander() const { return m_window_commander; }

	/**
	 * Clones OpPopupMenuRequestMessage.
	 *
	 * This function is a temporary solution that will either be removed (if we have reference-counted messages)
	 * or replaced by a proper copy call.
	 * The caller must delete the message after using it.
	 * @param message a message to copy
	 * @return new message object, or NULL if OOM
	 */
	static OpPopupMenuRequestMessage* CopyMessage(const OpPopupMenuRequestMessage& message);

	//--------------------
	struct WidgetContext
	{
		WidgetContext() : m_screen_pos(0, 0), m_packed_init(0), m_spell_session_id(0) {}

		OP_STATUS	Init(OpWidget * widget, const OpPoint & pos);
		BOOL		IsInitialized() const { return m_packed.is_initialized; }

		OP_STATUS	SetSelectedTextFromWidget(OpWidget * widget);
		void		SetScreenPosFromWidget(OpWidget * widget, const OpPoint & pos);

		INT32			m_widget_type;	// really of type OpTypedObject::Type
		OpPoint			m_screen_pos;
		OpString		m_selected_text;

		union { 
			struct { 
				unsigned int is_initialized:1; 
				unsigned int is_form_element:1; 
				unsigned int is_enabled:1;
				unsigned int is_read_only:1; 
				unsigned int has_text_selection:1;
			}	m_packed; 
			unsigned int m_packed_init; 
		}; 

#ifdef INTERNAL_SPELLCHECK_SUPPORT 
		int		m_spell_session_id; 
#endif // INTERNAL_SPELLCHECK_SUPPORT 

	} m_widget_context;

private:
	OpDocumentContext* m_context;
	URLInformation* m_file_url_info;
	DesktopWindow* m_desktop_window;
	OpPopupMenuRequestMessage* m_message;
	OpWindowCommander* m_window_commander;

};

#endif //DESKTOPCONTEXTMENU_H


