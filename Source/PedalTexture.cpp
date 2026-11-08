#include "PedalTexture.h"

#include <algorithm>
#include <array>
#include <vector>

namespace pedal
{
namespace
{
struct Field
{
    Field (int width, int height) : w (width), h (height), v ((size_t) (width * height), 0.0f) {}

    float& at (int x, int y) { return v[(size_t) (y * w + x)]; }
    float get (int x, int y) const
    {
        return v[(size_t) (juce::jlimit (0, h - 1, y) * w + juce::jlimit (0, w - 1, x))];
    }

    int w, h;
    std::vector<float> v;
};

void blurLine (const float* src, float* dst, int n, int stride, int r)
{
    const float norm = 1.0f / (float) (2 * r + 1);
    float sum = 0.0f;
    for (int i = -r; i <= r; ++i)
        sum += src[juce::jlimit (0, n - 1, i) * stride];

    for (int i = 0; i < n; ++i)
    {
        dst[i * stride] = sum * norm;
        sum += src[juce::jlimit (0, n - 1, i + r + 1) * stride] - src[juce::jlimit (0, n - 1, i - r) * stride];
    }
}

// Three box passes approximate a gaussian.
void blur (Field& f, int radius)
{
    if (radius <= 0)
        return;

    std::vector<float> tmp (f.v.size());
    for (int pass = 0; pass < 3; ++pass)
    {
        for (int y = 0; y < f.h; ++y)
            blurLine (f.v.data() + y * f.w, tmp.data() + y * f.w, f.w, 1, radius);
        for (int x = 0; x < f.w; ++x)
            blurLine (tmp.data() + x, f.v.data() + x, f.h, f.w, radius);
    }
}

template <typename Draw>
Field paintMask (int w, int h, float scale, Draw&& draw)
{
    juce::Image image (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    {
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (scale));
        g.setColour (juce::Colours::white);
        draw (g);
    }

    Field f (w, h);
    const juce::Image::BitmapData data (image, juce::Image::BitmapData::readOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            f.at (x, y) = (float) ((const juce::PixelARGB*) data.getPixelPointer (x, y))->getAlpha() / 255.0f;
    return f;
}

class ValueNoise
{
public:
    explicit ValueNoise (juce::int64 seed)
    {
        juce::Random r (seed);
        for (auto& v : table)
            v = r.nextFloat();
    }

    float eval (float x, float y) const
    {
        const int xi = (int) std::floor (x), yi = (int) std::floor (y);
        const float fx = smooth (x - (float) xi), fy = smooth (y - (float) yi);
        const float a = lattice (xi, yi) + fx * (lattice (xi + 1, yi) - lattice (xi, yi));
        const float b = lattice (xi, yi + 1) + fx * (lattice (xi + 1, yi + 1) - lattice (xi, yi + 1));
        return a + fy * (b - a);
    }

    float fractal (float x, float y) const
    {
        return 0.55f * eval (x, y) + 0.30f * eval (x * 2.1f, y * 2.1f) + 0.15f * eval (x * 4.3f, y * 4.3f);
    }

private:
    static float smooth (float t) { return t * t * (3.0f - 2.0f * t); }
    float lattice (int x, int y) const { return table[(size_t) ((x & 255) + ((y & 255) << 8))]; }

    std::array<float, 65536> table;
};

float smoothstep (float a, float b, float x)
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - a) / (b - a));
    return t * t * (3.0f - 2.0f * t);
}

float roundedBoxDistance (float x, float y, float cx, float cy, float halfW, float halfH, float radius)
{
    const float qx = std::abs (x - cx) - halfW + radius;
    const float qy = std::abs (y - cy) - halfH + radius;
    const float outside = std::hypot (juce::jmax (qx, 0.0f), juce::jmax (qy, 0.0f));
    return outside + juce::jmin (juce::jmax (qx, qy), 0.0f) - radius;
}

void strokeScratch (juce::Graphics& g, juce::Random& r, float x, float y, float angle, float length, float width)
{
    const float bend = (r.nextFloat() - 0.5f) * length * 0.18f;
    const float cx = x + std::cos (angle) * length * 0.5f - std::sin (angle) * bend;
    const float cy = y + std::sin (angle) * length * 0.5f + std::cos (angle) * bend;

    juce::Path path;
    path.startNewSubPath (x, y);
    path.quadraticTo (cx, cy, x + std::cos (angle) * length, y + std::sin (angle) * length);
    g.strokePath (path, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

float randomAngle (juce::Random& r)
{
    // Scrapes tend to follow how a pedal slides around in a bag.
    const float gaussian = (r.nextFloat() + r.nextFloat() + r.nextFloat() - 1.5f) * 0.9f;
    return (r.nextBool() ? -0.45f : 2.2f) + gaussian;
}

void drawFineScratches (juce::Graphics& g, const Layout& l)
{
    juce::Random r (8081);
    for (int i = 0; i < 520; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f + r.nextFloat() * 0.5f));
        strokeScratch (g, r, r.nextFloat() * (float) l.width, r.nextFloat() * (float) l.height,
                       randomAngle (r), 8.0f + r.nextFloat() * r.nextFloat() * 110.0f, 0.3f + r.nextFloat() * 0.35f);
    }

    // Boot scuffs gather around the footswitch.
    const auto foot = l.footswitch.toFloat().expanded (14.0f);
    for (int cluster = 0; cluster < 34; ++cluster)
    {
        const float x = foot.getX() + r.nextFloat() * foot.getWidth();
        const bool above = r.nextBool();
        const float y = above ? foot.getY() + r.nextFloat() * 14.0f : foot.getBottom() - r.nextFloat() * 14.0f;
        const float angle = randomAngle (r);
        for (int i = 0; i < 9; ++i)
        {
            g.setColour (juce::Colours::white.withAlpha (0.2f + r.nextFloat() * 0.35f));
            strokeScratch (g, r, x + r.nextFloat() * 14.0f, y + r.nextFloat() * 6.0f,
                           angle + (r.nextFloat() - 0.5f) * 0.3f, 4.0f + r.nextFloat() * 18.0f, 0.4f);
        }
    }
}

void drawDeepScratches (juce::Graphics& g, const Layout& l)
{
    juce::Random r (4558);
    for (int i = 0; i < 26; ++i)
        strokeScratch (g, r, 16.0f + r.nextFloat() * ((float) l.width - 32.0f),
                       16.0f + r.nextFloat() * ((float) l.height - 32.0f), randomAngle (r),
                       6.0f + r.nextFloat() * 62.0f, 0.45f + r.nextFloat() * 0.55f);
}

void drawScale (juce::Graphics& g, juce::Rectangle<int> knob, float radiusScale)
{
    const auto b = knob.toFloat();
    const auto centre = b.getCentre();
    const float radius = b.getWidth() * radiusScale;

    for (int i = 0; i <= 10; ++i)
    {
        const float angle = rotaryStart + (rotaryEnd - rotaryStart) * (float) i / 10.0f;
        const float size = (i == 0 || i == 5 || i == 10) ? 2.0f : 1.3f;
        const float x = centre.x + radius * std::sin (angle);
        const float y = centre.y - radius * std::cos (angle);
        g.fillEllipse (x - size, y - size, size * 2.0f, size * 2.0f);
    }
}

juce::Font faceFont (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

void drawSilkscreen (juce::Graphics& g, const Layout& l)
{
    drawScale (g, l.drive, 0.57f);
    drawScale (g, l.level, 0.57f);
    drawScale (g, l.tone, 0.60f);
    drawScale (g, l.calibration, 0.62f);

    // Printed outline around the lower panel
    g.drawRoundedRectangle (31.0f, 240.0f, (float) l.width - 62.0f, (float) l.height - 272.0f, 6.0f, 1.6f);

    const auto label = [&] (juce::Rectangle<int> knob, const char* text, float size)
    {
        g.setFont (faceFont (size));
        g.drawText (text, knob.getCentreX() - 65, knob.getY() - 25, 130, 18, juce::Justification::centred);
    };
    label (l.drive, "DRIVE", 14.0f);
    label (l.level, "LEVEL", 14.0f);
    label (l.tone, "TONE", 13.0f);

    g.setFont (faceFont (10.0f));
    g.drawText ("INPUT CAL", l.calibration.translated (-78, 0).withWidth (70), juce::Justification::centredRight);

    g.setFont (faceFont (34.0f));
    g.drawText ("GreenDrive", 0, 236, l.width, 42, juce::Justification::centred);
    g.setFont (faceFont (14.0f, false));
    g.drawText ("O V E R D R I V E", 0, 276, l.width, 20, juce::Justification::centred);
}

void drawOccluders (juce::Graphics& g, const Layout& l, float dx, float dy, float grow)
{
    for (const auto& knob : { l.drive, l.level, l.tone, l.calibration })
        g.fillEllipse (knob.toFloat().reduced (4.0f - grow).translated (dx, dy));

    g.fillRoundedRectangle (l.footswitch.toFloat().reduced (2.0f - grow).translated (dx, dy), 12.0f);
    g.fillEllipse (l.led.expanded (3.0f + grow).translated (dx * 0.3f, dy * 0.3f));
}

void drawLedBezel (juce::Graphics& g, const Layout& l)
{
    const auto c = l.led.getCentre();
    const float lit = juce::jlimit (0.2f, 1.0f, lightAt (c.x, c.y) * 1.2f);
    const auto bezel = l.led.expanded (3.5f);

    g.setGradientFill (juce::ColourGradient (juce::Colour::greyLevel (juce::jmin (1.0f, 0.5f + lit * 0.5f)),
                                             bezel.getX(), bezel.getY(),
                                             juce::Colour::greyLevel (0.1f), bezel.getRight(), bezel.getBottom(), false));
    g.fillEllipse (bezel);
    g.setColour (juce::Colour (0xff070707));
    g.fillEllipse (l.led.expanded (1.0f));
}
} // namespace

juce::Image renderFace (const Layout& l, float s)
{
    const int w = juce::roundToInt ((float) l.width * s);
    const int h = juce::roundToInt ((float) l.height * s);

    const auto logicalW = (float) l.width, logicalH = (float) l.height;
    const float bodyCx = logicalW * 0.5f, bodyCy = logicalH * 0.5f;
    const float bodyHalfW = bodyCx - 12.0f, bodyHalfH = bodyCy - 12.0f, bodyRadius = 16.0f;
    const float bevel = 7.0f;

    Field fine = paintMask (w, h, s, [&] (juce::Graphics& g) { drawFineScratches (g, l); });
    Field deep = paintMask (w, h, s, [&] (juce::Graphics& g) { drawDeepScratches (g, l); });
    Field print = paintMask (w, h, s, [&] (juce::Graphics& g) { drawSilkscreen (g, l); });

    Field cast = paintMask (w, h, s, [&] (juce::Graphics& g) { drawOccluders (g, l, 9.0f, 13.0f, 0.0f); });
    blur (cast, juce::roundToInt (5.0f * s));
    Field contact = paintMask (w, h, s, [&] (juce::Graphics& g) { drawOccluders (g, l, 0.0f, 0.0f, 1.5f); });
    blur (contact, juce::roundToInt (1.5f * s));
    Field ground = paintMask (w, h, s, [&] (juce::Graphics& g)
    {
        g.fillRoundedRectangle (bodyCx - bodyHalfW + 9.0f, bodyCy - bodyHalfH + 13.0f,
                                bodyHalfW * 2.0f, bodyHalfH * 2.0f, bodyRadius);
    });
    blur (ground, juce::roundToInt (5.0f * s));

    // Orange-peel powder coat: blurred white noise, normalised to unit variance.
    Field peel (w, h);
    {
        juce::Random r (2472);
        for (auto& v : peel.v)
            v = r.nextFloat() - 0.5f;
        blur (peel, 1);
        float variance = 0.0f;
        for (auto v : peel.v)
            variance += v * v;
        const float scale = 1.0f / std::sqrt (variance / (float) peel.v.size());
        for (auto& v : peel.v)
            v *= scale;
    }

    const ValueNoise patchNoise (11), blotchNoise (22);
    juce::Random grain (31);

    Field height (w, h);
    Field wear (w, h);
    Field primer (w, h);
    Field coverage (w, h);

    for (int y = 0; y < h; ++y)
    {
        for (int x = 0; x < w; ++x)
        {
            const float lx = ((float) x + 0.5f) / s, ly = ((float) y + 0.5f) / s;
            const float d = roundedBoxDistance (lx, ly, bodyCx, bodyCy, bodyHalfW, bodyHalfH, bodyRadius);
            const float t = juce::jlimit (0.0f, 1.0f, -d / bevel);
            const float inside = juce::jlimit (0.0f, 1.0f, 0.5f - d * s);

            const float patches = patchNoise.fractal (lx * 0.055f, ly * 0.055f);
            // Enamel breaks away with crisp, ragged edges, leaving a thin ring of
            // undercoat, and tiny flecks gather towards the edges.
            const float ragged = patchNoise.fractal (lx * 0.45f, ly * 0.45f) - 0.5f;
            const float chipValue = patches + 0.34f * std::exp (d / 5.0f) + ragged * 0.10f;
            const float flecks = smoothstep (0.700f, 0.706f,
                                             patchNoise.eval (lx * 0.42f + 90.0f, ly * 0.42f)
                                                 * (0.71f + 0.20f * std::exp (d / 9.0f)));
            const float chips = juce::jmax (smoothstep (0.743f, 0.749f, chipValue), flecks);
            const float undercoat = smoothstep (0.731f, 0.737f, chipValue) * (1.0f - chips);

            const float rim = bevel * std::sqrt (juce::jmax (0.0f, 1.0f - (1.0f - t) * (1.0f - t)));
            const float panelStep = smoothstep (225.0f, 228.0f, ly) * 2.4f * s;
            height.at (x, y) = inside * (rim * s + panelStep
                                         + 0.012f * peel.at (x, y) * (1.0f - chips)
                                         - 0.55f * fine.at (x, y)
                                         - 1.1f * deep.at (x, y)
                                         - 1.2f * chips - 0.35f * undercoat);
            wear.at (x, y) = chips;
            primer.at (x, y) = undercoat;
            coverage.at (x, y) = inside;
        }
    }

    juce::Image image (juce::Image::RGB, w, h, false, juce::SoftwareImageType());
    {
        const juce::Image::BitmapData data (image, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const float lx = ((float) x + 0.5f) / s, ly = ((float) y + 0.5f) / s;
                const float inside = coverage.at (x, y);

                // Surface normal from the height field.
                const float nx0 = height.get (x - 1, y) - height.get (x + 1, y);
                const float ny0 = height.get (x, y - 1) - height.get (x, y + 1);
                const float nz0 = 2.0f;
                const float nLen = 1.0f / std::sqrt (nx0 * nx0 + ny0 * ny0 + nz0 * nz0);
                const float nx = nx0 * nLen, ny = ny0 * nLen, nz = nz0 * nLen;

                // Direction and distance to the light.
                const float ldx = lightX - lx, ldy = lightY - ly;
                const float dist2 = ldx * ldx + ldy * ldy + lightZ * lightZ;
                const float invDist = 1.0f / std::sqrt (dist2);
                const float lvx = ldx * invDist, lvy = ldy * invDist, lvz = lightZ * invDist;

                const float attenuation = lightZ * lightZ / dist2;
                const float ndotl = juce::jmax (0.0f, nx * lvx + ny * lvy + nz * lvz);

                // Blinn half vector, with the viewer straight above.
                const float hx0 = lvx, hy0 = lvy, hz0 = lvz + 1.0f;
                const float hLen = 1.0f / std::sqrt (hx0 * hx0 + hy0 * hy0 + hz0 * hz0);
                const float ndoth = juce::jmax (0.0f, (nx * hx0 + ny * hy0 + nz * hz0) * hLen);

                // Materials, in linear light.
                const float blotch = 0.92f + 0.16f * blotchNoise.fractal (lx * 0.02f, ly * 0.02f);
                float r = 0.020f * blotch, g = 0.400f * blotch, b = 0.085f * blotch;

                const float scratch = fine.at (x, y);
                r += scratch * 0.06f;
                g += scratch * 0.07f;
                b += scratch * 0.055f;

                const float deepMark = deep.at (x, y) * 0.75f;
                const float paintLoss = juce::jmax (wear.at (x, y), deepMark);

                const float flaking = smoothstep (0.66f, 0.80f, blotchNoise.eval (lx * 0.75f, ly * 0.75f));
                const float ink = print.at (x, y) * (1.0f - paintLoss)
                                  * (1.0f - 0.7f * scratch) * (1.0f - 0.3f * flaking);
                r += (0.82f - r) * ink;
                g += (0.78f - g) * ink;
                b += (0.58f - b) * ink;

                const float halo = primer.at (x, y);
                r += (0.60f - r) * halo;
                g += (0.60f - g) * halo;
                b += (0.52f - b) * halo;

                r += (0.40f - r) * paintLoss;
                g += (0.40f - g) * paintLoss;
                b += (0.38f - b) * paintLoss;

                const float power = 70.0f + paintLoss * 60.0f;
                const float gloss = (0.13f - 0.07f * scratch - 0.05f * paintLoss) * std::pow (ndoth, power);

                float outR, outG, outB;
                if (inside > 0.0f)
                {
                    const float diffuse = 0.10f + 2.1f * attenuation * ndotl;
                    const float spec = gloss * attenuation * 2.2f;
                    const float occlusion = (1.0f - 0.72f * cast.at (x, y)) * (1.0f - 0.55f * contact.at (x, y));
                    outR = (r * diffuse + spec) * occlusion;
                    outG = (g * diffuse + spec) * occlusion;
                    outB = (b * diffuse + spec) * occlusion;
                }
                else
                {
                    outR = outG = outB = 0.0f;
                }

                // Dark tabletop around the pedal, with the pedal's shadow on it.
                const float table = (0.012f + 0.016f * blotchNoise.fractal (lx * 0.3f, ly * 0.08f))
                                    * (0.3f + 2.6f * attenuation) * (1.0f - 0.92f * ground.at (x, y));
                const float grainAmount = 1.0f + (grain.nextFloat() - 0.5f) * 0.018f;

                const auto encode = [&] (float linear, float base)
                {
                    const float mixed = (linear * inside + base * (1.0f - inside)) * grainAmount;
                    return (juce::uint8) juce::roundToInt (255.0f * std::sqrt (juce::jlimit (0.0f, 1.0f, mixed)));
                };

                auto* pixel = (juce::PixelRGB*) data.getPixelPointer (x, y);
                pixel->setARGB (255, encode (outR, table * 1.1f), encode (outG, table), encode (outB, table * 0.95f));
            }
        }
    }

    {
        juce::Graphics g (image);
        g.addTransform (juce::AffineTransform::scale (s));
        drawLedBezel (g, l);
    }

    return image;
}
} // namespace pedal
