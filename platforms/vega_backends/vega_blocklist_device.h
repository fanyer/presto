/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef VEGA_BACKEND_BLOCKLIST_H
#define VEGA_BACKEND_BLOCKLIST_H

class OperaGPU;

/**
   mixin to VEGA3dDevice to provide blocklisting functionality
*/
class VEGABlocklistDevice
{
public:
	/// the status of a blocklist device or blocklist entry
	enum BlocklistStatus
	{
		Supported = 0, ///< the device is supported
		BlockedDriverVersion, ///< the driver version is not supported
		BlockedDevice, ///< the device is not supported
		Discouraged, ///< using HW acceleration with this device is discouraged
		ListError, ///< reading the blocklist failed for some reason
	};
	enum BlocklistType
	{
		D3d10,  ///< DirectX 10
		WinGL,  ///< Windows OpenGL
		UnixGL, ///< Unix OpenGL
		MacGL,  ///< Mac OpenGL

#ifdef SELFTEST
		// secret, extra-special type used by selftest. overrides
		// GetURL with a changable string.
		SelftestBlocklistFile,
#endif // SELFTEST

		BlocklistTypeCount ///< number of blocklists
	};

	// to be implemeted by the 3d-device implementation
	///< the blocklist to use for this device
	virtual BlocklistType GetBlocklistType() const = 0;


	/**
	   used to provide data about platform when matching against
	   blocklist entries. this is a separate class so that
	   implementations can cache and/or fetch-on-demand values that
	   are expensive to obtain. it will live only while matching
	   blocklist entries.
	 */
	class DataProvider
	{
	public:
		virtual ~DataProvider() {}
		/// fetches value for key
		virtual OP_STATUS GetValueForKey(const uni_char* key, OpString& val) = 0;
	};
	/**
		@return a data provider object, or NULL on OOM. caller should OP_DELETE.
	 */
	virtual DataProvider* CreateDataProvider() = 0;

	/// for opera:gpu: temporarily create a VEGA3dDevice and call GenerateBackendInfo on it
	static OP_STATUS CreateContent(unsigned int device_type_index, OperaGPU* page);
	// for opera:gpu - called from factory after Construct regardless of status
	// construct_device - if TRUE, call VEGA3dDevice::ConstructDevice and include status
	OP_STATUS GenerateBackendInfo(OperaGPU* page, BOOL construct_device);
	// backend-specific information
	virtual OP_STATUS GenerateSpecificBackendInfo(OperaGPU* page, class VEGABlocklistFile* blocklist, DataProvider* provider) { return OpStatus::OK; }

private:
	// cludge: allow VEGABlocklistDevice to call VEGA3dDevice::ConstructDevice
	virtual OP_STATUS Construct3dDevice() = 0;
};

#endif  // !VEGA_BACKEND_BLOCKLIST_H
