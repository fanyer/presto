/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _ROOT_STORE_VERSION_H_
#define _ROOT_STORE_VERSION_H_

#define SSL_Options_Version_old 0x03000000

#define SSL_Options_Version_old_3_00_a 0x03000001
#define SSL_Options_Version_3_01 0x03010000
#define SSL_Options_Version_3_50 0x03010001
#define SSL_Options_Version_3_61 0x03010002
#define SSL_Options_Version_3_62 0x03010003
#define SSL_Options_Version_4_10 0x03010004
#define SSL_Options_Version_NewBase 0x05000000
#define SSL_Options_Thirdparty_version 0x05000000
#define SSL_Options_Version_5_03 0x05000001
#define SSL_Options_Version_5_03a 0x05000002
#define SSL_Options_Version_5_03b 0x05000003
#define SSL_Options_Version_5_50 0x05050000
#define SSL_Options_Version_6_02 0x05050001
#define SSL_Options_Version_6_05 0x05050004
#define SSL_Options_Version_7_00 0x05050005
#define SSL_Options_Version_7_00a 0x05050006
#define SSL_Options_Version_7_00b 0x05050007
#define SSL_Options_Version_7_20  0x05050008
#define SSL_Options_Version_7_20a  0x05050009
#define SSL_Options_Version_7_50  0x0505000a
#define SSL_Options_Version_7_52  0x0505000b
#define SSL_Options_Version_7_60  0x0505000c
#define SSL_Options_Version_7_60a  0x0505000d
#define SSL_Options_Version_8_00  0x0505000e
#define SSL_Options_Version_8_00a  0x0505000f

// Leave some room for 8.x
#define SSL_Options_Version_9_00  0x05050020 
#define SSL_Options_Version_9_00a  0x05050021 
#define SSL_Options_Version_9_00b  0x05050022 
#define SSL_Options_Version_9_00c  0x05050023 
#define SSL_Options_Version_9_00d  0x05050024 

#define SSL_Options_Version_9_22  0x05050025 
#define SSL_Options_Version_9_26  0x05050026 

// leave some room for 9.2x
#define SSL_Options_Version_9_50  0x05050030
#define SSL_Options_Version_9_50a 0x05050031
#define SSL_Options_Version_9_50b 0x05050032

#define SSL_Options_Version_9_51 0x05050033
#define SSL_Options_Version_9_52 0x05050034

#define SSL_Options_Version_9_64 0x05050035
#define SSL_Options_Version_9_65 0x05050036
#define SSL_Options_Version_10_0 0x05050037
#define SSL_Options_Version_10_01 0x05050038
#define SSL_Options_Version_10_52 0x05050039
#define SSL_Options_Version_10_61 0x0505003A
#define SSL_Options_Version_11_10 0x0505003B
#define SSL_Options_Version_11_51 0x0505003C
#define SSL_Options_Version_11_52 0x0505003D
#define SSL_Options_Version_11_60 0x0505003E
#define SSL_Options_Version_11_62 0x0505003F
#define SSL_Options_Version_12_00 0x05050041

// DO NOT CHANGE!!: First UTF-8 version 
#define SSL_UTF8_Version SSL_Options_Version_5_50
#define SSL_Options_Mask   0xffffff00

#define SSL_Options_Version_AutoTurnOffTLS11 SSL_Options_Version_8_00a
#define SSL_Options_Version_AutoTurnOnTLS11 SSL_Options_Version_9_00

// KEEP THIS UPDATED
#define SSL_Options_Version SSL_Options_Version_12_00

#endif // _ROOT_STORE_VERSION_H_
