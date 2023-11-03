#include "httpserverthread.h"
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
        svr.Get("/play", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_content("Play", "text/plain");
            Q_EMIT playMedia();
        });

        svr.Get("/pause", [this](const httplib::Request& req, httplib::Response& res) {
            res.set_content("Pause", "text/plain");
            Q_EMIT pauseMedia();
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
