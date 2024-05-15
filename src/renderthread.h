#ifndef RENDERTHREAD_H
#define RENDERTHREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>

class RenderThread : public QThread
{
    Q_OBJECT

public:
    RenderThread(QObject *parent = nullptr);
    ~RenderThread();

    void render();

    Q_INVOKABLE void terminate();

protected:
    void run() override;

private:
    QMutex mutex;
    QWaitCondition condition;
    bool restart = false;
    bool abort = false;
};

#endif // RENDERTHREAD_H
