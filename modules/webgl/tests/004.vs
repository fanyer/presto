// test of the 'minimal' CPP support currently provided..
#ifdef GL_ES
  # ifndef GL_ES
  attribute vec4 position1;
  #  else // else
  attribute vec4 position2;
  #  endif
attribute vec4 position3;
#else
attribute vec4 position4;
#endif  // aa

attribute vec2 texCoord;
attribute vec3 normal;
attribute int  texCoord1;
int minu()
{
    attribute vec4 position;
    return 2;
}
