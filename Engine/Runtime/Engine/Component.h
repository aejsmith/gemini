/*
 * Copyright (C) 2018 Alex Smith
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

#pragma once

#include "Engine/Entity.h"

/**
 * Components implement the behaviour of an entity in the game world. The Entity
 * class only provides some minimal properties such as a transformation. All
 * other functionality is implemented in components which are attached to an
 * Entity.
 *
 * Components have a number of hook functions that get called from the Entity
 * to which they are attached, which can be overridden by derived classes to
 * implement their behaviour.
 *
 * Components must always be created through Entity::CreateComponent(). This
 * constructs the component and handles attaching it to the entity. They should
 * only be destroyed by calling Destroy(). The function call sequence for
 * creating a component is:
 *
 *   Entity::CreateComponent()
 *    |-> constructors
 *    |-> Entity::AddComponent()
 *    |-> Component::Transformed()
 *
 * The call sequence for destroying a component is:
 *
 *   Component::Destroy()
 *    |-> Component::Deactivated() (if currently active)
 *    |-> Entity::RemoveComponent()
 *    |-> destructors (once no other references remain)
 *
 * As can be seen, this ensures that the hook functions are called when the
 * component is fully constructed.
 */
class Component : public Object
{
    CLASS("constructable": false);

public:
    void                    Destroy();

    Entity*                 GetEntity() const { return mEntity; }

    /**
     * Whether the component is active. Even if a component is marked active,
     * it is only really active in the world if its entity is also active in
     * the world. Use GetActiveInWorld() to check this.
     */
    VPROPERTY(bool, active);
    bool                    GetActive() const { return mActive; }
    void                    SetActive(const bool inActive);

    /**
     * Whether the entity is really active, based on the active property of this
     * entity and all of its parents.
     */
    bool                    GetActiveInWorld() const;

    /**
     * Entity property shortcuts.
     */

    const Transform&        GetTransform() const        { return mEntity->GetTransform(); }
    const glm::vec3&        GetPosition() const         { return mEntity->GetPosition(); }
    const glm::quat&        GetOrientation() const      { return mEntity->GetOrientation(); }
    const glm::vec3&        GetScale() const            { return mEntity->GetScale(); }
    const Transform&        GetWorldTransform() const   { return mEntity->GetWorldTransform(); }
    const glm::vec3&        GetWorldPosition() const    { return mEntity->GetWorldPosition(); }
    const glm::quat&        GetWorldOrientation() const { return mEntity->GetWorldOrientation(); }
    const glm::vec3&        GetWorldScale() const       { return mEntity->GetWorldScale(); }

protected:
                            Component();
                            ~Component();

    void                    Serialise(Serialiser& inSerialiser) const override;
    void                    Deserialise(Serialiser& inSerialiser) override;

    /** Called when the component becomes (in)active in the world. */
    virtual void            Activated() {}
    virtual void            Deactivated() {}

    /** Called when the transformation changes. */
    virtual void            Transformed() {}

private:
    EntityPtr               mEntity;
    bool                    mActive;

    friend class Entity;
};

using ComponentPtr = ObjectPtr<Component>;

inline bool Component::GetActiveInWorld() const
{
    return mActive && mEntity->GetActiveInWorld();
}