#include <cstdio>
#include <libretro.h>
#include <string>
#include <functional>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Core/Boot/Boot.h"
#include "Core/BootManager.h"
#include "Core/Config/GraphicsSettings.h"
#include "Core/Config/MainSettings.h"
#include "Core/Config/SYSCONFSettings.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/HW/DVD/DVDInterface.h"
#include "Core/HW/VideoInterface.h"
#include "Core/HW/WiimoteReal/WiimoteReal.h"
#include "Core/PowerPC/PowerPC.h"
#include "Core/System.h"
#include "DolphinLibretro/Audio.h"
#include "DolphinLibretro/Input.h"
#include "DolphinLibretro/Log.h"
#include "DolphinLibretro/Common/Options.h"
#include "DolphinLibretro/Video.h"
#include "DolphinLibretro/VideoContexts/ContextStatus.h"
#include "InputCommon/ControllerInterface/ControllerInterface.h"
#include "UICommon/DiscordPresence.h"
#include "UICommon/UICommon.h"
#include "VideoCommon/AsyncRequests.h"
#include "VideoCommon/VideoBackendBase.h"
#include "VideoCommon/VideoConfig.h"
#include "VideoCommon/Assets/CustomResourceManager.h"
#include "VideoCommon/Fifo.h"
#include "VideoCommon/OnScreenDisplay.h"

#ifdef _MSC_VER
#include <filesystem>
namespace fs = std::filesystem;
#endif

namespace Libretro
{
extern retro_environment_t environ_cb;

// Disk swapping
static void InitDiskControlInterface();
static std::string NormalizePath(const std::string& path);
static std::string DenormalizePath(const std::string& path);
static unsigned disk_index = 0;
static bool eject_state;
static std::vector<std::string> disk_paths;
} // namespace Libretro

bool retro_load_game(const struct retro_game_info* game)
{
  const char* save_dir = NULL;
  const char* system_dir = NULL;
  const char* core_assets_dir = NULL;
  std::string user_dir;
  std::string sys_dir;

  Libretro::environ_cb(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY, &save_dir);
  Libretro::environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &system_dir);
  Libretro::environ_cb(RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY, &core_assets_dir);
  Libretro::InitDiskControlInterface();

  if (save_dir && *save_dir)
    user_dir = std::string(save_dir) + DIR_SEP "User";
  else if (system_dir && *system_dir)
    user_dir = std::string(system_dir) + DIR_SEP "dolphin-emu" DIR_SEP "User";

  if (system_dir && *system_dir)
    sys_dir = std::string(system_dir) + DIR_SEP "dolphin-emu" DIR_SEP "Sys";
  else if (core_assets_dir && *core_assets_dir)
    sys_dir = std::string(core_assets_dir) + DIR_SEP "dolphin-emu" DIR_SEP "Sys";
  else if (save_dir && *save_dir)
    sys_dir = std::string(save_dir) + DIR_SEP "Sys";
  else
    sys_dir = "dolphin-emu" DIR_SEP "Sys";

#ifdef ANDROID
  static bool sysdir_set = false;

  if(!sysdir_set)
  {
    File::SetSysDirectory(sys_dir);
    sysdir_set = true;
  }
#else
  File::SetSysDirectory(sys_dir);
#endif

  UICommon::SetUserDirectory(user_dir);
  UICommon::CreateDirectories();
  UICommon::Init();
  Libretro::Log::Init();
  Discord::SetDiscordPresenceEnabled(false);
  Common::SetEnableAlert(false);
  Common::SetAbortOnPanicAlert(false);
  Common::RegisterMsgAlertHandler([](const char* caption, const char* text,
    bool yes_no, Common::MsgType style) -> bool
  {
    // Log the message instead of showing a popup
    INFO_LOG_FMT(COMMON, "Suppressed popup: {} - {}", caption, text);
    return true; // Always "continue"
  });

  INFO_LOG_FMT(COMMON, "User Directory set to '{}'", user_dir);
  INFO_LOG_FMT(COMMON, "System Directory set to '{}'", sys_dir);

  using namespace Libretro::Options;

  Libretro::Options::Init();

  // Main.Core
  Config::SetCurrent(Config::MAIN_CPU_CORE,
    static_cast<PowerPC::CPUCore>(
        Libretro::GetOption<int>(
            core::CPU_CORE,
            static_cast<int>(PowerPC::DefaultCPUCore()))));

#if defined(_DEBUG)
  Config::SetCurrent(Config::MAIN_FASTMEM, false);
  Config::SetCurrent(Config::MAIN_FASTMEM_ARENA, false);
#else
  Config::SetCurrent(Config::MAIN_FASTMEM,
                     Libretro::GetOption<bool>(core::FASTMEM, /*def=*/true));
  Config::SetCurrent(Config::MAIN_FASTMEM_ARENA,
                     Libretro::GetOption<bool>(core::FASTMEM_ARENA, /*def=*/true));
#endif
  Config::SetCurrent(Config::MAIN_DSP_HLE,
                     Libretro::GetOption<bool>(audio::DSP_HLE, /*def=*/true));

  // dual core (true) or single core (false)
  Config::SetCurrent(Config::MAIN_CPU_THREAD,
    Libretro::GetOption<bool>(core::MAIN_CPU_THREAD, /*def=*/true));

  Config::SetCurrent(Config::MAIN_ENABLE_CHEATS,
                     Libretro::GetOption<bool>(core::CHEATS_ENABLED, /*def=*/false));

  const int language = Libretro::GetOption<int>(core::LANGUAGE, static_cast<int>(DiscIO::Language::English));
  Config::SetCurrent(Config::SYSCONF_LANGUAGE, language);
  Config::SetCurrent(Config::MAIN_GC_LANGUAGE, DiscIO::ToGameCubeLanguage(static_cast<DiscIO::Language>(language)));

  Config::SetCurrent(Config::MAIN_DPL2_DECODER, false);
  Config::SetCurrent(Config::MAIN_AUDIO_LATENCY, 0);
  Config::SetCurrent(Config::MAIN_AUDIO_FILL_GAPS, false);

  Config::SetCurrent(Config::MAIN_EMULATION_SPEED,
                     Libretro::GetOption<double>(core::EMULATION_SPEED, /*def=*/0.0));
  {
    // Overclock (cpu clock rate) — option values in option_defs used strings like "100%" etc.
    double multiplier = Libretro::GetOption<double>(core::CPU_CLOCK_RATE, 1.0);
    Config::SetCurrent(Config::MAIN_OVERCLOCK, multiplier);
    Config::SetCurrent(Config::MAIN_OVERCLOCK_ENABLE, multiplier != 1.0);
  }

  Config::SetCurrent(Config::MAIN_PRECISION_FRAME_TIMING,
    Libretro::GetOption<bool>(core::MAIN_PRECISION_FRAME_TIMING,
                            /*def=*/false)); // true is the standalone default

  Config::SetCurrent(Config::MAIN_WIIMOTE_CONTINUOUS_SCANNING,
                     Libretro::GetOption<bool>(sysconf::WIIMOTE_CONTINUOUS_SCANNING,
                                             /*def=*/false));
  Config::SetCurrent(Config::MAIN_MMU,
                     Libretro::GetOption<bool>(core::MAIN_MMU, /*def=*/false));

  Config::SetCurrent(Config::MAIN_FAST_DISC_SPEED,
                     Libretro::GetOption<bool>(core::FAST_DISC_SPEED, /*def=*/false));

  // Main.Interface
  Config::SetCurrent(Config::MAIN_OSD_MESSAGES,
                     Libretro::GetOption<bool>(main_interface::OSD_ENABLED, /*def=*/true));
  Config::SetCurrent(Config::MAIN_ENABLE_DEBUGGING,
                     Libretro::GetOption<bool>(main_interface::ENABLE_DEBUGGING, /*def=*/false));

  // Main.General
  Config::SetCurrent(Config::MAIN_TIME_TRACKING, false);

  // Main.DSP
  Config::SetCurrent(Config::MAIN_DSP_JIT,
                     Libretro::GetOption<bool>(audio::DSP_JIT, /*def=*/true));
  Config::SetCurrent(Config::MAIN_DUMP_AUDIO, false);
  Config::SetCurrent(Config::MAIN_AUDIO_BACKEND, BACKEND_NULLSOUND);

  // SYSCONF.IPL
  Config::SetBase(Config::SYSCONF_WIDESCREEN,
                 Libretro::GetOption<bool>(sysconf::WIDESCREEN, /*def=*/true));
  Config::SetBase(Config::SYSCONF_PROGRESSIVE_SCAN,
                 Libretro::GetOption<bool>(sysconf::PROGRESSIVE_SCAN, /*def=*/true));
  Config::SetBase(Config::SYSCONF_PAL60,
                 Libretro::GetOption<bool>(sysconf::PAL60, /*def=*/true));

  // SYSCONF.BT
  Config::SetBase(Config::SYSCONF_SENSOR_BAR_POSITION,
                 Libretro::GetOption<int>(sysconf::SENSOR_BAR_POSITION, 0));
  Config::SetBase(Config::SYSCONF_WIIMOTE_MOTOR,
                 Libretro::GetOption<bool>(sysconf::ENABLE_RUMBLE, /*def=*/true));

  // Graphics.Hardware
  //Config::SetBase(Config::GFX_VSYNC,
  //               Libretro::GetOption<bool>(gfx_hardware::VSYNC, /*def=*/false));

  // Graphics.Settings
  Config::SetBase(Config::GFX_WIDESCREEN_HACK,
                 Libretro::GetOption<bool>(gfx_settings::WIDESCREEN_HACK, /*def=*/false));
  Config::SetBase(Config::GFX_ASPECT_RATIO, AspectMode::Stretch);
  Config::SetBase(Config::GFX_SAFE_TEXTURE_CACHE_COLOR_SAMPLES,
                 Libretro::GetOption<int>(gfx_settings::TEXTURE_CACHE_ACCURACY, 128));
  Config::SetBase(Config::GFX_HIRES_TEXTURES,
                 Libretro::GetOption<bool>(gfx_enhancements::LOAD_CUSTOM_TEXTURES, /*def=*/false));
  Config::SetBase(Config::GFX_CACHE_HIRES_TEXTURES,
                 Libretro::GetOption<bool>(gfx_enhancements::CACHE_CUSTOM_TEXTURES, /*def=*/false));
  Config::SetBase(Config::GFX_ENABLE_GPU_TEXTURE_DECODING,
                 Libretro::GetOption<bool>(gfx_settings::GPU_TEXTURE_DECODING, /*def=*/false));
  Config::SetBase(Config::GFX_ENABLE_PIXEL_LIGHTING,
                  Libretro::GetOption<bool>(gfx_settings::ENABLE_PIXEL_LIGHTING, /*def=*/false));
  Config::SetBase(Config::GFX_FAST_DEPTH_CALC,
                 Libretro::GetOption<bool>(gfx_settings::FAST_DEPTH_CALCULATION, /*def=*/true));
  Config::SetBase(Config::GFX_DISABLE_FOG,
                  Libretro::GetOption<bool>(gfx_settings::DISABLE_FOG, /*def=*/false));
  Config::SetBase(Config::GFX_EFB_SCALE,
                 Libretro::GetOption<int>(gfx_settings::EFB_SCALE, /*def=*/1));
  Config::SetBase(Config::GFX_BACKEND_MULTITHREADING, false);
  Config::SetBase(Config::GFX_SHADER_COMPILATION_MODE,
                  static_cast<ShaderCompilationMode>(
                    Libretro::GetOption<int>(
                      gfx_settings::SHADER_COMPILATION_MODE,
                      static_cast<int>(ShaderCompilationMode::Synchronous))));
  Config::SetBase(Config::GFX_WAIT_FOR_SHADERS_BEFORE_STARTING,
                 Libretro::GetOption<bool>(gfx_settings::WAIT_FOR_SHADERS, /*def=*/false));

  // Graphics.Enhancements
  Config::SetBase(Config::GFX_ENHANCE_FORCE_TEXTURE_FILTERING,
                  static_cast<TextureFilteringMode>(
                    Libretro::GetOption<int>(
                      gfx_enhancements::FORCE_TEXTURE_FILTERING_MODE,
                      static_cast<int>(TextureFilteringMode::Default))));

  Config::SetBase(Config::GFX_ENHANCE_MAX_ANISOTROPY,
                  static_cast<AnisotropicFilteringMode>(
                    Libretro::GetOption<int>(
                      gfx_enhancements::MAX_ANISOTROPY,
                      static_cast<int>(AnisotropicFilteringMode::Force1x))));

  Config::SetBase(Config::GFX_ENHANCE_OUTPUT_RESAMPLING,
                  static_cast<OutputResamplingMode>(
                    Libretro::GetOption<int>(
                      gfx_enhancements::GFX_ENHANCE_OUTPUT_RESAMPLING,
                        static_cast<int>(OutputResamplingMode::Default))));

  Config::SetBase(Config::GFX_ENHANCE_FORCE_TRUE_COLOR,
                          Libretro::GetOption<bool>(gfx_enhancements::FORCE_TRUE_COLOR,
                                                    /*def=*/true));

  Config::SetBase(Config::GFX_ENHANCE_DISABLE_COPY_FILTER,
                  Libretro::GetOption<bool>(gfx_enhancements::GFX_ENHANCE_DISABLE_COPY_FILTER,
                    /*def=*/true));
  Config::SetBase(Config::GFX_ENHANCE_HDR_OUTPUT,
                  Libretro::GetOption<bool>(gfx_enhancements::GFX_ENHANCE_HDR_OUTPUT,
                                            /*def=*/false));
  Config::SetBase(Config::GFX_ENHANCE_ARBITRARY_MIPMAP_DETECTION,
                  Libretro::GetOption<bool>(gfx_enhancements::GFX_ARBITRARY_MIPMAP_DETECTION, /*def=*/false));

  // Graphics.Hacks
  Config::SetBase(Config::GFX_HACK_EFB_ACCESS_ENABLE,
                 Libretro::GetOption<bool>(gfx_hacks::EFB_ACCESS_ENABLE, /*def=*/false));
  Config::SetBase(Config::GFX_HACK_EFB_DEFER_INVALIDATION,
                 Libretro::GetOption<bool>(gfx_hacks::EFB_ACCESS_DEFER_INVALIDATION, /*def=*/false));
  Config::SetBase(Config::GFX_HACK_EFB_ACCESS_TILE_SIZE,
                 Libretro::GetOption<int>(gfx_hacks::EFB_ACCESS_TILE_SIZE, /*def=*/64));
  Config::SetBase(Config::GFX_HACK_BBOX_ENABLE,
                 Libretro::GetOption<bool>(gfx_hacks::BBOX_ENABLED, /*def=*/false));
  Config::SetBase(Config::GFX_HACK_FORCE_PROGRESSIVE,
                 Libretro::GetOption<bool>(gfx_hacks::FORCE_PROGRESSIVE, /*def=*/true));
  Config::SetBase(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM,
                 Libretro::GetOption<bool>(gfx_hacks::EFB_TO_TEXTURE, /*def=*/true));
  Config::SetBase(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM,
                 Libretro::GetOption<bool>(gfx_hacks::XFB_TO_TEXTURE_ENABLE, /*def=*/true));
  Config::SetBase(Config::GFX_HACK_DISABLE_COPY_TO_VRAM,
                 Libretro::GetOption<bool>(gfx_hacks::EFB_TO_VRAM, /*def=*/false));
  Config::SetBase(Config::GFX_HACK_DEFER_EFB_COPIES,
                 Libretro::GetOption<bool>(gfx_hacks::DEFER_EFB_COPIES, /*def=*/true));
  Config::SetBase(Config::GFX_HACK_IMMEDIATE_XFB,
                 Libretro::GetOption<bool>(gfx_hacks::IMMEDIATE_XFB, /*def=*/false));
  Config::SetBase(Config::GFX_HACK_SKIP_DUPLICATE_XFBS,
                 Libretro::GetOption<bool>(gfx_hacks::SKIP_DUPE_FRAMES, /*def=*/true));
  Config::SetBase(Config::GFX_HACK_EARLY_XFB_OUTPUT,
                 Libretro::GetOption<bool>(gfx_hacks::EARLY_XFB_OUTPUT, /*def=*/true));
  Config::SetBase(Config::GFX_HACK_COPY_EFB_SCALED,
                 Libretro::GetOption<bool>(gfx_hacks::EFB_SCALED_COPY, /*def=*/true));
  Config::SetBase(Config::GFX_HACK_EFB_EMULATE_FORMAT_CHANGES,
                 Libretro::GetOption<bool>(gfx_hacks::EFB_EMULATE_FORMAT_CHANGES, /*def=*/false));
  Config::SetBase(Config::GFX_HACK_VERTEX_ROUNDING,
                 Libretro::GetOption<bool>(gfx_hacks::VERTEX_ROUNDING, /*def=*/false));
  Config::SetBase(Config::GFX_HACK_VI_SKIP,
                 Libretro::GetOption<bool>(gfx_hacks::VI_SKIP, /*def=*/false));
  Config::SetBase(Config::GFX_HACK_FAST_TEXTURE_SAMPLING,
                 Libretro::GetOption<bool>(gfx_hacks::FAST_TEXTURE_SAMPLING, /*def=*/true));
  #ifdef __APPLE__
  Config::SetBase(Config::GFX_HACK_NO_MIPMAPPING,
                 Libretro::GetOption<bool>(gfx_hacks::NO_MIPMAPPING, /*def=*/false));
  #endif

  switch (Libretro::GetOption<int>(gfx_settings::ANTI_ALIASING, 0))
  {
    case 1:  // 2x MSAA
      Config::SetBase(Config::GFX_MSAA, 2);
      Config::SetBase(Config::GFX_SSAA, false);
      break;
    case 2:  // 4x MSAA
      Config::SetBase(Config::GFX_MSAA, 4);
      Config::SetBase(Config::GFX_SSAA, false);
      break;
    case 3:  // 8x MSAA
      Config::SetBase(Config::GFX_MSAA, 8);
      Config::SetBase(Config::GFX_SSAA, false);
      break;
    case 4:  // 2x SSAA
      Config::SetBase(Config::GFX_MSAA, 2);
      Config::SetBase(Config::GFX_SSAA, true);
      break;
    case 5:  // 4x SSAA
      Config::SetBase(Config::GFX_MSAA, 4);
      Config::SetBase(Config::GFX_SSAA, true);
      break;
    case 6:  // 8x SSAA
      Config::SetBase(Config::GFX_MSAA, 8);
      Config::SetBase(Config::GFX_SSAA, true);
      break;
    default: // disabled
      Config::SetBase(Config::GFX_MSAA, 1);
      Config::SetBase(Config::GFX_SSAA, false);
      break;
  }

  /* disable throttling emulation to match GetTargetRefreshRate() */
  Core::SetIsThrottlerTempDisabled(true);
  SConfig::GetInstance().bBootToPause = true;

#ifdef IPHONEOS
  bool can_jit = false;
  if (!Libretro::environ_cb(RETRO_ENVIRONMENT_GET_JIT_CAPABLE, &can_jit) || !can_jit)
  {
    auto current = Config::Get(Config::MAIN_CPU_CORE);
    if (current == PowerPC::CPUCore::JIT64 ||
        current == PowerPC::CPUCore::JITARM64)
    {
      Config::SetCurrent(Config::MAIN_CPU_CORE, PowerPC::CPUCore::CachedInterpreter);
    }

    Config::SetCurrent(Config::GFX_VERTEX_LOADER_TYPE, VertexLoaderType::Software);

    OSD::AddMessage("CPU: Just in time compiler disabled as unavailable on your system", OSD::Duration::NORMAL);
  }
#endif
  INFO_LOG_FMT(BOOT, "CPU Core: {}", Libretro::Options::CPUCoreToString(Config::Get(Config::MAIN_CPU_CORE)));
  INFO_LOG_FMT(BOOT, "Fastmem enabled = {}", (Config::Get(Config::MAIN_FASTMEM)) ? "Yes" : "No");
  INFO_LOG_FMT(BOOT, "JIT debug enabled = {}", Config::IsDebuggingEnabled() ? "Yes" : "No");

  Libretro::FrameTiming::Init();
  Libretro::Audio::Init();
  Libretro::Video::Init();
  WindowSystemInfo wsi(WindowSystemType::Libretro, nullptr, nullptr, nullptr);
  VideoBackendBase::PopulateBackendInfo(wsi);
  NOTICE_LOG_FMT(VIDEO, "Using GFX backend: {}", Config::Get(Config::MAIN_GFX_BACKEND));

  std::vector<std::string> normalized_game_paths;
  normalized_game_paths.push_back(Libretro::NormalizePath(game->path));
  std::string folder_path;
  std::string filename;
  std::string extension;
  SplitPath(normalized_game_paths.front(), &folder_path, &filename, &extension);
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

#ifdef _WIN32
  // If SplitPath only gave us "D:", rebuild the real directory from the full path
  if (folder_path.size() == 2 && folder_path[1] == ':')
  {
    // take everything up to the last backslash
    size_t last_slash = normalized_game_paths.front().find_last_of("\\/");
    if (last_slash != std::string::npos)
      folder_path = normalized_game_paths.front().substr(0, last_slash + 1);
  }
#endif

  if (extension == ".m3u" || extension == ".m3u8")
  {
    normalized_game_paths = ReadM3UFile(normalized_game_paths.front(), folder_path);
    if (normalized_game_paths.empty())
    {
      ERROR_LOG_FMT(BOOT, "Could not boot {}. M3U contains no paths", game->path);
      return false;
    }
  }

  for (auto& normalized_game_path : normalized_game_paths)
    Libretro::disk_paths.push_back(Libretro::DenormalizePath(normalized_game_path));

  Libretro::Input::Init(wsi);

  if (!BootManager::BootCore(Core::System::GetInstance(),
                             BootParameters::GenerateFromFile(normalized_game_paths), wsi))
  {
    ERROR_LOG_FMT(BOOT, "Could not boot {}", game->path);
    return false;
  }

  Libretro::Input::InitStage2();

  return true;
}

bool retro_load_game_special(unsigned game_type, const struct retro_game_info* info,
                             size_t num_info)
{
  return false;
}

void retro_unload_game(void)
{
  if (Core::IsRunning(Core::System::GetInstance()))
  {
    Core::Stop(Core::System::GetInstance());
    Core::Shutdown(Core::System::GetInstance());
  }

  if (!g_context_status.IsDestroyed() && g_video_backend)
    g_video_backend->Shutdown();

  // these are disabled in Shutdown on fullscreen/window toggle
  auto& system = Core::System::GetInstance();
  system.GetCustomResourceManager().Shutdown();
  system.GetFifo().Shutdown();

  // Rest of shutdown
  g_context_status.MarkUnitialized();
  Libretro::Input::Shutdown();
  Libretro::Log::Shutdown();
  UICommon::ShutdownControllers();
  UICommon::Shutdown();
}

namespace Libretro
{
// Disk swapping

// Dolphin expects to be able to use "/" (DIR_SEP) everywhere.
// RetroArch uses the OS separator.
// Convert between them when switching between systems.
std::string NormalizePath(const std::string& path)
{
  std::string newPath = path;
#ifdef _MSC_VER
  constexpr fs::path::value_type os_separator = fs::path::preferred_separator;
  static_assert(os_separator == DIR_SEP_CHR || os_separator == '\\', "Unsupported path separator");
  if (os_separator != DIR_SEP_CHR)
    std::replace(newPath.begin(), newPath.end(), '\\', DIR_SEP_CHR);
#endif

  return newPath;
}

std::string DenormalizePath(const std::string& path)
{
  std::string newPath = path;
#ifdef _MSC_VER
  constexpr fs::path::value_type os_separator = fs::path::preferred_separator;
  static_assert(os_separator == DIR_SEP_CHR || os_separator == '\\', "Unsupported path separator");
  if (os_separator != DIR_SEP_CHR)
    std::replace(newPath.begin(), newPath.end(), DIR_SEP_CHR, '\\');
#endif

  return newPath;
}

static bool retro_set_eject_state(bool ejected)
{
  if (eject_state == ejected)
    return false;

  eject_state = ejected;

  if (!ejected)
  {
    if (disk_index < disk_paths.size())
    {
      Core::RunOnCPUThread(Core::System::GetInstance(), [] {
        Core::CPUThreadGuard guard{Core::System::GetInstance()};
        const std::string path = NormalizePath(disk_paths[disk_index]);
        Core::System::GetInstance().GetDVDInterface().ChangeDisc(guard, path);
      }, true);  // wait_for_completion = true
    }
  }

  return true;
}

static bool retro_get_eject_state()
{
  return eject_state;
}

static unsigned retro_get_image_index()
{
  return disk_index;
}

static bool retro_set_image_index(unsigned index)
{
  if (eject_state)
    disk_index = index;

  return eject_state;
}

static unsigned retro_get_num_images()
{
  return (unsigned)disk_paths.size();
}

static bool retro_add_image_index()
{
  disk_paths.push_back("");

  return true;
}

static bool retro_replace_image_index(unsigned index, const struct retro_game_info* info)
{
  if (index >= disk_paths.size())
    return false;

  if (!info->path)
  {
    disk_paths.erase(disk_paths.begin() + index);
    if (!disk_paths.size())
      disk_index = -1;
    else if (disk_index > index)
      disk_index--;
  }
  else
    disk_paths[index] = info->path;

  return true;
}

static bool RETRO_CALLCONV retro_set_initial_image(unsigned index, const char* path)
{
  if (index >= disk_paths.size())
    index = 0;

  disk_index = index;

  return true;
}

static bool RETRO_CALLCONV retro_get_image_path(unsigned index, char* path, size_t len)
{
  if (index >= disk_paths.size())
    return false;

  if (disk_paths[index].empty())
    return false;

  strncpy(path, disk_paths[index].c_str(), len);
  return true;
}
static bool RETRO_CALLCONV retro_get_image_label(unsigned index, char* label, size_t len)
{
  if (index >= disk_paths.size())
    return false;

  if (disk_paths[index].empty())
    return false;

  strncpy(label, disk_paths[index].c_str(), len);
  return true;
}

static void InitDiskControlInterface()
{
  static retro_disk_control_ext_callback disk_control = {
      retro_set_eject_state,
      retro_get_eject_state,
      retro_get_image_index,
      retro_set_image_index,
      retro_get_num_images,
      retro_replace_image_index,
      retro_add_image_index,
      retro_set_initial_image,
      retro_get_image_path,
      retro_get_image_label,
  };

  environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &disk_control);
}
}  // namespace Libretro
