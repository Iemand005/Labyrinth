#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <random>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/backends/imgui_impl_opengl3.h>

#include <EditableGame.hpp>
#include <Primitives.hpp>
#include <Sensors/Accelerometer.hpp>
#include <Graphics/VulkanDevice.hpp>
#ifdef FE_INCLUDE_OPENVR
#include <openvr/OpenVR.hpp>
#endif

class Labyrinth : public fe::EditableGame {
public:

	bool showDebugUI = false;
	bool useRectangularPlayerHitbox = true;
	bool hasWon = false;

	std::vector<fe::Accelerometer> accelerometers;
	std::vector<glm::vec3> accelReadings;
	int selectedAccel = 0;
	std::shared_ptr<fe::Object<>> ballObject;
	std::shared_ptr<fe::Object<>> goalObject;

	static constexpr int MAZE_COLS = 8;
	static constexpr int MAZE_ROWS = 8;
	static constexpr float CELL_SIZE = 2.0f;
	static constexpr float WALL_HEIGHT = 1.0f;
	static constexpr float WALL_THICK = 0.3f;
	static constexpr float BALL_RADIUS = 0.5f;

	struct MazeCell {
		bool wallN = true, wallS = true, wallE = true, wallW = true;
		bool visited = false;
	};

	MazeCell mazeGrid[MAZE_ROWS][MAZE_COLS];

	void GenerateMaze() {
		std::mt19937 rng(42);
		std::uniform_int_distribution<int> dirDist(0, 3);
		int dx[] = { 0, 0, 1, -1 };
		int dz[] = { -1, 1, 0, 0 };

		auto carve = [&](auto& self, int cx, int cz) -> void {
			mazeGrid[cz][cx].visited = true;

			int dirs[] = { 0, 1, 2, 3 };
			std::shuffle(dirs, dirs + 4, rng);

			for (int d = 0; d < 4; d++) {
				int nd = dirs[d];
				int nx = cx + dx[nd];
				int nz = cz + dz[nd];
				if (nx < 0 || nx >= MAZE_COLS || nz < 0 || nz >= MAZE_ROWS) continue;
				if (mazeGrid[nz][nx].visited) continue;

				if (nd == 0) { mazeGrid[cz][cx].wallN = false; mazeGrid[nz][nx].wallS = false; }
				if (nd == 1) { mazeGrid[cz][cx].wallS = false; mazeGrid[nz][nx].wallN = false; }
				if (nd == 2) { mazeGrid[cz][cx].wallE = false; mazeGrid[nz][nx].wallW = false; }
				if (nd == 3) { mazeGrid[cz][cx].wallW = false; mazeGrid[nz][nx].wallE = false; }

				self(self, nx, nz);
			}
		};

		carve(carve, 0, 0);
	}

	glm::vec3 CellToWorld(int col, int row) {
		float totalW = MAZE_COLS * CELL_SIZE;
		float totalH = MAZE_ROWS * CELL_SIZE;
		return glm::vec3(
			col * CELL_SIZE + CELL_SIZE * 0.5f - totalW * 0.5f,
			0.0f,
			row * CELL_SIZE + CELL_SIZE * 0.5f - totalH * 0.5f
		);
	}

	void AddWall(glm::vec3 pos, glm::vec3 size, glm::vec3 color) {
		auto wallMesh = fe::Primitives::GenerateCube(
			{fe::PlaneDirection::Front, fe::PlaneDirection::Back, fe::PlaneDirection::Left,
			 fe::PlaneDirection::Right, fe::PlaneDirection::Top, fe::PlaneDirection::Bottom},
			fe::Primitives::defaultUVs, 1.0f);
		auto wall = std::make_shared<fe::Object<>>(wallMesh);
		wall->name = "Wall";
		wall->state.position = pos;
		wall->state.scale = size;
		wall->color = color;
		wall->isStatic = true;
		wall->SetPhysicsObject(GetPhysicsEngine()->CreateObject(size, false));
		if (wall->physicsObject) {
			wall->physicsObject->SetPosition(pos);
		}
		this->scene->AddObject(wall);
	}

	Labyrinth(int width = 1000, int height = 1000, bool vr = false) : fe::EditableGame(fe::XRGameOptions(width, height, vr)) {

		SetClearColor(0.1f, 0.3f, 1);

		if (!useVulkan)
			LoadShaders("resources/shaders/VertexShader.glsl", "resources/shaders/FragmentShader.glsl");

		LoadModels();

		GetPhysicsEngine()->EnableGravity();

		accelerometers = fe::Accelerometer::EnumerateAll();
		accelReadings.resize(accelerometers.size(), glm::vec3(0.0f));
		for (size_t i = 0; i < accelerometers.size(); i++) {
			accelerometers[i].Start([this, i](const glm::vec3& accel) {
				accelReadings[i] = accel;
			});
		}
	}

	void OnPreSwap() override {}

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

		GenerateMaze();

		float totalW = MAZE_COLS * CELL_SIZE;
		float totalH = MAZE_ROWS * CELL_SIZE;

		// Ground plane
		float groundSize = std::max(totalW, totalH) + 2.0f;
		auto groundMesh = fe::Primitives::GenerateCube(
			{fe::PlaneDirection::Front, fe::PlaneDirection::Back, fe::PlaneDirection::Left,
			 fe::PlaneDirection::Right, fe::PlaneDirection::Bottom},
			fe::Primitives::defaultUVs, 1.0f);
		auto ground = std::make_shared<fe::Object<>>(groundMesh);
		ground->name = "Ground";
		ground->state.position = glm::vec3(0.0f, -0.5f, 0.0f);
		ground->state.scale = glm::vec3(groundSize, 1.0f, groundSize);
		ground->color = glm::vec3(0.0f, 0.0f, 0.0f);
		ground->isStatic = true;
		ground->SetPhysicsObject(GetPhysicsEngine()->CreateObject(glm::vec3(groundSize, 1.0f, groundSize), false));
		if (ground->physicsObject) {
			ground->physicsObject->SetPosition(ground->state.position);
		}
		this->scene->AddObject(ground);

		// Ball - start at cell (0,0) top-left
		auto sphereMesh = fe::Primitives::GenerateSphere(BALL_RADIUS, 32, 24);
		ballObject = std::make_shared<fe::Object<>>(sphereMesh);
		ballObject->name = "Ball";
		glm::vec3 startPos = CellToWorld(0, 0);
		startPos.y = BALL_RADIUS + 0.01f;
		ballObject->state.position = startPos;
		ballObject->SetPhysicsObject(GetPhysicsEngine()->CreateSphereObject(BALL_RADIUS, true));
		if (ballObject->physicsObject) {
			ballObject->physicsObject->SetPosition(ballObject->state.position);
		}
		this->scene->AddObject(ballObject);

		// Goal object at cell (MAZE_COLS-1, MAZE_ROWS-1) bottom-right
		float goalSize = 0.8f;
		auto goalMesh = fe::Primitives::GenerateSphere(goalSize, 16, 12);
		goalObject = std::make_shared<fe::Object<>>(goalMesh);
		goalObject->name = "Goal";
		glm::vec3 goalPos = CellToWorld(MAZE_COLS - 1, MAZE_ROWS - 1);
		goalPos.y = 0.8f;
		goalObject->state.position = goalPos;
		goalObject->color = glm::vec3(0.0f, 1.0f, 0.0f);
		this->scene->AddObject(goalObject);

		glm::vec3 wallColor(0.3f, 0.3f, 0.3f);

		// Maze walls - for each cell, emit its N and W walls (to avoid duplicates), plus bottom row S and right col E
		float halfTotalW = totalW * 0.5f;
		float halfTotalH = totalH * 0.5f;

		for (int row = 0; row < MAZE_ROWS; row++) {
			for (int col = 0; col < MAZE_COLS; col++) {
				float cx = col * CELL_SIZE - halfTotalW + CELL_SIZE * 0.5f;
				float cz = row * CELL_SIZE - halfTotalH + CELL_SIZE * 0.5f;
				float hy = WALL_HEIGHT * 0.5f;

				if (mazeGrid[row][col].wallN) {
					float wz = cz - CELL_SIZE * 0.5f;
					AddWall(glm::vec3(cx, hy, wz), glm::vec3(CELL_SIZE + WALL_THICK, WALL_HEIGHT, WALL_THICK), wallColor);
				}
				if (mazeGrid[row][col].wallW) {
					float wx = cx - CELL_SIZE * 0.5f;
					AddWall(glm::vec3(wx, hy, cz), glm::vec3(WALL_THICK, WALL_HEIGHT, CELL_SIZE + WALL_THICK), wallColor);
				}
				if (row == MAZE_ROWS - 1 && mazeGrid[row][col].wallS) {
					float wz = cz + CELL_SIZE * 0.5f;
					AddWall(glm::vec3(cx, hy, wz), glm::vec3(CELL_SIZE + WALL_THICK, WALL_HEIGHT, WALL_THICK), wallColor);
				}
				if (col == MAZE_COLS - 1 && mazeGrid[row][col].wallE) {
					float wx = cx + CELL_SIZE * 0.5f;
					AddWall(glm::vec3(wx, hy, cz), glm::vec3(WALL_THICK, WALL_HEIGHT, CELL_SIZE + WALL_THICK), wallColor);
				}
			}
		}

		// Player
		this->player = std::make_shared<fe::Character>();
		this->scene->AddObject(player);
		this->player->state.position = glm::vec3(0.0f, 3.0f, 0.0f);
		RebuildPlayerPhysicsBody();
		if (this->player->physicsObject) {
			this->player->physicsObject->SetPosition(this->player->state.position);
		}
	}

	bool freeCamera = false;

	void SyncCameraToPlayer() {
		if (!player || freeCamera) return;

		camera->SetPos(glm::vec3(0.0f, 25.0f, 0.0f));
		camera->pitch = -90.0f;
		camera->yaw = 90.0f;
		camera->UpdateDirection();
	}

	void ProcessInput() {
		SDL_Event event;
		fe::SDLWindow *window = (fe::SDLWindow*)this->window.get();
		while (window->PollSDLEvent(&event)) {
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

	void CheckWinCondition() {
		if (hasWon || !ballObject || !goalObject) return;
		float dist = glm::length(ballObject->state.position - goalObject->state.position);
		if (dist < BALL_RADIUS + 0.8f) {
			hasWon = true;
		}
	}

	void Run() {
		auto window = this->GetWindow<fe::SDLWindow>();
		window->Show();
		window->DisableVSync();

		glm::vec3 startPos = CellToWorld(0, 0);
		player->state.position = startPos + glm::vec3(0.0f, 2.0f, 0.0f);
		if (player->physicsObject) {
			player->physicsObject->SetPosition(player->state.position);
		}
		// camera->farDist = farPlane;
		camera->SetAspect(camera->aspect);
		SyncCameraToPlayer();

		while (!window->ShouldClose()) {

			ProcessInput();

			if (!accelerometers.empty() && selectedAccel < (int)accelReadings.size()) {
				auto& ar = accelReadings[selectedAccel];
				GetPhysicsEngine()->SetGravity(glm::vec3(ar.x * 12.0f, -9.81f, ar.z * 12.0f));
			} else {
				GetPhysicsEngine()->SetGravity(glm::vec3(0.0f, -9.81f, 0.0f));
			}

			if (!hasWon) {
				CheckWinCondition();
			}

			if (!freeCamera) {
				SyncCameraToPlayer();
			}

			if (freeCamera) {
				int freeCamSpeed = 10;
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
			}

			Update();
			Redraw();
		}

		Destroy();
	}

	void InitUI() override {}

	void DrawUI() override {
		if (!showDebugUI && !hasWon) return;
		BeginFrame();

		if (hasWon) {
			ImGui::SetNextWindowSize(ImVec2(300, 120), ImGuiCond_Always);
			ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
				ImGuiCond_Always, ImVec2(0.5f, 0.5f));
			ImGui::Begin("##won", nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
			ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "YOU WON!");
			if (ImGui::Button("Play Again")) {
				hasWon = false;
				glm::vec3 startPos = CellToWorld(0, 0);
				startPos.y = 1.0f;
				ballObject->state.position = startPos;
				if (ballObject->physicsObject) {
					ballObject->physicsObject->SetPosition(startPos);
				}
			}
			ImGui::End();
		}

		if (showDebugUI) {
			DrawDebugUI();

			if (!accelerometers.empty()) {
				ImGui::Begin("Accelerometers");

				for (size_t i = 0; i < accelerometers.size(); i++) {
					bool isSelected = ((int)i == selectedAccel);
					if (ImGui::Selectable((accelerometers[i].GetName() + "##" + std::to_string(i)).c_str(), isSelected)) {
						selectedAccel = (int)i;
					}
					ImGui::SameLine();
					ImGui::TextDisabled("(%.4f, %.4f, %.4f)", accelReadings[i].x, accelReadings[i].y, accelReadings[i].z);

					if (isSelected) {
						ImGui::Indent();
						ImGui::Text("X: %.4f", accelReadings[i].x);
						ImGui::Text("Y: %.4f", accelReadings[i].y);
						ImGui::Text("Z: %.4f", accelReadings[i].z);
						if (ImGui::Button(("Calibrate##" + std::to_string(i)).c_str())) {
							accelerometers[i].Calibrate();
						}
						ImGui::Unindent();
					}
					ImGui::Separator();
				}

				ImGui::End();
			}
		}

		EndFrame();
	}
};
