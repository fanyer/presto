/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * @author Daniel Spang
 */

#include "core/pch.h"

#include "modules/ecmascript/carakan/src/es_pch.h"
#include "programs/stopwatch.h"

#ifdef MSWIN

void StopWatch::GetTime(TimeVal *t)
{
    QueryPerformanceCounter(t);
}

double StopWatch::GetTimeDiff(TimeVal t1, TimeVal t2)
{
    TimeVal time, frequency;
    QueryPerformanceFrequency(&frequency);
    time.QuadPart = t2.QuadPart - t1.QuadPart;
    return 1000 * (double)time.QuadPart /
        (double)frequency.QuadPart;
}

#elif UNIX

void StopWatch::GetTime(TimeVal *t)
{
    gettimeofday(t, 0);
}

double StopWatch::GetTimeDiff(TimeVal t1, TimeVal t2)
{
    double t = double(t2.tv_sec) * 1000 + double(t2.tv_usec) / 1000;
    t -= double(t1.tv_sec) * 1000 + double(t1.tv_usec) / 1000;
    return t;
}

#endif // UNIX

void StopWatch::Start()
{
    GetTime(&start);
}

void StopWatch::Stop()
{
    GetTime(&stop);
}

double StopWatch::GetElapsedTime()
{
    return GetTimeDiff(start, stop);
}
