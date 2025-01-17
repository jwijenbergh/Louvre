#include <LLog.h>
#include <sys/epoll.h>

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <fcntl.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <drm.h>
#include <drm_fourcc.h>
#include <unordered_map>

#include <LGraphicBackend.h>
#include <private/LCompositorPrivate.h>
#include <private/LSeatPrivate.h>
#include <private/LOutputPrivate.h>
#include <private/LOutputModePrivate.h>
#include <private/LPainterPrivate.h>
#include <private/LTexturePrivate.h>
#include <private/LCursorPrivate.h>
#include <private/LSeatPrivate.h>

#include <LTime.h>

#include <SRM/SRMCore.h>
#include <SRM/SRMDevice.h>
#include <SRM/SRMConnector.h>
#include <SRM/SRMConnectorMode.h>
#include <SRM/SRMBuffer.h>
#include <SRM/SRMListener.h>
#include <SRM/SRMList.h>
#include <SRM/SRMFormat.h>

using namespace Louvre;
using namespace std;

#define BKND_NAME "DRM BACKEND"

static bool libseatEnabled = false;

struct DEVICE_FD_ID
{
    int fd;
    int id;
};

struct Backend
{
    SRMCore *core;
    list<LOutput*>connectedOutputs;
    wl_event_source *monitor;
    list<LDMAFormat*>dmaFormats;
    std::list<DEVICE_FD_ID> devices;
    UInt32 rendererGPUs = 0;
};

struct Output
{
    SRMConnector *conn;
    LSize physicalSize;
    list<LOutputMode*>modes;
    LTexture **textures = nullptr;
};

struct OutputMode
{
    SRMConnectorMode *mode;
    LSize size;
};

static int openRestricted(const char *path, int flags, void *userData)
{
    LCompositor *compositor = (LCompositor*)userData;
    Backend *bknd = (Backend*)compositor->imp()->graphicBackendData;

    if (libseatEnabled)
    {
        DEVICE_FD_ID dev;

        dev.id = compositor->seat()->openDevice(path, &dev.fd);

        if (dev.id == -1)
            return -1;
        else
        {
            bknd->devices.push_back(dev);
            return dev.fd;
        }
    }
    else
        return open(path, flags);
}

static void closeRestricted(int fd, void *userData)
{
    LCompositor *compositor = (LCompositor*)userData;
    Backend *bknd = (Backend*)compositor->imp()->graphicBackendData;

    if (libseatEnabled)
    {
        DEVICE_FD_ID dev = {-1, -1};

        for (std::list<DEVICE_FD_ID>::iterator it = bknd->devices.begin(); it != bknd->devices.end(); it++)
        {
            if ((*it).fd == fd)
            {
                dev = (*it);
                bknd->devices.erase(it);
                break;
            }
        }

        if (dev.fd == -1)
            return;

        compositor->seat()->closeDevice(dev.id);
    }

    close(fd);
}

static SRMInterface srmInterface =
{
    .openRestricted = &openRestricted,
    .closeRestricted = &closeRestricted
};

static void initConnector(Backend *bknd, SRMConnector *conn)
{
    if (srmConnectorGetUserData(conn))
       return;

    LCompositor *compositor = (LCompositor*)srmCoreGetUserData(bknd->core);
    LOutput *output = compositor->createOutputRequest();
    srmConnectorSetUserData(conn, output);

    Output *bkndOutput = new Output();
    output->imp()->graphicBackendData = bkndOutput;
    bkndOutput->textures = nullptr;
    bkndOutput->conn = conn;
    bkndOutput->physicalSize.setW(srmConnectorGetmmWidth(conn));
    bkndOutput->physicalSize.setH(srmConnectorGetmmHeight(conn));

    SRMListForeach (modeIt, srmConnectorGetModes(conn))
    {
        SRMConnectorMode *mode = (SRMConnectorMode*)srmListItemGetData(modeIt);
        LOutputMode *outputMode = new LOutputMode(output);
        srmConnectorModeSetUserData(mode, outputMode);

        OutputMode *bkndOutputMode = new OutputMode();
        bkndOutputMode->mode = mode;
        bkndOutputMode->size.setW(srmConnectorModeGetWidth(mode));
        bkndOutputMode->size.setH(srmConnectorModeGetHeight(mode));

        outputMode->imp()->graphicBackendData = bkndOutputMode;
        bkndOutput->modes.push_back(outputMode);
    }

    output->imp()->updateRect();
    bknd->connectedOutputs.push_back(output);
}

static void uninitConnector(Backend *bknd, SRMConnector *conn)
{
    LOutput *output = (LOutput*)srmConnectorGetUserData(conn);

    if (!output)
        return;

    LCompositor *compositor = (LCompositor*)srmCoreGetUserData(bknd->core);

    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;

    while (!bkndOutput->modes.empty())
    {
        LOutputMode *mode = bkndOutput->modes.back();
        OutputMode *bkndMode = (OutputMode*)mode->imp()->graphicBackendData;
        srmConnectorModeSetUserData(bkndMode->mode, NULL);
        delete mode;
        delete bkndMode;
        bkndOutput->modes.pop_back();
    }

    compositor->destroyOutputRequest(output);
    bknd->connectedOutputs.remove(output);
    delete output;
    delete bkndOutput;
    srmConnectorSetUserData(conn, NULL);
}

static void connectorPluggedEventHandler(SRMListener *listener, SRMConnector *conn)
{
    Backend *bknd = (Backend*)srmListenerGetUserData(listener);
    LCompositor *compositor = (LCompositor*)srmCoreGetUserData(bknd->core);
    initConnector(bknd, conn);
    LOutput *output = (LOutput*)srmConnectorGetUserData(conn);
    compositor->seat()->imp()->backendOutputPlugged(output);
}

static void connectorUnpluggedEventHandler(SRMListener *listener, SRMConnector *conn)
{
    Backend *bknd = (Backend*)srmListenerGetUserData(listener);
    LCompositor *compositor = (LCompositor*)srmCoreGetUserData(bknd->core);

    LOutput *output = (LOutput*)srmConnectorGetUserData(conn);
    compositor->seat()->imp()->backendOutputUnplugged(output);
    compositor->removeOutput(output);
    uninitConnector(bknd, conn);
}

static int monitorEventHandler(Int32, UInt32, void *data)
{
    Backend *bknd = (Backend*)data;
    return srmCoreProcessMonitor(bknd->core, 0);
}

static void initializeGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);
    LOutput *output = (LOutput*)userData;
    output->imp()->backendInitializeGL();
}

static void paintGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);
    LOutput *output = (LOutput*)userData;
    output->imp()->backendPaintGL();
}

static void resizeGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);
    LOutput *output = (LOutput*)userData;
    output->imp()->backendResizeGL();
}

static void pageFlipped(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);
    LOutput *output = (LOutput*)userData;
    output->imp()->backendPageFlipped();
}

static void uninitializeGL(SRMConnector *connector, void *userData)
{
    SRM_UNUSED(connector);
    LOutput *output = (LOutput*)userData;
    output->imp()->backendUninitializeGL();
}

static SRMConnectorInterface connectorInterface =
{
    .initializeGL = &initializeGL,
    .paintGL = &paintGL,
    .pageFlipped = &pageFlipped,
    .resizeGL = &resizeGL,
    .uninitializeGL = &uninitializeGL
};

UInt32 LGraphicBackend::id()
{
    return LGraphicBackendDRM;
}

void *LGraphicBackend::getContextHandle()
{
    Backend *bknd = (Backend*)LCompositor::compositor()->imp()->graphicBackendData;
    return bknd->core;
}

bool LGraphicBackend::initialize()
{
    LCompositor *compositor = LCompositor::compositor();
    libseatEnabled = compositor->seat()->imp()->initLibseat();

    Backend *bknd = new Backend();
    compositor->imp()->graphicBackendData = bknd;
    bknd->core = srmCoreCreate(&srmInterface, compositor);

    if (!bknd->core)
    {
        LLog::fatal("[%] Failed to create SRM core.", BKND_NAME);
        goto fail;
    }

    // Fill DMA formats (LDMAFormat = SRMFormat)
    SRMListForeach (fmtIt, srmCoreGetSharedDMATextureFormats(bknd->core))
    {
        SRMFormat *fmt = (SRMFormat*)srmListItemGetData(fmtIt);
        bknd->dmaFormats.push_back((LDMAFormat*)fmt);
    }

    // Find connected outputs
    SRMListForeach (devIt, srmCoreGetDevices(bknd->core))
    {
        SRMDevice *dev = (SRMDevice*)srmListItemGetData(devIt);

        if (srmDeviceIsRenderer(dev))
            bknd->rendererGPUs++;

        SRMListForeach (connIt, srmDeviceGetConnectors(dev))
        {
            SRMConnector *conn = (SRMConnector*)srmListItemGetData(connIt);

            if (srmConnectorIsConnected(conn))
                initConnector(bknd, conn);
        }
    }

    // Listen to connector hotplug events
    srmCoreAddConnectorPluggedEventListener(bknd->core, &connectorPluggedEventHandler, bknd);
    srmCoreAddConnectorUnpluggedEventListener(bknd->core, &connectorUnpluggedEventHandler, bknd);

    bknd->monitor = LCompositor::addFdListener(srmCoreGetMonitorFD(bknd->core),
                                            bknd,
                                            &monitorEventHandler);

    compositor->imp()->graphicBackendData = bknd;
    return true;

    fail:
    delete bknd;
    return false;
}

void LGraphicBackend::uninitialize()
{
    LCompositor *compositor = LCompositor::compositor();
    Backend *bknd = (Backend*)compositor->imp()->graphicBackendData;
    LCompositor::removeFdListener(bknd->monitor);
    srmCoreDestroy(bknd->core);
    delete bknd;
}

void LGraphicBackend::pause()
{
    LCompositor *compositor = LCompositor::compositor();
    Backend *bknd = (Backend*)compositor->imp()->graphicBackendData;
    srmCoreSuspend(bknd->core);
}

void LGraphicBackend::resume()
{
    LCompositor *compositor = LCompositor::compositor();
    Backend *bknd = (Backend*)compositor->imp()->graphicBackendData;
    srmCoreResume(bknd->core);
}

const list<LOutput*> *LGraphicBackend::getConnectedOutputs()
{
    LCompositor *compositor = LCompositor::compositor();
    Backend *bknd = (Backend*)compositor->imp()->graphicBackendData;
    return &bknd->connectedOutputs;
}

UInt32 LGraphicBackend::rendererGPUs()
{
    LCompositor *compositor = LCompositor::compositor();
    Backend *bknd = (Backend*)compositor->imp()->graphicBackendData;
    return bknd->rendererGPUs;
}

bool LGraphicBackend::initializeOutput(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return srmConnectorInitialize(bkndOutput->conn, &connectorInterface, output);
}

bool LGraphicBackend::scheduleOutputRepaint(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return srmConnectorRepaint(bkndOutput->conn);
}

void LGraphicBackend::uninitializeOutput(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    UInt32 texturesCount = srmConnectorGetBuffersCount(bkndOutput->conn);
    srmConnectorUninitialize(bkndOutput->conn);

    if (bkndOutput->textures)
    {
        for (UInt32 i = 0; i < texturesCount; i++)
        {
            if (bkndOutput->textures[i])
            {
                // Do not destroy connectors native buffer
                bkndOutput->textures[i]->imp()->graphicBackendData = nullptr;
                delete bkndOutput->textures[i];
                bkndOutput->textures[i] = nullptr;
            }
        }

        free(bkndOutput->textures);
        bkndOutput->textures = nullptr;
    }
}

bool LGraphicBackend::hasBufferDamageSupport(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return srmConnectorHasBufferDamageSupport(bkndOutput->conn);
}

void LGraphicBackend::setOutputBufferDamage(LOutput *output, LRegion &region)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;

    if (!srmConnectorHasBufferDamageSupport(bkndOutput->conn) || srmConnectorGetState(bkndOutput->conn) != SRM_CONNECTOR_STATE_INITIALIZED)
        return;

    Int32 n;
    LBox *boxes = region.boxes(&n);

    SRMRect rects[n];

    for (Int32 i = 0; i < n; i++)
    {
        rects[i].x = boxes[i].x1;
        rects[i].y = boxes[i].y1;
        rects[i].width = boxes[i].x2 - boxes[i].x1;
        rects[i].height = boxes[i].y2 - boxes[i].y1;
    }

    srmConnectorSetBufferDamage(bkndOutput->conn, rects, n);
}

const LSize *LGraphicBackend::getOutputPhysicalSize(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return &bkndOutput->physicalSize;
}

Int32 LGraphicBackend::getOutputCurrentBufferIndex(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return srmConnectorGetCurrentBufferIndex(bkndOutput->conn);
}

UInt32 LGraphicBackend::getOutputBuffersCount(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return srmConnectorGetBuffersCount(bkndOutput->conn);
}

LTexture *LGraphicBackend::getOutputBuffer(LOutput *output, UInt32 bufferIndex)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;

    SRMBuffer *buffer = srmConnectorGetBuffer(bkndOutput->conn, bufferIndex);
    UInt32 buffersCount = srmConnectorGetBuffersCount(bkndOutput->conn);

    if (!buffer || !buffersCount)
        return nullptr;

    if (!bkndOutput->textures)
        bkndOutput->textures = (LTexture**)calloc(buffersCount, sizeof(LTexture*));

    if (bkndOutput->textures[bufferIndex])
        return bkndOutput->textures[bufferIndex];

    LTexture *tex = new LTexture();
    tex->imp()->graphicBackendData = buffer;
    tex->imp()->format = srmBufferGetFormat(buffer);
    tex->imp()->sizeB.setW(srmBufferGetWidth(buffer));
    tex->imp()->sizeB.setH(srmBufferGetHeight(buffer));
    bkndOutput->textures[bufferIndex] = tex;
    return tex;
}

const char *LGraphicBackend::getOutputName(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return srmConnectorGetName(bkndOutput->conn);
}

const char *LGraphicBackend::getOutputManufacturerName(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return srmConnectorGetManufacturer(bkndOutput->conn);
}

const char *LGraphicBackend::getOutputModelName(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return srmConnectorGetModel(bkndOutput->conn);
}

const char *LGraphicBackend::getOutputDescription(LOutput *output)
{
    L_UNUSED(output);
    return "DRM connector";
}

const LOutputMode *LGraphicBackend::getOutputPreferredMode(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    SRMConnectorMode *mode = srmConnectorGetPreferredMode(bkndOutput->conn);
    return (LOutputMode*)srmConnectorModeGetUserData(mode);
}

const LOutputMode *LGraphicBackend::getOutputCurrentMode(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    SRMConnectorMode *mode = srmConnectorGetCurrentMode(bkndOutput->conn);
    return (LOutputMode*)srmConnectorModeGetUserData(mode);
}

const std::list<LOutputMode *> *LGraphicBackend::getOutputModes(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return &bkndOutput->modes;
}

bool LGraphicBackend::setOutputMode(LOutput *output, LOutputMode *mode)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    OutputMode *bkndOutputMode = (OutputMode*)mode->imp()->graphicBackendData;
    return srmConnectorSetMode(bkndOutput->conn, bkndOutputMode->mode);
}

const LSize *LGraphicBackend::getOutputModeSize(LOutputMode *mode)
{
    OutputMode *bkndOutputMode = (OutputMode*)mode->imp()->graphicBackendData;
    return &bkndOutputMode->size;
}

Int32 LGraphicBackend::getOutputModeRefreshRate(LOutputMode *mode)
{
    OutputMode *bkndOutputMode = (OutputMode*)mode->imp()->graphicBackendData;
    return srmConnectorModeGetRefreshRate(bkndOutputMode->mode)*1000;
}

bool LGraphicBackend::getOutputModeIsPreferred(LOutputMode *mode)
{
    OutputMode *bkndOutputMode = (OutputMode*)mode->imp()->graphicBackendData;
    return srmConnectorModeIsPreferred(bkndOutputMode->mode);
}

bool LGraphicBackend::hasHardwareCursorSupport(LOutput *output)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    return srmConnectorHasHardwareCursor(bkndOutput->conn);
}

void LGraphicBackend::setCursorTexture(LOutput *output, UChar8 *buffer)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    srmConnectorSetCursor(bkndOutput->conn, buffer);
}

void LGraphicBackend::setCursorPosition(LOutput *output, const LPoint &position)
{
    Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
    srmConnectorSetCursorPos(bkndOutput->conn, position.x(), position.y());
}

const list<LDMAFormat*> *LGraphicBackend::getDMAFormats()
{
    LCompositor *compositor = LCompositor::compositor();
    Backend *bknd = (Backend*)compositor->imp()->graphicBackendData;
    return &bknd->dmaFormats;
}

EGLDisplay LGraphicBackend::getAllocatorEGLDisplay()
{
    LCompositor *compositor = LCompositor::compositor();
    Backend *bknd = (Backend*)compositor->imp()->graphicBackendData;
    return srmDeviceGetEGLDisplay(srmCoreGetAllocatorDevice(bknd->core));
}

EGLContext LGraphicBackend::getAllocatorEGLContext()
{
    LCompositor *compositor = LCompositor::compositor();
    Backend *bknd = (Backend*)compositor->imp()->graphicBackendData;
    return srmDeviceGetEGLContext(srmCoreGetAllocatorDevice(bknd->core));
}

bool LGraphicBackend::createTextureFromCPUBuffer(LTexture *texture, const LSize &size, UInt32 stride, UInt32 format, const void *pixels)
{
    Backend *bknd = (Backend*)LCompositor::compositor()->imp()->graphicBackendData;
    SRMBuffer *bkndBuffer = srmBufferCreateFromCPU(bknd->core, NULL, size.w(), size.h(), stride, pixels, format);

    if (bkndBuffer)
    {
        texture->imp()->graphicBackendData = bkndBuffer;
        return true;
    }

    return false;
}

bool LGraphicBackend::createTextureFromWaylandDRM(LTexture *texture, void *wlBuffer)
{
    Backend *bknd = (Backend*)LCompositor::compositor()->imp()->graphicBackendData;
    SRMBuffer *bkndBuffer = srmBufferCreateFromWaylandDRM(bknd->core, wlBuffer);

    if (bkndBuffer)
    {
        texture->imp()->graphicBackendData = bkndBuffer;
        texture->imp()->format = srmBufferGetFormat(bkndBuffer);
        texture->imp()->sizeB.setW(srmBufferGetWidth(bkndBuffer));
        texture->imp()->sizeB.setH(srmBufferGetHeight(bkndBuffer));
        return true;
    }

    return false;
}

bool LGraphicBackend::createTextureFromDMA(LTexture *texture, const LDMAPlanes *planes)
{
    Backend *bknd = (Backend*)LCompositor::compositor()->imp()->graphicBackendData;
    SRMBuffer *bkndBuffer = srmBufferCreateFromDMA(bknd->core, NULL, (SRMBufferDMAData*)planes);

    if (bkndBuffer)
    {
        texture->imp()->graphicBackendData = bkndBuffer;
        texture->imp()->format = srmBufferGetFormat(bkndBuffer);
        texture->imp()->sizeB.setW(srmBufferGetWidth(bkndBuffer));
        texture->imp()->sizeB.setH(srmBufferGetHeight(bkndBuffer));
        return true;
    }

    return false;
}

bool LGraphicBackend::updateTextureRect(LTexture *texture, UInt32 stride, const LRect &dst, const void *pixels)
{
    SRMBuffer *bkndBuffer = (SRMBuffer*)texture->imp()->graphicBackendData;
    return srmBufferWrite(bkndBuffer, stride, dst.x(), dst.y(), dst.w(), dst.h(), pixels);
}

UInt32 LGraphicBackend::getTextureID(LOutput *output, LTexture *texture)
{
    SRMDevice *bkndRendererDevice;

    if (output)
    {
        Output *bkndOutput = (Output*)output->imp()->graphicBackendData;
        bkndRendererDevice = srmDeviceGetRendererDevice(srmConnectorGetDevice(bkndOutput->conn));
    }
    else
    {
        Backend *bknd = (Backend*)LCompositor::compositor()->imp()->graphicBackendData;
        bkndRendererDevice = srmCoreGetAllocatorDevice(bknd->core);
    }

    return srmBufferGetTextureID(bkndRendererDevice, (SRMBuffer*)texture->imp()->graphicBackendData);
}

GLenum LGraphicBackend::getTextureTarget(LTexture *texture)
{
    SRMBuffer *bkndBuffer = (SRMBuffer*)texture->imp()->graphicBackendData;
    return srmBufferGetTextureTarget(bkndBuffer);
}

void LGraphicBackend::destroyTexture(LTexture *texture)
{
    SRMBuffer *buffer = (SRMBuffer*)texture->imp()->graphicBackendData;

    if (buffer)
        srmBufferDestroy(buffer);
}

static LGraphicBackendInterface API;

extern "C" LGraphicBackendInterface *getAPI()
{
    API.id = &LGraphicBackend::id;
    API.getContextHandle = &LGraphicBackend::getContextHandle;
    API.initialize = &LGraphicBackend::initialize;
    API.pause = &LGraphicBackend::pause;
    API.resume = &LGraphicBackend::resume;
    API.scheduleOutputRepaint = &LGraphicBackend::scheduleOutputRepaint;
    API.uninitialize = &LGraphicBackend::uninitialize;
    API.getConnectedOutputs = &LGraphicBackend::getConnectedOutputs;
    API.rendererGPUs = &LGraphicBackend::rendererGPUs;
    API.initializeOutput = &LGraphicBackend::initializeOutput;
    API.uninitializeOutput = &LGraphicBackend::uninitializeOutput;
    API.hasBufferDamageSupport = &LGraphicBackend::hasBufferDamageSupport;
    API.setOutputBufferDamage = &LGraphicBackend::setOutputBufferDamage;
    API.getOutputPhysicalSize = &LGraphicBackend::getOutputPhysicalSize;
    API.getOutputCurrentBufferIndex = &LGraphicBackend::getOutputCurrentBufferIndex;
    API.getOutputBuffersCount = &LGraphicBackend::getOutputBuffersCount;
    API.getOutputBuffer = &LGraphicBackend::getOutputBuffer;
    API.getOutputName = &LGraphicBackend::getOutputName;
    API.getOutputManufacturerName = &LGraphicBackend::getOutputManufacturerName;
    API.getOutputModelName = &LGraphicBackend::getOutputModelName;
    API.getOutputDescription = &LGraphicBackend::getOutputDescription;
    API.getOutputPreferredMode = &LGraphicBackend::getOutputPreferredMode;
    API.getOutputCurrentMode = &LGraphicBackend::getOutputCurrentMode;
    API.getOutputModes = &LGraphicBackend::getOutputModes;
    API.setOutputMode = &LGraphicBackend::setOutputMode;
    API.getOutputModeSize = &LGraphicBackend::getOutputModeSize;
    API.getOutputModeRefreshRate = &LGraphicBackend::getOutputModeRefreshRate;
    API.getOutputModeIsPreferred = &LGraphicBackend::getOutputModeIsPreferred;
    API.hasHardwareCursorSupport = &LGraphicBackend::hasHardwareCursorSupport;
    API.setCursorTexture = &LGraphicBackend::setCursorTexture;
    API.setCursorPosition = &LGraphicBackend::setCursorPosition;

    // Buffers
    API.getDMAFormats = &LGraphicBackend::getDMAFormats;
    API.getAllocatorEGLDisplay = &LGraphicBackend::getAllocatorEGLDisplay;
    API.getAllocatorEGLContext = &LGraphicBackend::getAllocatorEGLContext;
    API.createTextureFromCPUBuffer = &LGraphicBackend::createTextureFromCPUBuffer;
    API.createTextureFromWaylandDRM = &LGraphicBackend::createTextureFromWaylandDRM;
    API.createTextureFromDMA = &LGraphicBackend::createTextureFromDMA;
    API.updateTextureRect = &LGraphicBackend::updateTextureRect;
    API.getTextureID = &LGraphicBackend::getTextureID;
    API.getTextureTarget = &LGraphicBackend::getTextureTarget;
    API.destroyTexture = &LGraphicBackend::destroyTexture;

    return &API;
}
