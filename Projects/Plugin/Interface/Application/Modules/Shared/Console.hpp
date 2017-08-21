#pragma once
#include <memory>
#include <cstdint>

namespace SDR::Console
{
	struct Variable
	{
		Variable() = default;
		Variable(const char* ref);
		Variable(const Variable& other) = delete;
		Variable(Variable&& other);
		~Variable();

		void operator=(Variable&& other);

		bool GetBool() const;
		int GetInt() const;
		float GetFloat() const;
		const char* GetString() const;

		void SetValue(const char* value);
		void SetValue(float value);
		void SetValue(int value);

		void* Opaque = nullptr;
		std::unique_ptr<uint8_t[]> Blob;
	};

	using VariablePtr = Variable;

	struct CommandArgs
	{
		CommandArgs(const void* ptr);

		int Count() const;
		const char* At(int index) const;

		const void* Ptr;
	};

	struct Command
	{
		void* Opaque = nullptr;
		std::unique_ptr<uint8_t[]> Blob;
	};

	void Load();

	using CommandPtr = Command;

	using CommandCallbackArgsType = void(*)(const void* args);
	using CommandCallbackVoidType = void(*)();

	void MakeCommand(const char* name, CommandCallbackVoidType callback);

	VariablePtr MakeBool(const char* name, const char* value);

	VariablePtr MakeNumber(const char* name, const char* value, float min);
	VariablePtr MakeNumber(const char* name, const char* value, float min, float max);
	VariablePtr MakeNumberWithString(const char* name, const char* value, float min, float max);

	VariablePtr MakeString(const char* name, const char* value);
}
