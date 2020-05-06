/*
 * Copyright (C) 2018-2020 Alex Smith
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

#include "PlayerController.h"

#include "Core/Filesystem.h"

#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/JSONSerialiser.h"
#include "Engine/Mesh.h"

#include "Entity/Entity.h"
#include "Entity/World.h"

#include "Loaders/GLTFImporter.h"

#include "Render/Camera.h"
#include "Render/Light.h"
#include "Render/Material.h"
#include "Render/MeshRenderer.h"

#include <memory>

TestGame::TestGame()
{
}

TestGame::~TestGame()
{
}

static void ImportGLTFWorld(const Path& path,
                            const Path& assetDir,
                            const Path& worldPath)
{
    Engine::Get().CreateWorld();
    World* const world = Engine::Get().GetWorld();

    /* Create a camera, offset along Z behind the model since the model origin
     * will be at (0, 0). TODO: glTF has optional cameras. */
    Entity* playerEntity = world->CreateEntity("Player");
    playerEntity->Translate(glm::vec3(0.0f, 0.0f, 3.0f));
    playerEntity->SetActive(true);

    Entity* cameraEntity = playerEntity->CreateChild("Camera");
    cameraEntity->SetActive(true);

    Camera* camera = cameraEntity->CreateComponent<Camera>();
    camera->SetActive(true);

    PlayerController* controller = playerEntity->CreateComponent<PlayerController>();
    controller->camera = camera;
    controller->SetActive(true);

    GLTFImporter importer;
    if (!importer.Import(path, assetDir, world))
    {
        Fatal("Failed to load '%s'", path.GetCString());
    }

    if (!AssetManager::Get().SaveAsset(world, worldPath))
    {
        Fatal("Failed to save world");
    }
}

void TestGame::Init()
{
#if 0
    ImportGLTFWorld("Games/Test/AssetSource/glTF/DamagedHelmet/DamagedHelmet.gltf",
                    "Game/glTF/DamagedHelmet",
                    "Game/glTF/DamagedHelmet/World");
#endif

    Engine::Get().LoadWorld("Game/Worlds/LightingTest3");

#if 0
    Entity* parent = Engine::Get().GetWorld()->CreateEntity("Lights");

    int lightIndex = 0;

    static glm::vec3 kColours[] =
    {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
    };

    for (int y : { -2, 34 })
    {
        for (int x = -18; x <= 18; x+= 4)
        {
            for (int z = -18; z <= 18; z+= 4)
            {
                Entity* entity = parent->CreateChild(StringUtils::Format("Light_%d_%d_%d", x, y, z));
                entity->SetPosition(glm::vec3(x, y, z));
                entity->SetActive(true);

                Light* light = entity->CreateComponent<Light>();
                light->SetType(kLightType_Point);
                light->SetColour(kColours[lightIndex % ArraySize(kColours)]);
                light->SetIntensity(15.0f);
                light->SetRange(5.0f);
                light->SetActive(true);

                lightIndex++;
            }
        }
    }

    parent->SetActive(true);
#endif
}

const char* TestGame::GetName() const
{
    return "Test";
}

const char* TestGame::GetTitle() const
{
    return "Test";
}
