# Local Testing Guide - Running ScreenBuddy on Same PC

## Overview

You can test ScreenBuddy on a single PC by running two instances - one sharing the screen and one viewing it.

## Method 1: Two Separate Instances (Recommended)

### Step 1: Build the Application

```cmd
cd c:\Users\Ed\OneDrive\Documents\Development\ScreenBudy-NG\ScreenBuddy
build.cmd
```

This creates `ScreenBuddy.exe`

### Step 2: Create Two Copies

```cmd
mkdir test_sharing
mkdir test_viewing
copy ScreenBuddy.exe test_sharing\
copy ScreenBuddy.exe test_viewing\
```

This ensures each instance has its own configuration file (stored in the same directory as the .exe).

### Step 3: Start First Instance (Sharing)

1. Run `test_sharing\ScreenBuddy.exe`
2. Wait for initialization (it will fetch DERP regions)
3. Click "Share" button
4. Copy the generated code (looks like: `03a1b2c3d4e5f6...`)

**What happens:**
- Application starts screen capture
- Creates encrypted connection key
- Waits for someone to connect using the code

### Step 4: Start Second Instance (Viewing)

1. Run `test_viewing\ScreenBuddy.exe` (in a separate window)
2. Paste the code from Step 3 into the "Connect" section
3. Click "Connect" button

**What happens:**
- Application connects via Tailscale DERP relay
- Screen sharing begins
- You can now control the first instance's screen from the second window

### Step 5: Test Features

Once connected, you can test:

✅ **Mouse Control:**
- Move mouse in viewer window → cursor moves on shared screen
- Click in viewer window → click happens on shared screen
- Scroll in viewer window → scrolling on shared screen

✅ **Keyboard Input:** (newly implemented)
- Type in viewer window → text appears on shared screen
- Press shortcuts (Ctrl+C, Alt+Tab, etc.)
- Extended keys (arrows, F-keys, numpad)

✅ **Cursor Hiding:** (newly implemented)
- Local cursor hides on sharing side when connected
- Returns when disconnected

✅ **Bidirectional File Transfer:** (newly implemented)
- Drag file to sharing window → sends to viewer
- Drag file to viewer window → sends to sharer
- Accept/reject file transfer dialog appears

✅ **Improved Error Messages:** (newly implemented)
- Connection errors show helpful context
- GPU errors suggest driver updates
- Network errors show troubleshooting tips

## Method 2: Single Executable (Alternative)

If you don't want separate directories, you can run both from the same location, but they'll share the same configuration:

```cmd
start ScreenBuddy.exe
timeout /t 2
start ScreenBuddy.exe
```

## Network Configuration

**No configuration needed!** ScreenBuddy uses Tailscale's DERP relay servers, so:
- Works behind NAT/firewalls
- No port forwarding required
- No router configuration needed
- All traffic is end-to-end encrypted

## Expected Behavior

### Sharing Side (First Instance)
- Window title shows "Screen Buddy"
- "Share" button becomes "Stop" when active
- Status shows connection state
- Can receive file drops when connected
- Cursor hides when someone connects

### Viewing Side (Second Instance)
- Shows remote screen in window
- Window resizes to match remote aspect ratio
- Can control remote screen with mouse/keyboard
- Can drag files to send to remote side
- Connection status shown in title bar

## Testing Checklist

```
[ ] Start sharing instance
[ ] Generate share code
[ ] Start viewing instance  
[ ] Connect using code
[ ] See remote screen displayed
[ ] Move mouse - cursor moves on both sides
[ ] Click mouse - clicks work on remote side
[ ] Type text - keyboard input works
[ ] Test keyboard shortcuts (Ctrl+C, Alt+Tab)
[ ] Test arrow keys and function keys
[ ] Drag file from desktop to sharing window
[ ] Accept file on viewing side
[ ] Verify file received
[ ] Drag file from desktop to viewing window
[ ] Accept file on sharing side
[ ] Verify file received
[ ] Verify cursor hides on sharing side
[ ] Disconnect and verify cursor returns
[ ] Test error messages (invalid code, network issues)
```

## Troubleshooting

### "Cannot determine best DERP region"
- **Cause:** No internet connection or DERP servers unreachable
- **Fix:** Check internet connection, disable VPN if blocking connections

### Connection Timeout
- **Cause:** Firewall blocking outbound HTTPS connections
- **Fix:** Allow ScreenBuddy.exe in Windows Firewall

### "Cannot create GPU encoder"
- **Cause:** GPU doesn't support hardware video encoding or driver issues
- **Fix:** 
  - Update graphics drivers
  - Ensure you have Intel HD Graphics, NVIDIA, or AMD GPU
  - Check GPU supports H.264 encoding

### Screen Not Updating
- **Cause:** Desktop capture permission issues on Windows 11
- **Fix:** Run as administrator or grant screen capture permissions

### Keyboard Input Not Working
- **Cause:** Viewer window doesn't have focus
- **Fix:** Click in the viewer window to ensure it has keyboard focus

### File Transfer Fails
- **Cause:** File too large or path too long
- **Fix:** 
  - Use smaller files for testing
  - Ensure destination has disk space
  - Try shorter file paths

## Performance Tips

For local testing:
- **Lower Bitrate:** Edit `ScreenBuddy.c` line 77: `BUDDY_ENCODE_BITRATE = 2 * 1000 * 1000` (2 Mbps)
- **Lower Framerate:** Edit line 76: `BUDDY_ENCODE_FRAMERATE = 15` (15 FPS)
- **Rebuild:** Run `build.cmd` after changes

## Monitoring Traffic

Both instances will:
- Connect to Tailscale DERP relay (`login.tailscale.com`)
- Exchange encrypted video/input data through relay
- Show connection status in window title

Even though both are on same PC, they communicate via internet relay for realistic testing.

## Configuration Files

Each instance stores settings in its directory:
- **Location:** Same folder as `ScreenBuddy.exe`
- **File:** Hidden `.ini` file with DERP regions and encrypted private key
- **Sharing:** Each instance needs its own folder to have separate configs

## Advanced Testing

### Test with Multiple Monitors
1. Share screen from instance on Monitor 1
2. View on instance on Monitor 2
3. Test monitor switching and resolution handling

### Test Connection Recovery
1. Connect instances
2. Temporarily disable network adapter
3. Re-enable and observe reconnection behavior

### Test Concurrent Connections
- ScreenBuddy currently supports **one connection at a time**
- Starting a second connection will disconnect the first

## Security Note

Even in local testing:
- ✅ All data is **end-to-end encrypted** (using Curve25519)
- ✅ DERP relay cannot decrypt video/input data
- ✅ Connection requires **unique generated key**
- ✅ Keys are **randomly generated** each time
- ✅ Private keys are **encrypted at rest** (using Windows DPAPI)

## Next Steps

After local testing works:
1. Test between two different PCs on same network
2. Test between two PCs on different networks
3. Test with various screen resolutions
4. Test with different GPU vendors (Intel/NVIDIA/AMD)
5. Performance test with demanding applications

## Cleanup

To reset testing environment:

```cmd
cd c:\Users\Ed\OneDrive\Documents\Development\ScreenBudy-NG\ScreenBuddy
rmdir /s /q test_sharing
rmdir /s /q test_viewing
```

## Related Documentation

- `README.md` - Main project documentation
- `ENHANCEMENTS.md` - Details on new features
- `TESTING.md` - Comprehensive test suite guide
- `tests/README.md` - Unit test documentation
