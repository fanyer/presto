/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef ES_UTILS_OBJMAN_H
#define ES_UTILS_OBJMAN_H

#include "modules/ecmascript/ecmascript.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/adt/opvector.h"
#include "modules/dochand/docman.h"

class ES_Object;
class ES_Runtime;
class Window;
class DocumentTreeIterator;
class ES_ObjectPropertiesListener;
class ES_DebugObjectProperties;
class ES_DebugObjectChain;

/**
 * An ESU_ObjectManager can keep strong references to ES_Objects
 * and provide access to them via IDs.
 */
class ESU_ObjectManager
{
public:

	ESU_ObjectManager();
	/**< Create a new, empty ESU_ObjectManager. */

	OP_STATUS GetId(ES_Runtime *rt, ES_Object *obj, unsigned &id);
	/**< Find the id for an ES_Object, or create one if non-existent.

	     @param rt [in] The runtime containing the ES_Object. Can not be NULL.
	     @param obj [in] The object to create an ID for. Can not be NULL.
	     @param id [out] The id for the object. Zero on error.
	     @return OpStatus::OK, OpStatus::ERR_NO_MEMORY, or OpStatus::ERR if
	             the incoming ES_Object could not be Protected. */

	ES_Object *GetObject(ES_Runtime *rt, unsigned id);
	/**< Get the ES_Object with the specified ID.

	     @param rt The runtime for which contains the object represented by the ID.
	     @param id The ID of the ES_Object to find.
	     @return The corresponding ES_Object, or NULL if no such ES_Object is known. */

	ES_Object *GetObject(unsigned id);
	/**< Get the ES_Object with the specified ID.

	     @param id The ID of the ES_Object to find.
	     @return The corresponding ES_Object, or NULL if no such ES_Object is known. */

	void Release(ES_Runtime *rt, unsigned id);
	/**< Release an ES_Object to make it garbage-collectible. If no ES_Object with the
	     specifed ID is found, nothing happens.

	     @param rt The runtime for which contains the object represented by the ID.
	     @param id The ID for the object to release. */

	void Release(ES_Runtime *rt);
	/**< Release all objects on the specified runtime.

	     @param rt The runtime to release objects for. */

	void Release(unsigned id);
	/**< Release an ES_Object to make it garbage-collectible. If no ES_Object with the
	     specifed ID is found, nothing happens.

	     @param id The ID of the ES_Object to release. */

	void ReleaseAll();
	/**< Release all ES_Objects to make them garbage-collectible. */

	void Reset();
	/**< Release all ES_Objects and reset the ID counter to start at 1 again. */

private:

	/**
	 * The map contains a hash table to convert from ID to pointer, and
	 * vice-versa.
	 */
	struct ObjectMap
	{
		OpPointerIdHashTable<ES_Object, unsigned> objecttoid;
		/**< Maps ES_Object pointers to their IDs. */

		OpAutoINT32HashTable<ES_ObjectReference> idtoobject;
		/**< Maps IDs to ES_Objects. */
	};

	OP_STATUS GetObjectMap(ES_Runtime *rt, ObjectMap *&objmap);
	/** Get the ObjectMap for the specified ES_Runtime, or create one if
	    it does not already exist.

	    @param rt [in] The ES_Runtime to get an ObjectMap for.
	    @param objmap [out] The ObjectMap for the specified runtime on
	           OpStatus::OK, undefined for other return values.
	    @return OpStatus::OK on success, or OpStatus::ERR_NO_MEMORY. */

	ObjectMap *GetObjectMap(ES_Runtime *rt);
	/**< Get the ObjectMap for the specified ES_Runtime, but only if it
	     already exist (do not create one).

	     @return The ObjectMap for the specified runtime. */

	ObjectMap *FindObjectMap(unsigned id);
	/**< Find the ObjectMap that contains the specified ID, or NULL if
	     no such ObjectMap exists.
	
	    @param id The ID for the object we want to find.
	    @return The ObjectMap containing the object, or NULL if not found. */

	ES_Object *GetObject(ObjectMap *objmap, unsigned id);
	/**< Internal version of GetObject, which takes ObjectMap as a parameter. */

	void Release(ObjectMap *objmap, unsigned id);
	/**< Internal version of Release, which takes ObjectMap as a parameter. */

	OpAutoPointerHashTable<ES_Runtime, ObjectMap> object_maps;
	/**< One ObjectMap for each runtime. */

	unsigned nextid;
	/**< The IDs are unique per ESU_ObjectManager, and start at one. Zero
	     is an invalid ID. */
};

/**
 * Use this class to iterate through all available ES_Runtimes in Opera.
 */
class ESU_RuntimeIterator
{
public:

	ESU_RuntimeIterator(Window *first, BOOL create = TRUE);
	/**< Creates a new ESU_RuntimeIterator, which starts iteration from the
	     specified Window.

	     @param first The Window to start the iteration.
	     @param create Some pages do not have a DOM environment if there are no
	                   scripts on the page. Set this to TRUE if the DOM environment
	                   should be created on the fly for all documents. */

	OP_STATUS Next();
	/**< Find the next runtime.

	     @return OpStatus::OK if a runtime was found, OpStatus::ERR if
	             no runtime was found, or OOM. */

	ES_Runtime *GetRuntime();
	/**< Get the ES_Runtime previously found with a successful call of Next,
	     or NULL if no such ES_Runtime is available. */

private:
	ES_Runtime *runtime;
	Window *window;
	DocumentTreeIterator iter;
	BOOL create;
};

/**
 * This class maintains weak references between ES_Runtimes, and integral IDs.
 * Additionally, this class can find all ES_Runtimes (in all Windows), and extract
 * information about them.
 */
class ESU_RuntimeManager
{
public:

	ESU_RuntimeManager();
	/**< Create a new, empty ESU_RuntimeManager. */

	ES_Runtime *GetRuntime(unsigned id);
	/**< Find an ES_Runtime with a specific runtime ID.

	     @param id The ID of the ES_Runtime we wish to find.
	     @return The associated ES_Runtime, or NULL if nonexistent. */

	OP_STATUS GetId(ES_Runtime *rt, unsigned &id);
	/**< Gets the ID for an ES_Runtime, or create one if no ID exists.

	     @param [in] rt The ES_Runtime we want the ID for. Can not be NULL.
	     @param [out] The id of the ES_Runtime, or 0 on error.
	     @return OpStatus::OK if an ID was found or created, otherwise OOM. */

	void Reset();
	/**< Remove all ES_Runtimes and reset the ID counter to start at 1 again. */

	static OP_STATUS FindAllRuntimes(OpVector<ES_Runtime> &runtimes, BOOL create = TRUE);
	/**< Find all the ES_Runtimes in Opera.

	     @param [out] runtimes The found ES_Runtimes are put here.
	     @param [in] create TRUE to create DOM environments on the fly.
	            (See ESU_RuntimeIterator).
	     @return OpStatus::OK if all ES_Runtimes were found, otherwise OOM. */

	static unsigned GetWindowId(ES_Runtime *rt);
	/**< Gets the ID of the Window containing the specified ES_Runtime.

	     @param rt We want the associated Window ID for this ES_Runtime.
	     @return The nonzero Window ID, or zero if no Window is associated,
	             or if NULL was used as a parameter. */

	static OP_STATUS GetFramePath(ES_Runtime *rt, OpString &out);
	/**< Create the framepath for an ES_Runtime.

	     @param [in] rt The ES_Runtime we want the framepath for. Silent fail if NULL.
	     @param [out] out The framepath will be stored here.
	     @return OpStatus::OK if the framepath was generated, or OOM. */

	static OP_STATUS GetDocumentUri(ES_Runtime *rt, OpString &out);
	/**< Get the URI of an ES_Runtime.

	     @param [in] rt The ES_Runtime we want the URI for. Silent fail if NULL.
	     @param [out] out The framepath will be stored here.
	     @return OpStatus::OK if the framepath was generated, or OOM. */

private:

	OpPointerIdHashTable<ES_Runtime, unsigned> runtimetoid;
	/**< Maps ES_Runtime pointers to their IDs. */

	OpINT32HashTable<ES_Runtime> idtoruntime;
	/**< Maps IDs to ES_Runtime. */

	unsigned nextid;
	/**< The IDs are unique per ESU_RuntimeManager, and start at one. Zero
	     is an invalid ID. */
};

class ES_GetSlotHandler;
/**< ES_PropertyHandler handles callbacks from object property lookups, directly or via GetSlot().*/
class ES_PropertyHandler
{
protected:
	OpVector<ES_GetSlotHandler> m_getslotshandlers;

public:
	virtual void OnError() { }
	/**< Will be called on failed GetSlot() calls, or OOM. Other properties will be tried read. */

	virtual void OnDebugObjectChainCreated(ES_DebugObjectChain *targetstruct, unsigned count) { }
	/**< Object target has been allocated. */

	virtual OP_STATUS OnPropertyValue(ES_DebugObjectProperties *properties, const uni_char *propertyname, const ES_Value &result, unsigned propertyindex, BOOL exclude, GetNativeStatus getstatus) { return OpStatus::OK; }
	/**< A property value has been evaled and is ready if getstatus is GETNATIVE_SUCCESS, else result is undefined. exclude mean skip the property. */

	virtual OP_STATUS AddObjectPropertyListener(ES_Object *owner, unsigned property_count, ES_DebugObjectProperties* properties, BOOL chain_info) { return OpStatus::OK; }
	/**< Creates an ES_GetSlotHandler for the owner properties. ES_PropertyHandler should allocate space for the property_count in properties.
	     chain_info is used as parameter to ExportObject when the property result is finished and properties is filled. */

	virtual OP_STATUS GetSlotHandlerCreated(ES_GetSlotHandler* slothandler)
	{
		return m_getslotshandlers.Add(slothandler);
	}
	/**< ES_PropertyHandler owns the ES_GetSlotHandler until GetSlotHandlerDied() is called. */

	virtual OP_STATUS GetSlotHandlerDied(ES_GetSlotHandler* slothandler)
	{
		return m_getslotshandlers.RemoveByItem(slothandler);
	}
	/**< ES_GetSlotHandler signal back when it dies. */

	virtual ES_Thread *GetBlockedThread() { return NULL; };
	/**< Interrupted thread if any. Sent as parameter to GetSlot(). */

};

/**
 * Can provide additional information about ES_Objects, such as the class name
 * or properties of the object.
 */
class ESU_ObjectExporter
{
public:

	/**
	 * When a call is made to ExportObject and ExportProperties, the reply is
	 * sent to an object implementing this interface.
	 */
	class Handler : public ES_PropertyHandler
	{
	public:
		virtual OP_STATUS Object(BOOL is_callable, const char *class_name, const uni_char *function_name, ES_Object *prototype) = 0;
		/**< Called when additional information about an ES_Object is ready.

		     @param is_callable Whether the ES_Object can be called (like a function).
		     @param class_name The class of the ES_Object, for instance "Object" or "HTMLDocument".
		     @param function_name If the ES_Object is a function, this will contain the name of that
		                          function. If the function is anonymous, 'function_name' may contain
		                          the 'alias' of the function. May also be NULL of no name is available.
		     @param prototype The prototype for the ES_Object, or NULL if there is none. */

		virtual OP_STATUS PropertyCount(unsigned count) = 0;
		/**< Called when the number of properties on the ES_Object has been counted. Triggered
		     by a call to ExportProperties.

		     @param count The number of properties on the ES_Object.
		     @return OpStatus::ERR* will cause the caller (ExportProperties) to return
		             with the same status message. */

		virtual OP_STATUS Property(const uni_char *name, const ES_Value &value) = 0;
		/**< Called when a property is 'encountered' on the ES_Object. Triggered
		     by a call to ExportProperties.

		     @param name The name of the property.
		     @param value The value of the property.
		     @return OpStatus::ERR* will cause the caller (ExportProperties) to return
		             with the same status message. */

	};

	static OP_STATUS ExportObject(ES_Runtime *rt, ES_Object *object, Handler *handler);
	/**< Extract additional information about an ES_Object.

         @param rt Runtime.
	     @param object The ES_Object we are interested in.
	     @param handler The object that will handle the response.
	     @return If OpStatus::ERR* is returned from one of the handler callbacks,
	             this function will return that status. Otherwise OpStatus::OK. */

	static OP_STATUS ExportProperties(ES_Runtime *rt, ES_Object *object, Handler *handler);
	/**< Extract additional information about an ES_Object.

	     @param rt Runtime.
	     @param object The ES_Object we are interested in.
	     @param handler The object that will handle the response.
	     @return If OpStatus::ERR* is returned from one of the handler callbacks,
	             this function will return that status. Otherwise OpStatus::OK. */

private:
	ESU_ObjectExporter(){};
	/**< Don't allow instances of this class. */
};

#endif /* ES_UTILS_OBJMAN_H */
