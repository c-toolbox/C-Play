/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "httpserverthread.h"
#include <cmath>
#include "mpvobject.h"
#include "playbacksettings.h"
#include "playercontroller.h"
#include "slidesmodel.h"
#include "layersmodel.h"
#include "layers/baselayer.h"
#include "controleventfeed.h"
#include "tcp/tcpcontrolrestapi.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#pragma warning(disable : 4996)

std::string formatTime(double timeInSeconds, std::string format = "hh:mm:ss/zz") {
    QTime t(0, 0, 0);
    QString formatQ = QString::fromStdString(format);
    QString formattedTime = t.addMSecs(static_cast<qint64>(timeInSeconds * 1000)).toString(formatQ);
    return formattedTime.toStdString();
}

HttpServerThread::HttpServerThread(QObject *parent)
    : QThread(parent),
        m_mpv(nullptr),
        m_slidesModel(nullptr),
        runServer(false),
        portServer(7007) {
}

HttpServerThread::~HttpServerThread() {
    terminate();
    wait();
}

void HttpServerThread::setupHttpServer() {
    QFile httpServerConfFile(QStringLiteral("./data/http-server-conf.json"));

    QJsonObject httpServerConf;
    if (httpServerConfFile.open(QIODevice::ReadOnly))
        httpServerConf = QJsonDocument::fromJson(httpServerConfFile.readAll()).object();
    runServer = httpServerConf.value(QStringLiteral("run")).toString() == QStringLiteral("yes");
    portServer = httpServerConf.value(QStringLiteral("port")).toInt(7007);
    // Register actions even when the HTTP listener is disabled. TCP dispatch uses
    // these same callbacks on the player thread, without a loopback HTTP request.
    {
        registerTcpControlRoutes(svr, m_tcpControlManager,
            [this](const std::string &path, httplib::Server::Handler handler) { registerPost(path, std::move(handler)); });
        svr.Get("/event_topics", [](const httplib::Request &, httplib::Response &res) {
            res.set_content(QJsonDocument(QJsonArray::fromStringList(ControlEventFeed::availableTopics()))
                .toJson(QJsonDocument::Compact).toStdString(), "application/json");
        });
        const auto requestTopics = [](const httplib::Request &req) {
            return req.has_param("topics") ? QString::fromStdString(req.get_param_value("topics")).split(QLatin1Char(','))
                : QStringList{QStringLiteral("*")};
        };
        svr.Get("/events", [this, requestTopics](const httplib::Request &req, httplib::Response &res) {
            const auto topics = requestTopics(req);
            const QString since = QString::fromStdString(req.get_param_value("since"));
            bool cursorValid = true, waitValid = true;
            if (!since.isEmpty()) since.toULongLong(&cursorValid);
            const int wait = req.has_param("wait") ? QString::fromStdString(req.get_param_value("wait")).toInt(&waitValid) : 0;
            if (!ControlEventFeed::validateTopics(topics) || !cursorValid || !waitValid || wait < 0 || wait > 25000) {
                res.status = 400;
                res.set_content("Invalid topics, since cursor or wait (0-25000 ms)", "text/plain");
                return;
            }
            if (!m_eventFeed) { res.status = 503; return; }
            res.set_header("Cache-Control", "no-store");
            res.set_content(QJsonDocument(m_eventFeed->read(topics, since, wait))
                .toJson(QJsonDocument::Compact).toStdString(), "application/json");
        });
        if (runServer && m_eventFeed && httpServerConf.contains(QStringLiteral("websocketPort"))) {
            const int port = httpServerConf.value(QStringLiteral("websocketPort")).toInt();
            if (port > 0 && port <= 65535) m_eventFeed->setWebSocketPort(static_cast<quint16>(port));
            else qWarning("Invalid event websocketPort (must be 1-65535)");
        }
        svr.Get("/status", [](const httplib::Request &, httplib::Response &res) {
            res.set_content("OK", "text/plain");
        });
        registerPost("/status", [](const httplib::Request &, httplib::Response &res) {
            res.set_content("OK", "text/plain");
        });

        registerPost("/quit", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content("Quitting C-Play", "text/plain");
            Q_EMIT quitCPlay();
        });

        registerPost("/play", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT playMedia();
            res.set_content("Play", "text/plain");
        });

        registerPost("/pause", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT pauseMedia();
            res.set_content("Pause", "text/plain");
        });

        registerPost("/stop", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT rewindMedia();
            res.set_content("Stop/rewind", "text/plain");
        });

        registerPost("/rewind", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT rewindMedia();
            res.set_content("Stop/rewind", "text/plain");
        });

        auto positionGetHandler = [this](const httplib::Request &req, httplib::Response &res) {
            if (m_mpv) {
                if (req.has_param("format")) {
                    res.set_content(formatTime(m_mpv->position(), req.get_param_value("format")), "text/plain");
                } else {
                    res.set_content(std::to_string(m_mpv->position()), "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/position", positionGetHandler);
        registerPost("/position", [this, positionGetHandler](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("set")) {
                setPositionFromStr(req.get_param_value("set"));
            }
            positionGetHandler(req, res);
        });

        registerPost("/seek", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("time")) {
                int timeInSec = 0;
                if (stringToInt(req.get_param_value("time"), timeInSec)) {
                    Q_EMIT seekInMedia(timeInSec);
                    if (m_mpv) {
                        if (req.has_param("format")) {
                            res.set_content(formatTime(m_mpv->position() + timeInSec, req.get_param_value("format")), "text/plain");
                        } else {
                            res.set_content(std::to_string(m_mpv->position() + timeInSec), "text/plain");
                        }
                    } else {
                        res.set_content("Seeking " + req.get_param_value("time") + " s", "text/plain");
                    }
                } else {
                    res.set_content("Supply parameter time with a positive or negative value (in seconds)", "text/plain");
                }
            } else {
                res.set_content("Supply parameter time with a positive or negative value (in seconds)", "text/plain");
            }
        });

        auto remainingHandler = [this](const httplib::Request &req, httplib::Response &res) {
            if (m_mpv) {
                if (req.has_param("format")) {
                    res.set_content(formatTime(m_mpv->remaining(), req.get_param_value("format")), "text/plain");
                } else {
                    res.set_content(std::to_string(m_mpv->remaining()), "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/remaining", remainingHandler);
        registerPost("/remaining", remainingHandler);

        auto durationHandler = [this](const httplib::Request &req, httplib::Response &res) {
            if (m_mpv) {
                if (req.has_param("format")) {
                    res.set_content(formatTime(m_mpv->duration(), req.get_param_value("format")), "text/plain");
                } else {
                    res.set_content(std::to_string(m_mpv->duration()), "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/duration", durationHandler);
        registerPost("/duration", durationHandler);

        auto autoPlayGetHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                if (m_mpv->autoPlay()) {
                    res.set_content("1", "text/plain");
                } else {
                    res.set_content("0", "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/auto_play", autoPlayGetHandler);
        registerPost("/auto_play", [this, autoPlayGetHandler](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("on")) {
                int value = -1;
                if (stringToInt(req.get_param_value("on"), value)) {
                    if (value == 1) {
                        Q_EMIT setAutoPlay(true);
                        res.set_content("Auto-play is On", "text/plain");
                    } else if (value == 0) {
                        Q_EMIT setAutoPlay(false);
                        res.set_content("Auto-play is Off", "text/plain");
                    } else {
                        res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                    }
                } else {
                    res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                }
            } else {
                autoPlayGetHandler(req, res);
            }
        });

        auto speedGetHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->speed()), "text/plain");
            } else {
                res.set_content("1", "text/plain");
            }
        };
        svr.Get("/speed", speedGetHandler);
        registerPost("/speed", [this, speedGetHandler](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("factor")) {
                setSpeedFromStr(req.get_param_value("factor"));
            }
            speedGetHandler(req, res);
        });

        auto volumeGetHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->volume()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/volume", volumeGetHandler);
        registerPost("/volume", [this, volumeGetHandler](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("level")) {
                setVolumeFromStr(req.get_param_value("level"));
            }
            volumeGetHandler(req, res);
        });

        auto fadeDurationHandler = [](const httplib::Request &req, httplib::Response &res) {
            double fadeDurationTime = double(PlaybackSettings::fadeDuration()) / 1000.0;
            if (req.has_param("format")) {
                res.set_content(formatTime(fadeDurationTime, req.get_param_value("format")), "text/plain");
            } else {
                res.set_content(std::to_string(fadeDurationTime), "text/plain");
            }
        };
        svr.Get("/fade_duration", fadeDurationHandler);
        registerPost("/fade_duration", fadeDurationHandler);

        registerPost("/fade_volume_down", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT fadeVolumeDown();
            res.set_content("Fading volume down", "text/plain");
        });

        registerPost("/fade_volume_up", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT fadeVolumeUp();
            res.set_content("Fading volume up", "text/plain");
        });

        registerPost("/fade_image_down", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT fadeImageDown();
            res.set_content("Fading image down", "text/plain");
        });

        registerPost("/fade_image_up", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT fadeImageUp();
            res.set_content("Fading image up", "text/plain");
        });

        auto syncImageVolumeFadeGetHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->syncVolumeVisibilityFading()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/sync_image_volume_fade", syncImageVolumeFadeGetHandler);
        registerPost("/sync_image_volume_fade", [this, syncImageVolumeFadeGetHandler](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("value")) {
                std::string valueStr = req.get_param_value("value");
                setSyncImageFadingFromStr(valueStr);
                res.set_content("Setting syncImageVolumeFade to " + valueStr, "text/plain");
            } else {
                syncImageVolumeFadeGetHandler(req, res);
            }
        });

        auto visibilityHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->visibility()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/visibility", visibilityHandler);
        registerPost("/visibility", visibilityHandler);

        auto stereoModeHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->stereoscopicMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/stereo_mode", stereoModeHandler);
        registerPost("/stereo_mode", stereoModeHandler);

        auto gridModeHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->gridToMapOn()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/grid_mode", gridModeHandler);
        registerPost("/grid_mode", gridModeHandler);

        auto backgroundImageGetHandler = [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                if (playctrl->backgroundVisibility() == 0.f) {
                    res.set_content("0", "text/plain");
                } else {
                    res.set_content("1", "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/background_image", backgroundImageGetHandler);
        registerPost("/background_image", [this, backgroundImageGetHandler](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("on")) {
                int value = -1;
                if (stringToInt(req.get_param_value("on"), value)) {
                    if (value == 1) {
                        Q_EMIT setBackgroundVisibility(1.f);
                        res.set_content("Background image is On", "text/plain");
                    } else if (value == 0) {
                        Q_EMIT setBackgroundVisibility(0.f);
                        res.set_content("Background image is Off", "text/plain");
                    } else {
                        res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                    }
                } else {
                    res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                }
            } else {
                backgroundImageGetHandler(req, res);
            }
        });

        auto backgroundImageStereoModeHandler = [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                res.set_content(std::to_string(playctrl->backgroundStereoMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/background_image_stereo_mode", backgroundImageStereoModeHandler);
        registerPost("/background_image_stereo_mode", backgroundImageStereoModeHandler);

        auto backgroundImageGridModeHandler = [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                res.set_content(std::to_string(playctrl->backgroundGridMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/background_image_grid_mode", backgroundImageGridModeHandler);
        registerPost("/background_image_grid_mode", backgroundImageGridModeHandler);

        auto foregroundImageGetHandler = [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                if (playctrl->foregroundVisibility() == 0.f) {
                    res.set_content("0", "text/plain");
                } else {
                    res.set_content("1", "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/foreground_image", foregroundImageGetHandler);
        registerPost("/foreground_image", [this, foregroundImageGetHandler](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("on")) {
                int value = -1;
                if (stringToInt(req.get_param_value("on"), value)) {
                    if (value == 1) {
                        Q_EMIT setForegroundVisibility(1.f);
                        res.set_content("Foreground image is On", "text/plain");
                    } else if (value == 0) {
                        Q_EMIT setForegroundVisibility(0.f);
                        res.set_content("Foreground image is Off", "text/plain");
                    } else {
                        res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                    }
                } else {
                    res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                }
            } else {
                foregroundImageGetHandler(req, res);
            }
        });

        auto foregroundImageStereoModeHandler = [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                res.set_content(std::to_string(playctrl->foregroundStereoMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/foreground_image_stereo_mode", foregroundImageStereoModeHandler);
        registerPost("/foreground_image_stereo_mode", foregroundImageStereoModeHandler);

        auto foregroundImageGridModeHandler = [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                res.set_content(std::to_string(playctrl->foregroundGridMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/foreground_image_grid_mode", foregroundImageGridModeHandler);
        registerPost("/foreground_image_grid_mode", foregroundImageGridModeHandler);

        auto eofModeHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->eofMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/eof_mode", eofModeHandler);
        registerPost("/eof_mode", eofModeHandler);

        auto loopModeHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->eofMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/loop_mode", loopModeHandler);
        registerPost("/loop_mode", loopModeHandler);

        auto viewModeGetHandler = [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                res.set_content(std::to_string(playctrl->getViewModeOnClients()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/view_mode", viewModeGetHandler);
        registerPost("/view_mode", [this, viewModeGetHandler](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("mode")) {
                setViewModeFromStr(req.get_param_value("mode"));
            }
            viewModeGetHandler(req, res);
        });

        auto mediaTitleHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(m_mpv->mediaTitle().toStdString(), "text/plain");
            } else {
                res.set_content("", "text/plain");
            }
        };
        svr.Get("/media_title", mediaTitleHandler);
        registerPost("/media_title", mediaTitleHandler);

        auto audiotracksHandler = [this](const httplib::Request &req, httplib::Response &res) {
            std::string charsPerItem = "";
            std::string removeLoadedFilePrefix = "";
            if (req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            if (req.has_param("removeLoadedFilePrefix")) {
                removeLoadedFilePrefix = req.get_param_value("removeLoadedFilePrefix");
            }
            res.set_content(getAudioTracksItems(charsPerItem, removeLoadedFilePrefix), "text/plain");
        };
        svr.Get("/audiotracks", audiotracksHandler);
        registerPost("/audiotracks", audiotracksHandler);

        auto playingInAudiotracksHandler = [this](const httplib::Request &, httplib::Response &res) {
            res.set_content(getPlaylingItemIndexFromAudioTracks(), "text/plain");
        };
        svr.Get("/playing_in_audiotracks", playingInAudiotracksHandler);
        registerPost("/playing_in_audiotracks", playingInAudiotracksHandler);

        registerPost("/load_from_audiotracks", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromAudioTracks(indexStr), "text/plain");
            }
            else {
                res.set_content("Missing index parameter", "text/plain");
            }
        });

        auto playlistHandler = [this](const httplib::Request &req, httplib::Response &res) {
            std::string charsPerItem = "";
            if (req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            res.set_content(getPlayListItems(charsPerItem), "text/plain");
        };
        svr.Get("/playlist", playlistHandler);
        registerPost("/playlist", playlistHandler);

        auto playingInPlaylistHandler = [this](const httplib::Request &, httplib::Response &res) {
            res.set_content(getPlaylingItemIndexFromPlaylist(), "text/plain");
        };
        svr.Get("/playing_in_playlist", playingInPlaylistHandler);
        registerPost("/playing_in_playlist", playingInPlaylistHandler);

        registerPost("/load_from_playlist", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromPlaylist(indexStr), "text/plain");
            }
            else {
                res.set_content("Missing index parameter", "text/plain");
            }
        });

        auto sectionsHandler = [this](const httplib::Request &req, httplib::Response &res) {
            std::string charsPerItem = "";
            if (req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            res.set_content(getSectionsItems(charsPerItem), "text/plain");
        };
        svr.Get("/sections", sectionsHandler);
        registerPost("/sections", sectionsHandler);

        auto playingInSectionsHandler = [this](const httplib::Request &, httplib::Response &res) {
            res.set_content(getPlaylingItemIndexFromSections(), "text/plain");
        };
        svr.Get("/playing_in_sections", playingInSectionsHandler);
        registerPost("/playing_in_sections", playingInSectionsHandler);

        registerPost("/load_from_sections", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromSections(indexStr), "text/plain");
            }
            else {
                res.set_content("Missing index parameter", "text/plain");
            }
        });

        auto sectionStartTimeHandler = [this](const httplib::Request &req, httplib::Response &res) {
            if (m_mpv) {
                int playingIndex = m_mpv->getPlaySectionsModel()->getPlayingSection();
                if (playingIndex >= 0) {
                    double startTime = m_mpv->getPlaySectionsModel()->sectionStartTime(playingIndex);
                    if (req.has_param("format")) {
                        res.set_content(formatTime(startTime, req.get_param_value("format")), "text/plain");
                    } else {
                        res.set_content(std::to_string(startTime), "text/plain");
                    }
                } else {
                    res.set_content("0", "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/section_start_time", sectionStartTimeHandler);
        registerPost("/section_start_time", sectionStartTimeHandler);

        auto sectionEndTimeHandler = [this](const httplib::Request &req, httplib::Response &res) {
            if (m_mpv) {
                int playingIndex = m_mpv->getPlaySectionsModel()->getPlayingSection();
                if (playingIndex >= 0) {
                    double endTime = m_mpv->getPlaySectionsModel()->sectionEndTime(playingIndex);
                    if (req.has_param("format")) {
                        res.set_content(formatTime(endTime, req.get_param_value("format")), "text/plain");
                    } else {
                        res.set_content(std::to_string(endTime), "text/plain");
                    }
                } else {
                    res.set_content("0", "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/section_end_time", sectionEndTimeHandler);
        registerPost("/section_end_time", sectionEndTimeHandler);

        auto sectionEndModeHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                int playingIndex = m_mpv->getPlaySectionsModel()->getPlayingSection();
                if (playingIndex >= 0) {
                    int endMode = m_mpv->getPlaySectionsModel()->sectionEOSMode(playingIndex);
                    res.set_content(std::to_string(endMode), "text/plain");
                } else {
                    res.set_content("0", "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        };
        svr.Get("/section_end_mode", sectionEndModeHandler);
        registerPost("/section_end_mode", sectionEndModeHandler);

        auto playfileJsonHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                QJsonDocument doc;
                QJsonObject obj = doc.object();
                if (m_mpv->getPlaySectionsModel()->currentEditItem()) {
                    m_mpv->getPlaySectionsModel()->currentEditItem()->asJSON(obj);
                }
                doc.setObject(obj);
                res.set_content(doc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
            } else {
                res.set_content("Could not retrieve playlist", "text/plain");
            }
        };
        svr.Get("/playfile_json", playfileJsonHandler);
        registerPost("/playfile_json", playfileJsonHandler);

        auto playlistJsonHandler = [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                QJsonDocument doc;
                QJsonObject obj = doc.object();
                m_mpv->getPlayListModel()->asJSON(obj);
                doc.setObject(obj);
                res.set_content(doc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
            } else {
                res.set_content("Could not retrieve playlist", "text/plain");
            }
        };
        svr.Get("/playlist_json", playlistJsonHandler);
        registerPost("/playlist_json", playlistJsonHandler);

        registerPost("/spin_pitch_up", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("on")) {
                int runSpin = -1;
                if (stringToInt(req.get_param_value("on"), runSpin)) {
                    if (runSpin == 1) {
                        Q_EMIT spinPitchUp(true);
                        res.set_content("Pitch Up is On", "text/plain");
                    } else if (runSpin == 0) {
                        Q_EMIT spinPitchUp(false);
                        res.set_content("Pitch Up is Off", "text/plain");
                    } else {
                        res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                    }
                } else {
                    res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                }
            } else {
                res.set_content("Supply parameter on with value 0 or 1", "text/plain");
            }
        });

        registerPost("/spin_pitch_down", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("on")) {
                int runSpin = -1;
                if (stringToInt(req.get_param_value("on"), runSpin)) {
                    if (runSpin == 1) {
                        Q_EMIT spinPitchDown(true);
                        res.set_content("Pitch Down is On", "text/plain");
                    } else if (runSpin == 0) {
                        Q_EMIT spinPitchDown(false);
                        res.set_content("Pitch Down is Off", "text/plain");
                    } else {
                        res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                    }
                } else {
                    res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                }
            } else {
                res.set_content("Supply parameter on with value 0 or 1", "text/plain");
            }
        });

        registerPost("/spin_yaw_left", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("on")) {
                int runSpin = -1;
                if (stringToInt(req.get_param_value("on"), runSpin)) {
                    if (runSpin == 1) {
                        Q_EMIT spinYawLeft(true);
                        res.set_content("Yaw Left is On", "text/plain");
                    } else if (runSpin == 0) {
                        Q_EMIT spinYawLeft(false);
                        res.set_content("Yaw Left is Off", "text/plain");
                    } else {
                        res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                    }
                } else {
                    res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                }
            } else {
                res.set_content("Supply parameter on with value 0 or 1", "text/plain");
            }
        });

        registerPost("/spin_yaw_right", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("on")) {
                int runSpin = -1;
                if (stringToInt(req.get_param_value("on"), runSpin)) {
                    if (runSpin == 1) {
                        Q_EMIT spinYawRight(true);
                        res.set_content("Yaw Right is On", "text/plain");
                    } else if (runSpin == 0) {
                        Q_EMIT spinYawRight(false);
                        res.set_content("Yaw Right is Off", "text/plain");
                    } else {
                        res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                    }
                } else {
                    res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                }
            } else {
                res.set_content("Supply parameter on with value 0 or 1", "text/plain");
            }
        });

        registerPost("/spin_roll_ccw", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("on")) {
                int runSpin = -1;
                if (stringToInt(req.get_param_value("on"), runSpin)) {
                    if (runSpin == 1) {
                        Q_EMIT spinRollCCW(true);
                        res.set_content("Roll CCW is On", "text/plain");
                    } else if (runSpin == 0) {
                        Q_EMIT spinRollCCW(false);
                        res.set_content("Roll CCW is Off", "text/plain");
                    } else {
                        res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                    }
                } else {
                    res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                }
            } else {
                res.set_content("Supply parameter on with value 0 or 1", "text/plain");
            }
        });

        registerPost("/spin_roll_cw", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("on")) {
                int runSpin = -1;
                if (stringToInt(req.get_param_value("on"), runSpin)) {
                    if (runSpin == 1) {
                        Q_EMIT spinRollCW(true);
                        res.set_content("Roll CW is On", "text/plain");
                    } else if (runSpin == 0) {
                        Q_EMIT spinRollCW(false);
                        res.set_content("Roll CW is Off", "text/plain");
                    } else {
                        res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                    }
                } else {
                    res.set_content("Supply parameter on with value 0 or 1", "text/plain");
                }
            } else {
                res.set_content("Supply parameter on with value 0 or 1", "text/plain");
            }
        });

        registerPost("/orientation_reset", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT orientationAndSpinReset();
            res.set_content("Origin Reset", "text/plain");
        });

        registerPost("/surface_transition", [this](const httplib::Request &req, httplib::Response &res) {
            Q_EMIT runSurfaceTransition();

            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->surfaceTransitionTime()), "text/plain");
                if (req.has_param("format")) {
                    res.set_content(formatTime(m_mpv->surfaceTransitionTime(), req.get_param_value("format")), "text/plain");
                } else {
                    res.set_content(std::to_string(m_mpv->surfaceTransitionTime()), "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        });

        auto slideNameHandler = [this](const httplib::Request&, httplib::Response& res) {
            if (m_slidesModel) {
                LayersModel* layer = m_slidesModel->selectedSlide();
                if (layer) {
                    res.set_content(layer->getLayersName().toStdString(), "text/plain");
                }
                else {
                    res.set_content("", "text/plain");
                }
            }
            else {
                res.set_content("", "text/plain");
            }
        };
        svr.Get("/slide_name", slideNameHandler);
        registerPost("/slide_name", slideNameHandler);

        registerPost("/slide_previous", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT slidePrevious();
            res.set_content("Previous slide", "text/plain");
        });

        registerPost("/slide_next", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT slideNext();
            res.set_content("Next slide", "text/plain");
        });

        auto slidesHandler = [this](const httplib::Request& req, httplib::Response& res) {
            std::string charsPerItem = "";
            if (req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            res.set_content(getSlideItems(charsPerItem), "text/plain");
        };
        svr.Get("/slides", slidesHandler);
        registerPost("/slides", slidesHandler);

        auto playingInSlidesHandler = [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(getPlaylingItemIndexFromSlides(), "text/plain");
        };
        svr.Get("/playing_in_slides", playingInSlidesHandler);
        registerPost("/playing_in_slides", playingInSlidesHandler);

        auto resetSlidesToStartHandler = [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT resetSlidesToStart();
            res.set_content("Reset slides to start position", "text/plain");
        };
        svr.Get("/slides_reset_to_start", resetSlidesToStartHandler);
        registerPost("/slides_reset_to_start", resetSlidesToStartHandler);
        
        registerPost("/select_from_slides", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(SelectIndexFromSlides(indexStr), "text/plain");
            }
            else {
                res.set_content("Missing index parameter", "text/plain");
            }
        });

        registerPost("/load_from_slides", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromSlides(indexStr), "text/plain");
            }
            else {
                res.set_content("Missing index parameter", "text/plain");
            }
        });

        auto layersHandler = [this](const httplib::Request& req, httplib::Response& res) {
            std::string charsPerItem = "";
            if (req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            if (req.has_param("slide_name")) {
                std::string slideName = req.get_param_value("slide_name");
                res.set_content(getLayerItems(slideName, charsPerItem), "text/plain");
            }
            else if (req.has_param("slide_idx")) {
                std::string slideIdx = req.get_param_value("slide_idx");
                int index;
                if (stringToInt(slideIdx, index)) {
                    res.set_content(getLayerItems(index, charsPerItem), "text/plain");
                }
                else {
                    res.set_content("Could not interpret index parameter", "text/plain");
                }
            }
            else {
                res.set_content("Missing slide_name or slide_idx parameter", "text/plain");
            }
        };
        svr.Get("/layers", layersHandler);
        registerPost("/layers", layersHandler);

        auto layerVolumeGetHandler = [this](const httplib::Request& req, httplib::Response& res) {
            LayersModel* layerModel = nullptr;
            int layerIdx = -1;
            BaseLayer* layer = nullptr;
            res.set_content(getLayerFromRequest(req, layerModel, layerIdx, layer), "text/plain");
            if (layer) {
                res.set_content(std::to_string(layer->volume()), "text/plain");
            }
        };
        svr.Get("/layer_volume", layerVolumeGetHandler);
        registerPost("/layer_volume", [this](const httplib::Request& req, httplib::Response& res) {
            LayersModel* layerModel = nullptr;
            int layerIdx = -1;
            BaseLayer* layer = nullptr;
            res.set_content(getLayerFromRequest(req, layerModel, layerIdx, layer), "text/plain");
            if (layer) {
                if (req.has_param("level")) {
                    int volumeLevel = 0;
                    if (stringToInt(req.get_param_value("level"), volumeLevel)) {
                        if (volumeLevel >= 0 && volumeLevel <= 100) {
                            layer->setVolume(volumeLevel);
                            if (layerModel) layerModel->updateLayerRow(layer);
                        }
                    }
                }
                res.set_content(std::to_string(layer->volume()), "text/plain");
            }
        });

        auto layerVisibilityGetHandler = [this](const httplib::Request& req, httplib::Response& res) {
            LayersModel* layerModel = nullptr;
            int layerIdx = -1;
            BaseLayer* layer = nullptr;
            res.set_content(getLayerFromRequest(req, layerModel, layerIdx, layer), "text/plain");
            if (layer) {
                res.set_content(std::to_string(static_cast<int>(layer->alpha()*100.f)), "text/plain");
            }
        };
        svr.Get("/layer_visibility", layerVisibilityGetHandler);
        registerPost("/layer_visibility", [this](const httplib::Request& req, httplib::Response& res) {
            LayersModel* layerModel = nullptr;
            int layerIdx = -1;
            BaseLayer* layer = nullptr;
            res.set_content(getLayerFromRequest(req, layerModel, layerIdx, layer), "text/plain");
            if (layer) {
                if (req.has_param("value")) {
                    int value = 0;
                    if (stringToInt(req.get_param_value("value"), value)) {
                        if (value >= 0 && value <= 100) {
                            layer->setAlpha(static_cast<float>(value) * 0.01f);
                            if (layerModel) layerModel->updateLayerRow(layer);
                        }
                    }
                }
                res.set_content(std::to_string(static_cast<int>(layer->alpha()*100.f)), "text/plain");
            }
        });

        auto layerPlaneGetHandler = [this](const httplib::Request& req, httplib::Response& res) {
            LayersModel* layerModel = nullptr;
            int layerIdx = -1;
            BaseLayer* layer = nullptr;
            res.set_content(getLayerFromRequest(req, layerModel, layerIdx, layer), "text/plain");
            if (layer) {
                    std::string returnString = "";
                    // Guard against unsigned underflow when fewer than 2 params provided
                    size_t countParams = req.params.size() > 2 ? req.params.size() - 2 : 0;
                    size_t count = 0;
                    if (req.has_param("azimuth")) {
                        returnString += std::to_string(static_cast<int>(layer->planeAzimuth()));
                        count++;
                        if (count < countParams) returnString += "\n";
                    }
                    if (req.has_param("elevation")) {
                        returnString += std::to_string(static_cast<int>(layer->planeElevation()));
                        count++;
                        if (count < countParams) returnString += "\n";
                    }
                    if (req.has_param("roll")) {
                        returnString += std::to_string(static_cast<int>(layer->planeRoll()));
                        count++;
                        if (count < countParams) returnString += "\n";
                    }
                    if (req.has_param("distance")) {
                        returnString += std::to_string(static_cast<int>(layer->planeDistance()));
                        count++;
                        if (count < countParams) returnString += "\n";
                    }
                    if (req.has_param("horizontal")) {
                        returnString += std::to_string(static_cast<int>(layer->planeHorizontal()));
                        count++;
                        if (count < countParams) returnString += "\n";
                    }
                    if (req.has_param("vertical")) {
                        returnString += std::to_string(static_cast<int>(layer->planeVertical()));
                        count++;
                        if (count < countParams) returnString += "\n";
                    }
                    for (const auto &parameter : {"circular", "centreDegrees", "widthDegrees"}) {
                        if (!req.has_param(parameter)) continue;
                        if (count > 0 && !returnString.empty() && returnString.back() != '\n') returnString += "\n";
                        if (std::string(parameter) == "circular") returnString += layer->planeCircular() ? "1" : "0";
                        else if (std::string(parameter) == "centreDegrees") returnString += std::to_string(layer->planeCentreDegrees());
                        else returnString += std::to_string(layer->planeWidthDegrees());
                        ++count;
                    }
                    res.set_content(returnString, "text/plain");
            }
        };
        svr.Get("/layer_plane", layerPlaneGetHandler);
        registerPost("/layer_plane", [this](const httplib::Request& req, httplib::Response& res) {
            LayersModel* layerModel = nullptr;
            int layerIdx = -1;
            BaseLayer* layer = nullptr;
            res.set_content(getLayerFromRequest(req, layerModel, layerIdx, layer), "text/plain");
            if (layer) {
                    // Validate the new fields before changing any plane property.
                    double centreDegrees = layer->planeCentreDegrees(), widthDegrees = layer->planeWidthDegrees();
                    bool circular = layer->planeCircular();
                    if (req.has_param("circular") && !req.get_param_value("circular").empty()) {
                        const auto value = req.get_param_value("circular");
                        if (value != "0" && value != "1" && value != "true" && value != "false") {
                            res.status = 400; res.set_content("circular must be 0, 1, true or false", "text/plain"); return;
                        }
                        circular = value == "1" || value == "true";
                    }
                    for (const auto &parameter : {"centreDegrees", "widthDegrees"}) {
                        if (!req.has_param(parameter) || req.get_param_value(parameter).empty()) continue;
                        bool valid = false;
                        const double value = QString::fromStdString(req.get_param_value(parameter)).toDouble(&valid);
                        if (!valid || !std::isfinite(value) || (std::string(parameter) == "widthDegrees" && (value <= 0 || value > 360))) {
                            res.status = 400; res.set_content("Expected finite degrees; widthDegrees must be greater than 0 and at most 360", "text/plain"); return;
                        }
                        if (std::string(parameter) == "centreDegrees") centreDegrees = value; else widthDegrees = value;
                    }
                    bool updateLayer = false;
                    std::string returnString = "";
                    // Guard against unsigned underflow when fewer than 2 params provided
                    size_t countParams = req.params.size() > 2 ? req.params.size() - 2 : 0;
                    size_t count = 0;
                    if (req.has_param("azimuth")) {
                        int value = 0;
                        if (stringToInt(req.get_param_value("azimuth"), value)) {
                            layer->setPlaneAzimuth(static_cast<double>(value));
                            updateLayer = true;
                        }
                        returnString += std::to_string(static_cast<int>(layer->planeAzimuth()));
                        count++;
                        if (count < countParams)
                            returnString += "\n";
                    }
                    if (req.has_param("elevation")) {
                        int value = 0;
                        if (stringToInt(req.get_param_value("elevation"), value)) {
                            layer->setPlaneElevation(static_cast<double>(value));
                            updateLayer = true;
                        }
                        returnString += std::to_string(static_cast<int>(layer->planeElevation()));
                        count++;
                        if (count < countParams)
                            returnString += "\n";
                    }
                    if (req.has_param("roll")) {
                        int value = 0;
                        if (stringToInt(req.get_param_value("roll"), value)) {
                            layer->setPlaneRoll(static_cast<double>(value));
                            updateLayer = true;
                        }
                        returnString += std::to_string(static_cast<int>(layer->planeRoll()));
                        count++;
                        if (count < countParams)
                            returnString += "\n";
                    }
                    if (req.has_param("distance")) {
                        int value = 0;
                        if (stringToInt(req.get_param_value("distance"), value)) {
                            layer->setPlaneDistance(static_cast<double>(value));
                            updateLayer = true;
                        }
                        returnString += std::to_string(static_cast<int>(layer->planeDistance()));
                        count++;
                        if (count < countParams)
                            returnString += "\n";
                    }
                    if (req.has_param("horizontal")) {
                        int value = 0;
                        if (stringToInt(req.get_param_value("horizontal"), value)) {
                            layer->setPlaneHorizontal(static_cast<double>(value));
                            updateLayer = true;
                        }
                        returnString += std::to_string(static_cast<int>(layer->planeHorizontal()));
                        count++;
                        if (count < countParams)
                            returnString += "\n";
                    }
                    if (req.has_param("vertical")) {
                        int value = 0;
                        if (stringToInt(req.get_param_value("vertical"), value)) {
                            layer->setPlaneVertical(static_cast<double>(value));
                            updateLayer = true;
                        }
                        returnString += std::to_string(static_cast<int>(layer->planeVertical()));
                        count++;
                        if (count < countParams)
                            returnString += "\n";
                    }
                    if (req.has_param("circular") && !req.get_param_value("circular").empty()) { layer->setPlaneCircular(circular); updateLayer = true; }
                    if (req.has_param("centreDegrees") && !req.get_param_value("centreDegrees").empty()) { layer->setPlaneCentreDegrees(centreDegrees); updateLayer = true; }
                    if (req.has_param("widthDegrees") && !req.get_param_value("widthDegrees").empty()) { layer->setPlaneWidthDegrees(widthDegrees); updateLayer = true; }
                    for (const auto &parameter : {"circular", "centreDegrees", "widthDegrees"}) {
                        if (!req.has_param(parameter)) continue;
                        if (count > 0 && !returnString.empty() && returnString.back() != '\n') returnString += "\n";
                        if (std::string(parameter) == "circular") returnString += layer->planeCircular() ? "1" : "0";
                        else if (std::string(parameter) == "centreDegrees") returnString += std::to_string(layer->planeCentreDegrees());
                        else returnString += std::to_string(layer->planeWidthDegrees());
                        ++count;
                    }
                    if (updateLayer) {
                        if (layerModel) layerModel->updateLayerRow(layer);
                    }
                    res.set_content(returnString, "text/plain");
            }
        });

    }
}

void HttpServerThread::registerPost(const std::string &path, httplib::Server::Handler handler) {
    m_postHandlers[path] = handler;
    svr.Post(path, std::move(handler));
}

QJsonObject HttpServerThread::executeControlAction(const QString &path, const QJsonObject &parameters) {
    const auto it = m_postHandlers.find(path.toStdString());
    if (it == m_postHandlers.end() || !m_mpv || !m_slidesModel)
        return {{QStringLiteral("accepted"), false}, {QStringLiteral("message"), tr("Player action unavailable")}};
    httplib::Request request;
    request.method = "POST";
    request.path = path.toStdString();
    for (auto p = parameters.begin(); p != parameters.end(); ++p)
        request.params.emplace(p.key().toStdString(), p.value().toString().toStdString());
    httplib::Response response;
    it->second(request, response);
    const int status = response.status < 0 ? 200 : response.status;
    return {{QStringLiteral("accepted"), status >= 200 && status < 300},
        {QStringLiteral("status"), status}, {QStringLiteral("message"), QString::fromStdString(response.body)}};
}

void HttpServerThread::terminate() {
    if (m_eventFeed) m_eventFeed->stop();
    {
        QMutexLocker lock(&mutex);
        abort = true;
        runServer = false;
    }
    svr.stop();
}

void HttpServerThread::run() {
    mutex.lock();
    if (abort) {
        svr.stop();
        abort = false;
        runServer = false;
    }
    mutex.unlock();

    if (runServer) {
        svr.listen("0.0.0.0", portServer);
    }
}

bool HttpServerThread::stringToInt(std::string str, int &parsedInt) {
    if (!str.empty()) {
        try {
            parsedInt = std::stoi(str);
            return true;
        } catch (std::invalid_argument const &) {
            return false;
        } catch (std::out_of_range const &) {
            return false;
        }
    }
    return false;
}

bool HttpServerThread::stringToDouble(std::string str, double &parsedDouble) {
    if (!str.empty()) {
        try {
            parsedDouble = std::stod(str);
            return true;
        } catch (std::invalid_argument const &) {
            return false;
        } catch (std::out_of_range const &) {
            return true;
        }
    }
    return false;
}

void HttpServerThread::setPositionFromStr(std::string positionTimeStr) {
    double pos = 0;
    if (stringToDouble(positionTimeStr, pos)) {
        if (pos >= 0) {
            Q_EMIT setPosition(pos);
        }
    }
}

void HttpServerThread::setSpeedFromStr(std::string speedFactorStr) {
    double factor = 1;
    if (stringToDouble(speedFactorStr, factor)) {
        if (factor >= 0.01 && factor <= 100) {
            Q_EMIT setSpeed(factor);
        }
    }
}

void HttpServerThread::setVolumeFromStr(std::string volumeLevelStr) {
    int volumeLevel = 0;
    if (stringToInt(volumeLevelStr, volumeLevel)) {
        if (volumeLevel >= 0 && volumeLevel <= 100) {
            Q_EMIT setVolume(volumeLevel);
        }
    }
}

void HttpServerThread::setViewModeFromStr(std::string viewModeStr) {
    int viewMode = 0;
    if (stringToInt(viewModeStr, viewMode)) {
        if (viewMode >= 0 && viewMode <= 1) {
            Q_EMIT setViewMode(viewMode);
        }
    }
}

void HttpServerThread::setSyncImageFadingFromStr(std::string valueStr) {
    int value = 0;
    if (stringToInt(valueStr, value)) {
        if (value == 0) {
            Q_EMIT setSyncVolumeVisibilityFading(false);
        } else if (value <= 1) {
            Q_EMIT setSyncVolumeVisibilityFading(true);
        }
    }
}

const std::string HttpServerThread::getAudioTracksItems(std::string charsPerItemStr, std::string removeLoadedFilePrefixStr) {
    if (m_mpv) {
        int removeLoadedFilePrefix = 0;
        std::string prefixToRemove = "";
        if (stringToInt(removeLoadedFilePrefixStr, removeLoadedFilePrefix)) {
            if (removeLoadedFilePrefix == 1)
                prefixToRemove = m_mpv->getProperty(QStringLiteral("filename")).toString().toStdString();
        }
        int charsPerItem = 0;
        if (stringToInt(charsPerItemStr, charsPerItem))
            return m_mpv->audioTracksModel()->getListAsFormattedString(prefixToRemove, charsPerItem);
        else
            return m_mpv->audioTracksModel()->getListAsFormattedString(prefixToRemove);
    } else
        return "";
}

const std::string HttpServerThread::getPlayListItems(std::string charsPerItemStr) {
    if (m_mpv) {
        int charsPerItem = 0;
        if (stringToInt(charsPerItemStr, charsPerItem))
            return m_mpv->getPlayListModel()->getListAsFormattedString(charsPerItem);
        else
            return m_mpv->getPlayListModel()->getListAsFormattedString();
    } else
        return "";
}

const std::string HttpServerThread::getSectionsItems(std::string charsPerItemStr) {
    if (m_mpv) {
        int charsPerItem = 0;
        if (stringToInt(charsPerItemStr, charsPerItem))
            return m_mpv->getPlaySectionsModel()->getSectionsAsFormattedString(charsPerItem);
        else
            return m_mpv->getPlaySectionsModel()->getSectionsAsFormattedString();
    } else
        return "";
}

const std::string HttpServerThread::getSlideItems(std::string charsPerItemStr) {
    if (m_slidesModel) {
        int charsPerItem = 0;
        if (stringToInt(charsPerItemStr, charsPerItem))
            return m_slidesModel->getSlidesAsFormattedString(charsPerItem);
        else
            return m_slidesModel->getSlidesAsFormattedString();
    }
    else
        return "";
}

const std::string HttpServerThread::getLayerItems(std::string slideName, std::string charsPerItemStr) {
    if (m_slidesModel && !slideName.empty()) {
        LayersModel* lm = m_slidesModel->slide(slideName);
        if (!lm) {
            return "Could not find slide with name : " + slideName;
        }
        int charsPerItem = 0;
        if (stringToInt(charsPerItemStr, charsPerItem))
            return lm->getLayersAsFormattedString(charsPerItem);
        else
            return lm->getLayersAsFormattedString();
    }
    else
        return "";
}

const std::string HttpServerThread::getLayerItems(int slideIdx, std::string charsPerItemStr) {
    if (m_slidesModel) {
        LayersModel* lm = m_slidesModel->slide(slideIdx);
        if (!lm) {
            return "Could not find slide with idx : " + std::to_string(slideIdx);
        }
        int charsPerItem = 0;
        if (stringToInt(charsPerItemStr, charsPerItem))
            return lm->getLayersAsFormattedString(charsPerItem);
        else
            return lm->getLayersAsFormattedString();
    }
    else
        return "";
}

const std::string HttpServerThread::getPlaylingItemIndexFromAudioTracks() {
    if (m_mpv) {
        return std::to_string(m_mpv->audioId() - 1);
    }
    return "-1";
}

const std::string HttpServerThread::getPlaylingItemIndexFromPlaylist() {
    if (m_mpv) {
        return std::to_string(m_mpv->getPlayListModel()->getPlayingVideo());
    }
    return "-1";
}

const std::string HttpServerThread::getPlaylingItemIndexFromSections() {
    if (m_mpv) {
        return std::to_string(m_mpv->getPlaySectionsModel()->getPlayingSection());
    }
    return "-1";
}

const std::string HttpServerThread::getPlaylingItemIndexFromSlides() {
    if (m_slidesModel) {
        return std::to_string(m_slidesModel->selectedSlideIdx());
    }
    return "-1";
}

const std::string HttpServerThread::LoadIndexFromAudioTracks(std::string indexStr) {
    int index = -1;
    if (stringToInt(indexStr, index)) {
        if (m_mpv) {
            TracksModel* tracks = m_mpv->audioTracksModel();
            if (!tracks) {
                return "Audio tracks model unavailable";
            }
            if (index >= 0 && index < tracks->countTracks()) {
                Q_EMIT loadFromAudioTracks(index);
                return "Loading audio track with index: " + indexStr;
            } else {
                return "Index was out of bounds of audio tracks";
            }
        } else {
            return "Could not find reference to MPV";
        }
    } else {
        return "Could not interpret index parameter";
    }
}

const std::string HttpServerThread::LoadIndexFromPlaylist(std::string indexStr) {
    int index = -1;
    if (stringToInt(indexStr, index)) {
        if (m_mpv) {
            PlayListModel* pl = m_mpv->getPlayListModel();
            if (!pl) {
                return "Playlist model unavailable";
            }
            if (index >= 0 && index < pl->getPlayListSize()) {
                Q_EMIT loadFromPlaylist(index);
                return "Loading media with index: " + indexStr;
            } else {
                return "Index was out of bounds of playlist";
            }
        } else {
            return "Could not find reference to MPV";
        }
    } else {
        return "Could not interpret index parameter";
    }
}

const std::string HttpServerThread::LoadIndexFromSections(std::string indexStr) {
    int index = -1;
    if (stringToInt(indexStr, index)) {
        if (m_mpv) {
            PlaySectionsModel* sm = m_mpv->getPlaySectionsModel();
            if (!sm) {
                return "Sections model unavailable";
            }
            if (index >= -1 && index < sm->getNumberOfSections()) {
                Q_EMIT loadFromSections(index);
                return "Loading section with index: " + indexStr;
            } else {
                return "Index was out of bounds of sections list";
            }
        } else {
            return "Could not find reference to MPV";
        }
    } else {
        return "Could not interpret index parameter";
    }
}

const std::string HttpServerThread::LoadIndexFromSlides(std::string indexStr) {
    int index = -1;
    if (stringToInt(indexStr, index)) {
        if (m_slidesModel) {
            if (index >= 0 && index < m_slidesModel->numberOfSlides()) {
                Q_EMIT loadFromSlides(index);
                return "Loading slide with index: " + indexStr;
            }
            else {
                return "Index was out of bounds of slide list";
            }
        }
        else {
            return "Could not find reference to SlideModel";
        }
    }
    else {
        return "Could not interpret index parameter";
    }
}

const std::string HttpServerThread::SelectIndexFromSlides(std::string indexStr) {
    int index = -1;
    if (stringToInt(indexStr, index)) {
        if (m_slidesModel) {
            if (index >= 0 && index < m_slidesModel->numberOfSlides()) {
                Q_EMIT selectFromSlides(index);
                return "Selecting slide with index: " + indexStr;
            }
            else {
                return "Index was out of bounds of slide list";
            }
        }
        else {
            return "Could not find reference to SlideModel";
        }
    }
    else {
        return "Could not interpret index parameter";
    }
}

const std::string HttpServerThread::getLayerFromRequest(const httplib::Request& req, LayersModel*& lm, int& layerIdx, BaseLayer*& resolvedLayer) {
    lm = nullptr;
    layerIdx = -1;
    resolvedLayer = nullptr;
    if (m_slidesModel) {
        if (req.has_param("layer_title")) {
            lm = m_slidesModel->masterSlide();
            if (req.has_param("slide_name")) {
                std::string slideNameStr = req.get_param_value("slide_name");
                lm = m_slidesModel->slide(slideNameStr);
                if (!lm) {
                    return "Could not find slide with name : " + slideNameStr;
                }
            }
            if (lm) {
                std::string layerTitleStr = req.get_param_value("layer_title");
                // layer_idx / layer_title address the slide's own layers, not references.
                layerIdx = lm->layerIdx(layerTitleStr);
                BaseLayer* layer = lm->layerShared(layerIdx).get();
                if (!layer) {
                    return "Could not find layer with title: " + layerTitleStr;
                }
                resolvedLayer = layer;
            }
            else {
                return "Master slide missing";
            }
        }
        else if (req.has_param("layer_idx")) {
            lm = m_slidesModel->masterSlide();
            if (req.has_param("slide_idx")) {
                std::string slideIdx = req.get_param_value("slide_idx");
                int index;
                if (stringToInt(slideIdx, index)) {
                    lm = m_slidesModel->slide(index);
                    if (!lm) {
                        return "Could not find slide with idx : " + slideIdx;
                    }
                }
                else {
                    return "Could not interpret slide_idx as integer.";
                }
            }

            if (lm) {
                std::string layerIdxStr = req.get_param_value("layer_idx");
                if (stringToInt(layerIdxStr, layerIdx)) {
                    BaseLayer* layer = lm->layerShared(layerIdx).get();
                    if (!layer) {
                        return "Could not find layer with idx : " + layerIdxStr;
                    }
                    resolvedLayer = layer;
                }
                else {
                    return "Could not interpret layer_idx as integer.";
                }
            }
            else {
                return "Master slide missing";
            }
        }
        else {
            return "Missing layer title or index parameter";
        }

        // Resolve sublayer if requested
        if (resolvedLayer && resolvedLayer->hasSubLayers()) {
            auto& subLayers = resolvedLayer->getSubLayers();
            if (req.has_param("sub_layer_title")) {
                std::string subTitle = req.get_param_value("sub_layer_title");
                bool found = false;
                for (auto& sub : subLayers) {
                    if (sub && sub->title() == subTitle) {
                        resolvedLayer = sub.get();
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    return "Could not find sub-layer with title: " + subTitle;
                }
            }
            else if (req.has_param("sub_layer_idx")) {
                int subIdx = -1;
                std::string subIdxStr = req.get_param_value("sub_layer_idx");
                if (stringToInt(subIdxStr, subIdx)) {
                    if (subIdx >= 0 && subIdx < static_cast<int>(subLayers.size())) {
                        resolvedLayer = subLayers[subIdx].get();
                    }
                    else {
                        return "Sub-layer index out of bounds: " + subIdxStr;
                    }
                }
                else {
                    return "Could not interpret sub_layer_idx as integer.";
                }
            }
        }
        else if (req.has_param("sub_layer_title") || req.has_param("sub_layer_idx")) {
            return "Layer has no sub-layers";
        }

        if (resolvedLayer) {
            return "Found layer: " + resolvedLayer->title();
        }
        return "Missing layer title or index parameter";
    }
    else {
        return "No slides model found";
    }
}

void HttpServerThread::setMpv(MpvObject *mpv) {
    if (m_mpv == mpv) {
        return;
    }
    m_mpv = mpv;
}

void HttpServerThread::setSlidesModel(SlidesModel* sm) {
    if (m_slidesModel == sm) {
        return;
    }
    m_slidesModel = sm;
}
