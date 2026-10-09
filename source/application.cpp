#include "application.hpp"

#include <imgui.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <cmath>
#include <fstream>
#include <iostream>

struct Vertex {
    glm::vec3 position; 
    glm::vec3 color;    
};

struct GlobalUniforms {
    alignas(16) glm::mat4 mvp;       
    alignas(16) glm::vec4 baseColor; 
};

const std::vector<Vertex> CUBE_VERTICES = {
    {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, 
    {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 1.0f}}, 
    {{ 0.5f,  0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}},
    {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 1.0f}}, 
    {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}}, 
    {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, 
    {{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 0.0f}}, 
    {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}  
};

const std::vector<uint32_t> CUBE_INDICES = {
    0, 1, 2,  2, 3, 0, 
    5, 4, 7,  7, 6, 5, 
    4, 0, 3,  3, 7, 4, 
    1, 5, 6,  6, 2, 1,
    3, 2, 6,  6, 7, 3, 
    4, 5, 1,  1, 0, 4  
};

struct ObjectTransform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotation{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};
    float color[3] = {1.0f, 1.0f, 1.0f};
};

struct AppState {
    bool isPerspective = true;
    bool isAnimating = false;
    float animSpeed = 0.5f;
    float animRadius = 2.0f;
    float animTime = 1.0f;

    ObjectTransform cube1{ glm::vec3(-1.2f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f), {1.0f, 0.8f, 0.3f} };
    ObjectTransform cube2{ glm::vec3( 1.2f, 0.0f, 0.0f), glm::vec3(0.0f, 45.0f, 0.0f), glm::vec3(0.8f), {0.2f, 0.8f, 1.0f} };
};

static AppState g_appState;

namespace application {

VkBuffer vertex_buffer = VK_NULL_HANDLE;
VmaAllocation vertex_buffer_allocation = VK_NULL_HANDLE;

VkBuffer index_buffer = VK_NULL_HANDLE;
VmaAllocation index_buffer_allocation = VK_NULL_HANDLE;

VkBuffer global_uniform_buffer_1 = VK_NULL_HANDLE;
VmaAllocation global_uniform_buffer_allocation_1 = VK_NULL_HANDLE;
GlobalUniforms* global_uniform_buffer_memory_1 = nullptr;

VkBuffer global_uniform_buffer_2 = VK_NULL_HANDLE;
VmaAllocation global_uniform_buffer_allocation_2 = VK_NULL_HANDLE;
GlobalUniforms* global_uniform_buffer_memory_2 = nullptr;

VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

VkDescriptorSet descriptor_set_1 = VK_NULL_HANDLE;
VkDescriptorSet descriptor_set_2 = VK_NULL_HANDLE;

VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
VkPipeline pipeline = VK_NULL_HANDLE;

VkShaderModule loadShaderModule(const char path[]) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return VK_NULL_HANDLE;
    
    const size_t size = file.tellg();
    std::vector<uint32_t> buffer(size / sizeof(uint32_t));
    
    file.seekg(0);
    file.read(reinterpret_cast<char*>(buffer.data()), size);
    file.close();
    
    VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = size,
        .pCode = buffer.data(),
    };
    
    VkShaderModule result;
    if (vkCreateShaderModule(graphics::internal::context.device, &info, nullptr, &result) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return result;
}

bool initialize() {
    auto& context = graphics::internal::context;

    const VkBufferCreateInfo vertex_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(Vertex) * CUBE_VERTICES.size(),
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    const VmaAllocationCreateInfo vertex_alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };
    Vertex* vertex_mapped_memory = nullptr;
    vmaCreateBuffer(context.allocator, &vertex_buffer_info, &vertex_alloc_info, 
                    &vertex_buffer, &vertex_buffer_allocation, nullptr);
    vmaMapMemory(context.allocator, vertex_buffer_allocation, (void**)&vertex_mapped_memory);
    memcpy(vertex_mapped_memory, CUBE_VERTICES.data(), vertex_buffer_info.size);
    vmaUnmapMemory(context.allocator, vertex_buffer_allocation);

    const VkBufferCreateInfo index_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sizeof(uint32_t) * CUBE_INDICES.size(),
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };
    uint32_t* index_mapped_memory = nullptr;
    vmaCreateBuffer(context.allocator, &index_buffer_info, &vertex_alloc_info, 
                    &index_buffer, &index_buffer_allocation, nullptr);
    vmaMapMemory(context.allocator, index_buffer_allocation, (void**)&index_mapped_memory);
    memcpy(index_mapped_memory, CUBE_INDICES.data(), index_buffer_info.size);
    vmaUnmapMemory(context.allocator, index_buffer_allocation);

    const VkBufferCreateInfo uniform_buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = (sizeof(GlobalUniforms) + 0xf) & ~0xf,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    vmaCreateBuffer(context.allocator, &uniform_buffer_info, &vertex_alloc_info, 
                    &global_uniform_buffer_1, &global_uniform_buffer_allocation_1, nullptr);
    vmaMapMemory(context.allocator, global_uniform_buffer_allocation_1, (void**)&global_uniform_buffer_memory_1);

    vmaCreateBuffer(context.allocator, &uniform_buffer_info, &vertex_alloc_info, 
                    &global_uniform_buffer_2, &global_uniform_buffer_allocation_2, nullptr);
    vmaMapMemory(context.allocator, global_uniform_buffer_allocation_2, (void**)&global_uniform_buffer_memory_2);

    const VkDescriptorSetLayoutBinding descriptor_set_bindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        }
    };
    const VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = descriptor_set_bindings,
    };
    vkCreateDescriptorSetLayout(context.device, &descriptor_layout_info, nullptr, &descriptor_set_layout);

    const VkDescriptorPoolSize pool_sizes[] = {
        { .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 2 }
    };
    const VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 2, 
        .poolSizeCount = 1,
        .pPoolSizes = pool_sizes,
    };
    vkCreateDescriptorPool(context.device, &pool_info, nullptr, &descriptor_pool);

    VkDescriptorSetLayout layouts[2] = { descriptor_set_layout, descriptor_set_layout };
    const VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 2,
        .pSetLayouts = layouts,
    };

    VkDescriptorSet descriptor_sets[2];
    vkAllocateDescriptorSets(context.device, &alloc_info, descriptor_sets);
    descriptor_set_1 = descriptor_sets[0];
    descriptor_set_2 = descriptor_sets[1];

    const VkDescriptorBufferInfo ubo_descriptor_info_1 = {
        .buffer = global_uniform_buffer_1,
        .offset = 0,
        .range = sizeof(GlobalUniforms),
    };
    const VkDescriptorBufferInfo ubo_descriptor_info_2 = {
        .buffer = global_uniform_buffer_2,
        .offset = 0,
        .range = sizeof(GlobalUniforms),
    };

    const VkWriteDescriptorSet descriptor_writes[] = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set_1,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &ubo_descriptor_info_1,
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set_2,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &ubo_descriptor_info_2,
        }
    };
    vkUpdateDescriptorSets(context.device, 2, descriptor_writes, 0, nullptr);

    // КОНВЕЙЕР
    const VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_set_layout,
    };
    vkCreatePipelineLayout(context.device, &pipeline_layout_info, nullptr, &pipeline_layout);

    VkShaderModule vert_shader = loadShaderModule("shaders/shader.vert.spv");
    VkShaderModule frag_shader = loadShaderModule("shaders/shader.frag.spv");
    
    const VkPipelineShaderStageCreateInfo shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vert_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = frag_shader,
            .pName = "main",
        }
    };

    const VkVertexInputBindingDescription vertex_bindings[] = {
        { .binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX }
    };
    const VkVertexInputAttributeDescription vertex_attributes[] = {
        { .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position) },
        { .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color) }
    };
    const VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = vertex_bindings,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = vertex_attributes,
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };
    const VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };
    const VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };
    const VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };
    const VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    };
    const VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };
    const VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };
    const VkDynamicState dynamic_states_array[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    const VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states_array,
    };

    const VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = pipeline_layout,
        .renderPass = context.render_pass,
        .subpass = 0,
    };

    vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline);

    vkDestroyShaderModule(context.device, frag_shader, nullptr);
    vkDestroyShaderModule(context.device, vert_shader, nullptr);

    return true;
}

void shutdown() {
    auto& context = graphics::internal::context;
    vkQueueWaitIdle(context.graphics_queue);

    vkDestroyPipeline(context.device, pipeline, nullptr);
    vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);
    vkDestroyDescriptorPool(context.device, descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(context.device, descriptor_set_layout, nullptr);

    vmaUnmapMemory(context.allocator, global_uniform_buffer_allocation_1);
    vmaDestroyBuffer(context.allocator, global_uniform_buffer_1, global_uniform_buffer_allocation_1);

    vmaUnmapMemory(context.allocator, global_uniform_buffer_allocation_2);
    vmaDestroyBuffer(context.allocator, global_uniform_buffer_2, global_uniform_buffer_allocation_2);

    vmaDestroyBuffer(context.allocator, index_buffer, index_buffer_allocation);
    vmaDestroyBuffer(context.allocator, vertex_buffer, vertex_buffer_allocation);
}

void update(double deltaTime) {
    ImGui::Begin("Lab 1");

    ImGui::Text("Projection Type (Extra 1):");
    ImGui::RadioButton("Perspective", (int*)&g_appState.isPerspective, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Orthographic", (int*)&g_appState.isPerspective, 0);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Cube 1 Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Pos 1 (XYZ)", &g_appState.cube1.position.x, 0.05f);
        ImGui::DragFloat3("Rot 1 (deg)", &g_appState.cube1.rotation.x, 1.0f);
        ImGui::DragFloat3("Scale 1", &g_appState.cube1.scale.x, 0.05f, 0.1f, 5.0f);
        ImGui::ColorEdit3("Color 1", g_appState.cube1.color);
    }

    if (ImGui::CollapsingHeader("Cube 2 Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Pos 2 (XYZ)", &g_appState.cube2.position.x, 0.05f);
        ImGui::DragFloat3("Rot 2 (deg)", &g_appState.cube2.rotation.x, 1.0f);
        ImGui::DragFloat3("Scale 2", &g_appState.cube2.scale.x, 0.05f, 0.1f, 5.0f);
        ImGui::ColorEdit3("Color 2", g_appState.cube2.color);
    }

    ImGui::Separator();
    ImGui::Text("Animation:");
    if (ImGui::Button(g_appState.isAnimating ? "Pause" : "Start Animation")) {
        g_appState.isAnimating = !g_appState.isAnimating;
    }
    ImGui::SliderFloat("Speed", &g_appState.animSpeed, 0.01f, 5.0f);
    ImGui::SliderFloat("Radius", &g_appState.animRadius, 0.5f, 5.0f);

    ImGui::End();

    static double lastTime = deltaTime; 
    
    double trueDeltaTime = deltaTime - lastTime;
    lastTime = deltaTime;

    if (g_appState.isAnimating) {
        g_appState.animTime += static_cast<float>(trueDeltaTime) * g_appState.animSpeed;
    }

    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 6.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    auto& context = graphics::internal::context;
    float width = static_cast<float>(context.swapchain_extent.width);
    float height = static_cast<float>(context.swapchain_extent.height);
    float aspect = (height > 0.0f) ? (width / height) : 1.0f;

    glm::mat4 proj;
    if (g_appState.isPerspective) {
        proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    } else {
        float orthoSize = 3.5f;
        proj = glm::ortho(-orthoSize * aspect, orthoSize * aspect, -orthoSize, orthoSize, 0.1f, 100.0f);
    }
    proj[1][1] *= -1.0f;

    glm::mat4 model1 = glm::mat4(1.0f);
    glm::vec3 pos1 = g_appState.cube1.position;
    if (g_appState.isAnimating) {
        pos1.x += g_appState.animRadius * std::cos(g_appState.animTime);
        pos1.z += g_appState.animRadius * std::sin(g_appState.animTime);
    }
    model1 = glm::translate(model1, pos1);
    model1 = glm::rotate(model1, glm::radians(g_appState.cube1.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    float extraRot1 = g_appState.isAnimating ? g_appState.animTime * 45.0f : 0.0f;
    model1 = glm::rotate(model1, glm::radians(g_appState.cube1.rotation.y + extraRot1), glm::vec3(0.0f, 1.0f, 0.0f));
    model1 = glm::rotate(model1, glm::radians(g_appState.cube1.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model1 = glm::scale(model1, g_appState.cube1.scale);

    GlobalUniforms ubo1{};
    ubo1.mvp = proj * view * model1;
    ubo1.baseColor = glm::vec4(g_appState.cube1.color[0], g_appState.cube1.color[1], g_appState.cube1.color[2], 1.0f);

    if (global_uniform_buffer_memory_1) {
        memcpy(global_uniform_buffer_memory_1, &ubo1, sizeof(ubo1));
    }

    glm::mat4 model2 = glm::mat4(1.0f);
    glm::vec3 pos2 = g_appState.cube2.position;
    if (g_appState.isAnimating) {
        pos2.x -= g_appState.animRadius * std::cos(g_appState.animTime);
        pos2.z -= g_appState.animRadius * std::sin(g_appState.animTime);
    }
    model2 = glm::translate(model2, pos2);
    model2 = glm::rotate(model2, glm::radians(g_appState.cube2.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    float extraRot2 = g_appState.isAnimating ? -g_appState.animTime * 60.0f : 0.0f;
    model2 = glm::rotate(model2, glm::radians(g_appState.cube2.rotation.y + extraRot2), glm::vec3(0.0f, 1.0f, 0.0f));
    model2 = glm::rotate(model2, glm::radians(g_appState.cube2.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model2 = glm::scale(model2, g_appState.cube2.scale);

    GlobalUniforms ubo2{};
    ubo2.mvp = proj * view * model2;
    ubo2.baseColor = glm::vec4(g_appState.cube2.color[0], g_appState.cube2.color[1], g_appState.cube2.color[2], 1.0f);

    if (global_uniform_buffer_memory_2) {
        memcpy(global_uniform_buffer_memory_2, &ubo2, sizeof(ubo2));
    }
}

void render(const graphics::internal::FrameData& fd) {
    if (pipeline == VK_NULL_HANDLE) return;

    auto& context = graphics::internal::context;

    vkResetCommandBuffer(fd.command_buffer, 0);
    const VkCommandBufferBeginInfo command_buffer_begin = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(fd.command_buffer, &command_buffer_begin);

    const VkClearValue clear_values[] = {
        { .color = { .float32 = { 0.1f, 0.1f, 0.1f, 1.0f } } },
        { .depthStencil = { 1.0f, 0 } },
    };
    const VkRenderPassBeginInfo render_pass_begin = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = context.render_pass,
        .framebuffer = fd.framebuffer,
        .renderArea = { .extent = context.swapchain_extent },
        .clearValueCount = sizeof(clear_values) / sizeof(clear_values[0]),
        .pClearValues = clear_values,
    };
    vkCmdBeginRenderPass(fd.command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

    const VkViewport viewport = {
        .x = 0, .y = 0,
        .width = static_cast<float>(context.swapchain_extent.width),
        .height = static_cast<float>(context.swapchain_extent.height),
        .minDepth = 0, .maxDepth = 1,
    };
    const VkRect2D scissor = { .extent = context.swapchain_extent };
    
    vkCmdSetViewport(fd.command_buffer, 0, 1, &viewport);
    vkCmdSetScissor(fd.command_buffer, 0, 1, &scissor);

    vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vertex_buffer, offsets);
    vkCmdBindIndexBuffer(fd.command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, 0, 1, &descriptor_set_1, 0, nullptr);
    vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(CUBE_INDICES.size()), 1, 0, 0, 0);

    vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, 0, 1, &descriptor_set_2, 0, nullptr);
    vkCmdDrawIndexed(fd.command_buffer, static_cast<uint32_t>(CUBE_INDICES.size()), 1, 0, 0, 0);

    vkCmdEndRenderPass(fd.command_buffer);
    vkEndCommandBuffer(fd.command_buffer);
}

} // namespace application
