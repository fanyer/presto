/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DEVICEAPI_OPVCARDENTRY_H
#define DEVICEAPI_OPVCARDENTRY_H

#ifdef DAPI_VCARD_SUPPORT

#include "modules/device_api/OpDirectoryEntry.h"
#include "modules/pi/device_api/OpAddressBook.h"

class OpVCardEntry : public OpDirectoryEntry
{
public:

	/** Imports vCard data from OpAddressBookItem.
	 *
	 * If this operation fails then no rollback is performed so the
	 * vCard may be left in inconsistent state.
	 * @param item - Adddress book item which data will be imported.
	 */
	OP_STATUS ImportFromOpAddressBookItem(OpAddressBookItem* item);

	// see RFC2426 (vCard) - FN
	OP_STATUS SetFormattedName(const uni_char* fn);
	// see RFC2426 (vCard) - N - 1st field
	OP_STATUS AddFamilyName(const uni_char* family_name);
	// see RFC2426 (vCard) - N - 2nd field
	OP_STATUS AddGivenName(const uni_char* given_name);
	// see RFC2426 (vCard) - N - 3rd field
	OP_STATUS AddAdditionalName(const uni_char* additional_name);
	// see RFC2426 (vCard) - N - 4th field
	OP_STATUS AddHonorificPrefix(const uni_char* honorific_prefix);
	// see RFC2426 (vCard) - N - 5th field
	OP_STATUS AddHonorificSuffix(const uni_char* honorific_suffix);
	// see RFC2426 (vCard) - NICKNAME
	OP_STATUS AddNickname(const uni_char* nickname);

	// see RFC2426 (vCard) - BDAY
	/**
	 * @param year - 1 based - 1 AD = 1
	 * @param month - 0 based - Jan = 0, Feb = 1...
	 * @param day - 1 based - 1st = 1
	 * @param time_of_day - time of the day in miliseconds.
	 * @param timezone_offset - timezone in miliseconds in relation to UTC
	 */
	OP_STATUS SetBirthday(int year, int month, int day, double time_of_day, double time_offset);

	enum AddressTypeFlags
	{
		ADDRESS_DOMESTIC = 0x0001,
		ADDRESS_INTERNATIONAL = 0x0002,
		ADDRESS_POSTAL = 0x0004,
		ADDRESS_PARCEL = 0x0008,
		ADDRESS_HOME = 0x0010,
		ADDRESS_WORK = 0x0020,
		ADDRESS_PREFERRED = 0x0040,
		ADDRESS_LAST = ADDRESS_PREFERRED,
		ADDRESS_DEFAULT = ADDRESS_INTERNATIONAL | ADDRESS_POSTAL | ADDRESS_PARCEL | ADDRESS_WORK, // as specified in RFC2426
	};

	// see RFC2426 (vCard) - ADR
	/// @param type_flags - flags describing type of an address
	/// values of the flags come are in AddressTypeFlags enum
	OP_STATUS AddStructuredAddress(int type_flags
		               , const uni_char* post_office_box
					   , const uni_char* extended_address
					   , const uni_char* street_address
					   , const uni_char* locality
					   , const uni_char* region
					   , const uni_char* postal_code
					   , const uni_char* country_name);

	// see RFC2426 (vCard) - LABEL
	/// @param type_flags - flags describing type of an address
	/// values of the flags come are in AddressTypeFlags enum
	OP_STATUS AddPostalLabelAddress(int type_flags
		               , const uni_char* address);

	enum TelephoneTypeFlags
	{
		TELEPHONE_HOME = 0x0001,
		TELEPHONE_WORK = 0x0002,
		TELEPHONE_CELL = 0x0004,
		TELEPHONE_PREFERRED = 0x0008,
		TELEPHONE_VOICE = 0x0010,	// if nothing is specified this is default
		TELEPHONE_FAX = 0x0020,
		TELEPHONE_VIDEO = 0x0040,
		TELEPHONE_PAGER = 0x0080,
		TELEPHONE_BBS = 0x0100,
		TELEPHONE_MODEM = 0x0200,
		TELEPHONE_CAR = 0x0400,
		TELEPHONE_ISDN = 0x0800,
		TELEPHONE_PCS = 0x1000,
		TELEPHONE_SUPPORTS_VOICE_MESSAGING = 0x2000,
		TELEPHONE_LAST = TELEPHONE_SUPPORTS_VOICE_MESSAGING, // RFC2426 'msg'
		TELEPHONE_DEFAULT = TELEPHONE_VOICE,
	};
	// see RFC2426 (vCard) - TEL
	/// @param type_flags - flags describing type of an telephone
	/// values of the flags come are in TelephoneTypeFlags enum
	OP_STATUS AddTelephoneNumber(int type_flags
		               , const uni_char* number);

	enum EMailTypeFlags
	{
		EMAIL_INTERNET = 0x0001,
		EMAIL_X400 = 0x0002,
		EMAIL_PREFERRED = 0x0004,
		EMAIL_LAST = EMAIL_PREFERRED,
		EMAIL_DEFAULT = EMAIL_INTERNET,
	};

	// see RFC2426 (vCard) - EMAIL
	/// @param type_flags - flags describing type of an email
	/// values of the flags come are in EMailTypeFlags enum
	OP_STATUS AddEMail(int type_flags
		               , const uni_char* email);

	// see RFC2426 (vCard) - MAILER
	OP_STATUS SetMailer(const uni_char* mailer);

	// see RFC2426 (vCard) - TZ
	/// @param timezone_offset - timezone in miliseconds in relation to UTC
	OP_STATUS SetTimezone(double timezone_offset);

	// see RFC2426 (vCard) - GEO
	/// @param latitude - latitude in degree
	OP_STATUS SetLocation(double latitude, double longitude);

	// see RFC2426 (vCard) - TITLE
	OP_STATUS SetOrganizationTitle(const uni_char* title);

	// see RFC2426 (vCard) - ROLE
	OP_STATUS SetOrganizationRole(const uni_char* role	);

	// see RFC2426 (vCard) - AGENT
	OP_STATUS AddAgentURI(const uni_char* uri);
	OP_STATUS AddAgentVCard(const OpVCardEntry* v_card);
	OP_STATUS AddAgentText(const uni_char* agent_text);

	// see RFC2426 (vCard) - ORG
	/**
	 * @param organizational_chain - array of pointers to zero terminated strings representing
	 *        hierarchical organizational units. Most general organizational unit is first and most
	 *        specific is last. Example:
	 *        { "Opera Software", "Engineering", "Windows Mobile Deliveries" }
	 * @param chain_length number of entries in organizational_chain
	 */
	OP_STATUS SetOrganizationalChain(const uni_char** organizational_chain, UINT32 chain_length);

	// see RFC2426 (vCard) - CATEGORIES
	OP_STATUS AddCategory(const uni_char* category);

	// see RFC2426 (vCard) - NOTE
	OP_STATUS AddNote(const uni_char* note);

	// see RFC2426 (vCard) - SORT-STRING
	OP_STATUS AddSortString(const uni_char* sort_string);

	// see RFC2426 (vCard) - URL
	OP_STATUS AddURL(const uni_char* url);

	// see RFC2426 (vCard) - PHOTO
	OP_STATUS AddPhoto(const uni_char* uri);
	OP_STATUS AddPhoto(OpFileDescriptor* opened_input_file, const uni_char* type);

	// see RFC2426 (vCard) - LOGO
	OP_STATUS AddLogo(const uni_char* uri);
	OP_STATUS AddLogo(OpFileDescriptor* opened_input_file, const uni_char* type);

	// see RFC2426 (vCard) - SOUND
	OP_STATUS AddSound(const uni_char* uri);
	OP_STATUS AddSound(OpFileDescriptor* opened_input_file, const uni_char* type);

	// missing vCard types PRODID, REV, UID, CLASS, KEY, X-XXX...

	/**
     * Prints textual reprezentation of the vCard to the file.
	 * @param opened_file - file to which the vCard will be written.
	 */
	OP_STATUS Print(OpFileDescriptor* opened_file);
private:
	OP_STATUS ApplyTypeFlags(OpDirectoryEntryContentLine* value, int flags, const uni_char* const* flags_texts, int last_flag);
	OP_STATUS ApplyAddressTypeFlags(OpDirectoryEntryContentLine* value, int flags);
	OP_STATUS ApplyTelephoneTypeFlags(OpDirectoryEntryContentLine* value, int flags);
	OP_STATUS ApplyEmailTypeFlags(OpDirectoryEntryContentLine* value, int flags);

	static BOOL FindFieldWithMeaning(OpAddressBook::OpAddressBookFieldInfo::Meaning meaning, const OpAddressBook::OpAddressBookFieldInfo* infos, UINT32 infos_count, UINT32& index);
};
#endif // DAPI_VCARD_SUPPORT

#endif // DEVICEAPI_OPVCARDENTRY_H
