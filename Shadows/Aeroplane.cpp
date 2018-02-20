//*********************************************************************************************
// File:			Aeroplane.cpp
// Description:		A very simple class to represent an aeroplane as one object with all the 
//					hierarchical components stored internally within the class.
// Module:			Real-Time 3D Techniques for Games
// Created:			Jake - 2010-2011
// Notes:			
//*********************************************************************************************

#include "Aeroplane.h"

AeroplaneMeshes *AeroplaneMeshes::Load()
{
	AeroplaneMeshes *pMeshes = new AeroplaneMeshes;

	pMeshes->pPlaneMesh = CommonMesh::LoadFromXFile(Application::s_pApp, "Resources/Plane/plane.x");
	pMeshes->pPropMesh = CommonMesh::LoadFromXFile(Application::s_pApp, "Resources/Plane/prop.x");
	pMeshes->pTurretMesh = CommonMesh::LoadFromXFile(Application::s_pApp, "Resources/Plane/turret.x");
	pMeshes->pGunMesh = CommonMesh::LoadFromXFile(Application::s_pApp, "Resources/Plane/gun.x");

	if (!pMeshes->pPlaneMesh || !pMeshes->pPropMesh || !pMeshes->pTurretMesh || !pMeshes->pGunMesh)
	{
		delete pMeshes;
		return NULL;
	}

	return pMeshes;
}

AeroplaneMeshes::AeroplaneMeshes() :
pPlaneMesh(NULL),
pPropMesh(NULL),
pTurretMesh(NULL),
pGunMesh(NULL)
{
}

AeroplaneMeshes::~AeroplaneMeshes()
{
	delete this->pPlaneMesh;
	delete this->pPropMesh;
	delete this->pTurretMesh;
	delete this->pGunMesh;
}

Aeroplane::Aeroplane( float fX, float fY, float fZ, float fRotY )
{
	m_mWorldMatrix = XMMatrixIdentity();
	m_mPropWorldMatrix = XMMatrixIdentity();
	m_mTurretWorldMatrix = XMMatrixIdentity();
	m_mGunWorldMatrix = XMMatrixIdentity();
	m_mCamWorldMatrix = XMMatrixIdentity();

	m_v4Rot = XMFLOAT4(0.0f, fRotY, 0.0f, 0.0f);
	m_v4Pos = XMFLOAT4(fX, fY, fZ, 0.0f);

	m_v4PropOff = XMFLOAT4(0.0f, 0.0f, 1.9f, 0.0f);
	m_v4PropRot = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_v4TurretOff = XMFLOAT4(0.0f, 1.05f, -1.3f, 0.0f);
	m_v4TurretRot = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_v4GunOff = XMFLOAT4(0.0f, 0.5f, 0.0f, 0.0f);
	m_v4GunRot = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_v4CamOff = XMFLOAT4(0.0f, 4.5f, -15.0f, 0.0f);
	m_v4CamRot = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);

	m_vCamWorldPos = XMVectorZero();
	m_vForwardVector = XMVectorZero();

	m_fSpeed = 0.0f;

	m_bGunCam = false;
}

Aeroplane::~Aeroplane( void )
{
}

void Aeroplane::SetWorldPosition( float fX, float fY, float fZ  )
{
	m_v4Pos = XMFLOAT4(fX, fY, fZ, 0.0f);
	UpdateMatrices();
}


void Aeroplane::UpdateMatrices( void )
{
	XMMATRIX mRotX, mRotY, mRotZ, mTrans;
	XMMATRIX mPlaneCameraRot, mForwardMatrix;
	
	// Calculate m_mWorldMatrix for plane based on Euler rotation angles and position data.
	m_mWorldMatrix = XMMatrixIdentity();
	mRotX = XMMatrixRotationX(XMConvertToRadians(m_v4Rot.x));
	mRotY = XMMatrixRotationY(XMConvertToRadians(m_v4Rot.y));
	mRotZ = XMMatrixRotationZ(XMConvertToRadians(m_v4Rot.z));
	m_mWorldTrans = XMMatrixTranslation(m_v4Pos.x, m_v4Pos.y, m_v4Pos.z);

	m_mWorldMatrix = mRotZ * mRotX * mRotY * m_mWorldTrans;

	// [Week 2] Also calculate a matrix which ignores rotations in Z and X for the camera to parent to
	mPlaneCameraRot = mRotY * m_mWorldTrans;

	// [Week 2] Get the forward vector out of the world matrix
	m_vForwardVector = m_mWorldMatrix.r[2];

	// Calculate m_mPropWorldMatrix for propellor based on Euler rotation angles and position data.
	// Parent the propellor to the plane
	m_mPropWorldMatrix = XMMatrixIdentity();
	mRotX = XMMatrixRotationX(XMConvertToRadians(m_v4PropRot.x));
	mRotY = XMMatrixRotationY(XMConvertToRadians(m_v4PropRot.y));
	mRotZ = XMMatrixRotationZ(XMConvertToRadians(m_v4PropRot.z));
	mTrans = XMMatrixTranslation(m_v4PropOff.x, m_v4PropOff.y, m_v4PropOff.z);

	m_mPropWorldMatrix =  mRotX * mRotY * mRotZ * mTrans * m_mWorldMatrix;

	// Calculate m_mTurretWorldMatrix for propellor based on Euler rotation angles and position data.
	// Parent the turret to the plane
	m_mTurretWorldMatrix = XMMatrixIdentity();
	mRotX = XMMatrixRotationX(XMConvertToRadians(m_v4TurretRot.x));
	mRotY = XMMatrixRotationY(XMConvertToRadians(m_v4TurretRot.y));
	mRotZ = XMMatrixRotationZ(XMConvertToRadians(m_v4TurretRot.z));
	mTrans = XMMatrixTranslation(m_v4TurretOff.x, m_v4TurretOff.y, m_v4TurretOff.z);

	m_mTurretWorldMatrix =  mRotX * mRotY * mRotZ * mTrans * m_mWorldMatrix;

	// Calculate m_mGunWorldMatrix for gun based on Euler rotation angles and position data.
	// Parent the gun to the turret
	m_mGunWorldMatrix = XMMatrixIdentity();
	mRotX = XMMatrixRotationX(XMConvertToRadians(m_v4GunRot.x));
	mRotY = XMMatrixRotationY(XMConvertToRadians(m_v4GunRot.y));
	mRotZ = XMMatrixRotationZ(XMConvertToRadians(m_v4GunRot.z));
	mTrans = XMMatrixTranslation(m_v4GunOff.x, m_v4GunOff.y, m_v4GunOff.z);

	m_mGunWorldMatrix =  mRotX * mRotY * mRotZ * mTrans * m_mTurretWorldMatrix;

	// Calculate m_mCameraWorldMatrix for camera based on Euler rotation angles and position data.
	m_mCamWorldMatrix = XMMatrixIdentity();
	mRotX = XMMatrixRotationX(XMConvertToRadians(m_v4CamRot.x));
	mRotY = XMMatrixRotationY(XMConvertToRadians(m_v4CamRot.y));
	mRotZ = XMMatrixRotationZ(XMConvertToRadians(m_v4CamRot.z));
	mTrans = XMMatrixTranslation(m_v4CamOff.x, m_v4CamOff.y, m_v4CamOff.z);

	// [Week 2] Switch between parenting the camera to the plane (without X and Z rotations) and the gun based on m_bGunCam
	if( m_bGunCam )
		m_mCamWorldMatrix =  mTrans * mRotX * mRotY * mRotZ * m_mGunWorldMatrix;
	else
		m_mCamWorldMatrix =  mTrans * mRotX * mRotY * mRotZ * mPlaneCameraRot;
	
	// Get the camera's world position out of the matrix
	m_vCamWorldPos = m_mCamWorldMatrix.r[3];
}

void Aeroplane::Update( bool bPlayerControl )
{
	if( bPlayerControl )
	{
		// Step 1: Make the plane pitch upwards on the up arrow key and return to level when released
		// Maximum pitch = 60 degrees
		if (Application::s_pApp->IsKeyPressed('Q') && m_v4Rot.x < 60 && m_fSpeed > 0.5)
			m_v4Rot.x += 2.0f;
		else if (!Application::s_pApp->IsKeyPressed('Q') && m_v4Rot.x > 0)
			m_v4Rot.x -= 1.0f;

		// Step 2: Make the plane pitch downwards on the down arrow key and return to level when released
		// You can also impose a take off seeepd of 0.5 if you like
		// Minimum pitch = -60 degrees
		if (Application::s_pApp->IsKeyPressed('A') && m_v4Rot.x > -60 && m_fSpeed > 0.5)
			m_v4Rot.x -= 2.0f;
		else if (!Application::s_pApp->IsKeyPressed('A') && m_v4Rot.x < 0)
			m_v4Rot.x += 1.0f;

		// Step 3: Make the plane yaw and roll left on the left arrow key and return to level when released
		// Maximum roll = 20 degrees
		if( Application::s_pApp->IsKeyPressed('O') )
		{
			m_v4Rot.y -= 1.0f;
			if (m_v4Rot.z < 20)
				m_v4Rot.z += 2.0f;
		}
		else if (m_v4Rot.z > 0)
			m_v4Rot.z -= 1.0f;

		// Step 4: Make the plane yaw and roll right on the right arrow key and return to level when released
		// Minimum roll = -20 degrees
		if( Application::s_pApp->IsKeyPressed('P') )
		{
			m_v4Rot.y += 1.0f;
			if (m_v4Rot.z > -20)
				m_v4Rot.z -= 2.0f;
		}
		else if (m_v4Rot.z < 0)
			m_v4Rot.z += 1.0f;

	} // End of if player control

	// Apply a forward thrust and limit to a maximum speed of 1
	//m_fSpeed += 0.001f;

	if( m_fSpeed > 1 )
		m_fSpeed = 1;

	// Rotate propellor and turret
	m_v4PropRot.z += 100 * m_fSpeed;
	m_v4TurretRot.y += 0.1f;

	// Tilt gun up and down as turett rotates
	m_v4GunRot.x = (sin((float)XMConvertToRadians(m_v4TurretRot.y*4.0f)) * 10.0f) - 10.0f;

	UpdateMatrices();

	// Move Forward
	XMVECTOR vCurrPos = XMLoadFloat4(&m_v4Pos);
	vCurrPos += m_vForwardVector * m_fSpeed;
	XMStoreFloat4(&m_v4Pos, vCurrPos);
}



void Aeroplane::Draw(const AeroplaneMeshes *pMeshes)
{
	Application::s_pApp->SetWorldMatrix(m_mWorldMatrix);
	pMeshes->pPlaneMesh->Draw();

	Application::s_pApp->SetWorldMatrix(m_mPropWorldMatrix);
	pMeshes->pPropMesh->Draw();

	Application::s_pApp->SetWorldMatrix(m_mTurretWorldMatrix);
	pMeshes->pTurretMesh->Draw();

	Application::s_pApp->SetWorldMatrix(m_mGunWorldMatrix);
	pMeshes->pGunMesh->Draw();
}



