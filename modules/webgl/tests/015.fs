#define bar __LINE__
#undef foo
#define foo(x) x
#if 1 && ( (1 + foo(bar) > 2) && defined(GL_ES))
precision highp float;

#endif // foo

float f()
{
    float a = 1.0 * (2.) + foo(3. + 2.1);
    int x = 3 + foo(__LINE__);
}

void main()
{
    f();
}
