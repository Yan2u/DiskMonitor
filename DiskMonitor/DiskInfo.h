#pragma once
#define DISKINFO_BUFFER_SIZE 1024

class DiskInfo
{
public:
	enum class DiskType
	{
		HDD,
		SSD,
		SCM,
		LOGICAL,
		UNKNOWN
	};

public:
	CString Id;
	CString Caption;
	DiskType Type;
	uint64_t Size;

	DWORD DeviceId; // For logical disks only

	void Print(); // For debugging purposes
	CString GetDescription();
	
	DiskInfo();

	static DiskInfo LoadDiskInfo(const wchar_t* filename, const wchar_t* section);
	static void SaveDiskInfo(const wchar_t* filename, const wchar_t* section, const DiskInfo& diskInfo);
	static CString GetDiskTypeString(DiskType diskType);

	static bool IsPhysicalDisk(const DiskInfo& diskInfo);
	static bool IsLogicalDisk(const DiskInfo& diskInfo);
};

