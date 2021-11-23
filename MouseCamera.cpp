#include "MouseCamera.h"
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <bx/bx.h>
#include <sstream>
/*****************************************************************************
 * Copyright (c) 2018-2021 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *****************************************************************************/

glm::mat4 MouseCamera::getRotationMatrix() const
{
	return glm::eulerAngleZXY(_rotation.z, _rotation.x, _rotation.y);
}

glm::mat4 MouseCamera::GetViewMatrix() const
{
	return getRotationMatrix() * glm::translate(glm::mat4(1.0f), -_position);
}

glm::mat4 MouseCamera::GetViewProjectionMatrix() const
{
	return GetProjectionMatrix() * GetViewMatrix();
}

void MouseCamera::SetProjectionMatrixPerspective(float fov, float aspect, float nearclip, float farclip)
{
	_projectionMatrix = glm::perspective(glm::radians(fov), aspect, nearclip, farclip);
}

glm::vec3 MouseCamera::GetForward() const
{
	// Forward is +1 in openblack but is -1 in OpenGL
	glm::mat3 mRotation = glm::transpose(getRotationMatrix());
	return mRotation * glm::vec3(0, 0, 1);
}

glm::vec3 MouseCamera::GetRight() const
{
	glm::mat3 mRotation = glm::transpose(getRotationMatrix());
	return mRotation * glm::vec3(1, 0, 0);
}

glm::vec3 MouseCamera::GetUp() const
{
	glm::mat3 mRotation = glm::transpose(getRotationMatrix());
	return mRotation * glm::vec3(0, 1, 0);
}

void MouseCamera::DeprojectScreenToWorld(const glm::ivec2 screenPosition, const glm::ivec2 screenSize, glm::vec3& out_worldOrigin,
	glm::vec3& out_worldDirection)
{
	const float normalizedX = (float)screenPosition.x / (float)screenSize.x;
	const float normalizedY = (float)screenPosition.y / (float)screenSize.y;

	const float screenSpaceX = (normalizedX - 0.5f) * 2.0f;
	const float screenSpaceY = ((1.0f - normalizedY) - 0.5f) * 2.0f;

	// The start of the ray trace is defined to be at mousex,mousey,1 in
	// projection space (z=0 is near, z=1 is far - this gives us better
	// precision) To get the direction of the ray trace we need to use any z
	// between the near and the far plane, so let's use (mousex, mousey, 0.5)
	const glm::vec4 rayStartProjectionSpace = glm::vec4(screenSpaceX, screenSpaceY, 0.0f, 1.0f);
	const glm::vec4 rayEndProjectionSpace = glm::vec4(screenSpaceX, screenSpaceY, 0.5f, 1.0f);

	// Calculate our inverse view projection matrix
	glm::mat4 inverseViewProj = glm::inverse(GetViewProjectionMatrix());

	// Get our homogeneous coordinates for our start and end ray positions
	const glm::vec4 hgRayStartWorldSpace = inverseViewProj * rayStartProjectionSpace;
	const glm::vec4 hgRayEndWorldSpace = inverseViewProj * rayEndProjectionSpace;

	glm::vec3 rayStartWorldSpace(hgRayStartWorldSpace.x, hgRayStartWorldSpace.y, hgRayStartWorldSpace.z);
	glm::vec3 rayEndWorldSpace(hgRayEndWorldSpace.x, hgRayEndWorldSpace.y, hgRayEndWorldSpace.z);

	// divide vectors by W to undo any projection and get the 3-space coord
	if (hgRayStartWorldSpace.w != 0.0f)
		rayStartWorldSpace /= hgRayStartWorldSpace.w;

	if (hgRayEndWorldSpace.w != 0.0f)
		rayEndWorldSpace /= hgRayEndWorldSpace.w;

	const glm::vec3 rayDirWorldSpace = glm::normalize(rayEndWorldSpace - rayStartWorldSpace);

	// finally, store the results in the outputs
	out_worldOrigin = rayStartWorldSpace;
	out_worldDirection = rayDirWorldSpace;
}

bool MouseCamera::ProjectWorldToScreen(const glm::vec3 worldPosition, const glm::vec4 viewport, glm::vec3& out_screenPosition) const
{
	out_screenPosition = glm::project(worldPosition, GetViewMatrix(), GetProjectionMatrix(), viewport);
	if (out_screenPosition.x < viewport.x || out_screenPosition.y < viewport.y || out_screenPosition.x > viewport.z ||
		out_screenPosition.y > viewport.w)
	{
		return false;
	}
	if (out_screenPosition.z > 1.0f)
	{
		// Behind Camera
		return false;
	}

	if (out_screenPosition.z < 0.0f)
	{
		// Clipped
		return false;
	}

	return true;
}

void MouseCamera::handleMouseInput(entry::MouseState mouseState, bool mouseLocked)
{

	glm::vec3 rot = GetRotation();
	float relY = 0;
	float relX = 0;
	std::ostringstream d;
	d << mouseState.m_mx << " " << mouseState.m_my << std::endl;
	bx::debugOutput(d.str().c_str());
	if (mouseLocked)
	{
		if (mouseState.m_my != 1 && mouseState.m_my != -1)
			relY = mouseState.m_my;
		if (mouseState.m_mx != -1 && mouseState.m_mx != 1)
			relX = mouseState.m_mx;
	}
	else
	{
		relY = mouseState.m_my - _previousMouseState.m_my;
		relX = mouseState.m_mx - _previousMouseState.m_mx;
	}
	rot.x -= relY * Sensitivity * 0.1f;
	rot.y -= relX * Sensitivity * 0.1f;

	SetRotation(rot);
	_previousMouseState = mouseState;
}