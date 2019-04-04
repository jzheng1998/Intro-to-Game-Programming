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

#define MAX_BULLETS 50
#define MAX_ENEMIES 21

SDL_Window* displayWindow;
SDL_GLContext context;
ShaderProgram program;
ShaderProgram texturedProgram;
const Uint8 *keys;
glm::mat4 projectionMatrix, viewMatrix;

enum GameMode { MAIN_MENU, GAME_LEVEL, GAME_OVER };
int bulletIndex = 0;
int enemiesLeft = MAX_ENEMIES;
bool done = false;
bool gameOver = false;
float lastFrameTicks = 0.0f;
float timer = 0.0f;
bool canShoot = true;

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

struct GameState {
	Entity player;
	Entity enemies[MAX_ENEMIES];
	Entity bullets[MAX_BULLETS];
};

GLuint fontSheet;
GLuint textureSheet;
SheetSprite enemySprite, playerSprite, bulletSprite;
GameMode mode;
GameState state;
Entity enemies[MAX_ENEMIES];
Entity bullets[MAX_BULLETS];
Entity player;

void DrawText(ShaderProgram &program, int fontTexture, std::string text, float size, float spacing) {
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

bool clickStart(double x, double y) {
	return true;
}

void shootBullet() {
	bullets[bulletIndex].position.x = player.position.x;
	bullets[bulletIndex].position.y = player.position.y;
	bullets[bulletIndex].velocity.y = 1.0f;
	bulletIndex++;
	if (bulletIndex > MAX_BULLETS - 1) {
		bulletIndex = 0;
	}
}

bool contactWithSide() {
	for (int i = 0; i < MAX_ENEMIES; i++) {
		Entity enemy = enemies[i];
		if (enemy.position.x + enemy.sprite.width > 1.77) {
			return true;
		}
		if (enemy.position.x - enemies->sprite.width < -1.77) {
			return true;
		}
	}
	return false;
}

void SetupMainMenu() {
	gameOver = false;
	DrawText(texturedProgram, fontSheet, "Space Invaders", 0.2f, 0.0f);
	DrawText(texturedProgram, fontSheet, "Start", 0.125f, 0.0f);
}

void SetupGameLevel() {
	enemySprite = SheetSprite(textureSheet, 423.0f / 1024.0f, 728.0f / 1024.0f, 93.0f / 1024.0f, 84.0f / 1024.0f, 0.2f);
	playerSprite = SheetSprite(textureSheet, 211.0f / 1024.0f, 941.0f / 1024.0f, 99.0f / 1024.0f, 75.0f / 1024.0f, 0.2f);
	bulletSprite = SheetSprite(textureSheet, 856.0f / 1024.0f, 421.0f / 1024.0f, 9.0f / 1024.0f, 54.0f / 1024.0f, 0.1f);

	player.sprite = playerSprite;
	player.position = glm::vec3(0.0f, -0.87f, 0.0f);
	player.velocity = glm::vec3(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < MAX_BULLETS; i++) {
		bullets[i].sprite = bulletSprite;
		bullets[i].position = glm::vec3(-2000.0f, 0.0f, 0.0f);
		bullets[i].velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	}

	int row = 3;
	int numberOfEnemiesEachRow = MAX_ENEMIES / row;
	for (int i = 0; i < row; i++) {
		float position_x = -1.5f;
		float position_y = 0.8 - 0.25 * i;
		for (int j = i * numberOfEnemiesEachRow; j < (i + 1)*numberOfEnemiesEachRow; j++) {
			enemies[j].sprite = enemySprite;
			enemies[j].position = glm::vec3(position_x, position_y, 0.0f);
			enemies[j].velocity = glm::vec3(0.25f, 0.0f, 0.0f);
			position_x += 0.4f;
		}
	}
}

void Setup() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Space Invaders", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 640, SDL_WINDOW_OPENGL);
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
	SetupMainMenu();

	projectionMatrix = glm::mat4(1.0f);
	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);
	viewMatrix = glm::mat4(1.0f);
	
	program.SetProjectionMatrix(projectionMatrix);
	program.SetViewMatrix(viewMatrix);

	texturedProgram.SetProjectionMatrix(projectionMatrix);
	texturedProgram.SetViewMatrix(viewMatrix);

	glUseProgram(texturedProgram.programID);

	keys = SDL_GetKeyboardState(NULL);
}

void ProcessMainMenuInput() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		} else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == 1) {
			if (clickStart(event.button.x, event.button.y)) {
				mode = GAME_LEVEL;
				SetupGameLevel();
			}
		}
	}
}

void ProcessGameLevelInput() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}
	if (keys[SDL_SCANCODE_LEFT]) {
		player.velocity.x = -1.0f;
	}
	else if (keys[SDL_SCANCODE_RIGHT]) {
		player.velocity.x = 1.0f;
	}
	else {
		player.velocity.x = 0.0f;
	}

	if (keys[SDL_SCANCODE_SPACE] && canShoot) {
		canShoot = false;
		shootBullet();
	}
}

void ProcessEvents() {
	switch (mode) {
	case MAIN_MENU:
		ProcessMainMenuInput();
		break;
	case GAME_LEVEL:
		ProcessGameLevelInput();
		break;
	}
}

void UpdateMainMenu(float elapsed) {
}

void UpdateGameLevel(float elapsed) {
	if (enemiesLeft == 0) {
		gameOver = true;
	}
	if (gameOver) {
		mode = MAIN_MENU;
		SetupMainMenu();
	}

	if (!canShoot) {
		timer += elapsed;
	}
	if (timer > 1.0f && !canShoot) {
		canShoot = true;
		timer = 0.0f;
	}

	player.Update(elapsed);
	for (int i = 0; i < MAX_BULLETS; i++) {
		bullets[i].Update(elapsed);
		for (int j = 0; j < MAX_ENEMIES; j++) {
			if (bullets[i].CollidesWith(enemies[j])) {
				enemies[j].position = glm::vec3(0.0f, -500.0f, 0.0f);
				enemies[j].velocity = glm::vec3(0.0f, 0.0f, 0.0f);
				bullets[i].position = glm::vec3(-2000.0f, 0.0f, 0.0f);
				bullets[i].velocity = glm::vec3(0.0f, 0.0f, 0.0f);
				enemiesLeft--;
			}
		}
	}

	for (int i = 0; i < MAX_ENEMIES; i++) {
		enemies[i].Update(elapsed);
		if (enemies[i].CollidesWith(player)) {
			gameOver = true;
			enemies[i].position = glm::vec3(0.0f, -500.0f, 0.0f);
			enemies[i].velocity = glm::vec3(0.0f, 0.0f, 0.0f);
			player.position = glm::vec3(0.0f, -500.0f, 0.0f);
			player.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
		}
	}

	if (contactWithSide()) {
		for (int i = 0; i < MAX_ENEMIES; i++) {
			enemies[i].position.y -= 0.12f;
			enemies[i].position.x -= enemies[i].velocity.x * elapsed;
			enemies[i].velocity.x = -enemies[i].velocity.x;
			enemies[i].Update(elapsed);
		}
	}
}

void Update() {
	float ticks = (float) SDL_GetTicks() / 1000.0f;
	float elapsed = ticks - lastFrameTicks;
	lastFrameTicks = ticks;

	switch (mode) {
	case MAIN_MENU:
		UpdateMainMenu(elapsed);
		break;
	case GAME_LEVEL:
		UpdateGameLevel(elapsed);
		break;
	}
}

void RenderMainMenu() {
	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-1.3f, 0.3f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	DrawText(texturedProgram, fontSheet, "Space Invaders", 0.2f, 0.0f);

	modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(-0.3f, -0.3f, 0.0f));
	texturedProgram.SetModelMatrix(modelMatrix);
	DrawText(texturedProgram, fontSheet, "Start", 0.125f, 0.0f);
}

void RenderGameLevel() {
	player.Render(texturedProgram);
	for (int i = 0; i < MAX_BULLETS; i++) {
		bullets[i].Render(texturedProgram);
	}
	for (int i = 0; i < MAX_ENEMIES; i++) {
		enemies[i].Render(texturedProgram);
	}
}

void Render() {
	glClear(GL_COLOR_BUFFER_BIT);
	switch (mode) {
	case MAIN_MENU:
		RenderMainMenu();
		break;
	case GAME_LEVEL:
		RenderGameLevel();
		break;
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
