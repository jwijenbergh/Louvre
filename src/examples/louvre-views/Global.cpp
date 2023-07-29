#include <Output.h>
#include <LLog.h>
#include <Global.h>
#include <Compositor.h>
#include <string.h>
#include <LXCursor.h>
#include <LOpenGL.h>

static G::DockTextures _dockTextures;
static G::ToplevelTextures _toplevelTextures;
static G::Cursors xCursors;

Compositor *G::compositor()
{
    return (Compositor*)LCompositor::compositor();
}

LScene *G::scene()
{
    return compositor()->scene;
}

std::list<Output *> &G::outputs()
{
    return (std::list<Output*>&)compositor()->outputs();
}

void G::loadDockTextures()
{
    _dockTextures.left = LOpenGL::loadTexture("/usr/etc/Louvre/assets/dock_side.png");

    if (!_dockTextures.left)
    {
        LLog::fatal("[louvre-views] Failed to load dock_side.png texture.");
        exit(1);
    }

    _dockTextures.center = LOpenGL::loadTexture("/usr/etc/Louvre/assets/dock_clamp.png");

    if (!_dockTextures.center)
    {
        LLog::fatal("[louvre-views] Failed to load dock_center.png texture.");
        exit(1);
    }

    _dockTextures.right = _dockTextures.left->copyB(_dockTextures.left->sizeB(),
                                                    LRect(0,
                                                          0,
                                                          - _dockTextures.left->sizeB().w(),
                                                          _dockTextures.left->sizeB().h()));
}

G::DockTextures &G::dockTextures()
{
    return _dockTextures;
}

void G::loadCursors()
{
    xCursors.hand2 = LXCursor::loadXCursorB("hand2");
}

G::Cursors &G::cursors()
{
    return xCursors;
}

void G::loadToplevelTextures()
{
    _toplevelTextures.activeTL = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_active_upper_corner.png");
    _toplevelTextures.activeT = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_active_topbar_clamp.png");
    _toplevelTextures.activeTR = _toplevelTextures.activeTL->copyB(_toplevelTextures.activeTL->sizeB(),
                                                                  LRect(0,
                                                                        0,
                                                                        -_toplevelTextures.activeTL->sizeB().w(),
                                                                        _toplevelTextures.activeTL->sizeB().h()));
    _toplevelTextures.activeL = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_active_side_clamp.png");
    _toplevelTextures.activeR = _toplevelTextures.activeL->copyB(_toplevelTextures.activeL->sizeB(),
                                                                  LRect(0,
                                                                        0,
                                                                        -_toplevelTextures.activeL->sizeB().w(),
                                                                        _toplevelTextures.activeL->sizeB().h()));
    _toplevelTextures.activeBL = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_active_lower_corner.png");
    _toplevelTextures.activeB = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_active_lower_clamp.png");
    _toplevelTextures.activeBR = _toplevelTextures.activeBL->copyB(_toplevelTextures.activeBL->sizeB(),
                                                                  LRect(0,
                                                                        0,
                                                                        -_toplevelTextures.activeBL->sizeB().w(),
                                                                        _toplevelTextures.activeBL->sizeB().h()));

    _toplevelTextures.inactiveTL = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_inactive_upper_corner.png");
    _toplevelTextures.inactiveT = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_inactive_topbar_clamp.png");
    _toplevelTextures.inactiveTR = _toplevelTextures.inactiveTL->copyB(_toplevelTextures.inactiveTL->sizeB(),
                                                                  LRect(0,
                                                                        0,
                                                                        -_toplevelTextures.inactiveTL->sizeB().w(),
                                                                        _toplevelTextures.inactiveTL->sizeB().h()));
    _toplevelTextures.inactiveL = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_inactive_side_clamp.png");
    _toplevelTextures.inactiveR = _toplevelTextures.inactiveL->copyB(_toplevelTextures.inactiveL->sizeB(),
                                                                  LRect(0,
                                                                        0,
                                                                        -_toplevelTextures.inactiveL->sizeB().w(),
                                                                        _toplevelTextures.inactiveL->sizeB().h()));
    _toplevelTextures.inactiveBL = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_inactive_lower_corner.png");
    _toplevelTextures.inactiveB = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_inactive_lower_clamp.png");
    _toplevelTextures.inactiveBR = _toplevelTextures.inactiveBL->copyB(_toplevelTextures.inactiveBL->sizeB(),
                                                                  LRect(0,
                                                                        0,
                                                                        -_toplevelTextures.inactiveBL->sizeB().w(),
                                                                        _toplevelTextures.inactiveBL->sizeB().h()));


    _toplevelTextures.maskBL = LOpenGL::loadTexture("/usr/etc/Louvre/assets/toplevel_border_radius_mask.png");
    _toplevelTextures.maskBR = _toplevelTextures.maskBL->copyB(_toplevelTextures.maskBL->sizeB(),
                                                                  LRect(0,
                                                                        0,
                                                                        -_toplevelTextures.maskBL->sizeB().w(),
                                                                        _toplevelTextures.maskBL->sizeB().h()));

}

G::ToplevelTextures &G::toplevelTextures()
{
    return _toplevelTextures;
}
