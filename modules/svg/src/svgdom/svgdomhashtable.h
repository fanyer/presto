#ifndef SVG_DOM_HASH_TABLE_H
#define SVG_DOM_HASH_TABLE_H

#ifdef SVG_SUPPORT
#ifdef SVG_DOM

#include "modules/util/OpHashTable.h"

/*
 * This class exists becuase OpPointerHashTable defines a Delete()
 * function that requires the full definition of the data that is
 * stored in the OpPointerHashTable. We don't want that because we
 * store DOM_Object* pointers, and we only have forward-declarations
 * for these.
 */
class SVGDOMHashTable : private OpHashTable
{
public:
	SVGDOMHashTable() : OpHashTable() {}

	OP_STATUS Add(SVGObject* key, DOM_Object* data) { return OpHashTable::Add((void *)(key), (void *)(data)); }
	OP_STATUS Remove(SVGObject* key, DOM_Object** data) { return OpHashTable::Remove((void *)(key), (void **)(data)); }

	OP_STATUS GetData(SVGObject* key, DOM_Object** data) { return OpHashTable::GetData((void *)(key), (void **)(data)); }
	BOOL Contains(SVGObject* key) { return OpHashTable::Contains((void *)(key)); }

	OpHashIterator*	GetIterator() { return OpHashTable::GetIterator(); }

	void ForEach(void (*function)(const void *, void *)) { OpHashTable::ForEach(function); }
	void ForEach(void (*function)(const void *, void *, OpHashTable *)) { OpHashTable::ForEach(function); }
	void RemoveAll() { OpHashTable::RemoveAll(); }
	void DeleteAll() { OpHashTable::DeleteAll(); }

	INT32 GetCount() { return OpHashTable::GetCount(); }
};

#endif // SVG_SUPPORT
#endif // SVG_DOM
#endif // SVG_DOM_HASH_TABLE_H
