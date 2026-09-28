/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cluxpatterns.h"

#include <algorithm>
#include <cmath>

namespace {

// 2*pi without relying on M_PI, which MSVC does not define by default.
constexpr double kTwoPi = 6.2831853071795864769;

} // namespace

LuxColor luxHsvToRgb(double h, double s, double v) {
    h = std::fmod(h, 360.0);
    if (h < 0)
        h += 360.0;
    const double c = v * s;
    const double x = c * (1.0 - std::fabs(std::fmod(h / 60.0, 2.0) - 1.0));
    const double m = v - c;

    double r = 0;
    double g = 0;
    double b = 0;
    if (h < 60) {
        r = c;
        g = x;
    } else if (h < 120) {
        r = x;
        g = c;
    } else if (h < 180) {
        g = c;
        b = x;
    } else if (h < 240) {
        g = x;
        b = c;
    } else if (h < 300) {
        r = x;
        b = c;
    } else {
        r = c;
        b = x;
    }

    return {qRound((r + m) * 255), qRound((g + m) * 255), qRound((b + m) * 255)};
}

CLuxPattern::CLuxPattern(const QString& name, const QString& type, bool enabled, double opacity)
    : m_name(name), m_type(type), m_enabled(enabled), m_opacity(opacity) {
}

void CLuxPattern::resize(int nLights) {
    m_state.clear();
    m_state.resize(qMax(0, nLights));
}

void CLuxPattern::updateFrom(const QVariantMap& params) {
    if (params == m_params)
        return;
    m_params = params;
    m_enabled = params.value(QStringLiteral("enabled"), true).toBool();
    m_opacity = params.value(QStringLiteral("opacity"), 1.0).toDouble();
    applyParams(params);
}

void CLuxPattern::composite(QVector<double>& accum) const {
    for (int i = 0; i < m_state.size(); ++i) {
        const double alpha = m_state[i].a * m_opacity;
        const int dst = i * 3;
        accum[dst] = m_state[i].r * alpha + accum[dst] * (1.0 - alpha);
        accum[dst + 1] = m_state[i].g * alpha + accum[dst + 1] * (1.0 - alpha);
        accum[dst + 2] = m_state[i].b * alpha + accum[dst + 2] * (1.0 - alpha);
    }
}

CLuxStaticPattern::CLuxStaticPattern(const QString& name, bool enabled, double opacity)
    : CLuxPattern(name, QStringLiteral("StaticPattern"), enabled, opacity) {
}

void CLuxStaticPattern::applyParams(const QVariantMap& params) {
    const QVariantMap color = params.value(QStringLiteral("color")).toMap();
    if (!color.isEmpty()) {
        m_r = color.value(QStringLiteral("r"), m_r).toInt();
        m_g = color.value(QStringLiteral("g"), m_g).toInt();
        m_b = color.value(QStringLiteral("b"), m_b).toInt();
    }
    // Fractions of the ring bounding the lit arc. Older saved scenes predate them, so they
    // are optional and fall back to the full ring.
    if (params.contains(QStringLiteral("start")))
        m_start = params.value(QStringLiteral("start")).toDouble();
    if (params.contains(QStringLiteral("end")))
        m_end = params.value(QStringLiteral("end")).toDouble();

    m_fadeDuration = 0;
    m_fadeElapsed = 0;
    paint(m_r, m_g, m_b);
}

void CLuxStaticPattern::setColor(int r, int g, int b) {
    m_r = r;
    m_g = g;
    m_b = b;
    m_fadeDuration = 0;
    m_fadeElapsed = 0;
    paint(m_r, m_g, m_b);
}

void CLuxStaticPattern::fadeTo(const LuxColor& color, double duration) {
    if (duration <= 0) {
        setColor(color.r, color.g, color.b);
        return;
    }

    m_fadeFrom = currentColor();
    m_r = color.r;
    m_g = color.g;
    m_b = color.b;
    m_fadeDuration = duration;
    m_fadeElapsed = 0;
}

LuxColor CLuxStaticPattern::currentColor() const {
    const double t = progress();
    return {qRound(m_fadeFrom.r + (m_r - m_fadeFrom.r) * t),
            qRound(m_fadeFrom.g + (m_g - m_fadeFrom.g) * t),
            qRound(m_fadeFrom.b + (m_b - m_fadeFrom.b) * t)};
}

bool CLuxStaticPattern::fading() const {
    return m_fadeElapsed < m_fadeDuration;
}

double CLuxStaticPattern::progress() const {
    if (m_fadeDuration <= 0)
        return 1.0;
    return std::min(1.0, m_fadeElapsed / m_fadeDuration);
}

void CLuxStaticPattern::tick(double dt) {
    if (!fading())
        return;

    m_fadeElapsed += dt;
    const LuxColor color = currentColor();
    paint(color.r, color.g, color.b);
}

void CLuxStaticPattern::paint(int r, int g, int b) {
    const int n = m_state.size();
    for (int i = 0; i < n; ++i) {
        Light& light = m_state[i];
        light.r = r;
        light.g = g;
        light.b = b;
        light.a = inRange(i / double(n)) ? 1.0 : 0.0;
    }
}

bool CLuxStaticPattern::inRange(double position) const {
    if (m_start == m_end)
        return true;
    if (m_start < m_end)
        return position >= m_start && position < m_end;
    return position >= m_start || position < m_end;
}

CLuxSineWavePattern::CLuxSineWavePattern(const QString& name, bool enabled, double opacity)
    : CLuxPattern(name, QStringLiteral("SineWave"), enabled, opacity) {
}

void CLuxSineWavePattern::applyParams(const QVariantMap& params) {
    const QVariantMap color = params.value(QStringLiteral("color")).toMap();
    if (!color.isEmpty()) {
        m_r = color.value(QStringLiteral("r"), m_r).toInt();
        m_g = color.value(QStringLiteral("g"), m_g).toInt();
        m_b = color.value(QStringLiteral("b"), m_b).toInt();
    }
    // Wavelength is a fraction of the ring and speed is in full turns per second.
    if (params.contains(QStringLiteral("wavelength")))
        m_wavelength = params.value(QStringLiteral("wavelength")).toDouble();
    if (params.contains(QStringLiteral("speed")))
        m_speed = params.value(QStringLiteral("speed")).toDouble();
    if (params.contains(QStringLiteral("min")))
        m_min = params.value(QStringLiteral("min")).toDouble();
    if (params.contains(QStringLiteral("max")))
        m_max = params.value(QStringLiteral("max")).toDouble();

    render();
}

void CLuxSineWavePattern::tick(double dt) {
    const int n = m_state.size();
    if (n == 0)
        return;

    // Wrap the phase to the ring, like the server's ((x % n) + n) % n.
    m_phase = std::fmod(std::fmod(m_phase + m_speed * n * dt, double(n)) + n, double(n));
    render();
}

void CLuxSineWavePattern::render() {
    const int n = m_state.size();
    if (n == 0)
        return;

    // Snap to a whole number of waves around the ring, otherwise the wave is cut off where
    // the last light meets the first one.
    const double cycles = m_wavelength > 0 ? std::max(1.0, std::round(1.0 / m_wavelength)) : 1.0;
    const double wl = n / cycles;
    for (int i = 0; i < n; ++i) {
        const double wave = 0.5 + 0.5 * std::sin(kTwoPi * (i - m_phase) / wl);
        Light& light = m_state[i];
        light.r = m_r;
        light.g = m_g;
        light.b = m_b;
        light.a = m_min + (m_max - m_min) * wave;
    }
}

CLuxGradientPattern::CLuxGradientPattern(const QString& name, bool enabled, double opacity)
    : CLuxPattern(name, QStringLiteral("Gradient"), enabled, opacity) {
}

void CLuxGradientPattern::applyParams(const QVariantMap& params) {
    const QVariantMap color = params.value(QStringLiteral("color")).toMap();
    if (!color.isEmpty()) {
        m_r = color.value(QStringLiteral("r"), m_r).toInt();
        m_g = color.value(QStringLiteral("g"), m_g).toInt();
        m_b = color.value(QStringLiteral("b"), m_b).toInt();
    }
    const QVariantMap color2 = params.value(QStringLiteral("color2")).toMap();
    if (!color2.isEmpty()) {
        m_color2.r = color2.value(QStringLiteral("r"), m_color2.r).toInt();
        m_color2.g = color2.value(QStringLiteral("g"), m_color2.g).toInt();
        m_color2.b = color2.value(QStringLiteral("b"), m_color2.b).toInt();
    }
    if (params.contains(QStringLiteral("speed")))
        m_speed = params.value(QStringLiteral("speed")).toDouble();

    render();
}

void CLuxGradientPattern::tick(double dt) {
    // The phase grows without bound, exactly like the server's.
    m_phase += m_speed * dt;
    render();
}

void CLuxGradientPattern::render() {
    const int n = m_state.size();
    if (n == 0)
        return;

    for (int i = 0; i < n; ++i) {
        // Cosine blend so the two colors meet seamlessly around the ring.
        const double t = 0.5 - 0.5 * std::cos(kTwoPi * (i / double(n) + m_phase));
        Light& light = m_state[i];
        light.r = m_r + (m_color2.r - m_r) * t;
        light.g = m_g + (m_color2.g - m_g) * t;
        light.b = m_b + (m_color2.b - m_b) * t;
        light.a = 1.0;
    }
}

CLuxRainbowPattern::CLuxRainbowPattern(const QString& name, bool enabled, double opacity)
    : CLuxPattern(name, QStringLiteral("Rainbow"), enabled, opacity) {
}

void CLuxRainbowPattern::applyParams(const QVariantMap& params) {
    if (params.contains(QStringLiteral("speed")))
        m_speed = params.value(QStringLiteral("speed")).toDouble();
    if (params.contains(QStringLiteral("saturation")))
        m_saturation = params.value(QStringLiteral("saturation")).toDouble();
    if (params.contains(QStringLiteral("value")))
        m_value = params.value(QStringLiteral("value")).toDouble();
    if (params.contains(QStringLiteral("cycles")))
        m_cycles = params.value(QStringLiteral("cycles")).toDouble();

    render();
}

void CLuxRainbowPattern::tick(double dt) {
    m_phase += m_speed * dt;
    render();
}

void CLuxRainbowPattern::render() {
    const int n = m_state.size();
    if (n == 0)
        return;

    for (int i = 0; i < n; ++i) {
        const double hue = (i / double(n)) * 360.0 * m_cycles + m_phase;
        const LuxColor color = luxHsvToRgb(hue, m_saturation, m_value);
        Light& light = m_state[i];
        light.r = color.r;
        light.g = color.g;
        light.b = color.b;
        light.a = 1.0;
    }
}

std::unique_ptr<CLuxPattern> createLuxPattern(const QVariantMap& params, int nLights) {
    const QString type = params.value(QStringLiteral("type")).toString();
    const QString name = params.value(QStringLiteral("name")).toString();
    const bool enabled = params.value(QStringLiteral("enabled"), true).toBool();
    const double opacity = params.value(QStringLiteral("opacity"), 1.0).toDouble();

    std::unique_ptr<CLuxPattern> pattern;
    if (type == QLatin1String("StaticPattern"))
        pattern = std::make_unique<CLuxStaticPattern>(name, enabled, opacity);
    else if (type == QLatin1String("SineWave"))
        pattern = std::make_unique<CLuxSineWavePattern>(name, enabled, opacity);
    else if (type == QLatin1String("Gradient"))
        pattern = std::make_unique<CLuxGradientPattern>(name, enabled, opacity);
    else if (type == QLatin1String("Rainbow"))
        pattern = std::make_unique<CLuxRainbowPattern>(name, enabled, opacity);
    else
        return nullptr; // not implemented in the local preview yet

    // Allocate the state first, then apply the parameters - like the TypeScript
    // constructors' super(props) followed by set(props).
    pattern->resize(nLights);
    pattern->updateFrom(params);
    return pattern;
}
