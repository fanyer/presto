#ifndef WINDOWSOPFILECHOOSER_H
#define WINDOWSOPFILECHOOSER_H

#include "adjunct/desktop_pi/desktop_file_chooser.h"
#include "platforms/windows/windows_ui/OpShObjIdl.h"
#include "platforms/windows/utils/com_helpers.h"

class WindowsOpFileChooserProxy;

/*
* Class used to generate the extension used in the XP or Vista+ dialogs for a single filter. It's independent on the underlying implementation.
* There is one instance per file type, but each file type can have multiple extensions.
*/
class FileFilter
{
private:
	FileFilter() {}

public:
	FileFilter(const OpStringC& _description)
	{
		description.Set(_description);
	}
	~FileFilter()
	{
		m_filters.DeleteAll();
	}
	// Add an extension to this file type, eg "*.htm"
	OP_STATUS AddFilter(const OpStringC& filter);

	// Get the description of the file type, eg. "Text file"
	const OpStringC GetDescription() { return description.CStr(); }

	// The number of extensions
	UINT32 GetFilterCount() { return m_filters.GetCount(); }

	// Get the extension at a specific offset
	OpString* GetFilter(UINT32 i) { return m_filters.Get(i); }

private:
	OpString				description;		// description, aka the name of the file type
	OpAutoVector<OpString>	m_filters;			// the filters (extensions) associated with this file type
};

/*
* The file chooser base class containing all shared code among the two variations for XP and Vista+
*/
class WindowsFileChooserBase : public DesktopFileChooser, private MessageObject
{
public:
	WindowsFileChooserBase(WindowsOpFileChooserProxy* proxy);

	virtual OP_STATUS Execute(OpWindow* parent, DesktopFileChooserListener* listener, const DesktopFileChooserRequest& settings);

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	virtual void Cancel() = 0;
	virtual void Destroy() = 0;

	void AppendExtensionIfNeeded(OpString& current_name, UINT cur_name_len, DWORD max_file, DWORD ext_index);

	//Callback function for the folder selector
	static int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData);

	// let the platform remove the extension before opening the dialog, if wanted
	virtual void RemoveExtension(OpString& file_names);

protected:
	virtual ~WindowsFileChooserBase();

	virtual void ShowFileSelector() = 0;				// call to GetOpenFileName/GetSavefileName
	void ShowFolderSelector();							// call to SHBrowseForFolder
	OP_STATUS MakeWinFilter();							// Makes the filter string from the settings
	virtual OP_STATUS BuildFileFilter() = 0;			// Must be overridden
	void PrepareFileSelection(OpString& initial_dir, OpString& file_names, DWORD& MaxFile, uni_char*& file_name_start);

	const DesktopFileChooserRequest*	m_settings;
	WindowsOpFileChooserProxy*			m_proxy;

	uni_char*							m_filename_ext;
	BOOL								m_start_with_extension;
	DesktopFileChooserListener*			m_listener;
	OpAutoVector<FileFilter>			m_filters;			// translates into entries in the SetFileTypes call
	HWND								m_parent_window;
	DesktopFileChooserResult			m_results;
	HWND								m_chooser_window;
};

/*
* The file chooser used on XP
*/
class WindowsXPFileChooser : public WindowsFileChooserBase
{
public:
	WindowsXPFileChooser(WindowsOpFileChooserProxy* proxy);

	//File chooser hook and subclassing
	static UINT CALLBACK FileChooserHook( HWND hDlgChild, UINT uMsg, WPARAM wParam, LPARAM lParam);
	static UINT CALLBACK SubclassedDialogProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	virtual void Cancel();
	virtual void Destroy();

protected:
	virtual ~WindowsXPFileChooser();

	virtual OP_STATUS BuildFileFilter();
	virtual void ShowFileSelector();

//private:
protected:

	OpString m_filter;

private:
	static OpPointerHashTable<void, OPENFILENAME> m_subclassed_dialog_pointers;
	WNDPROC m_dialog_default_proc;
};

/*
* The file chooser used on Vista and higher
*/
class WindowsOpFileChooser : public WindowsFileChooserBase
{
public:
	WindowsOpFileChooser(WindowsOpFileChooserProxy* proxy);

	virtual void Cancel();
	virtual void Destroy();

	// we don't want to remove the extension at all as it triggers DSK-335820
	virtual void RemoveExtension(OpString& file_names) { }

protected:
	virtual void ShowFileSelector();				// call to IFileDialog derived interfaces
	virtual OP_STATUS BuildFileFilter();

private:

	virtual ~WindowsOpFileChooser();

	void FreeFilterSpecArray();

	OpComQIPtr<IFileDialog>		m_active_dialog;
	COMDLG_FILTERSPEC	*		m_file_filters;
	UINT32						m_file_filters_count;
	DWORD						m_event_cookie;			// set when the event callback is set
};

class WindowsVistaDialogEventHandler : public IFileDialogEvents
{
public:
	WindowsVistaDialogEventHandler() : m_ref_counter(0), m_results_ref(NULL), m_settings(NULL), m_chooser(NULL) {}
	~WindowsVistaDialogEventHandler() {};

	/******************* IUnknown interface *******************/
	//	For documentation, see
	//	http://msdn2.microsoft.com/en-us/library/ms680509.aspx

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void ** pvvObject);
	ULONG STDMETHODCALLTYPE AddRef();
	ULONG STDMETHODCALLTYPE Release();

	// IFileDialogEvents
	STDMETHODIMP OnFileOk(IFileDialog* pfd);
	STDMETHODIMP OnFolderChanging(IFileDialog* pfd, IShellItem* psiFolder);
	STDMETHODIMP OnFolderChange(IFileDialog* pfd);
	STDMETHODIMP OnSelectionChange(IFileDialog* pfd);
	STDMETHODIMP OnShareViolation(IFileDialog* pfd, IShellItem* psi, FDE_SHAREVIOLATION_RESPONSE* pResponse);
	STDMETHODIMP OnTypeChange(IFileDialog* pfd);
	STDMETHODIMP OnOverwrite(IFileDialog* pfd, IShellItem* psi, FDE_OVERWRITE_RESPONSE* pResponse);

	DesktopFileChooserResult *m_results_ref;
	const DesktopFileChooserRequest* m_settings;
	WindowsOpFileChooser	*m_chooser;

private:
	long m_ref_counter;
};

class WindowsOpFileChooserProxy : public DesktopFileChooser
{
public:
	WindowsOpFileChooserProxy() : m_actual_chooser(NULL) {}
	~WindowsOpFileChooserProxy() {m_actual_chooser->Destroy();}

	OP_STATUS Init();

	OP_STATUS Execute(OpWindow* parent, DesktopFileChooserListener* listener, const DesktopFileChooserRequest& settings) {return m_actual_chooser->Execute(parent, listener, settings);}
	void Cancel() {m_actual_chooser->Cancel();}


private:
	WindowsFileChooserBase* m_actual_chooser;
};

#endif // WINDOWSOPFILECHOOSER_H
