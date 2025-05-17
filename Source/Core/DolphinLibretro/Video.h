
#pragma once

#include <libretro.h>
#include "Common/GL/GLContext.h"
#include "DolphinLibretro/Options.h"
#include "VideoBackends/Null/NullGfx.h"
#include "VideoBackends/Software/SWOGLWindow.h"
#include "VideoBackends/Software/SWGfx.h"
#include "VideoBackends/Software/SWTexture.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#ifdef _WIN32
#include "VideoBackends/D3D/D3DBase.h"
#include "VideoBackends/D3D/D3DState.h"
#include "VideoBackends/D3D/DXShader.h"
#include "VideoBackends/D3D/DXTexture.h"
#include "VideoBackends/D3D/Render.h"
#include "VideoBackends/D3D/SwapChain.h"
#endif

namespace Libretro
{
namespace Video
{
void Init(void);
extern retro_video_refresh_t video_cb;
extern struct retro_hw_render_callback hw_render;

/* retroGL interface*/
class RGLContext : public GLContext
{
public:
  RGLContext()
  {
    WindowSystemInfo wsi(WindowSystemType::Libretro, nullptr, nullptr, nullptr);
    Initialize(wsi, g_Config.stereo_mode == StereoMode::QuadBuffer, true);
  }
  bool IsHeadless() const override { return false; }
  void Swap() override
  {
    Libretro::Video::video_cb(RETRO_HW_FRAME_BUFFER_VALID, m_backbuffer_width, m_backbuffer_height,
                              0);
  }
  void* GetFuncAddress(const std::string& name) override
  {
    return (void*)Libretro::Video::hw_render.get_proc_address(name.c_str());
  }
  virtual bool Initialize(const WindowSystemInfo& wsi, bool stereo, bool core) override
  {
    m_backbuffer_width = EFB_WIDTH * Libretro::Options::efbScale;
    m_backbuffer_height = EFB_HEIGHT * Libretro::Options::efbScale;
    switch (Libretro::Video::hw_render.context_type)
    {
    case RETRO_HW_CONTEXT_OPENGLES3:
      m_opengl_mode = Mode::OpenGLES;
      break;
    default:
    case RETRO_HW_CONTEXT_OPENGL_CORE:
    case RETRO_HW_CONTEXT_OPENGL:
      m_opengl_mode = Mode::OpenGL;
      break;
    }

    return true;
  }
};

class SWGfx : public SW::SWGfx
{
public:
  SWGfx()
    : SW::SWGfx(SWOGLWindow::Create(
            WindowSystemInfo(WindowSystemType::Libretro, nullptr, nullptr, nullptr)))
  {
  }
  void ShowImage(const AbstractTexture* source_texture,
                 const MathUtil::Rectangle<int>& source_rc) override
  {
    SW::SWGfx::ShowImage(source_texture, source_rc);
    video_cb(
      static_cast<const SW::SWTexture*>(source_texture)->GetData(0, 0),
      source_rc.GetWidth(),
      source_rc.GetHeight(),
      source_texture->GetWidth() * 4
    );
    UpdateActiveConfig();
  }
};

class NullGfx : public Null::NullGfx
{
public:
  void ShowImage(const AbstractTexture* source_texture,
                 const MathUtil::Rectangle<int>& source_rc) override
  {
    video_cb(NULL, 512, 512, 512 * 4);
    UpdateActiveConfig();
  }
};

#ifdef _WIN32
class DX11SwapChain : public DX11::SwapChain
{
public:
  DX11SwapChain(const WindowSystemInfo& wsi, int width, int height)
      : DX11::SwapChain(wsi, nullptr, nullptr)
  {
    m_width = width;
    m_height = height;
    m_stereo = WantsStereo();
    CreateSwapChainBuffers();
  }

  bool Present() override
  {
    ID3D11ShaderResourceView* srv = m_texture->GetD3DSRV();

    ID3D11RenderTargetView* nullView = nullptr;
    DX11::D3D::context->OMSetRenderTargets(1, &nullView, nullptr);
    DX11::D3D::context->PSSetShaderResources(0, 1, &srv);
    Libretro::Video::video_cb(RETRO_HW_FRAME_BUFFER_VALID, m_width, m_height, m_width);
    DX11::D3D::stateman->Restore();
    return true;
  }

protected:
  bool CreateSwapChainBuffers() override
  {
    TextureConfig config(m_width, m_height, 1, 1, 1, AbstractTextureFormat::RGBA8,
                         AbstractTextureFlag_RenderTarget);

    m_texture = DX11::DXTexture::Create(config);
    m_framebuffer = DX11::DXFramebuffer::Create(m_texture.get(), nullptr);
    return true;
  }
};

#endif
}  // namespace Video
}  // namespace Libretro
