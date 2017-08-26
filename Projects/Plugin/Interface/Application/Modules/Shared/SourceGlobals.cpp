#include "PrecompiledHeader.hpp"
#include "SourceGlobals.hpp"
#include "Interface\Application\Application.hpp"

namespace
{
	namespace ModuleSourceGlobals
	{
		IDirect3DDevice9Ex* DX9Device;
		bool* DrawLoading;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"D3D9_Device",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					DX9Device = **(IDirect3DDevice9Ex***)(address);
					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"DrawLoading",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					DrawLoading = *(bool**)(address);
					return true;
				}
			)
		);
	}
}

IDirect3DDevice9Ex* SDR::SourceGlobals::GetD3D9DeviceEx()
{
	return ModuleSourceGlobals::DX9Device;
}

bool SDR::SourceGlobals::IsDrawingLoading()
{
	return *ModuleSourceGlobals::DrawLoading;
}
