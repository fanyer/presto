#ifdef GL_ES
precision highp float;
#endif
vec2 poissonDisk[16];
uniform sampler2D basemap;
uniform float u_mapSize;
uniform float u_sampRadius;
uniform int u_numSamples;
varying vec2 vTexcoord;
float radialBlur(sampler2D map, vec2 coord);
void main()
{
   gl_FragColor =  vec4(0.0, 0.0, 0.0, radialBlur(basemap, vTexcoord));
}
float radialBlur(sampler2D map, vec2 coord)
{
float texmapscale = 1.0/u_mapSize;
poissonDisk[0] = vec2( -0.94201624, -0.39906216 );
poissonDisk[1] = vec2( 0.94558609, -0.76890725 );
poissonDisk[2] = vec2( -0.094184101, -0.92938870);
poissonDisk[3] = vec2( 0.34495938, 0.29387760 );
poissonDisk[4] = vec2( -0.91588581, 0.45771432 );
poissonDisk[5] = vec2( -0.81544232, -0.87912464 );
poissonDisk[6] = vec2( -0.38277543, 0.27676845 );
poissonDisk[7] = vec2( 0.97484398, 0.75648379 );
poissonDisk[8] = vec2( 0.44323325, -0.97511554 );
poissonDisk[9] = vec2( 0.53742981, -0.47373420 );
poissonDisk[10] = vec2( -0.26496911, -0.41893023 );
poissonDisk[11] = vec2( 0.79197514, 0.19090188 );
poissonDisk[12] = vec2( -0.24188840, 0.99706507 );
poissonDisk[13] = vec2( -0.81409955, 0.91437590 );
poissonDisk[14] = vec2( 0.19984126, 0.78641367 );
poissonDisk[15] = vec2( 0.14383161, -0.14100790 );
float sum = 0.0;
float a = 0.0;
for(int i = 0; i < u_numSamples ;i++)
{
vec2 offset = poissonDisk[i]*u_sampRadius;
a += texture2D(map, coord.xy + offset*texmapscale).w;
}
return a/float(u_numSamples);
}
