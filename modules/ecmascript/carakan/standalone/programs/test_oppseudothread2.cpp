#include "core/pch.h"
#include "modules/ecmascript/oppseudothread/oppseudothread.h"

BOOL ResumeThread(OpPseudoThread *thread)
{
  return thread->Resume();
}
