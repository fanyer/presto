int f(int x);
int f(vec2 x);
void main()
{
  // OK, matches declaration above.
  f(2);
  // Not OK, type mismatch.
  f(2.2);

  // OK, different overload.
  vec2 y1;
  f(y1);

  // Not OK
  vec2 y2;
  f(1 + f(2), y1);

  // Not OK;
  vec3 y3;
  f(y3);
}
int f(int y)
{
    return (2*(y + y));
}
