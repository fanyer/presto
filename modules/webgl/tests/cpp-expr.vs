// dump_output
#define A 2
#if A == /* Hey look at me, I'm a
multi-line comment */ 2
int pass;
#else
int fail;
#endif
void main()
{
  return;
}
