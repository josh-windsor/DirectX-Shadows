#include "Application.h"
#include "HeightMap.h"
#include "Aeroplane.h"

Application* Application::s_pApp = NULL;


const int CAMERA_MAP = 0;
const int CAMERA_PLANE = 1;
const int CAMERA_GUN = 2;
const int CAMERA_LIGHT = 3;
const int CAMERA_MAX = 4;

static const int RENDER_TARGET_WIDTH = 512;
static const int RENDER_TARGET_HEIGHT = 512;

static const float AEROPLANE_RADIUS = 6.f;

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

bool Application::HandleStart()
{
	s_pApp = this;

	this->SetWindowTitle("Shadows");

	m_pHeightMap = NULL;

	m_pAeroplane = NULL;
	m_pAeroplaneDefaultMeshes = NULL;
	m_pAeroplaneShadowMeshes = NULL;

	m_pRenderTargetColourTexture = NULL;
	m_pRenderTargetDepthStencilTexture = NULL;
	m_pRenderTargetColourTargetView = NULL;
	m_pRenderTargetColourTextureView = NULL;
	m_pRenderTargetDepthStencilTargetView = NULL;

	m_drawHeightMapShaderVSConstants.pCB = NULL;
	m_drawHeightMapShaderPSConstants.pCB = NULL;

	m_pRenderTargetDebugDisplayBuffer = NULL;

	m_shadowCastingLightPosition = XMFLOAT3(4.0f, 10.f, 0.f);
	m_shadowColour = XMFLOAT4(0.f, 0.f, 0.f, .25f);

	m_pShadowSamplerState = NULL;

	if (!this->CommonApp::HandleStart())
		return false;

	char aMaxNumLightsStr[100];
	_snprintf_s(aMaxNumLightsStr, sizeof aMaxNumLightsStr, _TRUNCATE, "%d", MAX_NUM_LIGHTS);

	const D3D_SHADER_MACRO aMacros[] = {
		{"MAX_NUM_LIGHTS", aMaxNumLightsStr, },
		{NULL},
	};

	if (!this->CompileShaderFromFile(&m_drawShadowCasterShader, "DrawShadowCaster.hlsl", aMacros, g_aVertexDesc_Pos3fColour4ubNormal3f, g_vertexDescSize_Pos3fColour4ubNormal3f))
		return false;

	// DrawHeightMap.hlsl
	{
		ID3D11VertexShader *pVS = NULL;
		ID3D11PixelShader *pPS = NULL;
		ID3D11InputLayout *pIL = NULL;
		ShaderDescription vs, ps;

		if (!CompileShadersFromFile(m_pD3DDevice, "DrawHeightMap.hlsl",
			"VSMain", &pVS, &vs,
			g_aVertexDesc_Pos3fColour4ubNormal3f, g_vertexDescSize_Pos3fColour4ubNormal3f, &pIL,
			"PSMain", &pPS, &ps,
			aMacros))
		{
			return false;
		}

		this->CreateShaderFromCompiledShader(&m_drawHeightMapShader, pVS, &vs, pIL, pPS, &ps);

		vs.FindCBuffer("DrawHeightMap", &m_drawHeightMapShaderVSConstants.slot);
		vs.FindFloat4x4(m_drawHeightMapShaderVSConstants.slot, "g_shadowMatrix", &m_drawHeightMapShaderVSConstants.shadowMatrix);
		vs.FindFloat4(m_drawHeightMapShaderVSConstants.slot, "g_shadowColour", &m_drawHeightMapShaderVSConstants.shadowColour);
		m_drawHeightMapShaderVSConstants.pCB = CreateBuffer(m_pD3DDevice, vs.GetCBufferSizeBytes(m_drawHeightMapShaderVSConstants.slot), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, NULL);

		ps.FindCBuffer("DrawHeightMap", &m_drawHeightMapShaderPSConstants.slot);
		ps.FindFloat4x4(m_drawHeightMapShaderPSConstants.slot, "g_shadowMatrix", &m_drawHeightMapShaderPSConstants.shadowMatrix);
		ps.FindFloat4(m_drawHeightMapShaderPSConstants.slot, "g_shadowColour", &m_drawHeightMapShaderPSConstants.shadowColour);
		m_drawHeightMapShaderPSConstants.pCB = CreateBuffer(m_pD3DDevice, ps.GetCBufferSizeBytes(m_drawHeightMapShaderPSConstants.slot), D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, NULL);

		ps.FindTexture("g_shadowTexture", &m_heightMapShaderShadowTexture);
		ps.FindSamplerState("g_shadowSampler", &m_heightMapShaderShadowSampler);
	}

	if (!this->CreateRenderTarget())
		return false;

	m_bWireframe = false;

	m_pHeightMap = new HeightMap( "Resources/heightmap.bmp", 2.0f, &m_drawHeightMapShader );
	m_pAeroplane = new Aeroplane( 0.0f, 3.5f, 0.0f, 105.0f );

	m_pAeroplaneDefaultMeshes = AeroplaneMeshes::Load();
	if (!m_pAeroplaneDefaultMeshes)
		return false;

	// And the shadow copy.
	//
	// For a larger-scale project, with more complicated meshes, there would
	// probably be simplified meshes used for drawing the shadows. This example
	// just draws the same meshes using a different shader.
	m_pAeroplaneShadowMeshes = AeroplaneMeshes::Load();
	if (!m_pAeroplaneShadowMeshes)
		return false;

	m_pAeroplaneShadowMeshes->pGunMesh->SetShaderForAllSubsets(&m_drawShadowCasterShader);
	m_pAeroplaneShadowMeshes->pPlaneMesh->SetShaderForAllSubsets(&m_drawShadowCasterShader);
	m_pAeroplaneShadowMeshes->pPropMesh->SetShaderForAllSubsets(&m_drawShadowCasterShader);
	m_pAeroplaneShadowMeshes->pTurretMesh->SetShaderForAllSubsets(&m_drawShadowCasterShader);

	m_cameraZ = 50.0f;
	m_rotationAngle = 0.f;
	m_rotationSpeed = 0.01f;

	m_cameraState = CAMERA_MAP;

	m_pShadowCastingLightMesh = CommonMesh::NewSphereMesh(this, 1.0f, 16, 16);
		
	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void Application::HandleStop()
{
	Release(m_pRenderTargetDebugDisplayBuffer);

	Release(m_pRenderTargetColourTexture);
	Release(m_pRenderTargetDepthStencilTexture);
	Release(m_pRenderTargetColourTargetView);
	Release(m_pRenderTargetColourTextureView);
	Release(m_pRenderTargetDepthStencilTargetView);

	Release(m_pShadowSamplerState);

	Release(m_drawHeightMapShaderVSConstants.pCB);
	Release(m_drawHeightMapShaderPSConstants.pCB);

	delete m_pHeightMap;

	delete m_pAeroplane;
	delete m_pAeroplaneDefaultMeshes;
	delete m_pAeroplaneShadowMeshes;

	m_drawShadowCasterShader.Reset();

	delete m_pShadowCastingLightMesh;

	this->CommonApp::HandleStop();
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void Application::HandleUpdate()
{
	m_rotationAngle += m_rotationSpeed;

	if( m_cameraState == CAMERA_MAP )
	{
		if (this->IsKeyPressed('Q'))
			m_cameraZ -= 2.0f;
		
		if (this->IsKeyPressed('A'))
			m_cameraZ += 2.0f;

		static bool dbS = false;

		if (this->IsKeyPressed(VK_SPACE))
		{
			if( !dbS )
			{
				if( m_rotationSpeed == 0.0f )
					m_rotationSpeed = 0.01f;
				else
					m_rotationSpeed = 0.0f;

				dbS = true;
			}
		}
		else
		{
			dbS = false;
		}
	}

	
	static bool dbC = false;

	if (this->IsKeyPressed('C') )	
	{
		if( !dbC )
		{
			if( ++m_cameraState == CAMERA_MAX )
				m_cameraState = CAMERA_MAP;

			dbC = true;
		}
	}
	else
	{
		dbC = false;
	}

	if( m_cameraState != CAMERA_PLANE && m_cameraState != CAMERA_GUN )
	{
		if( this->IsKeyPressed(VK_LEFT) )
			m_shadowCastingLightPosition.x+=.2f;

		if( this->IsKeyPressed(VK_RIGHT) )
			m_shadowCastingLightPosition.x-=.2f;

		if( this->IsKeyPressed(VK_UP ) )
			m_shadowCastingLightPosition.z+=.2f;

		if( this->IsKeyPressed(VK_DOWN ) )
			m_shadowCastingLightPosition.z-=.2f;

		if( this->IsKeyPressed(VK_PRIOR ) )
			m_shadowCastingLightPosition.y-=.2f;

		if( this->IsKeyPressed(VK_NEXT ) )
			m_shadowCastingLightPosition.y+=.2f;
	}

	static bool dbW = false;
	if (this->IsKeyPressed('W') )	
	{
		if( !dbW )
		{
			m_bWireframe = !m_bWireframe;
			this->SetRasterizerState( false, m_bWireframe );
			dbW = true;
		}
	}
	else
	{
		dbW = false;
	}

	
	m_pAeroplane->Update( m_cameraState != CAMERA_MAP );

}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void Application::HandleRender()
{
	this->RenderShadow();

	this->Render3D();

	this->Render2D();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Application::RenderShadow()
{
	// Only the alpha channel is relevant, but clear the RGB channels to white
	// so that it's easy to see what's going on.
	float clearColour[4] = {0.f, 0.f, 0.f, 0.f};
	m_pD3DDeviceContext->ClearRenderTargetView(m_pRenderTargetColourTargetView, clearColour);
	m_pD3DDeviceContext->ClearDepthStencilView(m_pRenderTargetDepthStencilTargetView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
	m_pD3DDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetColourTargetView, m_pRenderTargetDepthStencilTargetView);

	D3D11_VIEWPORT viewport = {0.f, 0.f, RENDER_TARGET_WIDTH, RENDER_TARGET_HEIGHT, 0.f, 1.f};
	m_pD3DDeviceContext->RSSetViewports(1, &viewport);

	XMFLOAT4 vTemp = m_pAeroplane->GetPosition();
	XMVECTOR vPlanePos = XMLoadFloat4(&vTemp);

	//*************************************************************************
	// Your code to adjust the perspective projection of the light goes here
	// You will need to calculate fovy, zn and zf instead of using these default values:
	float fovy = 0.8f;
	float zn = 1.0f;
	float zf = 100.0f;
	float aspect = RENDER_TARGET_WIDTH / RENDER_TARGET_HEIGHT;
	// You will find the following constants (defined above) useful:
	// RENDER_TARGET_WIDTH, RENDER_TARGET_HEIGHT, AEROPLANE_RADIUS
	//*************************************************************************

	// STEP 3:
	// Manipulate the texture RT so it occupys as much of the box regardless of the box
	// Take the FOV down on the light cam
	// Zoom right in on it too
	// DO in Application::RenderShadow();

	// Calculate an optimal near, far, fov to contain whole of the plane
	// The plane has a radius, use this too help
	// diff between light pos and plane, then add the radius for far plane, take away for the close plane
	// make a triangle between i
	// use inverse tangent (atan) to get fov?!


	// Sample render target to see if this pixel is in shadow

	// If it is then alpha blend between final colour and shadow colour

	XMVECTOR vecLightPos = XMLoadFloat3(&m_shadowCastingLightPosition);
	XMVECTOR vecLightDir = XMVector3Normalize(vPlanePos - vecLightPos);

	//gets the plane from the plane point along its radius towards the light direction
	XMVECTOR vecNearPlane = AEROPLANE_RADIUS * -vecLightDir + vPlanePos;
	XMVECTOR vecFarPlane =  AEROPLANE_RADIUS * vecLightDir + vPlanePos;

	XMVECTOR vecCamNear = vecLightPos - vecNearPlane;
	XMVECTOR vecCamFar = vecLightPos - vecFarPlane;

	XMVECTOR vecCamNearLength = XMVector3Length(vecCamNear);
	XMVECTOR vecCamFarLength = XMVector3Length(vecCamFar);

	zn = XMVectorGetX(vecCamNearLength);
	zf = XMVectorGetX(vecCamFarLength);

	fovy = sqrt(atan2(100, zn * zf - 150));

	dprintf("%f\n", fovy);

	XMMATRIX projMtx;
	projMtx = XMMatrixPerspectiveFovLH(fovy, aspect, zn, zf);
	this->SetProjectionMatrix(projMtx);

	XMMATRIX viewMtx;
	viewMtx = XMMatrixLookAtLH(XMLoadFloat3(&m_shadowCastingLightPosition), vPlanePos, XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f) );
	this->SetViewMatrix(viewMtx);

	m_shadowMtx = XMMatrixMultiply(viewMtx, projMtx );

	D3DXMATRIX worldMtx;
	D3DXMatrixIdentity(&worldMtx);
	this->SetWorldMatrix(worldMtx);

	this->SetDepthStencilState(true);
	this->SetRasterizerState(false, false);
	
	m_pAeroplane->Draw(m_pAeroplaneShadowMeshes);

	this->SetDefaultRenderTarget();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Application::Render3D()
{
	this->SetRasterizerState( false, m_bWireframe );

	this->SetDepthStencilState(true);

	XMVECTOR vCamera = XMVectorZero();
	XMVECTOR vLookat = XMVectorZero();;
	XMVECTOR vUpVector = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMFLOAT4 vPlanePos = m_pAeroplane->GetPosition();
	XMFLOAT4 vCamPos = m_pAeroplane->GetCameraPosition();
	XMFLOAT4 vFocusPos = m_pAeroplane->GetPosition();

	switch( m_cameraState )
	{
		case CAMERA_MAP:
			vCamera = XMVectorSet(sin(m_rotationAngle)*m_cameraZ, m_cameraZ/3 + 4, cos(m_rotationAngle)*m_cameraZ, 0.0f);
			vLookat = XMVectorSet(0.0f, 2.0f, 0.0f, 0.0f);
			break;
		case CAMERA_PLANE:
			m_pAeroplane->SetGunCamera( false );
			vCamera = XMLoadFloat4(&vCamPos);
			vLookat = XMLoadFloat4(&vFocusPos);
			break;
		case CAMERA_GUN:
			m_pAeroplane->SetGunCamera( true );
			vCamera = XMLoadFloat4(&vCamPos);
			vLookat = XMLoadFloat4(&vFocusPos);
			break;
		case CAMERA_LIGHT:
			vCamera = XMLoadFloat3(&m_shadowCastingLightPosition);
			vLookat = XMLoadFloat4(&vPlanePos);
			break;
			
	}

	XMMATRIX  matView;
	matView = XMMatrixLookAtLH(vCamera, vLookat, vUpVector);

	XMMATRIX matProj;
	matProj = XMMatrixPerspectiveFovLH(float(D3DX_PI / 4), 2, 1.5f, 5000.0f);

	this->SetViewMatrix(matView);
	this->SetProjectionMatrix(matProj);

	this->EnablePointLight(0, m_shadowCastingLightPosition, XMFLOAT3(1.0f, 1.0f, 1.0f) );
	this->SetLightAttenuation(0, 200.f, 2.f, 2.f, 2.f);
	this->EnableDirectionalLight(1, XMFLOAT3(-1.f, -1.f, -1.f), XMFLOAT3(0.65f, 0.55f, 0.65f));

	this->Clear(XMFLOAT4(.2f, .2f, .6f, 1.f));

	D3DXMATRIX  matWorld;
	D3DXMatrixIdentity( &matWorld );
	this->SetWorldMatrix(matWorld);

	{
		D3D11_MAPPED_SUBRESOURCE vsMap;
		if (!m_drawHeightMapShaderVSConstants.pCB || FAILED(m_pD3DDeviceContext->Map(m_drawHeightMapShaderVSConstants.pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &vsMap)))
			vsMap.pData = NULL;

		D3D11_MAPPED_SUBRESOURCE psMap;
		if (!m_drawHeightMapShaderPSConstants.pCB || FAILED(m_pD3DDeviceContext->Map(m_drawHeightMapShaderPSConstants.pCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &psMap)))
			psMap.pData = NULL;

		SetCBufferFloat4x4(vsMap, m_drawHeightMapShaderVSConstants.shadowMatrix, m_shadowMtx);
		SetCBufferFloat4x4(psMap, m_drawHeightMapShaderPSConstants.shadowMatrix, m_shadowMtx);

		SetCBufferFloat4(vsMap, m_drawHeightMapShaderVSConstants.shadowColour, m_shadowColour);
		SetCBufferFloat4(psMap, m_drawHeightMapShaderPSConstants.shadowColour, m_shadowColour);

		if (psMap.pData)
			m_pD3DDeviceContext->Unmap(m_drawHeightMapShaderPSConstants.pCB, 0);

		if (vsMap.pData)
			m_pD3DDeviceContext->Unmap(m_drawHeightMapShaderVSConstants.pCB, 0);

		if (m_drawHeightMapShaderVSConstants.slot >= 0)
			m_pD3DDeviceContext->VSSetConstantBuffers(m_drawHeightMapShaderVSConstants.slot, 1, &m_drawHeightMapShaderVSConstants.pCB);

		if (m_drawHeightMapShaderPSConstants.slot >= 0)
			m_pD3DDeviceContext->PSSetConstantBuffers(m_drawHeightMapShaderPSConstants.slot, 1, &m_drawHeightMapShaderPSConstants.pCB);

		if (m_heightMapShaderShadowSampler >= 0)
			m_pD3DDeviceContext->PSSetSamplers(m_heightMapShaderShadowSampler, 1, &m_pShadowSamplerState);

		if (m_heightMapShaderShadowTexture >= 0)
			m_pD3DDeviceContext->PSSetShaderResources(m_heightMapShaderShadowTexture, 1, &m_pRenderTargetColourTextureView);

		m_pHeightMap->Draw();
	}

	m_pAeroplane->Draw(m_pAeroplaneDefaultMeshes);
	
	D3DXMATRIX worldMtx;
	D3DXMatrixTranslation(&worldMtx, m_shadowCastingLightPosition.x,  m_shadowCastingLightPosition.y,  m_shadowCastingLightPosition.z);
	this->SetWorldMatrix(worldMtx);
	m_pShadowCastingLightMesh->Draw();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void Application::Render2D()
{
	float windowWidth, windowHeight;
	this->GetWindowSize(&windowWidth, &windowHeight);

	D3DXMATRIX projMtx;
	D3DXMatrixOrthoOffCenterLH(&projMtx, 0.f, windowWidth, 0.f, windowHeight, 1.f, 250.f);
	this->SetProjectionMatrix(projMtx);

	D3DXMATRIX viewMtx;
	D3DXMatrixTranslation(&viewMtx, 0.f, 0.f, 2.f);
	this->SetViewMatrix(viewMtx);

	D3DXMATRIX worldMtx;
	D3DXMatrixIdentity(&worldMtx);
	this->SetWorldMatrix(worldMtx);

	this->SetDepthStencilState(false, false);
	this->SetRasterizerState(false, false);
	this->SetBlendState(false);

	this->DrawTextured(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, m_pRenderTargetDebugDisplayBuffer, NULL, 4, m_pRenderTargetColourTextureView, this->GetSamplerState());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool Application::CreateRenderTarget()
{
	// Create render target texture, and two views for it - one for using it as
	// a render target, and one for using it as a texture.
	{
		HRESULT hr;

		D3D11_TEXTURE2D_DESC td;

		td.Width = RENDER_TARGET_WIDTH;
		td.Height = RENDER_TARGET_HEIGHT;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		td.CPUAccessFlags = 0;
		td.MiscFlags = 0;

		hr = m_pD3DDevice->CreateTexture2D(&td, NULL, &m_pRenderTargetColourTexture);
		if (FAILED(hr))
		{
			this->SetStartErrorMessage("Failed to create render target colour texture.");
			return false;
		}

		D3D11_RENDER_TARGET_VIEW_DESC rtvd;

		rtvd.Format = td.Format;
		rtvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvd.Texture2D.MipSlice = 0;

		hr = m_pD3DDevice->CreateRenderTargetView(m_pRenderTargetColourTexture, &rtvd, &m_pRenderTargetColourTargetView);
		if (FAILED(hr))
		{
			this->SetStartErrorMessage("Failed to create render target colour target view.");
			return false;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC svd;

		svd.Format = td.Format;
		svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		svd.Texture2D.MostDetailedMip = 0;
		svd.Texture2D.MipLevels = 1;

		hr = m_pD3DDevice->CreateShaderResourceView(m_pRenderTargetColourTexture, &svd, &m_pRenderTargetColourTextureView);
		if (FAILED(hr))
		{
			this->SetStartErrorMessage("Failed to create render target colour texture view.");
			return false;
		}
	}

	// Do the same again, roughly, for its matching depth buffer. (There is a
	// default depth buffer, but it can't really be re-used for this, because it
	// tracks the window size and the window could end up too small.)
	{
		HRESULT hr;

		D3D11_TEXTURE2D_DESC td;

		td.Width = RENDER_TARGET_WIDTH;
		td.Height = RENDER_TARGET_HEIGHT;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_D16_UNORM;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		td.CPUAccessFlags = 0;
		td.MiscFlags = 0;

		hr = m_pD3DDevice->CreateTexture2D(&td, NULL, &m_pRenderTargetDepthStencilTexture);
		if (FAILED(hr))
		{
			this->SetStartErrorMessage("Failed to create render target depth/stencil texture.");
			return false;
		}

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;

		dsvd.Format = td.Format;
		dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvd.Flags = 0;
		dsvd.Texture2D.MipSlice = 0;

		hr = m_pD3DDevice->CreateDepthStencilView(m_pRenderTargetDepthStencilTexture, &dsvd, &m_pRenderTargetDepthStencilTargetView);
		if (FAILED(hr))
		{
			this->SetStartErrorMessage("Failed to create render target depth/stencil view.");
			return false;
		}
	}

	// VB for drawing render target on screen.
	static const Vertex_Pos3fColour4ubTex2f aRenderTargetDebugDisplayBufferVtxs[] = {
		Vertex_Pos3fColour4ubTex2f(XMFLOAT3(0.f, 0.f, 0.f), VertexColour(255, 255, 255, 255), XMFLOAT2(0.f, 1.f)),
		Vertex_Pos3fColour4ubTex2f(XMFLOAT3(256.f, 0.f, 0.f), VertexColour(255, 255, 255, 255), XMFLOAT2(1.f, 1.f)),
		Vertex_Pos3fColour4ubTex2f(XMFLOAT3(0.f, 256.f, 0.f), VertexColour(255, 255, 255, 255), XMFLOAT2(0.f, 0.f)),
		Vertex_Pos3fColour4ubTex2f(XMFLOAT3(256.f, 256.f, 0.f), VertexColour(255, 255, 255, 255), XMFLOAT2(1.f, 0.f)),
	};

	m_pRenderTargetDebugDisplayBuffer = CreateImmutableVertexBuffer(m_pD3DDevice, sizeof aRenderTargetDebugDisplayBufferVtxs, aRenderTargetDebugDisplayBufferVtxs);

	// Sampler state for using the shadow texture to draw with.
	{
		D3D11_SAMPLER_DESC sd;

		sd.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;

		// Use BORDER addressing, so that anything outside the area the shadow
		// texture casts on can be given a specific fixed colour.
		sd.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
		sd.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
		sd.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;

		sd.MipLODBias = 0.f;
		sd.MaxAnisotropy = 16;
		sd.ComparisonFunc = D3D11_COMPARISON_NEVER;

		// Set the border colour to transparent, corresponding to unshadowed.
		sd.BorderColor[0] = 0.f;
		sd.BorderColor[1] = 0.f;
		sd.BorderColor[2] = 0.f;
		sd.BorderColor[3] = 0.f;

		sd.MinLOD = 0.f;
		sd.MaxLOD = D3D11_FLOAT32_MAX;

		if (FAILED(m_pD3DDevice->CreateSamplerState(&sd, &m_pShadowSamplerState)))
			return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////


int WINAPI WinMain(HINSTANCE,HINSTANCE,LPSTR,int)
{
	Application application;

	Run(&application);

	return 0;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
