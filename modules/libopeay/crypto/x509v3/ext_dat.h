/* ext_dat.h */
/* Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL
 * project 1999.
 */
/* ====================================================================
 * Copyright (c) 1999-2004 The OpenSSL Project.  All rights reserved.
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
/* This file contains a table of "standard" extensions */

#include "modules/libopeay/libopeay_arrays.h"

#ifdef HAS_COMPLEX_GLOBALS
extern const X509V3_EXT_METHOD g_v3_bcons, g_v3_nscert, g_v3_key_usage, g_v3_ext_ku;
extern const X509V3_EXT_METHOD g_v3_pkey_usage_period, g_v3_sxnet, g_v3_info, g_v3_sinfo;
extern const X509V3_EXT_METHOD g_v3_ns_ia5_list[], g_v3_alt[], g_v3_skey_id, g_v3_akey_id;
extern const X509V3_EXT_METHOD g_v3_crl_num, g_v3_crl_reason, g_v3_crl_invdate;
extern const X509V3_EXT_METHOD g_v3_delta_crl, g_v3_cpols, g_v3_crld;
extern const X509V3_EXT_METHOD g_v3_ocsp_nonce, g_v3_ocsp_accresp, g_v3_ocsp_acutoff;
extern const X509V3_EXT_METHOD g_v3_ocsp_crlid, g_v3_ocsp_nocheck, g_v3_ocsp_serviceloc;
extern const X509V3_EXT_METHOD g_v3_crl_hold, g_v3_pci;
extern const X509V3_EXT_METHOD g_v3_policy_mappings, g_v3_policy_constraints;
extern const X509V3_EXT_METHOD g_v3_name_constraints, g_v3_inhibit_anyp;
#ifndef OPENSSL_NO_RFC3779
extern const X509V3_EXT_METHOD g_v3_addr, g_v3_asid;
#endif
#endif

/* This table will be searched using OBJ_bsearch so it *must* kept in
 * order of the ext_nid values.
 */

#define OPERA_V3_METHOD_ENTRY(x) CONST_ENTRY(&OPENSSL_GLOBAL_ITEM_NAME(x) )
#define OPERA_V3_METHOD_ARRAY_ENTRY(x,i) CONST_ENTRY(&OPENSSL_GLOBAL_ARRAY_NAME(x)[i])

typedef const X509V3_EXT_METHOD *X509V3_EXT_METHOD_pointer;

OPENSSL_PREFIX_CONST_ARRAY(static, X509V3_EXT_METHOD_pointer , ext_dat__standard_exts, libopeay)
OPERA_V3_METHOD_ENTRY(v3_nscert)
OPERA_V3_METHOD_ARRAY_ENTRY(v3_ns_ia5_list,0)
OPERA_V3_METHOD_ARRAY_ENTRY(v3_ns_ia5_list,1)
OPERA_V3_METHOD_ARRAY_ENTRY(v3_ns_ia5_list,2)
OPERA_V3_METHOD_ARRAY_ENTRY(v3_ns_ia5_list,3)
OPERA_V3_METHOD_ARRAY_ENTRY(v3_ns_ia5_list,4)
OPERA_V3_METHOD_ARRAY_ENTRY(v3_ns_ia5_list,5)
OPERA_V3_METHOD_ARRAY_ENTRY(v3_ns_ia5_list,6)
OPERA_V3_METHOD_ENTRY(v3_skey_id)
OPERA_V3_METHOD_ENTRY(v3_key_usage)
OPERA_V3_METHOD_ENTRY(v3_pkey_usage_period)
OPERA_V3_METHOD_ARRAY_ENTRY(v3_alt,0)
OPERA_V3_METHOD_ARRAY_ENTRY(v3_alt,1)
OPERA_V3_METHOD_ENTRY(v3_bcons)
OPERA_V3_METHOD_ENTRY(v3_crl_num)
OPERA_V3_METHOD_ENTRY(v3_cpols)
OPERA_V3_METHOD_ENTRY(v3_akey_id)
OPERA_V3_METHOD_ENTRY(v3_crld)
OPERA_V3_METHOD_ENTRY(v3_ext_ku)
OPERA_V3_METHOD_ENTRY(v3_delta_crl)
OPERA_V3_METHOD_ENTRY(v3_crl_reason)
#ifndef OPENSSL_NO_OCSP
OPERA_V3_METHOD_ENTRY(v3_crl_invdate)
#endif
OPERA_V3_METHOD_ENTRY(v3_sxnet)
OPERA_V3_METHOD_ENTRY(v3_info)
#ifndef OPENSSL_NO_RFC3779
OPERA_V3_METHOD_ENTRY(v3_addr)
OPERA_V3_METHOD_ENTRY(v3_asid)
#endif
#ifndef OPENSSL_NO_OCSP
OPERA_V3_METHOD_ENTRY(v3_ocsp_nonce)
OPERA_V3_METHOD_ENTRY(v3_ocsp_crlid)
OPERA_V3_METHOD_ENTRY(v3_ocsp_accresp)
OPERA_V3_METHOD_ENTRY(v3_ocsp_nocheck)
OPERA_V3_METHOD_ENTRY(v3_ocsp_acutoff)
OPERA_V3_METHOD_ENTRY(v3_ocsp_serviceloc)
#endif
OPERA_V3_METHOD_ENTRY(v3_sinfo)
OPERA_V3_METHOD_ENTRY(v3_policy_constraints)
#ifndef OPENSSL_NO_OCSP
OPERA_V3_METHOD_ENTRY(v3_crl_hold)
#endif
OPERA_V3_METHOD_ENTRY(v3_pci)
OPERA_V3_METHOD_ENTRY(v3_policy_mappings)
OPERA_V3_METHOD_ENTRY(v3_name_constraints)
OPERA_V3_METHOD_ENTRY(v3_inhibit_anyp)
CONST_END(ext_dat__standard_exts)
#define standard_exts OPENSSL_GLOBAL_ARRAY_NAME(ext_dat__standard_exts)

/* Number of standard extensions */

#define STANDARD_EXTENSION_COUNT  OPENSSL_CONST_ARRAY_SIZE(ext_dat__standard_exts)
