

#ifndef PORTFORWARD_H
#define PORTFORWARD_H

#include "../miniupnp/miniupnpc/miniupnpc.h"
#include "../libnatpmp/natpmp.h"

class PortForwarder {
public:
  PortForwarder();
  PortForwarder(bool useUpnp, bool usePmp);
  ~PortForwarder();

  bool Forward(bool udp, int externalPort, int internalPort, char *ipAddress,
               char *description, int duration);
  bool Unforward(bool udp, int port);

  bool Initialize(bool useUpnp, bool usePmp);

  bool IsUsingUpnp();
  bool IsUsingPmp();

  static char internalIp[46],
              externalIp[46];

private:
  bool upnpInited,
       pmpInited;
  UPNPUrls urls;
  IGDdatas data;
  natpmp_t natPmp;
  bool wsaStarted;
};

#endif