#ifndef USAGEREPORT_H
#define USAGEREPORT_H

#include "modules/prefsfile/prefsfile.h"
#include "modules/util/opfile/opfolder.h"

#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/windows/DocumentWindowListener.h"
#include "adjunct/autoupdate/scheduler/optaskscheduler.h"

enum ReportOperation
{
	REPLACE,
	ADD,
	MINIMUM,
	MAXIMUM
};

class SearchReport;
class ErrorReport;
class ButtonClickReport;
class ContentSharingReport;

// ---------------------------------------------------------------------------------

class UsageReportInfoDialog : public SimpleDialog
{
	public:
		UsageReportInfoDialog(){}
		virtual ~UsageReportInfoDialog(){}

		virtual UINT32 OnOk();
		virtual void OnCancel();
};

// ---------------------------------------------------------------------------------

class OpUsageReport
{
	protected:
		OP_STATUS ReportValue(INT32 value, const uni_char *section, const uni_char *key, ReportOperation operation, PrefsFile *prefs_file);
		OP_STATUS ReportValue(const uni_char* value, const uni_char *section, const uni_char *key, PrefsFile *prefs_file);

	public:
		OpUsageReport() {}
		virtual ~OpUsageReport() {}

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file) = 0;
};

// ---------------------------------------------------------------------------------

class OpUsageReportManager : public OpTimerListener, public OpScheduledTaskListener
{
	private:
		PrefsFile *m_prefs_file;
		OpAutoVector<OpUsageReport> m_reports;
		OpString m_filename;
		OpFileFolder m_folder;
		URL_InUse m_url_inuse;
		OpTimer m_timer;
		UINT32 m_report_timeout;
		OpScheduledTask m_upload_task;
		SearchReport* m_search_report;
		ErrorReport* m_error_report;
		ButtonClickReport* m_button_click_report;		// report for button clicks
		ContentSharingReport* m_content_sharing_report;

	private:
		PrefsFile *CreateXMLFile(const uni_char *filename, OpFileFolder folder);

		OP_STATUS ReportValue(OpUsageReport *report);

		OP_STATUS Upload();

		// OpScheduledTaskListener
		virtual void OnTaskTimeOut(OpScheduledTask* task);

		// OpTimerListener
		virtual void OnTimeOut(OpTimer *timer);

	public:
		OpUsageReportManager(const uni_char *filename, OpFileFolder folder);
		virtual ~OpUsageReportManager();

		OP_STATUS AddReport(OpUsageReport *report) { return m_reports.Add(report); }

		OP_STATUS GenerateReport();

		SearchReport* GetSearchReport() { return m_search_report; }

		ErrorReport* GetErrorReport() { return m_error_report; }

		ButtonClickReport* GetButtonClickReport() { return m_button_click_report; }

		ContentSharingReport* GetContentSharingReport() { return m_content_sharing_report; }

		static void ShowInfoDialog();
		
		time_t GetLastReportUpload();
};

extern OpUsageReportManager *g_usage_report_manager;

/***************************************************************************************
*
* ButtonClickReport - class that optionally can record clicks on toolbar buttons or other
*                     buttons.  It's enabled by calling SetReportUsageStats() on the OpToolbar
*                     class (plus enabling usage reports of course)
*
****************************************************************************************/
class ButtonClickReport : public OpUsageReport
{
public:
	ButtonClickReport() {}
	virtual ~ButtonClickReport() { m_clicks_table.DeleteAll(); }
	
	virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);

	// add a click for this button
	void AddClick(OpWidget* button);

protected:

	// class for the hash table
	class ButtonClickEntry
	{
	public:
		ButtonClickEntry(const OpString& name) : m_clicks(1)
		{
			m_name.Set(name);
		}
		OpString m_name;		// the key in the hash table
		INT32	m_clicks;		// number of clicks
	};

private:
	OpStringHashTable<ButtonClickEntry> m_clicks_table;		// key is the action of the button, data is the ButtonClickEntry. If it contains no action, the name is used instead
};

// ---------------------------------------------------------------------------------

class HotlistReport : public OpUsageReport
{
	protected:
		OP_STATUS GetModelCount(HotlistModel *model, UINT32 &noItems, UINT32 &noFolders, INT32 &lastCreated)
		{
			if(model)
			{
				noFolders = 0;
				noItems = 0;
				lastCreated = 0;

				HotlistModelItem *item = NULL;

				for(UINT32 i=0; (item = model->GetItemByIndex(i)) != NULL ; i++)
				{
					if(item->IsFolder())
					{
						if(!item->GetIsTrashFolder())
						{
							noFolders++;
						}
					}
					else
					{
						noItems++;
					}

					INT32 created = item->GetCreated();
					if(created > lastCreated && !item->IsFolder())
					{
						lastCreated = created;
					}
				}
				return OpStatus::OK;
			}
			return OpStatus::ERR;
		}

	public:
		HotlistReport() {}
		virtual ~HotlistReport() {}
};

// ---------------------------------------------------------------------------------

class BookmarksReport : public HotlistReport
{
	public:
		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file)
		{
			HotlistModel *model = g_hotlist_manager->GetBookmarksModel();
			if(model)
			{
				UINT32 noFolders = 0;
				UINT32 noBookmarks = 0;
				INT32 lastCreated = 0;

				RETURN_IF_ERROR(GetModelCount(model, noBookmarks, noFolders, lastCreated));

				RETURN_IF_ERROR(ReportValue(noBookmarks, UNI_L("Bookmarks"), UNI_L("NumberOfBookmarks"), REPLACE, prefs_file));
				RETURN_IF_ERROR(ReportValue(noFolders, UNI_L("Bookmarks"), UNI_L("NumberOfFolders"), REPLACE, prefs_file));
				RETURN_IF_ERROR(ReportValue(lastCreated, UNI_L("Bookmarks"), UNI_L("LastCreatedBookmark"), REPLACE, prefs_file));

				return OpStatus::OK;
			}
			return OpStatus::ERR;
		}
};

// ---------------------------------------------------------------------------------

class NotesReport : public HotlistReport
{
	public:
		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file)
		{
			HotlistModel *model = g_hotlist_manager->GetNotesModel();
			if(model)
			{
				UINT32 noFolders = 0;
				UINT32 noNotes = 0;
				INT32 lastCreated = 0;

				RETURN_IF_ERROR(GetModelCount(model, noNotes, noFolders, lastCreated));

				RETURN_IF_ERROR(ReportValue(noNotes, UNI_L("Notes"), UNI_L("NumberOfNotes"), REPLACE, prefs_file));
				RETURN_IF_ERROR(ReportValue(noFolders, UNI_L("Notes"), UNI_L("NumberOfFolders"), REPLACE, prefs_file));
				RETURN_IF_ERROR(ReportValue(lastCreated, UNI_L("Notes"), UNI_L("LastCreatedNote"), REPLACE, prefs_file));

				return OpStatus::OK;
			}
			return OpStatus::ERR;
		}
};

// ---------------------------------------------------------------------------------

class ContactsReport : public HotlistReport
{
	public:
		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file)
		{
			HotlistModel *model = g_hotlist_manager->GetContactsModel();
			if(model)
			{
				UINT32 noFolders = 0;
				UINT32 noContacts = 0;
				INT32 lastCreated = 0;

				RETURN_IF_ERROR(GetModelCount(model, noContacts, noFolders, lastCreated));

				RETURN_IF_ERROR(ReportValue(noContacts, UNI_L("Contacts"), UNI_L("NumberOfContacts"), REPLACE, prefs_file));
				RETURN_IF_ERROR(ReportValue(noFolders, UNI_L("Contacts"), UNI_L("NumberOfFolders"), REPLACE, prefs_file));
				RETURN_IF_ERROR(ReportValue(lastCreated, UNI_L("Contacts"), UNI_L("LastCreatedContact"), REPLACE, prefs_file));

				return OpStatus::OK;
			}
			return OpStatus::ERR;
		}
};

// ---------------------------------------------------------------------------------

class AccountReport : public OpUsageReport
{
	public:
		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);
};

// ---------------------------------------------------------------------------------

class PreferencesReport : public OpUsageReport
{
	public:
		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);
};

// ---------------------------------------------------------------------------------

class WandReport : public OpUsageReport
{
	private:
		BOOL HasEnteredWandInformation();

	public:
		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);
};

// ---------------------------------------------------------------------------------

class SkinReport : public OpUsageReport
{
	public:
		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);
};

// ---------------------------------------------------------------------------------

class MailReport : public OpUsageReport
{
	UINT32 m_sent_messages_count;
	public:
		MailReport(): m_sent_messages_count(0) {}
		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);
		void IncreaseSentMailCount() { ++m_sent_messages_count; }
};

extern MailReport *g_mail_report;
// ---------------------------------------------------------------------------------

class FeedsReport : public OpUsageReport
{
	public:
		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);
};

// ---------------------------------------------------------------------------------

class HistoryReport : public OpUsageReport
{
	public:
		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);
};

// ---------------------------------------------------------------------------------

class WorkspaceReport : public OpUsageReport, public DesktopWindowListener, public OpWorkspace::WorkspaceListener, public DocumentWindowListener
{
	private:
		INT32 m_tabs;
		INT32 m_min_tabs;
		INT32 m_max_tabs;
		INT32 m_windows;
		INT32 m_min_windows;
		INT32 m_max_windows;
		INT32 m_document_windows;
		INT32 m_document_windows_speeddial;
		INT32 m_document_windows_fit_to_width;
		INT32 m_document_windows_show_images;
		INT32 m_document_windows_cached_images;
		INT32 m_document_windows_no_images;
		double m_time_sum;
		double m_time_start;
		double m_time_last;
		double m_average_tabs;
		OpVector<OpWorkspace> m_workspaces;
		OpVector<DesktopWindow> m_desktop_windows;
		INT32 m_visited_pages;
		OpString m_last_url;

	public:
		WorkspaceReport();
		virtual ~WorkspaceReport();

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);

		// DesktopWindowListener
		virtual void OnDesktopWindowChanged(DesktopWindow* desktop_window) {}
		virtual void OnDesktopWindowActivated(DesktopWindow* desktop_window, BOOL active) {}
		virtual void OnDesktopWindowClosing(DesktopWindow* desktop_window, BOOL user_initiated) {}
		virtual void OnDesktopWindowParentChanged(DesktopWindow* desktop_window) {}

		// OpWorkspace::WorkspaceListener
		virtual void OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window);
		virtual void OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window);
		virtual void OnDesktopWindowOrderChanged(OpWorkspace* workspace) {}
		virtual void OnDesktopWindowActivated(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL activate) {}
		virtual void OnDesktopWindowAttention(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL attention) {}
		virtual void OnDesktopWindowChanged(OpWorkspace* workspace, DesktopWindow* desktop_window) {}
		virtual void OnDesktopWindowClosing(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL user_initiated) {}
		virtual void OnDesktopGroupAdded(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
		virtual void OnDesktopGroupCreated(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
		virtual void OnWorkspaceEmpty(OpWorkspace* workspace,BOOL has_minimized_window) {}
		virtual void OnWorkspaceAdded(OpWorkspace* workspace);
		virtual void OnWorkspaceDeleted(OpWorkspace* workspace);
		virtual void OnDesktopGroupRemoved(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
		virtual void OnDesktopGroupDestroyed(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
		virtual void OnDesktopGroupChanged(OpWorkspace* workspace, DesktopGroupModelItem& group) {}

		// DocumentDesktopWindow::DocumentWindowListener
		virtual void OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url);
		virtual void OnSpeedDialViewCreated(DocumentDesktopWindow* document_window);
};

extern WorkspaceReport *g_workspace_report;

// ---------------------------------------------------------------------------------

class ActionReport : public OpUsageReport
{
	private:
		OpINT32HashTable<INT32> m_actions;

	public:
		ActionReport();
		virtual ~ActionReport();

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);

		void ActionFilter(OpInputAction *action);
};

extern ActionReport *g_action_report;

// ---------------------------------------------------------------------------------

class BittorrentReport : public OpUsageReport, public DialogListener
{
	private:
		INT32 m_downloads;

	public:
		BittorrentReport();
		virtual ~BittorrentReport();

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);

		virtual void OnOk(Dialog* dialog, UINT32 result);

};

extern BittorrentReport *g_bittorrent_report;

// ---------------------------------------------------------------------------------

class AppearanceReport : public OpUsageReport
{
	public:
		AppearanceReport() {}
		virtual ~AppearanceReport() {}

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);

};

// ---------------------------------------------------------------------------------

class GeneralReport : public OpUsageReport
{
	public:
		GeneralReport() {}
		virtual ~GeneralReport() {}

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);

};

// ---------------------------------------------------------------------------------

class SpeedDialReport : public OpUsageReport
{
	public:
		SpeedDialReport() {}
		virtual ~SpeedDialReport() {}

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);

};

// ---------------------------------------------------------------------------------

class SearchReport : public OpUsageReport
{
	public:
		SearchReport();
		virtual ~SearchReport();

		enum SearchAccessPoint
		{
			SearchFromSearchfield = 0,					// regular search from search field
			SearchDefaultFromAddressfieldDropdown,		// 
			SearchHistoryFromAddressfieldDropdown,		// selected an existing search in the address field dropdown
			SearchShortcutInAddressfield,				// "g opera" in the address field
			SearchKeywordsInAddressfield,				// "opera browser" in the address field
			SearchSelectedText,
			SearchSpeedDial,
			SearchErrorPage,
			SearchUnknown,
			// suggestions
			// all suggestions have an offset from their respective
			// access point values.
			SearchSuggestionOffset,
			SearchSuggestedFromSearchfield = SearchSuggestionOffset,
			SearchSuggestedFromDefaultFromAddressfieldDropdown = SearchSuggestedFromSearchfield + 1,
			SearchSuggestedFromShortcutInAddressfield = SearchSuggestedFromDefaultFromAddressfieldDropdown + 2,
			SearchSuggestedFromSpeedDial = SearchSuggestedFromShortcutInAddressfield + 3,
			SearchSuggestedFromErrorPage,
			SearchLast
		};

		OP_STATUS AddSearch(SearchAccessPoint access_point, const OpStringC& search_guid);
		void SetWasSelectedFromAddressDropdown(BOOL selected_from_dropdown) { m_selected_from_dropdown = selected_from_dropdown; }
		void SetWasDefaultSearch(BOOL default_search) { m_default_search = default_search; }
		void SetWasSuggestFromAddressDropdown(BOOL suggest_from_dropdown) { m_is_suggestion = suggest_from_dropdown; }
		void SetSelectedDefaultFromAddressDropdown(BOOL selected_default_from_dropdown) { m_selected_default_from_dropdown = selected_default_from_dropdown; }

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);

	private:
		OpStringHashTable<INT32> m_searches[SearchLast];
		BOOL m_selected_from_dropdown;
		BOOL m_default_search;
		BOOL m_selected_default_from_dropdown;
		BOOL m_is_suggestion;
		static const uni_char* s_access_point_strings[];
};

// ---------------------------------------------------------------------------------

class UpgradeReport : public OpUsageReport
{
	public:
		UpgradeReport() {}
		virtual ~UpgradeReport() {}

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);

};

// ---------------------------------------------------------------------------------

class ErrorReport : public OpUsageReport, public OpErrorListener
{
		int file_refused;
		int file_not_found;
		int http_request_uri_too_long;
		int http_bad_request;
		int http_not_found;
		int http_internal_server_error;
		int http_no_content;
		int http_reset_content;
		int http_forbidden;
		int http_method_not_allowed;
		int http_not_acceptable;
		int http_request_timeout;
		int http_conflict;
		int http_gone;
		int http_length_required;
		int http_precondition_failed;
		int http_request_entity_too_large;
		int http_unsupported_media_type;
		int http_not_implemented;
		int http_bad_gateway;
		int http_gateway_timeout;
		int http_http_version_not_supported;
		int http_service_unavailable;
		int network_connection;
#ifdef WC_CAP_HAS_PROXY_CONNECT_ERROR_MSG
		int network_proxy_connect_failed;
#endif // WC_CAP_HAS_PROXY_CONNECT_ERROR_MSG
		void OnOperaDocumentError(OpWindowCommander*, OperaDocumentError ed_errno){}
		void OnFileError(OpWindowCommander*, FileError f_errno);
		virtual void OnHttpError(OpWindowCommander* commander, HttpError error);
		void OnDsmccError(OpWindowCommander*, DsmccError d_errno){}
		void OnTlsError(OpWindowCommander*, TlsError t_errno){}
		void OnNetworkError(OpWindowCommander*, NetworkError n_errno);
		void OnBoundaryError(OpWindowCommander*, BoundaryError b_errno){}
		void OnRedirectionRegexpError(OpWindowCommander*){}
		void OnOomError(OpWindowCommander*){}
		void OnSoftOomError(OpWindowCommander*){}
	public:
		ErrorReport() :
			file_refused(0),
			file_not_found(0),
			http_request_uri_too_long(0),
			http_bad_request(0),
			http_not_found(0),
			http_internal_server_error(0),
			http_no_content(0),
			http_reset_content(0),
			http_forbidden(0),
			http_method_not_allowed(0),
			http_not_acceptable(0),
			http_request_timeout(0),
			http_conflict(0),
			http_gone(0),
			http_length_required(0),
			http_precondition_failed(0),
			http_request_entity_too_large(0),
			http_unsupported_media_type(0),
			http_not_implemented(0),
			http_bad_gateway(0),
			http_gateway_timeout(0),
			http_http_version_not_supported(0),
			http_service_unavailable(0),
			network_connection(0)
#ifdef WC_CAP_HAS_PROXY_CONNECT_ERROR_MSG
			, network_proxy_connect_failed(0)
#endif // WC_CAP_HAS_PROXY_CONNECT_ERROR_MSG
			{}

		virtual ~ErrorReport() {}

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);
};

class ContentSharingReport : public OpUsageReport
{
	int sharing_twitter, sharing_facebook, sharing_email, sharing_chat, sharing_shinaweibo;

	public:
		ContentSharingReport() :
			sharing_twitter(0),
			sharing_facebook(0),
			sharing_email(0),
			sharing_chat(0),
			sharing_shinaweibo(0)
			{}

		void ReportSharedOnTwitter() { ++sharing_twitter; }
		void ReportSharedOnFacebook() { ++sharing_facebook; }
		void ReportSharedViaEmail() { ++sharing_email; }
		void ReportSharedOnChat() { ++sharing_chat; }
		void ReportSharedOnShinaWeibo() { ++sharing_shinaweibo; }

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);
};

// ---------------------------------------------------------------------------------
#ifdef WEB_TURBO_MODE

class TurboReport : public OpUsageReport
{
	public:
		TurboReport() {}
		virtual ~TurboReport() {}

		virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);

};

#endif // WEB_TURBO_MODE

// ---------------------------------------------------------------------------------
#ifdef VEGA_USE_HW

class HWAReport : public OpUsageReport
{
public:
	HWAReport() {}
	virtual ~HWAReport() {}
	
	virtual OP_STATUS GenerateReport(PrefsFile *prefs_file);
	
};

#endif // VEGA_USE_HW
#endif // USAGEREPORT_H

