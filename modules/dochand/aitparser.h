/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifdef XML_AIT_SUPPORT

#ifndef AIT_PARSER_H
#define AIT_PARSER_H

#include "modules/url/url2.h"
#include "modules/windowcommander/AITData.h"

/**
 * This class parses Application Information Table (AIT) data received
 * in an AIT XML document.
 *
 * AIT data is information about JavaScript applications that can be
 * either web based or found in the object carousel in a broadcasted
 * mpeg-2 stream. The data includes information about the location
 * (and carriage) of the application, as well as signalling related to
 * the application lifecycle and security context of an application.
 *
 * The XML syntax used is defined in ETSI TS 102 809: "Digital Video
 * Broadcasting (DVB); Signalling and carriage of interactive
 * applications and services in Hybrid broadcast/broadband
 * environments". The parse function of this class currently mainly
 * supports the part of ETSI TS 102 809 that is required by ETSI TS
 * 102 796: "Hybrid Broadcast Broadband TV".
 *
 * Note that the parse function in this class include no validation of
 * the parsed values!
 *
 * Specifications:
 * - https://projects.oslo.osa/Ulysses/specifications/etsi/ts_102809v010101p_AIT.pdf
 * - https://projects.oslo.osa/Ulysses/specifications/etsi/ts_102809v010101p0.zip
 * - https://projects.oslo.osa/Ulysses/specifications/hbbtv/ETSI-TS-102796-1.1.1.pdf
 */
class AITParser
{
private:
	AITParser() {}

public:
   /** @short Parse data from an URL containing an AIT XML document.
    *
    * Parses the AIT (Application Information Table) data recieved in
    * the input URL into the AITData. The URL should be of type
    * URL_XML_AIT.
    *
    * @param url URL to parse data from.
    * @param data container for the parsed data.
    * @return Memory error if OOK, OK otherwise.
    */
   static OP_STATUS Parse(URL &url, AITData &ait_data);
};

#endif // AIT_PARSER_H

#endif // XML_AIT_SUPPORT
