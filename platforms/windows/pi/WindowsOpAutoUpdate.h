#ifndef WINDOWSOPAUTOUPDATEPI_H
#define WINDOWSOPAUTOUPDATEPI_H

#include "adjunct/autoupdate/pi/opautoupdatepi.h"

#ifdef AUTO_UPDATE_SUPPORT

class WindowsOpAutoUpdatePI : public OpAutoUpdatePI
{
public:
	virtual ~WindowsOpAutoUpdatePI() {}

	virtual OP_STATUS GetOSName(OpString& os);
	virtual OP_STATUS GetOSVersion(OpString& version);
	virtual OP_STATUS GetArchitecture(OpString& arch);
	virtual OP_STATUS GetPackageType(OpString& package);
	virtual OP_STATUS GetGccVersion(OpString& gcc);
	virtual OP_STATUS GetQTVersion(OpString& qt);

	OP_STATUS ExtractInstallationFiles(uni_char *package);
	BOOL ExtractionInBackground() {return TRUE;}
private:
	class PackageExtractor
	{
	public:
		PackageExtractor() : m_is_extracting(FALSE), m_process_handle(NULL), m_wait_object(NULL) {}
		~PackageExtractor() {}

		OP_STATUS Extract(uni_char *package);

	private:
		static VOID CALLBACK WaitOrTimerCallback(PVOID lpParameter, BOOLEAN TimerOrWaitFired);
		BOOL m_is_extracting;
		HANDLE m_process_handle;
		HANDLE m_wait_object;
	};
};

#endif // AUTO_UPDATE_SUPPORT

#endif // WINDOWSOPAUTOUPDATEPI_H
