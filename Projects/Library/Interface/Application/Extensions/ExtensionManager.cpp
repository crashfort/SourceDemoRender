#include "PrecompiledHeader.hpp"
#include "ExtensionManager.hpp"
#include "Interface\LibraryInterface.hpp"
#include "Interface\Application\Modules\Movie Record\MovieRecord.hpp"
#include "Interface\Application\Modules\Shared\SourceGlobals.hpp"

#include "SDR Shared\Json.hpp"

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

		std::string Name;
		HMODULE Module = nullptr;

		SDR::Extension::QueryData Info;

		SDR::Extension::ExportTypes::SDR_Query Query;
		SDR::Extension::ExportTypes::SDR_Initialize Initialize;
		SDR::Extension::ExportTypes::SDR_ConfigHandler ConfigHandler;
		SDR::Extension::ExportTypes::SDR_Ready Ready;
		SDR::Extension::ExportTypes::SDR_StartMovie StartMovie;
		SDR::Extension::ExportTypes::SDR_EndMovie EndMovie;
		SDR::Extension::ExportTypes::SDR_NewVideoFrame NewVideoFrame;
	};

	std::vector<ExtensionData> Loaded;
	std::vector<SDR::Console::Variable> Variables;

	void Initialize(ExtensionData& ext)
	{
		auto findexport = [&](const char* name, auto& object, bool required)
		{
			auto addr = (void*)GetProcAddress(ext.Module, name);

			if (!addr && required)
			{
				SDR::Error::Make("Extension \"%s\" misses export for \"%s\"", ext.Name.c_str(), name);
			}

			object = (decltype(object))addr;
		};

		findexport("SDR_Query", ext.Query, true);
		findexport("SDR_Initialize", ext.Initialize, true);
		findexport("SDR_ConfigHandler", ext.ConfigHandler, false);
		findexport("SDR_Ready", ext.Ready, true);
		findexport("SDR_StartMovie", ext.StartMovie, false);
		findexport("SDR_EndMovie", ext.EndMovie, false);
		findexport("SDR_NewVideoFrame", ext.NewVideoFrame, false);

		ext.Query(ext.Info);

		SDR::Extension::InitializeData data = {};
		data.Message = SDR::Log::Message;
		data.MessageColor = SDR::Log::MessageColor;
		data.Warning = SDR::Log::Warning;

		ext.Initialize(data);
	}

	void Load(const std::experimental::filesystem::path& path)
	{
		ExtensionData ext = {};

		auto filename = path.filename();
		auto fullutf8 = path.u8string();
		auto nameutf8 = filename.u8string();

		ext.Name = std::move(nameutf8);
		ext.Module = LoadLibraryA(fullutf8.c_str());

		if (!ext.Module)
		{
			SDR::Error::MS::ThrowLastError("Could not load extension \"%s\"", ext.Name.c_str());
		}

		Initialize(ext);

		SDR::Log::Message("SDR: Loaded extension \"%s\"\n", ext.Name.c_str());

		Loaded.emplace_back(std::move(ext));
	}

	void ResolveOrder()
	{
		if (Loaded.size() < 2)
		{
			return;
		}

		auto path = SDR::Library::BuildResourcePath("Extensions\\Enabled\\Order.json");
		
		rapidjson::Document document;

		try
		{
			document = SDR::Json::FromFile(path);
		}

		catch (SDR::File::ScopedFile::ExceptionType status)
		{
			SDR::Log::Warning("SDR: Could not find extension order config"s);
			return;
		}

		if (!document.IsArray())
		{
			SDR::Log::Warning("SDR: Extension order config not an array"s);
			return;
		}

		auto array = document.GetArray();
		
		if (array.Size() < 2)
		{
			return;
		}

		auto temp = std::move(Loaded);

		for (const auto& entry : array)
		{
			for (auto it = temp.begin(); it != temp.end(); ++it)
			{
				if (it->Name == entry.GetString())
				{
					Loaded.emplace_back(std::move(*it));
					break;
				}
			}
		}

		for (auto&& rem : temp)
		{
			if (rem.Module)
			{
				Loaded.emplace_back(std::move(rem));
			}
		}
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
			try
			{
				Load(obj);
			}

			catch (const SDR::Error::Exception& error)
			{
				continue;
			}
		}
	}

	ResolveOrder();
}

bool SDR::ExtensionManager::HasExtensions()
{
	return Loaded.empty() == false;
}

bool SDR::ExtensionManager::Events::CallHandlers(const char* name, const rapidjson::Value& value)
{
	for (const auto& ext : Loaded)
	{
		if (ext.ConfigHandler)
		{
			auto res = ext.ConfigHandler(name, value);

			if (res)
			{
				return true;
			}
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

	data.GetExternalBool = [](const char* name)
	{
		SDR::Console::Variable var(name);
		return var.GetBool();
	};

	data.GetExternalInt = [](const char* name)
	{
		SDR::Console::Variable var(name);
		return var.GetInt();
	};

	data.GetExternalFloat = [](const char* name)
	{
		SDR::Console::Variable var(name);
		return var.GetFloat();
	};

	data.GetExternalString = [](const char* name)
	{
		SDR::Console::Variable var(name);
		return var.GetString();
	};

	data.GetTimeNow = []()
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto start = std::chrono::duration<double>(now.time_since_epoch());

		return start.count();
	};

	data.IsRecordingVideo = SDR::MovieRecord::ShouldRecord;
	data.GetD3D9Device = SDR::SourceGlobals::GetD3D9DeviceEx;

	for (auto& ext : Loaded)
	{
		ext.Ready(data);
	}
}

void SDR::ExtensionManager::Events::StartMovie(const SDR::Extension::StartMovieData& data)
{
	for (const auto& ext : Loaded)
	{
		if (ext.StartMovie)
		{
			ext.StartMovie(data);
		}
	}
}

void SDR::ExtensionManager::Events::EndMovie()
{
	for (const auto& ext : Loaded)
	{
		if (ext.EndMovie)
		{
			ext.EndMovie();
		}
	}
}

void SDR::ExtensionManager::Events::NewVideoFrame(SDR::Extension::NewVideoFrameData& data)
{
	for (const auto& ext : Loaded)
	{
		if (ext.NewVideoFrame)
		{
			ext.NewVideoFrame(data);
		}
	}
}
