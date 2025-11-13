#pragma once

#include <cassert>
#include <libretro.h>
#include <string>
#include <vector>

#include "Common/Logging/Log.h"
#include "Core/PowerPC/PowerPC.h"
#include "DiscIO/Enums.h"
#include "VideoCommon/VideoConfig.h"

namespace Libretro
{
extern retro_environment_t environ_cb;

bool GetVariable(const char* key, const char*& out_value);
template <typename T>
T GetOption(const char* key, T def);

namespace Options
{
void Init();
void SetVariables();
void RegisterCache();
void CheckForUpdatedVariables();

template <typename T>
T GetCached(const char* key, T def = T{});
bool IsUpdated(const char* key);

template <typename T>
class Option
{
public:
  bool Updated()
  {
    if (m_dirty)
    {
      m_dirty = false;

      retro_variable var{m_id};
      T value = m_list.front().second;

      if (::Libretro::environ_cb && ::Libretro::environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
      {
        for (auto option : m_list)
        {
          if (option.first == var.value)
          {
            value = option.second;
            break;
          }
        }
      }

      if (m_value != value)
      {
        m_value = value;
        return true;
      }
    }
    return false;
  }

  operator T() const
  {
    const_cast<Option*>(this)->Updated();
    return m_value;
  }

  template <typename S>
  bool operator==(S value)
  {
    return (T)(*this) == value;
  }

  template <typename S>
  bool operator!=(S value)
  {
    return (T)(*this) != value;
  }

private:
  const char* m_id;
  const char* m_name;
  T m_value;
  bool m_dirty = true;
  std::string m_options;
  std::vector<std::pair<std::string, T>> m_list;
};

inline std::string CPUCoreToString(PowerPC::CPUCore core)
{
  switch (core)
  {
    case PowerPC::CPUCore::Interpreter:
      return "Interpreter";
    case PowerPC::CPUCore::CachedInterpreter:
       return "Cached Interpreter";
    case PowerPC::CPUCore::JIT64:
      return "JIT64";
    case PowerPC::CPUCore::JITARM64:
      return "JIT ARM64";
#ifdef _M_ARM_32
    //case PowerPC::CPUCore::JITARM:
    //  return "JIT ARM32";
#endif
  }

  return "";
}

// ======================================================
// Core
// ======================================================
namespace core {
  constexpr const char CPU_CORE[] = "dolphin_cpu_core";
  constexpr const char CPU_CLOCK_RATE[] = "dolphin_cpu_clock_rate";
  constexpr const char EMULATION_SPEED[] = "dolphin_emulation_speed";
  constexpr const char MAIN_CPU_THREAD[] = "dolphin_main_cpu_thread";
  constexpr const char MAIN_PRECISION_FRAME_TIMING[] = "dolphin_precision_frame_timing";
  constexpr const char FASTMEM[] = "dolphin_fastmem";
  constexpr const char FASTMEM_ARENA[] = "dolphin_fastmem_arena";
  constexpr const char CHEATS_ENABLED[] = "dolphin_cheats_enabled";
  constexpr const char SKIP_GC_BIOS[] = "dolphin_skip_gc_bios";
  constexpr const char LANGUAGE[] = "dolphin_language";
  constexpr const char FAST_DISC_SPEED[] = "dolphin_fast_disc_speed";
  constexpr const char MAIN_MMU[] = "dolphin_main_mmu";
}  // namespace core

// ======================================================
// Audio / DSP
// ======================================================
  namespace audio {
  constexpr const char DSP_HLE[] = "dolphin_dsp_hle";
  constexpr const char DSP_JIT[] = "dolphin_dsp_jit";
  constexpr const char MIXER_RATE[] = "dolphin_mixer_rate";
  constexpr const char CALL_BACK_AUDIO[] = "dolphin_call_back_audio";
}  // namespace audio

// ======================================================
// Interface
// ======================================================
namespace main_interface {
  constexpr const char OSD_ENABLED[] = "dolphin_osd_enabled";
  constexpr const char LOG_LEVEL[] = "dolphin_log_level";
  constexpr const char ENABLE_DEBUGGING[] = "dolphin_debug_mode_enabled";
}  // namespace main_interface

// ======================================================
// System Configuration (SYSCONF)
// ======================================================
namespace sysconf {
  constexpr const char WIDESCREEN[] = "dolphin_widescreen";
  constexpr const char PROGRESSIVE_SCAN[] = "dolphin_progressive_scan";
  constexpr const char PAL60[] = "dolphin_pal60";
  constexpr const char SENSOR_BAR_POSITION[] = "dolphin_sensor_bar_position";
  constexpr const char ENABLE_RUMBLE[] = "dolphin_enable_rumble";
  constexpr const char WIIMOTE_CONTINUOUS_SCANNING[] = "dolphin_wiimote_continuous_scanning";
  constexpr const char ALT_GC_PORTS_ON_WII[] = "dolphin_alt_gc_ports_on_wii";
}  // namespace sysconf

// ======================================================
// Graphics > Hardware
// ======================================================
namespace gfx_hardware {
//  constexpr const char VSYNC[] = "dolphin_vysnc";
}  // namespace gfx_hardware

// ======================================================
// Graphics > Settings
// ======================================================
namespace gfx_settings {
  constexpr const char RENDERER[] = "dolphin_renderer";
  constexpr const char WIDESCREEN_HACK[] = "dolphin_widescreen_hack";
  constexpr const char EFB_SCALE[] = "dolphin_efb_scale";
  constexpr const char SHADER_COMPILATION_MODE[] = "dolphin_shader_compilation_mode";
  constexpr const char WAIT_FOR_SHADERS[] = "dolphin_wait_for_shaders";
  constexpr const char ANTI_ALIASING[] = "dolphin_anti_aliasing";
  constexpr const char TEXTURE_CACHE_ACCURACY[] = "dolphin_texture_cache_accuracy";
  constexpr const char GPU_TEXTURE_DECODING[] = "dolphin_gpu_texture_decoding";
  constexpr const char ENABLE_PIXEL_LIGHTING[] = "dolphin_pixel_lighting";
  constexpr const char FAST_DEPTH_CALCULATION[] = "dolphin_fast_depth_calculation";
  constexpr const char DISABLE_FOG[] = "dolphin_disable_fog";
}  // namespace gfx_settings

// ======================================================
// Graphics > Enhancements
// ======================================================
namespace gfx_enhancements {
  constexpr const char MAX_ANISOTROPY[] = "dolphin_max_anisotropy";
  constexpr const char FORCE_TEXTURE_FILTERING_MODE[] = "dolphin_force_texture_filtering_mode";
  constexpr const char LOAD_CUSTOM_TEXTURES[] = "dolphin_load_custom_textures";
  constexpr const char CACHE_CUSTOM_TEXTURES[] = "dolphin_cache_custom_textures";
  constexpr const char GFX_ENHANCE_OUTPUT_RESAMPLING[] = "dolphin_enhance_output_resampling";
  constexpr const char FORCE_TRUE_COLOR[] = "dolphin_force_true_color";
  constexpr const char GFX_ENHANCE_DISABLE_COPY_FILTER[] = "dolphin_disable_copy_filter";
  constexpr const char GFX_ENHANCE_HDR_OUTPUT[] = "dolphin_enhance_hdr_output";
  constexpr const char GFX_ARBITRARY_MIPMAP_DETECTION[] = "dolphin_mipmap_detection";
}  // namespace gfx_enhancements

// ======================================================
// Graphics > Hacks
// ======================================================
namespace gfx_hacks {
  constexpr const char EFB_ACCESS_ENABLE[] = "dolphin_efb_access_enable";
  constexpr const char EFB_ACCESS_DEFER_INVALIDATION[] = "dolphin_efb_access_defer_invalidation";
  constexpr const char EFB_ACCESS_TILE_SIZE[] = "dolphin_efb_access_tile_size";
  constexpr const char BBOX_ENABLED[] = "dolphin_bbox_enabled";
  constexpr const char FORCE_PROGRESSIVE[] = "dolphin_force_progressive";
  constexpr const char EFB_TO_TEXTURE[] = "dolphin_efb_to_texture";
  constexpr const char XFB_TO_TEXTURE_ENABLE[] = "dolphin_xfb_to_texture_enable";
  constexpr const char EFB_TO_VRAM[] = "dolphin_efb_to_vram";
  constexpr const char DEFER_EFB_COPIES[] = "dolphin_defer_efb_copies";
  constexpr const char IMMEDIATE_XFB[] = "dolphin_immediate_xfb";
  constexpr const char SKIP_DUPE_FRAMES[] = "dolphin_skip_dupe_frames";
  constexpr const char EARLY_XFB_OUTPUT[] = "dolphin_early_xfb_output";
  constexpr const char EFB_SCALED_COPY[] = "dolphin_efb_scaled_copy";
  constexpr const char EFB_EMULATE_FORMAT_CHANGES[] = "dolphin_efb_emulate_format_changes";
  constexpr const char VERTEX_ROUNDING[] = "dolphin_vertex_rounding";
  constexpr const char VI_SKIP[] = "dolphin_vi_skip";
  constexpr const char FAST_TEXTURE_SAMPLING[] = "dolphin_fast_texture_sampling";
#ifdef __APPLE__
  constexpr const char NO_MIPMAPPING[] = "dolphin_no_mipmapping";
#endif
}  // namespace gfx_hacks

// ======================================================
// Wiimote IR
// ======================================================
namespace wiimote {
  constexpr const char IR_MODE[] = "dolphin_ir_mode";
  constexpr const char IR_OFFSET[] = "dolphin_ir_offset";
  constexpr const char IR_YAW[] = "dolphin_ir_yaw";
  constexpr const char IR_PITCH[] = "dolphin_ir_pitch";
}  // namespace wiimote
}  // namespace Options
}  // namespace Libretro
