#include "pch.h"
#include "PluginDiskSpeed.h"
#include "Utils.h"

const wchar_t* PluginDiskSpeed::GetItemName() const
{
    return m_name.GetString();
}

const wchar_t* PluginDiskSpeed::GetItemId() const
{
    return m_id.GetString();
}

const wchar_t* PluginDiskSpeed::GetItemLableText() const
{
	return Prefix.GetString();
}

const wchar_t* PluginDiskSpeed::GetItemValueText() const
{
	static CString str;
	str = Utils::FormatSpeed(Speed);

	return str.GetString();
}

const wchar_t* PluginDiskSpeed::GetItemValueSampleText() const
{
	static CString str;
	str = Prefix + L" 0.00 B/s";
	return str.GetString();
}

PluginDiskSpeed::PluginDiskSpeed(const wchar_t* name, const wchar_t* id)
	: m_name(name), m_id(id), Prefix(L""), Speed(0.0)
{
}
