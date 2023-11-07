#include <private/LCompositorPrivate.h>
#include <private/LViewPrivate.h>
#include <private/LScenePrivate.h>
#include <private/LSceneViewPrivate.h>
#include <LOutput.h>
#include <LLog.h>
#include <string.h>

using namespace Louvre;

LView::LView(UInt32 type, LView *parent)
{
    m_imp = new LViewPrivate();
    imp()->type = type;
    compositor()->imp()->views.push_back(this);
    imp()->compositorLink = std::prev(compositor()->imp()->views.end());
    setParent(parent);
}

LView::~LView()
{
    setParent(nullptr);

    while (!children().empty())
        children().front()->setParent(nullptr);

    compositor()->imp()->views.erase(imp()->compositorLink);
    delete m_imp;
}

LScene *LView::scene() const
{
    // Only the LScene mainView has this variable assigned
    if (imp()->scene)
        return imp()->scene;

    if (parent())
        return parent()->scene();

    return nullptr;
}

LSceneView *LView::parentSceneView() const
{
    if (parent())
    {
        if (parent()->type() == Scene)
            return (LSceneView*)parent();

        return parent()->parentSceneView();
    }
    return nullptr;
}

UInt32 LView::type() const
{
    return imp()->type;
}

void LView::repaint()
{
    if (imp()->repaintCalled)
        return;

    for (std::list<LOutput*>::const_iterator it = outputs().cbegin(); it != outputs().cend(); it++)
        (*it)->repaint();

    imp()->repaintCalled = true;
}

LView *LView::parent() const
{
    return imp()->parent;
}

void LView::setParent(LView *view)
{
    if (parent() == view || view == this)
        return;

    LScene *s = scene();

    if (s)
        s->imp()->listChanged = true;

    if (parent())
        parent()->imp()->children.erase(imp()->parentLink);

    if (view)
    {
        view->imp()->children.push_back(this);
        imp()->parentLink = std::prev(view->imp()->children.end());
    }
    else
    {
        imp()->damageScene(parentSceneView());
    }

    imp()->markAsChangedOrder();
    imp()->parent = view;
}

void LView::insertAfter(LView *prev, bool switchParent)
{
    if (prev == this)
        return;

    // If prev == nullptr, insert to the front of current parent children list
    if (!prev)
    {
        // If no parent, is a no-op
        if (!parent())
            return;

        // Already in front
        if (parent()->children().front() == this)
            return;

        parent()->imp()->children.erase(imp()->parentLink);
        parent()->imp()->children.push_front(this);
        imp()->parentLink = parent()->imp()->children.begin();

        imp()->markAsChangedOrder();

        repaint();
    }
    else
    {
        if (switchParent)
        {
            setParent(prev->parent());
        }
        else
        {
            if (prev->parent() != parent())
                return;
        }

        imp()->markAsChangedOrder();

        repaint();

        if (!parent())
            return;

        if (prev == parent()->children().back())
        {
            parent()->imp()->children.erase(imp()->parentLink);
            parent()->imp()->children.push_back(this);
            imp()->parentLink = std::prev(parent()->imp()->children.end());
        }
        else
        {
            parent()->imp()->children.erase(imp()->parentLink);
            imp()->parentLink = parent()->imp()->children.insert(std::next(prev->imp()->parentLink), this);
        }
    }
}

std::list<Louvre::LView *> &LView::children() const
{
    return imp()->children;
}

bool LView::parentOffsetEnabled() const
{
    return imp()->parentOffsetEnabled;
}

void LView::enableParentOffset(bool enabled)
{
    if (mapped() && enabled != parentOffsetEnabled())
        repaint();

    imp()->parentOffsetEnabled = enabled;
}

const LPoint &LView::pos() const
{
    imp()->tmpPos = nativePos();

    if (parent())
    {
        if (parentScalingEnabled())
            imp()->tmpPos *= parent()->scalingVector(parent()->type() == Scene);

        if (parentOffsetEnabled())
            imp()->tmpPos += parent()->pos();
    }

    return imp()->tmpPos;
}

const LSize &LView::size() const
{
    imp()->tmpSize = nativeSize();

    if (scalingEnabled())
        imp()->tmpSize *= scalingVector(true);

    if (parent() && parentScalingEnabled())
            imp()->tmpSize *= parent()->scalingVector(parent()->type() == Scene);

    return imp()->tmpSize;
}

bool LView::clippingEnabled() const
{
    return imp()->clippingEnabled;
}

void LView::enableClipping(bool enabled)
{
    if (imp()->clippingEnabled != enabled)
    {
        imp()->clippingEnabled = enabled;
        repaint();
    }
}

const LRect &LView::clippingRect() const
{
    return imp()->clippingRect;
}

void LView::setClippingRect(const LRect &rect)
{
    if (rect != imp()->clippingRect)
    {
        imp()->clippingRect = rect;
        repaint();
    }
}

bool LView::parentClippingEnabled() const
{
    return imp()->parentClippingEnabled;
}

void LView::enableParentClipping(bool enabled)
{
    if (mapped() && enabled != imp()->parentClippingEnabled)
        repaint();

    imp()->parentClippingEnabled = enabled;
}

bool LView::inputEnabled() const
{
    return imp()->inputEnabled;
}

void LView::enableInput(bool enabled)
{
    imp()->inputEnabled = enabled;
}

bool LView::scalingEnabled() const
{
    return imp()->scalingEnabled;
}

void LView::enableScaling(bool enabled)
{
    if (mapped() && enabled != imp()->scalingEnabled)
        repaint();

    imp()->scalingEnabled = enabled;
}

bool LView::parentScalingEnabled() const
{
    return imp()->parentScalingEnabled;
}

void LView::enableParentScaling(bool enabled)
{
    if (mapped() && enabled != imp()->parentScalingEnabled)
        repaint();

    imp()->parentScalingEnabled = enabled;
}

const LSizeF &LView::scalingVector(bool forceIgnoreParent) const
{
    if (forceIgnoreParent)
        return imp()->scalingVector;

    imp()->tmpScalingVector = imp()->scalingVector;

    if (parent() && parentScalingEnabled())
        imp()->tmpScalingVector *= parent()->scalingVector(parent()->type() == Scene);

    return imp()->tmpScalingVector;
}

void LView::setScalingVector(const LSizeF &scalingVector)
{
    if (mapped() && scalingVector != imp()->scalingVector)
        repaint();

    imp()->scalingVector = scalingVector;
}

bool LView::visible() const
{
    return imp()->visible;
}

void LView::setVisible(bool visible)
{
    bool prev = mapped();
    imp()->visible = visible;

    if (prev != mapped())
        repaint();
}

bool LView::mapped() const
{
    if (type() == Scene && !parent())
        return visible();

    return visible() && nativeMapped() && parent() && parent()->mapped();
}

Float32 LView::opacity(bool forceIgnoreParent) const
{
    if (forceIgnoreParent)
        return imp()->opacity;

    if (parentOpacityEnabled() && parent())
        return imp()->opacity * parent()->opacity(parent()->type() == Scene);

    return imp()->opacity;
}

void LView::setOpacity(Float32 opacity)
{
    if (opacity < 0.f)
        opacity = 0.f;
    else if(opacity > 1.f)
        opacity = 1.f;

    if (mapped() && opacity != imp()->opacity)
        repaint();

    imp()->opacity = opacity;
}

bool LView::parentOpacityEnabled() const
{
    return imp()->parentOpacityEnabled;
}

void LView::enableParentOpacity(bool enabled)
{
    if (mapped() && imp()->parentOpacityEnabled != enabled)
        repaint();

    imp()->parentOpacityEnabled = enabled;
}

bool LView::forceRequestNextFrameEnabled() const
{
    return imp()->forceRequestNextFrameEnabled;
}

void LView::enableForceRequestNextFrame(bool enabled) const
{
    imp()->forceRequestNextFrameEnabled = enabled;
}

void LView::setBlendFunc(GLenum sFactor, GLenum dFactor)
{
    if (imp()->sFactor != sFactor || imp()->dFactor != dFactor)
    {
        imp()->sFactor = sFactor;
        imp()->dFactor = dFactor;
        repaint();
    }
}

void LView::setColorFactor(Float32 r, Float32 g, Float32 b, Float32 a)
{
    if (imp()->colorFactor.r != r ||
        imp()->colorFactor.g != g ||
        imp()->colorFactor.b != b ||
        imp()->colorFactor.a != a)
    {
        imp()->colorFactor = {r, g, b, a};
        repaint();
        imp()->colorFactorEnabled = r != 1.f || g != 1.f || b != 1.f || a != 1.f;
    }
}

const LRGBAF &LView::colorFactor()
{
    return imp()->colorFactor;
}

bool LView::pointerIsOver() const
{
    return imp()->pointerIsOver;
}

void LView::enableBlockPointer(bool enabled)
{
    imp()->blockPointerEnabled = enabled;
}

bool LView::blockPointerEnabled() const
{
    return imp()->blockPointerEnabled;
}

LBox LView::boundingBox() const
{
    LBox box =
    {
        pos().x(),
        pos().y(),
        pos().x() + size().w(),
        pos().y() + size().h(),
    };

    LBox childBox;

    for (LView *child : children())
    {
        if (!child->mapped())
            continue;

        childBox = child->boundingBox();

        if (childBox.x1 < box.x1)
            box.x1 = childBox.x1;

        if (childBox.y1 < box.y1)
            box.y1 = childBox.y1;

        if (childBox.x2 > box.x2)
            box.x2 = childBox.x2;

        if (childBox.y2 > box.y2)
            box.y2 = childBox.y2;
    }

    return box;
}

void LView::pointerEnterEvent(const LPoint &localPos)
{
    L_UNUSED(localPos);
}

void LView::pointerMoveEvent(const LPoint &localPos)
{
    L_UNUSED(localPos);
}

void LView::pointerLeaveEvent()
{
}

void LView::pointerButtonEvent(LPointer::Button button, LPointer::ButtonState state)
{
    L_UNUSED(button);
    L_UNUSED(state);
}

void LView::pointerAxisEvent(Float64 axisX, Float64 axisY, Int32 discreteX, Int32 discreteY, UInt32 source)
{
    L_UNUSED(axisX);
    L_UNUSED(axisY);
    L_UNUSED(discreteX);
    L_UNUSED(discreteY);
    L_UNUSED(source);
}

void LView::keyModifiersEvent(UInt32 depressed, UInt32 latched, UInt32 locked, UInt32 group)
{
    L_UNUSED(depressed);
    L_UNUSED(latched);
    L_UNUSED(locked);
    L_UNUSED(group);
}

void LView::keyEvent(UInt32 keyCode, UInt32 keyState)
{
    L_UNUSED(keyCode);
    L_UNUSED(keyState);
}