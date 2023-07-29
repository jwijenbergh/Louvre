#ifndef LLAYERVIEW_H
#define LLAYERVIEW_H

#include <LView.h>

class Louvre::LLayerView : public LView
{
public:
    LLayerView(LView *parent = nullptr);
    ~LLayerView();

    virtual void setPos(Int32 x, Int32 y);
    virtual void setSize(Int32 w, Int32 h);
    virtual void setInputRegion(const LRegion *region) const;

    void setPos(const LPoint &pos);
    void setSize(const LSize &size);

    virtual bool nativeMapped() const override;
    virtual const LPoint &nativePos() const override;
    virtual const LSize &nativeSize() const override;
    virtual Int32 bufferScale() const override;
    virtual void enteredOutput(LOutput *output) override;
    virtual void leftOutput(LOutput *output) override;
    virtual const std::list<LOutput*> &outputs() const override;
    virtual bool isRenderable() const override;
    virtual void requestNextFrame(LOutput *output) override;
    virtual const LRegion *damage() const override;
    virtual const LRegion *translucentRegion() const override;
    virtual const LRegion *opaqueRegion() const override;
    virtual const LRegion *inputRegion() const override;
    virtual void paintRect(LPainter *p,
                           Int32 srcX, Int32 srcY,
                           Int32 srcW, Int32 srcH,
                           Int32 dstX, Int32 dstY,
                           Int32 dstW, Int32 dstH,
                           Float32 scale,
                           Float32 alpha) override;

LPRIVATE_IMP(LLayerView)
};

#endif // LLAYERVIEW_H
