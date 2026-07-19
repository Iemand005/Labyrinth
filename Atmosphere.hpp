#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <EditableGame.hpp>
#include <Primitives.hpp>
#include <Graphics/VulkanDevice.hpp>
#include <openvr/OpenVR.hpp>

class Atmosphere : public fe::EditableGame {
public:

	bool showDebugUI = false;
	
	bool useRectangularPlayerHitbox = true;

	std::vector<glm::vec3> path;
	int windowStart = 0;
	float pathIndex = 1.0f;
	std::vector<std::shared_ptr<fe::Object<>>> chunkObjects;  // Track loaded chunk objects
	std::vector<bool> chunksLoaded;  // Track which chunks have been meshed
	glm::vec3 lastUp = glm::vec3(0, 1, 0);
	glm::vec3 lastRight = glm::vec3(1, 0, 0);
	glm::vec3 prevEndForward{0};
	bool hasPrevEnd = false;

	static constexpr int POINTS_PER_CHUNK = 4;
	static constexpr int SHIFT = 3;
	static constexpr int MAX_CHUNKS = 32;
	static constexpr int TUNNEL_SEGMENTS = 64;
	static constexpr int SUBDIVISIONS_PER_SEG = 48;
	int CHUNK_LOAD_DISTANCE = 2;  // Load chunks within this many chunks of player
	static constexpr int GRID_WIDTH = 1;  // 5x5 grid
	static constexpr int GRID_HEIGHT = 1;

	int NUM_CHUNKS = 4;

	float lightSpeed = 0.3f;

	float bgColorFreq = 0.3f;
	float visualizerScale = 8.0f;

	float audioAmplitudeScale = 10.0f;
	float audioSpeedMultiplier = 0.15f;
	float baseSpeedElapsedTimeBumpy = 0.0002f;
	float baseSpeedElapsedTime = 0.0002f;

	float cameraSpeed = 1.0f;
	float motionAmount = 1.2f;
	float tunnelRoundness = 0.0f;
	float animationSpeed = 1.0f;
	float farPlane = 1000.0f;
	bool freeCamera = false;
	float freeCamSpeed = 15.0f;
	float segmentLength = 12.0f;
	
	Atmosphere(int width = 1000, int height = 1000, bool vr = false) : fe::EditableGame(fe::XRGameOptions(width, height, vr)) {

		SetClearColor(0.1f, 0.3f, 1);

		// IF 
		if (!useVulkan)
			LoadShaders("resources/shaders/VertexShader.glsl", "resources/shaders/FragmentShader.glsl");

		LoadModels();

		GetPhysicsEngine()->EnableGravity();
	}

	void OnPreSwap() override {
		// ovr.CaptureAndSubmit();
	}

	void RebuildPlayerPhysicsBody() {
		auto physicsEngine = GetPhysicsEngine();
		if (!player || !physicsEngine) return;

		const glm::vec3 size = useRectangularPlayerHitbox ? glm::vec3(0.4f, 1.5f, 0.4f) : glm::vec3(1.0f, 1.0f, 1.0f);
		auto newPhysics = physicsEngine->CreateObject(size, true);
		if (!newPhysics) return;

		this->player->SetPhysicsObject(std::move(newPhysics));
		if (this->player->physicsObject) {
			this->player->physicsObject->SetPosition(this->player->state.position);
		}
	}

	void LoadModels() {

		this->player = std::make_shared<fe::Character>();
		this->scene->AddObject(player);
		this->player->state.position = glm::vec3(0.0f, 2.0f, 5.0f);
		this->player->gravityEnabled = true;
		RebuildPlayerPhysicsBody();
		if (this->player->physicsObject) {
			this->player->physicsObject->SetPosition(this->player->state.position);
		}

		auto sphereMesh = fe::Primitives::GenerateSphere(0.5f, 32, 24);
		auto sphereObject = std::make_shared<fe::Object<>>(sphereMesh);
		sphereObject->name = "Sphere";
		sphereObject->state.position = glm::vec3(2.0f, 1.0f, 0.0f);
		this->scene->AddObject(sphereObject);
	}

	void SyncCameraToPlayer() {
		if (!player || freeCamera) return;

		const glm::vec3 headOffset(0.0f, 1.6f, 0.0f);
		camera->SetPos(player->state.position + headOffset);
	}

	void ProcessInput() {
		SDL_Event event;
		fe::SDLWindow *window = (fe::SDLWindow*)this->window.get();
		while (window->PollSDLEvent(&event)) {
			// continue;
			ImGui_ImplSDL3_ProcessEvent(&event);
			auto io = ImGui::GetIO();
			switch (event.type) {
				case SDL_EVENT_QUIT:
					window->PrepareClose();
					break;
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					if (event.button.button == SDL_BUTTON_LEFT && !io.WantCaptureMouse) {
						window->StartMouseCapture();
					}
					break;
			case SDL_EVENT_WINDOW_RESIZED:
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
				break;
				case SDL_EVENT_MOUSE_MOTION:
				{
					if (!window->IsCapturingMouse()) break;
					float sensitivity = 0.1f;
					camera->yaw   += event.motion.xrel * sensitivity;
					camera->pitch -= event.motion.yrel * sensitivity;
					camera->UpdateDirection();
					camera->pitch = std::clamp(camera->pitch, -89.0f, 89.0f);
					break;
				}
				case SDL_EVENT_KEY_DOWN:
					if (event.key.key == SDLK_F11) {
						window->ToggleFullscreen();
					}
					else if (event.key.key == SDLK_F3) {
						showDebugUI = !showDebugUI;
					}
					else if (event.key.key == SDLK_F2) {
						freeCamera = !freeCamera;
						window->StartMouseCapture();
					}
					break;
			}
		}

		if (!freeCamera) {
			if (window->IsKeyDown(SDL_SCANCODE_W)) this->player->Move(fe::Direction::Forwards, camera.get());
			if (window->IsKeyDown(SDL_SCANCODE_A)) this->player->Move(fe::Direction::Left, camera.get());
			if (window->IsKeyDown(SDL_SCANCODE_S)) this->player->Move(fe::Direction::Backwards, camera.get());
			if (window->IsKeyDown(SDL_SCANCODE_D)) this->player->Move(fe::Direction::Right, camera.get());

			if (window->IsKeyDown(SDL_SCANCODE_SPACE)) this->player->Move(fe::Direction::Up, camera.get());
			if (window->IsKeyDown(SDL_SCANCODE_LSHIFT)) this->player->Move(fe::Direction::Down, camera.get());
		}

		if (window->IsKeyDown(SDL_SCANCODE_ESCAPE)) window->StopMouseCapture();
		if (ImGui::GetIO().WantCaptureMouse) window->StopMouseCapture();
	}

	void Run() {
		auto window = this->GetWindow<fe::SDLWindow>();
		window->Show();
		window->DisableVSync();

		player->state.position.z = 5;
		player->state.position.y = 2;
		if (player->physicsObject) {
			player->physicsObject->SetPosition(player->state.position);
		}
		camera->farDist = farPlane;
		camera->SetAspect(camera->aspect);
		SyncCameraToPlayer();
		float elapsedTimeBumpy = 0.0f;
		float elapsedTime = 0.0f;

		while (!window->ShouldClose()) {

			ProcessInput();

			if (!freeCamera) {
				SyncCameraToPlayer();
			}

			if (freeCamera) {
				double dt = fpsCounter.deltaTime;
				float spd = freeCamSpeed * dt;
				glm::vec3 cp = camera->GetPos();
				glm::vec3 right = glm::normalize(glm::cross(camera->front, camera->up));
				if (window->IsKeyDown(SDL_SCANCODE_W)) cp += camera->front * spd;
				if (window->IsKeyDown(SDL_SCANCODE_S)) cp -= camera->front * spd;
				if (window->IsKeyDown(SDL_SCANCODE_A)) cp -= right * spd;
				if (window->IsKeyDown(SDL_SCANCODE_D)) cp += right * spd;
				if (window->IsKeyDown(SDL_SCANCODE_SPACE)) cp += camera->up * spd;
				if (window->IsKeyDown(SDL_SCANCODE_LSHIFT)) cp -= camera->up * spd;
				camera->SetPos(cp);
			} else {
			}

			Update();
			Redraw();
		}

		Destroy();
	}

	void InitUI() override {}


	void DrawUI() override {
		if (!showDebugUI) return;
		BeginFrame();

		DrawDebugUI();

		EndFrame();
	}
};
