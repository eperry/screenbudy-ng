# ScreenBuddy Troubleshooting Guide

## Connection Timeout Error

**Error Message:** "Timeout while connecting to remote computer!"

### Common Causes and Solutions

#### 1. DERP Region Not Initialized

**Problem:** Trying to connect before initialization completes.

**Solution:**
- Wait for "...initializing..." to disappear in both windows
- Share window should show a code like `03a1b2c3d4e5...`
- Don't click "Connect" until you see a valid code in the Share window

**How to verify:**
- Sharing window: Code field should show 66-character hex code (not "...initializing...")
- Viewing window: Wait 3-5 seconds after launch before pasting code

#### 2. Both Instances Trying to Share

**Problem:** Both windows in "Share" mode instead of one Share + one Connect.

**Solution:**
1. **First window (Sharing):**
   - Click "Share" button
   - Copy the code
   
2. **Second window (Viewing):**
   - Do NOT click "Share"
   - Paste code in "Connect" section
   - Click "Connect" button

#### 3. Invalid Connection Code Format

**Problem:** Extra spaces, incomplete code, or wrong format.

**Valid Code Format:**
- Exactly 66 characters
- First 2 chars: Region (e.g., `03`, `01`, `05`)
- Next 64 chars: Public key (hex)
- Example: `03a1b2c3d4e5f6789...` (66 chars total)

**How to fix:**
- Copy the entire code from Share window
- Check for extra spaces or line breaks
- Paste into Connect window's text field
- Verify 66 characters visible

#### 4. Firewall Blocking HTTPS

**Problem:** Windows Firewall or antivirus blocking outbound HTTPS to Tailscale servers.

**Check:**
```powershell
# Test connectivity to Tailscale
curl https://login.tailscale.com/derpmap/default
```

**Solution:**
- Allow ScreenBuddy.exe in Windows Firewall
- Temporarily disable antivirus
- Check corporate firewall settings
- Verify internet connectivity

#### 5. Both Instances Using Same Configuration

**Problem:** Both instances share the same config file and private key.

**Solution:**
- Ensure instances are in separate directories:
  ```
  test_sharing\ScreenBuddy.exe
  test_viewing\ScreenBuddy.exe
  ```
- Each directory should have its own config file
- Don't run both from the same directory

#### 6. Network Connectivity Issues

**Problem:** No internet or DERP servers unreachable.

**Quick Test:**
```cmd
ping login.tailscale.com
```

**Solution:**
- Verify internet connection
- Try different DERP region (click "Generate New Code")
- Disable VPN temporarily
- Check proxy settings

#### 7. Connection Timeout Too Short

**Fixed in latest build:** Timeout increased from 8 seconds to **30 seconds**.

**If still timing out:**
- Check network latency
- Try generating new code (different DERP region)
- Look for error messages in dialog boxes

### Step-by-Step Connection Process

#### On Sharing Computer:

1. Launch `test_sharing\ScreenBuddy.exe`
2. **Wait** for initialization (3-5 seconds)
3. Verify code field shows 66-character hex code
4. Click **"Share"** button
5. Button changes to "Stop"
6. Copy the entire code
7. Keep window open and wait

#### On Viewing Computer:

1. Launch `test_viewing\ScreenBuddy.exe`
2. **Wait** for initialization (3-5 seconds)
3. Paste code into "Connect" section
4. Verify code is 66 characters
5. Click **"Connect"** button
6. Wait up to 30 seconds
7. Should see remote screen

### Debugging Steps

#### Check if DERP servers are reachable:

```powershell
# Test DERP connectivity
Invoke-WebRequest -Uri "https://login.tailscale.com/derpmap/default" -UseBasicParsing
```

Should return JSON with DERP regions.

#### Check process status:

```powershell
Get-Process ScreenBuddy | Format-Table Id, Path, StartTime
```

Should show two separate processes from different directories.

#### Check for error dialogs:

- Look for popup error messages
- Check Windows Event Viewer: Applications and Services Logs → Windows → Application
- Common errors:
  - "Cannot create GPU encoder" → Update graphics drivers
  - "Cannot determine best DERP region" → Network issue
  - "Incorrect length for connection code" → Copy/paste error

#### Verify GPU support:

```powershell
# Check if GPU supports H.264 encoding
dxdiag
```

Look for:
- DirectX 11 support
- Hardware acceleration enabled
- Display drivers up to date

### Network Requirements

ScreenBuddy requires:
- ✅ Outbound HTTPS (port 443) access
- ✅ Access to `*.tailscale.com` domains
- ✅ No incoming ports (works through NAT)
- ❌ Does NOT require port forwarding
- ❌ Does NOT require VPN (but works with one)

### Connection States

```
INITIAL → SHARE_STARTED → SHARING → CONNECTED
                  ↓
              (waiting for viewer)

INITIAL → CONNECTING → CONNECTED
              ↓
          (timeout after 30s if no response)
```

### Timeout Values

- **Connection timeout:** 30 seconds
- **DERP region discovery:** 10 seconds per region
- **File transfer:** No timeout (progress-based)

### Common Error Messages

#### "Incorrect length for connection code!"
- Code must be exactly 66 characters
- Check for spaces or missing characters
- Re-copy from Share window

#### "Cannot determine best DERP region!"
- No internet connection
- Firewall blocking HTTPS
- Tailscale servers unreachable
- **Fix:** Check network, try again

#### "Cannot create GPU encoder!"
- GPU doesn't support H.264 encoding
- Graphics drivers outdated
- **Fix:** Update drivers, check GPU compatibility

#### "DerpNet disconnect while sending data!"
- Network connection lost
- DERP server disconnected
- Too much packet loss
- **Fix:** Check network stability, try different region

### Testing Checklist

Before reporting issues, verify:

```
[ ] Both instances launched from separate directories
[ ] Both instances initialized (no "...initializing..." text)
[ ] Valid 66-character code displayed
[ ] Only ONE instance clicked "Share"
[ ] Only ONE instance clicked "Connect"
[ ] Internet connectivity working (can browse web)
[ ] Firewall allows ScreenBuddy.exe
[ ] Windows Defender not blocking
[ ] Graphics drivers up to date
[ ] Waited full 30 seconds for connection
```

### Advanced Diagnostics

#### Enable Debug Build:

Edit `build.cmd` and remove `/DNDEBUG` flag to enable debug logging.

#### Check Configuration File:

Configuration stored in same directory as executable:
- Look for hidden `.ini` file
- Contains encrypted private key and DERP regions
- Delete to force regeneration

#### Test with Different DERP Regions:

1. In Share window, click "Generate New Code" multiple times
2. Each click selects current best region
3. Try different codes with different region numbers
4. Some regions may be faster/more reliable

### Performance Issues

If connected but laggy:

- **Lower Bitrate:** Default is 4 Mbps, try 2 Mbps
- **Lower Framerate:** Default is 30 FPS, try 15 FPS
- **Check Network:** Use speedtest.net to verify bandwidth
- **Check CPU/GPU:** Task Manager → Performance tab
- **Try Different Region:** Generate new code

### Still Not Working?

1. **Restart both instances:**
   ```cmd
   taskkill /F /IM ScreenBuddy.exe
   ```

2. **Delete config files:**
   ```cmd
   del test_sharing\*.ini
   del test_viewing\*.ini
   ```

3. **Rebuild application:**
   ```cmd
   build.cmd
   copy ScreenBuddy.exe test_sharing\
   copy ScreenBuddy.exe test_viewing\
   ```

4. **Test with different network:**
   - Try mobile hotspot
   - Try different WiFi
   - Check if corporate network blocks Tailscale

5. **Check system requirements:**
   - Windows 10/11 (64-bit)
   - GPU with H.264 hardware encoding support
   - Internet connection
   - Updated graphics drivers

### Getting Help

When reporting issues, include:

1. **Error message** (exact text)
2. **Steps taken** (what you clicked)
3. **Timing** (how long before error)
4. **Network** (home/corporate/VPN)
5. **Windows version** (`winver` command)
6. **GPU model** (from Device Manager)
7. **Connection code format** (first 4 chars only, e.g., "03a1...")

### Quick Fixes Summary

| Problem | Quick Fix |
|---------|-----------|
| Timeout | Wait for initialization, ensure 30 sec timeout |
| Invalid code | Copy entire 66 chars, no spaces |
| Both sharing | One Share, one Connect |
| No internet | Check connectivity to login.tailscale.com |
| GPU error | Update graphics drivers |
| Firewall | Allow ScreenBuddy.exe in Windows Firewall |
| Same config | Use separate directories for each instance |

### Related Files

- `LOCAL_TESTING.md` - Complete local testing guide
- `TESTING.md` - Unit test information
- `ENHANCEMENTS.md` - Feature documentation
- `README.md` - Main documentation
