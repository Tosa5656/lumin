#include "shaders.h"

std::vector<char> ShadersManager::ReadShader(const std::string& shaderPath)
{
    std::ifstream shaderFile(shaderPath, std::ios::ate | std::ios::binary);

    if(!shaderFile.is_open())
        std::cout << "Failed to open shader " << shaderPath << std::endl;
    
    size_t shaderFileSize = (size_t)shaderFile.tellg();
    std::vector<char> buffer(shaderFileSize);
    shaderFile.seekg(0);
    shaderFile.read(buffer.data(), shaderFileSize);

    shaderFile.close();

    return buffer;
}

VkShaderModule ShadersManager::CreateShaderModule(const std::vector<char>& shaderCode)
{
    VkShaderModuleCreateInfo moduleCreateInfo{};
    moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleCreateInfo.codeSize = shaderCode.size();
    moduleCreateInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(*m_device, &moduleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
        std::cout << "Failed to create shader module!" << std::endl;

    return shaderModule;
}