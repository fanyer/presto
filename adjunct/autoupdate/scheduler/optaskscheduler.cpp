#include "core/pch.h"

#include "adjunct/autoupdate/scheduler/optaskscheduler.h"

#include "modules/util/opfile/opfile.h"
#include "modules/prefsfile/prefsfile.h"

////////////////////////////////////////////////////////////////////////////////////////////////

OpScheduledTask::OpScheduledTask()
:  m_timeout(0),
   m_timer(NULL),
   m_listener(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

OpScheduledTask::~OpScheduledTask()
{
	g_task_scheduler->RemoveScheduledTask(this);
	OP_DELETE(m_timer);
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpScheduledTask::AddScheduledTask(time_t timeout)
{
	m_timeout = timeout;
	if (m_timer)
	{
		RETURN_IF_ERROR(g_task_scheduler->AddScheduledTask(this));

		RETURN_IF_ERROR(g_task_scheduler->SetTimeout(this, timeout));
		
		time_t last_timeout = 0;
		RETURN_IF_ERROR(g_task_scheduler->GetLastTimeout(this, last_timeout));
		
		time_t t = last_timeout + timeout - g_timecache->CurrentTime();

		if(t < 0)
			t = 0;

		m_timer->SetTimerListener(this);
		m_timer->Start(t*1000);

		return OpStatus::OK;
	}

	return OpStatus::ERR;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpScheduledTask::SetLastTimeout(time_t last_timeout)
{
	return g_task_scheduler->SetLastTimeout(this, last_timeout);
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpScheduledTask::InitTask(const char* task_name)
{
	RETURN_IF_ERROR(m_task_name.Set(task_name));
	
#ifdef HC_CAP_OPERA_RUN
	m_timer = OP_NEW(OpTimer, ());
#else
	g_systemFactory->CreateOpTimer(&m_timer);
#endif
	if(m_timer)
		return OpStatus::OK;
	
	return OpStatus::ERR_NO_MEMORY;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void OpScheduledTask::OnTimeOut(OpTimer* timer)
{
	if(m_listener)
	{
		m_listener->OnTaskTimeOut(this);
	}
	
	g_task_scheduler->SetLastTimeout(this, g_timecache->CurrentTime());
}

////////////////////////////////////////////////////////////////////////////////////////////////

OpTaskScheduler::OpTaskScheduler()
: m_prefs_file(NULL),
  m_tasks(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////////////////////

OpTaskScheduler::~OpTaskScheduler()
{
	Cleanup();
}

////////////////////////////////////////////////////////////////////////////////////////////////

OpTaskScheduler* OpTaskScheduler::GetInstance()
{
	static OpTaskScheduler task_scheduler;
	return &task_scheduler;
}

////////////////////////////////////////////////////////////////////////////////////////////////

void OpTaskScheduler::Cleanup()
{
	OP_DELETE(m_prefs_file);
	m_prefs_file = NULL;
	OP_DELETE(m_tasks);
	m_tasks = NULL;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpTaskScheduler::AddScheduledTask(OpScheduledTask* scheduled_task)
{
	if(!m_prefs_file)
		m_prefs_file = CreateTaskFile(UNI_L("tasks.xml"), OPFILE_HOME_FOLDER);
	
	if(!m_tasks)
		m_tasks = OP_NEW(OpString8HashTable<OpScheduledTask>, ());
	
	if(m_prefs_file)
	{
		OpString8* task_name = scheduled_task->GetTaskName();
		
		OpScheduledTask *task = NULL;
		
		OP_STATUS ret = m_tasks->GetData(task_name->CStr(), &task);
		if(OpStatus::IsError(ret))
		{
			OP_STATUS ret = m_tasks->Add(task_name->CStr(), scheduled_task);
			if(OpStatus::IsError(ret))
			{
				return ret;
			}
		}
		else
		{
			if(task != scheduled_task)
				return OpStatus::ERR;
		}
		
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpTaskScheduler::RemoveScheduledTask(OpScheduledTask* scheduled_task)
{
	if(m_tasks)
	{
		OpScheduledTask* task = NULL;
		OpString8* task_name = scheduled_task->GetTaskName();

		if(!task_name || !task_name->HasContent())
			return OpStatus::ERR;

		return m_tasks->Remove(task_name->CStr(), &task);
	}
	return OpStatus::ERR;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpTaskScheduler::SetLastTimeout(OpScheduledTask* scheduled_task, time_t last_timeout)
{
	OpString8* task_name = scheduled_task->GetTaskName();

	if(!task_name || !task_name->HasContent())
		return OpStatus::ERR;
	
	if(!m_prefs_file)
		m_prefs_file = CreateTaskFile(UNI_L("tasks.xml"), OPFILE_HOME_FOLDER);
	
	if(m_prefs_file)
	{
		TRAPD(err, m_prefs_file->WriteIntL(*task_name, "last_timeout", last_timeout));		
		if(OpStatus::IsError(err))
			return err;
		RETURN_IF_LEAVE(m_prefs_file->CommitL());
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpTaskScheduler::GetLastTimeout(OpScheduledTask* scheduled_task, time_t &last_timeout)
{
	OpString8* task_name = scheduled_task->GetTaskName();
	
	if(!task_name || !task_name->HasContent())
		return OpStatus::ERR;
	
	if(!m_prefs_file)
		m_prefs_file = CreateTaskFile(UNI_L("tasks.xml"), OPFILE_HOME_FOLDER);
	
	if(m_prefs_file)
	{
		TRAPD(err, last_timeout = m_prefs_file->ReadIntL(*task_name, "last_timeout"));		
		return err;
	}
	return OpStatus::ERR;
}

////////////////////////////////////////////////////////////////////////////////////////////////

OP_STATUS OpTaskScheduler::SetTimeout(OpScheduledTask* scheduled_task, time_t timeout)
{
	OpString8* task_name = scheduled_task->GetTaskName();
	
	if(!task_name || !task_name->HasContent())
		return OpStatus::ERR;
	
	if(!m_prefs_file)
		m_prefs_file = CreateTaskFile(UNI_L("tasks.xml"), OPFILE_HOME_FOLDER);
	
	if(m_prefs_file)
	{
		TRAPD(err, m_prefs_file->WriteIntL(*task_name, "timeout", timeout));		
		if(OpStatus::IsError(err))
			return err;
		RETURN_IF_LEAVE(m_prefs_file->CommitL());
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

////////////////////////////////////////////////////////////////////////////////////////////////

PrefsFile* OpTaskScheduler::CreateTaskFile(const uni_char *filename, OpFileFolder folder)
{
	OpFile file;
	
	if (OpStatus::IsError(file.Construct(filename, folder)))
		return NULL;
	
	PrefsFile* OP_MEMORY_VAR prefsfile = OP_NEW(PrefsFile, (PREFS_XML));
	if (!prefsfile)
		return NULL;
	
	TRAPD(err, prefsfile->ConstructL());
	if (OpStatus::IsError(err))
	{
		OP_DELETE(prefsfile);
		return NULL;
	}
	
	TRAP(err, prefsfile->SetFileL(&file));
	if (OpStatus::IsError(err))
	{
		OP_DELETE(prefsfile);
		return NULL;
	}
	
	TRAP(err, prefsfile->LoadAllL());
	
	return prefsfile;
}

////////////////////////////////////////////////////////////////////////////////////////////////
