/* crypto/objects/obj_dat.h */

/* !!NOTE!!NOTE!! MAKE NO DIRECT CHANGES IN THIS FILE.
 * IT IS GENERATED; **ALL** CHANGES WILL BE LOST */

/* THIS FILE IS GENERATED FROM objects.h by obj_dat.pl via the
 * following command:
 * perl obj_dat.pl obj_mac.h obj_dat.h
 */

/* Copyright (C) 1995-1997 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include "modules/libopeay/libopeay_arrays.h"
#include "modules/libopeay/libopeay_implmodule.h"

#define CONST_ENTRY_NID(_sn, _ln, _nid, len, _dat, _flg) \
	CONST_ENTRY_START(sn, _sn)\
	CONST_ENTRY_SINGLE(ln, _ln)\
	CONST_ENTRY_SINGLE(nid, _nid)\
	CONST_ENTRY_SINGLE(length, len)\
	CONST_ENTRY_SINGLE(data, _dat)\
	CONST_ENTRY_SINGLE(flags, _flg)\
	CONST_ENTRY_END

#ifndef ADS12
#define LVALUES_START static const unsigned char lvalues[]={
#define I(x) x,
#define LVALUES_END };
#else
#define LVALUES_START OPENSSL_PREFIX_CONST_ARRAY(static, unsigned char, lvalues, libopeay)
#define I(x) CONST_ENTRY(x)
#define LVALUES_END CONST_END(lvalues)
#define NEED_LVALUES_OVERRIDE
#endif

/* These arrays become too big for some compilers, e.g MSDev 6. In any case their size is under control */
#undef TEST_CONST_ARRAY_SIZE
#define TEST_CONST_ARRAY_SIZE ((void)0)


#define NUM_NID 893
#define NUM_SN 886
#define NUM_LN 886
#define NUM_OBJ 840

LVALUES_START
I(0x00)                                                /* [  0] OBJ_undef */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D)        /* [  1] OBJ_rsadsi */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) /* [  7] OBJ_pkcs */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x02) I(0x02) /* [ 14] OBJ_md2 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x02) I(0x05) /* [ 22] OBJ_md5 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x03) I(0x04) /* [ 30] OBJ_rc4 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) I(0x01) /* [ 38] OBJ_rsaEncryption */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) I(0x02) /* [ 47] OBJ_md2WithRSAEncryption */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) I(0x04) /* [ 56] OBJ_md5WithRSAEncryption */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x05) I(0x01) /* [ 65] OBJ_pbeWithMD2AndDES_CBC */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x05) I(0x03) /* [ 74] OBJ_pbeWithMD5AndDES_CBC */
I(0x55)                                                /* [ 83] OBJ_X500 */
I(0x55) I(0x04)                                        /* [ 84] OBJ_X509 */
I(0x55) I(0x04) I(0x03)                                /* [ 86] OBJ_commonName */
I(0x55) I(0x04) I(0x06)                                /* [ 89] OBJ_countryName */
I(0x55) I(0x04) I(0x07)                                /* [ 92] OBJ_localityName */
I(0x55) I(0x04) I(0x08)                                /* [ 95] OBJ_stateOrProvinceName */
I(0x55) I(0x04) I(0x0A)                                /* [ 98] OBJ_organizationName */
I(0x55) I(0x04) I(0x0B)                                /* [101] OBJ_organizationalUnitName */
I(0x55) I(0x08) I(0x01) I(0x01)                        /* [104] OBJ_rsa */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x07) /* [108] OBJ_pkcs7 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x07) I(0x01) /* [116] OBJ_pkcs7_data */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x07) I(0x02) /* [125] OBJ_pkcs7_signed */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x07) I(0x03) /* [134] OBJ_pkcs7_enveloped */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x07) I(0x04) /* [143] OBJ_pkcs7_signedAndEnveloped */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x07) I(0x05) /* [152] OBJ_pkcs7_digest */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x07) I(0x06) /* [161] OBJ_pkcs7_encrypted */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x03) /* [170] OBJ_pkcs3 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x03) I(0x01) /* [178] OBJ_dhKeyAgreement */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x06)                /* [187] OBJ_des_ecb */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x09)                /* [192] OBJ_des_cfb64 */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x07)                /* [197] OBJ_des_cbc */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x11)                /* [202] OBJ_des_ede_ecb */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x81) I(0x3C) I(0x07) I(0x01) I(0x01) I(0x02) /* [207] OBJ_idea_cbc */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x03) I(0x02) /* [218] OBJ_rc2_cbc */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x12)                /* [226] OBJ_sha */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x0F)                /* [231] OBJ_shaWithRSAEncryption */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x03) I(0x07) /* [236] OBJ_des_ede3_cbc */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x08)                /* [244] OBJ_des_ofb64 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) /* [249] OBJ_pkcs9 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x01) /* [257] OBJ_pkcs9_emailAddress */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x02) /* [266] OBJ_pkcs9_unstructuredName */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x03) /* [275] OBJ_pkcs9_contentType */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x04) /* [284] OBJ_pkcs9_messageDigest */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x05) /* [293] OBJ_pkcs9_signingTime */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x06) /* [302] OBJ_pkcs9_countersignature */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x07) /* [311] OBJ_pkcs9_challengePassword */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x08) /* [320] OBJ_pkcs9_unstructuredAddress */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x09) /* [329] OBJ_pkcs9_extCertAttributes */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) /* [338] OBJ_netscape */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x01) /* [345] OBJ_netscape_cert_extension */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x02) /* [353] OBJ_netscape_data_type */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x1A)                /* [361] OBJ_sha1 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) I(0x05) /* [366] OBJ_sha1WithRSAEncryption */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x0D)                /* [375] OBJ_dsaWithSHA */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x0C)                /* [380] OBJ_dsa_2 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x05) I(0x0B) /* [385] OBJ_pbeWithSHA1AndRC2_CBC */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x05) I(0x0C) /* [394] OBJ_id_pbkdf2 */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x1B)                /* [403] OBJ_dsaWithSHA1_2 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x01) I(0x01) /* [408] OBJ_netscape_cert_type */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x01) I(0x02) /* [417] OBJ_netscape_base_url */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x01) I(0x03) /* [426] OBJ_netscape_revocation_url */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x01) I(0x04) /* [435] OBJ_netscape_ca_revocation_url */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x01) I(0x07) /* [444] OBJ_netscape_renewal_url */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x01) I(0x08) /* [453] OBJ_netscape_ca_policy_url */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x01) I(0x0C) /* [462] OBJ_netscape_ssl_server_name */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x01) I(0x0D) /* [471] OBJ_netscape_comment */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x02) I(0x05) /* [480] OBJ_netscape_cert_sequence */
I(0x55) I(0x1D)                                        /* [489] OBJ_id_ce */
I(0x55) I(0x1D) I(0x0E)                                /* [491] OBJ_subject_key_identifier */
I(0x55) I(0x1D) I(0x0F)                                /* [494] OBJ_key_usage */
I(0x55) I(0x1D) I(0x10)                                /* [497] OBJ_private_key_usage_period */
I(0x55) I(0x1D) I(0x11)                                /* [500] OBJ_subject_alt_name */
I(0x55) I(0x1D) I(0x12)                                /* [503] OBJ_issuer_alt_name */
I(0x55) I(0x1D) I(0x13)                                /* [506] OBJ_basic_constraints */
I(0x55) I(0x1D) I(0x14)                                /* [509] OBJ_crl_number */
I(0x55) I(0x1D) I(0x20)                                /* [512] OBJ_certificate_policies */
I(0x55) I(0x1D) I(0x23)                                /* [515] OBJ_authority_key_identifier */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x97) I(0x55) I(0x01) I(0x02) /* [518] OBJ_bf_cbc */
I(0x55) I(0x08) I(0x03) I(0x65)                        /* [527] OBJ_mdc2 */
I(0x55) I(0x08) I(0x03) I(0x64)                        /* [531] OBJ_mdc2WithRSA */
I(0x55) I(0x04) I(0x2A)                                /* [535] OBJ_givenName */
I(0x55) I(0x04) I(0x04)                                /* [538] OBJ_surname */
I(0x55) I(0x04) I(0x2B)                                /* [541] OBJ_initials */
I(0x55) I(0x1D) I(0x1F)                                /* [544] OBJ_crl_distribution_points */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x03)                /* [547] OBJ_md5WithRSA */
I(0x55) I(0x04) I(0x05)                                /* [552] OBJ_serialNumber */
I(0x55) I(0x04) I(0x0C)                                /* [555] OBJ_title */
I(0x55) I(0x04) I(0x0D)                                /* [558] OBJ_description */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF6) I(0x7D) I(0x07) I(0x42) I(0x0A) /* [561] OBJ_cast5_cbc */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF6) I(0x7D) I(0x07) I(0x42) I(0x0C) /* [570] OBJ_pbeWithMD5AndCast5_CBC */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x38) I(0x04) I(0x03) /* [579] OBJ_dsaWithSHA1 */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x1D)                /* [586] OBJ_sha1WithRSA */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x38) I(0x04) I(0x01) /* [591] OBJ_dsa */
I(0x2B) I(0x24) I(0x03) I(0x02) I(0x01)                /* [598] OBJ_ripemd160 */
I(0x2B) I(0x24) I(0x03) I(0x03) I(0x01) I(0x02)        /* [603] OBJ_ripemd160WithRSA */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x03) I(0x08) /* [609] OBJ_rc5_cbc */
I(0x29) I(0x01) I(0x01) I(0x85) I(0x1A) I(0x01)        /* [617] OBJ_rle_compression */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x03) I(0x08) /* [623] OBJ_zlib_compression */
I(0x55) I(0x1D) I(0x25)                                /* [634] OBJ_ext_key_usage */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07)        /* [637] OBJ_id_pkix */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) /* [643] OBJ_id_kp */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) I(0x01) /* [650] OBJ_server_auth */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) I(0x02) /* [658] OBJ_client_auth */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) I(0x03) /* [666] OBJ_code_sign */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) I(0x04) /* [674] OBJ_email_protect */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) I(0x08) /* [682] OBJ_time_stamp */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x82) I(0x37) I(0x02) I(0x01) I(0x15) /* [690] OBJ_ms_code_ind */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x82) I(0x37) I(0x02) I(0x01) I(0x16) /* [700] OBJ_ms_code_com */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x82) I(0x37) I(0x0A) I(0x03) I(0x01) /* [710] OBJ_ms_ctl_sign */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x82) I(0x37) I(0x0A) I(0x03) I(0x03) /* [720] OBJ_ms_sgc */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x82) I(0x37) I(0x0A) I(0x03) I(0x04) /* [730] OBJ_ms_efs */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x86) I(0xF8) I(0x42) I(0x04) I(0x01) /* [740] OBJ_ns_sgc */
I(0x55) I(0x1D) I(0x1B)                                /* [749] OBJ_delta_crl */
I(0x55) I(0x1D) I(0x15)                                /* [752] OBJ_crl_reason */
I(0x55) I(0x1D) I(0x18)                                /* [755] OBJ_invalidity_date */
I(0x2B) I(0x65) I(0x01) I(0x04) I(0x01)                /* [758] OBJ_sxnet */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x01) I(0x01) /* [763] OBJ_pbe_WithSHA1And128BitRC4 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x01) I(0x02) /* [773] OBJ_pbe_WithSHA1And40BitRC4 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x01) I(0x03) /* [783] OBJ_pbe_WithSHA1And3_Key_TripleDES_CBC */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x01) I(0x04) /* [793] OBJ_pbe_WithSHA1And2_Key_TripleDES_CBC */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x01) I(0x05) /* [803] OBJ_pbe_WithSHA1And128BitRC2_CBC */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x01) I(0x06) /* [813] OBJ_pbe_WithSHA1And40BitRC2_CBC */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x0A) I(0x01) I(0x01) /* [823] OBJ_keyBag */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x0A) I(0x01) I(0x02) /* [834] OBJ_pkcs8ShroudedKeyBag */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x0A) I(0x01) I(0x03) /* [845] OBJ_certBag */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x0A) I(0x01) I(0x04) /* [856] OBJ_crlBag */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x0A) I(0x01) I(0x05) /* [867] OBJ_secretBag */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x0C) I(0x0A) I(0x01) I(0x06) /* [878] OBJ_safeContentsBag */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x14) /* [889] OBJ_friendlyName */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x15) /* [898] OBJ_localKeyID */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x16) I(0x01) /* [907] OBJ_x509Certificate */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x16) I(0x02) /* [917] OBJ_sdsiCertificate */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x17) I(0x01) /* [927] OBJ_x509Crl */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x05) I(0x0D) /* [937] OBJ_pbes2 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x05) I(0x0E) /* [946] OBJ_pbmac1 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x02) I(0x07) /* [955] OBJ_hmacWithSHA1 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x02) I(0x01) /* [963] OBJ_id_qt_cps */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x02) I(0x02) /* [971] OBJ_id_qt_unotice */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x0F) /* [979] OBJ_SMIMECapabilities */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x05) I(0x04) /* [988] OBJ_pbeWithMD2AndRC2_CBC */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x05) I(0x06) /* [997] OBJ_pbeWithMD5AndRC2_CBC */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x05) I(0x0A) /* [1006] OBJ_pbeWithSHA1AndDES_CBC */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x82) I(0x37) I(0x02) I(0x01) I(0x0E) /* [1015] OBJ_ms_ext_req */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x0E) /* [1025] OBJ_ext_req */
I(0x55) I(0x04) I(0x29)                                /* [1034] OBJ_name */
I(0x55) I(0x04) I(0x2E)                                /* [1037] OBJ_dnQualifier */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) /* [1040] OBJ_id_pe */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) /* [1047] OBJ_id_ad */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x01) /* [1054] OBJ_info_access */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) /* [1062] OBJ_ad_OCSP */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x02) /* [1070] OBJ_ad_ca_issuers */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) I(0x09) /* [1078] OBJ_OCSP_sign */
I(0x28)                                                /* [1086] OBJ_iso */
I(0x2A)                                                /* [1087] OBJ_member_body */
I(0x2A) I(0x86) I(0x48)                                /* [1088] OBJ_ISO_US */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x38)                /* [1091] OBJ_X9_57 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x38) I(0x04)        /* [1096] OBJ_X9cm */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) /* [1102] OBJ_pkcs1 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x05) /* [1110] OBJ_pkcs5 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) /* [1118] OBJ_SMIME */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x00) /* [1127] OBJ_id_smime_mod */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) /* [1137] OBJ_id_smime_ct */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) /* [1147] OBJ_id_smime_aa */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x03) /* [1157] OBJ_id_smime_alg */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x04) /* [1167] OBJ_id_smime_cd */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x05) /* [1177] OBJ_id_smime_spq */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x06) /* [1187] OBJ_id_smime_cti */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x00) I(0x01) /* [1197] OBJ_id_smime_mod_cms */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x00) I(0x02) /* [1208] OBJ_id_smime_mod_ess */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x00) I(0x03) /* [1219] OBJ_id_smime_mod_oid */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x00) I(0x04) /* [1230] OBJ_id_smime_mod_msg_v3 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x00) I(0x05) /* [1241] OBJ_id_smime_mod_ets_eSignature_88 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x00) I(0x06) /* [1252] OBJ_id_smime_mod_ets_eSignature_97 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x00) I(0x07) /* [1263] OBJ_id_smime_mod_ets_eSigPolicy_88 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x00) I(0x08) /* [1274] OBJ_id_smime_mod_ets_eSigPolicy_97 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) I(0x01) /* [1285] OBJ_id_smime_ct_receipt */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) I(0x02) /* [1296] OBJ_id_smime_ct_authData */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) I(0x03) /* [1307] OBJ_id_smime_ct_publishCert */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) I(0x04) /* [1318] OBJ_id_smime_ct_TSTInfo */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) I(0x05) /* [1329] OBJ_id_smime_ct_TDTInfo */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) I(0x06) /* [1340] OBJ_id_smime_ct_contentInfo */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) I(0x07) /* [1351] OBJ_id_smime_ct_DVCSRequestData */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) I(0x08) /* [1362] OBJ_id_smime_ct_DVCSResponseData */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x01) /* [1373] OBJ_id_smime_aa_receiptRequest */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x02) /* [1384] OBJ_id_smime_aa_securityLabel */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x03) /* [1395] OBJ_id_smime_aa_mlExpandHistory */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x04) /* [1406] OBJ_id_smime_aa_contentHint */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x05) /* [1417] OBJ_id_smime_aa_msgSigDigest */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x06) /* [1428] OBJ_id_smime_aa_encapContentType */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x07) /* [1439] OBJ_id_smime_aa_contentIdentifier */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x08) /* [1450] OBJ_id_smime_aa_macValue */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x09) /* [1461] OBJ_id_smime_aa_equivalentLabels */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x0A) /* [1472] OBJ_id_smime_aa_contentReference */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x0B) /* [1483] OBJ_id_smime_aa_encrypKeyPref */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x0C) /* [1494] OBJ_id_smime_aa_signingCertificate */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x0D) /* [1505] OBJ_id_smime_aa_smimeEncryptCerts */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x0E) /* [1516] OBJ_id_smime_aa_timeStampToken */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x0F) /* [1527] OBJ_id_smime_aa_ets_sigPolicyId */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x10) /* [1538] OBJ_id_smime_aa_ets_commitmentType */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x11) /* [1549] OBJ_id_smime_aa_ets_signerLocation */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x12) /* [1560] OBJ_id_smime_aa_ets_signerAttr */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x13) /* [1571] OBJ_id_smime_aa_ets_otherSigCert */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x14) /* [1582] OBJ_id_smime_aa_ets_contentTimestamp */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x15) /* [1593] OBJ_id_smime_aa_ets_CertificateRefs */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x16) /* [1604] OBJ_id_smime_aa_ets_RevocationRefs */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x17) /* [1615] OBJ_id_smime_aa_ets_certValues */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x18) /* [1626] OBJ_id_smime_aa_ets_revocationValues */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x19) /* [1637] OBJ_id_smime_aa_ets_escTimeStamp */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x1A) /* [1648] OBJ_id_smime_aa_ets_certCRLTimestamp */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x1B) /* [1659] OBJ_id_smime_aa_ets_archiveTimeStamp */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x1C) /* [1670] OBJ_id_smime_aa_signatureType */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x02) I(0x1D) /* [1681] OBJ_id_smime_aa_dvcs_dvc */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x03) I(0x01) /* [1692] OBJ_id_smime_alg_ESDHwith3DES */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x03) I(0x02) /* [1703] OBJ_id_smime_alg_ESDHwithRC2 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x03) I(0x03) /* [1714] OBJ_id_smime_alg_3DESwrap */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x03) I(0x04) /* [1725] OBJ_id_smime_alg_RC2wrap */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x03) I(0x05) /* [1736] OBJ_id_smime_alg_ESDH */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x03) I(0x06) /* [1747] OBJ_id_smime_alg_CMS3DESwrap */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x03) I(0x07) /* [1758] OBJ_id_smime_alg_CMSRC2wrap */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x04) I(0x01) /* [1769] OBJ_id_smime_cd_ldap */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x05) I(0x01) /* [1780] OBJ_id_smime_spq_ets_sqt_uri */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x05) I(0x02) /* [1791] OBJ_id_smime_spq_ets_sqt_unotice */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x06) I(0x01) /* [1802] OBJ_id_smime_cti_ets_proofOfOrigin */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x06) I(0x02) /* [1813] OBJ_id_smime_cti_ets_proofOfReceipt */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x06) I(0x03) /* [1824] OBJ_id_smime_cti_ets_proofOfDelivery */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x06) I(0x04) /* [1835] OBJ_id_smime_cti_ets_proofOfSender */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x06) I(0x05) /* [1846] OBJ_id_smime_cti_ets_proofOfApproval */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x06) I(0x06) /* [1857] OBJ_id_smime_cti_ets_proofOfCreation */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x02) I(0x04) /* [1868] OBJ_md4 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) /* [1876] OBJ_id_pkix_mod */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x02) /* [1883] OBJ_id_qt */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) /* [1890] OBJ_id_it */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) /* [1897] OBJ_id_pkip */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x06) /* [1904] OBJ_id_alg */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) /* [1911] OBJ_id_cmc */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x08) /* [1918] OBJ_id_on */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x09) /* [1925] OBJ_id_pda */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0A) /* [1932] OBJ_id_aca */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0B) /* [1939] OBJ_id_qcs */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0C) /* [1946] OBJ_id_cct */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x01) /* [1953] OBJ_id_pkix1_explicit_88 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x02) /* [1961] OBJ_id_pkix1_implicit_88 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x03) /* [1969] OBJ_id_pkix1_explicit_93 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x04) /* [1977] OBJ_id_pkix1_implicit_93 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x05) /* [1985] OBJ_id_mod_crmf */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x06) /* [1993] OBJ_id_mod_cmc */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x07) /* [2001] OBJ_id_mod_kea_profile_88 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x08) /* [2009] OBJ_id_mod_kea_profile_93 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x09) /* [2017] OBJ_id_mod_cmp */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x0A) /* [2025] OBJ_id_mod_qualified_cert_88 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x0B) /* [2033] OBJ_id_mod_qualified_cert_93 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x0C) /* [2041] OBJ_id_mod_attribute_cert */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x0D) /* [2049] OBJ_id_mod_timestamp_protocol */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x0E) /* [2057] OBJ_id_mod_ocsp */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x0F) /* [2065] OBJ_id_mod_dvcs */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x00) I(0x10) /* [2073] OBJ_id_mod_cmp2000 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x02) /* [2081] OBJ_biometricInfo */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x03) /* [2089] OBJ_qcStatements */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x04) /* [2097] OBJ_ac_auditEntity */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x05) /* [2105] OBJ_ac_targeting */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x06) /* [2113] OBJ_aaControls */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x07) /* [2121] OBJ_sbgp_ipAddrBlock */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x08) /* [2129] OBJ_sbgp_autonomousSysNum */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x09) /* [2137] OBJ_sbgp_routerIdentifier */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x02) I(0x03) /* [2145] OBJ_textNotice */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) I(0x05) /* [2153] OBJ_ipsecEndSystem */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) I(0x06) /* [2161] OBJ_ipsecTunnel */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) I(0x07) /* [2169] OBJ_ipsecUser */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x03) I(0x0A) /* [2177] OBJ_dvcs */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x01) /* [2185] OBJ_id_it_caProtEncCert */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x02) /* [2193] OBJ_id_it_signKeyPairTypes */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x03) /* [2201] OBJ_id_it_encKeyPairTypes */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x04) /* [2209] OBJ_id_it_preferredSymmAlg */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x05) /* [2217] OBJ_id_it_caKeyUpdateInfo */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x06) /* [2225] OBJ_id_it_currentCRL */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x07) /* [2233] OBJ_id_it_unsupportedOIDs */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x08) /* [2241] OBJ_id_it_subscriptionRequest */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x09) /* [2249] OBJ_id_it_subscriptionResponse */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x0A) /* [2257] OBJ_id_it_keyPairParamReq */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x0B) /* [2265] OBJ_id_it_keyPairParamRep */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x0C) /* [2273] OBJ_id_it_revPassphrase */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x0D) /* [2281] OBJ_id_it_implicitConfirm */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x0E) /* [2289] OBJ_id_it_confirmWaitTime */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x0F) /* [2297] OBJ_id_it_origPKIMessage */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) I(0x01) /* [2305] OBJ_id_regCtrl */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) I(0x02) /* [2313] OBJ_id_regInfo */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) I(0x01) I(0x01) /* [2321] OBJ_id_regCtrl_regToken */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) I(0x01) I(0x02) /* [2330] OBJ_id_regCtrl_authenticator */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) I(0x01) I(0x03) /* [2339] OBJ_id_regCtrl_pkiPublicationInfo */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) I(0x01) I(0x04) /* [2348] OBJ_id_regCtrl_pkiArchiveOptions */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) I(0x01) I(0x05) /* [2357] OBJ_id_regCtrl_oldCertID */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) I(0x01) I(0x06) /* [2366] OBJ_id_regCtrl_protocolEncrKey */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) I(0x02) I(0x01) /* [2375] OBJ_id_regInfo_utf8Pairs */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x05) I(0x02) I(0x02) /* [2384] OBJ_id_regInfo_certReq */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x06) I(0x01) /* [2393] OBJ_id_alg_des40 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x06) I(0x02) /* [2401] OBJ_id_alg_noSignature */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x06) I(0x03) /* [2409] OBJ_id_alg_dh_sig_hmac_sha1 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x06) I(0x04) /* [2417] OBJ_id_alg_dh_pop */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x01) /* [2425] OBJ_id_cmc_statusInfo */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x02) /* [2433] OBJ_id_cmc_identification */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x03) /* [2441] OBJ_id_cmc_identityProof */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x04) /* [2449] OBJ_id_cmc_dataReturn */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x05) /* [2457] OBJ_id_cmc_transactionId */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x06) /* [2465] OBJ_id_cmc_senderNonce */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x07) /* [2473] OBJ_id_cmc_recipientNonce */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x08) /* [2481] OBJ_id_cmc_addExtensions */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x09) /* [2489] OBJ_id_cmc_encryptedPOP */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x0A) /* [2497] OBJ_id_cmc_decryptedPOP */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x0B) /* [2505] OBJ_id_cmc_lraPOPWitness */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x0F) /* [2513] OBJ_id_cmc_getCert */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x10) /* [2521] OBJ_id_cmc_getCRL */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x11) /* [2529] OBJ_id_cmc_revokeRequest */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x12) /* [2537] OBJ_id_cmc_regInfo */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x13) /* [2545] OBJ_id_cmc_responseInfo */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x15) /* [2553] OBJ_id_cmc_queryPending */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x16) /* [2561] OBJ_id_cmc_popLinkRandom */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x17) /* [2569] OBJ_id_cmc_popLinkWitness */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x07) I(0x18) /* [2577] OBJ_id_cmc_confirmCertAcceptance */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x08) I(0x01) /* [2585] OBJ_id_on_personalData */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x09) I(0x01) /* [2593] OBJ_id_pda_dateOfBirth */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x09) I(0x02) /* [2601] OBJ_id_pda_placeOfBirth */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x09) I(0x03) /* [2609] OBJ_id_pda_gender */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x09) I(0x04) /* [2617] OBJ_id_pda_countryOfCitizenship */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x09) I(0x05) /* [2625] OBJ_id_pda_countryOfResidence */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0A) I(0x01) /* [2633] OBJ_id_aca_authenticationInfo */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0A) I(0x02) /* [2641] OBJ_id_aca_accessIdentity */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0A) I(0x03) /* [2649] OBJ_id_aca_chargingIdentity */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0A) I(0x04) /* [2657] OBJ_id_aca_group */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0A) I(0x05) /* [2665] OBJ_id_aca_role */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0B) I(0x01) /* [2673] OBJ_id_qcs_pkixQCSyntax_v1 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0C) I(0x01) /* [2681] OBJ_id_cct_crs */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0C) I(0x02) /* [2689] OBJ_id_cct_PKIData */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0C) I(0x03) /* [2697] OBJ_id_cct_PKIResponse */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x03) /* [2705] OBJ_ad_timeStamping */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x04) /* [2713] OBJ_ad_dvcs */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x01) /* [2721] OBJ_id_pkix_OCSP_basic */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x02) /* [2730] OBJ_id_pkix_OCSP_Nonce */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x03) /* [2739] OBJ_id_pkix_OCSP_CrlID */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x04) /* [2748] OBJ_id_pkix_OCSP_acceptableResponses */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x05) /* [2757] OBJ_id_pkix_OCSP_noCheck */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x06) /* [2766] OBJ_id_pkix_OCSP_archiveCutoff */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x07) /* [2775] OBJ_id_pkix_OCSP_serviceLocator */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x08) /* [2784] OBJ_id_pkix_OCSP_extendedStatus */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x09) /* [2793] OBJ_id_pkix_OCSP_valid */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x0A) /* [2802] OBJ_id_pkix_OCSP_path */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x01) I(0x0B) /* [2811] OBJ_id_pkix_OCSP_trustRoot */
I(0x2B) I(0x0E) I(0x03) I(0x02)                        /* [2820] OBJ_algorithm */
I(0x2B) I(0x0E) I(0x03) I(0x02) I(0x0B)                /* [2824] OBJ_rsaSignature */
I(0x55) I(0x08)                                        /* [2829] OBJ_X500algorithms */
I(0x2B)                                                /* [2831] OBJ_org */
I(0x2B) I(0x06)                                        /* [2832] OBJ_dod */
I(0x2B) I(0x06) I(0x01)                                /* [2834] OBJ_iana */
I(0x2B) I(0x06) I(0x01) I(0x01)                        /* [2837] OBJ_Directory */
I(0x2B) I(0x06) I(0x01) I(0x02)                        /* [2841] OBJ_Management */
I(0x2B) I(0x06) I(0x01) I(0x03)                        /* [2845] OBJ_Experimental */
I(0x2B) I(0x06) I(0x01) I(0x04)                        /* [2849] OBJ_Private */
I(0x2B) I(0x06) I(0x01) I(0x05)                        /* [2853] OBJ_Security */
I(0x2B) I(0x06) I(0x01) I(0x06)                        /* [2857] OBJ_SNMPv2 */
I(0x2B) I(0x06) I(0x01) I(0x07)                        /* [2861] OBJ_Mail */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01)                /* [2865] OBJ_Enterprises */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x8B) I(0x3A) I(0x82) I(0x58) /* [2870] OBJ_dcObject */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x19) /* [2879] OBJ_domainComponent */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x0D) /* [2889] OBJ_Domain */
I(0x00)                                                /* [2899] OBJ_joint_iso_ccitt */
I(0x55) I(0x01) I(0x05)                                /* [2900] OBJ_selected_attribute_types */
I(0x55) I(0x01) I(0x05) I(0x37)                        /* [2903] OBJ_clearance */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) I(0x03) /* [2907] OBJ_md4WithRSAEncryption */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x0A) /* [2916] OBJ_ac_proxying */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x0B) /* [2924] OBJ_sinfo_access */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x0A) I(0x06) /* [2932] OBJ_id_aca_encAttrs */
I(0x55) I(0x04) I(0x48)                                /* [2940] OBJ_role */
I(0x55) I(0x1D) I(0x24)                                /* [2943] OBJ_policy_constraints */
I(0x55) I(0x1D) I(0x37)                                /* [2946] OBJ_target_information */
I(0x55) I(0x1D) I(0x38)                                /* [2949] OBJ_no_rev_avail */
I(0x00)                                                /* [2952] OBJ_ccitt */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D)                /* [2953] OBJ_ansi_X9_62 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x01) I(0x01) /* [2958] OBJ_X9_62_prime_field */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x01) I(0x02) /* [2965] OBJ_X9_62_characteristic_two_field */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x02) I(0x01) /* [2972] OBJ_X9_62_id_ecPublicKey */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x01) I(0x01) /* [2979] OBJ_X9_62_prime192v1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x01) I(0x02) /* [2987] OBJ_X9_62_prime192v2 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x01) I(0x03) /* [2995] OBJ_X9_62_prime192v3 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x01) I(0x04) /* [3003] OBJ_X9_62_prime239v1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x01) I(0x05) /* [3011] OBJ_X9_62_prime239v2 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x01) I(0x06) /* [3019] OBJ_X9_62_prime239v3 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x01) I(0x07) /* [3027] OBJ_X9_62_prime256v1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x04) I(0x01) /* [3035] OBJ_ecdsa_with_SHA1 */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x82) I(0x37) I(0x11) I(0x01) /* [3042] OBJ_ms_csp_name */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x01) /* [3051] OBJ_aes_128_ecb */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x02) /* [3060] OBJ_aes_128_cbc */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x03) /* [3069] OBJ_aes_128_ofb128 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x04) /* [3078] OBJ_aes_128_cfb128 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x15) /* [3087] OBJ_aes_192_ecb */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x16) /* [3096] OBJ_aes_192_cbc */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x17) /* [3105] OBJ_aes_192_ofb128 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x18) /* [3114] OBJ_aes_192_cfb128 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x29) /* [3123] OBJ_aes_256_ecb */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x2A) /* [3132] OBJ_aes_256_cbc */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x2B) /* [3141] OBJ_aes_256_ofb128 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x2C) /* [3150] OBJ_aes_256_cfb128 */
I(0x55) I(0x1D) I(0x17)                                /* [3159] OBJ_hold_instruction_code */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x38) I(0x02) I(0x01) /* [3162] OBJ_hold_instruction_none */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x38) I(0x02) I(0x02) /* [3169] OBJ_hold_instruction_call_issuer */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x38) I(0x02) I(0x03) /* [3176] OBJ_hold_instruction_reject */
I(0x09)                                                /* [3183] OBJ_data */
I(0x09) I(0x92) I(0x26)                                /* [3184] OBJ_pss */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) /* [3187] OBJ_ucl */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) /* [3194] OBJ_pilot */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) /* [3202] OBJ_pilotAttributeType */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x03) /* [3211] OBJ_pilotAttributeSyntax */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) /* [3220] OBJ_pilotObjectClass */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x0A) /* [3229] OBJ_pilotGroups */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x03) I(0x04) /* [3238] OBJ_iA5StringSyntax */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x03) I(0x05) /* [3248] OBJ_caseIgnoreIA5StringSyntax */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x03) /* [3258] OBJ_pilotObject */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x04) /* [3268] OBJ_pilotPerson */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x05) /* [3278] OBJ_account */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x06) /* [3288] OBJ_document */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x07) /* [3298] OBJ_room */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x09) /* [3308] OBJ_documentSeries */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x0E) /* [3318] OBJ_rFC822localPart */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x0F) /* [3328] OBJ_dNSDomain */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x11) /* [3338] OBJ_domainRelatedObject */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x12) /* [3348] OBJ_friendlyCountry */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x13) /* [3358] OBJ_simpleSecurityObject */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x14) /* [3368] OBJ_pilotOrganization */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x15) /* [3378] OBJ_pilotDSA */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x04) I(0x16) /* [3388] OBJ_qualityLabelledData */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x01) /* [3398] OBJ_userId */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x02) /* [3408] OBJ_textEncodedORAddress */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x03) /* [3418] OBJ_rfc822Mailbox */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x04) /* [3428] OBJ_info */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x05) /* [3438] OBJ_favouriteDrink */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x06) /* [3448] OBJ_roomNumber */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x07) /* [3458] OBJ_photo */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x08) /* [3468] OBJ_userClass */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x09) /* [3478] OBJ_host */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x0A) /* [3488] OBJ_manager */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x0B) /* [3498] OBJ_documentIdentifier */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x0C) /* [3508] OBJ_documentTitle */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x0D) /* [3518] OBJ_documentVersion */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x0E) /* [3528] OBJ_documentAuthor */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x0F) /* [3538] OBJ_documentLocation */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x14) /* [3548] OBJ_homeTelephoneNumber */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x15) /* [3558] OBJ_secretary */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x16) /* [3568] OBJ_otherMailbox */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x17) /* [3578] OBJ_lastModifiedTime */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x18) /* [3588] OBJ_lastModifiedBy */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x1A) /* [3598] OBJ_aRecord */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x1B) /* [3608] OBJ_pilotAttributeType27 */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x1C) /* [3618] OBJ_mXRecord */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x1D) /* [3628] OBJ_nSRecord */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x1E) /* [3638] OBJ_sOARecord */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x1F) /* [3648] OBJ_cNAMERecord */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x25) /* [3658] OBJ_associatedDomain */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x26) /* [3668] OBJ_associatedName */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x27) /* [3678] OBJ_homePostalAddress */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x28) /* [3688] OBJ_personalTitle */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x29) /* [3698] OBJ_mobileTelephoneNumber */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x2A) /* [3708] OBJ_pagerTelephoneNumber */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x2B) /* [3718] OBJ_friendlyCountryName */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x2D) /* [3728] OBJ_organizationalStatus */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x2E) /* [3738] OBJ_janetMailbox */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x2F) /* [3748] OBJ_mailPreferenceOption */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x30) /* [3758] OBJ_buildingName */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x31) /* [3768] OBJ_dSAQuality */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x32) /* [3778] OBJ_singleLevelQuality */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x33) /* [3788] OBJ_subtreeMinimumQuality */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x34) /* [3798] OBJ_subtreeMaximumQuality */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x35) /* [3808] OBJ_personalSignature */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x36) /* [3818] OBJ_dITRedirect */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x37) /* [3828] OBJ_audio */
I(0x09) I(0x92) I(0x26) I(0x89) I(0x93) I(0xF2) I(0x2C) I(0x64) I(0x01) I(0x38) /* [3838] OBJ_documentPublisher */
I(0x55) I(0x04) I(0x2D)                                /* [3848] OBJ_x500UniqueIdentifier */
I(0x2B) I(0x06) I(0x01) I(0x07) I(0x01)                /* [3851] OBJ_mime_mhs */
I(0x2B) I(0x06) I(0x01) I(0x07) I(0x01) I(0x01)        /* [3856] OBJ_mime_mhs_headings */
I(0x2B) I(0x06) I(0x01) I(0x07) I(0x01) I(0x02)        /* [3862] OBJ_mime_mhs_bodies */
I(0x2B) I(0x06) I(0x01) I(0x07) I(0x01) I(0x01) I(0x01) /* [3868] OBJ_id_hex_partial_message */
I(0x2B) I(0x06) I(0x01) I(0x07) I(0x01) I(0x01) I(0x02) /* [3875] OBJ_id_hex_multipart_message */
I(0x55) I(0x04) I(0x2C)                                /* [3882] OBJ_generationQualifier */
I(0x55) I(0x04) I(0x41)                                /* [3885] OBJ_pseudonym */
I(0x67) I(0x2A)                                        /* [3888] OBJ_id_set */
I(0x67) I(0x2A) I(0x00)                                /* [3890] OBJ_set_ctype */
I(0x67) I(0x2A) I(0x01)                                /* [3893] OBJ_set_msgExt */
I(0x67) I(0x2A) I(0x03)                                /* [3896] OBJ_set_attr */
I(0x67) I(0x2A) I(0x05)                                /* [3899] OBJ_set_policy */
I(0x67) I(0x2A) I(0x07)                                /* [3902] OBJ_set_certExt */
I(0x67) I(0x2A) I(0x08)                                /* [3905] OBJ_set_brand */
I(0x67) I(0x2A) I(0x00) I(0x00)                        /* [3908] OBJ_setct_PANData */
I(0x67) I(0x2A) I(0x00) I(0x01)                        /* [3912] OBJ_setct_PANToken */
I(0x67) I(0x2A) I(0x00) I(0x02)                        /* [3916] OBJ_setct_PANOnly */
I(0x67) I(0x2A) I(0x00) I(0x03)                        /* [3920] OBJ_setct_OIData */
I(0x67) I(0x2A) I(0x00) I(0x04)                        /* [3924] OBJ_setct_PI */
I(0x67) I(0x2A) I(0x00) I(0x05)                        /* [3928] OBJ_setct_PIData */
I(0x67) I(0x2A) I(0x00) I(0x06)                        /* [3932] OBJ_setct_PIDataUnsigned */
I(0x67) I(0x2A) I(0x00) I(0x07)                        /* [3936] OBJ_setct_HODInput */
I(0x67) I(0x2A) I(0x00) I(0x08)                        /* [3940] OBJ_setct_AuthResBaggage */
I(0x67) I(0x2A) I(0x00) I(0x09)                        /* [3944] OBJ_setct_AuthRevReqBaggage */
I(0x67) I(0x2A) I(0x00) I(0x0A)                        /* [3948] OBJ_setct_AuthRevResBaggage */
I(0x67) I(0x2A) I(0x00) I(0x0B)                        /* [3952] OBJ_setct_CapTokenSeq */
I(0x67) I(0x2A) I(0x00) I(0x0C)                        /* [3956] OBJ_setct_PInitResData */
I(0x67) I(0x2A) I(0x00) I(0x0D)                        /* [3960] OBJ_setct_PI_TBS */
I(0x67) I(0x2A) I(0x00) I(0x0E)                        /* [3964] OBJ_setct_PResData */
I(0x67) I(0x2A) I(0x00) I(0x10)                        /* [3968] OBJ_setct_AuthReqTBS */
I(0x67) I(0x2A) I(0x00) I(0x11)                        /* [3972] OBJ_setct_AuthResTBS */
I(0x67) I(0x2A) I(0x00) I(0x12)                        /* [3976] OBJ_setct_AuthResTBSX */
I(0x67) I(0x2A) I(0x00) I(0x13)                        /* [3980] OBJ_setct_AuthTokenTBS */
I(0x67) I(0x2A) I(0x00) I(0x14)                        /* [3984] OBJ_setct_CapTokenData */
I(0x67) I(0x2A) I(0x00) I(0x15)                        /* [3988] OBJ_setct_CapTokenTBS */
I(0x67) I(0x2A) I(0x00) I(0x16)                        /* [3992] OBJ_setct_AcqCardCodeMsg */
I(0x67) I(0x2A) I(0x00) I(0x17)                        /* [3996] OBJ_setct_AuthRevReqTBS */
I(0x67) I(0x2A) I(0x00) I(0x18)                        /* [4000] OBJ_setct_AuthRevResData */
I(0x67) I(0x2A) I(0x00) I(0x19)                        /* [4004] OBJ_setct_AuthRevResTBS */
I(0x67) I(0x2A) I(0x00) I(0x1A)                        /* [4008] OBJ_setct_CapReqTBS */
I(0x67) I(0x2A) I(0x00) I(0x1B)                        /* [4012] OBJ_setct_CapReqTBSX */
I(0x67) I(0x2A) I(0x00) I(0x1C)                        /* [4016] OBJ_setct_CapResData */
I(0x67) I(0x2A) I(0x00) I(0x1D)                        /* [4020] OBJ_setct_CapRevReqTBS */
I(0x67) I(0x2A) I(0x00) I(0x1E)                        /* [4024] OBJ_setct_CapRevReqTBSX */
I(0x67) I(0x2A) I(0x00) I(0x1F)                        /* [4028] OBJ_setct_CapRevResData */
I(0x67) I(0x2A) I(0x00) I(0x20)                        /* [4032] OBJ_setct_CredReqTBS */
I(0x67) I(0x2A) I(0x00) I(0x21)                        /* [4036] OBJ_setct_CredReqTBSX */
I(0x67) I(0x2A) I(0x00) I(0x22)                        /* [4040] OBJ_setct_CredResData */
I(0x67) I(0x2A) I(0x00) I(0x23)                        /* [4044] OBJ_setct_CredRevReqTBS */
I(0x67) I(0x2A) I(0x00) I(0x24)                        /* [4048] OBJ_setct_CredRevReqTBSX */
I(0x67) I(0x2A) I(0x00) I(0x25)                        /* [4052] OBJ_setct_CredRevResData */
I(0x67) I(0x2A) I(0x00) I(0x26)                        /* [4056] OBJ_setct_PCertReqData */
I(0x67) I(0x2A) I(0x00) I(0x27)                        /* [4060] OBJ_setct_PCertResTBS */
I(0x67) I(0x2A) I(0x00) I(0x28)                        /* [4064] OBJ_setct_BatchAdminReqData */
I(0x67) I(0x2A) I(0x00) I(0x29)                        /* [4068] OBJ_setct_BatchAdminResData */
I(0x67) I(0x2A) I(0x00) I(0x2A)                        /* [4072] OBJ_setct_CardCInitResTBS */
I(0x67) I(0x2A) I(0x00) I(0x2B)                        /* [4076] OBJ_setct_MeAqCInitResTBS */
I(0x67) I(0x2A) I(0x00) I(0x2C)                        /* [4080] OBJ_setct_RegFormResTBS */
I(0x67) I(0x2A) I(0x00) I(0x2D)                        /* [4084] OBJ_setct_CertReqData */
I(0x67) I(0x2A) I(0x00) I(0x2E)                        /* [4088] OBJ_setct_CertReqTBS */
I(0x67) I(0x2A) I(0x00) I(0x2F)                        /* [4092] OBJ_setct_CertResData */
I(0x67) I(0x2A) I(0x00) I(0x30)                        /* [4096] OBJ_setct_CertInqReqTBS */
I(0x67) I(0x2A) I(0x00) I(0x31)                        /* [4100] OBJ_setct_ErrorTBS */
I(0x67) I(0x2A) I(0x00) I(0x32)                        /* [4104] OBJ_setct_PIDualSignedTBE */
I(0x67) I(0x2A) I(0x00) I(0x33)                        /* [4108] OBJ_setct_PIUnsignedTBE */
I(0x67) I(0x2A) I(0x00) I(0x34)                        /* [4112] OBJ_setct_AuthReqTBE */
I(0x67) I(0x2A) I(0x00) I(0x35)                        /* [4116] OBJ_setct_AuthResTBE */
I(0x67) I(0x2A) I(0x00) I(0x36)                        /* [4120] OBJ_setct_AuthResTBEX */
I(0x67) I(0x2A) I(0x00) I(0x37)                        /* [4124] OBJ_setct_AuthTokenTBE */
I(0x67) I(0x2A) I(0x00) I(0x38)                        /* [4128] OBJ_setct_CapTokenTBE */
I(0x67) I(0x2A) I(0x00) I(0x39)                        /* [4132] OBJ_setct_CapTokenTBEX */
I(0x67) I(0x2A) I(0x00) I(0x3A)                        /* [4136] OBJ_setct_AcqCardCodeMsgTBE */
I(0x67) I(0x2A) I(0x00) I(0x3B)                        /* [4140] OBJ_setct_AuthRevReqTBE */
I(0x67) I(0x2A) I(0x00) I(0x3C)                        /* [4144] OBJ_setct_AuthRevResTBE */
I(0x67) I(0x2A) I(0x00) I(0x3D)                        /* [4148] OBJ_setct_AuthRevResTBEB */
I(0x67) I(0x2A) I(0x00) I(0x3E)                        /* [4152] OBJ_setct_CapReqTBE */
I(0x67) I(0x2A) I(0x00) I(0x3F)                        /* [4156] OBJ_setct_CapReqTBEX */
I(0x67) I(0x2A) I(0x00) I(0x40)                        /* [4160] OBJ_setct_CapResTBE */
I(0x67) I(0x2A) I(0x00) I(0x41)                        /* [4164] OBJ_setct_CapRevReqTBE */
I(0x67) I(0x2A) I(0x00) I(0x42)                        /* [4168] OBJ_setct_CapRevReqTBEX */
I(0x67) I(0x2A) I(0x00) I(0x43)                        /* [4172] OBJ_setct_CapRevResTBE */
I(0x67) I(0x2A) I(0x00) I(0x44)                        /* [4176] OBJ_setct_CredReqTBE */
I(0x67) I(0x2A) I(0x00) I(0x45)                        /* [4180] OBJ_setct_CredReqTBEX */
I(0x67) I(0x2A) I(0x00) I(0x46)                        /* [4184] OBJ_setct_CredResTBE */
I(0x67) I(0x2A) I(0x00) I(0x47)                        /* [4188] OBJ_setct_CredRevReqTBE */
I(0x67) I(0x2A) I(0x00) I(0x48)                        /* [4192] OBJ_setct_CredRevReqTBEX */
I(0x67) I(0x2A) I(0x00) I(0x49)                        /* [4196] OBJ_setct_CredRevResTBE */
I(0x67) I(0x2A) I(0x00) I(0x4A)                        /* [4200] OBJ_setct_BatchAdminReqTBE */
I(0x67) I(0x2A) I(0x00) I(0x4B)                        /* [4204] OBJ_setct_BatchAdminResTBE */
I(0x67) I(0x2A) I(0x00) I(0x4C)                        /* [4208] OBJ_setct_RegFormReqTBE */
I(0x67) I(0x2A) I(0x00) I(0x4D)                        /* [4212] OBJ_setct_CertReqTBE */
I(0x67) I(0x2A) I(0x00) I(0x4E)                        /* [4216] OBJ_setct_CertReqTBEX */
I(0x67) I(0x2A) I(0x00) I(0x4F)                        /* [4220] OBJ_setct_CertResTBE */
I(0x67) I(0x2A) I(0x00) I(0x50)                        /* [4224] OBJ_setct_CRLNotificationTBS */
I(0x67) I(0x2A) I(0x00) I(0x51)                        /* [4228] OBJ_setct_CRLNotificationResTBS */
I(0x67) I(0x2A) I(0x00) I(0x52)                        /* [4232] OBJ_setct_BCIDistributionTBS */
I(0x67) I(0x2A) I(0x01) I(0x01)                        /* [4236] OBJ_setext_genCrypt */
I(0x67) I(0x2A) I(0x01) I(0x03)                        /* [4240] OBJ_setext_miAuth */
I(0x67) I(0x2A) I(0x01) I(0x04)                        /* [4244] OBJ_setext_pinSecure */
I(0x67) I(0x2A) I(0x01) I(0x05)                        /* [4248] OBJ_setext_pinAny */
I(0x67) I(0x2A) I(0x01) I(0x07)                        /* [4252] OBJ_setext_track2 */
I(0x67) I(0x2A) I(0x01) I(0x08)                        /* [4256] OBJ_setext_cv */
I(0x67) I(0x2A) I(0x05) I(0x00)                        /* [4260] OBJ_set_policy_root */
I(0x67) I(0x2A) I(0x07) I(0x00)                        /* [4264] OBJ_setCext_hashedRoot */
I(0x67) I(0x2A) I(0x07) I(0x01)                        /* [4268] OBJ_setCext_certType */
I(0x67) I(0x2A) I(0x07) I(0x02)                        /* [4272] OBJ_setCext_merchData */
I(0x67) I(0x2A) I(0x07) I(0x03)                        /* [4276] OBJ_setCext_cCertRequired */
I(0x67) I(0x2A) I(0x07) I(0x04)                        /* [4280] OBJ_setCext_tunneling */
I(0x67) I(0x2A) I(0x07) I(0x05)                        /* [4284] OBJ_setCext_setExt */
I(0x67) I(0x2A) I(0x07) I(0x06)                        /* [4288] OBJ_setCext_setQualf */
I(0x67) I(0x2A) I(0x07) I(0x07)                        /* [4292] OBJ_setCext_PGWYcapabilities */
I(0x67) I(0x2A) I(0x07) I(0x08)                        /* [4296] OBJ_setCext_TokenIdentifier */
I(0x67) I(0x2A) I(0x07) I(0x09)                        /* [4300] OBJ_setCext_Track2Data */
I(0x67) I(0x2A) I(0x07) I(0x0A)                        /* [4304] OBJ_setCext_TokenType */
I(0x67) I(0x2A) I(0x07) I(0x0B)                        /* [4308] OBJ_setCext_IssuerCapabilities */
I(0x67) I(0x2A) I(0x03) I(0x00)                        /* [4312] OBJ_setAttr_Cert */
I(0x67) I(0x2A) I(0x03) I(0x01)                        /* [4316] OBJ_setAttr_PGWYcap */
I(0x67) I(0x2A) I(0x03) I(0x02)                        /* [4320] OBJ_setAttr_TokenType */
I(0x67) I(0x2A) I(0x03) I(0x03)                        /* [4324] OBJ_setAttr_IssCap */
I(0x67) I(0x2A) I(0x03) I(0x00) I(0x00)                /* [4328] OBJ_set_rootKeyThumb */
I(0x67) I(0x2A) I(0x03) I(0x00) I(0x01)                /* [4333] OBJ_set_addPolicy */
I(0x67) I(0x2A) I(0x03) I(0x02) I(0x01)                /* [4338] OBJ_setAttr_Token_EMV */
I(0x67) I(0x2A) I(0x03) I(0x02) I(0x02)                /* [4343] OBJ_setAttr_Token_B0Prime */
I(0x67) I(0x2A) I(0x03) I(0x03) I(0x03)                /* [4348] OBJ_setAttr_IssCap_CVM */
I(0x67) I(0x2A) I(0x03) I(0x03) I(0x04)                /* [4353] OBJ_setAttr_IssCap_T2 */
I(0x67) I(0x2A) I(0x03) I(0x03) I(0x05)                /* [4358] OBJ_setAttr_IssCap_Sig */
I(0x67) I(0x2A) I(0x03) I(0x03) I(0x03) I(0x01)        /* [4363] OBJ_setAttr_GenCryptgrm */
I(0x67) I(0x2A) I(0x03) I(0x03) I(0x04) I(0x01)        /* [4369] OBJ_setAttr_T2Enc */
I(0x67) I(0x2A) I(0x03) I(0x03) I(0x04) I(0x02)        /* [4375] OBJ_setAttr_T2cleartxt */
I(0x67) I(0x2A) I(0x03) I(0x03) I(0x05) I(0x01)        /* [4381] OBJ_setAttr_TokICCsig */
I(0x67) I(0x2A) I(0x03) I(0x03) I(0x05) I(0x02)        /* [4387] OBJ_setAttr_SecDevSig */
I(0x67) I(0x2A) I(0x08) I(0x01)                        /* [4393] OBJ_set_brand_IATA_ATA */
I(0x67) I(0x2A) I(0x08) I(0x1E)                        /* [4397] OBJ_set_brand_Diners */
I(0x67) I(0x2A) I(0x08) I(0x22)                        /* [4401] OBJ_set_brand_AmericanExpress */
I(0x67) I(0x2A) I(0x08) I(0x23)                        /* [4405] OBJ_set_brand_JCB */
I(0x67) I(0x2A) I(0x08) I(0x04)                        /* [4409] OBJ_set_brand_Visa */
I(0x67) I(0x2A) I(0x08) I(0x05)                        /* [4413] OBJ_set_brand_MasterCard */
I(0x67) I(0x2A) I(0x08) I(0xAE) I(0x7B)                /* [4417] OBJ_set_brand_Novus */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x03) I(0x0A) /* [4422] OBJ_des_cdmf */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) I(0x06) /* [4430] OBJ_rsaOAEPEncryptionSET */
I(0x00)                                                /* [4439] OBJ_itu_t */
I(0x50)                                                /* [4440] OBJ_joint_iso_itu_t */
I(0x67)                                                /* [4441] OBJ_international_organizations */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x82) I(0x37) I(0x14) I(0x02) I(0x02) /* [4442] OBJ_ms_smartcard_login */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x82) I(0x37) I(0x14) I(0x02) I(0x03) /* [4452] OBJ_ms_upn */
I(0x55) I(0x04) I(0x09)                                /* [4462] OBJ_streetAddress */
I(0x55) I(0x04) I(0x11)                                /* [4465] OBJ_postalCode */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x15) /* [4468] OBJ_id_ppl */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x01) I(0x0E) /* [4475] OBJ_proxyCertInfo */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x15) I(0x00) /* [4483] OBJ_id_ppl_anyLanguage */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x15) I(0x01) /* [4491] OBJ_id_ppl_inheritAll */
I(0x55) I(0x1D) I(0x1E)                                /* [4499] OBJ_name_constraints */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x15) I(0x02) /* [4502] OBJ_Independent */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) I(0x0B) /* [4510] OBJ_sha256WithRSAEncryption */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) I(0x0C) /* [4519] OBJ_sha384WithRSAEncryption */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) I(0x0D) /* [4528] OBJ_sha512WithRSAEncryption */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x01) I(0x0E) /* [4537] OBJ_sha224WithRSAEncryption */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x02) I(0x01) /* [4546] OBJ_sha256 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x02) I(0x02) /* [4555] OBJ_sha384 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x02) I(0x03) /* [4564] OBJ_sha512 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x02) I(0x04) /* [4573] OBJ_sha224 */
I(0x2B)                                                /* [4582] OBJ_identified_organization */
I(0x2B) I(0x81) I(0x04)                                /* [4583] OBJ_certicom_arc */
I(0x67) I(0x2B)                                        /* [4586] OBJ_wap */
I(0x67) I(0x2B) I(0x01)                                /* [4588] OBJ_wap_wsg */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x01) I(0x02) I(0x03) /* [4591] OBJ_X9_62_id_characteristic_two_basis */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x01) I(0x02) I(0x03) I(0x01) /* [4599] OBJ_X9_62_onBasis */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x01) I(0x02) I(0x03) I(0x02) /* [4608] OBJ_X9_62_tpBasis */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x01) I(0x02) I(0x03) I(0x03) /* [4617] OBJ_X9_62_ppBasis */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x01) /* [4626] OBJ_X9_62_c2pnb163v1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x02) /* [4634] OBJ_X9_62_c2pnb163v2 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x03) /* [4642] OBJ_X9_62_c2pnb163v3 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x04) /* [4650] OBJ_X9_62_c2pnb176v1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x05) /* [4658] OBJ_X9_62_c2tnb191v1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x06) /* [4666] OBJ_X9_62_c2tnb191v2 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x07) /* [4674] OBJ_X9_62_c2tnb191v3 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x08) /* [4682] OBJ_X9_62_c2onb191v4 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x09) /* [4690] OBJ_X9_62_c2onb191v5 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x0A) /* [4698] OBJ_X9_62_c2pnb208w1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x0B) /* [4706] OBJ_X9_62_c2tnb239v1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x0C) /* [4714] OBJ_X9_62_c2tnb239v2 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x0D) /* [4722] OBJ_X9_62_c2tnb239v3 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x0E) /* [4730] OBJ_X9_62_c2onb239v4 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x0F) /* [4738] OBJ_X9_62_c2onb239v5 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x10) /* [4746] OBJ_X9_62_c2pnb272w1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x11) /* [4754] OBJ_X9_62_c2pnb304w1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x12) /* [4762] OBJ_X9_62_c2tnb359v1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x13) /* [4770] OBJ_X9_62_c2pnb368w1 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x03) I(0x00) I(0x14) /* [4778] OBJ_X9_62_c2tnb431r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x06)                /* [4786] OBJ_secp112r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x07)                /* [4791] OBJ_secp112r2 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x1C)                /* [4796] OBJ_secp128r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x1D)                /* [4801] OBJ_secp128r2 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x09)                /* [4806] OBJ_secp160k1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x08)                /* [4811] OBJ_secp160r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x1E)                /* [4816] OBJ_secp160r2 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x1F)                /* [4821] OBJ_secp192k1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x20)                /* [4826] OBJ_secp224k1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x21)                /* [4831] OBJ_secp224r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x0A)                /* [4836] OBJ_secp256k1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x22)                /* [4841] OBJ_secp384r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x23)                /* [4846] OBJ_secp521r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x04)                /* [4851] OBJ_sect113r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x05)                /* [4856] OBJ_sect113r2 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x16)                /* [4861] OBJ_sect131r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x17)                /* [4866] OBJ_sect131r2 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x01)                /* [4871] OBJ_sect163k1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x02)                /* [4876] OBJ_sect163r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x0F)                /* [4881] OBJ_sect163r2 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x18)                /* [4886] OBJ_sect193r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x19)                /* [4891] OBJ_sect193r2 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x1A)                /* [4896] OBJ_sect233k1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x1B)                /* [4901] OBJ_sect233r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x03)                /* [4906] OBJ_sect239k1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x10)                /* [4911] OBJ_sect283k1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x11)                /* [4916] OBJ_sect283r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x24)                /* [4921] OBJ_sect409k1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x25)                /* [4926] OBJ_sect409r1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x26)                /* [4931] OBJ_sect571k1 */
I(0x2B) I(0x81) I(0x04) I(0x00) I(0x27)                /* [4936] OBJ_sect571r1 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x01)                /* [4941] OBJ_wap_wsg_idm_ecid_wtls1 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x03)                /* [4946] OBJ_wap_wsg_idm_ecid_wtls3 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x04)                /* [4951] OBJ_wap_wsg_idm_ecid_wtls4 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x05)                /* [4956] OBJ_wap_wsg_idm_ecid_wtls5 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x06)                /* [4961] OBJ_wap_wsg_idm_ecid_wtls6 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x07)                /* [4966] OBJ_wap_wsg_idm_ecid_wtls7 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x08)                /* [4971] OBJ_wap_wsg_idm_ecid_wtls8 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x09)                /* [4976] OBJ_wap_wsg_idm_ecid_wtls9 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x0A)                /* [4981] OBJ_wap_wsg_idm_ecid_wtls10 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x0B)                /* [4986] OBJ_wap_wsg_idm_ecid_wtls11 */
I(0x67) I(0x2B) I(0x01) I(0x04) I(0x0C)                /* [4991] OBJ_wap_wsg_idm_ecid_wtls12 */
I(0x55) I(0x1D) I(0x20) I(0x00)                        /* [4996] OBJ_any_policy */
I(0x55) I(0x1D) I(0x21)                                /* [5000] OBJ_policy_mappings */
I(0x55) I(0x1D) I(0x36)                                /* [5003] OBJ_inhibit_any_policy */
I(0x2A) I(0x83) I(0x08) I(0x8C) I(0x9A) I(0x4B) I(0x3D) I(0x01) I(0x01) I(0x01) I(0x02) /* [5006] OBJ_camellia_128_cbc */
I(0x2A) I(0x83) I(0x08) I(0x8C) I(0x9A) I(0x4B) I(0x3D) I(0x01) I(0x01) I(0x01) I(0x03) /* [5017] OBJ_camellia_192_cbc */
I(0x2A) I(0x83) I(0x08) I(0x8C) I(0x9A) I(0x4B) I(0x3D) I(0x01) I(0x01) I(0x01) I(0x04) /* [5028] OBJ_camellia_256_cbc */
I(0x03) I(0xA2) I(0x31) I(0x05) I(0x03) I(0x01) I(0x09) I(0x01) /* [5039] OBJ_camellia_128_ecb */
I(0x03) I(0xA2) I(0x31) I(0x05) I(0x03) I(0x01) I(0x09) I(0x15) /* [5047] OBJ_camellia_192_ecb */
I(0x03) I(0xA2) I(0x31) I(0x05) I(0x03) I(0x01) I(0x09) I(0x29) /* [5055] OBJ_camellia_256_ecb */
I(0x03) I(0xA2) I(0x31) I(0x05) I(0x03) I(0x01) I(0x09) I(0x04) /* [5063] OBJ_camellia_128_cfb128 */
I(0x03) I(0xA2) I(0x31) I(0x05) I(0x03) I(0x01) I(0x09) I(0x18) /* [5071] OBJ_camellia_192_cfb128 */
I(0x03) I(0xA2) I(0x31) I(0x05) I(0x03) I(0x01) I(0x09) I(0x2C) /* [5079] OBJ_camellia_256_cfb128 */
I(0x03) I(0xA2) I(0x31) I(0x05) I(0x03) I(0x01) I(0x09) I(0x03) /* [5087] OBJ_camellia_128_ofb128 */
I(0x03) I(0xA2) I(0x31) I(0x05) I(0x03) I(0x01) I(0x09) I(0x17) /* [5095] OBJ_camellia_192_ofb128 */
I(0x03) I(0xA2) I(0x31) I(0x05) I(0x03) I(0x01) I(0x09) I(0x2B) /* [5103] OBJ_camellia_256_ofb128 */
I(0x55) I(0x1D) I(0x09)                                /* [5111] OBJ_subject_directory_attributes */
I(0x55) I(0x1D) I(0x1C)                                /* [5114] OBJ_issuing_distribution_point */
I(0x55) I(0x1D) I(0x1D)                                /* [5117] OBJ_certificate_issuer */
I(0x2A) I(0x83) I(0x1A) I(0x8C) I(0x9A) I(0x44)        /* [5120] OBJ_kisa */
I(0x2A) I(0x83) I(0x1A) I(0x8C) I(0x9A) I(0x44) I(0x01) I(0x03) /* [5126] OBJ_seed_ecb */
I(0x2A) I(0x83) I(0x1A) I(0x8C) I(0x9A) I(0x44) I(0x01) I(0x04) /* [5134] OBJ_seed_cbc */
I(0x2A) I(0x83) I(0x1A) I(0x8C) I(0x9A) I(0x44) I(0x01) I(0x06) /* [5142] OBJ_seed_ofb128 */
I(0x2A) I(0x83) I(0x1A) I(0x8C) I(0x9A) I(0x44) I(0x01) I(0x05) /* [5150] OBJ_seed_cfb128 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x08) I(0x01) I(0x01) /* [5158] OBJ_hmac_md5 */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x08) I(0x01) I(0x02) /* [5166] OBJ_hmac_sha1 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF6) I(0x7D) I(0x07) I(0x42) I(0x0D) /* [5174] OBJ_id_PasswordBasedMAC */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF6) I(0x7D) I(0x07) I(0x42) I(0x1E) /* [5183] OBJ_id_DHBasedMac */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x04) I(0x10) /* [5192] OBJ_id_it_suppLangTags */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x30) I(0x05) /* [5200] OBJ_caRepository */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) I(0x09) /* [5208] OBJ_id_smime_ct_compressedData */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x01) I(0x09) I(0x10) I(0x01) I(0x1B) /* [5219] OBJ_id_ct_asciiTextWithCRLF */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x05) /* [5230] OBJ_id_aes128_wrap */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x19) /* [5239] OBJ_id_aes192_wrap */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x01) I(0x2D) /* [5248] OBJ_id_aes256_wrap */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x04) I(0x02) /* [5257] OBJ_ecdsa_with_Recommended */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x04) I(0x03) /* [5264] OBJ_ecdsa_with_Specified */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x04) I(0x03) I(0x01) /* [5271] OBJ_ecdsa_with_SHA224 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x04) I(0x03) I(0x02) /* [5279] OBJ_ecdsa_with_SHA256 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x04) I(0x03) I(0x03) /* [5287] OBJ_ecdsa_with_SHA384 */
I(0x2A) I(0x86) I(0x48) I(0xCE) I(0x3D) I(0x04) I(0x03) I(0x04) /* [5295] OBJ_ecdsa_with_SHA512 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x02) I(0x06) /* [5303] OBJ_hmacWithMD5 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x02) I(0x08) /* [5311] OBJ_hmacWithSHA224 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x02) I(0x09) /* [5319] OBJ_hmacWithSHA256 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x02) I(0x0A) /* [5327] OBJ_hmacWithSHA384 */
I(0x2A) I(0x86) I(0x48) I(0x86) I(0xF7) I(0x0D) I(0x02) I(0x0B) /* [5335] OBJ_hmacWithSHA512 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x03) I(0x01) /* [5343] OBJ_dsa_with_SHA224 */
I(0x60) I(0x86) I(0x48) I(0x01) I(0x65) I(0x03) I(0x04) I(0x03) I(0x02) /* [5352] OBJ_dsa_with_SHA256 */
I(0x28) I(0xCF) I(0x06) I(0x03) I(0x00) I(0x37)        /* [5361] OBJ_whirlpool */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02)                /* [5367] OBJ_cryptopro */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x09)                /* [5372] OBJ_cryptocom */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x03)        /* [5377] OBJ_id_GostR3411_94_with_GostR3410_2001 */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x04)        /* [5383] OBJ_id_GostR3411_94_with_GostR3410_94 */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x09)        /* [5389] OBJ_id_GostR3411_94 */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x0A)        /* [5395] OBJ_id_HMACGostR3411_94 */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x13)        /* [5401] OBJ_id_GostR3410_2001 */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x14)        /* [5407] OBJ_id_GostR3410_94 */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x15)        /* [5413] OBJ_id_Gost28147_89 */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x16)        /* [5419] OBJ_id_Gost28147_89_MAC */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x17)        /* [5425] OBJ_id_GostR3411_94_prf */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x62)        /* [5431] OBJ_id_GostR3410_2001DH */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x63)        /* [5437] OBJ_id_GostR3410_94DH */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x0E) I(0x01) /* [5443] OBJ_id_Gost28147_89_CryptoPro_KeyMeshing */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x0E) I(0x00) /* [5450] OBJ_id_Gost28147_89_None_KeyMeshing */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x1E) I(0x00) /* [5457] OBJ_id_GostR3411_94_TestParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x1E) I(0x01) /* [5464] OBJ_id_GostR3411_94_CryptoProParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x1F) I(0x00) /* [5471] OBJ_id_Gost28147_89_TestParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x1F) I(0x01) /* [5478] OBJ_id_Gost28147_89_CryptoPro_A_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x1F) I(0x02) /* [5485] OBJ_id_Gost28147_89_CryptoPro_B_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x1F) I(0x03) /* [5492] OBJ_id_Gost28147_89_CryptoPro_C_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x1F) I(0x04) /* [5499] OBJ_id_Gost28147_89_CryptoPro_D_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x1F) I(0x05) /* [5506] OBJ_id_Gost28147_89_CryptoPro_Oscar_1_1_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x1F) I(0x06) /* [5513] OBJ_id_Gost28147_89_CryptoPro_Oscar_1_0_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x1F) I(0x07) /* [5520] OBJ_id_Gost28147_89_CryptoPro_RIC_1_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x20) I(0x00) /* [5527] OBJ_id_GostR3410_94_TestParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x20) I(0x02) /* [5534] OBJ_id_GostR3410_94_CryptoPro_A_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x20) I(0x03) /* [5541] OBJ_id_GostR3410_94_CryptoPro_B_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x20) I(0x04) /* [5548] OBJ_id_GostR3410_94_CryptoPro_C_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x20) I(0x05) /* [5555] OBJ_id_GostR3410_94_CryptoPro_D_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x21) I(0x01) /* [5562] OBJ_id_GostR3410_94_CryptoPro_XchA_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x21) I(0x02) /* [5569] OBJ_id_GostR3410_94_CryptoPro_XchB_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x21) I(0x03) /* [5576] OBJ_id_GostR3410_94_CryptoPro_XchC_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x23) I(0x00) /* [5583] OBJ_id_GostR3410_2001_TestParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x23) I(0x01) /* [5590] OBJ_id_GostR3410_2001_CryptoPro_A_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x23) I(0x02) /* [5597] OBJ_id_GostR3410_2001_CryptoPro_B_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x23) I(0x03) /* [5604] OBJ_id_GostR3410_2001_CryptoPro_C_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x24) I(0x00) /* [5611] OBJ_id_GostR3410_2001_CryptoPro_XchA_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x24) I(0x01) /* [5618] OBJ_id_GostR3410_2001_CryptoPro_XchB_ParamSet */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x14) I(0x01) /* [5625] OBJ_id_GostR3410_94_a */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x14) I(0x02) /* [5632] OBJ_id_GostR3410_94_aBis */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x14) I(0x03) /* [5639] OBJ_id_GostR3410_94_b */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x02) I(0x14) I(0x04) /* [5646] OBJ_id_GostR3410_94_bBis */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x09) I(0x01) I(0x06) I(0x01) /* [5653] OBJ_id_Gost28147_89_cc */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x09) I(0x01) I(0x05) I(0x03) /* [5661] OBJ_id_GostR3410_94_cc */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x09) I(0x01) I(0x05) I(0x04) /* [5669] OBJ_id_GostR3410_2001_cc */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x09) I(0x01) I(0x03) I(0x03) /* [5677] OBJ_id_GostR3411_94_with_GostR3410_94_cc */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x09) I(0x01) I(0x03) I(0x04) /* [5685] OBJ_id_GostR3411_94_with_GostR3410_2001_cc */
I(0x2A) I(0x85) I(0x03) I(0x02) I(0x09) I(0x01) I(0x08) I(0x01) /* [5693] OBJ_id_GostR3410_2001_ParamSet_cc */
I(0x2B) I(0x06) I(0x01) I(0x04) I(0x01) I(0x82) I(0x37) I(0x11) I(0x02) /* [5701] OBJ_LocalKeySet */
I(0x55) I(0x1D) I(0x2E)                                /* [5710] OBJ_freshest_crl */
I(0x2B) I(0x06) I(0x01) I(0x05) I(0x05) I(0x07) I(0x08) I(0x03) /* [5713] OBJ_id_on_permanentIdentifier */
I(0x55) I(0x04) I(0x0E)                                /* [5721] OBJ_searchGuide */
I(0x55) I(0x04) I(0x0F)                                /* [5724] OBJ_businessCategory */
I(0x55) I(0x04) I(0x10)                                /* [5727] OBJ_postalAddress */
I(0x55) I(0x04) I(0x12)                                /* [5730] OBJ_postOfficeBox */
I(0x55) I(0x04) I(0x13)                                /* [5733] OBJ_physicalDeliveryOfficeName */
I(0x55) I(0x04) I(0x14)                                /* [5736] OBJ_telephoneNumber */
I(0x55) I(0x04) I(0x15)                                /* [5739] OBJ_telexNumber */
I(0x55) I(0x04) I(0x16)                                /* [5742] OBJ_teletexTerminalIdentifier */
I(0x55) I(0x04) I(0x17)                                /* [5745] OBJ_facsimileTelephoneNumber */
I(0x55) I(0x04) I(0x18)                                /* [5748] OBJ_x121Address */
I(0x55) I(0x04) I(0x19)                                /* [5751] OBJ_internationaliSDNNumber */
I(0x55) I(0x04) I(0x1A)                                /* [5754] OBJ_registeredAddress */
I(0x55) I(0x04) I(0x1B)                                /* [5757] OBJ_destinationIndicator */
I(0x55) I(0x04) I(0x1C)                                /* [5760] OBJ_preferredDeliveryMethod */
I(0x55) I(0x04) I(0x1D)                                /* [5763] OBJ_presentationAddress */
I(0x55) I(0x04) I(0x1E)                                /* [5766] OBJ_supportedApplicationContext */
I(0x55) I(0x04) I(0x1F)                                /* [5769] OBJ_member */
I(0x55) I(0x04) I(0x20)                                /* [5772] OBJ_owner */
I(0x55) I(0x04) I(0x21)                                /* [5775] OBJ_roleOccupant */
I(0x55) I(0x04) I(0x22)                                /* [5778] OBJ_seeAlso */
I(0x55) I(0x04) I(0x23)                                /* [5781] OBJ_userPassword */
I(0x55) I(0x04) I(0x24)                                /* [5784] OBJ_userCertificate */
I(0x55) I(0x04) I(0x25)                                /* [5787] OBJ_cACertificate */
I(0x55) I(0x04) I(0x26)                                /* [5790] OBJ_authorityRevocationList */
I(0x55) I(0x04) I(0x27)                                /* [5793] OBJ_certificateRevocationList */
I(0x55) I(0x04) I(0x28)                                /* [5796] OBJ_crossCertificatePair */
I(0x55) I(0x04) I(0x2F)                                /* [5799] OBJ_enhancedSearchGuide */
I(0x55) I(0x04) I(0x30)                                /* [5802] OBJ_protocolInformation */
I(0x55) I(0x04) I(0x31)                                /* [5805] OBJ_distinguishedName */
I(0x55) I(0x04) I(0x32)                                /* [5808] OBJ_uniqueMember */
I(0x55) I(0x04) I(0x33)                                /* [5811] OBJ_houseIdentifier */
I(0x55) I(0x04) I(0x34)                                /* [5814] OBJ_supportedAlgorithms */
I(0x55) I(0x04) I(0x35)                                /* [5817] OBJ_deltaRevocationList */
I(0x55) I(0x04) I(0x36)                                /* [5820] OBJ_dmdName */
LVALUES_END

#ifdef NEED_LVALUES_OVERRIDE
// Some compilers (e.g. ADS) expands macros differently from most other compilers, so this have to be declared down here
#define lvalues OPENSSL_GLOBAL_ARRAY_NAME(lvalues)
#endif

OPENSSL_PREFIX_CONST_ARRAY(static, ASN1_OBJECT, nid_objs, libopeay)
CONST_ENTRY_NID("UNDEF","undefined",NID_undef,1,&(lvalues[0]),0)
CONST_ENTRY_NID("rsadsi","RSA Data Security, Inc.",NID_rsadsi,6,&(lvalues[1]),0)
CONST_ENTRY_NID("pkcs","RSA Data Security, Inc. PKCS",NID_pkcs,7,&(lvalues[7]),0)
CONST_ENTRY_NID("MD2","md2",NID_md2,8,&(lvalues[14]),0)
CONST_ENTRY_NID("MD5","md5",NID_md5,8,&(lvalues[22]),0)
CONST_ENTRY_NID("RC4","rc4",NID_rc4,8,&(lvalues[30]),0)
CONST_ENTRY_NID("rsaEncryption","rsaEncryption",NID_rsaEncryption,9,&(lvalues[38]),0)
CONST_ENTRY_NID("RSA-MD2","md2WithRSAEncryption",NID_md2WithRSAEncryption,9,
	&(lvalues[47]),0)
CONST_ENTRY_NID("RSA-MD5","md5WithRSAEncryption",NID_md5WithRSAEncryption,9,
	&(lvalues[56]),0)
CONST_ENTRY_NID("PBE-MD2-DES","pbeWithMD2AndDES-CBC",NID_pbeWithMD2AndDES_CBC,9,
	&(lvalues[65]),0)
CONST_ENTRY_NID("PBE-MD5-DES","pbeWithMD5AndDES-CBC",NID_pbeWithMD5AndDES_CBC,9,
	&(lvalues[74]),0)
CONST_ENTRY_NID("X500","directory services (X.500)",NID_X500,1,&(lvalues[83]),0)
CONST_ENTRY_NID("X509","X509",NID_X509,2,&(lvalues[84]),0)
CONST_ENTRY_NID("CN","commonName",NID_commonName,3,&(lvalues[86]),0)
CONST_ENTRY_NID("C","countryName",NID_countryName,3,&(lvalues[89]),0)
CONST_ENTRY_NID("L","localityName",NID_localityName,3,&(lvalues[92]),0)
CONST_ENTRY_NID("ST","stateOrProvinceName",NID_stateOrProvinceName,3,&(lvalues[95]),0)
CONST_ENTRY_NID("O","organizationName",NID_organizationName,3,&(lvalues[98]),0)
CONST_ENTRY_NID("OU","organizationalUnitName",NID_organizationalUnitName,3,
	&(lvalues[101]),0)
CONST_ENTRY_NID("RSA","rsa",NID_rsa,4,&(lvalues[104]),0)
CONST_ENTRY_NID("pkcs7","pkcs7",NID_pkcs7,8,&(lvalues[108]),0)
CONST_ENTRY_NID("pkcs7-data","pkcs7-data",NID_pkcs7_data,9,&(lvalues[116]),0)
CONST_ENTRY_NID("pkcs7-signedData","pkcs7-signedData",NID_pkcs7_signed,9,
	&(lvalues[125]),0)
CONST_ENTRY_NID("pkcs7-envelopedData","pkcs7-envelopedData",NID_pkcs7_enveloped,9,
	&(lvalues[134]),0)
CONST_ENTRY_NID("pkcs7-signedAndEnvelopedData","pkcs7-signedAndEnvelopedData",
	NID_pkcs7_signedAndEnveloped,9,&(lvalues[143]),0)
CONST_ENTRY_NID("pkcs7-digestData","pkcs7-digestData",NID_pkcs7_digest,9,
	&(lvalues[152]),0)
CONST_ENTRY_NID("pkcs7-encryptedData","pkcs7-encryptedData",NID_pkcs7_encrypted,9,
	&(lvalues[161]),0)
CONST_ENTRY_NID("pkcs3","pkcs3",NID_pkcs3,8,&(lvalues[170]),0)
CONST_ENTRY_NID("dhKeyAgreement","dhKeyAgreement",NID_dhKeyAgreement,9,
	&(lvalues[178]),0)
CONST_ENTRY_NID("DES-ECB","des-ecb",NID_des_ecb,5,&(lvalues[187]),0)
CONST_ENTRY_NID("DES-CFB","des-cfb",NID_des_cfb64,5,&(lvalues[192]),0)
CONST_ENTRY_NID("DES-CBC","des-cbc",NID_des_cbc,5,&(lvalues[197]),0)
CONST_ENTRY_NID("DES-EDE","des-ede",NID_des_ede_ecb,5,&(lvalues[202]),0)
CONST_ENTRY_NID("DES-EDE3","des-ede3",NID_des_ede3_ecb,0,NULL,0)
CONST_ENTRY_NID("IDEA-CBC","idea-cbc",NID_idea_cbc,11,&(lvalues[207]),0)
CONST_ENTRY_NID("IDEA-CFB","idea-cfb",NID_idea_cfb64,0,NULL,0)
CONST_ENTRY_NID("IDEA-ECB","idea-ecb",NID_idea_ecb,0,NULL,0)
CONST_ENTRY_NID("RC2-CBC","rc2-cbc",NID_rc2_cbc,8,&(lvalues[218]),0)
CONST_ENTRY_NID("RC2-ECB","rc2-ecb",NID_rc2_ecb,0,NULL,0)
CONST_ENTRY_NID("RC2-CFB","rc2-cfb",NID_rc2_cfb64,0,NULL,0)
CONST_ENTRY_NID("RC2-OFB","rc2-ofb",NID_rc2_ofb64,0,NULL,0)
CONST_ENTRY_NID("SHA","sha",NID_sha,5,&(lvalues[226]),0)
CONST_ENTRY_NID("RSA-SHA","shaWithRSAEncryption",NID_shaWithRSAEncryption,5,
	&(lvalues[231]),0)
CONST_ENTRY_NID("DES-EDE-CBC","des-ede-cbc",NID_des_ede_cbc,0,NULL,0)
CONST_ENTRY_NID("DES-EDE3-CBC","des-ede3-cbc",NID_des_ede3_cbc,8,&(lvalues[236]),0)
CONST_ENTRY_NID("DES-OFB","des-ofb",NID_des_ofb64,5,&(lvalues[244]),0)
CONST_ENTRY_NID("IDEA-OFB","idea-ofb",NID_idea_ofb64,0,NULL,0)
CONST_ENTRY_NID("pkcs9","pkcs9",NID_pkcs9,8,&(lvalues[249]),0)
CONST_ENTRY_NID("emailAddress","emailAddress",NID_pkcs9_emailAddress,9,
	&(lvalues[257]),0)
CONST_ENTRY_NID("unstructuredName","unstructuredName",NID_pkcs9_unstructuredName,9,
	&(lvalues[266]),0)
CONST_ENTRY_NID("contentType","contentType",NID_pkcs9_contentType,9,&(lvalues[275]),0)
CONST_ENTRY_NID("messageDigest","messageDigest",NID_pkcs9_messageDigest,9,
	&(lvalues[284]),0)
CONST_ENTRY_NID("signingTime","signingTime",NID_pkcs9_signingTime,9,&(lvalues[293]),0)
CONST_ENTRY_NID("countersignature","countersignature",NID_pkcs9_countersignature,9,
	&(lvalues[302]),0)
CONST_ENTRY_NID("challengePassword","challengePassword",NID_pkcs9_challengePassword,
	9,&(lvalues[311]),0)
CONST_ENTRY_NID("unstructuredAddress","unstructuredAddress",
	NID_pkcs9_unstructuredAddress,9,&(lvalues[320]),0)
CONST_ENTRY_NID("extendedCertificateAttributes","extendedCertificateAttributes",
	NID_pkcs9_extCertAttributes,9,&(lvalues[329]),0)
CONST_ENTRY_NID("Netscape","Netscape Communications Corp.",NID_netscape,7,
	&(lvalues[338]),0)
CONST_ENTRY_NID("nsCertExt","Netscape Certificate Extension",
	NID_netscape_cert_extension,8,&(lvalues[345]),0)
CONST_ENTRY_NID("nsDataType","Netscape Data Type",NID_netscape_data_type,8,
	&(lvalues[353]),0)
CONST_ENTRY_NID("DES-EDE-CFB","des-ede-cfb",NID_des_ede_cfb64,0,NULL,0)
CONST_ENTRY_NID("DES-EDE3-CFB","des-ede3-cfb",NID_des_ede3_cfb64,0,NULL,0)
CONST_ENTRY_NID("DES-EDE-OFB","des-ede-ofb",NID_des_ede_ofb64,0,NULL,0)
CONST_ENTRY_NID("DES-EDE3-OFB","des-ede3-ofb",NID_des_ede3_ofb64,0,NULL,0)
CONST_ENTRY_NID("SHA1","sha1",NID_sha1,5,&(lvalues[361]),0)
CONST_ENTRY_NID("RSA-SHA1","sha1WithRSAEncryption",NID_sha1WithRSAEncryption,9,
	&(lvalues[366]),0)
CONST_ENTRY_NID("DSA-SHA","dsaWithSHA",NID_dsaWithSHA,5,&(lvalues[375]),0)
CONST_ENTRY_NID("DSA-old","dsaEncryption-old",NID_dsa_2,5,&(lvalues[380]),0)
CONST_ENTRY_NID("PBE-SHA1-RC2-64","pbeWithSHA1AndRC2-CBC",NID_pbeWithSHA1AndRC2_CBC,
	9,&(lvalues[385]),0)
CONST_ENTRY_NID("PBKDF2","PBKDF2",NID_id_pbkdf2,9,&(lvalues[394]),0)
CONST_ENTRY_NID("DSA-SHA1-old","dsaWithSHA1-old",NID_dsaWithSHA1_2,5,&(lvalues[403]),0)
CONST_ENTRY_NID("nsCertType","Netscape Cert Type",NID_netscape_cert_type,9,
	&(lvalues[408]),0)
CONST_ENTRY_NID("nsBaseUrl","Netscape Base Url",NID_netscape_base_url,9,
	&(lvalues[417]),0)
CONST_ENTRY_NID("nsRevocationUrl","Netscape Revocation Url",
	NID_netscape_revocation_url,9,&(lvalues[426]),0)
CONST_ENTRY_NID("nsCaRevocationUrl","Netscape CA Revocation Url",
	NID_netscape_ca_revocation_url,9,&(lvalues[435]),0)
CONST_ENTRY_NID("nsRenewalUrl","Netscape Renewal Url",NID_netscape_renewal_url,9,
	&(lvalues[444]),0)
CONST_ENTRY_NID("nsCaPolicyUrl","Netscape CA Policy Url",NID_netscape_ca_policy_url,
	9,&(lvalues[453]),0)
CONST_ENTRY_NID("nsSslServerName","Netscape SSL Server Name",
	NID_netscape_ssl_server_name,9,&(lvalues[462]),0)
CONST_ENTRY_NID("nsComment","Netscape Comment",NID_netscape_comment,9,&(lvalues[471]),0)
CONST_ENTRY_NID("nsCertSequence","Netscape Certificate Sequence",
	NID_netscape_cert_sequence,9,&(lvalues[480]),0)
CONST_ENTRY_NID("DESX-CBC","desx-cbc",NID_desx_cbc,0,NULL,0)
CONST_ENTRY_NID("id-ce","id-ce",NID_id_ce,2,&(lvalues[489]),0)
CONST_ENTRY_NID("subjectKeyIdentifier","X509v3 Subject Key Identifier",
	NID_subject_key_identifier,3,&(lvalues[491]),0)
CONST_ENTRY_NID("keyUsage","X509v3 Key Usage",NID_key_usage,3,&(lvalues[494]),0)
CONST_ENTRY_NID("privateKeyUsagePeriod","X509v3 Private Key Usage Period",
	NID_private_key_usage_period,3,&(lvalues[497]),0)
CONST_ENTRY_NID("subjectAltName","X509v3 Subject Alternative Name",
	NID_subject_alt_name,3,&(lvalues[500]),0)
CONST_ENTRY_NID("issuerAltName","X509v3 Issuer Alternative Name",NID_issuer_alt_name,
	3,&(lvalues[503]),0)
CONST_ENTRY_NID("basicConstraints","X509v3 Basic Constraints",NID_basic_constraints,
	3,&(lvalues[506]),0)
CONST_ENTRY_NID("crlNumber","X509v3 CRL Number",NID_crl_number,3,&(lvalues[509]),0)
CONST_ENTRY_NID("certificatePolicies","X509v3 Certificate Policies",
	NID_certificate_policies,3,&(lvalues[512]),0)
CONST_ENTRY_NID("authorityKeyIdentifier","X509v3 Authority Key Identifier",
	NID_authority_key_identifier,3,&(lvalues[515]),0)
CONST_ENTRY_NID("BF-CBC","bf-cbc",NID_bf_cbc,9,&(lvalues[518]),0)
CONST_ENTRY_NID("BF-ECB","bf-ecb",NID_bf_ecb,0,NULL,0)
CONST_ENTRY_NID("BF-CFB","bf-cfb",NID_bf_cfb64,0,NULL,0)
CONST_ENTRY_NID("BF-OFB","bf-ofb",NID_bf_ofb64,0,NULL,0)
CONST_ENTRY_NID("MDC2","mdc2",NID_mdc2,4,&(lvalues[527]),0)
CONST_ENTRY_NID("RSA-MDC2","mdc2WithRSA",NID_mdc2WithRSA,4,&(lvalues[531]),0)
CONST_ENTRY_NID("RC4-40","rc4-40",NID_rc4_40,0,NULL,0)
CONST_ENTRY_NID("RC2-40-CBC","rc2-40-cbc",NID_rc2_40_cbc,0,NULL,0)
CONST_ENTRY_NID("GN","givenName",NID_givenName,3,&(lvalues[535]),0)
CONST_ENTRY_NID("SN","surname",NID_surname,3,&(lvalues[538]),0)
CONST_ENTRY_NID("initials","initials",NID_initials,3,&(lvalues[541]),0)
CONST_ENTRY_NID(NULL,NULL,NID_undef,0,NULL,0)
CONST_ENTRY_NID("crlDistributionPoints","X509v3 CRL Distribution Points",
	NID_crl_distribution_points,3,&(lvalues[544]),0)
CONST_ENTRY_NID("RSA-NP-MD5","md5WithRSA",NID_md5WithRSA,5,&(lvalues[547]),0)
CONST_ENTRY_NID("serialNumber","serialNumber",NID_serialNumber,3,&(lvalues[552]),0)
CONST_ENTRY_NID("title","title",NID_title,3,&(lvalues[555]),0)
CONST_ENTRY_NID("description","description",NID_description,3,&(lvalues[558]),0)
CONST_ENTRY_NID("CAST5-CBC","cast5-cbc",NID_cast5_cbc,9,&(lvalues[561]),0)
CONST_ENTRY_NID("CAST5-ECB","cast5-ecb",NID_cast5_ecb,0,NULL,0)
CONST_ENTRY_NID("CAST5-CFB","cast5-cfb",NID_cast5_cfb64,0,NULL,0)
CONST_ENTRY_NID("CAST5-OFB","cast5-ofb",NID_cast5_ofb64,0,NULL,0)
CONST_ENTRY_NID("pbeWithMD5AndCast5CBC","pbeWithMD5AndCast5CBC",
	NID_pbeWithMD5AndCast5_CBC,9,&(lvalues[570]),0)
CONST_ENTRY_NID("DSA-SHA1","dsaWithSHA1",NID_dsaWithSHA1,7,&(lvalues[579]),0)
CONST_ENTRY_NID("MD5-SHA1","md5-sha1",NID_md5_sha1,0,NULL,0)
CONST_ENTRY_NID("RSA-SHA1-2","sha1WithRSA",NID_sha1WithRSA,5,&(lvalues[586]),0)
CONST_ENTRY_NID("DSA","dsaEncryption",NID_dsa,7,&(lvalues[591]),0)
CONST_ENTRY_NID("RIPEMD160","ripemd160",NID_ripemd160,5,&(lvalues[598]),0)
CONST_ENTRY_NID(NULL,NULL,NID_undef,0,NULL,0)
CONST_ENTRY_NID("RSA-RIPEMD160","ripemd160WithRSA",NID_ripemd160WithRSA,6,
	&(lvalues[603]),0)
CONST_ENTRY_NID("RC5-CBC","rc5-cbc",NID_rc5_cbc,8,&(lvalues[609]),0)
CONST_ENTRY_NID("RC5-ECB","rc5-ecb",NID_rc5_ecb,0,NULL,0)
CONST_ENTRY_NID("RC5-CFB","rc5-cfb",NID_rc5_cfb64,0,NULL,0)
CONST_ENTRY_NID("RC5-OFB","rc5-ofb",NID_rc5_ofb64,0,NULL,0)
CONST_ENTRY_NID("RLE","run length compression",NID_rle_compression,6,&(lvalues[617]),0)
CONST_ENTRY_NID("ZLIB","zlib compression",NID_zlib_compression,11,&(lvalues[623]),0)
CONST_ENTRY_NID("extendedKeyUsage","X509v3 Extended Key Usage",NID_ext_key_usage,3,
	&(lvalues[634]),0)
CONST_ENTRY_NID("PKIX","PKIX",NID_id_pkix,6,&(lvalues[637]),0)
CONST_ENTRY_NID("id-kp","id-kp",NID_id_kp,7,&(lvalues[643]),0)
CONST_ENTRY_NID("serverAuth","TLS Web Server Authentication",NID_server_auth,8,
	&(lvalues[650]),0)
CONST_ENTRY_NID("clientAuth","TLS Web Client Authentication",NID_client_auth,8,
	&(lvalues[658]),0)
CONST_ENTRY_NID("codeSigning","Code Signing",NID_code_sign,8,&(lvalues[666]),0)
CONST_ENTRY_NID("emailProtection","E-mail Protection",NID_email_protect,8,
	&(lvalues[674]),0)
CONST_ENTRY_NID("timeStamping","Time Stamping",NID_time_stamp,8,&(lvalues[682]),0)
CONST_ENTRY_NID("msCodeInd","Microsoft Individual Code Signing",NID_ms_code_ind,10,
	&(lvalues[690]),0)
CONST_ENTRY_NID("msCodeCom","Microsoft Commercial Code Signing",NID_ms_code_com,10,
	&(lvalues[700]),0)
CONST_ENTRY_NID("msCTLSign","Microsoft Trust List Signing",NID_ms_ctl_sign,10,
	&(lvalues[710]),0)
CONST_ENTRY_NID("msSGC","Microsoft Server Gated Crypto",NID_ms_sgc,10,&(lvalues[720]),0)
CONST_ENTRY_NID("msEFS","Microsoft Encrypted File System",NID_ms_efs,10,
	&(lvalues[730]),0)
CONST_ENTRY_NID("nsSGC","Netscape Server Gated Crypto",NID_ns_sgc,9,&(lvalues[740]),0)
CONST_ENTRY_NID("deltaCRL","X509v3 Delta CRL Indicator",NID_delta_crl,3,
	&(lvalues[749]),0)
CONST_ENTRY_NID("CRLReason","X509v3 CRL Reason Code",NID_crl_reason,3,&(lvalues[752]),0)
CONST_ENTRY_NID("invalidityDate","Invalidity Date",NID_invalidity_date,3,
	&(lvalues[755]),0)
CONST_ENTRY_NID("SXNetID","Strong Extranet ID",NID_sxnet,5,&(lvalues[758]),0)
CONST_ENTRY_NID("PBE-SHA1-RC4-128","pbeWithSHA1And128BitRC4",
	NID_pbe_WithSHA1And128BitRC4,10,&(lvalues[763]),0)
CONST_ENTRY_NID("PBE-SHA1-RC4-40","pbeWithSHA1And40BitRC4",
	NID_pbe_WithSHA1And40BitRC4,10,&(lvalues[773]),0)
CONST_ENTRY_NID("PBE-SHA1-3DES","pbeWithSHA1And3-KeyTripleDES-CBC",
	NID_pbe_WithSHA1And3_Key_TripleDES_CBC,10,&(lvalues[783]),0)
CONST_ENTRY_NID("PBE-SHA1-2DES","pbeWithSHA1And2-KeyTripleDES-CBC",
	NID_pbe_WithSHA1And2_Key_TripleDES_CBC,10,&(lvalues[793]),0)
CONST_ENTRY_NID("PBE-SHA1-RC2-128","pbeWithSHA1And128BitRC2-CBC",
	NID_pbe_WithSHA1And128BitRC2_CBC,10,&(lvalues[803]),0)
CONST_ENTRY_NID("PBE-SHA1-RC2-40","pbeWithSHA1And40BitRC2-CBC",
	NID_pbe_WithSHA1And40BitRC2_CBC,10,&(lvalues[813]),0)
CONST_ENTRY_NID("keyBag","keyBag",NID_keyBag,11,&(lvalues[823]),0)
CONST_ENTRY_NID("pkcs8ShroudedKeyBag","pkcs8ShroudedKeyBag",NID_pkcs8ShroudedKeyBag,
	11,&(lvalues[834]),0)
CONST_ENTRY_NID("certBag","certBag",NID_certBag,11,&(lvalues[845]),0)
CONST_ENTRY_NID("crlBag","crlBag",NID_crlBag,11,&(lvalues[856]),0)
CONST_ENTRY_NID("secretBag","secretBag",NID_secretBag,11,&(lvalues[867]),0)
CONST_ENTRY_NID("safeContentsBag","safeContentsBag",NID_safeContentsBag,11,
	&(lvalues[878]),0)
CONST_ENTRY_NID("friendlyName","friendlyName",NID_friendlyName,9,&(lvalues[889]),0)
CONST_ENTRY_NID("localKeyID","localKeyID",NID_localKeyID,9,&(lvalues[898]),0)
CONST_ENTRY_NID("x509Certificate","x509Certificate",NID_x509Certificate,10,
	&(lvalues[907]),0)
CONST_ENTRY_NID("sdsiCertificate","sdsiCertificate",NID_sdsiCertificate,10,
	&(lvalues[917]),0)
CONST_ENTRY_NID("x509Crl","x509Crl",NID_x509Crl,10,&(lvalues[927]),0)
CONST_ENTRY_NID("PBES2","PBES2",NID_pbes2,9,&(lvalues[937]),0)
CONST_ENTRY_NID("PBMAC1","PBMAC1",NID_pbmac1,9,&(lvalues[946]),0)
CONST_ENTRY_NID("hmacWithSHA1","hmacWithSHA1",NID_hmacWithSHA1,8,&(lvalues[955]),0)
CONST_ENTRY_NID("id-qt-cps","Policy Qualifier CPS",NID_id_qt_cps,8,&(lvalues[963]),0)
CONST_ENTRY_NID("id-qt-unotice","Policy Qualifier User Notice",NID_id_qt_unotice,8,
	&(lvalues[971]),0)
CONST_ENTRY_NID("RC2-64-CBC","rc2-64-cbc",NID_rc2_64_cbc,0,NULL,0)
CONST_ENTRY_NID("SMIME-CAPS","S/MIME Capabilities",NID_SMIMECapabilities,9,
	&(lvalues[979]),0)
CONST_ENTRY_NID("PBE-MD2-RC2-64","pbeWithMD2AndRC2-CBC",NID_pbeWithMD2AndRC2_CBC,9,
	&(lvalues[988]),0)
CONST_ENTRY_NID("PBE-MD5-RC2-64","pbeWithMD5AndRC2-CBC",NID_pbeWithMD5AndRC2_CBC,9,
	&(lvalues[997]),0)
CONST_ENTRY_NID("PBE-SHA1-DES","pbeWithSHA1AndDES-CBC",NID_pbeWithSHA1AndDES_CBC,9,
	&(lvalues[1006]),0)
CONST_ENTRY_NID("msExtReq","Microsoft Extension Request",NID_ms_ext_req,10,
	&(lvalues[1015]),0)
CONST_ENTRY_NID("extReq","Extension Request",NID_ext_req,9,&(lvalues[1025]),0)
CONST_ENTRY_NID("name","name",NID_name,3,&(lvalues[1034]),0)
CONST_ENTRY_NID("dnQualifier","dnQualifier",NID_dnQualifier,3,&(lvalues[1037]),0)
CONST_ENTRY_NID("id-pe","id-pe",NID_id_pe,7,&(lvalues[1040]),0)
CONST_ENTRY_NID("id-ad","id-ad",NID_id_ad,7,&(lvalues[1047]),0)
CONST_ENTRY_NID("authorityInfoAccess","Authority Information Access",NID_info_access,
	8,&(lvalues[1054]),0)
CONST_ENTRY_NID("OCSP","OCSP",NID_ad_OCSP,8,&(lvalues[1062]),0)
CONST_ENTRY_NID("caIssuers","CA Issuers",NID_ad_ca_issuers,8,&(lvalues[1070]),0)
CONST_ENTRY_NID("OCSPSigning","OCSP Signing",NID_OCSP_sign,8,&(lvalues[1078]),0)
CONST_ENTRY_NID("ISO","iso",NID_iso,1,&(lvalues[1086]),0)
CONST_ENTRY_NID("member-body","ISO Member Body",NID_member_body,1,&(lvalues[1087]),0)
CONST_ENTRY_NID("ISO-US","ISO US Member Body",NID_ISO_US,3,&(lvalues[1088]),0)
CONST_ENTRY_NID("X9-57","X9.57",NID_X9_57,5,&(lvalues[1091]),0)
CONST_ENTRY_NID("X9cm","X9.57 CM ?",NID_X9cm,6,&(lvalues[1096]),0)
CONST_ENTRY_NID("pkcs1","pkcs1",NID_pkcs1,8,&(lvalues[1102]),0)
CONST_ENTRY_NID("pkcs5","pkcs5",NID_pkcs5,8,&(lvalues[1110]),0)
CONST_ENTRY_NID("SMIME","S/MIME",NID_SMIME,9,&(lvalues[1118]),0)
CONST_ENTRY_NID("id-smime-mod","id-smime-mod",NID_id_smime_mod,10,&(lvalues[1127]),0)
CONST_ENTRY_NID("id-smime-ct","id-smime-ct",NID_id_smime_ct,10,&(lvalues[1137]),0)
CONST_ENTRY_NID("id-smime-aa","id-smime-aa",NID_id_smime_aa,10,&(lvalues[1147]),0)
CONST_ENTRY_NID("id-smime-alg","id-smime-alg",NID_id_smime_alg,10,&(lvalues[1157]),0)
CONST_ENTRY_NID("id-smime-cd","id-smime-cd",NID_id_smime_cd,10,&(lvalues[1167]),0)
CONST_ENTRY_NID("id-smime-spq","id-smime-spq",NID_id_smime_spq,10,&(lvalues[1177]),0)
CONST_ENTRY_NID("id-smime-cti","id-smime-cti",NID_id_smime_cti,10,&(lvalues[1187]),0)
CONST_ENTRY_NID("id-smime-mod-cms","id-smime-mod-cms",NID_id_smime_mod_cms,11,
	&(lvalues[1197]),0)
CONST_ENTRY_NID("id-smime-mod-ess","id-smime-mod-ess",NID_id_smime_mod_ess,11,
	&(lvalues[1208]),0)
CONST_ENTRY_NID("id-smime-mod-oid","id-smime-mod-oid",NID_id_smime_mod_oid,11,
	&(lvalues[1219]),0)
CONST_ENTRY_NID("id-smime-mod-msg-v3","id-smime-mod-msg-v3",NID_id_smime_mod_msg_v3,
	11,&(lvalues[1230]),0)
CONST_ENTRY_NID("id-smime-mod-ets-eSignature-88","id-smime-mod-ets-eSignature-88",
	NID_id_smime_mod_ets_eSignature_88,11,&(lvalues[1241]),0)
CONST_ENTRY_NID("id-smime-mod-ets-eSignature-97","id-smime-mod-ets-eSignature-97",
	NID_id_smime_mod_ets_eSignature_97,11,&(lvalues[1252]),0)
CONST_ENTRY_NID("id-smime-mod-ets-eSigPolicy-88","id-smime-mod-ets-eSigPolicy-88",
	NID_id_smime_mod_ets_eSigPolicy_88,11,&(lvalues[1263]),0)
CONST_ENTRY_NID("id-smime-mod-ets-eSigPolicy-97","id-smime-mod-ets-eSigPolicy-97",
	NID_id_smime_mod_ets_eSigPolicy_97,11,&(lvalues[1274]),0)
CONST_ENTRY_NID("id-smime-ct-receipt","id-smime-ct-receipt",NID_id_smime_ct_receipt,
	11,&(lvalues[1285]),0)
CONST_ENTRY_NID("id-smime-ct-authData","id-smime-ct-authData",
	NID_id_smime_ct_authData,11,&(lvalues[1296]),0)
CONST_ENTRY_NID("id-smime-ct-publishCert","id-smime-ct-publishCert",
	NID_id_smime_ct_publishCert,11,&(lvalues[1307]),0)
CONST_ENTRY_NID("id-smime-ct-TSTInfo","id-smime-ct-TSTInfo",NID_id_smime_ct_TSTInfo,
	11,&(lvalues[1318]),0)
CONST_ENTRY_NID("id-smime-ct-TDTInfo","id-smime-ct-TDTInfo",NID_id_smime_ct_TDTInfo,
	11,&(lvalues[1329]),0)
CONST_ENTRY_NID("id-smime-ct-contentInfo","id-smime-ct-contentInfo",
	NID_id_smime_ct_contentInfo,11,&(lvalues[1340]),0)
CONST_ENTRY_NID("id-smime-ct-DVCSRequestData","id-smime-ct-DVCSRequestData",
	NID_id_smime_ct_DVCSRequestData,11,&(lvalues[1351]),0)
CONST_ENTRY_NID("id-smime-ct-DVCSResponseData","id-smime-ct-DVCSResponseData",
	NID_id_smime_ct_DVCSResponseData,11,&(lvalues[1362]),0)
CONST_ENTRY_NID("id-smime-aa-receiptRequest","id-smime-aa-receiptRequest",
	NID_id_smime_aa_receiptRequest,11,&(lvalues[1373]),0)
CONST_ENTRY_NID("id-smime-aa-securityLabel","id-smime-aa-securityLabel",
	NID_id_smime_aa_securityLabel,11,&(lvalues[1384]),0)
CONST_ENTRY_NID("id-smime-aa-mlExpandHistory","id-smime-aa-mlExpandHistory",
	NID_id_smime_aa_mlExpandHistory,11,&(lvalues[1395]),0)
CONST_ENTRY_NID("id-smime-aa-contentHint","id-smime-aa-contentHint",
	NID_id_smime_aa_contentHint,11,&(lvalues[1406]),0)
CONST_ENTRY_NID("id-smime-aa-msgSigDigest","id-smime-aa-msgSigDigest",
	NID_id_smime_aa_msgSigDigest,11,&(lvalues[1417]),0)
CONST_ENTRY_NID("id-smime-aa-encapContentType","id-smime-aa-encapContentType",
	NID_id_smime_aa_encapContentType,11,&(lvalues[1428]),0)
CONST_ENTRY_NID("id-smime-aa-contentIdentifier","id-smime-aa-contentIdentifier",
	NID_id_smime_aa_contentIdentifier,11,&(lvalues[1439]),0)
CONST_ENTRY_NID("id-smime-aa-macValue","id-smime-aa-macValue",
	NID_id_smime_aa_macValue,11,&(lvalues[1450]),0)
CONST_ENTRY_NID("id-smime-aa-equivalentLabels","id-smime-aa-equivalentLabels",
	NID_id_smime_aa_equivalentLabels,11,&(lvalues[1461]),0)
CONST_ENTRY_NID("id-smime-aa-contentReference","id-smime-aa-contentReference",
	NID_id_smime_aa_contentReference,11,&(lvalues[1472]),0)
CONST_ENTRY_NID("id-smime-aa-encrypKeyPref","id-smime-aa-encrypKeyPref",
	NID_id_smime_aa_encrypKeyPref,11,&(lvalues[1483]),0)
CONST_ENTRY_NID("id-smime-aa-signingCertificate","id-smime-aa-signingCertificate",
	NID_id_smime_aa_signingCertificate,11,&(lvalues[1494]),0)
CONST_ENTRY_NID("id-smime-aa-smimeEncryptCerts","id-smime-aa-smimeEncryptCerts",
	NID_id_smime_aa_smimeEncryptCerts,11,&(lvalues[1505]),0)
CONST_ENTRY_NID("id-smime-aa-timeStampToken","id-smime-aa-timeStampToken",
	NID_id_smime_aa_timeStampToken,11,&(lvalues[1516]),0)
CONST_ENTRY_NID("id-smime-aa-ets-sigPolicyId","id-smime-aa-ets-sigPolicyId",
	NID_id_smime_aa_ets_sigPolicyId,11,&(lvalues[1527]),0)
CONST_ENTRY_NID("id-smime-aa-ets-commitmentType","id-smime-aa-ets-commitmentType",
	NID_id_smime_aa_ets_commitmentType,11,&(lvalues[1538]),0)
CONST_ENTRY_NID("id-smime-aa-ets-signerLocation","id-smime-aa-ets-signerLocation",
	NID_id_smime_aa_ets_signerLocation,11,&(lvalues[1549]),0)
CONST_ENTRY_NID("id-smime-aa-ets-signerAttr","id-smime-aa-ets-signerAttr",
	NID_id_smime_aa_ets_signerAttr,11,&(lvalues[1560]),0)
CONST_ENTRY_NID("id-smime-aa-ets-otherSigCert","id-smime-aa-ets-otherSigCert",
	NID_id_smime_aa_ets_otherSigCert,11,&(lvalues[1571]),0)
CONST_ENTRY_NID("id-smime-aa-ets-contentTimestamp",
	"id-smime-aa-ets-contentTimestamp",NID_id_smime_aa_ets_contentTimestamp,11,
	&(lvalues[1582]),0)
CONST_ENTRY_NID("id-smime-aa-ets-CertificateRefs","id-smime-aa-ets-CertificateRefs",
	NID_id_smime_aa_ets_CertificateRefs,11,&(lvalues[1593]),0)
CONST_ENTRY_NID("id-smime-aa-ets-RevocationRefs","id-smime-aa-ets-RevocationRefs",
	NID_id_smime_aa_ets_RevocationRefs,11,&(lvalues[1604]),0)
CONST_ENTRY_NID("id-smime-aa-ets-certValues","id-smime-aa-ets-certValues",
	NID_id_smime_aa_ets_certValues,11,&(lvalues[1615]),0)
CONST_ENTRY_NID("id-smime-aa-ets-revocationValues",
	"id-smime-aa-ets-revocationValues",NID_id_smime_aa_ets_revocationValues,11,
	&(lvalues[1626]),0)
CONST_ENTRY_NID("id-smime-aa-ets-escTimeStamp","id-smime-aa-ets-escTimeStamp",
	NID_id_smime_aa_ets_escTimeStamp,11,&(lvalues[1637]),0)
CONST_ENTRY_NID("id-smime-aa-ets-certCRLTimestamp",
	"id-smime-aa-ets-certCRLTimestamp",NID_id_smime_aa_ets_certCRLTimestamp,11,
	&(lvalues[1648]),0)
CONST_ENTRY_NID("id-smime-aa-ets-archiveTimeStamp",
	"id-smime-aa-ets-archiveTimeStamp",NID_id_smime_aa_ets_archiveTimeStamp,11,
	&(lvalues[1659]),0)
CONST_ENTRY_NID("id-smime-aa-signatureType","id-smime-aa-signatureType",
	NID_id_smime_aa_signatureType,11,&(lvalues[1670]),0)
CONST_ENTRY_NID("id-smime-aa-dvcs-dvc","id-smime-aa-dvcs-dvc",
	NID_id_smime_aa_dvcs_dvc,11,&(lvalues[1681]),0)
CONST_ENTRY_NID("id-smime-alg-ESDHwith3DES","id-smime-alg-ESDHwith3DES",
	NID_id_smime_alg_ESDHwith3DES,11,&(lvalues[1692]),0)
CONST_ENTRY_NID("id-smime-alg-ESDHwithRC2","id-smime-alg-ESDHwithRC2",
	NID_id_smime_alg_ESDHwithRC2,11,&(lvalues[1703]),0)
CONST_ENTRY_NID("id-smime-alg-3DESwrap","id-smime-alg-3DESwrap",
	NID_id_smime_alg_3DESwrap,11,&(lvalues[1714]),0)
CONST_ENTRY_NID("id-smime-alg-RC2wrap","id-smime-alg-RC2wrap",
	NID_id_smime_alg_RC2wrap,11,&(lvalues[1725]),0)
CONST_ENTRY_NID("id-smime-alg-ESDH","id-smime-alg-ESDH",NID_id_smime_alg_ESDH,11,
	&(lvalues[1736]),0)
CONST_ENTRY_NID("id-smime-alg-CMS3DESwrap","id-smime-alg-CMS3DESwrap",
	NID_id_smime_alg_CMS3DESwrap,11,&(lvalues[1747]),0)
CONST_ENTRY_NID("id-smime-alg-CMSRC2wrap","id-smime-alg-CMSRC2wrap",
	NID_id_smime_alg_CMSRC2wrap,11,&(lvalues[1758]),0)
CONST_ENTRY_NID("id-smime-cd-ldap","id-smime-cd-ldap",NID_id_smime_cd_ldap,11,
	&(lvalues[1769]),0)
CONST_ENTRY_NID("id-smime-spq-ets-sqt-uri","id-smime-spq-ets-sqt-uri",
	NID_id_smime_spq_ets_sqt_uri,11,&(lvalues[1780]),0)
CONST_ENTRY_NID("id-smime-spq-ets-sqt-unotice","id-smime-spq-ets-sqt-unotice",
	NID_id_smime_spq_ets_sqt_unotice,11,&(lvalues[1791]),0)
CONST_ENTRY_NID("id-smime-cti-ets-proofOfOrigin","id-smime-cti-ets-proofOfOrigin",
	NID_id_smime_cti_ets_proofOfOrigin,11,&(lvalues[1802]),0)
CONST_ENTRY_NID("id-smime-cti-ets-proofOfReceipt","id-smime-cti-ets-proofOfReceipt",
	NID_id_smime_cti_ets_proofOfReceipt,11,&(lvalues[1813]),0)
CONST_ENTRY_NID("id-smime-cti-ets-proofOfDelivery",
	"id-smime-cti-ets-proofOfDelivery",NID_id_smime_cti_ets_proofOfDelivery,11,
	&(lvalues[1824]),0)
CONST_ENTRY_NID("id-smime-cti-ets-proofOfSender","id-smime-cti-ets-proofOfSender",
	NID_id_smime_cti_ets_proofOfSender,11,&(lvalues[1835]),0)
CONST_ENTRY_NID("id-smime-cti-ets-proofOfApproval",
	"id-smime-cti-ets-proofOfApproval",NID_id_smime_cti_ets_proofOfApproval,11,
	&(lvalues[1846]),0)
CONST_ENTRY_NID("id-smime-cti-ets-proofOfCreation",
	"id-smime-cti-ets-proofOfCreation",NID_id_smime_cti_ets_proofOfCreation,11,
	&(lvalues[1857]),0)
CONST_ENTRY_NID("MD4","md4",NID_md4,8,&(lvalues[1868]),0)
CONST_ENTRY_NID("id-pkix-mod","id-pkix-mod",NID_id_pkix_mod,7,&(lvalues[1876]),0)
CONST_ENTRY_NID("id-qt","id-qt",NID_id_qt,7,&(lvalues[1883]),0)
CONST_ENTRY_NID("id-it","id-it",NID_id_it,7,&(lvalues[1890]),0)
CONST_ENTRY_NID("id-pkip","id-pkip",NID_id_pkip,7,&(lvalues[1897]),0)
CONST_ENTRY_NID("id-alg","id-alg",NID_id_alg,7,&(lvalues[1904]),0)
CONST_ENTRY_NID("id-cmc","id-cmc",NID_id_cmc,7,&(lvalues[1911]),0)
CONST_ENTRY_NID("id-on","id-on",NID_id_on,7,&(lvalues[1918]),0)
CONST_ENTRY_NID("id-pda","id-pda",NID_id_pda,7,&(lvalues[1925]),0)
CONST_ENTRY_NID("id-aca","id-aca",NID_id_aca,7,&(lvalues[1932]),0)
CONST_ENTRY_NID("id-qcs","id-qcs",NID_id_qcs,7,&(lvalues[1939]),0)
CONST_ENTRY_NID("id-cct","id-cct",NID_id_cct,7,&(lvalues[1946]),0)
CONST_ENTRY_NID("id-pkix1-explicit-88","id-pkix1-explicit-88",
	NID_id_pkix1_explicit_88,8,&(lvalues[1953]),0)
CONST_ENTRY_NID("id-pkix1-implicit-88","id-pkix1-implicit-88",
	NID_id_pkix1_implicit_88,8,&(lvalues[1961]),0)
CONST_ENTRY_NID("id-pkix1-explicit-93","id-pkix1-explicit-93",
	NID_id_pkix1_explicit_93,8,&(lvalues[1969]),0)
CONST_ENTRY_NID("id-pkix1-implicit-93","id-pkix1-implicit-93",
	NID_id_pkix1_implicit_93,8,&(lvalues[1977]),0)
CONST_ENTRY_NID("id-mod-crmf","id-mod-crmf",NID_id_mod_crmf,8,&(lvalues[1985]),0)
CONST_ENTRY_NID("id-mod-cmc","id-mod-cmc",NID_id_mod_cmc,8,&(lvalues[1993]),0)
CONST_ENTRY_NID("id-mod-kea-profile-88","id-mod-kea-profile-88",
	NID_id_mod_kea_profile_88,8,&(lvalues[2001]),0)
CONST_ENTRY_NID("id-mod-kea-profile-93","id-mod-kea-profile-93",
	NID_id_mod_kea_profile_93,8,&(lvalues[2009]),0)
CONST_ENTRY_NID("id-mod-cmp","id-mod-cmp",NID_id_mod_cmp,8,&(lvalues[2017]),0)
CONST_ENTRY_NID("id-mod-qualified-cert-88","id-mod-qualified-cert-88",
	NID_id_mod_qualified_cert_88,8,&(lvalues[2025]),0)
CONST_ENTRY_NID("id-mod-qualified-cert-93","id-mod-qualified-cert-93",
	NID_id_mod_qualified_cert_93,8,&(lvalues[2033]),0)
CONST_ENTRY_NID("id-mod-attribute-cert","id-mod-attribute-cert",
	NID_id_mod_attribute_cert,8,&(lvalues[2041]),0)
CONST_ENTRY_NID("id-mod-timestamp-protocol","id-mod-timestamp-protocol",
	NID_id_mod_timestamp_protocol,8,&(lvalues[2049]),0)
CONST_ENTRY_NID("id-mod-ocsp","id-mod-ocsp",NID_id_mod_ocsp,8,&(lvalues[2057]),0)
CONST_ENTRY_NID("id-mod-dvcs","id-mod-dvcs",NID_id_mod_dvcs,8,&(lvalues[2065]),0)
CONST_ENTRY_NID("id-mod-cmp2000","id-mod-cmp2000",NID_id_mod_cmp2000,8,
	&(lvalues[2073]),0)
CONST_ENTRY_NID("biometricInfo","Biometric Info",NID_biometricInfo,8,&(lvalues[2081]),0)
CONST_ENTRY_NID("qcStatements","qcStatements",NID_qcStatements,8,&(lvalues[2089]),0)
CONST_ENTRY_NID("ac-auditEntity","ac-auditEntity",NID_ac_auditEntity,8,
	&(lvalues[2097]),0)
CONST_ENTRY_NID("ac-targeting","ac-targeting",NID_ac_targeting,8,&(lvalues[2105]),0)
CONST_ENTRY_NID("aaControls","aaControls",NID_aaControls,8,&(lvalues[2113]),0)
CONST_ENTRY_NID("sbgp-ipAddrBlock","sbgp-ipAddrBlock",NID_sbgp_ipAddrBlock,8,
	&(lvalues[2121]),0)
CONST_ENTRY_NID("sbgp-autonomousSysNum","sbgp-autonomousSysNum",
	NID_sbgp_autonomousSysNum,8,&(lvalues[2129]),0)
CONST_ENTRY_NID("sbgp-routerIdentifier","sbgp-routerIdentifier",
	NID_sbgp_routerIdentifier,8,&(lvalues[2137]),0)
CONST_ENTRY_NID("textNotice","textNotice",NID_textNotice,8,&(lvalues[2145]),0)
CONST_ENTRY_NID("ipsecEndSystem","IPSec End System",NID_ipsecEndSystem,8,
	&(lvalues[2153]),0)
CONST_ENTRY_NID("ipsecTunnel","IPSec Tunnel",NID_ipsecTunnel,8,&(lvalues[2161]),0)
CONST_ENTRY_NID("ipsecUser","IPSec User",NID_ipsecUser,8,&(lvalues[2169]),0)
CONST_ENTRY_NID("DVCS","dvcs",NID_dvcs,8,&(lvalues[2177]),0)
CONST_ENTRY_NID("id-it-caProtEncCert","id-it-caProtEncCert",NID_id_it_caProtEncCert,
	8,&(lvalues[2185]),0)
CONST_ENTRY_NID("id-it-signKeyPairTypes","id-it-signKeyPairTypes",
	NID_id_it_signKeyPairTypes,8,&(lvalues[2193]),0)
CONST_ENTRY_NID("id-it-encKeyPairTypes","id-it-encKeyPairTypes",
	NID_id_it_encKeyPairTypes,8,&(lvalues[2201]),0)
CONST_ENTRY_NID("id-it-preferredSymmAlg","id-it-preferredSymmAlg",
	NID_id_it_preferredSymmAlg,8,&(lvalues[2209]),0)
CONST_ENTRY_NID("id-it-caKeyUpdateInfo","id-it-caKeyUpdateInfo",
	NID_id_it_caKeyUpdateInfo,8,&(lvalues[2217]),0)
CONST_ENTRY_NID("id-it-currentCRL","id-it-currentCRL",NID_id_it_currentCRL,8,
	&(lvalues[2225]),0)
CONST_ENTRY_NID("id-it-unsupportedOIDs","id-it-unsupportedOIDs",
	NID_id_it_unsupportedOIDs,8,&(lvalues[2233]),0)
CONST_ENTRY_NID("id-it-subscriptionRequest","id-it-subscriptionRequest",
	NID_id_it_subscriptionRequest,8,&(lvalues[2241]),0)
CONST_ENTRY_NID("id-it-subscriptionResponse","id-it-subscriptionResponse",
	NID_id_it_subscriptionResponse,8,&(lvalues[2249]),0)
CONST_ENTRY_NID("id-it-keyPairParamReq","id-it-keyPairParamReq",
	NID_id_it_keyPairParamReq,8,&(lvalues[2257]),0)
CONST_ENTRY_NID("id-it-keyPairParamRep","id-it-keyPairParamRep",
	NID_id_it_keyPairParamRep,8,&(lvalues[2265]),0)
CONST_ENTRY_NID("id-it-revPassphrase","id-it-revPassphrase",NID_id_it_revPassphrase,
	8,&(lvalues[2273]),0)
CONST_ENTRY_NID("id-it-implicitConfirm","id-it-implicitConfirm",
	NID_id_it_implicitConfirm,8,&(lvalues[2281]),0)
CONST_ENTRY_NID("id-it-confirmWaitTime","id-it-confirmWaitTime",
	NID_id_it_confirmWaitTime,8,&(lvalues[2289]),0)
CONST_ENTRY_NID("id-it-origPKIMessage","id-it-origPKIMessage",
	NID_id_it_origPKIMessage,8,&(lvalues[2297]),0)
CONST_ENTRY_NID("id-regCtrl","id-regCtrl",NID_id_regCtrl,8,&(lvalues[2305]),0)
CONST_ENTRY_NID("id-regInfo","id-regInfo",NID_id_regInfo,8,&(lvalues[2313]),0)
CONST_ENTRY_NID("id-regCtrl-regToken","id-regCtrl-regToken",NID_id_regCtrl_regToken,
	9,&(lvalues[2321]),0)
CONST_ENTRY_NID("id-regCtrl-authenticator","id-regCtrl-authenticator",
	NID_id_regCtrl_authenticator,9,&(lvalues[2330]),0)
CONST_ENTRY_NID("id-regCtrl-pkiPublicationInfo","id-regCtrl-pkiPublicationInfo",
	NID_id_regCtrl_pkiPublicationInfo,9,&(lvalues[2339]),0)
CONST_ENTRY_NID("id-regCtrl-pkiArchiveOptions","id-regCtrl-pkiArchiveOptions",
	NID_id_regCtrl_pkiArchiveOptions,9,&(lvalues[2348]),0)
CONST_ENTRY_NID("id-regCtrl-oldCertID","id-regCtrl-oldCertID",
	NID_id_regCtrl_oldCertID,9,&(lvalues[2357]),0)
CONST_ENTRY_NID("id-regCtrl-protocolEncrKey","id-regCtrl-protocolEncrKey",
	NID_id_regCtrl_protocolEncrKey,9,&(lvalues[2366]),0)
CONST_ENTRY_NID("id-regInfo-utf8Pairs","id-regInfo-utf8Pairs",
	NID_id_regInfo_utf8Pairs,9,&(lvalues[2375]),0)
CONST_ENTRY_NID("id-regInfo-certReq","id-regInfo-certReq",NID_id_regInfo_certReq,9,
	&(lvalues[2384]),0)
CONST_ENTRY_NID("id-alg-des40","id-alg-des40",NID_id_alg_des40,8,&(lvalues[2393]),0)
CONST_ENTRY_NID("id-alg-noSignature","id-alg-noSignature",NID_id_alg_noSignature,8,
	&(lvalues[2401]),0)
CONST_ENTRY_NID("id-alg-dh-sig-hmac-sha1","id-alg-dh-sig-hmac-sha1",
	NID_id_alg_dh_sig_hmac_sha1,8,&(lvalues[2409]),0)
CONST_ENTRY_NID("id-alg-dh-pop","id-alg-dh-pop",NID_id_alg_dh_pop,8,&(lvalues[2417]),0)
CONST_ENTRY_NID("id-cmc-statusInfo","id-cmc-statusInfo",NID_id_cmc_statusInfo,8,
	&(lvalues[2425]),0)
CONST_ENTRY_NID("id-cmc-identification","id-cmc-identification",
	NID_id_cmc_identification,8,&(lvalues[2433]),0)
CONST_ENTRY_NID("id-cmc-identityProof","id-cmc-identityProof",
	NID_id_cmc_identityProof,8,&(lvalues[2441]),0)
CONST_ENTRY_NID("id-cmc-dataReturn","id-cmc-dataReturn",NID_id_cmc_dataReturn,8,
	&(lvalues[2449]),0)
CONST_ENTRY_NID("id-cmc-transactionId","id-cmc-transactionId",
	NID_id_cmc_transactionId,8,&(lvalues[2457]),0)
CONST_ENTRY_NID("id-cmc-senderNonce","id-cmc-senderNonce",NID_id_cmc_senderNonce,8,
	&(lvalues[2465]),0)
CONST_ENTRY_NID("id-cmc-recipientNonce","id-cmc-recipientNonce",
	NID_id_cmc_recipientNonce,8,&(lvalues[2473]),0)
CONST_ENTRY_NID("id-cmc-addExtensions","id-cmc-addExtensions",
	NID_id_cmc_addExtensions,8,&(lvalues[2481]),0)
CONST_ENTRY_NID("id-cmc-encryptedPOP","id-cmc-encryptedPOP",NID_id_cmc_encryptedPOP,
	8,&(lvalues[2489]),0)
CONST_ENTRY_NID("id-cmc-decryptedPOP","id-cmc-decryptedPOP",NID_id_cmc_decryptedPOP,
	8,&(lvalues[2497]),0)
CONST_ENTRY_NID("id-cmc-lraPOPWitness","id-cmc-lraPOPWitness",
	NID_id_cmc_lraPOPWitness,8,&(lvalues[2505]),0)
CONST_ENTRY_NID("id-cmc-getCert","id-cmc-getCert",NID_id_cmc_getCert,8,
	&(lvalues[2513]),0)
CONST_ENTRY_NID("id-cmc-getCRL","id-cmc-getCRL",NID_id_cmc_getCRL,8,&(lvalues[2521]),0)
CONST_ENTRY_NID("id-cmc-revokeRequest","id-cmc-revokeRequest",
	NID_id_cmc_revokeRequest,8,&(lvalues[2529]),0)
CONST_ENTRY_NID("id-cmc-regInfo","id-cmc-regInfo",NID_id_cmc_regInfo,8,
	&(lvalues[2537]),0)
CONST_ENTRY_NID("id-cmc-responseInfo","id-cmc-responseInfo",NID_id_cmc_responseInfo,
	8,&(lvalues[2545]),0)
CONST_ENTRY_NID("id-cmc-queryPending","id-cmc-queryPending",NID_id_cmc_queryPending,
	8,&(lvalues[2553]),0)
CONST_ENTRY_NID("id-cmc-popLinkRandom","id-cmc-popLinkRandom",
	NID_id_cmc_popLinkRandom,8,&(lvalues[2561]),0)
CONST_ENTRY_NID("id-cmc-popLinkWitness","id-cmc-popLinkWitness",
	NID_id_cmc_popLinkWitness,8,&(lvalues[2569]),0)
CONST_ENTRY_NID("id-cmc-confirmCertAcceptance","id-cmc-confirmCertAcceptance",
	NID_id_cmc_confirmCertAcceptance,8,&(lvalues[2577]),0)
CONST_ENTRY_NID("id-on-personalData","id-on-personalData",NID_id_on_personalData,8,
	&(lvalues[2585]),0)
CONST_ENTRY_NID("id-pda-dateOfBirth","id-pda-dateOfBirth",NID_id_pda_dateOfBirth,8,
	&(lvalues[2593]),0)
CONST_ENTRY_NID("id-pda-placeOfBirth","id-pda-placeOfBirth",NID_id_pda_placeOfBirth,
	8,&(lvalues[2601]),0)
CONST_ENTRY_NID(NULL,NULL,NID_undef,0,NULL,0)
CONST_ENTRY_NID("id-pda-gender","id-pda-gender",NID_id_pda_gender,8,&(lvalues[2609]),0)
CONST_ENTRY_NID("id-pda-countryOfCitizenship","id-pda-countryOfCitizenship",
	NID_id_pda_countryOfCitizenship,8,&(lvalues[2617]),0)
CONST_ENTRY_NID("id-pda-countryOfResidence","id-pda-countryOfResidence",
	NID_id_pda_countryOfResidence,8,&(lvalues[2625]),0)
CONST_ENTRY_NID("id-aca-authenticationInfo","id-aca-authenticationInfo",
	NID_id_aca_authenticationInfo,8,&(lvalues[2633]),0)
CONST_ENTRY_NID("id-aca-accessIdentity","id-aca-accessIdentity",
	NID_id_aca_accessIdentity,8,&(lvalues[2641]),0)
CONST_ENTRY_NID("id-aca-chargingIdentity","id-aca-chargingIdentity",
	NID_id_aca_chargingIdentity,8,&(lvalues[2649]),0)
CONST_ENTRY_NID("id-aca-group","id-aca-group",NID_id_aca_group,8,&(lvalues[2657]),0)
CONST_ENTRY_NID("id-aca-role","id-aca-role",NID_id_aca_role,8,&(lvalues[2665]),0)
CONST_ENTRY_NID("id-qcs-pkixQCSyntax-v1","id-qcs-pkixQCSyntax-v1",
	NID_id_qcs_pkixQCSyntax_v1,8,&(lvalues[2673]),0)
CONST_ENTRY_NID("id-cct-crs","id-cct-crs",NID_id_cct_crs,8,&(lvalues[2681]),0)
CONST_ENTRY_NID("id-cct-PKIData","id-cct-PKIData",NID_id_cct_PKIData,8,
	&(lvalues[2689]),0)
CONST_ENTRY_NID("id-cct-PKIResponse","id-cct-PKIResponse",NID_id_cct_PKIResponse,8,
	&(lvalues[2697]),0)
CONST_ENTRY_NID("ad_timestamping","AD Time Stamping",NID_ad_timeStamping,8,
	&(lvalues[2705]),0)
CONST_ENTRY_NID("AD_DVCS","ad dvcs",NID_ad_dvcs,8,&(lvalues[2713]),0)
CONST_ENTRY_NID("basicOCSPResponse","Basic OCSP Response",NID_id_pkix_OCSP_basic,9,
	&(lvalues[2721]),0)
CONST_ENTRY_NID("Nonce","OCSP Nonce",NID_id_pkix_OCSP_Nonce,9,&(lvalues[2730]),0)
CONST_ENTRY_NID("CrlID","OCSP CRL ID",NID_id_pkix_OCSP_CrlID,9,&(lvalues[2739]),0)
CONST_ENTRY_NID("acceptableResponses","Acceptable OCSP Responses",
	NID_id_pkix_OCSP_acceptableResponses,9,&(lvalues[2748]),0)
CONST_ENTRY_NID("noCheck","OCSP No Check",NID_id_pkix_OCSP_noCheck,9,&(lvalues[2757]),0)
CONST_ENTRY_NID("archiveCutoff","OCSP Archive Cutoff",NID_id_pkix_OCSP_archiveCutoff,
	9,&(lvalues[2766]),0)
CONST_ENTRY_NID("serviceLocator","OCSP Service Locator",
	NID_id_pkix_OCSP_serviceLocator,9,&(lvalues[2775]),0)
CONST_ENTRY_NID("extendedStatus","Extended OCSP Status",
	NID_id_pkix_OCSP_extendedStatus,9,&(lvalues[2784]),0)
CONST_ENTRY_NID("valid","valid",NID_id_pkix_OCSP_valid,9,&(lvalues[2793]),0)
CONST_ENTRY_NID("path","path",NID_id_pkix_OCSP_path,9,&(lvalues[2802]),0)
CONST_ENTRY_NID("trustRoot","Trust Root",NID_id_pkix_OCSP_trustRoot,9,
	&(lvalues[2811]),0)
CONST_ENTRY_NID("algorithm","algorithm",NID_algorithm,4,&(lvalues[2820]),0)
CONST_ENTRY_NID("rsaSignature","rsaSignature",NID_rsaSignature,5,&(lvalues[2824]),0)
CONST_ENTRY_NID("X500algorithms","directory services - algorithms",
	NID_X500algorithms,2,&(lvalues[2829]),0)
CONST_ENTRY_NID("ORG","org",NID_org,1,&(lvalues[2831]),0)
CONST_ENTRY_NID("DOD","dod",NID_dod,2,&(lvalues[2832]),0)
CONST_ENTRY_NID("IANA","iana",NID_iana,3,&(lvalues[2834]),0)
CONST_ENTRY_NID("directory","Directory",NID_Directory,4,&(lvalues[2837]),0)
CONST_ENTRY_NID("mgmt","Management",NID_Management,4,&(lvalues[2841]),0)
CONST_ENTRY_NID("experimental","Experimental",NID_Experimental,4,&(lvalues[2845]),0)
CONST_ENTRY_NID("private","Private",NID_Private,4,&(lvalues[2849]),0)
CONST_ENTRY_NID("security","Security",NID_Security,4,&(lvalues[2853]),0)
CONST_ENTRY_NID("snmpv2","SNMPv2",NID_SNMPv2,4,&(lvalues[2857]),0)
CONST_ENTRY_NID("Mail","Mail",NID_Mail,4,&(lvalues[2861]),0)
CONST_ENTRY_NID("enterprises","Enterprises",NID_Enterprises,5,&(lvalues[2865]),0)
CONST_ENTRY_NID("dcobject","dcObject",NID_dcObject,9,&(lvalues[2870]),0)
CONST_ENTRY_NID("DC","domainComponent",NID_domainComponent,10,&(lvalues[2879]),0)
CONST_ENTRY_NID("domain","Domain",NID_Domain,10,&(lvalues[2889]),0)
CONST_ENTRY_NID("NULL","NULL",NID_joint_iso_ccitt,1,&(lvalues[2899]),0)
CONST_ENTRY_NID("selected-attribute-types","Selected Attribute Types",
	NID_selected_attribute_types,3,&(lvalues[2900]),0)
CONST_ENTRY_NID("clearance","clearance",NID_clearance,4,&(lvalues[2903]),0)
CONST_ENTRY_NID("RSA-MD4","md4WithRSAEncryption",NID_md4WithRSAEncryption,9,
	&(lvalues[2907]),0)
CONST_ENTRY_NID("ac-proxying","ac-proxying",NID_ac_proxying,8,&(lvalues[2916]),0)
CONST_ENTRY_NID("subjectInfoAccess","Subject Information Access",NID_sinfo_access,8,
	&(lvalues[2924]),0)
CONST_ENTRY_NID("id-aca-encAttrs","id-aca-encAttrs",NID_id_aca_encAttrs,8,
	&(lvalues[2932]),0)
CONST_ENTRY_NID("role","role",NID_role,3,&(lvalues[2940]),0)
CONST_ENTRY_NID("policyConstraints","X509v3 Policy Constraints",
	NID_policy_constraints,3,&(lvalues[2943]),0)
CONST_ENTRY_NID("targetInformation","X509v3 AC Targeting",NID_target_information,3,
	&(lvalues[2946]),0)
CONST_ENTRY_NID("noRevAvail","X509v3 No Revocation Available",NID_no_rev_avail,3,
	&(lvalues[2949]),0)
CONST_ENTRY_NID("NULL","NULL",NID_ccitt,1,&(lvalues[2952]),0)
CONST_ENTRY_NID("ansi-X9-62","ANSI X9.62",NID_ansi_X9_62,5,&(lvalues[2953]),0)
CONST_ENTRY_NID("prime-field","prime-field",NID_X9_62_prime_field,7,&(lvalues[2958]),0)
CONST_ENTRY_NID("characteristic-two-field","characteristic-two-field",
	NID_X9_62_characteristic_two_field,7,&(lvalues[2965]),0)
CONST_ENTRY_NID("id-ecPublicKey","id-ecPublicKey",NID_X9_62_id_ecPublicKey,7,
	&(lvalues[2972]),0)
CONST_ENTRY_NID("prime192v1","prime192v1",NID_X9_62_prime192v1,8,&(lvalues[2979]),0)
CONST_ENTRY_NID("prime192v2","prime192v2",NID_X9_62_prime192v2,8,&(lvalues[2987]),0)
CONST_ENTRY_NID("prime192v3","prime192v3",NID_X9_62_prime192v3,8,&(lvalues[2995]),0)
CONST_ENTRY_NID("prime239v1","prime239v1",NID_X9_62_prime239v1,8,&(lvalues[3003]),0)
CONST_ENTRY_NID("prime239v2","prime239v2",NID_X9_62_prime239v2,8,&(lvalues[3011]),0)
CONST_ENTRY_NID("prime239v3","prime239v3",NID_X9_62_prime239v3,8,&(lvalues[3019]),0)
CONST_ENTRY_NID("prime256v1","prime256v1",NID_X9_62_prime256v1,8,&(lvalues[3027]),0)
CONST_ENTRY_NID("ecdsa-with-SHA1","ecdsa-with-SHA1",NID_ecdsa_with_SHA1,7,
	&(lvalues[3035]),0)
CONST_ENTRY_NID("CSPName","Microsoft CSP Name",NID_ms_csp_name,9,&(lvalues[3042]),0)
CONST_ENTRY_NID("AES-128-ECB","aes-128-ecb",NID_aes_128_ecb,9,&(lvalues[3051]),0)
CONST_ENTRY_NID("AES-128-CBC","aes-128-cbc",NID_aes_128_cbc,9,&(lvalues[3060]),0)
CONST_ENTRY_NID("AES-128-OFB","aes-128-ofb",NID_aes_128_ofb128,9,&(lvalues[3069]),0)
CONST_ENTRY_NID("AES-128-CFB","aes-128-cfb",NID_aes_128_cfb128,9,&(lvalues[3078]),0)
CONST_ENTRY_NID("AES-192-ECB","aes-192-ecb",NID_aes_192_ecb,9,&(lvalues[3087]),0)
CONST_ENTRY_NID("AES-192-CBC","aes-192-cbc",NID_aes_192_cbc,9,&(lvalues[3096]),0)
CONST_ENTRY_NID("AES-192-OFB","aes-192-ofb",NID_aes_192_ofb128,9,&(lvalues[3105]),0)
CONST_ENTRY_NID("AES-192-CFB","aes-192-cfb",NID_aes_192_cfb128,9,&(lvalues[3114]),0)
CONST_ENTRY_NID("AES-256-ECB","aes-256-ecb",NID_aes_256_ecb,9,&(lvalues[3123]),0)
CONST_ENTRY_NID("AES-256-CBC","aes-256-cbc",NID_aes_256_cbc,9,&(lvalues[3132]),0)
CONST_ENTRY_NID("AES-256-OFB","aes-256-ofb",NID_aes_256_ofb128,9,&(lvalues[3141]),0)
CONST_ENTRY_NID("AES-256-CFB","aes-256-cfb",NID_aes_256_cfb128,9,&(lvalues[3150]),0)
CONST_ENTRY_NID("holdInstructionCode","Hold Instruction Code",
	NID_hold_instruction_code,3,&(lvalues[3159]),0)
CONST_ENTRY_NID("holdInstructionNone","Hold Instruction None",
	NID_hold_instruction_none,7,&(lvalues[3162]),0)
CONST_ENTRY_NID("holdInstructionCallIssuer","Hold Instruction Call Issuer",
	NID_hold_instruction_call_issuer,7,&(lvalues[3169]),0)
CONST_ENTRY_NID("holdInstructionReject","Hold Instruction Reject",
	NID_hold_instruction_reject,7,&(lvalues[3176]),0)
CONST_ENTRY_NID("data","data",NID_data,1,&(lvalues[3183]),0)
CONST_ENTRY_NID("pss","pss",NID_pss,3,&(lvalues[3184]),0)
CONST_ENTRY_NID("ucl","ucl",NID_ucl,7,&(lvalues[3187]),0)
CONST_ENTRY_NID("pilot","pilot",NID_pilot,8,&(lvalues[3194]),0)
CONST_ENTRY_NID("pilotAttributeType","pilotAttributeType",NID_pilotAttributeType,9,
	&(lvalues[3202]),0)
CONST_ENTRY_NID("pilotAttributeSyntax","pilotAttributeSyntax",
	NID_pilotAttributeSyntax,9,&(lvalues[3211]),0)
CONST_ENTRY_NID("pilotObjectClass","pilotObjectClass",NID_pilotObjectClass,9,
	&(lvalues[3220]),0)
CONST_ENTRY_NID("pilotGroups","pilotGroups",NID_pilotGroups,9,&(lvalues[3229]),0)
CONST_ENTRY_NID("iA5StringSyntax","iA5StringSyntax",NID_iA5StringSyntax,10,
	&(lvalues[3238]),0)
CONST_ENTRY_NID("caseIgnoreIA5StringSyntax","caseIgnoreIA5StringSyntax",
	NID_caseIgnoreIA5StringSyntax,10,&(lvalues[3248]),0)
CONST_ENTRY_NID("pilotObject","pilotObject",NID_pilotObject,10,&(lvalues[3258]),0)
CONST_ENTRY_NID("pilotPerson","pilotPerson",NID_pilotPerson,10,&(lvalues[3268]),0)
CONST_ENTRY_NID("account","account",NID_account,10,&(lvalues[3278]),0)
CONST_ENTRY_NID("document","document",NID_document,10,&(lvalues[3288]),0)
CONST_ENTRY_NID("room","room",NID_room,10,&(lvalues[3298]),0)
CONST_ENTRY_NID("documentSeries","documentSeries",NID_documentSeries,10,
	&(lvalues[3308]),0)
CONST_ENTRY_NID("rFC822localPart","rFC822localPart",NID_rFC822localPart,10,
	&(lvalues[3318]),0)
CONST_ENTRY_NID("dNSDomain","dNSDomain",NID_dNSDomain,10,&(lvalues[3328]),0)
CONST_ENTRY_NID("domainRelatedObject","domainRelatedObject",NID_domainRelatedObject,
	10,&(lvalues[3338]),0)
CONST_ENTRY_NID("friendlyCountry","friendlyCountry",NID_friendlyCountry,10,
	&(lvalues[3348]),0)
CONST_ENTRY_NID("simpleSecurityObject","simpleSecurityObject",
	NID_simpleSecurityObject,10,&(lvalues[3358]),0)
CONST_ENTRY_NID("pilotOrganization","pilotOrganization",NID_pilotOrganization,10,
	&(lvalues[3368]),0)
CONST_ENTRY_NID("pilotDSA","pilotDSA",NID_pilotDSA,10,&(lvalues[3378]),0)
CONST_ENTRY_NID("qualityLabelledData","qualityLabelledData",NID_qualityLabelledData,
	10,&(lvalues[3388]),0)
CONST_ENTRY_NID("UID","userId",NID_userId,10,&(lvalues[3398]),0)
CONST_ENTRY_NID("textEncodedORAddress","textEncodedORAddress",
	NID_textEncodedORAddress,10,&(lvalues[3408]),0)
CONST_ENTRY_NID("mail","rfc822Mailbox",NID_rfc822Mailbox,10,&(lvalues[3418]),0)
CONST_ENTRY_NID("info","info",NID_info,10,&(lvalues[3428]),0)
CONST_ENTRY_NID("favouriteDrink","favouriteDrink",NID_favouriteDrink,10,
	&(lvalues[3438]),0)
CONST_ENTRY_NID("roomNumber","roomNumber",NID_roomNumber,10,&(lvalues[3448]),0)
CONST_ENTRY_NID("photo","photo",NID_photo,10,&(lvalues[3458]),0)
CONST_ENTRY_NID("userClass","userClass",NID_userClass,10,&(lvalues[3468]),0)
CONST_ENTRY_NID("host","host",NID_host,10,&(lvalues[3478]),0)
CONST_ENTRY_NID("manager","manager",NID_manager,10,&(lvalues[3488]),0)
CONST_ENTRY_NID("documentIdentifier","documentIdentifier",NID_documentIdentifier,10,
	&(lvalues[3498]),0)
CONST_ENTRY_NID("documentTitle","documentTitle",NID_documentTitle,10,&(lvalues[3508]),0)
CONST_ENTRY_NID("documentVersion","documentVersion",NID_documentVersion,10,
	&(lvalues[3518]),0)
CONST_ENTRY_NID("documentAuthor","documentAuthor",NID_documentAuthor,10,
	&(lvalues[3528]),0)
CONST_ENTRY_NID("documentLocation","documentLocation",NID_documentLocation,10,
	&(lvalues[3538]),0)
CONST_ENTRY_NID("homeTelephoneNumber","homeTelephoneNumber",NID_homeTelephoneNumber,
	10,&(lvalues[3548]),0)
CONST_ENTRY_NID("secretary","secretary",NID_secretary,10,&(lvalues[3558]),0)
CONST_ENTRY_NID("otherMailbox","otherMailbox",NID_otherMailbox,10,&(lvalues[3568]),0)
CONST_ENTRY_NID("lastModifiedTime","lastModifiedTime",NID_lastModifiedTime,10,
	&(lvalues[3578]),0)
CONST_ENTRY_NID("lastModifiedBy","lastModifiedBy",NID_lastModifiedBy,10,
	&(lvalues[3588]),0)
CONST_ENTRY_NID("aRecord","aRecord",NID_aRecord,10,&(lvalues[3598]),0)
CONST_ENTRY_NID("pilotAttributeType27","pilotAttributeType27",
	NID_pilotAttributeType27,10,&(lvalues[3608]),0)
CONST_ENTRY_NID("mXRecord","mXRecord",NID_mXRecord,10,&(lvalues[3618]),0)
CONST_ENTRY_NID("nSRecord","nSRecord",NID_nSRecord,10,&(lvalues[3628]),0)
CONST_ENTRY_NID("sOARecord","sOARecord",NID_sOARecord,10,&(lvalues[3638]),0)
CONST_ENTRY_NID("cNAMERecord","cNAMERecord",NID_cNAMERecord,10,&(lvalues[3648]),0)
CONST_ENTRY_NID("associatedDomain","associatedDomain",NID_associatedDomain,10,
	&(lvalues[3658]),0)
CONST_ENTRY_NID("associatedName","associatedName",NID_associatedName,10,
	&(lvalues[3668]),0)
CONST_ENTRY_NID("homePostalAddress","homePostalAddress",NID_homePostalAddress,10,
	&(lvalues[3678]),0)
CONST_ENTRY_NID("personalTitle","personalTitle",NID_personalTitle,10,&(lvalues[3688]),0)
CONST_ENTRY_NID("mobileTelephoneNumber","mobileTelephoneNumber",
	NID_mobileTelephoneNumber,10,&(lvalues[3698]),0)
CONST_ENTRY_NID("pagerTelephoneNumber","pagerTelephoneNumber",
	NID_pagerTelephoneNumber,10,&(lvalues[3708]),0)
CONST_ENTRY_NID("friendlyCountryName","friendlyCountryName",NID_friendlyCountryName,
	10,&(lvalues[3718]),0)
CONST_ENTRY_NID("organizationalStatus","organizationalStatus",
	NID_organizationalStatus,10,&(lvalues[3728]),0)
CONST_ENTRY_NID("janetMailbox","janetMailbox",NID_janetMailbox,10,&(lvalues[3738]),0)
CONST_ENTRY_NID("mailPreferenceOption","mailPreferenceOption",
	NID_mailPreferenceOption,10,&(lvalues[3748]),0)
CONST_ENTRY_NID("buildingName","buildingName",NID_buildingName,10,&(lvalues[3758]),0)
CONST_ENTRY_NID("dSAQuality","dSAQuality",NID_dSAQuality,10,&(lvalues[3768]),0)
CONST_ENTRY_NID("singleLevelQuality","singleLevelQuality",NID_singleLevelQuality,10,
	&(lvalues[3778]),0)
CONST_ENTRY_NID("subtreeMinimumQuality","subtreeMinimumQuality",
	NID_subtreeMinimumQuality,10,&(lvalues[3788]),0)
CONST_ENTRY_NID("subtreeMaximumQuality","subtreeMaximumQuality",
	NID_subtreeMaximumQuality,10,&(lvalues[3798]),0)
CONST_ENTRY_NID("personalSignature","personalSignature",NID_personalSignature,10,
	&(lvalues[3808]),0)
CONST_ENTRY_NID("dITRedirect","dITRedirect",NID_dITRedirect,10,&(lvalues[3818]),0)
CONST_ENTRY_NID("audio","audio",NID_audio,10,&(lvalues[3828]),0)
CONST_ENTRY_NID("documentPublisher","documentPublisher",NID_documentPublisher,10,
	&(lvalues[3838]),0)
CONST_ENTRY_NID("x500UniqueIdentifier","x500UniqueIdentifier",
	NID_x500UniqueIdentifier,3,&(lvalues[3848]),0)
CONST_ENTRY_NID("mime-mhs","MIME MHS",NID_mime_mhs,5,&(lvalues[3851]),0)
CONST_ENTRY_NID("mime-mhs-headings","mime-mhs-headings",NID_mime_mhs_headings,6,
	&(lvalues[3856]),0)
CONST_ENTRY_NID("mime-mhs-bodies","mime-mhs-bodies",NID_mime_mhs_bodies,6,
	&(lvalues[3862]),0)
CONST_ENTRY_NID("id-hex-partial-message","id-hex-partial-message",
	NID_id_hex_partial_message,7,&(lvalues[3868]),0)
CONST_ENTRY_NID("id-hex-multipart-message","id-hex-multipart-message",
	NID_id_hex_multipart_message,7,&(lvalues[3875]),0)
CONST_ENTRY_NID("generationQualifier","generationQualifier",NID_generationQualifier,
	3,&(lvalues[3882]),0)
CONST_ENTRY_NID("pseudonym","pseudonym",NID_pseudonym,3,&(lvalues[3885]),0)
CONST_ENTRY_NID(NULL,NULL,NID_undef,0,NULL,0)
CONST_ENTRY_NID("id-set","Secure Electronic Transactions",NID_id_set,2,
	&(lvalues[3888]),0)
CONST_ENTRY_NID("set-ctype","content types",NID_set_ctype,3,&(lvalues[3890]),0)
CONST_ENTRY_NID("set-msgExt","message extensions",NID_set_msgExt,3,&(lvalues[3893]),0)
CONST_ENTRY_NID("set-attr","set-attr",NID_set_attr,3,&(lvalues[3896]),0)
CONST_ENTRY_NID("set-policy","set-policy",NID_set_policy,3,&(lvalues[3899]),0)
CONST_ENTRY_NID("set-certExt","certificate extensions",NID_set_certExt,3,
	&(lvalues[3902]),0)
CONST_ENTRY_NID("set-brand","set-brand",NID_set_brand,3,&(lvalues[3905]),0)
CONST_ENTRY_NID("setct-PANData","setct-PANData",NID_setct_PANData,4,&(lvalues[3908]),0)
CONST_ENTRY_NID("setct-PANToken","setct-PANToken",NID_setct_PANToken,4,
	&(lvalues[3912]),0)
CONST_ENTRY_NID("setct-PANOnly","setct-PANOnly",NID_setct_PANOnly,4,&(lvalues[3916]),0)
CONST_ENTRY_NID("setct-OIData","setct-OIData",NID_setct_OIData,4,&(lvalues[3920]),0)
CONST_ENTRY_NID("setct-PI","setct-PI",NID_setct_PI,4,&(lvalues[3924]),0)
CONST_ENTRY_NID("setct-PIData","setct-PIData",NID_setct_PIData,4,&(lvalues[3928]),0)
CONST_ENTRY_NID("setct-PIDataUnsigned","setct-PIDataUnsigned",
	NID_setct_PIDataUnsigned,4,&(lvalues[3932]),0)
CONST_ENTRY_NID("setct-HODInput","setct-HODInput",NID_setct_HODInput,4,
	&(lvalues[3936]),0)
CONST_ENTRY_NID("setct-AuthResBaggage","setct-AuthResBaggage",
	NID_setct_AuthResBaggage,4,&(lvalues[3940]),0)
CONST_ENTRY_NID("setct-AuthRevReqBaggage","setct-AuthRevReqBaggage",
	NID_setct_AuthRevReqBaggage,4,&(lvalues[3944]),0)
CONST_ENTRY_NID("setct-AuthRevResBaggage","setct-AuthRevResBaggage",
	NID_setct_AuthRevResBaggage,4,&(lvalues[3948]),0)
CONST_ENTRY_NID("setct-CapTokenSeq","setct-CapTokenSeq",NID_setct_CapTokenSeq,4,
	&(lvalues[3952]),0)
CONST_ENTRY_NID("setct-PInitResData","setct-PInitResData",NID_setct_PInitResData,4,
	&(lvalues[3956]),0)
CONST_ENTRY_NID("setct-PI-TBS","setct-PI-TBS",NID_setct_PI_TBS,4,&(lvalues[3960]),0)
CONST_ENTRY_NID("setct-PResData","setct-PResData",NID_setct_PResData,4,
	&(lvalues[3964]),0)
CONST_ENTRY_NID("setct-AuthReqTBS","setct-AuthReqTBS",NID_setct_AuthReqTBS,4,
	&(lvalues[3968]),0)
CONST_ENTRY_NID("setct-AuthResTBS","setct-AuthResTBS",NID_setct_AuthResTBS,4,
	&(lvalues[3972]),0)
CONST_ENTRY_NID("setct-AuthResTBSX","setct-AuthResTBSX",NID_setct_AuthResTBSX,4,
	&(lvalues[3976]),0)
CONST_ENTRY_NID("setct-AuthTokenTBS","setct-AuthTokenTBS",NID_setct_AuthTokenTBS,4,
	&(lvalues[3980]),0)
CONST_ENTRY_NID("setct-CapTokenData","setct-CapTokenData",NID_setct_CapTokenData,4,
	&(lvalues[3984]),0)
CONST_ENTRY_NID("setct-CapTokenTBS","setct-CapTokenTBS",NID_setct_CapTokenTBS,4,
	&(lvalues[3988]),0)
CONST_ENTRY_NID("setct-AcqCardCodeMsg","setct-AcqCardCodeMsg",
	NID_setct_AcqCardCodeMsg,4,&(lvalues[3992]),0)
CONST_ENTRY_NID("setct-AuthRevReqTBS","setct-AuthRevReqTBS",NID_setct_AuthRevReqTBS,
	4,&(lvalues[3996]),0)
CONST_ENTRY_NID("setct-AuthRevResData","setct-AuthRevResData",
	NID_setct_AuthRevResData,4,&(lvalues[4000]),0)
CONST_ENTRY_NID("setct-AuthRevResTBS","setct-AuthRevResTBS",NID_setct_AuthRevResTBS,
	4,&(lvalues[4004]),0)
CONST_ENTRY_NID("setct-CapReqTBS","setct-CapReqTBS",NID_setct_CapReqTBS,4,
	&(lvalues[4008]),0)
CONST_ENTRY_NID("setct-CapReqTBSX","setct-CapReqTBSX",NID_setct_CapReqTBSX,4,
	&(lvalues[4012]),0)
CONST_ENTRY_NID("setct-CapResData","setct-CapResData",NID_setct_CapResData,4,
	&(lvalues[4016]),0)
CONST_ENTRY_NID("setct-CapRevReqTBS","setct-CapRevReqTBS",NID_setct_CapRevReqTBS,4,
	&(lvalues[4020]),0)
CONST_ENTRY_NID("setct-CapRevReqTBSX","setct-CapRevReqTBSX",NID_setct_CapRevReqTBSX,
	4,&(lvalues[4024]),0)
CONST_ENTRY_NID("setct-CapRevResData","setct-CapRevResData",NID_setct_CapRevResData,
	4,&(lvalues[4028]),0)
CONST_ENTRY_NID("setct-CredReqTBS","setct-CredReqTBS",NID_setct_CredReqTBS,4,
	&(lvalues[4032]),0)
CONST_ENTRY_NID("setct-CredReqTBSX","setct-CredReqTBSX",NID_setct_CredReqTBSX,4,
	&(lvalues[4036]),0)
CONST_ENTRY_NID("setct-CredResData","setct-CredResData",NID_setct_CredResData,4,
	&(lvalues[4040]),0)
CONST_ENTRY_NID("setct-CredRevReqTBS","setct-CredRevReqTBS",NID_setct_CredRevReqTBS,
	4,&(lvalues[4044]),0)
CONST_ENTRY_NID("setct-CredRevReqTBSX","setct-CredRevReqTBSX",
	NID_setct_CredRevReqTBSX,4,&(lvalues[4048]),0)
CONST_ENTRY_NID("setct-CredRevResData","setct-CredRevResData",
	NID_setct_CredRevResData,4,&(lvalues[4052]),0)
CONST_ENTRY_NID("setct-PCertReqData","setct-PCertReqData",NID_setct_PCertReqData,4,
	&(lvalues[4056]),0)
CONST_ENTRY_NID("setct-PCertResTBS","setct-PCertResTBS",NID_setct_PCertResTBS,4,
	&(lvalues[4060]),0)
CONST_ENTRY_NID("setct-BatchAdminReqData","setct-BatchAdminReqData",
	NID_setct_BatchAdminReqData,4,&(lvalues[4064]),0)
CONST_ENTRY_NID("setct-BatchAdminResData","setct-BatchAdminResData",
	NID_setct_BatchAdminResData,4,&(lvalues[4068]),0)
CONST_ENTRY_NID("setct-CardCInitResTBS","setct-CardCInitResTBS",
	NID_setct_CardCInitResTBS,4,&(lvalues[4072]),0)
CONST_ENTRY_NID("setct-MeAqCInitResTBS","setct-MeAqCInitResTBS",
	NID_setct_MeAqCInitResTBS,4,&(lvalues[4076]),0)
CONST_ENTRY_NID("setct-RegFormResTBS","setct-RegFormResTBS",NID_setct_RegFormResTBS,
	4,&(lvalues[4080]),0)
CONST_ENTRY_NID("setct-CertReqData","setct-CertReqData",NID_setct_CertReqData,4,
	&(lvalues[4084]),0)
CONST_ENTRY_NID("setct-CertReqTBS","setct-CertReqTBS",NID_setct_CertReqTBS,4,
	&(lvalues[4088]),0)
CONST_ENTRY_NID("setct-CertResData","setct-CertResData",NID_setct_CertResData,4,
	&(lvalues[4092]),0)
CONST_ENTRY_NID("setct-CertInqReqTBS","setct-CertInqReqTBS",NID_setct_CertInqReqTBS,
	4,&(lvalues[4096]),0)
CONST_ENTRY_NID("setct-ErrorTBS","setct-ErrorTBS",NID_setct_ErrorTBS,4,
	&(lvalues[4100]),0)
CONST_ENTRY_NID("setct-PIDualSignedTBE","setct-PIDualSignedTBE",
	NID_setct_PIDualSignedTBE,4,&(lvalues[4104]),0)
CONST_ENTRY_NID("setct-PIUnsignedTBE","setct-PIUnsignedTBE",NID_setct_PIUnsignedTBE,
	4,&(lvalues[4108]),0)
CONST_ENTRY_NID("setct-AuthReqTBE","setct-AuthReqTBE",NID_setct_AuthReqTBE,4,
	&(lvalues[4112]),0)
CONST_ENTRY_NID("setct-AuthResTBE","setct-AuthResTBE",NID_setct_AuthResTBE,4,
	&(lvalues[4116]),0)
CONST_ENTRY_NID("setct-AuthResTBEX","setct-AuthResTBEX",NID_setct_AuthResTBEX,4,
	&(lvalues[4120]),0)
CONST_ENTRY_NID("setct-AuthTokenTBE","setct-AuthTokenTBE",NID_setct_AuthTokenTBE,4,
	&(lvalues[4124]),0)
CONST_ENTRY_NID("setct-CapTokenTBE","setct-CapTokenTBE",NID_setct_CapTokenTBE,4,
	&(lvalues[4128]),0)
CONST_ENTRY_NID("setct-CapTokenTBEX","setct-CapTokenTBEX",NID_setct_CapTokenTBEX,4,
	&(lvalues[4132]),0)
CONST_ENTRY_NID("setct-AcqCardCodeMsgTBE","setct-AcqCardCodeMsgTBE",
	NID_setct_AcqCardCodeMsgTBE,4,&(lvalues[4136]),0)
CONST_ENTRY_NID("setct-AuthRevReqTBE","setct-AuthRevReqTBE",NID_setct_AuthRevReqTBE,
	4,&(lvalues[4140]),0)
CONST_ENTRY_NID("setct-AuthRevResTBE","setct-AuthRevResTBE",NID_setct_AuthRevResTBE,
	4,&(lvalues[4144]),0)
CONST_ENTRY_NID("setct-AuthRevResTBEB","setct-AuthRevResTBEB",
	NID_setct_AuthRevResTBEB,4,&(lvalues[4148]),0)
CONST_ENTRY_NID("setct-CapReqTBE","setct-CapReqTBE",NID_setct_CapReqTBE,4,
	&(lvalues[4152]),0)
CONST_ENTRY_NID("setct-CapReqTBEX","setct-CapReqTBEX",NID_setct_CapReqTBEX,4,
	&(lvalues[4156]),0)
CONST_ENTRY_NID("setct-CapResTBE","setct-CapResTBE",NID_setct_CapResTBE,4,
	&(lvalues[4160]),0)
CONST_ENTRY_NID("setct-CapRevReqTBE","setct-CapRevReqTBE",NID_setct_CapRevReqTBE,4,
	&(lvalues[4164]),0)
CONST_ENTRY_NID("setct-CapRevReqTBEX","setct-CapRevReqTBEX",NID_setct_CapRevReqTBEX,
	4,&(lvalues[4168]),0)
CONST_ENTRY_NID("setct-CapRevResTBE","setct-CapRevResTBE",NID_setct_CapRevResTBE,4,
	&(lvalues[4172]),0)
CONST_ENTRY_NID("setct-CredReqTBE","setct-CredReqTBE",NID_setct_CredReqTBE,4,
	&(lvalues[4176]),0)
CONST_ENTRY_NID("setct-CredReqTBEX","setct-CredReqTBEX",NID_setct_CredReqTBEX,4,
	&(lvalues[4180]),0)
CONST_ENTRY_NID("setct-CredResTBE","setct-CredResTBE",NID_setct_CredResTBE,4,
	&(lvalues[4184]),0)
CONST_ENTRY_NID("setct-CredRevReqTBE","setct-CredRevReqTBE",NID_setct_CredRevReqTBE,
	4,&(lvalues[4188]),0)
CONST_ENTRY_NID("setct-CredRevReqTBEX","setct-CredRevReqTBEX",
	NID_setct_CredRevReqTBEX,4,&(lvalues[4192]),0)
CONST_ENTRY_NID("setct-CredRevResTBE","setct-CredRevResTBE",NID_setct_CredRevResTBE,
	4,&(lvalues[4196]),0)
CONST_ENTRY_NID("setct-BatchAdminReqTBE","setct-BatchAdminReqTBE",
	NID_setct_BatchAdminReqTBE,4,&(lvalues[4200]),0)
CONST_ENTRY_NID("setct-BatchAdminResTBE","setct-BatchAdminResTBE",
	NID_setct_BatchAdminResTBE,4,&(lvalues[4204]),0)
CONST_ENTRY_NID("setct-RegFormReqTBE","setct-RegFormReqTBE",NID_setct_RegFormReqTBE,
	4,&(lvalues[4208]),0)
CONST_ENTRY_NID("setct-CertReqTBE","setct-CertReqTBE",NID_setct_CertReqTBE,4,
	&(lvalues[4212]),0)
CONST_ENTRY_NID("setct-CertReqTBEX","setct-CertReqTBEX",NID_setct_CertReqTBEX,4,
	&(lvalues[4216]),0)
CONST_ENTRY_NID("setct-CertResTBE","setct-CertResTBE",NID_setct_CertResTBE,4,
	&(lvalues[4220]),0)
CONST_ENTRY_NID("setct-CRLNotificationTBS","setct-CRLNotificationTBS",
	NID_setct_CRLNotificationTBS,4,&(lvalues[4224]),0)
CONST_ENTRY_NID("setct-CRLNotificationResTBS","setct-CRLNotificationResTBS",
	NID_setct_CRLNotificationResTBS,4,&(lvalues[4228]),0)
CONST_ENTRY_NID("setct-BCIDistributionTBS","setct-BCIDistributionTBS",
	NID_setct_BCIDistributionTBS,4,&(lvalues[4232]),0)
CONST_ENTRY_NID("setext-genCrypt","generic cryptogram",NID_setext_genCrypt,4,
	&(lvalues[4236]),0)
CONST_ENTRY_NID("setext-miAuth","merchant initiated auth",NID_setext_miAuth,4,
	&(lvalues[4240]),0)
CONST_ENTRY_NID("setext-pinSecure","setext-pinSecure",NID_setext_pinSecure,4,
	&(lvalues[4244]),0)
CONST_ENTRY_NID("setext-pinAny","setext-pinAny",NID_setext_pinAny,4,&(lvalues[4248]),0)
CONST_ENTRY_NID("setext-track2","setext-track2",NID_setext_track2,4,&(lvalues[4252]),0)
CONST_ENTRY_NID("setext-cv","additional verification",NID_setext_cv,4,
	&(lvalues[4256]),0)
CONST_ENTRY_NID("set-policy-root","set-policy-root",NID_set_policy_root,4,
	&(lvalues[4260]),0)
CONST_ENTRY_NID("setCext-hashedRoot","setCext-hashedRoot",NID_setCext_hashedRoot,4,
	&(lvalues[4264]),0)
CONST_ENTRY_NID("setCext-certType","setCext-certType",NID_setCext_certType,4,
	&(lvalues[4268]),0)
CONST_ENTRY_NID("setCext-merchData","setCext-merchData",NID_setCext_merchData,4,
	&(lvalues[4272]),0)
CONST_ENTRY_NID("setCext-cCertRequired","setCext-cCertRequired",
	NID_setCext_cCertRequired,4,&(lvalues[4276]),0)
CONST_ENTRY_NID("setCext-tunneling","setCext-tunneling",NID_setCext_tunneling,4,
	&(lvalues[4280]),0)
CONST_ENTRY_NID("setCext-setExt","setCext-setExt",NID_setCext_setExt,4,
	&(lvalues[4284]),0)
CONST_ENTRY_NID("setCext-setQualf","setCext-setQualf",NID_setCext_setQualf,4,
	&(lvalues[4288]),0)
CONST_ENTRY_NID("setCext-PGWYcapabilities","setCext-PGWYcapabilities",
	NID_setCext_PGWYcapabilities,4,&(lvalues[4292]),0)
CONST_ENTRY_NID("setCext-TokenIdentifier","setCext-TokenIdentifier",
	NID_setCext_TokenIdentifier,4,&(lvalues[4296]),0)
CONST_ENTRY_NID("setCext-Track2Data","setCext-Track2Data",NID_setCext_Track2Data,4,
	&(lvalues[4300]),0)
CONST_ENTRY_NID("setCext-TokenType","setCext-TokenType",NID_setCext_TokenType,4,
	&(lvalues[4304]),0)
CONST_ENTRY_NID("setCext-IssuerCapabilities","setCext-IssuerCapabilities",
	NID_setCext_IssuerCapabilities,4,&(lvalues[4308]),0)
CONST_ENTRY_NID("setAttr-Cert","setAttr-Cert",NID_setAttr_Cert,4,&(lvalues[4312]),0)
CONST_ENTRY_NID("setAttr-PGWYcap","payment gateway capabilities",NID_setAttr_PGWYcap,
	4,&(lvalues[4316]),0)
CONST_ENTRY_NID("setAttr-TokenType","setAttr-TokenType",NID_setAttr_TokenType,4,
	&(lvalues[4320]),0)
CONST_ENTRY_NID("setAttr-IssCap","issuer capabilities",NID_setAttr_IssCap,4,
	&(lvalues[4324]),0)
CONST_ENTRY_NID("set-rootKeyThumb","set-rootKeyThumb",NID_set_rootKeyThumb,5,
	&(lvalues[4328]),0)
CONST_ENTRY_NID("set-addPolicy","set-addPolicy",NID_set_addPolicy,5,&(lvalues[4333]),0)
CONST_ENTRY_NID("setAttr-Token-EMV","setAttr-Token-EMV",NID_setAttr_Token_EMV,5,
	&(lvalues[4338]),0)
CONST_ENTRY_NID("setAttr-Token-B0Prime","setAttr-Token-B0Prime",
	NID_setAttr_Token_B0Prime,5,&(lvalues[4343]),0)
CONST_ENTRY_NID("setAttr-IssCap-CVM","setAttr-IssCap-CVM",NID_setAttr_IssCap_CVM,5,
	&(lvalues[4348]),0)
CONST_ENTRY_NID("setAttr-IssCap-T2","setAttr-IssCap-T2",NID_setAttr_IssCap_T2,5,
	&(lvalues[4353]),0)
CONST_ENTRY_NID("setAttr-IssCap-Sig","setAttr-IssCap-Sig",NID_setAttr_IssCap_Sig,5,
	&(lvalues[4358]),0)
CONST_ENTRY_NID("setAttr-GenCryptgrm","generate cryptogram",NID_setAttr_GenCryptgrm,
	6,&(lvalues[4363]),0)
CONST_ENTRY_NID("setAttr-T2Enc","encrypted track 2",NID_setAttr_T2Enc,6,
	&(lvalues[4369]),0)
CONST_ENTRY_NID("setAttr-T2cleartxt","cleartext track 2",NID_setAttr_T2cleartxt,6,
	&(lvalues[4375]),0)
CONST_ENTRY_NID("setAttr-TokICCsig","ICC or token signature",NID_setAttr_TokICCsig,6,
	&(lvalues[4381]),0)
CONST_ENTRY_NID("setAttr-SecDevSig","secure device signature",NID_setAttr_SecDevSig,
	6,&(lvalues[4387]),0)
CONST_ENTRY_NID("set-brand-IATA-ATA","set-brand-IATA-ATA",NID_set_brand_IATA_ATA,4,
	&(lvalues[4393]),0)
CONST_ENTRY_NID("set-brand-Diners","set-brand-Diners",NID_set_brand_Diners,4,
	&(lvalues[4397]),0)
CONST_ENTRY_NID("set-brand-AmericanExpress","set-brand-AmericanExpress",
	NID_set_brand_AmericanExpress,4,&(lvalues[4401]),0)
CONST_ENTRY_NID("set-brand-JCB","set-brand-JCB",NID_set_brand_JCB,4,&(lvalues[4405]),0)
CONST_ENTRY_NID("set-brand-Visa","set-brand-Visa",NID_set_brand_Visa,4,
	&(lvalues[4409]),0)
CONST_ENTRY_NID("set-brand-MasterCard","set-brand-MasterCard",
	NID_set_brand_MasterCard,4,&(lvalues[4413]),0)
CONST_ENTRY_NID("set-brand-Novus","set-brand-Novus",NID_set_brand_Novus,5,
	&(lvalues[4417]),0)
CONST_ENTRY_NID("DES-CDMF","des-cdmf",NID_des_cdmf,8,&(lvalues[4422]),0)
CONST_ENTRY_NID("rsaOAEPEncryptionSET","rsaOAEPEncryptionSET",
	NID_rsaOAEPEncryptionSET,9,&(lvalues[4430]),0)
CONST_ENTRY_NID("ITU-T","itu-t",NID_itu_t,1,&(lvalues[4439]),0)
CONST_ENTRY_NID("JOINT-ISO-ITU-T","joint-iso-itu-t",NID_joint_iso_itu_t,1,
	&(lvalues[4440]),0)
CONST_ENTRY_NID("international-organizations","International Organizations",
	NID_international_organizations,1,&(lvalues[4441]),0)
CONST_ENTRY_NID("msSmartcardLogin","Microsoft Smartcardlogin",NID_ms_smartcard_login,
	10,&(lvalues[4442]),0)
CONST_ENTRY_NID("msUPN","Microsoft Universal Principal Name",NID_ms_upn,10,
	&(lvalues[4452]),0)
CONST_ENTRY_NID("AES-128-CFB1","aes-128-cfb1",NID_aes_128_cfb1,0,NULL,0)
CONST_ENTRY_NID("AES-192-CFB1","aes-192-cfb1",NID_aes_192_cfb1,0,NULL,0)
CONST_ENTRY_NID("AES-256-CFB1","aes-256-cfb1",NID_aes_256_cfb1,0,NULL,0)
CONST_ENTRY_NID("AES-128-CFB8","aes-128-cfb8",NID_aes_128_cfb8,0,NULL,0)
CONST_ENTRY_NID("AES-192-CFB8","aes-192-cfb8",NID_aes_192_cfb8,0,NULL,0)
CONST_ENTRY_NID("AES-256-CFB8","aes-256-cfb8",NID_aes_256_cfb8,0,NULL,0)
CONST_ENTRY_NID("DES-CFB1","des-cfb1",NID_des_cfb1,0,NULL,0)
CONST_ENTRY_NID("DES-CFB8","des-cfb8",NID_des_cfb8,0,NULL,0)
CONST_ENTRY_NID("DES-EDE3-CFB1","des-ede3-cfb1",NID_des_ede3_cfb1,0,NULL,0)
CONST_ENTRY_NID("DES-EDE3-CFB8","des-ede3-cfb8",NID_des_ede3_cfb8,0,NULL,0)
CONST_ENTRY_NID("street","streetAddress",NID_streetAddress,3,&(lvalues[4462]),0)
CONST_ENTRY_NID("postalCode","postalCode",NID_postalCode,3,&(lvalues[4465]),0)
CONST_ENTRY_NID("id-ppl","id-ppl",NID_id_ppl,7,&(lvalues[4468]),0)
CONST_ENTRY_NID("proxyCertInfo","Proxy Certificate Information",NID_proxyCertInfo,8,
	&(lvalues[4475]),0)
CONST_ENTRY_NID("id-ppl-anyLanguage","Any language",NID_id_ppl_anyLanguage,8,
	&(lvalues[4483]),0)
CONST_ENTRY_NID("id-ppl-inheritAll","Inherit all",NID_id_ppl_inheritAll,8,
	&(lvalues[4491]),0)
CONST_ENTRY_NID("nameConstraints","X509v3 Name Constraints",NID_name_constraints,3,
	&(lvalues[4499]),0)
CONST_ENTRY_NID("id-ppl-independent","Independent",NID_Independent,8,&(lvalues[4502]),0)
CONST_ENTRY_NID("RSA-SHA256","sha256WithRSAEncryption",NID_sha256WithRSAEncryption,9,
	&(lvalues[4510]),0)
CONST_ENTRY_NID("RSA-SHA384","sha384WithRSAEncryption",NID_sha384WithRSAEncryption,9,
	&(lvalues[4519]),0)
CONST_ENTRY_NID("RSA-SHA512","sha512WithRSAEncryption",NID_sha512WithRSAEncryption,9,
	&(lvalues[4528]),0)
CONST_ENTRY_NID("RSA-SHA224","sha224WithRSAEncryption",NID_sha224WithRSAEncryption,9,
	&(lvalues[4537]),0)
CONST_ENTRY_NID("SHA256","sha256",NID_sha256,9,&(lvalues[4546]),0)
CONST_ENTRY_NID("SHA384","sha384",NID_sha384,9,&(lvalues[4555]),0)
CONST_ENTRY_NID("SHA512","sha512",NID_sha512,9,&(lvalues[4564]),0)
CONST_ENTRY_NID("SHA224","sha224",NID_sha224,9,&(lvalues[4573]),0)
CONST_ENTRY_NID("identified-organization","identified-organization",
	NID_identified_organization,1,&(lvalues[4582]),0)
CONST_ENTRY_NID("certicom-arc","certicom-arc",NID_certicom_arc,3,&(lvalues[4583]),0)
CONST_ENTRY_NID("wap","wap",NID_wap,2,&(lvalues[4586]),0)
CONST_ENTRY_NID("wap-wsg","wap-wsg",NID_wap_wsg,3,&(lvalues[4588]),0)
CONST_ENTRY_NID("id-characteristic-two-basis","id-characteristic-two-basis",
	NID_X9_62_id_characteristic_two_basis,8,&(lvalues[4591]),0)
CONST_ENTRY_NID("onBasis","onBasis",NID_X9_62_onBasis,9,&(lvalues[4599]),0)
CONST_ENTRY_NID("tpBasis","tpBasis",NID_X9_62_tpBasis,9,&(lvalues[4608]),0)
CONST_ENTRY_NID("ppBasis","ppBasis",NID_X9_62_ppBasis,9,&(lvalues[4617]),0)
CONST_ENTRY_NID("c2pnb163v1","c2pnb163v1",NID_X9_62_c2pnb163v1,8,&(lvalues[4626]),0)
CONST_ENTRY_NID("c2pnb163v2","c2pnb163v2",NID_X9_62_c2pnb163v2,8,&(lvalues[4634]),0)
CONST_ENTRY_NID("c2pnb163v3","c2pnb163v3",NID_X9_62_c2pnb163v3,8,&(lvalues[4642]),0)
CONST_ENTRY_NID("c2pnb176v1","c2pnb176v1",NID_X9_62_c2pnb176v1,8,&(lvalues[4650]),0)
CONST_ENTRY_NID("c2tnb191v1","c2tnb191v1",NID_X9_62_c2tnb191v1,8,&(lvalues[4658]),0)
CONST_ENTRY_NID("c2tnb191v2","c2tnb191v2",NID_X9_62_c2tnb191v2,8,&(lvalues[4666]),0)
CONST_ENTRY_NID("c2tnb191v3","c2tnb191v3",NID_X9_62_c2tnb191v3,8,&(lvalues[4674]),0)
CONST_ENTRY_NID("c2onb191v4","c2onb191v4",NID_X9_62_c2onb191v4,8,&(lvalues[4682]),0)
CONST_ENTRY_NID("c2onb191v5","c2onb191v5",NID_X9_62_c2onb191v5,8,&(lvalues[4690]),0)
CONST_ENTRY_NID("c2pnb208w1","c2pnb208w1",NID_X9_62_c2pnb208w1,8,&(lvalues[4698]),0)
CONST_ENTRY_NID("c2tnb239v1","c2tnb239v1",NID_X9_62_c2tnb239v1,8,&(lvalues[4706]),0)
CONST_ENTRY_NID("c2tnb239v2","c2tnb239v2",NID_X9_62_c2tnb239v2,8,&(lvalues[4714]),0)
CONST_ENTRY_NID("c2tnb239v3","c2tnb239v3",NID_X9_62_c2tnb239v3,8,&(lvalues[4722]),0)
CONST_ENTRY_NID("c2onb239v4","c2onb239v4",NID_X9_62_c2onb239v4,8,&(lvalues[4730]),0)
CONST_ENTRY_NID("c2onb239v5","c2onb239v5",NID_X9_62_c2onb239v5,8,&(lvalues[4738]),0)
CONST_ENTRY_NID("c2pnb272w1","c2pnb272w1",NID_X9_62_c2pnb272w1,8,&(lvalues[4746]),0)
CONST_ENTRY_NID("c2pnb304w1","c2pnb304w1",NID_X9_62_c2pnb304w1,8,&(lvalues[4754]),0)
CONST_ENTRY_NID("c2tnb359v1","c2tnb359v1",NID_X9_62_c2tnb359v1,8,&(lvalues[4762]),0)
CONST_ENTRY_NID("c2pnb368w1","c2pnb368w1",NID_X9_62_c2pnb368w1,8,&(lvalues[4770]),0)
CONST_ENTRY_NID("c2tnb431r1","c2tnb431r1",NID_X9_62_c2tnb431r1,8,&(lvalues[4778]),0)
CONST_ENTRY_NID("secp112r1","secp112r1",NID_secp112r1,5,&(lvalues[4786]),0)
CONST_ENTRY_NID("secp112r2","secp112r2",NID_secp112r2,5,&(lvalues[4791]),0)
CONST_ENTRY_NID("secp128r1","secp128r1",NID_secp128r1,5,&(lvalues[4796]),0)
CONST_ENTRY_NID("secp128r2","secp128r2",NID_secp128r2,5,&(lvalues[4801]),0)
CONST_ENTRY_NID("secp160k1","secp160k1",NID_secp160k1,5,&(lvalues[4806]),0)
CONST_ENTRY_NID("secp160r1","secp160r1",NID_secp160r1,5,&(lvalues[4811]),0)
CONST_ENTRY_NID("secp160r2","secp160r2",NID_secp160r2,5,&(lvalues[4816]),0)
CONST_ENTRY_NID("secp192k1","secp192k1",NID_secp192k1,5,&(lvalues[4821]),0)
CONST_ENTRY_NID("secp224k1","secp224k1",NID_secp224k1,5,&(lvalues[4826]),0)
CONST_ENTRY_NID("secp224r1","secp224r1",NID_secp224r1,5,&(lvalues[4831]),0)
CONST_ENTRY_NID("secp256k1","secp256k1",NID_secp256k1,5,&(lvalues[4836]),0)
CONST_ENTRY_NID("secp384r1","secp384r1",NID_secp384r1,5,&(lvalues[4841]),0)
CONST_ENTRY_NID("secp521r1","secp521r1",NID_secp521r1,5,&(lvalues[4846]),0)
CONST_ENTRY_NID("sect113r1","sect113r1",NID_sect113r1,5,&(lvalues[4851]),0)
CONST_ENTRY_NID("sect113r2","sect113r2",NID_sect113r2,5,&(lvalues[4856]),0)
CONST_ENTRY_NID("sect131r1","sect131r1",NID_sect131r1,5,&(lvalues[4861]),0)
CONST_ENTRY_NID("sect131r2","sect131r2",NID_sect131r2,5,&(lvalues[4866]),0)
CONST_ENTRY_NID("sect163k1","sect163k1",NID_sect163k1,5,&(lvalues[4871]),0)
CONST_ENTRY_NID("sect163r1","sect163r1",NID_sect163r1,5,&(lvalues[4876]),0)
CONST_ENTRY_NID("sect163r2","sect163r2",NID_sect163r2,5,&(lvalues[4881]),0)
CONST_ENTRY_NID("sect193r1","sect193r1",NID_sect193r1,5,&(lvalues[4886]),0)
CONST_ENTRY_NID("sect193r2","sect193r2",NID_sect193r2,5,&(lvalues[4891]),0)
CONST_ENTRY_NID("sect233k1","sect233k1",NID_sect233k1,5,&(lvalues[4896]),0)
CONST_ENTRY_NID("sect233r1","sect233r1",NID_sect233r1,5,&(lvalues[4901]),0)
CONST_ENTRY_NID("sect239k1","sect239k1",NID_sect239k1,5,&(lvalues[4906]),0)
CONST_ENTRY_NID("sect283k1","sect283k1",NID_sect283k1,5,&(lvalues[4911]),0)
CONST_ENTRY_NID("sect283r1","sect283r1",NID_sect283r1,5,&(lvalues[4916]),0)
CONST_ENTRY_NID("sect409k1","sect409k1",NID_sect409k1,5,&(lvalues[4921]),0)
CONST_ENTRY_NID("sect409r1","sect409r1",NID_sect409r1,5,&(lvalues[4926]),0)
CONST_ENTRY_NID("sect571k1","sect571k1",NID_sect571k1,5,&(lvalues[4931]),0)
CONST_ENTRY_NID("sect571r1","sect571r1",NID_sect571r1,5,&(lvalues[4936]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls1","wap-wsg-idm-ecid-wtls1",
	NID_wap_wsg_idm_ecid_wtls1,5,&(lvalues[4941]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls3","wap-wsg-idm-ecid-wtls3",
	NID_wap_wsg_idm_ecid_wtls3,5,&(lvalues[4946]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls4","wap-wsg-idm-ecid-wtls4",
	NID_wap_wsg_idm_ecid_wtls4,5,&(lvalues[4951]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls5","wap-wsg-idm-ecid-wtls5",
	NID_wap_wsg_idm_ecid_wtls5,5,&(lvalues[4956]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls6","wap-wsg-idm-ecid-wtls6",
	NID_wap_wsg_idm_ecid_wtls6,5,&(lvalues[4961]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls7","wap-wsg-idm-ecid-wtls7",
	NID_wap_wsg_idm_ecid_wtls7,5,&(lvalues[4966]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls8","wap-wsg-idm-ecid-wtls8",
	NID_wap_wsg_idm_ecid_wtls8,5,&(lvalues[4971]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls9","wap-wsg-idm-ecid-wtls9",
	NID_wap_wsg_idm_ecid_wtls9,5,&(lvalues[4976]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls10","wap-wsg-idm-ecid-wtls10",
	NID_wap_wsg_idm_ecid_wtls10,5,&(lvalues[4981]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls11","wap-wsg-idm-ecid-wtls11",
	NID_wap_wsg_idm_ecid_wtls11,5,&(lvalues[4986]),0)
CONST_ENTRY_NID("wap-wsg-idm-ecid-wtls12","wap-wsg-idm-ecid-wtls12",
	NID_wap_wsg_idm_ecid_wtls12,5,&(lvalues[4991]),0)
CONST_ENTRY_NID("anyPolicy","X509v3 Any Policy",NID_any_policy,4,&(lvalues[4996]),0)
CONST_ENTRY_NID("policyMappings","X509v3 Policy Mappings",NID_policy_mappings,3,
	&(lvalues[5000]),0)
CONST_ENTRY_NID("inhibitAnyPolicy","X509v3 Inhibit Any Policy",
	NID_inhibit_any_policy,3,&(lvalues[5003]),0)
CONST_ENTRY_NID("Oakley-EC2N-3","ipsec3",NID_ipsec3,0,NULL,0)
CONST_ENTRY_NID("Oakley-EC2N-4","ipsec4",NID_ipsec4,0,NULL,0)
CONST_ENTRY_NID("CAMELLIA-128-CBC","camellia-128-cbc",NID_camellia_128_cbc,11,
	&(lvalues[5006]),0)
CONST_ENTRY_NID("CAMELLIA-192-CBC","camellia-192-cbc",NID_camellia_192_cbc,11,
	&(lvalues[5017]),0)
CONST_ENTRY_NID("CAMELLIA-256-CBC","camellia-256-cbc",NID_camellia_256_cbc,11,
	&(lvalues[5028]),0)
CONST_ENTRY_NID("CAMELLIA-128-ECB","camellia-128-ecb",NID_camellia_128_ecb,8,
	&(lvalues[5039]),0)
CONST_ENTRY_NID("CAMELLIA-192-ECB","camellia-192-ecb",NID_camellia_192_ecb,8,
	&(lvalues[5047]),0)
CONST_ENTRY_NID("CAMELLIA-256-ECB","camellia-256-ecb",NID_camellia_256_ecb,8,
	&(lvalues[5055]),0)
CONST_ENTRY_NID("CAMELLIA-128-CFB","camellia-128-cfb",NID_camellia_128_cfb128,8,
	&(lvalues[5063]),0)
CONST_ENTRY_NID("CAMELLIA-192-CFB","camellia-192-cfb",NID_camellia_192_cfb128,8,
	&(lvalues[5071]),0)
CONST_ENTRY_NID("CAMELLIA-256-CFB","camellia-256-cfb",NID_camellia_256_cfb128,8,
	&(lvalues[5079]),0)
CONST_ENTRY_NID("CAMELLIA-128-CFB1","camellia-128-cfb1",NID_camellia_128_cfb1,0,NULL,0)
CONST_ENTRY_NID("CAMELLIA-192-CFB1","camellia-192-cfb1",NID_camellia_192_cfb1,0,NULL,0)
CONST_ENTRY_NID("CAMELLIA-256-CFB1","camellia-256-cfb1",NID_camellia_256_cfb1,0,NULL,0)
CONST_ENTRY_NID("CAMELLIA-128-CFB8","camellia-128-cfb8",NID_camellia_128_cfb8,0,NULL,0)
CONST_ENTRY_NID("CAMELLIA-192-CFB8","camellia-192-cfb8",NID_camellia_192_cfb8,0,NULL,0)
CONST_ENTRY_NID("CAMELLIA-256-CFB8","camellia-256-cfb8",NID_camellia_256_cfb8,0,NULL,0)
CONST_ENTRY_NID("CAMELLIA-128-OFB","camellia-128-ofb",NID_camellia_128_ofb128,8,
	&(lvalues[5087]),0)
CONST_ENTRY_NID("CAMELLIA-192-OFB","camellia-192-ofb",NID_camellia_192_ofb128,8,
	&(lvalues[5095]),0)
CONST_ENTRY_NID("CAMELLIA-256-OFB","camellia-256-ofb",NID_camellia_256_ofb128,8,
	&(lvalues[5103]),0)
CONST_ENTRY_NID("subjectDirectoryAttributes","X509v3 Subject Directory Attributes",
	NID_subject_directory_attributes,3,&(lvalues[5111]),0)
CONST_ENTRY_NID("issuingDistributionPoint","X509v3 Issuing Distrubution Point",
	NID_issuing_distribution_point,3,&(lvalues[5114]),0)
CONST_ENTRY_NID("certificateIssuer","X509v3 Certificate Issuer",
	NID_certificate_issuer,3,&(lvalues[5117]),0)
CONST_ENTRY_NID(NULL,NULL,NID_undef,0,NULL,0)
CONST_ENTRY_NID("KISA","kisa",NID_kisa,6,&(lvalues[5120]),0)
CONST_ENTRY_NID(NULL,NULL,NID_undef,0,NULL,0)
CONST_ENTRY_NID(NULL,NULL,NID_undef,0,NULL,0)
CONST_ENTRY_NID("SEED-ECB","seed-ecb",NID_seed_ecb,8,&(lvalues[5126]),0)
CONST_ENTRY_NID("SEED-CBC","seed-cbc",NID_seed_cbc,8,&(lvalues[5134]),0)
CONST_ENTRY_NID("SEED-OFB","seed-ofb",NID_seed_ofb128,8,&(lvalues[5142]),0)
CONST_ENTRY_NID("SEED-CFB","seed-cfb",NID_seed_cfb128,8,&(lvalues[5150]),0)
CONST_ENTRY_NID("HMAC-MD5","hmac-md5",NID_hmac_md5,8,&(lvalues[5158]),0)
CONST_ENTRY_NID("HMAC-SHA1","hmac-sha1",NID_hmac_sha1,8,&(lvalues[5166]),0)
CONST_ENTRY_NID("id-PasswordBasedMAC","password based MAC",NID_id_PasswordBasedMAC,9,
	&(lvalues[5174]),0)
CONST_ENTRY_NID("id-DHBasedMac","Diffie-Hellman based MAC",NID_id_DHBasedMac,9,
	&(lvalues[5183]),0)
CONST_ENTRY_NID("id-it-suppLangTags","id-it-suppLangTags",NID_id_it_suppLangTags,8,
	&(lvalues[5192]),0)
CONST_ENTRY_NID("caRepository","CA Repository",NID_caRepository,8,&(lvalues[5200]),0)
CONST_ENTRY_NID("id-smime-ct-compressedData","id-smime-ct-compressedData",
	NID_id_smime_ct_compressedData,11,&(lvalues[5208]),0)
CONST_ENTRY_NID("id-ct-asciiTextWithCRLF","id-ct-asciiTextWithCRLF",
	NID_id_ct_asciiTextWithCRLF,11,&(lvalues[5219]),0)
CONST_ENTRY_NID("id-aes128-wrap","id-aes128-wrap",NID_id_aes128_wrap,9,
	&(lvalues[5230]),0)
CONST_ENTRY_NID("id-aes192-wrap","id-aes192-wrap",NID_id_aes192_wrap,9,
	&(lvalues[5239]),0)
CONST_ENTRY_NID("id-aes256-wrap","id-aes256-wrap",NID_id_aes256_wrap,9,
	&(lvalues[5248]),0)
CONST_ENTRY_NID("ecdsa-with-Recommended","ecdsa-with-Recommended",
	NID_ecdsa_with_Recommended,7,&(lvalues[5257]),0)
CONST_ENTRY_NID("ecdsa-with-Specified","ecdsa-with-Specified",
	NID_ecdsa_with_Specified,7,&(lvalues[5264]),0)
CONST_ENTRY_NID("ecdsa-with-SHA224","ecdsa-with-SHA224",NID_ecdsa_with_SHA224,8,
	&(lvalues[5271]),0)
CONST_ENTRY_NID("ecdsa-with-SHA256","ecdsa-with-SHA256",NID_ecdsa_with_SHA256,8,
	&(lvalues[5279]),0)
CONST_ENTRY_NID("ecdsa-with-SHA384","ecdsa-with-SHA384",NID_ecdsa_with_SHA384,8,
	&(lvalues[5287]),0)
CONST_ENTRY_NID("ecdsa-with-SHA512","ecdsa-with-SHA512",NID_ecdsa_with_SHA512,8,
	&(lvalues[5295]),0)
CONST_ENTRY_NID("hmacWithMD5","hmacWithMD5",NID_hmacWithMD5,8,&(lvalues[5303]),0)
CONST_ENTRY_NID("hmacWithSHA224","hmacWithSHA224",NID_hmacWithSHA224,8,
	&(lvalues[5311]),0)
CONST_ENTRY_NID("hmacWithSHA256","hmacWithSHA256",NID_hmacWithSHA256,8,
	&(lvalues[5319]),0)
CONST_ENTRY_NID("hmacWithSHA384","hmacWithSHA384",NID_hmacWithSHA384,8,
	&(lvalues[5327]),0)
CONST_ENTRY_NID("hmacWithSHA512","hmacWithSHA512",NID_hmacWithSHA512,8,
	&(lvalues[5335]),0)
CONST_ENTRY_NID("dsa_with_SHA224","dsa_with_SHA224",NID_dsa_with_SHA224,9,
	&(lvalues[5343]),0)
CONST_ENTRY_NID("dsa_with_SHA256","dsa_with_SHA256",NID_dsa_with_SHA256,9,
	&(lvalues[5352]),0)
CONST_ENTRY_NID("whirlpool","whirlpool",NID_whirlpool,6,&(lvalues[5361]),0)
CONST_ENTRY_NID("cryptopro","cryptopro",NID_cryptopro,5,&(lvalues[5367]),0)
CONST_ENTRY_NID("cryptocom","cryptocom",NID_cryptocom,5,&(lvalues[5372]),0)
CONST_ENTRY_NID("id-GostR3411-94-with-GostR3410-2001",
	"GOST R 34.11-94 with GOST R 34.10-2001",NID_id_GostR3411_94_with_GostR3410_2001,6,
	&(lvalues[5377]),0)
CONST_ENTRY_NID("id-GostR3411-94-with-GostR3410-94",
	"GOST R 34.11-94 with GOST R 34.10-94",NID_id_GostR3411_94_with_GostR3410_94,6,
	&(lvalues[5383]),0)
CONST_ENTRY_NID("md_gost94","GOST R 34.11-94",NID_id_GostR3411_94,6,&(lvalues[5389]),0)
CONST_ENTRY_NID("id-HMACGostR3411-94","HMAC GOST 34.11-94",NID_id_HMACGostR3411_94,6,
	&(lvalues[5395]),0)
CONST_ENTRY_NID("gost2001","GOST R 34.10-2001",NID_id_GostR3410_2001,6,
	&(lvalues[5401]),0)
CONST_ENTRY_NID("gost94","GOST R 34.10-94",NID_id_GostR3410_94,6,&(lvalues[5407]),0)
CONST_ENTRY_NID("gost89","GOST 28147-89",NID_id_Gost28147_89,6,&(lvalues[5413]),0)
CONST_ENTRY_NID("gost89-cnt","gost89-cnt",NID_gost89_cnt,0,NULL,0)
CONST_ENTRY_NID("gost-mac","GOST 28147-89 MAC",NID_id_Gost28147_89_MAC,6,
	&(lvalues[5419]),0)
CONST_ENTRY_NID("prf-gostr3411-94","GOST R 34.11-94 PRF",NID_id_GostR3411_94_prf,6,
	&(lvalues[5425]),0)
CONST_ENTRY_NID("id-GostR3410-2001DH","GOST R 34.10-2001 DH",NID_id_GostR3410_2001DH,
	6,&(lvalues[5431]),0)
CONST_ENTRY_NID("id-GostR3410-94DH","GOST R 34.10-94 DH",NID_id_GostR3410_94DH,6,
	&(lvalues[5437]),0)
CONST_ENTRY_NID("id-Gost28147-89-CryptoPro-KeyMeshing",
	"id-Gost28147-89-CryptoPro-KeyMeshing",NID_id_Gost28147_89_CryptoPro_KeyMeshing,7,
	&(lvalues[5443]),0)
CONST_ENTRY_NID("id-Gost28147-89-None-KeyMeshing","id-Gost28147-89-None-KeyMeshing",
	NID_id_Gost28147_89_None_KeyMeshing,7,&(lvalues[5450]),0)
CONST_ENTRY_NID("id-GostR3411-94-TestParamSet","id-GostR3411-94-TestParamSet",
	NID_id_GostR3411_94_TestParamSet,7,&(lvalues[5457]),0)
CONST_ENTRY_NID("id-GostR3411-94-CryptoProParamSet",
	"id-GostR3411-94-CryptoProParamSet",NID_id_GostR3411_94_CryptoProParamSet,7,
	&(lvalues[5464]),0)
CONST_ENTRY_NID("id-Gost28147-89-TestParamSet","id-Gost28147-89-TestParamSet",
	NID_id_Gost28147_89_TestParamSet,7,&(lvalues[5471]),0)
CONST_ENTRY_NID("id-Gost28147-89-CryptoPro-A-ParamSet",
	"id-Gost28147-89-CryptoPro-A-ParamSet",NID_id_Gost28147_89_CryptoPro_A_ParamSet,7,
	&(lvalues[5478]),0)
CONST_ENTRY_NID("id-Gost28147-89-CryptoPro-B-ParamSet",
	"id-Gost28147-89-CryptoPro-B-ParamSet",NID_id_Gost28147_89_CryptoPro_B_ParamSet,7,
	&(lvalues[5485]),0)
CONST_ENTRY_NID("id-Gost28147-89-CryptoPro-C-ParamSet",
	"id-Gost28147-89-CryptoPro-C-ParamSet",NID_id_Gost28147_89_CryptoPro_C_ParamSet,7,
	&(lvalues[5492]),0)
CONST_ENTRY_NID("id-Gost28147-89-CryptoPro-D-ParamSet",
	"id-Gost28147-89-CryptoPro-D-ParamSet",NID_id_Gost28147_89_CryptoPro_D_ParamSet,7,
	&(lvalues[5499]),0)
CONST_ENTRY_NID("id-Gost28147-89-CryptoPro-Oscar-1-1-ParamSet",
	"id-Gost28147-89-CryptoPro-Oscar-1-1-ParamSet",
	NID_id_Gost28147_89_CryptoPro_Oscar_1_1_ParamSet,7,&(lvalues[5506]),0)
CONST_ENTRY_NID("id-Gost28147-89-CryptoPro-Oscar-1-0-ParamSet",
	"id-Gost28147-89-CryptoPro-Oscar-1-0-ParamSet",
	NID_id_Gost28147_89_CryptoPro_Oscar_1_0_ParamSet,7,&(lvalues[5513]),0)
CONST_ENTRY_NID("id-Gost28147-89-CryptoPro-RIC-1-ParamSet",
	"id-Gost28147-89-CryptoPro-RIC-1-ParamSet",
	NID_id_Gost28147_89_CryptoPro_RIC_1_ParamSet,7,&(lvalues[5520]),0)
CONST_ENTRY_NID("id-GostR3410-94-TestParamSet","id-GostR3410-94-TestParamSet",
	NID_id_GostR3410_94_TestParamSet,7,&(lvalues[5527]),0)
CONST_ENTRY_NID("id-GostR3410-94-CryptoPro-A-ParamSet",
	"id-GostR3410-94-CryptoPro-A-ParamSet",NID_id_GostR3410_94_CryptoPro_A_ParamSet,7,
	&(lvalues[5534]),0)
CONST_ENTRY_NID("id-GostR3410-94-CryptoPro-B-ParamSet",
	"id-GostR3410-94-CryptoPro-B-ParamSet",NID_id_GostR3410_94_CryptoPro_B_ParamSet,7,
	&(lvalues[5541]),0)
CONST_ENTRY_NID("id-GostR3410-94-CryptoPro-C-ParamSet",
	"id-GostR3410-94-CryptoPro-C-ParamSet",NID_id_GostR3410_94_CryptoPro_C_ParamSet,7,
	&(lvalues[5548]),0)
CONST_ENTRY_NID("id-GostR3410-94-CryptoPro-D-ParamSet",
	"id-GostR3410-94-CryptoPro-D-ParamSet",NID_id_GostR3410_94_CryptoPro_D_ParamSet,7,
	&(lvalues[5555]),0)
CONST_ENTRY_NID("id-GostR3410-94-CryptoPro-XchA-ParamSet",
	"id-GostR3410-94-CryptoPro-XchA-ParamSet",
	NID_id_GostR3410_94_CryptoPro_XchA_ParamSet,7,&(lvalues[5562]),0)
CONST_ENTRY_NID("id-GostR3410-94-CryptoPro-XchB-ParamSet",
	"id-GostR3410-94-CryptoPro-XchB-ParamSet",
	NID_id_GostR3410_94_CryptoPro_XchB_ParamSet,7,&(lvalues[5569]),0)
CONST_ENTRY_NID("id-GostR3410-94-CryptoPro-XchC-ParamSet",
	"id-GostR3410-94-CryptoPro-XchC-ParamSet",
	NID_id_GostR3410_94_CryptoPro_XchC_ParamSet,7,&(lvalues[5576]),0)
CONST_ENTRY_NID("id-GostR3410-2001-TestParamSet","id-GostR3410-2001-TestParamSet",
	NID_id_GostR3410_2001_TestParamSet,7,&(lvalues[5583]),0)
CONST_ENTRY_NID("id-GostR3410-2001-CryptoPro-A-ParamSet",
	"id-GostR3410-2001-CryptoPro-A-ParamSet",NID_id_GostR3410_2001_CryptoPro_A_ParamSet,
	7,&(lvalues[5590]),0)
CONST_ENTRY_NID("id-GostR3410-2001-CryptoPro-B-ParamSet",
	"id-GostR3410-2001-CryptoPro-B-ParamSet",NID_id_GostR3410_2001_CryptoPro_B_ParamSet,
	7,&(lvalues[5597]),0)
CONST_ENTRY_NID("id-GostR3410-2001-CryptoPro-C-ParamSet",
	"id-GostR3410-2001-CryptoPro-C-ParamSet",NID_id_GostR3410_2001_CryptoPro_C_ParamSet,
	7,&(lvalues[5604]),0)
CONST_ENTRY_NID("id-GostR3410-2001-CryptoPro-XchA-ParamSet",
	"id-GostR3410-2001-CryptoPro-XchA-ParamSet",
	NID_id_GostR3410_2001_CryptoPro_XchA_ParamSet,7,&(lvalues[5611]),0)
CONST_ENTRY_NID("id-GostR3410-2001-CryptoPro-XchB-ParamSet",
	"id-GostR3410-2001-CryptoPro-XchB-ParamSet",
	NID_id_GostR3410_2001_CryptoPro_XchB_ParamSet,7,&(lvalues[5618]),0)
CONST_ENTRY_NID("id-GostR3410-94-a","id-GostR3410-94-a",NID_id_GostR3410_94_a,7,
	&(lvalues[5625]),0)
CONST_ENTRY_NID("id-GostR3410-94-aBis","id-GostR3410-94-aBis",
	NID_id_GostR3410_94_aBis,7,&(lvalues[5632]),0)
CONST_ENTRY_NID("id-GostR3410-94-b","id-GostR3410-94-b",NID_id_GostR3410_94_b,7,
	&(lvalues[5639]),0)
CONST_ENTRY_NID("id-GostR3410-94-bBis","id-GostR3410-94-bBis",
	NID_id_GostR3410_94_bBis,7,&(lvalues[5646]),0)
CONST_ENTRY_NID("id-Gost28147-89-cc","GOST 28147-89 Cryptocom ParamSet",
	NID_id_Gost28147_89_cc,8,&(lvalues[5653]),0)
CONST_ENTRY_NID("gost94cc","GOST 34.10-94 Cryptocom",NID_id_GostR3410_94_cc,8,
	&(lvalues[5661]),0)
CONST_ENTRY_NID("gost2001cc","GOST 34.10-2001 Cryptocom",NID_id_GostR3410_2001_cc,8,
	&(lvalues[5669]),0)
CONST_ENTRY_NID("id-GostR3411-94-with-GostR3410-94-cc",
	"GOST R 34.11-94 with GOST R 34.10-94 Cryptocom",
	NID_id_GostR3411_94_with_GostR3410_94_cc,8,&(lvalues[5677]),0)
CONST_ENTRY_NID("id-GostR3411-94-with-GostR3410-2001-cc",
	"GOST R 34.11-94 with GOST R 34.10-2001 Cryptocom",
	NID_id_GostR3411_94_with_GostR3410_2001_cc,8,&(lvalues[5685]),0)
CONST_ENTRY_NID("id-GostR3410-2001-ParamSet-cc",
	"GOST R 3410-2001 Parameter Set Cryptocom",NID_id_GostR3410_2001_ParamSet_cc,8,
	&(lvalues[5693]),0)
CONST_ENTRY_NID("HMAC","hmac",NID_hmac,0,NULL,0)
CONST_ENTRY_NID("LocalKeySet","Microsoft Local Key set",NID_LocalKeySet,9,
	&(lvalues[5701]),0)
CONST_ENTRY_NID("freshestCRL","X509v3 Freshest CRL",NID_freshest_crl,3,
	&(lvalues[5710]),0)
CONST_ENTRY_NID("id-on-permanentIdentifier","Permanent Identifier",
	NID_id_on_permanentIdentifier,8,&(lvalues[5713]),0)
CONST_ENTRY_NID("searchGuide","searchGuide",NID_searchGuide,3,&(lvalues[5721]),0)
CONST_ENTRY_NID("businessCategory","businessCategory",NID_businessCategory,3,
	&(lvalues[5724]),0)
CONST_ENTRY_NID("postalAddress","postalAddress",NID_postalAddress,3,&(lvalues[5727]),0)
CONST_ENTRY_NID("postOfficeBox","postOfficeBox",NID_postOfficeBox,3,&(lvalues[5730]),0)
CONST_ENTRY_NID("physicalDeliveryOfficeName","physicalDeliveryOfficeName",
	NID_physicalDeliveryOfficeName,3,&(lvalues[5733]),0)
CONST_ENTRY_NID("telephoneNumber","telephoneNumber",NID_telephoneNumber,3,
	&(lvalues[5736]),0)
CONST_ENTRY_NID("telexNumber","telexNumber",NID_telexNumber,3,&(lvalues[5739]),0)
CONST_ENTRY_NID("teletexTerminalIdentifier","teletexTerminalIdentifier",
	NID_teletexTerminalIdentifier,3,&(lvalues[5742]),0)
CONST_ENTRY_NID("facsimileTelephoneNumber","facsimileTelephoneNumber",
	NID_facsimileTelephoneNumber,3,&(lvalues[5745]),0)
CONST_ENTRY_NID("x121Address","x121Address",NID_x121Address,3,&(lvalues[5748]),0)
CONST_ENTRY_NID("internationaliSDNNumber","internationaliSDNNumber",
	NID_internationaliSDNNumber,3,&(lvalues[5751]),0)
CONST_ENTRY_NID("registeredAddress","registeredAddress",NID_registeredAddress,3,
	&(lvalues[5754]),0)
CONST_ENTRY_NID("destinationIndicator","destinationIndicator",
	NID_destinationIndicator,3,&(lvalues[5757]),0)
CONST_ENTRY_NID("preferredDeliveryMethod","preferredDeliveryMethod",
	NID_preferredDeliveryMethod,3,&(lvalues[5760]),0)
CONST_ENTRY_NID("presentationAddress","presentationAddress",NID_presentationAddress,
	3,&(lvalues[5763]),0)
CONST_ENTRY_NID("supportedApplicationContext","supportedApplicationContext",
	NID_supportedApplicationContext,3,&(lvalues[5766]),0)
CONST_ENTRY_NID("member","member",NID_member,3,&(lvalues[5769]),0)
CONST_ENTRY_NID("owner","owner",NID_owner,3,&(lvalues[5772]),0)
CONST_ENTRY_NID("roleOccupant","roleOccupant",NID_roleOccupant,3,&(lvalues[5775]),0)
CONST_ENTRY_NID("seeAlso","seeAlso",NID_seeAlso,3,&(lvalues[5778]),0)
CONST_ENTRY_NID("userPassword","userPassword",NID_userPassword,3,&(lvalues[5781]),0)
CONST_ENTRY_NID("userCertificate","userCertificate",NID_userCertificate,3,
	&(lvalues[5784]),0)
CONST_ENTRY_NID("cACertificate","cACertificate",NID_cACertificate,3,&(lvalues[5787]),0)
CONST_ENTRY_NID("authorityRevocationList","authorityRevocationList",
	NID_authorityRevocationList,3,&(lvalues[5790]),0)
CONST_ENTRY_NID("certificateRevocationList","certificateRevocationList",
	NID_certificateRevocationList,3,&(lvalues[5793]),0)
CONST_ENTRY_NID("crossCertificatePair","crossCertificatePair",
	NID_crossCertificatePair,3,&(lvalues[5796]),0)
CONST_ENTRY_NID("enhancedSearchGuide","enhancedSearchGuide",NID_enhancedSearchGuide,
	3,&(lvalues[5799]),0)
CONST_ENTRY_NID("protocolInformation","protocolInformation",NID_protocolInformation,
	3,&(lvalues[5802]),0)
CONST_ENTRY_NID("distinguishedName","distinguishedName",NID_distinguishedName,3,
	&(lvalues[5805]),0)
CONST_ENTRY_NID("uniqueMember","uniqueMember",NID_uniqueMember,3,&(lvalues[5808]),0)
CONST_ENTRY_NID("houseIdentifier","houseIdentifier",NID_houseIdentifier,3,
	&(lvalues[5811]),0)
CONST_ENTRY_NID("supportedAlgorithms","supportedAlgorithms",NID_supportedAlgorithms,
	3,&(lvalues[5814]),0)
CONST_ENTRY_NID("deltaRevocationList","deltaRevocationList",NID_deltaRevocationList,
	3,&(lvalues[5817]),0)
CONST_ENTRY_NID("dmdName","dmdName",NID_dmdName,3,&(lvalues[5820]),0)
CONST_END(nid_objs);

#define nid_objs OPENSSL_GLOBAL_ARRAY_NAME(nid_objs)

#ifndef OPERA_SMALL_VERSION
OPENSSL_PREFIX_CONST_ARRAY(static, unsigned int, sn_objs, libopeay)
CONST_ENTRY(364)	/* "AD_DVCS" */
CONST_ENTRY(419)	/* "AES-128-CBC" */
CONST_ENTRY(421)	/* "AES-128-CFB" */
CONST_ENTRY(650)	/* "AES-128-CFB1" */
CONST_ENTRY(653)	/* "AES-128-CFB8" */
CONST_ENTRY(418)	/* "AES-128-ECB" */
CONST_ENTRY(420)	/* "AES-128-OFB" */
CONST_ENTRY(423)	/* "AES-192-CBC" */
CONST_ENTRY(425)	/* "AES-192-CFB" */
CONST_ENTRY(651)	/* "AES-192-CFB1" */
CONST_ENTRY(654)	/* "AES-192-CFB8" */
CONST_ENTRY(422)	/* "AES-192-ECB" */
CONST_ENTRY(424)	/* "AES-192-OFB" */
CONST_ENTRY(427)	/* "AES-256-CBC" */
CONST_ENTRY(429)	/* "AES-256-CFB" */
CONST_ENTRY(652)	/* "AES-256-CFB1" */
CONST_ENTRY(655)	/* "AES-256-CFB8" */
CONST_ENTRY(426)	/* "AES-256-ECB" */
CONST_ENTRY(428)	/* "AES-256-OFB" */
CONST_ENTRY(91)	/* "BF-CBC" */
CONST_ENTRY(93)	/* "BF-CFB" */
CONST_ENTRY(92)	/* "BF-ECB" */
CONST_ENTRY(94)	/* "BF-OFB" */
CONST_ENTRY(14)	/* "C" */
CONST_ENTRY(751)	/* "CAMELLIA-128-CBC" */
CONST_ENTRY(757)	/* "CAMELLIA-128-CFB" */
CONST_ENTRY(760)	/* "CAMELLIA-128-CFB1" */
CONST_ENTRY(763)	/* "CAMELLIA-128-CFB8" */
CONST_ENTRY(754)	/* "CAMELLIA-128-ECB" */
CONST_ENTRY(766)	/* "CAMELLIA-128-OFB" */
CONST_ENTRY(752)	/* "CAMELLIA-192-CBC" */
CONST_ENTRY(758)	/* "CAMELLIA-192-CFB" */
CONST_ENTRY(761)	/* "CAMELLIA-192-CFB1" */
CONST_ENTRY(764)	/* "CAMELLIA-192-CFB8" */
CONST_ENTRY(755)	/* "CAMELLIA-192-ECB" */
CONST_ENTRY(767)	/* "CAMELLIA-192-OFB" */
CONST_ENTRY(753)	/* "CAMELLIA-256-CBC" */
CONST_ENTRY(759)	/* "CAMELLIA-256-CFB" */
CONST_ENTRY(762)	/* "CAMELLIA-256-CFB1" */
CONST_ENTRY(765)	/* "CAMELLIA-256-CFB8" */
CONST_ENTRY(756)	/* "CAMELLIA-256-ECB" */
CONST_ENTRY(768)	/* "CAMELLIA-256-OFB" */
CONST_ENTRY(108)	/* "CAST5-CBC" */
CONST_ENTRY(110)	/* "CAST5-CFB" */
CONST_ENTRY(109)	/* "CAST5-ECB" */
CONST_ENTRY(111)	/* "CAST5-OFB" */
CONST_ENTRY(13)	/* "CN" */
CONST_ENTRY(141)	/* "CRLReason" */
CONST_ENTRY(417)	/* "CSPName" */
CONST_ENTRY(367)	/* "CrlID" */
CONST_ENTRY(391)	/* "DC" */
CONST_ENTRY(31)	/* "DES-CBC" */
CONST_ENTRY(643)	/* "DES-CDMF" */
CONST_ENTRY(30)	/* "DES-CFB" */
CONST_ENTRY(656)	/* "DES-CFB1" */
CONST_ENTRY(657)	/* "DES-CFB8" */
CONST_ENTRY(29)	/* "DES-ECB" */
CONST_ENTRY(32)	/* "DES-EDE" */
CONST_ENTRY(43)	/* "DES-EDE-CBC" */
CONST_ENTRY(60)	/* "DES-EDE-CFB" */
CONST_ENTRY(62)	/* "DES-EDE-OFB" */
CONST_ENTRY(33)	/* "DES-EDE3" */
CONST_ENTRY(44)	/* "DES-EDE3-CBC" */
CONST_ENTRY(61)	/* "DES-EDE3-CFB" */
CONST_ENTRY(658)	/* "DES-EDE3-CFB1" */
CONST_ENTRY(659)	/* "DES-EDE3-CFB8" */
CONST_ENTRY(63)	/* "DES-EDE3-OFB" */
CONST_ENTRY(45)	/* "DES-OFB" */
CONST_ENTRY(80)	/* "DESX-CBC" */
CONST_ENTRY(380)	/* "DOD" */
CONST_ENTRY(116)	/* "DSA" */
CONST_ENTRY(66)	/* "DSA-SHA" */
CONST_ENTRY(113)	/* "DSA-SHA1" */
CONST_ENTRY(70)	/* "DSA-SHA1-old" */
CONST_ENTRY(67)	/* "DSA-old" */
CONST_ENTRY(297)	/* "DVCS" */
CONST_ENTRY(99)	/* "GN" */
CONST_ENTRY(855)	/* "HMAC" */
CONST_ENTRY(780)	/* "HMAC-MD5" */
CONST_ENTRY(781)	/* "HMAC-SHA1" */
CONST_ENTRY(381)	/* "IANA" */
CONST_ENTRY(34)	/* "IDEA-CBC" */
CONST_ENTRY(35)	/* "IDEA-CFB" */
CONST_ENTRY(36)	/* "IDEA-ECB" */
CONST_ENTRY(46)	/* "IDEA-OFB" */
CONST_ENTRY(181)	/* "ISO" */
CONST_ENTRY(183)	/* "ISO-US" */
CONST_ENTRY(645)	/* "ITU-T" */
CONST_ENTRY(646)	/* "JOINT-ISO-ITU-T" */
CONST_ENTRY(773)	/* "KISA" */
CONST_ENTRY(15)	/* "L" */
CONST_ENTRY(856)	/* "LocalKeySet" */
CONST_ENTRY( 3)	/* "MD2" */
CONST_ENTRY(257)	/* "MD4" */
CONST_ENTRY( 4)	/* "MD5" */
CONST_ENTRY(114)	/* "MD5-SHA1" */
CONST_ENTRY(95)	/* "MDC2" */
CONST_ENTRY(388)	/* "Mail" */
CONST_ENTRY(393)	/* "NULL" */
CONST_ENTRY(404)	/* "NULL" */
CONST_ENTRY(57)	/* "Netscape" */
CONST_ENTRY(366)	/* "Nonce" */
CONST_ENTRY(17)	/* "O" */
CONST_ENTRY(178)	/* "OCSP" */
CONST_ENTRY(180)	/* "OCSPSigning" */
CONST_ENTRY(379)	/* "ORG" */
CONST_ENTRY(18)	/* "OU" */
CONST_ENTRY(749)	/* "Oakley-EC2N-3" */
CONST_ENTRY(750)	/* "Oakley-EC2N-4" */
CONST_ENTRY( 9)	/* "PBE-MD2-DES" */
CONST_ENTRY(168)	/* "PBE-MD2-RC2-64" */
CONST_ENTRY(10)	/* "PBE-MD5-DES" */
CONST_ENTRY(169)	/* "PBE-MD5-RC2-64" */
CONST_ENTRY(147)	/* "PBE-SHA1-2DES" */
CONST_ENTRY(146)	/* "PBE-SHA1-3DES" */
CONST_ENTRY(170)	/* "PBE-SHA1-DES" */
CONST_ENTRY(148)	/* "PBE-SHA1-RC2-128" */
CONST_ENTRY(149)	/* "PBE-SHA1-RC2-40" */
CONST_ENTRY(68)	/* "PBE-SHA1-RC2-64" */
CONST_ENTRY(144)	/* "PBE-SHA1-RC4-128" */
CONST_ENTRY(145)	/* "PBE-SHA1-RC4-40" */
CONST_ENTRY(161)	/* "PBES2" */
CONST_ENTRY(69)	/* "PBKDF2" */
CONST_ENTRY(162)	/* "PBMAC1" */
CONST_ENTRY(127)	/* "PKIX" */
CONST_ENTRY(98)	/* "RC2-40-CBC" */
CONST_ENTRY(166)	/* "RC2-64-CBC" */
CONST_ENTRY(37)	/* "RC2-CBC" */
CONST_ENTRY(39)	/* "RC2-CFB" */
CONST_ENTRY(38)	/* "RC2-ECB" */
CONST_ENTRY(40)	/* "RC2-OFB" */
CONST_ENTRY( 5)	/* "RC4" */
CONST_ENTRY(97)	/* "RC4-40" */
CONST_ENTRY(120)	/* "RC5-CBC" */
CONST_ENTRY(122)	/* "RC5-CFB" */
CONST_ENTRY(121)	/* "RC5-ECB" */
CONST_ENTRY(123)	/* "RC5-OFB" */
CONST_ENTRY(117)	/* "RIPEMD160" */
CONST_ENTRY(124)	/* "RLE" */
CONST_ENTRY(19)	/* "RSA" */
CONST_ENTRY( 7)	/* "RSA-MD2" */
CONST_ENTRY(396)	/* "RSA-MD4" */
CONST_ENTRY( 8)	/* "RSA-MD5" */
CONST_ENTRY(96)	/* "RSA-MDC2" */
CONST_ENTRY(104)	/* "RSA-NP-MD5" */
CONST_ENTRY(119)	/* "RSA-RIPEMD160" */
CONST_ENTRY(42)	/* "RSA-SHA" */
CONST_ENTRY(65)	/* "RSA-SHA1" */
CONST_ENTRY(115)	/* "RSA-SHA1-2" */
CONST_ENTRY(671)	/* "RSA-SHA224" */
CONST_ENTRY(668)	/* "RSA-SHA256" */
CONST_ENTRY(669)	/* "RSA-SHA384" */
CONST_ENTRY(670)	/* "RSA-SHA512" */
CONST_ENTRY(777)	/* "SEED-CBC" */
CONST_ENTRY(779)	/* "SEED-CFB" */
CONST_ENTRY(776)	/* "SEED-ECB" */
CONST_ENTRY(778)	/* "SEED-OFB" */
CONST_ENTRY(41)	/* "SHA" */
CONST_ENTRY(64)	/* "SHA1" */
CONST_ENTRY(675)	/* "SHA224" */
CONST_ENTRY(672)	/* "SHA256" */
CONST_ENTRY(673)	/* "SHA384" */
CONST_ENTRY(674)	/* "SHA512" */
CONST_ENTRY(188)	/* "SMIME" */
CONST_ENTRY(167)	/* "SMIME-CAPS" */
CONST_ENTRY(100)	/* "SN" */
CONST_ENTRY(16)	/* "ST" */
CONST_ENTRY(143)	/* "SXNetID" */
CONST_ENTRY(458)	/* "UID" */
CONST_ENTRY( 0)	/* "UNDEF" */
CONST_ENTRY(11)	/* "X500" */
CONST_ENTRY(378)	/* "X500algorithms" */
CONST_ENTRY(12)	/* "X509" */
CONST_ENTRY(184)	/* "X9-57" */
CONST_ENTRY(185)	/* "X9cm" */
CONST_ENTRY(125)	/* "ZLIB" */
CONST_ENTRY(478)	/* "aRecord" */
CONST_ENTRY(289)	/* "aaControls" */
CONST_ENTRY(287)	/* "ac-auditEntity" */
CONST_ENTRY(397)	/* "ac-proxying" */
CONST_ENTRY(288)	/* "ac-targeting" */
CONST_ENTRY(368)	/* "acceptableResponses" */
CONST_ENTRY(446)	/* "account" */
CONST_ENTRY(363)	/* "ad_timestamping" */
CONST_ENTRY(376)	/* "algorithm" */
CONST_ENTRY(405)	/* "ansi-X9-62" */
CONST_ENTRY(746)	/* "anyPolicy" */
CONST_ENTRY(370)	/* "archiveCutoff" */
CONST_ENTRY(484)	/* "associatedDomain" */
CONST_ENTRY(485)	/* "associatedName" */
CONST_ENTRY(501)	/* "audio" */
CONST_ENTRY(177)	/* "authorityInfoAccess" */
CONST_ENTRY(90)	/* "authorityKeyIdentifier" */
CONST_ENTRY(882)	/* "authorityRevocationList" */
CONST_ENTRY(87)	/* "basicConstraints" */
CONST_ENTRY(365)	/* "basicOCSPResponse" */
CONST_ENTRY(285)	/* "biometricInfo" */
CONST_ENTRY(494)	/* "buildingName" */
CONST_ENTRY(860)	/* "businessCategory" */
CONST_ENTRY(691)	/* "c2onb191v4" */
CONST_ENTRY(692)	/* "c2onb191v5" */
CONST_ENTRY(697)	/* "c2onb239v4" */
CONST_ENTRY(698)	/* "c2onb239v5" */
CONST_ENTRY(684)	/* "c2pnb163v1" */
CONST_ENTRY(685)	/* "c2pnb163v2" */
CONST_ENTRY(686)	/* "c2pnb163v3" */
CONST_ENTRY(687)	/* "c2pnb176v1" */
CONST_ENTRY(693)	/* "c2pnb208w1" */
CONST_ENTRY(699)	/* "c2pnb272w1" */
CONST_ENTRY(700)	/* "c2pnb304w1" */
CONST_ENTRY(702)	/* "c2pnb368w1" */
CONST_ENTRY(688)	/* "c2tnb191v1" */
CONST_ENTRY(689)	/* "c2tnb191v2" */
CONST_ENTRY(690)	/* "c2tnb191v3" */
CONST_ENTRY(694)	/* "c2tnb239v1" */
CONST_ENTRY(695)	/* "c2tnb239v2" */
CONST_ENTRY(696)	/* "c2tnb239v3" */
CONST_ENTRY(701)	/* "c2tnb359v1" */
CONST_ENTRY(703)	/* "c2tnb431r1" */
CONST_ENTRY(881)	/* "cACertificate" */
CONST_ENTRY(483)	/* "cNAMERecord" */
CONST_ENTRY(179)	/* "caIssuers" */
CONST_ENTRY(785)	/* "caRepository" */
CONST_ENTRY(443)	/* "caseIgnoreIA5StringSyntax" */
CONST_ENTRY(152)	/* "certBag" */
CONST_ENTRY(677)	/* "certicom-arc" */
CONST_ENTRY(771)	/* "certificateIssuer" */
CONST_ENTRY(89)	/* "certificatePolicies" */
CONST_ENTRY(883)	/* "certificateRevocationList" */
CONST_ENTRY(54)	/* "challengePassword" */
CONST_ENTRY(407)	/* "characteristic-two-field" */
CONST_ENTRY(395)	/* "clearance" */
CONST_ENTRY(130)	/* "clientAuth" */
CONST_ENTRY(131)	/* "codeSigning" */
CONST_ENTRY(50)	/* "contentType" */
CONST_ENTRY(53)	/* "countersignature" */
CONST_ENTRY(153)	/* "crlBag" */
CONST_ENTRY(103)	/* "crlDistributionPoints" */
CONST_ENTRY(88)	/* "crlNumber" */
CONST_ENTRY(884)	/* "crossCertificatePair" */
CONST_ENTRY(806)	/* "cryptocom" */
CONST_ENTRY(805)	/* "cryptopro" */
CONST_ENTRY(500)	/* "dITRedirect" */
CONST_ENTRY(451)	/* "dNSDomain" */
CONST_ENTRY(495)	/* "dSAQuality" */
CONST_ENTRY(434)	/* "data" */
CONST_ENTRY(390)	/* "dcobject" */
CONST_ENTRY(140)	/* "deltaCRL" */
CONST_ENTRY(891)	/* "deltaRevocationList" */
CONST_ENTRY(107)	/* "description" */
CONST_ENTRY(871)	/* "destinationIndicator" */
CONST_ENTRY(28)	/* "dhKeyAgreement" */
CONST_ENTRY(382)	/* "directory" */
CONST_ENTRY(887)	/* "distinguishedName" */
CONST_ENTRY(892)	/* "dmdName" */
CONST_ENTRY(174)	/* "dnQualifier" */
CONST_ENTRY(447)	/* "document" */
CONST_ENTRY(471)	/* "documentAuthor" */
CONST_ENTRY(468)	/* "documentIdentifier" */
CONST_ENTRY(472)	/* "documentLocation" */
CONST_ENTRY(502)	/* "documentPublisher" */
CONST_ENTRY(449)	/* "documentSeries" */
CONST_ENTRY(469)	/* "documentTitle" */
CONST_ENTRY(470)	/* "documentVersion" */
CONST_ENTRY(392)	/* "domain" */
CONST_ENTRY(452)	/* "domainRelatedObject" */
CONST_ENTRY(802)	/* "dsa_with_SHA224" */
CONST_ENTRY(803)	/* "dsa_with_SHA256" */
CONST_ENTRY(791)	/* "ecdsa-with-Recommended" */
CONST_ENTRY(416)	/* "ecdsa-with-SHA1" */
CONST_ENTRY(793)	/* "ecdsa-with-SHA224" */
CONST_ENTRY(794)	/* "ecdsa-with-SHA256" */
CONST_ENTRY(795)	/* "ecdsa-with-SHA384" */
CONST_ENTRY(796)	/* "ecdsa-with-SHA512" */
CONST_ENTRY(792)	/* "ecdsa-with-Specified" */
CONST_ENTRY(48)	/* "emailAddress" */
CONST_ENTRY(132)	/* "emailProtection" */
CONST_ENTRY(885)	/* "enhancedSearchGuide" */
CONST_ENTRY(389)	/* "enterprises" */
CONST_ENTRY(384)	/* "experimental" */
CONST_ENTRY(172)	/* "extReq" */
CONST_ENTRY(56)	/* "extendedCertificateAttributes" */
CONST_ENTRY(126)	/* "extendedKeyUsage" */
CONST_ENTRY(372)	/* "extendedStatus" */
CONST_ENTRY(867)	/* "facsimileTelephoneNumber" */
CONST_ENTRY(462)	/* "favouriteDrink" */
CONST_ENTRY(857)	/* "freshestCRL" */
CONST_ENTRY(453)	/* "friendlyCountry" */
CONST_ENTRY(490)	/* "friendlyCountryName" */
CONST_ENTRY(156)	/* "friendlyName" */
CONST_ENTRY(509)	/* "generationQualifier" */
CONST_ENTRY(815)	/* "gost-mac" */
CONST_ENTRY(811)	/* "gost2001" */
CONST_ENTRY(851)	/* "gost2001cc" */
CONST_ENTRY(813)	/* "gost89" */
CONST_ENTRY(814)	/* "gost89-cnt" */
CONST_ENTRY(812)	/* "gost94" */
CONST_ENTRY(850)	/* "gost94cc" */
CONST_ENTRY(797)	/* "hmacWithMD5" */
CONST_ENTRY(163)	/* "hmacWithSHA1" */
CONST_ENTRY(798)	/* "hmacWithSHA224" */
CONST_ENTRY(799)	/* "hmacWithSHA256" */
CONST_ENTRY(800)	/* "hmacWithSHA384" */
CONST_ENTRY(801)	/* "hmacWithSHA512" */
CONST_ENTRY(432)	/* "holdInstructionCallIssuer" */
CONST_ENTRY(430)	/* "holdInstructionCode" */
CONST_ENTRY(431)	/* "holdInstructionNone" */
CONST_ENTRY(433)	/* "holdInstructionReject" */
CONST_ENTRY(486)	/* "homePostalAddress" */
CONST_ENTRY(473)	/* "homeTelephoneNumber" */
CONST_ENTRY(466)	/* "host" */
CONST_ENTRY(889)	/* "houseIdentifier" */
CONST_ENTRY(442)	/* "iA5StringSyntax" */
CONST_ENTRY(783)	/* "id-DHBasedMac" */
CONST_ENTRY(824)	/* "id-Gost28147-89-CryptoPro-A-ParamSet" */
CONST_ENTRY(825)	/* "id-Gost28147-89-CryptoPro-B-ParamSet" */
CONST_ENTRY(826)	/* "id-Gost28147-89-CryptoPro-C-ParamSet" */
CONST_ENTRY(827)	/* "id-Gost28147-89-CryptoPro-D-ParamSet" */
CONST_ENTRY(819)	/* "id-Gost28147-89-CryptoPro-KeyMeshing" */
CONST_ENTRY(829)	/* "id-Gost28147-89-CryptoPro-Oscar-1-0-ParamSet" */
CONST_ENTRY(828)	/* "id-Gost28147-89-CryptoPro-Oscar-1-1-ParamSet" */
CONST_ENTRY(830)	/* "id-Gost28147-89-CryptoPro-RIC-1-ParamSet" */
CONST_ENTRY(820)	/* "id-Gost28147-89-None-KeyMeshing" */
CONST_ENTRY(823)	/* "id-Gost28147-89-TestParamSet" */
CONST_ENTRY(849)	/* "id-Gost28147-89-cc" */
CONST_ENTRY(840)	/* "id-GostR3410-2001-CryptoPro-A-ParamSet" */
CONST_ENTRY(841)	/* "id-GostR3410-2001-CryptoPro-B-ParamSet" */
CONST_ENTRY(842)	/* "id-GostR3410-2001-CryptoPro-C-ParamSet" */
CONST_ENTRY(843)	/* "id-GostR3410-2001-CryptoPro-XchA-ParamSet" */
CONST_ENTRY(844)	/* "id-GostR3410-2001-CryptoPro-XchB-ParamSet" */
CONST_ENTRY(854)	/* "id-GostR3410-2001-ParamSet-cc" */
CONST_ENTRY(839)	/* "id-GostR3410-2001-TestParamSet" */
CONST_ENTRY(817)	/* "id-GostR3410-2001DH" */
CONST_ENTRY(832)	/* "id-GostR3410-94-CryptoPro-A-ParamSet" */
CONST_ENTRY(833)	/* "id-GostR3410-94-CryptoPro-B-ParamSet" */
CONST_ENTRY(834)	/* "id-GostR3410-94-CryptoPro-C-ParamSet" */
CONST_ENTRY(835)	/* "id-GostR3410-94-CryptoPro-D-ParamSet" */
CONST_ENTRY(836)	/* "id-GostR3410-94-CryptoPro-XchA-ParamSet" */
CONST_ENTRY(837)	/* "id-GostR3410-94-CryptoPro-XchB-ParamSet" */
CONST_ENTRY(838)	/* "id-GostR3410-94-CryptoPro-XchC-ParamSet" */
CONST_ENTRY(831)	/* "id-GostR3410-94-TestParamSet" */
CONST_ENTRY(845)	/* "id-GostR3410-94-a" */
CONST_ENTRY(846)	/* "id-GostR3410-94-aBis" */
CONST_ENTRY(847)	/* "id-GostR3410-94-b" */
CONST_ENTRY(848)	/* "id-GostR3410-94-bBis" */
CONST_ENTRY(818)	/* "id-GostR3410-94DH" */
CONST_ENTRY(822)	/* "id-GostR3411-94-CryptoProParamSet" */
CONST_ENTRY(821)	/* "id-GostR3411-94-TestParamSet" */
CONST_ENTRY(807)	/* "id-GostR3411-94-with-GostR3410-2001" */
CONST_ENTRY(853)	/* "id-GostR3411-94-with-GostR3410-2001-cc" */
CONST_ENTRY(808)	/* "id-GostR3411-94-with-GostR3410-94" */
CONST_ENTRY(852)	/* "id-GostR3411-94-with-GostR3410-94-cc" */
CONST_ENTRY(810)	/* "id-HMACGostR3411-94" */
CONST_ENTRY(782)	/* "id-PasswordBasedMAC" */
CONST_ENTRY(266)	/* "id-aca" */
CONST_ENTRY(355)	/* "id-aca-accessIdentity" */
CONST_ENTRY(354)	/* "id-aca-authenticationInfo" */
CONST_ENTRY(356)	/* "id-aca-chargingIdentity" */
CONST_ENTRY(399)	/* "id-aca-encAttrs" */
CONST_ENTRY(357)	/* "id-aca-group" */
CONST_ENTRY(358)	/* "id-aca-role" */
CONST_ENTRY(176)	/* "id-ad" */
CONST_ENTRY(788)	/* "id-aes128-wrap" */
CONST_ENTRY(789)	/* "id-aes192-wrap" */
CONST_ENTRY(790)	/* "id-aes256-wrap" */
CONST_ENTRY(262)	/* "id-alg" */
CONST_ENTRY(323)	/* "id-alg-des40" */
CONST_ENTRY(326)	/* "id-alg-dh-pop" */
CONST_ENTRY(325)	/* "id-alg-dh-sig-hmac-sha1" */
CONST_ENTRY(324)	/* "id-alg-noSignature" */
CONST_ENTRY(268)	/* "id-cct" */
CONST_ENTRY(361)	/* "id-cct-PKIData" */
CONST_ENTRY(362)	/* "id-cct-PKIResponse" */
CONST_ENTRY(360)	/* "id-cct-crs" */
CONST_ENTRY(81)	/* "id-ce" */
CONST_ENTRY(680)	/* "id-characteristic-two-basis" */
CONST_ENTRY(263)	/* "id-cmc" */
CONST_ENTRY(334)	/* "id-cmc-addExtensions" */
CONST_ENTRY(346)	/* "id-cmc-confirmCertAcceptance" */
CONST_ENTRY(330)	/* "id-cmc-dataReturn" */
CONST_ENTRY(336)	/* "id-cmc-decryptedPOP" */
CONST_ENTRY(335)	/* "id-cmc-encryptedPOP" */
CONST_ENTRY(339)	/* "id-cmc-getCRL" */
CONST_ENTRY(338)	/* "id-cmc-getCert" */
CONST_ENTRY(328)	/* "id-cmc-identification" */
CONST_ENTRY(329)	/* "id-cmc-identityProof" */
CONST_ENTRY(337)	/* "id-cmc-lraPOPWitness" */
CONST_ENTRY(344)	/* "id-cmc-popLinkRandom" */
CONST_ENTRY(345)	/* "id-cmc-popLinkWitness" */
CONST_ENTRY(343)	/* "id-cmc-queryPending" */
CONST_ENTRY(333)	/* "id-cmc-recipientNonce" */
CONST_ENTRY(341)	/* "id-cmc-regInfo" */
CONST_ENTRY(342)	/* "id-cmc-responseInfo" */
CONST_ENTRY(340)	/* "id-cmc-revokeRequest" */
CONST_ENTRY(332)	/* "id-cmc-senderNonce" */
CONST_ENTRY(327)	/* "id-cmc-statusInfo" */
CONST_ENTRY(331)	/* "id-cmc-transactionId" */
CONST_ENTRY(787)	/* "id-ct-asciiTextWithCRLF" */
CONST_ENTRY(408)	/* "id-ecPublicKey" */
CONST_ENTRY(508)	/* "id-hex-multipart-message" */
CONST_ENTRY(507)	/* "id-hex-partial-message" */
CONST_ENTRY(260)	/* "id-it" */
CONST_ENTRY(302)	/* "id-it-caKeyUpdateInfo" */
CONST_ENTRY(298)	/* "id-it-caProtEncCert" */
CONST_ENTRY(311)	/* "id-it-confirmWaitTime" */
CONST_ENTRY(303)	/* "id-it-currentCRL" */
CONST_ENTRY(300)	/* "id-it-encKeyPairTypes" */
CONST_ENTRY(310)	/* "id-it-implicitConfirm" */
CONST_ENTRY(308)	/* "id-it-keyPairParamRep" */
CONST_ENTRY(307)	/* "id-it-keyPairParamReq" */
CONST_ENTRY(312)	/* "id-it-origPKIMessage" */
CONST_ENTRY(301)	/* "id-it-preferredSymmAlg" */
CONST_ENTRY(309)	/* "id-it-revPassphrase" */
CONST_ENTRY(299)	/* "id-it-signKeyPairTypes" */
CONST_ENTRY(305)	/* "id-it-subscriptionRequest" */
CONST_ENTRY(306)	/* "id-it-subscriptionResponse" */
CONST_ENTRY(784)	/* "id-it-suppLangTags" */
CONST_ENTRY(304)	/* "id-it-unsupportedOIDs" */
CONST_ENTRY(128)	/* "id-kp" */
CONST_ENTRY(280)	/* "id-mod-attribute-cert" */
CONST_ENTRY(274)	/* "id-mod-cmc" */
CONST_ENTRY(277)	/* "id-mod-cmp" */
CONST_ENTRY(284)	/* "id-mod-cmp2000" */
CONST_ENTRY(273)	/* "id-mod-crmf" */
CONST_ENTRY(283)	/* "id-mod-dvcs" */
CONST_ENTRY(275)	/* "id-mod-kea-profile-88" */
CONST_ENTRY(276)	/* "id-mod-kea-profile-93" */
CONST_ENTRY(282)	/* "id-mod-ocsp" */
CONST_ENTRY(278)	/* "id-mod-qualified-cert-88" */
CONST_ENTRY(279)	/* "id-mod-qualified-cert-93" */
CONST_ENTRY(281)	/* "id-mod-timestamp-protocol" */
CONST_ENTRY(264)	/* "id-on" */
CONST_ENTRY(858)	/* "id-on-permanentIdentifier" */
CONST_ENTRY(347)	/* "id-on-personalData" */
CONST_ENTRY(265)	/* "id-pda" */
CONST_ENTRY(352)	/* "id-pda-countryOfCitizenship" */
CONST_ENTRY(353)	/* "id-pda-countryOfResidence" */
CONST_ENTRY(348)	/* "id-pda-dateOfBirth" */
CONST_ENTRY(351)	/* "id-pda-gender" */
CONST_ENTRY(349)	/* "id-pda-placeOfBirth" */
CONST_ENTRY(175)	/* "id-pe" */
CONST_ENTRY(261)	/* "id-pkip" */
CONST_ENTRY(258)	/* "id-pkix-mod" */
CONST_ENTRY(269)	/* "id-pkix1-explicit-88" */
CONST_ENTRY(271)	/* "id-pkix1-explicit-93" */
CONST_ENTRY(270)	/* "id-pkix1-implicit-88" */
CONST_ENTRY(272)	/* "id-pkix1-implicit-93" */
CONST_ENTRY(662)	/* "id-ppl" */
CONST_ENTRY(664)	/* "id-ppl-anyLanguage" */
CONST_ENTRY(667)	/* "id-ppl-independent" */
CONST_ENTRY(665)	/* "id-ppl-inheritAll" */
CONST_ENTRY(267)	/* "id-qcs" */
CONST_ENTRY(359)	/* "id-qcs-pkixQCSyntax-v1" */
CONST_ENTRY(259)	/* "id-qt" */
CONST_ENTRY(164)	/* "id-qt-cps" */
CONST_ENTRY(165)	/* "id-qt-unotice" */
CONST_ENTRY(313)	/* "id-regCtrl" */
CONST_ENTRY(316)	/* "id-regCtrl-authenticator" */
CONST_ENTRY(319)	/* "id-regCtrl-oldCertID" */
CONST_ENTRY(318)	/* "id-regCtrl-pkiArchiveOptions" */
CONST_ENTRY(317)	/* "id-regCtrl-pkiPublicationInfo" */
CONST_ENTRY(320)	/* "id-regCtrl-protocolEncrKey" */
CONST_ENTRY(315)	/* "id-regCtrl-regToken" */
CONST_ENTRY(314)	/* "id-regInfo" */
CONST_ENTRY(322)	/* "id-regInfo-certReq" */
CONST_ENTRY(321)	/* "id-regInfo-utf8Pairs" */
CONST_ENTRY(512)	/* "id-set" */
CONST_ENTRY(191)	/* "id-smime-aa" */
CONST_ENTRY(215)	/* "id-smime-aa-contentHint" */
CONST_ENTRY(218)	/* "id-smime-aa-contentIdentifier" */
CONST_ENTRY(221)	/* "id-smime-aa-contentReference" */
CONST_ENTRY(240)	/* "id-smime-aa-dvcs-dvc" */
CONST_ENTRY(217)	/* "id-smime-aa-encapContentType" */
CONST_ENTRY(222)	/* "id-smime-aa-encrypKeyPref" */
CONST_ENTRY(220)	/* "id-smime-aa-equivalentLabels" */
CONST_ENTRY(232)	/* "id-smime-aa-ets-CertificateRefs" */
CONST_ENTRY(233)	/* "id-smime-aa-ets-RevocationRefs" */
CONST_ENTRY(238)	/* "id-smime-aa-ets-archiveTimeStamp" */
CONST_ENTRY(237)	/* "id-smime-aa-ets-certCRLTimestamp" */
CONST_ENTRY(234)	/* "id-smime-aa-ets-certValues" */
CONST_ENTRY(227)	/* "id-smime-aa-ets-commitmentType" */
CONST_ENTRY(231)	/* "id-smime-aa-ets-contentTimestamp" */
CONST_ENTRY(236)	/* "id-smime-aa-ets-escTimeStamp" */
CONST_ENTRY(230)	/* "id-smime-aa-ets-otherSigCert" */
CONST_ENTRY(235)	/* "id-smime-aa-ets-revocationValues" */
CONST_ENTRY(226)	/* "id-smime-aa-ets-sigPolicyId" */
CONST_ENTRY(229)	/* "id-smime-aa-ets-signerAttr" */
CONST_ENTRY(228)	/* "id-smime-aa-ets-signerLocation" */
CONST_ENTRY(219)	/* "id-smime-aa-macValue" */
CONST_ENTRY(214)	/* "id-smime-aa-mlExpandHistory" */
CONST_ENTRY(216)	/* "id-smime-aa-msgSigDigest" */
CONST_ENTRY(212)	/* "id-smime-aa-receiptRequest" */
CONST_ENTRY(213)	/* "id-smime-aa-securityLabel" */
CONST_ENTRY(239)	/* "id-smime-aa-signatureType" */
CONST_ENTRY(223)	/* "id-smime-aa-signingCertificate" */
CONST_ENTRY(224)	/* "id-smime-aa-smimeEncryptCerts" */
CONST_ENTRY(225)	/* "id-smime-aa-timeStampToken" */
CONST_ENTRY(192)	/* "id-smime-alg" */
CONST_ENTRY(243)	/* "id-smime-alg-3DESwrap" */
CONST_ENTRY(246)	/* "id-smime-alg-CMS3DESwrap" */
CONST_ENTRY(247)	/* "id-smime-alg-CMSRC2wrap" */
CONST_ENTRY(245)	/* "id-smime-alg-ESDH" */
CONST_ENTRY(241)	/* "id-smime-alg-ESDHwith3DES" */
CONST_ENTRY(242)	/* "id-smime-alg-ESDHwithRC2" */
CONST_ENTRY(244)	/* "id-smime-alg-RC2wrap" */
CONST_ENTRY(193)	/* "id-smime-cd" */
CONST_ENTRY(248)	/* "id-smime-cd-ldap" */
CONST_ENTRY(190)	/* "id-smime-ct" */
CONST_ENTRY(210)	/* "id-smime-ct-DVCSRequestData" */
CONST_ENTRY(211)	/* "id-smime-ct-DVCSResponseData" */
CONST_ENTRY(208)	/* "id-smime-ct-TDTInfo" */
CONST_ENTRY(207)	/* "id-smime-ct-TSTInfo" */
CONST_ENTRY(205)	/* "id-smime-ct-authData" */
CONST_ENTRY(786)	/* "id-smime-ct-compressedData" */
CONST_ENTRY(209)	/* "id-smime-ct-contentInfo" */
CONST_ENTRY(206)	/* "id-smime-ct-publishCert" */
CONST_ENTRY(204)	/* "id-smime-ct-receipt" */
CONST_ENTRY(195)	/* "id-smime-cti" */
CONST_ENTRY(255)	/* "id-smime-cti-ets-proofOfApproval" */
CONST_ENTRY(256)	/* "id-smime-cti-ets-proofOfCreation" */
CONST_ENTRY(253)	/* "id-smime-cti-ets-proofOfDelivery" */
CONST_ENTRY(251)	/* "id-smime-cti-ets-proofOfOrigin" */
CONST_ENTRY(252)	/* "id-smime-cti-ets-proofOfReceipt" */
CONST_ENTRY(254)	/* "id-smime-cti-ets-proofOfSender" */
CONST_ENTRY(189)	/* "id-smime-mod" */
CONST_ENTRY(196)	/* "id-smime-mod-cms" */
CONST_ENTRY(197)	/* "id-smime-mod-ess" */
CONST_ENTRY(202)	/* "id-smime-mod-ets-eSigPolicy-88" */
CONST_ENTRY(203)	/* "id-smime-mod-ets-eSigPolicy-97" */
CONST_ENTRY(200)	/* "id-smime-mod-ets-eSignature-88" */
CONST_ENTRY(201)	/* "id-smime-mod-ets-eSignature-97" */
CONST_ENTRY(199)	/* "id-smime-mod-msg-v3" */
CONST_ENTRY(198)	/* "id-smime-mod-oid" */
CONST_ENTRY(194)	/* "id-smime-spq" */
CONST_ENTRY(250)	/* "id-smime-spq-ets-sqt-unotice" */
CONST_ENTRY(249)	/* "id-smime-spq-ets-sqt-uri" */
CONST_ENTRY(676)	/* "identified-organization" */
CONST_ENTRY(461)	/* "info" */
CONST_ENTRY(748)	/* "inhibitAnyPolicy" */
CONST_ENTRY(101)	/* "initials" */
CONST_ENTRY(647)	/* "international-organizations" */
CONST_ENTRY(869)	/* "internationaliSDNNumber" */
CONST_ENTRY(142)	/* "invalidityDate" */
CONST_ENTRY(294)	/* "ipsecEndSystem" */
CONST_ENTRY(295)	/* "ipsecTunnel" */
CONST_ENTRY(296)	/* "ipsecUser" */
CONST_ENTRY(86)	/* "issuerAltName" */
CONST_ENTRY(770)	/* "issuingDistributionPoint" */
CONST_ENTRY(492)	/* "janetMailbox" */
CONST_ENTRY(150)	/* "keyBag" */
CONST_ENTRY(83)	/* "keyUsage" */
CONST_ENTRY(477)	/* "lastModifiedBy" */
CONST_ENTRY(476)	/* "lastModifiedTime" */
CONST_ENTRY(157)	/* "localKeyID" */
CONST_ENTRY(480)	/* "mXRecord" */
CONST_ENTRY(460)	/* "mail" */
CONST_ENTRY(493)	/* "mailPreferenceOption" */
CONST_ENTRY(467)	/* "manager" */
CONST_ENTRY(809)	/* "md_gost94" */
CONST_ENTRY(875)	/* "member" */
CONST_ENTRY(182)	/* "member-body" */
CONST_ENTRY(51)	/* "messageDigest" */
CONST_ENTRY(383)	/* "mgmt" */
CONST_ENTRY(504)	/* "mime-mhs" */
CONST_ENTRY(506)	/* "mime-mhs-bodies" */
CONST_ENTRY(505)	/* "mime-mhs-headings" */
CONST_ENTRY(488)	/* "mobileTelephoneNumber" */
CONST_ENTRY(136)	/* "msCTLSign" */
CONST_ENTRY(135)	/* "msCodeCom" */
CONST_ENTRY(134)	/* "msCodeInd" */
CONST_ENTRY(138)	/* "msEFS" */
CONST_ENTRY(171)	/* "msExtReq" */
CONST_ENTRY(137)	/* "msSGC" */
CONST_ENTRY(648)	/* "msSmartcardLogin" */
CONST_ENTRY(649)	/* "msUPN" */
CONST_ENTRY(481)	/* "nSRecord" */
CONST_ENTRY(173)	/* "name" */
CONST_ENTRY(666)	/* "nameConstraints" */
CONST_ENTRY(369)	/* "noCheck" */
CONST_ENTRY(403)	/* "noRevAvail" */
CONST_ENTRY(72)	/* "nsBaseUrl" */
CONST_ENTRY(76)	/* "nsCaPolicyUrl" */
CONST_ENTRY(74)	/* "nsCaRevocationUrl" */
CONST_ENTRY(58)	/* "nsCertExt" */
CONST_ENTRY(79)	/* "nsCertSequence" */
CONST_ENTRY(71)	/* "nsCertType" */
CONST_ENTRY(78)	/* "nsComment" */
CONST_ENTRY(59)	/* "nsDataType" */
CONST_ENTRY(75)	/* "nsRenewalUrl" */
CONST_ENTRY(73)	/* "nsRevocationUrl" */
CONST_ENTRY(139)	/* "nsSGC" */
CONST_ENTRY(77)	/* "nsSslServerName" */
CONST_ENTRY(681)	/* "onBasis" */
CONST_ENTRY(491)	/* "organizationalStatus" */
CONST_ENTRY(475)	/* "otherMailbox" */
CONST_ENTRY(876)	/* "owner" */
CONST_ENTRY(489)	/* "pagerTelephoneNumber" */
CONST_ENTRY(374)	/* "path" */
CONST_ENTRY(112)	/* "pbeWithMD5AndCast5CBC" */
CONST_ENTRY(499)	/* "personalSignature" */
CONST_ENTRY(487)	/* "personalTitle" */
CONST_ENTRY(464)	/* "photo" */
CONST_ENTRY(863)	/* "physicalDeliveryOfficeName" */
CONST_ENTRY(437)	/* "pilot" */
CONST_ENTRY(439)	/* "pilotAttributeSyntax" */
CONST_ENTRY(438)	/* "pilotAttributeType" */
CONST_ENTRY(479)	/* "pilotAttributeType27" */
CONST_ENTRY(456)	/* "pilotDSA" */
CONST_ENTRY(441)	/* "pilotGroups" */
CONST_ENTRY(444)	/* "pilotObject" */
CONST_ENTRY(440)	/* "pilotObjectClass" */
CONST_ENTRY(455)	/* "pilotOrganization" */
CONST_ENTRY(445)	/* "pilotPerson" */
CONST_ENTRY( 2)	/* "pkcs" */
CONST_ENTRY(186)	/* "pkcs1" */
CONST_ENTRY(27)	/* "pkcs3" */
CONST_ENTRY(187)	/* "pkcs5" */
CONST_ENTRY(20)	/* "pkcs7" */
CONST_ENTRY(21)	/* "pkcs7-data" */
CONST_ENTRY(25)	/* "pkcs7-digestData" */
CONST_ENTRY(26)	/* "pkcs7-encryptedData" */
CONST_ENTRY(23)	/* "pkcs7-envelopedData" */
CONST_ENTRY(24)	/* "pkcs7-signedAndEnvelopedData" */
CONST_ENTRY(22)	/* "pkcs7-signedData" */
CONST_ENTRY(151)	/* "pkcs8ShroudedKeyBag" */
CONST_ENTRY(47)	/* "pkcs9" */
CONST_ENTRY(401)	/* "policyConstraints" */
CONST_ENTRY(747)	/* "policyMappings" */
CONST_ENTRY(862)	/* "postOfficeBox" */
CONST_ENTRY(861)	/* "postalAddress" */
CONST_ENTRY(661)	/* "postalCode" */
CONST_ENTRY(683)	/* "ppBasis" */
CONST_ENTRY(872)	/* "preferredDeliveryMethod" */
CONST_ENTRY(873)	/* "presentationAddress" */
CONST_ENTRY(816)	/* "prf-gostr3411-94" */
CONST_ENTRY(406)	/* "prime-field" */
CONST_ENTRY(409)	/* "prime192v1" */
CONST_ENTRY(410)	/* "prime192v2" */
CONST_ENTRY(411)	/* "prime192v3" */
CONST_ENTRY(412)	/* "prime239v1" */
CONST_ENTRY(413)	/* "prime239v2" */
CONST_ENTRY(414)	/* "prime239v3" */
CONST_ENTRY(415)	/* "prime256v1" */
CONST_ENTRY(385)	/* "private" */
CONST_ENTRY(84)	/* "privateKeyUsagePeriod" */
CONST_ENTRY(886)	/* "protocolInformation" */
CONST_ENTRY(663)	/* "proxyCertInfo" */
CONST_ENTRY(510)	/* "pseudonym" */
CONST_ENTRY(435)	/* "pss" */
CONST_ENTRY(286)	/* "qcStatements" */
CONST_ENTRY(457)	/* "qualityLabelledData" */
CONST_ENTRY(450)	/* "rFC822localPart" */
CONST_ENTRY(870)	/* "registeredAddress" */
CONST_ENTRY(400)	/* "role" */
CONST_ENTRY(877)	/* "roleOccupant" */
CONST_ENTRY(448)	/* "room" */
CONST_ENTRY(463)	/* "roomNumber" */
CONST_ENTRY( 6)	/* "rsaEncryption" */
CONST_ENTRY(644)	/* "rsaOAEPEncryptionSET" */
CONST_ENTRY(377)	/* "rsaSignature" */
CONST_ENTRY( 1)	/* "rsadsi" */
CONST_ENTRY(482)	/* "sOARecord" */
CONST_ENTRY(155)	/* "safeContentsBag" */
CONST_ENTRY(291)	/* "sbgp-autonomousSysNum" */
CONST_ENTRY(290)	/* "sbgp-ipAddrBlock" */
CONST_ENTRY(292)	/* "sbgp-routerIdentifier" */
CONST_ENTRY(159)	/* "sdsiCertificate" */
CONST_ENTRY(859)	/* "searchGuide" */
CONST_ENTRY(704)	/* "secp112r1" */
CONST_ENTRY(705)	/* "secp112r2" */
CONST_ENTRY(706)	/* "secp128r1" */
CONST_ENTRY(707)	/* "secp128r2" */
CONST_ENTRY(708)	/* "secp160k1" */
CONST_ENTRY(709)	/* "secp160r1" */
CONST_ENTRY(710)	/* "secp160r2" */
CONST_ENTRY(711)	/* "secp192k1" */
CONST_ENTRY(712)	/* "secp224k1" */
CONST_ENTRY(713)	/* "secp224r1" */
CONST_ENTRY(714)	/* "secp256k1" */
CONST_ENTRY(715)	/* "secp384r1" */
CONST_ENTRY(716)	/* "secp521r1" */
CONST_ENTRY(154)	/* "secretBag" */
CONST_ENTRY(474)	/* "secretary" */
CONST_ENTRY(717)	/* "sect113r1" */
CONST_ENTRY(718)	/* "sect113r2" */
CONST_ENTRY(719)	/* "sect131r1" */
CONST_ENTRY(720)	/* "sect131r2" */
CONST_ENTRY(721)	/* "sect163k1" */
CONST_ENTRY(722)	/* "sect163r1" */
CONST_ENTRY(723)	/* "sect163r2" */
CONST_ENTRY(724)	/* "sect193r1" */
CONST_ENTRY(725)	/* "sect193r2" */
CONST_ENTRY(726)	/* "sect233k1" */
CONST_ENTRY(727)	/* "sect233r1" */
CONST_ENTRY(728)	/* "sect239k1" */
CONST_ENTRY(729)	/* "sect283k1" */
CONST_ENTRY(730)	/* "sect283r1" */
CONST_ENTRY(731)	/* "sect409k1" */
CONST_ENTRY(732)	/* "sect409r1" */
CONST_ENTRY(733)	/* "sect571k1" */
CONST_ENTRY(734)	/* "sect571r1" */
CONST_ENTRY(386)	/* "security" */
CONST_ENTRY(878)	/* "seeAlso" */
CONST_ENTRY(394)	/* "selected-attribute-types" */
CONST_ENTRY(105)	/* "serialNumber" */
CONST_ENTRY(129)	/* "serverAuth" */
CONST_ENTRY(371)	/* "serviceLocator" */
CONST_ENTRY(625)	/* "set-addPolicy" */
CONST_ENTRY(515)	/* "set-attr" */
CONST_ENTRY(518)	/* "set-brand" */
CONST_ENTRY(638)	/* "set-brand-AmericanExpress" */
CONST_ENTRY(637)	/* "set-brand-Diners" */
CONST_ENTRY(636)	/* "set-brand-IATA-ATA" */
CONST_ENTRY(639)	/* "set-brand-JCB" */
CONST_ENTRY(641)	/* "set-brand-MasterCard" */
CONST_ENTRY(642)	/* "set-brand-Novus" */
CONST_ENTRY(640)	/* "set-brand-Visa" */
CONST_ENTRY(517)	/* "set-certExt" */
CONST_ENTRY(513)	/* "set-ctype" */
CONST_ENTRY(514)	/* "set-msgExt" */
CONST_ENTRY(516)	/* "set-policy" */
CONST_ENTRY(607)	/* "set-policy-root" */
CONST_ENTRY(624)	/* "set-rootKeyThumb" */
CONST_ENTRY(620)	/* "setAttr-Cert" */
CONST_ENTRY(631)	/* "setAttr-GenCryptgrm" */
CONST_ENTRY(623)	/* "setAttr-IssCap" */
CONST_ENTRY(628)	/* "setAttr-IssCap-CVM" */
CONST_ENTRY(630)	/* "setAttr-IssCap-Sig" */
CONST_ENTRY(629)	/* "setAttr-IssCap-T2" */
CONST_ENTRY(621)	/* "setAttr-PGWYcap" */
CONST_ENTRY(635)	/* "setAttr-SecDevSig" */
CONST_ENTRY(632)	/* "setAttr-T2Enc" */
CONST_ENTRY(633)	/* "setAttr-T2cleartxt" */
CONST_ENTRY(634)	/* "setAttr-TokICCsig" */
CONST_ENTRY(627)	/* "setAttr-Token-B0Prime" */
CONST_ENTRY(626)	/* "setAttr-Token-EMV" */
CONST_ENTRY(622)	/* "setAttr-TokenType" */
CONST_ENTRY(619)	/* "setCext-IssuerCapabilities" */
CONST_ENTRY(615)	/* "setCext-PGWYcapabilities" */
CONST_ENTRY(616)	/* "setCext-TokenIdentifier" */
CONST_ENTRY(618)	/* "setCext-TokenType" */
CONST_ENTRY(617)	/* "setCext-Track2Data" */
CONST_ENTRY(611)	/* "setCext-cCertRequired" */
CONST_ENTRY(609)	/* "setCext-certType" */
CONST_ENTRY(608)	/* "setCext-hashedRoot" */
CONST_ENTRY(610)	/* "setCext-merchData" */
CONST_ENTRY(613)	/* "setCext-setExt" */
CONST_ENTRY(614)	/* "setCext-setQualf" */
CONST_ENTRY(612)	/* "setCext-tunneling" */
CONST_ENTRY(540)	/* "setct-AcqCardCodeMsg" */
CONST_ENTRY(576)	/* "setct-AcqCardCodeMsgTBE" */
CONST_ENTRY(570)	/* "setct-AuthReqTBE" */
CONST_ENTRY(534)	/* "setct-AuthReqTBS" */
CONST_ENTRY(527)	/* "setct-AuthResBaggage" */
CONST_ENTRY(571)	/* "setct-AuthResTBE" */
CONST_ENTRY(572)	/* "setct-AuthResTBEX" */
CONST_ENTRY(535)	/* "setct-AuthResTBS" */
CONST_ENTRY(536)	/* "setct-AuthResTBSX" */
CONST_ENTRY(528)	/* "setct-AuthRevReqBaggage" */
CONST_ENTRY(577)	/* "setct-AuthRevReqTBE" */
CONST_ENTRY(541)	/* "setct-AuthRevReqTBS" */
CONST_ENTRY(529)	/* "setct-AuthRevResBaggage" */
CONST_ENTRY(542)	/* "setct-AuthRevResData" */
CONST_ENTRY(578)	/* "setct-AuthRevResTBE" */
CONST_ENTRY(579)	/* "setct-AuthRevResTBEB" */
CONST_ENTRY(543)	/* "setct-AuthRevResTBS" */
CONST_ENTRY(573)	/* "setct-AuthTokenTBE" */
CONST_ENTRY(537)	/* "setct-AuthTokenTBS" */
CONST_ENTRY(600)	/* "setct-BCIDistributionTBS" */
CONST_ENTRY(558)	/* "setct-BatchAdminReqData" */
CONST_ENTRY(592)	/* "setct-BatchAdminReqTBE" */
CONST_ENTRY(559)	/* "setct-BatchAdminResData" */
CONST_ENTRY(593)	/* "setct-BatchAdminResTBE" */
CONST_ENTRY(599)	/* "setct-CRLNotificationResTBS" */
CONST_ENTRY(598)	/* "setct-CRLNotificationTBS" */
CONST_ENTRY(580)	/* "setct-CapReqTBE" */
CONST_ENTRY(581)	/* "setct-CapReqTBEX" */
CONST_ENTRY(544)	/* "setct-CapReqTBS" */
CONST_ENTRY(545)	/* "setct-CapReqTBSX" */
CONST_ENTRY(546)	/* "setct-CapResData" */
CONST_ENTRY(582)	/* "setct-CapResTBE" */
CONST_ENTRY(583)	/* "setct-CapRevReqTBE" */
CONST_ENTRY(584)	/* "setct-CapRevReqTBEX" */
CONST_ENTRY(547)	/* "setct-CapRevReqTBS" */
CONST_ENTRY(548)	/* "setct-CapRevReqTBSX" */
CONST_ENTRY(549)	/* "setct-CapRevResData" */
CONST_ENTRY(585)	/* "setct-CapRevResTBE" */
CONST_ENTRY(538)	/* "setct-CapTokenData" */
CONST_ENTRY(530)	/* "setct-CapTokenSeq" */
CONST_ENTRY(574)	/* "setct-CapTokenTBE" */
CONST_ENTRY(575)	/* "setct-CapTokenTBEX" */
CONST_ENTRY(539)	/* "setct-CapTokenTBS" */
CONST_ENTRY(560)	/* "setct-CardCInitResTBS" */
CONST_ENTRY(566)	/* "setct-CertInqReqTBS" */
CONST_ENTRY(563)	/* "setct-CertReqData" */
CONST_ENTRY(595)	/* "setct-CertReqTBE" */
CONST_ENTRY(596)	/* "setct-CertReqTBEX" */
CONST_ENTRY(564)	/* "setct-CertReqTBS" */
CONST_ENTRY(565)	/* "setct-CertResData" */
CONST_ENTRY(597)	/* "setct-CertResTBE" */
CONST_ENTRY(586)	/* "setct-CredReqTBE" */
CONST_ENTRY(587)	/* "setct-CredReqTBEX" */
CONST_ENTRY(550)	/* "setct-CredReqTBS" */
CONST_ENTRY(551)	/* "setct-CredReqTBSX" */
CONST_ENTRY(552)	/* "setct-CredResData" */
CONST_ENTRY(588)	/* "setct-CredResTBE" */
CONST_ENTRY(589)	/* "setct-CredRevReqTBE" */
CONST_ENTRY(590)	/* "setct-CredRevReqTBEX" */
CONST_ENTRY(553)	/* "setct-CredRevReqTBS" */
CONST_ENTRY(554)	/* "setct-CredRevReqTBSX" */
CONST_ENTRY(555)	/* "setct-CredRevResData" */
CONST_ENTRY(591)	/* "setct-CredRevResTBE" */
CONST_ENTRY(567)	/* "setct-ErrorTBS" */
CONST_ENTRY(526)	/* "setct-HODInput" */
CONST_ENTRY(561)	/* "setct-MeAqCInitResTBS" */
CONST_ENTRY(522)	/* "setct-OIData" */
CONST_ENTRY(519)	/* "setct-PANData" */
CONST_ENTRY(521)	/* "setct-PANOnly" */
CONST_ENTRY(520)	/* "setct-PANToken" */
CONST_ENTRY(556)	/* "setct-PCertReqData" */
CONST_ENTRY(557)	/* "setct-PCertResTBS" */
CONST_ENTRY(523)	/* "setct-PI" */
CONST_ENTRY(532)	/* "setct-PI-TBS" */
CONST_ENTRY(524)	/* "setct-PIData" */
CONST_ENTRY(525)	/* "setct-PIDataUnsigned" */
CONST_ENTRY(568)	/* "setct-PIDualSignedTBE" */
CONST_ENTRY(569)	/* "setct-PIUnsignedTBE" */
CONST_ENTRY(531)	/* "setct-PInitResData" */
CONST_ENTRY(533)	/* "setct-PResData" */
CONST_ENTRY(594)	/* "setct-RegFormReqTBE" */
CONST_ENTRY(562)	/* "setct-RegFormResTBS" */
CONST_ENTRY(606)	/* "setext-cv" */
CONST_ENTRY(601)	/* "setext-genCrypt" */
CONST_ENTRY(602)	/* "setext-miAuth" */
CONST_ENTRY(604)	/* "setext-pinAny" */
CONST_ENTRY(603)	/* "setext-pinSecure" */
CONST_ENTRY(605)	/* "setext-track2" */
CONST_ENTRY(52)	/* "signingTime" */
CONST_ENTRY(454)	/* "simpleSecurityObject" */
CONST_ENTRY(496)	/* "singleLevelQuality" */
CONST_ENTRY(387)	/* "snmpv2" */
CONST_ENTRY(660)	/* "street" */
CONST_ENTRY(85)	/* "subjectAltName" */
CONST_ENTRY(769)	/* "subjectDirectoryAttributes" */
CONST_ENTRY(398)	/* "subjectInfoAccess" */
CONST_ENTRY(82)	/* "subjectKeyIdentifier" */
CONST_ENTRY(498)	/* "subtreeMaximumQuality" */
CONST_ENTRY(497)	/* "subtreeMinimumQuality" */
CONST_ENTRY(890)	/* "supportedAlgorithms" */
CONST_ENTRY(874)	/* "supportedApplicationContext" */
CONST_ENTRY(402)	/* "targetInformation" */
CONST_ENTRY(864)	/* "telephoneNumber" */
CONST_ENTRY(866)	/* "teletexTerminalIdentifier" */
CONST_ENTRY(865)	/* "telexNumber" */
CONST_ENTRY(459)	/* "textEncodedORAddress" */
CONST_ENTRY(293)	/* "textNotice" */
CONST_ENTRY(133)	/* "timeStamping" */
CONST_ENTRY(106)	/* "title" */
CONST_ENTRY(682)	/* "tpBasis" */
CONST_ENTRY(375)	/* "trustRoot" */
CONST_ENTRY(436)	/* "ucl" */
CONST_ENTRY(888)	/* "uniqueMember" */
CONST_ENTRY(55)	/* "unstructuredAddress" */
CONST_ENTRY(49)	/* "unstructuredName" */
CONST_ENTRY(880)	/* "userCertificate" */
CONST_ENTRY(465)	/* "userClass" */
CONST_ENTRY(879)	/* "userPassword" */
CONST_ENTRY(373)	/* "valid" */
CONST_ENTRY(678)	/* "wap" */
CONST_ENTRY(679)	/* "wap-wsg" */
CONST_ENTRY(735)	/* "wap-wsg-idm-ecid-wtls1" */
CONST_ENTRY(743)	/* "wap-wsg-idm-ecid-wtls10" */
CONST_ENTRY(744)	/* "wap-wsg-idm-ecid-wtls11" */
CONST_ENTRY(745)	/* "wap-wsg-idm-ecid-wtls12" */
CONST_ENTRY(736)	/* "wap-wsg-idm-ecid-wtls3" */
CONST_ENTRY(737)	/* "wap-wsg-idm-ecid-wtls4" */
CONST_ENTRY(738)	/* "wap-wsg-idm-ecid-wtls5" */
CONST_ENTRY(739)	/* "wap-wsg-idm-ecid-wtls6" */
CONST_ENTRY(740)	/* "wap-wsg-idm-ecid-wtls7" */
CONST_ENTRY(741)	/* "wap-wsg-idm-ecid-wtls8" */
CONST_ENTRY(742)	/* "wap-wsg-idm-ecid-wtls9" */
CONST_ENTRY(804)	/* "whirlpool" */
CONST_ENTRY(868)	/* "x121Address" */
CONST_ENTRY(503)	/* "x500UniqueIdentifier" */
CONST_ENTRY(158)	/* "x509Certificate" */
CONST_ENTRY(160)	/* "x509Crl" */
CONST_END(sn_objs)

#endif // !OPERA_SMALL_VERSION

#ifndef OPERA_SMALL_VERSION
OPENSSL_PREFIX_CONST_ARRAY(static, unsigned int, ln_objs, libopeay)
CONST_ENTRY(363)	/* "AD Time Stamping" */
CONST_ENTRY(405)	/* "ANSI X9.62" */
CONST_ENTRY(368)	/* "Acceptable OCSP Responses" */
CONST_ENTRY(664)	/* "Any language" */
CONST_ENTRY(177)	/* "Authority Information Access" */
CONST_ENTRY(365)	/* "Basic OCSP Response" */
CONST_ENTRY(285)	/* "Biometric Info" */
CONST_ENTRY(179)	/* "CA Issuers" */
CONST_ENTRY(785)	/* "CA Repository" */
CONST_ENTRY(131)	/* "Code Signing" */
CONST_ENTRY(783)	/* "Diffie-Hellman based MAC" */
CONST_ENTRY(382)	/* "Directory" */
CONST_ENTRY(392)	/* "Domain" */
CONST_ENTRY(132)	/* "E-mail Protection" */
CONST_ENTRY(389)	/* "Enterprises" */
CONST_ENTRY(384)	/* "Experimental" */
CONST_ENTRY(372)	/* "Extended OCSP Status" */
CONST_ENTRY(172)	/* "Extension Request" */
CONST_ENTRY(813)	/* "GOST 28147-89" */
CONST_ENTRY(849)	/* "GOST 28147-89 Cryptocom ParamSet" */
CONST_ENTRY(815)	/* "GOST 28147-89 MAC" */
CONST_ENTRY(851)	/* "GOST 34.10-2001 Cryptocom" */
CONST_ENTRY(850)	/* "GOST 34.10-94 Cryptocom" */
CONST_ENTRY(811)	/* "GOST R 34.10-2001" */
CONST_ENTRY(817)	/* "GOST R 34.10-2001 DH" */
CONST_ENTRY(812)	/* "GOST R 34.10-94" */
CONST_ENTRY(818)	/* "GOST R 34.10-94 DH" */
CONST_ENTRY(809)	/* "GOST R 34.11-94" */
CONST_ENTRY(816)	/* "GOST R 34.11-94 PRF" */
CONST_ENTRY(807)	/* "GOST R 34.11-94 with GOST R 34.10-2001" */
CONST_ENTRY(853)	/* "GOST R 34.11-94 with GOST R 34.10-2001 Cryptocom" */
CONST_ENTRY(808)	/* "GOST R 34.11-94 with GOST R 34.10-94" */
CONST_ENTRY(852)	/* "GOST R 34.11-94 with GOST R 34.10-94 Cryptocom" */
CONST_ENTRY(854)	/* "GOST R 3410-2001 Parameter Set Cryptocom" */
CONST_ENTRY(810)	/* "HMAC GOST 34.11-94" */
CONST_ENTRY(432)	/* "Hold Instruction Call Issuer" */
CONST_ENTRY(430)	/* "Hold Instruction Code" */
CONST_ENTRY(431)	/* "Hold Instruction None" */
CONST_ENTRY(433)	/* "Hold Instruction Reject" */
CONST_ENTRY(634)	/* "ICC or token signature" */
CONST_ENTRY(294)	/* "IPSec End System" */
CONST_ENTRY(295)	/* "IPSec Tunnel" */
CONST_ENTRY(296)	/* "IPSec User" */
CONST_ENTRY(182)	/* "ISO Member Body" */
CONST_ENTRY(183)	/* "ISO US Member Body" */
CONST_ENTRY(667)	/* "Independent" */
CONST_ENTRY(665)	/* "Inherit all" */
CONST_ENTRY(647)	/* "International Organizations" */
CONST_ENTRY(142)	/* "Invalidity Date" */
CONST_ENTRY(504)	/* "MIME MHS" */
CONST_ENTRY(388)	/* "Mail" */
CONST_ENTRY(383)	/* "Management" */
CONST_ENTRY(417)	/* "Microsoft CSP Name" */
CONST_ENTRY(135)	/* "Microsoft Commercial Code Signing" */
CONST_ENTRY(138)	/* "Microsoft Encrypted File System" */
CONST_ENTRY(171)	/* "Microsoft Extension Request" */
CONST_ENTRY(134)	/* "Microsoft Individual Code Signing" */
CONST_ENTRY(856)	/* "Microsoft Local Key set" */
CONST_ENTRY(137)	/* "Microsoft Server Gated Crypto" */
CONST_ENTRY(648)	/* "Microsoft Smartcardlogin" */
CONST_ENTRY(136)	/* "Microsoft Trust List Signing" */
CONST_ENTRY(649)	/* "Microsoft Universal Principal Name" */
CONST_ENTRY(393)	/* "NULL" */
CONST_ENTRY(404)	/* "NULL" */
CONST_ENTRY(72)	/* "Netscape Base Url" */
CONST_ENTRY(76)	/* "Netscape CA Policy Url" */
CONST_ENTRY(74)	/* "Netscape CA Revocation Url" */
CONST_ENTRY(71)	/* "Netscape Cert Type" */
CONST_ENTRY(58)	/* "Netscape Certificate Extension" */
CONST_ENTRY(79)	/* "Netscape Certificate Sequence" */
CONST_ENTRY(78)	/* "Netscape Comment" */
CONST_ENTRY(57)	/* "Netscape Communications Corp." */
CONST_ENTRY(59)	/* "Netscape Data Type" */
CONST_ENTRY(75)	/* "Netscape Renewal Url" */
CONST_ENTRY(73)	/* "Netscape Revocation Url" */
CONST_ENTRY(77)	/* "Netscape SSL Server Name" */
CONST_ENTRY(139)	/* "Netscape Server Gated Crypto" */
CONST_ENTRY(178)	/* "OCSP" */
CONST_ENTRY(370)	/* "OCSP Archive Cutoff" */
CONST_ENTRY(367)	/* "OCSP CRL ID" */
CONST_ENTRY(369)	/* "OCSP No Check" */
CONST_ENTRY(366)	/* "OCSP Nonce" */
CONST_ENTRY(371)	/* "OCSP Service Locator" */
CONST_ENTRY(180)	/* "OCSP Signing" */
CONST_ENTRY(161)	/* "PBES2" */
CONST_ENTRY(69)	/* "PBKDF2" */
CONST_ENTRY(162)	/* "PBMAC1" */
CONST_ENTRY(127)	/* "PKIX" */
CONST_ENTRY(858)	/* "Permanent Identifier" */
CONST_ENTRY(164)	/* "Policy Qualifier CPS" */
CONST_ENTRY(165)	/* "Policy Qualifier User Notice" */
CONST_ENTRY(385)	/* "Private" */
CONST_ENTRY(663)	/* "Proxy Certificate Information" */
CONST_ENTRY( 1)	/* "RSA Data Security, Inc." */
CONST_ENTRY( 2)	/* "RSA Data Security, Inc. PKCS" */
CONST_ENTRY(188)	/* "S/MIME" */
CONST_ENTRY(167)	/* "S/MIME Capabilities" */
CONST_ENTRY(387)	/* "SNMPv2" */
CONST_ENTRY(512)	/* "Secure Electronic Transactions" */
CONST_ENTRY(386)	/* "Security" */
CONST_ENTRY(394)	/* "Selected Attribute Types" */
CONST_ENTRY(143)	/* "Strong Extranet ID" */
CONST_ENTRY(398)	/* "Subject Information Access" */
CONST_ENTRY(130)	/* "TLS Web Client Authentication" */
CONST_ENTRY(129)	/* "TLS Web Server Authentication" */
CONST_ENTRY(133)	/* "Time Stamping" */
CONST_ENTRY(375)	/* "Trust Root" */
CONST_ENTRY(12)	/* "X509" */
CONST_ENTRY(402)	/* "X509v3 AC Targeting" */
CONST_ENTRY(746)	/* "X509v3 Any Policy" */
CONST_ENTRY(90)	/* "X509v3 Authority Key Identifier" */
CONST_ENTRY(87)	/* "X509v3 Basic Constraints" */
CONST_ENTRY(103)	/* "X509v3 CRL Distribution Points" */
CONST_ENTRY(88)	/* "X509v3 CRL Number" */
CONST_ENTRY(141)	/* "X509v3 CRL Reason Code" */
CONST_ENTRY(771)	/* "X509v3 Certificate Issuer" */
CONST_ENTRY(89)	/* "X509v3 Certificate Policies" */
CONST_ENTRY(140)	/* "X509v3 Delta CRL Indicator" */
CONST_ENTRY(126)	/* "X509v3 Extended Key Usage" */
CONST_ENTRY(857)	/* "X509v3 Freshest CRL" */
CONST_ENTRY(748)	/* "X509v3 Inhibit Any Policy" */
CONST_ENTRY(86)	/* "X509v3 Issuer Alternative Name" */
CONST_ENTRY(770)	/* "X509v3 Issuing Distrubution Point" */
CONST_ENTRY(83)	/* "X509v3 Key Usage" */
CONST_ENTRY(666)	/* "X509v3 Name Constraints" */
CONST_ENTRY(403)	/* "X509v3 No Revocation Available" */
CONST_ENTRY(401)	/* "X509v3 Policy Constraints" */
CONST_ENTRY(747)	/* "X509v3 Policy Mappings" */
CONST_ENTRY(84)	/* "X509v3 Private Key Usage Period" */
CONST_ENTRY(85)	/* "X509v3 Subject Alternative Name" */
CONST_ENTRY(769)	/* "X509v3 Subject Directory Attributes" */
CONST_ENTRY(82)	/* "X509v3 Subject Key Identifier" */
CONST_ENTRY(184)	/* "X9.57" */
CONST_ENTRY(185)	/* "X9.57 CM ?" */
CONST_ENTRY(478)	/* "aRecord" */
CONST_ENTRY(289)	/* "aaControls" */
CONST_ENTRY(287)	/* "ac-auditEntity" */
CONST_ENTRY(397)	/* "ac-proxying" */
CONST_ENTRY(288)	/* "ac-targeting" */
CONST_ENTRY(446)	/* "account" */
CONST_ENTRY(364)	/* "ad dvcs" */
CONST_ENTRY(606)	/* "additional verification" */
CONST_ENTRY(419)	/* "aes-128-cbc" */
CONST_ENTRY(421)	/* "aes-128-cfb" */
CONST_ENTRY(650)	/* "aes-128-cfb1" */
CONST_ENTRY(653)	/* "aes-128-cfb8" */
CONST_ENTRY(418)	/* "aes-128-ecb" */
CONST_ENTRY(420)	/* "aes-128-ofb" */
CONST_ENTRY(423)	/* "aes-192-cbc" */
CONST_ENTRY(425)	/* "aes-192-cfb" */
CONST_ENTRY(651)	/* "aes-192-cfb1" */
CONST_ENTRY(654)	/* "aes-192-cfb8" */
CONST_ENTRY(422)	/* "aes-192-ecb" */
CONST_ENTRY(424)	/* "aes-192-ofb" */
CONST_ENTRY(427)	/* "aes-256-cbc" */
CONST_ENTRY(429)	/* "aes-256-cfb" */
CONST_ENTRY(652)	/* "aes-256-cfb1" */
CONST_ENTRY(655)	/* "aes-256-cfb8" */
CONST_ENTRY(426)	/* "aes-256-ecb" */
CONST_ENTRY(428)	/* "aes-256-ofb" */
CONST_ENTRY(376)	/* "algorithm" */
CONST_ENTRY(484)	/* "associatedDomain" */
CONST_ENTRY(485)	/* "associatedName" */
CONST_ENTRY(501)	/* "audio" */
CONST_ENTRY(882)	/* "authorityRevocationList" */
CONST_ENTRY(91)	/* "bf-cbc" */
CONST_ENTRY(93)	/* "bf-cfb" */
CONST_ENTRY(92)	/* "bf-ecb" */
CONST_ENTRY(94)	/* "bf-ofb" */
CONST_ENTRY(494)	/* "buildingName" */
CONST_ENTRY(860)	/* "businessCategory" */
CONST_ENTRY(691)	/* "c2onb191v4" */
CONST_ENTRY(692)	/* "c2onb191v5" */
CONST_ENTRY(697)	/* "c2onb239v4" */
CONST_ENTRY(698)	/* "c2onb239v5" */
CONST_ENTRY(684)	/* "c2pnb163v1" */
CONST_ENTRY(685)	/* "c2pnb163v2" */
CONST_ENTRY(686)	/* "c2pnb163v3" */
CONST_ENTRY(687)	/* "c2pnb176v1" */
CONST_ENTRY(693)	/* "c2pnb208w1" */
CONST_ENTRY(699)	/* "c2pnb272w1" */
CONST_ENTRY(700)	/* "c2pnb304w1" */
CONST_ENTRY(702)	/* "c2pnb368w1" */
CONST_ENTRY(688)	/* "c2tnb191v1" */
CONST_ENTRY(689)	/* "c2tnb191v2" */
CONST_ENTRY(690)	/* "c2tnb191v3" */
CONST_ENTRY(694)	/* "c2tnb239v1" */
CONST_ENTRY(695)	/* "c2tnb239v2" */
CONST_ENTRY(696)	/* "c2tnb239v3" */
CONST_ENTRY(701)	/* "c2tnb359v1" */
CONST_ENTRY(703)	/* "c2tnb431r1" */
CONST_ENTRY(881)	/* "cACertificate" */
CONST_ENTRY(483)	/* "cNAMERecord" */
CONST_ENTRY(751)	/* "camellia-128-cbc" */
CONST_ENTRY(757)	/* "camellia-128-cfb" */
CONST_ENTRY(760)	/* "camellia-128-cfb1" */
CONST_ENTRY(763)	/* "camellia-128-cfb8" */
CONST_ENTRY(754)	/* "camellia-128-ecb" */
CONST_ENTRY(766)	/* "camellia-128-ofb" */
CONST_ENTRY(752)	/* "camellia-192-cbc" */
CONST_ENTRY(758)	/* "camellia-192-cfb" */
CONST_ENTRY(761)	/* "camellia-192-cfb1" */
CONST_ENTRY(764)	/* "camellia-192-cfb8" */
CONST_ENTRY(755)	/* "camellia-192-ecb" */
CONST_ENTRY(767)	/* "camellia-192-ofb" */
CONST_ENTRY(753)	/* "camellia-256-cbc" */
CONST_ENTRY(759)	/* "camellia-256-cfb" */
CONST_ENTRY(762)	/* "camellia-256-cfb1" */
CONST_ENTRY(765)	/* "camellia-256-cfb8" */
CONST_ENTRY(756)	/* "camellia-256-ecb" */
CONST_ENTRY(768)	/* "camellia-256-ofb" */
CONST_ENTRY(443)	/* "caseIgnoreIA5StringSyntax" */
CONST_ENTRY(108)	/* "cast5-cbc" */
CONST_ENTRY(110)	/* "cast5-cfb" */
CONST_ENTRY(109)	/* "cast5-ecb" */
CONST_ENTRY(111)	/* "cast5-ofb" */
CONST_ENTRY(152)	/* "certBag" */
CONST_ENTRY(677)	/* "certicom-arc" */
CONST_ENTRY(517)	/* "certificate extensions" */
CONST_ENTRY(883)	/* "certificateRevocationList" */
CONST_ENTRY(54)	/* "challengePassword" */
CONST_ENTRY(407)	/* "characteristic-two-field" */
CONST_ENTRY(395)	/* "clearance" */
CONST_ENTRY(633)	/* "cleartext track 2" */
CONST_ENTRY(13)	/* "commonName" */
CONST_ENTRY(513)	/* "content types" */
CONST_ENTRY(50)	/* "contentType" */
CONST_ENTRY(53)	/* "countersignature" */
CONST_ENTRY(14)	/* "countryName" */
CONST_ENTRY(153)	/* "crlBag" */
CONST_ENTRY(884)	/* "crossCertificatePair" */
CONST_ENTRY(806)	/* "cryptocom" */
CONST_ENTRY(805)	/* "cryptopro" */
CONST_ENTRY(500)	/* "dITRedirect" */
CONST_ENTRY(451)	/* "dNSDomain" */
CONST_ENTRY(495)	/* "dSAQuality" */
CONST_ENTRY(434)	/* "data" */
CONST_ENTRY(390)	/* "dcObject" */
CONST_ENTRY(891)	/* "deltaRevocationList" */
CONST_ENTRY(31)	/* "des-cbc" */
CONST_ENTRY(643)	/* "des-cdmf" */
CONST_ENTRY(30)	/* "des-cfb" */
CONST_ENTRY(656)	/* "des-cfb1" */
CONST_ENTRY(657)	/* "des-cfb8" */
CONST_ENTRY(29)	/* "des-ecb" */
CONST_ENTRY(32)	/* "des-ede" */
CONST_ENTRY(43)	/* "des-ede-cbc" */
CONST_ENTRY(60)	/* "des-ede-cfb" */
CONST_ENTRY(62)	/* "des-ede-ofb" */
CONST_ENTRY(33)	/* "des-ede3" */
CONST_ENTRY(44)	/* "des-ede3-cbc" */
CONST_ENTRY(61)	/* "des-ede3-cfb" */
CONST_ENTRY(658)	/* "des-ede3-cfb1" */
CONST_ENTRY(659)	/* "des-ede3-cfb8" */
CONST_ENTRY(63)	/* "des-ede3-ofb" */
CONST_ENTRY(45)	/* "des-ofb" */
CONST_ENTRY(107)	/* "description" */
CONST_ENTRY(871)	/* "destinationIndicator" */
CONST_ENTRY(80)	/* "desx-cbc" */
CONST_ENTRY(28)	/* "dhKeyAgreement" */
CONST_ENTRY(11)	/* "directory services (X.500)" */
CONST_ENTRY(378)	/* "directory services - algorithms" */
CONST_ENTRY(887)	/* "distinguishedName" */
CONST_ENTRY(892)	/* "dmdName" */
CONST_ENTRY(174)	/* "dnQualifier" */
CONST_ENTRY(447)	/* "document" */
CONST_ENTRY(471)	/* "documentAuthor" */
CONST_ENTRY(468)	/* "documentIdentifier" */
CONST_ENTRY(472)	/* "documentLocation" */
CONST_ENTRY(502)	/* "documentPublisher" */
CONST_ENTRY(449)	/* "documentSeries" */
CONST_ENTRY(469)	/* "documentTitle" */
CONST_ENTRY(470)	/* "documentVersion" */
CONST_ENTRY(380)	/* "dod" */
CONST_ENTRY(391)	/* "domainComponent" */
CONST_ENTRY(452)	/* "domainRelatedObject" */
CONST_ENTRY(116)	/* "dsaEncryption" */
CONST_ENTRY(67)	/* "dsaEncryption-old" */
CONST_ENTRY(66)	/* "dsaWithSHA" */
CONST_ENTRY(113)	/* "dsaWithSHA1" */
CONST_ENTRY(70)	/* "dsaWithSHA1-old" */
CONST_ENTRY(802)	/* "dsa_with_SHA224" */
CONST_ENTRY(803)	/* "dsa_with_SHA256" */
CONST_ENTRY(297)	/* "dvcs" */
CONST_ENTRY(791)	/* "ecdsa-with-Recommended" */
CONST_ENTRY(416)	/* "ecdsa-with-SHA1" */
CONST_ENTRY(793)	/* "ecdsa-with-SHA224" */
CONST_ENTRY(794)	/* "ecdsa-with-SHA256" */
CONST_ENTRY(795)	/* "ecdsa-with-SHA384" */
CONST_ENTRY(796)	/* "ecdsa-with-SHA512" */
CONST_ENTRY(792)	/* "ecdsa-with-Specified" */
CONST_ENTRY(48)	/* "emailAddress" */
CONST_ENTRY(632)	/* "encrypted track 2" */
CONST_ENTRY(885)	/* "enhancedSearchGuide" */
CONST_ENTRY(56)	/* "extendedCertificateAttributes" */
CONST_ENTRY(867)	/* "facsimileTelephoneNumber" */
CONST_ENTRY(462)	/* "favouriteDrink" */
CONST_ENTRY(453)	/* "friendlyCountry" */
CONST_ENTRY(490)	/* "friendlyCountryName" */
CONST_ENTRY(156)	/* "friendlyName" */
CONST_ENTRY(631)	/* "generate cryptogram" */
CONST_ENTRY(509)	/* "generationQualifier" */
CONST_ENTRY(601)	/* "generic cryptogram" */
CONST_ENTRY(99)	/* "givenName" */
CONST_ENTRY(814)	/* "gost89-cnt" */
CONST_ENTRY(855)	/* "hmac" */
CONST_ENTRY(780)	/* "hmac-md5" */
CONST_ENTRY(781)	/* "hmac-sha1" */
CONST_ENTRY(797)	/* "hmacWithMD5" */
CONST_ENTRY(163)	/* "hmacWithSHA1" */
CONST_ENTRY(798)	/* "hmacWithSHA224" */
CONST_ENTRY(799)	/* "hmacWithSHA256" */
CONST_ENTRY(800)	/* "hmacWithSHA384" */
CONST_ENTRY(801)	/* "hmacWithSHA512" */
CONST_ENTRY(486)	/* "homePostalAddress" */
CONST_ENTRY(473)	/* "homeTelephoneNumber" */
CONST_ENTRY(466)	/* "host" */
CONST_ENTRY(889)	/* "houseIdentifier" */
CONST_ENTRY(442)	/* "iA5StringSyntax" */
CONST_ENTRY(381)	/* "iana" */
CONST_ENTRY(824)	/* "id-Gost28147-89-CryptoPro-A-ParamSet" */
CONST_ENTRY(825)	/* "id-Gost28147-89-CryptoPro-B-ParamSet" */
CONST_ENTRY(826)	/* "id-Gost28147-89-CryptoPro-C-ParamSet" */
CONST_ENTRY(827)	/* "id-Gost28147-89-CryptoPro-D-ParamSet" */
CONST_ENTRY(819)	/* "id-Gost28147-89-CryptoPro-KeyMeshing" */
CONST_ENTRY(829)	/* "id-Gost28147-89-CryptoPro-Oscar-1-0-ParamSet" */
CONST_ENTRY(828)	/* "id-Gost28147-89-CryptoPro-Oscar-1-1-ParamSet" */
CONST_ENTRY(830)	/* "id-Gost28147-89-CryptoPro-RIC-1-ParamSet" */
CONST_ENTRY(820)	/* "id-Gost28147-89-None-KeyMeshing" */
CONST_ENTRY(823)	/* "id-Gost28147-89-TestParamSet" */
CONST_ENTRY(840)	/* "id-GostR3410-2001-CryptoPro-A-ParamSet" */
CONST_ENTRY(841)	/* "id-GostR3410-2001-CryptoPro-B-ParamSet" */
CONST_ENTRY(842)	/* "id-GostR3410-2001-CryptoPro-C-ParamSet" */
CONST_ENTRY(843)	/* "id-GostR3410-2001-CryptoPro-XchA-ParamSet" */
CONST_ENTRY(844)	/* "id-GostR3410-2001-CryptoPro-XchB-ParamSet" */
CONST_ENTRY(839)	/* "id-GostR3410-2001-TestParamSet" */
CONST_ENTRY(832)	/* "id-GostR3410-94-CryptoPro-A-ParamSet" */
CONST_ENTRY(833)	/* "id-GostR3410-94-CryptoPro-B-ParamSet" */
CONST_ENTRY(834)	/* "id-GostR3410-94-CryptoPro-C-ParamSet" */
CONST_ENTRY(835)	/* "id-GostR3410-94-CryptoPro-D-ParamSet" */
CONST_ENTRY(836)	/* "id-GostR3410-94-CryptoPro-XchA-ParamSet" */
CONST_ENTRY(837)	/* "id-GostR3410-94-CryptoPro-XchB-ParamSet" */
CONST_ENTRY(838)	/* "id-GostR3410-94-CryptoPro-XchC-ParamSet" */
CONST_ENTRY(831)	/* "id-GostR3410-94-TestParamSet" */
CONST_ENTRY(845)	/* "id-GostR3410-94-a" */
CONST_ENTRY(846)	/* "id-GostR3410-94-aBis" */
CONST_ENTRY(847)	/* "id-GostR3410-94-b" */
CONST_ENTRY(848)	/* "id-GostR3410-94-bBis" */
CONST_ENTRY(822)	/* "id-GostR3411-94-CryptoProParamSet" */
CONST_ENTRY(821)	/* "id-GostR3411-94-TestParamSet" */
CONST_ENTRY(266)	/* "id-aca" */
CONST_ENTRY(355)	/* "id-aca-accessIdentity" */
CONST_ENTRY(354)	/* "id-aca-authenticationInfo" */
CONST_ENTRY(356)	/* "id-aca-chargingIdentity" */
CONST_ENTRY(399)	/* "id-aca-encAttrs" */
CONST_ENTRY(357)	/* "id-aca-group" */
CONST_ENTRY(358)	/* "id-aca-role" */
CONST_ENTRY(176)	/* "id-ad" */
CONST_ENTRY(788)	/* "id-aes128-wrap" */
CONST_ENTRY(789)	/* "id-aes192-wrap" */
CONST_ENTRY(790)	/* "id-aes256-wrap" */
CONST_ENTRY(262)	/* "id-alg" */
CONST_ENTRY(323)	/* "id-alg-des40" */
CONST_ENTRY(326)	/* "id-alg-dh-pop" */
CONST_ENTRY(325)	/* "id-alg-dh-sig-hmac-sha1" */
CONST_ENTRY(324)	/* "id-alg-noSignature" */
CONST_ENTRY(268)	/* "id-cct" */
CONST_ENTRY(361)	/* "id-cct-PKIData" */
CONST_ENTRY(362)	/* "id-cct-PKIResponse" */
CONST_ENTRY(360)	/* "id-cct-crs" */
CONST_ENTRY(81)	/* "id-ce" */
CONST_ENTRY(680)	/* "id-characteristic-two-basis" */
CONST_ENTRY(263)	/* "id-cmc" */
CONST_ENTRY(334)	/* "id-cmc-addExtensions" */
CONST_ENTRY(346)	/* "id-cmc-confirmCertAcceptance" */
CONST_ENTRY(330)	/* "id-cmc-dataReturn" */
CONST_ENTRY(336)	/* "id-cmc-decryptedPOP" */
CONST_ENTRY(335)	/* "id-cmc-encryptedPOP" */
CONST_ENTRY(339)	/* "id-cmc-getCRL" */
CONST_ENTRY(338)	/* "id-cmc-getCert" */
CONST_ENTRY(328)	/* "id-cmc-identification" */
CONST_ENTRY(329)	/* "id-cmc-identityProof" */
CONST_ENTRY(337)	/* "id-cmc-lraPOPWitness" */
CONST_ENTRY(344)	/* "id-cmc-popLinkRandom" */
CONST_ENTRY(345)	/* "id-cmc-popLinkWitness" */
CONST_ENTRY(343)	/* "id-cmc-queryPending" */
CONST_ENTRY(333)	/* "id-cmc-recipientNonce" */
CONST_ENTRY(341)	/* "id-cmc-regInfo" */
CONST_ENTRY(342)	/* "id-cmc-responseInfo" */
CONST_ENTRY(340)	/* "id-cmc-revokeRequest" */
CONST_ENTRY(332)	/* "id-cmc-senderNonce" */
CONST_ENTRY(327)	/* "id-cmc-statusInfo" */
CONST_ENTRY(331)	/* "id-cmc-transactionId" */
CONST_ENTRY(787)	/* "id-ct-asciiTextWithCRLF" */
CONST_ENTRY(408)	/* "id-ecPublicKey" */
CONST_ENTRY(508)	/* "id-hex-multipart-message" */
CONST_ENTRY(507)	/* "id-hex-partial-message" */
CONST_ENTRY(260)	/* "id-it" */
CONST_ENTRY(302)	/* "id-it-caKeyUpdateInfo" */
CONST_ENTRY(298)	/* "id-it-caProtEncCert" */
CONST_ENTRY(311)	/* "id-it-confirmWaitTime" */
CONST_ENTRY(303)	/* "id-it-currentCRL" */
CONST_ENTRY(300)	/* "id-it-encKeyPairTypes" */
CONST_ENTRY(310)	/* "id-it-implicitConfirm" */
CONST_ENTRY(308)	/* "id-it-keyPairParamRep" */
CONST_ENTRY(307)	/* "id-it-keyPairParamReq" */
CONST_ENTRY(312)	/* "id-it-origPKIMessage" */
CONST_ENTRY(301)	/* "id-it-preferredSymmAlg" */
CONST_ENTRY(309)	/* "id-it-revPassphrase" */
CONST_ENTRY(299)	/* "id-it-signKeyPairTypes" */
CONST_ENTRY(305)	/* "id-it-subscriptionRequest" */
CONST_ENTRY(306)	/* "id-it-subscriptionResponse" */
CONST_ENTRY(784)	/* "id-it-suppLangTags" */
CONST_ENTRY(304)	/* "id-it-unsupportedOIDs" */
CONST_ENTRY(128)	/* "id-kp" */
CONST_ENTRY(280)	/* "id-mod-attribute-cert" */
CONST_ENTRY(274)	/* "id-mod-cmc" */
CONST_ENTRY(277)	/* "id-mod-cmp" */
CONST_ENTRY(284)	/* "id-mod-cmp2000" */
CONST_ENTRY(273)	/* "id-mod-crmf" */
CONST_ENTRY(283)	/* "id-mod-dvcs" */
CONST_ENTRY(275)	/* "id-mod-kea-profile-88" */
CONST_ENTRY(276)	/* "id-mod-kea-profile-93" */
CONST_ENTRY(282)	/* "id-mod-ocsp" */
CONST_ENTRY(278)	/* "id-mod-qualified-cert-88" */
CONST_ENTRY(279)	/* "id-mod-qualified-cert-93" */
CONST_ENTRY(281)	/* "id-mod-timestamp-protocol" */
CONST_ENTRY(264)	/* "id-on" */
CONST_ENTRY(347)	/* "id-on-personalData" */
CONST_ENTRY(265)	/* "id-pda" */
CONST_ENTRY(352)	/* "id-pda-countryOfCitizenship" */
CONST_ENTRY(353)	/* "id-pda-countryOfResidence" */
CONST_ENTRY(348)	/* "id-pda-dateOfBirth" */
CONST_ENTRY(351)	/* "id-pda-gender" */
CONST_ENTRY(349)	/* "id-pda-placeOfBirth" */
CONST_ENTRY(175)	/* "id-pe" */
CONST_ENTRY(261)	/* "id-pkip" */
CONST_ENTRY(258)	/* "id-pkix-mod" */
CONST_ENTRY(269)	/* "id-pkix1-explicit-88" */
CONST_ENTRY(271)	/* "id-pkix1-explicit-93" */
CONST_ENTRY(270)	/* "id-pkix1-implicit-88" */
CONST_ENTRY(272)	/* "id-pkix1-implicit-93" */
CONST_ENTRY(662)	/* "id-ppl" */
CONST_ENTRY(267)	/* "id-qcs" */
CONST_ENTRY(359)	/* "id-qcs-pkixQCSyntax-v1" */
CONST_ENTRY(259)	/* "id-qt" */
CONST_ENTRY(313)	/* "id-regCtrl" */
CONST_ENTRY(316)	/* "id-regCtrl-authenticator" */
CONST_ENTRY(319)	/* "id-regCtrl-oldCertID" */
CONST_ENTRY(318)	/* "id-regCtrl-pkiArchiveOptions" */
CONST_ENTRY(317)	/* "id-regCtrl-pkiPublicationInfo" */
CONST_ENTRY(320)	/* "id-regCtrl-protocolEncrKey" */
CONST_ENTRY(315)	/* "id-regCtrl-regToken" */
CONST_ENTRY(314)	/* "id-regInfo" */
CONST_ENTRY(322)	/* "id-regInfo-certReq" */
CONST_ENTRY(321)	/* "id-regInfo-utf8Pairs" */
CONST_ENTRY(191)	/* "id-smime-aa" */
CONST_ENTRY(215)	/* "id-smime-aa-contentHint" */
CONST_ENTRY(218)	/* "id-smime-aa-contentIdentifier" */
CONST_ENTRY(221)	/* "id-smime-aa-contentReference" */
CONST_ENTRY(240)	/* "id-smime-aa-dvcs-dvc" */
CONST_ENTRY(217)	/* "id-smime-aa-encapContentType" */
CONST_ENTRY(222)	/* "id-smime-aa-encrypKeyPref" */
CONST_ENTRY(220)	/* "id-smime-aa-equivalentLabels" */
CONST_ENTRY(232)	/* "id-smime-aa-ets-CertificateRefs" */
CONST_ENTRY(233)	/* "id-smime-aa-ets-RevocationRefs" */
CONST_ENTRY(238)	/* "id-smime-aa-ets-archiveTimeStamp" */
CONST_ENTRY(237)	/* "id-smime-aa-ets-certCRLTimestamp" */
CONST_ENTRY(234)	/* "id-smime-aa-ets-certValues" */
CONST_ENTRY(227)	/* "id-smime-aa-ets-commitmentType" */
CONST_ENTRY(231)	/* "id-smime-aa-ets-contentTimestamp" */
CONST_ENTRY(236)	/* "id-smime-aa-ets-escTimeStamp" */
CONST_ENTRY(230)	/* "id-smime-aa-ets-otherSigCert" */
CONST_ENTRY(235)	/* "id-smime-aa-ets-revocationValues" */
CONST_ENTRY(226)	/* "id-smime-aa-ets-sigPolicyId" */
CONST_ENTRY(229)	/* "id-smime-aa-ets-signerAttr" */
CONST_ENTRY(228)	/* "id-smime-aa-ets-signerLocation" */
CONST_ENTRY(219)	/* "id-smime-aa-macValue" */
CONST_ENTRY(214)	/* "id-smime-aa-mlExpandHistory" */
CONST_ENTRY(216)	/* "id-smime-aa-msgSigDigest" */
CONST_ENTRY(212)	/* "id-smime-aa-receiptRequest" */
CONST_ENTRY(213)	/* "id-smime-aa-securityLabel" */
CONST_ENTRY(239)	/* "id-smime-aa-signatureType" */
CONST_ENTRY(223)	/* "id-smime-aa-signingCertificate" */
CONST_ENTRY(224)	/* "id-smime-aa-smimeEncryptCerts" */
CONST_ENTRY(225)	/* "id-smime-aa-timeStampToken" */
CONST_ENTRY(192)	/* "id-smime-alg" */
CONST_ENTRY(243)	/* "id-smime-alg-3DESwrap" */
CONST_ENTRY(246)	/* "id-smime-alg-CMS3DESwrap" */
CONST_ENTRY(247)	/* "id-smime-alg-CMSRC2wrap" */
CONST_ENTRY(245)	/* "id-smime-alg-ESDH" */
CONST_ENTRY(241)	/* "id-smime-alg-ESDHwith3DES" */
CONST_ENTRY(242)	/* "id-smime-alg-ESDHwithRC2" */
CONST_ENTRY(244)	/* "id-smime-alg-RC2wrap" */
CONST_ENTRY(193)	/* "id-smime-cd" */
CONST_ENTRY(248)	/* "id-smime-cd-ldap" */
CONST_ENTRY(190)	/* "id-smime-ct" */
CONST_ENTRY(210)	/* "id-smime-ct-DVCSRequestData" */
CONST_ENTRY(211)	/* "id-smime-ct-DVCSResponseData" */
CONST_ENTRY(208)	/* "id-smime-ct-TDTInfo" */
CONST_ENTRY(207)	/* "id-smime-ct-TSTInfo" */
CONST_ENTRY(205)	/* "id-smime-ct-authData" */
CONST_ENTRY(786)	/* "id-smime-ct-compressedData" */
CONST_ENTRY(209)	/* "id-smime-ct-contentInfo" */
CONST_ENTRY(206)	/* "id-smime-ct-publishCert" */
CONST_ENTRY(204)	/* "id-smime-ct-receipt" */
CONST_ENTRY(195)	/* "id-smime-cti" */
CONST_ENTRY(255)	/* "id-smime-cti-ets-proofOfApproval" */
CONST_ENTRY(256)	/* "id-smime-cti-ets-proofOfCreation" */
CONST_ENTRY(253)	/* "id-smime-cti-ets-proofOfDelivery" */
CONST_ENTRY(251)	/* "id-smime-cti-ets-proofOfOrigin" */
CONST_ENTRY(252)	/* "id-smime-cti-ets-proofOfReceipt" */
CONST_ENTRY(254)	/* "id-smime-cti-ets-proofOfSender" */
CONST_ENTRY(189)	/* "id-smime-mod" */
CONST_ENTRY(196)	/* "id-smime-mod-cms" */
CONST_ENTRY(197)	/* "id-smime-mod-ess" */
CONST_ENTRY(202)	/* "id-smime-mod-ets-eSigPolicy-88" */
CONST_ENTRY(203)	/* "id-smime-mod-ets-eSigPolicy-97" */
CONST_ENTRY(200)	/* "id-smime-mod-ets-eSignature-88" */
CONST_ENTRY(201)	/* "id-smime-mod-ets-eSignature-97" */
CONST_ENTRY(199)	/* "id-smime-mod-msg-v3" */
CONST_ENTRY(198)	/* "id-smime-mod-oid" */
CONST_ENTRY(194)	/* "id-smime-spq" */
CONST_ENTRY(250)	/* "id-smime-spq-ets-sqt-unotice" */
CONST_ENTRY(249)	/* "id-smime-spq-ets-sqt-uri" */
CONST_ENTRY(34)	/* "idea-cbc" */
CONST_ENTRY(35)	/* "idea-cfb" */
CONST_ENTRY(36)	/* "idea-ecb" */
CONST_ENTRY(46)	/* "idea-ofb" */
CONST_ENTRY(676)	/* "identified-organization" */
CONST_ENTRY(461)	/* "info" */
CONST_ENTRY(101)	/* "initials" */
CONST_ENTRY(869)	/* "internationaliSDNNumber" */
CONST_ENTRY(749)	/* "ipsec3" */
CONST_ENTRY(750)	/* "ipsec4" */
CONST_ENTRY(181)	/* "iso" */
CONST_ENTRY(623)	/* "issuer capabilities" */
CONST_ENTRY(645)	/* "itu-t" */
CONST_ENTRY(492)	/* "janetMailbox" */
CONST_ENTRY(646)	/* "joint-iso-itu-t" */
CONST_ENTRY(150)	/* "keyBag" */
CONST_ENTRY(773)	/* "kisa" */
CONST_ENTRY(477)	/* "lastModifiedBy" */
CONST_ENTRY(476)	/* "lastModifiedTime" */
CONST_ENTRY(157)	/* "localKeyID" */
CONST_ENTRY(15)	/* "localityName" */
CONST_ENTRY(480)	/* "mXRecord" */
CONST_ENTRY(493)	/* "mailPreferenceOption" */
CONST_ENTRY(467)	/* "manager" */
CONST_ENTRY( 3)	/* "md2" */
CONST_ENTRY( 7)	/* "md2WithRSAEncryption" */
CONST_ENTRY(257)	/* "md4" */
CONST_ENTRY(396)	/* "md4WithRSAEncryption" */
CONST_ENTRY( 4)	/* "md5" */
CONST_ENTRY(114)	/* "md5-sha1" */
CONST_ENTRY(104)	/* "md5WithRSA" */
CONST_ENTRY( 8)	/* "md5WithRSAEncryption" */
CONST_ENTRY(95)	/* "mdc2" */
CONST_ENTRY(96)	/* "mdc2WithRSA" */
CONST_ENTRY(875)	/* "member" */
CONST_ENTRY(602)	/* "merchant initiated auth" */
CONST_ENTRY(514)	/* "message extensions" */
CONST_ENTRY(51)	/* "messageDigest" */
CONST_ENTRY(506)	/* "mime-mhs-bodies" */
CONST_ENTRY(505)	/* "mime-mhs-headings" */
CONST_ENTRY(488)	/* "mobileTelephoneNumber" */
CONST_ENTRY(481)	/* "nSRecord" */
CONST_ENTRY(173)	/* "name" */
CONST_ENTRY(681)	/* "onBasis" */
CONST_ENTRY(379)	/* "org" */
CONST_ENTRY(17)	/* "organizationName" */
CONST_ENTRY(491)	/* "organizationalStatus" */
CONST_ENTRY(18)	/* "organizationalUnitName" */
CONST_ENTRY(475)	/* "otherMailbox" */
CONST_ENTRY(876)	/* "owner" */
CONST_ENTRY(489)	/* "pagerTelephoneNumber" */
CONST_ENTRY(782)	/* "password based MAC" */
CONST_ENTRY(374)	/* "path" */
CONST_ENTRY(621)	/* "payment gateway capabilities" */
CONST_ENTRY( 9)	/* "pbeWithMD2AndDES-CBC" */
CONST_ENTRY(168)	/* "pbeWithMD2AndRC2-CBC" */
CONST_ENTRY(112)	/* "pbeWithMD5AndCast5CBC" */
CONST_ENTRY(10)	/* "pbeWithMD5AndDES-CBC" */
CONST_ENTRY(169)	/* "pbeWithMD5AndRC2-CBC" */
CONST_ENTRY(148)	/* "pbeWithSHA1And128BitRC2-CBC" */
CONST_ENTRY(144)	/* "pbeWithSHA1And128BitRC4" */
CONST_ENTRY(147)	/* "pbeWithSHA1And2-KeyTripleDES-CBC" */
CONST_ENTRY(146)	/* "pbeWithSHA1And3-KeyTripleDES-CBC" */
CONST_ENTRY(149)	/* "pbeWithSHA1And40BitRC2-CBC" */
CONST_ENTRY(145)	/* "pbeWithSHA1And40BitRC4" */
CONST_ENTRY(170)	/* "pbeWithSHA1AndDES-CBC" */
CONST_ENTRY(68)	/* "pbeWithSHA1AndRC2-CBC" */
CONST_ENTRY(499)	/* "personalSignature" */
CONST_ENTRY(487)	/* "personalTitle" */
CONST_ENTRY(464)	/* "photo" */
CONST_ENTRY(863)	/* "physicalDeliveryOfficeName" */
CONST_ENTRY(437)	/* "pilot" */
CONST_ENTRY(439)	/* "pilotAttributeSyntax" */
CONST_ENTRY(438)	/* "pilotAttributeType" */
CONST_ENTRY(479)	/* "pilotAttributeType27" */
CONST_ENTRY(456)	/* "pilotDSA" */
CONST_ENTRY(441)	/* "pilotGroups" */
CONST_ENTRY(444)	/* "pilotObject" */
CONST_ENTRY(440)	/* "pilotObjectClass" */
CONST_ENTRY(455)	/* "pilotOrganization" */
CONST_ENTRY(445)	/* "pilotPerson" */
CONST_ENTRY(186)	/* "pkcs1" */
CONST_ENTRY(27)	/* "pkcs3" */
CONST_ENTRY(187)	/* "pkcs5" */
CONST_ENTRY(20)	/* "pkcs7" */
CONST_ENTRY(21)	/* "pkcs7-data" */
CONST_ENTRY(25)	/* "pkcs7-digestData" */
CONST_ENTRY(26)	/* "pkcs7-encryptedData" */
CONST_ENTRY(23)	/* "pkcs7-envelopedData" */
CONST_ENTRY(24)	/* "pkcs7-signedAndEnvelopedData" */
CONST_ENTRY(22)	/* "pkcs7-signedData" */
CONST_ENTRY(151)	/* "pkcs8ShroudedKeyBag" */
CONST_ENTRY(47)	/* "pkcs9" */
CONST_ENTRY(862)	/* "postOfficeBox" */
CONST_ENTRY(861)	/* "postalAddress" */
CONST_ENTRY(661)	/* "postalCode" */
CONST_ENTRY(683)	/* "ppBasis" */
CONST_ENTRY(872)	/* "preferredDeliveryMethod" */
CONST_ENTRY(873)	/* "presentationAddress" */
CONST_ENTRY(406)	/* "prime-field" */
CONST_ENTRY(409)	/* "prime192v1" */
CONST_ENTRY(410)	/* "prime192v2" */
CONST_ENTRY(411)	/* "prime192v3" */
CONST_ENTRY(412)	/* "prime239v1" */
CONST_ENTRY(413)	/* "prime239v2" */
CONST_ENTRY(414)	/* "prime239v3" */
CONST_ENTRY(415)	/* "prime256v1" */
CONST_ENTRY(886)	/* "protocolInformation" */
CONST_ENTRY(510)	/* "pseudonym" */
CONST_ENTRY(435)	/* "pss" */
CONST_ENTRY(286)	/* "qcStatements" */
CONST_ENTRY(457)	/* "qualityLabelledData" */
CONST_ENTRY(450)	/* "rFC822localPart" */
CONST_ENTRY(98)	/* "rc2-40-cbc" */
CONST_ENTRY(166)	/* "rc2-64-cbc" */
CONST_ENTRY(37)	/* "rc2-cbc" */
CONST_ENTRY(39)	/* "rc2-cfb" */
CONST_ENTRY(38)	/* "rc2-ecb" */
CONST_ENTRY(40)	/* "rc2-ofb" */
CONST_ENTRY( 5)	/* "rc4" */
CONST_ENTRY(97)	/* "rc4-40" */
CONST_ENTRY(120)	/* "rc5-cbc" */
CONST_ENTRY(122)	/* "rc5-cfb" */
CONST_ENTRY(121)	/* "rc5-ecb" */
CONST_ENTRY(123)	/* "rc5-ofb" */
CONST_ENTRY(870)	/* "registeredAddress" */
CONST_ENTRY(460)	/* "rfc822Mailbox" */
CONST_ENTRY(117)	/* "ripemd160" */
CONST_ENTRY(119)	/* "ripemd160WithRSA" */
CONST_ENTRY(400)	/* "role" */
CONST_ENTRY(877)	/* "roleOccupant" */
CONST_ENTRY(448)	/* "room" */
CONST_ENTRY(463)	/* "roomNumber" */
CONST_ENTRY(19)	/* "rsa" */
CONST_ENTRY( 6)	/* "rsaEncryption" */
CONST_ENTRY(644)	/* "rsaOAEPEncryptionSET" */
CONST_ENTRY(377)	/* "rsaSignature" */
CONST_ENTRY(124)	/* "run length compression" */
CONST_ENTRY(482)	/* "sOARecord" */
CONST_ENTRY(155)	/* "safeContentsBag" */
CONST_ENTRY(291)	/* "sbgp-autonomousSysNum" */
CONST_ENTRY(290)	/* "sbgp-ipAddrBlock" */
CONST_ENTRY(292)	/* "sbgp-routerIdentifier" */
CONST_ENTRY(159)	/* "sdsiCertificate" */
CONST_ENTRY(859)	/* "searchGuide" */
CONST_ENTRY(704)	/* "secp112r1" */
CONST_ENTRY(705)	/* "secp112r2" */
CONST_ENTRY(706)	/* "secp128r1" */
CONST_ENTRY(707)	/* "secp128r2" */
CONST_ENTRY(708)	/* "secp160k1" */
CONST_ENTRY(709)	/* "secp160r1" */
CONST_ENTRY(710)	/* "secp160r2" */
CONST_ENTRY(711)	/* "secp192k1" */
CONST_ENTRY(712)	/* "secp224k1" */
CONST_ENTRY(713)	/* "secp224r1" */
CONST_ENTRY(714)	/* "secp256k1" */
CONST_ENTRY(715)	/* "secp384r1" */
CONST_ENTRY(716)	/* "secp521r1" */
CONST_ENTRY(154)	/* "secretBag" */
CONST_ENTRY(474)	/* "secretary" */
CONST_ENTRY(717)	/* "sect113r1" */
CONST_ENTRY(718)	/* "sect113r2" */
CONST_ENTRY(719)	/* "sect131r1" */
CONST_ENTRY(720)	/* "sect131r2" */
CONST_ENTRY(721)	/* "sect163k1" */
CONST_ENTRY(722)	/* "sect163r1" */
CONST_ENTRY(723)	/* "sect163r2" */
CONST_ENTRY(724)	/* "sect193r1" */
CONST_ENTRY(725)	/* "sect193r2" */
CONST_ENTRY(726)	/* "sect233k1" */
CONST_ENTRY(727)	/* "sect233r1" */
CONST_ENTRY(728)	/* "sect239k1" */
CONST_ENTRY(729)	/* "sect283k1" */
CONST_ENTRY(730)	/* "sect283r1" */
CONST_ENTRY(731)	/* "sect409k1" */
CONST_ENTRY(732)	/* "sect409r1" */
CONST_ENTRY(733)	/* "sect571k1" */
CONST_ENTRY(734)	/* "sect571r1" */
CONST_ENTRY(635)	/* "secure device signature" */
CONST_ENTRY(878)	/* "seeAlso" */
CONST_ENTRY(777)	/* "seed-cbc" */
CONST_ENTRY(779)	/* "seed-cfb" */
CONST_ENTRY(776)	/* "seed-ecb" */
CONST_ENTRY(778)	/* "seed-ofb" */
CONST_ENTRY(105)	/* "serialNumber" */
CONST_ENTRY(625)	/* "set-addPolicy" */
CONST_ENTRY(515)	/* "set-attr" */
CONST_ENTRY(518)	/* "set-brand" */
CONST_ENTRY(638)	/* "set-brand-AmericanExpress" */
CONST_ENTRY(637)	/* "set-brand-Diners" */
CONST_ENTRY(636)	/* "set-brand-IATA-ATA" */
CONST_ENTRY(639)	/* "set-brand-JCB" */
CONST_ENTRY(641)	/* "set-brand-MasterCard" */
CONST_ENTRY(642)	/* "set-brand-Novus" */
CONST_ENTRY(640)	/* "set-brand-Visa" */
CONST_ENTRY(516)	/* "set-policy" */
CONST_ENTRY(607)	/* "set-policy-root" */
CONST_ENTRY(624)	/* "set-rootKeyThumb" */
CONST_ENTRY(620)	/* "setAttr-Cert" */
CONST_ENTRY(628)	/* "setAttr-IssCap-CVM" */
CONST_ENTRY(630)	/* "setAttr-IssCap-Sig" */
CONST_ENTRY(629)	/* "setAttr-IssCap-T2" */
CONST_ENTRY(627)	/* "setAttr-Token-B0Prime" */
CONST_ENTRY(626)	/* "setAttr-Token-EMV" */
CONST_ENTRY(622)	/* "setAttr-TokenType" */
CONST_ENTRY(619)	/* "setCext-IssuerCapabilities" */
CONST_ENTRY(615)	/* "setCext-PGWYcapabilities" */
CONST_ENTRY(616)	/* "setCext-TokenIdentifier" */
CONST_ENTRY(618)	/* "setCext-TokenType" */
CONST_ENTRY(617)	/* "setCext-Track2Data" */
CONST_ENTRY(611)	/* "setCext-cCertRequired" */
CONST_ENTRY(609)	/* "setCext-certType" */
CONST_ENTRY(608)	/* "setCext-hashedRoot" */
CONST_ENTRY(610)	/* "setCext-merchData" */
CONST_ENTRY(613)	/* "setCext-setExt" */
CONST_ENTRY(614)	/* "setCext-setQualf" */
CONST_ENTRY(612)	/* "setCext-tunneling" */
CONST_ENTRY(540)	/* "setct-AcqCardCodeMsg" */
CONST_ENTRY(576)	/* "setct-AcqCardCodeMsgTBE" */
CONST_ENTRY(570)	/* "setct-AuthReqTBE" */
CONST_ENTRY(534)	/* "setct-AuthReqTBS" */
CONST_ENTRY(527)	/* "setct-AuthResBaggage" */
CONST_ENTRY(571)	/* "setct-AuthResTBE" */
CONST_ENTRY(572)	/* "setct-AuthResTBEX" */
CONST_ENTRY(535)	/* "setct-AuthResTBS" */
CONST_ENTRY(536)	/* "setct-AuthResTBSX" */
CONST_ENTRY(528)	/* "setct-AuthRevReqBaggage" */
CONST_ENTRY(577)	/* "setct-AuthRevReqTBE" */
CONST_ENTRY(541)	/* "setct-AuthRevReqTBS" */
CONST_ENTRY(529)	/* "setct-AuthRevResBaggage" */
CONST_ENTRY(542)	/* "setct-AuthRevResData" */
CONST_ENTRY(578)	/* "setct-AuthRevResTBE" */
CONST_ENTRY(579)	/* "setct-AuthRevResTBEB" */
CONST_ENTRY(543)	/* "setct-AuthRevResTBS" */
CONST_ENTRY(573)	/* "setct-AuthTokenTBE" */
CONST_ENTRY(537)	/* "setct-AuthTokenTBS" */
CONST_ENTRY(600)	/* "setct-BCIDistributionTBS" */
CONST_ENTRY(558)	/* "setct-BatchAdminReqData" */
CONST_ENTRY(592)	/* "setct-BatchAdminReqTBE" */
CONST_ENTRY(559)	/* "setct-BatchAdminResData" */
CONST_ENTRY(593)	/* "setct-BatchAdminResTBE" */
CONST_ENTRY(599)	/* "setct-CRLNotificationResTBS" */
CONST_ENTRY(598)	/* "setct-CRLNotificationTBS" */
CONST_ENTRY(580)	/* "setct-CapReqTBE" */
CONST_ENTRY(581)	/* "setct-CapReqTBEX" */
CONST_ENTRY(544)	/* "setct-CapReqTBS" */
CONST_ENTRY(545)	/* "setct-CapReqTBSX" */
CONST_ENTRY(546)	/* "setct-CapResData" */
CONST_ENTRY(582)	/* "setct-CapResTBE" */
CONST_ENTRY(583)	/* "setct-CapRevReqTBE" */
CONST_ENTRY(584)	/* "setct-CapRevReqTBEX" */
CONST_ENTRY(547)	/* "setct-CapRevReqTBS" */
CONST_ENTRY(548)	/* "setct-CapRevReqTBSX" */
CONST_ENTRY(549)	/* "setct-CapRevResData" */
CONST_ENTRY(585)	/* "setct-CapRevResTBE" */
CONST_ENTRY(538)	/* "setct-CapTokenData" */
CONST_ENTRY(530)	/* "setct-CapTokenSeq" */
CONST_ENTRY(574)	/* "setct-CapTokenTBE" */
CONST_ENTRY(575)	/* "setct-CapTokenTBEX" */
CONST_ENTRY(539)	/* "setct-CapTokenTBS" */
CONST_ENTRY(560)	/* "setct-CardCInitResTBS" */
CONST_ENTRY(566)	/* "setct-CertInqReqTBS" */
CONST_ENTRY(563)	/* "setct-CertReqData" */
CONST_ENTRY(595)	/* "setct-CertReqTBE" */
CONST_ENTRY(596)	/* "setct-CertReqTBEX" */
CONST_ENTRY(564)	/* "setct-CertReqTBS" */
CONST_ENTRY(565)	/* "setct-CertResData" */
CONST_ENTRY(597)	/* "setct-CertResTBE" */
CONST_ENTRY(586)	/* "setct-CredReqTBE" */
CONST_ENTRY(587)	/* "setct-CredReqTBEX" */
CONST_ENTRY(550)	/* "setct-CredReqTBS" */
CONST_ENTRY(551)	/* "setct-CredReqTBSX" */
CONST_ENTRY(552)	/* "setct-CredResData" */
CONST_ENTRY(588)	/* "setct-CredResTBE" */
CONST_ENTRY(589)	/* "setct-CredRevReqTBE" */
CONST_ENTRY(590)	/* "setct-CredRevReqTBEX" */
CONST_ENTRY(553)	/* "setct-CredRevReqTBS" */
CONST_ENTRY(554)	/* "setct-CredRevReqTBSX" */
CONST_ENTRY(555)	/* "setct-CredRevResData" */
CONST_ENTRY(591)	/* "setct-CredRevResTBE" */
CONST_ENTRY(567)	/* "setct-ErrorTBS" */
CONST_ENTRY(526)	/* "setct-HODInput" */
CONST_ENTRY(561)	/* "setct-MeAqCInitResTBS" */
CONST_ENTRY(522)	/* "setct-OIData" */
CONST_ENTRY(519)	/* "setct-PANData" */
CONST_ENTRY(521)	/* "setct-PANOnly" */
CONST_ENTRY(520)	/* "setct-PANToken" */
CONST_ENTRY(556)	/* "setct-PCertReqData" */
CONST_ENTRY(557)	/* "setct-PCertResTBS" */
CONST_ENTRY(523)	/* "setct-PI" */
CONST_ENTRY(532)	/* "setct-PI-TBS" */
CONST_ENTRY(524)	/* "setct-PIData" */
CONST_ENTRY(525)	/* "setct-PIDataUnsigned" */
CONST_ENTRY(568)	/* "setct-PIDualSignedTBE" */
CONST_ENTRY(569)	/* "setct-PIUnsignedTBE" */
CONST_ENTRY(531)	/* "setct-PInitResData" */
CONST_ENTRY(533)	/* "setct-PResData" */
CONST_ENTRY(594)	/* "setct-RegFormReqTBE" */
CONST_ENTRY(562)	/* "setct-RegFormResTBS" */
CONST_ENTRY(604)	/* "setext-pinAny" */
CONST_ENTRY(603)	/* "setext-pinSecure" */
CONST_ENTRY(605)	/* "setext-track2" */
CONST_ENTRY(41)	/* "sha" */
CONST_ENTRY(64)	/* "sha1" */
CONST_ENTRY(115)	/* "sha1WithRSA" */
CONST_ENTRY(65)	/* "sha1WithRSAEncryption" */
CONST_ENTRY(675)	/* "sha224" */
CONST_ENTRY(671)	/* "sha224WithRSAEncryption" */
CONST_ENTRY(672)	/* "sha256" */
CONST_ENTRY(668)	/* "sha256WithRSAEncryption" */
CONST_ENTRY(673)	/* "sha384" */
CONST_ENTRY(669)	/* "sha384WithRSAEncryption" */
CONST_ENTRY(674)	/* "sha512" */
CONST_ENTRY(670)	/* "sha512WithRSAEncryption" */
CONST_ENTRY(42)	/* "shaWithRSAEncryption" */
CONST_ENTRY(52)	/* "signingTime" */
CONST_ENTRY(454)	/* "simpleSecurityObject" */
CONST_ENTRY(496)	/* "singleLevelQuality" */
CONST_ENTRY(16)	/* "stateOrProvinceName" */
CONST_ENTRY(660)	/* "streetAddress" */
CONST_ENTRY(498)	/* "subtreeMaximumQuality" */
CONST_ENTRY(497)	/* "subtreeMinimumQuality" */
CONST_ENTRY(890)	/* "supportedAlgorithms" */
CONST_ENTRY(874)	/* "supportedApplicationContext" */
CONST_ENTRY(100)	/* "surname" */
CONST_ENTRY(864)	/* "telephoneNumber" */
CONST_ENTRY(866)	/* "teletexTerminalIdentifier" */
CONST_ENTRY(865)	/* "telexNumber" */
CONST_ENTRY(459)	/* "textEncodedORAddress" */
CONST_ENTRY(293)	/* "textNotice" */
CONST_ENTRY(106)	/* "title" */
CONST_ENTRY(682)	/* "tpBasis" */
CONST_ENTRY(436)	/* "ucl" */
CONST_ENTRY( 0)	/* "undefined" */
CONST_ENTRY(888)	/* "uniqueMember" */
CONST_ENTRY(55)	/* "unstructuredAddress" */
CONST_ENTRY(49)	/* "unstructuredName" */
CONST_ENTRY(880)	/* "userCertificate" */
CONST_ENTRY(465)	/* "userClass" */
CONST_ENTRY(458)	/* "userId" */
CONST_ENTRY(879)	/* "userPassword" */
CONST_ENTRY(373)	/* "valid" */
CONST_ENTRY(678)	/* "wap" */
CONST_ENTRY(679)	/* "wap-wsg" */
CONST_ENTRY(735)	/* "wap-wsg-idm-ecid-wtls1" */
CONST_ENTRY(743)	/* "wap-wsg-idm-ecid-wtls10" */
CONST_ENTRY(744)	/* "wap-wsg-idm-ecid-wtls11" */
CONST_ENTRY(745)	/* "wap-wsg-idm-ecid-wtls12" */
CONST_ENTRY(736)	/* "wap-wsg-idm-ecid-wtls3" */
CONST_ENTRY(737)	/* "wap-wsg-idm-ecid-wtls4" */
CONST_ENTRY(738)	/* "wap-wsg-idm-ecid-wtls5" */
CONST_ENTRY(739)	/* "wap-wsg-idm-ecid-wtls6" */
CONST_ENTRY(740)	/* "wap-wsg-idm-ecid-wtls7" */
CONST_ENTRY(741)	/* "wap-wsg-idm-ecid-wtls8" */
CONST_ENTRY(742)	/* "wap-wsg-idm-ecid-wtls9" */
CONST_ENTRY(804)	/* "whirlpool" */
CONST_ENTRY(868)	/* "x121Address" */
CONST_ENTRY(503)	/* "x500UniqueIdentifier" */
CONST_ENTRY(158)	/* "x509Certificate" */
CONST_ENTRY(160)	/* "x509Crl" */
CONST_ENTRY(125)	/* "zlib compression" */
CONST_END(ln_objs)
#endif // !OPERA_SMALL_VERSION

OPENSSL_PREFIX_CONST_ARRAY(static, unsigned int, obj_objs, libopeay)
CONST_ENTRY( 0)	/* OBJ_undef                        0 */
CONST_ENTRY(393)	/* OBJ_joint_iso_ccitt              OBJ_joint_iso_itu_t */
CONST_ENTRY(404)	/* OBJ_ccitt                        OBJ_itu_t */
CONST_ENTRY(645)	/* OBJ_itu_t                        0 */
CONST_ENTRY(434)	/* OBJ_data                         0 9 */
CONST_ENTRY(181)	/* OBJ_iso                          1 */
CONST_ENTRY(182)	/* OBJ_member_body                  1 2 */
CONST_ENTRY(379)	/* OBJ_org                          1 3 */
CONST_ENTRY(676)	/* OBJ_identified_organization      1 3 */
CONST_ENTRY(646)	/* OBJ_joint_iso_itu_t              2 */
CONST_ENTRY(11)	/* OBJ_X500                         2 5 */
CONST_ENTRY(647)	/* OBJ_international_organizations  2 23 */
CONST_ENTRY(380)	/* OBJ_dod                          1 3 6 */
CONST_ENTRY(12)	/* OBJ_X509                         2 5 4 */
CONST_ENTRY(378)	/* OBJ_X500algorithms               2 5 8 */
CONST_ENTRY(81)	/* OBJ_id_ce                        2 5 29 */
CONST_ENTRY(512)	/* OBJ_id_set                       2 23 42 */
CONST_ENTRY(678)	/* OBJ_wap                          2 23 43 */
CONST_ENTRY(435)	/* OBJ_pss                          0 9 2342 */
CONST_ENTRY(183)	/* OBJ_ISO_US                       1 2 840 */
CONST_ENTRY(381)	/* OBJ_iana                         1 3 6 1 */
CONST_ENTRY(677)	/* OBJ_certicom_arc                 1 3 132 */
CONST_ENTRY(394)	/* OBJ_selected_attribute_types     2 5 1 5 */
CONST_ENTRY(13)	/* OBJ_commonName                   2 5 4 3 */
CONST_ENTRY(100)	/* OBJ_surname                      2 5 4 4 */
CONST_ENTRY(105)	/* OBJ_serialNumber                 2 5 4 5 */
CONST_ENTRY(14)	/* OBJ_countryName                  2 5 4 6 */
CONST_ENTRY(15)	/* OBJ_localityName                 2 5 4 7 */
CONST_ENTRY(16)	/* OBJ_stateOrProvinceName          2 5 4 8 */
CONST_ENTRY(660)	/* OBJ_streetAddress                2 5 4 9 */
CONST_ENTRY(17)	/* OBJ_organizationName             2 5 4 10 */
CONST_ENTRY(18)	/* OBJ_organizationalUnitName       2 5 4 11 */
CONST_ENTRY(106)	/* OBJ_title                        2 5 4 12 */
CONST_ENTRY(107)	/* OBJ_description                  2 5 4 13 */
CONST_ENTRY(859)	/* OBJ_searchGuide                  2 5 4 14 */
CONST_ENTRY(860)	/* OBJ_businessCategory             2 5 4 15 */
CONST_ENTRY(861)	/* OBJ_postalAddress                2 5 4 16 */
CONST_ENTRY(661)	/* OBJ_postalCode                   2 5 4 17 */
CONST_ENTRY(862)	/* OBJ_postOfficeBox                2 5 4 18 */
CONST_ENTRY(863)	/* OBJ_physicalDeliveryOfficeName   2 5 4 19 */
CONST_ENTRY(864)	/* OBJ_telephoneNumber              2 5 4 20 */
CONST_ENTRY(865)	/* OBJ_telexNumber                  2 5 4 21 */
CONST_ENTRY(866)	/* OBJ_teletexTerminalIdentifier    2 5 4 22 */
CONST_ENTRY(867)	/* OBJ_facsimileTelephoneNumber     2 5 4 23 */
CONST_ENTRY(868)	/* OBJ_x121Address                  2 5 4 24 */
CONST_ENTRY(869)	/* OBJ_internationaliSDNNumber      2 5 4 25 */
CONST_ENTRY(870)	/* OBJ_registeredAddress            2 5 4 26 */
CONST_ENTRY(871)	/* OBJ_destinationIndicator         2 5 4 27 */
CONST_ENTRY(872)	/* OBJ_preferredDeliveryMethod      2 5 4 28 */
CONST_ENTRY(873)	/* OBJ_presentationAddress          2 5 4 29 */
CONST_ENTRY(874)	/* OBJ_supportedApplicationContext  2 5 4 30 */
CONST_ENTRY(875)	/* OBJ_member                       2 5 4 31 */
CONST_ENTRY(876)	/* OBJ_owner                        2 5 4 32 */
CONST_ENTRY(877)	/* OBJ_roleOccupant                 2 5 4 33 */
CONST_ENTRY(878)	/* OBJ_seeAlso                      2 5 4 34 */
CONST_ENTRY(879)	/* OBJ_userPassword                 2 5 4 35 */
CONST_ENTRY(880)	/* OBJ_userCertificate              2 5 4 36 */
CONST_ENTRY(881)	/* OBJ_cACertificate                2 5 4 37 */
CONST_ENTRY(882)	/* OBJ_authorityRevocationList      2 5 4 38 */
CONST_ENTRY(883)	/* OBJ_certificateRevocationList    2 5 4 39 */
CONST_ENTRY(884)	/* OBJ_crossCertificatePair         2 5 4 40 */
CONST_ENTRY(173)	/* OBJ_name                         2 5 4 41 */
CONST_ENTRY(99)	/* OBJ_givenName                    2 5 4 42 */
CONST_ENTRY(101)	/* OBJ_initials                     2 5 4 43 */
CONST_ENTRY(509)	/* OBJ_generationQualifier          2 5 4 44 */
CONST_ENTRY(503)	/* OBJ_x500UniqueIdentifier         2 5 4 45 */
CONST_ENTRY(174)	/* OBJ_dnQualifier                  2 5 4 46 */
CONST_ENTRY(885)	/* OBJ_enhancedSearchGuide          2 5 4 47 */
CONST_ENTRY(886)	/* OBJ_protocolInformation          2 5 4 48 */
CONST_ENTRY(887)	/* OBJ_distinguishedName            2 5 4 49 */
CONST_ENTRY(888)	/* OBJ_uniqueMember                 2 5 4 50 */
CONST_ENTRY(889)	/* OBJ_houseIdentifier              2 5 4 51 */
CONST_ENTRY(890)	/* OBJ_supportedAlgorithms          2 5 4 52 */
CONST_ENTRY(891)	/* OBJ_deltaRevocationList          2 5 4 53 */
CONST_ENTRY(892)	/* OBJ_dmdName                      2 5 4 54 */
CONST_ENTRY(510)	/* OBJ_pseudonym                    2 5 4 65 */
CONST_ENTRY(400)	/* OBJ_role                         2 5 4 72 */
CONST_ENTRY(769)	/* OBJ_subject_directory_attributes 2 5 29 9 */
CONST_ENTRY(82)	/* OBJ_subject_key_identifier       2 5 29 14 */
CONST_ENTRY(83)	/* OBJ_key_usage                    2 5 29 15 */
CONST_ENTRY(84)	/* OBJ_private_key_usage_period     2 5 29 16 */
CONST_ENTRY(85)	/* OBJ_subject_alt_name             2 5 29 17 */
CONST_ENTRY(86)	/* OBJ_issuer_alt_name              2 5 29 18 */
CONST_ENTRY(87)	/* OBJ_basic_constraints            2 5 29 19 */
CONST_ENTRY(88)	/* OBJ_crl_number                   2 5 29 20 */
CONST_ENTRY(141)	/* OBJ_crl_reason                   2 5 29 21 */
CONST_ENTRY(430)	/* OBJ_hold_instruction_code        2 5 29 23 */
CONST_ENTRY(142)	/* OBJ_invalidity_date              2 5 29 24 */
CONST_ENTRY(140)	/* OBJ_delta_crl                    2 5 29 27 */
CONST_ENTRY(770)	/* OBJ_issuing_distribution_point   2 5 29 28 */
CONST_ENTRY(771)	/* OBJ_certificate_issuer           2 5 29 29 */
CONST_ENTRY(666)	/* OBJ_name_constraints             2 5 29 30 */
CONST_ENTRY(103)	/* OBJ_crl_distribution_points      2 5 29 31 */
CONST_ENTRY(89)	/* OBJ_certificate_policies         2 5 29 32 */
CONST_ENTRY(747)	/* OBJ_policy_mappings              2 5 29 33 */
CONST_ENTRY(90)	/* OBJ_authority_key_identifier     2 5 29 35 */
CONST_ENTRY(401)	/* OBJ_policy_constraints           2 5 29 36 */
CONST_ENTRY(126)	/* OBJ_ext_key_usage                2 5 29 37 */
CONST_ENTRY(857)	/* OBJ_freshest_crl                 2 5 29 46 */
CONST_ENTRY(748)	/* OBJ_inhibit_any_policy           2 5 29 54 */
CONST_ENTRY(402)	/* OBJ_target_information           2 5 29 55 */
CONST_ENTRY(403)	/* OBJ_no_rev_avail                 2 5 29 56 */
CONST_ENTRY(513)	/* OBJ_set_ctype                    2 23 42 0 */
CONST_ENTRY(514)	/* OBJ_set_msgExt                   2 23 42 1 */
CONST_ENTRY(515)	/* OBJ_set_attr                     2 23 42 3 */
CONST_ENTRY(516)	/* OBJ_set_policy                   2 23 42 5 */
CONST_ENTRY(517)	/* OBJ_set_certExt                  2 23 42 7 */
CONST_ENTRY(518)	/* OBJ_set_brand                    2 23 42 8 */
CONST_ENTRY(679)	/* OBJ_wap_wsg                      2 23 43 1 */
CONST_ENTRY(382)	/* OBJ_Directory                    1 3 6 1 1 */
CONST_ENTRY(383)	/* OBJ_Management                   1 3 6 1 2 */
CONST_ENTRY(384)	/* OBJ_Experimental                 1 3 6 1 3 */
CONST_ENTRY(385)	/* OBJ_Private                      1 3 6 1 4 */
CONST_ENTRY(386)	/* OBJ_Security                     1 3 6 1 5 */
CONST_ENTRY(387)	/* OBJ_SNMPv2                       1 3 6 1 6 */
CONST_ENTRY(388)	/* OBJ_Mail                         1 3 6 1 7 */
CONST_ENTRY(376)	/* OBJ_algorithm                    1 3 14 3 2 */
CONST_ENTRY(395)	/* OBJ_clearance                    2 5 1 5 55 */
CONST_ENTRY(19)	/* OBJ_rsa                          2 5 8 1 1 */
CONST_ENTRY(96)	/* OBJ_mdc2WithRSA                  2 5 8 3 100 */
CONST_ENTRY(95)	/* OBJ_mdc2                         2 5 8 3 101 */
CONST_ENTRY(746)	/* OBJ_any_policy                   2 5 29 32 0 */
CONST_ENTRY(519)	/* OBJ_setct_PANData                2 23 42 0 0 */
CONST_ENTRY(520)	/* OBJ_setct_PANToken               2 23 42 0 1 */
CONST_ENTRY(521)	/* OBJ_setct_PANOnly                2 23 42 0 2 */
CONST_ENTRY(522)	/* OBJ_setct_OIData                 2 23 42 0 3 */
CONST_ENTRY(523)	/* OBJ_setct_PI                     2 23 42 0 4 */
CONST_ENTRY(524)	/* OBJ_setct_PIData                 2 23 42 0 5 */
CONST_ENTRY(525)	/* OBJ_setct_PIDataUnsigned         2 23 42 0 6 */
CONST_ENTRY(526)	/* OBJ_setct_HODInput               2 23 42 0 7 */
CONST_ENTRY(527)	/* OBJ_setct_AuthResBaggage         2 23 42 0 8 */
CONST_ENTRY(528)	/* OBJ_setct_AuthRevReqBaggage      2 23 42 0 9 */
CONST_ENTRY(529)	/* OBJ_setct_AuthRevResBaggage      2 23 42 0 10 */
CONST_ENTRY(530)	/* OBJ_setct_CapTokenSeq            2 23 42 0 11 */
CONST_ENTRY(531)	/* OBJ_setct_PInitResData           2 23 42 0 12 */
CONST_ENTRY(532)	/* OBJ_setct_PI_TBS                 2 23 42 0 13 */
CONST_ENTRY(533)	/* OBJ_setct_PResData               2 23 42 0 14 */
CONST_ENTRY(534)	/* OBJ_setct_AuthReqTBS             2 23 42 0 16 */
CONST_ENTRY(535)	/* OBJ_setct_AuthResTBS             2 23 42 0 17 */
CONST_ENTRY(536)	/* OBJ_setct_AuthResTBSX            2 23 42 0 18 */
CONST_ENTRY(537)	/* OBJ_setct_AuthTokenTBS           2 23 42 0 19 */
CONST_ENTRY(538)	/* OBJ_setct_CapTokenData           2 23 42 0 20 */
CONST_ENTRY(539)	/* OBJ_setct_CapTokenTBS            2 23 42 0 21 */
CONST_ENTRY(540)	/* OBJ_setct_AcqCardCodeMsg         2 23 42 0 22 */
CONST_ENTRY(541)	/* OBJ_setct_AuthRevReqTBS          2 23 42 0 23 */
CONST_ENTRY(542)	/* OBJ_setct_AuthRevResData         2 23 42 0 24 */
CONST_ENTRY(543)	/* OBJ_setct_AuthRevResTBS          2 23 42 0 25 */
CONST_ENTRY(544)	/* OBJ_setct_CapReqTBS              2 23 42 0 26 */
CONST_ENTRY(545)	/* OBJ_setct_CapReqTBSX             2 23 42 0 27 */
CONST_ENTRY(546)	/* OBJ_setct_CapResData             2 23 42 0 28 */
CONST_ENTRY(547)	/* OBJ_setct_CapRevReqTBS           2 23 42 0 29 */
CONST_ENTRY(548)	/* OBJ_setct_CapRevReqTBSX          2 23 42 0 30 */
CONST_ENTRY(549)	/* OBJ_setct_CapRevResData          2 23 42 0 31 */
CONST_ENTRY(550)	/* OBJ_setct_CredReqTBS             2 23 42 0 32 */
CONST_ENTRY(551)	/* OBJ_setct_CredReqTBSX            2 23 42 0 33 */
CONST_ENTRY(552)	/* OBJ_setct_CredResData            2 23 42 0 34 */
CONST_ENTRY(553)	/* OBJ_setct_CredRevReqTBS          2 23 42 0 35 */
CONST_ENTRY(554)	/* OBJ_setct_CredRevReqTBSX         2 23 42 0 36 */
CONST_ENTRY(555)	/* OBJ_setct_CredRevResData         2 23 42 0 37 */
CONST_ENTRY(556)	/* OBJ_setct_PCertReqData           2 23 42 0 38 */
CONST_ENTRY(557)	/* OBJ_setct_PCertResTBS            2 23 42 0 39 */
CONST_ENTRY(558)	/* OBJ_setct_BatchAdminReqData      2 23 42 0 40 */
CONST_ENTRY(559)	/* OBJ_setct_BatchAdminResData      2 23 42 0 41 */
CONST_ENTRY(560)	/* OBJ_setct_CardCInitResTBS        2 23 42 0 42 */
CONST_ENTRY(561)	/* OBJ_setct_MeAqCInitResTBS        2 23 42 0 43 */
CONST_ENTRY(562)	/* OBJ_setct_RegFormResTBS          2 23 42 0 44 */
CONST_ENTRY(563)	/* OBJ_setct_CertReqData            2 23 42 0 45 */
CONST_ENTRY(564)	/* OBJ_setct_CertReqTBS             2 23 42 0 46 */
CONST_ENTRY(565)	/* OBJ_setct_CertResData            2 23 42 0 47 */
CONST_ENTRY(566)	/* OBJ_setct_CertInqReqTBS          2 23 42 0 48 */
CONST_ENTRY(567)	/* OBJ_setct_ErrorTBS               2 23 42 0 49 */
CONST_ENTRY(568)	/* OBJ_setct_PIDualSignedTBE        2 23 42 0 50 */
CONST_ENTRY(569)	/* OBJ_setct_PIUnsignedTBE          2 23 42 0 51 */
CONST_ENTRY(570)	/* OBJ_setct_AuthReqTBE             2 23 42 0 52 */
CONST_ENTRY(571)	/* OBJ_setct_AuthResTBE             2 23 42 0 53 */
CONST_ENTRY(572)	/* OBJ_setct_AuthResTBEX            2 23 42 0 54 */
CONST_ENTRY(573)	/* OBJ_setct_AuthTokenTBE           2 23 42 0 55 */
CONST_ENTRY(574)	/* OBJ_setct_CapTokenTBE            2 23 42 0 56 */
CONST_ENTRY(575)	/* OBJ_setct_CapTokenTBEX           2 23 42 0 57 */
CONST_ENTRY(576)	/* OBJ_setct_AcqCardCodeMsgTBE      2 23 42 0 58 */
CONST_ENTRY(577)	/* OBJ_setct_AuthRevReqTBE          2 23 42 0 59 */
CONST_ENTRY(578)	/* OBJ_setct_AuthRevResTBE          2 23 42 0 60 */
CONST_ENTRY(579)	/* OBJ_setct_AuthRevResTBEB         2 23 42 0 61 */
CONST_ENTRY(580)	/* OBJ_setct_CapReqTBE              2 23 42 0 62 */
CONST_ENTRY(581)	/* OBJ_setct_CapReqTBEX             2 23 42 0 63 */
CONST_ENTRY(582)	/* OBJ_setct_CapResTBE              2 23 42 0 64 */
CONST_ENTRY(583)	/* OBJ_setct_CapRevReqTBE           2 23 42 0 65 */
CONST_ENTRY(584)	/* OBJ_setct_CapRevReqTBEX          2 23 42 0 66 */
CONST_ENTRY(585)	/* OBJ_setct_CapRevResTBE           2 23 42 0 67 */
CONST_ENTRY(586)	/* OBJ_setct_CredReqTBE             2 23 42 0 68 */
CONST_ENTRY(587)	/* OBJ_setct_CredReqTBEX            2 23 42 0 69 */
CONST_ENTRY(588)	/* OBJ_setct_CredResTBE             2 23 42 0 70 */
CONST_ENTRY(589)	/* OBJ_setct_CredRevReqTBE          2 23 42 0 71 */
CONST_ENTRY(590)	/* OBJ_setct_CredRevReqTBEX         2 23 42 0 72 */
CONST_ENTRY(591)	/* OBJ_setct_CredRevResTBE          2 23 42 0 73 */
CONST_ENTRY(592)	/* OBJ_setct_BatchAdminReqTBE       2 23 42 0 74 */
CONST_ENTRY(593)	/* OBJ_setct_BatchAdminResTBE       2 23 42 0 75 */
CONST_ENTRY(594)	/* OBJ_setct_RegFormReqTBE          2 23 42 0 76 */
CONST_ENTRY(595)	/* OBJ_setct_CertReqTBE             2 23 42 0 77 */
CONST_ENTRY(596)	/* OBJ_setct_CertReqTBEX            2 23 42 0 78 */
CONST_ENTRY(597)	/* OBJ_setct_CertResTBE             2 23 42 0 79 */
CONST_ENTRY(598)	/* OBJ_setct_CRLNotificationTBS     2 23 42 0 80 */
CONST_ENTRY(599)	/* OBJ_setct_CRLNotificationResTBS  2 23 42 0 81 */
CONST_ENTRY(600)	/* OBJ_setct_BCIDistributionTBS     2 23 42 0 82 */
CONST_ENTRY(601)	/* OBJ_setext_genCrypt              2 23 42 1 1 */
CONST_ENTRY(602)	/* OBJ_setext_miAuth                2 23 42 1 3 */
CONST_ENTRY(603)	/* OBJ_setext_pinSecure             2 23 42 1 4 */
CONST_ENTRY(604)	/* OBJ_setext_pinAny                2 23 42 1 5 */
CONST_ENTRY(605)	/* OBJ_setext_track2                2 23 42 1 7 */
CONST_ENTRY(606)	/* OBJ_setext_cv                    2 23 42 1 8 */
CONST_ENTRY(620)	/* OBJ_setAttr_Cert                 2 23 42 3 0 */
CONST_ENTRY(621)	/* OBJ_setAttr_PGWYcap              2 23 42 3 1 */
CONST_ENTRY(622)	/* OBJ_setAttr_TokenType            2 23 42 3 2 */
CONST_ENTRY(623)	/* OBJ_setAttr_IssCap               2 23 42 3 3 */
CONST_ENTRY(607)	/* OBJ_set_policy_root              2 23 42 5 0 */
CONST_ENTRY(608)	/* OBJ_setCext_hashedRoot           2 23 42 7 0 */
CONST_ENTRY(609)	/* OBJ_setCext_certType             2 23 42 7 1 */
CONST_ENTRY(610)	/* OBJ_setCext_merchData            2 23 42 7 2 */
CONST_ENTRY(611)	/* OBJ_setCext_cCertRequired        2 23 42 7 3 */
CONST_ENTRY(612)	/* OBJ_setCext_tunneling            2 23 42 7 4 */
CONST_ENTRY(613)	/* OBJ_setCext_setExt               2 23 42 7 5 */
CONST_ENTRY(614)	/* OBJ_setCext_setQualf             2 23 42 7 6 */
CONST_ENTRY(615)	/* OBJ_setCext_PGWYcapabilities     2 23 42 7 7 */
CONST_ENTRY(616)	/* OBJ_setCext_TokenIdentifier      2 23 42 7 8 */
CONST_ENTRY(617)	/* OBJ_setCext_Track2Data           2 23 42 7 9 */
CONST_ENTRY(618)	/* OBJ_setCext_TokenType            2 23 42 7 10 */
CONST_ENTRY(619)	/* OBJ_setCext_IssuerCapabilities   2 23 42 7 11 */
CONST_ENTRY(636)	/* OBJ_set_brand_IATA_ATA           2 23 42 8 1 */
CONST_ENTRY(640)	/* OBJ_set_brand_Visa               2 23 42 8 4 */
CONST_ENTRY(641)	/* OBJ_set_brand_MasterCard         2 23 42 8 5 */
CONST_ENTRY(637)	/* OBJ_set_brand_Diners             2 23 42 8 30 */
CONST_ENTRY(638)	/* OBJ_set_brand_AmericanExpress    2 23 42 8 34 */
CONST_ENTRY(639)	/* OBJ_set_brand_JCB                2 23 42 8 35 */
CONST_ENTRY(805)	/* OBJ_cryptopro                    1 2 643 2 2 */
CONST_ENTRY(806)	/* OBJ_cryptocom                    1 2 643 2 9 */
CONST_ENTRY(184)	/* OBJ_X9_57                        1 2 840 10040 */
CONST_ENTRY(405)	/* OBJ_ansi_X9_62                   1 2 840 10045 */
CONST_ENTRY(389)	/* OBJ_Enterprises                  1 3 6 1 4 1 */
CONST_ENTRY(504)	/* OBJ_mime_mhs                     1 3 6 1 7 1 */
CONST_ENTRY(104)	/* OBJ_md5WithRSA                   1 3 14 3 2 3 */
CONST_ENTRY(29)	/* OBJ_des_ecb                      1 3 14 3 2 6 */
CONST_ENTRY(31)	/* OBJ_des_cbc                      1 3 14 3 2 7 */
CONST_ENTRY(45)	/* OBJ_des_ofb64                    1 3 14 3 2 8 */
CONST_ENTRY(30)	/* OBJ_des_cfb64                    1 3 14 3 2 9 */
CONST_ENTRY(377)	/* OBJ_rsaSignature                 1 3 14 3 2 11 */
CONST_ENTRY(67)	/* OBJ_dsa_2                        1 3 14 3 2 12 */
CONST_ENTRY(66)	/* OBJ_dsaWithSHA                   1 3 14 3 2 13 */
CONST_ENTRY(42)	/* OBJ_shaWithRSAEncryption         1 3 14 3 2 15 */
CONST_ENTRY(32)	/* OBJ_des_ede_ecb                  1 3 14 3 2 17 */
CONST_ENTRY(41)	/* OBJ_sha                          1 3 14 3 2 18 */
CONST_ENTRY(64)	/* OBJ_sha1                         1 3 14 3 2 26 */
CONST_ENTRY(70)	/* OBJ_dsaWithSHA1_2                1 3 14 3 2 27 */
CONST_ENTRY(115)	/* OBJ_sha1WithRSA                  1 3 14 3 2 29 */
CONST_ENTRY(117)	/* OBJ_ripemd160                    1 3 36 3 2 1 */
CONST_ENTRY(143)	/* OBJ_sxnet                        1 3 101 1 4 1 */
CONST_ENTRY(721)	/* OBJ_sect163k1                    1 3 132 0 1 */
CONST_ENTRY(722)	/* OBJ_sect163r1                    1 3 132 0 2 */
CONST_ENTRY(728)	/* OBJ_sect239k1                    1 3 132 0 3 */
CONST_ENTRY(717)	/* OBJ_sect113r1                    1 3 132 0 4 */
CONST_ENTRY(718)	/* OBJ_sect113r2                    1 3 132 0 5 */
CONST_ENTRY(704)	/* OBJ_secp112r1                    1 3 132 0 6 */
CONST_ENTRY(705)	/* OBJ_secp112r2                    1 3 132 0 7 */
CONST_ENTRY(709)	/* OBJ_secp160r1                    1 3 132 0 8 */
CONST_ENTRY(708)	/* OBJ_secp160k1                    1 3 132 0 9 */
CONST_ENTRY(714)	/* OBJ_secp256k1                    1 3 132 0 10 */
CONST_ENTRY(723)	/* OBJ_sect163r2                    1 3 132 0 15 */
CONST_ENTRY(729)	/* OBJ_sect283k1                    1 3 132 0 16 */
CONST_ENTRY(730)	/* OBJ_sect283r1                    1 3 132 0 17 */
CONST_ENTRY(719)	/* OBJ_sect131r1                    1 3 132 0 22 */
CONST_ENTRY(720)	/* OBJ_sect131r2                    1 3 132 0 23 */
CONST_ENTRY(724)	/* OBJ_sect193r1                    1 3 132 0 24 */
CONST_ENTRY(725)	/* OBJ_sect193r2                    1 3 132 0 25 */
CONST_ENTRY(726)	/* OBJ_sect233k1                    1 3 132 0 26 */
CONST_ENTRY(727)	/* OBJ_sect233r1                    1 3 132 0 27 */
CONST_ENTRY(706)	/* OBJ_secp128r1                    1 3 132 0 28 */
CONST_ENTRY(707)	/* OBJ_secp128r2                    1 3 132 0 29 */
CONST_ENTRY(710)	/* OBJ_secp160r2                    1 3 132 0 30 */
CONST_ENTRY(711)	/* OBJ_secp192k1                    1 3 132 0 31 */
CONST_ENTRY(712)	/* OBJ_secp224k1                    1 3 132 0 32 */
CONST_ENTRY(713)	/* OBJ_secp224r1                    1 3 132 0 33 */
CONST_ENTRY(715)	/* OBJ_secp384r1                    1 3 132 0 34 */
CONST_ENTRY(716)	/* OBJ_secp521r1                    1 3 132 0 35 */
CONST_ENTRY(731)	/* OBJ_sect409k1                    1 3 132 0 36 */
CONST_ENTRY(732)	/* OBJ_sect409r1                    1 3 132 0 37 */
CONST_ENTRY(733)	/* OBJ_sect571k1                    1 3 132 0 38 */
CONST_ENTRY(734)	/* OBJ_sect571r1                    1 3 132 0 39 */
CONST_ENTRY(624)	/* OBJ_set_rootKeyThumb             2 23 42 3 0 0 */
CONST_ENTRY(625)	/* OBJ_set_addPolicy                2 23 42 3 0 1 */
CONST_ENTRY(626)	/* OBJ_setAttr_Token_EMV            2 23 42 3 2 1 */
CONST_ENTRY(627)	/* OBJ_setAttr_Token_B0Prime        2 23 42 3 2 2 */
CONST_ENTRY(628)	/* OBJ_setAttr_IssCap_CVM           2 23 42 3 3 3 */
CONST_ENTRY(629)	/* OBJ_setAttr_IssCap_T2            2 23 42 3 3 4 */
CONST_ENTRY(630)	/* OBJ_setAttr_IssCap_Sig           2 23 42 3 3 5 */
CONST_ENTRY(642)	/* OBJ_set_brand_Novus              2 23 42 8 6011 */
CONST_ENTRY(735)	/* OBJ_wap_wsg_idm_ecid_wtls1       2 23 43 1 4 1 */
CONST_ENTRY(736)	/* OBJ_wap_wsg_idm_ecid_wtls3       2 23 43 1 4 3 */
CONST_ENTRY(737)	/* OBJ_wap_wsg_idm_ecid_wtls4       2 23 43 1 4 4 */
CONST_ENTRY(738)	/* OBJ_wap_wsg_idm_ecid_wtls5       2 23 43 1 4 5 */
CONST_ENTRY(739)	/* OBJ_wap_wsg_idm_ecid_wtls6       2 23 43 1 4 6 */
CONST_ENTRY(740)	/* OBJ_wap_wsg_idm_ecid_wtls7       2 23 43 1 4 7 */
CONST_ENTRY(741)	/* OBJ_wap_wsg_idm_ecid_wtls8       2 23 43 1 4 8 */
CONST_ENTRY(742)	/* OBJ_wap_wsg_idm_ecid_wtls9       2 23 43 1 4 9 */
CONST_ENTRY(743)	/* OBJ_wap_wsg_idm_ecid_wtls10      2 23 43 1 4 10 */
CONST_ENTRY(744)	/* OBJ_wap_wsg_idm_ecid_wtls11      2 23 43 1 4 11 */
CONST_ENTRY(745)	/* OBJ_wap_wsg_idm_ecid_wtls12      2 23 43 1 4 12 */
CONST_ENTRY(804)	/* OBJ_whirlpool                    1 0 10118 3 0 55 */
CONST_ENTRY(124)	/* OBJ_rle_compression              1 1 1 1 666 1 */
CONST_ENTRY(773)	/* OBJ_kisa                         1 2 410 200004 */
CONST_ENTRY(807)	/* OBJ_id_GostR3411_94_with_GostR3410_2001 1 2 643 2 2 3 */
CONST_ENTRY(808)	/* OBJ_id_GostR3411_94_with_GostR3410_94 1 2 643 2 2 4 */
CONST_ENTRY(809)	/* OBJ_id_GostR3411_94              1 2 643 2 2 9 */
CONST_ENTRY(810)	/* OBJ_id_HMACGostR3411_94          1 2 643 2 2 10 */
CONST_ENTRY(811)	/* OBJ_id_GostR3410_2001            1 2 643 2 2 19 */
CONST_ENTRY(812)	/* OBJ_id_GostR3410_94              1 2 643 2 2 20 */
CONST_ENTRY(813)	/* OBJ_id_Gost28147_89              1 2 643 2 2 21 */
CONST_ENTRY(815)	/* OBJ_id_Gost28147_89_MAC          1 2 643 2 2 22 */
CONST_ENTRY(816)	/* OBJ_id_GostR3411_94_prf          1 2 643 2 2 23 */
CONST_ENTRY(817)	/* OBJ_id_GostR3410_2001DH          1 2 643 2 2 98 */
CONST_ENTRY(818)	/* OBJ_id_GostR3410_94DH            1 2 643 2 2 99 */
CONST_ENTRY( 1)	/* OBJ_rsadsi                       1 2 840 113549 */
CONST_ENTRY(185)	/* OBJ_X9cm                         1 2 840 10040 4 */
CONST_ENTRY(127)	/* OBJ_id_pkix                      1 3 6 1 5 5 7 */
CONST_ENTRY(505)	/* OBJ_mime_mhs_headings            1 3 6 1 7 1 1 */
CONST_ENTRY(506)	/* OBJ_mime_mhs_bodies              1 3 6 1 7 1 2 */
CONST_ENTRY(119)	/* OBJ_ripemd160WithRSA             1 3 36 3 3 1 2 */
CONST_ENTRY(631)	/* OBJ_setAttr_GenCryptgrm          2 23 42 3 3 3 1 */
CONST_ENTRY(632)	/* OBJ_setAttr_T2Enc                2 23 42 3 3 4 1 */
CONST_ENTRY(633)	/* OBJ_setAttr_T2cleartxt           2 23 42 3 3 4 2 */
CONST_ENTRY(634)	/* OBJ_setAttr_TokICCsig            2 23 42 3 3 5 1 */
CONST_ENTRY(635)	/* OBJ_setAttr_SecDevSig            2 23 42 3 3 5 2 */
CONST_ENTRY(436)	/* OBJ_ucl                          0 9 2342 19200300 */
CONST_ENTRY(820)	/* OBJ_id_Gost28147_89_None_KeyMeshing 1 2 643 2 2 14 0 */
CONST_ENTRY(819)	/* OBJ_id_Gost28147_89_CryptoPro_KeyMeshing 1 2 643 2 2 14 1 */
CONST_ENTRY(845)	/* OBJ_id_GostR3410_94_a            1 2 643 2 2 20 1 */
CONST_ENTRY(846)	/* OBJ_id_GostR3410_94_aBis         1 2 643 2 2 20 2 */
CONST_ENTRY(847)	/* OBJ_id_GostR3410_94_b            1 2 643 2 2 20 3 */
CONST_ENTRY(848)	/* OBJ_id_GostR3410_94_bBis         1 2 643 2 2 20 4 */
CONST_ENTRY(821)	/* OBJ_id_GostR3411_94_TestParamSet 1 2 643 2 2 30 0 */
CONST_ENTRY(822)	/* OBJ_id_GostR3411_94_CryptoProParamSet 1 2 643 2 2 30 1 */
CONST_ENTRY(823)	/* OBJ_id_Gost28147_89_TestParamSet 1 2 643 2 2 31 0 */
CONST_ENTRY(824)	/* OBJ_id_Gost28147_89_CryptoPro_A_ParamSet 1 2 643 2 2 31 1 */
CONST_ENTRY(825)	/* OBJ_id_Gost28147_89_CryptoPro_B_ParamSet 1 2 643 2 2 31 2 */
CONST_ENTRY(826)	/* OBJ_id_Gost28147_89_CryptoPro_C_ParamSet 1 2 643 2 2 31 3 */
CONST_ENTRY(827)	/* OBJ_id_Gost28147_89_CryptoPro_D_ParamSet 1 2 643 2 2 31 4 */
CONST_ENTRY(828)	/* OBJ_id_Gost28147_89_CryptoPro_Oscar_1_1_ParamSet 1 2 643 2 2 31 5 */
CONST_ENTRY(829)	/* OBJ_id_Gost28147_89_CryptoPro_Oscar_1_0_ParamSet 1 2 643 2 2 31 6 */
CONST_ENTRY(830)	/* OBJ_id_Gost28147_89_CryptoPro_RIC_1_ParamSet 1 2 643 2 2 31 7 */
CONST_ENTRY(831)	/* OBJ_id_GostR3410_94_TestParamSet 1 2 643 2 2 32 0 */
CONST_ENTRY(832)	/* OBJ_id_GostR3410_94_CryptoPro_A_ParamSet 1 2 643 2 2 32 2 */
CONST_ENTRY(833)	/* OBJ_id_GostR3410_94_CryptoPro_B_ParamSet 1 2 643 2 2 32 3 */
CONST_ENTRY(834)	/* OBJ_id_GostR3410_94_CryptoPro_C_ParamSet 1 2 643 2 2 32 4 */
CONST_ENTRY(835)	/* OBJ_id_GostR3410_94_CryptoPro_D_ParamSet 1 2 643 2 2 32 5 */
CONST_ENTRY(836)	/* OBJ_id_GostR3410_94_CryptoPro_XchA_ParamSet 1 2 643 2 2 33 1 */
CONST_ENTRY(837)	/* OBJ_id_GostR3410_94_CryptoPro_XchB_ParamSet 1 2 643 2 2 33 2 */
CONST_ENTRY(838)	/* OBJ_id_GostR3410_94_CryptoPro_XchC_ParamSet 1 2 643 2 2 33 3 */
CONST_ENTRY(839)	/* OBJ_id_GostR3410_2001_TestParamSet 1 2 643 2 2 35 0 */
CONST_ENTRY(840)	/* OBJ_id_GostR3410_2001_CryptoPro_A_ParamSet 1 2 643 2 2 35 1 */
CONST_ENTRY(841)	/* OBJ_id_GostR3410_2001_CryptoPro_B_ParamSet 1 2 643 2 2 35 2 */
CONST_ENTRY(842)	/* OBJ_id_GostR3410_2001_CryptoPro_C_ParamSet 1 2 643 2 2 35 3 */
CONST_ENTRY(843)	/* OBJ_id_GostR3410_2001_CryptoPro_XchA_ParamSet 1 2 643 2 2 36 0 */
CONST_ENTRY(844)	/* OBJ_id_GostR3410_2001_CryptoPro_XchB_ParamSet 1 2 643 2 2 36 1 */
CONST_ENTRY( 2)	/* OBJ_pkcs                         1 2 840 113549 1 */
CONST_ENTRY(431)	/* OBJ_hold_instruction_none        1 2 840 10040 2 1 */
CONST_ENTRY(432)	/* OBJ_hold_instruction_call_issuer 1 2 840 10040 2 2 */
CONST_ENTRY(433)	/* OBJ_hold_instruction_reject      1 2 840 10040 2 3 */
CONST_ENTRY(116)	/* OBJ_dsa                          1 2 840 10040 4 1 */
CONST_ENTRY(113)	/* OBJ_dsaWithSHA1                  1 2 840 10040 4 3 */
CONST_ENTRY(406)	/* OBJ_X9_62_prime_field            1 2 840 10045 1 1 */
CONST_ENTRY(407)	/* OBJ_X9_62_characteristic_two_field 1 2 840 10045 1 2 */
CONST_ENTRY(408)	/* OBJ_X9_62_id_ecPublicKey         1 2 840 10045 2 1 */
CONST_ENTRY(416)	/* OBJ_ecdsa_with_SHA1              1 2 840 10045 4 1 */
CONST_ENTRY(791)	/* OBJ_ecdsa_with_Recommended       1 2 840 10045 4 2 */
CONST_ENTRY(792)	/* OBJ_ecdsa_with_Specified         1 2 840 10045 4 3 */
CONST_ENTRY(258)	/* OBJ_id_pkix_mod                  1 3 6 1 5 5 7 0 */
CONST_ENTRY(175)	/* OBJ_id_pe                        1 3 6 1 5 5 7 1 */
CONST_ENTRY(259)	/* OBJ_id_qt                        1 3 6 1 5 5 7 2 */
CONST_ENTRY(128)	/* OBJ_id_kp                        1 3 6 1 5 5 7 3 */
CONST_ENTRY(260)	/* OBJ_id_it                        1 3 6 1 5 5 7 4 */
CONST_ENTRY(261)	/* OBJ_id_pkip                      1 3 6 1 5 5 7 5 */
CONST_ENTRY(262)	/* OBJ_id_alg                       1 3 6 1 5 5 7 6 */
CONST_ENTRY(263)	/* OBJ_id_cmc                       1 3 6 1 5 5 7 7 */
CONST_ENTRY(264)	/* OBJ_id_on                        1 3 6 1 5 5 7 8 */
CONST_ENTRY(265)	/* OBJ_id_pda                       1 3 6 1 5 5 7 9 */
CONST_ENTRY(266)	/* OBJ_id_aca                       1 3 6 1 5 5 7 10 */
CONST_ENTRY(267)	/* OBJ_id_qcs                       1 3 6 1 5 5 7 11 */
CONST_ENTRY(268)	/* OBJ_id_cct                       1 3 6 1 5 5 7 12 */
CONST_ENTRY(662)	/* OBJ_id_ppl                       1 3 6 1 5 5 7 21 */
CONST_ENTRY(176)	/* OBJ_id_ad                        1 3 6 1 5 5 7 48 */
CONST_ENTRY(507)	/* OBJ_id_hex_partial_message       1 3 6 1 7 1 1 1 */
CONST_ENTRY(508)	/* OBJ_id_hex_multipart_message     1 3 6 1 7 1 1 2 */
CONST_ENTRY(57)	/* OBJ_netscape                     2 16 840 1 113730 */
CONST_ENTRY(754)	/* OBJ_camellia_128_ecb             0 3 4401 5 3 1 9 1 */
CONST_ENTRY(766)	/* OBJ_camellia_128_ofb128          0 3 4401 5 3 1 9 3 */
CONST_ENTRY(757)	/* OBJ_camellia_128_cfb128          0 3 4401 5 3 1 9 4 */
CONST_ENTRY(755)	/* OBJ_camellia_192_ecb             0 3 4401 5 3 1 9 21 */
CONST_ENTRY(767)	/* OBJ_camellia_192_ofb128          0 3 4401 5 3 1 9 23 */
CONST_ENTRY(758)	/* OBJ_camellia_192_cfb128          0 3 4401 5 3 1 9 24 */
CONST_ENTRY(756)	/* OBJ_camellia_256_ecb             0 3 4401 5 3 1 9 41 */
CONST_ENTRY(768)	/* OBJ_camellia_256_ofb128          0 3 4401 5 3 1 9 43 */
CONST_ENTRY(759)	/* OBJ_camellia_256_cfb128          0 3 4401 5 3 1 9 44 */
CONST_ENTRY(437)	/* OBJ_pilot                        0 9 2342 19200300 100 */
CONST_ENTRY(776)	/* OBJ_seed_ecb                     1 2 410 200004 1 3 */
CONST_ENTRY(777)	/* OBJ_seed_cbc                     1 2 410 200004 1 4 */
CONST_ENTRY(779)	/* OBJ_seed_cfb128                  1 2 410 200004 1 5 */
CONST_ENTRY(778)	/* OBJ_seed_ofb128                  1 2 410 200004 1 6 */
CONST_ENTRY(852)	/* OBJ_id_GostR3411_94_with_GostR3410_94_cc 1 2 643 2 9 1 3 3 */
CONST_ENTRY(853)	/* OBJ_id_GostR3411_94_with_GostR3410_2001_cc 1 2 643 2 9 1 3 4 */
CONST_ENTRY(850)	/* OBJ_id_GostR3410_94_cc           1 2 643 2 9 1 5 3 */
CONST_ENTRY(851)	/* OBJ_id_GostR3410_2001_cc         1 2 643 2 9 1 5 4 */
CONST_ENTRY(849)	/* OBJ_id_Gost28147_89_cc           1 2 643 2 9 1 6 1 */
CONST_ENTRY(854)	/* OBJ_id_GostR3410_2001_ParamSet_cc 1 2 643 2 9 1 8 1 */
CONST_ENTRY(186)	/* OBJ_pkcs1                        1 2 840 113549 1 1 */
CONST_ENTRY(27)	/* OBJ_pkcs3                        1 2 840 113549 1 3 */
CONST_ENTRY(187)	/* OBJ_pkcs5                        1 2 840 113549 1 5 */
CONST_ENTRY(20)	/* OBJ_pkcs7                        1 2 840 113549 1 7 */
CONST_ENTRY(47)	/* OBJ_pkcs9                        1 2 840 113549 1 9 */
CONST_ENTRY( 3)	/* OBJ_md2                          1 2 840 113549 2 2 */
CONST_ENTRY(257)	/* OBJ_md4                          1 2 840 113549 2 4 */
CONST_ENTRY( 4)	/* OBJ_md5                          1 2 840 113549 2 5 */
CONST_ENTRY(797)	/* OBJ_hmacWithMD5                  1 2 840 113549 2 6 */
CONST_ENTRY(163)	/* OBJ_hmacWithSHA1                 1 2 840 113549 2 7 */
CONST_ENTRY(798)	/* OBJ_hmacWithSHA224               1 2 840 113549 2 8 */
CONST_ENTRY(799)	/* OBJ_hmacWithSHA256               1 2 840 113549 2 9 */
CONST_ENTRY(800)	/* OBJ_hmacWithSHA384               1 2 840 113549 2 10 */
CONST_ENTRY(801)	/* OBJ_hmacWithSHA512               1 2 840 113549 2 11 */
CONST_ENTRY(37)	/* OBJ_rc2_cbc                      1 2 840 113549 3 2 */
CONST_ENTRY( 5)	/* OBJ_rc4                          1 2 840 113549 3 4 */
CONST_ENTRY(44)	/* OBJ_des_ede3_cbc                 1 2 840 113549 3 7 */
CONST_ENTRY(120)	/* OBJ_rc5_cbc                      1 2 840 113549 3 8 */
CONST_ENTRY(643)	/* OBJ_des_cdmf                     1 2 840 113549 3 10 */
CONST_ENTRY(680)	/* OBJ_X9_62_id_characteristic_two_basis 1 2 840 10045 1 2 3 */
CONST_ENTRY(684)	/* OBJ_X9_62_c2pnb163v1             1 2 840 10045 3 0 1 */
CONST_ENTRY(685)	/* OBJ_X9_62_c2pnb163v2             1 2 840 10045 3 0 2 */
CONST_ENTRY(686)	/* OBJ_X9_62_c2pnb163v3             1 2 840 10045 3 0 3 */
CONST_ENTRY(687)	/* OBJ_X9_62_c2pnb176v1             1 2 840 10045 3 0 4 */
CONST_ENTRY(688)	/* OBJ_X9_62_c2tnb191v1             1 2 840 10045 3 0 5 */
CONST_ENTRY(689)	/* OBJ_X9_62_c2tnb191v2             1 2 840 10045 3 0 6 */
CONST_ENTRY(690)	/* OBJ_X9_62_c2tnb191v3             1 2 840 10045 3 0 7 */
CONST_ENTRY(691)	/* OBJ_X9_62_c2onb191v4             1 2 840 10045 3 0 8 */
CONST_ENTRY(692)	/* OBJ_X9_62_c2onb191v5             1 2 840 10045 3 0 9 */
CONST_ENTRY(693)	/* OBJ_X9_62_c2pnb208w1             1 2 840 10045 3 0 10 */
CONST_ENTRY(694)	/* OBJ_X9_62_c2tnb239v1             1 2 840 10045 3 0 11 */
CONST_ENTRY(695)	/* OBJ_X9_62_c2tnb239v2             1 2 840 10045 3 0 12 */
CONST_ENTRY(696)	/* OBJ_X9_62_c2tnb239v3             1 2 840 10045 3 0 13 */
CONST_ENTRY(697)	/* OBJ_X9_62_c2onb239v4             1 2 840 10045 3 0 14 */
CONST_ENTRY(698)	/* OBJ_X9_62_c2onb239v5             1 2 840 10045 3 0 15 */
CONST_ENTRY(699)	/* OBJ_X9_62_c2pnb272w1             1 2 840 10045 3 0 16 */
CONST_ENTRY(700)	/* OBJ_X9_62_c2pnb304w1             1 2 840 10045 3 0 17 */
CONST_ENTRY(701)	/* OBJ_X9_62_c2tnb359v1             1 2 840 10045 3 0 18 */
CONST_ENTRY(702)	/* OBJ_X9_62_c2pnb368w1             1 2 840 10045 3 0 19 */
CONST_ENTRY(703)	/* OBJ_X9_62_c2tnb431r1             1 2 840 10045 3 0 20 */
CONST_ENTRY(409)	/* OBJ_X9_62_prime192v1             1 2 840 10045 3 1 1 */
CONST_ENTRY(410)	/* OBJ_X9_62_prime192v2             1 2 840 10045 3 1 2 */
CONST_ENTRY(411)	/* OBJ_X9_62_prime192v3             1 2 840 10045 3 1 3 */
CONST_ENTRY(412)	/* OBJ_X9_62_prime239v1             1 2 840 10045 3 1 4 */
CONST_ENTRY(413)	/* OBJ_X9_62_prime239v2             1 2 840 10045 3 1 5 */
CONST_ENTRY(414)	/* OBJ_X9_62_prime239v3             1 2 840 10045 3 1 6 */
CONST_ENTRY(415)	/* OBJ_X9_62_prime256v1             1 2 840 10045 3 1 7 */
CONST_ENTRY(793)	/* OBJ_ecdsa_with_SHA224            1 2 840 10045 4 3 1 */
CONST_ENTRY(794)	/* OBJ_ecdsa_with_SHA256            1 2 840 10045 4 3 2 */
CONST_ENTRY(795)	/* OBJ_ecdsa_with_SHA384            1 2 840 10045 4 3 3 */
CONST_ENTRY(796)	/* OBJ_ecdsa_with_SHA512            1 2 840 10045 4 3 4 */
CONST_ENTRY(269)	/* OBJ_id_pkix1_explicit_88         1 3 6 1 5 5 7 0 1 */
CONST_ENTRY(270)	/* OBJ_id_pkix1_implicit_88         1 3 6 1 5 5 7 0 2 */
CONST_ENTRY(271)	/* OBJ_id_pkix1_explicit_93         1 3 6 1 5 5 7 0 3 */
CONST_ENTRY(272)	/* OBJ_id_pkix1_implicit_93         1 3 6 1 5 5 7 0 4 */
CONST_ENTRY(273)	/* OBJ_id_mod_crmf                  1 3 6 1 5 5 7 0 5 */
CONST_ENTRY(274)	/* OBJ_id_mod_cmc                   1 3 6 1 5 5 7 0 6 */
CONST_ENTRY(275)	/* OBJ_id_mod_kea_profile_88        1 3 6 1 5 5 7 0 7 */
CONST_ENTRY(276)	/* OBJ_id_mod_kea_profile_93        1 3 6 1 5 5 7 0 8 */
CONST_ENTRY(277)	/* OBJ_id_mod_cmp                   1 3 6 1 5 5 7 0 9 */
CONST_ENTRY(278)	/* OBJ_id_mod_qualified_cert_88     1 3 6 1 5 5 7 0 10 */
CONST_ENTRY(279)	/* OBJ_id_mod_qualified_cert_93     1 3 6 1 5 5 7 0 11 */
CONST_ENTRY(280)	/* OBJ_id_mod_attribute_cert        1 3 6 1 5 5 7 0 12 */
CONST_ENTRY(281)	/* OBJ_id_mod_timestamp_protocol    1 3 6 1 5 5 7 0 13 */
CONST_ENTRY(282)	/* OBJ_id_mod_ocsp                  1 3 6 1 5 5 7 0 14 */
CONST_ENTRY(283)	/* OBJ_id_mod_dvcs                  1 3 6 1 5 5 7 0 15 */
CONST_ENTRY(284)	/* OBJ_id_mod_cmp2000               1 3 6 1 5 5 7 0 16 */
CONST_ENTRY(177)	/* OBJ_info_access                  1 3 6 1 5 5 7 1 1 */
CONST_ENTRY(285)	/* OBJ_biometricInfo                1 3 6 1 5 5 7 1 2 */
CONST_ENTRY(286)	/* OBJ_qcStatements                 1 3 6 1 5 5 7 1 3 */
CONST_ENTRY(287)	/* OBJ_ac_auditEntity               1 3 6 1 5 5 7 1 4 */
CONST_ENTRY(288)	/* OBJ_ac_targeting                 1 3 6 1 5 5 7 1 5 */
CONST_ENTRY(289)	/* OBJ_aaControls                   1 3 6 1 5 5 7 1 6 */
CONST_ENTRY(290)	/* OBJ_sbgp_ipAddrBlock             1 3 6 1 5 5 7 1 7 */
CONST_ENTRY(291)	/* OBJ_sbgp_autonomousSysNum        1 3 6 1 5 5 7 1 8 */
CONST_ENTRY(292)	/* OBJ_sbgp_routerIdentifier        1 3 6 1 5 5 7 1 9 */
CONST_ENTRY(397)	/* OBJ_ac_proxying                  1 3 6 1 5 5 7 1 10 */
CONST_ENTRY(398)	/* OBJ_sinfo_access                 1 3 6 1 5 5 7 1 11 */
CONST_ENTRY(663)	/* OBJ_proxyCertInfo                1 3 6 1 5 5 7 1 14 */
CONST_ENTRY(164)	/* OBJ_id_qt_cps                    1 3 6 1 5 5 7 2 1 */
CONST_ENTRY(165)	/* OBJ_id_qt_unotice                1 3 6 1 5 5 7 2 2 */
CONST_ENTRY(293)	/* OBJ_textNotice                   1 3 6 1 5 5 7 2 3 */
CONST_ENTRY(129)	/* OBJ_server_auth                  1 3 6 1 5 5 7 3 1 */
CONST_ENTRY(130)	/* OBJ_client_auth                  1 3 6 1 5 5 7 3 2 */
CONST_ENTRY(131)	/* OBJ_code_sign                    1 3 6 1 5 5 7 3 3 */
CONST_ENTRY(132)	/* OBJ_email_protect                1 3 6 1 5 5 7 3 4 */
CONST_ENTRY(294)	/* OBJ_ipsecEndSystem               1 3 6 1 5 5 7 3 5 */
CONST_ENTRY(295)	/* OBJ_ipsecTunnel                  1 3 6 1 5 5 7 3 6 */
CONST_ENTRY(296)	/* OBJ_ipsecUser                    1 3 6 1 5 5 7 3 7 */
CONST_ENTRY(133)	/* OBJ_time_stamp                   1 3 6 1 5 5 7 3 8 */
CONST_ENTRY(180)	/* OBJ_OCSP_sign                    1 3 6 1 5 5 7 3 9 */
CONST_ENTRY(297)	/* OBJ_dvcs                         1 3 6 1 5 5 7 3 10 */
CONST_ENTRY(298)	/* OBJ_id_it_caProtEncCert          1 3 6 1 5 5 7 4 1 */
CONST_ENTRY(299)	/* OBJ_id_it_signKeyPairTypes       1 3 6 1 5 5 7 4 2 */
CONST_ENTRY(300)	/* OBJ_id_it_encKeyPairTypes        1 3 6 1 5 5 7 4 3 */
CONST_ENTRY(301)	/* OBJ_id_it_preferredSymmAlg       1 3 6 1 5 5 7 4 4 */
CONST_ENTRY(302)	/* OBJ_id_it_caKeyUpdateInfo        1 3 6 1 5 5 7 4 5 */
CONST_ENTRY(303)	/* OBJ_id_it_currentCRL             1 3 6 1 5 5 7 4 6 */
CONST_ENTRY(304)	/* OBJ_id_it_unsupportedOIDs        1 3 6 1 5 5 7 4 7 */
CONST_ENTRY(305)	/* OBJ_id_it_subscriptionRequest    1 3 6 1 5 5 7 4 8 */
CONST_ENTRY(306)	/* OBJ_id_it_subscriptionResponse   1 3 6 1 5 5 7 4 9 */
CONST_ENTRY(307)	/* OBJ_id_it_keyPairParamReq        1 3 6 1 5 5 7 4 10 */
CONST_ENTRY(308)	/* OBJ_id_it_keyPairParamRep        1 3 6 1 5 5 7 4 11 */
CONST_ENTRY(309)	/* OBJ_id_it_revPassphrase          1 3 6 1 5 5 7 4 12 */
CONST_ENTRY(310)	/* OBJ_id_it_implicitConfirm        1 3 6 1 5 5 7 4 13 */
CONST_ENTRY(311)	/* OBJ_id_it_confirmWaitTime        1 3 6 1 5 5 7 4 14 */
CONST_ENTRY(312)	/* OBJ_id_it_origPKIMessage         1 3 6 1 5 5 7 4 15 */
CONST_ENTRY(784)	/* OBJ_id_it_suppLangTags           1 3 6 1 5 5 7 4 16 */
CONST_ENTRY(313)	/* OBJ_id_regCtrl                   1 3 6 1 5 5 7 5 1 */
CONST_ENTRY(314)	/* OBJ_id_regInfo                   1 3 6 1 5 5 7 5 2 */
CONST_ENTRY(323)	/* OBJ_id_alg_des40                 1 3 6 1 5 5 7 6 1 */
CONST_ENTRY(324)	/* OBJ_id_alg_noSignature           1 3 6 1 5 5 7 6 2 */
CONST_ENTRY(325)	/* OBJ_id_alg_dh_sig_hmac_sha1      1 3 6 1 5 5 7 6 3 */
CONST_ENTRY(326)	/* OBJ_id_alg_dh_pop                1 3 6 1 5 5 7 6 4 */
CONST_ENTRY(327)	/* OBJ_id_cmc_statusInfo            1 3 6 1 5 5 7 7 1 */
CONST_ENTRY(328)	/* OBJ_id_cmc_identification        1 3 6 1 5 5 7 7 2 */
CONST_ENTRY(329)	/* OBJ_id_cmc_identityProof         1 3 6 1 5 5 7 7 3 */
CONST_ENTRY(330)	/* OBJ_id_cmc_dataReturn            1 3 6 1 5 5 7 7 4 */
CONST_ENTRY(331)	/* OBJ_id_cmc_transactionId         1 3 6 1 5 5 7 7 5 */
CONST_ENTRY(332)	/* OBJ_id_cmc_senderNonce           1 3 6 1 5 5 7 7 6 */
CONST_ENTRY(333)	/* OBJ_id_cmc_recipientNonce        1 3 6 1 5 5 7 7 7 */
CONST_ENTRY(334)	/* OBJ_id_cmc_addExtensions         1 3 6 1 5 5 7 7 8 */
CONST_ENTRY(335)	/* OBJ_id_cmc_encryptedPOP          1 3 6 1 5 5 7 7 9 */
CONST_ENTRY(336)	/* OBJ_id_cmc_decryptedPOP          1 3 6 1 5 5 7 7 10 */
CONST_ENTRY(337)	/* OBJ_id_cmc_lraPOPWitness         1 3 6 1 5 5 7 7 11 */
CONST_ENTRY(338)	/* OBJ_id_cmc_getCert               1 3 6 1 5 5 7 7 15 */
CONST_ENTRY(339)	/* OBJ_id_cmc_getCRL                1 3 6 1 5 5 7 7 16 */
CONST_ENTRY(340)	/* OBJ_id_cmc_revokeRequest         1 3 6 1 5 5 7 7 17 */
CONST_ENTRY(341)	/* OBJ_id_cmc_regInfo               1 3 6 1 5 5 7 7 18 */
CONST_ENTRY(342)	/* OBJ_id_cmc_responseInfo          1 3 6 1 5 5 7 7 19 */
CONST_ENTRY(343)	/* OBJ_id_cmc_queryPending          1 3 6 1 5 5 7 7 21 */
CONST_ENTRY(344)	/* OBJ_id_cmc_popLinkRandom         1 3 6 1 5 5 7 7 22 */
CONST_ENTRY(345)	/* OBJ_id_cmc_popLinkWitness        1 3 6 1 5 5 7 7 23 */
CONST_ENTRY(346)	/* OBJ_id_cmc_confirmCertAcceptance 1 3 6 1 5 5 7 7 24 */
CONST_ENTRY(347)	/* OBJ_id_on_personalData           1 3 6 1 5 5 7 8 1 */
CONST_ENTRY(858)	/* OBJ_id_on_permanentIdentifier    1 3 6 1 5 5 7 8 3 */
CONST_ENTRY(348)	/* OBJ_id_pda_dateOfBirth           1 3 6 1 5 5 7 9 1 */
CONST_ENTRY(349)	/* OBJ_id_pda_placeOfBirth          1 3 6 1 5 5 7 9 2 */
CONST_ENTRY(351)	/* OBJ_id_pda_gender                1 3 6 1 5 5 7 9 3 */
CONST_ENTRY(352)	/* OBJ_id_pda_countryOfCitizenship  1 3 6 1 5 5 7 9 4 */
CONST_ENTRY(353)	/* OBJ_id_pda_countryOfResidence    1 3 6 1 5 5 7 9 5 */
CONST_ENTRY(354)	/* OBJ_id_aca_authenticationInfo    1 3 6 1 5 5 7 10 1 */
CONST_ENTRY(355)	/* OBJ_id_aca_accessIdentity        1 3 6 1 5 5 7 10 2 */
CONST_ENTRY(356)	/* OBJ_id_aca_chargingIdentity      1 3 6 1 5 5 7 10 3 */
CONST_ENTRY(357)	/* OBJ_id_aca_group                 1 3 6 1 5 5 7 10 4 */
CONST_ENTRY(358)	/* OBJ_id_aca_role                  1 3 6 1 5 5 7 10 5 */
CONST_ENTRY(399)	/* OBJ_id_aca_encAttrs              1 3 6 1 5 5 7 10 6 */
CONST_ENTRY(359)	/* OBJ_id_qcs_pkixQCSyntax_v1       1 3 6 1 5 5 7 11 1 */
CONST_ENTRY(360)	/* OBJ_id_cct_crs                   1 3 6 1 5 5 7 12 1 */
CONST_ENTRY(361)	/* OBJ_id_cct_PKIData               1 3 6 1 5 5 7 12 2 */
CONST_ENTRY(362)	/* OBJ_id_cct_PKIResponse           1 3 6 1 5 5 7 12 3 */
CONST_ENTRY(664)	/* OBJ_id_ppl_anyLanguage           1 3 6 1 5 5 7 21 0 */
CONST_ENTRY(665)	/* OBJ_id_ppl_inheritAll            1 3 6 1 5 5 7 21 1 */
CONST_ENTRY(667)	/* OBJ_Independent                  1 3 6 1 5 5 7 21 2 */
CONST_ENTRY(178)	/* OBJ_ad_OCSP                      1 3 6 1 5 5 7 48 1 */
CONST_ENTRY(179)	/* OBJ_ad_ca_issuers                1 3 6 1 5 5 7 48 2 */
CONST_ENTRY(363)	/* OBJ_ad_timeStamping              1 3 6 1 5 5 7 48 3 */
CONST_ENTRY(364)	/* OBJ_ad_dvcs                      1 3 6 1 5 5 7 48 4 */
CONST_ENTRY(785)	/* OBJ_caRepository                 1 3 6 1 5 5 7 48 5 */
CONST_ENTRY(780)	/* OBJ_hmac_md5                     1 3 6 1 5 5 8 1 1 */
CONST_ENTRY(781)	/* OBJ_hmac_sha1                    1 3 6 1 5 5 8 1 2 */
CONST_ENTRY(58)	/* OBJ_netscape_cert_extension      2 16 840 1 113730 1 */
CONST_ENTRY(59)	/* OBJ_netscape_data_type           2 16 840 1 113730 2 */
CONST_ENTRY(438)	/* OBJ_pilotAttributeType           0 9 2342 19200300 100 1 */
CONST_ENTRY(439)	/* OBJ_pilotAttributeSyntax         0 9 2342 19200300 100 3 */
CONST_ENTRY(440)	/* OBJ_pilotObjectClass             0 9 2342 19200300 100 4 */
CONST_ENTRY(441)	/* OBJ_pilotGroups                  0 9 2342 19200300 100 10 */
CONST_ENTRY(108)	/* OBJ_cast5_cbc                    1 2 840 113533 7 66 10 */
CONST_ENTRY(112)	/* OBJ_pbeWithMD5AndCast5_CBC       1 2 840 113533 7 66 12 */
CONST_ENTRY(782)	/* OBJ_id_PasswordBasedMAC          1 2 840 113533 7 66 13 */
CONST_ENTRY(783)	/* OBJ_id_DHBasedMac                1 2 840 113533 7 66 30 */
CONST_ENTRY( 6)	/* OBJ_rsaEncryption                1 2 840 113549 1 1 1 */
CONST_ENTRY( 7)	/* OBJ_md2WithRSAEncryption         1 2 840 113549 1 1 2 */
CONST_ENTRY(396)	/* OBJ_md4WithRSAEncryption         1 2 840 113549 1 1 3 */
CONST_ENTRY( 8)	/* OBJ_md5WithRSAEncryption         1 2 840 113549 1 1 4 */
CONST_ENTRY(65)	/* OBJ_sha1WithRSAEncryption        1 2 840 113549 1 1 5 */
CONST_ENTRY(644)	/* OBJ_rsaOAEPEncryptionSET         1 2 840 113549 1 1 6 */
CONST_ENTRY(668)	/* OBJ_sha256WithRSAEncryption      1 2 840 113549 1 1 11 */
CONST_ENTRY(669)	/* OBJ_sha384WithRSAEncryption      1 2 840 113549 1 1 12 */
CONST_ENTRY(670)	/* OBJ_sha512WithRSAEncryption      1 2 840 113549 1 1 13 */
CONST_ENTRY(671)	/* OBJ_sha224WithRSAEncryption      1 2 840 113549 1 1 14 */
CONST_ENTRY(28)	/* OBJ_dhKeyAgreement               1 2 840 113549 1 3 1 */
CONST_ENTRY( 9)	/* OBJ_pbeWithMD2AndDES_CBC         1 2 840 113549 1 5 1 */
CONST_ENTRY(10)	/* OBJ_pbeWithMD5AndDES_CBC         1 2 840 113549 1 5 3 */
CONST_ENTRY(168)	/* OBJ_pbeWithMD2AndRC2_CBC         1 2 840 113549 1 5 4 */
CONST_ENTRY(169)	/* OBJ_pbeWithMD5AndRC2_CBC         1 2 840 113549 1 5 6 */
CONST_ENTRY(170)	/* OBJ_pbeWithSHA1AndDES_CBC        1 2 840 113549 1 5 10 */
CONST_ENTRY(68)	/* OBJ_pbeWithSHA1AndRC2_CBC        1 2 840 113549 1 5 11 */
CONST_ENTRY(69)	/* OBJ_id_pbkdf2                    1 2 840 113549 1 5 12 */
CONST_ENTRY(161)	/* OBJ_pbes2                        1 2 840 113549 1 5 13 */
CONST_ENTRY(162)	/* OBJ_pbmac1                       1 2 840 113549 1 5 14 */
CONST_ENTRY(21)	/* OBJ_pkcs7_data                   1 2 840 113549 1 7 1 */
CONST_ENTRY(22)	/* OBJ_pkcs7_signed                 1 2 840 113549 1 7 2 */
CONST_ENTRY(23)	/* OBJ_pkcs7_enveloped              1 2 840 113549 1 7 3 */
CONST_ENTRY(24)	/* OBJ_pkcs7_signedAndEnveloped     1 2 840 113549 1 7 4 */
CONST_ENTRY(25)	/* OBJ_pkcs7_digest                 1 2 840 113549 1 7 5 */
CONST_ENTRY(26)	/* OBJ_pkcs7_encrypted              1 2 840 113549 1 7 6 */
CONST_ENTRY(48)	/* OBJ_pkcs9_emailAddress           1 2 840 113549 1 9 1 */
CONST_ENTRY(49)	/* OBJ_pkcs9_unstructuredName       1 2 840 113549 1 9 2 */
CONST_ENTRY(50)	/* OBJ_pkcs9_contentType            1 2 840 113549 1 9 3 */
CONST_ENTRY(51)	/* OBJ_pkcs9_messageDigest          1 2 840 113549 1 9 4 */
CONST_ENTRY(52)	/* OBJ_pkcs9_signingTime            1 2 840 113549 1 9 5 */
CONST_ENTRY(53)	/* OBJ_pkcs9_countersignature       1 2 840 113549 1 9 6 */
CONST_ENTRY(54)	/* OBJ_pkcs9_challengePassword      1 2 840 113549 1 9 7 */
CONST_ENTRY(55)	/* OBJ_pkcs9_unstructuredAddress    1 2 840 113549 1 9 8 */
CONST_ENTRY(56)	/* OBJ_pkcs9_extCertAttributes      1 2 840 113549 1 9 9 */
CONST_ENTRY(172)	/* OBJ_ext_req                      1 2 840 113549 1 9 14 */
CONST_ENTRY(167)	/* OBJ_SMIMECapabilities            1 2 840 113549 1 9 15 */
CONST_ENTRY(188)	/* OBJ_SMIME                        1 2 840 113549 1 9 16 */
CONST_ENTRY(156)	/* OBJ_friendlyName                 1 2 840 113549 1 9 20 */
CONST_ENTRY(157)	/* OBJ_localKeyID                   1 2 840 113549 1 9 21 */
CONST_ENTRY(681)	/* OBJ_X9_62_onBasis                1 2 840 10045 1 2 3 1 */
CONST_ENTRY(682)	/* OBJ_X9_62_tpBasis                1 2 840 10045 1 2 3 2 */
CONST_ENTRY(683)	/* OBJ_X9_62_ppBasis                1 2 840 10045 1 2 3 3 */
CONST_ENTRY(417)	/* OBJ_ms_csp_name                  1 3 6 1 4 1 311 17 1 */
CONST_ENTRY(856)	/* OBJ_LocalKeySet                  1 3 6 1 4 1 311 17 2 */
CONST_ENTRY(390)	/* OBJ_dcObject                     1 3 6 1 4 1 1466 344 */
CONST_ENTRY(91)	/* OBJ_bf_cbc                       1 3 6 1 4 1 3029 1 2 */
CONST_ENTRY(315)	/* OBJ_id_regCtrl_regToken          1 3 6 1 5 5 7 5 1 1 */
CONST_ENTRY(316)	/* OBJ_id_regCtrl_authenticator     1 3 6 1 5 5 7 5 1 2 */
CONST_ENTRY(317)	/* OBJ_id_regCtrl_pkiPublicationInfo 1 3 6 1 5 5 7 5 1 3 */
CONST_ENTRY(318)	/* OBJ_id_regCtrl_pkiArchiveOptions 1 3 6 1 5 5 7 5 1 4 */
CONST_ENTRY(319)	/* OBJ_id_regCtrl_oldCertID         1 3 6 1 5 5 7 5 1 5 */
CONST_ENTRY(320)	/* OBJ_id_regCtrl_protocolEncrKey   1 3 6 1 5 5 7 5 1 6 */
CONST_ENTRY(321)	/* OBJ_id_regInfo_utf8Pairs         1 3 6 1 5 5 7 5 2 1 */
CONST_ENTRY(322)	/* OBJ_id_regInfo_certReq           1 3 6 1 5 5 7 5 2 2 */
CONST_ENTRY(365)	/* OBJ_id_pkix_OCSP_basic           1 3 6 1 5 5 7 48 1 1 */
CONST_ENTRY(366)	/* OBJ_id_pkix_OCSP_Nonce           1 3 6 1 5 5 7 48 1 2 */
CONST_ENTRY(367)	/* OBJ_id_pkix_OCSP_CrlID           1 3 6 1 5 5 7 48 1 3 */
CONST_ENTRY(368)	/* OBJ_id_pkix_OCSP_acceptableResponses 1 3 6 1 5 5 7 48 1 4 */
CONST_ENTRY(369)	/* OBJ_id_pkix_OCSP_noCheck         1 3 6 1 5 5 7 48 1 5 */
CONST_ENTRY(370)	/* OBJ_id_pkix_OCSP_archiveCutoff   1 3 6 1 5 5 7 48 1 6 */
CONST_ENTRY(371)	/* OBJ_id_pkix_OCSP_serviceLocator  1 3 6 1 5 5 7 48 1 7 */
CONST_ENTRY(372)	/* OBJ_id_pkix_OCSP_extendedStatus  1 3 6 1 5 5 7 48 1 8 */
CONST_ENTRY(373)	/* OBJ_id_pkix_OCSP_valid           1 3 6 1 5 5 7 48 1 9 */
CONST_ENTRY(374)	/* OBJ_id_pkix_OCSP_path            1 3 6 1 5 5 7 48 1 10 */
CONST_ENTRY(375)	/* OBJ_id_pkix_OCSP_trustRoot       1 3 6 1 5 5 7 48 1 11 */
CONST_ENTRY(418)	/* OBJ_aes_128_ecb                  2 16 840 1 101 3 4 1 1 */
CONST_ENTRY(419)	/* OBJ_aes_128_cbc                  2 16 840 1 101 3 4 1 2 */
CONST_ENTRY(420)	/* OBJ_aes_128_ofb128               2 16 840 1 101 3 4 1 3 */
CONST_ENTRY(421)	/* OBJ_aes_128_cfb128               2 16 840 1 101 3 4 1 4 */
CONST_ENTRY(788)	/* OBJ_id_aes128_wrap               2 16 840 1 101 3 4 1 5 */
CONST_ENTRY(422)	/* OBJ_aes_192_ecb                  2 16 840 1 101 3 4 1 21 */
CONST_ENTRY(423)	/* OBJ_aes_192_cbc                  2 16 840 1 101 3 4 1 22 */
CONST_ENTRY(424)	/* OBJ_aes_192_ofb128               2 16 840 1 101 3 4 1 23 */
CONST_ENTRY(425)	/* OBJ_aes_192_cfb128               2 16 840 1 101 3 4 1 24 */
CONST_ENTRY(789)	/* OBJ_id_aes192_wrap               2 16 840 1 101 3 4 1 25 */
CONST_ENTRY(426)	/* OBJ_aes_256_ecb                  2 16 840 1 101 3 4 1 41 */
CONST_ENTRY(427)	/* OBJ_aes_256_cbc                  2 16 840 1 101 3 4 1 42 */
CONST_ENTRY(428)	/* OBJ_aes_256_ofb128               2 16 840 1 101 3 4 1 43 */
CONST_ENTRY(429)	/* OBJ_aes_256_cfb128               2 16 840 1 101 3 4 1 44 */
CONST_ENTRY(790)	/* OBJ_id_aes256_wrap               2 16 840 1 101 3 4 1 45 */
CONST_ENTRY(672)	/* OBJ_sha256                       2 16 840 1 101 3 4 2 1 */
CONST_ENTRY(673)	/* OBJ_sha384                       2 16 840 1 101 3 4 2 2 */
CONST_ENTRY(674)	/* OBJ_sha512                       2 16 840 1 101 3 4 2 3 */
CONST_ENTRY(675)	/* OBJ_sha224                       2 16 840 1 101 3 4 2 4 */
CONST_ENTRY(802)	/* OBJ_dsa_with_SHA224              2 16 840 1 101 3 4 3 1 */
CONST_ENTRY(803)	/* OBJ_dsa_with_SHA256              2 16 840 1 101 3 4 3 2 */
CONST_ENTRY(71)	/* OBJ_netscape_cert_type           2 16 840 1 113730 1 1 */
CONST_ENTRY(72)	/* OBJ_netscape_base_url            2 16 840 1 113730 1 2 */
CONST_ENTRY(73)	/* OBJ_netscape_revocation_url      2 16 840 1 113730 1 3 */
CONST_ENTRY(74)	/* OBJ_netscape_ca_revocation_url   2 16 840 1 113730 1 4 */
CONST_ENTRY(75)	/* OBJ_netscape_renewal_url         2 16 840 1 113730 1 7 */
CONST_ENTRY(76)	/* OBJ_netscape_ca_policy_url       2 16 840 1 113730 1 8 */
CONST_ENTRY(77)	/* OBJ_netscape_ssl_server_name     2 16 840 1 113730 1 12 */
CONST_ENTRY(78)	/* OBJ_netscape_comment             2 16 840 1 113730 1 13 */
CONST_ENTRY(79)	/* OBJ_netscape_cert_sequence       2 16 840 1 113730 2 5 */
CONST_ENTRY(139)	/* OBJ_ns_sgc                       2 16 840 1 113730 4 1 */
CONST_ENTRY(458)	/* OBJ_userId                       0 9 2342 19200300 100 1 1 */
CONST_ENTRY(459)	/* OBJ_textEncodedORAddress         0 9 2342 19200300 100 1 2 */
CONST_ENTRY(460)	/* OBJ_rfc822Mailbox                0 9 2342 19200300 100 1 3 */
CONST_ENTRY(461)	/* OBJ_info                         0 9 2342 19200300 100 1 4 */
CONST_ENTRY(462)	/* OBJ_favouriteDrink               0 9 2342 19200300 100 1 5 */
CONST_ENTRY(463)	/* OBJ_roomNumber                   0 9 2342 19200300 100 1 6 */
CONST_ENTRY(464)	/* OBJ_photo                        0 9 2342 19200300 100 1 7 */
CONST_ENTRY(465)	/* OBJ_userClass                    0 9 2342 19200300 100 1 8 */
CONST_ENTRY(466)	/* OBJ_host                         0 9 2342 19200300 100 1 9 */
CONST_ENTRY(467)	/* OBJ_manager                      0 9 2342 19200300 100 1 10 */
CONST_ENTRY(468)	/* OBJ_documentIdentifier           0 9 2342 19200300 100 1 11 */
CONST_ENTRY(469)	/* OBJ_documentTitle                0 9 2342 19200300 100 1 12 */
CONST_ENTRY(470)	/* OBJ_documentVersion              0 9 2342 19200300 100 1 13 */
CONST_ENTRY(471)	/* OBJ_documentAuthor               0 9 2342 19200300 100 1 14 */
CONST_ENTRY(472)	/* OBJ_documentLocation             0 9 2342 19200300 100 1 15 */
CONST_ENTRY(473)	/* OBJ_homeTelephoneNumber          0 9 2342 19200300 100 1 20 */
CONST_ENTRY(474)	/* OBJ_secretary                    0 9 2342 19200300 100 1 21 */
CONST_ENTRY(475)	/* OBJ_otherMailbox                 0 9 2342 19200300 100 1 22 */
CONST_ENTRY(476)	/* OBJ_lastModifiedTime             0 9 2342 19200300 100 1 23 */
CONST_ENTRY(477)	/* OBJ_lastModifiedBy               0 9 2342 19200300 100 1 24 */
CONST_ENTRY(391)	/* OBJ_domainComponent              0 9 2342 19200300 100 1 25 */
CONST_ENTRY(478)	/* OBJ_aRecord                      0 9 2342 19200300 100 1 26 */
CONST_ENTRY(479)	/* OBJ_pilotAttributeType27         0 9 2342 19200300 100 1 27 */
CONST_ENTRY(480)	/* OBJ_mXRecord                     0 9 2342 19200300 100 1 28 */
CONST_ENTRY(481)	/* OBJ_nSRecord                     0 9 2342 19200300 100 1 29 */
CONST_ENTRY(482)	/* OBJ_sOARecord                    0 9 2342 19200300 100 1 30 */
CONST_ENTRY(483)	/* OBJ_cNAMERecord                  0 9 2342 19200300 100 1 31 */
CONST_ENTRY(484)	/* OBJ_associatedDomain             0 9 2342 19200300 100 1 37 */
CONST_ENTRY(485)	/* OBJ_associatedName               0 9 2342 19200300 100 1 38 */
CONST_ENTRY(486)	/* OBJ_homePostalAddress            0 9 2342 19200300 100 1 39 */
CONST_ENTRY(487)	/* OBJ_personalTitle                0 9 2342 19200300 100 1 40 */
CONST_ENTRY(488)	/* OBJ_mobileTelephoneNumber        0 9 2342 19200300 100 1 41 */
CONST_ENTRY(489)	/* OBJ_pagerTelephoneNumber         0 9 2342 19200300 100 1 42 */
CONST_ENTRY(490)	/* OBJ_friendlyCountryName          0 9 2342 19200300 100 1 43 */
CONST_ENTRY(491)	/* OBJ_organizationalStatus         0 9 2342 19200300 100 1 45 */
CONST_ENTRY(492)	/* OBJ_janetMailbox                 0 9 2342 19200300 100 1 46 */
CONST_ENTRY(493)	/* OBJ_mailPreferenceOption         0 9 2342 19200300 100 1 47 */
CONST_ENTRY(494)	/* OBJ_buildingName                 0 9 2342 19200300 100 1 48 */
CONST_ENTRY(495)	/* OBJ_dSAQuality                   0 9 2342 19200300 100 1 49 */
CONST_ENTRY(496)	/* OBJ_singleLevelQuality           0 9 2342 19200300 100 1 50 */
CONST_ENTRY(497)	/* OBJ_subtreeMinimumQuality        0 9 2342 19200300 100 1 51 */
CONST_ENTRY(498)	/* OBJ_subtreeMaximumQuality        0 9 2342 19200300 100 1 52 */
CONST_ENTRY(499)	/* OBJ_personalSignature            0 9 2342 19200300 100 1 53 */
CONST_ENTRY(500)	/* OBJ_dITRedirect                  0 9 2342 19200300 100 1 54 */
CONST_ENTRY(501)	/* OBJ_audio                        0 9 2342 19200300 100 1 55 */
CONST_ENTRY(502)	/* OBJ_documentPublisher            0 9 2342 19200300 100 1 56 */
CONST_ENTRY(442)	/* OBJ_iA5StringSyntax              0 9 2342 19200300 100 3 4 */
CONST_ENTRY(443)	/* OBJ_caseIgnoreIA5StringSyntax    0 9 2342 19200300 100 3 5 */
CONST_ENTRY(444)	/* OBJ_pilotObject                  0 9 2342 19200300 100 4 3 */
CONST_ENTRY(445)	/* OBJ_pilotPerson                  0 9 2342 19200300 100 4 4 */
CONST_ENTRY(446)	/* OBJ_account                      0 9 2342 19200300 100 4 5 */
CONST_ENTRY(447)	/* OBJ_document                     0 9 2342 19200300 100 4 6 */
CONST_ENTRY(448)	/* OBJ_room                         0 9 2342 19200300 100 4 7 */
CONST_ENTRY(449)	/* OBJ_documentSeries               0 9 2342 19200300 100 4 9 */
CONST_ENTRY(392)	/* OBJ_Domain                       0 9 2342 19200300 100 4 13 */
CONST_ENTRY(450)	/* OBJ_rFC822localPart              0 9 2342 19200300 100 4 14 */
CONST_ENTRY(451)	/* OBJ_dNSDomain                    0 9 2342 19200300 100 4 15 */
CONST_ENTRY(452)	/* OBJ_domainRelatedObject          0 9 2342 19200300 100 4 17 */
CONST_ENTRY(453)	/* OBJ_friendlyCountry              0 9 2342 19200300 100 4 18 */
CONST_ENTRY(454)	/* OBJ_simpleSecurityObject         0 9 2342 19200300 100 4 19 */
CONST_ENTRY(455)	/* OBJ_pilotOrganization            0 9 2342 19200300 100 4 20 */
CONST_ENTRY(456)	/* OBJ_pilotDSA                     0 9 2342 19200300 100 4 21 */
CONST_ENTRY(457)	/* OBJ_qualityLabelledData          0 9 2342 19200300 100 4 22 */
CONST_ENTRY(189)	/* OBJ_id_smime_mod                 1 2 840 113549 1 9 16 0 */
CONST_ENTRY(190)	/* OBJ_id_smime_ct                  1 2 840 113549 1 9 16 1 */
CONST_ENTRY(191)	/* OBJ_id_smime_aa                  1 2 840 113549 1 9 16 2 */
CONST_ENTRY(192)	/* OBJ_id_smime_alg                 1 2 840 113549 1 9 16 3 */
CONST_ENTRY(193)	/* OBJ_id_smime_cd                  1 2 840 113549 1 9 16 4 */
CONST_ENTRY(194)	/* OBJ_id_smime_spq                 1 2 840 113549 1 9 16 5 */
CONST_ENTRY(195)	/* OBJ_id_smime_cti                 1 2 840 113549 1 9 16 6 */
CONST_ENTRY(158)	/* OBJ_x509Certificate              1 2 840 113549 1 9 22 1 */
CONST_ENTRY(159)	/* OBJ_sdsiCertificate              1 2 840 113549 1 9 22 2 */
CONST_ENTRY(160)	/* OBJ_x509Crl                      1 2 840 113549 1 9 23 1 */
CONST_ENTRY(144)	/* OBJ_pbe_WithSHA1And128BitRC4     1 2 840 113549 1 12 1 1 */
CONST_ENTRY(145)	/* OBJ_pbe_WithSHA1And40BitRC4      1 2 840 113549 1 12 1 2 */
CONST_ENTRY(146)	/* OBJ_pbe_WithSHA1And3_Key_TripleDES_CBC 1 2 840 113549 1 12 1 3 */
CONST_ENTRY(147)	/* OBJ_pbe_WithSHA1And2_Key_TripleDES_CBC 1 2 840 113549 1 12 1 4 */
CONST_ENTRY(148)	/* OBJ_pbe_WithSHA1And128BitRC2_CBC 1 2 840 113549 1 12 1 5 */
CONST_ENTRY(149)	/* OBJ_pbe_WithSHA1And40BitRC2_CBC  1 2 840 113549 1 12 1 6 */
CONST_ENTRY(171)	/* OBJ_ms_ext_req                   1 3 6 1 4 1 311 2 1 14 */
CONST_ENTRY(134)	/* OBJ_ms_code_ind                  1 3 6 1 4 1 311 2 1 21 */
CONST_ENTRY(135)	/* OBJ_ms_code_com                  1 3 6 1 4 1 311 2 1 22 */
CONST_ENTRY(136)	/* OBJ_ms_ctl_sign                  1 3 6 1 4 1 311 10 3 1 */
CONST_ENTRY(137)	/* OBJ_ms_sgc                       1 3 6 1 4 1 311 10 3 3 */
CONST_ENTRY(138)	/* OBJ_ms_efs                       1 3 6 1 4 1 311 10 3 4 */
CONST_ENTRY(648)	/* OBJ_ms_smartcard_login           1 3 6 1 4 1 311 20 2 2 */
CONST_ENTRY(649)	/* OBJ_ms_upn                       1 3 6 1 4 1 311 20 2 3 */
CONST_ENTRY(751)	/* OBJ_camellia_128_cbc             1 2 392 200011 61 1 1 1 2 */
CONST_ENTRY(752)	/* OBJ_camellia_192_cbc             1 2 392 200011 61 1 1 1 3 */
CONST_ENTRY(753)	/* OBJ_camellia_256_cbc             1 2 392 200011 61 1 1 1 4 */
CONST_ENTRY(196)	/* OBJ_id_smime_mod_cms             1 2 840 113549 1 9 16 0 1 */
CONST_ENTRY(197)	/* OBJ_id_smime_mod_ess             1 2 840 113549 1 9 16 0 2 */
CONST_ENTRY(198)	/* OBJ_id_smime_mod_oid             1 2 840 113549 1 9 16 0 3 */
CONST_ENTRY(199)	/* OBJ_id_smime_mod_msg_v3          1 2 840 113549 1 9 16 0 4 */
CONST_ENTRY(200)	/* OBJ_id_smime_mod_ets_eSignature_88 1 2 840 113549 1 9 16 0 5 */
CONST_ENTRY(201)	/* OBJ_id_smime_mod_ets_eSignature_97 1 2 840 113549 1 9 16 0 6 */
CONST_ENTRY(202)	/* OBJ_id_smime_mod_ets_eSigPolicy_88 1 2 840 113549 1 9 16 0 7 */
CONST_ENTRY(203)	/* OBJ_id_smime_mod_ets_eSigPolicy_97 1 2 840 113549 1 9 16 0 8 */
CONST_ENTRY(204)	/* OBJ_id_smime_ct_receipt          1 2 840 113549 1 9 16 1 1 */
CONST_ENTRY(205)	/* OBJ_id_smime_ct_authData         1 2 840 113549 1 9 16 1 2 */
CONST_ENTRY(206)	/* OBJ_id_smime_ct_publishCert      1 2 840 113549 1 9 16 1 3 */
CONST_ENTRY(207)	/* OBJ_id_smime_ct_TSTInfo          1 2 840 113549 1 9 16 1 4 */
CONST_ENTRY(208)	/* OBJ_id_smime_ct_TDTInfo          1 2 840 113549 1 9 16 1 5 */
CONST_ENTRY(209)	/* OBJ_id_smime_ct_contentInfo      1 2 840 113549 1 9 16 1 6 */
CONST_ENTRY(210)	/* OBJ_id_smime_ct_DVCSRequestData  1 2 840 113549 1 9 16 1 7 */
CONST_ENTRY(211)	/* OBJ_id_smime_ct_DVCSResponseData 1 2 840 113549 1 9 16 1 8 */
CONST_ENTRY(786)	/* OBJ_id_smime_ct_compressedData   1 2 840 113549 1 9 16 1 9 */
CONST_ENTRY(787)	/* OBJ_id_ct_asciiTextWithCRLF      1 2 840 113549 1 9 16 1 27 */
CONST_ENTRY(212)	/* OBJ_id_smime_aa_receiptRequest   1 2 840 113549 1 9 16 2 1 */
CONST_ENTRY(213)	/* OBJ_id_smime_aa_securityLabel    1 2 840 113549 1 9 16 2 2 */
CONST_ENTRY(214)	/* OBJ_id_smime_aa_mlExpandHistory  1 2 840 113549 1 9 16 2 3 */
CONST_ENTRY(215)	/* OBJ_id_smime_aa_contentHint      1 2 840 113549 1 9 16 2 4 */
CONST_ENTRY(216)	/* OBJ_id_smime_aa_msgSigDigest     1 2 840 113549 1 9 16 2 5 */
CONST_ENTRY(217)	/* OBJ_id_smime_aa_encapContentType 1 2 840 113549 1 9 16 2 6 */
CONST_ENTRY(218)	/* OBJ_id_smime_aa_contentIdentifier 1 2 840 113549 1 9 16 2 7 */
CONST_ENTRY(219)	/* OBJ_id_smime_aa_macValue         1 2 840 113549 1 9 16 2 8 */
CONST_ENTRY(220)	/* OBJ_id_smime_aa_equivalentLabels 1 2 840 113549 1 9 16 2 9 */
CONST_ENTRY(221)	/* OBJ_id_smime_aa_contentReference 1 2 840 113549 1 9 16 2 10 */
CONST_ENTRY(222)	/* OBJ_id_smime_aa_encrypKeyPref    1 2 840 113549 1 9 16 2 11 */
CONST_ENTRY(223)	/* OBJ_id_smime_aa_signingCertificate 1 2 840 113549 1 9 16 2 12 */
CONST_ENTRY(224)	/* OBJ_id_smime_aa_smimeEncryptCerts 1 2 840 113549 1 9 16 2 13 */
CONST_ENTRY(225)	/* OBJ_id_smime_aa_timeStampToken   1 2 840 113549 1 9 16 2 14 */
CONST_ENTRY(226)	/* OBJ_id_smime_aa_ets_sigPolicyId  1 2 840 113549 1 9 16 2 15 */
CONST_ENTRY(227)	/* OBJ_id_smime_aa_ets_commitmentType 1 2 840 113549 1 9 16 2 16 */
CONST_ENTRY(228)	/* OBJ_id_smime_aa_ets_signerLocation 1 2 840 113549 1 9 16 2 17 */
CONST_ENTRY(229)	/* OBJ_id_smime_aa_ets_signerAttr   1 2 840 113549 1 9 16 2 18 */
CONST_ENTRY(230)	/* OBJ_id_smime_aa_ets_otherSigCert 1 2 840 113549 1 9 16 2 19 */
CONST_ENTRY(231)	/* OBJ_id_smime_aa_ets_contentTimestamp 1 2 840 113549 1 9 16 2 20 */
CONST_ENTRY(232)	/* OBJ_id_smime_aa_ets_CertificateRefs 1 2 840 113549 1 9 16 2 21 */
CONST_ENTRY(233)	/* OBJ_id_smime_aa_ets_RevocationRefs 1 2 840 113549 1 9 16 2 22 */
CONST_ENTRY(234)	/* OBJ_id_smime_aa_ets_certValues   1 2 840 113549 1 9 16 2 23 */
CONST_ENTRY(235)	/* OBJ_id_smime_aa_ets_revocationValues 1 2 840 113549 1 9 16 2 24 */
CONST_ENTRY(236)	/* OBJ_id_smime_aa_ets_escTimeStamp 1 2 840 113549 1 9 16 2 25 */
CONST_ENTRY(237)	/* OBJ_id_smime_aa_ets_certCRLTimestamp 1 2 840 113549 1 9 16 2 26 */
CONST_ENTRY(238)	/* OBJ_id_smime_aa_ets_archiveTimeStamp 1 2 840 113549 1 9 16 2 27 */
CONST_ENTRY(239)	/* OBJ_id_smime_aa_signatureType    1 2 840 113549 1 9 16 2 28 */
CONST_ENTRY(240)	/* OBJ_id_smime_aa_dvcs_dvc         1 2 840 113549 1 9 16 2 29 */
CONST_ENTRY(241)	/* OBJ_id_smime_alg_ESDHwith3DES    1 2 840 113549 1 9 16 3 1 */
CONST_ENTRY(242)	/* OBJ_id_smime_alg_ESDHwithRC2     1 2 840 113549 1 9 16 3 2 */
CONST_ENTRY(243)	/* OBJ_id_smime_alg_3DESwrap        1 2 840 113549 1 9 16 3 3 */
CONST_ENTRY(244)	/* OBJ_id_smime_alg_RC2wrap         1 2 840 113549 1 9 16 3 4 */
CONST_ENTRY(245)	/* OBJ_id_smime_alg_ESDH            1 2 840 113549 1 9 16 3 5 */
CONST_ENTRY(246)	/* OBJ_id_smime_alg_CMS3DESwrap     1 2 840 113549 1 9 16 3 6 */
CONST_ENTRY(247)	/* OBJ_id_smime_alg_CMSRC2wrap      1 2 840 113549 1 9 16 3 7 */
CONST_ENTRY(125)	/* OBJ_zlib_compression             1 2 840 113549 1 9 16 3 8 */
CONST_ENTRY(248)	/* OBJ_id_smime_cd_ldap             1 2 840 113549 1 9 16 4 1 */
CONST_ENTRY(249)	/* OBJ_id_smime_spq_ets_sqt_uri     1 2 840 113549 1 9 16 5 1 */
CONST_ENTRY(250)	/* OBJ_id_smime_spq_ets_sqt_unotice 1 2 840 113549 1 9 16 5 2 */
CONST_ENTRY(251)	/* OBJ_id_smime_cti_ets_proofOfOrigin 1 2 840 113549 1 9 16 6 1 */
CONST_ENTRY(252)	/* OBJ_id_smime_cti_ets_proofOfReceipt 1 2 840 113549 1 9 16 6 2 */
CONST_ENTRY(253)	/* OBJ_id_smime_cti_ets_proofOfDelivery 1 2 840 113549 1 9 16 6 3 */
CONST_ENTRY(254)	/* OBJ_id_smime_cti_ets_proofOfSender 1 2 840 113549 1 9 16 6 4 */
CONST_ENTRY(255)	/* OBJ_id_smime_cti_ets_proofOfApproval 1 2 840 113549 1 9 16 6 5 */
CONST_ENTRY(256)	/* OBJ_id_smime_cti_ets_proofOfCreation 1 2 840 113549 1 9 16 6 6 */
CONST_ENTRY(150)	/* OBJ_keyBag                       1 2 840 113549 1 12 10 1 1 */
CONST_ENTRY(151)	/* OBJ_pkcs8ShroudedKeyBag          1 2 840 113549 1 12 10 1 2 */
CONST_ENTRY(152)	/* OBJ_certBag                      1 2 840 113549 1 12 10 1 3 */
CONST_ENTRY(153)	/* OBJ_crlBag                       1 2 840 113549 1 12 10 1 4 */
CONST_ENTRY(154)	/* OBJ_secretBag                    1 2 840 113549 1 12 10 1 5 */
CONST_ENTRY(155)	/* OBJ_safeContentsBag              1 2 840 113549 1 12 10 1 6 */
CONST_ENTRY(34)	/* OBJ_idea_cbc                     1 3 6 1 4 1 188 7 1 1 2 */
CONST_END(obj_objs)


#define sn_objs OPENSSL_GLOBAL_ARRAY_NAME(sn_objs)
#define ln_objs OPENSSL_GLOBAL_ARRAY_NAME(ln_objs)
#define obj_objs OPENSSL_GLOBAL_ARRAY_NAME(obj_objs)

