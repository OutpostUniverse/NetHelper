

// Implements automatic port forwarding via UPnP and NAT-PMP/PCP
// Built on miniupnpc and libnatpmp

#include <windows.h>
#include <stdio.h>
#include <winsock2.h>
#include <iphlpapi.h>
#include <memory>
#include "PortForward.h"

#include "../miniupnp/miniupnpc/miniwget.h"
#include "../miniupnp/miniupnpc/upnpcommands.h"
#include "../miniupnp/miniupnpc/upnperrors.h"
#include "../miniupnp/miniupnpc/miniupnpcstrings.h"
#include "../libnatpmp/natpmp.h"

static bool GetInterfaceToInternet(in_addr_t *outGateway);
static int ListenForPmpResponse(natpmp_t &natPmp, natpmpresp_t *response = nullptr,
                                int maxTries = 9);

char PortForwarder::internalIp[INET6_ADDRSTRLEN] = {},
     PortForwarder::externalIp[INET6_ADDRSTRLEN] = {};


PortForwarder::PortForwarder() {
  PortForwarder(true, true);
}


PortForwarder::PortForwarder(bool useUpnp, bool usePmp) {
  upnpInited = false;
  pmpInited  = false;

  WSADATA wsaData;
  wsaStarted = WSAStartup(MAKEWORD(2, 2), &wsaData) == NO_ERROR;
  if (!wsaStarted) {
    return;
  }

  memset(&urls, 0, sizeof(urls));
  memset(&data, 0, sizeof(data));

  Initialize(useUpnp, usePmp);
}


PortForwarder::~PortForwarder() {
  if (pmpInited) {
    closenatpmp(&natPmp);
  }
  if (upnpInited) {
    FreeUPNPUrls(&urls);
  }
  if (wsaStarted) {
    WSACleanup();
  }
}


// Adds a new port forward mapping
bool PortForwarder::Forward(bool udp, int externalPort, int internalPort,
                            char *ipAddress, char *description, int duration) {
  // Use NAT-PMP/PCP if it was initialized
  if (pmpInited) {
    // Remove any mapping that already exists for the protocol and port first
    sendnewportmappingrequest(&natPmp, udp ? NATPMP_PROTOCOL_UDP :
      NATPMP_PROTOCOL_TCP, internalPort, 0, 0);
    if (ListenForPmpResponse(natPmp) == NATPMP_TRYAGAIN) {
      return false;
    }

    // Request the new port mapping
    if (sendnewportmappingrequest(&natPmp, udp ? NATPMP_PROTOCOL_UDP :
        NATPMP_PROTOCOL_TCP, internalPort, externalPort, duration) != 12) {
      return false;
    }

    // Listen for response and test if the correct ports were mapped
    natpmpresp_t response;
    if (ListenForPmpResponse(natPmp, &response) != 0) {
      return false;
    }
    if (response.pnu.newportmapping.mappedpublicport != externalPort ||
        response.pnu.newportmapping.privateport      != internalPort) {
      // Wrong ports mapped, delete the rule
      sendnewportmappingrequest(&natPmp, udp ? NATPMP_PROTOCOL_UDP :
        NATPMP_PROTOCOL_TCP, response.pnu.newportmapping.privateport, 0, 0);
      ListenForPmpResponse(natPmp);
      return false;
    }
    return true;
  }
  else if (!upnpInited) {
    return false;
  }

  if (!ipAddress || strlen(ipAddress) < 7) {
    if (!internalIp[0]) {
      return false;
    }
    ipAddress = internalIp;
  }

  char *protocol = udp ? "UDP" : "TCP",
       ePort[11],
       iPort[11],
       time[11];
  if (sprintf_s(ePort, sizeof(ePort), "%i", externalPort) < 0 ||
      sprintf_s(iPort, sizeof(iPort), "%i", internalPort) < 0 ||
      sprintf_s(time,  sizeof(time),  "%i", duration)     < 0) {
    return false;
  }

  // Remove any mapping that already exists for the protocol and port first
  UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, ePort, protocol,
    nullptr);

  // Add the new mapping
  return UPNP_AddPortMapping(urls.controlURL, data.first.servicetype, ePort,
    iPort, ipAddress, description, protocol, 0, time) == UPNPCOMMAND_SUCCESS;
}


// Removes a port forward mapping.
// For UPnP, port is external port. For NAT-PMP/PCP, port is internal port.
bool PortForwarder::Unforward(bool udp, int port) {
  // Use NAT-PMP/PCP if it was initialized
  if (pmpInited) {
    // Request to remove the specified mapping
    if (sendnewportmappingrequest(&natPmp, udp ? NATPMP_PROTOCOL_UDP :
          NATPMP_PROTOCOL_TCP, port, 0, 0) != 12 ||
        ListenForPmpResponse(natPmp) != 0) {
      return false;
    }
  }
  else if (!upnpInited) {
    return false;
  }

  char *protocol = udp ? "UDP" : "TCP",
       ePort[11];
  if (sprintf_s(ePort, sizeof(ePort), "%i", port) < 0) {
    return false;
  }

  // Request to remove the specified mapping
  return UPNP_DeletePortMapping(urls.controlURL, data.first.servicetype, ePort,
    protocol, nullptr) == UPNPCOMMAND_SUCCESS;
}


// Initialize NAT-PMP/PCP or UPnP
bool PortForwarder::Initialize(bool useUpnp, bool usePmp) {
  if (pmpInited || upnpInited) {
    return true;
  }

  // Try NAT-PMP/PCP first. If it succeeds, use that and ignore UPnP
  if (usePmp && !pmpInited) {
    in_addr_t gateway = NULL;
    bool forceGateway = GetInterfaceToInternet(&gateway);
    if (initnatpmp(&natPmp, forceGateway, gateway) == 0 &&
        sendpublicaddressrequest(&natPmp) == 2) {
      // Try to communicate via NAT-PMP/PCP packets
      natpmpresp_t response;
      int error = ListenForPmpResponse(natPmp, &response, useUpnp ? 2 : 9);

      // Successfully initialized NAT-PMP/PCP, store external IP
      if (!error) {
        if (!externalIp[0]) {
          strcpy_s(externalIp, sizeof(externalIp),
                   inet_ntoa(response.pnu.publicaddress.addr));
        }
        return (pmpInited = true);
      }
    }
  }

  if (useUpnp && !upnpInited) {
    // Get list of UPnP devices, then find the IGD, internal IP, and external IP
    int error = 0;
    bool result = false;
    UPNPDev *devices = upnpDiscover(2000, nullptr, nullptr, 0, false, 2, &error);
    if (!devices) {
      return false;
    }
    if (UPNP_GetValidIGD(devices, &urls, &data, internalIp, sizeof(internalIp))) {
      if (!externalIp[0]) {
        UPNP_GetExternalIPAddress(urls.controlURL, data.first.servicetype,
                                  externalIp);
      }
      upnpInited = true;
    }
    freeUPNPDevlist(devices);

    if (!upnpInited && !pmpInited && usePmp) {
      // Try initializing NAT-PMP/PCP again, with more retry attempts this time
      return Initialize(false, true);
    }
  }

  return pmpInited || upnpInited;
}


bool PortForwarder::IsUsingUpnp() {
  return upnpInited;
}


bool PortForwarder::IsUsingPmp() {
  return pmpInited;
}


// Obtains the adapter interface that reaches the internet, get its gateway, and
// store local IP. (Libnatpmp's built-in gateway detection is broken in WINE)
static bool GetInterfaceToInternet(in_addr_t *outGateway) {
  if (!outGateway) {
    return false;
  }

  // Get the best network interface for 0.0.0.0
  DWORD bestInterfaceIndex = NULL;
  if (GetBestInterface(ADDR_ANY, &bestInterfaceIndex) != NO_ERROR) {
    return false;
  }

  // Request list of adapters
  DWORD numAdapters = 1;
  GetNumberOfInterfaces(&numAdapters);

  ULONG bufLen  = sizeof(IP_ADAPTER_INFO) * numAdapters,
        error   = NO_ERROR,
        resizes = 0;
  std::unique_ptr<BYTE[]> infos;
  do {
    infos.reset(new BYTE[bufLen]);
    if (!infos) {
      return false;
    }

    error = GetAdaptersInfo(reinterpret_cast<IP_ADAPTER_INFO*>(infos.get()), &bufLen);
    ++resizes;
  } while (error == ERROR_BUFFER_OVERFLOW && resizes < 3);

  // Enumerate through adapters and get the one with the index we're looking for
  if (error == NO_ERROR) {
    for (auto *curAdapter = reinterpret_cast<IP_ADAPTER_INFO*>(infos.get());
         curAdapter != nullptr; curAdapter = curAdapter->Next) {
      if (curAdapter->Index == bestInterfaceIndex) {
        if (!PortForwarder::internalIp[0]) {
          strcpy_s(PortForwarder::internalIp, sizeof(PortForwarder::internalIp),
                   curAdapter->IpAddressList.IpAddress.String);
        }
        *outGateway = inet_addr(curAdapter->GatewayList.IpAddress.String);
        return true;
      }
    }
  }

  return false;
}


static int ListenForPmpResponse(natpmp_t &natPmp, natpmpresp_t *response,
                                int maxTries) {
  natpmpresp_t resp;
  if (!response) {
    response = &resp;
  }

  fd_set fds;
  int error,
      tries = 0;
  timeval timeout;
  do {
    FD_ZERO(&fds);
    FD_SET(natPmp.s, &fds);

    getnatpmprequesttimeout(&natPmp, &timeout);
    error = select(FD_SETSIZE, &fds, nullptr, nullptr, &timeout);
    if (error < 0) {
      return error;
    }
    error = readnatpmpresponseorretry(&natPmp, response);
    ++tries;
  } while (error == NATPMP_TRYAGAIN && tries < maxTries);
  return error;
}