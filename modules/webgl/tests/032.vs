#if UNDEFINED_FOO
  // according to ES GLSL spec 3.4 undefined symbols should fail.
#else
  precision mediump float;
  void main()
  {
      gl_FragColor = vec4(0.0,0.0,0.0,1.0);
  }
#endif
