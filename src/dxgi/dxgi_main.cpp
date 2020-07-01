#include "dxgi_factory.h"
#include "dxgi_include.h"

namespace dxvk {
  
  Logger Logger::s_instance("dxgi.log");

  std::array<const WCHAR*, 3> g_blacklistedModules = {
    L"amdvlk64.dll",
    L"amdvlk32.dll",
    L"vulkan-1.dll"
  };

  static HMODULE LoadSystemDXGI() {
    WCHAR systemPath[MAX_PATH];
    GetSystemDirectoryW(systemPath, MAX_PATH);
    wcsncat(systemPath, L"\\dxgi.dll", MAX_PATH);

    return LoadLibraryW(systemPath);
  }

  template <typename T, typename... Args>
  HRESULT ForwardCall(const char* call, Args... args) {
    static HMODULE module = LoadSystemDXGI();
    return reinterpret_cast<T>(GetProcAddress(module, call))(args...);
  }

  template <typename T, typename... Args>
  std::pair<BOOL, HRESULT> ForwardCallBlacklist(const char* call, void* returnAddress, Args... args) {
    MEMORY_BASIC_INFORMATION mbi;
    if (::VirtualQuery(returnAddress, &mbi, sizeof(mbi))) {
      HMODULE module = reinterpret_cast<HMODULE>(mbi.AllocationBase);

      WCHAR moduleName[MAX_PATH] = {};
      ::GetModuleFileNameW(module, moduleName, MAX_PATH);
      for (auto blacklistedModule : g_blacklistedModules) {
        if (wcsstr(moduleName, blacklistedModule) != nullptr)
          return std::make_pair(TRUE, ForwardCall<T>(call, args...));
      }
    }

    return std::make_pair(FALSE, S_OK);
  }
  
  HRESULT createDxgiFactory(UINT Flags, REFIID riid, void **ppFactory) {
    try {
      Com<DxgiFactory> factory = new DxgiFactory(Flags);
      HRESULT hr = factory->QueryInterface(riid, ppFactory);

      if (FAILED(hr))
        return hr;
      
      return S_OK;
    } catch (const DxvkError& e) {
      Logger::err(e.message());
      return E_FAIL;
    }
  }
}

using PFN_CreateDXGIFactory2 = HRESULT(__stdcall*)(UINT, REFIID, void**);
using PFN_CreateDXGIFactory1 = HRESULT(__stdcall*)(REFIID, void**);
using PFN_CreateDXGIFactory  = HRESULT(__stdcall*)(REFIID, void**);

#ifdef _MSC_VER
#define RETURN_ADDRESS() _ReturnAddress()
#else
#define RETURN_ADDRESS() __builtin_return_address(0)
#endif

#define FORWARD_CALL(name, ...)                                                                        \
  auto forwardResult = ::dxvk::ForwardCallBlacklist<PFN_##name>(#name, RETURN_ADDRESS(), __VA_ARGS__); \
  if (forwardResult.first) return forwardResult.second;                                                \

extern "C" {
  DLLEXPORT HRESULT __stdcall CreateDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory) {
    FORWARD_CALL(CreateDXGIFactory2, Flags, riid, ppFactory);
    dxvk::Logger::warn("CreateDXGIFactory2: Ignoring flags");
    return dxvk::createDxgiFactory(Flags, riid, ppFactory);
  }

  DLLEXPORT HRESULT __stdcall CreateDXGIFactory1(REFIID riid, void **ppFactory) {
    FORWARD_CALL(CreateDXGIFactory1, riid, ppFactory);
    return dxvk::createDxgiFactory(0, riid, ppFactory);
  }
  
  DLLEXPORT HRESULT __stdcall CreateDXGIFactory(REFIID riid, void **ppFactory) {
    FORWARD_CALL(CreateDXGIFactory, riid, ppFactory);
    return dxvk::createDxgiFactory(0, riid, ppFactory);
  }

  DLLEXPORT HRESULT __stdcall DXGIDeclareAdapterRemovalSupport() {
    static bool enabled = false;

    if (std::exchange(enabled, true))
      return 0x887a0036; // DXGI_ERROR_ALREADY_EXISTS;

    dxvk::Logger::warn("DXGIDeclareAdapterRemovalSupport: Stub");
    return S_OK;
  }
}