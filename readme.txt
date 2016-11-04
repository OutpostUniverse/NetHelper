=========================
Outpost 2 NetHelper 1.5.2
by Arklon
10/31/2016
=========================

=====
ABOUT
=====
Adds automatic port forwarding for direct play using UPnP or NAT-PMP/PCP. It also
changes the net code to bind to all network adapters instead of just binding to
the primary one, so the "Hamachi fix" is no longer necessary to use it or other
VPNs and fixes issues with multiplayer when running the game under WINE. It is
compatible with NetFix, but may be used as a substitute for it. NetFix's reliability
might even improve with the port forwarding.

UPnP or NAT-PMP/PCP must be enabled on your router for the port forwarding to work;
most routers have one turned on by default. Note that it may take several seconds
after game load for the port forwarding rules to be applied.

Do not try to have people connect to a game with a mix of both direct connections
and Hamachi or other VPN connections; every player should be connected via the same
means.

==================
INSTALLATION/USAGE
==================
Extract NetHelper.dll to [Outpost 2 folder]\NetHelper\NetHelper.dll.
Then, open [Outpost 2 folder]\outpost2.ini, and change this line under [Game]:

LoadAddons = "NetFix"

To:

LoadAddons = "NetFix, NetHelper"

Or if the "LoadAddons" line does not exist, add:

LoadAddons = "NetHelper"

And then add to the bottom of outpost2.ini:

[NetHelper]
Dll = "NetHelper\NetHelper.dll"
BindAll = 1
ForwardMode = 1
LeaseSec = 86400

If port forwarding or joining games is not working for you:
- Try switching around which player is the host of the game.
- If using UPnP, try setting LeaseSec to 0. Note that some newer routers actually
  forbid a lease time of 0 (unlimited/static) for UPnP, but many older routers fail
  with a non-0 lease time. Do not use 0 for NAT-PMP/PCP (NetHelper will ignore the
  setting and use 24 hours in that case).
- If using NAT-PMP/PCP, add "AllowPMPReset = 1" under the "LeaseSec" line. Note
  that this will request ALL UDP port mappings set via NAT-PMP/PCP to your computer
  to be deleted if an attempt to map a port fails, which may interfere with other
  applications.
- For UPnP, UDP port 1900 and TCP ports 2869 and 5000 need to not be blocked by
  your computer's firewall software. For NAT-PMP/PCP, UDP ports 5350 and 5351 need
  to not be blocked by your computer's firewall.
- Note that DMZ or manually-set port forwarding rules for UDP ports 47776-47807
  cannot be overridden by UPnP or NAT-PMP/PCP.
- With UPnP, if you start another Outpost 2 client on another computer on your LAN,
  note that it will overwrite the port mappings of the previous client.
- With NAT-PMP/PCP, note that if ports are actively forwarded to another client,
  they cannot be overridden until they expire or the other client requests to delete
  them. In this case, if ForwardMode is set to 1, it will attempt to use UPnP which
  can override the other client's mappings.
- If your network is a multiple router/repeater/etc. setup, ensure that only the
  root router (i.e. the one that directly connects to the internet) has UPnP or
  NAT-PMP/PCP enabled; if it's enabled on an intermediate router, it may try to
  request the forwarding rules to be set on the wrong device, which will cause it
  to not work.

If the net code changes somehow cause issues for you, set BindAll to 0.

If you do not wish to use automatic port forwarding, set ForwardMode to 0.
ForwardMode's default setting of 1 means try NAT-PMP/PCP first, and if that fails
try UPnP. To use UPnP only, set ForwardMode to 2. To use NAT-PMP/PCP only, set
ForwardMode to 3.

The value for LeaseSec is in seconds. The default value of 86400 is therefore 24
hours. The maximum value allowed for UPnP is 604800 (7 days). NAT-PMP/PCP does not
have a standard defined maximum value, and may vary between devices.

If you really want to, you can override the ports to be forwarded by adding the
lines "StartPort = ###" and "EndPort = ###", but it is recommended to just leave
these at their implied defaults (47776 and 47807).

=========
CHANGELOG
=========

1.5.2
- Fixed a minor memory leak.

1.5
- Now deletes port mappings on game exit. This feature requires Outpost 2 1.3.6.

1.4.1
- If ForwardMode is set to 1 and NAT-PMP/PCP is supported but unable to map ports,
  it will now try to fall back to UPnP.
- If LeaseSec is set to 0 and NAT-PMP/PCP is used, the lease will be set to 24 hours.
- If LeaseSec is not set to 0, UPnP is used, and the router indicates the port mapping
  request failed, it will retry using a lease of 0 seconds (unlimited/static).

1.4
- Added support for NAT-PMP/PCP.

1.3
- Replaced Microsoft's UPnP API with MiniUPnP.
- Port forwarding should now work in WINE due to use of the new library.

1.2
- Further adjustments to IP auto detection.

1.1
- Patched OP2's "your local IP address is" message to display your external IP
  instead. (Requires UPnP)
- Implemented better IP auto detection.

1.0
- Initial release.

=======
LICENSE
=======
Copyright (c) 2016.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published
by the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/lgpl.html>.
(A local copy of the license terms is located at </src/COPYING>.)


MiniUPnP and libnatpmp are licensed under the terms of the BSD 3-Clause License.
See </src/miniupnp/LICENSE> or </src/libnatpmp/LICENSE> for details.