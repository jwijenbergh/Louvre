#include <private/LSeatPrivate.h>
#include <private/LCompositorPrivate.h>
#include <private/LOutputPrivate.h>
#include <LLog.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

void LSeat::LSeatPrivate::seatEnabled(libseat *seat, void *data)
{
    LSeat *lseat = (LSeat*)data;

    lseat->imp()->enabled = true;

    if (compositor()->state() != LCompositor::Initialized)
        return;

    compositor()->imp()->unlock();

    if (compositor()->isGraphicBackendInitialized())
        compositor()->imp()->graphicBackend->resume();

    compositor()->imp()->lock();

    if (compositor()->isInputBackendInitialized())
        compositor()->imp()->inputBackend->resume();

    // Restore Wayland events
    epoll_ctl(compositor()->imp()->epollFd,
              EPOLL_CTL_ADD,
              compositor()->imp()->events[2].data.fd,
              &compositor()->imp()->events[2]);

    LLog::debug("[LSeatPrivate::seatEnabled] %s enabled.", libseat_seat_name(seat));

    lseat->enabledChanged();
}

void LSeat::LSeatPrivate::seatDisabled(libseat *seat, void *data)
{
    LSeat *lseat = (LSeat*)data;

    if (!lseat->imp()->enabled)
        return;

    lseat->imp()->enabled = false;

    if (compositor()->state() != LCompositor::Initialized)
        return;

    if (compositor()->isInputBackendInitialized())
        compositor()->imp()->inputBackend->suspend();

    compositor()->imp()->unlock();

    if (compositor()->isGraphicBackendInitialized())
        compositor()->imp()->graphicBackend->pause();

    compositor()->imp()->lock();

    libseat_disable_seat(seat);

    // Disable Wayland events
    epoll_ctl(compositor()->imp()->epollFd,
              EPOLL_CTL_DEL,
              compositor()->imp()->events[2].data.fd,
              NULL);

    LLog::debug("[LSeatPrivate::seatDisabled] %s disabled.", libseat_seat_name(seat));

    lseat->enabledChanged();
}

void LSeat::LSeatPrivate::dispatchSeat()
{
    if (libseatHandle)
        while (libseat_dispatch(libseatHandle, 0) > 0);
}

bool LSeat::LSeatPrivate::initLibseat()
{
    if (libseatHandle)
        return true;

    char *env = getenv("LOUVRE_ENABLE_LIBSEAT");

    if (env && atoi(env) == 0)
        return false;

    listener.enable_seat = &LSeat::LSeatPrivate::seatEnabled;
    listener.disable_seat = &LSeat::LSeatPrivate::seatDisabled;
    libseatHandle = libseat_open_seat(&listener, LCompositor::compositor()->seat());

    if (!libseatHandle)
        return false;

    int fd = libseat_get_fd(libseatHandle);

    if (fd == -1)
    {
        libseat_close_seat(libseatHandle);
        libseatHandle = nullptr;
        return false;
    }

    fcntl(fd, F_SETFD, FD_CLOEXEC);
    compositor()->imp()->events[1].events = EPOLLIN;
    compositor()->imp()->events[1].data.fd = fd;

    epoll_ctl(compositor()->imp()->epollFd,
              EPOLL_CTL_ADD,
              compositor()->imp()->events[1].data.fd,
              &compositor()->imp()->events[1]);

    LCompositor::compositor()->imp()->lock();
    dispatchSeat();
    LCompositor::compositor()->imp()->unlock();

    LLog::debug("[LSeatPrivate::initLibseat] Using libseat.");
    return true;
}

void LSeat::LSeatPrivate::backendOutputPlugged(LOutput *output)
{
    if (enabled)
        seat()->outputPlugged(output);
}

void LSeat::LSeatPrivate::backendOutputUnplugged(LOutput *output)
{
    if (enabled)
        seat()->outputUnplugged(output);
}
