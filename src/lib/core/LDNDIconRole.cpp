#include <private/LDNDIconRolePrivate.h>
#include <private/LBaseSurfaceRolePrivate.h>
#include <private/LSurfacePrivate.h>

#include <LSurface.h>
#include <LCompositor.h>

using namespace Louvre;

LDNDIconRole::LDNDIconRole(Params *params) : LBaseSurfaceRole(params->surface->surfaceResource(), params->surface, LSurface::Role::DNDIcon)
{
    m_imp = new LDNDIconRolePrivate();

    surface()->imp()->receiveInput = false;
}

LDNDIconRole::~LDNDIconRole()
{
    if (surface())
        surface()->imp()->setMapped(false);

    delete m_imp;
}

const LPoint &LDNDIconRole::hotspot() const
{
    return imp()->currentHotspot;
}

const LPoint &LDNDIconRole::hotspotB() const
{
    return imp()->currentHotspotB;
}

void LDNDIconRole::handleSurfaceOffset(Int32 x, Int32 y)
{
    imp()->pendingHotspotOffset = LPoint(x,y);
}

void LDNDIconRole::handleSurfaceCommit(Protocols::Wayland::RSurface::CommitOrigin origin)
{
    L_UNUSED(origin);

    imp()->currentHotspot -= imp()->pendingHotspotOffset;
    imp()->pendingHotspotOffset = LPoint();
    imp()->currentHotspotB = imp()->currentHotspot * surface()->bufferScale();
    hotspotChanged();

    surface()->imp()->setMapped(surface()->buffer() != nullptr);
}
