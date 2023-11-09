#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int i;
layout (location = 3) in int n;
layout (location = 4) in int hide;
layout (location = 5) in vec3 aNor;
layout (location = 6) in int alight;
layout (location = 7) in vec3 plight1;
layout (location = 8) in vec3 plight2;
layout (location = 9) in vec3 acorner_light1;
layout (location = 10) in vec3 acorner_light2;

out vec2 TexCoord;
out vec2 LightCoord;
out vec2 nu;
out vec3 Normal;
out vec3 FragPos;
out vec4 FragPosLightSpace;
out float lightFrag;
out vec4 side_lights;
out vec4 corner_lights;


uniform mat4 proj;
uniform mat4 view;
uniform mat4 model;
uniform vec3 cPos;
uniform mat4 lightSpaceMatrix;




void main()
{
    int b = int(gl_VertexID / 6);
    int g = (hide >> b) & 1;
    int side_light = 0;
    int corner_light = 0;
    if (b<3){
        side_light = int(plight1[b]);
        corner_light = int(acorner_light1[b]);
    }
    else{
        side_light = int(plight2[b-3]);
        corner_light = int(acorner_light2[b-3]);
    }
    side_lights = vec4(side_light & 15,(side_light >> 4) & 15,(side_light >> 8) & 15,(side_light >> 12) & 15);
    corner_lights = vec4(corner_light & 15,(corner_light >> 4) & 15,(corner_light >> 8) & 15,(corner_light >> 12) & 15);

    LightCoord = aTexCoord;

    int lcg = (alight >> b*4) & 15;
    if (g == 0){
        vec3 OPos = vec3(i%16,int(i/256),int(i%256/16));

        vec3 Pos = cPos+aPos+OPos;
        gl_Position = proj * view * model * vec4(Pos, 1.0);
        TexCoord = vec2(aTexCoord.x/8+float(b)/8,aTexCoord.y/4+1-float(n)/4);
        FragPos = vec3(model * vec4(Pos, 1.0));

        FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

    }
    else{
        gl_Position = vec4(1.0);
        TexCoord = vec2(0,0);
        FragPos = vec3(model * vec4(aPos, 1.0));

    }
    lightFrag = lcg;
    nu = vec2(n, g);


    Normal = transpose(inverse(mat3(model))) * aNor; 
}