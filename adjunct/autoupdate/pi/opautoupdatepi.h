#ifndef OPAUTOUPDATEPI_H
#define OPAUTOUPDATEPI_H

#ifdef AUTO_UPDATE_SUPPORT

class OpAutoUpdatePI
{
public:
	virtual ~OpAutoUpdatePI() {}
	
	static OpAutoUpdatePI* Create();

	/*
	 * Get OS name
	 *
	 * Name of the operating system as will be passed to the auto update 
	 * system. It should be of the form as show on the download page
	 * i.e. http://www.opera.com/download/index.dml?custom=yes
	 *
	 * @param os (in/out) Name of the OS
	 *
	 * Example of values: FreeBSD, Linux, MacOS, Windows
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetOSName(OpString& os) = 0;

	/*
	 * Get OS version (except *nix)
	 *
	 * Version of the operating system as will be passed to the auto update system
	 *
	 * @param version (in/out) Name of OS version
	 *
	 * Example of values: 10.2, 10.3, 7, 95
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetOSVersion(OpString& version) = 0;

	/*
	 * Get architecture
	 *
	 * @param arch (in/out) Name of architecture
	 *
	 * Example of values: i386, ppc, sparc, x86-64
	 *	 
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 */
	virtual OP_STATUS GetArchitecture(OpString& arch) = 0;

	/*
	 * Get package type (*nix only)
	 *
	 * @param package (in/out) Name of *nix package
	 *
	 * Example of values: deb, rpm, tar.gz, tar.bz2, tar.Z, pkg.gz, pkg.bz2, pkg.Z
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 *
	 * Platforms other than *nix should set the string to empty and return OpStatus::OK.
	 */
	virtual OP_STATUS GetPackageType(OpString& package) = 0;

	/*
	 * Get gcc version (*nix only)
	 *
	 * @param gcc (in/out) GCC version
	 *
	 * Example of values: gcc295, gcc3, gcc4
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 *
	 * Platforms other than *nix should set the string to empty and return OpStatus::OK.
	 */
	virtual OP_STATUS GetGccVersion(OpString& gcc) = 0;

	/*
	 * Get QT version (*nix only)
	 *
	 * @param qt (in/out) QT version
	 *
	 * Example of values: qt3-shared, qt3-static, qt4-unbundled, qt4-bundled
	 *
	 * @return OP_STATUS Returns OpStatus::OK on success, otherwise OpStatus::ERR.
	 *
	 * Platforms other than *nix should set the string to empty and return OpStatus::OK.
	 */
	virtual OP_STATUS GetQTVersion(OpString& qt) = 0;

	/**
	 * Used on platforms that need to extract files required for
	 * installation from the downloaded package.
	 *
	 * @return AUF_Status. 
	 */
	virtual OP_STATUS ExtractInstallationFiles(uni_char *package) = 0;

	/**
	 * Return TRUE if ExtractInstallationFiles executes any long
	 * operation in background.
	 */
	virtual BOOL ExtractionInBackground() = 0;

};

#endif // AUTO_UPDATE_SUPPORT
#endif // OPAUTOUPDATEPI_H
