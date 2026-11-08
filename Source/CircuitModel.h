// Stage-by-stage model of the TS808 overdrive circuit. No JUCE dependency, so
// the test program can compile it on its own. All signals are AC volts about
// the 4.5 V bias, and every stage assumes ideal op-amps.
//
// Signal path:
//   input buffer -> clipping amp -> 1k/0.22u low-pass + active tone -> level -> output buffer
#pragma once

#include <algorithm>
#include <cmath>

namespace circuit
{
constexpr double pi = 3.14159265358979323846;

// Component values, from the traced LTspice netlist of the circuit.
constexpr double C1 = 0.02e-6, R2 = 510e3;              // input coupling, bias
constexpr double C2 = 1e-6, R4 = 10e3;                  // coupling into clipping amp
constexpr double Rfb = 51e3, RdriveMax = 500e3;         // clipping amp feedback
constexpr double Cfb = 51e-12;
constexpr double Ri = 4.7e3, Ci = 0.047e-6;             // inverting leg to ground
constexpr double R8 = 1e3, C3 = 0.22e-6, R9 = 10e3;     // post-clip low-pass, bias
constexpr double RtoneMax = 20e3, R10 = 1e3;            // tone pot, op-amp feedback
constexpr double R11 = 220.0, C6 = 0.22e-6;             // tone wiper leg
constexpr double C7 = 1e-6, R14 = 1e3;                  // tone stage output
constexpr double RlevelMax = 100e3;
constexpr double R18 = 510e3, R19 = 510e3, C8 = 1e-6;   // output buffer bias, coupling
constexpr double RoutSeries = 100.0, RoutLoad = 10e3;   // TS808 output resistors

// 1N4148 SPICE parameters. Rs is left out: feedback currents are tiny.
constexpr double Is = 2.52e-9, nVt = 1.752 * 0.02585;

// Approximate swing limit of a 4558 on a 9 V supply (estimate, not measured).
constexpr double railVolts = 3.5;

// Audio-taper approximation: 15% of the resistance at the halfway position.
inline double audioTaper (double x)
{
    constexpr double a = 32.0;
    return (std::pow (a, x) - 1.0) / (a - 1.0);
}

// Pot resistances keep the 1 ohm minimum the LTspice model uses.
inline double driveResistance (double x) { return audioTaper (x) * RdriveMax + 1.0; }
inline double toneRb (double x)          { return RtoneMax * (1.0 - x) + 1.0; } // wiper to inverting input
inline double toneRa (double x)          { return RtoneMax * x + 1.0; }         // wiper to non-inverting input

inline double levelLoad() { return 1.0 / (1.0 / R18 + 1.0 / R19); }

struct LevelNetwork
{
    double gain, seriesR;
};

inline LevelNetwork levelNetwork (double x)
{
    const double lower = audioTaper (x) * RlevelMax + 1.0;
    const double upper = RlevelMax - lower + 2.0;
    const double loaded = 1.0 / (1.0 / lower + 1.0 / levelLoad());
    return { loaded / (R14 + upper + loaded), R14 + upper + loaded };
}

// s-domain transfer function of the low-pass + tone stage, from nodal analysis:
//   H(s) = (b1 s + b0) / (a2 s^2 + a1 s + a0)
struct ToneCoeffs
{
    double b1, b0, a2, a1, a0;
};

inline ToneCoeffs toneCoeffs (double tone)
{
    const double Ra = toneRa (tone), Rb = toneRb (tone);
    const double G = 1.0 / Ra + 1.0 / Rb;
    const double a = C6 * (1.0 + G * R11);
    const double k = 1.0 + R8 / R9;
    return { a + R10 * C6 / Rb,
             G,
             R8 * C3 * a,
             k * a + R8 * C3 * G + R8 * C6 / Ra,
             k * G };
}

class OnePoleHighPass
{
public:
    void setCutoff (double hz, double fs) { const double t = std::tan (pi * hz / fs); g = t / (1.0 + t); }
    void reset() { s = 0.0; }

    double process (double x)
    {
        const double v = (x - s) * g;
        const double lp = v + s;
        s = lp + v;
        return x - lp;
    }

private:
    double g = 0.0, s = 0.0;
};

// Non-inverting amp with antiparallel diodes in the feedback loop, solved
// sample by sample with trapezoidal capacitors. With an ideal op-amp the
// inverting node follows the input, so vout = vin + vf and the only unknown
// is the feedback voltage vf:
//   vf/Rf + Cf dvf/dt + 2 Is sinh(vf / nVt) = i(Ri + Ci leg)
class ClippingAmp
{
public:
    void prepare (double fs)
    {
        gc = 2.0 * Ci * fs;
        gf = 2.0 * Cfb * fs;
        gs = 1.0 / (Ri + 1.0 / gc);
        reset();
    }

    void reset() { hc = hf = vf = 0.0; }

    void setFeedbackResistance (double r) { gfb = 1.0 / r; }

    double process (double vin)
    {
        const double i1 = gs * (vin - hc / gc);
        vf = solve (i1);

        const double icf = gf * vf - hf;
        hf = gf * vf + icf;
        const double vc = (i1 + hc) / gc;
        hc = gc * vc + i1;

        return std::clamp (vin + vf, -railVolts, railVolts);
    }

private:
    double solve (double i1) const
    {
        // The residual rises monotonically with v, so a bracketed Newton
        // search cannot diverge.
        const double slope = gfb + gf;
        double lo = -1.5, hi = 1.5, v = vf;

        for (int i = 0; i < 40; ++i)
        {
            const double x = v / nVt;
            const double f = v * slope - hf + 2.0 * Is * std::sinh (x) - i1;
            const double df = slope + 2.0 * Is * std::cosh (x) / nVt;

            (f > 0.0 ? hi : lo) = v;

            double next = v - f / df;
            if (next <= lo || next >= hi)
                next = 0.5 * (lo + hi);

            const double delta = std::abs (next - v);
            v = next;
            if (delta < 1.0e-10)
                break;
        }
        return v;
    }

    double gc = 0.0, gf = 0.0, gs = 0.0, gfb = 1.0 / Rfb;
    double hc = 0.0, hf = 0.0, vf = 0.0;
};

class ToneStage
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { z1 = z2 = 0.0; }

    void setTone (double tone)
    {
        const auto c = toneCoeffs (tone);
        const double K = 2.0 * fs, K2 = K * K;
        const double norm = 1.0 / (c.a2 * K2 + c.a1 * K + c.a0);
        b0z = (c.b1 * K + c.b0) * norm;
        b1z = 2.0 * c.b0 * norm;
        b2z = (c.b0 - c.b1 * K) * norm;
        a1z = 2.0 * (c.a0 - c.a2 * K2) * norm;
        a2z = (c.a2 * K2 - c.a1 * K + c.a0) * norm;
    }

    double process (double x)
    {
        const double y = b0z * x + z1;
        z1 = b1z * x - a1z * y + z2;
        z2 = b2z * x - a2z * y;
        return std::clamp (y, -railVolts, railVolts);
    }

private:
    double fs = 44100.0, b0z = 1.0, b1z = 0.0, b2z = 0.0, a1z = 0.0, a2z = 0.0, z1 = 0.0, z2 = 0.0;
};

// One channel of the whole pedal. Run it at the oversampled rate.
class Pedal
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        inputCoupling.setCutoff (1.0 / (2.0 * pi * C1 * R2), fs);
        clipCoupling.setCutoff (1.0 / (2.0 * pi * C2 * R4), fs);
        outputCoupling.setCutoff (1.0 / (2.0 * pi * C8 * levelLoad()), fs);
        clip.prepare (fs);
        tone.prepare (fs);
        setControls (0.5, 0.5, 0.5);
        reset();
    }

    void reset()
    {
        inputCoupling.reset();
        clipCoupling.reset();
        toneCoupling.reset();
        outputCoupling.reset();
        clip.reset();
        tone.reset();
    }

    void setControls (double drive, double toneAmount, double level)
    {
        clip.setFeedbackResistance (Rfb + driveResistance (drive));
        tone.setTone (toneAmount);
        const auto net = levelNetwork (level);
        levelGain = net.gain;
        toneCoupling.setCutoff (1.0 / (2.0 * pi * C7 * net.seriesR), fs);
    }

    double process (double vin)
    {
        double v = clipCoupling.process (inputCoupling.process (vin));
        v = tone.process (clip.process (v));
        v = toneCoupling.process (v) * levelGain;
        return outputCoupling.process (v) * RoutLoad / (RoutLoad + RoutSeries);
    }

private:
    double fs = 176400.0, levelGain = 0.5;
    OnePoleHighPass inputCoupling, clipCoupling, toneCoupling, outputCoupling;
    ClippingAmp clip;
    ToneStage tone;
};
} // namespace circuit
