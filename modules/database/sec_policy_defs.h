/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _MODULES_DATABASE_SEC_POLICY_DEFS_H_
#define _MODULES_DATABASE_SEC_POLICY_DEFS_H_

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

/**
 * Global policy NULL policy -> no limits. For trusted uses
 * */
class OpDefaultGlobalPolicy : public PS_Policy
{
public:
	OpDefaultGlobalPolicy(PS_Policy* parent = NULL) : PS_Policy(parent) {}
	virtual ~OpDefaultGlobalPolicy() {}

	virtual OpFileLength    GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;
	virtual unsigned        GetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;
	/** NOTE: As documented in PS_Policy, the returned pointer is only
	 * valid until the next call to GetAttribute, and 0 is a valid
	 * return value.
	 */
	virtual const uni_char* GetAttribute(SecAttrUniStr attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;

private:
	mutable OpString m_getattr_returnbuffer;
};


#ifdef DATABASE_STORAGE_SUPPORT
/**
 * Global policy for webpage databases
 * Will have the default policy as its parent
 * */
class WSD_DatabaseGlobalPolicy : public PS_Policy
{
public:
	WSD_DatabaseGlobalPolicy(PS_Policy* parent = NULL) : PS_Policy(parent) {}
	virtual ~WSD_DatabaseGlobalPolicy() {}

	virtual OpFileLength    GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;
	virtual unsigned        GetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;
	/** NOTE: As documented in PS_Policy, the returned pointer is only
	 * valid until the next call to GetAttribute, and 0 is a valid
	 * return value.
	 */
	virtual const uni_char* GetAttribute(SecAttrUniStr attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;

	virtual OP_STATUS		SetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, OpFileLength new_value, const uni_char* domain = NULL, const Window* window = NULL);
	virtual OP_STATUS		SetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, unsigned new_value, const uni_char* domain = NULL, const Window* window = NULL);

	virtual BOOL IsConfigurable(SecAttrUint64 attr) const;
	virtual BOOL IsConfigurable(SecAttrUint   attr) const;
};
#endif // DATABASE_STORAGE_SUPPORT

#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
/**
 * Global policy for webpage localStorage
 * Will have the default policy as it's parent
 * */
class WebStoragePolicy : public PS_Policy
{
	PS_Policy::PS_ObjectType m_type;
public:
	WebStoragePolicy(PS_Policy::PS_ObjectType type, PS_Policy* parent = NULL)
		: PS_Policy(parent), m_type(type) {}
	virtual ~WebStoragePolicy() {}

	virtual OpFileLength    GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;
	virtual unsigned        GetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;

	virtual OP_STATUS		SetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, OpFileLength new_value, const uni_char* domain = NULL, const Window* window = NULL);
	virtual OP_STATUS		SetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, unsigned new_value, const uni_char* domain = NULL, const Window* window = NULL);

	virtual BOOL IsConfigurable(SecAttrUint64 attr) const;
	virtual BOOL IsConfigurable(SecAttrUint   attr) const;
};

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
/**
 * Global policy for widget.preferences
 * Will have the default policy as it's parent
 * */
class WidgetPreferencesPolicy : public PS_Policy
{
	PS_Policy::PS_ObjectType m_type;
public:
	WidgetPreferencesPolicy(PS_Policy* parent = NULL)
		: PS_Policy(parent) {}
	virtual ~WidgetPreferencesPolicy() {}

	virtual OpFileLength    GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;
	virtual unsigned        GetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;

	virtual OP_STATUS		SetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, OpFileLength new_value, const uni_char* domain = NULL, const Window* window = NULL);
	virtual OP_STATUS		SetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, unsigned new_value, const uni_char* domain = NULL, const Window* window = NULL);

	virtual BOOL IsConfigurable(SecAttrUint64 attr) const;
	virtual BOOL IsConfigurable(SecAttrUint   attr) const;
};

#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
/**
 * Global policy for user script sstorage
 * Will have the default policy as it's parent
 * */
class WebStorageUserScriptPolicy : public PS_Policy
{
public:
	WebStorageUserScriptPolicy(PS_Policy* parent = NULL)
		: PS_Policy(parent){}
	virtual ~WebStorageUserScriptPolicy() {}

	virtual OpFileLength    GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;
	virtual unsigned        GetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;

	virtual BOOL IsConfigurable(SecAttrUint64 attr) const;
	virtual BOOL IsConfigurable(SecAttrUint   attr) const;
};

#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#ifdef SUPPORT_DATABASE_INTERNAL
/*************
 * Implementation of the several sql validators
 *************/
class TrustedSqlValidator: public SqlValidator
{
public:
	TrustedSqlValidator() {}
	virtual ~TrustedSqlValidator() {}
	virtual int Validate(int sqlite_action, const char* d1, const char* d2, const char* d3, const char* d4) const;
	virtual Type GetType() const { return TRUSTED; }
};

class UntrustedSqlValidator: public SqlValidator
{
public:
	UntrustedSqlValidator() {}
	virtual ~UntrustedSqlValidator() {}
	virtual int Validate(int sqlite_action, const char* d1, const char* d2, const char* d3, const char* d4) const;
	virtual Type GetType() const { return UNTRUSTED; }
};
#endif //SUPPORT_DATABASE_INTERNAL

#endif //DATABASE_MODULE_MANAGER_SUPPORT

#endif //_MODULES_DATABASE_SEC_POLICY_DEFS_H_
