/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/* JIL FS MGR needs for proper working:
 * 1. Properly filled in a mapping file (the mapping file name and location are confugurable by Device Settings File and
 * Device Settings Directory preferences accordingly) *OR* mappings added via JILFSMgr::AddMapping().
 * Example of the mapping file looks like this:
 *
 * [Mappings Info]
 * Count=2
 *
 * [Mapping 1]
 * Mount Point=C:
 * \temp\music=/virtual/music
 * \temp\download=/virtual/download
 *
 * [Mapping 2]
 * Mount Point=D:
 * \uploads\upload=/virtual/photos
 * \my_videos\videos=/virtual/videos
 *
 * It specifies a numer of mappings and indexed mapping information staring from index 1. Mapping information consist of
 * location of a mount point (e.g. C: on Windows, /mnt/diskc on Linux), and directories translation (real directories should be specified
 * without repeating the mount point).
 * Real directories should always start with a path separator.
 *
 * On Unix-like systems the root mount point is designated as "/" (other paths don't end with a path separator).
 * Internally it is handled as an empty string, keep that in mind when debugging.
 *
 * Virtual roots must not be enclosed in any other, i.e. configuration with roots /virtual and /virtual/music is invalid.
 *
 * 2. OpStorageMonitor PI implemented. An implementation should properly informs CORE about mount points appearing/disppearing.
 *
 */

#ifndef DEVICEAPI_JIL_JILFSMGR_H
#define DEVICEAPI_JIL_JILFSMGR_H

#ifdef DAPI_JIL_FILESYSTEM_SUPPORT

#include "modules/pi/device_api/OpStorageMonitor.h"
#include "modules/util/simset.h"
#include "modules/util/opstring.h"
#include "modules/util/OpHashTable.h"

#define JILPATHSEPCHAR '/'
#define JILPATHSEP     "/"

class JILFSMgr : public OpStorageListener
{
private:
	OpStorageMonitor *m_drive_monitor;

	class FSMappingMgr
	{
	public: // BREW compiler (ADS) requires these types to be public (class friendship does not solve this)
		struct MountPointInfo;

		class FSMapping : public ListElement<FSMapping>
		{
		public:
			MountPointInfo *mount_point_info;
			OpString real;
			OpString virt;

			FSMapping() : mount_point_info(NULL) {}
			OP_STATUS Construct(MountPointInfo *mount_point_info, const uni_char *real, const uni_char *virt);
			/** Checks whether mapping is active i.e. mount_point was mounted and file specified by real path exists */
			BOOL IsActive();
		};

		struct MountPointInfo
		{
			OpString mount_point;
			BOOL is_mounted;
			AutoDeleteList<FSMapping> mappings;

			MountPointInfo() : is_mounted(FALSE) {}
		};

		//typedefs for simplicity
		typedef FSMapping MappingType;
		typedef OpAutoStringHashTable<MountPointInfo> MountPointMapType;

	private:
		MountPointMapType m_mount_points;

		class FSMappingDataGrabber
		{
		private:
			MountPointMapType *m_mount_points;
			/** Search params */

		public:
			enum FindParam
			{
				/** Look for real path as passed in path is virtual */
				VIRTUAL     = 0x1,
				/** Look for virtual path as passed in path is real */
				REAL        = 0x2,
				/** Look for mount point. This must be ORed with either VIRTUAL or REAL, then searches mount point
				 *  for real or virtual accordingly.
				 */
				MOUNT_POINT = 0x4
			};

			FSMappingDataGrabber(MountPointMapType *mapping_table) : m_mount_points(mapping_table) {}
			virtual ~FSMappingDataGrabber() {}

			/** Collects mapping data
			 *
			 * @param real<OUT> - array where real paths will be placed (may be NULL then real paths will not be returned)
			 * @param virt<OUT> - array where virtual paths will be placed (may be NULL then virtual paths will not be returned)
			 * @param active_only<IN> - if TRUE collects data for active mapping only
			 *
			 * @return OP_STATUS
			 */
			OP_STATUS CollectData(OpVector<OpString> *real, OpVector<OpString> *virt, BOOL active_only);

			/** Finds proper real path or virtual path or mount point (depending on fp parameter value) for given ref path using f as compare function
			 *
			 * @param find_params_mask<IN> - FindParam search mask
			 * @param ref<IN> - reference path used for search
			 * @param cmp_func<IN> - compare function
			 * @param result<OUT> - placaholder for result
			 * @param active_only<IN> - if TRUE only data for active mapping are found
			 *
			 * @return OP_STATUS
			 *  - OK - success
			 *  - ERR_FILE_NOT_FOUND - not found
			 *  - ERR_NO_MEMORY
			 */
			OP_STATUS Find(int find_params_mask, const uni_char *ref, int (*cmp_func)(const uni_char *, const uni_char *), OpString &result, BOOL active_only = TRUE);
		};

		FSMappingDataGrabber m_data_grabber;

		/** Checks whether a virtual path incompatibility.
		 *
		 * Virtual path roots must not be enclosed in any other root. This
		 * function checks this.
		 *
		 * @param virt A virtual root that is to be checked.
		 * @return OpStatus::OK if virt may be added to mapping,
		 *         OpStatus::ERR if it clashes with already defined mappings.
		 */
		OP_STATUS CheckVirtualRootClash(const uni_char *virt);

		/** Finds or creates a mount point info for the given path.
		 *
		 * Finds the MountPointInfo object in m_mount_points or - if not
		 * present - creates one, inserts it into m_mount_points and returns
		 * a pointer to it.
		 *
		 * @param mount_point_path The path of the mount point to find or create.
		 * @return Pointer to the MountPointInfo or NULL in case of OOM. The object
		 *         is owned by m_mount_points.
		 */
		MountPointInfo *GetOrCreateMountPointInfo(const uni_char* mount_point_path);

	public:
		FSMappingMgr() : m_data_grabber(&m_mount_points) {}
		virtual ~FSMappingMgr() {}

		/** Populates mapping data from mapping file
		 *
		 * @return OP_STATUS
		 */
		OP_STATUS PopulateDataFromFile();

		/** Sets mapping's mounted flag for given mount point
		 *
		 * @param mount_point<IN> - mount point for which mounted flag should be set
		 * @param val<IN> - BOOL value mounted flag will be set to
		 *
		 * @return OP_STATUS
		 */
		OP_STATUS SetMounted(const uni_char *mount_point, BOOL val);

		/**
		 * Retrieves mapping data
		 *
		 * @param real<OUT> - list of real directories. Content of real array must be freed by the callee
		 * @param virt<OUT> - list of virtual directories. Content of virt array must be freed by the callee
		 * @param active_only<IN> - take into account active mappings only
		 *
		 * @return OP_STATUS
		 */
		OP_STATUS GetData(OpVector<OpString> *real, OpVector<OpString> *virt, BOOL active_only = TRUE);

		/**
		 * Deletes all mapping data
		 */
		void DeleteAll() { m_mount_points.DeleteAll(); }

		/**
		 * Finds mount point from given virtual path using cmp_func as compare function
		 */
		OP_STATUS FindMountPointFromVirtual(const uni_char *virt, int (*cmp_func)(const uni_char *, const uni_char *), OpString &result, BOOL active_only = TRUE);

		/**
		 * Finds virtual path from given real path using cmp_func as compare function
		 */
		OP_STATUS FindVirtualFromReal(const uni_char *real, int (*cmp_func)(const uni_char *, const uni_char *), OpString &result, BOOL active_only = TRUE);

		/**
		 * Finds real path from given virtual path using cmp_func as compare function
		 */
		OP_STATUS FindRealFromVirtual(const uni_char *virt, int (*cmp_func)(const uni_char *, const uni_char *), OpString &result, BOOL active_only = TRUE);

		/**
		 * Adds single mapping data
		 */
		OP_STATUS AddData(const uni_char *mount_point, const uni_char *real, const uni_char *virt, BOOL mounted = FALSE);

		/**
		 * Removes mapping data.
		 *
		 * The mapping to be removed is identified by a combination of real and virt.
		 * If real and virt are NULLs all mappings for given mount point are removed.
		 */
		OP_STATUS RemoveData(const uni_char *mount_point, const uni_char *real, const uni_char *virt);

#ifdef SELFTEST
		OP_BOOLEAN IsMounted(const uni_char *mount_point);
#endif // SELFTEST
	};

	FSMappingMgr m_fs_mapping_manager;

	/**
	 * Maps real system path to virtual JIL's one
	 */
	OP_STATUS MapRealToVirtual(const uni_char *real, OpString &virt);

	/**
	 * Maps virtual JIL's path to real system's one
	 */
	OP_STATUS MapVirtualToReal(const uni_char *virt, OpString &real);

	static int CmpIfPathFromRoot(const uni_char *root, const uni_char *path)
	{
		return uni_strncmp(root, path, uni_strlen(root));
	}

	OP_STATUS ParseMappingFile();

#ifdef SELFTEST // In case of self test all below functions MUST BE PUBLIC
public:
	OP_STATUS RemoveMapping(const uni_char *mount_point, const uni_char *real, const uni_char *virt)
	{
		return m_fs_mapping_manager.RemoveData(mount_point, real, virt);
	}

	OP_STATUS SetActive(const uni_char *virt, BOOL active);

	OP_BOOLEAN IsMounted(const uni_char *mount_point) { return m_fs_mapping_manager.IsMounted(mount_point); }
#endif // SELFTEST

public:
	JILFSMgr();
	OP_STATUS Construct();
	virtual ~JILFSMgr();

	// OpStorageListener's interface
	OP_STATUS OnStorageMounted(const StorageInfo &info);
	OP_STATUS OnStorageUnmounted(const StorageInfo &info);

	/**
	 * Gets size of the JIL filesystem
	 *
	 * @param mount_point<IN> - mount point of the files system size of should be retrieved
	 * @param size<OUT> - to be filled with size of given filesystem
	 *
	 * @return OP_STATUS
	 */
	OP_STATUS GetFileSystemSize(const uni_char *mount_point, UINT64 *size);

	/**
	 * Checks whether given path is one of the virtual roots (or part of any of virtual root when part = TRUE)
	 *
	 * @param path<IN> - path which should be checked
	 * @param part<IN> - if TRUE it's checked whether given path if part (beginning part) of any of virtual roots
	 */
	BOOL IsVirtualRoot(const uni_char *path, BOOL part = FALSE);

	/**
	 * Converts path separator characters to system specific one - if neccesary.
	 */
	static void ToSystemPathSeparator(uni_char *path);

	/**
	 * Converts path separator characters to JIL specific one - if neccesary.
	 */
	static void ToJILPathSeparator(uni_char *path);

	/**
	 * Converts path from JIL's virtual to system's one
	 * @param jil_path<IN> - path to be translated
	 * @param system_path<OUT> - translated path
	 *
	 * @return OP_STATUS (ERR_FILE_NOT_FOUND is returned if path is wrong and thus can not be translated)
	 */
	OP_STATUS JILToSystemPath(const uni_char *jil_path, OpString &system_path);

	/**
	 * Converts path from system's to JIL's virtual one
	 *
	 * @param system_path<IN> - path to be translated
	 * @param jil_path<OUT> - translated path
	 *
	 * @return OP_STATUS (ERR_FILE_NOT_FOUND is returned if path is wrond and thus can not be translated)
	 */
	OP_STATUS SystemToJILPath(const uni_char *system_path, OpString &jil_path);

	/**
	 * Retrieves list of all available virtual directories
	 *
	 * @param virt_path<OUT> - list of virtual roots. Content of virt_path array must be freed by the callee
	 *
	 * @return OP_STATUS
	 */
	OP_STATUS GetAvailableVirtualDirs(OpVector<OpString> *virt_path)
	{
		return m_fs_mapping_manager.GetData(NULL, virt_path);
	}

	/**
	 * Checks if access to the file indicated via file:// URL is allowed
	 *
	 * @param url<IN> - file URL
	 */
	BOOL IsFileAccessAllowed(URL url);

	/** Adds single mapping data
	 *
	 * @param mount_point - mount point of the real directory
	 * @param real - real directory path (without mount point), must start with a path separator
	 * @param virt - full virtual path
	 * @param mounted - if TRUE a new mapping should be considered as mounted immediately (even without proper notification from OpStorageMonitor)
	 */
	OP_STATUS AddMapping(const uni_char *mount_point, const uni_char *real, const uni_char *virt, BOOL mounted = FALSE)
	{
		return m_fs_mapping_manager.AddData(mount_point, real, virt, mounted);
	}

	/** Joins two paths.
	 *
	 * Note: The function is not intended for general use by code
	 * outside JILFSMgr, it's public only for the sake of BREW compiler.
	 *
	 * @param result Set to the resulting path.
	 * @param path1 First path, must not end with a path separator.
	 * @param path2 Path to be appended after path1. May or may not start
	 *              with a path separator.
	 * @param path_separator Path separator to use.
	 */
	static OP_STATUS SafeJoinPath(OpString *result, const uni_char* path1, const uni_char* path2, uni_char path_separator);
};

#endif // DAPI_JIL_FILESYSTEM_SUPPORT

#endif // DEVICEAPI_JIL_JILFSMGR_H
