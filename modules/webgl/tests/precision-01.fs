precision highp float;
float x = 2.0;
float f(float a)
{
  precision mediump float;
  float y = 2.0 + a;
}

int main()
{
    f(2.0);
}
