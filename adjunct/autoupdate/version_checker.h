#ifndef VERSION_CHECKER_H
#define VERSION_CHECKER_H

#ifdef AUTO_UPDATE_SUPPORT
#ifdef AUTOUPDATE_PACKAGE_INSTALLATION

#include "adjunct/autoupdate/autoupdater.h"
#include "adjunct/autoupdate/updatableresource.h"
#include "adjunct/autoupdate/updater/audatafile_reader.h"
#include "adjunct/desktop_util/version/operaversion.h"

/**
 * Listener interface to be implemented by anyone using VersionChecker - the check is async and this way you will be notified back
 * when it finishes.
 */
class VersionCheckerListener
{
public:
	virtual ~VersionCheckerListener() {}

	/**
	 * Called when the version check is finished, succeeded or not. It's only after you receive this callback, that you can trust th
	 * status read from the VersionChecker.
	 */
	virtual void VersionCheckFinished() = 0;
};

/**
 * The VersionChecker is meant to be a small utility class that allows checking whether the Opera update package that has been downloaded and
 * is about to be installed is not too old.
 * This class uses the autoupdate server to check whether a newer Opera package is available. The check occurs if there is a downloaded package
 * waiting for installation and at the same time the modification time of the autoupdate.txt file included in that package is more than MaxDownloadedPackageAgeDays
 * days old.
 * This class is meant to be used during startup of the Opera and it is not probable that you will have the chance to utilize it somewhere else.
 */
class VersionChecker:
	public StatusXMLDownloaderListener
{
public:
	enum VCStatus {
		/**
		 * The object has not been initialized at all, i.e. Init() was not called.
		 */
		VCSNotInitialized,
		/**
		 * Something went wrong during the Init() call, the object can't be used.
		 */
		VCSInitFailed,
		/**
		 * Version check is not needed as the update file is not older than MaxDownloadedPackageAgeDays.
		 */
		VCSCheckNotNeeded,
		/**
		 * Version check is needed since the update file is too old, you need to call CheckRemoteVersion().
		 */
		VCSCheckNotPerformed,
		/**
		 * Version check is in progress, wait until you'll be notified via a VersionCheckerListener callback
		 */
		VCSCheckInProgress,
		/**
		 * Checking of update version failed for some reason, network or autoupdate server issue probably
		 */
		VCSCheckVersionFailed,
		/**
		 * The server didn't sent us any update in response to our check request.
		 */
		VCSNoUpdateAvailable,
		/**
		 * An update is available on the server. The update version is older than what we got.
		 */
		VCSOlderUpdateAvailable,
		/**
		 * An update is available on the server. The update version is newer than what we got.
		 */
		VCSNewerUpdateAvailable,
		/**
		 * An update is available on the server. The update version is the same as what we got.
		 */
		VCSSameUpdateAvailable
	};

	VersionChecker();
	~VersionChecker();

	/**
	 * The VersionChecker object needs to be initialized before it can be used.
	 *
	 * @param listener - the VersionCheckerListener implementation that will be notified when the version check is over.
	 *
	 * @returns - OpStatus::OK if the object was initialized correctly, OpStatus::ERR otherwise.
	 */
	OP_STATUS Init(VersionCheckerListener* listener);

	/**
	 * Call this method to check whether a newer update than the one that is already downloaded is available on the server.
	 * Note this method will fail if the check is not needed.
	 * If this method returns OpStatus::OK, you should wait for the callback via the VersionCheckerListener interface 
	 * before proceeding further, this is due to the time needed to communicate with the autoupdate server.
	 *
	 * @returns - OpStatus::OK - communication with server in progress, wait for callback, OpStatus::ERR - check not needed
	 *            or starting the server communication failed, don't expect the callback and proceed now.
	 */
	OP_STATUS CheckRemoteVersion();

	/**
	 * Get the current status of the object, see the VCStatus enum for description.
	 *
	 * @returns - the current status of the object.
	 */
	VCStatus GetStatus() const { return m_status; }
	
	/**
	 * Implementation of StatusXMLDownloaderListener
	 */
	void StatusXMLDownloaded(StatusXMLDownloader* downloader);
	void StatusXMLDownloadFailed(StatusXMLDownloader* downloader, StatusXMLDownloader::DownloadStatus);

private:
	/* This is the number of days that we allow the downloaded update to be "fresh enough" to be installed.
	 * In case that an update that we have stored on the disk was downloaded more than MaxDownloadedPackageAgeDays days ago,
	 * we go and try to find a newer version with the autoupdate server before installing it.
	 */
	static const unsigned int MaxDownloadedPackageAgeDays = 5;

	/**
	 * Determine whether the update available on the disk is older than MaxDownloadedPackageAgeDays, in which case a check
	 * with the autoupdate server is needed.
	 *
	 * @returns - true in case the update is too old, false is we can install it safely.
	 */
	bool CheckIsNeeded();

	/**
	 * Sets the m_downloaded_version with the version found in the update stored on the disk.
	 *
	 * @returns - OpStatus::OK in case everything went OK, ERR otherwise.
	 */
	OP_STATUS ReadVersionOfExistingUpdate();

	/**
	 * Checks the modification time of the autoupdate.txt file. This is considered the time that the update was downloaded at.
	 *
	 * @param result - a reference to a time_t variable that will receive the modification time in case this method succeeds.
	 *
	 * @returns - OpStatus::OK in case everything went OK, ERR otherwise.
	 */
	OP_STATUS GetDataFileLastModificationTime(time_t& result);

	/**
	 * The autoupdate.txt file reader helper.
	 */
	AUDataFileReader m_data_file_reader;

	/**
	 * This object will store the version of the update available on the disk, if any.
	 * The information here is NOT valid if the object was not Init()ed, the Init() failed, or if the 
	 * current status is VCSCheckNotNeeded.
	 */
	OperaVersion m_downloaded_version;

	/**
	 * The current status of the object.
	 */
	VCStatus m_status;

	/**
	 * The autoupdate request XML construction helper.
	 */
	AutoUpdateXML* m_autoupdate_xml;

	/**
	 * The autoupdate XML communication helper.
	 */
	StatusXMLDownloader* m_status_xml_downloader;

	/**
	 * The listener that will be notified about the succesfull autoupdate version check.
	 */
	VersionCheckerListener* m_listener;

	/**
	 * Autoupdate server address helper.
	 */
	AutoUpdateServerURL* m_autoupdate_server_url;
};

#endif // AUTOUPDATE_PACKAGE_INSTALLATION
#endif // AUTO_UPDATE_SUPPORT

#endif // VERSION_CHECKER_H
