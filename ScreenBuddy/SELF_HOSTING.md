# Self-Hosted DERP Server Guide

## Overview

Run your own DERP relay server instead of using Tailscale's public infrastructure.

## Option A: Using Tailscale's DERP Server (Recommended)

### Requirements
- Go 1.21+ installed
- Linux/Windows server or VPS
- Domain name (optional, can use IP)
- TLS certificate (Let's Encrypt recommended)

### Installation

**1. Install Go DERP Server:**
```bash
go install tailscale.com/cmd/derper@latest
```

**2. Generate TLS Certificate (Let's Encrypt):**
```bash
sudo apt-get install certbot
sudo certbot certonly --standalone -d derp.yourdomain.com
```

**3. Run DERP Server:**
```bash
# With TLS certificate
derper \
  -hostname=derp.yourdomain.com \
  -certmode=manual \
  -certdir=/etc/letsencrypt/live/derp.yourdomain.com

# Or with self-signed certificate (testing only)
derper -hostname=derp.yourdomain.com -certmode=letsencrypt

# Or plain HTTP (for local network only)
derper -hostname=localhost -http-port=8080
```

**4. Verify Server Running:**
```bash
curl https://derp.yourdomain.com/derp
# Should return: "DERP requires connection upgrade"
```

### Server Configuration

**Port Requirements:**
- Port 443 (HTTPS) - For encrypted DERP traffic
- Port 80 (HTTP) - Optional, for Let's Encrypt renewal

**Firewall Rules:**
```bash
sudo ufw allow 443/tcp
sudo ufw allow 80/tcp
```

### Run as System Service (Linux)

**Create systemd service:**
```bash
sudo nano /etc/systemd/system/derper.service
```

**Service file content:**
```ini
[Unit]
Description=Tailscale DERP Server
After=network.target

[Service]
Type=simple
User=derp
ExecStart=/home/derp/go/bin/derper \
  -hostname=derp.yourdomain.com \
  -certmode=letsencrypt \
  -stun
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

**Enable and start:**
```bash
sudo systemctl daemon-reload
sudo systemctl enable derper
sudo systemctl start derper
sudo systemctl status derper
```

---

## Option B: Modify ScreenBuddy to Use Your DERP Server

### Code Changes Required

**1. Replace DERP Map URL:**

In `ScreenBuddy.c` line ~262, change:
```c
// OLD:
WinHttpConnect(HttpSession, L"login.tailscale.com", INTERNET_DEFAULT_HTTPS_PORT, 0);

// NEW:
WinHttpConnect(HttpSession, L"derp.yourdomain.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
```

**2. Update DERP Map Endpoint:**

Line ~265, change:
```c
// OLD:
WinHttpOpenRequest(HttpConnection, L"GET", L"/derpmap/default", ...);

// NEW (if you want custom map):
WinHttpOpenRequest(HttpConnection, L"GET", L"/derpmap", ...);
```

**3. Create Custom DERP Map:**

Host a JSON file at `https://derp.yourdomain.com/derpmap`:

```json
{
  "Regions": {
    "1": {
      "RegionID": 1,
      "RegionCode": "custom",
      "RegionName": "My Server",
      "Nodes": [
        {
          "Name": "1a",
          "RegionID": 1,
          "HostName": "derp.yourdomain.com",
          "IPv4": "1.2.3.4",
          "DERPPort": 443
        }
      ]
    }
  }
}
```

**4. Or Hardcode Your Server:**

Skip DERP map download entirely:

```c
// In Buddy_StartSharing() around line 2025
char DerpHostName[256];
// OLD: Downloads map and tests all servers
// NEW: Hardcode your server
lstrcpyA(DerpHostName, "derp.yourdomain.com");

if (DerpNet_Open(&Buddy->Net, DerpHostName, &Buddy->MyPrivateKey))
{
    // Rest of code...
}
```

**5. Connection code format stays the same:**
- Still `03a1b2c3d4...` format
- Region ID now always points to your server
- Public key encryption still works

---

## Option C: Local Network Direct Connection (No DERP at All)

Remove DERP completely for LAN-only usage.

### Architecture Change

**Current:** Client ↔ DERP Relay ↔ Server
**New:** Client ↔ Direct TCP ↔ Server

### Major Code Changes Needed

**1. Replace DerpNet with TCP sockets**
**2. Sharing side becomes TCP server**
**3. Viewing side becomes TCP client**
**4. Connection code becomes IP:PORT**

### Implementation Overview

```c
// Sharing side - START TCP SERVER
SOCKET ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
struct sockaddr_in ServerAddr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = INADDR_ANY,
    .sin_port = htons(12345)
};
bind(ListenSocket, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr));
listen(ListenSocket, 1);

// Show IP address in UI instead of DERP code
char MyIP[32];
// Get local IP...
sprintf(ShareCode, "%s:12345", MyIP);

// Accept connection
SOCKET ClientSocket = accept(ListenSocket, NULL, NULL);

// Viewing side - CONNECT TO IP:PORT
SOCKET Socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
struct sockaddr_in ServerAddr = {
    .sin_family = AF_INET,
    .sin_addr.s_addr = inet_addr(ip_from_code),
    .sin_port = htons(port_from_code)
};
connect(Socket, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr));
```

**Major refactoring required:**
- Remove all DerpNet_* function calls
- Replace with send()/recv() socket calls
- Remove public key encryption (or implement manually)
- Add TCP framing/buffering
- Handle connection drops differently

---

## Option D: Use Alternative Relay (TURN/STUN)

Use standard WebRTC infrastructure (TURN/STUN servers).

### Popular Free TURN Servers
- Google STUN: `stun:stun.l.google.com:19302`
- Twilio TURN: `turn:global.turn.twilio.com` (requires account)

### Setup Your Own TURN Server (coturn)

```bash
# Install coturn
sudo apt-get install coturn

# Configure /etc/turnserver.conf
listening-port=3478
fingerprint
lt-cred-mech
user=username:password
realm=yourdomain.com
```

**Pros:** Standard protocol, WebRTC compatible
**Cons:** Different protocol than DERP, major code rewrite

---

## Comparison Matrix

| Solution | Complexity | NAT Traversal | Privacy | Internet Required |
|----------|------------|---------------|---------|-------------------|
| **Tailscale DERP (Current)** | ✅ Easy | ✅ Yes | ⚠️ Metadata visible | ✅ Yes |
| **Self-Hosted DERP** | 🟡 Medium | ✅ Yes | ✅ Full control | ✅ Yes (for server) |
| **Direct TCP (LAN)** | 🟡 Medium | ❌ No | ✅ Completely private | ❌ No |
| **Direct TCP (Internet)** | 🔴 Hard | ❌ No | ✅ Completely private | ✅ Yes + Port Forward |
| **TURN Server** | 🔴 Hard | ✅ Yes | ✅ Full control | ✅ Yes |

---

## Recommended Approach

### For Your Use Case:

**Local Network Testing:**
→ **Option C: Direct TCP** (simplest, no external dependency)

**Private Remote Access:**
→ **Option B: Self-Hosted DERP** (keeps code simple, full privacy)

**Production Use:**
→ **Keep Tailscale DERP** (reliable, maintained, global network)

---

## Quick Win: Local Network Only

Minimal code change for LAN testing without external servers:

### Change Connection Logic

```c
// Add at top of file
#define BUDDY_USE_LOCAL_NETWORK 1  // Toggle this

#if BUDDY_USE_LOCAL_NETWORK
    // Use direct TCP on LAN
    #define CONNECTION_METHOD "Direct TCP"
#else
    // Use DERP relay
    #define CONNECTION_METHOD "DERP Relay"
#endif
```

This way you can switch between modes easily.

---

## Need Help Implementing?

Let me know which approach you prefer:

1. **Self-hosted DERP** - I can modify the code to point to your server
2. **Direct TCP** - I can rewrite networking for LAN-only use
3. **Hybrid** - Both modes selectable via UI

Which direction would you like to go?
