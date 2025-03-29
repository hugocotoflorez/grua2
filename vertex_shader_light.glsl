#version 330 core

layout(location = 0) in vec3 aPos;      // Posición del vértice
layout(location = 1) in vec2 aTexCoord; // Coordenadas de textura
layout(location = 2) in vec3 aNormal;   // Normal del vértice

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 lightDir; // En espacio del objeto

out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;
out vec4 lightDirection;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    TexCoord = aTexCoord;

    // Transformamos la dirección de la luz con model para que siga al objeto
    lightDirection = normalize(model * vec4(lightDir, 0)); // <- CAMBIO IMPORTANTE
}

