#include "lve_device.hpp"

// std headers
#include <cstring>
#include <iostream>
#include <set>
#include <unordered_set>

namespace lve {

    /**
    * Debug callback function called by validation layers when issues are detected
    * Outputs validation messages to help debug Vulkan API usage errors
    */
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
            void *pUserData) {
        // Print validation layer messages to stderr for debugging
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        // Return VK_FALSE to continue execution (VK_TRUE would terminate)
        return VK_FALSE;
    }

    /**
    * Creates debug messenger extension (not part of core Vulkan)
    * Must be loaded dynamically since it's an extension function
    */
    VkResult CreateDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
            const VkAllocationCallbacks *pAllocator,
            VkDebugUtilsMessengerEXT *pDebugMessenger) {
        // Get function pointer for extension function
        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance,
                "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
        } else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

    /**
    * Destroys debug messenger extension
    * Also dynamically loaded since it's an extension function
    */
    void DestroyDebugUtilsMessengerEXT(
            VkInstance instance,
            VkDebugUtilsMessengerEXT debugMessenger,
            const VkAllocationCallbacks *pAllocator) {
        // Get function pointer for extension function
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                instance,
                "vkDestroyDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }

    // DEVICE CLASS IMPLEMENTATION
    /**
    * Constructor: Sets up complete Vulkan device in required order
    * Each step depends on the previous ones being completed
    */
    LveDevice::LveDevice(LveWindow &window) : window{window} {
        createInstance();        // Create Vulkan instance with extensions
        setupDebugMessenger();   // Setup validation layer debugging
        createSurface();         // Create window surface (requires GLFW)
        pickPhysicalDevice();    // Select suitable GPU from available devices
        createLogicalDevice();   // Create logical device with required queues
        createCommandPool();     // Create command pool for command buffer allocation
    }

    /**
    * Destructor: Cleanup Vulkan resources in reverse dependency order
    * Must destroy in correct order to avoid validation layer errors
    */
    LveDevice::~LveDevice() {
        vkDestroyCommandPool(device_, commandPool, nullptr);  // Destroy command pool first

        vkDestroyDevice(device_, nullptr);  // Destroy logical device

        // Only destroy debug messenger if validation layers were enabled
        if (enableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        }

        vkDestroySurfaceKHR(instance, surface_, nullptr);  // Destroy window surface
        vkDestroyInstance(instance, nullptr);              // Destroy instance last
    }

    /**
    * Creates Vulkan instance with required extensions and validation layers
    * Instance is the connection between application and Vulkan library
    */
    void LveDevice::createInstance() {
        // Check if validation layers are available when requested
        if (enableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        // Application info - used by drivers for optimization
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "LittleVulkanEngine App";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;  // Vulkan API version to use

        // Instance creation info
        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        // Get required extensions (GLFW + debug if enabled)
        auto extensions = getRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // Setup validation layers and debug messenger for debug builds
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();

            // Setup debug messenger info for instance creation debugging
            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
        } else {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        // Create the Vulkan instance
        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance!");
        }

        // Verify all required GLFW extensions are available
        hasGflwRequiredInstanceExtensions();
    }

    /**
    * Selects a suitable physical device (GPU) from available devices
    * Checks each device against requirements (queue families, extensions, etc.)
    */
    void LveDevice::pickPhysicalDevice() {
        // Get count of available physical devices
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }
        std::cout << "Device count: " << deviceCount << std::endl;

        // Get all available physical devices
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        // Find the first suitable device
        for (const auto &device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice = device;
                break;
            }
        }

        // Ensure we found a suitable device
        if (physicalDevice == VK_NULL_HANDLE) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        // Get and display selected device properties
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        std::cout << "physical device: " << properties.deviceName << std::endl;
    }

    /**
    * Creates logical device with required queue families and extensions
    * Logical device is the interface to the physical device
    */
    void LveDevice::createLogicalDevice() {
        // Get queue family indices for graphics and presentation
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

        // Create queue creation info for unique queue families
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

        float queuePriority = 1.0f;  // Queue priority (0.0 to 1.0)
        for (uint32_t queueFamily : uniqueQueueFamilies) {
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;  // Number of queues to create
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        // Specify device features to enable
        VkPhysicalDeviceFeatures deviceFeatures = {};
        deviceFeatures.samplerAnisotropy = VK_TRUE;  // Enable anisotropic filtering

        // Device creation info
        VkDeviceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        createInfo.pEnabledFeatures = &deviceFeatures;

        // Enable required device extensions (swap chain)
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        // Enable validation layers for device (deprecated but maintained for compatibility)
        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        // Create the logical device
        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device_) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        // Get handles to the created queues
        vkGetDeviceQueue(device_, indices.graphicsFamily, 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, indices.presentFamily, 0, &presentQueue_);
    }

    /**
    * Creates command pool for allocating command buffers
    * Command buffers record drawing commands to be submitted to queues
    */
    void LveDevice::createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findPhysicalQueueFamilies();

        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
        // TRANSIENT_BIT: Command buffers are short-lived
        // RESET_COMMAND_BUFFER_BIT: Allow individual command buffer reset
        poolInfo.flags =
                VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    /**
    * Creates window surface through GLFW
    * Surface represents the interface between Vulkan and the window system
    */
    void LveDevice::createSurface() {
        window.createWindowSurface(instance, &surface_);
    }

    /**
    * Checks if a physical device meets all requirements
    * Requirements: queue families, device extensions, swap chain support, anisotropy
    */
    bool LveDevice::isDeviceSuitable(VkPhysicalDevice device) {
        // Check if device has required queue families
        QueueFamilyIndices indices = findQueueFamilies(device);

        // Check if device supports required extensions
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        // Check swap chain support (only if extensions are supported)
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            // Need at least one format and one present mode
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }

        // Check if device supports required features
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        // Device is suitable if all requirements are met
        return indices.isComplete() && extensionsSupported && swapChainAdequate &&
               supportedFeatures.samplerAnisotropy;
    }

    /**
    * Sets up debug messenger create info structure
    * Configures what types of messages to receive and callback function
    */
    void LveDevice::populateDebugMessengerCreateInfo(
            VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
        createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        // Receive warning and error messages (skip verbose info)
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        // Receive all types of messages
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;  // Our callback function
        createInfo.pUserData = nullptr;  // Optional user data passed to callback
    }

    /**
    * Sets up debug messenger for validation layer output
    * Only called when validation layers are enabled
    */
    void LveDevice::setupDebugMessenger() {
        if (!enableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo;
        populateDebugMessengerCreateInfo(createInfo);

        if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    /**
    * Checks if all requested validation layers are available
    * Returns false if any requested layer is missing
    */
    bool LveDevice::checkValidationLayerSupport() {
        // Get count of available layers
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        // Get all available layer properties
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        // Check if each requested layer is available
        for (const char *layerName : validationLayers) {
            bool layerFound = false;

            for (const auto &layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                }
            }

            if (!layerFound) {
                return false;
            }
        }

        return true;
    }

    /**
    * Gets required extensions for Vulkan instance
    * Always includes GLFW extensions, adds debug extension if validation enabled
    */
    std::vector<const char *> LveDevice::getRequiredExtensions() {
        // Get GLFW required extensions
        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        // Create vector from GLFW extensions
        std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        // Add debug extension if validation layers are enabled
        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    /**
    * Verifies that all required GLFW extensions are available
    * Prints available and required extensions for debugging
    */
    void LveDevice::hasGflwRequiredInstanceExtensions() {
        // Get all available instance extensions
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> extensions(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

        // Create set of available extension names for fast lookup
        std::cout << "available extensions:" << std::endl;
        std::unordered_set<std::string> available;
        for (const auto &extension : extensions) {
            std::cout << "\t" << extension.extensionName << std::endl;
            available.insert(extension.extensionName);
        }

        // Check that all required extensions are available
        std::cout << "required extensions:" << std::endl;
        auto requiredExtensions = getRequiredExtensions();
        for (const auto &required : requiredExtensions) {
            std::cout << "\t" << required << std::endl;
            if (available.find(required) == available.end()) {
                throw std::runtime_error("Missing required glfw extension");
            }
        }
    }

    /**
    * Checks if physical device supports all required device extensions
    * Currently only checks for swap chain extension
    */
    bool LveDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {
        // Get available device extensions
        uint32_t extensionCount;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(
                device,
                nullptr,
                &extensionCount,
                availableExtensions.data());

        // Create set of required extensions
        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        // Remove each available extension from required set
        for (const auto &extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        // All extensions supported if set is empty
        return requiredExtensions.empty();
    }

    /**
    * Finds queue families that support required operations
    * Need graphics operations and presentation to window surface
    */
    QueueFamilyIndices LveDevice::findQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        // Get queue family properties
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        // Find queue families with required capabilities
        int i = 0;
        for (const auto &queueFamily : queueFamilies) {
            // Check for graphics support
            if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                indices.graphicsFamily = i;
                indices.graphicsFamilyHasValue = true;
            }

            // Check for presentation support
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupport);
            if (queueFamily.queueCount > 0 && presentSupport) {
                indices.presentFamily = i;
                indices.presentFamilyHasValue = true;
            }

            // Early exit if both found
            if (indices.isComplete()) {
                break;
            }

            i++;
        }

        return indices;
    }

    /**
    * Queries swap chain support details for a physical device
    * Gets surface capabilities, supported formats, and present modes
    */
    SwapChainSupportDetails LveDevice::querySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;

        // Get surface capabilities (image count, extent, etc.)
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &details.capabilities);

        // Get supported surface formats
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);

        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, details.formats.data());
        }

        // Get supported present modes
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);

        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                    device,
                    surface_,
                    &presentModeCount,
                    details.presentModes.data());
        }
        return details;
    }

    /**
    * Finds supported format from candidates list
    * Checks if format supports required tiling and features
    */
    VkFormat LveDevice::findSupportedFormat(
            const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        // Check each candidate format
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

            // Check linear tiling support
            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                return format;
            }
                // Check optimal tiling support
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }
        throw std::runtime_error("failed to find supported format!");
    }

    /**
    * Finds suitable memory type for buffer/image allocation
    * Memory type must match filter and have required properties
    */
    uint32_t LveDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        // Get memory properties of physical device
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        // Check each memory type
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            // Check if type is allowed by filter and has required properties
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                return i;
            }
  }
  throw std::runtime_error("failed to find suitable memory type!");
}
    //BUFFER AND IMAGE HELPER FUNCTIONS
    /**
    * Creates buffer with specified size, usage, and memory properties
    * Allocates device memory and binds it to the buffer
    */
    void LveDevice::createBuffer(
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties,
            VkBuffer &buffer,
            VkDeviceMemory &bufferMemory) {
        // Setup buffer creation info
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;                                 // Size in bytes
        bufferInfo.usage = usage;                               // How buffer will be used (vertex, index, uniform, etc.)
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;     // Buffer owned by single queue family

        // Create the buffer object
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vertex buffer!");
        }

        // Get memory requirements for the buffer
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device_, buffer, &memRequirements);

        // Setup memory allocation info
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;       // Required memory size
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);  // Find suitable memory type

        // Allocate device memory
        if (vkAllocateMemory(device_, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }

        // Bind allocated memory to buffer (offset 0)
        vkBindBufferMemory(device_, buffer, bufferMemory, 0);
    }

    /**
    * Begins single-time command buffer for one-off operations
    * Used for operations like buffer copying that need immediate execution
    */
    VkCommandBuffer LveDevice::beginSingleTimeCommands() {
        // Setup command buffer allocation info
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;      // Primary command buffer (can be submitted to queue)
        allocInfo.commandPool = commandPool;                    // Pool to allocate from
        allocInfo.commandBufferCount = 1;                       // Allocate single command buffer

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device_, &allocInfo, &commandBuffer);

        // Begin recording commands
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;  // Used once then reset

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    /**
    * Ends single-time command buffer and submits to graphics queue
    * Waits for completion then frees the command buffer
    */
    void LveDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
        // Stop recording commands
        vkEndCommandBuffer(commandBuffer);

        // Setup submission info
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        // Submit to graphics queue and wait for completion
        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(graphicsQueue_);  // Block until queue operations complete

        // Free the temporary command buffer
        vkFreeCommandBuffers(device_, commandPool, 1, &commandBuffer);
    }

    /**
    * Copies data from source buffer to destination buffer
    * Uses single-time command buffer for the copy operation
    */
    void LveDevice::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        // Setup copy region
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;  // Start at beginning of source buffer
        copyRegion.dstOffset = 0;  // Start at beginning of destination buffer
        copyRegion.size = size;    // Number of bytes to copy

        // Record copy command
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    /**
    * Copies buffer data to image (used for texture loading)
    * Assumes image is in VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL layout
    */
    void LveDevice::copyBufferToImage(
            VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        // Setup buffer-to-image copy region
        VkBufferImageCopy region{};
        region.bufferOffset = 0;       // Start at beginning of buffer
        region.bufferRowLength = 0;    // Tightly packed (no padding)
        region.bufferImageHeight = 0;  // Tightly packed (no padding)

        // Specify which part of image to copy to
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;  // Color data
        region.imageSubresource.mipLevel = 0;                            // Base mip level
        region.imageSubresource.baseArrayLayer = 0;                     // First array layer
        region.imageSubresource.layerCount = layerCount;                // Number of layers

        region.imageOffset = {0, 0, 0};           // Start at image origin
        region.imageExtent = {width, height, 1};  // Copy entire image extent

        // Record copy command (assumes image is in correct layout)
        vkCmdCopyBufferToImage(
                commandBuffer,
                buffer,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,  // Image layout during transfer
                1,
                &region);
        endSingleTimeCommands(commandBuffer);
    }

    /**
    * Creates image with specified properties and allocates memory
    * Similar to createBuffer but for images (textures, render targets, etc.)
    */
    void LveDevice::createImageWithInfo(
            const VkImageCreateInfo &imageInfo,
            VkMemoryPropertyFlags properties,
            VkImage &image,
            VkDeviceMemory &imageMemory) {
        // Create the image object
        if (vkCreateImage(device_, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        // Get memory requirements for the image
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device_, image, &memRequirements);

        // Setup memory allocation info
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;       // Required memory size
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);  // Find suitable memory type

        // Allocate device memory for image
        if (vkAllocateMemory(device_, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        // Bind allocated memory to image
        if (vkBindImageMemory(device_, image, imageMemory, 0) != VK_SUCCESS) {
            throw std::runtime_error("failed to bind image memory!");
        }
}

}  // namespace lve