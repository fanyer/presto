struct S { float a; int b; };

float f(S s) { return s.a; }

void
main()
{
    float f = f(S(2.3, 1));
}
