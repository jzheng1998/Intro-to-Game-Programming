#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengl.h>
#include <SDL_image.h>
#include <chrono>
#include <thread>
#include <vector>

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "ShaderProgram.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"

SDL_Window* displayWindow;
SDL_GLContext context;
ShaderProgram program;
ShaderProgram texturedProgram;
const Uint8 *keys;
glm::mat4 projectionMatrix, viewMatrix;

enum GameMode { MAIN_MENU, GAME_LEVEL, GAME_OVER };
bool done = false;
float lastFrameTicks = 0.0f;
float timer = 0.0f;

GLuint LoadTexture(const char *filePath) {
	int w, h, comp;
	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);

	if (image == NULL) {
		std::cout << "Unable to load image. Make sure the path is correct\n";
		assert(false);
	}

	GLuint retTexture;
	glGenTextures(1, &retTexture);
	glBindTexture(GL_TEXTURE_2D, retTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(image);
	return retTexture;
}

class SheetSprite {
public:
	SheetSprite() {};
	SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size);
	
	void Draw(ShaderProgram &program);

	float size;
	unsigned int textureID;
	float u;
	float v;
	float width;
	float height;
};

SheetSprite::SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size) {
	this->textureID = textureID;
	this->u = u;
	this->v = v;
	this->width = width;
	this->height = height;
	this->size = size;
}

void SheetSprite::Draw(ShaderProgram &program) {
	glBindTexture(GL_TEXTURE_2D, textureID);
	GLfloat texCoords[] = {
		u, v + height,
		u + width, v,
		u, v,
		u + width, v,
		u, v + height,
		u + width, v + height
	};
	float aspect = width / height;
	float vertices[] = {
		-0.5f * size * aspect, -0.5f * size,
		 0.5f * size * aspect, 0.5f * size,
		 -0.5f * size * aspect, 0.5f * size,
		 0.5f * size * aspect, 0.5f * size,
		 -0.5f * size * aspect, -0.5f * size ,
		 0.5f * size * aspect, -0.5f * size };

	glUseProgram(program.programID);

	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program.positionAttribute);

	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
	glEnableVertexAttribArray(program.texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(program.positionAttribute);
	glDisableVertexAttribArray(program.texCoordAttribute);
}

class Entity {
public:

	void Update(float elapsed);
	void Render(ShaderProgram &program);
	bool CollidesWith(Entity &entity);

	glm::vec3 position;
	glm::vec3 velocity;
	glm::vec3 size = glm::vec3(1.0f, 1.0f, 1.0f);

	float rotation;

	SheetSprite sprite;
};

void Entity::Update(float elapsed) {
	this->position.x += this->velocity.x * elapsed;
	this->position.y += this->velocity.y * elapsed;
}

void Entity::Render(ShaderProgram &program) {
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, position);
	modelMatrix = glm::scale(modelMatrix, size);
	program.SetModelMatrix(modelMatrix);
	sprite.Draw(program);
}

bool Entity::CollidesWith(Entity &entity) {
	if (this->position.x + this->sprite.width < entity.position.x - entity.sprite.width) return false;
	if (this->position.x - this->sprite.width > entity.position.x + entity.sprite.width) return false;
	if (this->position.y + this->sprite.height < entity.position.y - entity.sprite.height) return false;
	if (this->position.y - this->sprite.height > entity.position.y + entity.sprite.height) return false;
	return true;
}

struct MainMenuState {
	void DrawText(ShaderProgram &program, int fontTexture, std::string text, float size, float spacing);

	void Setup();
	void ProcessEvents();
	void Render();
};

struct GameState {
	void Setup();
	void ProcessEvents();
	void Update(float elapsed);
	void Render();
};

GLuint fontSheet;
GLuint textureSheet;
SheetSprite enemySprite, playerSprite, bulletSprite;
GameMode mode;
GameState gameState;
MainMenuState mainMenuState;

void MainMenuState::DrawText(ShaderProgram &program, int fontTexture, std::string text, float size, float spacing) {
	float character_size = 1.0 / 16.0f;
	std::vector<float> vertexData;
	std::vector<float> texCoordData;

	for (unsigned int i = 0; i < text.size(); i++) {
		int spriteIndex = (int) text[i];
		float texture_x = (float)(spriteIndex % 16) / 16.0f;
		float texture_y = (float)(spriteIndex / 16) / 16.0f;

		vertexData.insert(vertexData.end(), {
			((size + spacing) * i) + (-0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
		});
		texCoordData.insert(texCoordData.end(), {
			texture_x, texture_y,
			texture_x, texture_y + character_size,
			texture_x + character_size, texture_y,
			texture_x + character_size, texture_y + character_size,
			texture_x + character_size, texture_y,
			texture_x, texture_y + character_size,
		});
	}
	glBindTexture(GL_TEXTURE_2D, fontTexture);

	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program.positionAttribute);

	glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program.texCoordAttribute);

	glDrawArrays(GL_TRIANGLES, 0, 6 * (int) text.size());

	glDisableVertexAttribArray(program.positionAttribute);
	glDisableVertexAttribArray(program.texCoordAttribute);
}

void MainMenuState::Setup() {

}

void GameState::Setup() {

}

void Setup() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Final Project", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 640, SDL_WINDOW_OPENGL);
	context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
	glewInit();
#endif

	glViewport(0, 0, 640, 640);
	program.Load("vertex.glsl", "fragment.glsl");
	texturedProgram.Load("vertex_textured.glsl", "fragment_textured.glsl");

	fontSheet = LoadTexture("assets/font.png");
	textureSheet = LoadTexture("assets/SpaceShooter/Spritesheet/sheet.png");
	mode = MAIN_MENU;

	projectionMatrix = glm::mat4(1.0f);
	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);
	viewMatrix = glm::mat4(1.0f);
	
	program.SetProjectionMatrix(projectionMatrix);
	program.SetViewMatrix(viewMatrix);

	texturedProgram.SetProjectionMatrix(projectionMatrix);
	texturedProgram.SetViewMatrix(viewMatrix);

	glUseProgram(texturedProgram.programID);

	keys = SDL_GetKeyboardState(NULL);

	mainMenuState.Setup();
}

void MainMenuState::ProcessEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}
}

void GameState::ProcessEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}
}

void ProcessEvents() {
	switch (mode) {

	}
}

void Update() {
	float ticks = (float) SDL_GetTicks() / 1000.0f;
	float elapsed = ticks - lastFrameTicks;
	lastFrameTicks = ticks;

	switch (mode) {

	}
}

void Render() {
	glClear(GL_COLOR_BUFFER_BIT);
	switch (mode) {

	}
	SDL_GL_SwapWindow(displayWindow);
}

void Cleanup() {

}

int main(int argc, char *argv[]) {
	Setup();
	while (!done) {
		ProcessEvents();
		Update();
		Render();
	}
	Cleanup();
	SDL_Quit();
	return 0;
}
