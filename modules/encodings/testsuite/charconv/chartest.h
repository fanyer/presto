/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#if !defined CHARTEST_H && (defined SELFTEST || defined ENCODINGS_REGTEST)
#define CHARTEST_H

enum
{
    ENC_TESTBUF_SIZE = 5000
};

// Single-byte
int encodings_test_iso_8859_1(void);
int encodings_test_iso_8859_7_2003(void);
int encodings_test_ascii(void);
int encodings_test_iso_8859_2(void);
int encodings_test_iso_8859_3(void);
int encodings_test_iso_8859_4(void);
int encodings_test_iso_8859_5(void);
int encodings_test_iso_8859_6(void);
int encodings_test_iso_8859_7(void);
int encodings_test_iso_8859_8(void);
int encodings_test_iso_8859_10(void);
int encodings_test_iso_8859_11(void);
int encodings_test_iso_8859_13(void);
int encodings_test_iso_8859_14(void);
int encodings_test_iso_8859_15(void);
int encodings_test_iso_8859_16(void);
int encodings_test_ibm866(void);
int encodings_test_windows_1250(void);
int encodings_test_windows_1251(void);
int encodings_test_windows_1252(void);
int encodings_test_windows_1253(void);
int encodings_test_windows_1254(void);
int encodings_test_windows_1255(void);
int encodings_test_windows_1256(void);
int encodings_test_windows_1257(void);
int encodings_test_windows_1258(void);
int encodings_test_mac_roman(void);
int encodings_test_mac_ce(void);
int encodings_test_mac_cyrillic(void);
int encodings_test_mac_greek(void);
int encodings_test_mac_turkish(void);
int encodings_test_viscii(void);
int encodings_test_koi8r(void);
int encodings_test_koi8u(void);

// Unicode
int encodings_test_utf8(void);
int encodings_test_utf16(void);

#ifdef ENCODINGS_HAVE_UTF7
int encodings_test_utf7(void);
#endif

// Chinese
#ifdef ENCODINGS_HAVE_CHINESE
int encodings_test_big5(void);
int encodings_test_big5_2003(void);
int encodings_test_big5hkscs(void);
int encodings_test_big5hkscs_2004(void);
int encodings_test_big5hkscs_2008(void);
int encodings_test_gbk(void);
int encodings_test_gb18030(void);
int encodings_test_euctw(void);
int encodings_test_iso2022cn(void);
int encodings_test_hz(void);
#endif

// Japanese
#ifdef ENCODINGS_HAVE_JAPANESE
int encodings_test_iso2022jp(void);
int encodings_test_iso2022jp1(void);
int encodings_test_shiftjis(void);
int encodings_test_eucjp(void);
#endif

// Korean
#ifdef ENCODINGS_HAVE_KOREAN
int encodings_test_euckr(void);
int encodings_test_iso2022kr(void);
#endif

#endif // !CHARTEST_H && (SELFTEST || ENCODINGS_REGTEST)
