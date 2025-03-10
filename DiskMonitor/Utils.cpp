#include "pch.h"
#include "Utils.h"
#include "resource.h"

#include <afx.h>
#include <propvarutil.h>
#include <winioctl.h>
#include <fstream>

CString Utils::logFilePath;

void Utils::assertHResult(HRESULT hr)
{
	if (FAILED(hr))
	{
		_com_error err(hr);
		DISKMONITOR_ASSERT(!(FAILED(hr)), IDS_GETDISKINFO_ERROR, err.ErrorMessage());
	}
}

void Utils::initWbemService(IWbemLocator*& pLoc, IWbemServices*& pSvc, const wchar_t* path)
{
	HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID*)&pLoc);
	assertHResult(hr);

	hr = pLoc->ConnectServer(_bstr_t(path), NULL, NULL, 0, NULL, 0, 0, &pSvc);
	assertHResult(hr);

	hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
	assertHResult(hr);
}

DWORD Utils::GetPhysicalDiskDeviceId(const wchar_t* diskId)
{
	// using STORAGE_DEVICE_NUMBER
	// using DiskIoControl

	HANDLE hDevice = CreateFile(Utils::FormatCString(L"\\\\.\\%s", diskId), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	DISKMONITOR_ASSERT(
		hDevice != INVALID_HANDLE_VALUE,
		IDS_GETPHYSICALDISK_ERROR,
		Utils::GetErrorString(GetLastError())
	);

	DWORD bytesReturned;
	STORAGE_DEVICE_NUMBER sdn;

	BOOL result = DeviceIoControl(hDevice, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &sdn, sizeof(sdn), &bytesReturned, NULL);
	DISKMONITOR_ASSERT(
		result,
		IDS_GETPHYSICALDISK_ERROR,
		Utils::GetErrorString(GetLastError())
	);

	CloseHandle(hDevice);
	return sdn.DeviceNumber;
}

void Utils::logToFile(const wchar_t* filename, const wchar_t* prefix, const wchar_t* message)
{
	std::ifstream ifs(filename, std::ios::ate | std::ios::binary);
	if (ifs.tellg() > (uint64_t)1024 * 1024)
	{
		std::ofstream ofs(filename, std::ios::trunc);
		ofs.close();
	}

	static CString logMessage;
	SYSTEMTIME st;
	GetLocalTime(&st);
	logMessage.Format(L"[%s] [%02d-%02d-%02d %02d:%02d:%02d] %s", prefix, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, message);

	std::wofstream ofs(filename, std::ios::app);
	ofs.imbue(std::locale("chs"));
	ofs << logMessage.GetString() << std::endl;
	ofs.close();
}

CString Utils::GetErrorString(DWORD errorNumber)
{
	CString result;
	LPTSTR buffer = nullptr;
	DWORD size = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, errorNumber, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&buffer, 0, NULL);
	if (size > 0)
	{
		result = buffer;
		LocalFree(buffer);
	}
	else
	{
		result = Utils::FormatCString(IDS_GETERRORSTRING_ERROR, errorNumber);
	}

	return result;
}

CString Utils::FormatDataAmount(double dataAmount)
{
	CString result;
	if (dataAmount < 1024.0)
	{
		result.Format(L"%.2f B", dataAmount);
	}
	else if (dataAmount < 1024.0 * 1024.0)
	{
		result.Format(L"%.2f KB", dataAmount / 1024.0);
	}
	else if (dataAmount < 1024.0 * 1024.0 * 1024.0)
	{
		result.Format(L"%.2f MB", dataAmount / 1024.0 / 1024.0);
	}
	else
	{
		result.Format(L"%.2f GB", dataAmount / 1024.0 / 1024.0 / 1024.0);
	}

	return result;
}

CString Utils::FormatSpeed(double speed)
{
	return Utils::FormatDataAmount(speed) + L"/s";
}

CString Utils::LoadStringTable(UINT stringID)
{
	CString result;
	result.LoadString(stringID);

	return result;
}

CString Utils::FormatCString(const wchar_t* format, ...)
{
	CString result;
	va_list args;
	va_start(args, format);
	result.FormatV(format, args);
	va_end(args);

	return result;
}

CString Utils::FormatCString(UINT stringID, ...)
{
	static CString format;
	format.LoadString(stringID);

	CString result;
	va_list args;
	va_start(args, stringID);
	result.FormatV(format.GetString(), args);
	va_end(args);

	return result;
}

void Utils::GetAllDiskInfo(std::vector<DiskInfo>& diskInfos)
{
	// Try APAERTMENTTHREADED first, if failed, try MULTITHREADED

	HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);

	if (FAILED(hr))
	{
		hr = CoInitializeEx(0, COINIT_MULTITHREADED);
	}

	assertHResult(hr);

	diskInfos.clear();

	IWbemLocator* pLoc = nullptr;
	IWbemServices* pSvc = nullptr;

	// Physical Disks

	initWbemService(pLoc, pSvc, L"ROOT\\Microsoft\\Windows\\Storage");

	IEnumWbemClassObject* pEnumerator = nullptr;
	hr = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT * FROM MSFT_PhysicalDisk"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator
	);
	assertHResult(hr);

	IWbemClassObject* pclsObj = nullptr;
	ULONG uReturn = 0;

	while (pEnumerator)
	{
		hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (0 == uReturn)
		{
			break;
		}

		DiskInfo diskInfo;

		VARIANT vtProp;
		hr = pclsObj->Get(L"MediaType", 0, &vtProp, 0, 0);
		assertHResult(hr);
		switch (vtProp.uintVal)
		{
		case 3:
			diskInfo.Type = DiskInfo::DiskType::HDD;
			break;
		case 4:
			diskInfo.Type = DiskInfo::DiskType::SSD;
			break;
		case 5:
			diskInfo.Type = DiskInfo::DiskType::SCM;
			break;
		default:
			diskInfo.Type = DiskInfo::DiskType::UNKNOWN;
			break;
		}

		hr = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
		assertHResult(hr);
		diskInfo.Id = vtProp.bstrVal;

		hr = pclsObj->Get(L"Size", 0, &vtProp, 0, 0);
		assertHResult(hr);
		uint64_t sizeUi64Val;
		VariantToUInt64(vtProp, &sizeUi64Val);
		diskInfo.Size = sizeUi64Val;

		hr = pclsObj->Get(L"FriendlyName", 0, &vtProp, 0, 0);
		assertHResult(hr);
		diskInfo.Caption = COLE2CT(vtProp.bstrVal);

		diskInfo.DeviceId = _wtoi(diskInfo.Id);

		diskInfos.push_back(diskInfo);

		VariantClear(&vtProp);
		pclsObj->Release();
	}

	pLoc->Release();
	pSvc->Release();
	pEnumerator->Release();

	// Logical Disks

	initWbemService(pLoc, pSvc, L"ROOT\\CIMV2");

	hr = pSvc->ExecQuery(
		bstr_t("WQL"),
		bstr_t("SELECT * FROM Win32_LogicalDisk"),
		WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
		NULL,
		&pEnumerator
	);
	assertHResult(hr);

	while (pEnumerator)
	{
		hr = pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn);
		if (0 == uReturn)
		{
			break;
		}

		DiskInfo diskInfo;

		VARIANT vtProp;
		hr = pclsObj->Get(L"VolumeName", 0, &vtProp, 0, 0);
		assertHResult(hr);
		diskInfo.Caption = COLE2CT(vtProp.bstrVal);

		if (diskInfo.Caption.IsEmpty())
		{
			diskInfo.Caption = Utils::FormatCString(IDS_DEFAULT_LOGICAL_DISK_CAPTION, diskInfo.Id);
		}

		hr = pclsObj->Get(L"DeviceID", 0, &vtProp, 0, 0);
		assertHResult(hr);
		diskInfo.Id = COLE2CT(vtProp.bstrVal);

		hr = pclsObj->Get(L"Size", 0, &vtProp, 0, 0);
		assertHResult(hr);
		uint64_t sizeUi64Val;
		VariantToUInt64(vtProp, &sizeUi64Val);
		diskInfo.Size = sizeUi64Val;

		diskInfo.Type = DiskInfo::DiskType::LOGICAL;
		diskInfo.DeviceId = Utils::GetPhysicalDiskDeviceId(diskInfo.Id);

		diskInfos.push_back(diskInfo);

		VariantClear(&vtProp);
		pclsObj->Release();
	}

	pLoc->Release();
	pSvc->Release();
	pEnumerator->Release();
	CoUninitialize();
}

void Utils::ClearPrivateProfile(const wchar_t* filename)
{
	std::ofstream ofs(filename, std::ios::trunc);
	ofs.close();
}

CString Utils::GetCurrentModuleFileDirectory()
{
	CString result;
	TCHAR buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	PathRemoveFileSpec(buffer);
	result = buffer;

	return result;
}

CString Utils::GetCurrentWorkingDirectory()
{
	CString result;
	TCHAR buffer[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, buffer);
	result = buffer;

	return result;
}

void Utils::SetLogFilePath(const wchar_t* path)
{
	logFilePath = path;
}

void Utils::LogInfo(const wchar_t* format, ...)
{
	static CString message;
	va_list args;
	va_start(args, format);
	message.FormatV(format, args);
	va_end(args);

	logToFile(Utils::logFilePath, L"INFO", message.GetString());
}

void Utils::LogError(const wchar_t* format, ...)
{
	static CString message;
	va_list args;
	va_start(args, format);
	message.FormatV(format, args);
	va_end(args);

	logToFile(Utils::logFilePath, L"ERROR", message.GetString());
}

void Utils::LogInfo(UINT stringID, ...)
{
	static CString message, result;
	message.LoadString(stringID);
	va_list args;
	va_start(args, stringID);
	result.FormatV(message.GetString(), args);
	va_end(args);

	logToFile(Utils::logFilePath, L"INFO", result.GetString());
}

void Utils::LogError(UINT stringID, ...)
{
	static CString message, result;
	message.LoadString(stringID);
	va_list args;
	va_start(args, stringID);
	result.FormatV(message, args);
	va_end(args);

	logToFile(Utils::logFilePath, L"ERROR", result.GetString());
}

bool Utils::require(bool pred, const wchar_t* format, ...)
{
	static CString message;

	if (!pred)
	{
		va_list args;
		va_start(args, format);
		message.FormatV(format, args);
		va_end(args);

		AfxMessageBox(message, MB_OK);
		return false;
	}

	return true;
}

bool Utils::require(bool pred, UINT stringID, ...)
{
	static CString format, message;

	if (!pred)
	{
		format.LoadString(stringID);

		va_list args;
		va_start(args, stringID);
		message.FormatV(format.GetString(), args);
		va_end(args);

		AfxMessageBox(message, MB_OK);
		
		return false;
	}

	return true;
}

void Utils::assertCritical(CString file, int line, CString expr, bool pred, const wchar_t* format, ...)
{
	static CString message;

	if (!pred)
	{
		va_list args;
		va_start(args, format);
		message.FormatV(format, args);
		va_end(args);

		AfxMessageBox(message, MB_OK);
		Utils::LogError(IDS_ASSERT_FAIL, file, line, expr, message.GetString());
		exit(1);
	}
}

void Utils::assertCritical(CString file, int line, CString expr, bool pred, UINT stringID, ...)
{
	static CString format, message;

	if (!pred)
	{
		format.LoadString(stringID);

		va_list args;
		va_start(args, stringID);
		message.FormatV(format.GetString(), args);
		va_end(args);

		AfxMessageBox(message, MB_OK);
		Utils::LogError(IDS_ASSERT_FAIL, file, line, expr, message.GetString());
		exit(1);
	}
}