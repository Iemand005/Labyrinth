#version 450

layout (location = 0) out vec4 FragColor;

layout (location = 2) in vec2 TexCoord;
layout (location = 3) in vec4 ObjectColor;

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 projection;
};

layout(binding = 1) uniform sampler2D ourTexture;

void main()
{
    FragColor = ObjectColor;
}
