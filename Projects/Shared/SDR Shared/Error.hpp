#pragma once
#include "String.hpp"
#include "Log.hpp"
#include <comdef.h>

using namespace std::string_literals;

namespace SDR::Error
{
	struct Exception
	{
		std::string Description;
	};

	void SetPrintFormat(const char* format);
	void Print(const Exception& error);

	struct ScopedContext
	{
		ScopedContext(std::string&& str);
		~ScopedContext();
	};

	/*
		For use with unrecoverable errors.
	*/
	inline void Make(std::string&& str)
	{
		Exception info;
		info.Description = std::move(str);

		Print(info);
		throw info;
	}

	/*
		For use with unrecoverable errors.
	*/
	template <typename... Args>
	inline void Make(const char* format, Args&&... args)
	{
		Make(String::Format(format, std::forward<Args>(args)...));
	}

	/*
		For use with unrecoverable errors.
	*/
	template <typename... Args>
	inline void ThrowIfNull(const void* ptr, const char* format, Args&&... args)
	{
		if (ptr == nullptr)
		{
			Make(format, std::forward<Args>(args)...);
		}
	}

	/*
		For use with unrecoverable errors.
	*/
	inline void ThrowIfNull(const void* ptr)
	{
		if (ptr == nullptr)
		{
			Make("Null pointer"s);
		}
	}

	/*
		For use with unrecoverable errors.
	*/
	template <typename T, typename... Args>
	inline void ThrowIfZero(T value, const char* format, Args&&... args)
	{
		if (value == 0)
		{
			Make(format, std::forward<Args>(args)...);
		}
	}

	namespace MS
	{
		/*
			For use with unrecoverable errors.
		*/
		template <typename... Args>
		inline void ThrowIfFailed(HRESULT hr, const char* format, Args&&... args)
		{
			if (FAILED(hr))
			{
				_com_error error(hr);
				auto message = error.ErrorMessage();

				auto user = String::Format(format, std::forward<Args>(args)...);
				auto final = String::Format("%08X (%s) -> ", hr, message);

				final += user;

				Make(std::move(final));
			}
		}

		/*
			For use with unrecoverable errors.
		*/
		template <typename... Args>
		inline void ThrowLastError(const char* format, Args&&... args)
		{
			auto error = GetLastError();
			ThrowIfFailed(HRESULT_FROM_WIN32(error), format, std::forward<Args>(args)...);
		}

		/*
			For use with unrecoverable errors.
		*/
		template <typename T, typename... Args>
		inline void ThrowIfZero(T value, const char* format, Args&&... args)
		{
			if (value == 0)
			{
				ThrowLastError(format, std::forward<Args>(args)...);
			}
		}
	}
}
