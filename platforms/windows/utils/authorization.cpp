// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2003-2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Julien Picalausa
//

#include "core/pch.h"

#include "platforms/windows/utils/authorization.h"
#include "platforms/windows/win_handy.h"

#include <AclAPI.h>
#include <Sddl.h>
#include <LMCons.h>

BOOL WindowsUtils::IsPrivilegedUser(BOOL check_linked_token)
{
	HANDLE tk;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tk))
		return FALSE;

	if (check_linked_token && IsSystemWinVista())
	{
		TOKEN_ELEVATION_TYPE elv;
		DWORD len;

		if (!GetTokenInformation(tk, (TOKEN_INFORMATION_CLASS)TokenElevationType, &elv, sizeof(elv), &len))
		{
			CloseHandle(tk);
			return FALSE;
		}

		if (elv == TokenElevationTypeLimited)
		{
			TOKEN_LINKED_TOKEN linked_tk;
			if (!GetTokenInformation(tk, (TOKEN_INFORMATION_CLASS)TokenLinkedToken, &linked_tk, sizeof(linked_tk), &len))
			{
				CloseHandle(tk);
				return FALSE;
			}

			CloseHandle(tk);
			tk = linked_tk.LinkedToken;
		}

	}

	HANDLE dup_tk;
	if (!DuplicateToken(tk, SecurityIdentification, &dup_tk))
	{
		CloseHandle(tk);
		return FALSE;
	}

	BOOL b;
	SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
	PSID AdministratorsGroup; 
	b = AllocateAndInitializeSid(
		&NtAuthority,
		2,
		SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0,
		&AdministratorsGroup); 
	if(b) 
	{
		if (!CheckTokenMembership(dup_tk, AdministratorsGroup, &b)) 
		{
			b = FALSE;
		} 
		FreeSid(AdministratorsGroup); 
	}

	CloseHandle(dup_tk);
	CloseHandle(tk);

	return(b);
}

BOOL WindowsUtils::CheckObjectAccess(OpStringC &object_name, SE_OBJECT_TYPE object_type, DWORD access)
{
	PSECURITY_DESCRIPTOR sec_desc;
	HANDLE tk;
	HANDLE dup_tk;

	PRIVILEGE_SET PrivilegeSet;
	DWORD dwPrivSetSize = sizeof( PRIVILEGE_SET );
	DWORD access_granted;
	BOOL access_status = FALSE;

	GENERIC_MAPPING gen_map;

	switch (object_type)
	{
		case SE_FILE_OBJECT:
			gen_map.GenericRead = FILE_GENERIC_READ;
			gen_map.GenericWrite = FILE_GENERIC_WRITE;
			gen_map.GenericExecute = FILE_GENERIC_EXECUTE;
			gen_map.GenericAll = FILE_ALL_ACCESS;
			break;
		case SE_REGISTRY_KEY:
			gen_map.GenericRead = KEY_READ;
			gen_map.GenericWrite = KEY_WRITE;
			gen_map.GenericExecute = KEY_EXECUTE;
			gen_map.GenericAll = KEY_ALL_ACCESS;
			break;
		default:
			OP_ASSERT(!"Need to implement generic map for this object kind");
			return FALSE;
	}

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_DUPLICATE, &tk))
		return FALSE;

	if (!DuplicateToken(tk, SecurityIdentification, &dup_tk))
	{
		CloseHandle(tk);
		return FALSE;
	}

	CloseHandle(tk);


	if (GetNamedSecurityInfo(object_name.CStr(), object_type, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL, &sec_desc) != ERROR_SUCCESS)
	{
		CloseHandle(dup_tk);
		return FALSE;
	}

	AccessCheck(sec_desc, dup_tk, access, &gen_map, &PrivilegeSet, &dwPrivSetSize, &access_granted, &access_status);

	CloseHandle(dup_tk);
	LocalFree(sec_desc);

	return access_status;
}

BOOL WindowsUtils::GiveObjectAccess(OpString &object_name, SE_OBJECT_TYPE object_type, DWORD access, BOOL permanent, RestoreAccessInfo *&restore_access_info)
{
	restore_access_info = NULL;

	DWORD res;
	DWORD buf_size = UNLEN + 1;
	uni_char user_name[UNLEN + 1];

	GetUserName(user_name, &buf_size);

	PSECURITY_DESCRIPTOR sec_desc;
	PACL dacl;
	PACL temp_dacl;
	PACL new_dacl;
	EXPLICIT_ACCESS explicit_access;
	ULONG explicit_entries_count;
	EXPLICIT_ACCESS* explicit_entries;


	res = GetNamedSecurityInfo(object_name.CStr(), object_type, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sec_desc);

	if (res == ERROR_ACCESS_DENIED && TakeOwnership(object_name, object_type))
		res = GetNamedSecurityInfo(object_name.CStr(), object_type, DACL_SECURITY_INFORMATION, NULL, NULL, &dacl, NULL, &sec_desc);

	if (res != ERROR_SUCCESS)
		return FALSE;

	if (GetExplicitEntriesFromAcl(dacl, &explicit_entries_count, &explicit_entries) != ERROR_SUCCESS)
	{
		LocalFree(sec_desc);
		return FALSE;
	}

	for (ULONG i = 0; i < explicit_entries_count; i++)
	{
		if (explicit_entries[i].grfAccessMode == DENY_ACCESS && explicit_entries[i].Trustee.TrusteeForm == TRUSTEE_IS_SID)
		{
			BOOL is_member;
			if (!CheckTokenMembership(NULL, explicit_entries[i].Trustee.ptstrName, &is_member) || !is_member)
				continue;

			explicit_entries[i].grfAccessPermissions &= ~access;
		}
	}

	BuildExplicitAccessWithName(&explicit_access, user_name, access, GRANT_ACCESS, SUB_CONTAINERS_AND_OBJECTS_INHERIT);

	do
	{
		if (SetEntriesInAcl(1, &explicit_access, NULL, &temp_dacl) != ERROR_SUCCESS)
			break;

		res = SetEntriesInAcl(explicit_entries_count, explicit_entries, temp_dacl, &new_dacl);
		LocalFree(temp_dacl);

		if (res != ERROR_SUCCESS)
			break;

		res = SetNamedSecurityInfo(object_name.CStr(), object_type, DACL_SECURITY_INFORMATION, NULL, NULL, new_dacl, NULL);

		if (res == ERROR_ACCESS_DENIED && TakeOwnership(object_name, object_type))
			res = SetNamedSecurityInfo(object_name.CStr(), object_type, DACL_SECURITY_INFORMATION, NULL, NULL, new_dacl, NULL);

		LocalFree(new_dacl);

		if (res != ERROR_SUCCESS)
			break;

		if (permanent)
		{
			LocalFree(sec_desc);
		}
		else
		{
			restore_access_info = OP_NEW(RestoreAccessInfo, ());
			restore_access_info->object_name.Set(object_name);
			restore_access_info->old_acl = dacl;
			restore_access_info->sec_desc = sec_desc;
			restore_access_info->object_type = object_type;
		}

		return TRUE;
	}
	while (FALSE);

	LocalFree(explicit_entries);
	LocalFree(sec_desc);
	return FALSE;
}

BOOL WindowsUtils::TakeOwnership(OpString &object_name, SE_OBJECT_TYPE object_type)
{
	if (OpStatus::IsError(SetPrivilege(SE_TAKE_OWNERSHIP_NAME, TRUE)))
		return FALSE;

	SID_IDENTIFIER_AUTHORITY nt_authority_sid = SECURITY_NT_AUTHORITY;
	PSID admin_sid;

	if (!AllocateAndInitializeSid(&nt_authority_sid, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,0, 0, 0, 0, 0, 0, &admin_sid))
		return FALSE;

	DWORD res = SetNamedSecurityInfo(object_name.CStr(), object_type, OWNER_SECURITY_INFORMATION, admin_sid, NULL, NULL, NULL);

	OpStatus::Ignore(SetPrivilege(SE_TAKE_OWNERSHIP_NAME, FALSE));

	return res == ERROR_SUCCESS;
}

BOOL WindowsUtils::RestoreObjectAccess(RestoreAccessInfo *&restore_access_info)
{
	if (restore_access_info == NULL)
		return FALSE;

	DWORD res = SetNamedSecurityInfo(restore_access_info->object_name.CStr(), restore_access_info->object_type, DACL_SECURITY_INFORMATION, NULL, NULL, restore_access_info->old_acl, NULL);

	OP_DELETE(restore_access_info);
	restore_access_info = NULL;

	return res == ERROR_SUCCESS;
}

OP_STATUS WindowsUtils::RegkeyToObjectname(HKEY root_key, OpStringC &key_path, OpString &object_name)
{
		switch ((ULONG_PTR)root_key)
		{
			case HKEY_CLASSES_ROOT:
				RETURN_IF_ERROR(object_name.Set("CLASSES_ROOT\\"));
				break;
			case HKEY_LOCAL_MACHINE:
				RETURN_IF_ERROR(object_name.Set("MACHINE\\"));
				break;
			case HKEY_CURRENT_USER:
				RETURN_IF_ERROR(object_name.Set("CURRENT_USER\\"));
				break;
			case HKEY_USERS:
				RETURN_IF_ERROR(object_name.Set("USERS\\"));
				break;
			default:
				return OpStatus::ERR;
		}
		return object_name.Append(key_path);
}

OP_STATUS WindowsUtils::SetPrivilege(const uni_char* nt_defined_privilege, BOOL enable)
{
	OpAutoHANDLE hToken;
	
	// Try to get the access token associated with the process
	// Will fail if opera does not have the PROCESS_QUERY_INFORMATION access permission.
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
		return OpStatus::ERR;

	LUID privilige_id;

	// Looks up the locally unique id (LUID) to represent
	// the specified privilege name on the local system.
	if (!LookupPrivilegeValue(NULL, nt_defined_privilege, &privilige_id))
		return OpStatus::ERR;

	TOKEN_PRIVILEGES token_privileges;
	token_privileges.PrivilegeCount = 1;
	token_privileges.Privileges[0].Luid = privilige_id;
	token_privileges.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;

	// If PreviousState is not NULL, the token also needs TOKEN_QUERY
	if (!AdjustTokenPrivileges(hToken, FALSE, &token_privileges, 0, NULL, NULL) || GetLastError() == ERROR_NOT_ALL_ASSIGNED)
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS WindowsUtils::GetOwnStringSID(OpString &sid)
{
	OpAutoHANDLE token;
	TOKEN_USER *token_user;
	DWORD length;
	uni_char* string_sid;

	if (!OpenProcessToken( GetCurrentProcess(), TOKEN_QUERY, &token ))
		return OpStatus::ERR;

	if (!GetTokenInformation(token, TokenUser, NULL, 0,  &length))
	{
		DWORD err = GetLastError();
		if (err != ERROR_INSUFFICIENT_BUFFER)
			return OpStatus::ERR;
	}

	token_user = reinterpret_cast<TOKEN_USER*>(OP_NEWA(BYTE, length));

	if (!GetTokenInformation(token, TokenUser, token_user, length,  &length))
	{
		OP_DELETEA(token_user);
		return OpStatus::ERR;
	}

	ConvertSidToStringSid(token_user->User.Sid, &string_sid);
	sid.Set(string_sid);

	LocalFree(string_sid);
	OP_DELETEA(token_user);

	return OpStatus::OK;
}
