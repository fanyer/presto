
precision highp float;

uniform float t;
varying vec2 coords;

const float maxdist=1.0;
const float maxlevel=6.0;
const float epsilon=0.000001;

float f(float currlevel)
{
    float x = 1.0;
    for(float level; level<maxlevel; level += 1.0)
	x *= 3.0;

    return x;
}

void main()
{
    f(3.0);
}
