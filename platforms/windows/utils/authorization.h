// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#ifndef WINDOWS_AUTHORIZATION_H
#define WINDOWS_AUTHORIZATION_H

#include <AccCtrl.h>

namespace WindowsUtils
{
	/** Checks if the current user has administrator privileges
	  *
	  * @param check_linked_token checks if the user has a linked token that has administrator privileges.
	  *                           This parameter does nothing on systems that came before Windows Vista.
	  *                           On windows Vista and later, passing FALSE will check if the user needs to elevate at all.
	  *                           while passing TRUE checks if the user will need to know an administrator password to proceed.
	  *
	  * @return TRUE if the current user has administrator privileges and FALSE otherwise.
	  */
	BOOL IsPrivilegedUser(BOOL check_linked_token);

	/** Check if the current user has certain access permissions on a file
	  *
	  * @param file_path The path to the file to check
	  *
	  * @param access. Which accesses to check for
	  *
	  * @return TRUE if the current user has the desired accesses and FALSE otherwise
	  */
	BOOL CheckObjectAccess(OpStringC &object_name, SE_OBJECT_TYPE object_type, DWORD access);

	//Used by the functions below
	struct RestoreAccessInfo
	{
		~RestoreAccessInfo() {LocalFree(sec_desc);}

		OpString object_name;
		SE_OBJECT_TYPE object_type;
		PACL old_acl;
		PSECURITY_DESCRIPTOR sec_desc;
	};

	/** Obtain some permissions on a object, if possible
	  *
	  * This function will only be able to grant more permissions on a object if the current user
	  * has the power to modify the ACL of said object. Otherwise, it will fail.
	  *
	  * @param object_name The path or name to the object on which more permissions are desired.
	  *
	  * @param access The accesses desired on the object
	  *
	  * @param permanent Whether the permisisons change should be permanent or reverted at a later point.
	  *
	  * @param restore_access_info returns informations needed to cancel the permission changes.
	  *                            This information should be passed to RestoreObjectAccess at a later point.
	  *                            Will be set to NULL if the function fails or if permanent is FALSE.
	  *
	  * @return TRUE if the accesses were successfully given to the object and FALSE otherwise
	  */
	BOOL GiveObjectAccess(OpString &object_name, SE_OBJECT_TYPE object_type, DWORD access, BOOL permanent, RestoreAccessInfo *&restore_access_info);

	BOOL TakeOwnership(OpString &object_name, SE_OBJECT_TYPE object_type);

	/** Cancel earlier changes made to the ACL of a object
	  *
	  * @param restore_access_info A pointer to a RestoreAccessInfo struct previously returned by a call to GiveObjectAccess.
	  *                            The resources pointed to by this pointer will be freed and the pointer will be set to NULL,
	  *                            regardless of the outcome of this function.
	  *
	  * @return TRUE if the accesses were properly restored. FALSE otherwise.
	  */
	BOOL RestoreObjectAccess(RestoreAccessInfo *&restore_access_info);

	OP_STATUS RegkeyToObjectname(HKEY root_key, OpStringC &key_path, OpString &object_name);

	/** Enables a privilege for the current Opera process.
	  * NB: Remember to set the privilege back to what it was when you are done.
	  *
	  *	@param nt_defined_privilege The name of the privilege to be enabled.
	  *								The names are defined in winnt.h as SE_***_NAME
	  *								Privilege Constants:
	  *								http://msdn.microsoft.com/en-us/library/bb530716.aspx
	  *
	  *	@return	OK if the privilege was enabled for Opera, otherwise ERR.
	  */
	OP_STATUS SetPrivilege(const uni_char* nt_defined_privilege, BOOL enable);

	OP_STATUS GetOwnStringSID(OpString &sid);

};

#endif //AUTHORIZATION_H

