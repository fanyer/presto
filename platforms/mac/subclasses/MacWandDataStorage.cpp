/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "platforms/mac/subclasses/MacWandDataStorage.h"
#include "platforms/mac/util/systemcapabilities.h"
#include "platforms/mac/util/CTextConverter.h"

#include "modules/util/str.h"

#define kMaxPassLength 255

#ifdef WAND_EXTERNAL_STORAGE

MacWandDataStorage::MacWandDataStorage()
: WandDataStorage()
, mSearchRef(NULL)
, mLastIndex(0)
{
}

MacWandDataStorage::~MacWandDataStorage()
{
	if(mSearchRef)
	{
		KCReleaseSearch(&mSearchRef);
		mSearchRef = NULL;
	}
}

BOOL MacWandDataStorage::FetchData(const uni_char* server, WandDataStorageEntry* entry, INT32 index)
{
	BOOL result = FALSE;
	KCItemRef 	itemRef = 0;
	uni_char* servername = NULL;
	uni_char* path = NULL;

	if(index == 0)
	{
		if(mSearchRef)
		{
			KCReleaseSearch(&mSearchRef);
			mSearchRef = NULL;
		}

		KCAttributeList attrList;
		KCAttribute attrs[5];
		KCItemClass itemClass = kInternetPasswordKCItemClass;

		UInt16 port;
		FourCharCode protocol;
		int i = 0;
		Str255 pservername;
		Str255 ppath;

		MacWandDataStorage::ParseResult res = ParseString(server, servername, path, port, protocol);

		attrs[i].tag = kClassKCItemAttr;
		attrs[i].length = sizeof(itemClass);
		attrs[i].data = &itemClass;
		i++;

		if(res & kServerNameValid)
		{
			pservername[0] = gTextConverter.ConvertBufferToMac(servername, uni_strlen(servername), (char*)&pservername[1], 255, kTextCoverterDefaultOptions, kStringEncodingUniChar);

			attrs[i].tag = kServerKCItemAttr;
			attrs[i].length = pservername[0];
			attrs[i].data = &pservername[1];
			i++;
		}
		if(res & kPathValid)
		{
			ppath[0] = gTextConverter.ConvertBufferToMac(path, uni_strlen(path), (char*)&ppath[1], 255, kTextCoverterDefaultOptions, kStringEncodingUniChar);

			attrs[i].tag = kPathKCItemAttr;
			attrs[i].length = ppath[0];
			attrs[i].data = &ppath[1];
			i++;
		}
		if(res & kPortValid)
		{
			attrs[i].tag = kPortKCItemAttr;
			attrs[i].length = sizeof(port);
			attrs[i].data = &port;
			i++;
		}
		if(res & kProtocolValid)
		{
			attrs[i].tag = kProtocolKCItemAttr;
			attrs[i].length = sizeof(protocol);
			attrs[i].data = &protocol;
			i++;
		}

		attrList.count = i;
		attrList.attr = attrs;

		OSStatus err = KCFindFirstItem(NULL, &attrList, &mSearchRef, &itemRef);
		if(err == noErr)
		{
			result = GetAccountAndPassword(itemRef, entry);
			KCReleaseItem(&itemRef);
		}
		else
		{
			// Didn't find an exact match with path, try again without
			int pathidx = 1;
			int itemsToMove = 0;

			if(res & kPathValid)
			{
				attrList.count = i-1;

				if(res & kServerNameValid)
				{
					pathidx++;
				}

				if(res & kPortValid)
				{
					itemsToMove++;
				}
				if(res & kProtocolValid)
				{
					itemsToMove++;
				}

				if(itemsToMove)
				{
					for(int j = pathidx; j < pathidx+itemsToMove; j++)
					{
						attrs[j].tag = attrs[j+1].tag;
						attrs[j].length = attrs[j+1].length;
						attrs[j].data = attrs[j+1].data;
					}
				}

				err = KCFindFirstItem(NULL, &attrList, &mSearchRef, &itemRef);
				if(err == noErr)
				{
					result = GetAccountAndPassword(itemRef, entry);
					KCReleaseItem(&itemRef);
				}
			}
		}
	}
	else if(index > 0)
	{
		if(mSearchRef)
		{
			if(mLastIndex == (index-1))
			{
				// we're enumerating in sequence
				OSStatus err = KCFindNextItem(mSearchRef, &itemRef);
				if(err == noErr)
				{
					result = GetAccountAndPassword(itemRef, entry);
					KCReleaseItem(&itemRef);
				}
			}
			else
			{
				// jump forward to get correct item
				Boolean success = true;

				for(int i = 0; i < (index - mLastIndex); i++)
				{
					OSStatus err = KCFindNextItem(mSearchRef, &itemRef);
					if(err != noErr)
					{
						success = false;
						break;
					}
				}

				if(success)
				{
					result = GetAccountAndPassword(itemRef, entry);
					KCReleaseItem(&itemRef);
				}
			}
		}
		else
		{
			// start over with new search
			if(FetchData(server, entry, 0))
			{
				Boolean success = true;

				for(int i = 0; i < (index - mLastIndex); i++)
				{
					OSStatus err = KCFindNextItem(mSearchRef, &itemRef);
					if(err != noErr)
					{
						success = false;
						break;
					}
				}

				if(success)
				{
					result = GetAccountAndPassword(itemRef, entry);
					KCReleaseItem(&itemRef);
				}
			}
		}
	}

	mLastIndex = index;

	if(servername)
		delete[] servername;
	if(path)
		delete[] path;

	if(result == FALSE)
	{
		// this means we finished the search
		if(mSearchRef)
		{
			KCReleaseSearch(&mSearchRef);
			mSearchRef = NULL;
		}
	}

	return result;
}

void MacWandDataStorage::StoreData(const uni_char* server, WandDataStorageEntry* entry)
{
	uni_char* 		uni_servername = NULL;
	uni_char* 		uni_path = NULL;

	if(server && entry)
	{
		UInt16			port = 0;	// ignore port
		FourCharCode	theProtocol = kAnyProtocol;

		OSStatus		status;
		KCItemRef		item;
		Str255			password;
		Str255			username;
		Boolean			exactMatch = false;

		// store
		Str255			label;
		Str255			comment;
		Str255			chainName = "\pDefault";
		Str255			servername;
		Str255			path;
		KCAttribute		attr;

		MacWandDataStorage::ParseResult res = ParseString(server, uni_servername, uni_path, port, theProtocol);

		if(!(res & (kServerNameValid | kPathValid)))
		{
			if(uni_servername)
				delete[] uni_servername;
			if(uni_path)
				delete[] uni_path;

			return;
		}

		status = FindEntryInKeychain(	uni_servername, uni_strlen(uni_servername),
							entry->username.CStr(), entry->username.Length(),
							uni_path, uni_path ? uni_strlen(uni_path) : 0,
							port,
							theProtocol,
							kAnyAuthType,
							kMaxPassLength,
							password,
							exactMatch, &item);
		if(status == noErr)
		{

			OSStatus		err;
			UInt32			len;
			Boolean			replaced = false;

			if(password[0] == 0)
			{
				goto exit_store;
			}

			// --- get the Label
			attr.tag = kLabelKCItemAttr;
			attr.length = 255;
			attr.data = &label[1];
			err = KCGetAttribute(item, &attr, &len);
			if(err != noErr)
			{
				label[0] = 0;
			}

			// --- get the Comment
			attr.tag = kCommentKCItemAttr;
			attr.length = 255;
			attr.data = &comment[1];
			err = KCGetAttribute(item, &attr, &len);
			if(err != noErr)
			{
				comment[0] = 0;
			}

			// --- get the Account
			attr.tag = kAccountKCItemAttr;
			attr.length = 255;
			attr.data = &username[1];
			err = KCGetAttribute(item, &attr, &len);
			if(err != noErr)
			{
				username[0] = 0;
			}

			if(exactMatch)
			{
#if 0
				if(!hasName)
				{
					hasName = GetKeychainName(chainName);
				}
				KCRef keychain = 0;

				status = KCGetDefaultKeychain(&keychain);
				if(status == noErr)
				{
					status = KCGetKeychainName(keychain, chainName);
				}

				ParamText(label, "\p"/*service*/, username, chainName);
				if(UModalAlerts::CautionAlert(501) == 1)
				{
					return;
				}
#endif
				replaced = true;
				status = KCDeleteItem(item);
			}
		}
		else if(status == errKCItemNotFound)
		{
			status = noErr;
		}
		if(status == noErr)
		{
			gTextConverter.ConvertStringToMacP(entry->username.CStr(), username);
			gTextConverter.ConvertStringToMacP(entry->password.CStr(), password);
			servername[0] = gTextConverter.ConvertBufferToMac(uni_servername, uni_strlen(uni_servername), (char*)&servername[1], 255, kTextCoverterDefaultOptions, kStringEncodingUniChar);
			path[0] = gTextConverter.ConvertBufferToMac(uni_path, uni_path ? uni_strlen(uni_path) : 0, (char*)&path[1], 255, kTextCoverterDefaultOptions, kStringEncodingUniChar);

			status = KCAddInternetPasswordWithPath(servername, NULL, username, path,
					port, theProtocol, kAnyAuthType, password[0], &password[1], &item);

			if(status == noErr && item)
			{
				attr.tag = kCommentKCItemAttr;
				attr.length = comment[0];
				attr.data = &comment[1];
				status = KCSetAttribute(item, &attr);
			}

			if(status == noErr && item)
			{
				attr.tag = kLabelKCItemAttr;
				attr.length = label[0];
				attr.data = &label[1];
				status = KCSetAttribute(item, &attr);
			}
		}
		if(status != noErr)
		{
			if(status == errKCNoDefaultKeychain)
			{
//				UModalAlerts::StopAlert(510);
#warning "Need a dialog here saying 'There is no default keychain. Open the control panel ÓKeychain AccessÒ first and create one."
			}
			else
			{
#warning "Need a dialog here saying 'Could not add the keychain entry, because an error occurred...'"
			}
		}
		else
		{
#if 0
			LStr255	serviceName(mServerName);
			serviceName += mPath;
			if(!hasName)
			{
				hasName = GetKeychainName(chainName);
			}
//			ParamText(label, serviceName, mAccountName, chainName);
//			UModalAlerts::NoteAlert(500);								// Successfully added a keychain entry (WELL! -If we get an error, let user know, if it's OK, *DON'T* nag the user!!)
//			try {
				CAutoResizeDialog* sucessDialog = (CAutoResizeDialog*) LWindow::CreateWindow(kKeychainDialogID, nil);
				const uni_char* title = LanguageInterface::GetLanguageStringDirect(79600);
				const uni_char* message = LanguageInterface::GetLanguageStringDirect(79610);
				if (!*title) { title = UNI_L("Store in Keychain"); }
				if (!*message) { message = UNI_L("Successfully created a keychain entry"); }
				sucessDialog->SetStaticTextString(kKeychainAddressField, mAddress);
				sucessDialog->SetStaticTextString(kKeychainUsernameField, mAccountName);
				sucessDialog->SetStaticTextString(kKeychainKeychainField, chainName);
				sucessDialog->SetDialogCaption(title);
				sucessDialog->SetDialogMessage(message);
				sucessDialog->SetParentWindow(mWindow);
				sucessDialog->FindPaneByID('canc')->Hide();
				sucessDialog->FindPaneByID('icn1')->Show();
				sucessDialog->Show();
//			} catch (...) {}
#endif
		}
	}

exit_store:
	if(uni_servername)
		delete[] uni_servername;
	if(uni_path)
		delete[] uni_path;
}

OP_STATUS MacWandDataStorage::DeleteData(const uni_char* server, const uni_char* username)
{
	uni_char* 		uni_servername = NULL;
	uni_char* 		uni_path = NULL;
	OP_STATUS		result = OpStatus::ERR;

	if(server && username)
	{
		UInt16			port = 0;	// ignore port
		FourCharCode	theProtocol = kAnyProtocol;

		OSStatus		status;
		KCItemRef		item;
		Str255			password;
		Boolean			exactMatch = false;

		MacWandDataStorage::ParseResult res = ParseString(server, uni_servername, uni_path, port, theProtocol);

		if(!(res & (kServerNameValid | kPathValid)))
		{
			if(uni_servername)
				delete[] uni_servername;
			if(uni_path)
				delete[] uni_path;

			return OpStatus::ERR;
		}

		status = FindEntryInKeychain(	uni_servername, uni_strlen(uni_servername),
							username, uni_strlen(username),
							uni_path, uni_path ? uni_strlen(uni_path) : 0,
							port,
							theProtocol,
							kAnyAuthType,
							kMaxPassLength,
							password,
							exactMatch, &item);
		if(status == noErr)
		{
			if(exactMatch)
			{
				status = KCDeleteItem(item);
				if(status == noErr)
				{
					result = OpStatus::OK;
				}
			}
		}
	}

	return result;
}

OSStatus MacWandDataStorage::FindEntryInKeychain(	const uni_char* uni_server,
									size_t uni_serverlen,
									const uni_char* uni_username,
									size_t uni_usernamelen,
									const uni_char* uni_path,
									size_t uni_pathlen,
									UInt16 port,
									OSType protocol,
									OSType authtype,
									UInt32 maxlength,
									Str255 &passwordData,
									Boolean &exactMatch,
									KCItemRef* item)
{
	OSStatus		status;
	UInt32			actualLength;
	exactMatch = false;

	Str255 serverName;
	Str255 userName;
	Str255 path;
	Str255 password;

	serverName[0] = gTextConverter.ConvertBufferToMac(uni_server, uni_serverlen, (char*)&serverName[1], 255, kTextCoverterDefaultOptions, kStringEncodingUniChar);
	path[0] = gTextConverter.ConvertBufferToMac(uni_path, uni_pathlen, (char*)&path[1], 255, kTextCoverterDefaultOptions, kStringEncodingUniChar);

	if(uni_usernamelen > 0)
	{
		gTextConverter.ConvertStringToMacP(uni_username, userName);

		// 1 = ServerName
		// 2 = Realm (servermessage)
		// 3 = account
		// 4 = path
		// --- Try full package of specification without Realm but with Path ---
		status = KCFindInternetPasswordWithPath(serverName, NULL, userName, path,
										port, protocol, authtype,
										maxlength, &passwordData[1], &actualLength, item);
		if(status == noErr)
		{
			*(StringPtr(passwordData)) = actualLength;
			//password[0] = passLength;
			/*if(entry->password.Reserve(passLength+1))
			{
				gTextConverter.ConvertStringFromMacP(password, entry->password.CStr(), passLength+1);
			}*/
			exactMatch = true;
			return(noErr);
		}
		// --- Try full package of specification without Path ---
		status = KCFindInternetPasswordWithPath(serverName, NULL, userName, NULL,
										port, protocol, authtype,
										maxlength, &passwordData[1], &actualLength, item);
		if(status == noErr)
		{
			*(StringPtr(passwordData)) = actualLength;
			//password[0] = passLength;
			/*if(entry->password.Reserve(passLength+1))
			{
				gTextConverter.ConvertStringFromMacP(password, entry->password.CStr(), passLength+1);
			}*/
			return(noErr);
		}
	}
	else
	{
		// --- Try with specification of ServerName, Realm, path ---
		status = KCFindInternetPasswordWithPath(serverName, NULL, NULL, path,
										port, protocol, authtype,
										maxlength, &passwordData[1], &actualLength, item);
		if(status == noErr)
		{
			*(StringPtr(passwordData)) = actualLength;
			//password[0] = passLength;
			/*if(entry->password.Reserve(passLength+1))
			{
				gTextConverter.ConvertStringFromMacP(password, entry->password.CStr(), passLength+1);
			}*/
			exactMatch = true;
			return(noErr);
		}
		// --- Try with specification of ServerName, Realm ---
		status = KCFindInternetPasswordWithPath(serverName, NULL, NULL, NULL,
										port, protocol, authtype,
										maxlength, &passwordData[1], &actualLength, item);
		if(status == noErr)
		{
			*(StringPtr(passwordData)) = actualLength;
			//password[0] = passLength;
			/*if(entry->password.Reserve(passLength+1))
			{
				gTextConverter.ConvertStringFromMacP(password, entry->password.CStr(), passLength+1);
			}*/
			return(noErr);
		}
	}

	*(StringPtr(passwordData)) = 0;
	return status;
}

/**
* Get's the accountname and password for a KCItemRef.
* @param itemRef The KCItemRef to get details from
* @param entry The account & password will be output to here
*
* @returns TRUE if successful, FALSE otherwise
*/
BOOL MacWandDataStorage::GetAccountAndPassword(KCItemRef &itemRef, WandDataStorageEntry* entry)
{
	KCAttribute	attr;
	UInt32		len;
	OSErr		err;
	Str255		username;
	Str255		password;
	OSStatus	status;
	UInt32		actualSize;

	attr.tag = kAccountKCItemAttr;
	attr.length = 255;
	attr.data = &username[1];
	err = KCGetAttribute(itemRef, &attr, &len);
	if(noErr != err)
	{
		len = 0;
	}
	username[0] = len;

	status = KCGetData( itemRef, 255, (void*)&password[1], &actualSize);
    if (status != noErr)
    {
    	password[0] = 0;
    }
    else
    {
    	if(actualSize > 255)
    		actualSize = 255;

    	password[0] = actualSize;
    }

	if(entry->username.Reserve(len+1))
	{
		gTextConverter.ConvertStringFromMacP(username, entry->username.CStr(), len+1);
	}
	if(entry->password.Reserve(password[0]+1))
	{
		gTextConverter.ConvertStringFromMacP(password, entry->password.CStr(), password[0]+1);
	}

	return(username[0] && password[0]);
}

/**
* Parse a URL string for information.
*
* @param str The URL-string (from wand) [IN]
* @param outservername The servername [OUT]
* @param outpath The path [OUT]
* @param outport The port [OUT]
* @param outprotocol The protocol [OUT]
*
* @returns MacWandDataStorage::ParseResult telling which of the outparameters that are valid
*/
MacWandDataStorage::ParseResult MacWandDataStorage::ParseString(const uni_char* str,
																uni_char* &outservername,
																uni_char* &outpath,
																UInt16 &outport,
																FourCharCode &outprotocol)
{
	int result = kAllInvalid;

	if(str)
	{
		outprotocol = kAnyProtocol;
		result |= kProtocolValid;

		size_t uni_servernamelen = 0;

		const uni_char* uni_protocol = uni_strstr(str, UNI_L("://"));

		if(uni_protocol)
		{
			int protoLength = uni_protocol - str;
			if(uni_strnicmp(str, UNI_L("http"), protoLength) == 0)
			{
				outprotocol = kKCProtocolTypeHTTP;
			}
			else if(uni_strnicmp(str, UNI_L("ftp"), protoLength) == 0)
			{
				outprotocol = kKCProtocolTypeFTP;
			}
			else if(uni_strnicmp(str, UNI_L("irc"), protoLength) == 0)
			{
				outprotocol = kKCProtocolTypeIRC;
			}
			else if(uni_strnicmp(str, UNI_L("news"), protoLength) == 0 || uni_strnicmp(str, UNI_L("nntp"), protoLength) == 0)
			{
				outprotocol = kKCProtocolTypeNNTP;
			}
			else if(uni_strnicmp(str, UNI_L("telnet"), protoLength) == 0)
			{
				outprotocol = kKCProtocolTypeTelnet;
			}

			uni_protocol = uni_protocol + 3;
		}
		else
		{
			uni_protocol = str;
		}

		const uni_char* uni_port = uni_strchr(uni_protocol, UNI_L(':'));
		if(uni_port)
		{
			uni_servernamelen = uni_port - uni_protocol;
			uni_port++;
		}

		const uni_char* uni_path = uni_strchr(uni_protocol, UNI_L('/'));
		if(uni_path && !uni_port)
		{
			uni_servernamelen = uni_path - uni_protocol;

			outpath = UniSetNewStr(uni_path);
			if(outpath)
			{
				result |= kPathValid;
			}
		}

		if(uni_port && uni_path)
		{
			size_t uni_portlen = (uni_path-uni_port);
			uni_char *buf = new uni_char[uni_portlen+1];
			if(buf)
			{
				memcpy(buf, uni_port, uni_portlen*sizeof(uni_char));
				buf[uni_portlen] = 0;
				outport = uni_atoi(uni_port);
				result |= kPortValid;

				delete[] buf;
			}
		}
		else if(uni_port)
		{
			outport = uni_atoi(uni_port);
			result |= kPortValid;
		}

		if(uni_protocol)
		{
			outservername = new uni_char[uni_servernamelen+1];
			if(outservername)
			{
				memcpy(outservername, uni_protocol, uni_servernamelen*sizeof(uni_char));
				outservername[uni_servernamelen] = 0;
				result |= kServerNameValid;
			}
		}
	}

	return (MacWandDataStorage::ParseResult)result;
}

#endif // WAND_EXTERNAL_STORAGE
