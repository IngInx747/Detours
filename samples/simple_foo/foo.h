#ifndef FOO_H
#define FOO_H

#ifdef FOO_EXPORTS
#define FOO_API __declspec(dllexport)
#else
#define FOO_API __declspec(dllimport)
#endif

extern "C" FOO_API void foo();

#endif