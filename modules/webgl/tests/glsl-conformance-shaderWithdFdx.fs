#extension GL_OES_standard_derivatives:enable
precision mediump float;
void main()
{
    gl_FragColor = vec4(dFdx(0.5),0.0,0.0,1.0);
}

