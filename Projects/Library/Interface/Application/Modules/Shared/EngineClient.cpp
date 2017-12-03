#include "PrecompiledHeader.hpp"
#include "EngineClient.hpp"
#include "Interface\Application\Application.hpp"

namespace
{
	namespace ModuleEngineClient
	{
		void* Ptr;

		namespace Entries
		{
			SDR::Hooking::ModuleShared::Variant::Entry ConsoleVisible;
			SDR::Hooking::ModuleShared::Variant::Entry FlashWindow;
			SDR::Hooking::ModuleShared::Variant::Entry ClientCommand;
		}

		namespace Variant0
		{
			using ConsoleVisibleType = bool(__fastcall*)(void* thisptr, void* edx);
			SDR::Hooking::ModuleShared::Variant::Function<ConsoleVisibleType> ConsoleVisible(Entries::ConsoleVisible);

			using FlashWindowType = void(__fastcall*)(void* thisptr, void* edx);
			SDR::Hooking::ModuleShared::Variant::Function<FlashWindowType> FlashWindow(Entries::FlashWindow);

			using ClientCommandType = void(__fastcall*)(void* thisptr, void* edx, const char* str);
			SDR::Hooking::ModuleShared::Variant::Function<ClientCommandType> ClientCommand(Entries::ClientCommand);
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"EngineClientPtr",
				[](const rapidjson::Value& value)
				{
					auto address = SDR::Hooking::GetAddressFromJsonPattern(value);

					Ptr = **(void***)(address);
					SDR::Error::ThrowIfNull(Ptr);

					SDR::Hooking::ModuleShared::Registry::SetKeyValue("EngineClientPtr", Ptr);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"EngineClient_ConsoleVisible",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericVariantInit(Entries::ConsoleVisible, value);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"EngineClient_FlashWindow",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericVariantInit(Entries::FlashWindow, value);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"EngineClient_ClientCommand",
				[](const rapidjson::Value& value)
				{
					SDR::Hooking::GenericVariantInit(Entries::ClientCommand, value);
				}
			)
		);
	}
}

void* SDR::EngineClient::GetPtr()
{
	return ModuleEngineClient::Ptr;
}

bool SDR::EngineClient::IsConsoleVisible()
{
	if (ModuleEngineClient::Entries::ConsoleVisible == 0)
	{
		return ModuleEngineClient::Variant0::ConsoleVisible()(GetPtr(), nullptr);
	}

	return false;
}

void SDR::EngineClient::FlashWindow()
{
	if (ModuleEngineClient::Entries::FlashWindow == 0)
	{
		ModuleEngineClient::Variant0::FlashWindow()(GetPtr(), nullptr);
	}
}

void SDR::EngineClient::ClientCommand(const char* str)
{
	if (ModuleEngineClient::Entries::ClientCommand == 0)
	{
		ModuleEngineClient::Variant0::ClientCommand()(GetPtr(), nullptr, str);
	}
}
