/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Marius Blomli, Michal Zajaczkowski
 */

#ifndef _UPDATEABLERESOURCE_H_INCLUDED_
#define _UPDATEABLERESOURCE_H_INCLUDED_

#ifdef AUTO_UPDATE_SUPPORT

#include "modules/xmlutils/xmlfragment.h"

class UpdatableResource;

/**
 * Implementations of the UpdatableResource class need to be able to store properties of the represented file, i.e. a mime-type for a plugin.
 * The properties are stored dynamically in order to not to hardcode parsing of the response XML and passing the information to hardcoded string 
 * properties of the objects.
 *
 * Each of the URAttr enum items represents one XML element that may be found in the response XML, i.e.
 * Each value is held as a string value. In case you belive a specific attribute is parseable as int, you can try to get it using the int version of the GetAttrValue()
 * method that will try to parse the value for you.
 *
 * <plugin>
 *  <mime-type>application/x-test-mime-type</mime-type>
 *  <has-installer>0</has-installer>
 * </plugin>
 *
 * The value found for the <mime-type> element is stored and accesed using the URA_MIME_TYPE value.
 * In order to make the parsing possible, the values of this enum need to be coupled with the string values found in the UpdatableResourceFactory::URAttrName[]
 * array.
 * All that is important here is: 
 * 1. You need to add new XML element descriptions to the URAttr enum if you add a new resource type(s) described by new XML elements;
 * 2. You need to add new XML element names to the UpdatableResourceFactory::URAttrName[] array if you add a new resource type(s) described by new XML elements;
 * 3. The items in the URAttr enum *MUST* have the same order that the strings in the UpdatableResourceFactory::URAttrName[] have;
 * 4. The last item in the URAttr enum *MUST* be URA_LAST.
  */
enum URAttr
{
	URA_TYPE = 0,
	URA_NAME,
	URA_FOLDER,
	URA_URL,
	URA_URLALT,
	URA_VERSION,
	URA_BUILDNUM,
	URA_ID,
	URA_SIZE,
	URA_LANG,
	URA_TIMESTAMP,
	URA_CHECKSUM,
	URA_INFOURL,
	URA_SHOWDIALOG,
	URA_VENDOR_NAME,
	URA_INSTALL_PARAMS,
	URA_EULA_URL,
	URA_PLUGIN_NAME,
	URA_MIME_TYPE,
	URA_INSTALLS_SILENTLY,
	URA_HAS_INSTALLER,
	URA_SECTION,
	URA_KEY,
	URA_DATA,
	URA_FILE,
	/***CAREFUL*WHEN*ADDING*VALUES*HERE*SEE*THE*UpdatableResourceFactory::URAttrName[]*ARRAY***/

	URA_LAST
};

/**
 * Class that represents an UpdatableResource property coupled with the value for that property as found in the autoupdate XML response.
 *
 * Example:
 * <mime-type>application/x-mime-type</mime-type> => URANameValuePair::m_attr_name = URA_MIME_TYPE, URANameValuePair::m_attr_value = "application/x-mime-type"
  */
class URNameValuePair
{
	friend class UpdatableResource;
	friend class UpdatableResourceFactory;

private:
	/**
	 * Return the attribute that this pair represents, i.e. URA_MIME_TYPE.
	 *
	 * returns - A value of URAttr type representing the XML element name that this pair represents.
	 */
	URAttr GetAttrName() { return m_attr_name; }

	/**
	 * Set the attribute that this pair is to represent, i.e. URA_MIME_TYPE.
	 *
	 * @param attr_name - The URAttr value that this pair should represent.
	 */
	void SetAttrName(URAttr attr_name) { m_attr_name = attr_name; }

	/**
	 * Get the value that this pair represents, i.e. "application/x-mime-type".
	 *
	 * @param value - A reference to an OpString object that will hold the parameter value.
	 *
	 * @returns - OpStatus::OK if everything went OK, ERR otherwise.
	 */
	OP_STATUS GetAttrValue(OpString& value) { return value.Set(m_attr_value); }

	/**
	 * Set the value that this pair is to represent, i.e. "application/x-mime-type".
	 *
	 * @param value - The value that should be set for this pair.
	 *
	 * @returns - OpStatus::OK if the value was set with success, ERR otherwise.
	 */
	OP_STATUS SetValue(const OpStringC& value) { return m_attr_value.Set(value); }

	/**
	 * Checks if this pair represents an attribute of the given type.
	 *
	 * @param attr - The type that we want to check this pair against.
	 *
	 * @returns - TRUE if this pair has the given type, FALSE otherwise.
	 */
	BOOL IsAttr(URAttr attr) { return m_attr_name == attr; }

	/**
	 * The type of this pair, i.e. the XML element name that this pair represents
	 */
	URAttr m_attr_name;

	/**
	 * The value of this pair, i.e. the XML element value that this pair represents.
	 * Note that all XML element values are stored as strings and reparsed for an int value if needed at the time of 
	 * retrieving the value using the UpdatableResource::GetAttrValue() method.
	 */
	OpString m_attr_value;
};

/**
 * This class creates implementations of the UpdatableResource class with the proper XML name-value pairs as found in the autoupdate response XML.
 * This class is fed with an XMLFragment and returns the parsed object or NULL if an error occurred.
 */
class UpdatableResourceFactory
{
	friend class UpdatableResource;

public:
	/**
	 * Takes an XMLFragment representing an UpdatableFile (<file>...</file>) or an UpdatableSetting (<setting>...</setting>) and returns a pointer to 
	 * a newly created UpdatableResource* object. Will do all the work needed to parse thorugh the XML, recognize the XML element names and values and
	 * store them with the newly created object.
	 * Also, will choose the correct implementation of the UpdatableResource abstract basing on the <type> XML element found.
	 *
	 * @param resource_type - "file" or "setting" in order to be able to parse the XMLFragment correctly
	 * @param fragment - the XML fragment at the start of the resource description, i.e. right after the <file> or <setting> element
	 * @param resource - an UpdatableResource** pointer that will be changed to point to the newly created resource. Will be set to NULL if no resource 
	 *                   could be created with this call.
	 *
	 * @returns - OpStatus::OK if an object implementing the UpdatableResource abstract could be created successfuly with the given XML, ERR otherwise.
	 *            You receive the ownership of the resulting resource in case this function succeeds, and you get a NULL otherwise.
	 */
	static OP_STATUS	GetResourceFromXML(const OpStringC& resource_type, XMLFragment& fragment, UpdatableResource** resource);

protected:
	/** 
	* The maximal attribute count per resource. XML sanity safety mechanism, can be raised reasonably if needed.
	*/
	static const unsigned URA_MAX_PROPERTY_COUNT = 20;

	/**
	 * The array representing the XML element names as found in the request XML. See documentation in updatableresource.cpp.
	 */
	static const uni_char*			URAttrName[];

	/**
	 * Tries to convert the XML element name passed as the string parameter to a value of URAttr type. 
	 * Relies on the correct order of the elements in the URAttr enum and URAttrName[]!
	 *
	 * @param name - The string representing the XML element name, i.e. "mime-type".
	 *
	 * @returns - An item from the URAttr enum representing the given string (i.e. URA_MIME_TYPE), or URA_LAST in case the string could not be
	 *            recognized.
	 */
	static URAttr				ConvertNameToAttr(const OpStringC& name);

	/**
	 * Creates (i.e. OP_NEW()) a new resource of the correct type basing on the type string passed as the parameter.
	 * May return NULL if the resource type string is not recognized or on OOM.
	 *
	 * @param type - A string representing the resource type as found in the XML, i.e. <type>plugin</type>, additionally the "setting" type is recognized
	 *
	 * @returns - A pointer to the newly created object or NULL if something went wrong.
	 */
	static UpdatableResource*	GetEmptyResourceByType(const OpStringC& type);
};

/**
 * This is a base class for a representation of a resource (file or pref)
 * that is to be kept up to date by the auto update system.
 *
 * There are two primary subclasses, UpdatableFile and UpdatableSetting,
 * for holding information files and settings that should be kept up to date.
 * Instances of these classes are registered with the AutoUpdater, which 
 * stores them and checks them against whatever the status xml says when it's
 * downloaded. If the status xml has notifications of updates that match
 * updates that have been registered, they should be downloaded.
 *
 * UpdatableFile and Updatable Setting are both intended to be subclassed if
 * they need the CheckResource function to do something special, like 
 * integrity or signature checks.
 */
 
class UpdatableResource
{
	friend class UpdatableResourceFactory;
public:

	enum ResourceClass
	{
		File,
		Setting
	};

	/**
	 * Updatable resource type. This is what each implementation of this abstract class should return with the GetType() call.
	 */
	enum UpdatableResourceType
	{
		RTEmpty				= 0x0000,
		RTPackage			= 0x0001,
		RTPatch				= 0x0002,
		RTSpoofFile			= 0x0004,
		RTBrowserJSFile		= 0x0008,
		RTSetting			= 0x0010,
		RTDictionary		= 0x0040,
		RTPlugin			= 0x0080,
		RTHardwareBlocklist	= 0x0100,
		RTHandlersIgnore    = 0x0200,
		RTAll				= 0xFFFFFFFF
	};

	UpdatableResource();
	virtual ~UpdatableResource() {}

	/**
	 * Gets the type of resource
	 *
	 * @return returns the type of resource based on the UpdatableResourceType
	 */
	virtual UpdatableResourceType GetType() = 0;

	/**
	 * Gets the class of resource
	 *
	 * @return returns the type of resource based on the ResourceClass enum
	 */
	virtual ResourceClass GetResourceClass() = 0;

	/** 
	 * Function that checks whether a resource is ok to install. Override this
	 * to introduce signature/sanity checking of downloaded resources.
	 *
	 * @return TRUE if the resource checks out and can be used, FALSE otherwise.
	 */
	virtual BOOL CheckResource() = 0;

	/**
	 * Function that initiates the replacement of the old resource with the 
	 * updated one.
	 */
	virtual OP_STATUS UpdateResource() = 0;

	/**
	 * Function to clean up after updating.
	 */
	virtual OP_STATUS Cleanup() = 0;

	/**
	 * Get name of resource
	 */
	virtual const uni_char* GetResourceName() = 0;

	/**
	 * @return TRUE if resource requires an additional unpacking step to be executed
	 */
	virtual BOOL UpdateRequiresUnpacking() = 0;

	/**
	 * @return TRUE if resource implies a restart (of the Opera binary) for the update to complete
	 */
	virtual BOOL UpdateRequiresRestart() = 0;

	/**
	 * This defines the resource key that is needed to resolve the resource type as an addition.
	 * You only need to implement this if you want to use the particular resource type as an addition since
	 * additions need to have an unique key.
	 * The key for plugins is the mime-type, the key for dictionaries is the language ID.
	 *
	 * @returns URAttr name of the attribute that should serve as the key, i.e. URA_MIME_TYPE for plugins.
	 */
	virtual URAttr GetResourceKey() { return URA_LAST; };

	/**
	 * Retrieve the value of the given attribute as string.
	 *
	 * @param attr - URAttr name of the requested attribute
	 * @param value - the string value of the requested attribute, if found
	 *
	 * @returns OpStatus::OK if the attribute with the given name could be found and the value could be set OK, ERR otherwise
	 */
	OP_STATUS GetAttrValue(URAttr attr, OpString& value);

	/**
	 * Retrieve the value of the given attribute as int.
	 * Note that it might not be possible to parse the attribute as int and an OpStatus::ERR will be returned.
	 *
	 * @param attr - URAttr name of the requested attribute
	 * @param value - the int value of the attribute if it's possible to parse the value as int
	 *
	 * @returns OpStatus::OK if the attribute was found and could be parsed as int, ERR otherwise.
	 */
	OP_STATUS GetAttrValue(URAttr attr, int& value);

	/**
	 * Retrieve the value of the given attribute as bool.
	 * Note that it might not be possible to parse the attribute as bool and an OpStatus::ERR will be returned.
	 * The parser looks for the following strings: "1", "0", "true", "false", "yes", "no". If the attribute value
	 * doesn't match any of the above, ERR is returned and the value of the bool parameter is not determined.
	 *
	 * @param attr - URAttr name of the requested attribute
	 * @param value - the bool value of the attribute if it's possible to parse the value as int
	 *
	 * @returns OpStatus::OK if the attribute was found and could be parsed as int, ERR otherwise.
	 */
	OP_STATUS GetAttrValue(URAttr attr, bool& value);

protected:
	/**
	 * Helper method. Searches the given pairs vector for an attribute with the attr name, returns the value of the attribute with the value parameter.
	 *
	 * @param pairs - The input pairs vector that should be searched
	 * @param attr - The URAttr name of the attribute searched for
	 * @param value - The value of the found attribute, if any
	 *
	 * @returns OpStatus::OK if the attribute could be found and the value was set with success, ERR otherwise.
	 */
	static OP_STATUS GetValueByAttr(OpAutoVector<URNameValuePair>& pairs, URAttr attr, OpString& value);

	/**
	 * Implement this method to verify the attribute set after the response XML has been parsed.
	 * This method should return FALSE in case that the attribute list and/or the attribute values are not acceptable for the resource that implements
	 * this method. A resource that failes the attribute verification won't be returned as a valid resource by the StatusXMLDownloader after parsing the 
	 * response XML.
	 *
	 * returns TRUE if the resource is valid, FALSE otherwise.
	 */
	virtual BOOL VerifyAttributes() = 0;

	/**
	 * Add a name/value pair for this resource. Note that there is a limit of attribute count per resource, see URA_MAX_PROPERTY_COUNT.
	 *
	 * @param pair The name/value pair that should be added to this resource. Ownership of the URNameValuePair object is NOT passed to the UpdatableResource class.
	 *
	 * @returns OpStatus::OK if everything went OK, ERR otherwise.
	 */
	OP_STATUS AddNameValuePair(URNameValuePair* pair);

	/**
	 * Checks if this resource has an attribute with the given name that has any content, i.e. the string representing the value of the attribute is not empty.
	 *
	 * @returns TRUE if the above condition is fulfilled, FALSE otherwise.
	 */
	BOOL HasAttrWithContent(URAttr attr);

	/**
	 * Holds the name/value pairs.
	 */
	OpAutoVector<URNameValuePair> m_pairs;
};

#endif // AUTO_UPDATE_SUPPORT

#endif // _UPDATEABLERESOURCE_H_INCLUDED_
