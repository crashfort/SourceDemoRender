#pragma once

namespace SDR
{
	void Setup
	(
		const char* gamepath,
		const char* gamename
	);

	void Close();

	struct StartupFuncData
	{
		using FuncType = bool(*)();

		const char* Name;
		FuncType Function;
	};

	void AddPluginStartupFunction
	(
		const StartupFuncData& data
	);

	struct PluginStartupFunctionAdder
	{
		PluginStartupFunctionAdder
		(
			const char* name,
			StartupFuncData::FuncType function
		)
		{
			StartupFuncData data;
			data.Name = name;
			data.Function = function;

			AddPluginStartupFunction(data);
		}
	};

	void CallPluginStartupFunctions();

	using ShutdownFuncType = void(*)();
	void AddPluginShutdownFunction
	(
		ShutdownFuncType function
	);

	struct PluginShutdownFunctionAdder
	{
		PluginShutdownFunctionAdder
		(
			ShutdownFuncType function
		)
		{
			AddPluginShutdownFunction
			(
				function
			);
		}
	};

	struct ModuleHandlerData
	{
		using FuncType = bool(*)
		(
			const char* name,
			rapidjson::Value& value
		);

		const char* Name;
		FuncType Function;
	};

	void AddModuleHandler
	(
		const ModuleHandlerData& data
	);

	struct ModuleHandlerAdder
	{
		ModuleHandlerAdder
		(
			const char* name,
			ModuleHandlerData::FuncType function
		)
		{
			ModuleHandlerData data;
			data.Name = name;
			data.Function = function;

			AddModuleHandler
			(
				data
			);
		}
	};

	template <typename... Types>
	constexpr auto CreateAdders
	(
		Types&&... types
	)
	{
		return std::array<ModuleHandlerAdder, sizeof...(Types)>
		{
			{
				std::forward<Types>(types)...
			}
		};
	}

	struct ModuleInformation
	{
		ModuleInformation
		(
			const char* name
		)
			: Name(name)
		{
			MODULEINFO info;
			
			K32GetModuleInformation
			(
				GetCurrentProcess(),
				GetModuleHandleA(name),
				&info,
				sizeof(info)
			);

			MemoryBase = info.lpBaseOfDll;
			MemorySize = info.SizeOfImage;
		}

		const char* Name;

		void* MemoryBase;
		size_t MemorySize;
	};

	struct BytePattern
	{
		struct Entry
		{
			uint8_t Value;
			bool Unknown;
		};

		std::vector<Entry> Bytes;
	};

	template <typename FuncSignature>
	struct HookModule
	{
		inline auto GetOriginal() const
		{
			return static_cast<FuncSignature>(OriginalFunction);
		}

		void* TargetFunction;
		void* NewFunction;
		void* OriginalFunction;
	};

	BytePattern GetPatternFromString
	(
		const char* input
	);

	void* GetAddressFromPattern
	(
		const ModuleInformation& library,
		const BytePattern& pattern
	);

	void* GetAddressFromJsonPattern
	(
		rapidjson::Value& value
	);

	void* GetVirtualAddressFromIndex
	(
		void* ptr,
		int index
	);

	void* GetVirtualAddressFromJson
	(
		void* ptr,
		rapidjson::Value& value
	);

	namespace ModuleShared
	{
		template <typename T>
		bool SetFromAddress
		(
			T& type,
			void* address
		)
		{
			type = (T)(address);

			if (!address)
			{
				return false;
			}

			return true;
		}

		inline int GetJsonOffsetInt
		(
			rapidjson::Value& value
		)
		{
			auto iter = value.FindMember("Offset");

			if (iter == value.MemberEnd())
			{
				Warning
				(
					"SDR: Offset field does not exist\n"
				);

				throw false;
			}

			if (!iter->value.IsNumber())
			{
				Warning
				(
					"SDR: Offset field not a number\n"
				);

				throw false;
			}

			return iter->value.GetInt();
		}

		inline void Verify
		(
			void* address,
			const char* module,
			const char* name
		)
		{
			if (!address)
			{
				Warning
				(
					"SDR: Could not enable module \"%s\"\n",
					name
				);

				throw name;
			}
		}
	}

	template <typename FuncType>
	void CreateHook
	(
		HookModule<FuncType>& hook,
		FuncType override,
		void* address
	)
	{
		hook.TargetFunction = address;
		hook.NewFunction = override;

		auto res = MH_CreateHookEx
		(
			hook.TargetFunction,
			hook.NewFunction,
			&hook.OriginalFunction
		);

		if (res != MH_OK)
		{
			throw res;
		}

		res = MH_EnableHook(hook.TargetFunction);

		if (res != MH_OK)
		{
			throw res;
		}
	}

	template <typename FuncType>
	void CreateHook
	(
		HookModule<FuncType>& hook,
		FuncType override,
		const char* module,
		const BytePattern& pattern
	)
	{
		auto address = GetAddressFromPattern
		(
			module,
			pattern
		);

		CreateHook
		(
			hook,
			override,
			address
		);
	}

	template <typename FuncType>
	void CreateHook
	(
		HookModule<FuncType>& hook,
		FuncType override,
		rapidjson::Value& value
	)
	{
		auto module = value["Module"].GetString();
		auto patternstr = value["Pattern"].GetString();

		auto pattern = GetPatternFromString(patternstr);

		CreateHook
		(
			hook,
			override,
			module,
			pattern
		);
	}

	template <typename FuncType>
	bool CreateHookShort
	(
		HookModule<FuncType>& hook,
		FuncType override,
		rapidjson::Value& value
	)
	{
		try
		{
			CreateHook
			(
				hook,
				override,
				value
			);
		}

		catch (MH_STATUS status)
		{
			return false;
		}

		return true;
	}

	struct AddressFinder
	{
		AddressFinder
		(
			const char* module,
			const BytePattern& pattern,
			int offset = 0
		)
		{
			auto addr = GetAddressFromPattern
			(
				module,
				pattern
			);

			auto addrmod = static_cast<uint8_t*>(addr);

			if (addrmod)
			{
				/*
					Increment for any extra instructions
				*/
				addrmod += offset;
			}

			Address = addrmod;
		}

		void* Get() const
		{
			return Address;
		}

		void* Address;
	};

	/*
		First byte at target address should be E8
	*/
	struct RelativeJumpFunctionFinder
	{
		RelativeJumpFunctionFinder
		(
			void* address
		)
		{
			auto addrmod = static_cast<uint8_t*>(address);

			/*
				Skip the E8 byte
			*/
			addrmod += sizeof(uint8_t);

			auto offset = *reinterpret_cast<ptrdiff_t*>(addrmod);

			/*
				E8 jumps count relative distance from the next instruction,
				in 32 bit the distance will be measued in 4 bytes.
			*/
			addrmod += sizeof(uintptr_t);

			/*
				Do the jump, address will now be the target function
			*/
			addrmod += offset;

			Address = addrmod;
		}

		void* Get() const
		{
			return Address;
		}

		void* Address;
	};

	struct StructureWalker
	{
		StructureWalker
		(
			void* address
		) :
			Address(static_cast<uint8_t*>(address)),
			Start(Address)
		{

		}

		template <typename Modifier = uint8_t>
		uint8_t* Advance(int offset)
		{
			Address += offset * sizeof(Modifier);
			return Address;
		}

		template <typename Modifier = uint8_t>
		uint8_t* AdvanceAbsolute(int offset)
		{
			Reset();
			Address += offset * sizeof(Modifier);
			return Address;
		}

		void Reset()
		{
			Address = Start;
		}

		uint8_t* Address;
		uint8_t* Start;
	};
}
