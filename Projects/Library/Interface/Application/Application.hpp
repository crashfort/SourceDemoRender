#pragma once
#include <array>
#include <SDR Shared\Json.hpp>
#include <SDR Shared\Hooking.hpp>

namespace SDR
{
	void PreEngineSetup();

	void Setup();

	struct StartupFuncData
	{
		using FuncType = void(*)();

		const char* Name;
		FuncType Function;
	};

	void AddStartupFunction(const StartupFuncData& data);

	struct StartupFunctionAdder
	{
		StartupFunctionAdder(const char* name, StartupFuncData::FuncType function)
		{
			StartupFuncData data;
			data.Name = name;
			data.Function = function;

			AddStartupFunction(data);
		}
	};

	struct ModuleHandlerData
	{
		using FuncType = void(*)(const rapidjson::Value& value);

		const char* Name;
		FuncType Function;
	};

	void AddModuleHandler(const ModuleHandlerData& data);

	struct ModuleHandlerAdder
	{
		ModuleHandlerAdder(const char* name, ModuleHandlerData::FuncType function)
		{
			ModuleHandlerData data;
			data.Name = name;
			data.Function = function;

			AddModuleHandler(data);
		}
	};

	template <typename... Types>
	constexpr auto CreateAdders(Types&&... types)
	{
		return std::array<std::common_type_t<Types...>, sizeof...(Types)>{{std::forward<Types>(types)...}};
	}
}
