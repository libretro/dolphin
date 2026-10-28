#include "DolphinLibretro/VR.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include <libretro.h>

#include "Common/Logging/Log.h"
#include "Common/Matrix.h"
#include "Core/Config/GraphicsSettings.h"
#include "DolphinLibretro/Common/Globals.h"
#include "DolphinLibretro/Common/Options.h"
#include "DolphinLibretro/Input.h"
#include "VideoCommon/Present.h"
#include "VideoCommon/VertexShaderManager.h"
#include "VideoCommon/VideoConfig.h"

namespace Libretro
{
namespace VR
{
static bool s_views_sent = false;
// heading that counts as straight ahead for the game camera
static bool s_have_forward = false;
static Common::Quaternion s_forward = Common::Quaternion::Identity();
// The frontend showed both eyes this session, then stopped):
// stereoscopy is off until it shows both again, so the GPU doesn't draw a second eye.
static bool s_frontend_stereo_seen = false;
static bool s_frontend_flat = false;
// The right controller as player 1's Wiimote: its last velocity, for acceleration, and the
// heading the cursor is measured from.
static bool s_have_controller_velocity = false;
static Common::Vec3 s_controller_velocity;
static std::chrono::steady_clock::time_point s_controller_time;
static bool s_have_point_forward = false;
static Common::Quaternion s_point_forward = Common::Quaternion::Identity();

static Common::Quaternion ToQuaternion(const float* xyzw)
{
  return Common::Quaternion(xyzw[3], xyzw[0], xyzw[1], xyzw[2]).Normalized();
}

// A rotation about +Y by the heading of q's forward (-Z) axis, leaving out pitch and roll.
static Common::Quaternion Heading(const Common::Quaternion& q)
{
  const Common::Vec4& v = q.data;  // x, y, z, w
  const float yaw =
      std::atan2(2.0f * (v.x * v.z + v.w * v.y), 1.0f - 2.0f * (v.x * v.x + v.y * v.y));
  return Common::Quaternion(std::cos(yaw * 0.5f), 0.0f, std::sin(yaw * 0.5f), 0.0f);
}

Mode GetMode()
{
  const std::string mode = Options::GetCached<std::string>(Options::gfx_settings::VR_MODE);
  if (mode == "stereo")
    return Mode::Stereo;
  if (mode == "immersive")
    return Mode::Immersive;
  return Mode::Off;
}

bool IsEnabled()
{
  return GetMode() != Mode::Off;
}

void ApplyConfig()
{
  const Mode mode = GetMode();
  const bool enabled = mode != Mode::Off;

  // The headset's field of view must cover each eye's whole half of the frame.
  // Without VR, Dolphin stretches over the frame and the frontend is given
  // the option's aspect ratio (Main.cpp, FrameAspect).
  const AspectMode aspect = mode == Mode::Immersive || !enabled ?
    AspectMode::Stretch :
    static_cast<AspectMode>(Libretro::GetOption<int>(
      Options::gfx_settings::ASPECT_RATIO, static_cast<int>(AspectMode::Stretch)));
  Config::SetCurrent(Config::GFX_ASPECT_RATIO, aspect);
  Config::SetCurrent(Config::GFX_STEREO_MODE,
                     enabled && !s_frontend_flat ? StereoMode::SideBySide : StereoMode::Off);
  Config::SetCurrent(Config::GFX_STEREO_PER_EYE_RESOLUTION_FULL,
                     enabled && aspect != AspectMode::Stretch);
}

static void DisableCamera()
{
  // Take a new forward heading when the camera is next used
  s_have_forward = false;
  if (g_libretro_vr_camera.enabled)
  {
    g_libretro_vr_camera.enabled = false;
    g_libretro_vr_camera.dirty = true;
  }
}

static void ClearViews()
{
  if (!s_views_sent)
    return;

  retro_video_views views{};
  environ_cb(RETRO_ENVIRONMENT_SET_VIDEO_VIEWS, &views);
  s_views_sent = false;
}

static bool UpdateCamera(const retro_vr_frame_state& state)
{
  const float* left = state.eyes[RETRO_VR_EYE_LEFT].fov_tan;
  const float* right = state.eyes[RETRO_VR_EYE_RIGHT].fov_tan;
  for (int i = 0; i < 4; ++i)
  {
    if (!(left[i] > 0.0f) || !(right[i] > 0.0f))
      return false;
  }

  // fov_tan is left, right, up, down. Dolphin's projection maps view-space x / -z in [-l, r] to
  // [-1, 1] with scale 2 / (l + r) and offset (r - l) / (l + r); likewise y with up and down.
  const auto offset = [](float neg, float pos) { return (pos - neg) / (neg + pos); };
  const float left_offset_x = offset(left[0], left[1]);
  const float right_offset_x = offset(right[0], right[1]);

  LibretroVRCamera& camera = g_libretro_vr_camera;
  camera.scale_x = 4.0f / (left[0] + left[1] + right[0] + right[1]);
  camera.offset_x = (left_offset_x + right_offset_x) * 0.5f;
  camera.eye_offset_x = (right_offset_x - left_offset_x) * 0.5f;
  camera.scale_y = 4.0f / (left[2] + left[3] + right[2] + right[3]);
  camera.offset_y = (offset(left[3], left[2]) + offset(right[3], right[2])) * 0.5f;

  const Common::Quaternion head = ToQuaternion(state.eyes[RETRO_VR_EYE_LEFT].orientation);

  if (!s_have_forward || (state.flags & RETRO_VR_FRAME_RECENTERED))
  {
    s_forward = Heading(head);
    s_have_forward = true;
  }
  const Common::Quaternion relative = s_forward.Conjugate() * head;
  camera.view = Common::Matrix44::FromQuaternion(relative.Conjugate());

  camera.enabled = true;
  camera.dirty = true;
  return true;
}

static void UpdateVRMotion()
{
  if (!Input::UsesVRMotion(0))
    return;

  constexpr float G = 9.80665f;
  float accel[3] = {0.0f, 0.0f, G};  // at rest, level
  float gyro[3] = {};
  float point[2] = {};

  retro_vr_controller_poses poses{};
  const retro_vr_hand_state& hand = poses.hands[RETRO_VR_HAND_RIGHT];
  const retro_vr_pose& aim = hand.aim;
  if (!environ_cb(RETRO_ENVIRONMENT_GET_VR_CONTROLLER_POSES, &poses) ||
      !(hand.flags & RETRO_VR_HAND_ACTIVE) || !(aim.flags & RETRO_VR_POSE_ORIENTATION_VALID))
  {
    s_have_controller_velocity = false;
    Input::SetVRMotion(0, accel, gyro, point);
    return;
  }

  const Common::Quaternion remote = ToQuaternion(aim.orientation);
  const Common::Quaternion to_remote = remote.Conjugate();

  Common::Vec3 acceleration(0.0f, G, 0.0f);
  const auto now = std::chrono::steady_clock::now();
  if (aim.flags & RETRO_VR_POSE_LINEAR_VELOCITY_VALID)
  {
    const Common::Vec3 velocity(aim.linear_velocity[0], aim.linear_velocity[1],
                                aim.linear_velocity[2]);
    const float dt = std::chrono::duration<float>(now - s_controller_time).count();
    if (s_have_controller_velocity && dt > 1.0f / 500.0f && dt < 0.1f)
      acceleration += (velocity - s_controller_velocity) / dt;
    s_controller_velocity = velocity;
    s_have_controller_velocity = true;
  }
  else
  {
    s_have_controller_velocity = false;
  }
  s_controller_time = now;

  Common::Vec3 rotation;
  if (aim.flags & RETRO_VR_POSE_ANGULAR_VELOCITY_VALID)
    rotation = Common::Vec3(aim.angular_velocity[0], aim.angular_velocity[1],
                            aim.angular_velocity[2]);

  // Into the remote's axes (+X right, +Y up, +Z back), then the Wiimote's (+X left, +Y back,
  // +Z up): a rotation, so rates map the same way as accelerations.
  const Common::Vec3 a = to_remote * acceleration;
  const Common::Vec3 w = to_remote * rotation;
  accel[0] = -a.x;
  accel[1] = a.z;
  accel[2] = a.y;
  gyro[0] = -w.x;
  gyro[1] = w.z;
  gyro[2] = w.y;

  // The cursor is where the remoting is pointing.
  retro_vr_frame_state frame_state{};
  if (environ_cb(RETRO_ENVIRONMENT_GET_VR_FRAME_STATE, &frame_state) &&
      (frame_state.flags & RETRO_VR_FRAME_RECENTERED))
  {
    s_have_point_forward = false;
  }
  retro_vr_head_pose head{};
  if (!s_have_point_forward &&
      environ_cb(RETRO_ENVIRONMENT_GET_VR_HEAD_POSE, &head) &&
      (head.flags & RETRO_VR_HEAD_POSE_ORIENTATION_VALID))
  {
    s_point_forward = Heading(ToQuaternion(head.orientation));
    s_have_point_forward = true;
  }
  const Common::Vec3 direction = s_point_forward.Conjugate() * (remote * Common::Vec3(0, 0, -1));
  const float yaw = std::atan2(direction.x, -direction.z);
  const float pitch =
      std::atan2(direction.y, std::sqrt(direction.x * direction.x + direction.z * direction.z));
  constexpr float DEGREES = 180.0f / 3.14159265f;
  const float half_yaw =
      std::max(1, Options::GetCached<int>(Options::wiimote::IR_YAW, 25)) * 0.5f;
  const float half_pitch =
      std::max(1, Options::GetCached<int>(Options::wiimote::IR_PITCH, 25)) * 0.5f;
  point[0] = std::clamp(yaw * DEGREES / half_yaw, -1.0f, 1.0f);
  point[1] = std::clamp(pitch * DEGREES / half_pitch, -1.0f, 1.0f);

  Input::SetVRMotion(0, accel, gyro, point);
}

void Update(float eye_aspect_ratio)
{
  UpdateVRMotion();

  const Mode mode = GetMode();
  if (mode == Mode::Off)
  {
    DisableCamera();
    ClearViews();
    s_frontend_stereo_seen = false;
    s_frontend_flat = false;
    return;
  }

  unsigned status = 0;
  if (!environ_cb(RETRO_ENVIRONMENT_GET_VIDEO_VIEWS_STATUS, &status))
    status = 0;

  const bool frontend_stereo = (status & RETRO_VIDEO_VIEWS_STATUS_STEREO) != 0;
  s_frontend_stereo_seen |= frontend_stereo;
  const bool frontend_flat = s_frontend_stereo_seen && !frontend_stereo;
  if (frontend_flat != s_frontend_flat)
  {
    s_frontend_flat = frontend_flat;
    INFO_LOG_FMT(VIDEO, "VR: frontend {} both eyes, stereoscopy {}",
                 frontend_flat ? "stopped showing" : "shows", frontend_flat ? "off" : "on");
    Config::SetCurrent(Config::GFX_STEREO_MODE,
                       frontend_flat ? StereoMode::Off : StereoMode::SideBySide);
  }

  const unsigned width = g_presenter ? g_presenter->GetBackbufferWidth() : 0;
  const unsigned height = g_presenter ? g_presenter->GetBackbufferHeight() : 0;

  if (!(status & RETRO_VIDEO_VIEWS_STATUS_PRESENTS) || width < 2 || height == 0)
  {
    DisableCamera();
    ClearViews();
    return;
  }

  const bool stereo = g_ActiveConfig.stereo_mode == StereoMode::SideBySide;
  const unsigned eye_width = stereo ? width / 2 : width;

  retro_video_view views[2] = {};
  retro_video_views map{};
  map.views = views;
  map.reference_space = RETRO_VR_REFERENCE_SPACE_LOCAL;

  if (stereo && (status & RETRO_VIDEO_VIEWS_STATUS_STEREO))
  {
    views[0] = {0, 0, eye_width, height, 0, RETRO_VIDEO_VIEW_EYE_LEFT, eye_aspect_ratio};
    views[1] = {eye_width, 0, eye_width, height, 0, RETRO_VIDEO_VIEW_EYE_RIGHT, eye_aspect_ratio};
    map.num_views = 2;
  }
  else
  {
    views[0] = {0, 0, eye_width, height, 0, RETRO_VIDEO_VIEW_EYE_NONE, eye_aspect_ratio};
    map.num_views = 1;
  }

  g_libretro_vr_camera.display_aspect = eye_aspect_ratio;

  retro_vr_frame_state frame_state{};
  if (mode == Mode::Immersive && map.num_views == 2 && (status & RETRO_VIDEO_VIEWS_STATUS_HMD) &&
      environ_cb(RETRO_ENVIRONMENT_GET_VR_FRAME_STATE, &frame_state) && UpdateCamera(frame_state))
  {
    map.flags = RETRO_VIDEO_VIEWS_FLAG_TRACKED;
  }
  else
  {
    DisableCamera();
  }

  environ_cb(RETRO_ENVIRONMENT_SET_VIDEO_VIEWS, &map);
  s_views_sent = true;
}

void Shutdown()
{
  DisableCamera();
  g_libretro_vr_camera.applied = false;
  g_libretro_vr_camera.game_tan_x = 0.0f;
  g_libretro_vr_camera.game_tan_y = 0.0f;
  s_views_sent = false;
  s_frontend_stereo_seen = false;
  s_frontend_flat = false;
  s_have_controller_velocity = false;
  s_have_point_forward = false;
}
}  // namespace VR
}  // namespace Libretro
