/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#ifndef REMOTE_FRAMEWORK_MANAGER_H
#define REMOTE_FRAMEWORK_MANAGER_H

#if defined SELFTEST && defined _NATIVE_SSL_SUPPORT_
#include "modules/url/url2.h"
#include "modules/updaters/xml_update.h"
#include "modules/network_selftest/urldoctestman.h"

/** The central manager class of the Remote Selftest Framework 
 *	This class manages the loading of the master testsuite list(s),
 *	keep the managers of the testconfiguration loaders, and the list
 *	of testbatches.
 *
 *	The manager loads one or more XML files to be parsed by RemoteFrameworkMaster, 
 *	which then configures the loading of any number of XML files that will be
 *	parsed by RemoteFrameworkTestSuites, which then configures the actual testsuites
 *	to be loaded.
 *
 *	Not configuring a testcase is considered a failure.
 *
 *	Used in a Opera selftest file this object must be created in global segment
 *	and the test must be asynchronous. See this module's selftests for examples.
 *
 *	Note that AddTestSuite MUST be called successfully at least once for the tests to start. 
 *	If AddTestSuite is not called, then the test will hang indefinitely.
 *
 *	Implementations may expand the syntax of the XML parsers by creating 
 *	a subclass of RemoteFrameworkManager than overrides the function
 *	ProduceTestSuiteMaster() so that it returns a subclass of RemoteFrameworkMaster
 *	which can then construct subclasses of RemoteFrameworkTestSuites that
 *	can optionally create new kinds of testcases.
 */
class RemoteFrameworkManager : public AutoFetch_Manager
{
public:
	class RemoteFrameworkTestSuites;

	class RemoteTextMaster : public URL_Updater
	{
	private:
		/** The URL being loaded. Used as a base URL */
		URL framework_url;

		/** The manager of this object, used to pass on testsuite loaders*/
		RemoteFrameworkManager *manager;

	protected:
		/** The file is ready for processing */
		virtual OP_STATUS ResourceLoaded(URL &url);

	public:

		/** Constructor. mgr must be the containing manager 
		 *
		 *	@param	mgr	Parent framework manager
		 */
		RemoteTextMaster(RemoteFrameworkManager *mgr): manager(mgr){}

		/** Constructs handling of, and starts loading the URL testsuite_list 
		 *	
		 *	@parama	testsuite_list	URL containing the the textfile of the testsuite specification
		 */
		OP_STATUS Construct(URL &testsuite_list);

		/** The processing of the text file
		 *	This function may be overridden, but should then either be using a completely different 
		 *	syntax (never calling this function) or encapsulate a testsuite
		 */
		virtual OP_STATUS ProcessFile();

		/** Handle individual textlines */
		virtual OP_STATUS HandleTextLine(const OpStringC8 &line);

		/** return the URL being processed */
		URL GetURL() {return framework_url;}

		/** Get the Manager of this object */
		RemoteFrameworkManager *GetManager(){return manager;}
	};


	/**
	 *	Loads an XML file specifying a list of URLs with the actual testcases to be run.
	 *
	 *	Implementations may expand the syntax of the file
	 *
	 *	Such expansion is done by declaring implementing subclasses of  
	 *  RemoteFrameworkManager, RemoteFrameworkManager::RemoteFrameworkMaster, 
	 *	and optionally RemoteFrameworkManager::RemoteFrameworkTestSuites, 
	 *	where the new class's implementation of 
	 *	RemoteFrameworkManager::RemoteFrameworkMaster::ProduceTestSuiteMaster
	 *	return an object of the RemoteFrameworkMaster-based class.
	 *
	 *	In RemoteFrameworkMaster these functions may be updated
	 *
	 *		HandleUnknownKeyword which handle unknown element names direct from 
	 *			the member "parser", inside the <testsuites> segment.
	 *
	 *		ProduceTestSuite which gets some parameters, and the implementation may 
	 *			use "parser.RestartCurrentElement()" to get access to more parameters, 
	 *			if needed. This can be used to produce extensions to the
	 *			RemoteFrameworkTestSuites class.
	 *		
	 *	File syntax (and example) (pseudo code):
	 *
	 *	<?xml version="1.0" encoding="utf-8" ?> 
	 *	<testsuites> 
	 *		 <condition from="Mx" until="2.4.y" /> 
	 *		<testsuite>file1.xml</testsuite> 
	 *		<testsuite>file2.xml</testsuite> 
	 *	<testsuites>
	 */
	class RemoteFrameworkMaster : public XML_Updater
	{
	private:
		/** The URL being loaded. Used as a base URL */
		URL framework_url;

		/** The manager of this object, used to pass on testsuite loaders*/
		RemoteFrameworkManager *manager;

	public:

		/** Constructor. mgr must be the containing manager 
		 *
		 *	@param	mgr	Parent framework manager
		 */
		RemoteFrameworkMaster(RemoteFrameworkManager *mgr): manager(mgr){SetVerifyFile(FALSE);}

		/** Constructs handling of, and starts loading the URL testsuite_list 
		 *	
		 *	@parama	testsuite_list	URL containing the the XML of the testsuite specification
		 */
		OP_STATUS Construct(URL &testsuite_list);

		/** The processing of the XML file
		 *	This function may be overridden, but should then either be using a completely different 
		 *	syntax (never calling this function), encapsulate a testsuite, or restart the 
		 *	parser before calling this function
		 */
		virtual OP_STATUS ProcessFile();

		/** return the URL being processed */
		URL GetURL() {return framework_url;}

		/** Get the Manager of this object */
		RemoteFrameworkManager *GetManager(){return manager;}

		/** Allows unknown elements inside <testsuites> to be handled.
		 *	Implementations may override this function to handle such 
		 *	unknown elements. Such code can access the parser object 
		 *	and do all parsing operations allowed on such objects, but MUST
		 *	MUST return with the parser in the same state as when it entered.
		 */
		virtual OP_STATUS HandleUnknownKeyword();

		/** Allocates and initializes a testsuite loading based on the suite_type and the
		 *	the url (which must be resolved relative to this->GetURL()), and the 
		 *	passdata strings. 
		 *	
		 *	Implementations may override this function to handle 
		 *	unknown suite_types. Such code can access the parser object 
		 *	and, after a restart of the element,do all parsing operations 
		 *	allowed on such objects, but MUST return with the parser in 
		 *	the same state as when it entered.
		 *
		 *	@param	suite_type		Identifies the type of testsuite to be loaded 
		 *							(from the "type" parameter in the <testsuite> tag)
		 *	@param	url				The relative or absolute URL of the testsuite
		 *	@return					a URL_DocSelfTest_Item based object
		 */
		virtual RemoteFrameworkTestSuites *ProduceTestSuite(const OpStringC &suite_type, const OpStringC &url, OP_STATUS &op_err);
	};

	/**
	 *	Loads an XML file specifying the actual list of URLs to load and 
	 *	the passconditions that apply for the test.
	 *
	 *	Implementations may expand the type of testbatches and actual 
	 *	tests (based on passconditions), and actual test_items.
	 *
	 *	Such expansion is done by declaring implementing subclasses of  
	 *  RemoteFrameworkManager, RemoteFrameworkManager::RemoteFrameworkMaster, 
	 *	and RemoteFrameworkTestSuites, where the new class's implementation of 
	 *	RemoteFrameworkManager::RemoteFrameworkMaster::ProduceTestSuite
	 *	return an object of the RemoteFrameworkTestSuites-based class.
	 *
	 *	In RemoteFrameworkTestSuites the following functions may be implemented:
	 *
	 *		HandleUnknownTestBatch which handle unknown element names direct from 
	 *			the member "parser", inside the <testsuite> segment.
	 *
	 *		HandleUnknownTestLevel which handle unknown element names direct from 
	 *			the member "parser", inside the <testbatch> segment.
	 *
	 *		ProduceTestcase which gets some parameters, and the implementation may 
	 *			use "parser.RestartCurrentElement()" to get access to more parameters, 
	 *			if needed.
	 *
	 *	File syntax (and example) (pseudo code):
	 *
	 *		<?xml version="1.0" encoding="utf-8" ?> 
	 *		<testsuite> 
	 *		  <condition from="<Milstone>" until="<major>.<minor>.<milestone>" /> 
	 *		  <testbatch> 
	 *			<condition from="<Milstone>" until="<major>.<minor>.<milestone>" /> 
	 *			<testconfig>updated preferences, such as proxies</testconfig> <!-- not implemented -->
	 *			<test> 
	 *			  <condition from="<Milstone>" until="<major>.<minor>.<milestone>" /> 
	 *			  <url>http://www.opera.com/</url> 
	 *			  <result type=security-level | passcontent | matchfile> 
	 *				 <--security-level-case:--> 0 
	 *				 <--passcontent :-->text 
	 *				 <---matchcontent-->relative URL testfiles/dynamic/file1.html 
	 *			  </result> 
	 *			</test> 
	 *			<test> 
	 *			  <url>http://www.example.com/</url> 
	 *			  <result type=security-level | passcontent | matchfile> 
	 *				 <--security-level-case:--> 0 
	 *				 <--passcontent :-->text 
	 *				 <---matchcontent-->relative URL testfiles/dynamic/file1.html 
	 *			  </result> 
	 *			</test> 
	 *		  </testbatch> 
	 *		</testsuite> 
	 */
	class RemoteFrameworkTestSuites  : public XML_Updater
	{
	private:
		/** The URL being loaded. Used as a base URL */
		URL testsuite_url;

		/** The manager of this object, used to pass on testcases */
		RemoteFrameworkManager *manager;

	public:

		/** Constructor. mgr must be the containing manager 
		 *
		 *	@param	mgr	Parent framework manager
		 */
		RemoteFrameworkTestSuites(RemoteFrameworkManager *mgr): manager(mgr){SetVerifyFile(FALSE);}

		/** Constructs handling of, and starts loading the URL testsuite_list 
		 *	
		 *	@parama	testsuite_list	URL containing the the XML of the testsuite specification
		 */
		OP_STATUS Construct(URL &testsuite_list);

		/** The processing of the XML file
		 *	This function may be overridden, but should then either be using a completely different 
		 *	syntax (never calling this function), encapsulate a testsuite, or restart the 
		 *	parser before calling this function
		 */
		virtual OP_STATUS ProcessFile();

		/** return the URL being processed */
		URL GetURL() {return testsuite_url;}

		/** Get the Manager of this object */
		RemoteFrameworkManager *GetManager(){return manager;}

		/** Allows unknown elements inside <testsuite> to be handled.
		 *	Implementations may override this function to handle such 
		 *	unknown elements. Such code can access the parser object 
		 *	and do all parsing operations allowed on such objects, but MUST
		 *	MUST return with the parser in the same state as when it entered.
		 */
		virtual OP_STATUS HandleUnknownTestBatch();

		/** Allows unknown elements inside <testbatch> to be handled.
		 *	Implementations may override this function to handle such 
		 *	unknown elements. Such code can access the parser object 
		 *	and do all parsing operations allowed on such objects, but MUST
		 *	MUST retrun with the parser in the same state as when it entered.
		 */
		virtual OP_STATUS HandleUnknownTestLevel();

		/** Allocates and initializes a testcase based on the testcasettype,
		 *	the url (which must be resolved relative to this->GetURL()), and the 
		 *	passdata strings. 
		 *	
		 *	Implementations may override this function to handle 
		 *	unknown testcasetypes. Such code can access the parser object 
		 *	and, after a restart of the element,do all parsing operations 
		 *	allowed on such objects, but MUST return with the parser in 
		 *	the same state as when it entered.
		 *
		 *	@param	testcasetype	Identifies the testcase type to be constructed
		 *	@param	url				The relative or absolute URL of the testcase
		 *	@param	pass_data		The pass_data to be used for the testcase
		 *	@param	op_err			error return code
		 *	@return					a URL_DocSelfTest_Item based object
		 */
		virtual URL_DocSelfTest_Item *ProduceTestcase(const OpStringC &testcasetype, const OpStringC &url, const OpStringC &pass_data, OP_STATUS &op_err);
	};


private:
	/** Manages the loading of the testsuites, and signals the manager when 
	 *	loading has completed
	 */
	class TestSuiteLoader : public AutoFetch_Manager
	{
	private:
		/** The manager of this object*/
		RemoteFrameworkManager *manager;

	public:
		/** Constructor. mgr must be the containing manager 
		 *
		 *	@param	mgr	Parent framework manager
		 */
		TestSuiteLoader(RemoteFrameworkManager *mgr):manager(mgr){}

		/** Called by AutoFetch_Manager base class when loading is completed */
		virtual void OnAllUpdatersFinished();
	};

	URL_CONTEXT_ID	context_id;

	OP_STATUS CheckContext();

	/** Testsuite Load handler */
	TestSuiteLoader suite_loader;

	/** Manager of the testbatches created by the testsuite loaders */
	URL_DocSelfTest_Manager *test_manager;

	/** Have any testcases been added */
	BOOL added_testbatches;

	/** The message handler to use when loading the specifications and the testcases; if the message handler is deleted the owner will cancel all loading */
	TwoWayPointer<MessageHandler> document_mh;

public:

	/** Constructor */
	RemoteFrameworkManager(MessageHandler *mh=NULL):
		context_id(0),
		suite_loader(this), 
		test_manager(NULL), 
		added_testbatches(FALSE), 
		document_mh(mh ? mh : g_main_message_handler) 
		{
		}

	/** Destructor */
	~RemoteFrameworkManager();

	/** Completes initialization */
	OP_STATUS Construct(MessageHandler *mh=NULL);

	/** Add a testsuite list specified by the URL. Loading starts immediately.
	 *	The function can be called repeatedly.
	 *
	 *	@param	testsuite_list	URL to be loaded when fetching the list of 
	 */
	OP_STATUS AddTestSuite(URL &testsuite_list);

	/** Add a testsuite list specified by the URL. Loading starts immediately.
	 *	The function can be called repeatedly.
	 *
	 *	@param	testsuite_list	String of URL to be loaded when fetching the list of 
	 *	@param	ctx				URL Context of URL to be loaded. If this is 0 (zero) a the manager will create one for this manager
	 */
	OP_STATUS AddTestSuite(const OpStringC8 &testsuite_list, URL_CONTEXT_ID ctx = 0);

	/** Add a testsuite list specified by the URL. Loading starts immediately.
	 *	The function can be called repeatedly.
	 *
	 *	@param	testsuite_list	String of URL to be loaded when fetching the list of 
	 *	@param	ctx				URL Context of URL to be loaded. If this is 0 (zero) a the manager will create one for this manager
	 */
	OP_STATUS AddTestSuite(const OpStringC &testsuite_list, URL_CONTEXT_ID ctx = 0);

	/** Adds a testsuite loader. Takes ownership of the suite and starts loading
	 *
	 *	@param	suite	Testsuite loader to be started
	 */
	OP_STATUS AddTestSuiteLoader(RemoteFrameworkTestSuites *suite);

	/** Adds a batch of tests. Takes ownership of the test. 
	 *	The batches will be started in sequence once all tests are configured
	 *	
	 *	@param	batch	pointer to test batch manager object.
	 */
	void AddTestBatch(URL_DocSelfTest_Batch *batch){test_manager->AddBatch(batch);added_testbatches= TRUE;}

	MessageHandler *GetMessageHandler(){return document_mh;}

	URL_DocSelfTest_Manager *GetTestManager(){return test_manager;}

	/** Called by suite_loader when all testsuites have completed loading. 
	 *	This will start the loading of the configured testcases 
	 */
	void TestSuitesReady();

	/** Called by AutoFetch_Manager base class when loading is completed */
	virtual void OnAllUpdatersFinished();

	/** Allocates and initializes a RemoteFrameworkMaster that will load the specified URL
	 *	Implementations may override this function to create customized loaders and parsers
	 *	by creating expanded versions of RemoteFrameworkMaster and RemoteFrameworkTestSuites
	 */
	virtual AutoFetch_Element *ProduceTestSuiteMaster(URL &url, OP_STATUS &op_err);


	/** Produce a new testmanager */
	virtual URL_DocSelfTest_Manager *ProduceTestManager(){return OP_NEW(URL_DocSelfTest_Manager,());}
};

#endif  // defined SELFTEST && defined _NATIVE_SSL_SUPPORT_
#endif  // REMOTE_FRAMEWORK_MANAGER_H

