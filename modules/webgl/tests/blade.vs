
uniform mat4 worldViewProjection;
uniform mat4 world;
uniform mat4 viewInverse;
uniform float time;
uniform float bladeWidth;
uniform float topWidth;
uniform float bladeSpacing;
uniform float heightRange;
uniform float horizontalRange;
uniform float xWorldMult;
uniform float zWorldMult;
uniform float xTimeMult;
uniform float zTimeMult;
uniform float rand1Mult;
uniform float rand2Mult;
uniform float swayRange;
uniform float elevationPeriod1;
uniform float elevationPeriod2;
uniform float elevationPointX;
uniform float elevationPointZ;
uniform float elevationRange;
attribute vec4 position;
attribute vec4 bladeId;
varying vec4 v_color;

vec4 brightColor = vec4(175.0 / 255.0, 218.0 / 255.0, 44.0 / 255.0, 1);
vec4 darkColor = vec4(53.0 / 255.0, 85.0 / 255.0, 7.0 / 255.0, 1);
vec4 darkerColor = vec4(53.0 / 255.0, 85.0 / 255.0, 7.0 / 255.0, 1) * 0.5;

float circleWave(vec4 center, vec4 position, float period) {
  vec4 diff = position - center;
  float dist = sqrt(diff.x * diff.x + diff.z * diff.z);
  return sin(dist * period);
}

void main() {
  float height = position.y;
  float lerp = height;

  mat4 vm = mat4(
      viewInverse[0],
      viewInverse[1],
      viewInverse[2],
     vec4(0,0,0,1));
  float width = mix(bladeWidth, topWidth, lerp);
  vec4 pt = vm * vec4(
      position.x * width,
      0,
      position.z * width,
      1) +
      vec4(0, position.y, 0, 0);

  float xbase = bladeId.x;
  float zbase = bladeId.y;
  float rand1 = bladeId.z;
  float rand2 = bladeId.w;
  float hbase = time * sin((xbase + zbase) * 0.1);
  float hsin  = sin(hbase);
  float hcos  = cos(hbase);
  float worldOff = world[3][0] * xWorldMult * world[3][2] * zWorldMult;
  float xoff  = sin(time * xTimeMult + rand1 + worldOff + xbase * 0.3 + sin(zbase)) * swayRange;
  float yoff  = 0.0;
  float zoff  = sin(time * zTimeMult + rand2 + worldOff + zbase * 0.1 + cos(xbase) * 1.3) * swayRange;
  float effect = 1.0 - cos(3.14159 * 0.5 * lerp) * 1.0;
  vec4 p = vec4(
      pt.x + xbase * bladeSpacing + xoff * effect + rand1 * bladeSpacing,
      pt.y                        + yoff          + rand1 * heightRange,
      pt.z + zbase * bladeSpacing + zoff * effect + rand1 * bladeSpacing,
      1);

  vec4 wp = world * p;

  rand1 = mod(rand1 + wp.x * rand1Mult + wp.z * rand2Mult, 1.0);
  rand2 = mod(rand2 + wp.z * rand1Mult + wp.x * rand2Mult, 1.0);
  float elevationBasis1 = circleWave(vec4(0,0,0,0), wp, elevationPeriod1);
  float elevationBasis2 = circleWave(vec4(elevationPointX, 0, elevationPointZ, 0), wp, elevationPeriod2);
  vec4 nextP = vec4(wp.x + bladeSpacing, wp.y, wp.z + bladeSpacing, 1);
  float elevationBasis3 = circleWave(vec4(0,0,0,0), nextP, elevationPeriod1);
  float elevationBasis4 = circleWave(vec4(elevationPointX, 0, elevationPointZ, 0), nextP, elevationPeriod2);
  float elevationBasis = elevationBasis2 - elevationBasis1;
  float elevationBasisNext = elevationBasis4 - elevationBasis3;

  vec4 clipPosition = worldViewProjection * vec4(
      p.x,
      p.y + elevationBasis * elevationRange,
      p.z,
      1);
  gl_Position = clipPosition;

  vec4 color = mix(darkColor, brightColor, lerp);
  vec4 randColor = vec4(rand2 * 0.2, rand2 * 0.2 ,rand2 * 0.2, 0);
  float l = dot(vec3(0.18814417367671946, 0.9407208683835973, 0.28221626051507914),
                normalize(vec3(0.06, elevationBasis * elevationRange - elevationBasisNext * elevationRange, 0.06)));

  // encode the depth mix amount into the alpha channel
  float depthMix = (clipPosition.z / clipPosition.w + 1.0) * 0.5;
  v_color = vec4((randColor + mix(color, darkerColor, l)).xyz, depthMix);
}

