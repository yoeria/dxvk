#pragma once

#include "d3d9_subresource.h"

#include "d3d9_common_texture.h"

#include "../util/util_gdi.h"

#include <algorithm>

namespace dxvk {

  // The Surface's container may be a swapchain unlike our
  // other subresource types...
  enum class D3D9SurfaceContainerType {
    Texture,
    Swapchain
  };

  using D3D9GDIDesc = D3DKMT_DESTROYDCFROMMEMORY;

  using D3D9SurfaceBase = D3D9Subresource<IDirect3DSurface9>;
  class D3D9Surface final : public D3D9SurfaceBase {

  public:

    D3D9Surface(
            D3D9DeviceEx*             pDevice,
      const D3D9_COMMON_TEXTURE_DESC* pDesc,
            IUnknown*                 pContainer    = nullptr,
            D3D9SurfaceContainerType  ContainerType = D3D9SurfaceContainerType::Texture);

    D3D9Surface(
            D3D9DeviceEx*             pDevice,
            D3D9CommonTexture*        pTexture,
            UINT                      Face,
            UINT                      MipLevel,
            IUnknown*                 pContainer,
            D3D9SurfaceContainerType  ContainerType = D3D9SurfaceContainerType::Texture);

    void AddRefPrivate();

    void ReleasePrivate();

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);

    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() final;

    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC *pDesc) final;

    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags) final;

    HRESULT STDMETHODCALLTYPE UnlockRect() final;

    HRESULT STDMETHODCALLTYPE GetDC(HDC *phDC) final;

    HRESULT STDMETHODCALLTYPE ReleaseDC(HDC hDC) final;

    inline VkExtent2D GetSurfaceExtent() const {
      const auto* desc = m_texture->Desc();

      return VkExtent2D { 
        std::max(1u, desc->Width  >> GetMipLevel()),
        std::max(1u, desc->Height >> GetMipLevel())
      };
    }

  private:

    D3D9SurfaceContainerType m_containerType;

    D3D9GDIDesc m_dcDesc;

  };
}