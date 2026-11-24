/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CLUXPREVIEWRENDERER_H
#define CLUXPREVIEWRENDERER_H

#include <QObject>
#include <QElapsedTimer>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include <memory>
#include <vector>

class CLuxPattern;
class CLuxStaticPattern;

// Local stand-in for the C-Lux engine's tick loop: it advances the same pattern stack the
// server holds, plays the timelines of the scenes switched on, and composites frames like
// the server's blend() - the solid color layer under the enabled patterns, each by its own
// blend mode - so preview mode can render from local state instead of following the live
// frame feed. Cross-dissolves are not reproduced: a local scene change or pattern toggle
// switches at once to its final state rather than easing over the server's
// sceneTransition.
class CLuxPreviewRenderer : public QObject {
    Q_OBJECT

public:
    explicit CLuxPreviewRenderer(QObject* parent = nullptr);
    // Defined in the .cpp, where the pattern classes are complete: destroying the
    // unique_ptr members needs their full definitions.
    ~CLuxPreviewRenderer() override;

    // How many lights the ring has; pattern instances are (re)built when it changes.
    void setLightCount(int nLights);
    // The serialized /patterns entries, in stack order (bottom to top).
    void setPatterns(const QVariantList& serialized);
    // The solid color layer the server composites under the patterns. Color changes ease
    // over the server's solidColorTransition; enabling or disabling switches at once.
    void setSolidColor(int r, int g, int b, double a, bool enabled);
    // The timelines of the scenes switched on, as /scenes/timelines entries: {scene,
    // timeline: {duration, loop, tracks}, time, playing}. A scene seen for the first time
    // (or any scene while the renderer is stopped) takes its clock from the entry, with
    // time 0 and playing when it carries none; running clocks keep going, and scenes no
    // longer listed drop theirs.
    void setTimelines(const QVariantList& timelines);
    // Forget every timeline clock, so the next setTimelines() starts them over - what the
    // server does when a scene replaces the whole stack.
    void restartTimelines();

    // Start or stop ticking at the server's tick rate (~30 fps).
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();

Q_SIGNALS:
    // nLights*3 ints (r, g, b per light): the unmasked composite of the current stack.
    void frameReady(const QVariantList& frame);

private:
    // Rebuild the pattern instances to match m_serialized and m_nLights, keeping running
    // patterns in place when they keep their name, as the server's stacks do.
    void rebuild();
    void onTick();

    // shared/timeline.ts: a stretch during which a pattern is lit, easing in and out.
    struct Clip {
        double start = 0;
        double end = 0;
        double fadeIn = 0;
        double fadeOut = 0;
    };
    // A scene's running timeline (engine.ts's Playback): its own clock plus the clips of
    // every pattern it switches, by pattern name.
    struct Playback {
        double duration = 0;
        bool loop = false;
        QHash<QString, QVector<Clip>> tracks;
        double time = 0;
        bool playing = true;
    };
    // engine.ts's advancePlaybacks() and updateGates(): move every clock on, then light each
    // pattern as its timelines have it (the brightest where several time the same one).
    void advancePlaybacks(double dt);
    void updateGates();

    QTimer m_timer;
    QElapsedTimer m_elapsed;

    int m_nLights = 0;
    QVariantList m_serialized;
    // std::vector: QList requires copyable elements and cannot hold unique_ptr.
    std::vector<std::unique_ptr<CLuxPattern>> m_patterns;
    std::unique_ptr<CLuxStaticPattern> m_solidColor;
    QHash<QString, Playback> m_playbacks;

    int m_solidColorValue[3] = {0, 0, 0};
    double m_solidAlpha = 1.0;
    bool m_solidEnabled = false;
    // Pattern types without a local implementation, warned about once each.
    QSet<QString> m_unsupportedTypes;
};

#endif // CLUXPREVIEWRENDERER_H
