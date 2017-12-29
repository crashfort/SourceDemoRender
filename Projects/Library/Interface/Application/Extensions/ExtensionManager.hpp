#pragma once
#include <SDR Extension\Extension.hpp>
#include <d3d11.h>

namespace SDR::ExtensionManager
{
	void LoadExtensions();
	bool HasExtensions();

	bool IsNamespaceLoaded(const char* object);

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
		void StartMovie(const SDR::Extension::StartMovieData& data);

		/*
			
		*/
		void EndMovie();

		/*
			
		*/
		void NewVideoFrame(const SDR::Extension::NewVideoFrameData& data);
	}
}
