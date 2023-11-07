#ifndef SEAT_H
#define SEAT_H

#include <LSeat.h>

using namespace Louvre;

class Seat : public LSeat
{
public:
    Seat(Params *params);
    void enabledChanged() override;
};

#endif // SEAT_H