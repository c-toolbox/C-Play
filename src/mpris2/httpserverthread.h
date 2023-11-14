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
    void setPosition(double position);
    void setVolume(int level);
    void fadeVolumeDown();
    void fadeVolumeUp();
    void fadeImageDown();
    void fadeImageUp();
    void loadFromPlaylist(int idx);
    void loadFromSections(int idx);

protected:
    void run() override;

private:
    bool stringToInt(std::string str, int& parsedInt);
    bool stringToDouble(std::string str, double& parsedDouble);

    void setPositionFromStr(std::string positionTimeStr);
    void setVolumeFromStr(std::string volumeLevelStr);

    const std::string getPlayListItems(std::string charsPerItemStr = "");
    const std::string getSectionsItems(std::string charsPerItemStr = "");

    const std::string getPlaylingItemIndexFromPlaylist();
    const std::string getPlaylingItemIndexFromSections();

    const std::string LoadIndexFromPlaylist(std::string indexStr);
    const std::string LoadIndexFromSections(std::string indexStr);

    MpvObject* m_mpv;
    httplib::Server svr;
    bool runServer;
    int portServer;

    QMutex mutex;
    QWaitCondition condition;
    bool abort = false;
};

#endif // HTTPSERVERTHREAD_H
