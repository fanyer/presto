// Test #if and #ifdef with undefined variables
#if 1 && SOMETHING_RANDOM
int x;
#else
int y;
#endif // SOMETHING_RANDOM
