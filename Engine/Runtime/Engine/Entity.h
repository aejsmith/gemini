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

#include "Core/IntrusiveList.h"
#include "Core/Math/Transform.h"

#include "Engine/Object.h"

class Component;
class World;

/**
 * All entities that exist in the game world are an instance of this class. It
 * defines basic properties, such as position and orientation. The behaviour of
 * an entity is defined by the components attached to it.
 *
 * Entities in the world form a tree. The transformation properties of an entity
 * are defined relative to its parent's transformation. The transformation
 * functions of this class operate on the relative transformation, except where
 * noted.
 */
class Entity final : public Object
{
    CLASS();

public:
    /**
     * Destroys the entity. This first deactivates the entity if it is active.
     * Then, all child entities are destroyed, followed by all attached
     * components. Finally the entity is removed from its parent. Once all other
     * remaining references to the entity are released, it will be deleted.
     */
    void                    Destroy();

    World*                  GetWorld() const    { return mWorld; }
    Entity*                 GetParent() const   { return mParent; }

    /**
     * Name of the entity. Cannot contain a '/': entities can be referred to
     * by a path in the hierarchy formed out of their names.
     */
    VPROPERTY(std::string, name);
    const std::string&      GetName() const { return mName; }
    void                    SetName(std::string inName);

    /**
     * Whether the entity is active. Even if an entity is marked active, it is
     * only really active in the world if all parents in the hierarchy are also
     * active. Use GetActiveInWorld() to check this.
     */
    VPROPERTY(bool, active);
    bool                    GetActive() const { return mActive; }
    void                    SetActive(const bool inActive);

    /**
     * Whether the entity is really active, based on the active property of this
     * entity and all of its parents.
     */
    bool                    GetActiveInWorld() const { return mActiveInWorld; }

    Entity*                 CreateChild(std::string inName);

    /**
     * Components.
     */

    template <typename T, typename ...Args>
    T*                      CreateComponent(Args&&... args);
    Component*              CreateComponent(const MetaClass& inMetaClass);

    /**
     * Find a component of a given class. If inExactClass is true, then the
     * component must be an instance of the exact class specified, otherwise
     * it can be an instance of that class or any derived from it.
     */
    template <typename T>
    T*                      FindComponent(const bool inExactClass = false) const;
    Component*              FindComponent(const MetaClass& inMetaClass,
                                          const bool       inExactClass = false) const;

    void                    RemoveComponent(Component* const inComponent,
                                            OnlyCalledBy<Component>);

    /**
     * Transformation.
     */

    const Transform&        GetTransform() const { return mTransform; }

    void                    SetTransform(const Transform& inTransform);
    void                    SetTransform(const glm::vec3& inPosition,
                                         const glm::quat& inOrientation,
                                         const glm::vec3& inScale);

    VPROPERTY(glm::vec3, position);
    const glm::vec3&        GetPosition() const { return mTransform.GetPosition(); }
    void                    SetPosition(const glm::vec3& inPosition);

    VPROPERTY(glm::quat, orientation);
    const glm::quat&        GetOrientation() const { return mTransform.GetOrientation(); }
    void                    SetOrientation(const glm::quat& inOrientation);

    VPROPERTY(glm::vec3, scale);
    const glm::vec3&        GetScale() const { return mTransform.GetScale(); }
    void                    SetScale(const glm::vec3& inScale);

    void                    Translate(const glm::vec3& inVector);
    void                    Rotate(const glm::quat& inRotation);
    void                    Rotate(const float      inAngle,
                                   const glm::vec3& inAxis);

    /**
     * World transformation is the effective transformation in the world based
     * on parent entities.
     */
    const Transform&        GetWorldTransform() const   { return mWorldTransform; }
    const glm::vec3&        GetWorldPosition() const    { return mWorldTransform.GetPosition(); }
    const glm::quat&        GetWorldOrientation() const { return mWorldTransform.GetOrientation(); }
    const glm::vec3&        GetWorldScale() const       { return mWorldTransform.GetScale(); }

private:
    /** Link to parent's child entity list. */
    IntrusiveListNode       mNode;
    using EntityList      = IntrusiveList<Entity, &Entity::mNode>;

    using ComponentArray  = std::vector<ObjectPtr<Component>>;

private:
                            Entity();
                            ~Entity();

    void                    Serialise(Serialiser& inSerialiser) const override;
    void                    Deserialise(Serialiser& inSerialiser) override;

    void                    Activate();
    void                    Deactivate();

    void                    AddChild(Entity* const inEntity);

    void                    AddComponent(ObjectPtr<Component> inComponent);

    void                    UpdateTransform();

private:
    World*                  mWorld;

    /**
     * Entity hierarchy. An entity references its parent and all of its
     * children. The reference to children keeps entities from being deleted
     * while they are still live. These references are released once an entity
     * is explicitly destroyed with Destroy(). The reason for the reference to
     * the parent is to keep the parents from being deleted if, after Destroy(),
     * an entity still has external references to it - we need to keep the
     * whole branch in the tree alive in this case.
     */
    ObjectPtr<Entity>       mParent;
    EntityList              mChildren;

    std::string             mName;
    bool                    mActive;
    bool                    mActiveInWorld;

    /**
     * Array of components. Components reference their parent, and entities
     * reference all their children. This is for the same reason as on the
     * entity hierarchy (see above).
     */
    ComponentArray          mComponents;

    Transform               mTransform;
    Transform               mWorldTransform;

    /* World needs to initialise the root entity. */
    friend class World;
};

using EntityPtr = ObjectPtr<Entity>;

template <typename T, typename... Args>
inline T* Entity::CreateComponent(Args&&... args)
{
    T* const component = new T(std::forward<Args>(args)...);
    AddComponent(component);
    return component;
}

template <typename T>
inline T* Entity::FindComponent(const bool inExactClass) const
{
    static_assert(std::is_base_of<Component, T>::value,
                  "Type must be derived from Component");

    return static_cast<T*>(FindComponent(T::staticMetaClass));
}
