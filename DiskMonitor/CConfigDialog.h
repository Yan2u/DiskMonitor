#pragma once
#include "afxdialogex.h"
#include "DiskInfo.h"

#include <vector>
#include <chrono>
#include <mutex>

// CConfigDialog 对话框

class CConfigDialog : public CDialog
{
	DECLARE_DYNAMIC(CConfigDialog)

public:
	std::vector<DiskInfo> AllDiskInfos;
	std::vector<DiskInfo> MonitoredDiskInfos;

	CString PrefixSpeedRead, PrefixSpeedWrite;

	void UpdateConfig();
	void UpdateView();

	CConfigDialog(CWnd* pParent = nullptr);   // 标准构造函数
	virtual ~CConfigDialog();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CONFIG_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持
	virtual BOOL OnInitDialog() override;
	void OnDestroy();

	void initListCtrl(CListCtrl& listCtrl);
	void updateListCtrl(CListCtrl& listCtrl, std::vector<DiskInfo>& diskInfos);
	void updatePrefixString();

	void onAllDiskAddToMonitor();
	void onAllDiskRefresh();
	void onMonitoredDiskDelete();
	void onMonitoredDiskClear();

	DECLARE_MESSAGE_MAP()

	CListCtrl m_listAllDiskInfo;
	CListCtrl m_listMonitoredDiskInfo;
	CStatic m_textSpeedRead;
	CStatic m_textSpeedWrite;
	CString m_editPrefixRead;
	CString m_editPrefixWrite;

	std::vector<HANDLE> m_hDevices;
	std::mutex m_locker;
	int64_t m_lastBytesRead = 0;
	int64_t m_lastBytesWritten = 0;
	double m_currentSpeedRead = 0.0;
	double m_currentSpeedWrite = 0.0;
	std::chrono::high_resolution_clock::time_point m_lastTimePoint;

public: /* events */
	afx_msg void OnNMRClickListAlldiskinfo(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnNMRClickListMonitoreddiskinfo(NMHDR* pNMHDR, LRESULT* pResult);

	afx_msg void OnAllDiskAddToMonitor();
	afx_msg void OnAllDiskRefresh();
	afx_msg void OnMonitoredDiskDelete();
	afx_msg void OnMonitoredDiskClear();
	afx_msg void OnBnClickedBtnAlldiskRefresh();
	afx_msg void OnBnClickedBtnAlldiskAddtomonitored();
	afx_msg void OnBnClickedBtnMonitoreddiskDelete();
	afx_msg void OnBnClickedBtnMonitoreddiskClear();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
};
