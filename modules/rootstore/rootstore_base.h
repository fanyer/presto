/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2007-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _ROOTSTORE_BASE_H_
#define _ROOTSTORE_BASE_H_

#ifdef ROOTSTORE_DISTRIBUTION_BASE
typedef const unsigned char DEFCERT_certificate_data;

struct DEFCERT_keyid_item
{
	const byte *keyid;
	unsigned int keyid_len;		
};

struct DEFCERT_depcertificate_item
{
	const DEFCERT_certificate_data *cert;
	int before;
	int after;
};

struct DEFCERT_dependency
{
	const DEFCERT_keyid_item * const *key_ids;
	const DEFCERT_depcertificate_item *dependencies;
	const char * const *EV_OIDs;
	unsigned int EV_valid_to_year;
	unsigned int EV_valid_to_month;
	unsigned int include_first_version;
	unsigned int include_last_version;
	int before; // minor version 
	int after; // minor version 
	int update_before; // minor version 
	int update_after; // minor version 
};

struct DEFCERT_crl_overrides
{
	DEFCERT_certificate_data *certificate;
	const unsigned char * const name_field;
	unsigned int data_field_len;
	const char * const real_crl;
	uint32 before;
	uint32 after;
	unsigned int include_first_version;
	unsigned int include_last_version;
};

typedef const char * const DEFCERT_ocsp_overrides;
#endif

// This Array must be terminated with a NULL pointer in the dercert_data field of the last item
struct DEFCERT_st{
	const byte *dercert_data;
	uint32 dercert_len;
	BOOL def_warn;
	BOOL def_deny;
	BOOL def_replace;
	BOOL def_replace_flags;
	const char *nickname;
#ifndef ROOTSTORE_DISTRIBUTION_BASE
	uint32 version;
#endif
#ifdef ROOTSTORE_DISTRIBUTION_BASE
	const DEFCERT_dependency *dependencies;
#endif
};

#ifdef ROOTSTORE_DISTRIBUTION_BASE
// This Array must be terminated with a NULL pointer in the dercert_data field of the last item
// List of certificates that must be deleted on the client, if present
struct DEFCERT_DELETE_cert_st{
	const byte *dercert_data;
	uint32 dercert_len;
	const char *reason;
};
#endif

#if !defined LIBSSL_AUTO_UPDATE_ROOTS || defined ROOTSTORE_DISTRIBUTION_BASE
// This Array must be terminated with a NULL pointer in the dercert_data field of the last item
// List of certificates that must be added to the untrusted store on the client
struct DEFCERT_UNTRUSTED_cert_st{
	const byte *dercert_data;
	uint32 dercert_len;
	const char *nickname;
	const char *reason;
	uint32 before;
	uint32 after;
	unsigned int include_first_version;
	unsigned int include_last_version;
};
#endif

#ifdef ROOTSTORE_DISTRIBUTION_BASE
#define DEFCERT_ITEM_DEPEND(name) CONST_ENTRY_SINGLE(dependencies, name)
#define CONST_ENTRY_VERSION(name, arg)
#else
#define DEFCERT_ITEM_DEPEND(name)
#define CONST_ENTRY_VERSION(name, arg) CONST_ENTRY_SINGLE(name, arg)
#endif

#define DEFCERT_ITEM_BASE(name, size, warn, deny, replace, replaceflags, a_nickname, a_version, depend) \
		CONST_ENTRY_START(dercert_data, name)	\
		CONST_ENTRY_SINGLE(dercert_len, size) \
		CONST_ENTRY_SINGLE(def_warn,warn)		\
		CONST_ENTRY_SINGLE(def_deny, deny)		\
		CONST_ENTRY_SINGLE(def_replace, replace)\
		CONST_ENTRY_SINGLE(def_replace_flags, replaceflags) \
		CONST_ENTRY_SINGLE(nickname, a_nickname)\
		CONST_ENTRY_VERSION(version, a_version)	\
		DEFCERT_ITEM_DEPEND(depend)				\
		CONST_ENTRY_END

#if defined LIBSSL_AUTO_UPDATE_ROOTS && !defined ROOTSTORE_DISTRIBUTION_BASE && !defined ROOTSTORE_USE_LOCALFILE_REPOSITORY
#define DEFCERT_ITEM_BASE_VAL(name, size, warn, deny, replace, replaceflags, a_nickname, a_version, depend)
#else
#define DEFCERT_ITEM_BASE_VAL(name, size, warn, deny, replace, replaceflags, a_nickname, a_version, depend) \
				DEFCERT_ITEM_BASE(name, size, warn, deny, replace, replaceflags, a_nickname, a_version, depend)
#endif

#define DEFCERT_ITEM(name, warn, deny, nickname, version) DEFCERT_ITEM_BASE_VAL(name, sizeof(name), warn, deny, FALSE, FALSE, nickname, version, NULL)
#define DEFCERT_REPLACEITEM(name, warn, deny, nickname, version) DEFCERT_ITEM_BASE_VAL(name, sizeof(name), warn, deny, TRUE, FALSE, nickname, version, NULL)
#define DEFCERT_REPLACEITEMFLAGS(name, warn, deny, nickname, version) DEFCERT_ITEM_BASE_VAL(name, sizeof(name), warn, deny, TRUE, TRUE, nickname, version, NULL)
#define DEFCERT_ITEM_WITH_DEPEND(name, warn, deny, nickname, version, depend) DEFCERT_ITEM_BASE_VAL(name, sizeof(name), warn, deny, FALSE, FALSE, nickname, version, depend)
#define DEFCERT_REPLACEITEM_WITH_DEPEND(name, warn, deny, nickname, version, depend) DEFCERT_ITEM_BASE_VAL(name, sizeof(name), warn, deny, TRUE, FALSE, nickname, version, depend)

#define DEFCERT_ITEM_PRESHIP(name, warn, deny, nickname, version) DEFCERT_ITEM_BASE(name, sizeof(name), warn, deny, FALSE, FALSE, nickname, version, NULL)
#define DEFCERT_ITEM_WITH_DEPEND_PRESHIP(name, warn, deny, nickname, version, depend) DEFCERT_ITEM_BASE(name, sizeof(name), warn, deny, FALSE, FALSE, nickname, version, depend)
#define DEFCERT_REPLACEITEM_WITH_DEPEND_PRESHIP(name, warn, deny, nickname, version, depend) DEFCERT_ITEM_BASE(name, sizeof(name), warn, deny, TRUE, FALSE, nickname, version, depend)
#define DEFCERT_REPLACEITEM_PRESHIP(name, warn, deny, nickname, version) DEFCERT_ITEM_BASE(name, sizeof(name), warn, deny, TRUE, FALSE, nickname, version, NULL)

#if defined ROOTSTORE_DISTRIBUTION_BASE || defined SSL_CHECK_EXT_VALIDATION_POLICY
#define DEFCERT_ITEM_WITH_DEPEND_DEMO(name, warn, deny, nickname, version, depend) DEFCERT_ITEM_WITH_DEPEND(name, warn, deny, nickname, version, depend)
#define DEFCERT_ITEM_WITH_DEPEND_DEMO_PRESHIP(name, warn, deny, nickname, version, depend) DEFCERT_ITEM_WITH_DEPEND_PRESHIP(name, warn, deny, nickname, version, depend)
#else
#define DEFCERT_ITEM_WITH_DEPEND_DEMO(name, warn, deny, nickname, version, depend) 
#define DEFCERT_ITEM_WITH_DEPEND_DEMO_PRESHIP(name, warn, deny, nickname, version, depend) 
#endif

#ifdef ROOTSTORE_DISTRIBUTION_BASE
#define DECLARE_DEPENDENCY(name, keys, certs, EV_OIDs, EV_val_year, EV_val_mon) \
	static DEFCERT_dependency name= { \
		keys,        \
		certs,       \
		EV_OIDs,     \
		EV_val_year, \
		EV_val_mon,  \
		0,0,         \
		0,0,         \
		0,0          \
	}

#define DECLARE_DEPENDENCY_RANGE(name, keys, certs, EV_OIDs, EV_val_year, EV_val_mon, first_ver, last_ver) \
	static DEFCERT_dependency name= { \
		keys,        \
		certs,       \
		EV_OIDs,     \
		EV_val_year, \
		EV_val_mon,  \
		first_ver,   \
		last_ver,    \
		0,0,         \
		0,0          \
	}

#define DECLARE_DEPENDENCY_RANGE2(name, keys, certs, EV_OIDs, EV_val_year, EV_val_mon, first_ver, last_ver, before, after) \
	static DEFCERT_dependency name= { \
		keys,        \
		certs,       \
		EV_OIDs,     \
		EV_val_year, \
		EV_val_mon,  \
		first_ver,   \
		last_ver,    \
		before,      \
		after,       \
		0,0          \
	}

#define DECLARE_DEPENDENCY_UPDATERANGE(name, keys, certs, EV_OIDs, EV_val_year, EV_val_mon, first_ver, last_ver, before, after,update_before, update_after) \
	static DEFCERT_dependency name= { \
		keys,        \
		certs,       \
		EV_OIDs,     \
		EV_val_year, \
		EV_val_mon,  \
		first_ver,   \
		last_ver,    \
		before,      \
		after,       \
		update_before,\
		update_after \
	}

#define DECLARE_DEP_CERT_LIST(name) \
	static const DEFCERT_depcertificate_item name[] = {

#define DECLARE_DEP_CERT_ITEM(cert) \
		{cert, 0, 0},

#define DECLARE_DEP_CERT_ITEM_COND(cert, before, after) \
		{cert, before, after},

#define DECLARE_DEP_CERT_LIST_END \
		{NULL, 0,0} \
	}

#define DECLARE_DEP_EV_OID_LIST(name) \
	static const char *name[] = {

#define DECLARE_DEP_EV_OID(oid) oid,

#define DECLARE_DEP_EV_OID_END	\
		NULL					\
	}



#define DC_DELETE_CERT_ITEM(name, a_reason) \
		CONST_ENTRY_START(dercert_data, name)	\
		CONST_ENTRY_SINGLE(dercert_len, sizeof(name)) \
		CONST_ENTRY_SINGLE(reason, a_reason)\
		CONST_ENTRY_END

#define DC_UNTRUSTED_CERT_ITEM(name, a_nickname, a_reason) \
		CONST_ENTRY_START(dercert_data, name)	\
		CONST_ENTRY_SINGLE(dercert_len, sizeof(name)) \
		CONST_ENTRY_SINGLE(nickname, a_nickname)\
		CONST_ENTRY_SINGLE(reason, a_reason)\
		CONST_ENTRY_END
#endif

#endif // _ROOTSTORE_BASE_H_
