

#ifndef ODPRINTF_H
#define ODPRINTF_H

#define ODPRINTF_ENABLED

#if defined(_DEBUG) || defined(ODPRINTF_ENABLED)
#include <windows.h>
#include <stdio.h>
#define odprintf(format, ...) do { char odp[1025]; sprintf_s(odp, sizeof(odp), \
  format "\n", __VA_ARGS__); OutputDebugStringA(odp); } while (0)
#else
#define odprintf(format, ...)
#endif

#endif