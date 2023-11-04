#include "httpserverthread.h"
#include "mpvobject.h"
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
            res.set_content("Play", "text/plain");
            Q_EMIT playMedia();
        });

        svr.Post("/pause", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_content("Pause", "text/plain");
            Q_EMIT pauseMedia();
         });

        svr.Post("/playlist", [this](const httplib::Request& req, httplib::Response& res) {
            std::string charsPerItem = "";
            if(req.has_param("charsPerItem")) {
                charsPerItem = req.get_param_value("charsPerItem");
            }
            res.set_content(getPlayListItems(charsPerItem), "text/plain");
        });

        svr.Post("/playing_in_list", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_content(getPlaylingItemIndexFromPlaylist(), "text/plain");
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

void HttpServerThread::setMpv(MpvObject* mpv)
{
    if (m_mpv == mpv) {
        return;
    }
    m_mpv = mpv;
}
