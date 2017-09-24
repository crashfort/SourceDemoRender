#include "PrecompiledHeader.hpp"
#include "MaterialSystem.hpp"
#include "Interface\Application\Application.hpp"

namespace
{
	namespace ModuleMaterialSystem
	{		
		void* Ptr;

		namespace Entries
		{
			SDR::ModuleShared::Variant::Entry GetBackBufferDimensions;
		}

		namespace Variant0
		{
			using GetBackBufferDimensionsType = void(__fastcall*)(void* thisptr, void* edx, int& width, int& height);
			SDR::ModuleShared::Variant::Function<GetBackBufferDimensionsType> GetBackBufferDimensions(Entries::GetBackBufferDimensions);
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"MaterialsPtr",
				[](const rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					Ptr = **(void***)(address);
					SDR::Error::ThrowIfNull(Ptr);

					SDR::ModuleShared::Registry::SetKeyValue("MaterialsPtr", Ptr);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_GetBackBufferDimensions",
				[](const rapidjson::Value& value)
				{
					SDR::GenericVariantInit(Entries::GetBackBufferDimensions, value);
				}
			)
		);
	}
}

void* SDR::MaterialSystem::GetPtr()
{
	return ModuleMaterialSystem::Ptr;
}

void SDR::MaterialSystem::GetBackBufferDimensions(int& width, int& height)
{
	if (ModuleMaterialSystem::Entries::GetBackBufferDimensions == 0)
	{
		ModuleMaterialSystem::Variant0::GetBackBufferDimensions()(GetPtr(), nullptr, width, height);
	}
}
