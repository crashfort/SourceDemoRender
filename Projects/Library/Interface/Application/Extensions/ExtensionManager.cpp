#include "PrecompiledHeader.hpp"
#include "ExtensionManager.hpp"
#include "Interface\LibraryInterface.hpp"

namespace
{
	struct ExtensionData
	{
		ExtensionData() = default;

		ExtensionData(ExtensionData&& other)
		{
			*this = std::move(other);
			other.Module = nullptr;
		}

		ExtensionData& operator=(const ExtensionData&) = delete;
		ExtensionData& operator=(ExtensionData&&) = default;

		~ExtensionData()
		{
			if (Module)
			{
				FreeLibrary(Module);
			}
		}

		HMODULE Module = nullptr;

		SDR::Extension::QueryData Info;

		SDR::Extension::ExportTypes::SDR_Query Query;
		SDR::Extension::ExportTypes::SDR_CallHandlers CallHandlers;
		SDR::Extension::ExportTypes::SDR_Ready Ready;
		SDR::Extension::ExportTypes::SDR_ModifyFrame ModifyFrame;
	};

	std::vector<ExtensionData> Loaded;

	void Load(const std::string& name)
	{
		ExtensionData data = {};

		data.Module = LoadLibraryA(name.c_str());

		if (!data.Module)
		{
			SDR::Error::MS::ThrowLastError("Could not load extension \"%s\"", name.c_str());
		}

		data.Query = (SDR::Extension::ExportTypes::SDR_Query)GetProcAddress(data.Module, "SDR_Query");
		data.CallHandlers = (SDR::Extension::ExportTypes::SDR_CallHandlers)GetProcAddress(data.Module, "SDR_CallHandlers");
		data.Ready = (SDR::Extension::ExportTypes::SDR_Ready)GetProcAddress(data.Module, "SDR_Ready");
		data.ModifyFrame = (SDR::Extension::ExportTypes::SDR_ModifyFrame)GetProcAddress(data.Module, "SDR_ModifyFrame");

		data.Query(&data.Info);

		Loaded.emplace_back(std::move(data));
	}
}

void SDR::ExtensionManager::LoadExtensions()
{
	auto path = SDR::Library::BuildResourcePath("Extensions\\Enabled\\");

	for (const auto& it : std::experimental::filesystem::directory_iterator(path))
	{
		const auto& obj = it.path();

		if (obj.extension() == ".dll")
		{
			Load(obj.u8string());
		}
	}
}

bool SDR::ExtensionManager::HasExtensions()
{
	return Loaded.empty() == false;
}

bool SDR::ExtensionManager::Events::CallHandlers(const char* name, const rapidjson::Value& value)
{
	for (const auto& ext : Loaded)
	{
		auto res = ext.CallHandlers(name, value);

		if (res)
		{
			return true;
		}
	}

	return false;
}

void SDR::ExtensionManager::Events::Ready()
{
	SDR::Extension::ReadyData data = {};
	data.Message = SDR::Log::Message;
	data.MessageColor = SDR::Log::MessageColor;
	data.Warning = SDR::Log::Warning;
	data.MakeError = SDR::Error::Make;

	for (const auto& ext : Loaded)
	{
		ext.Ready(&data);
	}
}

void SDR::ExtensionManager::Events::ModifyFrame(ID3D11DeviceContext* context)
{
	for (const auto& ext : Loaded)
	{
		ext.ModifyFrame(context);
	}
}
