#include "FirstPersonController.h"
#include "entry/entry.h"
#include <bx/math.h>

void FirstPersonController::Update(char keyboard, uint8_t modifiers, entry::MouseState mouseState, bool mouseLocked) {
	this->Camera.handleMouseInput(mouseState, mouseLocked);
	//Speed Modifier
	if (running == true && isMoving && isGrounded) {
		speed = RunSpeed;
	}
	else if (running && isMoving && isGrounded) {
		speed = WalkSpeed;
	}
	else {
		speed = WalkSpeed / AirModifier;
	}

	isGrounded = false;//TODO
	running = modifiers & entry::Modifier::LeftShift;
	jump = keyboard == ' ';
	bool up = keyboard == 'w';
	bool left = keyboard == 'a';
	bool right = keyboard == 'd';
	bool down = keyboard == 's';
	bool crouch = keyboard == 'c';

	auto rightForce = this->Camera.GetRight() * (float)(right - left);
	auto upForce = this->Camera.GetUp() * (float)(jump - crouch);
	auto downForce = (this->Camera.GetForward() * (float)(up - down));
	auto moveForce = glm::normalize(rightForce + upForce + downForce);

	if (up || down || right || left || jump) {
		isMoving = true;
	}
	else {
		isMoving = false;
		moveForce = glm::vec3(0, 0, 0);
	}

	//Limit Speed
	if (velocity.length() > speed) {
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