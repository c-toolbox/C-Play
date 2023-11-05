#include "httpserverthread.h"
#include "mpvobject.h"
#include "playbacksettings.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

HttpServerThread::HttpServerThread(QObject *parent)
    : QThread(parent), runServer(false), portServer(7007)
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
        svr.Post("/play", [this](const httplib::Request& req, httplib::Response& res) {
            Q_EMIT playMedia();
            res.set_content("Play", "text/plain");
        });

        svr.Post("/pause", [this](const httplib::Request& req, httplib::Response& res) {
            Q_EMIT pauseMedia();
            res.set_content("Pause", "text/plain");
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

        svr.Post("/fade_duration", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_content(std::to_string(double(PlaybackSettings::fadeDuration())/1000.0), "text/plain");
        });

        svr.Post("/fade_volume_down", [this](const httplib::Request& req, httplib::Response& res) {
            Q_EMIT fadeVolumeDown();
            res.set_content("Fading volume down", "text/plain");
        });

        svr.Post("/fade_volume_up", [this](const httplib::Request& req, httplib::Response& res) {
            Q_EMIT fadeVolumeUp();
            res.set_content("Fading volume up", "text/plain");
        });

        svr.Post("/visibility", [this](const httplib::Request& req, httplib::Response& res) {
            if (m_mpv) {
                res.set_content(std::to_string(m_mpv->visibility()), "text/plain");
            }
            else {
                res.set_content("0", "text/plain");
            }
        });

        svr.Post("/fade_image_down", [this](const httplib::Request& req, httplib::Response& res) {
            Q_EMIT fadeImageDown();
            res.set_content("Fading image down", "text/plain");
        });

        svr.Post("/fade_image_up", [this](const httplib::Request& req, httplib::Response& res) {
            Q_EMIT fadeImageUp();
            res.set_content("Fading image up", "text/plain");
        });

        svr.Post("/playlist", [this](const httplib::Request& req, httplib::Response& res) {
            std::string charsPerItem = "";
            if(req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            res.set_content(getPlayListItems(charsPerItem), "text/plain");
        });

        svr.Post("/playing_in_playlist", [this](const httplib::Request& req, httplib::Response& res) {
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

        svr.Post("/playing_in_sections", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_content(getPlaylingItemIndexFromSections(), "text/plain");
        });

        svr.Post("/load_from_sections", [this](const httplib::Request& req, httplib::Response& res) {
            if (req.has_param("index")) {
                std::string indexStr = req.get_param_value("index");
                res.set_content(LoadIndexFromSections(indexStr), "text/plain");
            }
            res.set_content("Missing index parameter", "text/plain");
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
        catch (std::invalid_argument const& ex)
        {
            return false;
        }
        catch (std::out_of_range const& ex)
        {
            return true;
        }
    }
    return false;
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
