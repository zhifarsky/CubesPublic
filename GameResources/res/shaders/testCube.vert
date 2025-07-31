#version 330 core

//layout (location = 0) in vec3 aPos;
layout (location = 0) in int aPos;
layout (location = 1) in int faceMask;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float size;

out vec3 ourNormal;
out vec3 FragPos;

const vec3 vertices[8] = vec3[8](
    vec3(-1, -1,  1),
    vec3( 1, -1,  1),
    vec3( 1,  1,  1),
    vec3(-1,  1,  1),
    vec3(-1, -1, -1),
    vec3( 1, -1, -1),
    vec3( 1,  1, -1),
    vec3(-1,  1, -1)
);
    
const int indices[36] = int[36](
    // Передняя грань
    0, 1, 2, 2, 3, 0,
    // Правая грань
    1, 5, 6, 6, 2, 1,
    // Задняя грань
    7, 6, 5, 5, 4, 7,
    // Левая грань
    4, 0, 3, 3, 7, 4,
    // Нижняя грань
    4, 5, 1, 1, 0, 4,
    // Верхняя грань
    3, 2, 6, 6, 7, 3
);

const vec3 normals[6] = vec3[6](
    vec3(0,  0,  1),
    vec3(1,  0,  0),
    vec3(0,  0, -1),
    vec3(-1, 0,  0),
    vec3(0, -1,  0),
    vec3(0,  1,  0)
);

void main() {
    int CHUNK_SX = 16;
    int CHUNK_SZ = 16;
    int CHUNK_SY = 24;

    int x = aPos % CHUNK_SX;
//    int z = aPos / CHUNK_SX;
//    int y = aPos / (CHUNK_SX * CHUNK_SZ);
    int z = (aPos / CHUNK_SX) % CHUNK_SZ;
    int y = aPos / (CHUNK_SX * CHUNK_SZ);
    vec3 posUnfolded = vec3(x, y, z);
    
    int vertexIndex = gl_VertexID % 36;
    int faceID = vertexIndex / 6;

    int idx = indices[vertexIndex];
    vec3 vertexPos = posUnfolded + vertices[idx] * size;
    ourNormal = normals[faceID];
    FragPos = vec3(model * vec4(vertexPos, 1.0));

    // Проверяем, нужно ли отрисовывать эту грань
    bool shouldDraw = (faceMask & (1 << faceID)) != 0;
    // Если грань не должна отрисовываться, помещаем вершину за камерой
    //if (!shouldDraw) {
        //gl_Position = vec4(0.0, 0.0, -100.0, 1.0); // Помещаем за камеру
        //return;
    //}

	gl_Position = projection * view * model * vec4(vertexPos, 1.0);
}