#pragma once

#include "PluginInterface.h"
#include "CConfigDialog.h"
#include "Utils.h"
#include "PluginDiskSpeed.h"

#include <chrono>
#include <vector>

#define DISKMONITOR_CONFIG_FILE L"./DiskMonitor.dll.ini"
#define DISKMONITOR_CONFIG_FILE_TEMPLATE L"%s\\DiskMonitor.dll.ini"

class DiskMonitor : public ITMPlugin
{
protected:
	PluginDiskSpeed m_speedRead, m_speedWrite;
	uint64_t m_lastBytesRead = 0, m_lastBytesWritten = 0;
	std::chrono::high_resolution_clock::time_point m_lastTimePoint;
	std::mutex m_locker;
	std::vector<DiskInfo> m_allDiskInfos, m_monitoredDiskInfos;
	std::vector<HANDLE> m_hDevices;

	void updateHandles();

public:
	CString ConfigFilePath;

	// Í¨¹ý ITMPlugin ¼Ì³Ð
	IPluginItem* GetItem(int index) override;
	virtual void DataRequired() override;
	const wchar_t* GetInfo(PluginInfoIndex index) override;
	const wchar_t* GetTooltipInfo() override;
	virtual ITMPlugin::OptionReturn ShowOptionsDialog(void* parent) override;
	virtual void OnExtenedInfo(ITMPlugin::ExtendedInfoIndex index, const wchar_t* data) override;

	void LoadConfig();
	void SaveConfig();

	DiskMonitor();
	~DiskMonitor();
};