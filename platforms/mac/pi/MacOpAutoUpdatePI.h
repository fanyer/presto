#ifndef MACOPAUTOUPDATEPI_H
#define MACOPAUTOUPDATEPI_H

#ifdef AUTO_UPDATE_SUPPORT

#include "adjunct/autoupdate/pi/opautoupdatepi.h"

class MacOpAutoUpdatePI : public OpAutoUpdatePI
{
public:
	virtual ~MacOpAutoUpdatePI() {}

	virtual OP_STATUS GetOSName(OpString& os);
	virtual OP_STATUS GetOSVersion(OpString& version);
	virtual OP_STATUS GetArchitecture(OpString& arch);
	virtual OP_STATUS GetPackageType(OpString& package);
	virtual OP_STATUS GetGccVersion(OpString& gcc);
	virtual OP_STATUS GetQTVersion(OpString& qt);

	virtual OP_STATUS ExtractInstallationFiles(uni_char *package) { return OpStatus::OK; }
	virtual BOOL ExtractionInBackground() { return FALSE; }
};

#endif // AUTO_UPDATE_SUPPORT

#endif // MACOPAUTOUPDATEPI_H
