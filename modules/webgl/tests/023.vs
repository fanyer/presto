attribute vec4 vPosition;
uniform mat4 world4;
uniform mat3 world3;
uniform mat2 world2;
void main()
{
  gl_Position = vec4(vPosition.xyz, world3[0].x + world2[0].x) * world4;
}
