// VCSNX Vulkan probe (Plan 2, Task 3): checks on this console that mesa-switch's NVK driver can
// create a device, clear an image on the GPU and read it back, before the game's backend relies
// on exactly that path (GPU render + CPU readback). Prints to the screen and to
// sdmc:/switch/vcsnx/vkprobe.log.

#include <switch.h>
#include <vulkan/vulkan.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr std::uint32_t kSize = 256u;
std::FILE *g_log = nullptr;

void say(const char *format, ...) {
    char line[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    std::printf("%s\n", line);
    if (g_log != nullptr) {
        std::fprintf(g_log, "%s\n", line);
        std::fflush(g_log);
    }
    consoleUpdate(nullptr);
}

const char *result_name(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    default: return "VK_ERROR_(other)";
    }
}

bool check(VkResult result, const char *what) {
    if (result == VK_SUCCESS) return true;
    say("FAIL %s -> %s (%d)", what, result_name(result), static_cast<int>(result));
    return false;
}

bool find_memory_type(const VkPhysicalDeviceMemoryProperties &memory, std::uint32_t allowed,
                      VkMemoryPropertyFlags wanted, std::uint32_t &index) {
    for (std::uint32_t i = 0; i < memory.memoryTypeCount; ++i) {
        if ((allowed & (1u << i)) != 0u && (memory.memoryTypes[i].propertyFlags & wanted) == wanted) {
            index = i;
            return true;
        }
    }
    return false;
}

bool near(std::uint8_t value, int expected) { return std::abs(static_cast<int>(value) - expected) <= 1; }

bool run_probe() {
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "vcsnx-vkprobe";
    app.apiVersion = VK_API_VERSION_1_1;
    VkInstanceCreateInfo instance_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instance_info.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    if (!check(vkCreateInstance(&instance_info, nullptr, &instance), "vkCreateInstance")) return false;

    std::uint32_t gpu_count = 0;
    if (!check(vkEnumeratePhysicalDevices(instance, &gpu_count, nullptr), "vkEnumeratePhysicalDevices")) return false;
    if (gpu_count == 0u) {
        say("FAIL no physical devices");
        return false;
    }
    std::vector<VkPhysicalDevice> gpus(gpu_count);
    (void)vkEnumeratePhysicalDevices(instance, &gpu_count, gpus.data());
    const VkPhysicalDevice gpu = gpus[0];

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(gpu, &properties);
    say("device: %s", properties.deviceName);
    say("api %u.%u.%u  driver 0x%08x  vendor 0x%04x  device 0x%04x", VK_API_VERSION_MAJOR(properties.apiVersion),
        VK_API_VERSION_MINOR(properties.apiVersion), VK_API_VERSION_PATCH(properties.apiVersion),
        properties.driverVersion, properties.vendorID, properties.deviceID);
    say("maxImageDimension2D %u  maxSamplerAnisotropy %.1f", properties.limits.maxImageDimension2D,
        static_cast<double>(properties.limits.maxSamplerAnisotropy));
    VkPhysicalDeviceMemoryProperties memory{};
    vkGetPhysicalDeviceMemoryProperties(gpu, &memory);
    for (std::uint32_t i = 0; i < memory.memoryHeapCount; ++i)
        say("heap %u: %llu MB flags 0x%x", i, static_cast<unsigned long long>(memory.memoryHeaps[i].size >> 20),
            memory.memoryHeaps[i].flags);

    std::uint32_t family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &family_count, families.data());
    std::uint32_t family = UINT32_MAX;
    for (std::uint32_t i = 0; i < family_count; ++i) {
        say("queue family %u: flags 0x%x count %u", i, families[i].queueFlags, families[i].queueCount);
        if (family == UINT32_MAX && (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0u) family = i;
    }
    if (family == UINT32_MAX) {
        say("FAIL no graphics queue");
        return false;
    }

    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue_info.queueFamilyIndex = family;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &priority;
    VkDeviceCreateInfo device_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    device_info.queueCreateInfoCount = 1;
    device_info.pQueueCreateInfos = &queue_info;
    VkDevice device = VK_NULL_HANDLE;
    if (!check(vkCreateDevice(gpu, &device_info, nullptr, &device), "vkCreateDevice")) return false;
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, family, 0, &queue);

    VkImageCreateInfo image_info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.extent = {kSize, kSize, 1};
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    VkImage image = VK_NULL_HANDLE;
    if (!check(vkCreateImage(device, &image_info, nullptr, &image), "vkCreateImage")) return false;
    VkMemoryRequirements image_needs{};
    vkGetImageMemoryRequirements(device, image, &image_needs);
    std::uint32_t image_type = 0;
    if (!find_memory_type(memory, image_needs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image_type) &&
        !find_memory_type(memory, image_needs.memoryTypeBits, 0, image_type)) {
        say("FAIL no memory type for the image");
        return false;
    }
    VkMemoryAllocateInfo image_alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    image_alloc.allocationSize = image_needs.size;
    image_alloc.memoryTypeIndex = image_type;
    VkDeviceMemory image_memory = VK_NULL_HANDLE;
    if (!check(vkAllocateMemory(device, &image_alloc, nullptr, &image_memory), "vkAllocateMemory(image)")) return false;
    if (!check(vkBindImageMemory(device, image, image_memory, 0), "vkBindImageMemory")) return false;

    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = static_cast<VkDeviceSize>(kSize) * kSize * 4u;
    buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer buffer = VK_NULL_HANDLE;
    if (!check(vkCreateBuffer(device, &buffer_info, nullptr, &buffer), "vkCreateBuffer")) return false;
    VkMemoryRequirements buffer_needs{};
    vkGetBufferMemoryRequirements(device, buffer, &buffer_needs);
    std::uint32_t buffer_type = 0;
    if (!find_memory_type(memory, buffer_needs.memoryTypeBits,
                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buffer_type)) {
        say("FAIL no host-visible coherent memory for the readback buffer");
        return false;
    }
    VkMemoryAllocateInfo buffer_alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    buffer_alloc.allocationSize = buffer_needs.size;
    buffer_alloc.memoryTypeIndex = buffer_type;
    VkDeviceMemory buffer_memory = VK_NULL_HANDLE;
    if (!check(vkAllocateMemory(device, &buffer_alloc, nullptr, &buffer_memory), "vkAllocateMemory(buffer)")) return false;
    if (!check(vkBindBufferMemory(device, buffer, buffer_memory, 0), "vkBindBufferMemory")) return false;

    VkCommandPoolCreateInfo pool_info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool_info.queueFamilyIndex = family;
    VkCommandPool pool = VK_NULL_HANDLE;
    if (!check(vkCreateCommandPool(device, &pool_info, nullptr, &pool), "vkCreateCommandPool")) return false;
    VkCommandBufferAllocateInfo command_alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    command_alloc.commandPool = pool;
    command_alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_alloc.commandBufferCount = 1;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    if (!check(vkAllocateCommandBuffers(device, &command_alloc, &commands), "vkAllocateCommandBuffers")) return false;

    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (!check(vkBeginCommandBuffer(commands, &begin), "vkBeginCommandBuffer")) return false;
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageMemoryBarrier to_transfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    to_transfer.srcAccessMask = 0;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = image;
    to_transfer.subresourceRange = range;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &to_transfer);
    const VkClearColorValue color{{0.25f, 0.5f, 0.75f, 1.0f}};
    vkCmdClearColorImage(commands, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &range);
    VkImageMemoryBarrier to_source = to_transfer;
    to_source.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_source.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    to_source.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_source.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &to_source);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {kSize, kSize, 1};
    vkCmdCopyImageToBuffer(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1, &copy);
    VkBufferMemoryBarrier to_host{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    to_host.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_host.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
    to_host.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_host.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_host.buffer = buffer;
    to_host.size = VK_WHOLE_SIZE;
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, nullptr, 1,
                         &to_host, 0, nullptr);
    if (!check(vkEndCommandBuffer(commands), "vkEndCommandBuffer")) return false;

    VkFenceCreateInfo fence_info{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    if (!check(vkCreateFence(device, &fence_info, nullptr, &fence), "vkCreateFence")) return false;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commands;
    const std::uint64_t submitted = armGetSystemTick();
    if (!check(vkQueueSubmit(queue, 1, &submit, fence), "vkQueueSubmit")) return false;
    if (!check(vkWaitForFences(device, 1, &fence, VK_TRUE, 5'000'000'000ull), "vkWaitForFences")) return false;
    say("GPU clear + copy finished in %llu us",
        static_cast<unsigned long long>(armTicksToNs(armGetSystemTick() - submitted) / 1000u));

    void *mapped = nullptr;
    if (!check(vkMapMemory(device, buffer_memory, 0, VK_WHOLE_SIZE, 0, &mapped), "vkMapMemory")) return false;
    const auto *pixels = static_cast<const std::uint8_t *>(mapped);
    const std::uint8_t *first = pixels;
    const std::uint8_t *last = pixels + (static_cast<std::size_t>(kSize) * kSize - 1u) * 4u;
    say("pixel (0,0)     = %u %u %u %u", first[0], first[1], first[2], first[3]);
    say("pixel (255,255) = %u %u %u %u", last[0], last[1], last[2], last[3]);
    const bool ok = near(first[0], 64) && near(first[1], 128) && near(first[2], 191) && first[3] == 255 &&
                    near(last[0], 64) && near(last[1], 128) && near(last[2], 191) && last[3] == 255;
    if (!ok) say("FAIL readback does not match the clear color (expected 64 128 191 255)");
    vkUnmapMemory(device, buffer_memory);

    vkDestroyFence(device, fence, nullptr);
    vkDestroyCommandPool(device, pool, nullptr);
    vkDestroyBuffer(device, buffer, nullptr);
    vkFreeMemory(device, buffer_memory, nullptr);
    vkDestroyImage(device, image, nullptr);
    vkFreeMemory(device, image_memory, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
    return ok;
}

} // namespace

int main() {
    consoleInit(nullptr);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    PadState pad;
    padInitializeDefault(&pad);
    g_log = std::fopen("sdmc:/switch/vcsnx/vkprobe.log", "w");
    say("VCSNX Vulkan probe (mesa-switch NVK)");
    const bool ok = run_probe();
    say(ok ? "RESULT: PASS" : "RESULT: FAIL");
    say("Press + to exit.");
    while (appletMainLoop()) {
        padUpdate(&pad);
        if ((padGetButtonsDown(&pad) & HidNpadButton_Plus) != 0u) break;
        consoleUpdate(nullptr);
    }
    if (g_log != nullptr) std::fclose(g_log);
    consoleExit(nullptr);
    return ok ? 0 : 1;
}
