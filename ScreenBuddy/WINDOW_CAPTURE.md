# Window Capture Feature

## Overview

ScreenBuddy now supports capturing individual application windows in addition to full-screen capture. When starting a screen sharing session, you can choose between:

- **Full Screen Mode** - Captures the entire primary monitor (original behavior)
- **Window Mode** - Captures a specific application window

## How to Use

### Starting a Share Session

1. Click the "Share" button in the main ScreenBuddy dialog
2. A selection dialog will appear with two options:
   - **Yes** = Capture Full Screen
   - **No** = Select Specific Window
   - **Cancel** = Cancel sharing

### Full Screen Capture

- Select "Yes" to capture the entire primary monitor
- All content on your screen will be visible to the viewer
- This is the original ScreenBuddy behavior

### Window Capture

- Select "No" to capture a specific window
- ScreenBuddy will enumerate all visible windows with titles
- Currently, the first available window is automatically selected
- A confirmation message shows which window will be captured
- Only the content of that specific window will be shared

## Features

### Window Lifecycle Handling

- **Window Closed**: If the captured window is closed during sharing, the session automatically disconnects
- **Window Minimized**: Capture continues when window is minimized (may show last frame or black screen depending on OS)
- **Window Resizing**: The capture automatically handles window resize events

### Configuration Persistence

- Your last selected capture mode (Full Screen vs Window) is saved to the INI file
- Setting: `CaptureFullScreen=0` (Window mode) or `CaptureFullScreen=1` (Full screen mode)
- Default is Full Screen mode

### Window Enumeration

The window selection process:
- Enumerates all top-level visible windows
- Filters out invisible windows and system windows (e.g., "Program Manager")
- Captures window titles and associated process names
- Supports up to 256 windows

## Technical Implementation

### API Used

- **Windows API**: `EnumWindows` for window enumeration
- **Screen Capture API**: `ScreenCapture_CreateForWindow` for window-specific capture
- **Window Information**: `GetWindowText`, `GetWindowThreadProcessId`, `QueryFullProcessImageName`

### Configuration

Window capture settings are stored in `ScreenBuddy.ini`:

```ini
[Buddy]
CaptureFullScreen=0  ; 0 = Window mode, 1 = Full screen mode
```

### Capture Modes

#### Full Screen (Monitor Capture)
```c
ScreenCapture_CreateForMonitor(&Buddy->Capture, Buddy->Device, Monitor, NULL)
```

#### Window Capture
```c
ScreenCapture_CreateForWindow(&Buddy->Capture, Buddy->Device, Window, 
                             false,  // OnlyClientArea
                             true)   // DisableRoundedCorners
```

## Future Enhancements

### Planned Features

1. **Enhanced Window Selection Dialog**
   - Custom dialog with list of all available windows
   - Real-time filter/search functionality
   - Window thumbnails
   - Sort by recently active or alphabetically

2. **Window Selection UI Elements**
   - Filter text box for searching windows by title or process name
   - ListBox displaying "Window Title - [ProcessName.exe]"
   - Buttons: "Full Screen", "Select Window", "Cancel"

3. **Advanced Options**
   - Client area only capture (exclude title bar and borders)
   - Remember last selected window per session
   - Auto-reselect window if it's reopened

4. **Multi-Monitor Support**
   - Select specific monitor for full-screen capture
   - Window capture works across multiple monitors

## Keyboard Shortcuts

Currently, window selection is via dialog only. Future versions may include:
- Quick toggle between last window and full screen
- Hotkey to change capture source during active session

## Troubleshooting

### No Windows Available
If "No windows available to capture!" appears:
- Ensure you have applications running with visible windows
- Make sure windows have titles (some background apps don't)
- Try opening a new application window

### Window Capture Not Working
- Verify the window is not minimized when starting capture
- Some protected windows (UAC prompts, secure desktop) cannot be captured
- Ensure your graphics drivers support window capture (Windows 10 1903+ recommended)

### Window Closed During Capture
- Session automatically disconnects with message "Captured window was closed!"
- Restart sharing and select a different window or full screen

## Requirements

- Windows 10 version 1903 or later (for Windows.Graphics.Capture API)
- Graphics hardware with screen capture support
- Valid window handle (HWND) for window mode

## Code Structure

### New Components

- `BuddyWindowList` - Structure for storing enumerated windows
- `Buddy_EnumWindowsProc` - Callback for window enumeration
- `Buddy_SelectCaptureSource` - Main function for source selection
- Window lifecycle monitoring in `Buddy_OnFrameCapture`

### Modified Components

- `ScreenBuddy` structure - Added `SelectedWindow` and `CaptureFullScreen` fields
- `Buddy_StartSharing` - Now calls window selection before starting capture
- `Buddy_LoadConfig` - Loads capture mode preference
- Configuration constants - Added window selection IDs and limits

## Performance Considerations

- Window enumeration is fast (< 100ms typically)
- Window capture has similar performance to full-screen capture
- Minimized windows may have reduced capture rate (OS-dependent)

## Known Limitations

1. **Simple Selection**: Current implementation auto-selects first window - full UI pending
2. **No Filter**: Window filter UI not yet implemented
3. **No Thumbnails**: Window preview thumbnails not included
4. **Single Monitor**: Full screen captures primary monitor only
5. **Protected Content**: Cannot capture UAC dialogs, secure desktop, or DRM content
