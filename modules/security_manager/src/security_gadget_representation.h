/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef SECURITY_GADGET_REPRESENTATION_H
#define SECURITY_GADGET_REPRESENTATION_H

#include "modules/security_manager/src/security_utilities.h"
#include "modules/util//simset.h"

class GadgetSecurityPolicy;
class GadgetSecurityDirective;
class GadgetAccess;
class GadgetPrivateNetwork;
class GadgetBlacklist;

/**
 * @file security_gadget_representation.h
 *
 * GadgetSecurityPolicy holds a security policy fragment.  There are
 *    two sources:
 *
 *      - global policy (from widgets.xml)
 *      - per-widget policy (from config.xml)
 *
 *	A GadgetSecurityPolicy is the owner of all the policy data it points
 *	to, except whatever is referenced through the 'next' field.  Its
 *   destructor will delete all those data.
 *
 *	Widget security policies can't override the global security policy.
 *	They can only be more restrictive than the global security policy.
 *
 * @author lasse@opera.com
*/

class GadgetSecurityPolicy
	: public Link
{
public:
	typedef enum
	{
		GLOBAL,			// for widgets.xml
		GADGET			// for config.xml
	} PolicyContext;


	static OP_STATUS Make(GadgetSecurityPolicy** new_obj, PolicyContext context);
	virtual ~GadgetSecurityPolicy();

	OP_STATUS SetName(const uni_char *name) { return signature_name.Set(name); }

	BOOL3 plugin_content;				// Gadget may load plugins
	BOOL3 webserver_content;			// Gadget may use embedded server
	BOOL3 file_content;					// Gadget may use file API
	BOOL3 java_content;					// Gadget may embed java content
	BOOL3 jsplugin_content;				// Gadget may use jsplugins

	GadgetAccess			*access;
	GadgetPrivateNetwork	*private_network;
	GadgetBlacklist			*blacklist;

	BOOL restricted;					// if TRUE, deny all access. */

	PolicyContext context;

	OpString signature_name;

private:
	GadgetSecurityPolicy(PolicyContext context);
	OP_STATUS Construct();
};

/**
 * @file security_gadget_representation.h
 *
 * GadgetAccess represents access directive.
 *
 * @author lasse@opera.com
*/

class GadgetAccess
{
public:
	static OP_STATUS Make(GadgetAccess** new_obj);
	virtual ~GadgetAccess() { entries.Clear(); }

	Head entries;
	BOOL all;
	/** @return TRUE if there is any rule allowing network access */
	BOOL IsAnyAccessAllowed() { return entries.First() || all; }
protected:
	GadgetAccess() : all(FALSE) {};
};


/**
 * @file security_gadget_representation.h
 *
 * GadgetPrivateNetwork represents private-network directive.
 *
 * @author lasse@opera.com
*/

class GadgetPrivateNetwork
{
public:
	enum ALLOW
	{
		UNKNOWN			= 0,
		NONE			= 1,
		RESTRICTED		= 2,
		UNRESTRICTED	= 4,
		INTRANET		= 8,
		INTERNET		= 16
	};

	static OP_STATUS Make(GadgetPrivateNetwork** new_obj);
	virtual ~GadgetPrivateNetwork() { entries.Clear(); };

	UINT8 allow;				// Allow attribute of private-network element.
	Head entries;
private:
	GadgetPrivateNetwork() : allow(INTRANET | INTERNET | UNRESTRICTED) {};
};


/**
 * @file security_gadget_representation.h
 *
 * Data structure that maintains an exclusion and a set of inclusions for
 * a blacklist element.
 */
class GadgetBlacklist
{
public:
	static OP_STATUS Make(GadgetBlacklist** new_obj);
	virtual ~GadgetBlacklist() { excludes.Clear(); includes.Clear(); }

	Head excludes;
	Head includes;
};

/**
 * @file security_gadget_representation.h
 *
 * GadgetActionUrl represents the following elements:
 *
 *	 - protocol
 *	 - host
 *	 - port
 *	 - path
 *
 *	 These elements are present in different sections of a policy.
 *	 Therefore, according to the parent element (access, private-network),
 *	 instances of this class may be used in different ways. And a GadgetActionUrl
 *	 object does not contain more than one type of information - it can not
 *	 be protocol and host at the same time, for instance.
 *
 *	 This class models data and implement possible operations with such data.
 *	 It contains comparison operators, which are used during parsing of widget
 *	 policies. These policies may not override global policy. To checking if an
 *	 URL is in the range of a certain GadgetActionUrl, method AllowAccessTo
 *	 shall be called. For example, if GadgetActionUrl object is a port range,
 *	 AllowAccessTo extracts the port from input URL and checks if it is range
 *	 represented by callee object.
 *
 * @author lasse@opera.com
 */

class GadgetActionUrl : public Link
{
public:
	enum FieldType
	{
		FIELD_NONE		= 0,
		FIELD_PROTOCOL	= 1,
		FIELD_HOST		= 2,
		FIELD_PORT		= 3,
		FIELD_PATH		= 4
	};

	enum DataType
	{
		DATA_NONE			= 0,
		DATA_HOST_LOCALHOST = 1,
		DATA_HOST_STRING	= 2,
		DATA_HOST_IP		= 3
	};

	static OP_STATUS Make(GadgetActionUrl** new_obj, const uni_char* value, FieldType field, DataType data = DATA_NONE);
	virtual ~GadgetActionUrl();

	BOOL AllowAccessTo(const URL& url, OpSecurityState& state);

	/**
	 * Return the type of field which is stored in this object.
	 */
	FieldType GetFieldType() const { return ft; }

	/**
	 * Return the protocol as a URLType.
	 * @param[out] protocol URLType for protocol.
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR if the current object does not contain a protocol entry.
	 */
	OP_STATUS GetProtocol(URLType &protocol) const;
	/**
	 * Return the protocol as a string.
	 * @param[out] protocol String representation of protocol, e.g. @c "http" or @c "" for unknown protocol types.
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR if the current object does not contain a protocol entry.
	 */
	OP_STATUS GetProtocol(OpString &protocol) const;
	/**
	 * Return the hostname or ip-address as a string in @a host.
	 * @param[out] protocol Hostname or ip-address.
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR if the current object does not contain a host entry.
	 */
	OP_STATUS GetHost(OpString &host) const;
	/**
	 * Return the port range.
	 * @param[out] low The lowest port in the range.
	 * @param[out] high The highest port in the range.
	 * @return OK if successful or ERR if the current object does not contain a port entry.
	 */
	OP_STATUS GetPort(unsigned &low, unsigned &high) const;
	/**
	 * Return the path as a string.
	 * @param[out] path The path, e.g. "/"
	 * @return OK if successful, ERR_NO_MEMORY on OOM, ERR if the current object does not contain a path entry.
	 */
	OP_STATUS GetPath(OpString &path) const;

	GadgetActionUrl* Suc() { return (GadgetActionUrl*) Link::Suc(); }

private:
	GadgetActionUrl();
	OP_STATUS Construct(const uni_char* value, FieldType field, DataType data);

private:
	FieldType ft;
	DataType dt;

	union u_field
	{
		URLType protocol;

		IPRange host;				// May be an IP address or a range of IP addresses

		struct s_port
		{
			unsigned int low;		// First port allowed
			unsigned int high;		// Last port allowed
		} port;

		uni_char* value;			// Used to represent host name or path
	};

	u_field field;

	enum WILDCARD_MATCH_STATUS
	{
		IS_MATCH	= 0,
		NO_MATCH	= 1
	};
	WILDCARD_MATCH_STATUS WildcardCmp(const uni_char *expr, const uni_char *data) const;
	// simple wildcard comparison. *.opera.com will match ftp.opera.com and so on.
};

/**
 * @file security_gadget_representation.h
 *
 * A small helper class
 *
 */

class GadgetActrionUrlHead : public Head
{
public:
	GadgetActionUrl* First() { return static_cast<GadgetActionUrl*>(Head::First()); }
	virtual ~GadgetActrionUrlHead() { Clear(); }
};

/**
 * @file security_gadget_representation.h
 *
 * GadgetSecurityDirective represents directives in policy file.
 *
 * @author lasse@opera.com
*/

class GadgetSecurityDirective : public Link
{
public:
	virtual ~GadgetSecurityDirective();

	static OP_STATUS Make(GadgetSecurityDirective **new_obj);

	GadgetActrionUrlHead protocol;	// List of protocol elements in the directive.
	GadgetActrionUrlHead host;		// List of host elements in the directive.
	GadgetActrionUrlHead port;		// List of port elements in the directive.
	GadgetActrionUrlHead path;		// List of path elements in the directive.

protected:
	friend class GadgetBlacklist;
	GadgetSecurityDirective();
};

#endif // SECURITY_GADGET_REPRESENTATION_H
