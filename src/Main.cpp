

// Adds UPnP port forwarding and makes the TCP layer bind to all adapters.

#include <windows.h>
#include <memory>
#include "NetPatches.h"
#include "PortForward.h"


DWORD WINAPI PortForwardTask(LPVOID lpParam);

enum fwdMode {
  noForward = 0,
  pmpOrUpnp,
  upnpOnly,
  pmpOnly
} mode = noForward;
bool doPmpReset = false;
int leaseSec  = 0,
    startPort = 47776,
    endPort   = 47807;

std::unique_ptr<PortForwarder> forwarder;
HANDLE hFwdThread = nullptr;
bool shuttingDown = false;


extern "C" __declspec(dllexport) void InitMod(char* iniSectionName) {
  mode = (fwdMode)GetPrivateProfileInt(iniSectionName, "ForwardMode", 1,
                                       ".\\Outpost2.ini");

  if (GetPrivateProfileInt(iniSectionName, "BindAll", 1, ".\\Outpost2.ini")) {
    SetBindPatches(true);
  }

  if (mode != noForward) {
    doPmpReset = GetPrivateProfileInt(iniSectionName, "AllowPMPReset", 0,
                                      ".\\Outpost2.ini") != 0;
    leaseSec   = GetPrivateProfileInt(iniSectionName, "LeaseSec", 24*60*60,
                                      ".\\Outpost2.ini");
    startPort  = GetPrivateProfileInt(iniSectionName, "StartPort", 47776,
                                      ".\\Outpost2.ini");
    endPort    = GetPrivateProfileInt(iniSectionName, "EndPort", 47807,
                                      ".\\Outpost2.ini");

    SetGetIPPatch(true);

    // Do port forwarding in its own thread because of network response delay
    DWORD threadId = NULL;
    hFwdThread = CreateThread(nullptr, 0, PortForwardTask, nullptr, 0, &threadId);
  }
}


extern "C" __declspec(dllexport) bool DestroyMod() {
  bool result = true;

  if (!SetBindPatches(false)) {
    result = false;
  }

  if (mode != noForward) {
    if (!SetGetIPPatch(false)) {
      result = false;
    }

    if (hFwdThread) {
      shuttingDown = true;
      WaitForSingleObject(hFwdThread, INFINITE);
    }

    if (forwarder.get()) {
      for (int i = startPort; i <= endPort; ++i) {
        forwarder->Unforward(true, i);
      }

      forwarder.reset(nullptr);
    }
  }

  return result;
}


DWORD WINAPI PortForwardTask(LPVOID lpParam) {
  forwarder.reset(new PortForwarder(mode == pmpOrUpnp || mode == upnpOnly,
                                    mode == pmpOrUpnp || mode == pmpOnly));

  DWORD result = 0;

  for (int i = startPort; i <= endPort; ++i) {
    if (shuttingDown) {
      return result;
    }

    if (!forwarder->Forward(true, i, i, nullptr, "Outpost 2",
        (forwarder->IsUsingPmp() && leaseSec == 0) ? 24*60*60 : leaseSec)) {
      if (forwarder->IsUsingPmp()) {
        if (doPmpReset) {
          // Request to clear all NAT-PMP/PCP UDP port mappings and retry
          doPmpReset = false;
          forwarder->Unforward(true, 0);

          i = startPort - 1;
          continue;
        }
        else if (mode == pmpOrUpnp) {
          // NAT-PMP/PCP is supported but unable to map ports, retry with UPnP
          forwarder.reset(new PortForwarder(true, false));

          i = startPort - 1;
          continue;
        }
      }
      else if (forwarder->IsUsingUpnp() && leaseSec != 0) {
        // Failed using dynamic forwarding, retry using static forwarding
        leaseSec = 0;

        i = startPort - 1;
        continue;
      }

      result = 1;
      break;
    }
  }

  hFwdThread = nullptr;
  return result;
}