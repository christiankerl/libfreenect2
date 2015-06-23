#version 330

uniform usampler2D Tex;
uniform float Scale;

in VertexData {
    vec2 TexCoord;
} FragmentIn;

layout(location = 0) out vec4 Color;

void main(void)
{
  vec3 rgb = texture(Tex, FragmentIn.TexCoord).xyz;
  Color = vec4(rgb / 255.0, 1.0);
}