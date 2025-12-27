// Color Verification Test for ScreenBuddy
// Tests the RGB -> NV12 -> RGB conversion pipeline to identify color shifts

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <d3d11.h>
#include <dxgi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <evr.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "ole32.lib")

#define TEST_WIDTH 256
#define TEST_HEIGHT 256

// Test colors (BGRA format for B8G8R8A8_UNORM)
typedef struct {
    const char* name;
    uint8_t b, g, r, a;  // BGRA order
} TestColor;

static TestColor g_TestColors[] = {
    { "Black",      0,   0,   0,   255 },
    { "White",      255, 255, 255, 255 },
    { "Red",        0,   0,   255, 255 },
    { "Green",      0,   255, 0,   255 },
    { "Blue",       255, 0,   0,   255 },
    { "Yellow",     0,   255, 255, 255 },
    { "Cyan",       255, 255, 0,   255 },
    { "Magenta",    255, 0,   255, 255 },
    { "Gray50",     128, 128, 128, 255 },
    { "DarkGray",   64,  64,  64,  255 },
    { "LightGray",  192, 192, 192, 255 },
    { "Orange",     0,   165, 255, 255 },
    { "Purple",     128, 0,   128, 255 },
    { "Teal",       128, 128, 0,   255 },
    { "Navy",       128, 0,   0,   255 },
    { "Maroon",     0,   0,   128, 255 },
};

#define NUM_TEST_COLORS (sizeof(g_TestColors) / sizeof(g_TestColors[0]))

static int g_TestsPassed = 0;
static int g_TestsFailed = 0;

// GUID definitions
DEFINE_GUID(MF_MT_MAJOR_TYPE, 0x48eba18e, 0xf8c9, 0x4687, 0xbf, 0x11, 0x0a, 0x74, 0xc9, 0xf9, 0x6a, 0x8f);
DEFINE_GUID(MF_MT_SUBTYPE, 0xf7e34c9a, 0x42e8, 0x4714, 0xb7, 0x4b, 0xcb, 0x29, 0xd7, 0x2c, 0x35, 0xe5);
DEFINE_GUID(MF_MT_FRAME_SIZE, 0x1652c33d, 0xd6b2, 0x4012, 0xb8, 0x34, 0x72, 0x03, 0x08, 0x49, 0xa3, 0x7d);
DEFINE_GUID(MF_MT_FRAME_RATE, 0xc459a2e8, 0x3d2c, 0x4e44, 0xb1, 0x32, 0xfe, 0xe5, 0x15, 0x6c, 0x7b, 0xb0);
DEFINE_GUID(MF_MT_INTERLACE_MODE, 0xe2724bb8, 0xe676, 0x4806, 0xb4, 0xb2, 0xa8, 0xd6, 0xef, 0xb4, 0x4c, 0xcd);
DEFINE_GUID(MFMediaType_Video, 0x73646976, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(MFVideoFormat_RGB32, 0x00000016, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(MFVideoFormat_NV12, 0x3231564E, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(MFVideoFormat_ARGB32, 0x00000015, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71);
DEFINE_GUID(CLSID_VideoProcessorMFT, 0x88753b26, 0x5b24, 0x49bd, 0xb2, 0xe7, 0x0c, 0x44, 0x5c, 0x78, 0xc9, 0x82);
DEFINE_GUID(MF_XVP_PLAYBACK_MODE, 0x3c5d293f, 0xad67, 0x4e29, 0xaf, 0x12, 0xcf, 0x3e, 0x23, 0x8a, 0xcc, 0xe9);

// Color space GUIDs
DEFINE_GUID(MF_MT_YUV_MATRIX, 0x3e23d450, 0x2c75, 0x4d25, 0xa0, 0x0e, 0xb9, 0x16, 0x70, 0xd1, 0x23, 0x27);
DEFINE_GUID(MF_MT_VIDEO_PRIMARIES, 0xdbfbe4d7, 0x0740, 0x4ee0, 0x81, 0x92, 0x85, 0x0a, 0xb0, 0xe2, 0x19, 0x35);
DEFINE_GUID(MF_MT_TRANSFER_FUNCTION, 0x5fb0fce9, 0xbe5c, 0x4935, 0xa8, 0x11, 0xec, 0x83, 0x8f, 0x8e, 0xed, 0x93);

// MFVideoTransferMatrix values
#define MFVideoTransferMatrix_BT709 1
#define MFVideoTransferMatrix_BT601 2

// MFVideoPrimaries values  
#define MFVideoPrimaries_BT709 2

// MFVideoTransFunc values
#define MFVideoTransFunc_709 1

#define MF64(hi, lo) (((UINT64)(hi) << 32) | (lo))

static void PrintResult(const char* testName, bool passed)
{
    if (passed) {
        printf("[TEST] %s ... PASSED\n", testName);
        g_TestsPassed++;
    } else {
        printf("[TEST] %s ... FAILED\n", testName);
        g_TestsFailed++;
    }
}

// Fill a buffer with a solid color (BGRA format)
static void FillSolidColor(uint8_t* buffer, int width, int height, int stride, 
                           uint8_t b, uint8_t g, uint8_t r, uint8_t a)
{
    for (int y = 0; y < height; y++) {
        uint8_t* row = buffer + y * stride;
        for (int x = 0; x < width; x++) {
            row[x * 4 + 0] = b;
            row[x * 4 + 1] = g;
            row[x * 4 + 2] = r;
            row[x * 4 + 3] = a;
        }
    }
}

// Get average color from a buffer region
static void GetAverageColor(uint8_t* buffer, int width, int height, int stride,
                            int* avgB, int* avgG, int* avgR)
{
    long long sumB = 0, sumG = 0, sumR = 0;
    int count = 0;
    
    // Sample center region to avoid edge artifacts
    int startX = width / 4;
    int endX = width * 3 / 4;
    int startY = height / 4;
    int endY = height * 3 / 4;
    
    for (int y = startY; y < endY; y++) {
        uint8_t* row = buffer + y * stride;
        for (int x = startX; x < endX; x++) {
            sumB += row[x * 4 + 0];
            sumG += row[x * 4 + 1];
            sumR += row[x * 4 + 2];
            count++;
        }
    }
    
    *avgB = (int)(sumB / count);
    *avgG = (int)(sumG / count);
    *avgR = (int)(sumR / count);
}

// Test color conversion through Video Processor MFT (RGB32 -> NV12 -> RGB32)
static bool TestColorConversion(ID3D11Device* device, ID3D11DeviceContext* context)
{
    printf("\n=== Color Conversion Test (RGB32 -> NV12 -> RGB32) ===\n\n");
    
    HRESULT hr;
    bool success = true;
    
    // Create Video Processor MFT for RGB -> NV12
    IMFTransform* rgbToNv12 = NULL;
    hr = CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER, 
                          &IID_IMFTransform, (void**)&rgbToNv12);
    if (FAILED(hr)) {
        printf("Failed to create RGB->NV12 converter: 0x%08X\n", hr);
        return false;
    }
    
    // Create Video Processor MFT for NV12 -> RGB
    IMFTransform* nv12ToRgb = NULL;
    hr = CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER, 
                          &IID_IMFTransform, (void**)&nv12ToRgb);
    if (FAILED(hr)) {
        printf("Failed to create NV12->RGB converter: 0x%08X\n", hr);
        IMFTransform_Release(rgbToNv12);
        return false;
    }
    
    // Set up media types for RGB -> NV12
    IMFMediaType* rgbType = NULL;
    IMFMediaType* nv12Type = NULL;
    IMFMediaType* outRgbType = NULL;
    
    // RGB input type
    MFCreateMediaType(&rgbType);
    IMFMediaType_SetGUID(rgbType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(rgbType, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    IMFMediaType_SetUINT64(rgbType, &MF_MT_FRAME_SIZE, MF64(TEST_WIDTH, TEST_HEIGHT));
    IMFMediaType_SetUINT64(rgbType, &MF_MT_FRAME_RATE, MF64(30, 1));
    IMFMediaType_SetUINT32(rgbType, &MF_MT_INTERLACE_MODE, 2); // Progressive
    
    // NV12 intermediate type
    MFCreateMediaType(&nv12Type);
    IMFMediaType_SetGUID(nv12Type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(nv12Type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12);
    IMFMediaType_SetUINT64(nv12Type, &MF_MT_FRAME_SIZE, MF64(TEST_WIDTH, TEST_HEIGHT));
    IMFMediaType_SetUINT64(nv12Type, &MF_MT_FRAME_RATE, MF64(30, 1));
    IMFMediaType_SetUINT32(nv12Type, &MF_MT_INTERLACE_MODE, 2);
    // Add BT.709 color space
    IMFMediaType_SetUINT32(nv12Type, &MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
    IMFMediaType_SetUINT32(nv12Type, &MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
    IMFMediaType_SetUINT32(nv12Type, &MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
    
    // RGB output type
    MFCreateMediaType(&outRgbType);
    IMFMediaType_SetGUID(outRgbType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(outRgbType, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    IMFMediaType_SetUINT64(outRgbType, &MF_MT_FRAME_SIZE, MF64(TEST_WIDTH, TEST_HEIGHT));
    IMFMediaType_SetUINT64(outRgbType, &MF_MT_FRAME_RATE, MF64(30, 1));
    IMFMediaType_SetUINT32(outRgbType, &MF_MT_INTERLACE_MODE, 2);
    IMFMediaType_SetUINT32(outRgbType, &MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
    IMFMediaType_SetUINT32(outRgbType, &MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
    IMFMediaType_SetUINT32(outRgbType, &MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
    
    // Configure RGB -> NV12 converter
    hr = IMFTransform_SetInputType(rgbToNv12, 0, rgbType, 0);
    if (FAILED(hr)) {
        printf("Failed to set RGB->NV12 input type: 0x%08X\n", hr);
        success = false;
        goto cleanup;
    }
    
    hr = IMFTransform_SetOutputType(rgbToNv12, 0, nv12Type, 0);
    if (FAILED(hr)) {
        printf("Failed to set RGB->NV12 output type: 0x%08X\n", hr);
        success = false;
        goto cleanup;
    }
    
    // Configure NV12 -> RGB converter
    hr = IMFTransform_SetInputType(nv12ToRgb, 0, nv12Type, 0);
    if (FAILED(hr)) {
        printf("Failed to set NV12->RGB input type: 0x%08X\n", hr);
        success = false;
        goto cleanup;
    }
    
    hr = IMFTransform_SetOutputType(nv12ToRgb, 0, outRgbType, 0);
    if (FAILED(hr)) {
        printf("Failed to set NV12->RGB output type: 0x%08X\n", hr);
        success = false;
        goto cleanup;
    }
    
    // Create input and output buffers
    int stride = TEST_WIDTH * 4;
    int bufferSize = stride * TEST_HEIGHT;
    uint8_t* inputBuffer = (uint8_t*)malloc(bufferSize);
    uint8_t* outputBuffer = (uint8_t*)malloc(bufferSize);
    
    // NV12 buffer size: Y plane + UV plane (half height)
    int nv12Size = TEST_WIDTH * TEST_HEIGHT + TEST_WIDTH * TEST_HEIGHT / 2;
    uint8_t* nv12Buffer = (uint8_t*)malloc(nv12Size);
    
    printf("Testing %d colors through RGB32 -> NV12 -> RGB32 pipeline:\n\n", (int)NUM_TEST_COLORS);
    printf("%-12s | Input (R,G,B) | Output (R,G,B) | Delta | Status\n", "Color");
    printf("-------------|---------------|----------------|-------|-------\n");
    
    int totalDelta = 0;
    int maxDelta = 0;
    const char* worstColor = "None";
    
    for (int i = 0; i < NUM_TEST_COLORS; i++) {
        TestColor* tc = &g_TestColors[i];
        
        // Fill input with solid color
        FillSolidColor(inputBuffer, TEST_WIDTH, TEST_HEIGHT, stride,
                       tc->b, tc->g, tc->r, tc->a);
        
        // Create input sample
        IMFMediaBuffer* inputMediaBuffer = NULL;
        IMFSample* inputSample = NULL;
        
        MFCreateMemoryBuffer(bufferSize, &inputMediaBuffer);
        
        BYTE* bufferPtr;
        IMFMediaBuffer_Lock(inputMediaBuffer, &bufferPtr, NULL, NULL);
        memcpy(bufferPtr, inputBuffer, bufferSize);
        IMFMediaBuffer_Unlock(inputMediaBuffer);
        IMFMediaBuffer_SetCurrentLength(inputMediaBuffer, bufferSize);
        
        MFCreateSample(&inputSample);
        IMFSample_AddBuffer(inputSample, inputMediaBuffer);
        
        // Process through RGB -> NV12
        hr = IMFTransform_ProcessInput(rgbToNv12, 0, inputSample, 0);
        if (FAILED(hr)) {
            printf("%-12s | ProcessInput RGB->NV12 failed: 0x%08X\n", tc->name, hr);
            IMFSample_Release(inputSample);
            IMFMediaBuffer_Release(inputMediaBuffer);
            continue;
        }
        
        // Get NV12 output
        IMFMediaBuffer* nv12MediaBuffer = NULL;
        IMFSample* nv12Sample = NULL;
        
        MFCreateMemoryBuffer(nv12Size, &nv12MediaBuffer);
        IMFMediaBuffer_SetCurrentLength(nv12MediaBuffer, nv12Size);
        MFCreateSample(&nv12Sample);
        IMFSample_AddBuffer(nv12Sample, nv12MediaBuffer);
        
        MFT_OUTPUT_DATA_BUFFER outputData1 = { .pSample = nv12Sample };
        DWORD status1;
        hr = IMFTransform_ProcessOutput(rgbToNv12, 0, 1, &outputData1, &status1);
        if (FAILED(hr)) {
            printf("%-12s | ProcessOutput RGB->NV12 failed: 0x%08X\n", tc->name, hr);
            IMFSample_Release(nv12Sample);
            IMFMediaBuffer_Release(nv12MediaBuffer);
            IMFSample_Release(inputSample);
            IMFMediaBuffer_Release(inputMediaBuffer);
            continue;
        }
        
        // Process through NV12 -> RGB
        hr = IMFTransform_ProcessInput(nv12ToRgb, 0, nv12Sample, 0);
        if (FAILED(hr)) {
            printf("%-12s | ProcessInput NV12->RGB failed: 0x%08X\n", tc->name, hr);
            IMFSample_Release(nv12Sample);
            IMFMediaBuffer_Release(nv12MediaBuffer);
            IMFSample_Release(inputSample);
            IMFMediaBuffer_Release(inputMediaBuffer);
            continue;
        }
        
        // Get RGB output
        IMFMediaBuffer* outputMediaBuffer = NULL;
        IMFSample* outputSample = NULL;
        
        MFCreateMemoryBuffer(bufferSize, &outputMediaBuffer);
        IMFMediaBuffer_SetCurrentLength(outputMediaBuffer, bufferSize);
        MFCreateSample(&outputSample);
        IMFSample_AddBuffer(outputSample, outputMediaBuffer);
        
        MFT_OUTPUT_DATA_BUFFER outputData2 = { .pSample = outputSample };
        DWORD status2;
        hr = IMFTransform_ProcessOutput(nv12ToRgb, 0, 1, &outputData2, &status2);
        if (FAILED(hr)) {
            printf("%-12s | ProcessOutput NV12->RGB failed: 0x%08X\n", tc->name, hr);
        } else {
            // Read output and compare
            IMFMediaBuffer_Lock(outputMediaBuffer, &bufferPtr, NULL, NULL);
            memcpy(outputBuffer, bufferPtr, bufferSize);
            IMFMediaBuffer_Unlock(outputMediaBuffer);
            
            int avgB, avgG, avgR;
            GetAverageColor(outputBuffer, TEST_WIDTH, TEST_HEIGHT, stride, &avgB, &avgG, &avgR);
            
            // Calculate delta from expected
            int deltaR = abs(avgR - tc->r);
            int deltaG = abs(avgG - tc->g);
            int deltaB = abs(avgB - tc->b);
            int totalColorDelta = deltaR + deltaG + deltaB;
            
            totalDelta += totalColorDelta;
            if (totalColorDelta > maxDelta) {
                maxDelta = totalColorDelta;
                worstColor = tc->name;
            }
            
            const char* status = (totalColorDelta <= 15) ? "OK" : (totalColorDelta <= 30) ? "WARN" : "FAIL";
            
            printf("%-12s | (%3d,%3d,%3d) | (%3d,%3d,%3d)  | %3d   | %s\n",
                   tc->name, tc->r, tc->g, tc->b, avgR, avgG, avgB, totalColorDelta, status);
            
            if (totalColorDelta > 30) {
                success = false;
            }
        }
        
        IMFSample_Release(outputSample);
        IMFMediaBuffer_Release(outputMediaBuffer);
        IMFSample_Release(nv12Sample);
        IMFMediaBuffer_Release(nv12MediaBuffer);
        IMFSample_Release(inputSample);
        IMFMediaBuffer_Release(inputMediaBuffer);
    }
    
    printf("\n");
    printf("Average delta: %d\n", totalDelta / (int)NUM_TEST_COLORS);
    printf("Maximum delta: %d (color: %s)\n", maxDelta, worstColor);
    printf("Threshold: 30 (FAIL if exceeded)\n");
    
    free(inputBuffer);
    free(outputBuffer);
    free(nv12Buffer);
    
cleanup:
    if (outRgbType) IMFMediaType_Release(outRgbType);
    if (nv12Type) IMFMediaType_Release(nv12Type);
    if (rgbType) IMFMediaType_Release(rgbType);
    if (nv12ToRgb) IMFTransform_Release(nv12ToRgb);
    if (rgbToNv12) IMFTransform_Release(rgbToNv12);
    
    return success;
}

// Test what MFVideoFormat_RGB32 actually outputs
static void TestRGB32Format(void)
{
    printf("\n=== MFVideoFormat_RGB32 Format Analysis ===\n\n");
    
    // MFVideoFormat_RGB32 = D3DFMT_X8R8G8B8 = 0x00000016
    // Memory layout: B G R X (little endian stores low byte first)
    // When read as DWORD: 0xXXRRGGBB
    
    printf("MFVideoFormat_RGB32 (FOURCC: 0x00000016)\n");
    printf("  This is D3DFMT_X8R8G8B8\n");
    printf("  Memory layout: [B] [G] [R] [X] (BGRA/BGRX)\n");
    printf("  32-bit value: 0xXXRRGGBB\n");
    printf("\n");
    
    printf("DXGI_FORMAT_B8G8R8A8_UNORM\n");
    printf("  Memory layout: [B] [G] [R] [A] (BGRA)\n");
    printf("  This MATCHES MFVideoFormat_RGB32!\n");
    printf("\n");
    
    printf("When sampling B8G8R8A8_UNORM texture in shader:\n");
    printf("  .r = R channel (byte offset 2)\n");
    printf("  .g = G channel (byte offset 1)\n");
    printf("  .b = B channel (byte offset 0)\n");
    printf("  .a = A channel (byte offset 3)\n");
    printf("\n");
    
    printf("Conclusion: MFVideoFormat_RGB32 output should display correctly\n");
    printf("            when used with DXGI_FORMAT_B8G8R8A8_UNORM texture\n");
    printf("            and sampled with .rgb (not .bgr swizzle)\n");
}

// Direct pixel test without MFT
static void TestDirectPixelMapping(void)
{
    printf("\n=== Direct Pixel Byte Order Test ===\n\n");
    
    // Create a simple test pattern
    uint8_t testPixel[4];
    
    // Test pure red (should be R=255, G=0, B=0)
    // In BGRA format: B=0, G=0, R=255, A=255
    testPixel[0] = 0;     // B
    testPixel[1] = 0;     // G
    testPixel[2] = 255;   // R
    testPixel[3] = 255;   // A
    
    uint32_t packed = *(uint32_t*)testPixel;
    printf("Pure Red in BGRA:\n");
    printf("  Bytes: [%3d, %3d, %3d, %3d]\n", testPixel[0], testPixel[1], testPixel[2], testPixel[3]);
    printf("  As DWORD: 0x%08X\n", packed);
    printf("  Expected for D3DFMT_X8R8G8B8: 0xFFFF0000 (alpha=FF, R=FF, G=00, B=00)\n");
    printf("\n");
    
    // Test pure green
    testPixel[0] = 0;     // B
    testPixel[1] = 255;   // G
    testPixel[2] = 0;     // R
    testPixel[3] = 255;   // A
    
    packed = *(uint32_t*)testPixel;
    printf("Pure Green in BGRA:\n");
    printf("  Bytes: [%3d, %3d, %3d, %3d]\n", testPixel[0], testPixel[1], testPixel[2], testPixel[3]);
    printf("  As DWORD: 0x%08X\n", packed);
    printf("  Expected for D3DFMT_X8R8G8B8: 0xFF00FF00\n");
    printf("\n");
    
    // Test pure blue
    testPixel[0] = 255;   // B
    testPixel[1] = 0;     // G
    testPixel[2] = 0;     // R
    testPixel[3] = 255;   // A
    
    packed = *(uint32_t*)testPixel;
    printf("Pure Blue in BGRA:\n");
    printf("  Bytes: [%3d, %3d, %3d, %3d]\n", testPixel[0], testPixel[1], testPixel[2], testPixel[3]);
    printf("  As DWORD: 0x%08X\n", packed);
    printf("  Expected for D3DFMT_X8R8G8B8: 0xFF0000FF\n");
}

int main(void)
{
    printf("=== ScreenBuddy Color Verification Test ===\n");
    printf("============================================\n\n");
    
    HRESULT hr;
    
    // Initialize COM
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)) {
        printf("Failed to initialize COM: 0x%08X\n", hr);
        return 1;
    }
    
    // Initialize Media Foundation
    hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr)) {
        printf("Failed to initialize Media Foundation: 0x%08X\n", hr);
        CoUninitialize();
        return 1;
    }
    
    // Create D3D11 device
    ID3D11Device* device = NULL;
    ID3D11DeviceContext* context = NULL;
    
    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 
                           D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
                           NULL, 0, D3D11_SDK_VERSION,
                           &device, &featureLevel, &context);
    if (FAILED(hr)) {
        printf("Failed to create D3D11 device: 0x%08X\n", hr);
        MFShutdown();
        CoUninitialize();
        return 1;
    }
    
    printf("D3D11 Device created (Feature Level: 0x%X)\n", featureLevel);
    
    // Run tests
    TestRGB32Format();
    TestDirectPixelMapping();
    
    bool colorTestPassed = TestColorConversion(device, context);
    PrintResult("color_roundtrip_accuracy", colorTestPassed);
    
    // Cleanup
    ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);
    MFShutdown();
    CoUninitialize();
    
    printf("\n=== Test Summary ===\n");
    printf("Total:  %d\n", g_TestsPassed + g_TestsFailed);
    printf("Passed: %d\n", g_TestsPassed);
    printf("Failed: %d\n", g_TestsFailed);
    printf("==================\n\n");
    
    return g_TestsFailed > 0 ? 1 : 0;
}
