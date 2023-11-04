#ifndef HTTPSERVERTHREAD_H
#define HTTPSERVERTHREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <cpp-httplib/httplib.h>

class MpvObject;

class HttpServerThread : public QThread
{
    Q_OBJECT

public:
    HttpServerThread(QObject *parent = nullptr);
    ~HttpServerThread();

    void setupHttpServer();
    void setMpv(MpvObject* mpv);

public slots:
    void terminate();

Q_SIGNALS:
    void pauseMedia();
    void playMedia();

protected:
    void run() override;

private:
    bool stringToInt(std::string str, int& parsedInt);

    const std::string getPlayListItems(std::string charsPerItemStr = "");
    const std::string getSectionsItems(std::string charsPerItemStr = "");

    MpvObject* m_mpv;
    httplib::Server svr;
    bool runServer;
    int portServer;

    QMutex mutex;
    QWaitCondition condition;
    bool abort = false;
};

#endif // HTTPSERVERTHREAD_H
