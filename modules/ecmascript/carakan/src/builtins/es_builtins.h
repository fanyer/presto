/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*- */

#ifndef ES_BUILTINS_H
#define ES_BUILTINS_H

#define APPEND_PROPERTY(class, property, value) prototype->PutCachedAtIndex(ES_PropertyIndex(class ## _ ## property), value)
#define DECLARE_PROPERTY(class, property, attributes, type) prototype_class->AddL(context, idents[ESID_ ## property], ES_Property_Info(attributes | CP), type, FALSE)
#define ASSERT_CLASS_SIZE(class) OP_ASSERT(prototype->Capacity() >= prototype->Class()->SizeOf(class ## _LAST_PROPERTY))
#define ASSERT_OBJECT_COUNT(class) OP_ASSERT(prototype->Count() == class ## Count)

#define ES_GET_GLOBAL_OBJECT() static_cast<ES_Function *>(argv[-1].GetObject())->GetGlobalObject()

#endif // ES_ARRAY_BUILTINS_H
