#ifndef M2_GLUE_H
#define M2_GLUE_H

#if defined(M2_SUPPORT)

// ----------------------------------------------------

#include "adjunct/m2/src/engine/listeners.h"
#include "adjunct/m2/src/glue/factory.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/layout/bidi/characterdata.h"
#ifndef HAVE_REAL_UNI_TOLOWER
#include "modules/encodings/tablemanager/optablemanager.h"
#endif

// ----------------------------------------------------

class AHotListContact;
class AHotListFolder;
class Window;
class MailerGlue;
class BrowserUtilsGlue;
class MimeUtilsGlue;
class OpDLL;
class OpTaskbar;
class ProgressInfo;
class MessageEngine;

// ----------------------------------------------------

#ifdef _DEBUG
class DebugReferences
{
public:
	enum DebugReferencesTypes
	{
		M2REF_COMMGLUE,
		M2REF_INPUTSTREAM,
		M2REF_MESSAGELOOP,
		M2REF_OPFILE,
		M2REF_OUTPUTSTREAM,
		M2REF_PREFSFILE,
		M2REF_PREFSSECTION,
		M2REF_URL,
		M2REF_INPUTCONVERTER,
		M2REF_OUTPUTCONVERTER,
		M2REF_UTF8TOUTF16CONVERTER,
		M2REF_UTF16TOUTF8CONVERTER,
		M2REF_UTF7TOUTF16CONVERTER,
		M2REF_UTF16TOUTF7CONVERTER
	};

	DebugReferencesTypes type;
	OpString8   source_file;
	int		    source_line;
	const void*	object_ptr;
};
void AddToReferencesList(DebugReferences::DebugReferencesTypes type, const OpStringC8& source_file, int source_line, const void* object_ptr);
void RemoveFromReferencesList(DebugReferences::DebugReferencesTypes type, const void* object_ptr);
#endif

// ----------------------------------------------------

class MailerUtilFactory : public GlueFactory
{
private:
    BrowserUtilsGlue* m_browser_utils_glue;
    MimeUtilsGlue*    m_mime_utils_glue;

public:
    MailerUtilFactory();
    ~MailerUtilFactory();

// ----------------------------------------------------

#ifdef _DEBUG
	ProtocolConnection* CreateCommGlueDbg(const OpStringC8& source_file, int source_line);
#else
	ProtocolConnection* CreateCommGlue();
#endif
	void DeleteCommGlue(ProtocolConnection*);

// ----------------------------------------------------

#ifdef _DEBUG
	MessageLoop* CreateMessageLoopDbg(const OpStringC8& source_file, int source_line);
#else
	MessageLoop* CreateMessageLoop();
#endif
    void         DeleteMessageLoop(MessageLoop* loop);

// ----------------------------------------------------

#ifdef _DEBUG
	PrefsFile* CreatePrefsFileDbg(const OpStringC& path, const OpStringC8& source_file, int source_line);
#else
	PrefsFile* CreatePrefsFile(const OpStringC& path);
#endif
    void       DeletePrefsFile(PrefsFile* file);

// ----------------------------------------------------

#ifdef _DEBUG
    PrefsSection* CreatePrefsSectionDbg(PrefsFile* prefsfile, const OpStringC& section, const OpStringC8& source_file, int source_line);
#else
    PrefsSection* CreatePrefsSection(PrefsFile* prefsfile, const OpStringC& section);
#endif
    void          DeletePrefsSection(PrefsSection* section);

// ----------------------------------------------------

#ifdef _DEBUG
	OpFile* CreateOpFileDbg(const OpStringC& path, const OpStringC8& source_file, int source_line);
#else
	OpFile* CreateOpFile(const OpStringC& path);
#endif
    void    DeleteOpFile(OpFile* file);

// ----------------------------------------------------

#ifdef _DEBUG
    URL* CreateURLDbg(const OpStringC8& source_file, int source_line);
#else
    URL* CreateURL();
#endif
    void DeleteURL(URL* url);

// ----------------------------------------------------

#ifdef _DEBUG
    UnicodeFileInputStream*  CreateUnicodeFileInputStreamDbg(OpFile *inFile, UnicodeFileInputStream::LocalContentType content, const OpStringC8& source_file, int source_line);
#else
    UnicodeFileInputStream*  CreateUnicodeFileInputStream(OpFile *inFile, UnicodeFileInputStream::LocalContentType content);
#endif
    void                     DeleteUnicodeFileInputStream(UnicodeFileInputStream* stream);

// ----------------------------------------------------

#ifdef _DEBUG
    UnicodeFileOutputStream* CreateUnicodeFileOutputStreamDbg(const uni_char* filename, const char* encoding, const OpStringC8& source_file, int source_line);
#else
    UnicodeFileOutputStream* CreateUnicodeFileOutputStream(const uni_char* filename, const char* encoding);
#endif
    void                     DeleteUnicodeFileOutputStream(UnicodeFileOutputStream* stream);

// ----------------------------------------------------

	OpSocket*			CreateSocket(OpSocketListener &listener);
	void				DestroySocket(OpSocket *socket);

	OpSocketAddress*	CreateSocketAddress();
	void				DestroySocketAddress(OpSocketAddress* socket_address);

	OpHostResolver*		CreateHostResolver(OpHostResolverListener &resolver_listener);
	void				DestroyHostResolver(OpHostResolver* host_resolver);

// ----------------------------------------------------

    BrowserUtils* GetBrowserUtils();

	MimeUtils* GetMimeUtils();

    char*     OpNewChar(int size);
    uni_char* OpNewUnichar(int size);
    void      OpDeleteArray(void* p);
};

// ----------------------------------------------------

class MailerGlue : public InteractionListener
#ifdef IRC_SUPPORT
                 , public ChatListener
#endif // IRC_SUPPORT
{

public:

	MailerGlue();
    virtual ~MailerGlue();

	OP_STATUS Start(OpString8& status);
	OP_STATUS Stop();

	OP_STATUS MailCommand(URL& url);

    MailerUtilFactory* GetMailerUtilFactory() const {return m_factory;}

private:

	typedef OP_STATUS (*get_engine_glue_fn_t)(MessageEngine**);
	typedef OP_STATUS (*delete_engine_glue_fn_t)();
	typedef INT32 (*get_library_version_fn_t)();

#if !defined(MSWIN) && !defined(_UNIX_DESKTOP_)
	OpDLL*					m_dll;
#endif

	MessageEngine*				m_engine_glue;
	MailerUtilFactory*		m_factory;

	// Implementing the MessageEngine listener interfaces
	void OnChangeNickRequired(UINT16 account_id, const OpStringC& old_nick);
	void OnRoomPasswordRequired(UINT16 account_id, const OpStringC& room);
	void OnError(UINT16 account_id, const OpStringC& errormessage, const OpStringC& context, EngineTypes::ErrorSeverity severity = EngineTypes::GENERIC_ERROR);
	void OnYesNoInputRequired(UINT16 account_id, EngineTypes::YesNoQuestionType type, OpString* sender=NULL, OpString* param=NULL);

#ifdef IRC_SUPPORT
	//ChatListener
	void OnChatStatusChanged(UINT16 account_id) {}
	void OnChatMessageReceived(UINT16 account_id, EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat);
	void OnChatServerInformation(UINT16 account_id, const OpStringC& server, const OpStringC& information) {}
	void OnChatRoomJoining(UINT16 account_id, const ChatInfo& room) {}
	void OnChatRoomJoined(UINT16 account_id, const ChatInfo& room);
	void OnChatRoomLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) {}
	void OnChatRoomLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) {}
	BOOL OnChatterJoining(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) {return TRUE;}
	void OnChatterJoined(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) {}
	void OnChatterLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) {}
	void OnChatterLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) {}
	void OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& changed_by, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value) {}
	void OnWhoisReply(UINT16 account_id, const OpStringC& nick, const OpStringC& user_id, const OpStringC& host, const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message, const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& channels) {}
	void OnInvite(UINT16 account_id, const OpStringC& nick, const ChatInfo& room);
	void OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id);
	void OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread) {}
	void OnChatReconnecting(UINT16 account_id, const ChatInfo& room) {}
#endif
};

// ----------------------------------------------------

class BrowserUtilsGlue : public BrowserUtils
{
public:
	BrowserUtilsGlue();
	virtual ~BrowserUtilsGlue();

public:
	const char* GetLocalHost();
	const char* GetLocalFQDN();
    OP_STATUS ResolveUrlName(const OpStringC& szNameIn, OpString& szResolved);
    OP_STATUS GetURL(URL*& url, const uni_char* url_string);
	OP_STATUS WriteStartOfMailUrl(URL* url, const Message* message);
	OP_STATUS WriteEndOfMailUrl(URL* url, const Message* message);
	OP_STATUS WriteToUrl(URL* url, const char* data, int length) const;
	OP_STATUS SetUrlComment(URL* url, const OpStringC& comment);

	OP_STATUS CreateTimer(OpTimer** timer, OpTimerListener* listener = NULL);

#ifndef HAVE_REAL_UNI_TOLOWER
	const void*	  GetTable(const char* table_name, long &byte_count) {return g_table_manager->Get(table_name, byte_count);}
#endif

	OP_STATUS GetUserAgent(OpString8& useragent, UA_BaseStringId override_useragent = UA_NotOverridden);
	OP_STATUS GetOperaMailRootDir(OpString &target);
	time_t    CurrentTime();
	long	  CurrentTimezone();
	BOOL	  OfflineMode();
	BOOL	  TLSAvailable();

	double GetWallClockMS();

	OP_STATUS ConvertFromIMAAAddress(const OpStringC8& imaa_address, OpString16& address, BOOL& is_mailaddress) const;

	/** Utility function to combine paths */
	OP_STATUS PathDirFileCombine(OUT OpString& dest, IN const OpStringC& directory, IN const OpStringC& file) const;

	/** Utility function to add a subdirectory to a given path */
	OP_STATUS PathAddSubdir (OUT OpString& dest, IN const OpStringC& subdir);

	OP_STATUS GetSelectedText(OUT OpString16& text) const;

	OP_STATUS FilenamePartFromFullFilename(const OpStringC& full_filename, OpString &filename_part) const;

	OP_STATUS GetContactName(const OpStringC &mail_address, OpString &full_name);
	OP_STATUS GetContactImage(const OpStringC &mail_address, const char*& image);
	OP_STATUS AddContact(const OpStringC& mail_address, const OpStringC& full_name, BOOL is_mailing_list = FALSE, int parent_folder_id = 0);
	OP_STATUS AddContact(const OpStringC& mail_address, const OpStringC& full_name, const OpStringC& notes, BOOL is_mailing_list = FALSE, int parent_folder_id = 0);
	BOOL      IsContact(const OpStringC &mail_address);
	INT32     GetContactCount();
	OP_STATUS GetContactByIndex(INT32 index, INT32& id, OpString& nick);

	OP_STATUS PlaySound(OpString& path);

	void	  UpdateAllInputActionsButtons() { g_input_manager->UpdateAllInputStates(); }

	/**
	 * Number of messages to keep essential headers from in cache, typically 4000-20000 for desktop,
	 * testing is needed to find best level on non-desktop platforms.
	 */
	int       NumberOfCachedHeaders();

    OP_STATUS ShowIndex(index_gid_t id, BOOL force_download = TRUE);

	OP_STATUS SetFromUTF8(OpString* string, const char* aUTF8str, int aLength = KAll);

	OP_STATUS ReplaceEscapes(OpString& string);

	// Needed for fetching RSS feeds
	OP_STATUS GetTransferItem(OpTransferItem** item, const OpStringC& url_string, BOOL* already_created = 0);
	OP_STATUS StartLoading(URL* url);
	OP_STATUS ReadDocumentData(URL& url, BrowserUtilsURLStream& url_stream);

	void      DebugStatusText(const uni_char* message);

#ifdef MSWIN
	OP_STATUS OpRegReadStrValue(IN HKEY hKeyRoot, IN LPCTSTR szKeyName, IN LPCTSTR szValueName, OUT LPCTSTR szValue, IO DWORD *pdwSize);

	LONG	  _RegEnumKeyEx(HKEY hKey, DWORD dwIndex, LPWSTR lpName, LPDWORD lpcbName, LPDWORD lpReserved, LPWSTR lpClass, LPDWORD lpcbClass, PFILETIME lpftLastWriteTime);
	LONG	  _RegOpenKeyEx(HKEY hKey, LPCWSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
	LONG	  _RegQueryValueEx(HKEY hKey, LPCWSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
	LONG	  _RegCloseKey(HKEY hKey);
	DWORD	  _ExpandEnvironmentStrings(LPCWSTR lpSrc, LPWSTR lpDst, DWORD nSize);
#endif //MSWIN

    //Dialogs requested opened from M2
    void      SubscribeDialog(UINT16 account_id);
	BOOL      MatchRegExp(const OpStringC& regexp, const OpStringC& string);

private:
#ifdef MSWIN
	AHotListContact* GetContactFromAddress(AHotListFolder *root, const OpStringC &mail_address);
#endif // MSWIN
};

#endif // M2_SUPPORT

#endif // M2_GLUE_H

// ----------------------------------------------------
