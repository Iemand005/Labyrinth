#version 330 core
out vec4 FragColor;
in vec3 Normal;
in vec3 ObjectSeed;

vec3 hash3(vec3 p)
{
    p = fract(p * vec3(443.8975, 397.2973, 491.1871));
    p += dot(p, p.yxz + 19.19);
    return fract((p.xxy + p.yxx) * p.zyx);
}

void main()
{
    vec3 n = normalize(Normal);
    vec3 lightDir = normalize(vec3(0.4, 0.6, 1.0));
    float diff = max(dot(n, lightDir), 0.0);
    vec3 lighting = vec3(0.25) + diff * 0.75;

    vec3 debugColor = hash3(ObjectSeed + vec3(7.0)); // distinct-ish per object
    FragColor = vec4(debugColor * lighting, 1.0);
}