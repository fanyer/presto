#ifndef OPNAMEDWIDGETS_CONTAINER_H
#define OPNAMEDWIDGETS_CONTAINER_H

#ifdef NAMED_WIDGET

#include "modules/widgets/OpWidget.h"

// ---------------------------------------
// Extended APIS exported by module.export
//
// Products that want to use these will have
// to explicitly import them.
// ---------------------------------------
class OpNamedWidgetsCollection
{
public:
	OpNamedWidgetsCollection();
	virtual ~OpNamedWidgetsCollection();

	/**
	 * Change the name of widget to name. This will update indexing
	 * appropriately.
	 *
	 * @param widget that will have its name changed
	 * @param name that it should have
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS ChangeName(OpWidget * widget, const OpStringC8 & name);

	/**
	 * Should be called when a widget has been added to the collection.
	 *
	 * @param child that has been added
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS WidgetAdded(OpWidget * child);

	/**
	 * Should be called when a widget has been removed from the collection.
	 *
	 * @param child that has been removed
	 * @return OpStatus::OK if successful
	 */
	OP_STATUS WidgetRemoved(OpWidget * child);

	/**
	 * Search the collection for the widget with this name
	 *
	 * @param name to search for
	 * @return pointer to the widget with that name or NULL if no such widget exists
	 */
	OpWidget* GetWidgetByName(const OpStringC8 & name);

private:

	/**
	 * Class containing the hash functions used by the m_named_widgets table.
	 * Note that this is case insensitive because that is how the interface
	 * has always been.
	 */
	class StringHash : public OpHashFunctions
	    {
	    public:
		~StringHash() {}

		/**
		 * Calculates the hash value for a key.
		 *
		 * @param key the hash key
		 * @return hash value for the key
		 */
		UINT32 Hash(const void* key);

		/**
		 * Compares if to keys are equal.
		 *
		 * @param key1
		 * @param key2
		 * @return true if keys are considered equal
		 */
		BOOL KeysAreEqual(const void* key1, const void* key2);
	    };

	StringHash  m_hash_function;
	OpHashTable m_named_widgets;
};
#endif // NAMED_WIDGET

#endif // OPNAMEDWIDGETS_CONTAINER_H
