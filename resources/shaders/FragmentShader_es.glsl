#version 300 es
precision mediump float;
out vec4 FragColor;

in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoord;

uniform sampler2D ourTexture;

struct PointLight {
    vec3 position;
    vec3 color;
    float intensity;
    float radius;
};

uniform int lightCount;
uniform PointLight pointLights[8];

void main()
{
    vec3 n = normalize(Normal);
    vec3 albedo = texture(ourTexture, TexCoord).rgb;

    vec3 lighting = vec3(0.1);
    for (int i = 0; i < 8; ++i) {
        if (i >= lightCount) break;
        vec3 L = pointLights[i].position - FragPos;
        float dist = length(L);
        if (dist < 0.0001) continue;
        vec3 ldir = L / dist;

        float diff = max(dot(n, ldir), 0.0);
        float radius = max(pointLights[i].radius, 0.001);
        float atten = 1.0 / (1.0 + (dist * dist) / (radius * radius));

        vec3 contrib = pointLights[i].color * pointLights[i].intensity * diff * atten;
        lighting += contrib;
    }

    FragColor = vec4(albedo * lighting, 1.0);
}
