/*
 * John D. Cook's public domain version of lgamma, from
 * http://www.johndcook.com/stand_alone_code.html
 *
 * Replaces the C99 standard lgamma for stone-age C compilers like the one
 * from Redmond.
 *
 * I removed the test cases and added the cfloat import (Vlad N. <vlad@vene.ro>)
 *
 * Translated to C by Lars Buitinck. Input validation removed; we handle
 * that in the Cython wrapper.
 *
 * sklearn_ prefix removed to avoid confusing lda readers by Allen B. Riddell.
 */

#include <float.h>
#include <math.h>

#include "gamma.h"

/* Euler's gamma constant. */
#define GAMMA 0.577215664901532860606512090

#define HALF_LOG2_PI 0.91893853320467274178032973640562

static double lda_gamma(double x)
{
    /*
     * Split the function domain into three intervals:
     * (0, 0.001), [0.001, 12), and (12, infinity).
     */

    /*
     * First interval: (0, 0.001).
     *
     * For small x, 1/Gamma(x) has power series x + gamma x^2  - ...
     * So in this range, 1/Gamma(x) = x + gamma x^2 with error
     * on the order of x^3.
     * The relative error over this interval is less than 6e-7.
     */
    if (x < 0.001)
        return 1.0 / (x * (1.0 + GAMMA * x));

    /*
     * Second interval: [0.001, 12).
     */
    if (x < 12.0)
    {
        /* numerator coefficients for approximation over the interval (1,2) */
        static const double p[] = {
            -1.71618513886549492533811E+0,
            2.47656508055759199108314E+1,
            -3.79804256470945635097577E+2,
            6.29331155312818442661052E+2,
            8.66966202790413211295064E+2,
            -3.14512729688483675254357E+4,
            -3.61444134186911729807069E+4,
            6.64561438202405440627855E+4};

        /* denominator coefficients for approximation over the interval (1,2) */
        static const double q[] = {
            -3.08402300119738975254353E+1,
            3.15350626979604161529144E+2,
            -1.01515636749021914166146E+3,
            -3.10777167157231109440444E+3,
            2.25381184209801510330112E+4,
            4.75584627752788110767815E+3,
            -1.34659959864969306392456E+5,
            -1.15132259675553483497211E+5};

        double den, num, result, z;

        /* The algorithm directly approximates gamma over (1,2) and uses
         * reduction identities to reduce other arguments to this interval. */
        double y = x;
        int i, n = 0;
        int arg_was_less_than_one = (y < 1.0);

        /* Add or subtract integers as necessary to bring y into (1,2)
         * Will correct for this below */
        if (arg_was_less_than_one)
            y += 1.0;
        else
        {
            n = (int)floor(y) - 1;
            y -= n;
        }

        num = 0.0;
        den = 1.0;

        z = y - 1;
        for (i = 0; i < 8; i++)
        {
            num = (num + p[i]) * z;
            den = den * z + q[i];
        }
        result = num / den + 1.0;

        /* Apply correction if argument was not initially in (1,2) */
        if (arg_was_less_than_one)
            /* Use identity gamma(z) = gamma(z+1)/z
             * The variable "result" now holds gamma of the original y + 1
             * Thus we use y-1 to get back the original y. */
            result /= (y - 1.0);
        else
            /* Use the identity gamma(z+n) = z*(z+1)* ... *(z+n-1)*gamma(z) */
            for (i = 0; i < n; i++, y++)
                result *= y;

        return result;
    }

    /*
     * Third interval: [12, infinity).
     */
    if (x > 171.624)
        /* Correct answer too large to display, force +infinity. */
        return 2 * DBL_MAX;
    return exp(lda_lgamma(x));
}

double lda_lgamma(double x)
{
    /*
     * Abramowitz and Stegun 6.1.41
     * Asymptotic series should be good to at least 11 or 12 figures
     * For error analysis, see Whittiker and Watson
     * A Course in Modern Analysis (1927), page 252
     */
    static const double c[8] =
        {
            1.0 / 12.0,
            -1.0 / 360.0,
            1.0 / 1260.0,
            -1.0 / 1680.0,
            1.0 / 1188.0,
            -691.0 / 360360.0,
            1.0 / 156.0,
            -3617.0 / 122400.0};

    double z, sum;
    int i;

    if (x < 12.0)
        return log(fabs(lda_gamma(x)));

    z = 1.0 / (x * x);
    sum = c[7];
    for (i = 6; i >= 0; i--)
    {
        sum *= z;
        sum += c[i];
    }

    return (x - 0.5) * log(x) - x + HALF_LOG2_PI + sum / x;
}

double lda_digamma(double x)
{
    /*
    reference: https://people.sc.fsu.edu/~jburkardt/cpp_src/asa103/asa103.cpp
    */
    const double c = 8.5;
    const double pi = 3.14159265359;
    const double euler = 0.57721566490153286060;
    double r;
    double value = 0.0e0;
    double x2;

    if ((x <= 0.000001) && (x > 0))
    {
        value = value - euler - 1.0 / x + 1.6449340668482264365 * x;
        return value;
    }

    if (x < 0)
    {
        value = -pi / tan(pi * x);
        x = 1.0e0 - x;
    }

    x2 = x;
    while (x2 < c)
    {
        value = value - 1.0 / x2;
        x2 = x2 + 1.0;
    }

    r = 1.0 / x2;
    value = value + log(x2) - 0.5 * r;

    r = r * r;

    value = value - r * (1.0 / 12.0 - r * (1.0 / 120.0 - r * (1.0 / 252.0 - r * (1.0 / 240.0 - r * (1.0 / 132.0)))));

    return value;
}