#include <windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <cstdio>
#include <cwchar>
#include <cstdint>

static const wchar_t* hrhex(HRESULT hr, wchar_t* buf, size_t n) {
    swprintf_s(buf, n, L"0x%08X", static_cast<unsigned>(hr));
    return buf;
}

static D3D_DRIVER_TYPE parse_driver(const wchar_t* s) {
    return (_wcsicmp(s, L"warp") == 0) ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE;
}

static const wchar_t* driver_name(D3D_DRIVER_TYPE t) {
    return t == D3D_DRIVER_TYPE_WARP ? L"WARP" : L"HARDWARE";
}

static int child_main(DWORD sourcePid, uintptr_t sourceHandleValue,
                      UINT expectedWidth, UINT expectedHeight,
                      D3D_DRIVER_TYPE driverType) {
    HANDLE sourceProcess = nullptr;
    HANDLE duplicated = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11Device1* device1 = nullptr;
    ID3D11Texture2D* texture = nullptr;
    wchar_t hbuf[32];

    sourceProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, sourcePid);
    if (!sourceProcess) {
        std::fwprintf(stderr, L"CHILD_FAIL OpenProcess Win32=%lu\n", GetLastError());
        return 20;
    }

    HANDLE sourceHandle = reinterpret_cast<HANDLE>(sourceHandleValue);
    if (!DuplicateHandle(sourceProcess, sourceHandle, GetCurrentProcess(),
                         &duplicated, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        std::fwprintf(stderr, L"CHILD_FAIL DuplicateHandle Win32=%lu\n", GetLastError());
        CloseHandle(sourceProcess);
        return 21;
    }

    D3D_FEATURE_LEVEL featureLevel{};
    HRESULT hr = D3D11CreateDevice(
        nullptr, driverType, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION,
        &device, &featureLevel, &context);
    if (FAILED(hr)) {
        std::fwprintf(stderr, L"CHILD_FAIL D3D11CreateDevice driver=%ls hr=%ls\n",
                      driver_name(driverType), hrhex(hr, hbuf, 32));
        CloseHandle(duplicated);
        CloseHandle(sourceProcess);
        return 22;
    }

    hr = device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&device1));
    if (FAILED(hr) || !device1) {
        std::fwprintf(stderr, L"CHILD_FAIL QueryInterface(ID3D11Device1) hr=%ls\n",
                      hrhex(hr, hbuf, 32));
        context->Release();
        device->Release();
        CloseHandle(duplicated);
        CloseHandle(sourceProcess);
        return 23;
    }

    // Mirror CORE UI's raw COM ABI call: ID3D11Device1::OpenSharedResource1
    // through vtable slot 48.
    using OpenSharedResource1Fn =
        HRESULT (STDMETHODCALLTYPE*)(ID3D11Device1*, HANDLE, REFIID, void**);
    void** deviceVtable = *reinterpret_cast<void***>(device1);
    auto openSharedResource1 =
        reinterpret_cast<OpenSharedResource1Fn>(deviceVtable[48]);

    hr = openSharedResource1(
        device1, duplicated, __uuidof(ID3D11Texture2D),
        reinterpret_cast<void**>(&texture));
    if (FAILED(hr) || !texture) {
        std::fwprintf(stderr, L"CHILD_FAIL OpenSharedResource1(slot=48) hr=%ls\n",
                      hrhex(hr, hbuf, 32));
        device1->Release();
        context->Release();
        device->Release();
        CloseHandle(duplicated);
        CloseHandle(sourceProcess);
        return 24;
    }

    // Mirror CORE UI's raw COM ABI call: ID3D11Texture2D::GetDesc at slot 10.
    using GetDescFn =
        void (STDMETHODCALLTYPE*)(ID3D11Texture2D*, D3D11_TEXTURE2D_DESC*);
    void** textureVtable = *reinterpret_cast<void***>(texture);
    auto getDesc = reinterpret_cast<GetDescFn>(textureVtable[10]);
    D3D11_TEXTURE2D_DESC desc{};
    getDesc(texture, &desc);

    bool ok =
        desc.Width == expectedWidth &&
        desc.Height == expectedHeight &&
        desc.MipLevels == 1 &&
        desc.ArraySize == 1 &&
        desc.SampleDesc.Count == 1;

    std::wprintf(
        L"CHILD_DESC driver=%ls featureLevel=0x%X size=%ux%u mip=%u array=%u "
        L"samples=%u format=%u bind=0x%X misc=0x%X\n",
        driver_name(driverType), static_cast<unsigned>(featureLevel),
        desc.Width, desc.Height, desc.MipLevels, desc.ArraySize,
        desc.SampleDesc.Count, static_cast<unsigned>(desc.Format),
        desc.BindFlags, desc.MiscFlags);

    texture->Release();
    device1->Release();
    context->Release();
    device->Release();
    CloseHandle(duplicated);
    CloseHandle(sourceProcess);

    if (!ok) {
        std::fwprintf(stderr, L"CHILD_FAIL descriptor mismatch\n");
        return 25;
    }

    std::wprintf(L"NATIVE_DUPLICATE_OPEN_SLOT48_GETDESC_SLOT10=PASS driver=%ls\n",
                 driver_name(driverType));
    return 0;
}

static int parent_main(const wchar_t* exe, D3D_DRIVER_TYPE driverType) {
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11Texture2D* texture = nullptr;
    IDXGIResource1* dxgiResource1 = nullptr;
    HANDLE sharedHandle = nullptr;
    wchar_t hbuf[32];

    D3D_FEATURE_LEVEL featureLevel{};
    HRESULT hr = D3D11CreateDevice(
        nullptr, driverType, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION,
        &device, &featureLevel, &context);
    if (FAILED(hr)) {
        std::fwprintf(stderr, L"PARENT_DEVICE_UNAVAILABLE driver=%ls hr=%ls\n",
                      driver_name(driverType), hrhex(hr, hbuf, 32));
        return 10;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = 854;
    desc.Height = 480;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags =
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
        D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    hr = device->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr) || !texture) {
        std::fwprintf(stderr, L"PARENT_FAIL CreateTexture2D driver=%ls hr=%ls\n",
                      driver_name(driverType), hrhex(hr, hbuf, 32));
        context->Release();
        device->Release();
        return 11;
    }

    hr = texture->QueryInterface(
        __uuidof(IDXGIResource1), reinterpret_cast<void**>(&dxgiResource1));
    if (FAILED(hr) || !dxgiResource1) {
        std::fwprintf(stderr, L"PARENT_FAIL QueryInterface(IDXGIResource1) hr=%ls\n",
                      hrhex(hr, hbuf, 32));
        texture->Release();
        context->Release();
        device->Release();
        return 12;
    }

    hr = dxgiResource1->CreateSharedHandle(
        nullptr,
        DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr,
        &sharedHandle);
    if (FAILED(hr) || !sharedHandle) {
        std::fwprintf(stderr, L"PARENT_FAIL CreateSharedHandle driver=%ls hr=%ls\n",
                      driver_name(driverType), hrhex(hr, hbuf, 32));
        dxgiResource1->Release();
        texture->Release();
        context->Release();
        device->Release();
        return 13;
    }

    std::wprintf(
        L"PARENT_SHARED_NTHANDLE driver=%ls pid=%lu handle=%llu featureLevel=0x%X\n",
        driver_name(driverType), GetCurrentProcessId(),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(sharedHandle)),
        static_cast<unsigned>(featureLevel));

    wchar_t cmd[4096];
    swprintf_s(
        cmd, L"\"%ls\" --child %lu %llu %u %u %ls",
        exe, GetCurrentProcessId(),
        static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(sharedHandle)),
        desc.Width, desc.Height, driver_name(driverType));

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    BOOL created = CreateProcessW(
        nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
    if (!created) {
        std::fwprintf(stderr, L"PARENT_FAIL CreateProcess Win32=%lu\n", GetLastError());
        CloseHandle(sharedHandle);
        dxgiResource1->Release();
        texture->Release();
        context->Release();
        device->Release();
        return 14;
    }

    DWORD wait = WaitForSingleObject(pi.hProcess, 30000);
    DWORD exitCode = 999;
    if (wait == WAIT_OBJECT_0) {
        GetExitCodeProcess(pi.hProcess, &exitCode);
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    CloseHandle(sharedHandle);
    dxgiResource1->Release();
    texture->Release();
    context->Release();
    device->Release();

    if (wait != WAIT_OBJECT_0) {
        std::fwprintf(stderr, L"PARENT_FAIL child wait=%lu driver=%ls\n",
                      wait, driver_name(driverType));
        return 15;
    }
    if (exitCode != 0) {
        std::fwprintf(stderr, L"PARENT_FAIL child exit=%lu driver=%ls\n",
                      exitCode, driver_name(driverType));
        return static_cast<int>(exitCode);
    }

    std::wprintf(L"NATIVE_CROSS_PROCESS_D3D_NTHANDLE=PASS driver=%ls\n",
                 driver_name(driverType));
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    if (argc >= 7 && _wcsicmp(argv[1], L"--child") == 0) {
        DWORD pid = static_cast<DWORD>(_wcstoui64(argv[2], nullptr, 10));
        uintptr_t handleValue =
            static_cast<uintptr_t>(_wcstoui64(argv[3], nullptr, 10));
        UINT width = static_cast<UINT>(_wcstoui64(argv[4], nullptr, 10));
        UINT height = static_cast<UINT>(_wcstoui64(argv[5], nullptr, 10));
        D3D_DRIVER_TYPE driver = parse_driver(argv[6]);
        return child_main(pid, handleValue, width, height, driver);
    }

    D3D_DRIVER_TYPE driver =
        (argc >= 2) ? parse_driver(argv[1]) : D3D_DRIVER_TYPE_WARP;

    wchar_t exe[MAX_PATH];
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    return parent_main(exe, driver);
}
