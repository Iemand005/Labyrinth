#version 450

layout (location = 0) out vec4 FragColor;

layout (location = 2) in vec2 TexCoord;

layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 projection;
};

layout (binding = 1) uniform sampler2D ourTexture;

void main()
{
    FragColor = vec4(0.0, 1.0, 0.0, 1.0);
}
