#include "Graphs.h"

#include "Theme.h"
#include "core/Timing.h"

using namespace peak;

namespace
{
constexpr double kPi = 3.14159265358979323846;
double clampd (double x, double a, double b) { return x < a ? a : (x > b ? b : x); }

// A repeatable pseudo-random harmonic mix for wavetables (an illustration, not the real table).
struct Harmonics
{
    double amp[9], ph[9], norm = 1;
    explicit Harmonics (int seed)
    {
        uint32_t s = (uint32_t) seed * 9301u + 49297u;
        auto rnd = [&s] { s = (s * 9301u + 49297u) % 233280u; return s / 233280.0; };
        for (int k = 0; k < 9; ++k)
        {
            amp[k] = rnd() * 1.2 / std::pow (k + 1.0, 0.6 + rnd() * 0.6);
            ph[k] = rnd() * 6.28;
        }
        double mx = 1e-9;
        for (int i = 0; i < 200; ++i)
            mx = std::max (mx, std::abs (raw (i / 200.0, 0)));
        norm = mx;
    }
    double raw (double t, double shape) const
    {
        double a = 0;
        for (int k = 0; k < 9; ++k)
            a += amp[k] * std::sin (2 * kPi * (k + 1) * t + ph[k] + shape * k * 0.5);
        return a;
    }
    double at (double t, double shape) const { return raw (t, shape) / norm * 0.9; }
};
} // namespace

int GraphBase::v (const std::string& id) const
{
    const int i = indexOfId (id);
    return i < 0 ? 0 : params.value ((size_t) i);
}

void GraphBase::refresh()
{
    auto now = inputs();
    if (now != last)
    {
        last = std::move (now);
        repaint();
    }
}

void GraphBase::paint (juce::Graphics& g)
{
    using namespace theme;
    auto r = getLocalBounds().toFloat();
    g.setColour (well);
    g.fillRect (r);
    g.setColour (juce::Colour (0xff151a20));
    for (int i = 1; i < 8; ++i)
        g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 8), r.getY(), r.getBottom());
    for (int j = 1; j < 4; ++j)
        g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * j / 4), r.getX(), r.getRight());
    g.saveState();
    g.reduceClipRegion (getLocalBounds());
    draw (g, r.reduced (4.0f, 6.0f));
    g.restoreState();
    g.setColour (juce::Colours::black);
    g.drawRect (getLocalBounds());
}

void GraphBase::strokeFilled (juce::Graphics& g, const juce::Path& p, juce::Rectangle<float> r, float baseline)
{
    juce::Path fill (p);
    fill.lineTo (r.getRight(), baseline);
    fill.lineTo (r.getX(), baseline);
    fill.closeSubPath();
    g.setColour (theme::accent.withAlpha (0.15f));
    g.fillPath (fill);
    g.setColour (theme::accent);
    g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
}

// ---- oscillator ----

std::vector<int> WaveGraph::inputs() const { return { v (k + "wave"), v (k + "wavetable"), v (k + "shape") }; }

void WaveGraph::draw (juce::Graphics& g, juce::Rectangle<float> r)
{
    const int wave = v (k + "wave");
    const double s = (v (k + "shape") - 64) / 64.0;
    Harmonics h (v (k + "wavetable"));
    auto f = [&] (double t) -> double
    {
        switch (wave)
        {
            case 0: return std::sin (2 * kPi * t + s * 1.6 * std::sin (2 * kPi * t));
            case 1:
            {
                const double p = 0.5 + 0.42 * s;
                t = std::fmod (t + 0.25, 1.0);
                return t < p ? -1 + 2 * t / p : 1 - 2 * (t - p) / (1 - p);
            }
            case 2:
            {
                const double x = 1 - 2 * t;
                return (x < 0 ? -1 : 1) * std::pow (std::abs (x), 1 + s * 0.7);
            }
            case 3: return t < 0.5 + 0.44 * s ? 0.85 : -0.85;
            default: return h.at (t, s);
        }
    };
    juce::Path p;
    const float mid = r.getCentreY();
    for (int i = 0; i <= 240; ++i)
    {
        const double x = i / 240.0;
        const float px = r.getX() + (float) x * r.getWidth();
        const float py = mid - (float) (f (std::fmod (x * 2, 1.0)) * r.getHeight() * 0.45);
        i == 0 ? p.startNewSubPath (px, py) : p.lineTo (px, py);
    }
    g.setColour (juce::Colour (0xff1f262e));
    g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
    strokeFilled (g, p, r, mid);
}

// ---- filter ----

std::vector<int> FilterGraph::inputs() const
{
    return { v ("flt_freq"), v ("flt_res"), v ("flt_shape"), v ("flt_slope") };
}

void FilterGraph::draw (juce::Graphics& g, juce::Rectangle<float> r)
{
    const double fc = 20 * std::pow (1000.0, v ("flt_freq") / 255.0);
    const double q = 0.6 + v ("flt_res") / 127.0 * 9;
    const int shape = v ("flt_shape");
    const bool steep = v ("flt_slope") == 1;
    const double top = 18, bot = -42;
    juce::Path p;
    for (int i = 0; i <= 200; ++i)
    {
        const double f = 20 * std::pow (1000.0, i / 200.0), x = f / fc;
        const double den = std::sqrt ((1 - x * x) * (1 - x * x) + (x / q) * (x / q));
        double m = shape == 0 ? 1 / den : (shape == 1 ? (x / q) / den * q * 0.7 : (x * x) / den);
        if (steep)
            m *= m;
        const double db = clampd (20 * std::log10 (m + 1e-9), bot, top);
        const float px = r.getX() + r.getWidth() * i / 200.0f;
        const float py = r.getY() + (float) ((top - db) / (top - bot)) * r.getHeight();
        i == 0 ? p.startNewSubPath (px, py) : p.lineTo (px, py);
    }
    strokeFilled (g, p, r, r.getBottom());
}

// ---- envelope ----

std::vector<int> EnvGraph::inputs() const
{
    return { v (k + "attack"), v (k + "decay"), v (k + "sustain"), v (k + "release") };
}

void EnvGraph::draw (juce::Graphics& g, juce::Rectangle<float> r)
{
    auto seg = [] (int val) { return 4.0 + std::pow (val / 127.0, 1.4) * 86.0; };
    double a = seg (v (k + "attack")), d = seg (v (k + "decay")), s = 46, rel = seg (v (k + "release"));
    const double scale = std::min (1.0, (r.getWidth() - 4) / (a + d + s + rel));
    a *= scale, d *= scale, s *= scale, rel *= scale;
    const float y0 = r.getBottom(), y1 = r.getY();
    const float sy = y0 - (y0 - y1) * v (k + "sustain") / 127.0f;
    float x = r.getX();
    juce::Path p;
    p.startNewSubPath (x, y0);
    x += (float) a;
    p.lineTo (x, y1);
    const float xd = x;
    x += (float) d;
    p.quadraticTo (xd + (float) d * 0.15f, sy, x, sy);
    const float xs = x;
    x += (float) s;
    p.lineTo (x, sy);
    const float xr = x;
    x += (float) rel;
    p.quadraticTo (xr + (float) rel * 0.15f, y0, x, y0);
    g.setColour (theme::led.withAlpha (0.05f));
    g.fillRect (juce::Rectangle<float> (xs, r.getY() - 6, (float) s, r.getHeight() + 12));
    strokeFilled (g, p, r, y0);
    g.setColour (theme::ink3);
    g.setFont (theme::fontUi (9.0f, false));
    g.drawText ("SUSTAIN", juce::Rectangle<float> (xs + 3, r.getBottom() - 12, 60, 12), juce::Justification::left);
}

// ---- LFO ----

std::vector<int> LfoGraph::inputs() const
{
    const auto k = "lfo" + std::to_string (lfo) + "_";
    if (lfo > 2)
        return { lfo, v (k + "phase"), v (k + "slew"), v (k + "fade") };
    return { lfo, v (k + "wave"), v (k + "range"), v (k + "phase"), v (k + "slew"), v (k + "fade"), v (k + "sync") };
}

void LfoGraph::refresh()
{
    double ph = -1;
    if (lfo <= 2 && enabled && enabled() && feed.hasHost.load() && feed.playing.load())
    {
        const auto k = "lfo" + std::to_string (lfo) + "_";
        if (v (k + "range") == 2)  // Sync
            ph = lfoSyncPhase (feed.ppqNow (juce::Time::getMillisecondCounterHiRes()), v (k + "sync"));
    }
    if (std::abs (ph - playPhase) > 1e-4)
    {
        playPhase = ph;
        repaint();
    }
    GraphBase::refresh();
}

double LfoGraph::waveAt (double t) const
{
    const auto k = "lfo" + std::to_string (lfo) + "_";
    const int wave = lfo > 2 ? 0 : v (k + "wave");
    t = std::fmod (t + v (k + "phase") / 120.0, 1.0);
    switch (wave)
    {
        case 0: return t < 0.5 ? -1 + 4 * t : 3 - 4 * t;
        case 1: return 1 - 2 * t;
        case 2: return t < 0.5 ? 1 : -1;
        default:
        {
            static const double sh[] = { 0.6, -0.3, 0.9, -0.8, 0.1, -0.5, 0.4, -0.1 };
            return sh[(int) (t * 8) % 8];
        }
    }
}

void LfoGraph::draw (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto k = "lfo" + std::to_string (lfo) + "_";
    const float mid = r.getCentreY(), amp = r.getHeight() * 0.42f;
    const double slew = v (k + "slew") / 140.0, fade = v (k + "fade") / 127.0;
    juce::Path p;
    double prev = 0;
    constexpr int cycles = 2;
    for (int i = 0; i <= 240; ++i)
    {
        const double x = i / 240.0;
        double y = waveAt (std::fmod (x * cycles, 1.0));
        if (i > 0 && slew > 0)
            y = prev + (y - prev) * (1 - slew);
        prev = y;
        const double f = fade > 0 ? std::min (1.0, x / (fade * 0.9)) : 1.0;
        const float px = r.getX() + (float) x * r.getWidth(), py = mid - (float) (y * f) * amp;
        i == 0 ? p.startNewSubPath (px, py) : p.lineTo (px, py);
    }
    g.setColour (juce::Colour (0xff1f262e));
    g.drawHorizontalLine ((int) mid, r.getX(), r.getRight());
    strokeFilled (g, p, r, mid);

    if (playPhase >= 0)
    {
        // The drawing shows two cycles; mark the position in the first.
        const double y = waveAt (playPhase);
        const float px = r.getX() + (float) (playPhase / cycles) * r.getWidth();
        const float py = mid - (float) y * amp;
        g.setColour (theme::led.withAlpha (0.35f));
        g.drawVerticalLine ((int) px, r.getY() - 6, r.getBottom() + 6);
        g.setColour (theme::led);
        g.fillEllipse (px - 4, py - 4, 8, 8);
        g.setColour (theme::ink3);
        g.setFont (theme::fontUi (9.5f));
        g.drawText ("APPROX", r.withTrimmedTop (-4).removeFromTop (12).removeFromRight (60), juce::Justification::right);
    }
}

// ---- FX ----

std::vector<int> FxGraph::inputs() const
{
    switch (type)
    {
        case Type::Distortion: return { v ("dist_level") };
        case Type::Chorus: return { v ("cho_rate"), v ("cho_depth") };
        case Type::Delay: return { v ("dly_feedback") };
        case Type::Reverb: return { v ("rev_time") };
    }
    return {};
}

void FxGraph::draw (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::Path p;
    const float mid = r.getCentreY();
    switch (type)
    {
        case Type::Distortion:
        {
            const double d = v ("dist_level") / 127.0, k = 1 + d * 8;
            for (int i = 0; i <= 60; ++i)
            {
                const double x = i / 30.0 - 1, y = std::tanh (x * k) / std::tanh (k);
                const float px = r.getX() + r.getWidth() * i / 60.0f, py = mid - (float) y * r.getHeight() * 0.42f;
                i == 0 ? p.startNewSubPath (px, py) : p.lineTo (px, py);
            }
            g.setColour (theme::accent);
            g.strokePath (p, juce::PathStrokeType (1.6f));
            return;
        }
        case Type::Chorus:
        {
            const double rate = 1 + v ("cho_rate") / 32.0, depth = 0.2 + v ("cho_depth") / 160.0;
            for (int i = 0; i <= 120; ++i)
            {
                const double x = i / 120.0;
                const double y = std::sin (x * kPi * 2 * rate) * depth + std::sin (x * kPi * 9) * 0.15;
                const float px = r.getX() + (float) x * r.getWidth(), py = mid - (float) y * r.getHeight() * 0.5f;
                i == 0 ? p.startNewSubPath (px, py) : p.lineTo (px, py);
            }
            g.setColour (theme::accent);
            g.strokePath (p, juce::PathStrokeType (1.6f));
            return;
        }
        case Type::Delay:
        {
            const double fb = v ("dly_feedback") / 127.0;
            g.setColour (theme::accent);
            for (int k = 0; k < 6; ++k)
            {
                const float h = r.getHeight() * 0.9f * (float) std::pow (0.25 + fb * 0.7, k);
                const float x = r.getX() + 6 + k * r.getWidth() / 6.0f;
                g.setOpacity (1.0f - k * 0.12f);
                g.fillRect (x, r.getBottom() - h, 6.0f, h);
            }
            g.setOpacity (1.0f);
            return;
        }
        case Type::Reverb:
        {
            const double t = v ("rev_time") / 127.0;
            p.startNewSubPath (r.getX(), r.getBottom());
            p.lineTo (r.getX() + 6, r.getY());
            for (int i = 6; i <= 120; ++i)
            {
                const float px = r.getX() + r.getWidth() * i / 120.0f;
                const float py = r.getBottom() - r.getHeight() * (float) std::exp (-(i - 6) / (10 + t * 70));
                p.lineTo (px, py);
            }
            strokeFilled (g, p, r, r.getBottom());
            return;
        }
    }
}
