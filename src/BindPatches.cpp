

// Hooks the TCP/IP net transport layer to bind to all network adapters.

#include <windows.h>
#include <winsock2.h>
#include "Patcher.h"


int __stdcall BindWrapper(SOCKET s, sockaddr_in *name, int namelen) {
  name->sin_addr.s_addr = INADDR_ANY;
  return bind(s, (sockaddr*)name, namelen);
}


bool SetBindPatches(bool enable) {
  static _Patch *unknown1    = NULL,
                *unknown2    = NULL,
                *unknown3    = NULL,
                *findSession = NULL,
                *setupSocket = NULL,
                *hostGame    = NULL,
                *joinGame    = NULL;

  if (enable) {
    if (!(unknown1    || (unknown1    = Patcher::PatchFunctionCall(
                          (void*)OP2Addr(0x48C0FE), &BindWrapper, false))) ||
        !(unknown2    || (unknown2    = Patcher::PatchFunctionCall(
                          (void*)OP2Addr(0x48C12B), &BindWrapper, false))) ||
        !(unknown3    || (unknown3    = Patcher::PatchFunctionCall(
                          (void*)OP2Addr(0x48C700), &BindWrapper, false))) ||
        !(findSession || (findSession = Patcher::PatchFunctionCall(
                          (void*)OP2Addr(0x49165C), &BindWrapper, false))) ||
        !(setupSocket || (setupSocket = Patcher::PatchFunctionCall(
                          (void*)OP2Addr(0x495F69), &BindWrapper, false))) ||
        !(hostGame    || (hostGame    = Patcher::PatchFunctionCall(
                          (void*)OP2Addr(0x4960F5), &BindWrapper, false))) ||
        !(joinGame    || (joinGame    = Patcher::PatchFunctionCall(
                          (void*)OP2Addr(0x4964DA), &BindWrapper, false)))) {
      SetBindPatches(false);
      return false;
    }
  }
  else {
    if (unknown1) {
      unknown1->Unpatch(true);
      unknown1 = NULL;
    }
    if (unknown2) {
      unknown2->Unpatch(true);
      unknown2 = NULL;
    }
    if (unknown3) {
      unknown3->Unpatch(true);
      unknown3 = NULL;
    }
    if (findSession) {
      findSession->Unpatch(true);
      findSession = NULL;
    }
    if (setupSocket) {
      setupSocket->Unpatch(true);
      setupSocket = NULL;
    }
    if (hostGame) {
      hostGame->Unpatch(true);
      hostGame = NULL;
    }
    if (joinGame) {
      joinGame->Unpatch(true);
      joinGame = NULL;
    }
  }

  return true;
}