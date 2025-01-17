#include <LCursor.h>
#include <LAnimation.h>
#include <LLayerView.h>
#include <LSurfaceView.h>
#include <LPainter.h>
#include <LLog.h>
#include <LOpenGL.h>
#include <LTextureView.h>

#include "Global.h"
#include "Output.h"
#include "Compositor.h"
#include "Dock.h"
#include "Topbar.h"
#include "Workspace.h"
#include "Toplevel.h"
#include "Surface.h"
#include "ToplevelView.h"

Output::Output() : LOutput() {}

void Output::loadWallpaper()
{
    if (wallpaperView)
    {
        if (wallpaperView->texture())
        {
            if (sizeB() == wallpaperView->texture()->sizeB())
            {
                wallpaperView->setBufferScale(scale());
                return;
            }

            delete wallpaperView->texture();
        }
    }
    else
    {
        wallpaperView = new LTextureView(nullptr, &G::compositor()->backgroundLayer);
        wallpaperView->enableParentOffset(false);
    }

    char wallpaperPath[256];
    sprintf(wallpaperPath, "%s/.config/Louvre/wallpaper.jpg", getenv("HOME"));
    LTexture *tmpWallpaper = LOpenGL::loadTexture(wallpaperPath);

    if (!tmpWallpaper)
        tmpWallpaper = LOpenGL::loadTexture("/usr/etc/Louvre/assets/wallpaper.png");

    if (tmpWallpaper)
    {
        // Clip and scale wallpaper so that it covers the entire screen

        LRect srcB;
        float w = float(size().w() * tmpWallpaper->sizeB().h()) / float(size().h());

        if (w >= tmpWallpaper->sizeB().w())
        {
            srcB.setX(0);
            srcB.setW(tmpWallpaper->sizeB().w());
            srcB.setH((tmpWallpaper->sizeB().w() * size().h()) / size().w());
            srcB.setY((tmpWallpaper->sizeB().h() - srcB.h()) / 2);
        }
        else
        {
            srcB.setY(0);
            srcB.setH(tmpWallpaper->sizeB().h());
            srcB.setW((tmpWallpaper->sizeB().h() * size().w()) / size().h());
            srcB.setX((tmpWallpaper->sizeB().w() - srcB.w()) / 2);
        }
        wallpaperView->setTexture(tmpWallpaper->copyB(sizeB(), srcB));
        wallpaperView->setBufferScale(scale());
        delete tmpWallpaper;
    }
    else
    {
        wallpaperView->setVisible(false);
    }

    LRegion trans;
    wallpaperView->setTranslucentRegion(&trans);
    wallpaperView->setPos(pos());
}

void Output::setWorkspace(Workspace *ws, UInt32 animMs, Float32 curve, Float32 start)
{
    animStart = start;
    easingCurve = curve;
    workspaceAnim->stop();
    workspaceAnim->setDuration(animMs);
    currentWorkspace = ws;

    for (Output *o : G::outputs())
        o->workspaces.front()->stealChildren();

    workspaceAnim->start(false);
}

void Output::updateWorkspacesPos()
{
    Int32 offset = 0;
    Int32 spacing = 128;

    for (Workspace *ws : workspaces)
    {
        if (ws->nativePos().x() != offset)
        {
            ws->setPos(offset, 0);

            if (ws->toplevel)
                ws->toplevel->configure(size(), ws->toplevel->states());
        }

        offset += size().w() + spacing;
    }
}

void Output::initializeGL()
{
    workspaceAnim = LAnimation::create(400,
        [this](LAnimation *anim)
        {
            repaint();

            if (swippingWorkspace)
            {
                anim->stop();
                return;
            }

            // Hide non visible workspaces
            for (Workspace *ws : workspaces)
                ws->setVisible(LRect(ws->pos() + pos(), size()).intersects(rect()));

            Float32 ease = 1.f - powf(animStart + (1.f - animStart) * anim->value(), easingCurve);

            workspaceOffset = workspaceOffset * ease + Float32( - currentWorkspace->nativePos().x()) * (1.f - ease);
            workspacesContainer->setPos(workspaceOffset, 0);

            for (Output *o : G::outputs())
                for (Workspace *workspace : o->workspaces)
                    workspace->clipChildren();

            if (animatedFullscreenToplevel)
            {
                Toplevel *tl = animatedFullscreenToplevel;

                if (tl->destructorCalled || tl->quickUnfullscreen)
                {
                    anim->stop();
                    return;
                }

                tl->surface()->requestNextFrame(false);

                // Current fullscreen size
                LSize cSize;

                // Scaling vector for the black toplevel background container so it matches cSize
                LSizeF sVector;

                Float32 val = powf(anim->value(), 6.f);
                Float32 inv = 1.f - val;

                if (tl->fullscreen())
                {       
                    tl->animView.setVisible(true);
                    tl->animScene->render();
                    tl->animView.setTexture(tl->animScene->texture());
                    tl->animView.setPos((pos() * val) + (tl->prevBoundingRect.pos() * (inv)));
                    cSize = (tl->fullscreenOutput->size() * val) + (tl->prevBoundingRect.size() * (inv));
                    tl->animView.setDstSize(cSize);
                    tl->animView.setOpacity(val);

                    tl->capture.setPos(tl->animView.pos());
                    tl->capture.setDstSize(cSize);
                    LRegion transRegion = tl->captureTransRegion;
                    LSizeF transRegionScale = LSizeF(cSize) / LSizeF(tl->prevBoundingRect.size());
                    transRegion.multiply(transRegionScale.x(), transRegionScale.y());
                    tl->capture.setTranslucentRegion(&transRegion);
                }
                else
                {
                    tl->animScene->setPos(pos());
                    LPoint animPos = (pos() * inv) + (tl->prevBoundingRect.pos() * val);
                    tl->surf()->setPos(0);
                    LBox box = tl->surf()->getView()->boundingBox();
                    LSize boxSize = LSize(box.x2 - box.x1, box.y2 - box.y1);
                    tl->animScene->setSizeB(boxSize * 2);
                    cSize = (size() * inv) + (boxSize * val);

                    tl->capture.setOpacity(inv);
                    tl->capture.setPos(animPos);
                    tl->capture.setDstSize(cSize);

                    if (tl->decoratedView)
                        tl->surf()->setPos(LPoint() - (LPoint(box.x1, box.y1) - tl->animScene->nativePos()));
                    else
                        tl->surf()->setPos(tl->windowGeometry().pos());

                    tl->animScene->render();
                    LRegion transReg;
                    transReg = *tl->animScene->translucentRegion();
                    transReg.offset(LPoint() - tl->animScene->pos());
                    tl->animView.setTexture(tl->animScene->texture());
                    tl->animView.enableDstSize(true);
                    tl->animView.enableParentOffset(false);
                    tl->animView.setPos(animPos);
                    tl->animView.setDstSize(cSize);

                    LSizeF regScale = LSizeF(cSize) / LSizeF(boxSize);
                    transReg.multiply(regScale.x(), regScale.y());
                    tl->animView.setTranslucentRegion(&transReg);

                    tl->configure(tl->prevRect.size(), LToplevelRole::Activated);
                }

                if (tl->decoratedView)
                    tl->decoratedView->updateGeometry();
            }
        },
        [this](LAnimation *)
        {
            if (currentWorkspace->toplevel)
            {
                Toplevel *tl = currentWorkspace->toplevel;

                tl->blackFullscreenBackground.setVisible(false);

                if (tl->capture.texture())
                    delete tl->capture.texture();

                tl->animView.setTexture(nullptr);

                if (tl->animScene)
                {
                    delete tl->animScene;
                    tl->animScene = nullptr;
                }

                if (tl->destructorCalled || tl->quickUnfullscreen)
                    goto returnChildren;

                seat()->pointer()->setFocus(tl->surface());
                seat()->keyboard()->setFocus(tl->surface());
                tl->configure(tl->states() | LToplevelRole::Activated);
            }

            if (animatedFullscreenToplevel)
            {
                Toplevel *tl = animatedFullscreenToplevel;

                tl->blackFullscreenBackground.setVisible(false);

                if (tl->capture.texture())
                    delete tl->capture.texture();

                tl->animView.setTexture(nullptr);

                if (tl->animScene)
                {
                    delete tl->animScene;
                    tl->animScene = nullptr;
                }

                if (tl->destructorCalled || tl->quickUnfullscreen)
                    goto returnChildren;

                if (tl->fullscreen())
                {
                    tl->surf()->setPos(pos().x(), 0);
                    G::reparentWithSubsurfaces(tl->surf(), &tl->fullscreenWorkspace->surfaces);
                    currentWorkspace->clipChildren();
                }
                else
                {
                    tl->surf()->setPos(tl->prevRect.pos());
                    G::reparentWithSubsurfaces(tl->surf(), &workspaces.front()->surfaces, false);
                    G::repositionNonVisibleToplevelChildren(this, tl->surf());
                    tl->surf()->getView()->setVisible(true);
                    tl->surf()->raise();
                    delete tl->fullscreenWorkspace;
                    tl->fullscreenWorkspace = nullptr;
                }
                animatedFullscreenToplevel = nullptr;

                if (tl->decoratedView)
                    tl->decoratedView->updateGeometry();
            }

            returnChildren:
            for (Output *o : G::outputs())
                if (!o->swippingWorkspace)
                    o->currentWorkspace->returnChildren();

            updateWorkspacesPos();
            G::scene()->mainView()->damageAll(this);
            repaint();
        });

    workspacesContainer = new LLayerView(&G::compositor()->workspacesLayer);
    workspacesContainer->enableParentOffset(false);
    workspacesContainer->setPos(0, 0);
    currentWorkspace = new Workspace(this);

    new Topbar(this);
    topbar->update();

    new Dock(this);
    loadWallpaper();
    G::compositor()->scene.handleInitializeGL(this);
}

void Output::resizeGL()
{
    G::arrangeOutputs();
    updateWorkspacesPos();
    setWorkspace(currentWorkspace, 1);
    topbar->update();
    dock->update();
    loadWallpaper();
    G::compositor()->scene.handleResizeGL(this);
}

void Output::moveGL()
{
    updateWorkspacesPos();
    topbar->update();
    dock->update();
    wallpaperView->setPos(pos());
    setWorkspace(currentWorkspace, 1);
    G::compositor()->scene.handleMoveGL(this);
}

void Output::paintGL()
{
    // Show black screen during output removal
    if (!G::compositor()->checkUpdateOutputUnplug())
    {
        painter()->clearScreen();
        repaint();
        return;
    }

    // Check pointer events before painting
    if (G::compositor()->updatePointerBeforePaint)
    {
        seat()->pointer()->pointerMoveEvent(0, 0, false);
        G::compositor()->updatePointerBeforePaint = false;
    }

    // Check if hw cursor is supported
    if (cursor()->hasHardwareSupport(this))
        G::compositor()->softwareCursor.setTexture(nullptr);
    else
    {
        G::compositor()->softwareCursor.setTexture(cursor()->texture());
        G::compositor()->softwareCursor.setPos(cursor()->rect().pos());
        G::compositor()->softwareCursor.setDstSize(cursor()->rect().size());
        G::compositor()->softwareCursor.setVisible(cursor()->visible());
    }

    G::compositor()->scene.handlePaintGL(this);
}

void Output::uninitializeGL()
{
    G::compositor()->outputUnplugHandled = false;

    // Find another output
    Output *aliveOutput = nullptr;

    for (Output *o : G::outputs())
    {
        if (o != this)
        {
            aliveOutput = o;
            break;
        }
    }

    // Unfullscreen toplevels
    while (workspaces.size() != 1)
    {
        Toplevel *tl = workspaces.back()->toplevel;
        tl->surf()->sendOutputEnterEvent(aliveOutput);
        tl->outputUnplugConfigureCount = 0;
        tl->prevStates = LToplevelRole::Activated;
        tl->prevRect.setPos(LPoint(0, TOPBAR_HEIGHT));
        tl->configure(tl->prevRect.size(), LToplevelRole::Activated);
        tl->quickUnfullscreen = true;
        tl->unsetFullscreen();
        tl->surf()->localOutputPos = tl->prevRect.pos() - pos();
        tl->surf()->localOutputSize = size();
        tl->surf()->outputUnplugHandled = false;
        workspaceAnim->stop();
    }

    workspacesContainer->setPos(0, 0);

    for (Surface *s : G::surfaces())
    {
        if (s->cursorRole() || (s->toplevel() && s->toplevel()->fullscreen()))
            continue;

        Output *intersectedOutput = G::mostIntersectedOuput(s->getView());

        if (intersectedOutput == this)
        {
            s->localOutputPos = s->pos() - pos();
            s->localOutputSize = size();
            s->outputUnplugHandled = false;
        }
        else if (!intersectedOutput)
        {
            s->localOutputPos = LPoint(200, 200);
            s->localOutputSize = size();
            s->outputUnplugHandled = false;
        }

        if (s->minimizedOutput == this)
        {
            if (s->minimizeAnim)
                s->minimizeAnim->stop();

            s->minimizedOutput = aliveOutput;
            s->minimizeStartRect.setPos(LPoint(rand() % 128, TOPBAR_HEIGHT + (rand() % 128)));
        }
    }

    delete dock;
    dock = nullptr;
    delete topbar;
    topbar = nullptr;

    if (wallpaperView->texture())
        delete wallpaperView->texture();

    delete wallpaperView;
    wallpaperView = nullptr;

    workspaceAnim->stop();
    workspaceAnim->destroy();
    workspaceAnim = nullptr;

    while (!workspaces.empty())
        delete workspaces.back();

    delete workspacesContainer;
    workspacesContainer = nullptr;

    currentWorkspace = nullptr;
    animatedFullscreenToplevel = nullptr;

    G::compositor()->scene.handleUninitializeGL(this);
}
