// Test #ifdef handling of unbound names, but in unevaluated contexts.
#if 1 || (1 && defined(SOMETHING_RANDOM))
int x;
void main() { return;  }
#else
int y;
#endif // SOMETHING_RANDOM
