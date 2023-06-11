#include <private/LClientPrivate.h>
#include <private/LDataDevicePrivate.h>

#include <LKeyboard.h>
#include <LTime.h>
#include <LCompositor.h>
#include <LSeat.h>
#include <LDNDManager.h>
#include <LClient.h>
#include <LCursor.h>
#include <LOutput.h>
#include <LLog.h>

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

using namespace Louvre;

//! [keyModifiersEvent]
void LKeyboard::keyModifiersEvent(UInt32 depressed, UInt32 latched, UInt32 locked, UInt32 group)
{
    sendModifiersEvent(depressed, latched, locked, group);
}
//! [keyModifiersEvent]

//! [keyEvent]
void LKeyboard::keyEvent(UInt32 keyCode, UInt32 keyState)
{
    sendKeyEvent(keyCode, keyState);

    bool L_CTRL = isKeyCodePressed(KEY_LEFTCTRL);
    bool L_SHIFT = isKeyCodePressed(KEY_LEFTSHIFT);
    bool mods = isKeyCodePressed(KEY_LEFTALT) && L_CTRL;

    if (keyState == LIBINPUT_KEY_STATE_RELEASED)
    {
        // F1: Launches weston-terminal.
        if (keyCode == KEY_F1 && !mods)
        {
            if (fork() == 0)
            {
                system("weston-terminal");
                exit(0);
            }
        }

        // CTRL + SHIFT + ESC : Kils the compositor.
        else if (keyCode == KEY_ESC && L_CTRL && L_SHIFT)
        {
            LLog::warning("Killing compositor.");
            compositor()->finish();
        }

        // F8 : Unminimize all surfaces.
        else if (keyCode == KEY_F8 && !mods)
        {
            for (LSurface *surface : compositor()->surfaces())
                surface->setMinimized(false);

            compositor()->repaintAllOutputs();
        }

        // CTRL: Unsets the **Copy** as the preferred action in drag & drop sesión
        else if (L_CTRL)
        {
            if (seat()->dndManager()->preferredAction() == LDNDManager::Copy)
                seat()->dndManager()->setPreferredAction(LDNDManager::NoAction);
        }

        // SHIFT: Unsets the **Move** as the preferred action in drag & drop sesión
        else if (L_SHIFT)
        {
            if (seat()->dndManager()->preferredAction() == LDNDManager::Move)
                seat()->dndManager()->setPreferredAction(LDNDManager::NoAction);
        }
    }
    // Key press
    else
    {
        // CTRL: Sets the **Copy** as the preferred action in drag & drop sesión
        if (L_CTRL)
        {
            seat()->dndManager()->setPreferredAction(LDNDManager::Copy);
        }

        // SHIFT: Sets the **Move** as the preferred action in drag & drop sesión
        else if (L_SHIFT)
        {
            seat()->dndManager()->setPreferredAction(LDNDManager::Move);
        }
    }
}
//! [keyEvent]
