/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef WAND_SUPPORT

#include "modules/wand/wand_module.h"
#include "modules/wand/wandmanager.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/libcrypto/include/CryptoMasterPasswordHandler.h"

#ifdef HAS_COMPLEX_GLOBALS
# define CONST_ECOM_ARRAY(name) const WandModule::ECOM_ITEM name[] = {
# define CONST_ECOM_ENTRY(x,y)         { x, y }
# define CONST_ECOM_END(name, expected_len)            };
# define CONST_ECOM_ARRAY_INIT(name) g_opera->wand_module.m_##name = name;
#else
# define CONST_ECOM_ARRAY(name) void init_##name () { WandModule::ECOM_ITEM *local = g_opera->wand_module.m_##name; int i=0;
# define CONST_ECOM_ENTRY(x,y )         local[i].field_name = x; local[i].pref_id = y; i++
# define CONST_ECOM_END(name, expected_len) ; OP_ASSERT(i == expected_len);}
# define CONST_ECOM_ARRAY_INIT(name) init_##name()
#endif // HAS_COMPLEX_GLOBALS

#ifdef WAND_ECOMMERCE_SUPPORT
// Field name from the ECommerce RFC (RFC 3106)

//    FIELD                       NAME                         Min  Notes
// 
// ship to title             Ecom_ShipTo_Postal_Name_Prefix       4  ( 1)
// ship to first name        Ecom_ShipTo_Postal_Name_First       15
// ship to middle name       Ecom_ShipTo_Postal_Name_Middle      15  ( 2)
// ship to last name         Ecom_ShipTo_Postal_Name_Last        15
// ship to name suffix       Ecom_ShipTo_Postal_Name_Suffix       4  ( 3)
// ship to company name      Ecom_ShipTo_Postal_Company          20
// ship to street line1      Ecom_ShipTo_Postal_Street_Line1     20  ( 4)
// ship to street line2      Ecom_ShipTo_Postal_Street_Line2     20  ( 4)
// ship to street line3      Ecom_ShipTo_Postal_Street_Line3     20  ( 4)
// ship to city              Ecom_ShipTo_Postal_City             22
// ship to state/province    Ecom_ShipTo_Postal_StateProv         2  ( 5)
// ship to zip/postal code   Ecom_ShipTo_Postal_PostalCode       14  ( 6)
// ship to country           Ecom_ShipTo_Postal_CountryCode       2  ( 7)
// ship to phone             Ecom_ShipTo_Telecom_Phone_Number    10  ( 8)
// ship to email             Ecom_ShipTo_Online_Email            40  ( 9)
// 
// bill to title             Ecom_BillTo_Postal_Name_Prefix       4  ( 1)
// bill to first name        Ecom_BillTo_Postal_Name_First       15
// bill to middle name       Ecom_BillTo_Postal_Name_Middle      15  ( 2)
// bill to last name         Ecom_BillTo_Postal_Name_Last        15
// bill to name suffix       Ecom_BillTo_Postal_Name_Suffix       4  ( 3)
// bill to company name      Ecom_BillTo_Postal_Company          20
// bill to street line1      Ecom_BillTo_Postal_Street_Line1     20  ( 4)
// bill to street line2      Ecom_BillTo_Postal_Street_Line2     20  ( 4)
// bill to street line3      Ecom_BillTo_Postal_Street_Line3     20  ( 4)
// bill to city              Ecom_BillTo_Postal_City             22
// bill to state/province    Ecom_BillTo_Postal_StateProv         2  ( 5)
// bill to zip/postal code   Ecom_BillTo_Postal_PostalCode       14  ( 6)
// bill to country           Ecom_BillTo_Postal_CountryCode       2  ( 7)
// bill to phone             Ecom_BillTo_Telecom_Phone_Number    10  ( 8)
// bill to email             Ecom_BillTo_Online_Email            40  ( 9)
// 
// receipt to                                                        (32)
// receipt to title          Ecom_ReceiptTo_Postal_Name_Prefix    4  ( 1)
// receipt to first name     Ecom_ReceiptTo_Postal_Name_First    15
// receipt to middle name    Ecom_ReceiptTo_Postal_Name_Middle   15  ( 2)
// receipt to last name      Ecom_ReceiptTo_Postal_Name_Last     15
// receipt to name suffix    Ecom_ReceiptTo_Postal_Name_Suffix    4  ( 3)
// receipt to company name   Ecom_ReceiptTo_Postal_Company       20
// receipt to street line1   Ecom_ReceiptTo_Postal_Street_Line1  20  ( 4)
// receipt to street line2   Ecom_ReceiptTo_Postal_Street_Line2  20  ( 4)
// receipt to street line3   Ecom_ReceiptTo_Postal_Street_Line3  20  ( 4)
// receipt to city           Ecom_ReceiptTo_Postal_City          22
// receipt to state/province Ecom_ReceiptTo_Postal_StateProv      2  ( 5)
// receipt to postal code    Ecom_ReceiptTo_Postal_PostalCode    14  ( 6)
// receipt to country        Ecom_ReceiptTo_Postal_CountryCode    2  ( 7)
// receipt to phone          Ecom_ReceiptTo_Telecom_Phone_Number 10  ( 8)
// receipt to email          Ecom_ReceiptTo_Online_Email         40  ( 9)
// 
// name on card              Ecom_Payment_Card_Name              30  (10)
// 
// card type                 Ecom_Payment_Card_Type               4  (11)
// card number               Ecom_Payment_Card_Number            19  (12)
// card verification value   Ecom_Payment_Card_Verification       4  (13)
// card expire date day      Ecom_Payment_Card_ExpDate_Day        2  (14)
// card expire date month    Ecom_Payment_Card_ExpDate_Month      2  (15)
// card expire date year     Ecom_Payment_Card_ExpDate_Year       4  (16)
// 
// card protocols            Ecom_Payment_Card_Protocol          20  (17)
// 
// consumer order ID         Ecom_ConsumerOrderID                20  (18)
// 
// user ID                   Ecom_User_ID                        40  (19)
// user password             Ecom_User_Password                  20  (19)
// 
// schema version            Ecom_SchemaVersion                  30  (20)
// 
// wallet id                 Ecom_WalletID                       40  (21)
// 
// end transaction flag      Ecom_TransactionComplete             -  (22)
// 
// The following fields are used to communicate from the merchant to the
// consumer:
// 
//    FIELD                       NAME                         Min  Notes
// 
// merchant home domain      Ecom_Merchant                      128  (23)
// processor home domain     Ecom_Processor                     128  (24)
// transaction identifier    Ecom_Transaction_ID                128  (25)
// transaction URL inquiry   Ecom_Transaction_Inquiry           500  (26)
// transaction amount        Ecom_Transaction_Amount            128  (27)
// transaction currency      Ecom_Transaction_CurrencyCode        3  (28)
// transaction date          Ecom_Transaction_Date               80  (29)
// transaction type          Ecom_Transaction_Type               40  (30)
// transaction signature     Ecom_Transaction_Signature         160  (31)
// 
// end transaction flag      Ecom_TransactionComplete             -  (22)

#define PREFS_NAMESPACE PrefsCollectionCore

CONST_ECOM_ARRAY(eCom_items)

	CONST_ECOM_ENTRY( "ShipTo_Postal_Name_Prefix", 0 ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_Name_First", PREFS_NAMESPACE::Firstname ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_Name_Middle", 0 ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_Name_Last", PREFS_NAMESPACE::Surname ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_Name_Suffix", 0 ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_Company", 0 ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_Street_Line1", PREFS_NAMESPACE::Address ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_Street_Line2", 0 ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_Street_Line3", 0 ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_City", PREFS_NAMESPACE::City ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_StateProv", PREFS_NAMESPACE::State ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_PostalCode", PREFS_NAMESPACE::Zip ),
	CONST_ECOM_ENTRY( "ShipTo_Postal_CountryCode", 0 ),
	CONST_ECOM_ENTRY( "ShipTo_Telecom_Phone_Number", PREFS_NAMESPACE::Telephone ),
	CONST_ECOM_ENTRY( "ShipTo_Online_Email", PREFS_NAMESPACE::EMail ),
	CONST_ECOM_ENTRY( "BillTo_Postal_Name_Prefix", 0 ),
	CONST_ECOM_ENTRY( "BillTo_Postal_Name_First", PREFS_NAMESPACE::Firstname ),
	CONST_ECOM_ENTRY( "BillTo_Postal_Name_Middle", 0 ),
	CONST_ECOM_ENTRY( "BillTo_Postal_Name_Last", PREFS_NAMESPACE::Surname ),
	CONST_ECOM_ENTRY( "BillTo_Postal_Name_Suffix", 0 ),
	CONST_ECOM_ENTRY( "BillTo_Postal_Company", 0 ),
	CONST_ECOM_ENTRY( "BillTo_Postal_Street_Line1", PREFS_NAMESPACE::Address ),
	CONST_ECOM_ENTRY( "BillTo_Postal_Street_Line2", 0 ),
	CONST_ECOM_ENTRY( "BillTo_Postal_Street_Line3", 0 ),
	CONST_ECOM_ENTRY( "BillTo_Postal_City", PREFS_NAMESPACE::City ),
	CONST_ECOM_ENTRY( "BillTo_Postal_StateProv", PREFS_NAMESPACE::State ),
	CONST_ECOM_ENTRY( "BillTo_Postal_PostalCode", PREFS_NAMESPACE::Zip ),
	CONST_ECOM_ENTRY( "BillTo_Postal_CountryCode", 0 ),
	CONST_ECOM_ENTRY( "BillTo_Telecom_Phone_Number", PREFS_NAMESPACE::Telephone ),
	CONST_ECOM_ENTRY( "BillTo_Online_Email", PREFS_NAMESPACE::EMail ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_Name_Prefix", 0 ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_Name_First", PREFS_NAMESPACE::Firstname ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_Name_Middle", 0 ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_Name_Last", PREFS_NAMESPACE::Surname ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_Name_Suffix", 0 ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_Company", 0 ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_Street_Line1", PREFS_NAMESPACE::Address ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_Street_Line2", 0 ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_Street_Line3", 0 ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_City", PREFS_NAMESPACE::City ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_StateProv", PREFS_NAMESPACE::State ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_PostalCode", PREFS_NAMESPACE::Zip ),
	CONST_ECOM_ENTRY( "ReceiptTo_Postal_CountryCode", 0 ),
	CONST_ECOM_ENTRY( "ReceiptTo_Telecom_Phone_Number", PREFS_NAMESPACE::Telephone ),
	CONST_ECOM_ENTRY( "ReceiptTo_Online_Email", PREFS_NAMESPACE::EMail )
	// 45 entries above
	/*,
	CONST_ECOM_ENTRY( "Payment_Card_Name", 0 ),
	CONST_ECOM_ENTRY( "Payment_Card_Type", 0 ),
	CONST_ECOM_ENTRY( "Payment_Card_Number", 0 ),
	CONST_ECOM_ENTRY( "Payment_Card_Verification", 0 ),
	CONST_ECOM_ENTRY( "Payment_Card_ExpDate_Day", 0 ),
	CONST_ECOM_ENTRY( "Payment_Card_ExpDate_Month", 0 ),
	CONST_ECOM_ENTRY( "Payment_Card_ExpDate_Year", 0 )
	*/
CONST_ECOM_END(eCom_items, ECOM_ITEMS_LEN);

#ifdef WAND_GUESS_COMMON_FORMFIELD_MEANING

CONST_ECOM_ARRAY(common_items)
//	CONST_ECOM_ENTRY( "name", PREFS_NAMESPACE::Firstname ), // "username" used on many webmail
//	CONST_ECOM_ENTRY( "user", PREFS_NAMESPACE::Firstname ), // and NOT "pass" or "name"
//	CONST_ECOM_ENTRY( "login", PREFS_NAMESPACE::EMail ),
//	CONST_ECOM_ENTRY( "loginname", PREFS_NAMESPACE::EMail ),
	CONST_ECOM_ENTRY( "mail", PREFS_NAMESPACE::EMail ), // *NOTE* if the index of this changes, then GetECommerceItem must change.
	CONST_ECOM_ENTRY( "country", PREFS_NAMESPACE::Country ),
	CONST_ECOM_ENTRY( "zip", PREFS_NAMESPACE::Zip ),
	CONST_ECOM_ENTRY( "city", PREFS_NAMESPACE::City ),
	CONST_ECOM_ENTRY( "state", PREFS_NAMESPACE::State ),
	CONST_ECOM_ENTRY( "phone", PREFS_NAMESPACE::Telephone )
CONST_ECOM_END(common_items, ECOM_COMMON_ITEMS_LEN);

#endif // WAND_GUESS_COMMON_FORMFIELD_MEANING
#endif // WAND_ECOMMERCE_SUPPORT



void WandModule::InitL(const OperaInitInfo& info)
{
	wand_manager = OP_NEW_L(WandManager, ());
	wand_manager->SetActive(g_pccore->GetIntegerPref(PrefsCollectionCore::EnableWand));

	obfuscation_password = "\x83\x7D\xFC\x0F\x8E\xB3\xE8\x69\x73\xAF\xFF";

	// Used to obfuscate data, even when master password is used (for example when reading/writing to disc)
	LEAVE_IF_NULL(wand_obfuscation_password = ObfuscatedPassword::Create((const UINT8*)obfuscation_password, op_strlen(obfuscation_password), FALSE));

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	LEAVE_IF_ERROR(wand_manager->InitSync());
#endif // SYNC_HAVE_PASSWORD_MANAGER
	// This should be done later. Way later.
#ifndef _MACINTOSH_
	OpFile wand_file;
	g_pcfiles->GetFileL(PrefsCollectionFiles::WandFile, wand_file);
	wand_manager->Open(wand_file, TRUE, TRUE);
#endif

	// If wand is not encrypted, its obfuscated. We start with an obfuscating password.
	if (!wand_manager->GetIsSecurityEncrypted())
		LEAVE_IF_NULL(wand_encryption_password = ObfuscatedPassword::Create((const UINT8*)obfuscation_password, op_strlen(obfuscation_password), FALSE));
	else
		wand_encryption_password = NULL; // If wand is encrypted, this will be set later when we ask the user for it.

#ifdef WAND_ECOMMERCE_SUPPORT
	CONST_ECOM_ARRAY_INIT(eCom_items);
# ifdef WAND_GUESS_COMMON_FORMFIELD_MEANING
	CONST_ECOM_ARRAY_INIT(common_items);
# endif // WAND_GUESS_COMMON_FORMFIELD_MEANING
#endif // WAND_ECOMMERCE_SUPPORT
}

void WandModule::Destroy()
{
	OP_DELETE(wand_manager);
	wand_manager = NULL;
	OP_DELETE(wand_encryption_password);
	OP_DELETE(wand_obfuscation_password);
}

#endif // WAND_SUPPORT
