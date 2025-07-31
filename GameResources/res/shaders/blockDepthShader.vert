#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in int instancePackedOffset;
layout (location = 2) in int instanceFaceDirection;
layout (location = 3) in int instanceTextureID;

uniform mat4 model;
uniform mat4 lightSpaceMatrix;

uniform ivec2 chunkPos;

void main() {
    int CHUNK_SX = 16;
    int CHUNK_SZ = 16;
    int CHUNK_SY = 24;
    vec3 pos = aPos;
    
    // y+
    if (instanceFaceDirection == 0) {
        pos.xz = pos.zx; // переворачиваем полигон (для верной работы FACE_CULL)
        pos.y++;
    }
    // x+
    else if (instanceFaceDirection == 2) {
        pos.xz = pos.zx;
        pos.xy = pos.yx;
    }
    // x-
    else if (instanceFaceDirection == 3) {
        pos.xy = pos.yx;
        pos.x++;
    }
    // z+
    else if (instanceFaceDirection == 4) {
        pos.xz = pos.zx;
        pos.zy = pos.yz;
    }
    // z-
    else if (instanceFaceDirection == 5) {
        pos.zy = pos.yz;
        pos.z++;
    }
    // y-
    else {
    }
    
    // распаковываем offset
    vec3 offset = vec3(
	    instancePackedOffset % CHUNK_SX,
	    instancePackedOffset / (CHUNK_SX * CHUNK_SZ),
	    instancePackedOffset / CHUNK_SX % CHUNK_SZ
    );

    vec3 vertexPos = pos + offset + vec3(chunkPos.x, 0, chunkPos.y);

	gl_Position = lightSpaceMatrix * model * vec4(vertexPos, 1.0);
}