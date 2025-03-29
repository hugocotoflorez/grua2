#version 330 core

in vec3 FragPos;
out vec4 FragColor;
in vec2 TexCoord;
in vec4 lightDirection;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 Ka;
uniform vec3 Kd;
uniform vec3 color;
uniform sampler2D texture1;
uniform float cosineCutoff;

void main()
{
    vec3 dx = dFdx(FragPos);
    vec3 dy = dFdy(FragPos);
    vec3 normal = normalize(cross(dx, dy));

    vec3 lightDir = normalize(vec3(lightDirection)); // Direccion normalizada
    vec3 fragToLight = normalize(lightPos - FragPos);

    // Corregir la comparación de dirección
    float theta = dot(-lightDir, fragToLight); // Invertido si la luz sale en la dirección contraria

    float spotlightEffect = smoothstep(cosineCutoff - 0.05, cosineCutoff, theta); // Spotlight

    float diff = max(dot(normal, fragToLight), 0.0) * spotlightEffect;

    vec3 diffuse = diff * Kd * lightColor;
    vec3 ambient = Ka * lightColor;

    FragColor = vec4(diffuse + ambient, 1.0)  * vec4(color, 1) * texture(texture1, TexCoord);
}

