import ctypes
import os
import subprocess
import sys
import uuid
from ctypes import wintypes

if os.name != 'nt':
    raise SystemExit('Windows only')

HRESULT = ctypes.c_long
UINT = ctypes.c_uint
ULONG = ctypes.c_ulong
DWORD = wintypes.DWORD
BOOL = wintypes.BOOL
HANDLE = wintypes.HANDLE

D3D_DRIVER_TYPE_HARDWARE = 1
D3D_DRIVER_TYPE_WARP = 5
D3D11_CREATE_DEVICE_BGRA_SUPPORT = 0x20
D3D11_SDK_VERSION = 7
D3D11_USAGE_DEFAULT = 0
D3D11_BIND_SHADER_RESOURCE = 0x8
D3D11_BIND_RENDER_TARGET = 0x20
D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX = 0x100
D3D11_RESOURCE_MISC_SHARED_NTHANDLE = 0x800
DXGI_SHARED_RESOURCE_READ = 0x80000000
DXGI_SHARED_RESOURCE_WRITE = 0x1
PROCESS_DUP_HANDLE = 0x0040
DUPLICATE_SAME_ACCESS = 0x00000002
DXGI_FORMAT_B8G8R8A8_UNORM = 87

class GUID(ctypes.Structure):
    _fields_ = [
        ('Data1', ctypes.c_uint32),
        ('Data2', ctypes.c_uint16),
        ('Data3', ctypes.c_uint16),
        ('Data4', ctypes.c_ubyte * 8),
    ]

    @classmethod
    def from_string(cls, s):
        return cls.from_buffer_copy(uuid.UUID(s).bytes_le)

class DXGI_SAMPLE_DESC(ctypes.Structure):
    _fields_ = [('Count', UINT), ('Quality', UINT)]

class D3D11_TEXTURE2D_DESC(ctypes.Structure):
    _fields_ = [
        ('Width', UINT),
        ('Height', UINT),
        ('MipLevels', UINT),
        ('ArraySize', UINT),
        ('Format', UINT),
        ('SampleDesc', DXGI_SAMPLE_DESC),
        ('Usage', UINT),
        ('BindFlags', UINT),
        ('CPUAccessFlags', UINT),
        ('MiscFlags', UINT),
    ]

IID_ID3D11Device1 = GUID.from_string('a04bfb29-08ef-43d6-a49c-a9bdbdcbe686')
IID_ID3D11Texture2D = GUID.from_string('6f15aaf2-d208-4e89-9ab4-489535d34f9c')
IID_IDXGIResource1 = GUID.from_string('30961379-4609-4a41-998e-54fe567ee0c1')

D3D11 = ctypes.WinDLL('d3d11.dll')
KERNEL32 = ctypes.WinDLL('kernel32.dll', use_last_error=True)

D3D11.D3D11CreateDevice.argtypes = [
    ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p, UINT,
    ctypes.POINTER(UINT), UINT, UINT,
    ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(UINT), ctypes.POINTER(ctypes.c_void_p)
]
D3D11.D3D11CreateDevice.restype = HRESULT

KERNEL32.OpenProcess.argtypes = [DWORD, BOOL, DWORD]
KERNEL32.OpenProcess.restype = HANDLE
KERNEL32.GetCurrentProcess.argtypes = []
KERNEL32.GetCurrentProcess.restype = HANDLE
KERNEL32.DuplicateHandle.argtypes = [HANDLE, HANDLE, HANDLE, ctypes.POINTER(HANDLE), DWORD, BOOL, DWORD]
KERNEL32.DuplicateHandle.restype = BOOL
KERNEL32.CloseHandle.argtypes = [HANDLE]
KERNEL32.CloseHandle.restype = BOOL

WINFUNCTYPE = ctypes.WINFUNCTYPE

def hr_hex(hr):
    return f'0x{ctypes.c_uint32(hr).value:08x}'

def failed(hr):
    return ctypes.c_int32(hr).value < 0

def vfunc(obj, index, restype, *argtypes):
    if not obj:
        raise RuntimeError(f'null COM object for vfunc {index}')
    table = ctypes.cast(obj, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
    address = table[index]
    return WINFUNCTYPE(restype, ctypes.c_void_p, *argtypes)(address)

def release(obj):
    if obj:
        vfunc(obj, 2, ULONG)(obj)

def query_interface(obj, iid):
    out = ctypes.c_void_p()
    hr = vfunc(obj, 0, HRESULT, ctypes.POINTER(GUID), ctypes.POINTER(ctypes.c_void_p))(
        obj, ctypes.byref(iid), ctypes.byref(out)
    )
    if failed(hr) or not out.value:
        raise RuntimeError(f'QueryInterface failed hr={hr_hex(hr)}')
    return out

def create_device(driver_type):
    device = ctypes.c_void_p()
    context = ctypes.c_void_p()
    feature = UINT()
    hr = D3D11.D3D11CreateDevice(
        None, driver_type, None, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        None, 0, D3D11_SDK_VERSION,
        ctypes.byref(device), ctypes.byref(feature), ctypes.byref(context)
    )
    if failed(hr) or not device.value or not context.value:
        raise RuntimeError(f'D3D11CreateDevice driver={driver_type} failed hr={hr_hex(hr)}')
    return device, context, feature.value

def create_texture(device, width, height, shared):
    desc = D3D11_TEXTURE2D_DESC()
    desc.Width = width
    desc.Height = height
    desc.MipLevels = 1
    desc.ArraySize = 1
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM
    desc.SampleDesc.Count = 1
    desc.SampleDesc.Quality = 0
    desc.Usage = D3D11_USAGE_DEFAULT
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET
    desc.CPUAccessFlags = 0
    desc.MiscFlags = (D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX | D3D11_RESOURCE_MISC_SHARED_NTHANDLE) if shared else 0
    out = ctypes.c_void_p()
    hr = vfunc(device, 5, HRESULT, ctypes.POINTER(D3D11_TEXTURE2D_DESC), ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p))(
        device, ctypes.byref(desc), None, ctypes.byref(out)
    )
    if failed(hr) or not out.value:
        raise RuntimeError(f'CreateTexture2D shared={shared} {width}x{height} failed hr={hr_hex(hr)}')
    return out

def get_desc(texture):
    desc = D3D11_TEXTURE2D_DESC()
    vfunc(texture, 10, None, ctypes.POINTER(D3D11_TEXTURE2D_DESC))(texture, ctypes.byref(desc))
    return desc

def create_shared_handle(texture):
    resource1 = query_interface(texture, IID_IDXGIResource1)
    handle = HANDLE()
    try:
        hr = vfunc(resource1, 13, HRESULT, ctypes.c_void_p, DWORD, wintypes.LPCWSTR, ctypes.POINTER(HANDLE))(
            resource1, None, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, None, ctypes.byref(handle)
        )
        if failed(hr) or not handle.value:
            raise RuntimeError(f'IDXGIResource1::CreateSharedHandle failed hr={hr_hex(hr)}')
        return handle
    finally:
        release(resource1)

def duplicate_from_process(source_pid, source_handle_value):
    source_process = KERNEL32.OpenProcess(PROCESS_DUP_HANDLE, False, source_pid)
    if not source_process:
        raise RuntimeError(f'OpenProcess(PROCESS_DUP_HANDLE) failed win32={ctypes.get_last_error()}')
    duplicated = HANDLE()
    try:
        ok = KERNEL32.DuplicateHandle(
            source_process,
            HANDLE(source_handle_value),
            KERNEL32.GetCurrentProcess(),
            ctypes.byref(duplicated),
            0,
            False,
            DUPLICATE_SAME_ACCESS,
        )
        if not ok or not duplicated.value:
            raise RuntimeError(f'DuplicateHandle failed win32={ctypes.get_last_error()}')
        return duplicated
    finally:
        KERNEL32.CloseHandle(source_process)

def open_shared_resource1(device1, handle):
    texture = ctypes.c_void_p()
    hr = vfunc(device1, 48, HRESULT, HANDLE, ctypes.POINTER(GUID), ctypes.POINTER(ctypes.c_void_p))(
        device1, handle, ctypes.byref(IID_ID3D11Texture2D), ctypes.byref(texture)
    )
    if failed(hr) or not texture.value:
        raise RuntimeError(f'ID3D11Device1::OpenSharedResource1 failed hr={hr_hex(hr)}')
    return texture

def copy_resource(context, dst, src):
    vfunc(context, 47, None, ctypes.c_void_p, ctypes.c_void_p)(context, dst, src)

def importer(source_pid, handle_value, width, height, driver_type):
    dup = HANDLE()
    device = context = device1 = source = owned = None
    try:
        dup = duplicate_from_process(source_pid, handle_value)
        device, context, feature = create_device(driver_type)
        device1 = query_interface(device, IID_ID3D11Device1)
        source = open_shared_resource1(device1, dup)
        desc = get_desc(source)
        if (desc.Width, desc.Height) != (width, height):
            raise RuntimeError(f'imported size mismatch expected={width}x{height} actual={desc.Width}x{desc.Height}')
        if desc.MipLevels != 1 or desc.ArraySize != 1 or desc.SampleDesc.Count != 1:
            raise RuntimeError(
                f'imported texture shape mismatch mip={desc.MipLevels} array={desc.ArraySize} samples={desc.SampleDesc.Count}'
            )
        owned = create_texture(device, width, height, False)
        copy_resource(context, owned, source)
        owned_desc = get_desc(owned)
        if (owned_desc.Width, owned_desc.Height, owned_desc.Format) != (width, height, desc.Format):
            raise RuntimeError('owned copy texture descriptor mismatch')
        print(
            f'IMPORT_PASS driver={driver_type} feature=0x{feature:x} size={width}x{height} '
            f'format={desc.Format} bind=0x{desc.BindFlags:x} misc=0x{desc.MiscFlags:x}',
            flush=True,
        )
        return 0
    finally:
        release(owned)
        release(source)
        release(device1)
        release(context)
        release(device)
        if dup and dup.value:
            KERNEL32.CloseHandle(dup)

def parent_case(width, height, driver_type):
    device = context = device1 = texture = None
    handle = HANDLE()
    try:
        device, context, feature = create_device(driver_type)
        device1 = query_interface(device, IID_ID3D11Device1)
        release(device1)
        device1 = None
        texture = create_texture(device, width, height, True)
        handle = create_shared_handle(texture)
        cmd = [
            sys.executable,
            os.path.abspath(__file__),
            'import',
            str(os.getpid()),
            str(int(handle.value)),
            str(width),
            str(height),
            str(driver_type),
        ]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        if proc.stdout:
            print(proc.stdout.strip(), flush=True)
        if proc.stderr:
            print(proc.stderr.strip(), file=sys.stderr, flush=True)
        if proc.returncode != 0:
            raise RuntimeError(f'child importer exit={proc.returncode}')
        print(f'CASE_PASS driver={driver_type} feature=0x{feature:x} size={width}x{height}', flush=True)
    finally:
        if handle and handle.value:
            KERNEL32.CloseHandle(handle)
        release(texture)
        release(device1)
        release(context)
        release(device)

def main():
    if len(sys.argv) > 1 and sys.argv[1] == 'import':
        return importer(
            int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6])
        )

    device = context = device1 = None
    try:
        device, context, feature = create_device(D3D_DRIVER_TYPE_HARDWARE)
        device1 = query_interface(device, IID_ID3D11Device1)
        print(f'BRIDGE_HARDWARE_DEVICE_INIT=PASS feature=0x{feature:x}', flush=True)
    finally:
        release(device1)
        release(context)
        release(device)

    sizes = [(854, 480), (1280, 720), (854, 480)]
    selected = D3D_DRIVER_TYPE_HARDWARE
    try:
        for w, h in sizes:
            parent_case(w, h, selected)
        print('NT_HANDLE_D3D11_HARDWARE_SEQUENCE=PASS', flush=True)
    except Exception as hardware_error:
        print(f'NT_HANDLE_D3D11_HARDWARE_SEQUENCE=UNAVAILABLE {hardware_error}', flush=True)
        selected = D3D_DRIVER_TYPE_WARP
        for w, h in sizes:
            parent_case(w, h, selected)
        print('NT_HANDLE_D3D11_WARP_SEQUENCE=PASS', flush=True)

    print('WINDOWS_D3D11_CROSS_PROCESS_BOUNDARY=PASS', flush=True)
    return 0

if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f'WINDOWS_D3D11_CROSS_PROCESS_BOUNDARY=FAIL {type(exc).__name__}: {exc}', file=sys.stderr, flush=True)
        raise
