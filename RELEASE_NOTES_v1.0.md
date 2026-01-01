# Screen Buddy v1.0.0 (Build 180)

## 🎉 Initial Production Release

Screen Buddy is a high-performance screen sharing application built with DERP relay networking for secure, peer-to-peer connectivity.

## ✨ Key Features

### Core Functionality
- **DERP Relay Networking**: Secure peer-to-peer screen sharing through DERP relay servers
- **Simple Sharing Code**: Generate and share a unique code to start screen sharing
- **Low Latency**: Optimized for real-time screen capture and transmission
- **Connection Confirmation**: Security feature requiring explicit confirmation before connections

### User Interface
- **Sci-Fi Themed GUI**: Futuristic design inspired by Star Trek LCARS and Babylon 5
  - Dark background: RGB(15,15,25) deep blue-black
  - LCARS orange buttons: RGB(255,153,0)
  - Cyan accents: RGB(0,220,255)
  - Custom rounded buttons with owner-drawn graphics
- **Ultra-Compact Layout**: Minimalist design with efficient screen space usage
- **Rounded Section Borders**: Clean cyan borders with 8px rounded corners
- **Professional Spacing**: 4px padding, 16px item height, optimized for clarity

### Configuration
- **Settings Dialog**: Comprehensive configuration options
  - DERP server configuration
  - Log directory management
  - Connection timeout settings
  - Security preferences
- **Persistent Configuration**: JSON-based config with auto-save

### Menu System
- **File Menu**: Quick exit option
- **Edit Menu**: Access to settings
- **Help Menu**: About dialog with version info, creator, and GitHub link

## 🔧 Technical Details

### Architecture
- Built with pure Win32 C for Windows
- Direct3D 11 for screen capture
- WinHTTP for networking
- Media Foundation for encoding
- Custom dialog layout system

### Performance
- Optimized screen capture with Windows Desktop Duplication API
- Efficient DERP relay protocol implementation
- Low CPU and memory footprint

### Testing
- 54 automated tests (22 system + 21 feature + 11 server)
- Comprehensive validation suite
- DERP server connectivity verification

## 📦 Installation

1. Download `ScreenBuddy-v1.0-build180.exe`
2. Run the executable (no installation required)
3. Configure DERP server settings if needed (default: localhost:8080)
4. Start sharing or connect to a remote computer

## 🚀 Quick Start

### Sharing Your Screen
1. Launch Screen Buddy
2. Your sharing code is displayed in the "Share Your Screen" section
3. Click the copy button to share the code
4. Click "Share" to start broadcasting

### Connecting to Remote Computer
1. Obtain the sharing code from the remote user
2. Enter the code in the "Connect to Remote Computer" section
3. Click "Connect"
4. Confirm the connection when prompted

## 🔒 Security Features

- 5-minute connection timeout
- Connection confirmation requirement
- Encrypted DERP relay communication
- Private key protection with Windows DPAPI

## 📋 System Requirements

- Windows 10 or Windows 11
- Direct3D 11 capable GPU
- Network connectivity
- DERP server (local or remote)

## 🐛 Known Issues

None reported in this release.

## 👨‍💻 Credits

**Created by**: Edward Perry  
**GitHub**: https://github.com/eperry/screenbudy-ng  
**License**: All rights reserved © 2026

## 📝 Release Artifacts

- `ScreenBuddy-v1.0-build180.exe` - Compiled executable (Windows x64)
- `screenbuddy-ng-v1.0-source.tar.gz` - Source code archive

## 🔄 Build Information

- **Build Number**: 180
- **Compiler**: MSVC with /O1 optimization
- **Target**: x64 Windows
- **Branch**: gui-update
- **Commit**: 94449b6
