// dump_output
attribute vec4 aPosition;
varying vec4 vColor;

#define sign_emu(value) ((value) == 0.0 ? 0.0 : ((value) > 0.0 ? 1.0 : -1.0))

void main()
{
   gl_Position = aPosition;
   vec2 texcoord = vec2(aPosition.xy * 0.5 + vec2(0.5, 0.5));
   vec4 color = vec4(
       texcoord,
       texcoord.x * texcoord.y,
       (1.0 - texcoord.x) * texcoord.y * 0.5 + 0.5);
   vColor = vec4(
    sign_emu(color.x * 2.0 - 1.0) * 0.5 + 0.5,
    sign_emu(color.y * 2.0 - 1.0) * 0.5 + 0.5,
    0,
    1);
}
