// Sadly I can not force the current path so this could fail beacuse include.vs
// does not exist, not because #include is disallowed.
#include "include.vs"
attribute vec4 vPosition;
void main()
{
    gl_Position = vPosition;
}
