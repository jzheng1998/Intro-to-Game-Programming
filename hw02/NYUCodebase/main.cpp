#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#define GL_GLEXT_PROTOTYPES 1
#include <SDL_opengl.h>
#include <SDL_image.h>

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

class Entity {
public:
	void draw(ShaderProgram &p);
	void move(float elapsed);
	float x;
	float y;
	float rotation = 0.0f;
	int textureID;

	float width;
	float height;
	float velocity = 1.0f;
	float direction_x = 0.0f;
	float direction_y = 0.0f;
};

void Entity::draw(ShaderProgram &program) {
	float vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5 };
	glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program.positionAttribute);

	glm::mat4 modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, glm::vec3(x, y, 0.0f));
	modelMatrix = glm::scale(modelMatrix, glm::vec3(width, height, 1.0f));
	program.SetModelMatrix(modelMatrix);

	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(program.positionAttribute);
}

void Entity::move(float elapsed) {
	this->x = this->x + (this->direction_x * elapsed * this->velocity);
	this->y = this->y + (this->direction_y * elapsed * this->velocity);
}

bool collision(Entity &a, Entity &b, float space) {
	float distance_between_x = abs(a.x - b.x) - ((a.width + b.width) / 2) - space;
	float distance_between_y = abs(a.y - b.y) - ((a.height + b.height) / 2) - space;
	if (distance_between_x < 0.0f && distance_between_y < 0.0f) {
		return true;
	}
	return false;
}

bool outOfBound(Entity &ball) {
	if (abs(ball.x) > 1.78) {
		return true;
	}
	return false;
}

SDL_Window* displayWindow;
SDL_GLContext context;
ShaderProgram program;
const Uint8 *keys;
bool done = false;
glm::mat4 projectionMatrix, viewMatrix;
Entity leftPaddle, rightPaddle, ball, topBar, bottomBar;
float lastFrameTicks = 0.0f;

void Setup() {
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Pong Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 360, SDL_WINDOW_OPENGL);
	context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);

#ifdef _WINDOWS
	glewInit();
#endif

	glViewport(0, 0, 640, 360);
	program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");

	leftPaddle.x = -1.72f;
	leftPaddle.y = 0.0f;
	leftPaddle.width = 0.04f;
	leftPaddle.height = 0.33f;

	rightPaddle.x = 1.72f;
	rightPaddle.y = 0.0f;
	rightPaddle.width = 0.04f;
	rightPaddle.height = 0.33f;

	ball.x = 0.0f;
	ball.y = 0.0f;
	ball.width = 0.05f;
	ball.height = 0.05f;
	ball.direction_x = 0.9f;
	ball.direction_y = 0.5f;

	topBar.x = 0.0f;
	topBar.y = 0.95f;
	topBar.width = 3.5f;
	topBar.height = 0.04f;

	bottomBar.x = 0.0f;
	bottomBar.y = -0.95f;
	bottomBar.width = 3.5f;
	bottomBar.height = 0.04f;

	projectionMatrix = glm::mat4(1.0f);
	projectionMatrix = glm::ortho(-1.777f, 1.777f, -1.0f, 1.0f, -1.0f, 1.0f);
	viewMatrix = glm::mat4(1.0f);

	program.SetProjectionMatrix(projectionMatrix);
	program.SetViewMatrix(viewMatrix);

	glUseProgram(program.programID);

	keys = SDL_GetKeyboardState(NULL);
}

void ProcessEvents() {
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
			done = true;
		}
	}

	if (keys[SDL_SCANCODE_UP]) {
		rightPaddle.direction_y = 1.0f;
	} else if (keys[SDL_SCANCODE_DOWN]) {
		rightPaddle.direction_y = -1.0f;
	} else {
		rightPaddle.direction_y = 0.0f;
	}

	if (keys[SDL_SCANCODE_W]) {
		leftPaddle.direction_y = 1.0f;
	} else if (keys[SDL_SCANCODE_S]) {
		leftPaddle.direction_y = -1.0f;
	} else {
		leftPaddle.direction_y = 0.0f;
	}
}

void Update() {
	float ticks = (float)SDL_GetTicks() / 1000.0f;
	float elapsed = ticks - lastFrameTicks;
	lastFrameTicks = ticks;

	leftPaddle.move(elapsed);
	rightPaddle.move(elapsed);
	ball.move(elapsed);

	// Check collision
	if (collision(leftPaddle, topBar, 0.02f) || collision(leftPaddle, bottomBar, 0.02f)) {
		leftPaddle.move(-1.0f * elapsed);
	}

	if (collision(rightPaddle, topBar, 0.02f) || collision(rightPaddle, bottomBar, 0.02f)) {
		rightPaddle.move(-1.0f * elapsed);
	}

	if (collision(ball, leftPaddle, 0.0f) || collision(ball, rightPaddle, 0.0f)) {
		ball.direction_x = -ball.direction_x;
	}

	if (outOfBound(ball)) {
		ball.direction_x = -ball.direction_x;
	}

	// Check winning conditions
	// Check if blocks reach border
	if (collision(ball, topBar, 0.0f) || collision(ball, bottomBar, 0.0f)) {
		ball.direction_y = 0 - ball.direction_y;
	}
}

void Render() {
	glClear(GL_COLOR_BUFFER_BIT);

	leftPaddle.draw(program);
	rightPaddle.draw(program);
	ball.draw(program);
	topBar.draw(program);
	bottomBar.draw(program);

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

//GLuint LoadTexture(const char *filePath) {
//	int w, h, comp;
//	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha);
//
//	if (image == NULL) {
//		std::cout << "Unable to load image. Make sure the path is correct\n";
//		assert(false);
//	}
//
//	GLuint retTexture;
//	glGenTextures(1, &retTexture);
//	glBindTexture(GL_TEXTURE_2D, retTexture);
//	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
//
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
//	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
//
//	stbi_image_free(image);
//	return retTexture;
//}
