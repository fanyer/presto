/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef WINDOWSCOMMON_HOOKAPI_H
#define WINDOWSCOMMON_HOOKAPI_H

namespace WindowsCommonUtils
{
	/**
	 * Class for hooking imported functions of specified module.
	 */
	class HookAPI
	{
	public:
		/**
		 * Constructor of the class.
		 *
		 * @param module handle to the module whose import table should be changed.
		 */
		HookAPI(HMODULE module);

		/**
		 * Iterates through import table of module (passed to the constructor
		 * of this class) and replaces pointer to specified function with our
		 * custom one.
		 *
		 * Replaced function pointer will NOT be inherited by modules loaded
		 * from module whose function was replaced.
		 *
		 * @param dll_name name of dll that exports function that we want to replace.
		 * @param proc_name name of the imported function to be replaced.
		 * @param new_proc pointer to our custom function that will replace original.
		 *
		 * @return pointer to original function.
		 */
		FARPROC						SetImportAddress(const char* dll_name, const char* proc_name, FARPROC new_proc);

	private:
		/**
		 * @return absolute pointer converted from address relative to module.
		 */
		const void*					RVA2Ptr(SIZE_T rva);
		/**
		 * @return address of a PE directory.
		 */
		const void*					GetDirectory(int id);
		/**
		 * @return Import description of an imported module.
		 */
		IMAGE_IMPORT_DESCRIPTOR*	GetImportDescriptor(const char* dll_name);
		/**
		 * @return address of imported function with specified name.
		 */
		const void*					GetFunctionPtr(IMAGE_IMPORT_DESCRIPTOR* import_desc, const char* proc_name);

		const char*					m_module_ptr;
		IMAGE_NT_HEADERS*			m_ntheader;
	};
};

#endif // WINDOWSCOMMON_HOOKAPI_H
