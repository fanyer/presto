/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve N. Pettersen
*/

#include "core/pch.h"

#if defined SELFTEST && defined _NATIVE_SSL_SUPPORT_
#include "modules/url/url_man.h"
#include "modules/selftest/src/testutils.h"
#include "modules/network_selftest/remote_framework.h"
#include "modules/network_selftest/seclevel_test.h"
#include "modules/network_selftest/scanpass.h"
#include "modules/network_selftest/justload.h"
#include "modules/network_selftest/matchurl.h"

static BOOL GetVersion(const OpStringC &str, uint32 &major, uint32 &minor, uint32 &milestone)
{
	major = minor = milestone = 0;

	if(str.IsEmpty())
		return FALSE;

	if(str.FindFirstOf('.') == KNotFound)
	{
		milestone = (uint32) uni_atoi(str.CStr());
		return (milestone != 0);
	}

	uni_sscanf(str.CStr(),UNI_L("%u.%u.%u"), &major, &minor, &milestone);

	return (major != 0 &&  milestone != 0); // Minor may be 0, not major or milestone
}

static BOOL CheckConditionals(XMLFragment &parser, const OpStringC &conditional_name)
{
	BOOL seen_condition = FALSE;
	while(parser.EnterAnyElement())
	{
		if(parser.GetElementName() == conditional_name.CStr())
		{
			seen_condition  = TRUE;
			BOOL match_from = FALSE;
			BOOL match_until = FALSE;
			uint32 major, minor, milestone;

			if(GetVersion(parser.GetAttribute(UNI_L("from")), major, minor, milestone))
			{
				if(major == CORE_VERSION_MAJOR &&
					minor == CORE_VERSION_MINOR &&
					milestone <= CORE_VERSION_MILESTONE)
					match_from = TRUE;
				else if(milestone <= CORE_VERSION_MILESTONE)
					match_from = TRUE;
			}
			else
				match_from = TRUE;

			if(GetVersion(parser.GetAttribute(UNI_L("until")), major, minor, milestone))
			{
				if(major){
					if(major < CORE_VERSION_MAJOR)
						match_until = TRUE;
					else if	(major == CORE_VERSION_MAJOR)
					{
						if(minor < CORE_VERSION_MINOR)
							match_until = TRUE;
						else if(minor == CORE_VERSION_MINOR &&
							milestone >= CORE_VERSION_MILESTONE)
							match_until = TRUE;
					}
				}
				else if(milestone >= CORE_VERSION_MILESTONE)
					match_until = TRUE;
			}
			else
				match_until = TRUE;

			if(match_from && match_until)
			{
				parser.LeaveElement();
				parser.RestartCurrentElement();
				return TRUE;
			}
		}
		parser.LeaveElement();
	}

	parser.RestartCurrentElement();

	return seen_condition ? FALSE : TRUE;
}

static BOOL XMLHasTag(XMLFragment &parser, const OpStringC &tag_name)
{
	BOOL seen_tag= FALSE;
	while(parser.EnterAnyElement())
	{
		if(parser.GetElementName() == tag_name.CStr())
		{
			seen_tag=TRUE;
			parser.LeaveElement();
			break;
		}
		parser.LeaveElement();
	}

	parser.RestartCurrentElement();

	return seen_tag;
}


OP_STATUS RemoteFrameworkManager::Construct(MessageHandler *mh)
{
	if(mh)
		document_mh = mh;
	test_manager = ProduceTestManager();
	RETURN_OOM_IF_NULL(test_manager);
	TRAP_AND_RETURN(op_err, suite_loader.InitL());
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_NETSELFTEST_REMFRAME_LOADER, 0));
	output("\n"); // get the first line of information at the beginning of a line, not at the end of the first
	return g_main_message_handler->SetCallBack(&suite_loader, MSG_NETSELFTEST_REMFRAME_LOADER, 0);
}

RemoteFrameworkManager::~RemoteFrameworkManager()
{
	if(context_id != 0)
		urlManager->RemoveContext(context_id, TRUE);

	OP_DELETE(test_manager);
	test_manager = NULL;
}

OP_STATUS RemoteFrameworkManager::CheckContext()
{
	if(context_id != 0)
		return OpStatus::OK;

	context_id = urlManager->GetNewContextID();
	if(context_id == 0)
		return OpStatus::ERR;

	TRAP_AND_RETURN(op_err, urlManager->AddContextL(context_id,
		OPFILE_ABSOLUTE_FOLDER, OPFILE_ABSOLUTE_FOLDER, OPFILE_ABSOLUTE_FOLDER, OPFILE_ABSOLUTE_FOLDER
		));
	return OpStatus::OK;
}

OP_STATUS RemoteFrameworkManager::AddTestSuite(URL &testsuite_list)
{
	OpString8 url_name;

	testsuite_list.GetAttribute(URL::KName_Escaped, url_name);

	//output("Adding Testsuite master %s\n", url_name.CStr());
	if(testsuite_list.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS op_err = OpStatus::OK;
	OpAutoPtr<AutoFetch_Element> loader(ProduceTestSuiteMaster(testsuite_list, op_err));

	RETURN_IF_ERROR(op_err);

	return AddUpdater(loader.release());
}

AutoFetch_Element *RemoteFrameworkManager::ProduceTestSuiteMaster(URL &testsuite_list, OP_STATUS &op_err)
{
	op_err = OpStatus::OK;
	OpAutoPtr<RemoteFrameworkMaster> loader(OP_NEW(RemoteFrameworkMaster, (this)));

	if(!loader.get())
	{
		op_err = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}

	op_err = loader->Construct(testsuite_list);
	if(OpStatus::IsError(op_err))
		loader.reset();

	return loader.release();
}

OP_STATUS RemoteFrameworkManager::AddTestSuite(const OpStringC8 &testsuite_list, URL_CONTEXT_ID ctx)
{
	if(testsuite_list.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	if(ctx == 0)
	{
		RETURN_IF_ERROR(CheckContext());
		ctx = context_id;
	}

	OpStringC8 empty;
	URL url = g_url_api->GetURL(testsuite_list, empty, TRUE, ctx);

	if(url.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	return AddTestSuite(url);
}

OP_STATUS RemoteFrameworkManager::AddTestSuite(const OpStringC &testsuite_list, URL_CONTEXT_ID ctx)
{
	if(testsuite_list.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	if(ctx == 0)
	{
		RETURN_IF_ERROR(CheckContext());
		ctx = context_id;
	}

	OpStringC empty;
	URL url = g_url_api->GetURL(testsuite_list, empty, TRUE, ctx);

	if(url.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	return AddTestSuite(url);
}

OP_STATUS RemoteFrameworkManager::RemoteTextMaster::Construct(URL &testsuite_list)
{
	framework_url = testsuite_list;
	return URL_Updater::Construct(testsuite_list, MSG_NETSELFTEST_REMFRAME_LOADER, manager->GetMessageHandler());
}

OP_STATUS RemoteFrameworkManager::RemoteTextMaster::ResourceLoaded(URL &url)
{
	return ProcessFile();
}

OP_STATUS RemoteFrameworkManager::RemoteTextMaster::ProcessFile()
{
	OpAutoPtr<URL_DataDescriptor> desc(framework_url.GetDescriptor(NULL, URL::KFollowRedirect, TRUE));

	if(desc.get() == NULL)
		return OpStatus::ERR;

	BOOL more = FALSE;
	do{
		unsigned long buf_len = desc->RetrieveData(more);

		if(buf_len == 0)
			break;

		const char *start_pos, *pos = (const char *)desc->GetBuffer();
		unsigned long start_pos1=0, pos1 = 0;

		start_pos = pos;
		start_pos1 = pos1;
		while(pos1 < buf_len)
		{
			while(pos1 < buf_len && *pos != '\n'&& *pos != '\r')
			{
				pos ++;
				pos1++;
			}

			if(pos1 >= buf_len && more)
				continue;

			if(start_pos != pos)
			{
				OpString8 line;

				RETURN_IF_ERROR(line.Set(start_pos, pos1-start_pos1));

				line.Strip();

				RETURN_IF_ERROR(HandleTextLine(line));
			}
			while(pos1 < buf_len && (*pos == '\n'|| *pos == '\r'))
			{
				pos ++;
				pos1++;
			}
			start_pos = pos;
			start_pos1 = pos1;

		}

		desc->ConsumeData(start_pos1);
	}while(more);

	return OpStatus::OK;
}

OP_STATUS RemoteFrameworkManager::RemoteTextMaster::HandleTextLine(const OpStringC8 &line)
{
	if(line.IsEmpty())
		return OpStatus::OK;

	URL ref_url;

	OpAutoPtr<URL_DocSelfTest_Batch> batch(OP_NEW(URL_DocSelfTest_Batch, ()));
	RETURN_OOM_IF_NULL(batch.get());

	OpAutoPtr<JustLoad_Tester> test(OP_NEW(JustLoad_Tester, ()));
	RETURN_OOM_IF_NULL(test.get());

	RETURN_IF_ERROR(test->Construct(line, ref_url, TRUE));

	if(!batch->AddTestCase(test.release()))
		return OpStatus::ERR;

	manager->AddTestBatch(batch.release());

	return OpStatus::OK;
}


OP_STATUS RemoteFrameworkManager::RemoteFrameworkMaster::Construct(URL &testsuite_list)
{
	if(testsuite_list.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

	framework_url = testsuite_list;
	return XML_Updater::Construct(testsuite_list, MSG_NETSELFTEST_REMFRAME_LOADER, manager->GetMessageHandler());
}

OP_STATUS RemoteFrameworkManager::RemoteFrameworkMaster::ProcessFile()
{
	if(!parser.EnterElement(UNI_L("testsuites")))
		return OpStatus::ERR;

	if(XMLHasTag(parser, UNI_L("disabled")) || !CheckConditionals(parser, UNI_L("condition")))
	{
		parser.LeaveElement();
		return OpStatus::OK;
	}

	while(parser.EnterAnyElement())
	{
		if(parser.GetElementName() == UNI_L("testsuite"))
		{
			OpString url_name;

			RETURN_IF_ERROR(GetTextData(url_name));

			if(url_name.HasContent())
			{
				OP_STATUS op_err = OpStatus::OK;

				OpAutoPtr<RemoteFrameworkManager::RemoteFrameworkTestSuites> temp(ProduceTestSuite(parser.GetAttribute(UNI_L("type")), url_name, op_err));

				RETURN_IF_ERROR(op_err);

				RETURN_IF_ERROR(manager->AddTestSuiteLoader(temp.release()));
			}
		}
		else
		{
			RETURN_IF_ERROR(HandleUnknownKeyword());
		}

		parser.LeaveElement();
	}

	return OpStatus::OK;
}

OP_STATUS RemoteFrameworkManager::RemoteFrameworkMaster::HandleUnknownKeyword()
{
	return OpStatus::OK;
}

RemoteFrameworkManager::RemoteFrameworkTestSuites *RemoteFrameworkManager::RemoteFrameworkMaster::ProduceTestSuite(const OpStringC &suite_type, const OpStringC &url_string, OP_STATUS &op_err)
{
	op_err = OpStatus::OK;

	OpAutoPtr<RemoteFrameworkManager::RemoteFrameworkTestSuites> temp(OP_NEW(RemoteFrameworkManager::RemoteFrameworkTestSuites, (manager)));

	if(temp.get() == NULL)
	{
		op_err = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}

	URL url = g_url_api->GetURL(framework_url, url_string, TRUE);

	if(url.IsEmpty())
	{
		op_err = OpStatus::ERR;
		return NULL;
	}
		

	/*
	OpString8 url_name;

	framework_url.GetAttribute(URL::KName_Escaped, url_name);

	output("Adding Testsuite loaded %s\n", url_name.CStr());
	*/

	op_err = temp->Construct(url);
	RETURN_VALUE_IF_ERROR(op_err, NULL);

	return temp.release();
}

void RemoteFrameworkManager::TestSuiteLoader::OnAllUpdatersFinished()
{
	if(manager)
		manager->TestSuitesReady();
	else
		AutoFetch_Manager::OnAllUpdatersFinished();
}

void RemoteFrameworkManager::TestSuitesReady()
{
	if(Active() || suite_loader.Active())
		return; // Don't start testsuites until all are completely configured

	if(!added_testbatches)
	{
		ST_failed("No Testcases");
		return;
	}

	output("starting Testcases\n");
	if(!test_manager->SetLastBatch())
	{
		if(!test_manager->ReportedStatus())
			ST_failed("Starting batch failed");
	}
}

void RemoteFrameworkManager::OnAllUpdatersFinished()
{
	TestSuitesReady();
}


OP_STATUS RemoteFrameworkManager::AddTestSuiteLoader(RemoteFrameworkManager::RemoteFrameworkTestSuites *suite)
{
	if(!suite)
		return OpStatus::OK;

	OP_STATUS op_err = suite_loader.AddUpdater(suite);
	if(OpStatus::IsError(op_err))
	{
		suite_loader.Clear();
		ST_failed("Starting load of testsuite failed");
	}

	return op_err;
}


OP_STATUS RemoteFrameworkManager::RemoteFrameworkTestSuites::Construct(URL &testsuite_list)
{
	testsuite_url = testsuite_list;
	return XML_Updater::Construct(testsuite_list,  MSG_NETSELFTEST_REMFRAME_LOADER, manager->GetMessageHandler());
}

OP_STATUS RemoteFrameworkManager::RemoteFrameworkTestSuites::ProcessFile()
{
	if(!parser.EnterElement(UNI_L("testsuite")))
		return OpStatus::ERR;

	if(XMLHasTag(parser, UNI_L("disabled")) || !CheckConditionals(parser, UNI_L("condition")))
	{
		parser.LeaveElement();
		return OpStatus::OK;
	}

	OpString batch_name_prefix_default;
	RETURN_IF_ERROR(GetURL().GetAttribute(URL::KSuggestedFileName_L, batch_name_prefix_default));
	OpString8 batch_name_prefix_default8;
	RETURN_IF_ERROR(batch_name_prefix_default8.SetUTF8FromUTF16(batch_name_prefix_default));

	int batch_id = 0;
	while(parser.EnterAnyElement())
	{
		if(parser.GetElementName() == UNI_L("testbatch"))
		{
			batch_id++;

			OpString8 batch_name;
			RETURN_IF_ERROR(batch_name.SetUTF8FromUTF16(parser.GetAttribute(UNI_L("name"))));

			if(batch_name.IsEmpty())
				batch_name.AppendFormat("{%s}-%d", batch_name_prefix_default8.CStr(), batch_id);

			OpAutoPtr<URL_DocSelfTest_Batch> batch(OP_NEW(URL_DocSelfTest_Batch, ()));
			RETURN_OOM_IF_NULL(batch.get());
			batch->Construct(manager->GetMessageHandler());

			BOOL added_testcase = FALSE;

			batch->SetBatchID(batch_name);

			if(XMLHasTag(parser, UNI_L("disabled")) || !CheckConditionals(parser, UNI_L("condition")))
			{
				parser.LeaveElement();
				continue;
			}
			while(parser.EnterAnyElement())
			{
				if(parser.GetElementName() == UNI_L("test"))
				{
					OpString url_name;
					OpString passtype;
					OpString pass_condition;

					if(XMLHasTag(parser, UNI_L("disabled")) || !CheckConditionals(parser, UNI_L("condition")))
					{
						parser.LeaveElement();
						continue;
					}
					while(parser.EnterAnyElement())
					{
						if(parser.GetElementName() == UNI_L("url"))
						{
							RETURN_IF_ERROR(GetTextData(url_name));
						}
						else if(parser.GetElementName() == UNI_L("result"))
						{
							RETURN_IF_ERROR(passtype.Set(parser.GetAttribute(UNI_L("type"))));
							RETURN_IF_ERROR(GetTextData(pass_condition));
						}
						parser.LeaveElement();
					}
				
					if(url_name.HasContent())
					{
						OP_STATUS op_err = OpStatus::OK;

						OpAutoPtr<URL_DocSelfTest_Item> temp(ProduceTestcase(passtype, url_name, pass_condition, op_err));

						RETURN_IF_ERROR(op_err);

						if(temp.get() != NULL)
						{
							batch->AddTestCase(temp.release());
							added_testcase = TRUE;
							//output("Added Testcase\n");
						}
					}
				}
				else
				{
					RETURN_IF_ERROR(HandleUnknownTestLevel());
				}
				parser.LeaveElement();
			}
			if(added_testcase)
				manager->AddTestBatch(batch.release());
			//output("Added batch\n");
		}
		else
		{
			RETURN_IF_ERROR(HandleUnknownTestBatch());
		}

		parser.LeaveElement();
	}

	return OpStatus::OK;
}

OP_STATUS RemoteFrameworkManager::RemoteFrameworkTestSuites::HandleUnknownTestBatch()
{
	return OpStatus::OK;
}

OP_STATUS RemoteFrameworkManager::RemoteFrameworkTestSuites::HandleUnknownTestLevel()
{
	return OpStatus::OK;
}

URL_DocSelfTest_Item *RemoteFrameworkManager::RemoteFrameworkTestSuites::ProduceTestcase(const OpStringC &testcasetype, const OpStringC &url_name, const OpStringC &pass_data, OP_STATUS &op_err)
{
	URL url = g_url_api->GetURL(testsuite_url, url_name,TRUE);

	if(url.IsEmpty())
	{
		op_err = OpStatus::ERR;
		return NULL;
	}

	OpString8 url_name8;

	url.GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, url_name8);

	output("Adding Testcase %s\n", url_name8.CStr());

	op_err = OpStatus::OK;
	if(testcasetype.Compare("security-level") == 0)
	{
		if(pass_data.IsEmpty())
		{
			op_err = OpStatus::ERR;
			return NULL;
		}

		int sec_level = uni_atoi(pass_data.CStr());
		if(sec_level <=0 || sec_level >= SECURITY_STATE_UNKNOWN)
		{
			op_err = OpStatus::ERR;
			return NULL;
		}


		OpAutoPtr<Security_Level_Tester> tester(OP_NEW(Security_Level_Tester, (sec_level)));

		if(tester.get() == NULL)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
			return NULL;
		}

		URL empty;

		op_err = tester->Construct(url,empty);
		RETURN_VALUE_IF_ERROR(op_err, NULL);

		return tester.release();
	}
	else if(testcasetype.Compare("passcontent") == 0)
	{
		if(pass_data.IsEmpty())
		{
			op_err = OpStatus::ERR;
			return NULL;
		}

		OpAutoPtr<ScanPass_SimpleTester> tester(OP_NEW(ScanPass_SimpleTester,()));

		if(tester.get() == NULL)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
			return NULL;
		}

		URL empty;

		op_err = tester->Construct(url, empty, pass_data);
		RETURN_VALUE_IF_ERROR(op_err, NULL);

		return tester.release();
	}
	else if(testcasetype.Compare("passcontent-uni") == 0)
	{
		if(pass_data.IsEmpty())
		{
			op_err = OpStatus::ERR;
			return NULL;
		}

		OpAutoPtr<UniScanPass_SimpleTester> tester(OP_NEW(UniScanPass_SimpleTester,()));

		if(tester.get() == NULL)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
			return NULL;
		}

		URL empty;

		op_err = tester->Construct(url, empty, pass_data);
		RETURN_VALUE_IF_ERROR(op_err, NULL);

		return tester.release();
	}
	else if(testcasetype.Compare("matchcontent") == 0)
	{
		// Pass_data is a relative URL
		if(pass_data.IsEmpty())
		{
			op_err = OpStatus::ERR;
			return NULL;
		}
		URL match_url = g_url_api->GetURL(testsuite_url, pass_data, TRUE);

		if(match_url.IsEmpty())
		{
			op_err = OpStatus::ERR;
			return NULL;
		}

		OpAutoPtr<MatchWithURL_Tester> tester(OP_NEW(MatchWithURL_Tester,()));

		if(tester.get() == NULL)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
			return NULL;
		}

		URL empty;

		op_err = tester->Construct(url, empty, testsuite_url, match_url);
		RETURN_VALUE_IF_ERROR(op_err, NULL);

		return tester.release();
	}
	else
	{
		OpAutoPtr<JustLoad_Tester> tester(OP_NEW(JustLoad_Tester,()));

		if(tester.get() == NULL)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
			return NULL;
		}

		URL empty;

		op_err = tester->Construct(url, empty, FALSE);
		RETURN_VALUE_IF_ERROR(op_err, NULL);

		return tester.release();
	}
}

#endif  // defined SELFTEST && defined _NATIVE_SSL_SUPPORT_

