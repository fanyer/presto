/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
**
** Copyright (C) 2000-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

/* This file contains modified versions of code found in OpenSSL */

/* ====================================================================
 * Copyright (c) 1999 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */
#include <openssl/cryptlib.h>
#include "modules/libopeay/addon/purposes.h"


/* borrowed from v3_purp.c */

/* CA checks common to all purposes
 * return codes:
 * 0 not a CA
 * 1 is a CA
 * 2 basicConstraints absent so "maybe" a CA
 * 3 basicConstraints absent but self signed V1.
 */

#define V1_ROOT (EXFLAG_V1|EXFLAG_SS)
#define ku_reject(x, usage) \
	(((x)->ex_flags & EXFLAG_KUSAGE) && !((x)->ex_kusage & (usage)))
#define xku_reject(x, usage) \
	(((x)->ex_flags & EXFLAG_XKUSAGE) && !((x)->ex_xkusage & (usage)))
#define ns_reject(x, usage) \
	(((x)->ex_flags & EXFLAG_NSCERT) && !((x)->ex_nscert & (usage)))

int trust_1oidany(X509_TRUST *trust, X509 *x, int flags);
int check_ca(const X509 *x);

/* Check SSL CA: common checks for SSL client and server */
static int check_codesign_ca(const X509 *x)
{
	int ca_ret;
	ca_ret = check_ca(x);
	if(!ca_ret) return 0;
	/* check nsCertType if present */
	if(x->ex_flags & EXFLAG_NSCERT) {
		if(x->ex_nscert & NS_OBJSIGN_CA) return ca_ret;
		return 0;
	}
	if(ca_ret != 2) return ca_ret;
	else return 0;
}

static int check_purpose_code_signing(const X509_PURPOSE *xp, const X509 *x, int ca)
{
	if(xku_reject(x,XKU_CODE_SIGN)) return 0;
	if(ca) return check_codesign_ca(x);
	if(ns_reject(x, NS_OBJSIGN)) return 0;
	/* We need to do digital signatures with it */
	if(ku_reject(x,KU_DIGITAL_SIGNATURE)) return 0;
	return 1;
}

// Called only by LibopeayModule::InitL().
int AddOperaSpecificPurposes()
{
	return X509_PURPOSE_add(OPERA_X509_PURPOSE_CODE_SIGN, X509_TRUST_OBJECT_SIGN, 0, check_purpose_code_signing, "Code signing", "codesign", NULL);
}
