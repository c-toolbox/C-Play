#include "renderthread.h"
#include <sgct/sgct.h>

RenderThread::RenderThread(QObject *parent)
    : QThread(parent)
{

}

RenderThread::~RenderThread()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();
}

void RenderThread::render()
{
    QMutexLocker locker(&mutex);

    //variables
    if (!isRunning()) {
        start();
    } else {
        restart = true;
        condition.wakeOne();
    }
}

void RenderThread::run()
{
    mutex.lock();
    if(abort)
        return;
    sgct::Log::Info("Starting rendering");
    mutex.unlock();

    sgct::Engine::instance().render();

    mutex.lock();
    sgct::Log::Info("Finished rendering");
    sgct::Engine::destroy();
    mutex.unlock();
}
