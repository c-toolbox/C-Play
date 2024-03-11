#include "httpserverthread.h"
#include "mpvobject.h"
#include "playbacksettings.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

HttpServerThread::HttpServerThread(QObject *parent)
    : QThread(parent), 
    runServer(false), 
    portServer(7007),
    m_mpv(nullptr)
{
}

HttpServerThread::~HttpServerThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();
}

void HttpServerThread::setupHttpServer()
{
    QFile httpServerConfFile(QStringLiteral("./data/http-server-conf.json"));

    if (!httpServerConfFile.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open http server configuration file.");
        runServer = false;
        return;
    }

    QByteArray httpServerConfigArray = httpServerConfFile.readAll();
    QJsonDocument httpServerConfDoc(QJsonDocument::fromJson(httpServerConfigArray));
    QJsonObject httpServerConf = httpServerConfDoc.object();

    QString runServerStr = "";
    if (httpServerConf.contains("run")) {
        runServerStr = httpServerConf.value("run").toString();
    }
    else {
        qWarning("Couldn't find run parameter in http server configuration file.");
        runServer = false;
        return;
    }

    if (httpServerConf.contains("port")) {
        portServer = httpServerConf.value("port").toInt();
    }
    else {
        qWarning("Couldn't find port parameter in http server configuration file.");
        runServer = false;
        return;
    }

    if (runServerStr == "yes") {
        svr.Post("/status", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content("OK", "text/plain");
        });

        svr.Post("/play", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT playMedia();
            res.set_content("Play", "text/plain");
        });

        svr.Post("/pause", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT pauseMedia();
            res.set_content("Pause", "text/plain");
        });

        svr.Post("/rewind", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT rewindMedia();
            res.set_content("Rewind", "text/plain");
        });

        svr.Post("/volume", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("level")) {
                setVolumeFromStr(req.get_param_value("level"));
            }
            
            if(m_mpv){
                res.set_content(std::to_string(m_mpv->volume()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/fade_duration", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(std::to_string(double(PlaybackSettings::fadeDuration())/1000.0), "text/plain");
        });

        svr.Post("/fade_volume_down", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT fadeVolumeDown();
            res.set_content("Fading volume down", "text/plain");
        });

        svr.Post("/fade_volume_up", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT fadeVolumeUp();
            res.set_content("Fading volume up", "text/plain");
        });

        svr.Post("/fade_image_down", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT fadeImageDown();
            res.set_content("Fading image down", "text/plain");
        });

        svr.Post("/fade_image_up", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT fadeImageUp();
            res.set_content("Fading image up", "text/plain");
        });

        svr.Post("/visibility", [this](const httplib::Request&, httplib::Response& res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->visibility()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/stereo_mode", [this](const httplib::Request&, httplib::Response& res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->stereoscopicMode()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/grid_mode", [this](const httplib::Request&, httplib::Response& res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->gridToMapOn()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/background_stereo_mode", [this](const httplib::Request&, httplib::Response& res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->stereoscopicModeBackground()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/background_grid_mode", [this](const httplib::Request&, httplib::Response& res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->gridToMapOnBackground()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/loop_mode", [this](const httplib::Request&, httplib::Response& res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->loopMode()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/view_mode", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("mode")) {
                setViewModeFromStr(req.get_param_value("mode"));
            }

            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->viewModeOnClients()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/media_title", [this](const httplib::Request&, httplib::Response& res) {
            if (m_mpv) {
                res.set_content(m_mpv->mediaTitle().toStdString(), "text/plain");
            }
            else {
                res.set_content("", "text/plain");
            }
        });

        svr.Post("/position", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("set")) {
                setPositionFromStr(req.get_param_value("set"));
            }
            
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->position()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/remaining", [this](const httplib::Request&, httplib::Response& res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->remaining()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/duration", [this](const httplib::Request&, httplib::Response& res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->duration()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/audiotracks", [this](const httplib::Request& req, httplib::Response& res) {
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

        svr.Post("/playing_in_audiotracks", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(getPlaylingItemIndexFromAudioTracks(), "text/plain");
        });

        svr.Post("/load_from_audiotracks", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromAudioTracks(indexStr), "text/plain");
            }
            res.set_content("Missing index parameter", "text/plain");
        });

        svr.Post("/playlist", [this](const httplib::Request& req, httplib::Response& res) {
            std::string charsPerItem = "";
            if(req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            res.set_content(getPlayListItems(charsPerItem), "text/plain");
        });

        svr.Post("/playing_in_playlist", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(getPlaylingItemIndexFromPlaylist(), "text/plain");
        });

        svr.Post("/load_from_playlist", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromPlaylist(indexStr), "text/plain");
            }
            res.set_content("Missing index parameter", "text/plain");
        });

        svr.Post("/sections", [this](const httplib::Request& req, httplib::Response& res) {
            std::string charsPerItem = "";
            if (req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            res.set_content(getSectionsItems(charsPerItem), "text/plain");
        });

        svr.Post("/playing_in_sections", [this](const httplib::Request&, httplib::Response& res) {
            res.set_content(getPlaylingItemIndexFromSections(), "text/plain");
        });

        svr.Post("/load_from_sections", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromSections(indexStr), "text/plain");
            }
            res.set_content("Missing index parameter", "text/plain");
        });

        svr.Post("/spin_pitch_up", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT spinPitchUp();
            res.set_content("Pitch Up", "text/plain");
        });

        svr.Post("/spin_pitch_down", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT spinPitchDown();
            res.set_content("Pitch Down", "text/plain");
        });

        svr.Post("/spin_yaw_left", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT spinYawLeft();
            res.set_content("Yaw Left", "text/plain");
        });

        svr.Post("/spin_yaw_right", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT spinYawRight();
            res.set_content("Yaw Right", "text/plain");
        });

        svr.Post("/spin_roll_ccw", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT spinRollCCW();
            res.set_content("Roll CCW", "text/plain");
        });

        svr.Post("/spin_roll_cw", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT spinRollCW();
            res.set_content("Roll CCW", "text/plain");
        });

        svr.Post("/orientation_reset", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT orientationAndSpinReset();
            res.set_content("Origin Reset", "text/plain");
        });

        svr.Post("/surface_transition", [this](const httplib::Request&, httplib::Response& res) {
            Q_EMIT runSurfaceTransistion();
            res.set_content("Surface Transition", "text/plain");
        });

        runServer = true;
        return;
    }
    
    runServer = false;
    return;
}

void HttpServerThread::terminate(){
    mutex.lock();
    abort = true;
    mutex.unlock();
}

void HttpServerThread::run()
{
    mutex.lock();
    if (abort) {
        svr.stop();
        abort = false;
        runServer = false;
    }
    mutex.unlock();

    if (runServer) {
        svr.listen("localhost", portServer);
    }
}

bool HttpServerThread::stringToInt(std::string str, int& parsedInt)
{
    if (!str.empty()) {
        try
        {
            parsedInt = std::stoi(str);
            return true;
        }
        catch (std::invalid_argument const&)
        {
            return false;
        }
        catch (std::out_of_range const&)
        {
            return true;
        }
    }
    return false;
}

bool HttpServerThread::stringToDouble(std::string str, double& parsedDouble)
{
    if (!str.empty()) {
        try
        {
            parsedDouble = std::stod(str);
            return true;
        }
        catch (std::invalid_argument const&)
        {
            return false;
        }
        catch (std::out_of_range const&)
        {
            return true;
        }
    }
    return false;
}

void HttpServerThread::setPositionFromStr(std::string positionTimeStr)
{
    double pos = 0;
    if (stringToDouble(positionTimeStr, pos)) {
        if (pos >= 0) {
            Q_EMIT setPosition(pos);
        }
    }
}

void HttpServerThread::setVolumeFromStr(std::string volumeLevelStr)
{
    int volumeLevel = 0;
    if (stringToInt(volumeLevelStr, volumeLevel)) {
        if (volumeLevel >= 0 && volumeLevel <= 100) {
            Q_EMIT setVolume(volumeLevel);
        }
    }
}

void HttpServerThread::setViewModeFromStr(std::string viewModeStr)
{
    int viewMode = 0;
    if (stringToInt(viewModeStr, viewMode)) {
        if (viewMode >= 0 && viewMode <= 1) {
            Q_EMIT setViewMode(viewMode);
        }
    }
}

const std::string HttpServerThread::getAudioTracksItems(std::string charsPerItemStr, std::string removeLoadedFilePrefixStr)
{
    if (m_mpv) {
        int removeLoadedFilePrefix = 0;
        std::string prefixToRemove = "";
        if (stringToInt(removeLoadedFilePrefixStr, removeLoadedFilePrefix)) {
            if(removeLoadedFilePrefix == 1)
                prefixToRemove = m_mpv->getProperty("filename").toString().toStdString();
        }
        int charsPerItem = 0;
        if (stringToInt(charsPerItemStr, charsPerItem))
            return m_mpv->audioTracksModel()->getListAsFormattedString(prefixToRemove, charsPerItem);
        else
            return m_mpv->audioTracksModel()->getListAsFormattedString(prefixToRemove);
    }
    else
        return "";
}

const std::string HttpServerThread::getPlayListItems(std::string charsPerItemStr)
{
    if (m_mpv) {
        int charsPerItem = 0;
        if(stringToInt(charsPerItemStr, charsPerItem))
            return m_mpv->getPlayListModel()->getListAsFormattedString(charsPerItem);
        else
            return m_mpv->getPlayListModel()->getListAsFormattedString();
    }
    else
        return "";
}

const std::string HttpServerThread::getSectionsItems(std::string charsPerItemStr)
{
    if (m_mpv) {
        int charsPerItem = 0;
        if (stringToInt(charsPerItemStr, charsPerItem))
            return m_mpv->getPlaySectionsModel()->getSectionsAsFormattedString(charsPerItem);
        else
            return m_mpv->getPlaySectionsModel()->getSectionsAsFormattedString();
    }
    else
        return "";
}

const std::string HttpServerThread::getPlaylingItemIndexFromAudioTracks()
{
    if (m_mpv) {
        return std::to_string(m_mpv->audioId()-1);
    }
    return "-1";
}


const std::string HttpServerThread::getPlaylingItemIndexFromPlaylist()
{
    if (m_mpv) {
        return std::to_string(m_mpv->getPlayListModel()->getPlayingVideo());
    }
    return "-1";
}

const std::string HttpServerThread::getPlaylingItemIndexFromSections()
{
    if (m_mpv) {
        return std::to_string(m_mpv->getPlaySectionsModel()->getPlayingSection());
    }
    return "-1";
}

const std::string HttpServerThread::LoadIndexFromAudioTracks(std::string indexStr)
{
    int index = -1;
    if (stringToInt(indexStr, index)) {
        if (m_mpv) {
            if (index >= 0 && index < m_mpv->audioTracksModel()->countTracks()) {
                Q_EMIT loadFromAudioTracks(index);
                return "Loading audio track with index: " + indexStr;
            }
            else {
                return "Index was out of bounds of audio tracks";
            }
        }
        else {
            return "Could not find reference to MPV";
        }
    }
    else {
        return "Could not interpret index parameter";
    }
}

const std::string HttpServerThread::LoadIndexFromPlaylist(std::string indexStr) 
{
    int index = -1;
    if (stringToInt(indexStr, index)) {
        if (m_mpv) {
            if (index >= 0 && index < m_mpv->getPlayListModel()->getPlayListSize()) {
                Q_EMIT loadFromPlaylist(index);
                return "Loading media with index: " + indexStr;
            }
            else {
                return "Index was out of bounds of playlist";
            }
        }
        else{
            return "Could not find reference to MPV";
        }
    }
    else {
        return "Could not interpret index parameter";
    }
}

const std::string HttpServerThread::LoadIndexFromSections(std::string indexStr)
{
    int index = -1;
    if (stringToInt(indexStr, index)) {
        if (m_mpv) {
            if (index >= 0 && index < m_mpv->getPlaySectionsModel()->getNumberOfSections()) {
                Q_EMIT loadFromSections(index);
                return "Loading section with index: " + indexStr;
            }
            else {
                return "Index was out of bounds of sections list";
            }
        }
        else {
            return "Could not find reference to MPV";
        }
    }
    else {
        return "Could not interpret index parameter";
    }
}

void HttpServerThread::setMpv(MpvObject* mpv)
{
    if (m_mpv == mpv) {
        return;
    }
    m_mpv = mpv;
}
