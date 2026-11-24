/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cluxpreviewrenderer.h"
#include "cluxpatterns.h"

#include <QVector>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

// The server's tick rate (config.json: server.tickRate) and its solid color transition,
// kept in sync with the C-Lux defaults so the preview moves at the same pace.
constexpr int kTickIntervalMs = 1000 / 30;
constexpr double kSolidColorTransition = 1.0;

} // namespace

CLuxPreviewRenderer::CLuxPreviewRenderer(QObject* parent)
    : QObject(parent), m_timer(this) {
    connect(&m_timer, &QTimer::timeout, this, &CLuxPreviewRenderer::onTick);
}

// After the includes above, so CLuxPattern/CLuxStaticPattern are complete here.
CLuxPreviewRenderer::~CLuxPreviewRenderer() = default;

void CLuxPreviewRenderer::setLightCount(int nLights) {
    const int count = qMax(0, nLights);
    if (count == m_nLights)
        return;

    m_nLights = count;
    // Instances are sized to the old ring, so rebuild them all.
    m_patterns.clear();
    rebuild();
}

void CLuxPreviewRenderer::setPatterns(const QVariantList& serialized) {
    if (serialized == m_serialized)
        return;

    m_serialized = serialized;
    rebuild();
}

void CLuxPreviewRenderer::setSolidColor(int r, int g, int b, double a, bool enabled) {
    r = qBound(0, r, 255);
    g = qBound(0, g, 255);
    b = qBound(0, b, 255);
    a = qBound(0.0, a, 1.0);
    const bool colorChanged = m_solidColorValue[0] != r || m_solidColorValue[1] != g
                              || m_solidColorValue[2] != b || m_solidAlpha != a;

    m_solidColorValue[0] = r;
    m_solidColorValue[1] = g;
    m_solidColorValue[2] = b;
    m_solidAlpha = a;
    m_solidEnabled = enabled;

    // The server eases solid color changes over its transition time.
    if (m_solidColor && colorChanged)
        m_solidColor->fadeTo({r, g, b, a}, kSolidColorTransition);
}

void CLuxPreviewRenderer::setTimelines(const QVariantList& timelines) {
    QHash<QString, Playback> next;
    for (const QVariant& v : timelines) {
        const QVariantMap entry = v.toMap();
        const QString scene = entry.value(QStringLiteral("scene")).toString();
        const QVariantMap timeline = entry.value(QStringLiteral("timeline")).toMap();

        Playback playback;
        playback.duration = timeline.value(QStringLiteral("duration")).toDouble();
        playback.loop = timeline.value(QStringLiteral("loop")).toBool();
        if (scene.isEmpty() || playback.duration <= 0)
            continue; // not a timeline the server would accept

        const QVariantMap tracks = timeline.value(QStringLiteral("tracks")).toMap();
        for (auto it = tracks.constBegin(); it != tracks.constEnd(); ++it) {
            QVector<Clip> clips;
            for (const QVariant& c : it.value().toList()) {
                const QVariantMap clip = c.toMap();
                clips.append({clip.value(QStringLiteral("start")).toDouble(),
                              clip.value(QStringLiteral("end")).toDouble(),
                              clip.value(QStringLiteral("fadeIn")).toDouble(),
                              clip.value(QStringLiteral("fadeOut")).toDouble()});
            }
            playback.tracks.insert(it.key(), clips);
        }

        const auto running = m_playbacks.constFind(scene);
        if (m_timer.isActive() && running != m_playbacks.constEnd()) {
            // A clock that is already running keeps its place, but follows timeline edits
            // the way the server's setTimeline() does: wrapped into the new length.
            playback.time = running->time;
            playback.playing = running->playing;
        } else {
            playback.time = entry.value(QStringLiteral("time"), 0.0).toDouble();
            playback.playing = entry.value(QStringLiteral("playing"), true).toBool();
        }
        playback.time = playback.loop
                            ? std::fmod(std::fmod(playback.time, playback.duration) + playback.duration,
                                        playback.duration)
                            : qBound(0.0, playback.time, playback.duration);
        next.insert(scene, playback);
    }
    m_playbacks = next;
    updateGates();
}

void CLuxPreviewRenderer::restartTimelines() {
    m_playbacks.clear();
}

void CLuxPreviewRenderer::advancePlaybacks(double dt) {
    for (Playback& playback : m_playbacks) {
        if (!playback.playing)
            continue;
        const double time = playback.time + dt;
        if (!playback.loop && time >= playback.duration)
            playback.playing = false;
        // wrapTime(): around when it loops, otherwise held at either end.
        playback.time = playback.loop
                            ? std::fmod(std::fmod(time, playback.duration) + playback.duration,
                                        playback.duration)
                            : qBound(0.0, time, playback.duration);
    }
}

void CLuxPreviewRenderer::updateGates() {
    for (auto& p : m_patterns) {
        bool timed = false;
        double gate = 0;
        for (const Playback& playback : std::as_const(m_playbacks)) {
            const auto track = playback.tracks.constFind(p->name());
            if (track == playback.tracks.constEnd())
                continue;
            timed = true;
            // gateAt(): how lit the track is now; overlapping clips take the brighter one.
            for (const Clip& clip : *track) {
                if (playback.time < clip.start || playback.time > clip.end)
                    continue;
                const double rise = clip.fadeIn > 0 ? (playback.time - clip.start) / clip.fadeIn : 1.0;
                const double fall = clip.fadeOut > 0 ? (clip.end - playback.time) / clip.fadeOut : 1.0;
                gate = std::max(gate, std::min({1.0, rise, fall}));
            }
        }
        // Patterns without a track stay fully lit.
        p->setTimelineGate(timed ? gate : 1.0);
    }
}

void CLuxPreviewRenderer::start() {
    if (m_timer.isActive())
        return;

    m_elapsed.start();
    m_timer.start(kTickIntervalMs);
}

void CLuxPreviewRenderer::stop() {
    m_timer.stop();
}

void CLuxPreviewRenderer::rebuild() {
    if (m_nLights <= 0) {
        m_patterns.clear();
        m_solidColor.reset();
        return;
    }

    // The solid color layer, kept out of the pattern list like on the server.
    if (!m_solidColor || m_solidColor->lightCount() != m_nLights) {
        auto solid = std::make_unique<CLuxStaticPattern>(QStringLiteral("solid-color"), true, 1.0);
        solid->resize(m_nLights);
        solid->setColor({m_solidColorValue[0], m_solidColorValue[1], m_solidColorValue[2], m_solidAlpha});
        m_solidColor = std::move(solid);
    }

    std::vector<std::unique_ptr<CLuxPattern>> next;
    next.reserve(static_cast<size_t>(m_serialized.size()));
    for (const QVariant& v : m_serialized) {
        const QVariantMap params = v.toMap();
        const QString name = params.value(QStringLiteral("name")).toString();
        const QString type = params.value(QStringLiteral("type")).toString();

        std::unique_ptr<CLuxPattern> pattern;
        // Reuse the running instance when a pattern keeps its place in the stack, as the
        // server does: applying a scene leaves already-running patterns untouched.
        for (auto& existing : m_patterns) {
            if (!existing)
                continue; // already claimed by an earlier entry this pass
            if (existing->name() == name && existing->type() == type) {
                pattern = std::move(existing);
                break;
            }
        }

        if (!pattern) {
            pattern = createLuxPattern(params, m_nLights);
            if (!pattern && !m_unsupportedTypes.contains(type)) {
                m_unsupportedTypes.insert(type);
                qWarning() << "CLuxPreviewRenderer: no local implementation for pattern type"
                           << type << "- it renders dark in preview mode";
            }
        } else {
            // No-op when the parameters are unchanged, like the server's update().
            pattern->updateFrom(params);
        }

        if (pattern)
            next.push_back(std::move(pattern));
    }
    m_patterns = std::move(next);
    updateGates();
}

void CLuxPreviewRenderer::onTick() {
    // Measured frame time, like the server's Date.now() delta.
    const double dt = m_elapsed.restart() / 1000.0;

    // The server's tick() without its scene dissolves: move the timeline clocks on and
    // gate the patterns by them, advance every enabled pattern its timeline has not
    // switched off (in stack order), then the solid color layer.
    advancePlaybacks(dt);
    updateGates();
    for (auto& p : m_patterns) {
        if (p->enabled() && p->timelineGate() > 0)
            p->tick(dt);
    }
    if (m_solidColor)
        m_solidColor->tick(dt);

    // compositeStack(): the solid color layer under the enabled patterns, each by its own
    // blend mode.
    QVector<double> accum(m_nLights * 3, 0.0);
    if (m_solidEnabled && m_solidColor)
        m_solidColor->composite(accum);
    for (auto& p : m_patterns) {
        if (p->enabled() && p->timelineGate() > 0)
            p->composite(accum);
    }

    QVariantList frame;
    frame.reserve(m_nLights * 3);
    for (double v : accum)
        frame.append(qRound(v));
    Q_EMIT frameReady(frame);
}
