#pragma once
#include "Math/MathTypes.h"

namespace SG
{

	// in seagull engine, we use right-hand side z-up coordinate
	class ICameraController
	{
	public:
		virtual ~ICameraController() = default;

		virtual Vec3 GetPosition() const = 0;
		virtual void SetPosition(const Vec3&) = 0;
		
		virtual Vec3 GetUpVector() const = 0;
		virtual Vec3 GetRightVector() const = 0;
		virtual Vec3 GetLookVector() const = 0;
		
		virtual void OnUpdate(float deltaTime) = 0;
		
		virtual void OnRotate(const Vec2& screenMouseMove) = 0;
		virtual void OnMove(const Vec3& screenMouseMove) = 0;
		virtual void OnZoom(const Vec2& scrollValue) = 0;

		virtual void SetCameraLens(float fovY, float aspect, float zn, float zf) = 0;

		virtual Matrix4 GetViewMatrix() const = 0;
		virtual Matrix4 GetProjMatrix() const = 0;
	};

	/// <summary>
	/// simple perspective camera
	/// </summary>
	/// <param name="startPos"> -- the position of the camera at the beginning </param>
	/// <param name="startLookat"> -- the look at direction of the camera at the beginning </param>
	/// <returns> return a ICameraController for user to use </returns>
	ICameraController* create_perspective_camera(const Vec3& startPos, const Vec3& startLookat);

	void destroy_camera(ICameraController* pCamera);

}