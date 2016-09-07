

// Adds UPnP port forwarding and makes the TCP layer bind to all adapters.

#include <windows.h>
#include "PortForward.h"

#ifdef _DEBUG
#include <stdio.h>
#define odprintf(format, ...) do { char odp[1025]; sprintf_s(odp, sizeof(odp), \
  format "\n", __VA_ARGS__); OutputDebugString(odp); } while (0)
#else
#define odprintf(format, ...)
#endif

#define numof(array) (sizeof(array) / sizeof(array[0]))

const int OP2_BEGIN_PORT = 47776,
          OP2_END_PORT   = 47807;


bool SetBindPatches(bool enable);
DWORD WINAPI PortForwardTask(LPVOID lpParam);
DWORD WINAPI PortUnforwardTask(LPVOID lpParam);

enum fwdType {
  noForward = 0,
  staticForward,
  dynamicForward // ** Currently unsupported by UPnPNAT API
} upnpMode;
int leaseSec;

wchar_t localIp[46] = {};
HANDLE hFwdThread = NULL;


BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
  if (fdwReason == DLL_PROCESS_DETACH) {
    SetBindPatches(false);

    if (upnpMode != noForward && localIp[0]) {
      // Do UPnP in its own thread due to unresponsiveness of COM
      // This doesn't seem to work here, blame Hooman for not adding mod cleanup
      DWORD threadId = NULL;
      if (hFwdThread) {
        WaitForSingleObject(hFwdThread, INFINITE);
      }
      WaitForSingleObject(
        CreateThread(NULL, 0, PortUnforwardTask, NULL, 0, &threadId),
        INFINITE);
    }
  }
  return TRUE;
}


extern "C" __declspec(dllexport) void InitMod(char* iniSectionName) {
  int bind = GetPrivateProfileInt(iniSectionName, "BindAll", 1, ".\\Outpost2.ini");
  upnpMode = (fwdType)GetPrivateProfileInt(iniSectionName, "UPnPMode", 1,
                                           ".\\Outpost2.ini");
  leaseSec = GetPrivateProfileInt(iniSectionName, "LeaseSec", 24*60*60,
                                  ".\\Outpost2.ini");
  char forcedIp[46];
  GetPrivateProfileString(iniSectionName, "UseIP", "", forcedIp, sizeof(forcedIp),
                          ".\\Outpost2.ini");
  if (strlen(forcedIp) >= 7) {
    size_t converted = 0;
    mbstowcs_s(&converted, localIp, numof(localIp), forcedIp, _TRUNCATE);
  }

  if (bind) {
    SetBindPatches(true);
  }

  if (upnpMode != noForward) {
    // Do UPnP in its own thread due to unresponsiveness of COM
    DWORD threadId = NULL;
    hFwdThread = CreateThread(NULL, 0, PortForwardTask, NULL, 0, &threadId);
  }
}


DWORD WINAPI PortForwardTask(LPVOID lpParam) {
  if (!localIp[0] && !GetLocalIPAddress(localIp, numof(localIp))) {
    hFwdThread = NULL;
    return 1;
  }

  PortForwarder forwarder;
  DWORD result = 0;
  for (int i = OP2_BEGIN_PORT; i <= OP2_END_PORT; ++i) {
    if ((upnpMode == staticForward &&
         !forwarder.StaticForward('u', i, i, localIp, L"Outpost 2")) ||
        (upnpMode == dynamicForward &&
         !forwarder.DynamicForward(L"*", 'u', i, i, localIp, L"Outpost 2",
                                   leaseSec))) {
      result = 1;
    }
  }
  hFwdThread = NULL;
  return result;
}

DWORD WINAPI PortUnforwardTask(LPVOID lpParam) {
  PortForwarder forwarder;
  DWORD result = 0;
  for (int i = OP2_BEGIN_PORT; i <= OP2_END_PORT; ++i) {
    if ((upnpMode == staticForward  && !forwarder.StaticUnforward('u', i)) ||
        (upnpMode == dynamicForward && !forwarder.DynamicUnforward(L"*", 'u', i))) {
      result = 1;
    }
  }
  return result;
}