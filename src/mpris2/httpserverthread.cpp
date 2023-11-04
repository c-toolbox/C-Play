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

const std::string HttpServerThread::getPlayListItems(std::string charsPerItemStr)
{
    int charsPerItem = 0;
    if(!charsPerItemStr.empty()) {
        try
        {
            charsPerItem = std::stoi(charsPerItemStr);
        }
        catch (std::invalid_argument const& ex)
        {
            charsPerItem = 0;
        }
        catch (std::out_of_range const& ex)
        {
            charsPerItem = 0;
        }
    }

    if (m_mpv) {
        if(charsPerItem > 0)
            return m_mpv->getPlayListModel()->getListAsFormattedString(charsPerItem);
        else
            return m_mpv->getPlayListModel()->getListAsFormattedString();
    }
    else
        return "";
}

void HttpServerThread::setMpv(MpvObject* mpv)
{
    if (m_mpv == mpv) {
        return;
    }
    m_mpv = mpv;
}
