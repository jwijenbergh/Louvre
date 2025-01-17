#ifndef CLIENT_H
#define CLIENT_H

#include <LClient.h>
#include <LTimer.h>
#include <LAnimation.h>

class App;

using namespace Louvre;

class Client : public LClient
{
public:
    Client(LClient::Params *params);
    ~Client();

    void pong(UInt32 serial) override;

    void createNonPinnedApp();

    App *app = nullptr;
    Int32 pid = -1;
    bool destroyed = false;

    LTimer *pingTimer = nullptr;
    UInt32 lastPing = 0;
    UInt32 lastPong = 0;

    UInt32 unresponsiveCount = 0;
    LAnimation *unresponsiveAnim = nullptr;
};

#endif // CLIENT_H
