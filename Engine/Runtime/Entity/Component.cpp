/*
 * Copyright (C) 2018-2019 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "Entity/Component.h"

#include "Engine/Serialiser.h"

Component::Component() :
    mActive (false)
{
}

Component::~Component()
{
}

void Component::Destroy()
{
    SetActive(false);

    mEntity->RemoveComponent(this, {});
}

void Component::Serialise(Serialiser& serialiser) const
{
    /* Serialise a reference to our entity (see Deserialise()). */
    serialiser.Write("entity", mEntity);

    /* Serialise properties. */
    Object::Serialise(serialiser);
}

void Component::Deserialise(Serialiser& serialiser)
{
    /* At this point we are not associated with our entity. Similarly to
     * Entity::Deserialise(), the first thing we must do *before* we deserialise
     * any properties is to set up this association and ensure the entity is
     * instantiated. We are added to the entity's component list by
     * Entity::Deserialise(), which ensures that order of components is
     * maintained. */
    serialiser.Read("entity", mEntity);

    /* Deserialise properties. */
    Object::Deserialise(serialiser);
}

void Component::SetActive(const bool active)
{
    const bool wasActive = GetActiveInWorld();

    mActive = active;

    if (mActive)
    {
        if (!wasActive && mEntity->GetActiveInWorld())
        {
            Activated();
        }
    }
    else
    {
        if (wasActive)
        {
            Deactivated();
        }
    }
}
