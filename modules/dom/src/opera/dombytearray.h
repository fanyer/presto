#ifndef DOMBYTEARRAY_H_
#define DOMBYTEARRAY_H_

#if defined WEBSERVER_SUPPORT || defined DOM_GADGET_FILE_API_SUPPORT
#include "modules/dom/domenvironment.h"
#include "modules/dom/src/domobj.h"
#include "modules/util/bytearray.h"


class DOM_ByteArray
	: public DOM_Object
{
public:

	static OP_STATUS Make(DOM_ByteArray *&byte_array, DOM_Runtime *runtime, unsigned int data_size);
	static OP_STATUS MakeFromFile(DOM_ByteArray *&byte_array, DOM_Runtime *runtime, OpFile *file, unsigned int data_size);

	unsigned char *GetDataPointer();

	int GetDataSize();

	/* From DOM_Object: */
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WEBSERVER_BINARY_OBJECT || DOM_Object::IsA(type); }
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndex(int property_index, ES_Value* return_value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutIndex(int property_index, ES_Value *value, ES_Runtime *origining_runtime);

private:
	ByteArray m_binary_data;
};

class DOM_ByteArray_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_ByteArray_Constructor() : DOM_BuiltInConstructor(DOM_Runtime::BYTEARRAY_PROTOTYPE) {}

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // WEBSERVER_SUPPORT || DOM_GADGET_FILE_API_SUPPORT
#endif /*DOMBYTEARRAY_H_*/
