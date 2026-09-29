#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <GL/gl.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

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


static std::string json_raw(const std::string& line, const char* key) {
    const std::string needle = std::string("\"") + key + "\":";
    size_t p = line.find(needle);
    if (p == std::string::npos) return {};
    p += needle.size();
    while (p < line.size() && std::isspace(static_cast<unsigned char>(line[p]))) ++p;
    if (p >= line.size()) return {};
    if (line[p] == '"') {
        ++p;
        std::string out;
        bool esc = false;
        for (; p < line.size(); ++p) {
            char c = line[p];
            if (esc) { out.push_back(c); esc = false; continue; }
            if (c == '\\') { esc = true; continue; }
            if (c == '"') break;
            out.push_back(c);
        }
        return out;
    }
    size_t e = p;
    while (e < line.size() && line[e] != ',' && line[e] != '}') ++e;
    while (e > p && std::isspace(static_cast<unsigned char>(line[e - 1]))) --e;
    return line.substr(p, e - p);
}

static long long json_i64(const std::string& line, const char* key) {
    const std::string raw = json_raw(line, key);
    if (raw.empty()) return 0;
    char* end = nullptr;
    const long long value = _strtoi64(raw.c_str(), &end, 10);
    return end && *end == '\0' ? value : 0;
}

static bool send_all(SOCKET s, const std::string& text) {
    const char* p = text.data();
    int left = static_cast<int>(text.size());
    while (left > 0) {
        const int n = send(s, p, left, 0);
        if (n <= 0) return false;
        p += n;
        left -= n;
    }
    return true;
}

static bool recv_line(SOCKET s, std::string& buffer, std::string& line) {
    for (;;) {
        const size_t nl = buffer.find('\n');
        if (nl != std::string::npos) {
            line = buffer.substr(0, nl);
            buffer.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return true;
        }
        char temp[4096];
        const int n = recv(s, temp, sizeof(temp), 0);
        if (n <= 0) return false;
        buffer.append(temp, temp + n);
        if (buffer.size() > 1024 * 1024) return false;
    }
}

static SOCKET connect_renderer() {
    for (int attempt = 0; attempt < 120; ++attempt) {
        SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == INVALID_SOCKET) return INVALID_SOCKET;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(37541);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0) {
            DWORD timeout = 30000;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            int one = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof(one));
            return s;
        }
        closesocket(s);
        Sleep(250);
    }
    return INVALID_SOCKET;
}

struct TargetGpu {
    ID3D11Device* device = nullptr;
    ID3D11Device1* device1 = nullptr;
    ID3D11DeviceContext* context = nullptr;
    HANDLE interopDevice = nullptr;
    ID3D11Texture2D* owned = nullptr;
    HANDLE interopObject = nullptr;
    GLuint glTexture = 0;
    UINT width = 0;
    UINT height = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    bool open() {
        D3D_FEATURE_LEVEL level{};
        HRESULT hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION, &device, &level, &context);
        if (FAILED(hr) || !device || !context) {
            std::fprintf(stderr, "TARGET_FAIL D3D11CreateDevice hr=0x%08X\n", static_cast<unsigned>(hr));
            return false;
        }
        hr = device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&device1));
        if (FAILED(hr) || !device1) {
            std::fprintf(stderr, "TARGET_FAIL QueryInterface(ID3D11Device1) hr=0x%08X\n", static_cast<unsigned>(hr));
            return false;
        }
        interopDevice = p_wglDXOpenDeviceNV(device);
        if (!interopDevice) {
            std::fprintf(stderr, "TARGET_FAIL wglDXOpenDeviceNV Win32=%lu\n", GetLastError());
            return false;
        }
        std::printf("TARGET_D3D11_DEVICE=PASS featureLevel=0x%X\n", static_cast<unsigned>(level));
        log_line("TARGET_WGL_DX_OPEN_DEVICE=PASS");
        return true;
    }

    void destroy_owned() {
        if (interopObject && interopDevice) {
            p_wglDXUnregisterObjectNV(interopDevice, interopObject);
            interopObject = nullptr;
        }
        if (glTexture) {
            glDeleteTextures(1, &glTexture);
            glTexture = 0;
        }
        if (owned) {
            owned->Release();
            owned = nullptr;
        }
        width = height = 0;
        format = DXGI_FORMAT_UNKNOWN;
    }

    bool ensure_owned(const D3D11_TEXTURE2D_DESC& src) {
        if (owned && width == src.Width && height == src.Height && format == src.Format) return true;
        destroy_owned();
        D3D11_TEXTURE2D_DESC desc = src;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;
        desc.MiscFlags = 0;
        HRESULT hr = device->CreateTexture2D(&desc, nullptr, &owned);
        if (FAILED(hr) || !owned) {
            std::fprintf(stderr, "TARGET_FAIL CreateTexture2D(owned) hr=0x%08X\n", static_cast<unsigned>(hr));
            return false;
        }
        glGenTextures(1, &glTexture);
        glBindTexture(GL_TEXTURE_2D, glTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
        interopObject = p_wglDXRegisterObjectNV(
            interopDevice, owned, glTexture, GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);
        if (!interopObject) {
            std::fprintf(stderr, "TARGET_FAIL wglDXRegisterObjectNV Win32=%lu\n", GetLastError());
            destroy_owned();
            return false;
        }
        width = src.Width;
        height = src.Height;
        format = src.Format;
        std::printf("TARGET_OWNED_INTEROP_TEXTURE=PASS size=%ux%u format=%u\n",
                    width, height, static_cast<unsigned>(format));
        return true;
    }

    bool import_present(DWORD sourcePid, uintptr_t sourceHandle, UINT expectedWidth, UINT expectedHeight) {
        HANDLE process = OpenProcess(PROCESS_DUP_HANDLE, FALSE, sourcePid);
        if (!process) {
            std::fprintf(stderr, "TARGET_FAIL OpenProcess Win32=%lu\n", GetLastError());
            return false;
        }
        HANDLE duplicated = nullptr;
        if (!DuplicateHandle(process, reinterpret_cast<HANDLE>(sourceHandle), GetCurrentProcess(),
                             &duplicated, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            std::fprintf(stderr, "TARGET_FAIL DuplicateHandle Win32=%lu\n", GetLastError());
            CloseHandle(process);
            return false;
        }
        CloseHandle(process);

        ID3D11Texture2D* source = nullptr;
        using OpenSharedResource1Fn = HRESULT (STDMETHODCALLTYPE*)(ID3D11Device1*, HANDLE, REFIID, void**);
        auto openSharedResource1 = reinterpret_cast<OpenSharedResource1Fn>(
            (*reinterpret_cast<void***>(device1))[48]);
        HRESULT hr = openSharedResource1(
            device1, duplicated, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&source));
        if (FAILED(hr) || !source) {
            std::fprintf(stderr, "TARGET_FAIL OpenSharedResource1(slot48) hr=0x%08X\n", static_cast<unsigned>(hr));
            CloseHandle(duplicated);
            return false;
        }

        using GetDescFn = void (STDMETHODCALLTYPE*)(ID3D11Texture2D*, D3D11_TEXTURE2D_DESC*);
        auto getDesc = reinterpret_cast<GetDescFn>((*reinterpret_cast<void***>(source))[10]);
        D3D11_TEXTURE2D_DESC desc{};
        getDesc(source, &desc);
        std::printf("TARGET_FRAME_DESC size=%ux%u mip=%u array=%u samples=%u format=%u bind=0x%X misc=0x%X\n",
                    desc.Width, desc.Height, desc.MipLevels, desc.ArraySize, desc.SampleDesc.Count,
                    static_cast<unsigned>(desc.Format), desc.BindFlags, desc.MiscFlags);
        if (desc.Width != expectedWidth || desc.Height != expectedHeight ||
            desc.MipLevels != 1 || desc.ArraySize != 1 || desc.SampleDesc.Count != 1) {
            std::fprintf(stderr, "TARGET_FAIL frame descriptor mismatch\n");
            source->Release();
            CloseHandle(duplicated);
            return false;
        }
        if (!ensure_owned(desc)) {
            source->Release();
            CloseHandle(duplicated);
            return false;
        }

        using CopyResourceFn = void (STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, ID3D11Resource*);
        auto copyResource = reinterpret_cast<CopyResourceFn>((*reinterpret_cast<void***>(context))[47]);
        copyResource(context, owned, source);
        source->Release();
        CloseHandle(duplicated);
        log_line("TARGET_SOURCE_RELEASED_AFTER_QUEUED_COPY=PASS");

        HANDLE objects[1] = { interopObject };
        if (!p_wglDXLockObjectsNV(interopDevice, 1, objects)) {
            std::fprintf(stderr, "TARGET_FAIL wglDXLockObjectsNV Win32=%lu\n", GetLastError());
            return false;
        }

        std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4u);
        glBindTexture(GL_TEXTURE_2D, glTexture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels.data());
        const GLenum glErr = glGetError();
        glBindTexture(GL_TEXTURE_2D, 0);
        const BOOL unlockOk = p_wglDXUnlockObjectsNV(interopDevice, 1, objects);
        if (glErr != GL_NO_ERROR || !unlockOk) {
            std::fprintf(stderr, "TARGET_FAIL WGL read/unlock gl=0x%X Win32=%lu\n",
                         static_cast<unsigned>(glErr), GetLastError());
            return false;
        }
        const size_t center = ((static_cast<size_t>(height) / 2u) * width + width / 2u) * 4u;
        if (center + 3 < pixels.size()) {
            std::printf("TARGET_CENTER_PIXEL bgra=%u,%u,%u,%u\n",
                        pixels[center], pixels[center+1], pixels[center+2], pixels[center+3]);
        }
        log_line("TARGET_WGL_LOCK_READ_UNLOCK=PASS");
        return true;
    }

    ~TargetGpu() {
        destroy_owned();
        if (interopDevice) p_wglDXCloseDeviceNV(interopDevice);
        if (device1) device1->Release();
        if (context) context->Release();
        if (device) device->Release();
    }
};

static bool send_pause(SOCKET s, int generation, int width, int height) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"type\":\"state\",\"screen\":\"pause\",\"session\":1,\"generation\":%d,\"width\":%d,\"height\":%d}\n",
        generation, width, height);
    return send_all(s, buf);
}

static bool send_viewport(SOCKET s, int generation, int width, int height) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"type\":\"viewport\",\"session\":1,\"generation\":%d,\"width\":%d,\"height\":%d}\n",
        generation, width, height);
    return send_all(s, buf);
}

static bool send_ack(SOCKET s, long long id, int generation) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"type\":\"frame_ack\",\"id\":%lld,\"session\":1,\"generation\":%d,\"ok\":true,\"detail\":\"target-probe\"}\n",
        id, generation);
    return send_all(s, buf);
}

int main() {
    log_line("CORE_UI_TARGET_RENDERER_PROBE=START");
    WinGl gl;
    if (!gl.open()) return 10;
    bool unsupported = false;
    if (!load_interop(gl.dc, unsupported)) {
        if (unsupported) log_line("TARGET_ENV_UNSUPPORTED missing=WGL_NV_DX_interop2");
        return unsupported ? 2 : 11;
    }

    TargetGpu gpu;
    if (!gpu.open()) return 12;

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 13;
    SOCKET s = connect_renderer();
    if (s == INVALID_SOCKET) {
        std::fprintf(stderr, "TARGET_FAIL connect renderer WSA=%d\n", WSAGetLastError());
        WSACleanup();
        return 14;
    }
    log_line("TARGET_RENDERER_TCP_CONNECT=PASS");

    std::string buffer;
    std::string line;
    if (!recv_line(s, buffer, line)) {
        closesocket(s); WSACleanup(); return 15;
    }
    std::printf("TARGET_RX %s\n", line.c_str());
    if (json_raw(line, "type") != "ready" || json_i64(line, "protocol") != 11) {
        std::fprintf(stderr, "TARGET_FAIL renderer protocol/ready mismatch\n");
        closesocket(s); WSACleanup(); return 16;
    }
    log_line("TARGET_RENDERER_PROTOCOL11=PASS");

    char hello[160];
    std::snprintf(hello, sizeof(hello),
        "{\"type\":\"hello\",\"protocol\":11,\"pid\":%lu}\n", GetCurrentProcessId());
    if (!send_all(s, hello) || !send_pause(s, 1, 854, 480)) {
        closesocket(s); WSACleanup(); return 17;
    }

    const int widths[3] = {854,1280,854};
    const int heights[3] = {480,720,480};
    int generation = 1;
    int passed = 0;

    while (passed < 3) {
        if (!recv_line(s, buffer, line)) {
            std::fprintf(stderr, "TARGET_FAIL renderer timeout generation=%d WSA=%d\n", generation, WSAGetLastError());
            closesocket(s); WSACleanup(); return 18;
        }
        std::printf("TARGET_RX %s\n", line.c_str());
        const std::string type = json_raw(line, "type");
        if (type == "renderer_error") {
            std::fprintf(stderr, "TARGET_FAIL renderer_error %s\n", json_raw(line, "message").c_str());
            closesocket(s); WSACleanup(); return 19;
        }
        if (type != "frame") continue;

        const long long session = json_i64(line, "session");
        const long long gen = json_i64(line, "generation");
        const long long id = json_i64(line, "id");
        const long long pid = json_i64(line, "pid");
        const long long width = json_i64(line, "width");
        const long long height = json_i64(line, "height");
        const std::string handleText = json_raw(line, "handle");
        const std::string format = json_raw(line, "format");

        if (session != 1 || gen != generation || id <= 0 || pid <= 0 ||
            width != widths[passed] || height != heights[passed] ||
            handleText.empty() || (format != "rgba" && format != "bgra")) {
            std::fprintf(stderr, "TARGET_FAIL frame metadata mismatch generation=%d\n", generation);
            closesocket(s); WSACleanup(); return 20;
        }

        char* end = nullptr;
        const unsigned long long handle = _strtoui64(handleText.c_str(), &end, 10);
        if (!end || *end != '\0' || handle == 0 ||
            !gpu.import_present(static_cast<DWORD>(pid), static_cast<uintptr_t>(handle),
                                static_cast<UINT>(width), static_cast<UINT>(height))) {
            closesocket(s); WSACleanup(); return 21;
        }
        if (!send_ack(s, id, generation)) {
            closesocket(s); WSACleanup(); return 22;
        }

        std::printf("TARGET_GENERATION_%d_FRAME_PRESENT=PASS size=%lldx%lld format=%s\n",
                    generation, width, height, format.c_str());
        ++passed;
        if (passed == 1) {
            generation = 2;
            if (!send_viewport(s, generation, 1280, 720)) return 23;
        } else if (passed == 2) {
            generation = 3;
            if (!send_viewport(s, generation, 854, 480)) return 24;
        }
    }

    send_all(s, "{\"type\":\"state\",\"screen\":\"game\",\"session\":1,\"generation\":3}\n");
    log_line("TARGET_RESIZE_GENERATION_SEQUENCE=854x480->1280x720->854x480 PASS");
    log_line("CORE_UI_0312_EXACT_RENDERER_D3D11_WGL_PRESENTATION=PASS");
    closesocket(s);
    WSACleanup();
    return 0;
}
