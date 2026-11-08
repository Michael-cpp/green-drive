// Checks the discrete model against closed-form circuit behaviour.
// Exit code is the number of failed checks.
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

#include "CircuitModel.h"

using namespace circuit;
using cd = std::complex<double>;

static int failures = 0;

static void check (const char* name, double value, double lo, double hi)
{
    const bool ok = value >= lo && value <= hi && std::isfinite (value);
    std::printf ("%-46s %12.4f   [%g, %g]  %s\n", name, value, lo, hi, ok ? "ok" : "FAIL");
    failures += ok ? 0 : 1;
}

static cd highPass (double hz, double f) { const cd x (0.0, f / hz); return x / (1.0 + x); }

// Small-signal gain of the clipping amp. Includes the 51 pF cap and the diodes'
// conductance at zero volts (2 Is / nVt, about 9 Mohm).
static cd clipGain (double f, double Rf)
{
    const cd s (0.0, 2.0 * pi * f);
    const cd Zf = 1.0 / (1.0 / Rf + s * Cfb + 2.0 * Is / nVt);
    const cd Zi = Ri + 1.0 / (s * Ci);
    return 1.0 + Zf / Zi;
}

// Independent small-signal response.
static double analyticDb (double f, double drive, double tone, double level)
{
    const cd s (0.0, 2.0 * pi * f);
    const cd clip = clipGain (f, Rfb + driveResistance (drive));

    const auto c = toneCoeffs (tone);
    const cd toneH = (c.b1 * s + c.b0) / (c.a2 * s * s + c.a1 * s + c.a0);

    const auto net = levelNetwork (level);
    const cd h = highPass (1.0 / (2.0 * pi * C1 * R2), f)
               * highPass (1.0 / (2.0 * pi * C2 * R4), f)
               * clip * toneH
               * highPass (1.0 / (2.0 * pi * C7 * net.seriesR), f) * net.gain
               * highPass (1.0 / (2.0 * pi * C8 * levelLoad()), f)
               * (RoutLoad / (RoutLoad + RoutSeries));
    return 20.0 * std::log10 (std::abs (h));
}

struct Measured
{
    double rmsIn, rmsOut, peakOut;
};

static Measured runSine (Pedal& p, double f, double amp, double fs, double settle)
{
    p.reset();
    const int settleN = (int) (settle * fs), measureN = (int) (std::ceil (f * 0.25) / f * fs);
    double in2 = 0.0, out2 = 0.0, peak = 0.0;
    for (int n = 0; n < settleN + measureN; ++n)
    {
        const double x = amp * std::sin (2.0 * pi * f * n / fs);
        const double y = p.process (x);
        if (n >= settleN)
        {
            in2 += x * x;
            out2 += y * y;
            peak = std::max (peak, std::abs (y));
        }
    }
    return { std::sqrt (in2 / measureN), std::sqrt (out2 / measureN), peak };
}

int main()
{
    const double fs = 176400.0;
    Pedal p;
    p.prepare (fs);

    std::puts ("-- small-signal response vs closed form (dB error) --");
    for (double drive : { 0.0, 0.5, 1.0 })
        for (double tone : { 0.0, 0.5, 1.0 })
        {
            p.setControls (drive, tone, 1.0);
            for (double f : { 100.0, 720.0, 3000.0, 8000.0 })
            {
                const auto m = runSine (p, f, 1.0e-5, fs, 1.5);
                const double err = 20.0 * std::log10 (m.rmsOut / m.rmsIn) - analyticDb (f, drive, tone, 1.0);
                char name[64];
                std::snprintf (name, sizeof name, "drive %.1f tone %.1f %6.0f Hz", drive, tone, f);
                check (name, err, -0.25, 0.25);
            }
        }

    std::puts ("-- circuit behaviour --");
    {
        // Clipping amp alone: gain formula, then the diode limit.
        ClippingAmp amp;
        amp.prepare (fs);
        amp.setFeedbackResistance (Rfb + driveResistance (0.0));
        double peak = 0.0;
        for (int n = 0; n < (int) fs; ++n)
        {
            const double x = 1.0e-5 * std::sin (2.0 * pi * 5000.0 * n / fs);
            const double y = amp.process (x);
            if (n > (int) fs / 2)
                peak = std::max (peak, std::abs (y));
        }
        const double minExpected = std::abs (clipGain (5000.0, Rfb + driveResistance (0.0)));
        check ("min-drive gain, 5 kHz", peak / 1.0e-5, minExpected * 0.98, minExpected * 1.02);

        amp.reset();
        amp.setFeedbackResistance (Rfb + driveResistance (1.0));
        peak = 0.0;
        for (int n = 0; n < (int) fs; ++n)
        {
            const double x = 1.0e-5 * std::sin (2.0 * pi * 3000.0 * n / fs);
            const double y = amp.process (x);
            if (n > (int) fs / 2)
                peak = std::max (peak, std::abs (y));
        }
        const double maxExpected = std::abs (clipGain (3000.0, Rfb + driveResistance (1.0)));
        check ("max-drive gain, 3 kHz", peak / 1.0e-5, maxExpected * 0.98, maxExpected * 1.02);

        amp.reset();
        double vfPeak = 0.0;
        for (int n = 0; n < (int) fs; ++n)
        {
            const double x = 0.1 * std::sin (2.0 * pi * 1000.0 * n / fs);
            const double y = amp.process (x);
            if (n > (int) fs / 2)
                vfPeak = std::max (vfPeak, std::abs (y - x));
        }
        // Peak current is 0.1 V over |Ri + 1/(s Ci)| at 1 kHz; the diode equation gives the drop.
        const double iPeak = 0.1 / std::abs (cd (Ri, -1.0 / (2.0 * pi * 1000.0 * Ci)));
        const double vfExpected = nVt * std::asinh (iPeak / (2.0 * Is));
        check ("diode drop at clipping (V)", vfPeak, vfExpected * 0.97, vfExpected * 1.03);
    }

    p.setControls (0.5, 0.5, 1.0);
    const auto hot = runSine (p, 440.0, 0.3, fs, 0.5);
    check ("clipped output peak, level max (V)", hot.peakOut, 0.05, 3.6);

    std::puts ("-- stability --");
    for (double rate : { 88200.0, 176400.0, 384000.0, 768000.0 })
    {
        Pedal q;
        q.prepare (rate);
        double worst = 0.0;
        unsigned seed = 1;
        for (int n = 0; n < (int) rate * 3; ++n)
        {
            seed = seed * 1664525u + 1013904223u;
            if (n % 37 == 0)
            {
                const double t = std::fmod (n * 0.000173, 1.0);
                q.setControls (t, 1.0 - t, t);
            }
            const double y = q.process (20.0 * ((seed >> 8) / 8388608.0 - 1.0));
            worst = std::isfinite (y) ? std::max (worst, std::abs (y)) : 1.0e12;
        }
        char name[64];
        std::snprintf (name, sizeof name, "extreme noise + control sweep at %.0f Hz", rate);
        check (name, worst, 0.0, 8.0);
    }

    std::puts ("-- cost --");
    {
        Pedal q;
        q.prepare (fs);
        q.setControls (1.0, 0.5, 0.5);
        const int N = (int) fs * 10;
        volatile double sink = 0.0;
        const auto start = std::chrono::steady_clock::now();
        for (int n = 0; n < N; ++n)
            sink = q.process (1.0 * std::sin (2.0 * pi * 220.0 * n / fs));
        const std::chrono::duration<double> took = std::chrono::steady_clock::now() - start;
        check ("CPU per channel at 44.1 kHz x4 (% of one core)", 100.0 * took.count() / 10.0, 0.0, 50.0);
    }

    std::printf ("\n%d check(s) failed\n", failures);
    return failures;
}
