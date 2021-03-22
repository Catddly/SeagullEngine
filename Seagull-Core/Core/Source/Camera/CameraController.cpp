#include "Interface/ICameraController.h"

#include "Interface/IInput.h"
#include "Interface/ILog.h"
#include <include/gtc/matrix_transform.hpp>

#include "Interface/IMemory.h"

namespace SG
{

	static void clamp(float& value, float min, float max)
	{
		if (value <= min)
			value = min;
		if (value >= max)
			value = max;
	}

#pragma region (Perspective)

	class PersPectiveCamera : public ICameraController
	{
	public:
		PersPectiveCamera(const Vec3& startPos, const Vec3& startLookAt)
			:mStartPos(startPos), mCurrPos(startPos), mStartLookAt(startLookAt),
			mUpVec({ 0, 0, 1 }), mLookAtVec(startLookAt), mRightVec({ 1, 0, 0 }), mNearPlane(0.001f),
			mFarPlane(10000.0f), mFovY(glm::radians(45.0f))
		{

		}

		virtual Vec3 GetPosition() const override { return mCurrPos; }
		virtual void SetPosition(const Vec3& vec) override { mCurrPos = vec; }

		virtual Vec3 GetUpVector() const override { return mUpVec; }
		virtual Vec3 GetRightVector() const override { return mRightVec; }
		virtual Vec3 GetLookVector() const override { return mLookAtVec; }

		virtual void OnUpdate(float deltaTime) override;

		virtual void OnRotate(const Vec2 & screenMouseMove) override;
		virtual void OnMove(const Vec3& screenMouseMove) override;
		virtual void OnZoom(const Vec2 & scrollValue) override;

		virtual void SetCameraLens(float fovY, float aspect, float zn, float zf) override;

		virtual Matrix4 GetViewMatrix() const override { return mViewMat; }
		virtual Matrix4 GetProjMatrix() const override { return mProjMat; }
	private:
		Vec3 mStartPos;
		Vec3 mStartLookAt;

		Vec3 mCurrPos;
		Vec3 mUpVec;
		Vec3 mLookAtVec;
		Vec3 mRightVec;

		float mNearPlane;
		float mFarPlane;
		float mAspect = 0.0f;
		float mFovY = 0.0f;

		float mCameraMoveSpeed = 4.0f;
		float mMouseXSensitity = 180.0f, mMouseYSensitity = 170.0f;

		float dx, dy, dz;
		float pitch = 0.0f, yaw = 0.0f;
		float drx, dry;

		Matrix4 mViewMat = Matrix4(1.0f);
		Matrix4 mProjMat = Matrix4(1.0f);

		bool mPosDirty = true;
		bool mViewDirty = true;
	};

	void PersPectiveCamera::OnUpdate(float deltaTime)
	{
		if (mViewDirty)
		{
			pitch -= drx * mMouseXSensitity * deltaTime;
			yaw -= dry * mMouseYSensitity * deltaTime;

			clamp(yaw, -89.5f, 89.5f);

			mLookAtVec.x = glm::cos(glm::radians(pitch));
			mLookAtVec.y = glm::sin(glm::radians(pitch)) * glm::cos(glm::radians(yaw));
			mLookAtVec.z = glm::sin(glm::radians(yaw));

			glm::normalize(mLookAtVec);

			mCurrPos += dx * deltaTime * mCameraMoveSpeed * mLookAtVec;
			mCurrPos += dy * deltaTime * mCameraMoveSpeed * glm::cross(mLookAtVec, mUpVec);
			mCurrPos += dz * deltaTime * mCameraMoveSpeed * mUpVec;

			mViewDirty = false;
			drx = 0.0f;
			dry = 0.0f;
			dx = 0.0f;
			dy = 0.0f;
			dz = 0.0f;

			mViewMat = glm::lookAt(mCurrPos, mCurrPos + mLookAtVec, mUpVec);
		}
	}

	void PersPectiveCamera::OnRotate(const Vec2& screenMouseMove)
	{
		drx = screenMouseMove.x;
		dry = screenMouseMove.y;

		mViewDirty = true;
	}

	void PersPectiveCamera::OnMove(const Vec3& screenMouseMove)
	{
		dx = screenMouseMove.x;
		dy = screenMouseMove.y;
		dz = screenMouseMove.z;

		mViewDirty = true;
	}

	void PersPectiveCamera::OnZoom(const Vec2& scrollValue)
	{

	}

	void PersPectiveCamera::SetCameraLens(float fovY, float aspect, float zn, float zf)
	{
		if (fovY != mFovY || aspect != mAspect || zn != mNearPlane || zf != mFarPlane)
		{
			mFovY = fovY;
			mAspect = aspect;
			mNearPlane = zn;
			mFarPlane = zf;

			mProjMat = glm::perspective(mFovY, mAspect, mNearPlane, mFarPlane);
		}
	}

#pragma endregion (Perspective)

	ICameraController* create_perspective_camera(const Vec3& startPos, const Vec3& startLookat)
	{
		PersPectiveCamera* cc = sg_placement_new<PersPectiveCamera>(sg_calloc(1, sizeof(PersPectiveCamera)), startPos, startLookat);
		return cc;
	}

	void destroy_camera(ICameraController* pCamera)
	{
		ASSERT(pCamera);
		
		pCamera->~ICameraController();
		sg_free(pCamera);
	}

}