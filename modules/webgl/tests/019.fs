#ifdef GL_ES
precision highp float;
#endif
uniform sampler2D srcTex;
uniform vec3 texel_ofs;
varying vec2 vTex;
vec3 rangeValHDR(vec3 src)
{
return (src.r>0.90||src.g>0.90||src.b>0.90)?(src):vec3(0.0,0.0,0.0);
}
vec4 hdrSample(float rad)
{
vec3 accum;
float radb = rad*0.707106781;
accum =  rangeValHDR(texture2D(srcTex, vec2(vTex.s+texel_ofs.x*rad,  vTex.t)).rgb);
accum += rangeValHDR(texture2D(srcTex, vec2(vTex.s,				 vTex.t+texel_ofs.y*rad)).rgb);
accum += rangeValHDR(texture2D(srcTex, vec2(vTex.s-texel_ofs.x*rad,  vTex.t)).rgb);
accum += rangeValHDR(texture2D(srcTex, vec2(vTex.s,				 vTex.t-texel_ofs.y*rad)).rgb);
accum += rangeValHDR(texture2D(srcTex, vec2(vTex.s+texel_ofs.x*radb, vTex.t+texel_ofs.y*radb)).rgb);
accum += rangeValHDR(texture2D(srcTex, vec2(vTex.s-texel_ofs.x*radb, vTex.t-texel_ofs.y*radb)).rgb);
accum += rangeValHDR(texture2D(srcTex, vec2(vTex.s+texel_ofs.x*radb, vTex.t-texel_ofs.y*radb)).rgb);
accum += rangeValHDR(texture2D(srcTex, vec2(vTex.s-texel_ofs.x*radb, vTex.t+texel_ofs.y*radb)).rgb);
accum /= 8.0;
return vec4(accum,1.0);
}
void main(void)
{
vec4 color;
color = hdrSample(2.0);
color += hdrSample(4.0);
color += hdrSample(6.0);
gl_FragColor = color/2.0;
}
