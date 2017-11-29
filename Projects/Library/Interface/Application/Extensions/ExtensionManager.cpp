#include "PrecompiledHeader.hpp"
#include "ExtensionManager.hpp"
#include "Interface\LibraryInterface.hpp"
#include "Interface\Application\Modules\Movie Record\MovieRecord.hpp"

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
		SDR::Extension::ExportTypes::SDR_EndMovie EndMovie;
		SDR::Extension::ExportTypes::SDR_ModifyFrame ModifyFrame;
	};

	std::vector<ExtensionData> Loaded;
	std::vector<SDR::Console::Variable> Variables;

	void Initialize(ExtensionData& ext)
	{
		ext.Query = (SDR::Extension::ExportTypes::SDR_Query)GetProcAddress(ext.Module, "SDR_Query");
		ext.Initialize = (SDR::Extension::ExportTypes::SDR_Initialize)GetProcAddress(ext.Module, "SDR_Initialize");
		ext.CallHandlers = (SDR::Extension::ExportTypes::SDR_CallHandlers)GetProcAddress(ext.Module, "SDR_CallHandlers");
		ext.Ready = (SDR::Extension::ExportTypes::SDR_Ready)GetProcAddress(ext.Module, "SDR_Ready");
		ext.StartMovie = (SDR::Extension::ExportTypes::SDR_StartMovie)GetProcAddress(ext.Module, "SDR_StartMovie");
		ext.EndMovie = (SDR::Extension::ExportTypes::SDR_EndMovie)GetProcAddress(ext.Module, "SDR_EndMovie");
		ext.ModifyFrame = (SDR::Extension::ExportTypes::SDR_ModifyFrame)GetProcAddress(ext.Module, "SDR_ModifyFrame");

		ext.Query(&ext.Info);

		SDR::Extension::InitializeData data = {};
		data.Message = SDR::Log::Message;
		data.MessageColor = SDR::Log::MessageColor;
		data.Warning = SDR::Log::Warning;

		ext.Initialize(&data);
	}

	void Load(const std::experimental::filesystem::path& path)
	{
		ExtensionData ext = {};

		auto filename = path.filename();
		auto fullutf8 = path.u8string();
		auto nameutf8 = filename.u8string();

		ext.Module = LoadLibraryA(fullutf8.c_str());

		if (!ext.Module)
		{
			SDR::Error::MS::ThrowLastError("Could not load extension \"%s\"", nameutf8.c_str());
		}

		Initialize(ext);

		SDR::Log::Message("SDR: Loaded extension \"%s\"\n", nameutf8.c_str());

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
			Load(obj);
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
	SDR::Extension::ImportData data = {};

	data.MakeBool = [](const char* name, const char* value)
	{
		auto var = SDR::Console::MakeBool(name, value);
		Variables.emplace_back(std::move(var));

		return Variables.size() - 1;
	};

	data.MakeNumber = [](const char* name, const char* value)
	{
		auto var = SDR::Console::MakeNumber(name, value);
		Variables.emplace_back(std::move(var));

		return Variables.size() - 1;
	};

	data.MakeNumberMin = [](const char* name, const char* value, float min)
	{
		auto var = SDR::Console::MakeNumber(name, value, min);
		Variables.emplace_back(std::move(var));

		return Variables.size() - 1;
	};

	data.MakeNumberMinMax = [](const char* name, const char* value, float min, float max)
	{
		auto var = SDR::Console::MakeNumber(name, value, min, max);
		Variables.emplace_back(std::move(var));

		return Variables.size() - 1;
	};

	data.MakeNumberMinMaxString = [](const char* name, const char* value, float min, float max)
	{
		auto var = SDR::Console::MakeNumberWithString(name, value, min, max);
		Variables.emplace_back(std::move(var));

		return Variables.size() - 1;
	};

	data.MakeString = [](const char* name, const char* value)
	{
		auto var = SDR::Console::MakeString(name, value);
		Variables.emplace_back(std::move(var));

		return Variables.size() - 1;
	};

	data.GetBool = [](uint32_t key)
	{
		auto& var = Variables[key];
		return var.GetBool();
	};

	data.GetInt = [](uint32_t key)
	{
		auto& var = Variables[key];
		return var.GetInt();
	};

	data.GetFloat = [](uint32_t key)
	{
		auto& var = Variables[key];
		return var.GetFloat();
	};

	data.GetString = [](uint32_t key)
	{
		auto& var = Variables[key];
		return var.GetString();
	};

	data.GetTimeNow = []()
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto start = std::chrono::duration<double>(now.time_since_epoch());

		return start.count();
	};

	data.IsRecordingVideo = SDR::MovieRecord::ShouldRecord;

	for (auto& ext : Loaded)
	{
		ext.Ready(data);
	}
}

void SDR::ExtensionManager::Events::StartMovie(const SDR::Extension::StartMovieData& data)
{
	for (const auto& ext : Loaded)
	{
		ext.StartMovie(data);
	}
}

void SDR::ExtensionManager::Events::EndMovie()
{
	for (const auto& ext : Loaded)
	{
		ext.EndMovie();
	}
}

void SDR::ExtensionManager::Events::ModifyFrame(SDR::Extension::ModifyFrameData& data)
{
	for (const auto& ext : Loaded)
	{
		ext.ModifyFrame(data);
	}
}
