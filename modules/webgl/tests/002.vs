struct t1
{
    int x;
    int y;
};

struct t2
{
    int x;
    int y;
};

struct t3
{
    int dup_x;
    int dup_x;
};

struct t4
{
    int dup_x1[2];
    float dup_x2[2];
};

highp int some_nonconstant;

struct t5
{
    int dup_x1[some_nonconstant];
    float dup_x2[2.1];
};
