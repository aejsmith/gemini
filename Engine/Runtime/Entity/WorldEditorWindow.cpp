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

#include "Entity/WorldEditorWindow.h"

#include "Core/Utility.h"

#include "Engine/AssetManager.h"

#include "Entity/Component.h"
#include "Entity/Entity.h"
#include "Entity/World.h"

WorldEditorWindow::WorldEditorWindow(World* const inWorld) :
    DebugWindow     ("Entity", "World Editor"),
    mWorld          (inWorld),
    mCurrentEntity  (nullptr),
    mEntityToOpen   (nullptr)
{
}

void WorldEditorWindow::Render()
{
    ImGuiIO &io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 450 - 10, 30), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(450, io.DisplaySize.y - 40), ImGuiCond_Once);

    if (!Begin(ImGuiWindowFlags_MenuBar))
    {
        return;
    }

    if (!mCurrentEntity)
    {
        mCurrentEntity = mWorld->GetRoot();
    }

    RenderMenuBar();

    RenderEntityTree();
    ImGui::Separator();
    ImGui::Spacing();

    RenderEntityEditor();

    ImGui::End();
}

void WorldEditorWindow::RenderMenuBar()
{
    if (!ImGui::BeginMenuBar())
    {
        return;
    }

    bool openSave         = false;
    bool openAddChild     = false;
    bool openAddComponent = false;

    if (ImGui::BeginMenu("World"))
    {
        openSave = ImGui::MenuItem("Save...");

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Entity"))
    {
        openAddChild     = ImGui::MenuItem("Add Child...");
        openAddComponent = ImGui::MenuItem("Add Component...");

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();

    static char nameBuf[128] = {};

    auto NamePopup = [&] (const char* const inTitle,
                          const char* const inText) -> bool
    {
        bool result = false;

        if (ImGui::BeginPopupModal(inTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text(inText);

            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
            }

            ImGui::PushItemWidth(-1);
            const bool ok = ImGui::InputText("", &nameBuf[0], ArraySize(nameBuf), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();

            ImGui::Spacing();

            if (ok || ImGui::Button("OK", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();

                result = true;
            }

            ImGui::SameLine();

            if (ImGui::Button("Cancel", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        return result;
    };

    if (openSave)
    {
        ImGui::OpenPopup("Save");
        strncpy(nameBuf, mWorld->GetPath().c_str(), ArraySize(nameBuf) - 1);
        nameBuf[ArraySize(nameBuf) - 1] = 0;
    }
    if (NamePopup("Save", "Asset path:"))
    {
        if (!AssetManager::Get().SaveAsset(mWorld, nameBuf))
        {
            ImGui::OpenPopup("Save Error");
        }
    }
    if (ImGui::BeginPopupModal("Save Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Failed to save world to '%s' (see log for details)", nameBuf);
        ImGui::Spacing();

        if (ImGui::Button("OK", ImVec2(-1, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (openAddChild)
    {
        ImGui::OpenPopup("Add Child");
        nameBuf[0] = 0;
    }
    if (NamePopup("Add Child", "Entity name:"))
    {
        mEntityToOpen  = mCurrentEntity;
        mCurrentEntity = mCurrentEntity->CreateChild(nameBuf);
    }

    if (openAddComponent)
    {
        ImGui::OpenPopup("Add Component");
    }
    if (ImGui::BeginPopupModal("Add Component", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::IsWindowAppearing())
        {
            ImGui::SetKeyboardFocusHere();
        }

        const MetaClass* const componentClass = Component::staticMetaClass.DebugUIClassSelector();
        if (componentClass)
        {
            ImGui::CloseCurrentPopup();
            mCurrentEntity->CreateComponent(*componentClass);
        }

        if (ImGui::Button("Cancel", ImVec2(-1, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void WorldEditorWindow::RenderEntityTree()
{
    ImGui::BeginChild("EntityTree",
                      ImVec2(0, ImGui::GetContentRegionAvail().y * 0.3f),
                      false);

    Entity* nextEntity = nullptr;

    std::function<void (Entity*)> AddEntity = [&] (Entity* const inEntity)
    {
        ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow |
                                       ImGuiTreeNodeFlags_OpenOnDoubleClick;

        if (inEntity == mWorld->GetRoot())
        {
            nodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        if (inEntity == mCurrentEntity)
        {
            nodeFlags |= ImGuiTreeNodeFlags_Selected;
        }

        if (inEntity->GetChildren().IsEmpty())
        {
            nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        if (inEntity == mEntityToOpen)
        {
            ImGui::SetNextTreeNodeOpen(true);
            mEntityToOpen = nullptr;
        }

        bool nodeOpen = ImGui::TreeNodeEx(inEntity, nodeFlags, "%s", inEntity->GetName().c_str());

        if (ImGui::IsItemClicked())
        {
            nextEntity = inEntity;
        }

        if (nodeOpen)
        {
            for (const Entity* child : inEntity->GetChildren())
            {
                AddEntity(const_cast<Entity*>(child));
            }

            if (!inEntity->GetChildren().IsEmpty())
            {
                ImGui::TreePop();
            }
        }
    };

    AddEntity(mWorld->GetRoot());

    /* Change outside loop to avoid visual inconsistency if selection changes. */
    if (nextEntity)
    {
        mCurrentEntity = nextEntity;
    }

    ImGui::EndChild();
}

void WorldEditorWindow::RenderEntityEditor()
{
    ImGui::BeginChild("EntityEditor", ImVec2(0, 0), false);
    auto guard = MakeScopeGuard([] { ImGui::EndChild(); });

    if (!mCurrentEntity)
    {
        return;
    }

    ImGui::Text("Entity '%s'", mCurrentEntity->GetPath().c_str());
    ImGui::Spacing();

    /* Editor for base entity properties. We do not allow these to be edited
     * on the root, since this is just the transformation and the root cannot
     * be transformed. */
    if (mCurrentEntity->GetParent())
    {
        bool destroyEntity = false;
        mCurrentEntity->DebugUIEditor(Object::kDebugUIEditor_IncludeChildren | Object::kDebugUIEditor_AllowDestruction, &destroyEntity);
        if (destroyEntity)
        {
            Entity* nextEntity = mCurrentEntity->GetParent();
            mCurrentEntity->Destroy();
            mCurrentEntity = nextEntity;
            return;
        }
    }

    Component* toDestroy = nullptr;

    /* Editor for each component's properties. */
    for (Component* component : mCurrentEntity->GetComponents())
    {
        bool destroyComponent = false;
        component->DebugUIEditor(Object::kDebugUIEditor_IncludeChildren | Object::kDebugUIEditor_AllowDestruction, &destroyComponent);
        if (destroyComponent)
        {
            /* Must destroy after the loop, otherwise we modify the list while
             * it is in use and break things. */
            toDestroy = component;
        }
    }

    if (toDestroy)
    {
        toDestroy->Destroy();
    }
}
