/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Adam Minchinton
 */

#ifndef _OPERAVERSION_H_INCLUDED_
#define _OPERAVERSION_H_INCLUDED_

// Include build number stuff here so it saves craziness in the selftest includes
#if defined(MSWIN)
# include "platforms/windows/windows_ui/res/#buildnr.rci"
#elif defined(UNIX)
# include "platforms/unix/product/config.h"
#elif defined(_MACINTOSH_)
# include "platforms/mac/Resources/buildnum.h"
#elif defined(WIN_CE)
# include "platforms/win_ce/res/buildnum.h"
#else
# pragma error("include the file where VER_BUILD_NUMBER is defined")
#endif

// Shifts for the version data
#define OV_MAJOR_VER_SHIFT		24
#define OV_MINOR_VER_SHIFT		16

// Masks to retrieve the version data
#define OV_MINOR_VER_MASK		0xFF
#define OV_BUILD_NUM_MASK		0x0000FFFF

/**
 * This class is used to create version objects from strings and compare them 
 */
class OperaVersion
{
public:
	/**
	 * Constructs an OperaVersion instance that represents the version of the
	 * running Opera.
	 */
	OperaVersion();

	/**
	 * Constructs an OperaVersion instance that represents any version of
	 * Opera.
	 */
	OperaVersion(UINT32 major, UINT32 minor, UINT32 build = 1);

	OperaVersion(const OperaVersion& other);
	OperaVersion& operator=(OperaVersion other);
	void Swap(OperaVersion& other);

	~OperaVersion();

	/**
	 * Calling this method changes the version stored in this OperaVersion object to the one
	 * found in the autoupdate.ini file. This is used for testing autoupdate, so that the 
	 * version of the tested binary can be faked to any version needed.
	 * The autoupdate.ini file needs to be stored in the Opera *binary* directory, meaning the
	 * directory where the Opera executable is (this is NOT the profile directory).
	 * The autoupdate.ini file should contain only one line in a MAJOR.MINOR.BUILDNO format.
	 * Since changing the version will only occur if the file is present and its format can
	 * be recognized, there is no return value - this method will fail silently.
	 * Setting the fake version works only until Set() is called on this OperaVersion object, in
	 * which case the stored version will change.
	 */
	void AllowAutoupdateIniOverride();

	/**
	 * Sets the version inside the object. Strings not in the correct format
	 * or that have 0 as major and/or build number will be regarded as
	 * incorrect format
	 *
	 * @param version A string containing the version information 
	 * in the format "major.minor.buildnumber"
	 */
	OP_STATUS Set(const uni_char* version);

	/**
	 * Sets the version inside the object. Strings not in the correct format
	 * or that have 0 as major and/or build number will be regarded as
	 * incorrect format
	 *
	 * @param version A string containing the version information 
	 * in the format "major.minor"
	 * @param buildnumber A string containing the buildnumber information
	 */
	OP_STATUS Set(const uni_char* version, const uni_char* buildnumber);

	/** 
	 * Returns a pointer to a string representing the complete version 
	 * string in the format "major.minor.buildnumber"
	 * CAUTION: do not use returned pointer after object's destruction
	 * or after another call of GetFullString()!
	 */
	const uni_char* GetFullString() const;

	/**
	 * Returns the version number in the format MMmm (MM: major, mm: minor, eg. 910)
	 */
	UINT32 GetFullVersionNumber() const { return m_version >> OV_MINOR_VER_SHIFT; }

	/** 
	 * Returns the major version number
	 */
	UINT32 GetMajor() const { return m_version >> OV_MAJOR_VER_SHIFT; }

	/**
	 * Returns the minor version number
	 */
	UINT32 GetMinor() const { return (m_version >> OV_MINOR_VER_SHIFT) & OV_MINOR_VER_MASK; }

	/**
	 * Returns the build number
	 */
	UINT32 GetBuild() const { return m_version & OV_BUILD_NUM_MASK; }

	// Operators
	bool operator !=(const OperaVersion& other) const { return (m_version != other.m_version); }
	bool operator ==(const OperaVersion& other) const { return (m_version == other.m_version); }
	bool operator  <(const OperaVersion& other) const { return (m_version  < other.m_version); }
	bool operator <=(const OperaVersion& other) const { return (m_version <= other.m_version); }
	bool operator  >(const OperaVersion& other) const { return (m_version  > other.m_version); }
	bool operator >=(const OperaVersion& other) const { return (m_version >= other.m_version); }

private:
	UINT32 m_version; ///< Holds the version number in 0xMMIIBBBB format MM = Major, II = Minor, BBBB = build number
	mutable uni_char* m_version_string; ///< Holds the version string if requested
	
	/** Function used to set the major version. */ 
	void SetMajor(UINT32 major_version);

	/** Function used to set the minor version. */ 
	void SetMinor(UINT32 minor_version);

	/** Function used to set the build. */ 
	void SetBuild(UINT32 build_number);

	/** 
	 * Used to parse version string to create the internal version.
	 * Format is "major.minor.buildnumber"
	 *
	 * @param version string to parse
	 * @param major variable where major version will be written
	 * @param minor variable where minor version will be written
	 * @param buildnumber variable where buildnumber will be written
	 * @return OP_STATUS
	 */
	OP_STATUS ParseVersionString(const uni_char* version, UINT32& major, UINT32& minor, UINT32& buildnumber);
};


template <> inline void op_swap(OperaVersion& x, OperaVersion& y) { x.Swap(y); }


#endif // _OPERAVERSION_H_INCLUDED_
