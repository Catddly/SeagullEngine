#include "Interface/ICameraController.h"

#include "Interface/IInput.h"
#include <include/gtc/matrix_transform.hpp>

#include "Interface/IMemory.h"

namespace SG
{

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
		float mMouseXSensitity = 0.1f, mMouseYSensitity = 0.1f;

		float dx, dy, dz;
		float rx, ry;

		Matrix4 mViewMat = Matrix4(1.0f);
		Matrix4 mProjMat = Matrix4(1.0f);

		bool mViewDirty = true;
	};

	void PersPectiveCamera::OnUpdate(float deltaTime)
	{
		if (mViewDirty)
		{
			rx *= deltaTime * mMouseXSensitity;
			ry *= deltaTime * mMouseYSensitity;

			mLookAtVec = Vec4(mLookAtVec, 1.0f) * glm::rotate(Matrix4(1.0f), rx, mUpVec);
			mLookAtVec = Vec4(mLookAtVec, 1.0f) * glm::rotate(Matrix4(1.0f), ry, mRightVec);

			mCurrPos += dx * deltaTime * mCameraMoveSpeed * mLookAtVec;
			mCurrPos += dy * deltaTime * mCameraMoveSpeed * glm::cross(mLookAtVec, mUpVec);
			mCurrPos += dz * deltaTime * mCameraMoveSpeed * mUpVec;

			//mViewMat[0][0] = mRightVec.x;
			//mViewMat[1][0] = mRightVec.y;
			//mViewMat[2][0] = mRightVec.z;
			//mViewMat[3][0] = -glm::dot(mCurrPos, mRightVec);

			//mViewMat[0][1] = mUpVec.x;
			//mViewMat[1][1] = mUpVec.y;
			//mViewMat[2][1] = mUpVec.z;
			//mViewMat[3][1] = -glm::dot(mCurrPos, mLookAtVec);

			//mViewMat[0][2] = mLookAtVec.x;
			//mViewMat[1][2] = mLookAtVec.y;
			//mViewMat[2][2] = mLookAtVec.z;
			//mViewMat[3][2] = -glm::dot(mCurrPos, mUpVec);

			//mViewMat[0][3] = 0.0f;
			//mViewMat[1][3] = 0.0f;
			//mViewMat[2][3] = 0.0f;
			//mViewMat[3][3] = 1.0f;

			mViewMat = glm::lookAt(mCurrPos, mCurrPos + mLookAtVec, mUpVec);

			mViewDirty = false;
		}
	}

	void PersPectiveCamera::OnRotate(const Vec2& screenMouseMove)
	{
		rx = screenMouseMove.x;
		ry = screenMouseMove.y;

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