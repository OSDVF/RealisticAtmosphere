#pragma once
#include "MouseCamera.h"
#include "glm/glm.hpp"

class FirstPersonController
{
public:
	MouseCamera Camera;
	bool CanJump = true;
	float WalkSpeed = 1.4;
	float RunSpeed = 10;
	float AirModifier = 1.5f;
	float Friction = 0.86f;
	float JumpForce = 40;

	bool jump = false;
	bool running = false;
	// Currently choosen speed
	float speed = 0;
	// Current physical velocity of the controller
	glm::vec3 velocity;
	bool isMoving = true;
	bool isJumping = false;

	void Update(float deltaTime, bool mouseLocked, entry::MouseState mouseState);
};

