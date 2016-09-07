=======================
Outpost 2 NetHelper 1.0
by Arklon
9/5/2016
=======================

=====
ABOUT
=====
Adds UPnP automatic port forwarding for direct play, and changes the net code to
bind to all network adapters instead of just binding to the primary one (so the
"Hamachi fix" is no longer necessary to use it). It is compatible with NetFix.

UPnP must be enabled on your router for the port forwarding to work; it is on by
default for most routers. Note that it will take several seconds after game load
for the port forwarding rules to be applied.

Do not try to have people connect to a game with a mix of both direct connections
and Hamachi or other VPN connections; every player should be connected via the
same means.

============
INSTALLATION
============
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
UPnPMode = 1
BindAll = 1

If you do not wish to use UPnP port forwarding, change UPnPMode to 0. If the net
code changes somehow cause issues for you, change BindAll to 0.

=================
ADVANCED SETTINGS
=================
These go under the [NetHelper] section in outpost2.ini.

UseIP = "192.168.x.x"  Forces the port forwarding rules to be made for this LAN
                       IP rather than autodetecting the IP. Try setting this if
		       port forwarding is not working for you.
UPnPMode = 2  Sets port forwarding mode to dynamic. This currently does nothing
              as Microsoft's UPnP API doesn't yet implement it, but NetHelper has
              a code path for it written in preparation. Dynamic port forwards
	      expire after a set amount of time, as opposed to static (UPnPMode 1).
	      Future routers may drop support for static forwarding, so UPnPMode 2
	      may be required at some point.
LeaseSec = #  Only relevant for UPnPMode 2. Duration (in seconds) until the port
              forwarding rules expire. Defaults to 86400 (24 hours).

=======
LICENSE
=======
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