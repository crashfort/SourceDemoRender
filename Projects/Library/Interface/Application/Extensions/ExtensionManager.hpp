#pragma once
#include "SDR Extension\Extension.hpp"
#include <d3d11.h>

namespace SDR::ExtensionManager
{
	void LoadExtensions();
	bool HasExtensions();

	namespace Events
	{
		/*
			
		*/
		bool CallHandlers(const char* name, const rapidjson::Value& value);

		/*
			
		*/
		void Ready();

		/*
			
		*/
		void ModifyFrame(ID3D11DeviceContext* context);
	}
}
