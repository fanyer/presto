import os
from cppgen.cpp import TemplatedGenerator, CppScopeBase, render, memoize, currentCppConfig, _member_name, _var_name, fileHeader
from hob import utils

# The main class is CppScopeManagerGenerators which should be instantiated by the caller
# It will return the correct

# The Meta* classes are meant to mimick the interface of similar classes in the proto package,
# but they only contain the information needed by the managers in this module

class MetaOptions(dict):
    pass

class MetaOptionValue(object):
    def __init__(self, value):
        self.value = value

class MetaService(object):
    # Note: filename is a deviation from proto.Service
    def __init__(self, name, options, filename=None):
        self.name = name
        self.options = options
        self.filename = filename

class ManagerOptions(object):
    def __init__(self, cpp_config):
        fields = ""
        if cpp_config.has_option("Manager", "shared_fields"):
            fields = cpp_config.get("Manager", "shared_fields", "")
        includes = ""
        if cpp_config.has_option("Manager", "includes"):
            includes = cpp_config.get("Manager", "includes", "")
        self.shared_fields = [field.strip() for field in fields.split(";") if field.strip()]
        self.includes = [include.strip() for include in includes.split(";") if include.strip()]

class CppScopeManagerGenerators(object):
    """
    Contains generators for the C++ files which make up the scope manager code.
    Call generators() to get an iterator over the generators.
    """
    def __init__(self, opera_root, build_module, module_path, services, dependencies=None):
        self.services = services
        self.operaRoot = opera_root
        self.buildModule = build_module
        self.modulePath = module_path
        self.basePath = os.path.join(module_path, "src", "generated")
        self.dependencies = dependencies

    def generators(self):
        """Iterator over generator objects for the the scope manager, may be empty."""
        yield CppScopeManagerImplementation(os.path.join(self.operaRoot, self.basePath, "g_scope_manager.cpp"), self.buildModule, self.services, dependencies=self.dependencies)
        yield CppScopeManagerDeclaration(os.path.join(self.operaRoot, self.basePath, "g_scope_manager.h"), self.buildModule, self.services, dependencies=self.dependencies)

class CppScopeManagerImplementation(TemplatedGenerator, CppScopeBase):
    """
    C++ code generator for the implementation of the scope manager.

    TODO: This code uses the old generate() API, it should be updated to use the new generateFiles() API, see scope_service.py:CppServiceInterfaceDeclaration for an example.
    """
    def __init__(self, file_path, build_module, services, dependencies=None):
        TemplatedGenerator.__init__(self)
        CppScopeBase.__init__(self, file_path=file_path, dependencies=dependencies)
        self.services = services
        self.buildModule = build_module

    def generate(self):
        text = ""

        text += fileHeader()
        text += "\n" + render("""
#include "core/pch.h"

#ifdef SCOPE_SUPPORT

#include "modules/scope/src/generated/g_scope_manager.h"

""") + "\n"

        gen = CppManagerGenerator(self.services)
        text += gen.getImplementation()

        text += render("""
#endif // SCOPE_SUPPORT
""") + "\n"

        return text

    def createFile(self):
        return CppScopeBase._createFile(self, build_module=self.buildModule)

class CppScopeManagerDeclaration(TemplatedGenerator, CppScopeBase):
    """
    C++ code generator for the declaration of the scope manager.

    TODO: This code uses the old generate() API, it should be updated to use the new generateFiles() API, see scope_service.py:CppServiceInterfaceDeclaration for an example.
    """
    def __init__(self, file_path, build_module, services, dependencies=None):
        TemplatedGenerator.__init__(self)
        CppScopeBase.__init__(self, file_path=file_path, dependencies=dependencies)
        self.services = services
        self.buildModule = build_module

    def generate(self):
        text = ""

        text += fileHeader()
        text += "\n" + render("""
#ifndef OP_SCOPE_MANAGER_G_H
#define OP_SCOPE_MANAGER_G_H

#ifdef SCOPE_SUPPORT

""") + "\n"

        gen = CppManagerGenerator(self.services)
        text += gen.getDeclaration()

        text += render("""

#endif // SCOPE_SUPPORT

#endif // OP_SCOPE_MANAGER_G_H
""") + "\n"

        return text

    def createFile(self):
        return CppScopeBase._createFile(self, build_module=self.buildModule)

class CppManagerGenerator(TemplatedGenerator):
    """
    C++ code generator for the implementation and the declaration of the scope manager.

    TODO: This is an old-style generator and should be merged with CppScopeManagerImplementation and CppScopeManagerDeclaration.
    """
    def __init__(self, services):
        self.manager_options = ManagerOptions(currentCppConfig())
        self.service_list = services
        TemplatedGenerator.__init__(self)

    def varName(self, name):
        return _var_name(name)

    def memberName(self, name):
        return _member_name(name)

    @memoize
    def getDeclaration(self):
        return self.renderDeclaration(self.service_list)

    def renderDeclaration(self, services):
        text = ""

        text += "\n" + render("""

            // Includes

            #include "modules/scope/src/scope_service.h"

            // Includes for shared variables

            """) + "\n\n"

        for include in self.manager_options.includes:
            text += """#include "%s"\n""" % include

        text += "\n" + render("""

            // Forward declarations

            """) + "\n\n"

        for service in services:
            if "cpp_instance" not in service.options or service.options["cpp_instance"].value == "true":
                if "cpp_feature" in service.options:
                    text += "#ifdef %(feature)s\n" % {"feature": service.options["cpp_feature"].value}

                text += "class %(class)s;\n" % {"class": service.options["cpp_class"].value}

                if "cpp_feature" in service.options:
                    text += "#endif // %(feature)s\n" % {"feature": service.options["cpp_feature"].value}

        text += "\n" + render("""

            // Interface definitions

            """) + "\n\n"

        for service in services:
            if "cpp_feature" in service.options:
                text += "#ifdef %(feature)s\n" % {"feature": service.options["cpp_feature"].value}

            prefix = "g_scope_"
            basename = utils.join_underscore(utils.split_camelcase(service.name))
            if "cpp_file" in service.options:
                basename = service.options["cpp_file"].value
                prefix = "g_"
            text += """# include "%(cpp_gen_base)s/%(file)s_interface.h"\n""" % {
                "file": prefix + basename,
                "cpp_gen_base": service.options["cpp_gen_base"].value,
                }

            if "cpp_feature" in service.options:
                text += "#endif // %(feature)s\n" % {"feature": service.options["cpp_feature"].value}

        text += "\n" + render("""

class OpScopeDescriptorSet;
class OpScopeManager;
class OpScopeServiceManager;

/**
 * This is a generated class which defines all services which are use in scope.
 * It provides methods for registering meta-services as well as creating all
 * service objects.
 * In addition it keeps an instance of the OpScopeDescriptorSet class, this is
 * used to provide descriptor objects for messages in each service.
 *
 * This class is meant to be sub-classed into a real scope manager. The sub-class
 * must call RegisterMetaService() to register meta-services, and then call
 * CreateServices() and CreateServiceDescriptors() at a convenient time to
 * initialize the class properly.
 *
 * @note This class is generated.
 */
class OpScopeServiceFactory
{
public:
	/**
	 * Initializes the generated manager with all service pointers set to NULL.
	 */
	OpScopeServiceFactory();
	/**
	 * Deletes all active services and the descriptor set.
	 */
	virtual ~OpScopeServiceFactory();

	/**
	 * Get a reference to the descriptor set. Can only be used
	 * after a successful call to CreateServiceDescriptors.
	 */
	OpScopeDescriptorSet &GetDescriptorSet();

	/**
	 * Add a new meta-service named @a name. Meta-services are services
	 * which have no functionality, no version and cannot be introspected.
	 *
	 * The actual service will be created when CreateServices() is called
	 * by a sub-class. Adding a meta-service after that point will have no
	 * effect.
	 *
	 * If the meta-service has already been registered it will return OpStatus::ERR.
	 *
	 * @param name The name of the meta-service.
	 * @return OpStatus::OK, OpStatus::ERR or OpStatus::ERR_NO_MEMORY
	 *
	 */
	OP_STATUS RegisterMetaService(const uni_char *name);
	/**
	 * @see RegisterMetaService(const uni_char *name);
	 */
	OP_STATUS RegisterMetaService(const OpString &name) { return RegisterMetaService(name.CStr()); }

protected:

	/**
	 * Creates all services.
	 *
	 * @param manager The manager that owns the services.
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY otherwise.
	 */
	OP_STATUS CreateServices(OpScopeServiceManager *manager);

	/**
	 * Deletes all services. No more events will be accepted from Core
	 * after this method is called, even events triggered by service
	 * destruction.
	 */
	void DeleteServices();

	/**
	 * Create service descriptions.
	 *
	 * @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY otherwise.
	 */
	 OP_STATUS CreateServiceDescriptors();

public:

	OpScopeDescriptorSet *descriptors;

	// Services:

""") + "\n\n"

        for service in services:
            if "cpp_instance" not in service.options or service.options["cpp_instance"].value == "true":
                if "cpp_feature" in service.options:
                    text += "#ifdef %(feature)s\n" % {"feature": service.options["cpp_feature"].value}

                text += "	%(class)s *%(name)s;\n" % {
                    "class": service.options["cpp_class"].value,
                    "name": self.memberName(service.name),
                    }

                if "cpp_feature" in service.options:
                    text += "#endif // %(feature)s\n" % {"feature": service.options["cpp_feature"].value}

        text += "\n" + render("""

            private:
            	/**
            	 * Defines the name of a meta-service and manages the service object.
            	 * The meta-service is first initailized with a name and later on in the
            	 * CreateServices() a service object is created and placed in service.
            	 * When the MetaService is destructed it will delete the service object.
            	 */
            	struct MetaService : public ListElement<MetaService>
            	{
            		MetaService() : service(NULL) {}
            		~MetaService() { OP_DELETE(service); }
            		OpString name;
            		OpScopeService *service;
            	};
            	/**
            	 * List of meta-services currently registered.
            	 */
            	List<MetaService> meta_services;

            	// NOTE: These two methods are meant to be manually created outside of these classes
            	//       See modules/scope/src/scope_manager.cpp for the implementation
            	/**
            	 * Initializes any shared member variables, called before the services are created.
            	 */
            	OP_STATUS InitializeShared();
            	/**
            	 * Cleans up the shared member variables, called after the services has been deleted.
            	 */
            	void CleanupShared();

            	// Shared member variables. See cpp.conf for more information.
            """) + "\n"

        for field in self.manager_options.shared_fields:
            text += "	%s;\n" % field

        text += render("""
            }; // OpScopeServiceFactory

            /**
             * Contains the descriptors of all services in use by scope.
             *
             * @note This class is generated.
             */
            class OpScopeDescriptorSet
            {
            public:
            	OpScopeDescriptorSet();
            	~OpScopeDescriptorSet();

            	OP_STATUS Construct();

            private:
            	// Not really used for anything, except to prevent errors on certain
            	// compile configurations.
            	int dummy;

            public:
            """) + "\n"

        for service in services:
            if "cpp_feature" in service.options:
                text += "#ifdef %(feature)s\n" % {"feature": service.options["cpp_feature"].value}

            text += "	%(class)s_SI::Descriptors *%(name)s;\n" % {
                "class": service.options["cpp_class"].value,
                "name": self.varName(service.name),
                }

            if "cpp_feature" in service.options:
                text += "#endif // %(feature)s\n" % {"feature": service.options["cpp_feature"].value}

        text += "\n" + render("""
            }; // OpScopeDescriptorSet
            """) + "\n"

        return text

    @memoize
    def getImplementation(self):
        return self.renderImplementation(self.service_list)

    def renderImplementation(self, services):
        text = ""

        fields = {}
        for service in services:
            fields[service] = {
                "name": service.name,
                "memberName": self.memberName(service.name),
                "varName": self.varName(service.name),
                "class": service.options["cpp_class"].value,
            }
            if "cpp_feature" in service.options:
                fields[service]["feature"] = service.options["cpp_feature"].value
            if "cpp_construct" in service.options:
                fields[service]["construct"] = service.options["cpp_construct"].value

        text += "\n\n" + render("""

            #include "modules/scope/src/generated/g_scope_manager.h"

            // Services we need to instantiate

            """) + "\n\n"

        for service in services:
            if "cpp_instance" not in service.options or service.options["cpp_instance"].value == "true":
                if "cpp_feature" in service.options:
                    text += "#ifdef %(feature)s\n" % {"feature": service.options["cpp_feature"].value}

                text += """# include "%(file)s"\n""" % {"file": service.cpp.files()["serviceDeclaration"]}

                if "cpp_feature" in service.options:
                    text += "#endif // %(feature)s\n" % {"feature": service.options["cpp_feature"].value}

        text += "\n" + render("""

            /* OpScopeServiceFactory */

            OpScopeServiceFactory::OpScopeServiceFactory()
            	: descriptors(NULL)
            """) + "\n"

        for service in services:
            if "cpp_instance" not in service.options or service.options["cpp_instance"].value == "true":
                if "cpp_feature" in service.options:
                    text += "#ifdef %(feature)s\n" % fields[service]

                text += "	, %(memberName)s(NULL)\n" % fields[service]
                if "cpp_feature" in service.options:
                    text += "#endif // %(feature)s\n" % fields[service]

        text += render("""
            {
            }

            OP_STATUS
            OpScopeServiceFactory::CreateServices(OpScopeServiceManager *manager)
            {
            	RETURN_IF_ERROR(InitializeShared());

            """) + "\n\n"

        for service in services:
            if "cpp_instance" not in service.options or service.options["cpp_instance"].value == "true":
                if "cpp_feature" in service.options:
                    text += "#ifdef %(feature)s\n" % fields[service]

                text += self.indent(render("""
                    {
                    	%(memberName)s = OP_NEW(%(class)s, ());
                    	RETURN_OOM_IF_NULL(%(memberName)s);
                    	%(memberName)s->SetManager(manager);
                    """, fields[service]) + "\n")
                if "cpp_construct" in service.options:
                    text += "		OP_STATUS status = %(memberName)s->Construct(%(construct)s);\n" % fields[service]
                else:
                    text += "		OP_STATUS status = %(memberName)s->Construct();\n" % fields[service]

                if "cpp_is_essential" in service.options and service.options["cpp_is_essential"].value == "true":
                    text += self.indent(render("""
                        // Ensure that service '%(name)s' is initialized, cannot work without it
                        if (OpStatus::IsError(status))
                        	return status;
                        manager->RegisterService(%(memberName)s);
                        """, fields[service]), 2)
                else:
                    text += self.indent(render("""
                        if (OpStatus::IsMemoryError(status))
                        	return status;
                        // Only register the service if there are no errors while constructing it
                        if (OpStatus::IsSuccess(status))
                        	manager->RegisterService(%(memberName)s);
                        """, fields[service]), 2)

                text += "\n	}\n"
                if "cpp_feature" in service.options:
                    text += "#endif // %(feature)s\n" % fields[service]


        text += "\n" + self.indent(render("""
            	// Add meta-services

            	for (MetaService *meta_service = meta_services.First(); meta_service; meta_service = meta_service->Suc())
            	{
            		meta_service->service = OP_NEW_L(OpScopeMetaService, (meta_service->name.CStr()));
            		RETURN_OOM_IF_NULL(meta_service->service);
            		meta_service->service->SetManager(manager);
            		OP_STATUS status = meta_service->service->Construct();
            		if (OpStatus::IsMemoryError(status))
            			return status;
            		// Only register the service if there are no errors while constructing it
            		if (OpStatus::IsSuccess(status))
            			manager->RegisterService(meta_service->service);
            	}

            	// All services has been initialized, now continue with the intra-service setup
            	OpScopeServiceManager::ServiceRange range = manager->GetServices();
            	for (; !range.IsEmpty(); range.PopFront())
            	{
            		OP_STATUS status = range.Front()->OnPostInitialization();
            		if (OpStatus::IsMemoryError(status))
            			return status;

            		// Any errors in post initialization makes the service unavailable
            		if (OpStatus::IsError(status))
            		{
            			manager->UnregisterService(range.Front());
            """) + "\n")

        for service in services:
            if "cpp_instance" not in service.options or service.options["cpp_instance"].value == "true":
                if "cpp_is_essential" in service.options and service.options["cpp_is_essential"].value == "true":

                    if "cpp_feature" in service.options:
                        text += "#ifdef %(feature)s\n" % fields[service]

                    text += self.indent(render("""
                        			// Ensure that post initialization of service '%(name)s' succeeds, cannot work without it
                        			if (range.Front() == %(memberName)s)
                        				return status;
                        """, fields[service]) + "\n", 3)

                    if "cpp_feature" in service.options:
                        text += "#endif // %(feature)s\n" % fields[service]

        text += "		" + render("""
            		}
            	}

            	return OpStatus::OK;
            }

            void
            OpScopeServiceFactory::DeleteServices()
            {
            	// Once we have starting shutting down services, we don't want any new
            	// events from Core. This protects against shutdown crashes.

            """) + "\n\n"

        for service in services:
            if "cpp_instance" not in service.options or service.options["cpp_instance"].value == "true":
                if "cpp_feature" in service.options:
                    text += "#ifdef %(feature)s\n" % fields[service]

                text += self.indent(render("""
                        %(class)s *tmp_%(memberName)s = %(memberName)s;
                        %(memberName)s = NULL;
                    """, fields[service]) + "\n")
                if "cpp_feature" in service.options:
                    text += "#endif // %(feature)s\n" % fields[service]

        text += "\n	// Now destroy all services.\n\n"

        for service in services:
            if "cpp_instance" not in service.options or service.options["cpp_instance"].value == "true":
                if "cpp_feature" in service.options:
                    text += "#ifdef %(feature)s\n" % fields[service]

                text += "	OP_DELETE(tmp_%(memberName)s);\n" % fields[service]
                if "cpp_feature" in service.options:
                    text += "#endif // %(feature)s\n" % fields[service]

        text += "\n	" + render("""

            	// Delete meta-services

            	meta_services.Clear();

            	// Cleanup any shared member variables

            	CleanupShared();
            }

            /* OpScopeDescriptorSet */

            OpScopeDescriptorSet::OpScopeDescriptorSet()
            	: dummy(0)
            """) + "\n"

        for service in services:
            if "cpp_feature" in service.options:
                text += "#ifdef %(feature)s\n" % fields[service]

            text += "	, %(varName)s(NULL)\n" % fields[service]
            if "cpp_feature" in service.options:
                text += "#endif // %(feature)s\n" % fields[service]

        text += render("""
            {
            	(void)dummy;
            }

            OpScopeDescriptorSet::~OpScopeDescriptorSet()
            {
            """) + "\n"

        for service in services:
            if "cpp_feature" in service.options:
                text += "#ifdef %(feature)s\n" % fields[service]

            text += "	OP_DELETE(%(varName)s);\n" % fields[service]
            if "cpp_feature" in service.options:
                text += "#endif // %(feature)s\n" % fields[service]

        text += render("""
            }

            OP_STATUS
            OpScopeDescriptorSet::Construct()
            {
            """) + "\n"

        for service in services:
            if "cpp_feature" in service.options:
                text += "#ifdef %(feature)s\n" % fields[service]

            text += self.indent(render("""
                %(varName)s = OP_NEW(%(class)s_SI::Descriptors, ());
                RETURN_OOM_IF_NULL(%(varName)s);
                RETURN_IF_ERROR(%(varName)s->Construct());
            """, fields[service]) + "\n")
            if "cpp_feature" in service.options:
                text += "#endif // %(feature)s\n" % fields[service]

        text += "	return OpStatus::OK;\n}\n"

        return text
