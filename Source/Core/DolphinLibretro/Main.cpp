
#include <cstdint>
#include <libretro.h>
#include <string>
#include <thread>

#include <fstream>
#include <vector>

#include "AudioCommon/AudioCommon.h"
#include "Common/ChunkFile.h"
#include "Common/Event.h"
#include "Common/IniFile.h"
#include "Common/FileUtil.h"
#include "Common/GL/GLContext.h"
#include "Common/Logging/LogManager.h"
#include "Common/Thread.h"
#include "Common/scmrev.h"
#include "Common/Version.h"
#include "Core/ActionReplay.h"
#include "Core/BootManager.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/GeckoCode.h"
#include "Core/GeckoCodeConfig.h"
#include "Core/HW/CPU.h"
#include "Core/HW/EXI/EXI.h"
#include "Core/HW/EXI/EXI_Device.h"
#include "Core/HW/EXI/EXI_DeviceMic.h"
#include "Core/HW/Memmap.h"
#include "Core/HW/ProcessorInterface.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/IOS/USB/Emulated/Microphone.h"
#include "Core/PatchEngine.h"
#include "Core/State.h"
#include "Core/System.h"
#include "DolphinLibretro/Audio.h"
#include "DolphinLibretro/Input.h"
#include "DolphinLibretro/Common/Options.h"
#include "DolphinLibretro/Video.h"
#include "VideoBackends/OGL/OGLTexture.h"
#include "VideoBackends/OGL/OGLGfx.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OnScreenDisplay.h"
#include "VideoCommon/TextureConfig.h"
#include "VideoCommon/VideoCommon.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/Widescreen.h"
#include "Core/Boot/Boot.h"

#ifdef PERF_TEST
static struct retro_perf_callback perf_cb;

#define RETRO_PERFORMANCE_INIT(name)                                                               \
  retro_perf_tick_t current_ticks;                                                                 \
  static struct retro_perf_counter name = {#name};                                                 \
  if (!name.registered)                                                                            \
    perf_cb.perf_register(&(name));                                                                \
  current_ticks = name.total

#define RETRO_PERFORMANCE_START(name) perf_cb.perf_start(&(name))
#define RETRO_PERFORMANCE_STOP(name)                                                               \
  perf_cb.perf_stop(&(name));                                                                      \
  current_ticks = name.total - current_ticks;
#else
#define RETRO_PERFORMANCE_INIT(name)
#define RETRO_PERFORMANCE_START(name)
#define RETRO_PERFORMANCE_STOP(name)
#endif

namespace Libretro
{
extern retro_environment_t environ_cb;
static bool widescreen;
double g_core_refresh_rate{0};
extern void reload_cheats_from_ini();
}  // namespace Libretro

extern "C" {

void retro_set_environment(retro_environment_t cb)
{
  Libretro::environ_cb = cb;
#ifdef PERF_TEST
  environ_cb(RETRO_ENVIRONMENT_GET_PERF_INTERFACE, &perf_cb);
#endif
}

void retro_init(void)
{
  enum retro_pixel_format xrgb888 = RETRO_PIXEL_FORMAT_XRGB8888;
  Libretro::environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &xrgb888);
}

void retro_deinit(void)
{
  Libretro::g_emuthread_launched = false;
#ifdef PERF_TEST
  perf_cb.perf_log();
#endif
}

void retro_get_system_info(retro_system_info* info)
{
  // Build a semver string from SCM_DESC_STR (e.g. "2512-226" -> "2512.0.226")
  // with the short git hash as build metadata (e.g. "+13192dec3d")
  static std::string version_str = []() {
    std::string desc = SCM_DESC_STR;
    std::string major, commits;
    auto dash = desc.find('-');
    if (dash != std::string::npos)
    {
      major = desc.substr(0, dash);
      auto dash2 = desc.find('-', dash + 1);
      commits = (dash2 != std::string::npos)
                    ? desc.substr(dash + 1, dash2 - dash - 1)
                    : desc.substr(dash + 1);
    }
    else
    {
      major = desc;
      commits = "0";
    }
    std::string hash = SCM_REV_STR;
    if (hash.size() > 10)
      hash = hash.substr(0, 10);
    return major + ".0." + commits + "+" + hash;
  }();

  info->need_fullpath = true;
  info->valid_extensions = "elf|dol|gcm|iso|tgc|wbfs|ciso|gcz|wad|wia|rvz|m3u";
  info->library_version = version_str.c_str();
  info->library_name = "dolphin-emu";
  info->block_extract = true;
}

void retro_get_system_av_info(retro_system_av_info* info)
{
  int efbScale = Libretro::Options::GetCached<int>(
    Libretro::Options::gfx_settings::EFB_SCALE);

  int base_height = EFB_HEIGHT;
  const bool crop_overscan = Libretro::Options::GetCached<bool>(
    Libretro::Options::gfx_settings::CROP_OVERSCAN);

  if (crop_overscan && retro_get_region() == RETRO_REGION_NTSC)
    base_height = 480;

  info->geometry.base_width  = EFB_WIDTH * efbScale;
  info->geometry.base_height = base_height * efbScale;

  info->geometry.max_width   = info->geometry.base_width;
  info->geometry.max_height  = info->geometry.base_height;

  if (g_widescreen)
    Libretro::widescreen = g_widescreen->IsGameWidescreen() || g_Config.bWidescreenHack;
  else if (Core::System::GetInstance().IsWii())
    Libretro::widescreen = Config::Get(Config::SYSCONF_WIDESCREEN);

  info->geometry.aspect_ratio = Libretro::widescreen ? 16.0 / 9.0 : 4.0 / 3.0;

  Core::System& system = Core::System::GetInstance();
  double fps = system.GetVideoInterface().GetTargetRefreshRate();
  if (fps <= 0.0)
    fps = (retro_get_region() == RETRO_REGION_NTSC) ? (60.0 / 1.001) : 50.0;
  info->timing.fps = fps;
  info->timing.sample_rate = Libretro::Audio::GetActiveSampleRate();
}

void retro_reset(void)
{
  Core::System::GetInstance().GetProcessorInterface().ResetButton_Tap();
}

void retro_run(void)
{
  Libretro::Input::InitSensors();
  Libretro::Options::CheckForUpdatedVariables();
  Libretro::FrameTiming::CheckForFastForwarding();
#if defined(_DEBUG)
  Common::Log::LogManager::GetInstance()->SetConfigLogLevel(Common::Log::LogLevel::LDEBUG);
#else
  Common::Log::LogManager::GetInstance()->SetConfigLogLevel(
    static_cast<Common::Log::LogLevel>(
        Libretro::Options::GetCached<int>(
            Libretro::Options::main_interface::LOG_LEVEL, static_cast<int>(Common::Log::LogLevel::LINFO))));
#endif
  double cpuClock = Libretro::Options::GetCached<double>(
    Libretro::Options::core::CPU_CLOCK_RATE);
  Config::SetCurrent(Config::MAIN_OVERCLOCK, cpuClock);
  Config::SetCurrent(Config::MAIN_OVERCLOCK_ENABLE, cpuClock != 1.0);
  g_Config.bWidescreenHack = Libretro::Options::GetCached<bool>(
    Libretro::Options::gfx_settings::WIDESCREEN_HACK);

  const bool crop_overscan = Libretro::Options::GetCached<bool>(
    Libretro::Options::gfx_settings::CROP_OVERSCAN);

  if (crop_overscan && retro_get_region() == RETRO_REGION_NTSC)
    g_Config.bCrop = true;
  else
    g_Config.bCrop = false;

  Libretro::Input::Update();

  Core::System& system = Core::System::GetInstance();

  if (Core::GetState(Core::System::GetInstance()) == Core::State::Starting &&
      !Libretro::g_emuthread_launched)
  {
    WindowSystemInfo wsi(WindowSystemType::Libretro, nullptr, nullptr, nullptr);
    if (system.IsDualCoreMode())
    {
      Core::s_emu_thread = std::thread(Core::EmuThread,
        std::ref(Core::System::GetInstance()), std::move(Core::g_boot_params), wsi);

      // Wait until CPU thread has reached Run()
      auto& cpu_manager = Core::System::GetInstance().GetCPU();
      while (!cpu_manager.HasCPURunStateBeenReached())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    else
      Core::EmuThread(Core::System::GetInstance(), std::move(Core::g_boot_params), wsi);

    Libretro::g_emuthread_launched = true;

    if(Config::Get(Config::MAIN_GFX_BACKEND) == "Software Renderer")
    {
      g_gfx.reset();
      g_gfx = std::make_unique<Libretro::Video::SWGfx>();
    }
    else if (Config::Get(Config::MAIN_GFX_BACKEND) == "Null")
    {
      g_gfx.reset();
      g_gfx = std::make_unique<Libretro::Video::NullGfx>();
    }

    while (!Core::IsRunningOrStarting(Core::System::GetInstance()))
      Common::SleepCurrentThread(100);

    Libretro::g_core_refresh_rate = system.GetVideoInterface().GetTargetRefreshRate();

    // Expose GameCube and Wii memory maps to libretro
    {
      auto& memory = system.GetMemory();
      struct retro_memory_descriptor descs[2] = {};
      unsigned num_descs = 0;

      descs[num_descs].ptr = memory.GetRAM();
      descs[num_descs].start = 0x80000000;
      descs[num_descs].len = memory.GetRamSizeReal();
      descs[num_descs].flags = RETRO_MEMDESC_BIGENDIAN | RETRO_MEMORY_SYSTEM_RAM;
      descs[num_descs].addrspace = "MEM1";

      num_descs++;

      if (system.IsWii())
      {
        descs[num_descs].ptr = memory.GetEXRAM();
        descs[num_descs].start = 0x90000000;
        descs[num_descs].len = memory.GetExRamSizeReal();
        descs[num_descs].flags = RETRO_MEMDESC_BIGENDIAN | RETRO_MEMORY_SYSTEM_RAM;
        descs[num_descs].addrspace = "MEM2";
        num_descs++;
      }

      struct retro_memory_map mmap = {descs, num_descs};
      Libretro::environ_cb(RETRO_ENVIRONMENT_SET_MEMORY_MAPS, &mmap);
    }
  }

  if(!Libretro::g_emuthread_launched)
  {
    DEBUG_LOG_FMT(COMMON, "retro_run() - waiting for g_emuthread_launched");
    return;
  }

  if (g_gfx && Config::Get(Config::MAIN_GFX_BACKEND) == "OGL")
  {
    static_cast<OGL::OGLGfx*>(g_gfx.get())
        ->SetSystemFrameBuffer((GLuint)Libretro::Video::hw_render.get_current_framebuffer());
  }

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_settings::EFB_SCALE))
  {
    g_Config.iEFBScale = Libretro::Options::GetCached<int>(
      Libretro::Options::gfx_settings::EFB_SCALE);

    unsigned cmd = RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO;
    if (Libretro::Video::hw_render.context_type == RETRO_HW_CONTEXT_D3D11 ||
        Libretro::Video::hw_render.context_type == RETRO_HW_CONTEXT_D3D12)
      cmd = RETRO_ENVIRONMENT_SET_GEOMETRY;
    retro_system_av_info info;
    retro_get_system_av_info(&info);
    Libretro::environ_cb(cmd, &info);
  }

  if (g_widescreen &&
      Libretro::widescreen != (g_widescreen->IsGameWidescreen() || g_Config.bWidescreenHack))
  {
    retro_system_av_info info;
    retro_get_system_av_info(&info);
    Libretro::environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &info);
  }

  // target refresh rate has changed - e.g. user has chosen 60 Hz for a PAL game
  double new_rate = system.GetVideoInterface().GetTargetRefreshRate();

  if (round(Libretro::g_core_refresh_rate * 1e6) != round(new_rate * 1e6))
  {
    Libretro::g_core_refresh_rate = new_rate;

    retro_system_av_info info;
    retro_get_system_av_info(&info);
    Libretro::environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &info);
  }

  WiimoteUpdateFlags flags;
  flags.irMode  = Libretro::Options::IsUpdated(Libretro::Options::wiimote::IR_MODE);
  flags.irOffset = Libretro::Options::IsUpdated(Libretro::Options::wiimote::IR_OFFSET);
  flags.irYaw = Libretro::Options::IsUpdated(Libretro::Options::wiimote::IR_YAW);
  flags.irPitch = Libretro::Options::IsUpdated(Libretro::Options::wiimote::IR_PITCH);
  flags.irDeadzone = Libretro::Options::IsUpdated(Libretro::Options::wiimote::IR_DEADZONE);
  flags.irModifier = Libretro::Options::IsUpdated(Libretro::Options::wiimote::IR_MODIFIER);
  flags.swingModifier = Libretro::Options::IsUpdated(Libretro::Options::wiimote::SWING_MODIFIER);
  flags.swingAngle = Libretro::Options::IsUpdated(Libretro::Options::wiimote::SWING_ANGLE);
  flags.sideways = Libretro::Options::IsUpdated(Libretro::Options::wiimote::HOTKEY_SIDEWAYS_TOGGLE);
  flags.rumble = Libretro::Options::IsUpdated(Libretro::Options::sysconf::ENABLE_RUMBLE);
  flags.gcMicBtn = Libretro::Options::IsUpdated(Libretro::Options::sysconf_gc::HOTKEY_ACTIVATE_MICROPHONE);

  if (flags.any())
    Libretro::Input::ResetControllers(flags);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_settings::ASPECT_RATIO))
    Config::SetCurrent(
      Config::GFX_ASPECT_RATIO, static_cast<AspectMode>(
        Libretro::GetOption<int>(
          Libretro::Options::gfx_settings::ASPECT_RATIO, static_cast<int>(AspectMode::Stretch)
        )
      )
    );

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::EFB_TO_TEXTURE))
    g_Config.bSkipEFBCopyToRam = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::EFB_TO_TEXTURE, true);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::EFB_ACCESS_ENABLE))
    g_Config.bEFBAccessEnable = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::EFB_ACCESS_ENABLE, false);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::EFB_ACCESS_DEFER_INVALIDATION))
    g_Config.bEFBAccessDeferInvalidation = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::EFB_ACCESS_DEFER_INVALIDATION, false);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::EFB_ACCESS_TILE_SIZE))
    g_Config.iEFBAccessTileSize = Libretro::Options::GetCached<int>(Libretro::Options::gfx_hacks::EFB_ACCESS_TILE_SIZE, 64);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::BBOX_ENABLED))
    g_Config.bBBoxEnable = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::BBOX_ENABLED, false);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::XFB_TO_TEXTURE_ENABLE))
    g_Config.bSkipXFBCopyToRam = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::XFB_TO_TEXTURE_ENABLE, true);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::EFB_TO_VRAM))
    g_Config.bDisableCopyToVRAM = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::EFB_TO_VRAM, false);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::DEFER_EFB_COPIES))
    g_Config.bDeferEFBCopies = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::DEFER_EFB_COPIES, true);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::IMMEDIATE_XFB))
    g_Config.bImmediateXFB = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::IMMEDIATE_XFB, false);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::SKIP_DUPE_FRAMES))
    g_Config.bSkipPresentingDuplicateXFBs = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::SKIP_DUPE_FRAMES, true);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::EFB_SCALED_COPY))
    g_Config.bCopyEFBScaled = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::EFB_SCALED_COPY, true);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::EFB_EMULATE_FORMAT_CHANGES))
    g_Config.bEFBEmulateFormatChanges = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::EFB_EMULATE_FORMAT_CHANGES, false);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::VERTEX_ROUNDING))
    g_Config.bVertexRounding = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::VERTEX_ROUNDING, false);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::VI_SKIP))
    g_Config.bVISkip = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::VI_SKIP, false);

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::FAST_TEXTURE_SAMPLING))
    g_Config.bFastTextureSampling = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::FAST_TEXTURE_SAMPLING, true);

#ifdef __APPLE__
  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_hacks::NO_MIPMAPPING))
    g_Config.bNoMipmapping = Libretro::Options::GetCached<bool>(Libretro::Options::gfx_hacks::NO_MIPMAPPING, false);
#endif

  if (Libretro::Options::IsUpdated(Libretro::Options::gfx_settings::TEXTURE_CACHE_ACCURACY))
    g_Config.iSafeTextureCache_ColorSamples = Libretro::Options::GetCached<int>(Libretro::Options::gfx_settings::TEXTURE_CACHE_ACCURACY, 128);

  if (Libretro::Options::IsUpdated(Libretro::Options::sysconf::WII_SPEAK_MUTED))
    Config::SetCurrent(Config::MAIN_WII_SPEAK_MUTED,
      Libretro::Options::GetCached<bool>(Libretro::Options::sysconf::WII_SPEAK_MUTED));

  if (Libretro::Options::IsUpdated(Libretro::Options::sysconf::WIIMOTE_CONTINUOUS_SCANNING))
  {
    Config::SetCurrent(Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING,
      Libretro::Options::GetCached<bool>(Libretro::Options::sysconf::WIIMOTE_CONTINUOUS_SCANNING));
    WiimoteReal::Initialize(Wiimote::InitializeMode::DO_NOT_WAIT_FOR_WIIMOTES);
  }

  if (Config::Get(Config::MAIN_BLUETOOTH_PASSTHROUGH_ENABLED))
    Libretro::Input::BluetoothPassthroughBind();

  RETRO_PERFORMANCE_INIT(dolphin_main_func);
  RETRO_PERFORMANCE_START(dolphin_main_func);

  if (system.IsDualCoreMode())
  {
    Core::DoFrameStep(system);
    system.GetFifo().RunGpuLoop();
  }
  else
  {
    system.GetCPU().RunSingleFrame();
  }

  RETRO_PERFORMANCE_STOP(dolphin_main_func);

  if (auto* sound_stream = system.GetSoundStream())
  {
    auto* libretro_stream = static_cast<Libretro::Audio::Stream*>(sound_stream);
    if (libretro_stream)
    {
      libretro_stream->PushAudioForFrame();
    }
  }

  if (Libretro::Input::g_has_microphone_support)
  {
    bool wiiSpeakEnable = Libretro::Options::GetCached<bool>(Libretro::Options::sysconf::WII_SPEAK_ENABLE);
    bool wiiLogiMicEnable = Libretro::Options::GetCached<bool>(Libretro::Options::sysconf::WII_LOGI_MICROPHONE_ENABLE);
    bool gcMicEnable = Libretro::Options::GetCached<bool>(Libretro::Options::sysconf_gc::ENABLE_GAMECUBE_MIC);

    if (system.IsWii() && (wiiSpeakEnable || wiiLogiMicEnable))
    {
      for (auto* mic : Libretro::Input::g_active_microphones)
        mic->PollRetroArchMic();
    }
    else if(gcMicEnable)
    {
      // Poll GC mic
      auto* exi_device = system.GetExpansionInterface().GetDevice(ExpansionInterface::Slot::B);
      if (exi_device && Config::Get(Config::GetInfoForEXIDevice(ExpansionInterface::Slot::B))
        == ExpansionInterface::EXIDeviceType::Microphone)
      {
        auto* gc_mic = static_cast<ExpansionInterface::CEXIMic*>(exi_device);
        gc_mic->PollLibretroMic();
      }
    }
  }
}

size_t retro_serialize_size(void)
{
  size_t size = 0;

  Core::System& system = Core::System::GetInstance();
  AsyncRequests* ar = AsyncRequests::GetInstance();

  if (system.IsDualCoreMode())
    ar->SetPassthrough(true);

  Core::RunOnCPUThread(Core::System::GetInstance(), [&] {
    PointerWrap p((u8**)&size, sizeof(size_t), PointerWrap::Mode::Measure);
    State::DoState(Core::System::GetInstance(), p);
    }, true);  // wait = true

  if (system.IsDualCoreMode())
    ar->SetPassthrough(false);

  return size;
}

bool retro_serialize(void* data, size_t size)
{
  Core::System& system = Core::System::GetInstance();
  AsyncRequests* ar = AsyncRequests::GetInstance();

  if (system.IsDualCoreMode())
    ar->SetPassthrough(true);

  Core::RunOnCPUThread(Core::System::GetInstance(), [&] {

    PointerWrap p((u8**)&data, size, PointerWrap::Mode::Write);
    State::DoState(Core::System::GetInstance(), p);
  }, true);

  if (system.IsDualCoreMode())
    ar->SetPassthrough(false);

  return true;
}
bool retro_unserialize(const void* data, size_t size)
{
  Core::System& system = Core::System::GetInstance();
  AsyncRequests* ar = AsyncRequests::GetInstance();

  if (system.IsDualCoreMode())
    ar->SetPassthrough(true);

  Core::RunOnCPUThread(Core::System::GetInstance(), [&] {
    PointerWrap p((u8**)&data, size, PointerWrap::Mode::Read);
    State::DoState(Core::System::GetInstance(), p);
  }, true);

  if (system.IsDualCoreMode())
    ar->SetPassthrough(false);

  return true;
}

unsigned retro_get_region()
{
  Core::System& system = Core::System::GetInstance();

  if (DiscIO::IsNTSC(SConfig::GetInstance().m_region) ||
      (Core::System::GetInstance().IsWii() && Config::Get(Config::SYSCONF_PAL60)) ||
       system.IsTriforce())
    return RETRO_REGION_NTSC;

  return RETRO_REGION_PAL;
}

unsigned retro_api_version()
{
  return RETRO_API_VERSION;
}

size_t retro_get_memory_size(unsigned id)
{
  auto& memory = Core::System::GetInstance().GetMemory();
  if (id == RETRO_MEMORY_SYSTEM_RAM)
    return memory.GetRamSizeReal();
  return 0;
}

void* retro_get_memory_data(unsigned id)
{
  auto& memory = Core::System::GetInstance().GetMemory();
  if (id == RETRO_MEMORY_SYSTEM_RAM)
    return memory.GetRAM();
  return NULL;
}

static void enable_cheat_by_code(bool enabled, const char* code)
{
  std::string incoming(code);
  bool found = false;

  // Match Action Replay codes
  for (auto& c : Libretro::g_ar_codes)
  {
    std::string serialized;
    serialized.reserve(64);

    for (const auto& op : c.ops)
    {
      if (!serialized.empty())
          serialized += "+";

      serialized += fmt::format("{:08X} {:08X}", op.cmd_addr, op.value);
    }

    if (serialized == incoming)
    {
      c.enabled = enabled;
      found = true;
      break;
    }
  }

  // Match Gecko codes only if not already matched
  if (!found)
  {
    for (auto& c : Libretro::g_gecko_codes)
    {
      std::string serialized;
      serialized.reserve(64);

      for (const auto& line : c.codes)
      {
        if (!serialized.empty())
            serialized += "+";

        serialized += line.original_line;
      }

      if (serialized == incoming)
      {
        c.enabled = enabled;
        found = true;
        break;
      }
    }
  }
}

// reset cheats to off
void retro_cheat_reset(void)
{
  const bool importCheats = Libretro::GetOption<bool>(Libretro::Options::core::CHEATS_IMPORT, true);

  if (!Config::AreCheatsEnabled() || !importCheats)
    return;

  Core::RunOnCPUThread(Core::System::GetInstance(), [&] {
    PatchEngine::Shutdown();
  }, true);

  Libretro::reload_cheats_from_ini();
}

// Enable/disable a cheat
void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
  if (!Config::AreCheatsEnabled())
  {
    OSD::AddMessage("You cannot apply any cheats until you have enabled the following option:\n"
      "Core > Internal Cheats - set this to ON", OSD::Duration::VERY_LONG, OSD::Color::RED);
    return;
  }

  const bool importCheats = Libretro::GetOption<bool>(Libretro::Options::core::CHEATS_IMPORT, true);
  if (!importCheats)
  {
    OSD::AddMessage("You cannot apply any cheats until you enabled the following option:\n"
      "Core > Automatically Import Cheats into RetroArch - set this to ON", OSD::Duration::VERY_LONG, OSD::Color::RED);
    return;
  }

  const std::string& game_id = SConfig::GetInstance().GetGameID();
  const u16 game_revision = SConfig::GetInstance().GetRevision();

  // Look up by code as index is unreliable
  enable_cheat_by_code(enabled, code);

  Core::RunOnCPUThread(Core::System::GetInstance(), [&] {
    // Apply Action Replay
    ActionReplay::ApplyCodes(Libretro::g_ar_codes, game_id, game_revision);

    // Apply Gecko
    Gecko::SetActiveCodes(Libretro::g_gecko_codes, game_id, game_revision);
  }, true);
}
} // extern "C"
