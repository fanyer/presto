// Static recursion not allowed.
int f(int x);
int h(int x)
{
    int y = x + 1;
    return f(y);
}
int g(int x)
{
    int y = x + 1;
    return h(y);
}
int f(int x)
{
    int y = x + 1;
    return g(y);
}

void
main()
{
  f(0);
}
