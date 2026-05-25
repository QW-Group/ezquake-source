// Logic that modifies the usercmd_t's cmd->forwardmove and cmd->sidemove to make air control easy.

#include "quakedef.h"

cvar_t cl_easyaircontrol = { "cl_easyaircontrol", "0" };

// Function to rotate a 2D vector (x, y) by an angle in degrees
void rotateVector(float* x, float* y, float angle)
{
	float rad = angle * (M_PI / 180);  // Convert angle to radians
	float x_new = *x * cosf(rad) - *y * sinf(rad);
	float y_new = *x * sinf(rad) + *y * cosf(rad);
	*x = x_new;
	*y = y_new;
}

void CL_EasyAirControl(usercmd_t* cmd)
{
	vec3_t wishdir;
	float wishdiff, rotateAmount;
	float velocityX = cl.simvel[0];
	float velocityY = cl.simvel[1];
	float playerYaw = cl.simangles[1];

	// Normalize playerYaw to be between -180 and 180 degrees
	//playerYaw = fmodf(playerYaw + 180.0f, 360.0f) - 180.0f;
	while (playerYaw > 180) playerYaw -= 360;
	while (playerYaw < -180) playerYaw += 360;

	wishdir[0] = cmd->forwardmove;
	wishdir[1] = -1 * cmd->sidemove; // sidemove points right, but positive velocityY points left.

	rotateVector(&wishdir[0], &wishdir[1], playerYaw);

	float velocityMagnitude = sqrt(velocityX * velocityX + velocityY * velocityY);
	if (velocityMagnitude < 30.0f || (fabsf((wishdir)[0]) < 0.1f && fabsf((wishdir)[1]) < 0.1f))
		return;

	// Calculate wishdiff
	float wishdirYaw = atan2f((wishdir)[1], (wishdir)[0]) * (180.0f / M_PI);
	float velocityYaw = atan2f(velocityY, velocityX) * (180.0f / M_PI);
	wishdiff = wishdirYaw - velocityYaw;

	// Normalize wishdiff to be between -180 and 180 degrees
	//wishdiff = fmodf(wishdiff + 180.0f, 360.0f) - 180.0f;
	while (wishdiff > 180) wishdiff -= 360;
	while (wishdiff < -180) wishdiff += 360;

	// Give a small deadzone
	if (fabs(wishdiff) < 2)
		return;

	// Check if the absolute value of WishDiff is less than the user config cl_easyaircontrol.value
	if (fabs(wishdiff) < cl_easyaircontrol.value)
		rotateAmount = 90.0f; // if so, we're giving optimal speed gain in the desired direction
	else
	{
		// otherwise, we're overturning (relative to how far we are past the user config value)
		float ratio = (fabs(wishdiff) - cl_easyaircontrol.value) / (180.0f - cl_easyaircontrol.value);
		rotateAmount = 90.0f + (90.0f * ratio);
	}

	// set the direction accordingly
	if (wishdiff < 0.0f)
		rotateAmount *= -1.0f;

	// 
	// Rotate velocity by rotateAmount, unrotate per player yaw, then normalize it
	rotateVector(&velocityX, &velocityY, rotateAmount);
	rotateVector(&velocityX, &velocityY, -1.0f * playerYaw);

	float magnitude = sqrtf(velocityX * velocityX + velocityY * velocityY);
	if (magnitude > 0)
	{
		velocityX = (velocityX / magnitude) * 400.0f;
		velocityY = (velocityY / magnitude) * 400.0f;
	}

	// Set the new cmd->forwardmove and cmd->sidemove to that vector
	cmd->forwardmove = (short)velocityX;
	cmd->sidemove = -1 * (short)velocityY; // sidemove points right, but positive velocityY points left.
}