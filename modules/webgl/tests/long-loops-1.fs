#ifdef GL_ES
precision highp float;
#endif

uniform float x,y,z;
varying vec2 texCoord0;
vec4 iter_z(float cr, float ci) {
     int i;
     float nzr, nzi, zr = 0.0, zi = 0.0;
     vec4 color = vec4(0.0);
     for (i=0; i<2500; i++) {
         nzr = zr * zr - zi * zi + cr;
         nzi = 2.0 * zr * zi + ci;
         zr = nzr;
         zi = nzi;
     }
     color = vec4(zi);
     color.a = 1.0;
     return color;
}

void main()
{
    gl_FragColor = iter_z(x+z*(2.0*texCoord0.s-1.5), y+z*(2.0*texCoord0.t-1.0));
}
