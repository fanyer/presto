/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifdef XML_AIT_SUPPORT

#ifndef AIT_DATA_H
#define AIT_DATA_H


/** @short container class for the data in the MhpVersion element of the XML AIT */
class AITApplicationMhpVersion
{
public:
	AITApplicationMhpVersion() :
		profile(0), version_major(0), version_minor(0), version_micro(0) {}
	~AITApplicationMhpVersion() {}

	unsigned short profile;
	unsigned char version_major;
	unsigned char version_minor;
	unsigned char version_micro;
};


/** @short container class for the data in the ApplicationDescriptor element of the XML AIT */
class AITApplicationDescriptor
{
public:
    typedef enum {
        CONTROL_CODE_AUTOSTART = 1,
        CONTROL_CODE_PRESENT = 2,
        CONTROL_CODE_KILL = 4,
        CONTROL_CODE_DISABLED = 7,
        CONTROL_CODE_UNKNOWN = 100
    } ControlCode;

    typedef enum {
        VISIBILITY_NOT_VISIBLE_ALL = 0,
        VISIBILITY_NOT_VISIBLE_USERS = 1,
        VISIBILITY_VISIBLE_ALL = 3,
        VISIBILITY_UNKNOWN = 100
    } Visibility;

	AITApplicationDescriptor() :
		control_code(CONTROL_CODE_UNKNOWN), visibility(VISIBILITY_UNKNOWN),
		service_bound(TRUE), priority(0) {}
	~AITApplicationDescriptor();

	ControlCode control_code;
	Visibility visibility;
	BOOL service_bound;
	unsigned char priority;
	OpVector<AITApplicationMhpVersion> mhp_versions;
};


/** @short container class for the data in the ApplicationTransport element of the XML AIT */
class AITApplicationTransport
{
public:
	AITApplicationTransport() :
		protocol(0), remote(FALSE), original_network_id(0),
		transport_stream_id(0), service_id(0), component_tag(0) {}
	~AITApplicationTransport() {}

	unsigned short protocol;
	OpString base_url;
	BOOL remote;
	unsigned short original_network_id;
	unsigned short transport_stream_id;
	unsigned short service_id;
	unsigned char component_tag;
};

/** @short container class for the data in the Application element of the XML AIT */
class AITApplication
{
public:
	AITApplication() : org_id(0), app_id(0), usage(0) {}
	~AITApplication();

	OpString name;
	unsigned long org_id;
	unsigned short app_id;
	AITApplicationDescriptor descriptor;
	OpString location;
	int usage;
	OpVector<AITApplicationTransport> transports;
	OpVector<OpString> boundaries;
};


/** @short Holds application data parsed from an AIT XML document.
 *
 * This class is a container for Application Information Table (AIT)
 * data that has been parsed from an AIT XML document.
 *
 * AIT data is information about JavaScript applications that can be
 * either web based or found in the object carousel in a broadcasted
 * mpeg-2 stream. The data includes information about the location
 * (and carriage) of the application, as well as signalling related to
 * the application lifecycle and security context of an application.
 *
 * The attributes matching the parameters in the container are defined
 * in ETSI TS 102 809: "Digital Video Broadcasting (DVB); Signalling
 * and carriage of interactive applications and services in Hybrid
 * broadcast/broadband environments". This class mainly supports the
 * part of ETSI TS 102 809 that is required by ETSI TS 102 796:
 * "Hybrid Broadcast Broadband TV"
 */
class AITData
{
public:
	AITData() {}
	~AITData();

	OpVector<AITApplication> applications;


#ifdef SELFTEST
	bool Compare(AITData &ait_data, OpString8 &err);
#endif // SELFTEST
};


#endif // AIT_DATA_H

#endif // XML_AIT_SUPPORT

