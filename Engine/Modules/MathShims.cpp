extern "C" {
    float _hypotf(float, float);
    double ldexp(double, int);

    float hypotf(float x, float y) {
        return _hypotf(x, y);
    }

    float ldexpf(float x, int n) {
        return static_cast<float>(ldexp(static_cast<double>(x), n));
    }
}
