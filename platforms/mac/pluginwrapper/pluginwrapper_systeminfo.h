/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "modules/pi/OpSystemInfo.h"

class PluginSystemInfo : public OpSystemInfo
{
public:
	virtual void GetWallClock(unsigned long& seconds, unsigned long& milliseconds) {}
	virtual double GetWallClockResolution() { return 0; }
	virtual double GetTimeUTC() { return 0; }
	virtual double GetRuntimeMS();
	virtual unsigned int GetRuntimeTickMS() { return 0; }
	virtual long GetTimezone() { return 0; }
	virtual double DaylightSavingsTimeAdjustmentMS(double t) { return 0; }
	virtual UINT32 GetPhysicalMemorySizeMB() { return 0; }
	virtual OP_STATUS GetAvailablePhysicalMemorySizeMB(UINT32* result) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual void GetProxySettingsL(const uni_char *protocol, int &enabled, OpString &proxyserver) {}
	virtual void GetAutoProxyURLL(OpString *url, BOOL *enabled) {}
	virtual void GetProxyExceptionsL(OpString *exceptions, BOOL *enabled) {}
	virtual void GetPluginPathL(const OpStringC &dfpath, OpString &newpath) {}
    virtual OP_STATUS GetStaticPlugin(struct StaticPlugin& plugin_spec) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS DetectPlugins(const OpStringC& suggested_plugin_paths, class OpPluginDetectionListener* listener) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL IsInMainThread() { return TRUE; }
	virtual INT32 GetCurrentProcessId() { return 0; }
	virtual OP_STATUS GetBinaryPath(OpString *path) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual const char *GetSystemEncodingL() { return NULL; }
	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, OpString* out) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetFileTypeName(const uni_char* filename, uni_char *out, size_t out_max_len) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetProtocolHandler(const OpString& uri_string, OpString& protocol, OpString& handler) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetFileHandler(const OpString* filename, OpString& contentType, OpString& handler) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL GetIsExecutable(OpString* filename) { return FALSE; }
	virtual INT32 GetShiftKeyState() { return 0; }
	virtual const uni_char* GetDefaultTextEditorL() { return NULL; }
	virtual OP_STATUS ExecuteApplication(const uni_char* application, const uni_char* args, BOOL silent_errors = FALSE) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetSystemIp(OpString& ip) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual const char *GetOSStr(UA_BaseStringId ua) { return NULL; }
	virtual const uni_char *GetPlatformStr() { return NULL; }
	virtual OP_STATUS GetDeviceFirmware(OpString* firmware) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetDeviceManufacturer(OpString* manufacturer) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetDeviceModel(OpString* model) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS OpFileLengthToString(OpFileLength length, OpString8* result) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL HasSyncLookup() { return FALSE; }
	virtual OP_STATUS GetDnsAddress(OpSocketAddress** dns_address, int number = 0) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetDnsSuffixes(OpString* result) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OpAutoVector<OpSocketAddress> *ResolveExternalHost(const uni_char* hostname) { return NULL; }
	virtual OP_STATUS GetUserLanguages(OpString *result) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual void GetUserCountry(uni_char result[3]) {}
	virtual OP_STATUS GenerateGuid(OpGuid &guid) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual const uni_char* GetUniqueFileNamePattern() { return NULL; }
	virtual OP_STATUS PathsEqual(const uni_char* p1, const uni_char* p2, BOOL* equal) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual const char* GetSubscriberID() { return NULL; }
	virtual const char* GetDRMVersion() { return NULL; }
	virtual const char* GetAppId() { return NULL; }
	virtual BOOL IsLocalFile(const uni_char *str) { return FALSE; }
	virtual double GetSoundVolumeL() { return 0; }
	virtual BOOL IsScreenReaderRunning() { return FALSE; }
	virtual OP_STATUS GetProcessorUtilizationPercent(double* utilization_percent) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetProcessTime(double *time) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL SupportsVFP() { return FALSE; }
#ifdef OPSYSTEMINFO_CPU_FEATURES
	virtual unsigned int GetCPUFeatures() { return CPU_FEATURES_NONE; }
#endif
};
