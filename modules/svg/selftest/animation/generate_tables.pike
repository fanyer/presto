/*
 * This pike-program writes three files that are included by the
 * selftest files.
 *
 * The program will be generalized for more testcases. Now it just
 * generates data for animation.ot.
 *
 * /davve
 *
 */

void write_vanilla(string fn)
{
    string value_str = "50;10;50";
    array(float) value_arr =  map(value_str / ";",
                                  lambda(string i) { return (float)i; } );

	Stdio.File f = Stdio.File(fn, "tcw");

	if (sizeof(value_arr) == 0)
		return;

	array(float) key_time_arr = allocate(sizeof(value_arr));
    for (int i=0;i<sizeof(key_time_arr); ++i)
    {
		key_time_arr[i] = (float)(i / ((float)sizeof(key_time_arr) - 1));
		write("%f\n", key_time_arr[i]);
	}

    for (float w=0.0;w<=1.0;w+=0.1)
    {
        int to_idx = Array.search_array(key_time_arr, lambda(float a) {
                                                       return (a>w);
                                                   });
        int from_idx = (to_idx == 0) ? 0 : to_idx - 1;

        float mapped_w = (w - key_time_arr[from_idx]) / (key_time_arr[to_idx] - key_time_arr[from_idx]);

        float res = value_arr[from_idx] + (value_arr[to_idx] - value_arr[from_idx]) * mapped_w;
        f->write("%f\t%f\n", w, res);
    }

	f->close();
}

void write_key_times(string fn)
{
    string value_str = "50;10;50";
    string key_time_str =  "0.0;0.2;1.0";

    array(float) value_arr =  map(value_str / ";",
                                  lambda(string i) { return (float)i; } );
    array(float) key_time_arr = map(key_time_str / ";",
                                          lambda(string i) {
                                              return (float)i;
                                          });

    if (sizeof(key_time_arr) != sizeof(value_arr))
        return;

	Stdio.File f = Stdio.File(fn, "tcw");

    for (float w=0.0;w<=1.0;w+=0.1)
    {
        int to_idx = Array.search_array(key_time_arr, lambda(float a) {
                                                       return (a>w);
                                                   });
        int from_idx = (to_idx == 0) ? 0 : to_idx - 1;

        float mapped_w = (w - key_time_arr[from_idx]) / (key_time_arr[to_idx] - key_time_arr[from_idx]);

        float res = value_arr[from_idx] + (value_arr[to_idx] - value_arr[from_idx]) * mapped_w;
        f->write("%f\t%f\n", w, res);
    }

	f->close();
}

float mid(float a, float b) { return a + (b - a) * 0.5; }

float
calc_key_spline(array(float) cp, float sx, float flatness)
{
    array(float) buf = cp + ({});

    float res = sx;
    while (1)
    {
        array(float) lcp = allocate(8);
        array(float) rcp = allocate(8);
	
        lcp[0] = buf[0];
        lcp[1] = buf[1];
        lcp[2] = mid(buf[0], buf[2]);
        lcp[3] = mid(buf[1], buf[3]);
        float tmpx = mid(buf[2], buf[4]);
        float tmpy = mid(buf[3], buf[5]);
        rcp[4] = mid(buf[4], buf[6]);
        rcp[5] = mid(buf[5], buf[7]);
        rcp[6] = buf[6];
        rcp[7] = buf[7];
        lcp[4] = mid(lcp[2], tmpx);
        lcp[5] = mid(lcp[3], tmpy);
        rcp[2] = mid(rcp[4], tmpx);
        rcp[3] = mid(rcp[5], tmpy);
        lcp[6] = mid(lcp[4], rcp[2]);
        lcp[7] = mid(lcp[5], rcp[3]);
        rcp[0] = lcp[6]; rcp[1] = lcp[7];

        if (abs(rcp[0] - sx) < flatness)
        {
            res = rcp[1];
            break;
        }
        else if (sx > rcp[0])
        {
            for (int i=0;i<8;++i)
                buf[i] = rcp[i];
        }
        else
        {
            for (int i=0;i<8;++i)
                buf[i] = lcp[i];
        }
    }

    return res;
}

int write_key_splines(string fn)
{
    string value_str = "50;10;50";
    string key_time_str =  "0.0;0.2;1.0";
    string key_spline_str =  "0.7 0.0 1.0 0.3; 0.0 0.7 0.3 1.0"; // supports only ' ' as separator

    array(float) value_arr =  map(value_str / ";",
                                  lambda(string i) { return (float)i; } );
    array(float) key_time_arr = map(key_time_str / ";",
                                          lambda(string i) {
                                              return (float)i;
                                          });

    array(array(float)) key_spline_arr = map(key_spline_str / ";",
                                             lambda(string i) {
                                                 i = String.trim_whites(i);
                                                 return map (i / " ",
                                                             lambda(string j) {
                                                                 return (float)j;
                                                             }); });

    if (sizeof(key_time_arr) != sizeof(value_arr) &&
        sizeof(key_spline_arr) == sizeof(key_time_arr) - 1)
        return 1;

	Stdio.File f = Stdio.File(fn, "tcw");

    for (float w=0.0;w<=1.0;w+=0.05)
    {
        int to_idx = Array.search_array(key_time_arr, lambda(float a) {
                                                       return (a>w);
                                                   });
        int from_idx = (to_idx == 0) ? 0 : to_idx - 1;

        float mapped_w = (w - key_time_arr[from_idx]) / (key_time_arr[to_idx] - key_time_arr[from_idx]);

        float spline_mapped_w = calc_key_spline(({ 0.0, 0.0, @key_spline_arr[from_idx], 1.0, 1.0 }),
                                                mapped_w,
                                                0.01);

        float res = value_arr[from_idx] + (value_arr[to_idx] - value_arr[from_idx]) * spline_mapped_w;
        f->write("%f\t%.6f\n", w, res);
    }
	f->close();
}

int main() {
    write_vanilla("linear_interpolation_function_samples.txt");
    write_key_times("linear_interpolation_function_key_times_samples.txt");
    write_key_splines("linear_interpolation_function_key_splines_samples.txt");
}
