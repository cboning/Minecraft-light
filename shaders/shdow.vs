#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in int i;
layout (location = 3) in int n;
layout (location = 4) in int hide;
layout (location = 5) in vec3 aNor;
layout (location = 6) in int light[6];
layout (location = 7) in int plight1[6];
layout (location = 7) in int plight2[6];


out vec2 TexCoord;


uniform mat4 lightSpaceMatrix;
uniform mat4 model;
uniform vec3 cPos;




void main()
{   
    int b = int(gl_VertexID / 6);
    int g = (hide >> b) & 1;
    if (g == 0){
        vec3 OPos = vec3(i%16,int(i/256),int(i%256/16));

        vec3 Pos = cPos+aPos+OPos;
        gl_Position = lightSpaceMatrix * model * vec4(Pos, 1.0);
        TexCoord = vec2(aTexCoord.x/8+float(b)/8,aTexCoord.y/4+1-float(n)/4);


    }


}