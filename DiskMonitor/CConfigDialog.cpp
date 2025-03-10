// CConfigDialog.cpp: 实现文件
//

#include "pch.h"
#include "afxdialogex.h"
#include "CConfigDialog.h"
#include "resource.h"
#include "Utils.h"

#include <iostream>
#include <unordered_set>
#include <winioctl.h>

// CConfigDialog 对话框

IMPLEMENT_DYNAMIC(CConfigDialog, CDialog)

void CConfigDialog::UpdateConfig()
{
	std::unique_lock<std::mutex> lock(m_locker);

	for (auto&& h : m_hDevices)
	{
		CloseHandle(h);
	}
	m_hDevices.clear();

	int n = MonitoredDiskInfos.size();
	if (n > m_hDevices.size())
	{
		m_hDevices.resize(n);
	}

	for (int i = 0; i < n; ++i)
	{
		CString path;
		if (MonitoredDiskInfos[i].Type == DiskInfo::DiskType::LOGICAL)
		{
			path.Format(L"\\\\.\\%s", MonitoredDiskInfos[i].Id);
		}
		else
		{
			DISKMONITOR_ASSERT(
				MonitoredDiskInfos[i].Type != DiskInfo::DiskType::UNKNOWN,
				IDS_CONFIG_DISKTYPE_UNKNOWN,
				MonitoredDiskInfos[i].GetDescription()
			);

			// SSD, HDD, SCM
			path.Format(L"\\\\.\\PhysicalDrive%d", MonitoredDiskInfos[i].DeviceId);
		}

		HANDLE hDevice = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		DISKMONITOR_ASSERT(
			hDevice != INVALID_HANDLE_VALUE,
			IDS_GET_DISKSPEED_ERROR,
			Utils::GetErrorString(GetLastError())
		);
		
		m_hDevices[i] = hDevice;
	}

	m_lastBytesRead = 0;
	m_lastBytesWritten = 0;
}

void CConfigDialog::UpdateView()
{
	updateListCtrl(m_listAllDiskInfo, AllDiskInfos);
	updateListCtrl(m_listMonitoredDiskInfo, MonitoredDiskInfos);
}

CConfigDialog::CConfigDialog(CWnd* pParent /*=nullptr*/)
	: CDialog(IDD_CONFIG_DIALOG, pParent)
	, m_editPrefixRead(_T(""))
	, m_editPrefixWrite(_T(""))
{
}

CConfigDialog::~CConfigDialog()
{
}

void CConfigDialog::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_ALLDISKINFO, m_listAllDiskInfo);
	DDX_Control(pDX, IDC_LIST_MONITOREDDISKINFO, m_listMonitoredDiskInfo);
	DDX_Control(pDX, IDC_TEXT_SPEED_READ, m_textSpeedRead);
	DDX_Control(pDX, IDC_TEXT_SPEED_WRITE, m_textSpeedWrite);
	DDX_Text(pDX, IDC_EDIT_EDITPREFIX_READ, m_editPrefixRead);
	DDX_Text(pDX, IDC_EDIT_EDITPREFIX_WRITE, m_editPrefixWrite);
}

BOOL CConfigDialog::OnInitDialog()
{
	CDialog::OnInitDialog();

	initListCtrl(m_listAllDiskInfo);
	initListCtrl(m_listMonitoredDiskInfo);

	UpdateView();
	UpdateConfig();

	UpdateData(TRUE);
	m_editPrefixRead = PrefixSpeedRead;
	m_editPrefixWrite = PrefixSpeedWrite;
	UpdateData(FALSE);

	SetTimer(IDT_CONFIG_GETDISKSPEED, IDT_CONFIG_GETDISKSPEED_INTERVAL, 0);
	return TRUE;
}

void CConfigDialog::OnDestroy()
{
	KillTimer(IDT_CONFIG_GETDISKSPEED);
	for (auto&& h : m_hDevices)
	{
		CloseHandle(h);
	}

	CDialog::OnDestroy();
}

void CConfigDialog::initListCtrl(CListCtrl& listCtrl)
{
	UpdateData(TRUE);

	listCtrl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | TVS_EX_MULTISELECT);
	
	RECT rect;
	listCtrl.GetClientRect(&rect);
	
	// ratio: 0.1, 0.1, 0.45, 0.15, 0.2
	listCtrl.DeleteAllItems();
	listCtrl.InsertColumn(0, Utils::LoadStringTable(IDS_LISTCOLUMN_ID), LVCFMT_LEFT | LVCFMT_FIXED_WIDTH, rect.right * 0.1);
	listCtrl.InsertColumn(1, Utils::LoadStringTable(IDS_LISTCOLUMN_DISKTYPE), LVCFMT_LEFT | LVCFMT_FIXED_WIDTH, rect.right * 0.1);
	listCtrl.InsertColumn(2, Utils::LoadStringTable(IDS_LISTCOLUMN_CAPTION), LVCFMT_LEFT | LVCFMT_FIXED_WIDTH, rect.right * 0.45);
	listCtrl.InsertColumn(3, Utils::LoadStringTable(IDS_LISTCOLUMN_SIZE), LVCFMT_LEFT | LVCFMT_FIXED_WIDTH, rect.right * 0.15);
	listCtrl.InsertColumn(4, Utils::LoadStringTable(IDS_LISTCOLUMN_DEVICEID), LVCFMT_LEFT | LVCFMT_FIXED_WIDTH, rect.right * 0.2);

	UpdateData(FALSE);
}

void CConfigDialog::updateListCtrl(CListCtrl& listCtrl, std::vector<DiskInfo>& diskInfos)
{
	UpdateData(TRUE);

	listCtrl.DeleteAllItems();

	for (int i = 0; i < diskInfos.size(); ++i)
	{
		DiskInfo& diskInfo = diskInfos[i];
		listCtrl.InsertItem(i, diskInfo.Id);

		listCtrl.SetItemText(i, 1, DiskInfo::GetDiskTypeString(diskInfo.Type));
		listCtrl.SetItemText(i, 2, diskInfo.Caption);
		listCtrl.SetItemText(i, 3, Utils::FormatDataAmount((double)diskInfo.Size));
		listCtrl.SetItemText(i, 4, Utils::FormatCString(L"%d", diskInfo.DeviceId));
	}

	UpdateData(FALSE);
}

void CConfigDialog::updatePrefixString()
{
	if (m_editPrefixRead.IsEmpty())
	{
		PrefixSpeedRead = Utils::LoadStringTable(IDS_CONFIG_EXAMPLE_READ);
	}
	else
	{
		PrefixSpeedRead = m_editPrefixRead;
	}

	if (m_editPrefixWrite.IsEmpty())
	{
		PrefixSpeedWrite = Utils::LoadStringTable(IDS_CONFIG_EXAMPLE_WRITE);
	}
	else
	{
		PrefixSpeedWrite = m_editPrefixWrite;
	}
}

void CConfigDialog::onAllDiskAddToMonitor()
{
	POSITION pos = m_listAllDiskInfo.GetFirstSelectedItemPosition();
	std::vector<DiskInfo> tempDiskInfos = MonitoredDiskInfos;

	while (pos)
	{
		int nItem = m_listAllDiskInfo.GetNextSelectedItem(pos);
		DiskInfo diskInfo = AllDiskInfos[nItem];
		tempDiskInfos.push_back(diskInfo);
	}

	int n = tempDiskInfos.size();
	for (int i = 0; i < n; ++i)
	{
		// Check 1. Type cannot be UNKNOWN.
		DISKMONITOR_REQUIRE(
			tempDiskInfos[i].Type != DiskInfo::DiskType::UNKNOWN,
			IDS_CONFIG_DISKTYPE_UNKNOWN,
			tempDiskInfos[i].GetDescription()
		);

		for (int j = i + 1; j < n; ++j)
		{
			// Check 2. There cannot be two or more disks with the same Id.
			DISKMONITOR_REQUIRE(
				tempDiskInfos[i].GetDescription() != tempDiskInfos[j].GetDescription(),
				IDS_CONFIG_DISKID_CONFLICT,
				tempDiskInfos[i].GetDescription()
			);

			// Check 3. There cannot be two or more disks which are of different types (LOGICAL, PHYSICAL), but have the same DeviceId.

			if (DiskInfo::IsLogicalDisk(tempDiskInfos[i]) && DiskInfo::IsPhysicalDisk(tempDiskInfos[j]) ||
				DiskInfo::IsPhysicalDisk(tempDiskInfos[i]) && DiskInfo::IsLogicalDisk(tempDiskInfos[j]))
			{
				DISKMONITOR_REQUIRE(
					tempDiskInfos[i].DeviceId != tempDiskInfos[j].DeviceId,
					IDS_CONFIG_DISKDEVICE_CONFLICT,
					tempDiskInfos[i].GetDescription(),
					tempDiskInfos[j].GetDescription()
				);
			}
		}
	}

	{
		std::unique_lock<std::mutex> lock(m_locker);
		MonitoredDiskInfos = tempDiskInfos;
	}
	UpdateConfig();
	UpdateView();
}

void CConfigDialog::onAllDiskRefresh()
{
	std::vector<DiskInfo> infos;
	Utils::GetAllDiskInfo(infos);

	AllDiskInfos = infos;
	UpdateView();
	AfxMessageBox(Utils::LoadStringTable(IDS_CONFIG_ALLDISK_REFRESHED), MB_OK);
}

void CConfigDialog::onMonitoredDiskDelete()
{
	POSITION pos = m_listMonitoredDiskInfo.GetFirstSelectedItemPosition();
	std::unordered_set<int> selectedItems;

	while (pos)
	{
		int nItem = m_listMonitoredDiskInfo.GetNextSelectedItem(pos);
		selectedItems.insert(nItem);
	}

	std::vector<DiskInfo> tempDiskInfos;
	for (int i = 0; i < MonitoredDiskInfos.size(); ++i)
	{
		if (selectedItems.find(i) == selectedItems.end())
		{
			tempDiskInfos.push_back(MonitoredDiskInfos[i]);
		}
	}

	{
		std::unique_lock<std::mutex> lock(m_locker);
		MonitoredDiskInfos = tempDiskInfos;
	}
	UpdateConfig();
	UpdateView();
}

void CConfigDialog::onMonitoredDiskClear()
{
	if (AfxMessageBox(Utils::LoadStringTable(IDS_CONFIG_CONFIRM_CLEAR_MONITORED), MB_OKCANCEL) == IDOK)
	{
		{
			std::unique_lock<std::mutex> lock(m_locker);
			MonitoredDiskInfos.clear();
		}
		UpdateConfig();
		UpdateView();
	}
}


BEGIN_MESSAGE_MAP(CConfigDialog, CDialog)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_ALLDISKINFO, &CConfigDialog::OnNMRClickListAlldiskinfo)
	ON_NOTIFY(NM_RCLICK, IDC_LIST_MONITOREDDISKINFO, &CConfigDialog::OnNMRClickListMonitoreddiskinfo)
	ON_COMMAND(IDR_ALLDISK_ADDTOMONITOR, &CConfigDialog::OnAllDiskAddToMonitor)
	ON_COMMAND(IDR_ALLDISK_REFRESH, &CConfigDialog::OnAllDiskRefresh)
	ON_COMMAND(IDR_MONITOREDDISK_DELETE, &CConfigDialog::OnMonitoredDiskDelete)
	ON_COMMAND(IDR_MONITOREDDISK_CLEAR, &CConfigDialog::OnMonitoredDiskClear)
	ON_BN_CLICKED(IDC_BTN_ALLDISK_REFRESH, &CConfigDialog::OnBnClickedBtnAlldiskRefresh)
	ON_BN_CLICKED(IDC_BTN_ALLDISK_ADDTOMONITORED, &CConfigDialog::OnBnClickedBtnAlldiskAddtomonitored)
	ON_BN_CLICKED(IDC_BTN_MONITOREDDISK_DELETE, &CConfigDialog::OnBnClickedBtnMonitoreddiskDelete)
	ON_BN_CLICKED(IDC_BTN_MONITOREDDISK_CLEAR, &CConfigDialog::OnBnClickedBtnMonitoreddiskClear)
	ON_WM_TIMER()
	ON_WM_DESTROY()
END_MESSAGE_MAP()


// CConfigDialog 消息处理程序


void CConfigDialog::OnNMRClickListAlldiskinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	// Show context menu, menu 0
	CMenu menu;
	menu.LoadMenuW(IDR_MENU_DISKINFO);
	CMenu* pMenu = menu.GetSubMenu(0);
	CPoint point;
	GetCursorPos(&point);
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}


void CConfigDialog::OnNMRClickListMonitoreddiskinfo(NMHDR* pNMHDR, LRESULT* pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	*pResult = 0;

	// Show context menu, menu 1
	CMenu menu;
	menu.LoadMenuW(IDR_MENU_DISKINFO);
	CMenu* pMenu = menu.GetSubMenu(1);
	CPoint point;
	GetCursorPos(&point);
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this);
}


void CConfigDialog::OnAllDiskAddToMonitor()
{
	onAllDiskAddToMonitor();
}


void CConfigDialog::OnAllDiskRefresh()
{
	onAllDiskRefresh();
}


void CConfigDialog::OnMonitoredDiskDelete()
{
	onMonitoredDiskDelete();
}


void CConfigDialog::OnMonitoredDiskClear()
{
	onMonitoredDiskClear();
}


void CConfigDialog::OnBnClickedBtnAlldiskRefresh()
{
	onAllDiskRefresh();
}


void CConfigDialog::OnBnClickedBtnAlldiskAddtomonitored()
{
	onAllDiskAddToMonitor();
}


void CConfigDialog::OnBnClickedBtnMonitoreddiskDelete()
{
	onMonitoredDiskDelete();
}


void CConfigDialog::OnBnClickedBtnMonitoreddiskClear()
{
	onMonitoredDiskClear();
}

void CConfigDialog::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == IDT_CONFIG_GETDISKSPEED)
	{
		std::unique_lock<std::mutex> lock(m_locker);

		int n = m_hDevices.size();
		uint64_t totalBytesRead = 0, totalBytesWritten = 0;
		DISK_PERFORMANCE diskPerformance;
		DWORD bytesReturned;
		auto currentTimePoint = std::chrono::high_resolution_clock::now();
		for (int i = 0; i < n; ++i)
		{
			DISKMONITOR_ASSERT(
				DeviceIoControl(m_hDevices[i], IOCTL_DISK_PERFORMANCE, NULL, 0, &diskPerformance, sizeof(diskPerformance), &bytesReturned, NULL),
				IDS_GET_DISKSPEED_ERROR,
				Utils::GetErrorString(GetLastError())
			);

			totalBytesRead += diskPerformance.BytesRead.QuadPart;
			totalBytesWritten += diskPerformance.BytesWritten.QuadPart;
		}

		if (m_lastBytesRead == 0 || m_lastBytesWritten == 0)
		{
			m_lastBytesRead = totalBytesRead;
			m_lastBytesWritten = totalBytesWritten;
			m_lastTimePoint = currentTimePoint;

			m_currentSpeedRead = 0.0;
			m_currentSpeedWrite = 0.0;
		}
		else
		{
			double timeInterval = (currentTimePoint - m_lastTimePoint).count() / 1e9;
			m_currentSpeedRead = (totalBytesRead - m_lastBytesRead) / timeInterval;
			m_currentSpeedWrite = (totalBytesWritten - m_lastBytesWritten) / timeInterval;

			m_lastBytesRead = totalBytesRead;
			m_lastBytesWritten = totalBytesWritten;
			m_lastTimePoint = currentTimePoint;
		}

		UpdateData(TRUE);
		m_textSpeedRead.SetWindowTextW(Utils::FormatCString(L"%s %s", PrefixSpeedRead, Utils::FormatSpeed(m_currentSpeedRead)));
		m_textSpeedWrite.SetWindowTextW(Utils::FormatCString(L"%s %s", PrefixSpeedWrite, Utils::FormatSpeed(m_currentSpeedWrite)));
		UpdateData(FALSE);

		updatePrefixString();
	}
}
