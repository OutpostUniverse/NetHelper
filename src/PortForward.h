

#ifndef PORTFORWARD_H
#define PORTFORWARD_H

#include <Natupnp.h>
#include <UPnP.h>

class PortForwarder {
public:
  PortForwarder();
  ~PortForwarder();

  bool StaticForward(char protocol, int externalPort, int internalPort,
                     wchar_t *ipAddress, wchar_t *description);
  bool StaticUnforward(char protocol, int externalPort);

  bool DynamicForward(wchar_t *remoteHost, char protocol, int externalPort,
                      int internalPort, wchar_t *ipAddress, wchar_t *description,
                      int duration);
  bool DynamicUnforward(wchar_t *remoteHost, char protocol, int externalPort);

private:
  // COM interfaces
  IUPnPNAT                      *nat;
  IStaticPortMappingCollection  *staticCollection;
  IDynamicPortMappingCollection *dynamicCollection;

  bool Initialize();

  bool StaticAdd(char protocol, int externalPort, int internalPort,
                 wchar_t *ipAddress, wchar_t *description);
  bool StaticRemove(char protocol, int externalPort);

  bool DynamicAdd(wchar_t *remoteHost, char protocol, int externalPort,
                  int internalPort, wchar_t *ipAddress, wchar_t *description,
                  int duration);
  bool DynamicRemove(wchar_t *remoteHost, char protocol, int externalPort);
};

bool GetLocalIPAddress(wchar_t *out, size_t outLen);

#endif