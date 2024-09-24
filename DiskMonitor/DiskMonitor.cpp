// DiskMonitor.cpp: 定义 DLL 的初始化例程。
//

#include "pch.h"
#include "framework.h"
#include "afx.h"
#include "resource.h"
#include "Utils.h"
#include "CConfigDialog.h"
#include "DiskMonitor.h"

#include <vector>
#include <winioctl.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

void DiskMonitor::LoadConfig()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	bool isError = false;

	CString prefixRead, prefixWrite;
	CString defaultPrefix;

	defaultPrefix.LoadStringW(IDS_CONFIG_EXAMPLE_READ);
	GetPrivateProfileStringW(L"DiskMonitor", L"PrefixRead", defaultPrefix.GetString(), prefixRead.GetBuffer(DISKINFO_BUFFER_SIZE), DISKINFO_BUFFER_SIZE, ConfigFilePath.GetString());
	prefixRead.ReleaseBuffer();

	defaultPrefix.LoadStringW(IDS_CONFIG_EXAMPLE_WRITE);
	GetPrivateProfileStringW(L"DiskMonitor", L"PrefixWrite", defaultPrefix.GetString(), prefixWrite.GetBuffer(DISKINFO_BUFFER_SIZE), DISKINFO_BUFFER_SIZE, ConfigFilePath.GetString());
	prefixWrite.ReleaseBuffer();

	m_speedRead.Prefix = prefixRead;
	m_speedWrite.Prefix = prefixWrite;

	m_allDiskInfos.clear();
	m_monitoredDiskInfos.clear();

	int n = GetPrivateProfileIntW(L"DiskMonitor", L"All", -1, ConfigFilePath.GetString());
	if (n < 0)
	{
		Utils::LogError(L"Load All DiskInfo Error...");
		isError = true;
	}
	else
	{
		for (int i = 0; i < n; ++i)
		{
			DiskInfo diskInfo = DiskInfo::LoadDiskInfo(ConfigFilePath.GetString(), Utils::FormatCString(L"ALL_%d", i));
			if (diskInfo.Type == DiskInfo::DiskType::UNKNOWN)
			{
				isError = true;
				break;
			}
			m_allDiskInfos.push_back(diskInfo);
		}
	}

	n = GetPrivateProfileIntW(L"DiskMonitor", L"Monitored", -1, ConfigFilePath.GetString());
	if (n < 0)
	{
		Utils::LogError(L"Load Monitored DiskInfo Error...");
		isError = true;
	}
	else
	{
		for (int i = 0; i < n; ++i)
		{
			DiskInfo diskInfo = DiskInfo::LoadDiskInfo(ConfigFilePath.GetString(), Utils::FormatCString(L"MONITORED_%d", i));
			if (diskInfo.Type == DiskInfo::DiskType::UNKNOWN)
			{
				isError = true;
				break;
			}
			m_monitoredDiskInfos.push_back(diskInfo);
		}
	}

	if (isError)
	{
		m_allDiskInfos.clear();
		m_monitoredDiskInfos.clear();

		Utils::GetAllDiskInfo(m_allDiskInfos);
		Utils::ClearPrivateProfile(ConfigFilePath.GetString());
		SaveConfig();
	}

	updateHandles();
}

void DiskMonitor::SaveConfig()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	Utils::ClearPrivateProfile(ConfigFilePath.GetString());
	WritePrivateProfileStringW(L"DiskMonitor", L"All", Utils::FormatCString(L"%d", m_allDiskInfos.size()), ConfigFilePath.GetString());
	WritePrivateProfileStringW(L"DiskMonitor", L"Monitored", Utils::FormatCString(L"%d", m_monitoredDiskInfos.size()), ConfigFilePath.GetString());
	WritePrivateProfileStringW(L"DiskMonitor", L"PrefixRead", m_speedRead.Prefix, ConfigFilePath.GetString());
	WritePrivateProfileStringW(L"DiskMonitor", L"PrefixWrite", m_speedWrite.Prefix, ConfigFilePath.GetString());

	int n = m_allDiskInfos.size();
	for (int i = 0; i < n; ++i)
	{
		DiskInfo::SaveDiskInfo(ConfigFilePath.GetString(), Utils::FormatCString(L"ALL_%d", i), m_allDiskInfos[i]);
	}

	n = m_monitoredDiskInfos.size();
	for (int i = 0; i < n; ++i)
	{
		DiskInfo::SaveDiskInfo(ConfigFilePath.GetString(), Utils::FormatCString(L"MONITORED_%d", i), m_monitoredDiskInfos[i]);
	}
}

void DiskMonitor::updateHandles()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	for (auto&& h : m_hDevices)
	{
		CloseHandle(h);
	}
	m_hDevices.clear();

	int n = m_monitoredDiskInfos.size();
	if (m_hDevices.size() < n)
	{
		m_hDevices.resize(n);
	}

	for (int i = 0; i < n; ++i)
	{
		CString path;
		if (m_monitoredDiskInfos[i].Type == DiskInfo::DiskType::LOGICAL)
		{
			path.Format(L"\\\\.\\%s", m_monitoredDiskInfos[i].Id);
		}
		else if (m_monitoredDiskInfos[i].Type != DiskInfo::DiskType::UNKNOWN)
		{
			path.Format(L"\\\\.\\PhysicalDrive%d", m_monitoredDiskInfos[i].DeviceId);
		}
		else
		{
			AfxMessageBox(Utils::FormatCString(Utils::LoadStringTable(IDS_CONFIG_DISKTYPE_UNKNOWN), m_monitoredDiskInfos[i].GetDescription()), MB_OK);
			exit(1);
		}

		HANDLE hDevice = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hDevice == INVALID_HANDLE_VALUE)
		{
			AfxMessageBox(Utils::FormatCString(Utils::LoadStringTable(IDS_GET_DISKSPEED_ERROR), Utils::GetErrorString(GetLastError())), MB_OK);
			exit(1);
		}

		m_hDevices[i] = hDevice;
	}
}

IPluginItem* DiskMonitor::GetItem(int index)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	switch (index)
	{
	case 0:
		return &m_speedRead;
		break;
	case 1:
		return &m_speedWrite;
		break;
	default:
		return nullptr;
		break;
	}
}

void DiskMonitor::DataRequired()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	std::unique_lock<std::mutex> lock(m_locker);

	int n = m_hDevices.size();
	uint64_t totalBytesRead = 0, totalBytesWritten = 0;
	DISK_PERFORMANCE diskPerformance;
	DWORD bytesReturned;
	auto currentTimePoint = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < n; ++i)
	{
		if (DeviceIoControl(m_hDevices[i], IOCTL_DISK_PERFORMANCE, NULL, 0, &diskPerformance, sizeof(diskPerformance), &bytesReturned, NULL))
		{
			totalBytesRead += diskPerformance.BytesRead.QuadPart;
			totalBytesWritten += diskPerformance.BytesWritten.QuadPart;
		}
		else
		{
			CString errorString = Utils::GetErrorString(GetLastError());
			AfxMessageBox(Utils::FormatCString(Utils::LoadStringTable(IDS_GET_DISKSPEED_ERROR), errorString), MB_OK);
			exit(1);
		}
	}

	if (m_lastBytesRead == 0 || m_lastBytesWritten == 0)
	{
		m_lastBytesRead = totalBytesRead;
		m_lastBytesWritten = totalBytesWritten;
		m_lastTimePoint = currentTimePoint;

		m_speedRead.Speed = 0.0;
		m_speedWrite.Speed = 0.0;
	}
	else
	{
		double timeInterval = (currentTimePoint - m_lastTimePoint).count() / 1e9;
		m_speedRead.Speed = (totalBytesRead - m_lastBytesRead) / timeInterval;
		m_speedWrite.Speed = (totalBytesWritten - m_lastBytesWritten) / timeInterval;

		m_lastBytesRead = totalBytesRead;
		m_lastBytesWritten = totalBytesWritten;
		m_lastTimePoint = currentTimePoint;
	}
}

const wchar_t* DiskMonitor::GetInfo(PluginInfoIndex index)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	UINT ids;
	static CString str;

	switch (index)
	{
	case ITMPlugin::TMI_NAME:
		ids = IDS_PLUGIN_NAME;
		break;
	case ITMPlugin::TMI_DESCRIPTION:
		ids = IDS_PLUGIN_DESCRIPTION;
		break;
	case ITMPlugin::TMI_AUTHOR:
		ids = IDS_PLUGIN_AUTHOR;
		break;
	case ITMPlugin::TMI_COPYRIGHT:
		ids = IDS_PLUGIN_COPYRIGHT;
		break;
	case ITMPlugin::TMI_VERSION:
		ids = IDS_PLUGIN_VERSION;
		break;
	case ITMPlugin::TMI_URL:
		ids = IDS_PLUGIN_URL;
		break;
	case ITMPlugin::TMI_MAX:
	default:
		return L"";
	}

	str.LoadStringW(ids);
	return str.GetString();
}

const wchar_t* DiskMonitor::GetTooltipInfo()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	static CString tooltip;

	tooltip = Utils::LoadStringTable(IDS_PLUGIN_MONITORING_DISKS);

	int n = m_monitoredDiskInfos.size();
	for (int i = 0; i < n; ++i)
	{
		tooltip = tooltip + m_monitoredDiskInfos[i].GetDescription() + L"; ";
	}

	tooltip += L"\r\n";
	tooltip += Utils::FormatCString(Utils::LoadStringTable(IDS_PLUGIN_TOTAL_DISKIO), Utils::FormatDataAmount(m_lastBytesRead), Utils::FormatDataAmount(m_lastBytesWritten));	

	return tooltip.GetString();
}

ITMPlugin::OptionReturn DiskMonitor::ShowOptionsDialog(void* parent)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	CConfigDialog dlg(CWnd::FromHandle(reinterpret_cast<HWND>(parent)));
	dlg.AllDiskInfos = m_allDiskInfos;
	dlg.MonitoredDiskInfos = m_monitoredDiskInfos;
	dlg.PrefixSpeedRead = m_speedRead.Prefix;
	dlg.PrefixSpeedWrite = m_speedWrite.Prefix;

	if (dlg.DoModal() == IDOK)
	{
		std::unique_lock<std::mutex> locker(m_locker);
		m_monitoredDiskInfos = dlg.MonitoredDiskInfos;
		m_allDiskInfos = dlg.AllDiskInfos;
		m_speedRead.Prefix = dlg.PrefixSpeedRead;
		m_speedWrite.Prefix = dlg.PrefixSpeedWrite;
		updateHandles();
		SaveConfig();

		m_lastBytesRead = 0;
		m_lastBytesWritten = 0;

		return ITMPlugin::OptionReturn::OR_OPTION_CHANGED;
	}
	else
	{
		return ITMPlugin::OptionReturn::OR_OPTION_UNCHANGED;
	}
}

void DiskMonitor::OnExtenedInfo(ITMPlugin::ExtendedInfoIndex index, const wchar_t* data)
{
	switch (index)
	{
	case ITMPlugin::EI_CONFIG_DIR:
		LoadConfig();
		break;
	default:
		break;
	}
}


DiskMonitor::DiskMonitor()
	: m_speedRead(L"磁盘读取速度", L"62AA2265-8245-4ABE-BF2D-A8954303D885"), m_speedWrite(L"磁盘写入速度", L"79A10969-73E8-40B7-B25A-8906A862DB29")
{
}

DiskMonitor::~DiskMonitor()
{
	for (auto&& h : m_hDevices)
	{
		CloseHandle(h);
	}
}

extern "C" __declspec(dllexport) ITMPlugin * TMPluginGetInstance()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	static DiskMonitor instance;

	CString cwd, actualDir;
	cwd = Utils::GetCurrentWorkingDirectory();
	actualDir = Utils::GetCurrentModuleFileDirectory();

	Utils::SetLogFilePath(Utils::FormatCString(DISKMONITOR_LOG_FILE_TEMPLATE, actualDir));
	instance.ConfigFilePath = Utils::FormatCString(DISKMONITOR_CONFIG_FILE_TEMPLATE, actualDir);

	Utils::LogInfo(L"Enter DLL Main, Get DiskMonitor Instance...");
	Utils::LogInfo(L"Current Working Directory: " + cwd);
	Utils::LogInfo(L"Current Module File Directory: " + actualDir);
	return &instance;
}