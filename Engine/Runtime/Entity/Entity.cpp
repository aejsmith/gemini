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

#include "Entity/Entity.h"

#include "Engine/Serialiser.h"

#include "Entity/Component.h"
#include "Entity/World.h"

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
         * references to it or its children (or any components). */
        Release();
    }
}

void Entity::Serialise(Serialiser& serialiser) const
{
    /* Serialise a reference to our world and our parent (see Deserialise()). */
    serialiser.Write("world",  mWorld);
    serialiser.Write("parent", mParent);

    if (mParent)
    {
        Object::Serialise(serialiser);
    }

    serialiser.BeginArray("components");

    for (Component* component : mComponents)
    {
        serialiser.Push(component);
    }

    serialiser.EndArray();

    serialiser.BeginArray("children");

    for (const Entity* child : mChildren)
    {
        serialiser.Push(child);
    }

    serialiser.EndArray();
}

void Entity::Deserialise(Serialiser& serialiser)
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
    serialiser.Read("world", mWorld);
    serialiser.Read("parent", mParent);

    /* If this is the root entity, we don't deserialise properties. Two reasons:
     * firstly, the root entity's transformation cannot be changed anyway. Due
     * to floating point inaccuracy, deserialising the transformation can
     * trigger the assertion in UpdateTransform() to ensure that the root is not
     * transformed. Secondly, we do not want to activate things in the middle of
     * deserialisation as this will cause problems. We instead delay activation
     * to the end of deserialisation (in World::Deserialise()). */
    if (mParent)
    {
        Object::Deserialise(serialiser);
    }

    /* Deserialise components. We want these all available before our children. */
    if (serialiser.BeginArray("components"))
    {
        ComponentPtr component;
        while (serialiser.Pop(component))
        {
            AddComponent(std::move(component));
        }

        serialiser.EndArray();
    }

    /* Deserialise children. */
    if (serialiser.BeginArray("children"))
    {
        EntityPtr entity;
        while (serialiser.Pop(entity))
        {
            AddChild(entity);
        }

        serialiser.EndArray();
    }
}

void Entity::SetName(std::string name)
{
    Assert(!name.empty());
    Assert(name.find('/') == std::string::npos);

    mName = std::move(name);
}

std::string Entity::GetPath()
{
    if (mParent)
    {
        std::string path = mParent->GetPath();

        if (path[path.length() - 1] != '/')
        {
            path += '/';
        }

        path += mName;

        return path;
    }
    else
    {
        return "/";
    }
}

void Entity::SetActive(const bool active)
{
    mActive = active;

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

Entity* Entity::CreateChild(std::string name)
{
    Entity* const entity = new Entity();
    entity->SetName(std::move(name));
    AddChild(entity);
    return entity;
}

void Entity::AddChild(Entity* const entity)
{
    entity->mWorld  = mWorld;
    entity->mParent = this;

    entity->Retain();

    mChildren.Append(entity);

    /* Update the cached world transform to incorporate our transformation. */
    entity->UpdateTransform();
}

Component* Entity::CreateComponent(const MetaClass& metaClass)
{
    AssertMsg(Component::staticMetaClass.IsBaseOf(metaClass),
              "Specified class must be derived from Component");

    ComponentPtr component = metaClass.Construct().StaticCast<Component>();
    Component* ret = component.Get();
    AddComponent(std::move(component));
    return ret;
}

Component* Entity::FindComponent(const MetaClass& metaClass,
                                 const bool       exactClass) const
{
    for (Component* component : mComponents)
    {
        if (exactClass)
        {
            if (&metaClass == &component->GetMetaClass())
            {
                return component;
            }
        }
        else
        {
            if (metaClass.IsBaseOf(component->GetMetaClass()))
            {
                return component;
            }
        }
    }

    return nullptr;
}

void Entity::AddComponent(ComponentPtr component)
{
    /* This only checks for an exact match on class type, so for instance we
     * don't forbid multiple Behaviour-derived classes on the same object. */
    AssertMsg(!FindComponent(component->GetMetaClass(), true),
             "Component of type '%s' already exists on entity '%s'",
             component->GetMetaClass().GetName(), mName.c_str());

    component->mEntity = this;
    mComponents.emplace_back(std::move(component));

    /* We do not need to activate the component at this point as the component
     * is initially inactive. We do however need to let it do anything it needs
     * to with the new transformation. */
    mComponents.back()->Transformed();
}

void Entity::RemoveComponent(Component* const component,
                             OnlyCalledBy<Component>)
{
    /* This avoids an unnecessary reference to the component compared to calling
     * vector::remove(), passing the raw pointer to that converts to a
     * reference. */
    for (auto it = mComponents.begin(); it != mComponents.end(); ++it)
    {
        if (component == *it)
        {
            mComponents.erase(it);
            return;
        }
    }

    UnreachableMsg("Removing component '%s' which is not registered on entity '%s'",
                   component->GetMetaClass().GetName(), mName.c_str());
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

void Entity::SetTransform(const Transform& transform)
{
    mTransform = transform;

    UpdateTransform();
}

void Entity::SetTransform(const glm::vec3& position,
                          const glm::quat& orientation,
                          const glm::vec3& scale)
{
    mTransform.Set(position, orientation, scale);

    UpdateTransform();
}

void Entity::SetPosition(const glm::vec3& position)
{
    mTransform.SetPosition(position);

    UpdateTransform();
}

void Entity::SetOrientation(const glm::quat& orientation)
{
    mTransform.SetOrientation(orientation);

    UpdateTransform();
}

void Entity::SetScale(const glm::vec3& scale)
{
    mTransform.SetScale(scale);

    UpdateTransform();
}

void Entity::Translate(const glm::vec3& vector)
{
    mTransform.SetPosition(mTransform.GetPosition() + vector);

    UpdateTransform();
}

void Entity::Rotate(const glm::quat& rotation)
{
    /* The order of this is important, quaternion multiplication is not
     * commutative. */
    mTransform.SetOrientation(rotation * mTransform.GetOrientation());

    UpdateTransform();
}

void Entity::Rotate(const Degrees    angle,
                    const glm::vec3& axis)
{
    Rotate(glm::angleAxis(glm::radians(angle), glm::normalize(axis)));
}

void Entity::Tick(const float delta)
{
    // FIXME: This does not handle activation/deactivation quite well. When
    // an entity becomes active in a frame, it should *not* have it's tick
    // function called in the rest of the frame, otherwise it will get a
    // meaningless dt value. It shouldn't be called until next frame, where
    // dt would be time since activation.

    for (Component* component : mComponents)
    {
        if (component->GetActive())
        {
            component->Tick(delta);
        }
    }

    for (Entity* entity : mChildren)
    {
        if (entity->GetActive())
        {
            entity->Tick(delta);
        }
    }
}
