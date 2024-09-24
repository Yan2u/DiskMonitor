#pragma once

#include "PluginInterface.h"

class PluginDiskSpeed : public IPluginItem
{
protected:
	CString m_id;
	CString m_name;

public:
	// Í¨¹ý IPluginItem ¼Ì³Ð
	const wchar_t* GetItemName() const override;
	const wchar_t* GetItemId() const override;
	const wchar_t* GetItemLableText() const override;
	const wchar_t* GetItemValueText() const override;
	const wchar_t* GetItemValueSampleText() const override;

	CString Prefix;
	double Speed;
	PluginDiskSpeed(const wchar_t* name, const wchar_t* id);
};

