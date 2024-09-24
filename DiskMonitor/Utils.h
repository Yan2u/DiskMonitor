#pragma once

#include "DiskInfo.h"

#include <afx.h>
#include <vector>

#include <comdef.h>
#include <WbemIdl.h>

#define DISKMONITOR_LOG_FILE L"./DiskMonitor.dll.log"
#define DISKMONITOR_LOG_FILE_TEMPLATE L"%s\\DiskMonitor.dll.log"

class Utils
{
protected:
	static CString errorMessage;
	static CString logFilePath;
	static void assertHResult(HRESULT hr);
	static void initWbemService(IWbemLocator*& pLoc, IWbemServices*& pSvc, const wchar_t* path);
	static DWORD GetPhysicalDiskDeviceId(const wchar_t* diskId);
	static void logToFile(const wchar_t* filename, const wchar_t* prefix, const wchar_t* message);

public:
	static CString GetErrorString(DWORD errorNumber);
	static CString FormatDataAmount(double dataAmount);
	static CString FormatSpeed(double speed);
	static CString LoadStringTable(UINT stringID);
	static CString FormatCString(const wchar_t* format, ...);
	static CString GetErrorMessage();
	static void GetAllDiskInfo(std::vector<DiskInfo>& diskInfos);
	static void ClearPrivateProfile(const wchar_t* filename);

	static CString GetCurrentModuleFileDirectory();
	static CString GetCurrentWorkingDirectory();

	static void SetLogFilePath(const wchar_t* path);
	static void LogInfo(const wchar_t* format, ...);
	static void LogError(const wchar_t* format, ...);
};

