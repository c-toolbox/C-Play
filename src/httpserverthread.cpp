/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "httpserverthread.h"
#include "mpvobject.h"
#include "playbacksettings.h"
#include "playercontroller.h"
#include "slidesmodel.h"
#include "layersmodel.h"

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
      runServer(false),
      portServer(7007),
      m_mpv(nullptr),
      m_slidesModel(nullptr) {
}

HttpServerThread::~HttpServerThread() {
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();
}

void HttpServerThread::setupHttpServer() {
    QFile httpServerConfFile(QStringLiteral("./data/http-server-conf.json"));

    if (!httpServerConfFile.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open http server configuration file.");
        runServer = false;
        return;
    }

    QByteArray httpServerConfigArray = httpServerConfFile.readAll();
    QJsonDocument httpServerConfDoc(QJsonDocument::fromJson(httpServerConfigArray));
    QJsonObject httpServerConf = httpServerConfDoc.object();

    QString runServerStr = QStringLiteral("");
    if (httpServerConf.contains(QStringLiteral("run"))) {
        runServerStr = httpServerConf.value(QStringLiteral("run")).toString();
    } else {
        qWarning("Couldn't find run parameter in http server configuration file.");
        runServer = false;
        return;
    }

    if (httpServerConf.contains(QStringLiteral("port"))) {
        portServer = httpServerConf.value(QStringLiteral("port")).toInt();
    } else {
        qWarning("Couldn't find port parameter in http server configuration file.");
        runServer = false;
        return;
    }

    if (runServerStr == QStringLiteral("yes")) {
        svr.Post("/status", [this](const httplib::Request &, httplib::Response &res) {
            res.set_content("OK", "text/plain");
        });

        svr.Post("/play", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT playMedia();
            res.set_content("Play", "text/plain");
        });

        svr.Post("/pause", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT pauseMedia();
            res.set_content("Pause", "text/plain");
        });

        svr.Post("/stop", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT rewindMedia();
            res.set_content("Stop/rewind", "text/plain");
        });

        svr.Post("/rewind", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT rewindMedia();
            res.set_content("Stop/rewind", "text/plain");
        });

        svr.Post("/position", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("set")) {
                setPositionFromStr(req.get_param_value("set"));
            }

            if (m_mpv) {
                if (req.has_param("format")) {
                    res.set_content(formatTime(m_mpv->position(), req.get_param_value("format")), "text/plain");
                } else {
                    res.set_content(std::to_string(m_mpv->position()), "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/seek", [this](const httplib::Request &req, httplib::Response &res) {
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
                        res.set_content("Seeking " + req.get_param_value("time") + " s", "text / plain");
                    }
                } else {
                    res.set_content("Supply parameter time with a positive or negative value (in seconds)", "text/plain");
                }
            } else {
                res.set_content("Supply parameter time with a positive or negative value (in seconds)", "text/plain");
            }
        });

        svr.Post("/remaining", [this](const httplib::Request &req, httplib::Response &res) {
            if (m_mpv) {
                if (req.has_param("format")) {
                    res.set_content(formatTime(m_mpv->remaining(), req.get_param_value("format")), "text/plain");
                } else {
                    res.set_content(std::to_string(m_mpv->remaining()), "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/duration", [this](const httplib::Request &req, httplib::Response &res) {
            if (m_mpv) {
                if (req.has_param("format")) {
                    res.set_content(formatTime(m_mpv->duration(), req.get_param_value("format")), "text/plain");
                } else {
                    res.set_content(std::to_string(m_mpv->duration()), "text/plain");
                }
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/auto_play", [this](const httplib::Request &req, httplib::Response &res) {
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
                if (m_mpv) {
                    if (m_mpv->autoPlay()) {
                        res.set_content("1", "text/plain");
                    } else {
                        res.set_content("0", "text/plain");
                    }
                } else {
                    res.set_content("0", "text/plain");
                }
            }
        });

        svr.Post("/speed", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("factor")) {
                setSpeedFromStr(req.get_param_value("factor"));
            }

            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->speed()), "text/plain");
            }
            else {
                res.set_content("1", "text/plain");
            }
        });

        svr.Post("/volume", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("level")) {
                setVolumeFromStr(req.get_param_value("level"));
            }

            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->volume()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/fade_duration", [this](const httplib::Request &req, httplib::Response &res) {
            double fadeDurationTime = double(PlaybackSettings::fadeDuration()) / 1000.0;
            if (req.has_param("format")) {
                res.set_content(formatTime(fadeDurationTime, req.get_param_value("format")), "text/plain");
            } else {
                res.set_content(std::to_string(fadeDurationTime), "text/plain");
            }
        });

        svr.Post("/fade_volume_down", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT fadeVolumeDown();
            res.set_content("Fading volume down", "text/plain");
        });

        svr.Post("/fade_volume_up", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT fadeVolumeUp();
            res.set_content("Fading volume up", "text/plain");
        });

        svr.Post("/fade_image_down", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT fadeImageDown();
            res.set_content("Fading image down", "text/plain");
        });

        svr.Post("/fade_image_up", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT fadeImageUp();
            res.set_content("Fading image up", "text/plain");
        });

        svr.Post("/sync_image_volume_fade", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("value")) {
                std::string valueStr = req.get_param_value("value");
                setSyncImageFadingFromStr(valueStr);
                res.set_content("Setting syncImageVolumeFade to " + valueStr, "text/plain");
            } else {
                if (m_mpv) {
                    res.set_content(std::to_string(m_mpv->syncVolumeVisibilityFading()), "text/plain");
                } else {
                    res.set_content("0", "text/plain");
                }
            }
        });

        svr.Post("/visibility", [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->visibility()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/stereo_mode", [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->stereoscopicMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/grid_mode", [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->gridToMapOn()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/background_image", [this](const httplib::Request &req, httplib::Response &res) {
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
            }
        });

        svr.Post("/background_image_stereo_mode", [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                res.set_content(std::to_string(playctrl->backgroundStereoMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/background_image_grid_mode", [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                res.set_content(std::to_string(playctrl->backgroundGridMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/foreground_image", [this](const httplib::Request &req, httplib::Response &res) {
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
            }
        });

        svr.Post("/foreground_image_stereo_mode", [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                res.set_content(std::to_string(playctrl->foregroundStereoMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/foreground_image_grid_mode", [this](const httplib::Request &, httplib::Response &res) {
            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                res.set_content(std::to_string(playctrl->foregroundGridMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/eof_mode", [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->eofMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/loop_mode", [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->eofMode()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/view_mode", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("mode")) {
                setViewModeFromStr(req.get_param_value("mode"));
            }

            PlayerController *playctrl = qobject_cast<PlayerController *>(parent());
            if (playctrl) {
                res.set_content(std::to_string(playctrl->getViewModeOnClients()), "text/plain");
            } else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/media_title", [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                res.set_content(m_mpv->mediaTitle().toStdString(), "text/plain");
            } else {
                res.set_content("", "text/plain");
            }
        });

        svr.Post("/audiotracks", [this](const httplib::Request &req, httplib::Response &res) {
            std::string charsPerItem = "";
            std::string removeLoadedFilePrefix = "";
            if (req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            if (req.has_param("removeLoadedFilePrefix")) {
                removeLoadedFilePrefix = req.get_param_value("removeLoadedFilePrefix");
            }
            res.set_content(getAudioTracksItems(charsPerItem, removeLoadedFilePrefix), "text/plain");
        });

        svr.Post("/playing_in_audiotracks", [this](const httplib::Request &, httplib::Response &res) {
            res.set_content(getPlaylingItemIndexFromAudioTracks(), "text/plain");
        });

        svr.Post("/load_from_audiotracks", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromAudioTracks(indexStr), "text/plain");
            }
            res.set_content("Missing index parameter", "text/plain");
        });

        svr.Post("/playlist", [this](const httplib::Request &req, httplib::Response &res) {
            std::string charsPerItem = "";
            if (req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            res.set_content(getPlayListItems(charsPerItem), "text/plain");
        });

        svr.Post("/playing_in_playlist", [this](const httplib::Request &, httplib::Response &res) {
            res.set_content(getPlaylingItemIndexFromPlaylist(), "text/plain");
        });

        svr.Post("/load_from_playlist", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromPlaylist(indexStr), "text/plain");
            }
            res.set_content("Missing index parameter", "text/plain");
        });

        svr.Post("/sections", [this](const httplib::Request &req, httplib::Response &res) {
            std::string charsPerItem = "";
            if (req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            res.set_content(getSectionsItems(charsPerItem), "text/plain");
        });

        svr.Post("/playing_in_sections", [this](const httplib::Request &, httplib::Response &res) {
            res.set_content(getPlaylingItemIndexFromSections(), "text/plain");
        });

        svr.Post("/load_from_sections", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromSections(indexStr), "text/plain");
            }
            res.set_content("Missing index parameter", "text/plain");
        });

        svr.Post("/section_start_time", [this](const httplib::Request &req, httplib::Response &res) {
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
        });

        svr.Post("/section_end_time", [this](const httplib::Request &req, httplib::Response &res) {
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
        });

        svr.Post("/section_end_mode", [this](const httplib::Request &, httplib::Response &res) {
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
        });

        svr.Post("/playfile_json", [this](const httplib::Request &, httplib::Response &res) {
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
        });

        svr.Post("/playlist_json", [this](const httplib::Request &, httplib::Response &res) {
            if (m_mpv) {
                QJsonDocument doc;
                QJsonObject obj = doc.object();
                m_mpv->getPlayListModel()->asJSON(obj);
                doc.setObject(obj);
                res.set_content(doc.toJson(QJsonDocument::Compact).toStdString(), "application/json");
            } else {
                res.set_content("Could not retrieve playlist", "text/plain");
            }
        });

        svr.Post("/spin_pitch_up", [this](const httplib::Request &req, httplib::Response &res) {
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

        svr.Post("/spin_pitch_down", [this](const httplib::Request &req, httplib::Response &res) {
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

        svr.Post("/spin_yaw_left", [this](const httplib::Request &req, httplib::Response &res) {
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

        svr.Post("/spin_yaw_right", [this](const httplib::Request &req, httplib::Response &res) {
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

        svr.Post("/spin_roll_ccw", [this](const httplib::Request &req, httplib::Response &res) {
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

        svr.Post("/spin_roll_cw", [this](const httplib::Request &req, httplib::Response &res) {
            if (req.has_param("on")) {
                int runSpin = -1;
                if (stringToInt(req.get_param_value("on"), runSpin)) {
                    if (runSpin == 1) {
                        Q_EMIT spinRollCW(true);
                        res.set_content("Roll CCW is On", "text/plain");
                    } else if (runSpin == 0) {
                        Q_EMIT spinRollCW(false);
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

        svr.Post("/orientation_reset", [this](const httplib::Request &, httplib::Response &res) {
            Q_EMIT orientationAndSpinReset();
            res.set_content("Origin Reset", "text/plain");
        });

        svr.Post("/surface_transition", [this](const httplib::Request &req, httplib::Response &res) {
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

        svr.Post("/slide_name", [this](const httplib::Request&, httplib::Response& res) {
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
        });

        svr.Post("/slide_previous", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT slidePrevious();
            res.set_content("Previous slide", "text/plain");
        });

        svr.Post("/slide_next", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT slideNext();
            res.set_content("Next slide", "text/plain");
        });

        svr.Post("/slides", [this](const httplib::Request& req, httplib::Response& res) {
            std::string charsPerItem = "";
            if (req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            res.set_content(getSlideItems(charsPerItem), "text/plain");
        });

        svr.Post("/playing_in_slides", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(getPlaylingItemIndexFromSlides(), "text/plain");
        });
        
        svr.Post("/select_from_slides", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(SelectIndexFromSlides(indexStr), "text/plain");
            }
            res.set_content("Missing index parameter", "text/plain");
        });

        svr.Post("/load_from_slides", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromSlides(indexStr), "text/plain");
            }
            res.set_content("Missing index parameter", "text/plain");
        });

        runServer = true;
        return;
    }

    runServer = false;
    return;
}

void HttpServerThread::terminate() {
    mutex.lock();
    abort = true;
    mutex.unlock();
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
            return true;
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
            if (index >= 0 && index < m_mpv->audioTracksModel()->countTracks()) {
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
            if (index >= 0 && index < m_mpv->getPlayListModel()->getPlayListSize()) {
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
            if (index >= 0 && index < m_mpv->getPlaySectionsModel()->getNumberOfSections()) {
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
