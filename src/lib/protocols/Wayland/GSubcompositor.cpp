#include <protocols/Wayland/private/GSubcompositorPrivate.h>
#include <private/LClientPrivate.h>

using namespace Louvre::Protocols::Wayland;

GSubcompositor::GSubcompositor
(
    wl_client *client,
    const wl_interface *interface,
    Int32 version,
    UInt32 id,
    const void *implementation,
    wl_resource_destroy_func_t destroy
)
    :LResource
    (
        client,
        interface,
        version,
        id,
        implementation,
        destroy
    )
{
    m_imp = new GSubcompositorPrivate();

    this->client()->imp()->subcompositorGlobals.push_back(this);
    imp()->clientLink = std::prev(this->client()->imp()->subcompositorGlobals.end());
}

GSubcompositor::~GSubcompositor()
{
    client()->imp()->subcompositorGlobals.erase(imp()->clientLink);
    delete m_imp;
}
