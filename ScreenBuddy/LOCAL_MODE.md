# Quick Guide: Using ScreenBuddy Without External Servers

## Method 1: Local DERP Server (Recommended - Easy Setup)

### Step 1: Install Go (if not already installed)

Download from: https://go.dev/dl/

**Or via winget:**
```cmd
winget install GoLang.Go
```

### Step 2: Start Local DERP Server

```cmd
start_local_derp_server.cmd
```

This will:
- Install Tailscale's DERP server
- Start it on `localhost:8080`
- Keep running until you close the window

### Step 3: Build ScreenBuddy for Local Mode

Edit `ScreenBuddy.c` line 22:
```c
// Change this from 0 to 1:
#define DERPNET_USE_PLAIN_HTTP 1
```

Rebuild:
```cmd
build.cmd
copy ScreenBuddy.exe test_sharing\
copy ScreenBuddy.exe test_viewing\
```

### Step 4: Test

With DERP server running:
```cmd
start_local_test.cmd
```

**Now:**
- ✅ Both instances connect to YOUR local server
- ✅ No data sent to Tailscale
- ✅ Everything stays on your computer
- ✅ Works offline (no internet needed)

---

## Method 2: Toggle Mode at Compile Time

Create two versions:

**Version 1: Local Mode** (no external connection)
```c
#define DERPNET_USE_PLAIN_HTTP 1
```
Build → `ScreenBuddy_Local.exe`

**Version 2: Internet Mode** (uses Tailscale)
```c
#define DERPNET_USE_PLAIN_HTTP 0
```
Build → `ScreenBuddy_Internet.exe`

---

## Method 3: Self-Host DERP on VPS/Server

If you have a Linux server or VPS:

**On your server:**
```bash
# Install
go install tailscale.com/cmd/derper@latest

# Run
~/go/bin/derper -hostname=your-server.com -certmode=letsencrypt
```

**In ScreenBuddy.c:**

Line ~262, change:
```c
// OLD:
WinHttpConnect(HttpSession, L"login.tailscale.com", ...);

// NEW:
WinHttpConnect(HttpSession, L"your-server.com", ...);
```

Or skip DERP map entirely (line ~2030):
```c
lstrcpyA(DerpHostName, "your-server.com");
```

---

## Quick Start Commands

**Start everything for local testing:**
```cmd
REM Terminal 1: Start local DERP server
start_local_derp_server.cmd

REM Terminal 2: Rebuild for local mode
REM (Edit ScreenBuddy.c: Set DERPNET_USE_PLAIN_HTTP to 1)
build.cmd
copy ScreenBuddy.exe test_sharing\
copy ScreenBuddy.exe test_viewing\

REM Terminal 3: Test
start_local_test.cmd
```

---

## Verify Local Mode

When running locally, the connection code will still work the same way:
- Same format: `03a1b2c3d4e5f...`
- Same encryption: End-to-end with Curve25519
- Different routing: Through `localhost:8080` instead of Tailscale

**Check if it's working:**
- DERP server window should show connections
- No internet traffic (check with Wireshark if needed)
- Works even with network cable unplugged

---

## Switching Back to Internet Mode

1. Edit `ScreenBuddy.c` line 22:
   ```c
   #define DERPNET_USE_PLAIN_HTTP 0
   ```

2. Rebuild:
   ```cmd
   build.cmd
   ```

3. Close local DERP server

4. Run normally - connects to Tailscale servers

---

## Comparison

| Mode | DERP Server | Internet Needed | Privacy | Setup |
|------|-------------|-----------------|---------|-------|
| **Local** | localhost:8080 | ❌ No | ✅ 100% private | Run script |
| **Self-Hosted** | Your VPS | ✅ Yes | ✅ Full control | Deploy server |
| **Tailscale** | Public relays | ✅ Yes | ⚠️ Metadata visible | None |

---

## Troubleshooting

**"Cannot connect to DerpNet server"**
- Ensure local DERP server is running
- Check `DERPNET_USE_PLAIN_HTTP` is set to `1`
- Verify no firewall blocking localhost:8080

**"Error downloading DERP map"**
- Expected in local mode
- Code will use hardcoded "localhost" anyway
- Safe to ignore

**Want to use across network (not just one PC)?**
- Run DERP server on one computer
- Change other computers to connect to that IP:
  ```c
  lstrcpyA(DerpHostName, "192.168.1.100"); // IP of DERP server
  ```

---

## Ready to Try?

**Quickest test (5 minutes):**

1. `winget install GoLang.Go` (or download from go.dev)
2. `start_local_derp_server.cmd`
3. Edit ScreenBuddy.c: Set `DERPNET_USE_PLAIN_HTTP` to `1`
4. `build.cmd`
5. `start_local_test.cmd`

Now you're running 100% locally with no external dependencies!
