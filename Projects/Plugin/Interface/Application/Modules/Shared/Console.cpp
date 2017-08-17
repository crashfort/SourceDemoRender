#include "PrecompiledHeader.hpp"
#include "Console.hpp"
#include "Interface\Application\Application.hpp"

namespace
{
	namespace VTables
	{
		void* IConVar;
		void* ConCommandBase;

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"IConVar_VTable",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					IConVar = address;

					SDR::ModuleShared::Registry::SetKeyValue(name, IConVar);
					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"ConCommandBase_VTable",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					ConCommandBase = address;

					SDR::ModuleShared::Registry::SetKeyValue(name, ConCommandBase);
					return true;
				}
			)
		);
	}

	namespace ModuleConCommandBase
	{
		int Variant;

		namespace Entries
		{
			SDR::ModuleShared::Variant::Entry CreateBase;
		}

		enum
		{
			VariantCount = 1
		};

		namespace Variant0
		{
			struct Data
			{
				void* VTable_ConCommandBase;

				Data* Next;
				bool Registered;
				const char* Name;
				const char* HelpString;
				int Flags;
			};

			using CreateBaseType = void(__fastcall*)(void* thisptr, void* edx, const char* name, const char* helpstr, int flags);
			SDR::ModuleShared::Variant::Function<CreateBaseType> CreateBase(Entries::CreateBase);
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"ConCommandBase_Info",
				[](const char* name, rapidjson::Value& value)
				{
					Variant = SDR::GetVariantFromJson(value);
					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"ConCommandBase_CreateBase",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::GenericVariantInit(Entries::CreateBase, name, value, VariantCount);
				}
			)
		);
	}

	namespace ModuleCCommand
	{
		int Variant;

		namespace Variant0
		{
			struct Data
			{
				enum
				{
					COMMAND_MAX_ARGC = 64,
					COMMAND_MAX_LENGTH = 512,
				};

				int ArgC;
				int ArgV0Size;
				char ArgSBuffer[COMMAND_MAX_LENGTH];
				char ArgVBuffer[COMMAND_MAX_LENGTH];
				const char* ArgV[COMMAND_MAX_ARGC];
			};
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"CCommand_Info",
				[](const char* name, rapidjson::Value& value)
				{
					Variant = SDR::GetVariantFromJson(value);
					return true;
				}
			)
		);
	}

	namespace ModuleConCommand
	{
		int Variant;

		namespace Variant0
		{
			struct Data : ModuleConCommandBase::Variant0::Data
			{
				union
				{
					void* m_fnCommandCallbackV1;
					void* m_fnCommandCallback;
					void* m_pCommandCallback;
				};

				union
				{
					void* m_fnCompletionCallback;
					void* m_pCommandCompletionCallback;
				};

				bool m_bHasCompletionCallback : 1;
				bool m_bUsingNewCommandCallback : 1;
				bool m_bUsingCommandCallbackInterface : 1;
			};
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"ConCommand_Info",
				[](const char* name, rapidjson::Value& value)
				{
					Variant = SDR::GetVariantFromJson(value);
					return true;
				}
			)
		);
	}

	namespace ModuleConVar
	{
		int Variant;
		int NeverAsStringFlag;

		namespace Entries
		{
			SDR::ModuleShared::Variant::Entry Constructor3;
		}

		enum
		{
			VariantCount = 1
		};

		namespace Variant0
		{
			struct Data : ModuleConCommandBase::Variant0::Data
			{
				void* VTable_IConVar;

				void* Parent;
				const char* DefaultValue;

				char* String;
				int StringLength;

				float FloatValue;
				int IntValue;

				bool HasMin;
				float MinVal;
				bool HasMax;
				float MaxVal;

				void* ChangeCallback;
			};

			using Constructor3Type = void(__fastcall*)
			(
				void* thisptr, void* edx, const char* name, const char* value, int flags,
				const char* helpstr, bool hasmin, float min, bool hasmax, float max
			);

			SDR::ModuleShared::Variant::Function<Constructor3Type> Constructor3(Entries::Constructor3);
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"Convar_Info",
				[](const char* name, rapidjson::Value& value)
				{
					Variant = SDR::GetVariantFromJson(value);
					NeverAsStringFlag = value["NeverAsStringFlag"].GetInt();
					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"ConVar_Constructor3",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::GenericVariantInit(Entries::Constructor3, name, value, VariantCount);
				}
			)
		);
	}

	namespace ModuleCVar
	{
		void* Ptr;

		namespace Entries
		{
			SDR::ModuleShared::Variant::Entry FindVar;
		}

		enum
		{
			VariantCount = 1
		};

		namespace Variant0
		{
			using FindVarType = void*(__fastcall*)(void* thisptr, void* edx, const char* name);
			SDR::ModuleShared::Variant::Function<FindVarType> FindVar(Entries::FindVar);
		}

		auto Adders = SDR::CreateAdders
		(
			SDR::ModuleHandlerAdder
			(
				"CvarPtr",
				[](const char* name, rapidjson::Value& value)
				{
					auto address = SDR::GetAddressFromJsonPattern(value);

					if (!address)
					{
						return false;
					}

					Ptr = **(void***)(address);

					SDR::ModuleShared::Registry::SetKeyValue(name, Ptr);
					return true;
				}
			),
			SDR::ModuleHandlerAdder
			(
				"Cvar_FindVar",
				[](const char* name, rapidjson::Value& value)
				{
					return SDR::GenericVariantInit(Entries::FindVar, name, value, VariantCount);
				}
			)
		);
	}
}

namespace
{
	auto MakeGenericVariable(const char* name, const char* value, int flags = 0, bool hasmin = 0, float min = 0, bool hasmax = 0, float max = 0)
	{
		auto ret = std::make_unique<SDR::Console::Variable>();
		size_t size = 0;

		if (ModuleConVar::Variant == 0)
		{
			size = sizeof(ModuleConVar::Variant0::Data);
		}

		ret->Blob = std::make_unique<uint8_t[]>(size);
		ret->Opaque = ret->Blob.get();

		std::memset(ret->Opaque, 0, size);

		if (ModuleConVar::Variant == 0)
		{
			auto data = (ModuleConVar::Variant0::Data*)ret->Opaque;
			data->VTable_ConCommandBase = VTables::ConCommandBase;
			data->VTable_IConVar = VTables::IConVar;
		}

		if (ret->Opaque && ModuleConVar::Entries::Create == 0)
		{
			ModuleConVar::Variant0::Create()(ret->Opaque, nullptr, name, value, flags, "", hasmin, min, hasmax, max, nullptr);
		}

		return ret;
	}

	auto MakeGenericCommand(const char* name, void* callback)
	{
		auto ret = std::make_unique<SDR::Console::Command>();
		size_t size = 0;

		if (ModuleConCommand::Variant == 0)
		{
			size = sizeof(ModuleConCommand::Variant0::Data);
		}

		ret->Blob = std::make_unique<uint8_t[]>(size);
		ret->Opaque = ret->Blob.get();

		std::memset(ret->Opaque, 0, size);

		if (ModuleConCommand::Variant == 0)
		{
			auto data = (ModuleConCommand::Variant0::Data*)ret->Opaque;
			data->VTable_ConCommandBase = VTables::ConCommandBase;
			data->m_fnCommandCallback = callback;
		}

		if (ret->Opaque && ModuleConCommandBase::Entries::CreateBase == 0)
		{
			ModuleConCommandBase::Variant0::CreateBase()(ret->Opaque, nullptr, name, "", 0);
		}

		return ret;
	}

	SDR::PluginShutdownFunctionAdder A1([]()
	{

	});
}

namespace SDR::Console
{
	Variable::Variable(const char* ref)
	{
		if (ModuleCVar::Entries::FindVar == 0)
		{
			Opaque = ModuleCVar::Variant0::FindVar()(ModuleCVar::Ptr, nullptr, ref);
		}
	}

	Variable::Variable(Variable&& other)
	{
		if (this != &other)
		{
			Blob = std::move(other.Blob);
			Opaque = other.Opaque;
			other.Opaque = nullptr;
		}
	}

	Variable::~Variable()
	{

	}

	bool Variable::GetBool() const
	{
		return !!GetInt();
	}

	int Variable::GetInt() const
	{
		if (ModuleConVar::Variant == 0)
		{
			auto ptr = (ModuleConVar::Variant0::Data*)Opaque;
			return ptr->IntValue;
		}

		return 0;
	}

	float Variable::GetFloat() const
	{
		if (ModuleConVar::Variant == 0)
		{
			auto ptr = (ModuleConVar::Variant0::Data*)Opaque;
			return ptr->FloatValue;
		}

		return 0;
	}

	const char* Variable::GetString() const
	{
		if (ModuleConVar::Variant == 0)
		{
			auto ptr = (ModuleConVar::Variant0::Data*)Opaque;
			return ptr->String;
		}

		return nullptr;
	}

	void Variable::SetValue(int value)
	{

	}

	void Variable::SetValue(float value)
	{

	}

	void Variable::SetValue(const char* value)
	{

	}

	SDR::Console::CommandArgs::CommandArgs(const void* ptr) : Ptr(ptr)
	{
		
	}

	int CommandArgs::Count() const
	{
		if (ModuleCCommand::Variant == 0)
		{
			auto ptr = (ModuleCCommand::Variant0::Data*)Ptr;
			return ptr->ArgC;
		}

		return 0;
	}

	const char* CommandArgs::At(int index) const
	{
		if (ModuleCCommand::Variant == 0)
		{
			auto ptr = (ModuleCCommand::Variant0::Data*)Ptr;
			return ptr->ArgV[index];
		}

		return nullptr;
	}
}

std::unique_ptr<SDR::Console::Command> SDR::Console::MakeCommand(const char* name, CommandCallbackArgsType callback)
{
	return MakeGenericCommand(name, callback);
}

std::unique_ptr<SDR::Console::Command> SDR::Console::MakeCommand(const char* name, CommandCallbackVoidType callback)
{
	return MakeGenericCommand(name, callback);
}

std::unique_ptr<SDR::Console::Variable> SDR::Console::MakeBool(const char* name, const char* value)
{
	return MakeGenericVariable(name, value, ModuleConVar::NeverAsStringFlag, true, 0,  true, 1);
}

std::unique_ptr<SDR::Console::Variable> SDR::Console::MakeNumber(const char* name, const char* value, float min)
{
	return MakeGenericVariable(name, value, ModuleConVar::NeverAsStringFlag, true, min);
}

std::unique_ptr<SDR::Console::Variable> SDR::Console::MakeNumber(const char* name, const char* value, float min, float max)
{
	return MakeGenericVariable(name, value, ModuleConVar::NeverAsStringFlag, true, min, true, max);
}

std::unique_ptr<SDR::Console::Variable> SDR::Console::MakeNumberWithString(const char* name, const char* value, float min, float max)
{
	return MakeGenericVariable(name, value, 0, true, min, true, max);
}

std::unique_ptr<SDR::Console::Variable> SDR::Console::MakeString(const char* name, const char* value)
{
	return MakeGenericVariable(name, value);
}
