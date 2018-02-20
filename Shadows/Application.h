#ifndef APPLICATION_H
#define APPLICATION_H

#define WIN32_LEAN_AND_MEAN

#include <assert.h>

#include <stdio.h>
#include <windows.h>
#include <d3d11.h>

#include "CommonApp.h"
#include "CommonMesh.h"

class Aeroplane;
class AeroplaneMeshes;
class HeightMap;

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

class Application : public CommonApp
{
public:
	static Application* s_pApp;

protected:
	bool HandleStart();
	void HandleStop();
	void HandleUpdate();
	void HandleRender();

private:
	float m_rotationAngle;
	float m_rotationSpeed;
	float m_cameraZ;
	bool m_bWireframe;

	Shader m_drawShadowCasterShader;

	Shader m_drawHeightMapShader;
	int m_heightMapShaderShadowTexture;
	int m_heightMapShaderShadowSampler;

	struct DrawHeightMapShaderConstants
	{
		ID3D11Buffer *pCB;
		int slot;
		int shadowMatrix;
		int shadowColour;
	};

	DrawHeightMapShaderConstants m_drawHeightMapShaderVSConstants;
	DrawHeightMapShaderConstants m_drawHeightMapShaderPSConstants;

	XMMATRIX m_shadowMtx;

	int m_cameraState;

	Aeroplane* m_pAeroplane;
	AeroplaneMeshes *m_pAeroplaneDefaultMeshes;
	AeroplaneMeshes *m_pAeroplaneShadowMeshes;

	HeightMap* m_pHeightMap;

	CommonMesh *m_pShadowCastingLightMesh;
	XMFLOAT3 m_shadowCastingLightPosition;
	XMFLOAT4 m_shadowColour;

	ID3D11Texture2D *m_pRenderTargetColourTexture;//render target colour buffer
	ID3D11Texture2D *m_pRenderTargetDepthStencilTexture;//render target depth buffer
	ID3D11RenderTargetView *m_pRenderTargetColourTargetView;//colour buffer, as render target
	ID3D11ShaderResourceView *m_pRenderTargetColourTextureView;//colour buffer, as texture
	ID3D11DepthStencilView *m_pRenderTargetDepthStencilTargetView;//depth buffer, as render target

	ID3D11Buffer *m_pRenderTargetDebugDisplayBuffer;

	ID3D11SamplerState *m_pShadowSamplerState;

	bool CreateRenderTarget();

	void RenderShadow();
	void Render3D();
	void Render2D();
};

#endif

