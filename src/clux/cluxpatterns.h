/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CLUXPATTERNS_H
#define CLUXPATTERNS_H

#include <QVector>
#include <QString>
#include <QVariantMap>

#include <memory>

// C++ port of the C-Lux shared pattern classes (shared/patterns/*.ts), just enough for the
// local preview renderer to reproduce what the server would composite. Each class mirrors
// its TypeScript counterpart - same fields, defaults and tick/render math - so a pattern
// stored by the server renders identically here.

struct LuxColor {
    int r = 0;
    int g = 0;
    int b = 0;
    // Opacity in [0, 1]; colors stored before C-Lux had transparency omit it and are opaque.
    double a = 1.0;
};

// Reads a serialized {r, g, b, a} color, keeping the fallback's value for every channel the
// map leaves out - like the TypeScript set()'s `r ?? this.r`.
LuxColor luxColorFrom(const QVariantMap& map, const LuxColor& fallback);

// Port of shared/patterns/pattern.ts's hsvToRgb(): HSV (h in degrees, s and v in [0, 1])
// to 8-bit RGB.
LuxColor luxHsvToRgb(double h, double s, double v);

// How a layer is composited onto the ones beneath it, pattern.ts's BlendMode.
enum class LuxBlendMode {
    Alpha = 0,
    Additive = 1,
    Multiply = 2,
    Subtract = 3
};

// Base class mirroring the abstract Pattern: per-light RGBA state plus the shared enabled,
// opacity and blend mode fields. composite() is the server's blend of one layer onto an
// accumulator (engine.ts's composite()).
class CLuxPattern {
public:
    // Internal per-light color carrying an alpha channel in [0, 1] used for blending.
    struct Light {
        double r = 0;
        double g = 0;
        double b = 0;
        double a = 0;
    };

    CLuxPattern(const QString& name, const QString& type, bool enabled, double opacity);
    virtual ~CLuxPattern() = default;

    // (Re)allocate the per-light state, zeroed like the TypeScript constructor does.
    void resize(int nLights);
    int lightCount() const { return m_state.size(); }

    QString name() const { return m_name; }
    QString type() const { return m_type; }
    bool enabled() const { return m_enabled; }
    double opacity() const { return m_opacity; }
    LuxBlendMode blendMode() const { return m_blendMode; }

    // How lit a scene timeline has the pattern, from 0 to 1 (Pattern.timelineGate). A
    // pattern gated to 0 is neither ticked nor blended, like on the server.
    double timelineGate() const { return m_timelineGate; }
    void setTimelineGate(double gate) { m_timelineGate = gate; }

    // Apply a serialized parameter set (a /patterns entry). No-op when nothing changed,
    // mirroring the server's update(): the shared fields plus the subclass's own through
    // applyParams().
    void updateFrom(const QVariantMap& params);

    virtual void tick(double dt) = 0;

    // Composite this pattern onto accum (nLights*3 doubles) by its blend mode, with alpha
    // scaled by opacity and the timeline gate exactly like Pattern.data() feeds engine.ts's
    // composite().
    void composite(QVector<double>& accum) const;

protected:
    // The subclass fields plus an immediate re-render, the counterpart of set().
    virtual void applyParams(const QVariantMap& params) = 0;

    QVector<Light> m_state;

private:
    QString m_name;
    QString m_type;
    bool m_enabled;
    double m_opacity;
    LuxBlendMode m_blendMode = LuxBlendMode::Alpha;
    double m_timelineGate = 1.0;
    // Last applied parameter set, so updateFrom() can skip unchanged patterns.
    QVariantMap m_params;
};

// Port of shared/patterns/static.ts: one color over a (possibly wrapping) arc of the ring,
// with fadeTo() easing between colors. The renderer also uses it for the server's solid
// color layer, which lives outside the pattern list.
class CLuxStaticPattern : public CLuxPattern {
public:
    CLuxStaticPattern(const QString& name, bool enabled, double opacity);

    // The counterpart of set({r,g,b,a}): switches immediately, cancelling any fade.
    void setColor(const LuxColor& color);
    // Ease from the color currently on the lights to `color` over `duration` seconds; a
    // non-positive duration switches immediately.
    void fadeTo(const LuxColor& color, double duration);

    void tick(double dt) override;

protected:
    void applyParams(const QVariantMap& params) override;

private:
    // The color currently on the lights, which differs from the configured one while a
    // fade started by fadeTo() is still running.
    LuxColor currentColor() const;
    bool fading() const;
    double progress() const;
    void paint(const LuxColor& color);
    // `position` is a light's place on the ring as a fraction. An empty range (start ==
    // end) covers everything, and a range whose end precedes its start wraps the seam.
    bool inRange(double position) const;

    LuxColor m_color{77, 171, 247};
    double m_start = 0;
    double m_end = 1;

    LuxColor m_fadeFrom{0, 0, 0};
    double m_fadeDuration = 0;
    double m_fadeElapsed = 0;
};

// Port of shared/patterns/sine-wave.ts: a travelling sine wave modulating the alpha, scaled
// by the color's own alpha.
class CLuxSineWavePattern : public CLuxPattern {
public:
    CLuxSineWavePattern(const QString& name, bool enabled, double opacity);

    void tick(double dt) override;

protected:
    void applyParams(const QVariantMap& params) override;

private:
    void render();

    LuxColor m_color{77, 171, 247};
    double m_wavelength = 0.14;
    double m_speed = 0.07;
    double m_min = 0;
    double m_max = 1;

    // Travelling-wave phase, measured in lights.
    double m_phase = 0;
};

// Port of shared/patterns/gradient.ts: a palette of colors spread evenly around the ring,
// each blending into the next with a cosine, drifting at `speed` cycles per second.
class CLuxGradientPattern : public CLuxPattern {
public:
    CLuxGradientPattern(const QString& name, bool enabled, double opacity);

    void tick(double dt) override;

protected:
    void applyParams(const QVariantMap& params) override;

private:
    void render();

    QVector<LuxColor> m_colors{LuxColor{77, 171, 247}, LuxColor{247, 77, 77}};
    double m_speed = 0.1;

    // Drift offset in cycles that slides the gradient around the ring.
    double m_phase = 0;
};

// Port of shared/patterns/rainbow.ts: a full HSV spectrum around the ring, scrolling at
// `speed` degrees per second.
class CLuxRainbowPattern : public CLuxPattern {
public:
    CLuxRainbowPattern(const QString& name, bool enabled, double opacity);

    void tick(double dt) override;

protected:
    void applyParams(const QVariantMap& params) override;

private:
    void render();

    double m_speed = 60;
    double m_saturation = 1;
    double m_value = 1;
    double m_cycles = 1;

    // Hue offset (in degrees) that scrolls the spectrum around the ring.
    double m_phase = 0;
};

// Build a pattern from its serialized /patterns entry, the counterpart of the server's
// patternFromParameters(). Returns nullptr for types the local preview does not implement
// yet; those simply contribute nothing to the frame.
std::unique_ptr<CLuxPattern> createLuxPattern(const QVariantMap& params, int nLights);

#endif // CLUXPATTERNS_H
