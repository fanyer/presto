/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef SELFTEST_ASYNCTEST_H
#define SELFTEST_ASYNCTEST_H

#include "modules/selftest/testutils.h"


#define ASYNC_SELFTEST_VERIFY(assertion, line)								\
if (!(assertion))															\
{																			\
	TestFailed("Failed: " #assertion, OpStatus::ERR, line);					\
	return OpStatus::OK;													\
}

#define ASYNC_SELFTEST_VERIFY_SUCCESS(status, line)						\
if (OpStatus::IsError(status))											\
{																		\
	TestFailed("Failed: " #status "  yields an error.", status, line);	\
	return OpStatus::OK;												\
}


/** A helper class for creating multistage asynchronous tests.
  *
  * To use this mechanism, create a subclass and override the
  * TestStep function. Usually it looks somewhat like this:
  *
  *    OP_STATUS TestStep(current_step) {
  *        switch(current_step) {
  *        case 0:
  *            // initiate some asynchronous operation
  *            break;
  *        case 1:
  *            ASYNC_SELFTEST_VERIFY(a == b, __LINE__); // check result of the asynchronous operation
  *            // code of the second step
  *            break;
  *        case 2:
  *            // code of the last step
  *            TestSuccess();
  *            break;
  *        }
  *        return OpStatus::OK;
  *    }
  *
  * Each asynchronous operation is responsible to advance the test by calling
  * Continue.
  * In the example above the operation initiated in case 0 must call Continue
  * when it finishes, then TestStep will be called with the next value of
  * current_step.
  *
  * In selftests this class is usually defined in the global section and
  * then used like that (assume MyAsyncTest derives from AsyncTest):
  *
  *    test("My async test")
  *        async;
  *    {
  *        MyAsyncTest* test = OP_NEW(MyAsyncTest, ());
  *        if (!test)
  *            ST_failed_with_status("Unable to create test", OpStatus::ERR_NO_MEMORY, __LINE__);
  *        else {
  *            ST_delete_after_group(test);
  *            test->Start();
  *        }
  *    }
  */
class AsyncTest : private MessageObject
{
public:
	AsyncTest()
		: m_current_step(0)
	{}

	virtual ~AsyncTest()
	{
		if (g_main_message_handler)
			g_main_message_handler->RemoveCallBack(this, MSG_PI_SELFTESTHELPER_ASYNC_STEP);
	}

	/** Starts the test execution.
	  *
	  * Calls Construct and if it succeeds starts the test.
	  * TestStep will be called with current_step being 0.
	  *
	  * If Construct returns an error, the test fails.
	  */
	void Start()
	{
		OP_STATUS status = Construct();
		if (OpStatus::IsError(status))
			TestFailed("Test construction failed unexpectedly", status, __LINE__);
		else if (g_main_message_handler)
		{
			g_main_message_handler->SetCallBack(this, MSG_PI_SELFTESTHELPER_ASYNC_STEP, reinterpret_cast<MH_PARAM_1>(this));
			AsyncStep();
		}
		else
		{
			TestFailed("Test step failed unexpectedly", status, __LINE__);
		}
	}

	/** Advances the test.
	  *
	  * It should be called when the test is ready to advance to then
	  * next step. Schedules TestStep to be executed with an incremented
	  * current step number.
	  */
	void Continue()
	{
		++m_current_step;
		OnContinue();
		AsyncStep();
	}

	/** Used to signal test failure.
	  *
	  * No further action should be performed after calling this
	  * function that. (it will schedule the next test in the
	  * selftest group).
	  *
	  * It calls the Cleanup method.
	  */
	void TestFailed(const char* comment, OP_STATUS error, UINT32 line)
	{
		Cleanup();
		ST_failed_with_status(comment, error, line);
	}

	/** Used to signal that the test has finished successfully.
	  *
	  * No further action should be performed after calling this
	  * function that. (it will schedule the next test in the
	  * selftest group).
	  *
	  * It calls the Cleanup method.
	  */
	void TestSuccess()
	{
		Cleanup();
		ST_passed();
	}
protected:
	/** Perform additional setup of the test.
	  *
	  * @return OK on success, the test may be run.
	  * Error values if the setup has failed and the test cannot be started.
	  */
	virtual OP_STATUS Construct() { return OpStatus::OK; }

	/** The current asynchronous step has been finished.
	  *
	  * Called after Continue has been invoked.
	  * May be overriden to perform some accounting step before
	  * TestStep is called again.
	  * Could be used e.g. for time measurement.
	  */
	virtual void OnContinue() {}

	/** Implements the test steps.
	  *
	  * This function implements the body of the test.
	  * It is called at the start of the test and then after each call to
	  * Continue.
	  * This means that after each step the test code is required to advance
	  * the test by handling asynchronous notifications itself.
	  *
	  * Test failure should be reported by calling TestFailed,
	  * not by returning any other values than OK from this
	  * method.
	  *
	  * @param current_step Number of the step to perform. The first step
	  * (initiated by the Start method) is numbered 0.
	  *
	  * @return 
	  *  - OK - step performed without errors (even if a test assertion fails).
	  *  - ERR - the test cannot be performed for some reason.
	  */
	virtual OP_STATUS TestStep(unsigned current_step) = 0;

	/** Perform cleanup after the test has finished.
	  *
	  * Called after the test finishes (either
	  * successfully or not).
	  */
	virtual void Cleanup() {}

private:
	void AsyncStep()
	{
		if (g_main_message_handler)
			g_main_message_handler->PostMessage(MSG_PI_SELFTESTHELPER_ASYNC_STEP, reinterpret_cast<MH_PARAM_1>(this), 0);
		else
			TestFailed("Test step failed unexpectedly - g_main_message_handler == NULL", OpStatus::ERR, __LINE__);
	}

	/**
		* @name Implementation of MessageObject
		* @{
		*/
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
		{
			OP_ASSERT(MSG_PI_SELFTESTHELPER_ASYNC_STEP == msg);
			OP_ASSERT(reinterpret_cast<MH_PARAM_1>(this) == par1);
			OP_STATUS status = TestStep(m_current_step);
			if (OpStatus::IsError(status))
				TestFailed("Test step failed unexpectedly", status, __LINE__);
		}

	/** @} */ // Implementation of MessageObject

	unsigned m_current_step;
};

#endif // SELFTEST_ASYNCTEST_H
