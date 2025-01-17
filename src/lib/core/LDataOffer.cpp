#include "LLog.h"
#include <protocols/Wayland/RDataSource.h>
#include <protocols/Wayland/RDataOffer.h>

#include <private/LDataOfferPrivate.h>
#include <private/LDataDevicePrivate.h>
#include <private/LDNDManagerPrivate.h>
#include <private/LDataSourcePrivate.h>

#include <LClient.h>
#include <LSeat.h>

using namespace Louvre;

LDataOffer::LDataOffer(Protocols::Wayland::RDataOffer *dataOfferResource)
{
    m_imp = new LDataOfferPrivate();
    imp()->dataOfferResource = dataOfferResource;
}

LDataOffer::~LDataOffer()
{
    delete m_imp;
}

Protocols::Wayland::RDataOffer *LDataOffer::dataOfferResource() const
{
    return imp()->dataOfferResource;
}

LDataOffer::Usage LDataOffer::usedFor() const
{
    return imp()->usedFor;
}

// Since 3
void LDataOffer::LDataOfferPrivate::updateDNDAction()
{
    LDNDManager *dndManager = seat()->dndManager();

    if (!dndManager->focus())
        return;

    if (dndManager->focus()->client() != dataOfferResource->client())
        return;

    bool offerIsV3 = dataOfferResource->version() >= 3;
    UInt32 compositorAction = dndManager->preferredAction();
    UInt32 final = 0;

    // If has source
    if (dndManager->source())
    {
        bool sourceIsV3 = dndManager->source()->dataSourceResource()->version() >= 3;

        // If both are v3
        if (sourceIsV3 && offerIsV3)
        {
            // If offer has not sent preferred action
            if (preferredAction == LOUVRE_DND_NO_ACTION_SET)
            {
                caseA:
                UInt32 both = dndManager->source()->dndActions();

                final = both;

                if (compositorAction != LDNDManager::NoAction)
                    final = both & compositorAction;

                if (final & LDNDManager::Copy)
                    final = LDNDManager::Copy;

                else if (final & LDNDManager::Move)
                    final = LDNDManager::Move;

                else if (final & LDNDManager::Ask)
                    final = LDNDManager::Ask;

                else
                    final = LDNDManager::NoAction;

                dndManager->source()->dataSourceResource()->action(final);
            }
            // Offer has set action
            else
            {
                UInt32 both = dndManager->source()->dndActions() & acceptedActions;

                final = both;

                if (compositorAction != LDNDManager::NoAction)
                    final = both & compositorAction;

                if (final & preferredAction)
                    final = preferredAction;

                else if (final & LDNDManager::Copy)
                    final = LDNDManager::Copy;

                else if (final & LDNDManager::Move)
                    final = LDNDManager::Move;

                else if (final & LDNDManager::Ask)
                    final = LDNDManager::Ask;

                else
                    final = LDNDManager::NoAction;

                dataOfferResource->action(final);
                dndManager->source()->dataSourceResource()->action(final);
            }

        }
        else if (sourceIsV3 && !offerIsV3)
        {
            goto caseA;
        }
        else if (!sourceIsV3 && offerIsV3)
        {
            if (preferredAction != LOUVRE_DND_NO_ACTION_SET)
            {
                UInt32 both = acceptedActions;

                final = both;

                if (compositorAction != LDNDManager::NoAction)
                    final = both & compositorAction;

                if (final & preferredAction)
                    final = preferredAction;

                else if (final & LDNDManager::Copy)
                    final = LDNDManager::Copy;

                else if (final & LDNDManager::Move)
                    final = LDNDManager::Move;

                else if (final & LDNDManager::Ask)
                    final = LDNDManager::Ask;
                else
                    final = LDNDManager::NoAction;

                dataOfferResource->action(final);
            }
        }

    }
    // If no source
    else
    {
        if (offerIsV3 && preferredAction != LOUVRE_DND_NO_ACTION_SET)
        {
            UInt32 both = acceptedActions;

            final = both;

            if (compositorAction != LDNDManager::NoAction)
                final = both & compositorAction;

            if (final & preferredAction)
                final = preferredAction;

            else if (final & LDNDManager::Copy)
                final = LDNDManager::Copy;

            else if (final & LDNDManager::Move)
                final = LDNDManager::Move;

            else if (final & LDNDManager::Ask)
                final = LDNDManager::Ask;
            else
                final = LDNDManager::NoAction;

            dataOfferResource->action(final);
        }
    }
}
