#include "FirstPersonController.h"
#include "entry/entry.h"
#include <bx/math.h>
#include <SDL2/SDL_events.h>

void FirstPersonController::Update(float deltaTime, bool mouseLocked) {
	this->Camera.handleMouseInput(mouseLocked, deltaTime);

	auto keyboardState = SDL_GetKeyboardState(nullptr);
	//Speed Modifier
	running = keyboardState[SDL_SCANCODE_LSHIFT];
	if (running == true) {
		speed = RunSpeed;
	}
	else if (running && isMoving && isGrounded) {
		speed = WalkSpeed;
	}
	else {
		speed = WalkSpeed / AirModifier;
	}

	isGrounded = false;//TODO
	jump = keyboardState[SDL_SCANCODE_SPACE];
	bool up = keyboardState[SDL_SCANCODE_W];
	bool left = keyboardState[SDL_SCANCODE_A];
	bool right = keyboardState[SDL_SCANCODE_D];
	bool down = keyboardState[SDL_SCANCODE_S];
	bool crouch = keyboardState[SDL_SCANCODE_C];

	auto rightForce = this->Camera.GetRight() * (float)(right - left);
	auto upForce = this->Camera.GetUp() * (float)(jump - crouch);
	auto forwardForce = (this->Camera.GetForward() * (float)(up - down));
	auto moveForce = glm::normalize(rightForce + upForce + forwardForce);

	if (up || down || right || left || jump || crouch) {
		isMoving = true;
	}
	else {
		isMoving = false;
		moveForce = glm::vec3(0, 0, 0);
	}

	//Limit Speed
	if (glm::length(velocity) > speed) {
		velocity = normalize(velocity) * speed;
	}

	//
	// Movement
	//

	//Apply Friction
	velocity *= Friction;

	velocity += moveForce * speed;

	this->Camera.SetPosition(this->Camera.GetPosition() + glm::vec3(velocity.x, velocity.y, velocity.z));

	/*if (isGrounded && CanJump && jump || isUnderWater) {
		velocity += vec3(0,1,0) * JumpForce;
	}*/
}