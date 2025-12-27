// End-to-End Color Test for ScreenBuddy
// Tests the FULL encode/decode pipeline: RGB32 -> NV12 -> H264 -> NV12 -> RGB32
// This replicates the actual ScreenBuddy pipeline

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_DEPRECATE
#define INITGUID

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <initguid.h>
#include <d3d11.h>
#include <dxgi.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <evr.h>
#include <codecapi.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "dxguid.lib")

#define TEST_WIDTH 256
#define TEST_HEIGHT 256
#define TEST_FRAMERATE 30
#define TEST_BITRATE 2000000

#define MF64(hi, lo) (((UINT64)(hi) << 32) | (lo))

// Test colors in BGRA format (matching DXGI_FORMAT_B8G8R8A8_UNORM memory layout)
typedef struct {
    const char* name;
    uint8_t b, g, r, a;
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
    { "DarkBlue",   139, 0,   0,   255 },  // EVE sidebar color
    { "Navy",       128, 0,   0,   255 },
    { "Teal",       128, 128, 0,   255 },
};
#define NUM_TEST_COLORS (sizeof(g_TestColors) / sizeof(g_TestColors[0]))

// Global D3D11 device
static ID3D11Device* g_Device = NULL;
static ID3D11DeviceContext* g_Context = NULL;

// Encoder pipeline (matches ScreenBuddy)
static IMFTransform* g_EncodeConverter = NULL;  // RGB32 -> NV12
static IMFTransform* g_Encoder = NULL;          // NV12 -> H264

// Decoder pipeline (matches ScreenBuddy)
static IMFTransform* g_Decoder = NULL;          // H264 -> NV12
static IMFTransform* g_DecodeConverter = NULL;  // NV12 -> RGB32

static IMFDXGIDeviceManager* g_DeviceManager = NULL;
static UINT g_DeviceManagerToken = 0;

// Fill texture with solid color
static void FillTexture(ID3D11Texture2D* texture, uint8_t b, uint8_t g, uint8_t r, uint8_t a)
{
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D_GetDesc(texture, &desc);
    
    // Create staging texture
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
    
    ID3D11Texture2D* staging;
    HRESULT hr = ID3D11Device_CreateTexture2D(g_Device, &stagingDesc, NULL, &staging);
    if (FAILED(hr)) {
        printf("Failed to create staging texture: 0x%08X\n", hr);
        return;
    }
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ID3D11DeviceContext_Map(g_Context, (ID3D11Resource*)staging, 0, D3D11_MAP_WRITE, 0, &mapped);
    if (FAILED(hr)) {
        printf("Failed to map staging texture: 0x%08X\n", hr);
        ID3D11Texture2D_Release(staging);
        return;
    }
    
    for (UINT y = 0; y < desc.Height; y++) {
        uint8_t* row = (uint8_t*)mapped.pData + y * mapped.RowPitch;
        for (UINT x = 0; x < desc.Width; x++) {
            row[x * 4 + 0] = b;
            row[x * 4 + 1] = g;
            row[x * 4 + 2] = r;
            row[x * 4 + 3] = a;
        }
    }
    
    ID3D11DeviceContext_Unmap(g_Context, (ID3D11Resource*)staging, 0);
    ID3D11DeviceContext_CopyResource(g_Context, (ID3D11Resource*)texture, (ID3D11Resource*)staging);
    ID3D11Texture2D_Release(staging);
}

// Read average color from texture center
static void ReadTextureColor(ID3D11Texture2D* texture, int* outB, int* outG, int* outR)
{
    D3D11_TEXTURE2D_DESC desc;
    ID3D11Texture2D_GetDesc(texture, &desc);
    
    // Create staging texture
    D3D11_TEXTURE2D_DESC stagingDesc = desc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    
    ID3D11Texture2D* staging;
    HRESULT hr = ID3D11Device_CreateTexture2D(g_Device, &stagingDesc, NULL, &staging);
    if (FAILED(hr)) {
        printf("Failed to create staging texture for read: 0x%08X\n", hr);
        *outB = *outG = *outR = -1;
        return;
    }
    
    ID3D11DeviceContext_CopyResource(g_Context, (ID3D11Resource*)staging, (ID3D11Resource*)texture);
    
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = ID3D11DeviceContext_Map(g_Context, (ID3D11Resource*)staging, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        printf("Failed to map staging texture for read: 0x%08X\n", hr);
        ID3D11Texture2D_Release(staging);
        *outB = *outG = *outR = -1;
        return;
    }
    
    // Sample center pixel
    UINT centerX = desc.Width / 2;
    UINT centerY = desc.Height / 2;
    uint8_t* pixel = (uint8_t*)mapped.pData + centerY * mapped.RowPitch + centerX * 4;
    
    *outB = pixel[0];
    *outG = pixel[1];
    *outR = pixel[2];
    
    // Also print a few pixels for debugging
    printf("  Center pixel [%u,%u]: B=%d G=%d R=%d A=%d\n", centerX, centerY, pixel[0], pixel[1], pixel[2], pixel[3]);
    
    ID3D11DeviceContext_Unmap(g_Context, (ID3D11Resource*)staging, 0);
    ID3D11Texture2D_Release(staging);
}

static bool InitD3D11(void)
{
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr = D3D11CreateDevice(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        NULL, 0, D3D11_SDK_VERSION,
        &g_Device, &featureLevel, &g_Context
    );
    
    if (FAILED(hr)) {
        printf("Failed to create D3D11 device: 0x%08X\n", hr);
        return false;
    }
    
    // Enable multithread protection
    ID3D10Multithread* mt;
    hr = ID3D11Device_QueryInterface(g_Device, &IID_ID3D10Multithread, (void**)&mt);
    if (SUCCEEDED(hr)) {
        ID3D10Multithread_SetMultithreadProtected(mt, TRUE);
        ID3D10Multithread_Release(mt);
    }
    
    // Create DXGI device manager
    hr = MFCreateDXGIDeviceManager(&g_DeviceManagerToken, &g_DeviceManager);
    if (FAILED(hr)) {
        printf("Failed to create DXGI device manager: 0x%08X\n", hr);
        return false;
    }
    
    hr = IMFDXGIDeviceManager_ResetDevice(g_DeviceManager, (IUnknown*)g_Device, g_DeviceManagerToken);
    if (FAILED(hr)) {
        printf("Failed to reset DXGI device manager: 0x%08X\n", hr);
        return false;
    }
    
    printf("D3D11 Device created (Feature Level: 0x%X)\n", featureLevel);
    return true;
}

static bool CreateEncoder(void)
{
    HRESULT hr;
    
    // Find hardware H264 encoder
    MFT_REGISTER_TYPE_INFO inputType = { MFMediaType_Video, MFVideoFormat_NV12 };
    MFT_REGISTER_TYPE_INFO outputType = { MFMediaType_Video, MFVideoFormat_H264 };
    
    IMFActivate** activates;
    UINT32 count;
    hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, 
                   MFT_ENUM_FLAG_HARDWARE | MFT_ENUM_FLAG_SORTANDFILTER,
                   &inputType, &outputType, &activates, &count);
    
    if (FAILED(hr) || count == 0) {
        printf("No hardware H264 encoder found, trying software...\n");
        hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, 
                       MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
                       &inputType, &outputType, &activates, &count);
    }
    
    if (FAILED(hr) || count == 0) {
        printf("No H264 encoder found: 0x%08X\n", hr);
        return false;
    }
    
    hr = IMFActivate_ActivateObject(activates[0], &IID_IMFTransform, (void**)&g_Encoder);
    for (UINT32 i = 0; i < count; i++) IMFActivate_Release(activates[i]);
    CoTaskMemFree(activates);
    
    if (FAILED(hr)) {
        printf("Failed to activate encoder: 0x%08X\n", hr);
        return false;
    }
    
    // Create video processor for RGB -> NV12
    hr = CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IMFTransform, (void**)&g_EncodeConverter);
    if (FAILED(hr)) {
        printf("Failed to create encode converter: 0x%08X\n", hr);
        return false;
    }
    
    // Set D3D manager on both
    hr = IMFTransform_ProcessMessage(g_EncodeConverter, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)g_DeviceManager);
    if (FAILED(hr)) printf("Warning: Failed to set D3D manager on encode converter: 0x%08X\n", hr);
    
    hr = IMFTransform_ProcessMessage(g_Encoder, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)g_DeviceManager);
    if (FAILED(hr)) printf("Warning: Failed to set D3D manager on encoder: 0x%08X\n", hr);
    
    // Configure encode converter: RGB32 -> NV12
    IMFMediaType* rgbType;
    MFCreateMediaType(&rgbType);
    IMFMediaType_SetGUID(rgbType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(rgbType, &MF_MT_SUBTYPE, &MFVideoFormat_RGB32);
    IMFMediaType_SetUINT64(rgbType, &MF_MT_FRAME_SIZE, MF64(TEST_WIDTH, TEST_HEIGHT));
    IMFMediaType_SetUINT64(rgbType, &MF_MT_FRAME_RATE, MF64(TEST_FRAMERATE, 1));
    IMFMediaType_SetUINT32(rgbType, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    
    IMFMediaType* nv12Type;
    MFCreateMediaType(&nv12Type);
    IMFMediaType_SetGUID(nv12Type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(nv12Type, &MF_MT_SUBTYPE, &MFVideoFormat_NV12);
    IMFMediaType_SetUINT64(nv12Type, &MF_MT_FRAME_SIZE, MF64(TEST_WIDTH, TEST_HEIGHT));
    IMFMediaType_SetUINT64(nv12Type, &MF_MT_FRAME_RATE, MF64(TEST_FRAMERATE, 1));
    IMFMediaType_SetUINT32(nv12Type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    
    hr = IMFTransform_SetInputType(g_EncodeConverter, 0, rgbType, 0);
    if (FAILED(hr)) { printf("Failed to set encode converter input type: 0x%08X\n", hr); return false; }
    
    hr = IMFTransform_SetOutputType(g_EncodeConverter, 0, nv12Type, 0);
    if (FAILED(hr)) { printf("Failed to set encode converter output type: 0x%08X\n", hr); return false; }
    
    // Configure encoder: NV12 -> H264
    IMFMediaType* h264Type;
    MFCreateMediaType(&h264Type);
    IMFMediaType_SetGUID(h264Type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(h264Type, &MF_MT_SUBTYPE, &MFVideoFormat_H264);
    IMFMediaType_SetUINT64(h264Type, &MF_MT_FRAME_SIZE, MF64(TEST_WIDTH, TEST_HEIGHT));
    IMFMediaType_SetUINT64(h264Type, &MF_MT_FRAME_RATE, MF64(TEST_FRAMERATE, 1));
    IMFMediaType_SetUINT32(h264Type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    IMFMediaType_SetUINT32(h264Type, &MF_MT_AVG_BITRATE, TEST_BITRATE);
    IMFMediaType_SetUINT32(h264Type, &MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Main);
    
    hr = IMFTransform_SetOutputType(g_Encoder, 0, h264Type, 0);
    if (FAILED(hr)) { printf("Failed to set encoder output type: 0x%08X\n", hr); return false; }
    
    hr = IMFTransform_SetInputType(g_Encoder, 0, nv12Type, 0);
    if (FAILED(hr)) { printf("Failed to set encoder input type: 0x%08X\n", hr); return false; }
    
    IMFMediaType_Release(rgbType);
    IMFMediaType_Release(nv12Type);
    IMFMediaType_Release(h264Type);
    
    // Start streaming
    IMFTransform_ProcessMessage(g_EncodeConverter, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    IMFTransform_ProcessMessage(g_Encoder, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    
    printf("Encoder pipeline created successfully\n");
    return true;
}

static bool CreateDecoder(void)
{
    HRESULT hr;
    
    // Find H264 decoder
    MFT_REGISTER_TYPE_INFO inputType = { MFMediaType_Video, MFVideoFormat_H264 };
    MFT_REGISTER_TYPE_INFO outputType = { MFMediaType_Video, MFVideoFormat_NV12 };
    
    IMFActivate** activates;
    UINT32 count;
    hr = MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER,
                   MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER,
                   &inputType, &outputType, &activates, &count);
    
    if (FAILED(hr) || count == 0) {
        printf("No H264 decoder found: 0x%08X\n", hr);
        return false;
    }
    
    hr = IMFActivate_ActivateObject(activates[0], &IID_IMFTransform, (void**)&g_Decoder);
    for (UINT32 i = 0; i < count; i++) IMFActivate_Release(activates[i]);
    CoTaskMemFree(activates);
    
    if (FAILED(hr)) {
        printf("Failed to activate decoder: 0x%08X\n", hr);
        return false;
    }
    
    // Create video processor for NV12 -> RGB
    hr = CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IMFTransform, (void**)&g_DecodeConverter);
    if (FAILED(hr)) {
        printf("Failed to create decode converter: 0x%08X\n", hr);
        return false;
    }
    
    // Set D3D manager
    hr = IMFTransform_ProcessMessage(g_Decoder, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)g_DeviceManager);
    if (FAILED(hr)) printf("Warning: Failed to set D3D manager on decoder: 0x%08X\n", hr);
    
    hr = IMFTransform_ProcessMessage(g_DecodeConverter, MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)g_DeviceManager);
    if (FAILED(hr)) printf("Warning: Failed to set D3D manager on decode converter: 0x%08X\n", hr);
    
    // Configure decoder input: H264
    IMFMediaType* h264Type;
    MFCreateMediaType(&h264Type);
    IMFMediaType_SetGUID(h264Type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(h264Type, &MF_MT_SUBTYPE, &MFVideoFormat_H264);
    IMFMediaType_SetUINT64(h264Type, &MF_MT_FRAME_SIZE, MF64(TEST_WIDTH, TEST_HEIGHT));
    IMFMediaType_SetUINT64(h264Type, &MF_MT_FRAME_RATE, MF64(TEST_FRAMERATE, 1));
    IMFMediaType_SetUINT32(h264Type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    
    hr = IMFTransform_SetInputType(g_Decoder, 0, h264Type, 0);
    if (FAILED(hr)) { printf("Failed to set decoder input type: 0x%08X\n", hr); return false; }
    IMFMediaType_Release(h264Type);
    
    printf("Decoder pipeline created (output types will be configured after first frame)\n");
    return true;
}

static bool ConfigureDecoderOutput(void)
{
    HRESULT hr;
    
    // Get decoder's preferred output type (NV12)
    IMFMediaType* nv12Type = NULL;
    DWORD index = 0;
    while (SUCCEEDED(IMFTransform_GetOutputAvailableType(g_Decoder, 0, index, &nv12Type))) {
        GUID subtype;
        if (SUCCEEDED(IMFMediaType_GetGUID(nv12Type, &MF_MT_SUBTYPE, &subtype))) {
            if (IsEqualGUID(&subtype, &MFVideoFormat_NV12)) {
                break;
            }
        }
        IMFMediaType_Release(nv12Type);
        nv12Type = NULL;
        index++;
    }
    
    if (!nv12Type) {
        printf("Decoder doesn't support NV12 output!\n");
        return false;
    }
    
    // Print NV12 type info
    UINT64 frameSize;
    IMFMediaType_GetUINT64(nv12Type, &MF_MT_FRAME_SIZE, &frameSize);
    printf("Decoder NV12 output: %ux%u\n", (UINT32)(frameSize >> 32), (UINT32)frameSize);
    
    hr = IMFTransform_SetOutputType(g_Decoder, 0, nv12Type, 0);
    if (FAILED(hr)) { printf("Failed to set decoder output type: 0x%08X\n", hr); return false; }
    
    // Configure decode converter: NV12 -> RGB32
    hr = IMFTransform_SetInputType(g_DecodeConverter, 0, nv12Type, 0);
    if (FAILED(hr)) { printf("Failed to set decode converter input type: 0x%08X\n", hr); return false; }
    
    UINT64 frameRate;
    IMFMediaType_GetUINT64(nv12Type, &MF_MT_FRAME_RATE, &frameRate);
    
    // Try different RGB output formats to find what works
    const GUID* rgbFormats[] = {
        &MFVideoFormat_RGB32,
        &MFVideoFormat_ARGB32,
    };
    const char* rgbFormatNames[] = { "RGB32", "ARGB32" };
    
    IMFMediaType* rgbType = NULL;
    for (int i = 0; i < 2; i++) {
        MFCreateMediaType(&rgbType);
        IMFMediaType_SetGUID(rgbType, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
        IMFMediaType_SetGUID(rgbType, &MF_MT_SUBTYPE, rgbFormats[i]);
        IMFMediaType_SetUINT64(rgbType, &MF_MT_FRAME_SIZE, frameSize);
        IMFMediaType_SetUINT64(rgbType, &MF_MT_FRAME_RATE, frameRate);
        IMFMediaType_SetUINT32(rgbType, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        
        hr = IMFTransform_SetOutputType(g_DecodeConverter, 0, rgbType, 0);
        if (SUCCEEDED(hr)) {
            printf("Decode converter output format: %s\n", rgbFormatNames[i]);
            break;
        }
        IMFMediaType_Release(rgbType);
        rgbType = NULL;
    }
    
    if (!rgbType) {
        printf("Failed to set any RGB output format on decode converter\n");
        return false;
    }
    
    IMFMediaType_Release(nv12Type);
    IMFMediaType_Release(rgbType);
    
    // Start streaming
    IMFTransform_ProcessMessage(g_Decoder, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    IMFTransform_ProcessMessage(g_DecodeConverter, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    
    return true;
}

// Encode a single frame and return H264 data
static bool EncodeFrame(ID3D11Texture2D* inputTexture, uint8_t** h264Data, DWORD* h264Size)
{
    HRESULT hr;
    
    // Create input buffer from texture
    IMFMediaBuffer* inputBuffer;
    hr = MFCreateDXGISurfaceBuffer(&IID_ID3D11Texture2D, (IUnknown*)inputTexture, 0, FALSE, &inputBuffer);
    if (FAILED(hr)) { printf("MFCreateDXGISurfaceBuffer failed: 0x%08X\n", hr); return false; }
    
    DWORD maxLen;
    IMFMediaBuffer_GetMaxLength(inputBuffer, &maxLen);
    IMFMediaBuffer_SetCurrentLength(inputBuffer, maxLen);
    
    IMFSample* inputSample;
    MFCreateSample(&inputSample);
    IMFSample_AddBuffer(inputSample, inputBuffer);
    IMFSample_SetSampleTime(inputSample, 0);
    IMFSample_SetSampleDuration(inputSample, 10000000 / TEST_FRAMERATE);
    IMFMediaBuffer_Release(inputBuffer);
    
    // Process through RGB -> NV12 converter
    hr = IMFTransform_ProcessInput(g_EncodeConverter, 0, inputSample, 0);
    IMFSample_Release(inputSample);
    if (FAILED(hr)) { printf("Encode converter ProcessInput failed: 0x%08X\n", hr); return false; }
    
    // Get NV12 output
    MFT_OUTPUT_DATA_BUFFER converterOutput = {0};
    DWORD status;
    hr = IMFTransform_ProcessOutput(g_EncodeConverter, 0, 1, &converterOutput, &status);
    if (FAILED(hr)) { printf("Encode converter ProcessOutput failed: 0x%08X\n", hr); return false; }
    
    // Feed NV12 to encoder
    hr = IMFTransform_ProcessInput(g_Encoder, 0, converterOutput.pSample, 0);
    IMFSample_Release(converterOutput.pSample);
    if (FAILED(hr)) { printf("Encoder ProcessInput failed: 0x%08X\n", hr); return false; }
    
    // Get H264 output
    MFT_OUTPUT_DATA_BUFFER encoderOutput = {0};
    hr = IMFTransform_ProcessOutput(g_Encoder, 0, 1, &encoderOutput, &status);
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        printf("Encoder needs more input (normal for first frame)\n");
        *h264Data = NULL;
        *h264Size = 0;
        return true;
    }
    if (FAILED(hr)) { printf("Encoder ProcessOutput failed: 0x%08X\n", hr); return false; }
    
    // Extract H264 data
    IMFMediaBuffer* h264Buffer;
    IMFSample_GetBufferByIndex(encoderOutput.pSample, 0, &h264Buffer);
    
    BYTE* data;
    DWORD len;
    IMFMediaBuffer_Lock(h264Buffer, &data, NULL, &len);
    
    *h264Data = (uint8_t*)malloc(len);
    *h264Size = len;
    memcpy(*h264Data, data, len);
    
    IMFMediaBuffer_Unlock(h264Buffer);
    IMFMediaBuffer_Release(h264Buffer);
    IMFSample_Release(encoderOutput.pSample);
    
    return true;
}

// Decode H264 data and return RGB texture
static bool DecodeFrame(uint8_t* h264Data, DWORD h264Size, ID3D11Texture2D** outputTexture)
{
    HRESULT hr;
    static bool decoderOutputConfigured = false;
    
    // Create H264 input buffer
    IMFMediaBuffer* h264Buffer;
    MFCreateMemoryBuffer(h264Size, &h264Buffer);
    
    BYTE* bufferData;
    IMFMediaBuffer_Lock(h264Buffer, &bufferData, NULL, NULL);
    memcpy(bufferData, h264Data, h264Size);
    IMFMediaBuffer_Unlock(h264Buffer);
    IMFMediaBuffer_SetCurrentLength(h264Buffer, h264Size);
    
    IMFSample* h264Sample;
    MFCreateSample(&h264Sample);
    IMFSample_AddBuffer(h264Sample, h264Buffer);
    IMFSample_SetSampleTime(h264Sample, 0);
    IMFSample_SetSampleDuration(h264Sample, 10000000 / TEST_FRAMERATE);
    IMFMediaBuffer_Release(h264Buffer);
    
    // Feed to decoder
    hr = IMFTransform_ProcessInput(g_Decoder, 0, h264Sample, 0);
    IMFSample_Release(h264Sample);
    if (FAILED(hr)) { printf("Decoder ProcessInput failed: 0x%08X\n", hr); return false; }
    
    // Try to get decoder output
    MFT_OUTPUT_DATA_BUFFER decoderOutput = {0};
    DWORD status;
    hr = IMFTransform_ProcessOutput(g_Decoder, 0, 1, &decoderOutput, &status);
    
    if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
        printf("Decoder stream change - configuring output...\n");
        if (!ConfigureDecoderOutput()) return false;
        decoderOutputConfigured = true;
        
        // Try again
        hr = IMFTransform_ProcessOutput(g_Decoder, 0, 1, &decoderOutput, &status);
    }
    
    if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
        printf("Decoder needs more input\n");
        *outputTexture = NULL;
        return true;
    }
    
    if (FAILED(hr)) { printf("Decoder ProcessOutput failed: 0x%08X\n", hr); return false; }
    
    // Configure decoder output if not done yet
    if (!decoderOutputConfigured) {
        IMFSample_Release(decoderOutput.pSample);
        if (!ConfigureDecoderOutput()) return false;
        decoderOutputConfigured = true;
        *outputTexture = NULL;
        return true;
    }
    
    // Process NV12 -> RGB through decode converter
    hr = IMFTransform_ProcessInput(g_DecodeConverter, 0, decoderOutput.pSample, 0);
    IMFSample_Release(decoderOutput.pSample);
    if (FAILED(hr)) { printf("Decode converter ProcessInput failed: 0x%08X\n", hr); return false; }
    
    // Get RGB output
    MFT_OUTPUT_DATA_BUFFER converterOutput = {0};
    hr = IMFTransform_ProcessOutput(g_DecodeConverter, 0, 1, &converterOutput, &status);
    if (FAILED(hr)) { printf("Decode converter ProcessOutput failed: 0x%08X\n", hr); return false; }
    
    // Extract texture from output sample
    IMFMediaBuffer* rgbBuffer;
    IMFSample_GetBufferByIndex(converterOutput.pSample, 0, &rgbBuffer);
    
    IMFDXGIBuffer* dxgiBuffer;
    hr = IMFMediaBuffer_QueryInterface(rgbBuffer, &IID_IMFDXGIBuffer, (void**)&dxgiBuffer);
    if (SUCCEEDED(hr)) {
        IMFDXGIBuffer_GetResource(dxgiBuffer, &IID_ID3D11Texture2D, (void**)outputTexture);
        IMFDXGIBuffer_Release(dxgiBuffer);
    } else {
        printf("Output is not a DXGI buffer - reading from system memory\n");
        // Create a texture and copy data
        D3D11_TEXTURE2D_DESC texDesc = {
            .Width = TEST_WIDTH,
            .Height = TEST_HEIGHT,
            .MipLevels = 1,
            .ArraySize = 1,
            .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
            .SampleDesc = { 1, 0 },
            .Usage = D3D11_USAGE_DEFAULT,
            .BindFlags = D3D11_BIND_SHADER_RESOURCE,
        };
        ID3D11Device_CreateTexture2D(g_Device, &texDesc, NULL, outputTexture);
        
        BYTE* data;
        DWORD len;
        IMFMediaBuffer_Lock(rgbBuffer, &data, NULL, &len);
        
        // Copy to texture via staging
        D3D11_TEXTURE2D_DESC stagingDesc = texDesc;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        
        ID3D11Texture2D* staging;
        ID3D11Device_CreateTexture2D(g_Device, &stagingDesc, NULL, &staging);
        
        D3D11_MAPPED_SUBRESOURCE mapped;
        ID3D11DeviceContext_Map(g_Context, (ID3D11Resource*)staging, 0, D3D11_MAP_WRITE, 0, &mapped);
        for (UINT y = 0; y < TEST_HEIGHT; y++) {
            memcpy((uint8_t*)mapped.pData + y * mapped.RowPitch, data + y * TEST_WIDTH * 4, TEST_WIDTH * 4);
        }
        ID3D11DeviceContext_Unmap(g_Context, (ID3D11Resource*)staging, 0);
        
        ID3D11DeviceContext_CopyResource(g_Context, (ID3D11Resource*)*outputTexture, (ID3D11Resource*)staging);
        ID3D11Texture2D_Release(staging);
        
        IMFMediaBuffer_Unlock(rgbBuffer);
    }
    
    IMFMediaBuffer_Release(rgbBuffer);
    IMFSample_Release(converterOutput.pSample);
    
    return true;
}

static bool TestColorRoundtrip(TestColor* color, int* deltaR, int* deltaG, int* deltaB)
{
    // Create input texture
    D3D11_TEXTURE2D_DESC texDesc = {
        .Width = TEST_WIDTH,
        .Height = TEST_HEIGHT,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = { 1, 0 },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_SHADER_RESOURCE,
    };
    
    ID3D11Texture2D* inputTexture;
    HRESULT hr = ID3D11Device_CreateTexture2D(g_Device, &texDesc, NULL, &inputTexture);
    if (FAILED(hr)) {
        printf("Failed to create input texture: 0x%08X\n", hr);
        return false;
    }
    
    // Fill with test color
    FillTexture(inputTexture, color->b, color->g, color->r, color->a);
    printf("Input: B=%d G=%d R=%d (BGRA in memory)\n", color->b, color->g, color->r);
    
    // Encode multiple frames to get keyframe
    uint8_t* h264Data = NULL;
    DWORD h264Size = 0;
    
    for (int i = 0; i < 5; i++) {
        if (h264Data) free(h264Data);
        h264Data = NULL;
        
        if (!EncodeFrame(inputTexture, &h264Data, &h264Size)) {
            ID3D11Texture2D_Release(inputTexture);
            return false;
        }
        
        if (h264Data && h264Size > 0) {
            printf("Got H264 frame %d: %u bytes\n", i, h264Size);
        }
    }
    
    ID3D11Texture2D_Release(inputTexture);
    
    if (!h264Data || h264Size == 0) {
        printf("No H264 data produced!\n");
        return false;
    }
    
    // Decode
    ID3D11Texture2D* outputTexture = NULL;
    for (int i = 0; i < 5 && !outputTexture; i++) {
        if (!DecodeFrame(h264Data, h264Size, &outputTexture)) {
            free(h264Data);
            return false;
        }
    }
    free(h264Data);
    
    if (!outputTexture) {
        printf("No decoded frame produced!\n");
        return false;
    }
    
    // Read output color
    int outB, outG, outR;
    ReadTextureColor(outputTexture, &outB, &outG, &outR);
    ID3D11Texture2D_Release(outputTexture);
    
    if (outB < 0) return false;
    
    // Calculate deltas
    *deltaR = outR - color->r;
    *deltaG = outG - color->g;
    *deltaB = outB - color->b;
    
    printf("Output: B=%d G=%d R=%d\n", outB, outG, outR);
    printf("Delta: dR=%+d dG=%+d dB=%+d\n", *deltaR, *deltaG, *deltaB);
    
    return true;
}

int main(void)
{
    printf("==============================================\n");
    printf("ScreenBuddy End-to-End Color Test\n");
    printf("==============================================\n\n");
    
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_LITE);
    
    if (!InitD3D11()) {
        printf("FATAL: D3D11 init failed\n");
        return 1;
    }
    
    if (!CreateEncoder()) {
        printf("FATAL: Encoder creation failed\n");
        return 1;
    }
    
    if (!CreateDecoder()) {
        printf("FATAL: Decoder creation failed\n");
        return 1;
    }
    
    printf("\n=== Testing Colors ===\n\n");
    
    int passed = 0, failed = 0;
    int totalDeltaR = 0, totalDeltaG = 0, totalDeltaB = 0;
    
    for (size_t i = 0; i < NUM_TEST_COLORS; i++) {
        TestColor* tc = &g_TestColors[i];
        printf("\n--- Testing %s ---\n", tc->name);
        
        int dR, dG, dB;
        if (TestColorRoundtrip(tc, &dR, &dG, &dB)) {
            totalDeltaR += dR;
            totalDeltaG += dG;
            totalDeltaB += dB;
            
            int totalDelta = abs(dR) + abs(dG) + abs(dB);
            if (totalDelta <= 30) {
                printf("PASS (delta=%d)\n", totalDelta);
                passed++;
            } else {
                printf("FAIL (delta=%d > 30)\n", totalDelta);
                failed++;
            }
        } else {
            printf("ERROR - test could not complete\n");
            failed++;
        }
    }
    
    printf("\n=== Summary ===\n");
    printf("Passed: %d, Failed: %d\n", passed, failed);
    printf("Average delta: R=%+d G=%+d B=%+d\n", 
           totalDeltaR / (int)NUM_TEST_COLORS,
           totalDeltaG / (int)NUM_TEST_COLORS, 
           totalDeltaB / (int)NUM_TEST_COLORS);
    
    // Analyze color shift pattern
    printf("\n=== Color Shift Analysis ===\n");
    if (totalDeltaR > 0 && totalDeltaG < 0 && totalDeltaB > 0) {
        printf("Pattern: R↑ G↓ B↑ - Possible green channel suppression or wrong YUV matrix\n");
    } else if (totalDeltaG > 50 * (int)NUM_TEST_COLORS) {
        printf("Pattern: Strong green shift - likely R/B channels swapped with G\n");
    } else if (abs(totalDeltaR) > 100 && abs(totalDeltaB) > 100) {
        printf("Pattern: R and B shifted - possible R/B swap in pipeline\n");
    }
    
    // Cleanup
    if (g_EncodeConverter) IMFTransform_Release(g_EncodeConverter);
    if (g_Encoder) IMFTransform_Release(g_Encoder);
    if (g_Decoder) IMFTransform_Release(g_Decoder);
    if (g_DecodeConverter) IMFTransform_Release(g_DecodeConverter);
    if (g_DeviceManager) IMFDXGIDeviceManager_Release(g_DeviceManager);
    if (g_Context) ID3D11DeviceContext_Release(g_Context);
    if (g_Device) ID3D11Device_Release(g_Device);
    
    MFShutdown();
    CoUninitialize();
    
    return failed > 0 ? 1 : 0;
}
