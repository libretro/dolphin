#pragma once

#include <string>

#include <libretro.h>
#include "Common/WindowSystemInfo.h"

// only 4 support sensors, but the array may be upto 8 if connected GC controllers
#define NUM_CONTROLLERS_FOR_SENSORS 8

// Sensors addressable on one port. Index 0 is the controller itself, index 1
// whatever is plugged into it (for a Wii Remote, the Nunchuk).
#define NUM_SENSOR_SUBDEVICES 2

struct WiimoteUpdateFlags
{
    bool irMode        = false;
    bool irOffset      = false; // center
    bool irYaw         = false; // width
    bool irPitch       = false; // height
    bool irDeadzone    = false;
    bool irModifier    = false;
    bool swingModifier = false;
    bool swingAngle    = false;
    bool sideways      = false; // hotkey
    bool upright       = false; // hotkey
    bool rumble        = false;
    bool gcMicBtn      = false;
    bool irPassthrough = false;

    bool any() const {
        return irMode || irOffset || irYaw || irPitch ||
               irDeadzone || irModifier || swingModifier ||
               swingAngle || sideways || rumble || gcMicBtn || irPassthrough;
    }
};

void refresh_all_wiimote_flags(unsigned port, unsigned device);
void poll_microphone();
void UpdateInputDescriptors();

namespace Libretro
{
namespace Input
{
constexpr std::string_view source = "Libretro";
extern double g_accel_pos[NUM_CONTROLLERS_FOR_SENSORS][NUM_SENSOR_SUBDEVICES][3];
extern double g_accel_neg[NUM_CONTROLLERS_FOR_SENSORS][NUM_SENSOR_SUBDEVICES][3];
extern double g_gyro_pos[NUM_CONTROLLERS_FOR_SENSORS][NUM_SENSOR_SUBDEVICES][3];
extern double g_gyro_neg[NUM_CONTROLLERS_FOR_SENSORS][NUM_SENSOR_SUBDEVICES][3];

static retro_sensor_interface sensor_interface = {0};

/// The ciface device name for one port's sub-device. Index 0 keeps the plain
/// "Sensor", so pre-existing control expressions still resolve.
inline std::string SensorDeviceName(unsigned subdevice)
{
  return subdevice == 0 ? std::string("Sensor") : "Sensor" + std::to_string(subdevice);
}

void Init(const WindowSystemInfo& wsi);
void InitStage2();
void InitSensors();
void UpdateAccelerometer(unsigned port, unsigned subdevice);
void UpdateGyro(unsigned port);
void Update();
void Shutdown();
void ResetControllers(const WiimoteUpdateFlags& f);
void BluetoothPassthroughBind();
void UpdateWiimoteMappings(const WiimoteUpdateFlags& f, unsigned port, unsigned device);
void UpdateGCMappings(const WiimoteUpdateFlags& f, unsigned port, unsigned device);
} // namespace Input
} // namespace Libretro

class SensorDevice : public ciface::Core::Device
{
public:
  /// `port` picks the player, `subdevice` the sensor on that port.
  SensorDevice(unsigned port, unsigned subdevice)
      : m_port(port), m_subdevice(subdevice),
        m_name(Libretro::Input::SensorDeviceName(subdevice))
  {
  }

  std::string GetName() const override { return m_name; }
  std::string GetSource() const override { return std::string(Libretro::Input::source); }
  unsigned int GetPort() const { return m_port; }
  ciface::Core::DeviceRemoval UpdateInput() override { return ciface::Core::DeviceRemoval::Keep; }

private:
  class ScalarInput : public ciface::Core::Device::Input
  {
  public:
    ScalarInput(const char* name, const double* slot) : m_name(name), m_slot(slot) {}
    std::string GetName() const override { return m_name; }
    ControlState GetState() const override { return *m_slot; }
  private:
    const char* m_name;
    const double* m_slot;
  };

public:
  /// Each axis is a PAIR of one-sided inputs: ControlExpression clamps a control
  /// to >= 0, so a lone signed input loses half its travel. The IMU groups
  /// subtract one direction from the other, rebuilding the signed value.
  void RegisterAll()
  {
    AddInput(new ScalarInput("GyroX+", &Libretro::Input::g_gyro_pos[m_port][m_subdevice][0]));
    AddInput(new ScalarInput("GyroX-", &Libretro::Input::g_gyro_neg[m_port][m_subdevice][0]));
    AddInput(new ScalarInput("GyroY+", &Libretro::Input::g_gyro_pos[m_port][m_subdevice][1]));
    AddInput(new ScalarInput("GyroY-", &Libretro::Input::g_gyro_neg[m_port][m_subdevice][1]));
    AddInput(new ScalarInput("GyroZ+", &Libretro::Input::g_gyro_pos[m_port][m_subdevice][2]));
    AddInput(new ScalarInput("GyroZ-", &Libretro::Input::g_gyro_neg[m_port][m_subdevice][2]));
    AddInput(new ScalarInput("AccelX+", &Libretro::Input::g_accel_pos[m_port][m_subdevice][0]));
    AddInput(new ScalarInput("AccelX-", &Libretro::Input::g_accel_neg[m_port][m_subdevice][0]));
    AddInput(new ScalarInput("AccelY+", &Libretro::Input::g_accel_pos[m_port][m_subdevice][1]));
    AddInput(new ScalarInput("AccelY-", &Libretro::Input::g_accel_neg[m_port][m_subdevice][1]));
    AddInput(new ScalarInput("AccelZ+", &Libretro::Input::g_accel_pos[m_port][m_subdevice][2]));
    AddInput(new ScalarInput("AccelZ-", &Libretro::Input::g_accel_neg[m_port][m_subdevice][2]));
  }

private:
  unsigned m_port;
  unsigned m_subdevice;
  std::string m_name;
};

class GyroDevice : public ciface::Core::Device
{
private:
  class GyroAxis : public ciface::Core::Device::Input
  {
  public:
    enum Axis { PITCH, ROLL, YAW };

    GyroAxis(unsigned port, Axis axis, const char* name)
        : m_port(port), m_axis(axis), m_name(name) {}

    std::string GetName() const override { return m_name; }

    ControlState GetState() const override
    {
      if (!Libretro::Input::sensor_interface.get_sensor_input)
        return 0.0;

      switch (m_axis)
      {
      case PITCH:
        return Libretro::Input::sensor_interface.get_sensor_input(m_port, RETRO_SENSOR_GYROSCOPE_Y);
      case ROLL:
        return Libretro::Input::sensor_interface.get_sensor_input(m_port, RETRO_SENSOR_GYROSCOPE_X);
      case YAW:
        return Libretro::Input::sensor_interface.get_sensor_input(m_port, RETRO_SENSOR_GYROSCOPE_Z);
      }
      return 0.0;
    }

  private:
    const unsigned m_port;
    const Axis m_axis;
    const char* m_name;
  };

public:
  GyroDevice(unsigned port)
  {
    AddInput(new GyroAxis(port, GyroAxis::PITCH, "Pitch"));
    AddInput(new GyroAxis(port, GyroAxis::ROLL, "Roll"));
    AddInput(new GyroAxis(port, GyroAxis::YAW, "Yaw"));
  }

  std::string GetName() const override { return "Gyroscope"; }
  std::string GetSource() const override { return std::string(Libretro::Input::source); }
};
