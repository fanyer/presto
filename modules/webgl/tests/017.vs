int func(inout int x)
{
    x = 2;
    return x;
}

float f(ivec4 vec)
{
  for (int i = 0; i < 4; i++)
  {
      func(i);
      int val = vec[i];
      if (val == i)
         return float(i);
   }

  return 2.0;
}

float g(int y, ivec4 vec)
{
  for (int i = 0; i < 4; i -= -1)
      if (vec[i] == i)
         return float(i);

  return 2.0;
}

