#include <iostream>
#include <fstream>
#include <vector>

#include <vulkan/vulkan.h>

class ShadersManager
{
public:
    ShadersManager() {};
    ShadersManager(VkDevice* device) { m_device = device; };
    ~ShadersManager() {};

    std::vector<char> ReadShader(const std::string& shaderPath);
    VkShaderModule CreateShaderModule(const std::vector<char>& shaderCode);
private:
    VkDevice* m_device;
};