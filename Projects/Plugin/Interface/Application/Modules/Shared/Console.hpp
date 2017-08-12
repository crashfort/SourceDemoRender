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

		bool GetBool() const;
		int GetInt() const;
		float GetFloat() const;
		const char* GetString() const;

		void SetValue(int value);
		void SetValue(float value);
		void SetValue(const char* value);

		void* Opaque = nullptr;
		std::unique_ptr<uint8_t[]> Blob;
	};

	using VariablePtr = std::unique_ptr<Variable>;

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

	using CommandPtr = std::unique_ptr<Command>;

	using CommandCallbackArgsType = void(*)(const void* args);
	using CommandCallbackVoidType = void(*)();

	std::unique_ptr<Command> MakeCommand(const char* name, CommandCallbackArgsType callback);
	std::unique_ptr<Command> MakeCommand(const char* name, CommandCallbackVoidType callback);

	std::unique_ptr<Variable> MakeBool(const char* name, const char* value);

	std::unique_ptr<Variable> MakeNumber(const char* name, const char* value, float min);
	std::unique_ptr<Variable> MakeNumber(const char* name, const char* value, float min, float max);
	std::unique_ptr<Variable> MakeNumberWithString(const char* name, const char* value, float min, float max);

	std::unique_ptr<Variable> MakeString(const char* name, const char* value);
}
