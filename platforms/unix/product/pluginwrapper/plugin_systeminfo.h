/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "modules/pi/OpSystemInfo.h"

class PluginSystemInfo : public OpSystemInfo
{
public:
	// We need an implementation of this function in the plugin wrapper
	virtual double GetRuntimeMS();

	// Other functions unimplemented
	virtual void GetWallClock(unsigned long& seconds, unsigned long& milliseconds) { OP_ASSERT(!"Not implemented!"); }
	virtual double GetWallClockResolution() { OP_ASSERT(!"Not implemented!"); return 0; }
	virtual double GetTimeUTC() { OP_ASSERT(!"Not implemented!"); return 0; }
	virtual unsigned int GetRuntimeTickMS() { OP_ASSERT(!"Not implemented!"); return 0; }
	virtual long GetTimezone() { OP_ASSERT(!"Not implemented!"); return 0; }
	virtual double DaylightSavingsTimeAdjustmentMS(double t) { OP_ASSERT(!"Not implemented!"); return 0; }
	virtual UINT32 GetPhysicalMemorySizeMB() { OP_ASSERT(!"Not implemented!"); return 0; }
	virtual OP_STATUS GetAvailablePhysicalMemorySizeMB(UINT32* result) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual void GetProxySettingsL(const uni_char *protocol, int &enabled, OpString &proxyserver) { OP_ASSERT(!"Not implemented!"); }
	virtual void GetAutoProxyURLL(OpString *url, BOOL *enabled) { OP_ASSERT(!"Not implemented!"); }
	virtual void GetProxyExceptionsL(OpString *exceptions, BOOL *enabled) { OP_ASSERT(!"Not implemented!"); }
	virtual void GetPluginPathL(const OpStringC &dfpath, OpString &newpath) { OP_ASSERT(!"Not implemented!"); }
    virtual OP_STATUS GetStaticPlugin(struct StaticPlugin& plugin_spec) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS DetectPlugins(const OpStringC& suggested_plugin_paths, class OpPluginDetectionListener* listener) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL IsInMainThread() { OP_ASSERT(!"Not implemented!"); return TRUE; }
	virtual INT32 GetCurrentProcessId() { OP_ASSERT(!"Not implemented!"); return 0; }
	virtual OP_STATUS GetBinaryPath(OpString *path) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual const char *GetSystemEncodingL() { OP_ASSERT(!"Not implemented!"); return NULL; }
	virtual OP_STATUS ExpandSystemVariablesInString(const uni_char* in, OpString* out) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetFileTypeName(const uni_char* filename, uni_char *out, size_t out_max_len) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetProtocolHandler(const OpString& uri_string, OpString& protocol, OpString& handler) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetFileHandler(const OpString* filename, OpString& contentType, OpString& handler) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL GetIsExecutable(OpString* filename) { OP_ASSERT(!"Not implemented!"); return FALSE; }
	virtual INT32 GetShiftKeyState() { OP_ASSERT(!"Not implemented!"); return 0; }
	virtual const uni_char* GetDefaultTextEditorL() { OP_ASSERT(!"Not implemented!"); return NULL; }
	virtual OP_STATUS ExecuteApplication(const uni_char* application, const uni_char* args, BOOL) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetSystemIp(OpString& ip) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual const char *GetOSStr(UA_BaseStringId ua) { OP_ASSERT(!"Not implemented!"); return NULL; }
	virtual const uni_char *GetPlatformStr() { OP_ASSERT(!"Not implemented!"); return NULL; }
	virtual OP_STATUS GetDeviceFirmware(OpString* firmware) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetDeviceManufacturer(OpString* manufacturer) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetDeviceModel(OpString* model) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS OpFileLengthToString(OpFileLength length, OpString8* result) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL HasSyncLookup() { OP_ASSERT(!"Not implemented!"); return FALSE; }
	virtual OP_STATUS GetDnsAddress(OpSocketAddress** dns_address, int number = 0) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetDnsSuffixes(OpString* result) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OpAutoVector<OpSocketAddress> *ResolveExternalHost(const uni_char* hostname) { OP_ASSERT(!"Not implemented!"); return NULL; }
	virtual OP_STATUS GetUserLanguages(OpString *result) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual void GetUserCountry(uni_char result[3]) { OP_ASSERT(!"Not implemented!"); }
	virtual OP_STATUS GenerateGuid(OpGuid &guid) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual const uni_char* GetUniqueFileNamePattern() { OP_ASSERT(!"Not implemented!"); return NULL; }
	virtual OP_STATUS PathsEqual(const uni_char* p1, const uni_char* p2, BOOL* equal) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual const char* GetSubscriberID() { OP_ASSERT(!"Not implemented!"); return NULL; }
	virtual const char* GetDRMVersion() { OP_ASSERT(!"Not implemented!"); return NULL; }
	virtual const char* GetAppId() { OP_ASSERT(!"Not implemented!"); return NULL; }
	virtual BOOL IsLocalFile(const uni_char *str) { OP_ASSERT(!"Not implemented!"); return FALSE; }
	virtual double GetSoundVolumeL() { OP_ASSERT(!"Not implemented!"); return 0; }
	virtual BOOL IsScreenReaderRunning() { OP_ASSERT(!"Not implemented!"); return FALSE; }
	virtual OP_STATUS GetProcessorUtilizationPercent(double* utilization_percent) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS GetProcessTime(double *time) { OP_ASSERT(!"Not implemented!"); return OpStatus::ERR_NOT_SUPPORTED; }
	virtual BOOL SupportsVFP() { OP_ASSERT(!"Not implemented!"); return FALSE; }
	virtual unsigned int GetCPUFeatures() { OP_ASSERT(!"Not implemented!"); return 0; }
};
