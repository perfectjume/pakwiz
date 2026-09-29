#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <GL/gl.h>

#include <cstdio>
#include <cstring>
#include <string>

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

#ifndef WGL_ACCESS_READ_ONLY_NV
#define WGL_ACCESS_READ_ONLY_NV 0x0000
#endif

using PFNWGLGETEXTENSIONSSTRINGARBPROC = const char* (WINAPI*)(HDC);
using PFNWGLDXOPENDEVICENVPROC = HANDLE (WINAPI*)(void*);
using PFNWGLDXCLOSEDEVICENVPROC = BOOL (WINAPI*)(HANDLE);
using PFNWGLDXREGISTEROBJECTNVPROC = HANDLE (WINAPI*)(HANDLE, void*, GLuint, GLenum, GLenum);
using PFNWGLDXUNREGISTEROBJECTNVPROC = BOOL (WINAPI*)(HANDLE, HANDLE);
using PFNWGLDXLOCKOBJECTSNVPROC = BOOL (WINAPI*)(HANDLE, GLint, HANDLE*);
using PFNWGLDXUNLOCKOBJECTSNVPROC = BOOL (WINAPI*)(HANDLE, GLint, HANDLE*);

static PFNWGLDXOPENDEVICENVPROC p_wglDXOpenDeviceNV = nullptr;
static PFNWGLDXCLOSEDEVICENVPROC p_wglDXCloseDeviceNV = nullptr;
static PFNWGLDXREGISTEROBJECTNVPROC p_wglDXRegisterObjectNV = nullptr;
static PFNWGLDXUNREGISTEROBJECTNVPROC p_wglDXUnregisterObjectNV = nullptr;
static PFNWGLDXLOCKOBJECTSNVPROC p_wglDXLockObjectsNV = nullptr;
static PFNWGLDXUNLOCKOBJECTSNVPROC p_wglDXUnlockObjectsNV = nullptr;

static void log_line(const char* s) {
    std::printf("%s\n", s);
    std::fflush(stdout);
}

static std::string win32_error(const char* prefix) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%s GetLastError=%lu", prefix, GetLastError());
    return std::string(buf);
}

static bool contains_extension(const char* all, const char* needle) {
    if (!all || !needle || !*needle) return false;
    const size_t n = std::strlen(needle);
    const char* p = all;
    while ((p = std::strstr(p, needle)) != nullptr) {
        const bool left = (p == all) || p[-1] == ' ';
        const bool right = p[n] == '\0' || p[n] == ' ';
        if (left && right) return true;
        p += n;
    }
    return false;
}

struct WinGl {
    HINSTANCE instance = nullptr;
    HWND hwnd = nullptr;
    HDC dc = nullptr;
    HGLRC rc = nullptr;
    ATOM atom = 0;

    bool open() {
        instance = GetModuleHandleW(nullptr);
        const wchar_t* klass = L"CoreUiWglDx11ProbeWindow";

        WNDCLASSW wc{};
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = instance;
        wc.lpszClassName = klass;
        atom = RegisterClassW(&wc);
        if (!atom && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            std::fprintf(stderr, "FAIL RegisterClassW Win32=%lu\n", GetLastError());
            return false;
        }

        hwnd = CreateWindowExW(
            0, klass, L"CORE UI WGL DX11 probe",
            WS_OVERLAPPEDWINDOW,
            0, 0, 64, 64,
            nullptr, nullptr, instance, nullptr);
        if (!hwnd) {
            std::fprintf(stderr, "FAIL CreateWindowExW Win32=%lu\n", GetLastError());
            return false;
        }

        dc = GetDC(hwnd);
        if (!dc) {
            std::fprintf(stderr, "FAIL GetDC Win32=%lu\n", GetLastError());
            return false;
        }

        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cAlphaBits = 8;
        pfd.cDepthBits = 24;
        pfd.iLayerType = PFD_MAIN_PLANE;

        const int pf = ChoosePixelFormat(dc, &pfd);
        if (!pf) {
            std::fprintf(stderr, "FAIL ChoosePixelFormat Win32=%lu\n", GetLastError());
            return false;
        }
        if (!SetPixelFormat(dc, pf, &pfd)) {
            std::fprintf(stderr, "FAIL SetPixelFormat Win32=%lu\n", GetLastError());
            return false;
        }

        rc = wglCreateContext(dc);
        if (!rc) {
            std::fprintf(stderr, "FAIL wglCreateContext Win32=%lu\n", GetLastError());
            return false;
        }
        if (!wglMakeCurrent(dc, rc)) {
            std::fprintf(stderr, "FAIL wglMakeCurrent Win32=%lu\n", GetLastError());
            return false;
        }
        return true;
    }

    ~WinGl() {
        if (rc) {
            wglMakeCurrent(nullptr, nullptr);
            wglDeleteContext(rc);
        }
        if (dc && hwnd) ReleaseDC(hwnd, dc);
        if (hwnd) DestroyWindow(hwnd);
        if (atom && instance) UnregisterClassW(L"CoreUiWglDx11ProbeWindow", instance);
    }
};

static bool load_interop(HDC dc, bool& unsupported) {
    unsupported = false;

    const char* glVendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::printf("GL_VENDOR=%s\n", glVendor ? glVendor : "<null>");
    std::printf("GL_RENDERER=%s\n", glRenderer ? glRenderer : "<null>");
    std::printf("GL_VERSION=%s\n", glVersion ? glVersion : "<null>");

    auto getExtensions =
        reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(
            wglGetProcAddress("wglGetExtensionsStringARB"));
    const char* extensions = getExtensions ? getExtensions(dc) : nullptr;
    const bool interop = contains_extension(extensions, "WGL_NV_DX_interop");
    const bool interop2 = contains_extension(extensions, "WGL_NV_DX_interop2");

    std::printf("WGL_NV_DX_interop=%s\n", interop ? "true" : "false");
    std::printf("WGL_NV_DX_interop2=%s\n", interop2 ? "true" : "false");

    if (!interop2) {
        unsupported = true;
        log_line("WGL_DX11_INTEROP_ENV=UNSUPPORTED missing=WGL_NV_DX_interop2");
        return false;
    }

    p_wglDXOpenDeviceNV =
        reinterpret_cast<PFNWGLDXOPENDEVICENVPROC>(wglGetProcAddress("wglDXOpenDeviceNV"));
    p_wglDXCloseDeviceNV =
        reinterpret_cast<PFNWGLDXCLOSEDEVICENVPROC>(wglGetProcAddress("wglDXCloseDeviceNV"));
    p_wglDXRegisterObjectNV =
        reinterpret_cast<PFNWGLDXREGISTEROBJECTNVPROC>(wglGetProcAddress("wglDXRegisterObjectNV"));
    p_wglDXUnregisterObjectNV =
        reinterpret_cast<PFNWGLDXUNREGISTEROBJECTNVPROC>(wglGetProcAddress("wglDXUnregisterObjectNV"));
    p_wglDXLockObjectsNV =
        reinterpret_cast<PFNWGLDXLOCKOBJECTSNVPROC>(wglGetProcAddress("wglDXLockObjectsNV"));
    p_wglDXUnlockObjectsNV =
        reinterpret_cast<PFNWGLDXUNLOCKOBJECTSNVPROC>(wglGetProcAddress("wglDXUnlockObjectsNV"));

    if (!p_wglDXOpenDeviceNV || !p_wglDXCloseDeviceNV ||
        !p_wglDXRegisterObjectNV || !p_wglDXUnregisterObjectNV ||
        !p_wglDXLockObjectsNV || !p_wglDXUnlockObjectsNV) {
        std::fprintf(stderr, "FAIL WGL_NV_DX_interop2 advertised but required entrypoint missing\n");
        return false;
    }

    log_line("WGL_INTEROP_ENTRYPOINTS=PASS");
    return true;
}

static bool pixel_close(const unsigned char* p, int b, int g, int r, int a) {
    auto near = [](int x, int y) { return x >= y - 1 && x <= y + 1; };
    return near(p[0], b) && near(p[1], g) && near(p[2], r) && p[3] == a;
}

static bool gl_read_pixel(GLuint texture, unsigned char out[4]) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, out);
    const GLenum err = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (err != GL_NO_ERROR) {
        std::fprintf(stderr, "FAIL glGetTexImage error=0x%X\n", static_cast<unsigned>(err));
        return false;
    }
    return true;
}

static bool run_frame(
    ID3D11DeviceContext* context,
    ID3D11RenderTargetView* rtv,
    HANDLE interopDevice,
    HANDLE interopObject,
    GLuint glTexture,
    const float rgba[4],
    int expectedB,
    int expectedG,
    int expectedR,
    int cycle)
{
    context->ClearRenderTargetView(rtv, rgba);
    context->Flush();

    HANDLE objects[1] = { interopObject };
    if (!p_wglDXLockObjectsNV(interopDevice, 1, objects)) {
        std::fprintf(stderr, "FAIL wglDXLockObjectsNV cycle=%d Win32=%lu\n",
                     cycle, GetLastError());
        return false;
    }

    unsigned char px[4] = {};
    bool readOk = gl_read_pixel(glTexture, px);

    if (!p_wglDXUnlockObjectsNV(interopDevice, 1, objects)) {
        std::fprintf(stderr, "FAIL wglDXUnlockObjectsNV cycle=%d Win32=%lu\n",
                     cycle, GetLastError());
        return false;
    }

    if (!readOk) return false;

    std::printf("WGL_CYCLE_%d_PIXEL bgra=%u,%u,%u,%u\n",
                cycle, px[0], px[1], px[2], px[3]);

    if (!pixel_close(px, expectedB, expectedG, expectedR, 255)) {
        std::fprintf(stderr,
                     "FAIL WGL pixel mismatch cycle=%d expected=%d,%d,%d,255 actual=%u,%u,%u,%u\n",
                     cycle, expectedB, expectedG, expectedR,
                     px[0], px[1], px[2], px[3]);
        return false;
    }

    std::printf("WGL_LOCK_READ_UNLOCK_CYCLE_%d=PASS\n", cycle);
    return true;
}

int main() {
    WinGl gl;
    if (!gl.open()) return 10;

    bool unsupported = false;
    if (!load_interop(gl.dc, unsupported)) {
        return unsupported ? 2 : 11;
    }

    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    D3D_FEATURE_LEVEL featureLevel{};

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &device,
        &featureLevel,
        &context);
    if (FAILED(hr) || !device || !context) {
        std::fprintf(stderr, "FAIL D3D11CreateDevice hr=0x%08X\n", static_cast<unsigned>(hr));
        return 12;
    }
    std::printf("D3D11_CREATE_DEVICE=PASS featureLevel=0x%X\n", static_cast<unsigned>(featureLevel));

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 64;
    desc.Height = 64;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Texture2D* texture = nullptr;
    hr = device->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr) || !texture) {
        std::fprintf(stderr, "FAIL CreateTexture2D hr=0x%08X\n", static_cast<unsigned>(hr));
        context->Release();
        device->Release();
        return 13;
    }

    ID3D11RenderTargetView* rtv = nullptr;
    hr = device->CreateRenderTargetView(texture, nullptr, &rtv);
    if (FAILED(hr) || !rtv) {
        std::fprintf(stderr, "FAIL CreateRenderTargetView hr=0x%08X\n", static_cast<unsigned>(hr));
        texture->Release();
        context->Release();
        device->Release();
        return 14;
    }

    HANDLE interopDevice = p_wglDXOpenDeviceNV(device);
    if (!interopDevice) {
        std::fprintf(stderr, "FAIL wglDXOpenDeviceNV Win32=%lu\n", GetLastError());
        rtv->Release();
        texture->Release();
        context->Release();
        device->Release();
        return 15;
    }
    log_line("WGL_DX_OPEN_DEVICE=PASS");

    GLuint glTexture = 0;
    glGenTextures(1, &glTexture);
    glBindTexture(GL_TEXTURE_2D, glTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glBindTexture(GL_TEXTURE_2D, 0);

    HANDLE interopObject = p_wglDXRegisterObjectNV(
        interopDevice,
        texture,
        glTexture,
        GL_TEXTURE_2D,
        WGL_ACCESS_READ_ONLY_NV);
    if (!interopObject) {
        std::fprintf(stderr, "FAIL wglDXRegisterObjectNV Win32=%lu\n", GetLastError());
        glDeleteTextures(1, &glTexture);
        p_wglDXCloseDeviceNV(interopDevice);
        rtv->Release();
        texture->Release();
        context->Release();
        device->Release();
        return 16;
    }
    log_line("WGL_DX_REGISTER_OBJECT=PASS type=GL_TEXTURE_2D access=READ_ONLY");

    const float first[4] = { 0.25f, 0.50f, 0.75f, 1.00f };
    const float second[4] = { 0.75f, 0.25f, 0.50f, 1.00f };
    bool ok =
        run_frame(context, rtv, interopDevice, interopObject, glTexture,
                  first, 191, 128, 64, 1) &&
        run_frame(context, rtv, interopDevice, interopObject, glTexture,
                  second, 128, 64, 191, 2);

    if (!p_wglDXUnregisterObjectNV(interopDevice, interopObject)) {
        std::fprintf(stderr, "FAIL wglDXUnregisterObjectNV Win32=%lu\n", GetLastError());
        ok = false;
    } else {
        log_line("WGL_DX_UNREGISTER_OBJECT=PASS");
    }

    glDeleteTextures(1, &glTexture);

    if (!p_wglDXCloseDeviceNV(interopDevice)) {
        std::fprintf(stderr, "FAIL wglDXCloseDeviceNV Win32=%lu\n", GetLastError());
        ok = false;
    } else {
        log_line("WGL_DX_CLOSE_DEVICE=PASS");
    }

    rtv->Release();
    texture->Release();
    context->Release();
    device->Release();

    if (!ok) return 17;

    log_line("WGL_NV_DX_INTEROP2_D3D11_PIXEL_PATH=PASS");
    return 0;
}
