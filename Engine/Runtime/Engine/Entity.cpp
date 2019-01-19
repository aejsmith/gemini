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

#include "Engine/Entity.h"

#include "Engine/Component.h"
#include "Engine/Serialiser.h"
#include "Engine/World.h"

Entity::Entity() :
    mWorld          (nullptr),
    mParent         (nullptr),
    mActive         (false),
    mActiveInWorld  (false)
{
}

Entity::~Entity()
{
    /* An entity is deleted when its reference count becomes 0. This should
     * only happen if we have called Destroy() to remove references to the
     * entity from the world. */
    AssertMsg(!mActive && mComponents.empty() && mChildren.IsEmpty() && !mParent,
              "Entity '%s' has no remaining references yet has not been destroyed",
              mName.c_str());
}

void Entity::Destroy()
{
    SetActive(false);

    while (!mChildren.IsEmpty())
    {
        /* The child's Destroy() function removes it from the list (see below). */
        mChildren.Last()->Destroy();
    }

    while (!mComponents.empty())
    {
        /* Same as above. */
        mComponents.back()->Destroy();
    }

    if (mParent)
    {
        EntityPtr parent(std::move(mParent));
        parent->mChildren.Remove(this);

        /* Parent holds a reference to us as well. This will cause us to be
         * destroyed if this was the last reference. There could still be
         * remaining references to the entity if there are any external
         * references to it or its children (or any components). Warn if this
         * happens since it could indicate behaviour that will cause a memory
         * leak. */
        const int32_t count = Release();
        if (count != 0)
        {
            LogDebug("Entity '%s' still has remaining references after destruction request. Check if this is expected",
                     mName.c_str());
        }
    }
}

void Entity::Serialise(Serialiser& inSerialiser) const
{
    /* Serialise a reference to our world and our parent (see Deserialise()). */
    inSerialiser.Write("world",  mWorld);
    inSerialiser.Write("parent", mParent);

    if (mParent)
    {
        Object::Serialise(inSerialiser);
    }

    inSerialiser.BeginArray("components");

    for (Component* component : mComponents)
    {
        inSerialiser.Push(component);
    }

    inSerialiser.EndArray();

    inSerialiser.BeginArray("children");

    for (const Entity* child : mChildren)
    {
        inSerialiser.Push(child);
    }

    inSerialiser.EndArray();
}

void Entity::Deserialise(Serialiser& inSerialiser)
{
    /* At this point we are not associated with our parent or a world. The
     * first thing we must do *before* we deserialise any properties is to set
     * up this association. Due to references held by other objects, it may be
     * the case that we are actually instantiated before our parent (rather
     * than as a result of the parent's deserialisation). This ensures that the
     * parent and all of its components are instantiated before we try to set
     * any of our properties. Note that we don't get added to the parent's
     * child list until its Deserialise() call reaches us, to ensure that the
     * correct child order is maintained. */
    inSerialiser.Read("world", mWorld);
    inSerialiser.Read("parent", mParent);

    /* If this is the root entity, we don't deserialise properties. Two reasons:
     * firstly, the root entity's transformation cannot be changed anyway. Due
     * to floating point inaccuracy, deserialising the transformation can
     * trigger the assertion in UpdateTransform() to ensure that the root is not
     * transformed. Secondly, we do not want to activate things in the middle of
     * deserialisation as this will cause problems. We instead delay activation
     * to the end of deserialisation (in World::Deserialise()). */
    if (mParent)
    {
        Object::Deserialise(inSerialiser);
    }

    /* Deserialise components. We want these all available before our children. */
    if (inSerialiser.BeginArray("components"))
    {
        ComponentPtr component;
        while (inSerialiser.Pop(component))
        {
            AddComponent(std::move(component));
        }

        inSerialiser.EndArray();
    }

    /* Deserialise children. */
    if (inSerialiser.BeginArray("children"))
    {
        EntityPtr entity;
        while (inSerialiser.Pop(entity))
        {
            AddChild(entity);
        }

        inSerialiser.EndArray();
    }
}

void Entity::SetName(std::string inName)
{
    Assert(inName.find('/') == std::string::npos);
    mName = std::move(inName);
}

void Entity::SetActive(const bool inActive)
{
    mActive = inActive;

    if (mActive)
    {
        if ((!mParent || mParent->GetActiveInWorld()) && !GetActiveInWorld())
        {
            Activate();
        }
    }
    else
    {
        if (GetActiveInWorld())
        {
            Deactivate();
        }
    }
}

void Entity::Activate()
{
    Assert(mActive);
    Assert(!mActiveInWorld);

    mActiveInWorld = true;

    /* Order is important: components become activated before child entities
     * do. */
    for (Component* component : mComponents)
    {
        if (component->GetActive())
        {
            component->Activated();
        }
    }

    for (Entity* entity : mChildren)
    {
        if (entity->GetActive())
        {
            entity->Activate();
        }
    }
}

void Entity::Deactivate()
{
    Assert(mActiveInWorld);

    for (Entity* entity : mChildren)
    {
        if (entity->GetActive())
        {
            entity->Deactivate();
        }
    }

    for (Component* component : mComponents)
    {
        if (component->GetActive())
        {
            component->Deactivated();
        }
    }

    mActiveInWorld = false;
}

Entity* Entity::CreateChild(std::string inName)
{
    Entity* const entity = new Entity();
    entity->SetName(std::move(inName));
    AddChild(entity);
    return entity;
}

void Entity::AddChild(Entity* const inEntity)
{
    inEntity->mWorld  = mWorld;
    inEntity->mParent = this;

    inEntity->Retain();

    mChildren.Append(inEntity);

    /* Update the cached world transform to incorporate our transformation. */
    inEntity->UpdateTransform();
}

Component* Entity::CreateComponent(const MetaClass& inMetaClass)
{
    AssertMsg(Component::staticMetaClass.IsBaseOf(inMetaClass),
              "Specified class must be derived from Component");

    ComponentPtr component = inMetaClass.Construct().StaticCast<Component>();
    Component* ret = component.Get();
    AddComponent(std::move(component));
    return ret;
}

Component* Entity::FindComponent(const MetaClass& inMetaClass,
                                 const bool       inExactClass) const
{
    for (Component* component : mComponents)
    {
        if (inExactClass)
        {
            if (&inMetaClass == &component->GetMetaClass())
            {
                return component;
            }
        }
        else
        {
            if (inMetaClass.IsBaseOf(component->GetMetaClass()))
            {
                return component;
            }
        }
    }

    return nullptr;
}

void Entity::AddComponent(ComponentPtr inComponent)
{
    /* This only checks for an exact match on class type, so for instance we
     * don't forbid multiple Behaviour-derived classes on the same object. */
    AssertMsg(!FindComponent(inComponent->GetMetaClass(), true),
             "Component of type '%s' already exists on entity '%s'",
             inComponent->GetMetaClass().GetName(), mName.c_str());

    inComponent->mEntity = this;
    mComponents.emplace_back(std::move(inComponent));

    /* We do not need to activate the component at this point as the component
     * is initially inactive. We do however need to let it do anything it needs
     * to with the new transformation. */
    mComponents.back()->Transformed();
}

void Entity::RemoveComponent(Component* const inComponent,
                             OnlyCalledBy<Component>)
{
    /* This avoids an unnecessary reference to the component compared to calling
     * vector::remove(), passing the raw pointer to that converts to a
     * reference. */
    for (auto it = mComponents.begin(); it != mComponents.end(); ++it)
    {
        if (inComponent == *it)
        {
            mComponents.erase(it);
            return;
        }
    }

    UnreachableMsg("Removing component '%s' which is not registered on entity '%s'",
                   inComponent->GetMetaClass().GetName(), mName.c_str());
}

void Entity::UpdateTransform()
{
    glm::vec3 worldPosition    = GetPosition();
    glm::quat worldOrientation = GetOrientation();
    glm::vec3 worldScale       = GetScale();

    /* Recalculate absolute transformations. We don't allow the root entity to
     * be transformed so we can skip this for entities at the root. */
    if (mParent && mParent->mParent)
    {
        glm::vec3 parentPosition    = mParent->GetWorldPosition();
        glm::quat parentOrientation = mParent->GetWorldOrientation();
        glm::vec3 parentScale       = mParent->GetWorldScale();

        worldPosition    = (parentOrientation * (parentScale * worldPosition)) + parentPosition;
        worldOrientation = parentOrientation * worldOrientation;
        worldScale       = parentScale * worldScale;
    }
    else if (!mParent)
    {
        AssertMsg(worldPosition == glm::vec3() && worldOrientation == glm::quat() && worldScale == glm::vec3(),
                  "Cannot transform root entity");
    }

    mWorldTransform.Set(worldPosition, worldOrientation, worldScale);

    /* Let components know about the transformation. */
    for (Component* component : mComponents)
    {
         component->Transformed();
    }

    /* Visit children and recalculate their transformations. */
    for (Entity* entity : mChildren)
    {
        entity->UpdateTransform();
    }
}

void Entity::SetTransform(const Transform& inTransform)
{
    mTransform = inTransform;

    UpdateTransform();
}

void Entity::SetTransform(const glm::vec3& inPosition,
                          const glm::quat& inOrientation,
                          const glm::vec3& inScale)
{
    mTransform.Set(inPosition, inOrientation, inScale);

    UpdateTransform();
}

void Entity::SetPosition(const glm::vec3& inPosition)
{
    mTransform.SetPosition(inPosition);

    UpdateTransform();
}

void Entity::SetOrientation(const glm::quat& inOrientation)
{
    mTransform.SetOrientation(inOrientation);

    UpdateTransform();
}

void Entity::SetScale(const glm::vec3& inScale)
{
    mTransform.SetScale(inScale);

    UpdateTransform();
}

void Entity::Translate(const glm::vec3& inVector)
{
    mTransform.SetPosition(mTransform.GetPosition() + inVector);

    UpdateTransform();
}

void Entity::Rotate(const glm::quat& inRotation)
{
    /* The order of this is important, quaternion multiplication is not
     * commutative. */
    mTransform.SetOrientation(inRotation * mTransform.GetOrientation());

    UpdateTransform();
}

void Entity::Rotate(const float      inAngle,
                    const glm::vec3& inAxis)
{
    Rotate(glm::angleAxis(glm::radians(inAngle), glm::normalize(inAxis)));
}
