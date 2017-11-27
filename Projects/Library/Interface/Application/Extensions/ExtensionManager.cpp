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
		SDR::Extension::ExportTypes::SDR_Initialize Initialize;
		SDR::Extension::ExportTypes::SDR_CallHandlers CallHandlers;
		SDR::Extension::ExportTypes::SDR_Ready Ready;
		SDR::Extension::ExportTypes::SDR_StartMovie StartMovie;
		SDR::Extension::ExportTypes::SDR_ModifyFrame ModifyFrame;
	};

	std::vector<ExtensionData> Loaded;

	void Initialize(ExtensionData& ext)
	{
		ext.Query = (SDR::Extension::ExportTypes::SDR_Query)GetProcAddress(ext.Module, "SDR_Query");
		ext.Initialize = (SDR::Extension::ExportTypes::SDR_Initialize)GetProcAddress(ext.Module, "SDR_Initialize");
		ext.CallHandlers = (SDR::Extension::ExportTypes::SDR_CallHandlers)GetProcAddress(ext.Module, "SDR_CallHandlers");
		ext.Ready = (SDR::Extension::ExportTypes::SDR_Ready)GetProcAddress(ext.Module, "SDR_Ready");
		ext.ModifyFrame = (SDR::Extension::ExportTypes::SDR_ModifyFrame)GetProcAddress(ext.Module, "SDR_ModifyFrame");

		ext.Query(&ext.Info);

		SDR::Extension::InitializeData data = {};
		data.Message = SDR::Log::Message;
		data.MessageColor = SDR::Log::MessageColor;
		data.Warning = SDR::Log::Warning;

		ext.Initialize(&data);
	}

	void Load(const std::string& name)
	{
		ExtensionData ext = {};

		ext.Module = LoadLibraryA(name.c_str());

		if (!ext.Module)
		{
			SDR::Error::MS::ThrowLastError("Could not load extension \"%s\"", name.c_str());
		}

		Initialize(ext);

		Loaded.emplace_back(std::move(ext));
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
	for (const auto& ext : Loaded)
	{
		ext.Ready();
	}
}

void SDR::ExtensionManager::Events::StartMovie(ID3D11Device* device, int width, int height)
{
	for (const auto& ext : Loaded)
	{
		ext.StartMovie(device, width, height);
	}
}

void SDR::ExtensionManager::Events::ModifyFrame(ID3D11DeviceContext* context)
{
	for (const auto& ext : Loaded)
	{
		ext.ModifyFrame(context);
	}
}
