#if GL_ES == 1
  precision mediump float;
  void main()
  {
      gl_FragColor = vec4(0.0,0.0,0.0,1.0);
  }
#else
  foo
#endif
