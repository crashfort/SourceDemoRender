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
				[](const char* name, const rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);
					DX9Device = **(IDirect3DDevice9Ex***)(address);

					SDR::Error::ThrowIfNull(DX9Device);
				}
			),
			SDR::ModuleHandlerAdder
			(
				"DrawLoading",
				[](const char* name, const rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);
					DrawLoading = *(bool**)(address);

					SDR::Error::ThrowIfNull(DrawLoading);
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
