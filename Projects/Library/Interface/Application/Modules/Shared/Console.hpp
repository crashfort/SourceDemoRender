#pragma once
#include <memory>
#include <cstdint>
#include <SDR Shared\ConsoleTypes.hpp>

namespace SDR::Console
{
	struct Variable
	{
		Variable() = default;
		Variable(const Variable& other) = delete;
		Variable(Variable&& other) = default;

		Variable& operator=(const Variable& other) = delete;
		Variable& operator=(Variable&& other) = default;

		explicit operator bool() const;

		static Variable Find(const char* name);
		
		template <typename T = void>
		static T SetValueGetOld(const char* name, T value)
		{
			
		}

		template <>
		static bool SetValueGetOld(const char* name, bool value)
		{
			auto variable = Find(name);

			if (variable)
			{
				auto old = variable.GetBool();
				variable.SetValue(value);

				return old;
			}

			return {};
		}

		template <>
		static int SetValueGetOld(const char* name, int value)
		{
			auto variable = Find(name);

			if (variable)
			{
				auto old = variable.GetInt();
				variable.SetValue(value);

				return old;
			}

			return {};
		}

		template <>
		static float SetValueGetOld(const char* name, float value)
		{
			auto variable = Find(name);

			if (variable)
			{
				auto old = variable.GetFloat();
				variable.SetValue(value);

				return old;
			}

			return {};
		}

		template <>
		static const char* SetValueGetOld(const char* name, const char* value)
		{
			auto variable = Find(name);

			if (variable)
			{
				auto old = variable.GetString();
				variable.SetValue(value);

				return old;
			}

			return {};
		}

		template <typename T>
		static void SetValue(const char* name, T value)
		{
			auto variable = Find(name);

			if (variable)
			{
				variable.SetValue(value);
			}
		}

		bool GetBool() const;
		int GetInt() const;
		float GetFloat() const;
		const char* GetString() const;

		void SetValue(bool value);
		void SetValue(int value);
		void SetValue(float value);
		void SetValue(const char* value);

		void* Opaque = nullptr;
	};

	struct CommandArgs
	{
		CommandArgs(const void* ptr);

		int Count() const;
		const char* At(int index) const;
		const char* FullArgs() const;
		const char* FullValue() const;

		const void* Ptr;
	};

	struct Command
	{
		void* Opaque = nullptr;
	};

	void Load();
	bool IsOutputToGameConsole();

	void MakeCommand(const char* name, Types::CommandCallbackVoidType callback);
	void MakeCommand(const char* name, Types::CommandCallbackArgsType callback);

	Variable MakeBool(const char* name, const char* value);

	Variable MakeNumber(const char* name, const char* value);
	Variable MakeNumber(const char* name, const char* value, float min);
	Variable MakeNumber(const char* name, const char* value, float min, float max);
	Variable MakeNumberWithString(const char* name, const char* value, float min, float max);

	Variable MakeString(const char* name, const char* value);
}
