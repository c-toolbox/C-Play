/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "streamsettings.h"
#include "ytdlpmetadata.h"
#include "ytdlpresolver.h"

#include <sgct/sgct.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <algorithm>
#include <optional>

namespace {

// Upper bound for a single metadata fetch. Resolving a YouTube URL normally takes a few
// seconds; anything slower is reported as a failure so the UI does not spin forever.
const int kFetchTimeoutMs = 120000;

// [[test-extract:begin]] Everything between these markers is copied verbatim into the standalone
// test harness (builds/.../_test_rank_extracted.inc, see _test_rank_extract.py); keep it free of
// dependencies on anything outside this region.
// Short display name for a video codec string ("avc1.640028" -> "H.264").
QString shortVideoCodec(const QString& codec) {
    if (codec.startsWith(QLatin1String("avc1")) || codec == QLatin1String("h264"))
        return QStringLiteral("H.264");
    if (codec == QLatin1String("vp9") || codec.startsWith(QLatin1String("vp09")))
        return QStringLiteral("VP9");
    if (codec.startsWith(QLatin1String("av01")))
        return QStringLiteral("AV1");
    return codec.left(8);
}

// One candidate video format for the quality list.
struct VideoCandidate {
    int height = 0;
    double fps = 0.0;
    double tbr = 0.0;
    QString id;
    QString codec;
};

// Reduce the raw "formats" array to one entry per resolution/fps combination (the highest
// bitrate variant wins), sorted from highest to lowest resolution. Each result is a
// {id, label} map where id is the yt-dlp format id usable as the --format selector.
QVariantList buildVideoFormats(const QJsonArray& formats) {
    QMap<QString, VideoCandidate> bestByGroup;
    for (const QJsonValue& fv : formats) {
        const QJsonObject f = fv.toObject();
        const QString vcodec = f.value(QStringLiteral("vcodec")).toString();
        if (vcodec.isEmpty() || vcodec == QLatin1String("none"))
            continue; // audio-only or unknown entry
        const int height = f.value(QStringLiteral("height")).toInt(0);
        if (height <= 0)
            continue;
        const QString id = f.value(QStringLiteral("format_id")).toString();
        if (id.isEmpty())
            continue;
        const double fps = f.value(QStringLiteral("fps")).toDouble(0.0);
        const double tbr = f.value(QStringLiteral("tbr")).toDouble(0.0);
        const QString key = QString::number(height) + QLatin1Char('|') + QString::number(fps, 'f', 0);
        auto it = bestByGroup.find(key);
        if (it == bestByGroup.end() || tbr > it->tbr) {
            VideoCandidate c;
            c.height = height;
            c.fps = fps;
            c.tbr = tbr;
            c.id = id;
            c.codec = vcodec;
            bestByGroup.insert(key, std::move(c));
        }
    }

    QVector<VideoCandidate> candidates;
    candidates.reserve(bestByGroup.size());
    for (const auto& it : bestByGroup)
        candidates.append(it);
    std::sort(candidates.begin(), candidates.end(), [](const VideoCandidate& a, const VideoCandidate& b) {
        if (a.height != b.height)
            return a.height > b.height;
        if (a.fps != b.fps)
            return a.fps > b.fps;
        return a.tbr > b.tbr;
    });

    QVariantList result;
    for (const VideoCandidate& c : candidates) {
        QString label = QStringLiteral("%1p").arg(c.height);
        if (c.fps > 0.0)
            label += QStringLiteral(" %1 fps").arg(static_cast<int>(qRound(c.fps)));
        label += QStringLiteral(" (%1)").arg(shortVideoCodec(c.codec));
        result.append(QVariantMap{{QStringLiteral("id"), c.id}, {QStringLiteral("label"), label}});
    }
    return result;
}


// All audio-only formats, in the order yt-dlp reports them. Each result is a {id, label} map
// where id is the format id merged into --format as "<video>+<audio>".
QVariantList buildAudioTracks(const QJsonArray& formats) {
    QVariantList result;
    for (const QJsonValue& fv : formats) {
        const QJsonObject f = fv.toObject();
        const QString vcodec = f.value(QStringLiteral("vcodec")).toString();
        const QString acodec = f.value(QStringLiteral("acodec")).toString();
        if (!vcodec.isEmpty() && vcodec != QLatin1String("none"))
            continue; // has a video stream - not an audio track on its own
        if (acodec.isEmpty() || acodec == QLatin1String("none"))
            continue; // no audio at all
        const QString id = f.value(QStringLiteral("format_id")).toString();
        if (id.isEmpty())
            continue;

        QString label = f.value(QStringLiteral("name")).toString();
        if (label.isEmpty()) {
            const QString lang = f.value(QStringLiteral("language")).toString();
            const QString note = f.value(QStringLiteral("format_note")).toString();
            if (!lang.isEmpty() && !note.isEmpty())
                label = lang + QStringLiteral(" (") + note + QStringLiteral(")");
            else if (!lang.isEmpty())
                label = lang;
            else if (!note.isEmpty())
                label = note; // e.g. "Default, low" / "Default, high"
            else
                label = QStringLiteral("Default");
        }
        result.append(QVariantMap{{QStringLiteral("id"), id}, {QStringLiteral("label"), label}});
    }
    return result;
}

// Human-readable bitrate ("~5000 kbps" / "~8.34 Mbps"); empty when unknown.
QString formatBitrate(double kbps) {
    if (kbps <= 0.0)
        return {};
    if (kbps >= 1000.0)
        return QStringLiteral("~%1 Mbps").arg(kbps / 1000.0, 0, 'f', 2);
    return QStringLiteral("~%1 kbps").arg(static_cast<int>(qRound(kbps)));
}

// ---------------------------------------------------------------------------
// Mirrors yt-dlp's default "best" format selection (yt_dlp/utils/_utils.py, FormatSorter) so the
// log can report what mpv's ytdl_hook will actually play. mpv runs yt-dlp with the selector
// "bv*+ba/b": the best video-only format plus the best audio-only format, or - when no such pair
// exists - the best combined format. Each part is ranked with yt-dlp's *effective* sort order,
// which FormatSorter.evaluate_params builds as: forced fields + priority fields (in default-tuple
// order) + the extractor-provided "_format_sort_fields" from the info dict + the full default
// tuple, deduplicated by first occurrence. YouTube supplies such a list, which is why e.g. "source"
// (source_preference) outranks "vcodec" there and can let a VP9 format beat AV1. Numeric fields
// prefer larger values; unknown numeric values sort last; the codec/protocol/extension fields use
// yt-dlp's ordered preference lists; a "field:limit" entry demotes values above the cap (the
// default "hdr:12" resolves to the list position of "(hdr)?12", so Dolby Vision is demoted). Note
// that "quality" is not constant: e.g. YouTube assigns it 3 to regular audio formats but only 2.5
// to their DRC variants, which is why plain opus beats the higher-bitrate DRC one.
// ---------------------------------------------------------------------------

// Stands in for yt-dlp's (-10, 0) rank that unknown numeric field values get (worst possible).
const double kUnknownRank = -1e9;

// Full preference lists from FormatSorter.settings; empty strings stand in for the '' and None
// entries, which never match (yt-dlp skips falsy list entries in its match loop). Earlier
// entries score higher: score = listLength - index.
const QStringList kVcodecOrder = {QStringLiteral("av0?1"), QStringLiteral("vp0?9\\.0?2"), QStringLiteral("vp0?9"),
                                  QStringLiteral("[hx]265|he?vc?"), QStringLiteral("[hx]264|avc"), QStringLiteral("vp0?8"),
                                  QStringLiteral("mp4v|h263"), QStringLiteral("theora"), QString(), QString(),
                                  QStringLiteral("none")}; // 11 entries, None at index 9
const QStringList kAcodecOrder = {QStringLiteral("[af]lac"), QStringLiteral("wav|aiff"), QStringLiteral("opus"),
                                  QStringLiteral("vorbis|ogg"), QStringLiteral("aac"), QStringLiteral("mp?4a?"),
                                  QStringLiteral("mp3"), QStringLiteral("ac-?4"), QStringLiteral("e-?a?c-?3"),
                                  QStringLiteral("ac-?3"), QStringLiteral("dts"), QString(), QString(),
                                  QStringLiteral("none")}; // 14 entries, None at index 12
const QStringList kHdrOrder = {QStringLiteral("dv"), QStringLiteral("(hdr)?12"), QStringLiteral("(hdr)?10\\+"),
                               QStringLiteral("(hdr)?10"), QStringLiteral("hlg"), QString(), QStringLiteral("sdr"),
                               QString()}; // 8 entries, None at index 7
const QStringList kProtoOrder = {QStringLiteral("(ht|f)tps"), QStringLiteral("(ht|f)tp$"), QStringLiteral("m3u8.*"),
                                 QStringLiteral(".*dash"), QStringLiteral("websocket_frag"), QStringLiteral("rtmpe?"),
                                 QString(), QStringLiteral("ws|websocket"), QStringLiteral("f4")}; // 9 entries, '' at index 6
const QStringList kVextOrder = {QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("webm"), QStringLiteral("flv"),
                                QString(), QStringLiteral("none")}; // 6 entries, no None entry ('' at index 4)
const QStringList kAextOrder = {QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("mp3"), QStringLiteral("ogg"),
                                QStringLiteral("opus"), QStringLiteral("web[am]"), QString(), QStringLiteral("none")}; // 8 entries, '' at index 6

// Score for an ordered field (vcodec/acodec/hdr/proto/vext/aext). Matching uses re.match semantics:
// anchored at the start of the lowercased value, no end anchor - except vext, which yt-dlp matches
// by exact membership (useRegex = false). A missing/null value scores like the list's None entry -
// or its '' entry when there is no None; a non-null value matching nothing scores like the ''
// entry. Both sit below any real codec/protocol/extension.
int orderedFieldScore(const QJsonValue& value, const QStringList& order, int nonePos, bool useRegex = true) {
    if (!value.isString())
        return order.size() - nonePos;
    const QString text = value.toString().toLower();
    for (int i = 0; i < order.size(); ++i) {
        const QString pattern = order.at(i);
        if (pattern.isEmpty())
            continue; // '' and None entries never match in yt-dlp's loop either
        if (useRegex) {
            QRegularExpression rx(pattern);
            const auto m = rx.match(text);
            if (m.hasMatch() && m.capturedStart(0) == 0)
                return order.size() - i;
        } else if (text == pattern) // yt-dlp matches the vext list by exact membership, not regex
            return order.size() - i;
    }
    // No match: score of the '' entry, which always precedes the None placeholder.
    return order.size() - order.indexOf(QString());
}

// yt-dlp's FormatSorter._fill_sorting_fields derives protocol/ext/video_ext/audio_ext and the
// bitrates before sorting (video_ext/audio_ext are recomputed from ext even when the extractor
// provided them). These helpers reproduce that so the ranking matches for any input, not just
// YouTube output where the keys happen to be present already.
QString determineExt(const QString& url) {
    if (url.isEmpty() || !url.contains(QLatin1Char('.')))
        return QStringLiteral("unknown_video");
    const QString beforeQuery = url.section(QLatin1Char('?'), 0, 0);
    // Python's rpartition('.')[2]: text after the last dot, or the whole string when there is no dot.
    const int dot = beforeQuery.lastIndexOf(QLatin1Char('.'));
    QString guess = (dot >= 0) ? beforeQuery.mid(dot + 1) : beforeQuery;
    static const QRegularExpression kAlnum(QStringLiteral("^[A-Za-z0-9]+$"));
    if (!kAlnum.match(guess).hasMatch()) {
        // Trailing-slash form ("bar.mp4/"): yt-dlp only accepts it for known extensions; accepting any
        // alphanumeric guess here is scoring-equivalent, since an unrecognized ext scores like
        // "unknown_video" in every preference list.
        while (guess.endsWith(QLatin1Char('/')))
            guess.chop(1);
        if (!kAlnum.match(guess).hasMatch())
            return QStringLiteral("unknown_video");
    }
    return guess;
}

QString determineProtocol(const QJsonObject& f) {
    const QString url = f.value(QStringLiteral("url")).toString();
    if (url.startsWith(QLatin1String("rtmp")))
        return QStringLiteral("rtmp");
    const QString ext = determineExt(url);
    if (ext == QLatin1String("m3u8"))
        return f.value(QStringLiteral("is_live")).toBool() ? QStringLiteral("m3u8") : QStringLiteral("m3u8_native");
    if (ext == QLatin1String("f4m"))
        return QStringLiteral("f4m");
    const int sep = url.indexOf(QLatin1Char(':'));
    return (sep > 0) ? url.left(sep).toLower() : QString();
}

// Bitrates after yt-dlp's _fill_sorting_fields fill-in (nullopt = still None, 0 is a real value).
struct BrFill {
    std::optional<double> tbr;
    std::optional<double> vbr;
    std::optional<double> abr;
};

BrFill fillBitrates(const QJsonObject& f) {
    const bool vNone = f.value(QStringLiteral("vcodec")).toString() == QLatin1String("none");
    const bool aNone = f.value(QStringLiteral("acodec")).toString() == QLatin1String("none");
    auto num = [](const QJsonValue& v) -> std::optional<double> { return v.isDouble() ? std::make_optional(v.toDouble()) : std::nullopt; };
    BrFill b;
    b.tbr = num(f.value(QStringLiteral("tbr")));
    b.vbr = num(f.value(QStringLiteral("vbr")));
    b.abr = num(f.value(QStringLiteral("abr")));
    if (vNone)
        b.vbr = 0.0;
    if (aNone)
        b.abr = 0.0;
    // try_call(...) or None: only when both operands are present, and a zero result counts as missing.
    const auto diffOrNone = [](std::optional<double> a, std::optional<double> c) -> std::optional<double> {
        if (!a || !c)
            return std::nullopt;
        const double d = *a - *c;
        return (d != 0.0) ? std::make_optional(d) : std::nullopt;
    };
    if ((!b.vbr || *b.vbr == 0.0) && !vNone)
        b.vbr = diffOrNone(b.tbr, b.abr);
    if ((!b.abr || *b.abr == 0.0) && !aNone)
        b.abr = diffOrNone(b.tbr, b.vbr);
    if (!b.tbr || *b.tbr == 0.0) {
        b.tbr = std::nullopt;
        if (b.vbr && b.abr && (*b.vbr + *b.abr) != 0.0)
            b.tbr = *b.vbr + *b.abr;
    }
    return b;
}

// The "br" sort value: first non-null of tbr/vbr/abr after yt-dlp's bitrate fill-in.
double brFromFill(const BrFill& b) {
    for (const auto* v : {&b.tbr, &b.vbr, &b.abr}) // filter(None): missing and zero values are skipped
        if (*v && **v != 0.0)
            return **v;
    return kUnknownRank;
}

// One candidate's position in yt-dlp's effective sort (all keys best-first). The field order is
// not fixed - buildEffectiveOrder() assembles it per info dict; these are just the values each
// field reads from a format.
struct FormatRank {
    double hasvid = 0.0;   // 0 with video, -1 without (only matters for the "b" fallback)
    bool prefSet = false;  // per-format "preference" is set (drives hidden/ie_pref)
    double pref = -1.0;    // raw "preference" value
    double lang = -1.0;    // language_preference (-1 when unset)
    double quality = -1.0; // extractor-assigned quality (-1 when unset); e.g. YouTube marks DRC audio variants lower
    double res = 0.0;      // min(height, width); 0 when unknown
    double fps = kUnknownRank;
    double hdr = kUnknownRank;   // raw ordered score of dynamic_range ("hdr:12" limit applied at compare time)
    double source = -1.0;        // source_preference (-1 when unset)
    double vcodec = 0.0;         // ordered score of the video codec
    double channels = kUnknownRank; // audio_channels
    double acodec = 0.0;         // ordered score of the audio codec
    double protoScore = 0.0;     // ordered score of protocol
    double size = kUnknownRank;  // filesize or filesize_approx, in bytes
    BrFill brf{};                // filled-in tbr/vbr/abr (nullopt = None)
    double br = kUnknownRank;    // first known of tbr/vbr/abr
    double asr = kUnknownRank;   // audio sample rate
    double vextScore = 0.0;      // ordered score of video_ext
    double aextScore = 0.0;      // ordered score of audio_ext
    QString id;                  // format_id (yt-dlp sorts string ids lexicographically)
};

FormatRank makeFormatRank(const QJsonObject& f) {
    FormatRank r;
    const QString vcodec = f.value(QStringLiteral("vcodec")).toString();
    r.hasvid = (!vcodec.isEmpty() && vcodec != QLatin1String("none")) ? 0.0 : -1.0;

    // "convert": "float" with default -1 in yt-dlp's settings (a missing value sorts below any real one).
    // QJsonValue::toDouble does not parse strings, so numeric strings are converted explicitly to
    // match float_or_none().
    auto floatOrDefault = [](const QJsonValue& v, double dflt) {
        if (v.isDouble())
            return v.toDouble(dflt);
        if (v.isString()) {
            bool ok = false;
            const double d = v.toString().toDouble(&ok);
            if (ok)
                return d;
        }
        return dflt;
    };
    r.lang = floatOrDefault(f.value(QStringLiteral("language_preference")), -1.0);
    r.quality = floatOrDefault(f.value(QStringLiteral("quality")), -1.0);
    r.source = floatOrDefault(f.value(QStringLiteral("source_preference")), -1.0);

    const QJsonValue prefValue = f.value(QStringLiteral("preference"));
    if (prefValue.isDouble() || prefValue.isString()) {
        r.prefSet = true;
        r.pref = floatOrDefault(prefValue, -1.0);
    }

    // yt-dlp: min(filter(None, (height, width)), default=0) - one known dimension is enough.
    const double height = f.value(QStringLiteral("height")).toDouble(0.0);
    const double width = f.value(QStringLiteral("width")).toDouble(0.0);
    if (height > 0.0 && width > 0.0)
        r.res = std::min(height, width);
    else if (height > 0.0 || width > 0.0)
        r.res = height > 0.0 ? height : width;

    // "float_none" conversion: numeric strings count too; anything else stays unknown.
    r.fps = floatOrDefault(f.value(QStringLiteral("fps")), kUnknownRank);

    // Raw ordered score of dynamic_range; the "hdr:12" limit is applied at compare time by rankBeats().
    r.hdr = orderedFieldScore(f.value(QStringLiteral("dynamic_range")), kHdrOrder, 7);

    r.vcodec = orderedFieldScore(f.value(QStringLiteral("vcodec")), kVcodecOrder, 9);

    r.channels = floatOrDefault(f.value(QStringLiteral("audio_channels")), kUnknownRank);

    const QString acodec = f.value(QStringLiteral("acodec")).toString();
    r.acodec = orderedFieldScore(QJsonValue(acodec), kAcodecOrder, 12);

    // protocol/ext/video_ext/audio_ext as yt-dlp's _fill_sorting_fields leaves them before sorting.
    QString ext = f.value(QStringLiteral("ext")).toString();
    if (ext.isEmpty() && f.contains(QStringLiteral("url")))
        ext = determineExt(f.value(QStringLiteral("url")).toString()).toLower();
    const bool vNone = vcodec == QLatin1String("none");
    const bool aNone = acodec == QLatin1String("none");
    // yt-dlp: `if not format.get('protocol'): format['protocol'] = determine_protocol(format)`.
    // determine_protocol() itself returns an existing non-None value unchanged, so an explicit
    // empty string is preserved and only a missing/null protocol is derived from the url.
    const QJsonValue protoValue = f.value(QStringLiteral("protocol"));
    QString proto;
    if (protoValue.isString())
        proto = protoValue.toString();
    else if (f.contains(QStringLiteral("url")))
        proto = determineProtocol(f);
    r.protoScore = orderedFieldScore(QJsonValue(proto), kProtoOrder, 6);
    const QString vext = vNone ? QStringLiteral("none") : ext;
    const QString aext = (!vNone || aNone) ? QStringLiteral("none") : ext;
    r.vextScore = orderedFieldScore(QJsonValue(vext), kVextOrder, 4, false); // exact membership in yt-dlp
    r.aextScore = orderedFieldScore(QJsonValue(aext), kAextOrder, 6);

    // _fill_sorting_fields demotes out-of-spec HEVC-over-FLV when the extractor set no preference.
    if (!r.prefSet && ext == QLatin1String("flv")) {
        static const QRegularExpression kHevcFlv(QStringLiteral("[hx]265|he?vc?"));
        const auto m = kHevcFlv.match(vcodec); // yt-dlp matches the raw, unlowercased vcodec
        if (m.hasMatch() && m.capturedStart(0) == 0) {
            r.prefSet = true;
            r.pref = -100.0;
        }
    }

    const QJsonValue fsValue = f.value(QStringLiteral("filesize"));
    const QJsonValue faValue = f.value(QStringLiteral("filesize_approx"));
    if (fsValue.isDouble() && fsValue.toDouble() > 0.0)
        r.size = fsValue.toDouble();
    else if (faValue.isDouble() && faValue.toDouble() > 0.0)
        r.size = faValue.toDouble();

    r.brf = fillBitrates(f);
    r.br = brFromFill(r.brf);

    r.asr = floatOrDefault(f.value(QStringLiteral("asr")), kUnknownRank);

    r.id = f.value(QStringLiteral("format_id")).toString();
    return r;
}

// One entry of yt-dlp's effective sort order (FormatSorter._order after evaluate_params).
struct SortKey {
    QString field; // base field name, e.g. "hdr"
    double limit = 0.0; // resolved limit value ("hdr:12" -> 7); only used when hasLimit is set
    bool hasLimit = false;
    bool reverse = false; // "+" prefix: ascending order
    bool closest = false; // "~" separator: the value nearest the limit wins
};

// yt-dlp's alias settings entries (FormatSorter.settings, 'type': 'alias').
QString resolveFieldAlias(const QString& field) {
    static const QHash<QString, QString> kAliases = {
        {QStringLiteral("format_id"), QStringLiteral("id")},
        {QStringLiteral("preference"), QStringLiteral("ie_pref")},
        {QStringLiteral("language_preference"), QStringLiteral("lang")},
        {QStringLiteral("source_preference"), QStringLiteral("source")},
        {QStringLiteral("protocol"), QStringLiteral("proto")},
        {QStringLiteral("filesize_approx"), QStringLiteral("fs_approx")},
        {QStringLiteral("audio_channels"), QStringLiteral("channels")},
        {QStringLiteral("dimension"), QStringLiteral("res")},
        {QStringLiteral("resolution"), QStringLiteral("res")},
        {QStringLiteral("extension"), QStringLiteral("ext")},
        {QStringLiteral("bitrate"), QStringLiteral("br")},
        {QStringLiteral("total_bitrate"), QStringLiteral("tbr")},
        {QStringLiteral("video_bitrate"), QStringLiteral("vbr")},
        {QStringLiteral("audio_bitrate"), QStringLiteral("abr")},
        {QStringLiteral("framerate"), QStringLiteral("fps")},
        {QStringLiteral("filesize_estimate"), QStringLiteral("size")},
        {QStringLiteral("samplerate"), QStringLiteral("asr")},
        {QStringLiteral("video_ext"), QStringLiteral("vext")},
        {QStringLiteral("audio_ext"), QStringLiteral("aext")},
        {QStringLiteral("video_codec"), QStringLiteral("vcodec")},
        {QStringLiteral("audio_codec"), QStringLiteral("acodec")},
        {QStringLiteral("video"), QStringLiteral("hasvid")},
        {QStringLiteral("has_video"), QStringLiteral("hasvid")},
        {QStringLiteral("audio"), QStringLiteral("hasaud")},
        {QStringLiteral("has_audio"), QStringLiteral("hasaud")},
        {QStringLiteral("extractor"), QStringLiteral("ie_pref")},
        {QStringLiteral("extractor_preference"), QStringLiteral("ie_pref")},
    };
    return kAliases.value(field, field);
}

// Resolve a "field:limit" limit text the way yt-dlp does (_resolve_field_value): ordered fields
// match the limit against their preference list, everything else parses it as a number.
double resolveFieldLimit(const QString& field, const QString& text) {
    if (field == QLatin1String("vcodec")) return orderedFieldScore(QJsonValue(text), kVcodecOrder, 9);
    if (field == QLatin1String("acodec")) return orderedFieldScore(QJsonValue(text), kAcodecOrder, 12);
    if (field == QLatin1String("hdr")) return orderedFieldScore(QJsonValue(text), kHdrOrder, 7);
    if (field == QLatin1String("proto")) return orderedFieldScore(QJsonValue(text), kProtoOrder, 6);
    if (field == QLatin1String("vext")) return orderedFieldScore(QJsonValue(text), kVextOrder, 4, false);
    if (field == QLatin1String("aext")) return orderedFieldScore(QJsonValue(text), kAextOrder, 6);
    bool ok = false;
    const double v = text.toDouble(&ok);
    return ok ? v : 0.0;
}

// Assemble the effective sort order exactly like FormatSorter.evaluate_params: forced fields and
// priority fields (in default-tuple order), then the extractor-provided "_format_sort_fields" from
// the info dict, then the full default tuple - deduplicated by first occurrence. Each item is parsed
// with yt-dlp's regex ("[+]field[:|~]limit"), aliases are resolved, and the combined field "ext"
// expands to its sub-fields vext and aext (a limit on it splits across them).
QVector<SortKey> buildEffectiveOrder(const QJsonObject& info) {
    // FormatSorter.default (yt_dlp/utils/_utils.py).
    static const char* const kDefault[] = {"hidden",   "aud_or_vid", "hasvid",  "ie_pref", "lang",     "quality",
                                           "res",      "fps",        "hdr:12",  "vcodec",  "channels", "acodec",
                                           "size",     "br",         "asr",     "proto",   "ext",      "hasaud",
                                           "source",   "id"};

    QVector<SortKey> order;
    auto add = [&order](const QString& item) {
        QString rest = item.trimmed();
        bool reverse = false;
        if (rest.startsWith(QLatin1Char('+'))) {
            reverse = true;
            rest.remove(0, 1);
        }
        int i = 0;
        while (i < rest.size() && (rest.at(i).isLetterOrNumber() || rest.at(i) == QLatin1Char('_')))
            ++i;
        const QString base = resolveFieldAlias(rest.left(i));
        if (base.isEmpty())
            return;
        bool hasLimit = false;
        bool closest = false;
        QString limitText;
        if (i < rest.size() && (rest.at(i) == QLatin1Char(':') || rest.at(i) == QLatin1Char('~'))) {
            hasLimit = true;
            closest = (rest.at(i) == QLatin1Char('~'));
            limitText = rest.mid(i + 1);
        }
        // "ext" is a combined field: yt-dlp adds its sub-fields vext and aext instead. A limit on it
        // splits across the sub-fields ("ext:mp4:m4a"); with one part only the first gets it.
        const QStringList fields = (base == QLatin1String("ext")) ? QStringList{QStringLiteral("vext"), QStringLiteral("aext")}
                                                                  : QStringList{base};
        const QStringList limits = (hasLimit && base == QLatin1String("ext")) ? limitText.split(QLatin1Char(':')) : QStringList();
        for (int fi = 0; fi < fields.size(); ++fi) {
            const QString f = fields.at(fi);
            if (std::any_of(order.begin(), order.end(), [&f](const SortKey& k) { return k.field == f; }))
                continue;
            SortKey key;
            key.field = f;
            key.reverse = reverse;
            key.closest = closest;
            if (hasLimit && ((base != QLatin1String("ext")) || fi < limits.size())) {
                key.hasLimit = true;
                key.limit = resolveFieldLimit(f, (base == QLatin1String("ext")) ? limits.at(fi) : limitText);
            }
            order.append(key);
        }
    };

    // Forced fields, then priority fields - both in default-tuple order.
    for (const QString& f : {QStringLiteral("hidden"), QStringLiteral("aud_or_vid")})
        add(f);
    for (const QString& f : {QStringLiteral("hasvid"), QStringLiteral("ie_pref")})
        add(f);

    const QJsonArray extractorFields = info.value(QStringLiteral("_format_sort_fields")).toArray();
    for (const QJsonValue& v : extractorFields)
        add(v.toString());

    for (const char* f : kDefault)
        add(QLatin1String(f));
    return order;
}

// The raw sort value of one field for a candidate (yt-dlp's _calculate_field_preference), before
// any "field:limit" demotion. Fields yt-dlp would read from per-format data this port does not
// model (arbitrary extractor fields) report the unknown rank, which keeps them neutral.
double rankFieldValue(const FormatRank& r, const QString& field) {
    if (field == QLatin1String("hidden"))
        // "extractor" type with max -1000: unset or >= -1000 preferences all sort as -1.
        return (!r.prefSet || r.pref >= -1000.0) ? -1.0 : r.pref;
    if (field == QLatin1String("ie_pref"))
        return r.prefSet ? r.pref : -1.0; // "extractor" type: unset sorts as -1
    if (field == QLatin1String("hasvid"))
        return r.hasvid;
    if (field == QLatin1String("lang"))
        return r.lang;
    if (field == QLatin1String("quality"))
        return r.quality;
    if (field == QLatin1String("res"))
        return r.res;
    if (field == QLatin1String("fps"))
        return r.fps;
    if (field == QLatin1String("hdr"))
        return r.hdr; // raw ordered score; the "hdr:12" limit is applied by rankBeats()
    if (field == QLatin1String("source"))
        return r.source;
    if (field == QLatin1String("vcodec"))
        return r.vcodec;
    if (field == QLatin1String("channels"))
        return r.channels;
    if (field == QLatin1String("acodec"))
        return r.acodec;
    if (field == QLatin1String("proto"))
        return r.protoScore;
    if (field == QLatin1String("size"))
        return r.size;
    if (field == QLatin1String("br"))
        return r.br;
    // Filled-in bitrates: 0 is a real value, only None sorts as unknown.
    if (field == QLatin1String("tbr"))
        return r.brf.tbr.value_or(kUnknownRank);
    if (field == QLatin1String("vbr"))
        return r.brf.vbr.value_or(kUnknownRank);
    if (field == QLatin1String("abr"))
        return r.brf.abr.value_or(kUnknownRank);
    if (field == QLatin1String("asr"))
        return r.asr;
    if (field == QLatin1String("vext"))
        return r.vextScore;
    if (field == QLatin1String("aext"))
        return r.aextScore;
    return kUnknownRank; // unknown/arbitrary field: neutral for all candidates
}

// One element of yt-dlp's per-field rank tuple (larger is better, compared lexicographically).
struct RankTuple {
    int tag = 0; // -10: value missing, -1: demoted below the limit band, 0: normal, 1: string value
    double x = 0.0;
    double y = 0.0;
};

// _calculate_field_preference_from_value for numeric fields (string-valued fields are handled by
// rankBeats directly). kUnknownRank stands in for yt-dlp's None.
RankTuple fieldRank(const FormatRank& r, const SortKey& key) {
    const double v = rankFieldValue(r, key.field);
    if (v == kUnknownRank)
        return {-10, 0.0, 0.0}; // "value is None"
    if (key.closest && key.hasLimit)
        return {0, -qAbs(v - key.limit), key.reverse ? v - key.limit : key.limit - v};
    if (!key.reverse && (!key.hasLimit || v <= key.limit))
        return {0, v, 0.0}; // larger wins (values at or under the cap)
    if (!key.hasLimit || (key.reverse && v == key.limit) || v > key.limit)
        return {0, -v, 0.0}; // smaller wins: reverse fields, and values above a non-reverse cap
    return {-1, v, 0.0}; // reverse + limit: values below the cap are demoted
}

// True when a sorts after b in yt-dlp's effective order, i.e. is the "better" pick.
bool rankBeats(const QVector<SortKey>& order, const FormatRank& a, const FormatRank& b) {
    for (const SortKey& key : order) {
        // aud_or_vid is 1 for every candidate in any pool; hasaud is constant within each pool -
        // neither can discriminate between two candidates of the same pool.
        if (key.field == QLatin1String("aud_or_vid") || key.field == QLatin1String("hasaud"))
            continue;
        if (key.field == QLatin1String("id")) {
            // "convert": "string" - format_id compared lexicographically, higher wins.
            if (a.id != b.id)
                return a.id > b.id;
            continue;
        }
        const RankTuple ta = fieldRank(a, key);
        const RankTuple tb = fieldRank(b, key);
        if (ta.tag != tb.tag)
            return ta.tag > tb.tag;
        if (ta.x != tb.x)
            return ta.x > tb.x;
        if (ta.y != tb.y)
            return ta.y > tb.y;
    }
    return false; // complete tie: Python's stable sort keeps the earlier format, as does this loop
}

// One stream picked as "best" by yt-dlp's default criteria.
struct BestPick {
    bool found = false;
    QString id;
    QString vcodec;
    QString acodec;
    int height = 0;
    double fps = 0.0;
    double tbr = 0.0;
    FormatRank rank{};
};

// Describe what mpv's ytdl_hook will pick with its default format selector ("bv*+ba/b"): the
// best video-only stream plus the best audio-only stream, or - when no such pair exists - the
// best combined stream. Used for logging only.
QString describeDefaultFormatSelection(const QJsonObject& info) {
    const QVector<SortKey> order = buildEffectiveOrder(info);
    BestPick video; // "bv*" - best video-only stream
    BestPick audio; // "ba" - best audio-only stream
    BestPick combined; // "b" fallback: carries both streams
    for (const QJsonValue& fv : info.value(QStringLiteral("formats")).toArray()) {
        const QJsonObject f = fv.toObject();
        const QString vcodec = f.value(QStringLiteral("vcodec")).toString();
        const QString acodec = f.value(QStringLiteral("acodec")).toString();
        const bool hasVideo = !vcodec.isEmpty() && vcodec != QLatin1String("none");
        const bool hasAudio = !acodec.isEmpty() && acodec != QLatin1String("none");
        if (!hasVideo && !hasAudio)
            continue;
        BestPick p;
        p.found = true;
        p.id = f.value(QStringLiteral("format_id")).toString();
        if (p.id.isEmpty())
            continue;
        p.vcodec = vcodec;
        p.acodec = acodec;
        p.height = hasVideo ? f.value(QStringLiteral("height")).toInt(0) : 0;
        p.fps = f.value(QStringLiteral("fps")).toDouble(0.0);
        p.tbr = f.value(QStringLiteral("tbr")).toDouble(0.0);
        p.rank = makeFormatRank(f);
        if (hasVideo && !hasAudio) {
            if (!video.found || rankBeats(order, p.rank, video.rank))
                video = p;
        } else if (!hasVideo && hasAudio) {
            if (!audio.found || rankBeats(order, p.rank, audio.rank))
                audio = p;
        } else if (hasVideo && hasAudio) {
            if (!combined.found || rankBeats(order, p.rank, combined.rank))
                combined = p;
        }
    }

    auto describeStream = [](const BestPick& p, bool isVideo) -> QString {
        QString s = QStringLiteral("%1 %2 (").arg(isVideo ? QStringLiteral("video") : QStringLiteral("audio"), p.id);
        if (isVideo) {
            s += shortVideoCodec(p.vcodec);
            if (p.height > 0)
                s += QStringLiteral(", %1p").arg(p.height);
            if (p.fps > 0.0)
                s += QStringLiteral(", %1 fps").arg(static_cast<int>(qRound(p.fps)));
        } else {
            s += p.acodec;
        }
        const QString br = formatBitrate(p.tbr);
        if (!br.isEmpty())
            s += QStringLiteral(", ") + br;
        return s + QLatin1Char(')');
    };

    if (video.found && audio.found)
        return describeStream(video, true) + QStringLiteral(" + ") + describeStream(audio, false);
    if (combined.found)
        return describeStream(combined, true) + QStringLiteral(" (combined video+audio stream)");
    return QStringLiteral("no downloadable formats");
}
// [[test-extract:end]]

// Collect language entries from a "subtitles"/"automatic_captions" object (language code ->
// list of format objects). Manual subtitles are collected first, so they win on duplicates.
void collectSubtitleLanguages(const QJsonValue& jv, QSet<QString>& seen, QVariantList& out) {
    const QJsonObject obj = jv.toObject();
    for (const QString& code : obj.keys()) {
        if (code.isEmpty() || seen.contains(code))
            continue;
        seen.insert(code);
        QString label = code;
        const QJsonArray arr = obj.value(code).toArray();
        if (!arr.isEmpty()) {
            const QString name = arr.first().toObject().value(QStringLiteral("name")).toString();
            if (!name.isEmpty())
                label = name;
        }
        out.append(QVariantMap{{QStringLiteral("id"), code}, {QStringLiteral("label"), label}});
    }
}

// Extract a short, readable error from yt-dlp's stderr (it ends with an "ERROR: ..." line).
QString extractYtdlpError(const QByteArray& stderrData) {
    QString err = QString::fromUtf8(stderrData).trimmed();
    const int errIdx = err.lastIndexOf(QStringLiteral("ERROR:"));
    if (errIdx >= 0)
        return err.mid(errIdx);
    if (err.length() > 240)
        return err.left(240) + QStringLiteral("…");
    return err;
}

} // namespace

YtdlpMetadataModel::YtdlpMetadataModel(QObject* parent) : QObject(parent) {
}

YtdlpMetadataModel::~YtdlpMetadataModel() {
    // Cancel an in-flight fetch and wait for its worker to finish (the process is killed, so
    // this returns quickly).
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_activeFetch && m_activeFetch->process) {
            m_activeFetch->cancelled.store(true);
            m_activeFetch->process->kill();
        }
    }
    if (m_worker.joinable())
        m_worker.join();
}

bool YtdlpMetadataModel::isLoading() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_isLoading;
}

QString YtdlpMetadataModel::errorText() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_errorText;
}

QString YtdlpMetadataModel::title() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_title;
}

QString YtdlpMetadataModel::playbackFormat() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_playbackFormat;
}

QVariantList YtdlpMetadataModel::videoFormats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_videoFormats;
}

QVariantList YtdlpMetadataModel::audioTracks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_audioTracks;
}

QVariantList YtdlpMetadataModel::subtitles() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_subtitles;
}

void YtdlpMetadataModel::fetch(const QString& url) {
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty())
        return;

    // Cancel any in-flight fetch first.
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_activeFetch && m_activeFetch->process) {
            m_activeFetch->cancelled.store(true);
            m_activeFetch->process->kill();
        }
    }
    if (m_worker.joinable())
        m_worker.join();

    const QString ytdlpPath = QString::fromStdString(YtdlpResolver::resolveYtdlpPath());
    // Read on the main thread so the worker never touches KConfig.
    const QString configuredFormat = StreamSettings::ytdlFormat().trimmed();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_errorText.clear();
        m_title.clear();
        m_playbackFormat.clear();
        m_videoFormats.clear();
        m_audioTracks.clear();
        m_subtitles.clear();
        if (ytdlpPath.isEmpty()) {
            m_isLoading = false;
            m_errorText = QStringLiteral("yt-dlp not found on this machine.");
        } else {
            m_isLoading = true;
            auto* state = new FetchState();
            state->process = new QProcess();
            m_activeFetch = state;
        }
    }

    if (ytdlpPath.isEmpty()) {
        Q_EMIT dataChanged();
        return;
    }

    sgct::Log::Info(std::format("Fetching yt-dlp metadata for '{}'.", trimmed.toStdString()));
    m_worker = std::thread([this, ytdlpPath, trimmed, configuredFormat]() { runFetch(ytdlpPath, trimmed, configuredFormat); });
}

void YtdlpMetadataModel::clear() {
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_activeFetch && m_activeFetch->process) {
            m_activeFetch->cancelled.store(true);
            m_activeFetch->process->kill();
        }
        changed = m_isLoading || !m_errorText.isEmpty() || !m_title.isEmpty() ||
                  !m_playbackFormat.isEmpty() || !m_videoFormats.isEmpty() ||
                  !m_audioTracks.isEmpty() || !m_subtitles.isEmpty();
        m_isLoading = false;
        m_errorText.clear();
        m_title.clear();
        m_playbackFormat.clear();
        m_videoFormats.clear();
        m_audioTracks.clear();
        m_subtitles.clear();
    }
    if (m_worker.joinable())
        m_worker.join();
    if (changed)
        Q_EMIT dataChanged();
}


void YtdlpMetadataModel::runFetch(const QString& ytdlpPath, const QString& url, const QString& configuredFormat) {
    FetchState* state = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        state = m_activeFetch;
    }
    if (!state || !state->process)
        return;

    QProcess* proc = state->process;
    QStringList args{QStringLiteral("--no-warnings"), QStringLiteral("-J"), QStringLiteral("--no-playlist")};
    // "--" protects the URL from being misread as an option.
    args << QStringLiteral("--") << url;
    proc->start(ytdlpPath, args);

    QString errorText;
    QString title;
    QString playbackFormat;
    QJsonObject root;
    QJsonArray formats;
    QVariantList videoFormats, audioTracks, subtitleLangs;
    if (!proc->waitForStarted(10000)) {
        errorText = QStringLiteral("Failed to start yt-dlp.");
    } else if (!proc->waitForFinished(kFetchTimeoutMs)) {
        errorText = QStringLiteral("yt-dlp timed out while resolving the URL.");
        proc->kill();
        proc->waitForFinished(5000);
    } else if (state->cancelled.load()) {
        // Superseded by a newer fetch or cleared - drop the result silently.
    } else if (proc->exitStatus() != QProcess::NormalExit || proc->exitCode() != 0) {
        errorText = extractYtdlpError(proc->readAllStandardError());
        if (errorText.isEmpty())
            errorText = QStringLiteral("yt-dlp failed to resolve the URL.");
    } else {
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(proc->readAllStandardOutput(), &parseError);
        if (doc.isNull() || !doc.isObject()) {
            errorText = QStringLiteral("Could not parse the yt-dlp output.");
        } else {
            root = doc.object();
            title = root.value(QStringLiteral("title")).toString().trimmed();
            formats = root.value(QStringLiteral("formats")).toArray();
            videoFormats = buildVideoFormats(formats);
            audioTracks = buildAudioTracks(formats);
            QSet<QString> seen;
            collectSubtitleLanguages(root.value(QStringLiteral("subtitles")), seen, subtitleLangs);
            collectSubtitleLanguages(root.value(QStringLiteral("automatic_captions")), seen, subtitleLangs);
            // What mpv's ytdl_hook will actually play: the configured format selector when one is
            // set in Settings -> Playback, otherwise a description of the default "bv*+ba/b" best
            // combination resolved from the formats list (the same info that goes to the log below).
            playbackFormat = !configuredFormat.isEmpty() ? configuredFormat : describeDefaultFormatSelection(root);
        }
    }

    bool cancelled = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!state->cancelled.load()) {
            m_isLoading = false;
            m_errorText = errorText;
            m_title = title;
            m_playbackFormat = playbackFormat;
            m_videoFormats = videoFormats;
            m_audioTracks = audioTracks;
            m_subtitles = subtitleLangs;
        } else {
            cancelled = true;
        }
        if (m_activeFetch == state)
            m_activeFetch = nullptr;
    }

    delete proc;
    delete state;
    // A cancelled fetch was superseded by a newer fetch() or clear(), which are responsible for
    // notifying the UI. Emitting here would deliver an extra, stale dataChanged to QML after the
    // new state has already been applied (e.g. re-filling the layer title with the old video's).
    if (!cancelled) {
        if (errorText.isEmpty()) {
            // Report what mpv's ytdl_hook will actually use for playback: the configured format
            // selector when one is set, otherwise the default "bv*+ba/b" best combination.
            if (!configuredFormat.isEmpty()) {
                sgct::Log::Info(std::format("Fetched yt-dlp metadata for '{}': title '{}' - playback uses the configured yt-dlp format '{}' (Settings -> Playback).",
                                            url.toStdString(), title.toStdString(), configuredFormat.toStdString()));
            } else {
                sgct::Log::Info(std::format("Fetched yt-dlp metadata for '{}': title '{}' - playback will use {} (mpv/yt-dlp default 'bv*+ba/b').",
                                            url.toStdString(), title.toStdString(), describeDefaultFormatSelection(root).toStdString()));
            }
        }
        Q_EMIT dataChanged();
    }
}

