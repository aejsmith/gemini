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

#include "TestGame.h"

#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/Entity.h"
#include "Engine/JSONSerialiser.h"
#include "Engine/Mesh.h"
#include "Engine/World.h"

#include "Render/BasicRenderPipeline.h"
#include "Render/Camera.h"
#include "Render/MeshRenderer.h"

TestGame::TestGame()
{
}

TestGame::~TestGame()
{
}

void TestGame::Init()
{
    Engine::Get().LoadWorld("Game/Worlds/Test");

    World* world = Engine::Get().GetWorld();

    Entity* playerEntity = world->CreateEntity("Player");
    playerEntity->SetActive(true);

    Camera* camera = playerEntity->CreateComponent<Camera>();
    camera->SetActive(true);

    auto renderPipeline = static_cast<BasicRenderPipeline*>(camera->renderPipeline.Get());
    renderPipeline->clearColour = glm::vec4(0.0f, 0.0f, 0.2f, 1.0f);

    MeshPtr mesh = AssetManager::Get().Load<Mesh>("Game/Meshes/CompanionCube");

    Entity* cubeEntity = world->CreateEntity("Cube");
    cubeEntity->Translate(glm::vec3(0.0f, 0.0f, -3.0f));
    cubeEntity->SetActive(true);

    MeshRenderer* meshRenderer = cubeEntity->CreateComponent<MeshRenderer>();
    meshRenderer->SetMesh(mesh);
    meshRenderer->SetActive(true);

    //Entity* entity1 = world->CreateEntity("Test");
    //entity1->Translate(glm::vec3(0.0f, 1.5f, 0.0f));
    //entity1->SetActive(true);

    //JSONSerialiser serialiser;
    //std::vector<uint8_t> data = serialiser.Serialise(world);
    //std::unique_ptr<File> file(Filesystem::OpenFile("derp.object", kFileMode_Write | kFileMode_Create | kFileMode_Truncate));
    //file->Write(&data[0], data.size());
}

const char* TestGame::GetName() const
{
    return "Test";
}
