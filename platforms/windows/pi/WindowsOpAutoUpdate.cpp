#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT

#include "modules/pi/OpThreadTools.h"
#include "platforms/windows/pi/WindowsOpAutoUpdate.h"
#include "platforms/windows/win_handy.h"

OpAutoUpdatePI* OpAutoUpdatePI::Create()
{
	return OP_NEW(WindowsOpAutoUpdatePI, ());
}

OP_STATUS WindowsOpAutoUpdatePI::GetOSName(OpString& os)
{
	// This must match the download page i.e. http://www.opera.com/download/index.dml?custom=yes
	return os.Set(UNI_L("Windows"));
}

OP_STATUS WindowsOpAutoUpdatePI::GetOSVersion(OpString& version)
{

	return GetOSVersionStr(version);
	
}

OP_STATUS WindowsOpAutoUpdatePI::GetArchitecture(OpString& arch)
{
#ifdef _WIN64
	return arch.Set(UNI_L("x64"));
#else
	return arch.Set(UNI_L("i386"));
#endif
}

OP_STATUS WindowsOpAutoUpdatePI::GetPackageType(OpString& package)
{
	return package.Set("exe");
}

OP_STATUS WindowsOpAutoUpdatePI::GetGccVersion(OpString& gcc)
{
	return gcc.Set("");
}

OP_STATUS WindowsOpAutoUpdatePI::GetQTVersion(OpString& qt)
{
	return qt.Set("");
}

OP_STATUS WindowsOpAutoUpdatePI::ExtractInstallationFiles(uni_char *package)
{
	PackageExtractor *pack_ext = new PackageExtractor();
	OP_STATUS status = pack_ext->Extract(package);

	// If the extraction works, pack_ext will be deleted later on in PackageExtractor()
	// if not, then delete it here.
	if (status != OpStatus::OK)
		delete pack_ext;

	return status;

}

OP_STATUS WindowsOpAutoUpdatePI::PackageExtractor::Extract(uni_char *package)
{
	if (m_is_extracting)
		return OpStatus::ERR;

	m_is_extracting = TRUE;

	SHELLEXECUTEINFO info;
	memset(&info, 0, sizeof(info));

	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOCLOSEPROCESS;
	info.lpVerb = NULL;
	info.lpFile = package;
	info.nShow = SW_HIDE;

	if (!ShellExecuteEx(&info))
	{
		return OpStatus::ERR;
	}

	m_process_handle = info.hProcess;

	if (!RegisterWaitForSingleObject(&m_wait_object, m_process_handle, WaitOrTimerCallback, this, INFINITE, WT_EXECUTEONLYONCE))
	{
		CloseHandle(m_process_handle);
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

VOID CALLBACK WindowsOpAutoUpdatePI::PackageExtractor::WaitOrTimerCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired)
{
	g_thread_tools->PostMessageToMainThread(MSG_AUTOUPDATE_UNPACKING_COMPLETE, (MH_PARAM_1)lpParameter, NULL);

	PackageExtractor* pkg_ext = (PackageExtractor*)lpParameter;

	UnregisterWait(pkg_ext->m_wait_object);

	CloseHandle(pkg_ext->m_process_handle);

	delete pkg_ext;
}

#endif // AUTO_UPDATE_SUPPORT
