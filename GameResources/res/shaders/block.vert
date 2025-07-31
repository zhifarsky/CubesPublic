#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in int instancePackedOffset;
layout (location = 2) in int instanceFaceDirection;
layout (location = 3) in int instanceTextureID;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 lightSpaceMatrix;

uniform ivec2 atlasSize;
uniform ivec2 chunkPos;

out vec3 ourColor;
out vec3 ourNormal;
out vec2 ourUV;
out vec3 FragPos;
out vec4 FragPosLightSpace;

const vec2 uvs[4] = vec2[4](
    vec2(0.0,  0.0),
    vec2(0.0,  1.0),
    vec2(1.0,  1.0),
    vec2(1.0,  0.0)
);

void main() {
    int CHUNK_SX = 16;
    int CHUNK_SZ = 16;
    int CHUNK_SY = 24;
    vec3 pos = aPos;
    
    int texSize = 16;

    // y+
    if (instanceFaceDirection == 0) {
        pos.xz = pos.zx; // переворачиваем полигон (для верной работы FACE_CULL)
        pos.y++;
        ourNormal = vec3(0,1,0);
    }
    // x+
    else if (instanceFaceDirection == 2) {
        ourNormal = vec3(-1,0,0);
        pos.xz = pos.zx;
        pos.xy = pos.yx;
    }
    // x-
    else if (instanceFaceDirection == 3) {
        ourNormal = vec3(1,0,0);
        pos.xy = pos.yx;
        pos.x++;
    }
    // z+
    else if (instanceFaceDirection == 4) {
        ourNormal = vec3(0,0,-1);
        pos.xz = pos.zx;
        pos.zy = pos.yz;
    }
    // z-
    else if (instanceFaceDirection == 5) {
        ourNormal = vec3(0,0,1);
        pos.zy = pos.yz;
        pos.z++;
    }
    // y-
    else {
        ourNormal = vec3(0,-1,0);        
    }
    
    // распаковываем offset
    vec3 offset = vec3(
	    instancePackedOffset % CHUNK_SX,
	    instancePackedOffset / (CHUNK_SX * CHUNK_SZ),
	    instancePackedOffset / CHUNK_SX % CHUNK_SZ
    );

    // todo: вычислить uv из instanceTextureID и atlasSize
    //float u = uvs[gl_VertexID].x / float(atlasSize.x) + float((instanceTextureID * texSize) % atlasSize.x) / float(atlasSize.x);
    //float v = uvs[gl_VertexID].y / float(atlasSize.x) + float(instanceTextureID * texSize / atlasSize.x * texSize) / float(atlasSize.y);
    float uvSize = (float(atlasSize) / float(texSize));
    float u = uvs[gl_VertexID].x / uvSize + (1.0 / uvSize) * float(instanceTextureID); // TODO: доделать (сейчас максимум один ряд текстур)
    float v = uvs[gl_VertexID].y / uvSize;
    ourUV = vec2(u,v);

    vec3 vertexPos = pos + offset + vec3(chunkPos.x, 0, chunkPos.y);
    FragPos = vec3(model * vec4(vertexPos, 1.0));

    FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

	gl_Position = projection * view * model * vec4(vertexPos, 1.0);
}