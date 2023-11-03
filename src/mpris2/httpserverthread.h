#ifndef HTTPSERVERTHREAD_H
#define HTTPSERVERTHREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <cpp-httplib/httplib.h>

class HttpServerThread : public QThread
{
    Q_OBJECT

public:
    HttpServerThread(QObject *parent = nullptr);
    ~HttpServerThread();

    void setupHttpServer();

public slots:
    void terminate();

Q_SIGNALS:
    void pauseMedia();
    void playMedia();

protected:
    void run() override;

private:
    httplib::Server svr;
    bool runServer;
    int portServer;

    QMutex mutex;
    QWaitCondition condition;
    bool abort = false;
};

#endif // HTTPSERVERTHREAD_H
