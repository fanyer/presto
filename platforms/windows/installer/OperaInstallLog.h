// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2010 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Øyvind Østlund
//

#include "modules/util/opfile/opfile.h"
#include "platforms/windows/installer/OperaInstaller.h"
#include "adjunct/desktop_util/shortcuts/DesktopShortcutInfo.h"

class XMLFragment;
class RegistryKey;

//
// Class ShortCut
//
class ShortCut
{
public:

	//
	// Static declarations/functions
	//

	// Array of the string names of the predefined shortcut destinations.
	static const uni_char* TYPE[];

	// Test to see if the declaration is within the defined values.
	static BOOL	IsValidDest(int destination);

	static int DestStr2DestNum(const uni_char* dest);

	static const uni_char* DestNum2DestStr(int dest);

	//
	// Public member functions
	//
	ShortCut() : m_destination(DesktopShortcutInfo::SC_DEST_NONE) {}

	OP_STATUS			SetPath(const uni_char* path);
	OP_STATUS			SetDestination(DesktopShortcutInfo::Destination destination);
	OP_STATUS			SetDestination(const uni_char* destination);

	const uni_char*		GetPath() const								{ return m_path.CStr(); }
	const DesktopShortcutInfo::Destination	GetDestination() const	{ return m_destination; }
	OP_STATUS			GetDestination(OpString& destination) const { return destination.Set(TYPE[m_destination]); }

private:
	OpString							m_path;
	DesktopShortcutInfo::Destination	m_destination;
};
	
//
// Class OperaInstallLog
//
class OperaInstallLog : public OpFile
{
public:

 	OperaInstallLog();
	~OperaInstallLog();
	

	OP_STATUS Parse();
	
	// Get methods for data parsed from a log file
	const OperaInstaller::Settings*	GetInstallMode() const;
	const uni_char*					GetInstallPath() const;
	const uni_char*					GetRegistryHive() const;
	
	// Get methods for data parsed from a log file.
	// Each call to these methods will raise the local index
	// meaning you will get the next element for each call
	// If there is no more elements stored in the logfile
	// a null pointer will be returned.
	const RegistryKey*				GetRegistryKey() const;
	const ShortCut*					GetShortcut() const;
	const uni_char*					GetFile() const;
	
	void ResetRegistryKeysIterator() const {m_reg_index = 0;}
	void ResetShortcutsIterator() const {m_shortcut_index = 0;}
	void ResetFilesIterator() const {m_file_index = 0;}

	// Set methods for data to be saved to the log file
	void SetInstallMode(OperaInstaller::Settings& install_mode);
	OP_STATUS SetInstallPath(const uni_char* install_path);
	OP_STATUS SetRegistryHive(const uni_char* reg_hive);
	
	// Set methods for data to be saved to the log file.
	// These calls can be called several times

	OP_STATUS SetRegistryKey(const RegistryKey& reg_key);
	OP_STATUS SetShortcut(const ShortCut& short_cut);
	OP_STATUS SetFile(const uni_char* file);
	
	OP_STATUS SaveToFile();
	
private:

	// Private functions to parse an element in a log file
	OP_STATUS ParseLogElement(XMLFragment& frag);
	OP_STATUS ParsePathElement(XMLFragment& frag);
	OP_STATUS ParseFilesElement(XMLFragment& frag);
	OP_STATUS ParseRegistryElement(XMLFragment& frag);
	OP_STATUS ParseShortcutElement(XMLFragment& frag);

	OP_STATUS SaveFileList(XMLFragment& frag);
	OP_STATUS SaveRegistryList(XMLFragment& frag);
	OP_STATUS SaveShortcutList(XMLFragment& frag);
	
	// Index pointing to the next item to read when
	// GetRegistryKey/GetShortcut/GetFile is called
	mutable UINT m_reg_index;
	mutable UINT m_shortcut_index;
	mutable UINT m_file_index;
	
	// Data containers for the data parsed from a log file
	OperaInstaller::Settings					m_mode;
	OpString									m_path;
	OpString									m_hive;
	OpVector<RegistryKey>						m_registry;
	OpVector<ShortCut>							m_shortcuts;
	OpVector<OpString>							m_files;
};