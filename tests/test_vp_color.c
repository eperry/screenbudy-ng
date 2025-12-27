// Video Processor Color Test
// Tests RGB32 -> NV12 -> RGB32 conversion through Video Processor MFT
// This is the core color conversion that happens in ScreenBuddy

#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_DEPRECATE

#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mf.lib")
#pragma comment(lib, "ole32.lib")

#define TEST_WIDTH 64
#define TEST_HEIGHT 64
#define MF64(hi, lo) (((UINT64)(hi) << 32) | (lo))

// Test colors: name, R, G, B (RGB order, as humans think of it)
typedef struct {
    const char* name;
    uint8_t r, g, b;
} ColorTest;

static ColorTest g_TestColors[] = {
    { "Black",      0,   0,   0   },
    { "White",      255, 255, 255 },
    { "Red",        255, 0,   0   },
    { "Green",      0,   255, 0   },
    { "Blue",       0,   0,   255 },
    { "Yellow",     255, 255, 0   },
    { "Cyan",       0,   255, 255 },
    { "Magenta",    255, 0,   255 },
    { "DarkBlue",   0,   0,   139 },  // EVE sidebar
    { "Navy",       0,   0,   128 },
};
#define NUM_COLORS (sizeof(g_TestColors) / sizeof(g_TestColors[0]))

static IMFTransform* g_RgbToNv12 = NULL;
static IMFTransform* g_Nv12ToRgb = NULL;
static MFT_OUTPUT_STREAM_INFO g_RgbToNv12Info;
static MFT_OUTPUT_STREAM_INFO g_Nv12ToRgbInfo;

static IMFMediaType* CreateVideoType(const GUID* subtype)
{
    IMFMediaType* type = NULL;
    if (FAILED(MFCreateMediaType(&type))) return NULL;
    IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Video);
    IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, subtype);
    IMFMediaType_SetUINT64(type, &MF_MT_FRAME_SIZE, MF64(TEST_WIDTH, TEST_HEIGHT));
    IMFMediaType_SetUINT64(type, &MF_MT_FRAME_RATE, MF64(30, 1));
    IMFMediaType_SetUINT32(type, &MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    IMFMediaType_SetUINT32(type, &MF_MT_VIDEO_NOMINAL_RANGE, MFNominalRange_0_255);
    IMFMediaType_SetUINT32(type, &MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
    IMFMediaType_SetUINT32(type, &MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
    IMFMediaType_SetUINT32(type, &MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
    return type;
}

static IMFSample* AllocSimpleSample(DWORD size)
{
    IMFMediaBuffer* buf = NULL;
    IMFSample* sample = NULL;
    if (FAILED(MFCreateMemoryBuffer(size, &buf))) goto fail;
    IMFMediaBuffer_SetCurrentLength(buf, size);
    if (FAILED(MFCreateSample(&sample))) goto fail;
    if (FAILED(IMFSample_AddBuffer(sample, buf))) goto fail;
    IMFMediaBuffer_Release(buf);
    return sample;
fail:
    if (buf) IMFMediaBuffer_Release(buf);
    if (sample) IMFSample_Release(sample);
    return NULL;
}

// ProcessOutput helper that retries when the MFT reports incomplete output but returns an empty sample
static bool GetOutputSample(IMFTransform* transform, MFT_OUTPUT_STREAM_INFO* info, DWORD sampleSize,
                            const char* label, MFT_OUTPUT_DATA_BUFFER* out)
{
    HRESULT hr;
    DWORD status = 0;

    for (int attempt = 0; attempt < 4; ++attempt) {
        status = 0;
        hr = IMFTransform_ProcessOutput(transform, 0, 1, out, &status);
        if (FAILED(hr)) {
            printf("%s ProcessOutput failed: 0x%08X (status=0x%08X)\n", label, hr, status);
            if (out->pSample) { IMFSample_Release(out->pSample); out->pSample = NULL; }
            return false;
        }
        if (status != 0) {
            printf("%s ProcessOutput status: 0x%08X\n", label, status);
        }

        if (out->pSample) {
            DWORD count = 0;
            if (SUCCEEDED(IMFSample_GetBufferCount(out->pSample, &count))) {
                printf("%s sample buffer count: %u\n", label, count);
                if (count > 0) {
                    return true;
                }
            } else {
                printf("%s sample: failed to get buffer count\n", label);
            }
        } else {
            printf("%s ProcessOutput returned NULL sample\n", label);
        }

        if (attempt == 3) break;
        printf("%s sample empty; retrying...\n", label);

        if (out->pSample) { IMFSample_Release(out->pSample); out->pSample = NULL; }
        if (!(info->dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
            out->pSample = AllocSimpleSample(sampleSize);
            if (!out->pSample) {
                printf("%s: failed to allocate retry sample\n", label);
                return false;
            }
        }
    }

    if (out->pSample) { IMFSample_Release(out->pSample); out->pSample = NULL; }
    return false;
}

static bool CreateConverters(void)
{
    HRESULT hr;
    
    // Create RGB -> NV12 converter
    hr = CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IMFTransform, (void**)&g_RgbToNv12);
    if (FAILED(hr)) {
        printf("Failed to create RGB->NV12 converter: 0x%08X\n", hr);
        return false;
    }
    
    // Create NV12 -> RGB converter
    hr = CoCreateInstance(&CLSID_VideoProcessorMFT, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IMFTransform, (void**)&g_Nv12ToRgb);
    if (FAILED(hr)) {
        printf("Failed to create NV12->RGB converter: 0x%08X\n", hr);
        return false;
    }
    
    IMFMediaType* rgbType = CreateVideoType(&MFVideoFormat_RGB32);
    IMFMediaType* nv12Type = CreateVideoType(&MFVideoFormat_NV12);
    
    hr = IMFTransform_SetInputType(g_RgbToNv12, 0, rgbType, 0);
    if (FAILED(hr)) { printf("Failed to set RGB->NV12 input: 0x%08X\n", hr); return false; }
    
    hr = IMFTransform_SetOutputType(g_RgbToNv12, 0, nv12Type, 0);
    if (FAILED(hr)) { printf("Failed to set RGB->NV12 output: 0x%08X\n", hr); return false; }

    // Capture output stream info
    IMFTransform_GetOutputStreamInfo(g_RgbToNv12, 0, &g_RgbToNv12Info);
    printf("RGB->NV12 Output flags: 0x%08X\n", g_RgbToNv12Info.dwFlags);
    
    // Configure NV12 -> RGB
    hr = IMFTransform_SetInputType(g_Nv12ToRgb, 0, nv12Type, 0);
    if (FAILED(hr)) { printf("Failed to set NV12->RGB input: 0x%08X\n", hr); return false; }
    
    hr = IMFTransform_SetOutputType(g_Nv12ToRgb, 0, rgbType, 0);
    if (FAILED(hr)) { printf("Failed to set NV12->RGB output: 0x%08X\n", hr); return false; }

    IMFTransform_GetOutputStreamInfo(g_Nv12ToRgb, 0, &g_Nv12ToRgbInfo);
    printf("NV12->RGB Output flags: 0x%08X\n", g_Nv12ToRgbInfo.dwFlags);
    
    IMFMediaType_Release(rgbType);
    IMFMediaType_Release(nv12Type);
    
    // Start streaming
    IMFTransform_ProcessMessage(g_RgbToNv12, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    IMFTransform_ProcessMessage(g_Nv12ToRgb, MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
    
    printf("Video Processor converters created\n");
    return true;
}

static bool RecreateConverters(void)
{
    if (g_RgbToNv12) { IMFTransform_Release(g_RgbToNv12); g_RgbToNv12 = NULL; }
    if (g_Nv12ToRgb) { IMFTransform_Release(g_Nv12ToRgb); g_Nv12ToRgb = NULL; }
    return CreateConverters();
}

// Create RGB32 buffer with solid color
// Memory layout for MFVideoFormat_RGB32: [B][G][R][X] per pixel (BGRX)
static IMFSample* CreateRgbSample(uint8_t r, uint8_t g, uint8_t b)
{
    int stride = TEST_WIDTH * 4;
    int bufferSize = stride * TEST_HEIGHT;
    
    IMFMediaBuffer* buffer;
    HRESULT hr = MFCreateMemoryBuffer(bufferSize, &buffer);
    if (FAILED(hr)) return NULL;
    
    BYTE* data;
    IMFMediaBuffer_Lock(buffer, &data, NULL, NULL);
    
    // Fill with color - MFVideoFormat_RGB32 is BGRX in memory
    for (int y = 0; y < TEST_HEIGHT; y++) {
        uint8_t* row = data + y * stride;
        for (int x = 0; x < TEST_WIDTH; x++) {
            row[x * 4 + 0] = b;  // B
            row[x * 4 + 1] = g;  // G
            row[x * 4 + 2] = r;  // R
            row[x * 4 + 3] = 255; // X/A
        }
    }
    
    IMFMediaBuffer_Unlock(buffer);
    IMFMediaBuffer_SetCurrentLength(buffer, bufferSize);
    
    IMFSample* sample;
    MFCreateSample(&sample);
    IMFSample_AddBuffer(sample, buffer);
    IMFSample_SetSampleTime(sample, 0);
    IMFSample_SetSampleDuration(sample, 333333);
    IMFMediaBuffer_Release(buffer);
    
    return sample;
}

// Read center pixel from RGB32 sample
static HRESULT ReadRgbSample(IMFSample* sample, uint8_t* r, uint8_t* g, uint8_t* b)
{
    IMFMediaBuffer* buffer;
    HRESULT hr = IMFSample_GetBufferByIndex(sample, 0, &buffer);
    if (FAILED(hr)) return hr;
    
    BYTE* data;
    DWORD len;
    hr = IMFMediaBuffer_Lock(buffer, &data, NULL, &len);
    if (FAILED(hr)) {
        IMFMediaBuffer_Release(buffer);
        return hr;
    }
    
    int stride = TEST_WIDTH * 4;
    int centerY = TEST_HEIGHT / 2;
    int centerX = TEST_WIDTH / 2;
    
    uint8_t* pixel = data + centerY * stride + centerX * 4;
    *b = pixel[0];
    *g = pixel[1];
    *r = pixel[2];
    
    IMFMediaBuffer_Unlock(buffer);
    IMFMediaBuffer_Release(buffer);
    
    return S_OK;
}

static bool DoColorTest(ColorTest* tc, int* dR, int* dG, int* dB)
{
    HRESULT hr;
    
    // Create input sample
    IMFSample* inputSample = CreateRgbSample(tc->r, tc->g, tc->b);
    if (!inputSample) {
        printf("Failed to create input sample\n");
        return false;
    }
    
    // Process RGB -> NV12
    hr = IMFTransform_ProcessInput(g_RgbToNv12, 0, inputSample, 0);
    IMFSample_Release(inputSample);
    if (FAILED(hr)) {
        printf("RGB->NV12 ProcessInput failed: 0x%08X\n", hr);
        return false;
    }
    
    // Get NV12 output
    IMFSample* nv12Sample = NULL;
    DWORD nv12Size = TEST_WIDTH * TEST_HEIGHT * 3 / 2;
    if (!(g_RgbToNv12Info.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
        nv12Sample = AllocSimpleSample(nv12Size);
    }

    MFT_OUTPUT_DATA_BUFFER nv12Output = { .pSample = nv12Sample };
    if (!GetOutputSample(g_RgbToNv12, &g_RgbToNv12Info, nv12Size, "RGB->NV12", &nv12Output)) {
        return false;
    }
    
    // Process NV12 -> RGB
    hr = IMFTransform_ProcessInput(g_Nv12ToRgb, 0, nv12Output.pSample, 0);
    IMFSample_Release(nv12Output.pSample);
    if (FAILED(hr)) {
        printf("NV12->RGB ProcessInput failed: 0x%08X\n", hr);
        return false;
    }
    if (nv12Sample && nv12Output.pSample != nv12Sample) {
        IMFSample_Release(nv12Sample);
    }
    
    // Get RGB output
    IMFSample* rgbSample = NULL;
    DWORD rgbSize = TEST_WIDTH * TEST_HEIGHT * 4;
    if (!(g_Nv12ToRgbInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)) {
        rgbSample = AllocSimpleSample(rgbSize);
    }

    MFT_OUTPUT_DATA_BUFFER rgbOutput = { .pSample = rgbSample };
    if (!GetOutputSample(g_Nv12ToRgb, &g_Nv12ToRgbInfo, rgbSize, "NV12->RGB", &rgbOutput)) {
        return false;
    }
    
    // Read output color
    uint8_t outR, outG, outB;
    hr = ReadRgbSample(rgbOutput.pSample, &outR, &outG, &outB);
    if (FAILED(hr)) {
        printf("ReadRgbSample failed: 0x%08X\n", hr);
        IMFSample_Release(rgbOutput.pSample);
        if (rgbSample) IMFSample_Release(rgbSample);
        return false;
    }
    IMFSample_Release(rgbOutput.pSample);
    if (rgbSample && rgbOutput.pSample != rgbSample) {
        IMFSample_Release(rgbSample);
    }
    
    *dR = (int)outR - (int)tc->r;
    *dG = (int)outG - (int)tc->g;
    *dB = (int)outB - (int)tc->b;
    
    printf("  In: R=%3d G=%3d B=%3d -> Out: R=%3d G=%3d B=%3d -> Delta: dR=%+4d dG=%+4d dB=%+4d\n",
           tc->r, tc->g, tc->b, outR, outG, outB, *dR, *dG, *dB);
    
    return true;
}

int main(void)
{
    printf("=====================================================\n");
    printf("Video Processor MFT Color Conversion Test\n");
    printf("Tests: RGB32 -> NV12 -> RGB32 (no H264 encoding)\n");
    printf("=====================================================\n\n");
    
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, MFSTARTUP_LITE);
    
    if (!CreateConverters()) {
        printf("FATAL: Failed to create converters\n");
        MFShutdown();
        CoUninitialize();
        return 1;
    }
    
    printf("\n=== Color Tests ===\n\n");
    
    int passed = 0, failed = 0;
    int sumDR = 0, sumDG = 0, sumDB = 0;
    
    for (size_t i = 0; i < NUM_COLORS; i++) {
        if (!RecreateConverters()) {
            printf("Failed to recreate converters\n");
            failed++;
            continue;
        }

        ColorTest* tc = &g_TestColors[i];
        printf("[%s]\n", tc->name);
        
        int dR, dG, dB;
        if (DoColorTest(tc, &dR, &dG, &dB)) {
            sumDR += dR;
            sumDG += dG;
            sumDB += dB;
            
            int totalDelta = abs(dR) + abs(dG) + abs(dB);
            if (totalDelta <= 10) {
                passed++;
            } else {
                failed++;
                printf("  ^^^ LARGE DELTA!\n");
            }
        } else {
            printf("  ERROR\n");
            failed++;
        }
    }
    
    printf("\n=== Results ===\n");
    printf("Passed: %d, Failed: %d\n", passed, failed);
    printf("Average delta: dR=%+d dG=%+d dB=%+d\n",
           sumDR / (int)NUM_COLORS, sumDG / (int)NUM_COLORS, sumDB / (int)NUM_COLORS);
    
    printf("\n=== Analysis ===\n");
    int avgR = sumDR / (int)NUM_COLORS;
    int avgG = sumDG / (int)NUM_COLORS;
    int avgB = sumDB / (int)NUM_COLORS;
    
    if (abs(avgR) < 5 && abs(avgG) < 5 && abs(avgB) < 5) {
        printf("Color conversion looks CORRECT! Issue is elsewhere.\n");
    } else if (avgG > 20) {
        printf("Strong GREEN shift detected - channels may be swapped.\n");
        printf("Try swapping R<->B in shader: use .bgr instead of .rgb\n");
    } else if (avgR > 20 && avgB < -20) {
        printf("R increased, B decreased - possible R<->B swap.\n");
    } else if (avgR < -20 && avgB > 20) {
        printf("R decreased, B increased - possible B<->R swap.\n");
    }
    
    // Cleanup
    if (g_RgbToNv12) IMFTransform_Release(g_RgbToNv12);
    if (g_Nv12ToRgb) IMFTransform_Release(g_Nv12ToRgb);
    
    MFShutdown();
    CoUninitialize();
    
    return failed > 0 ? 1 : 0;
}
