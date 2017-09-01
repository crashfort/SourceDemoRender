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

		enum
		{
			VariantCount = 1
		};

		namespace Variant0
		{
			using GetBackBufferDimensionsType = bool(__fastcall*)(void* thisptr, void* edx, int& width, int& height);
			SDR::ModuleShared::Variant::Function<GetBackBufferDimensionsType> GetBackBufferDimensions(Entries::GetBackBufferDimensions);
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"MaterialsPtr",
				[](const char* name, const rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					Ptr = **(void***)(address);

					SDR::ModuleShared::Registry::SetKeyValue(name, Ptr);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"MaterialSystem_GetBackBufferDimensions",
				[](const char* name, const rapidjson::Value& value)
				{
					SDR::GenericVariantInit(Entries::GetBackBufferDimensions, name, value, VariantCount);
				}
			)
		);
	}
}

void* SDR::MaterialSystem::GetPtr()
{
	return ModuleMaterialSystem::Ptr;
}

bool SDR::MaterialSystem::GetBackBufferDimensions(int& width, int& height)
{
	if (ModuleMaterialSystem::Entries::GetBackBufferDimensions == 0)
	{
		return ModuleMaterialSystem::Variant0::GetBackBufferDimensions()(GetPtr(), nullptr, width, height);
	}

	return false;
}
