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

#ifndef _ROOT_AUX_H_
#define _ROOT_AUX_H_

#include "modules/rootstore/extra/untrusted/rogue_md5_01_2009.h"
#include "modules/rootstore/extra/untrusted/fake_ms_objsig1.h"
#include "modules/rootstore/extra/untrusted/fake_ms_objsig2.h"
#include "modules/rootstore/extra/untrusted/nul_prefix_1.h"
#include "modules/rootstore/extra/untrusted/nul_prefix_2.h"

#include "modules/rootstore/extra/untrusted/untrusted_comodo_2011_03_01.h"
#include "modules/rootstore/extra/untrusted/untrusted_comodo_2011_03_02.h"
#include "modules/rootstore/extra/untrusted/untrusted_comodo_2011_03_03.h"
#include "modules/rootstore/extra/untrusted/untrusted_comodo_2011_03_04.h"
#include "modules/rootstore/extra/untrusted/untrusted_comodo_2011_03_05.h"
#include "modules/rootstore/extra/untrusted/untrusted_comodo_2011_03_06.h"
#include "modules/rootstore/extra/untrusted/untrusted_comodo_2011_03_07.h"
#include "modules/rootstore/extra/untrusted/untrusted_comodo_2011_03_08.h"
#include "modules/rootstore/extra/untrusted/untrusted_comodo_2011_03_09.h"

#include "modules/rootstore/defcert/diginotar2007.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr)	(sizeof(arr) / sizeof(arr[0]))
#endif

#ifdef ROOTSTORE_DISTRIBUTION_BASE
#define UNTRUSTED_BASE_ENTRY(cert, name, reason) {cert, ARRAY_SIZE(cert), name, reason, 0,0,0,0}
#define UNTRUSTED_BASE_ENTRY_RANGE(cert, name, reason, before, after, v_start, v_end) {cert, ARRAY_SIZE(cert), name, reason, before, after, v_start, v_end}
#else
#define UNTRUSTED_BASE_ENTRY_RANGE(cert, name, a_reason, a_before, a_after, v_start, v_end) \
	CONST_ENTRY_START(dercert_data, cert)    \
	CONST_ENTRY_SINGLE(dercert_len, ARRAY_SIZE(cert)) \
	CONST_ENTRY_SINGLE(nickname, name) \
	CONST_ENTRY_SINGLE(reason, a_reason)     \
	CONST_ENTRY_SINGLE(before, a_before)     \
	CONST_ENTRY_SINGLE(after,a_after)        \
	CONST_ENTRY_SINGLE(include_first_version,v_start) \
	CONST_ENTRY_SINGLE(include_last_version,v_end)    \
	CONST_ENTRY_END

#define UNTRUSTED_BASE_ENTRY(cert, name, reason) UNTRUSTED_BASE_ENTRY_RANGE(cert, name, reason, 0,0,0,0)
#endif

#if defined ROOTSTORE_DISTRIBUTION_BASE
// Serverside configurations

const static DEFCERT_DELETE_cert_st defcerts_delete_list[] =
{
};

// Untrusted certificates. Push unconditionally to all clients
const static DEFCERT_UNTRUSTED_cert_st defcerts_untrusted_list[] =
{
	UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_01, NULL, "Fraudulent certificate. Not the identified site",2,0,2,2),
	UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_02, NULL, "Fraudulent certificate. Not the identified site",2,0,2,2),
	UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_03, NULL, "Fraudulent certificate. Not the identified site",2,0,2,2),
	UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_04, NULL, "Fraudulent certificate. Not the identified site",2,0,2,2),
	//UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_05, NULL, "Fraudulent certificate. Not the identified site",2,0,2,2),
	//UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_06, NULL, "Fraudulent certificate. Not the identified site",2,0,2,2),
	UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_07, NULL, "Fraudulent certificate. Not the identified site",2,0,2,2),
	UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_08, NULL, "Fraudulent certificate. Not the identified site",2,0,2,2),
	UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_09, NULL, "Fraudulent certificate. Not the identified site",2,0,2,2),

	UNTRUSTED_BASE_ENTRY_RANGE(DIGINOTAR_2007, NULL, "Untrusted due to being hacked and having issued an unknown number of fraudulent certificates",2,0,2,2),
};
#endif

#if !defined LIBSSL_AUTO_UPDATE_ROOTS || defined ROOTSTORE_DISTRIBUTION_BASE
// Untrusted certificates. Clients fetch these if they need to.
LOCAL_PREFIX_CONST_ARRAY (static, DEFCERT_UNTRUSTED_cert_st, defcerts_untrusted_repository_list)
	UNTRUSTED_BASE_ENTRY(ROGUE_01_2009, NULL, "Fake intermediate CA certificate created using MD5 signature collisions")

	UNTRUSTED_BASE_ENTRY(FAKE_MS_OBJECTSIGN_1, NULL, "Fraudulent certificate, NOT Microsoft" )
	UNTRUSTED_BASE_ENTRY(FAKE_MS_OBJECTSIGN_2,  NULL, "Fraudulent certificate, NOT Microsoft" )
	UNTRUSTED_BASE_ENTRY(NUL_INNAME_CERT_1,  NULL, "Fraudulent certificate, uses format that can trick user agents into accepting certificate for wrong host" )
	UNTRUSTED_BASE_ENTRY(NUL_INNAME_CERT_2,  NULL, "Fraudulent certificate, uses format that can trick user agents into accepting certificate for wrong host")
	UNTRUSTED_BASE_ENTRY(UNTRUSTED_COMODO_2011_03_01, NULL, "Fraudulent certificate. Not the identified site")
	UNTRUSTED_BASE_ENTRY(UNTRUSTED_COMODO_2011_03_02, NULL, "Fraudulent certificate. Not the identified site")
	UNTRUSTED_BASE_ENTRY(UNTRUSTED_COMODO_2011_03_03, NULL, "Fraudulent certificate. Not the identified site")
	UNTRUSTED_BASE_ENTRY(UNTRUSTED_COMODO_2011_03_04, NULL, "Fraudulent certificate. Not the identified site")
	UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_05, NULL, "Fraudulent certificate. Not the identified site",0,0,0,3)
	UNTRUSTED_BASE_ENTRY_RANGE(UNTRUSTED_COMODO_2011_03_06, NULL, "Fraudulent certificate. Not the identified site",0,0,0,3)
	UNTRUSTED_BASE_ENTRY(UNTRUSTED_COMODO_2011_03_07, NULL, "Fraudulent certificate. Not the identified site")
	UNTRUSTED_BASE_ENTRY(UNTRUSTED_COMODO_2011_03_08, NULL, "Fraudulent certificate. Not the identified site")
	UNTRUSTED_BASE_ENTRY(UNTRUSTED_COMODO_2011_03_09, NULL, "Fraudulent certificate. Not the identified site")

	UNTRUSTED_BASE_ENTRY(DIGINOTAR_2007, NULL, "Untrusted due to being hacked and having issued an unknown number of fraudulent certificates")
CONST_END(defcerts_untrusted_repository_list)

#endif

#endif // _ROOT_AUX_H_
