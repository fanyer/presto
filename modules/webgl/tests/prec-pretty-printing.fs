// dump_output
precision mediump float;

/* Check if minimal (but no less) number of parens are emitted. */
void main()
{
    float x = 10.0 + ((2. * 50.0) / (1.5*1.5)) + 2. - (2.0 - (2. - 2.0));
}
