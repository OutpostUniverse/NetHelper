

// Hooks the TCP/IP net transport layer to bind to all network adapters.

#include <windows.h>
#include <winsock2.h>
#include "Patcher.h"
#include "PortForward.h"
#include <memory>
#include <vector>

using namespace Patcher;


int __stdcall BindWrapper(SOCKET s, sockaddr_in *name, int namelen) {
  name->sin_addr.s_addr = INADDR_ANY;

  static int (__stdcall *original)(SOCKET,const sockaddr*,int) = nullptr;
  if (!original) {
    original = reinterpret_cast<decltype(original)>(FixPtr(0x4C0E40));
  }
  return original(s, reinterpret_cast<const sockaddr*>(name), namelen);
}


bool SetBindPatches(bool enable) {
  static std::vector<std::shared_ptr<patch>> patches;
  static uintptr_t bindCalls[] = {
    0x48C0FE, 0x48C12B, 0x48C700, 0x49165C, 0x495F69, 0x4960F5, 0x4964DA
  };

  if (enable) {
    if (patches.empty()) {
      std::shared_ptr<patch> curPatch;
      for (int i = 0; i < _countof(bindCalls); ++i) {
        if (!(curPatch = PatchFunctionCall(FixPtr(bindCalls[i]), &BindWrapper))) {
          SetBindPatches(false);
          return false;
        }
        else {
          patches.emplace_back(std::move(curPatch));
        }
      }
    }
  }
  else {
    for (auto it = patches.begin(); it != patches.end(); ++it) {
      Unpatch(*it);
    }
    patches.clear();
  }

  return true;
}


bool __fastcall GetAddressString(void *thisPtr, int, char *buffer, size_t len) {
  if (PortForwarder::externalIp[0]) {
    return strcpy_s(buffer, len, PortForwarder::externalIp) == 0;
  }
  else if (PortForwarder::internalIp[0]) {
    return strcpy_s(buffer, len, PortForwarder::internalIp) == 0;
  }
  else {
    // Fall back to original function
    static bool (__thiscall *original)(void*,char*,size_t) = nullptr;
    if (!original) {
      original = reinterpret_cast<decltype(original)>(FixPtr(0x491400));
    }
    return original(thisPtr, buffer, len);
  }
}


bool SetGetIPPatch(bool enable) {
  static std::shared_ptr<patch> getIpPatch,
                                ipMsgPatch;

  if (enable) {
    if (!(getIpPatch ||
          (getIpPatch = PatchFunctionVirtual(FixPtr(0x4D64F8), FixPtr(0x491400),
                                             &GetAddressString))) ||
        (strcmp(*reinterpret_cast<char**>(FixPtr(0x4E9220)),
                                          "Your local IP address is %s.") == 0 &&
         !(ipMsgPatch ||
           (ipMsgPatch = Patch<char*>(FixPtr(0x4E9220),
                                      "Your IP address is %s."))))) {
      SetGetIPPatch(false);
      return false;
    }
  }
  else {
    if (getIpPatch) {
      Unpatch(getIpPatch);
    }
    if (ipMsgPatch) {
      Unpatch(ipMsgPatch);
    }
  }

  return true;
}