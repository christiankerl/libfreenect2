#version 330

uniform sampler2D Tex;
uniform float Scale;

in VertexData {
    vec2 TexCoord;
} FragmentIn;

layout(location = 0) out vec4 Color;

void main(void)
{
  Color = vec4(vec3(texture(Tex, FragmentIn.TexCoord).x) * Scale, 1.0);
}