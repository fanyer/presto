// dump_output
/* Check if #line without a second source-position number is handled properly. */
#line 15
#define sample_count 16
uniform vec3 samples[sample_count];
/* Check if #line with a second source-position number is handled properly. */
#line 15 3343
uniform vec3 samples2[__FILE__];

void main() {;}
