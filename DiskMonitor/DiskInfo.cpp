#include "pch.h"
#include "DiskInfo.h"
#include "resource.h"
#include "Utils.h"

#include <iostream>

void DiskInfo::Print()
{
	CString str;
	str.Format(L"DiskInfo(Id=%s, Caption=%s, Type=%s, Size=%s, DeviceId=%d)",
		Id, Caption, GetDiskTypeString(Type), Utils::FormatDataAmount(Size), DeviceId);

	std::wcout.imbue(std::locale("chs"));
	std::wcout << str.GetString() << std::endl;
}

CString DiskInfo::GetDescription()
{
	return Utils::FormatCString(L"%s (%s)", Caption, Id);
}

DiskInfo::DiskInfo() :
	DeviceId(0), Size(0), Type(DiskType::UNKNOWN), Id(L""), Caption(L"")
{
}

DiskInfo DiskInfo::LoadDiskInfo(const wchar_t* filename, const wchar_t* section)
{
	static auto GetConfig = [](const wchar_t* section, const wchar_t* key, const wchar_t* defaultValue, CString& str, const wchar_t* filename) -> bool
		{
			GetPrivateProfileStringW(section, key, defaultValue, str.GetBuffer(DISKINFO_BUFFER_SIZE), DISKINFO_BUFFER_SIZE, filename);
			str.ReleaseBuffer();

			if (GetLastError() == 0)
			{
				return true;
			}
			else
			{
				return false;
			}
		};

	DiskInfo diskInfo;
	CString str;

	if (!GetConfig(section, L"Id", L"", str, filename)) return diskInfo;
	diskInfo.Id = str;

	if (!GetConfig(section, L"Type", L"", str, filename)) return diskInfo;
	diskInfo.Type = static_cast<DiskInfo::DiskType>(_wtoi(str));

	if (!GetConfig(section, L"Size", L"", str, filename)) return diskInfo;
	diskInfo.Size = _wtoi64(str);

	if (!GetConfig(section, L"DeviceId", L"", str, filename)) return diskInfo;
	diskInfo.DeviceId = _wtoi(str);

	GetConfig(section, L"Caption", L"", str, filename);
	if (str.IsEmpty() && diskInfo.Type == DiskType::LOGICAL)
	{
		diskInfo.Caption = Utils::FormatCString(Utils::LoadStringTable(IDS_DEFAULT_LOGICAL_DISK_CAPTION), diskInfo.Id);
	}
	else
	{
		diskInfo.Caption = str;
	}

	return diskInfo;
}

void DiskInfo::SaveDiskInfo(const wchar_t* filename, const wchar_t* section, const DiskInfo& diskInfo)
{
	WritePrivateProfileStringW(section, L"Id", diskInfo.Id, filename);
	WritePrivateProfileStringW(section, L"Type", Utils::FormatCString(L"%d", static_cast<int>(diskInfo.Type)), filename);
	WritePrivateProfileStringW(section, L"Size", Utils::FormatCString(L"%I64d", diskInfo.Size), filename);
	WritePrivateProfileStringW(section, L"DeviceId", Utils::FormatCString(L"%d", diskInfo.DeviceId), filename);
	WritePrivateProfileStringW(section, L"Caption", diskInfo.Caption, filename);
}

CString DiskInfo::GetDiskTypeString(DiskType diskType)
{
	UINT ids;

	switch (diskType)
	{
	case DiskType::HDD:
		ids = IDS_DISKTYPE_HDD;
		break;
	case DiskType::SSD:
		ids = IDS_DISKTYPE_SSD;
		break;
	case DiskType::SCM:
		ids = IDS_DISKTYPE_SCM;
		break;
	case DiskType::LOGICAL:
		ids = IDS_DISKTYPE_LOGICAL;
		break;
	case DiskType::UNKNOWN:
	default:
		ids = IDS_DISKTYPE_UNKNOWN;
		break;
	}

	return Utils::LoadStringTable(ids);
}

bool DiskInfo::IsPhysicalDisk(const DiskInfo& diskInfo)
{
	return diskInfo.Type == DiskType::HDD || diskInfo.Type == DiskType::SSD || diskInfo.Type == DiskType::SCM;
}

bool DiskInfo::IsLogicalDisk(const DiskInfo& diskInfo)
{
	return diskInfo.Type == DiskType::LOGICAL;
}