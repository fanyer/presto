/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef PREFERENCES_DIALOG_H
#define PREFERENCES_DIALOG_H

#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/desktop_util/treemodel/simpletreemodel.h"

#include "adjunct/desktop_pi/DesktopFontChooser.h"
#include "adjunct/desktop_pi/DesktopColorChooser.h"
#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "adjunct/quick/dialogs/ChangeMasterPasswordDialog.h"
#include "modules/prefs/prefsmanager/prefstypes.h"
#include "modules/wand/wandmanager.h"

class FontListItem;
class FontAtt;
class MasterPasswordLifetime;
class OpSkinManager;
class TrustedProtocolList;
class OpLabel;
class Viewer;
class SecurityPasswordDialog;
class SSL_Options;

/***********************************************************************************
**
**	NewPreferencesDialog
**
***********************************************************************************/

class NewPreferencesDialog :
	public Dialog,
	public OpColorChooserListener,
	public OpFontChooserListener,
	public DesktopFileChooserListener,
	public MasterPasswordListener,
	public OpPrefsListener,
	public WandListener,
	public OpSSLListener

{
	public:
		enum PageId
		{
			GeneralPage = 0,
			WandPage,
			SearchPage,
			WebPagesPage,
			AdvancedPage,
			// add new pages before PageCount

			PageCount
		};

		// Please make sure this enum has the same structure as the treeview
		enum AdvancedPageId
		{
			FirstAdvancedPage = 10,

			TabsPage = FirstAdvancedPage,
			BrowserPage,
			SoundPage,			
			// -------------
			DummySeparator1,
			// -------------
			ContentPage,
			FontPage,
			DownloadPage,
			ProgramPage,
			// -------------
			DummySeparator2,
			// -------------
			HistoryPage,
			CookiePage,
			SecurityPage,
			NetworkPage,
			LocalStoragePage, // Application cache, web storage etc
			// -------------
			DummySeparator3,
			// -------------
			ToolbarPage,
			ShortcutPage,

			// add advanced pages before AdvancedPageCount
			AdvancedPageCount
		};

								NewPreferencesDialog();
		virtual					~NewPreferencesDialog();

		/***********************************************************************************
		 ** Initialize the preference dialog
		 **
		 ** @param parent_window	The window to which the dialog should be attached
		 ** @param initial_page		The page (tab or selection in advanced treeview) that should be initially shown
		 **							If it is set to -1 it will open with the first or last opened page
		 ** @param hide_other_pages	If TRUE and initial_page is set, all other pages will be hidden so the user can
		 **							only see and edit the initial page.
		 **
		 ***********************************************************************************/
		OP_STATUS				Init(DesktopWindow* parent_window, INT32 initial_page = -1, BOOL hide_other_pages = FALSE);

		/***********************************************************************************
		 ** Create a copy of preference dialog with a copy of all changes that are changed but
		 ** not comitted yet. Is used to reinitialize all widgets after changing that the language
		 ** has been changed.
		 ** @return	Possible error
		 ***********************************************************************************/
		OP_STATUS				RecreateDialog();

		/***********************************************************************************
		 ** Copy changes in widgets (text in edit fields, selection and state of checkboxes,
		 ** radiobuttons and dropdowns) from this dialog to another preference dialog
		 ** @param widget			Parent widget (normally an OpGroup) that contains the changed widgets as children
		 ** @param target_dialog	Dialog to which the changes should be copied
		 ** @return					Possible error
		 ***********************************************************************************/
		OP_STATUS				CopyWidgetChanges(OpWidget* widget, NewPreferencesDialog* target_dialog);

		/***********************************************************************************
		 ** Wrapper function to initialize a specific main page even when it's not shown yet.
		 ** @param index The index of the page that should be initialized. Pages in the advanced
		 **				 section of the preference dialog can be initialized with InitAdvanced()
		 ***********************************************************************************/
		void					InitPage(INT32 index);

		/***********************************************************************************
		 ** Force focus to a specific widget when the dialog is opened for the first time. This
		 ** function needs to be called before Init is called to have any effect.
		 ** @param widget_name	The name of the widget (as in dialog.ini) which should get focus
		 ** @return				Possible memory error
		 ***********************************************************************************/
		OP_STATUS				SetInitialFocus(const char* widget_name) { return m_initial_focused_widget.Set(widget_name); }

		DialogType				GetDialogType(){return TYPE_PROPERTIES;}

		Type					GetType()				{return DIALOG_TYPE_PREFERENCES;}
		const char*				GetWindowName()			{return "New Preferences Dialog";}
		const char*				GetDialogImage();
		const char*				GetHelpAnchor();

		BOOL					GetShowPagesAsTabs()	{return !m_hide_other_pages;}

		INT32					GetButtonCount() { return 3; };
		void					GetButtonInfo(INT32 index, OpInputAction*& action, OpString& text, BOOL& is_enabled, BOOL& is_default, OpString8& name);
		BOOL					ShowAdPrefs();

		void					AddGroup(const uni_char* title, const char* name, INT32 id);

		void					OnInitPage(INT32 page_number, BOOL first_time);
		void					OnInit();
		void					OnSetFocus();

		void					OnChange(OpWidget *widget, BOOL changed_by_mouse);
		void					OnClick(OpWidget *widget, UINT32 id = 0);
		void					OnItemEdited(OpWidget *widget, INT32 pos, INT32 column, OpString& text);
		BOOL					OnInputAction(OpInputAction* action);
		void					OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);

		UINT32					OnOk();
		void					OnCancel();

		// desktop window hooks
		virtual void			OnClose(BOOL user_initiated);

		// callbacks
		void					OnViewerAdded(Viewer *v);
		void					OnViewerChanged(Viewer *v);
		void					OnViewerDeleted(Viewer *v);

		// implemented interfaces
		void					OnFontSelected(FontAtt& font, COLORREF color);
		void					OnColorSelected(COLORREF color);

		DesktopFileChooserRequest	m_request;
		void					OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result);

		void					OnSettingsChanged(DesktopSettings* settings);
		void					OnMasterPasswordChanged();
		void					OnInitVisibility();

		void					PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

		// WandListener interface method that we are interested in
		void					OnSecurityStateChange(BOOL successful, BOOL changed, BOOL is_now_security_encrypted);

		// OpSSLListener interface
		void					OnCertificateBrowsingNeeded(OpWindowCommander*, SSLCertificateContext* context, SSLCertificateReason, SSLCertificateOption options)
									{context->OnCertificateBrowsingDone(FALSE, options);}
		void					OnCertificateBrowsingCancel(OpWindowCommander*, SSLCertificateContext*) {}
		void					OnSecurityPasswordNeeded(OpWindowCommander*, SSLSecurityPasswordCallback* callback);
		void					OnSecurityPasswordCancel(OpWindowCommander*, SSLSecurityPasswordCallback*){}

		/*
		 * Init and Save functions
		 *
		 */

		OP_STATUS				InitGeneral();
		OP_STATUS				SaveGeneral();

		OP_STATUS				InitLanguages();
		OP_STATUS				SaveLanguages();

		OP_STATUS				InitWand();
		OP_STATUS				SaveWand();

		OP_STATUS               InitWebPages();
		OP_STATUS               SaveWebPages();

		// Temporary functions to init and save all advanced prefs

		OP_STATUS 				InitAdvanced(int id);
		OP_STATUS				SaveAdvanced();

		OP_STATUS				InitSearch();
		OP_STATUS				SaveSearch();
		OP_STATUS				ConstructSearchMsg();

		OP_STATUS				InitToolbars();
		OP_STATUS				SaveToolbars();

		OP_STATUS				InitMenus();
		OP_STATUS				SaveMenus();

		OP_STATUS				InitKeyboard();
		OP_STATUS				SaveKeyboard();

		OP_STATUS				InitMouse();
		OP_STATUS				SaveMouse();

		OP_STATUS				InitSounds();
		OP_STATUS				SaveSounds();

		OP_STATUS				InitFonts();
		OP_STATUS				SaveFonts();

		OP_STATUS				InitTabs();
		OP_STATUS				SaveTabs();

		OP_STATUS				InitBrowsing();
		OP_STATUS				SaveBrowsing();

		OP_STATUS				InitDisplay();
		OP_STATUS				SaveDisplay();

		OP_STATUS				InitFiles();
		OP_STATUS				SaveFiles();

		OP_STATUS				InitPrograms();
		OP_STATUS				SavePrograms();

		OP_STATUS				InitHistory();
		OP_STATUS				SaveHistory();
		void					InitCheckModification(const char* widget_name, CHMOD chmod, int chtime);

		OP_STATUS				InitCookies();
		OP_STATUS				SaveCookies();

		OP_STATUS				InitSecurity();
		OP_STATUS				SaveSecurity();

		OP_STATUS				InitNetwork();
		OP_STATUS				SaveNetwork();

		OP_STATUS				InitSites();
		OP_STATUS				SaveSites();

		OP_STATUS				InitLocalStorage();
		OP_STATUS				SaveLocalStorage();
		OP_STATUS				InitStorageModel();
		OP_STATUS				DeleteLocalStorage(const uni_char* domain);

		void OnCancelled() { }
		void OnRelayout();

		void EditSearchEntry();

		// MessageObject
		virtual void			HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	private:
		void					GetFontAttAndColorFromFontListItem(FontListItem* font_item, FontAtt& font, COLORREF& color, BOOL& fontname_only);
		void					UpdateWidgetFont(const char* widget_name, HTML_ElementType style);
		void					OnColorSelected(const char* widget_name, COLORREF color);
		OP_STATUS				InitFontsModel();
		void					PopulateFileTypes();

		// Language files related functions
		void					PrepareLanguageFolders(const OpString& main_code, OpVector<OpString>& folders);

		void					SearchLanguageFileInDirectory(
									OpVector<OpString>& lng_file_candidate_locations,
									const OpString& main_code,
									OpString& language_file_path,
									OpString& language_file_folder);

		// return TRUE if AdvancedPage is the current page and the specified sub page is selected
		BOOL					IsCurrentAdvancedPage(AdvancedPageId id);

		OpTreeView*					m_advanced_treeview;
		TemplateTreeModel<OpGroup>	m_advanced_model;

		SimpleTreeModel			m_sounds_model;
		SimpleTreeModel			m_fonts_model;
		SimpleTreeModel			m_file_types_model;
		SimpleTreeModel			m_toolbars_model;
		SimpleTreeModel			m_menus_model;
		SimpleTreeModel			m_mice_model;
		SimpleTreeModel			m_keyboards_model;
		SimpleTreeModel 		m_trusted_protocol_model;
		SimpleTreeModel			m_local_storage_model;

		OpAutoVector<FontListItem>	m_font_list;

		TrustedProtocolList*	m_trusted_protocol_list;
		TrustedProtocolList* 	m_trusted_application_list;

		OpString8				m_selected_color_chooser;
		OpString8				m_selected_font_chooser;

		INT32 					m_initial_advanced_page_id;
		BOOL					m_language_changed;
		BOOL					m_recreate_dialog;			///< If this flag is set, the dialog will create a new preference dialog on closing. Used for language switching
		OpString8				m_initial_focused_widget;	///< Name of the widget that should get focus when the dialog is opened for the first time

		static INT32			s_last_selected_advanced_prefs;
		static INT32			s_last_selected_prefs_page;

		// Careful. Never use BOOL when we store a INT32 in it. Fails un UNIX where BOOL is 1 or 0 [espen 2007-06-20]
		INT32					m_initial_show_scrollbars;
		INT32					m_initial_show_progress_dlg;
		INT32					m_initial_scale;
		INT32					m_initial_show_win_size;
		INT32					m_initial_rendering_mode;

		BOOL					m_hide_other_pages;
		OpINT32Vector			m_advanced_setup;
		DesktopFileChooser*		m_chooser;

		BOOL					m_previous_paranoid_pref;
};


/*
 * Helper classes
 *
 */

class LanguageCodeInfoItem
{
	public:
		LanguageCodeInfoItem	(const OpStringC& code, const OpStringC& name) { m_code.Set(code); m_name.Set(name); }
		OP_STATUS				GetName(OpString& name) { if (m_name.HasContent()) {return name.Set(m_name);} else {return name.Set(m_code);} }
		OP_STATUS				GetCode(OpString& code) { return code.Set(m_code); }
		BOOL					operator==(OpString& code) { return !m_code.Compare(code); }
	private:
		OpString m_code;
		OpString m_name;
};

class LanguageCodeInfo
{
	public:
								LanguageCodeInfo();
								~LanguageCodeInfo() { m_languages.DeleteAll(); }
		OP_STATUS				GetNameByCode(OpString& code, OpString& name);

		LanguageCodeInfoItem*	GetItem(UINT index) { return m_languages.Get(index); };
		UINT					GetCount() { return m_languages.GetCount(); }
	private:
		OpVector<LanguageCodeInfoItem>	m_languages;
};

class FontListItem
{
	public:
		FontListItem(Str::LocaleString string_id, short html_type, int font_id, int color_id) : m_string_id(string_id), m_html_type(html_type), m_font_id(font_id), m_color_id(color_id) {  }

		Str::LocaleString		GetStringId() { return m_string_id; }
		short					GetHTMLType() { return m_html_type; }
		int 					GetFontId() { return m_font_id; }
		int 					GetColorId() { return m_color_id; }

	protected:
		Str::LocaleString		m_string_id;
		short					m_html_type;
		int 					m_font_id;
		int 					m_color_id;
};

class AddWebLanguageDialog : public Dialog
{
	public:
		virtual					~AddWebLanguageDialog();

		OP_STATUS				Init(DesktopWindow* parent_window);

		Type					GetType()				{return DIALOG_TYPE_ADD_WEB_LANGUAGE;}
		const char*				GetWindowName()			{return "Add Web Language Dialog";}

		virtual void			OnInit();
		UINT32					OnOk();
		void					OnCancel();
		// void					OnClick(OpWidget *widget, UINT32 id = 0);

	private:
		SimpleTreeModel			m_web_language_model;
		LanguageCodeInfo		m_language_info;
};

class TabClassicOptionsDialog : public Dialog
{
public:
	virtual					~TabClassicOptionsDialog();

	OP_STATUS				Init(DesktopWindow* parent_window);

	Type					GetType()				{return DIALOG_TYPE_CLASSIC_TAB_OPTIONS;}
	const char*				GetWindowName()			{return "Classic Tab Options Dialog";}

	virtual void			OnInit();
	UINT32					OnOk();
	void					OnCancel();

private:
};


class LanguagePreferencesDialog : public Dialog
{
	public:
		virtual					~LanguagePreferencesDialog();

		OP_STATUS				Init(DesktopWindow* parent_window);

		Type					GetType()				{return DIALOG_TYPE_LANGUAGE_PREFERENCES;}
		const char*				GetWindowName()			{return "Language Preferences Dialog";}

		virtual void			OnInit();
		UINT32					OnOk();
		void					OnCancel();
		BOOL					OnInputAction(OpInputAction* action);
		void 					OnInitVisibility();

		OP_STATUS				AddWebLanguage(OpString &language);

	private:
		SimpleTreeModel			m_web_language_model;
};

class DefaultApplicationDialog : public Dialog
{
	public:
		virtual					~DefaultApplicationDialog();

		OP_STATUS				Init(DesktopWindow* parent_window);

		Type					GetType()				{return DIALOG_TYPE_DEFAULT_APPLICATION;}
		const char*				GetWindowName()			{return "Default Application Dialog";}

		virtual void			OnInit();
		UINT32					OnOk();
		void					OnCancel();
		BOOL					OnInputAction(OpInputAction* action);
};

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
class SearchEngineItem
{
public:
	SearchEngineItem(const OpStringC& name, const OpStringC& url, const OpStringC& key, const OpStringC& query, BOOL is_post, BOOL is_default = FALSE, BOOL is_speeddial = FALSE, SearchTemplate* search_template = NULL)
		:	m_is_post(is_post),
			m_valid(TRUE),
			m_default(is_default),
			m_speeddial(is_speeddial),
			m_search_template(search_template)
	{
			m_name.Set(name.CStr());
			m_url.Set(url.CStr());
			m_key.Set(key.CStr());
			m_query.Set(query.CStr());
	}
	SearchEngineItem()
		:	m_is_post(FALSE),
			m_valid(TRUE),
			m_default(FALSE),
			m_speeddial(FALSE),
			m_search_template(NULL)
	{
	}
	virtual ~SearchEngineItem() {  }


	OP_STATUS GetName(OpString& name) { return name.Set(m_name.CStr()); }
	OP_STATUS GetURL(OpString& url) { return url.Set(m_url.CStr()); }
	OP_STATUS GetKey(OpString& key) { return key.Set(m_key.CStr()); }
	OP_STATUS GetQuery(OpString& query) { return query.Set(m_query.CStr()); }
	SearchTemplate* GetSearchTemplate() { return m_search_template; }

	BOOL IsPost() { return m_is_post; }
	BOOL IsValid() { return m_valid; }
	BOOL IsDefaultEngine() { return m_default; }
	BOOL IsSpeeddialEngine() { return m_speeddial; }

	OP_STATUS SetName(const OpStringC& name) { return m_name.Set(name.CStr()); }
	OP_STATUS SetURL(const OpStringC& url) { return m_url.Set(url.CStr()); }
	OP_STATUS SetKey(const OpStringC& key) { return m_key.Set(key.CStr()); }
	OP_STATUS SetQuery(const OpStringC& query) { return m_query.Set(query.CStr()); }
	void SetIsPost(BOOL is_post) { m_is_post = is_post; }
	void SetValid(BOOL valid) { m_valid = valid; }
	void SetIsDefaultEngine(BOOL is_default) { m_default = is_default; }
	void SetIsSpeeddialEngine(BOOL is_speeddial) { m_speeddial = is_speeddial; }

protected:
	OpString	m_name;
	OpString	m_url;
	OpString	m_key;
	OpString	m_query;
	BOOL		m_is_post;
	BOOL		m_valid;
	BOOL		m_default;
	BOOL		m_speeddial;
	SearchTemplate* m_search_template;
};

class EditSearchEngineDialog : public Dialog
{
	public:
		EditSearchEngineDialog();
		virtual					~EditSearchEngineDialog() {};

		/**
		 * Initializes the dialog.
		 *
		 * @param item_to_change NULL if you want to add a new search engine. Otherwise: the SearchTemplate item you wish to change.
		 */
		OP_STATUS				Init(DesktopWindow* parent_window, SearchEngineItem *search_engine_item, const OpString & char_set, SearchTemplate * item_to_change = NULL);

		Type					GetType()				{return DIALOG_TYPE_SEARCH_ENGINE_EDIT;}
		const char*				GetWindowName()			{return "Search Engine Dialog";}
		const char*				GetHelpAnchor()			{return "search.html#custom";}
		DialogType				GetDialogType()			{return TYPE_OK_CANCEL;}
		BOOL					GetModality()			{return TRUE;}

		virtual void			OnInit();
		UINT32					OnOk();
		void					OnCancel();
		void					OnClick(OpWidget *widget, UINT32 id);
		void					OnChange(OpWidget *widget, BOOL changed_by_mouse);
		BOOL					OnInputAction(OpInputAction* action);

	protected:
		BOOL					IsValidShortcut();

	private:
		SearchEngineItem		*m_search_engine_item;
		OpString				m_char_set;
		SearchTemplate			*m_item_to_change;
		OpEdit					*m_key_edit;
		OpLabel					*m_key_label;
};

#endif // DESKTOP_UTIL_SEARCH_ENGINES

#endif //PREFERENCES_DIALOG_H
