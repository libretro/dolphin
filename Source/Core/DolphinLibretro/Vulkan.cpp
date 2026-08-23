
// must be first
#include "VideoBackends/Vulkan/VulkanLoader.h"

#include <cassert>
#include <condition_variable>
#include <cstring>
#include <libretro_vulkan.h>
#include <mutex>
#include <vector>

#include "Core/Config/MainSettings.h"
#include "DolphinLibretro/Video.h"
#include "DolphinLibretro/Vulkan.h"
#include "VideoBackends/Vulkan/VulkanContext.h"
#include "VideoCommon/FramebufferManager.h"

#include <unordered_set>
#include <string>

#define LIBRETRO_VK_WARP_LIST()                                                                    \
  LIBRETRO_VK_WARP_FUNC(vkDestroyInstance);                                                        \
  LIBRETRO_VK_WARP_FUNC(vkCreateDevice);                                                           \
  LIBRETRO_VK_WARP_FUNC(vkDestroyDevice);                                                          \
  LIBRETRO_VK_WARP_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR);                                \
  LIBRETRO_VK_WARP_FUNC(vkDestroySurfaceKHR);                                                      \
  LIBRETRO_VK_WARP_FUNC(vkCreateSwapchainKHR);                                                     \
  LIBRETRO_VK_WARP_FUNC(vkGetSwapchainImagesKHR);                                                  \
  LIBRETRO_VK_WARP_FUNC(vkAcquireNextImageKHR);                                                    \
  LIBRETRO_VK_WARP_FUNC(vkQueuePresentKHR);                                                        \
  LIBRETRO_VK_WARP_FUNC(vkDestroySwapchainKHR);                                                    \
  LIBRETRO_VK_WARP_FUNC(vkQueueSubmit);                                                            \
  LIBRETRO_VK_WARP_FUNC(vkQueueWaitIdle);                                                          \
  LIBRETRO_VK_WARP_FUNC(vkCmdPipelineBarrier);                                                     \
  LIBRETRO_VK_WARP_FUNC(vkCreateRenderPass)

#define LIBRETRO_VK_WARP_FUNC(x)                                                                   \
  extern PFN_##x x;                                                                                \
  static PFN_##x x##_org

LIBRETRO_VK_WARP_FUNC(vkGetInstanceProcAddr);
LIBRETRO_VK_WARP_FUNC(vkGetDeviceProcAddr);
LIBRETRO_VK_WARP_LIST();

extern PFN_vkCreateInstance vkCreateInstance;
extern PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
extern PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
extern PFN_vkAllocateMemory vkAllocateMemory;
extern PFN_vkBindImageMemory vkBindImageMemory;
extern PFN_vkCreateImage vkCreateImage;
extern PFN_vkDestroyImage vkDestroyImage;
extern PFN_vkCreateImageView vkCreateImageView;
extern PFN_vkDestroyImageView vkDestroyImageView;
extern PFN_vkFreeMemory vkFreeMemory;

namespace Libretro
{
namespace Video
{
namespace Vk
{
static retro_hw_render_interface_vulkan* vulkan;

static struct
{
  VkInstance instance;
  VkPhysicalDevice gpu;
  VkSurfaceKHR surface;
  uint32_t width;
  uint32_t height;
  PFN_vkGetInstanceProcAddr get_instance_proc_addr;
  const char** required_device_extensions;
  unsigned num_required_device_extensions;
  const char** required_device_layers;
  unsigned num_required_device_layers;
  const VkPhysicalDeviceFeatures* required_features;
} initInfo;
static bool DEDICATED_ALLOCATION;

struct VkSwapchainImage
{
  VkImage handle;
  VkDeviceMemory memory;
  retro_vulkan_image retro_image;
};

struct VkSwapchainKHR_T
{
  uint32_t count;
  std::vector<VkSwapchainImage> images;
  std::mutex mutex;
  std::condition_variable condVar;
  int current_index;
};
static VkSwapchainKHR_T chain;

VkSurfaceKHR GetSurface() { return initInfo.surface; }

static VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                                       const VkAllocationCallbacks* pAllocator,
                                                       VkInstance* pInstance)
{
  *pInstance = initInfo.instance;
  return VK_SUCCESS;
}

static void AddNameUnique(std::vector<const char*>& list, const char* value)
{
  for (const char* name : list)
    if (!strcmp(value, name))
      return;

  list.push_back(value);
}


static VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physicalDevice,
  const VkDeviceCreateInfo* pCreateInfo,
  const VkAllocationCallbacks* pAllocator,
  VkDevice* pDevice)
{
  VkDeviceCreateInfo info = *pCreateInfo;

  // Copy requested layers/extensions/features from the incoming create-info
  std::vector<const char*> enabled_layers(
  info.ppEnabledLayerNames, info.ppEnabledLayerNames + info.enabledLayerCount);

  std::vector<const char*> enabled_exts(
  info.ppEnabledExtensionNames, info.ppEnabledExtensionNames + info.enabledExtensionCount);

  VkPhysicalDeviceFeatures enabled_features{};
  if (info.pEnabledFeatures)
  enabled_features = *info.pEnabledFeatures;

  // Merge in libretro-required layers/extensions/features
  for (unsigned i = 0; i < initInfo.num_required_device_layers; i++)
  AddNameUnique(enabled_layers, initInfo.required_device_layers[i]);

  for (unsigned i = 0; i < initInfo.num_required_device_extensions; i++)
  AddNameUnique(enabled_exts, initInfo.required_device_extensions[i]);

  AddNameUnique(enabled_exts, VK_KHR_SAMPLER_MIRROR_CLAMP_TO_EDGE_EXTENSION_NAME);

  // OR in required features bitwise (struct of VkBool32)
  if (initInfo.required_features)
  {
    for (unsigned i = 0; i < sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32); i++)
    {
      if (reinterpret_cast<const VkBool32*>(initInfo.required_features)[i])
        reinterpret_cast<VkBool32*>(&enabled_features)[i] = VK_TRUE;
      }
    }

  // Build availability set for this exact physicalDevice
  uint32_t ext_count = 0;
  vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &ext_count, nullptr);
  std::vector<VkExtensionProperties> props(ext_count);
  if (ext_count)
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &ext_count, props.data());

  std::unordered_set<std::string> avail;
  avail.reserve(props.size());
  for (const auto& p : props)
    avail.emplace(p.extensionName);

  // Filter and de-dup the requested extensions against availability
  std::vector<const char*> filtered_exts;
  filtered_exts.reserve(enabled_exts.size());

  std::unordered_set<std::string> seen;
  seen.reserve(enabled_exts.size());

  for (const char* name : enabled_exts)
  {
    if (!name)
      continue;

    // De-dup while preserving order
    if (!seen.insert(name).second)
      continue;

    if (avail.find(name) != avail.end())
    {
      filtered_exts.push_back(name);
    }
    else
    {
      WARN_LOG_FMT(VIDEO, "Dropping unsupported device extension: {}", name);
    }
  }

  // Side flag for VMA or other paths
  DEDICATED_ALLOCATION = false;
  for (const char* extension_name : filtered_exts)
  {
    if (extension_name &&
      !std::strcmp(extension_name, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME))
      {
        DEDICATED_ALLOCATION = true;
        break;
      }
    }

  // Finalize create-info with filtered lists
  info.enabledLayerCount = static_cast<uint32_t>(enabled_layers.size());
  info.ppEnabledLayerNames = info.enabledLayerCount ? enabled_layers.data() : nullptr;

  info.enabledExtensionCount = static_cast<uint32_t>(filtered_exts.size());
  info.ppEnabledExtensionNames = info.enabledExtensionCount ? filtered_exts.data() : nullptr;

  info.pEnabledFeatures = &enabled_features;

  // Call through to the original
  return vkCreateDevice_org(physicalDevice, &info, pAllocator, pDevice);
}

static VKAPI_ATTR VkResult VKAPI_CALL
vkCreateLibretroSurfaceKHR(VkInstance instance, const void* pCreateInfo,
                           const VkAllocationCallbacks* pAllocator, VkSurfaceKHR* pSurface)
{
  *pSurface = initInfo.surface;
  return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL
vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                                          VkSurfaceCapabilitiesKHR* pSurfaceCapabilities)
{
  VkResult res =
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR_org(physicalDevice, surface, pSurfaceCapabilities);
  if (res == VK_SUCCESS)
  {
    pSurfaceCapabilities->currentExtent.width = initInfo.width;
    pSurfaceCapabilities->currentExtent.height = initInfo.height;
  }
  return res;
}

static bool MemoryTypeFromProperties(uint32_t typeBits, VkFlags requirements_mask,
                                     uint32_t* typeIndex)
{
  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(vulkan->gpu, &memory_properties);
  // Search memtypes to find first index with those properties
  for (uint32_t i = 0; i < 32; i++)
  {
    if ((typeBits & 1) == 1)
    {
      // Type is available, does it match user properties?
      if ((memory_properties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask)
      {
        *typeIndex = i;
        return true;
      }
    }
    typeBits >>= 1;
  }
  // No memory types matched, return failure
  return false;
}

static VKAPI_ATTR VkResult VKAPI_CALL
vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR* pCreateInfo,
                     const VkAllocationCallbacks* pAllocator, VkSwapchainKHR* pSwapchain)
{
  uint32_t swapchain_mask = vulkan->get_sync_index_mask(vulkan->handle);

  chain.count = 0;
  while (swapchain_mask)
  {
    chain.count++;
    swapchain_mask >>= 1;
  }
  chain.images.resize(chain.count);

  for (uint32_t i = 0; i < chain.count; i++)
  {
    {
      VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
      info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
      info.imageType = VK_IMAGE_TYPE_2D;
      info.format = pCreateInfo->imageFormat;
      info.extent.width = pCreateInfo->imageExtent.width;
      info.extent.height = pCreateInfo->imageExtent.height;
      info.extent.depth = 1;
      info.mipLevels = 1;
      info.arrayLayers = 1;
      info.samples = VK_SAMPLE_COUNT_1_BIT;
      info.tiling = VK_IMAGE_TILING_OPTIMAL;
      info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                   VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

      vkCreateImage(device, &info, pAllocator, &chain.images[i].handle);
    }

    VkMemoryRequirements memreq;
    vkGetImageMemoryRequirements(device, chain.images[i].handle, &memreq);

    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize = memreq.size;

    VkMemoryDedicatedAllocateInfoKHR dedicated{
        VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR};
    if (DEDICATED_ALLOCATION)
    {
      alloc.pNext = &dedicated;
      dedicated.image = chain.images[i].handle;
    }

    MemoryTypeFromProperties(memreq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                             &alloc.memoryTypeIndex);
    VkResult res = vkAllocateMemory(device, &alloc, pAllocator, &chain.images[i].memory);
    (void)res;
    assert(res == VK_SUCCESS);
    res = vkBindImageMemory(device, chain.images[i].handle, chain.images[i].memory, 0);
    assert(res == VK_SUCCESS);

    chain.images[i].retro_image.create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    chain.images[i].retro_image.create_info.image = chain.images[i].handle;
    chain.images[i].retro_image.create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    chain.images[i].retro_image.create_info.format = pCreateInfo->imageFormat;
    chain.images[i].retro_image.create_info.components = {
        VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
        VK_COMPONENT_SWIZZLE_A};
    chain.images[i].retro_image.create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    chain.images[i].retro_image.create_info.subresourceRange.layerCount = 1;
    chain.images[i].retro_image.create_info.subresourceRange.levelCount = 1;
    res = vkCreateImageView(device, &chain.images[i].retro_image.create_info, pAllocator,
                            &chain.images[i].retro_image.image_view);
    assert(res == VK_SUCCESS);

    chain.images[i].retro_image.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  chain.current_index = -1;
  *pSwapchain = (VkSwapchainKHR)&chain;

  return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice device,
                                                              VkSwapchainKHR swapchain_,
                                                              uint32_t* pSwapchainImageCount,
                                                              VkImage* pSwapchainImages)
{
  VkSwapchainKHR_T* swapchain = (VkSwapchainKHR_T*)swapchain_;
  if (pSwapchainImages)
  {
    assert(*pSwapchainImageCount <= swapchain->count);
    for (uint32_t i = 0; i < *pSwapchainImageCount; i++)
      pSwapchainImages[i] = swapchain->images[i].handle;
  }
  else
    *pSwapchainImageCount = swapchain->count;

  return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice device,
                                                            VkSwapchainKHR swapchain,
                                                            uint64_t timeout, VkSemaphore semaphore,
                                                            VkFence fence, uint32_t* pImageIndex)
{
  vulkan->wait_sync_index(vulkan->handle);
  *pImageIndex = vulkan->get_sync_index(vulkan->handle);
#if 0
  vulkan->set_signal_semaphore(vulkan->handle, semaphore);
#endif
  return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue,
                                                        const VkPresentInfoKHR* pPresentInfo)
{
  VkSwapchainKHR_T* swapchain = (VkSwapchainKHR_T*)pPresentInfo->pSwapchains[0];
  std::unique_lock<std::mutex> lock(swapchain->mutex);
#if 0
	if(chain.current_index >= 0)
		chain.condVar.wait(lock);
#endif

  chain.current_index = pPresentInfo->pImageIndices[0];
#if 0
  vulkan->set_image(vulkan->handle, &swapchain->images[pPresentInfo->pImageIndices[0]].retro_image,
                    pPresentInfo->waitSemaphoreCount, pPresentInfo->pWaitSemaphores,
                    vulkan->queue_index);
#else
  vulkan->set_image(vulkan->handle, &swapchain->images[pPresentInfo->pImageIndices[0]].retro_image,
                    0, nullptr, vulkan->queue_index);
#endif
  swapchain->condVar.notify_all();
  uint32_t out_w = 0;
  uint32_t out_h = 0;

  if (g_framebuffer_manager)
  {
    if (AbstractTexture* efb_tex = g_framebuffer_manager->GetEFBColorTexture())
    {
      out_w = efb_tex->GetWidth();
      out_h = efb_tex->GetHeight();
    }
  }

  // Fallback: negotiated init dimensions (or a sane default).
  if (out_w == 0 || out_h == 0)
  {
    out_w = Libretro::Video::Vk::initInfo.width ? Libretro::Video::Vk::initInfo.width : 640;
    out_h = Libretro::Video::Vk::initInfo.height ? Libretro::Video::Vk::initInfo.height : 480;
  }

  video_cb(RETRO_HW_FRAME_BUFFER_VALID, out_w, out_h, 0);
  return VK_SUCCESS;
}

void WaitForPresentation()
{
  std::unique_lock<std::mutex> lock(chain.mutex);
  if (chain.current_index < 0)
    chain.condVar.wait(lock);
#if 0
	chain.current_index = -1;
	chain.condVar.notify_all();
#endif
}

static VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance instance,
                                                    const VkAllocationCallbacks* pAllocator)
{
}

static VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device,
                                                  const VkAllocationCallbacks* pAllocator)
{
}

static VKAPI_ATTR void VKAPI_CALL vkDestroySurfaceKHR(VkInstance instance, VkSurfaceKHR surface,
                                                      const VkAllocationCallbacks* pAllocator)
{
}

static VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain,
                                                        const VkAllocationCallbacks* pAllocator)
{
  for (uint32_t i = 0; i < chain.count; i++)
  {
    vkDestroyImage(device, chain.images[i].handle, pAllocator);
    vkDestroyImageView(device, chain.images[i].retro_image.image_view, pAllocator);
    vkFreeMemory(device, chain.images[i].memory, pAllocator);
  }

  chain.images.clear();
  chain.count = 0;
  chain.current_index = -1;
}

static VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue queue, uint32_t submitCount,
                                                    const VkSubmitInfo* pSubmits, VkFence fence)
{
  VkResult res = VK_SUCCESS;

#if 0
	for(int i = 0; i < submitCount; i++)
		vulkan->set_command_buffers(vulkan->handle, pSubmits[i].commandBufferCount, pSubmits[i].pCommandBuffers);
#else
#if 1
  for (uint32_t i = 0; i < submitCount; i++)
  {
    ((VkSubmitInfo*)pSubmits)[i].waitSemaphoreCount = 0;
    ((VkSubmitInfo*)pSubmits)[i].pWaitSemaphores = nullptr;
    ((VkSubmitInfo*)pSubmits)[i].signalSemaphoreCount = 0;
    ((VkSubmitInfo*)pSubmits)[i].pSignalSemaphores = nullptr;
  }
#endif
  vulkan->lock_queue(vulkan->handle);
  res = vkQueueSubmit_org(queue, submitCount, pSubmits, fence);
  vulkan->unlock_queue(vulkan->handle);
#endif

  return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue queue)
{
  vulkan->lock_queue(vulkan->handle);
  VkResult res = vkQueueWaitIdle_org(queue);
  vulkan->unlock_queue(vulkan->handle);
  return res;
}

static VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(
    VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount, const VkMemoryBarrier* pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount, const VkImageMemoryBarrier* pImageMemoryBarriers)
{
  VkImageMemoryBarrier* barriers = (VkImageMemoryBarrier*)pImageMemoryBarriers;
  for (uint32_t i = 0; i < imageMemoryBarrierCount; i++)
  {
    if (pImageMemoryBarriers[i].oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
      barriers[i].oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barriers[i].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    if (pImageMemoryBarriers[i].newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
      barriers[i].newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barriers[i].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
  }
  return vkCmdPipelineBarrier_org(commandBuffer, srcStageMask, dstStageMask, dependencyFlags,
                                  memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount,
                                  pBufferMemoryBarriers, imageMemoryBarrierCount, barriers);
}

static VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(VkDevice device,
                                                         const VkRenderPassCreateInfo* pCreateInfo,
                                                         const VkAllocationCallbacks* pAllocator,
                                                         VkRenderPass* pRenderPass)
{
  if (pCreateInfo->pAttachments[0].finalLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    ((VkAttachmentDescription*)pCreateInfo->pAttachments)[0].finalLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

  return vkCreateRenderPass_org(device, pCreateInfo, pAllocator, pRenderPass);
}

#undef LIBRETRO_VK_WARP_FUNC
#define LIBRETRO_VK_WARP_FUNC(x)                                                                   \
  do                                                                                               \
  {                                                                                                \
    if (!strcmp(pName, #x))                                                                        \
    {                                                                                              \
      x##_org = (PFN_##x)fptr;                                                                     \
      return (PFN_vkVoidFunction)Libretro::Video::Vk::x;                                           \
    }                                                                                              \
  } while (0)

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance,
                                                                      const char* pName)
{
  if (!strcmp(pName, "vkCreateLibretroSurfaceKHR") || !strcmp(pName, "vkCreateWin32SurfaceKHR") ||
      !strcmp(pName, "vkCreateAndroidSurfaceKHR") || !strcmp(pName, "vkCreateXlibSurfaceKHR") ||
      !strcmp(pName, "vkCreateXcbSurfaceKHR") || !strcmp(pName, "vkCreateWaylandSurfaceKHR"))
  {
    return (PFN_vkVoidFunction)vkCreateLibretroSurfaceKHR;
  }

  PFN_vkVoidFunction fptr = vkGetInstanceProcAddr_org(instance, pName);
  if (!fptr)
    return fptr;

  LIBRETRO_VK_WARP_LIST();

  return fptr;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device,
                                                                    const char* pName)
{
  PFN_vkVoidFunction fptr = vkGetDeviceProcAddr_org(device, pName);
  if (!fptr)
    return fptr;

  LIBRETRO_VK_WARP_LIST();

  return fptr;
}

void Init(VkInstance instance, VkPhysicalDevice gpu, VkSurfaceKHR surface,
          PFN_vkGetInstanceProcAddr get_instance_proc_addr, const char** required_device_extensions,
          unsigned num_required_device_extensions, const char** required_device_layers,
          unsigned num_required_device_layers, const VkPhysicalDeviceFeatures* required_features)
{
  assert(surface);

  initInfo.instance = instance;
  initInfo.gpu = gpu;
  initInfo.surface = surface;
  initInfo.width = -1;
  initInfo.height = -1;
  initInfo.get_instance_proc_addr = get_instance_proc_addr;

  static const char* extra_extensions[] = {
      "VK_KHR_get_physical_device_properties2"
  };

  static std::vector<const char*> combined_extensions;
  combined_extensions.clear();
  combined_extensions.reserve(num_required_device_extensions + std::size(extra_extensions));

  for (unsigned i = 0; i < num_required_device_extensions; ++i)
    combined_extensions.push_back(required_device_extensions[i]);
  for (const char* ext : extra_extensions)
    combined_extensions.push_back(ext);

  initInfo.required_device_extensions = combined_extensions.data();
  initInfo.num_required_device_extensions = static_cast<unsigned>(combined_extensions.size());

  initInfo.required_device_layers = required_device_layers;
  initInfo.num_required_device_layers = num_required_device_layers;
  initInfo.required_features = required_features;

  vkGetInstanceProcAddr_org = get_instance_proc_addr;
  ::vkGetInstanceProcAddr = vkGetInstanceProcAddr;
  vkGetDeviceProcAddr_org =
    (PFN_vkGetDeviceProcAddr)vkGetInstanceProcAddr_org(instance, "vkGetDeviceProcAddr");
  ::vkGetDeviceProcAddr = vkGetDeviceProcAddr;
  ::vkCreateInstance = vkCreateInstance;
}

void SetSurfaceSize(uint32_t width, uint32_t height)
{
  initInfo.width = width;
  initInfo.height = height;
}

void SetHWRenderInterface(retro_hw_render_interface* hw_render_interface)
{
  vulkan = (retro_hw_render_interface_vulkan*)hw_render_interface;
}

void Shutdown()
{
  memset(&initInfo, 0x00, sizeof(initInfo));
  vulkan = NULL;
  DEDICATED_ALLOCATION = false;
}
}  // namespace Vk
}  // namespace Video
}  // namespace Libretro
