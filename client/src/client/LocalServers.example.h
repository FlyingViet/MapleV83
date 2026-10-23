// Copy to LocalServers.h (gitignored) and fill in the Mac that runs the Cosmic server and data server.
// Without it the client uses 127.0.0.1, or the server IP saved in its settings.
#pragma once

#define MV83_HOME_SERVER "192.0.2.10"       // the Mac's address on your home LAN
#define MV83_TAILSCALE_SERVER "198.51.100.10" // the Mac's Tailscale address (optional; defaults to the LAN one)
