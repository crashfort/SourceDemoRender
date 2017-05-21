#pragma once

/*
	These can be copied as a starting point for any new modules
*/

#if 0
namespace Module_BaseTemplateMask
{
	auto Pattern = SDR::MemoryPattern
	(
		""
	);

	auto Mask =
	(
		""
	);

	void __cdecl Override();

	using ThisFunction = decltype(Override)*;

	SDR::HookModuleMask<ThisFunction> ThisHook
	{
		"", "", Override, Pattern, Mask
	};

	void __cdecl Override()
	{

	}
}

namespace Module_BaseTemplateStaticTest
{
	void __cdecl Override();

	using ThisFunction = decltype(Override)*;

	SDR::HookModuleStaticAddressTest<ThisFunction> ThisHook
	{
		"", "", Override, 0x00000000
	};

	void __cdecl Override()
	{

	}
}

namespace Module_BaseTemplateStatic
{
	void __cdecl Override();

	using ThisFunction = decltype(Override)*;

	SDR::HookModuleStaticAddress<ThisFunction> ThisHook
	{
		"", "", Override, nullptr
	};

	void __cdecl Override()
	{

	}
}

namespace Module_BaseTemplateAPI
{
	void __cdecl Override();

	using ThisFunction = decltype(Override)*;

	SDR::HookModuleAPI<ThisFunction> ThisHook
	{
		"", "", "", Override
	};

	void __cdecl Override()
	{

	}
}
#endif
