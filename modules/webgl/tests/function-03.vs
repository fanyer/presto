int f(int y)
{
    return f(y+1);
}

void
main()
{
    f(2);
}
