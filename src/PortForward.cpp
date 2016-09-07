

// NOTE: Code for dynamic port forwarding is written here, but it is currently
// not yet supported by Microsoft's UPnP NAT API.

#include <windows.h>
#include <iphlpapi.h>
#include "PortForward.h"

#define numof(array) (sizeof(array) / sizeof(array[0]))


PortForwarder::PortForwarder() {
  nat               = NULL;
  staticCollection  = NULL;
  dynamicCollection = NULL;
}


PortForwarder::~PortForwarder() {
  if (dynamicCollection) {
    dynamicCollection->Release();
  }
  if (staticCollection) {
    staticCollection->Release();
  }
  if (nat) {
    nat->Release();
  }
  CoUninitialize();
}


bool PortForwarder::StaticForward(char protocol, int externalPort, int internalPort,
                                  wchar_t *ipAddress, wchar_t *description) {
  // Connect to the COM object
  if ((!nat || !staticCollection) && (!Initialize() || !staticCollection)) {
    return false;
  }

  // Remove any mapping the router has for the given protocol and port
  StaticRemove(protocol, externalPort);

  // Add a new port mapping
  return StaticAdd(protocol, externalPort, internalPort, ipAddress, description);
}


bool PortForwarder::StaticUnforward(char protocol, int externalPort) {
  // Connect to the COM object
  if ((!nat || !staticCollection) && (!Initialize() || !staticCollection)) {
    return false;
  }

  // Remove any mapping the router has for the given protocol and port
  return StaticRemove(protocol, externalPort);
}


bool PortForwarder::DynamicForward(wchar_t *remoteHost, char protocol,
                                   int externalPort, int internalPort,
                                   wchar_t *ipAddress, wchar_t *description,
                                   int duration) {
  // Connect to the COM object
  if ((!nat || !dynamicCollection) && (!Initialize() || !dynamicCollection)) {
    return false;
  }

  // Remove any mapping the router has for the given protocol and port
  DynamicRemove(remoteHost, protocol, externalPort);

  // Add a new port mapping
  return DynamicAdd(remoteHost, protocol, externalPort, internalPort,
                    ipAddress, description, duration);
}


bool PortForwarder::DynamicUnforward(wchar_t *remoteHost, char protocol,
                                     int externalPort) {
  // Connect to the COM object
  if ((!nat || !dynamicCollection) && (!Initialize() || !dynamicCollection)) {
    return false;
  }

  // Remove any mapping the router has for the given protocol and port
  return DynamicRemove(remoteHost, protocol, externalPort);
}


bool PortForwarder::Initialize() {
  if (FAILED(CoInitialize(NULL))) {
    return false;
  }

  // Connect to the IUPnPNAT COM interface
  if (!nat) {
    if (FAILED(CoCreateInstance(__uuidof(UPnPNAT), NULL, CLSCTX_ALL,
                                __uuidof(IUPnPNAT), (void **)&nat)) || !nat) {
       return false;
    }
  }

  // Get the collection of static forwarded ports
  if (!staticCollection) {
    nat->get_StaticPortMappingCollection(&staticCollection);
  }

  // Get the collection of dynamic forwarded ports
  // ** This will always fail with the current verison of the UPnP NAT API
  if (!dynamicCollection) {
    nat->get_DynamicPortMappingCollection(&dynamicCollection);
  }

  return true;
}


bool PortForwarder::StaticAdd(char protocol, int externalPort, int internalPort,
                              wchar_t *ipAddress, wchar_t *description) {
  BSTR prot = (protocol == 't') ? SysAllocString(L"TCP") : SysAllocString(L"UDP"),
       ip   = SysAllocString(ipAddress),
       desc = SysAllocString(description);

  IStaticPortMapping *mapping = NULL;
  HRESULT result = staticCollection->Add(
    externalPort,
    prot,
    internalPort,
    ip,
    true,
    desc,
    &mapping);

  if (mapping) {
    mapping->Release();
  }
  SysFreeString(prot);
  SysFreeString(ip);
  SysFreeString(desc);

  return !(FAILED(result) || !mapping);
}


bool PortForwarder::StaticRemove(char protocol, int externalPort) {
  BSTR prot = (protocol == 't') ? SysAllocString(L"TCP") : SysAllocString(L"UDP");

  HRESULT result = staticCollection->Remove(externalPort, prot);

  SysFreeString(prot);

  return !(FAILED(result));
}


bool PortForwarder::DynamicAdd(wchar_t *remoteHost, char protocol, int externalPort,
                               int internalPort, wchar_t *ipAddress,
                               wchar_t *description, int duration) {
  BSTR prot = (protocol == 't') ? SysAllocString(L"TCP") : SysAllocString(L"UDP"),
       rmt  = SysAllocString(remoteHost),
       ip   = SysAllocString(ipAddress),
       desc = SysAllocString(description);

  IDynamicPortMapping *mapping = NULL;
  HRESULT result = dynamicCollection->Add(
    rmt,
    externalPort,
    prot,
    internalPort,
    ip,
    true,
    desc,
    duration,
    &mapping);

  if (mapping) {
    mapping->Release();
  }
  SysFreeString(prot);
  SysFreeString(rmt);
  SysFreeString(ip);
  SysFreeString(desc);

  return !(FAILED(result) || !mapping);
}


bool PortForwarder::DynamicRemove(wchar_t *remoteHost, char protocol,
                                  int externalPort) {
  BSTR prot = (protocol == 't') ? SysAllocString(L"TCP") : SysAllocString(L"UDP"),
       rmt  = SysAllocString(remoteHost);

  HRESULT result = dynamicCollection->Remove(rmt, externalPort, prot);

  SysFreeString(prot);
  SysFreeString(rmt);

  return !(FAILED(result));
}


// Obtains the local IP address of the adapter whose gateway is the UPnP NAT device
// ** TODO Could maybe integrate this into PortForwarder class
// ** FIXME Only works intelligently if UPnP device has a Presentation URL assigned
//          There's probably simpler and more reliable ways of doing this
bool GetLocalIPAddress(wchar_t *out, size_t outLen) {
  if (FAILED(CoInitialize(NULL))) {
    return false;
  }

  // Initialize the device finder COM interface
  IUPnPDeviceFinder *finder = NULL;
  if (FAILED(CoCreateInstance(CLSID_UPnPDeviceFinder, NULL, CLSCTX_ALL,
      IID_IUPnPDeviceFinder, (void**)&finder)) || !finder) {
    CoUninitialize();
    return false;
  }

  // Find UPnP devices of type InternetGatewayDevice
  BSTR typeStr =
    SysAllocString(L"urn:schemas-upnp-org:device:InternetGatewayDevice:1");
  IUPnPDevices *devices = NULL;
  HRESULT result = finder->FindByType(typeStr, 0, &devices);
  SysFreeString(typeStr);
  if (FAILED(result) || !devices) {
    finder->Release();
    CoUninitialize();
    return false;
  }

  IUPnPDevice *gateway = NULL;
  IUnknown *unk = NULL;
  result = S_OK;
  if (SUCCEEDED(devices->get__NewEnum(&unk)) && unk) {
    IEnumVARIANT *enumVar = NULL;
    result = unk->QueryInterface(IID_IEnumVARIANT, (void**)&enumVar);
    if (SUCCEEDED(result)) {
      VARIANT curDevice;
      VariantInit(&curDevice);
      enumVar->Reset();

      // Enumerate through collection, stop at first successful hit
      while (enumVar->Next(1, &curDevice, NULL) == S_OK) {
        IDispatch *deviceDispatch = V_DISPATCH(&curDevice);
        result = deviceDispatch->QueryInterface(IID_IUPnPDevice, (void**)&gateway);
        VariantClear(&curDevice);
        if (SUCCEEDED(result)) {
          break;
        }
      }
      enumVar->Release();
    }
    unk->Release();
  }
  finder->Release();

  if (!gateway) {
    devices->Release();
    CoUninitialize();
    return false;
  }

  BSTR presentationUrl = NULL;
  gateway->get_PresentationURL(&presentationUrl);

  // Request list of network adapters' information
  ULONG bufLen = sizeof(IP_ADAPTER_INFO);
  PIP_ADAPTER_INFO adapterInfo = (IP_ADAPTER_INFO*)(new char[bufLen]);

  result = S_FALSE;
  // Do an initial call just to get the required size of the buffer
  if (GetAdaptersInfo(adapterInfo, &bufLen) == ERROR_BUFFER_OVERFLOW) {
    delete [] adapterInfo;
    adapterInfo = (IP_ADAPTER_INFO*)(new char[bufLen]);
  }
  if (GetAdaptersInfo(adapterInfo, &bufLen) == NO_ERROR) {
    PIP_ADAPTER_INFO curAdapter   = adapterInfo,
                     firstAdapter = NULL;

    wchar_t gatewayIp[46];
    size_t converted;

    // Enumerate through network adapters
    while (curAdapter) {
      // Skip disconnected, Hamachi, Tunngle, Evolve, and Virtual/VPN/TAP adapters
      if (strlen(curAdapter->IpAddressList.IpAddress.String) >= 7            &&
          strcmp(curAdapter->IpAddressList.IpAddress.String, "0.0.0.0") != 0 &&
          !strstr(curAdapter->Description, "Hamachi")                        &&
          !strstr(curAdapter->Description, "Tunngle")                        &&
          !strstr(curAdapter->Description, "Evolve ")                        &&
          !strstr(curAdapter->Description, "TAP-")                           &&
          !strstr(curAdapter->Description, "VPN")                            &&
          !(strstr(curAdapter->Description, "Virtual")                       &&
            !strstr(curAdapter->Description, "Microsoft Virtual WiFi"))) {

        PIP_ADDR_STRING curGateway = &curAdapter->GatewayList;
        // Enumerate through current adapter's gateway list
        while (curGateway) {
          if (strlen(curGateway->IpAddress.String) >= 7 &&
              strcmp(curGateway->IpAddress.String, "0.0.0.0") != 0) {
            // Store first valid adapter in case presentation URL doesn't match any
            if (!firstAdapter) {
              firstAdapter = curAdapter;
            }

            converted = 0;
            mbstowcs_s(&converted, gatewayIp, numof(gatewayIp),
                       curGateway->IpAddress.String, _TRUNCATE);

            // Test if the gateway IP matches the UPnP device. If no presentation
            // URL, default to first adapter
            if (!presentationUrl || wcslen(presentationUrl) < 7 ||
                wcsstr(presentationUrl, gatewayIp)) {
              converted = 0;
              mbstowcs_s(&converted, out, outLen,
                         curAdapter->IpAddressList.IpAddress.String, _TRUNCATE);
              result = S_OK;
              break;
            }
          }
          curGateway = curGateway->Next;
        }
      }
      if (result == S_OK) {
        break;
      }
      curAdapter = curAdapter->Next;
    }
    // Presentation URL was probably invalid, just take the primary adapter's IP
    if (result == S_FALSE && firstAdapter) {
      converted = 0;
      mbstowcs_s(&converted, out, outLen,
                 firstAdapter->IpAddressList.IpAddress.String, _TRUNCATE);
      result = S_OK;
    }
  }

  if (adapterInfo) {
    delete [] adapterInfo;
  }
  gateway->Release();
  devices->Release();
  CoUninitialize();

  return result == S_OK;
}