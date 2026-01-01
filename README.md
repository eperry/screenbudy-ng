# Screen Buddy

Simple application for remote desktop control over internet for Windows 10 and 11.

Get latest binary here: [Releases](https://github.com/eperry/screenbudy-ng/releases)

**WARNING**: Windows Defender or other AV software might report false positive detection

## Features

* Privacy friendly - no accounts, no registration, no telemetry
* Simple to use - no network configuration needed, works across NAT's
* Secure - all data in transit is end-to-end encrypted
* Efficient - uses GPU accelerated video encoder & decoder for minimal CPU usage
* Lightweight - native code application, uses very small amount of memory
* Small - zero external dependencies, only default Windows libraries are used
* Integrated file transfer - upload file to remote computer who shares the screen
* Keyboard input support - full keyboard control of remote computer
* Sci-fi themed GUI - futuristic LCARS-inspired interface
* Settings management - configurable DERP servers, logging, and connection timeouts

![Screen Buddy GUI](https://github.com/user-attachments/assets/1cd6ee61-b202-4d4e-9b54-4225ed025bd7)

## Building

To build the binary from source code, have [Visual Studio][VS] installed, and simply run `build.cmd`.

### Project Structure

```
ScreenBudy-NG/
├── src/
│   ├── core/        - Main application and configuration
│   ├── network/     - DERP networking and connections
│   ├── ui/          - Settings dialog and UI components
│   └── utils/       - Logging, errors, cursor control
├── resources/       - Icons, manifests, shaders, RC files
├── tests/          - Unit test suites
├── external/       - Third-party header libraries
└── dist/           - Build output directory
```

### Running Tests

Run all tests with:
```cmd
run_tests.cmd
```

Or run individual test suites:
```cmd
cd tests
build_tests.cmd
build_feature_tests.cmd
build_server_tests.cmd
```

## Configuration

Screen Buddy uses two configuration files:

1. **ScreenBuddy.ini** - DERP server configuration (in application directory)
   ```ini
   [Buddy]
   DerpRegion=1
   DerpRegion1=localhost
   CaptureFullScreen=0
   ```

2. **config.json** - User settings (in `%AppData%\ScreenBuddy\`)
   - Connection timeout
   - Log directory location
   - Private key encryption

Access user settings via **Edit → Settings** menu.

## Technical Details

If you want to read source code, you can find there & learn about following things:

* Custom win32 UI with dialog API and owner-drawn controls
* Using [Media Foundation][] for hardware accelerated H264 video encoding and decoding
* Using [Video Processor MFT] to convert RGB texture to NV12 for encoding, and back from decoding
* Using asynchronous Media Foundation transform events for video encoding
* Capturing screen to D3D11 texture, using code from [wcap][]
* Simple D3D11 shader to render texture, optionally scaling it down by preserving aspect ratio
* Using [DerpNet][] library for network communication via DERP relays
* Using [WinHTTP][] for https requests to gather initial info about DERP relay regions
* Parsing JSON with [Windows.Data.Json][] API, using code from [TwitchNotify][]
* Copying & pasting text from/to clipboard
* Simple progress dialog using Windows [TaskDialog][] common control
* Basic drag & drop to handle files dropped on window, using [DragAcceptFiles][] function
* Retrieving Windows registered file icon using [SHGetFileInfo][] function
* Configuration management with JSON persistence and settings UI

## Configuration

Screen Buddy uses a JSON configuration file stored in `%AppData%\ScreenBuddy\config.json`. Settings include:

* DERP server URL and port
* Connection timeout
* Log directory location
* Private key encryption

Access settings via **Edit → Settings** menu.

## License

Created by: Edward Perry  
GitHub: https://github.com/eperry/screenbudy-ng  
© 2026 Edward Perry. All rights reserved.

[Media Foundation]: https://learn.microsoft.com/en-us/windows/win32/medfound/microsoft-media-foundation-sdk
[Video Processor MFT]: https://learn.microsoft.com/en-us/windows/win32/medfound/video-processor-mft
[WinHTTP]: https://learn.microsoft.com/en-us/windows/win32/winhttp/winhttp-start-page
[Windows.Data.Json]: https://learn.microsoft.com/en-us/uwp/api/windows.data.json
[wcap]: https://github.com/mmozeiko/wcap/
[DerpNet]: https://github.com/mmozeiko/derpnet/
[TwitchNotify]: https://github.com/mmozeiko/TwitchNotify/
[VS]: https://visualstudio.microsoft.com/vs/
[TaskDialog]: https://learn.microsoft.com/en-us/windows/win32/controls/task-dialogs-overview
[DragAcceptFiles]: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-dragacceptfiles
[SHGetFileInfo]: https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shgetfileinfow
