#include <LOutput.h>
#include <LPainter.h>
#include <LCompositor.h>
#include <LSurface.h>
#include <LSeat.h>
#include <LDNDManager.h>
#include <LDNDIconRole.h>
#include <LCursor.h>

using namespace Louvre;

//! [initializeGL]
void LOutput::initializeGL()
{
    painter()->setClearColor(1.f, 1.f, 1.f, 1.f);
}
//! [initializeGL]

//! [paintGL]
void LOutput::paintGL()
{
    LPainter *p = painter();

    p->clearScreen();

    // Check if a surface moved under cursor (simulating a pointer move event)
    if (seat()->pointer()->surfaceAt(cursor()->pos()) != seat()->pointer()->focus())
        seat()->pointer()->pointerMoveEvent(
            cursor()->pos().x(),
            cursor()->pos().y(),
            true);

    if (seat()->dndManager()->icon())
        seat()->dndManager()->icon()->surface()->raise();

    // Draw every surface
    for (LSurface *s : compositor()->surfaces())
    {
        // Skip some surfaces
        if (!s->mapped() || s->minimized() || s->cursorRole())
        {
            s->requestNextFrame();
            continue;
        }

        // Current surface rect
        LRect currentRect = LRect(
            s->rolePos(),  // Role pos in surface coords
            s->size());    // Surface size in surface coords

        // Calc which outputs intersect the surface
        for (LOutput *o : compositor()->outputs())
        {
            if (o->rect().intersects(currentRect))
                s->sendOutputEnterEvent(o);
            else
                s->sendOutputLeaveEvent(o);
        }

        // Draw the surface
        p->drawTexture(
            s->texture(),        // Surface texture
            LRect(0, s->sizeB()),// The entire texture size
            currentRect);        // The destination rect on screen

        // Notify the client it can now render a new surface frame
        s->requestNextFrame();
    }

    // Manualy draw the cursor if hardware composition is not supported
    if (!cursor()->hasHardwareSupport(this))
    {
        p->drawTexture(
            cursor()->texture(),                   
            LRect(0, cursor()->texture()->sizeB()),
            cursor()->rect());
    }
}
//! [paintGL]

//! [resizeGL]
void LOutput::resizeGL()
{
    repaint();
}
//! [resizeGL]

//! [moveGL]
void LOutput::moveGL()
{
    repaint();
}
//! [moveGL]

//! [uninitializeGL]
void LOutput::uninitializeGL()
{
    /* No default implementation */
}
//! [uninitializeGL]
