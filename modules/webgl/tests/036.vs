// Check that missing operands to the ternary operator generate syntax errors

attribute vec4 vPosition;
void main() {
    int a;
    a = true ? 1 : 0 ;
    a =      ? 1 : 0 ;
    a = true ?   : 0 ;
    a = true ? 1 :   ;
    a =      ?   :   ;

    gl_Position = vPosition;
}
