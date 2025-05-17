
#include "Common/Logging/Log.h"
#include "Common/Logging/LogManager.h"
#include "DolphinLibretro/Log.h"
#include "DolphinLibretro/Options.h"

namespace Libretro
{
extern retro_environment_t environ_cb;
namespace Log
{
class LogListener : public Common::Log::LogListener
{
public:
  LogListener(retro_log_printf_t log);
  ~LogListener() override;
  void Log(Common::Log::LogLevel level, const char* text) override;

private:
  retro_log_printf_t m_log;
};

static LogListener* logListener;

void Init()
{
  struct retro_log_callback log = {};
  if (environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &log) && log.log)
    logListener = new LogListener(log.log);
}

void Shutdown()
{
  delete logListener;
  logListener = nullptr;
}

LogListener::LogListener(retro_log_printf_t log) : m_log(log)
{
  Common::Log::LogManager::GetInstance()->SetLogLevel(Libretro::Options::logLevel);
  Common::Log::LogManager::GetInstance()->SetEnable(Common::Log::LogType::BOOT, true);
  Common::Log::LogManager::GetInstance()->SetEnable(Common::Log::LogType::CORE, true);
  Common::Log::LogManager::GetInstance()->SetEnable(Common::Log::LogType::VIDEO, true);
  Common::Log::LogManager::GetInstance()->SetEnable(Common::Log::LogType::HOST_GPU, true);
  Common::Log::LogManager::GetInstance()->SetEnable(Common::Log::LogType::COMMON, true);
  Common::Log::LogManager::GetInstance()->SetEnable(Common::Log::LogType::MEMMAP, true);
  Common::Log::LogManager::GetInstance()->SetEnable(Common::Log::LogType::DSPINTERFACE, true);
  Common::Log::LogManager::GetInstance()->SetEnable(Common::Log::LogType::DSPHLE, true);
  Common::Log::LogManager::GetInstance()->SetEnable(Common::Log::LogType::DSPLLE, true);
  Common::Log::LogManager::GetInstance()->SetEnable(Common::Log::LogType::DSP_MAIL, true);
  Common::Log::LogManager::GetInstance()->RegisterListener(LogListener::CUSTOM_LISTENER, this);
  Common::Log::LogManager::GetInstance()->EnableListener(LogListener::CONSOLE_LISTENER, false);
  Common::Log::LogManager::GetInstance()->EnableListener(LogListener::CUSTOM_LISTENER, true);
}

LogListener::~LogListener()
{
  Common::Log::LogManager::GetInstance()->EnableListener(LogListener::CUSTOM_LISTENER, false);
  Common::Log::LogManager::GetInstance()->EnableListener(LogListener::CONSOLE_LISTENER, true);
  Common::Log::LogManager::GetInstance()->RegisterListener(LogListener::CONSOLE_LISTENER, nullptr);
}

void LogListener::Log(Common::Log::LogLevel level, const char* text)
{
  switch (level)
  {
  case Common::Log::LogLevel::LDEBUG:
    m_log(RETRO_LOG_DEBUG, text);
    break;
  case Common::Log::LogLevel::LWARNING:
    m_log(RETRO_LOG_WARN, text);
    break;
  case Common::Log::LogLevel::LERROR:
    m_log(RETRO_LOG_ERROR, text);
    break;
  case Common::Log::LogLevel::LNOTICE:
  case Common::Log::LogLevel::LINFO:
  default:
    m_log(RETRO_LOG_INFO, text);
    break;
  }
}
}  // namespace Log
}  // namespace Libretro
