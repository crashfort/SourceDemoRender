#pragma once

#ifdef SVR_CORE_EXPORT
#define SVR_API __declspec(dllexport)
#else
#define SVR_API __declspec(dllimport)
#endif

#define SVR_EXPORT __declspec(dllexport)
