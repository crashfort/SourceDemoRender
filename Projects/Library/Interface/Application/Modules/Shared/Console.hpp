#pragma once
#include <memory>
#include <cstdint>
#include <SDR Shared\ConsoleTypes.hpp>

namespace SDR::Console
{
	struct Variable
	{
		Variable() = default;
		Variable(const char* ref);
		Variable(const Variable& other) = delete;
		Variable(Variable&& other) = default;
		Variable& operator=(const Variable& other) = delete;
		Variable& operator=(Variable&& other) = default;

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

	struct CommandArgs
	{
		CommandArgs(const void* ptr);

		int Count() const;
		const char* At(int index) const;
		const char* FullArgs() const;

		const void* Ptr;
	};

	struct Command
	{
		void* Opaque = nullptr;
		std::unique_ptr<uint8_t[]> Blob;
	};

	void Load();

	void MakeCommand(const char* name, Types::CommandCallbackVoidType callback);
	void MakeCommand(const char* name, Types::CommandCallbackArgsType callback);

	Variable MakeBool(const char* name, const char* value);

	Variable MakeNumber(const char* name, const char* value);
	Variable MakeNumber(const char* name, const char* value, float min);
	Variable MakeNumber(const char* name, const char* value, float min, float max);
	Variable MakeNumberWithString(const char* name, const char* value, float min, float max);

	Variable MakeString(const char* name, const char* value);
}
