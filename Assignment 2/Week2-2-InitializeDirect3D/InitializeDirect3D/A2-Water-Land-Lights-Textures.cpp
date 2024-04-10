/** @file Week4-5-ShapePractice.cpp
 *  @brief Shape Practice.
 *
 *  Place all of the scene geometry in one big vertex and index buffer.
 * Then use the DrawIndexedInstanced method to draw one object at a time ((as the
 * world matrix needs to be changed between objects)
 *
 *   Controls:
 *   Hold down '1' key to view scene in wireframe mode.
 *   Hold the left mouse button down and move the mouse to rotate.
 *   Hold the right mouse button down and move the mouse to zoom in and out.
 *
 *  @author Hooman Salamat
 */

#include "../../Common/Camera.h"
#include "../../Common/d3dApp.h"
#include "../../Common/GeometryGenerator.h"
#include "../../Common/MathHelper.h"
#include "../../Common/UploadBuffer.h"
#include "FrameResource.h"
#include "Waves.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	MeshGeometry* Geo = nullptr;
	Material* Mat = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int BaseVertexLocation = 0;

	BoundingBox Bounds;
	//step1: An invisible render-item will not be drawn.
	bool Visible = true;
};

// Tree step14
enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	AlphaTestedTreeSprites,
	Highlight,
	Count
};

class ShapesApp : public D3DApp
{
public:
	ShapesApp(HINSTANCE hInstance);
	ShapesApp(const ShapesApp& rhs) = delete;
	ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp();

	virtual bool Initialize()override;

private:

	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;

	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

	void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);
	void UpdateWaves(const GameTimer& gt);

	// Texture Step1
	void LoadTextures();
	void BuildRootSignature();

	// Texture Step2
	void BuildDescriptorHeaps();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildLandGeometry();
	void BuildSkullGeometry();

	// Tree Step1
	void BuildTreeSpritesGeometry();
	void BuildWavesGeometry();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildMaterials();
	void BuildRenderItems();
	void CreateItem(const char* item, XMMATRIX p, XMMATRIX q, XMMATRIX r, UINT ObjIndex, const char* material);
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	// Texture Step2-2
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;

	// Camera Collision Test with Maze Walls
	void MazeCollision(int sx, int sy);
	
private:

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mCbvSrvDescriptorSize = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

	RenderItem* mWavesRitem = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// Tree step7
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

	//ComPtr<ID3D12PipelineState> mOpaquePSO = nullptr;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	// Tree step 16
	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	std::unique_ptr<Waves> mWaves;

	// Render items divided by PSO.
	//std::vector<RenderItem*> mOpaqueRitems;

	PassConstants mMainPassCB;

	bool mIsWireframe = false;

	/* OLD camera
	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();
	float mTheta = 1.7f * XM_PI;
	float mPhi = XM_PIDIV2 - 0.3f;
	float mRadius = 100.0f;
	*/

	// NEW FPS Camera
	Camera mCamera;

	POINT mLastMousePos;

	bool bStopForwardMovement;
	RenderItem* mPickedRitem = nullptr;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		ShapesApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

ShapesApp::ShapesApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}
void ShapesApp::CreateItem(const char* item, XMMATRIX p, XMMATRIX q, XMMATRIX r, UINT ObjIndex, const char* material)
{
	auto RightWall = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&RightWall->World, p * q * r);
	RightWall->ObjCBIndex = ObjIndex;
	RightWall->Mat = mMaterials[material].get();// "Wood"
	RightWall->Geo = mGeometries["shapeGeo"].get();
	RightWall->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	RightWall->Bounds = RightWall->Geo->DrawArgs[item].Bounds;
	RightWall->IndexCount = RightWall->Geo->DrawArgs[item].IndexCount;
	RightWall->StartIndexLocation = RightWall->Geo->DrawArgs[item].StartIndexLocation;
	RightWall->BaseVertexLocation = RightWall->Geo->DrawArgs[item].BaseVertexLocation;
	//mAllRitems.push_back(std::move(RightWall));
	mRitemLayer[(int)RenderLayer::Opaque].push_back(RightWall.get());
	mAllRitems.push_back(std::move(RightWall));
}

bool ShapesApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);
	

	// Texture Step3
	LoadTextures();
	BuildRootSignature();
	// Texture Step4
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildSkullGeometry();
	BuildWavesGeometry();
	BuildLandGeometry();
	// Tree Step2
	BuildTreeSpritesGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void ShapesApp::OnResize()
{
	D3DApp::OnResize();


	//When the window is resized, we no longer rebuild the projection matrix explicitly,
	//and instead delegate the work to the Camera class with SetLens
	mCamera.SetLens(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	//XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	//XMStoreFloat4x4(&mProj, P);
}

void ShapesApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	//UpdateCamera(gt);
	//MazeCollision(mClientWidth /2, mClientHeight /2);

	// Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
	UpdateWaves(gt);
}

void ShapesApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	//ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mOpaquePSO.Get()));

	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}

	mCommandList->RSSetViewports(1, &mScreenViewport);
	mCommandList->RSSetScissorRects(1, &mScissorRect);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::Black, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	// Texture Step5
	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());
	
	// Tree step29
	//DrawRenderItems(mCommandList.Get(), mOpaqueRitems);
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	// Tree step13
	mCommandList->SetPipelineState(mPSOs["treeSprites"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	mCommandList->SetPipelineState(mPSOs["highlight"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Highlight]);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		mLastMousePos.x = x;
		mLastMousePos.y = y;

		SetCapture(mhMainWnd);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		//step4
		MazeCollision(x,y);
	}
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	//if left mouse button is down and moving
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		//step4: Instead of updating the angles based on input to orbit camera around scene, 
		//we rotate the camera’s look direction:
		//mTheta += dx;
		//mPhi += dy;

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	//GetAsyncKeyState returns a short (2 bytes)
	if (GetAsyncKeyState('W') & 0x8000) //most significant bit (MSB) is 1 when key is pressed (1000 000 000 000)
	{
		if (bStopForwardMovement == false)
		{
			mCamera.Walk(10.0f * dt);
		}
	}

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);

	mCamera.UpdateViewMatrix();
}

//void ShapesApp::UpdateCamera(const GameTimer& gt)
//{
//	const float dt = gt.DeltaTime();
//
//	//GetAsyncKeyState returns a short (2 bytes)
//	if (GetAsyncKeyState('W') & 0x8000) //most significant bit (MSB) is 1 when key is pressed (1000 000 000 000)
//	{
//		if (bStopForwardMovement == false)
//		{
//			mCamera.Walk(10.0f * dt);
//		}
//	}
//
//	if (GetAsyncKeyState('S') & 0x8000)
//		mCamera.Walk(-10.0f * dt);
//
//	if (GetAsyncKeyState('A') & 0x8000)
//		mCamera.Strafe(-10.0f * dt);
//
//	if (GetAsyncKeyState('D') & 0x8000)
//		mCamera.Strafe(10.0f * dt);
//
//	mCamera.UpdateViewMatrix();
//}

void ShapesApp::AnimateMaterials(const GameTimer& gt)
{
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["eight"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	tu += 0.1f * gt.DeltaTime();
	tv += 0.02f * gt.DeltaTime();

	if (tu >= 1.0f)
		tu -= 1.0f;

	if (tv >= 1.0f)
		tv -= 1.0f;

	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;

}


void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = DirectX::XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = DirectX::XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			DirectX::XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			DirectX::XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = DirectX::XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			DirectX::XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	DirectX::XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	DirectX::XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	DirectX::XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	DirectX::XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	DirectX::XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	DirectX::XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 1.0f, 0.3f, 1.9f, 0.8f };
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };

	// Light Step2 
// (Step 1 in Default.hlsl) (Adding a point light)
	mMainPassCB.Lights[3].Position = { -10.0f, 0.0f, 4.0f };
	mMainPassCB.Lights[3].Strength = { 2.0f, 2.0f, 0.0f };


	// (Adding a Spot light)
	// Top down forward
	mMainPassCB.Lights[4].Position = { -10.0f, 8.0f, 3.0f };
	mMainPassCB.Lights[4].Strength = { 18.0f, 0.0f, 0.0f };
	mMainPassCB.Lights[4].SpotPower = 18.0f;
	mMainPassCB.Lights[4].Direction = { 0.0f, -1.0f, 0.0f };

	// Top down back
	mMainPassCB.Lights[5].Position = { -10.0f, 4.0f, -11.0f };
	mMainPassCB.Lights[5].Strength = { 18.0f, 18.0f, 18.0f };
	mMainPassCB.Lights[5].SpotPower = 18.0f;
	mMainPassCB.Lights[5].Direction = { 0.0f, 0.0f, 1.0f };

	// Left right forward
	mMainPassCB.Lights[6].Position = { -18.0f, 3.0f, 2.0f };
	mMainPassCB.Lights[6].Strength = { 18.0f, 18.0f, 18.0f };
	mMainPassCB.Lights[6].SpotPower = 6.0f;
	mMainPassCB.Lights[6].Direction = { 1.0f, 0.0f, 0.0f };

	// Left right back
	mMainPassCB.Lights[7].Position = { -18.0f, 3.0f, 10.0f };
	mMainPassCB.Lights[7].Strength = { 18.0f, 18.0f, 18.0f };
	mMainPassCB.Lights[7].SpotPower = 6.0f;
	mMainPassCB.Lights[7].Direction = { 1.0f, 0.0f, 0.0f };

	// right left forward
	mMainPassCB.Lights[8].Position = { -2.0f, 3.0f, 2.0f };
	mMainPassCB.Lights[8].Strength = { 18.0f, 18.0f, 18.0f };
	mMainPassCB.Lights[8].SpotPower = 6.0f;
	mMainPassCB.Lights[8].Direction = { -1.0f, 0.0f, 0.0f };

	// right left back
	mMainPassCB.Lights[9].Position = { -2.0f, 3.0f, 10.0f };
	mMainPassCB.Lights[9].Strength = { 18.0f, 18.0f, 18.0f };
	mMainPassCB.Lights[9].SpotPower = 6.0f;
	mMainPassCB.Lights[9].Direction = { -1.0f, 0.0f, 0.0f };

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::UpdateWaves(const GameTimer& gt)
{
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;
	if ((mTimer.TotalTime() - t_base) >= 0.25f)
	{
		t_base += 0.25f;

		int i = MathHelper::Rand(4, mWaves->RowCount() - 5);
		int j = MathHelper::Rand(4, mWaves->ColumnCount() - 5);

		float r = MathHelper::RandF(0.2f, 0.5f);

		mWaves->Disturb(i, j, r);
	}

	// Update the wave simulation.
	mWaves->Update(gt.DeltaTime());

	// Update the wave vertex buffer with the new solution.
	auto currWavesVB = mCurrFrameResource->WavesVB.get();
	for (int i = 0; i < mWaves->VertexCount(); ++i)
	{
		Vertex v;

		v.Pos = mWaves->Position(i);
		v.Normal = mWaves->Normal(i);

		// Derive tex-coords from position by 
		// mapping [-w/2,w/2] --> [0,1]
		v.TexC.x = 0.5f + v.Pos.x / mWaves->Width();
		v.TexC.y = 0.5f - v.Pos.z / mWaves->Depth();

		currWavesVB->CopyData(i, v);
	}

	// Set the dynamic VB of the wave renderitem to the current frame VB.
	mWavesRitem->Geo->VertexBufferGPU = currWavesVB->Resource();
}

// Textures Step6
void ShapesApp::LoadTextures()
{
	auto oneTex = std::make_unique<Texture>();
	oneTex->Name = "oneTex";
	oneTex->Filename = L"../../MyTextures/one.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), oneTex->Filename.c_str(),
		oneTex->Resource, oneTex->UploadHeap));

	auto twoTex = std::make_unique<Texture>();
	twoTex->Name = "twoTex";
	twoTex->Filename = L"../../MyTextures/two.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), twoTex->Filename.c_str(),
		twoTex->Resource, twoTex->UploadHeap));

	auto threeTex = std::make_unique<Texture>();
	threeTex->Name = "threeTex";
	threeTex->Filename = L"../../MyTextures/three.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), threeTex->Filename.c_str(),
		threeTex->Resource, threeTex->UploadHeap));

	auto fourTex = std::make_unique<Texture>();
	fourTex->Name = "fourTex";
	fourTex->Filename = L"../../MyTextures/four.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fourTex->Filename.c_str(),
		fourTex->Resource, fourTex->UploadHeap));

	// Add texture step1
	//step9
	// Tree step9
	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"../../Textures/treearray.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeArrayTex->Filename.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap));

	auto sixTex = std::make_unique<Texture>();
	sixTex->Name = "sixTex";
	sixTex->Filename = L"../../MyTextures/six.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), sixTex->Filename.c_str(),
		sixTex->Resource, sixTex->UploadHeap));

	auto sevenTex = std::make_unique<Texture>();
	sevenTex->Name = "sevenTex";
	sevenTex->Filename = L"../../MyTextures/seven.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), sevenTex->Filename.c_str(),
		sevenTex->Resource, sevenTex->UploadHeap));

	auto eightTex = std::make_unique<Texture>();
	eightTex->Name = "eightTex";
	eightTex->Filename = L"../../MyTextures/eight.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), eightTex->Filename.c_str(),
		eightTex->Resource, eightTex->UploadHeap));

	auto nineTex = std::make_unique<Texture>();
	nineTex->Name = "nineTex";
	nineTex->Filename = L"../../MyTextures/nine256.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), nineTex->Filename.c_str(),
		nineTex->Resource, nineTex->UploadHeap));

	auto tenTex = std::make_unique<Texture>();
	tenTex->Name = "tenTex";
	tenTex->Filename = L"../../MyTextures/ten.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), tenTex->Filename.c_str(),
		tenTex->Resource, tenTex->UploadHeap));

	// Add texture step2
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);
	mTextures[fourTex->Name] = std::move(fourTex);
	mTextures[oneTex->Name] = std::move(oneTex);
	mTextures[twoTex->Name] = std::move(twoTex);
	mTextures[threeTex->Name] = std::move(threeTex);
	mTextures[sixTex->Name] = std::move(sixTex);
	mTextures[sevenTex->Name] = std::move(sevenTex);
	mTextures[eightTex->Name] = std::move(eightTex);
	mTextures[nineTex->Name] = std::move(nineTex);
	mTextures[tenTex->Name] = std::move(tenTex);
}

void ShapesApp::BuildRootSignature()
{
	// Textures Step7
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,  // number of descriptors
		0); // register t0

	// Root parameter can be a table, root descriptor or root constants.
	// Textures Step8
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Create root CBV.
	// Textures Step9
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
	slotRootParameter[1].InitAsConstantBufferView(0);
	slotRootParameter[2].InitAsConstantBufferView(1);
	slotRootParameter[3].InitAsConstantBufferView(2);

	// Textures Step10
	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	// Textures Step11
	//CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

// Texture Step12
void ShapesApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};

	// Add texture Step3
	// Tree Step10
	//srvHeapDesc.NumDescriptors = 3;
	srvHeapDesc.NumDescriptors = 10;

	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());


	// Add texture Step4
	auto oneTex = mTextures["oneTex"]->Resource;
	auto twoTex = mTextures["twoTex"]->Resource;
	auto threeTex = mTextures["threeTex"]->Resource;
	auto fourTex = mTextures["fourTex"]->Resource;
	// Tree step11
	auto treeArrayTex = mTextures["treeArrayTex"]->Resource;
	auto sixTex = mTextures["sixTex"]->Resource;
	auto sevenTex = mTextures["sevenTex"]->Resource;
	auto eightTex = mTextures["eightTex"]->Resource;
	auto nineTex = mTextures["nineTex"]->Resource;
	auto tenTex = mTextures["tenTex"]->Resource;



	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = oneTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = oneTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(oneTex.Get(), &srvDesc, hDescriptor);

	// Add texture Step5
	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = twoTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = twoTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(twoTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = threeTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = threeTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(threeTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = fourTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = fourTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(fourTex.Get(), &srvDesc, hDescriptor);


	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = sixTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = sixTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(sixTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = sevenTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = sevenTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(sevenTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = eightTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = eightTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(eightTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = nineTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = nineTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(nineTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	srvDesc.Format = tenTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = tenTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(tenTex.Get(), &srvDesc, hDescriptor);

	// Tree step12
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	//srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	auto desc = treeArrayTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = treeArrayTex->GetDesc().Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = treeArrayTex->GetDesc().DepthOrArraySize;
	md3dDevice->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);


}

void ShapesApp::BuildShadersAndInputLayout()
{
	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default1.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default1.hlsl", nullptr, "PS", "ps_5_1");
	//mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	// Tree Step6
	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void ShapesApp::BuildLandGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

	//
	// Extract the vertex elements we are interested and apply the height function to
	// each vertex.  In addition, color the vertices based on their height so we have
	// sandy looking beaches, grassy low hills, and snow mountain peaks.
	//

	std::vector<Vertex> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		auto& p = grid.Vertices[i].Position;
		vertices[i].Pos = p;
		vertices[i].Pos.y = GetHillsHeight(p.x, p.z);
		vertices[i].Normal = GetHillsNormal(p.x, p.z);
		vertices[i].TexC = grid.Vertices[i].TexC;
	}

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	std::vector<std::uint16_t> indices = grid.GetIndices16();
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "landGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["landGeo"] = std::move(geo);
}

void ShapesApp::BuildWavesGeometry()
{
	std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
	assert(mWaves->VertexCount() < 0x0000ffff);

	// Iterate over each quad.
	int m = mWaves->RowCount();
	int n = mWaves->ColumnCount();
	int k = 0;
	for (int i = 0; i < m - 1; ++i)
	{
		for (int j = 0; j < n - 1; ++j)
		{
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1) * n + j;

			indices[k + 3] = (i + 1) * n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1) * n + j + 1;

			k += 6; // next quad
		}
	}

	UINT vbByteSize = mWaves->VertexCount() * sizeof(Vertex);
	UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "waterGeo";

	// Set dynamically.
	geo->VertexBufferCPU = nullptr;
	geo->VertexBufferGPU = nullptr;

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["grid"] = submesh;

	mGeometries["waterGeo"] = std::move(geo);
}


void ShapesApp::BuildShapeGeometry()
{
	// Geometry Step1
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData box2 = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.5f, 3.0, 20, 20);
	GeometryGenerator::MeshData cylinder2 = geoGen.CreateCylinder(0.5f, 0.5f, 3.0, 20, 20);
	GeometryGenerator::MeshData cone = geoGen.CreateCone(1.f, 1.f, 40, 6);
	GeometryGenerator::MeshData wedge = geoGen.CreateWedge(1, 1, 1, 0);
	GeometryGenerator::MeshData diamond = geoGen.CreateDiamond(1, 2, 1, 0);


	// Geometry Step2
	UINT boxVertexOffset = 0;
	UINT box2VertexOffset = (UINT)box.Vertices.size();
	UINT cylinderVertexOffset = box2VertexOffset + (UINT)box2.Vertices.size();
	UINT cylinder2VertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();
	UINT coneVertexOffset = cylinder2VertexOffset + (UINT)cylinder2.Vertices.size();
	UINT wedgeVertexOffset = coneVertexOffset + (UINT)cone.Vertices.size();
	UINT diamondVertexOffset = wedgeVertexOffset + (UINT)wedge.Vertices.size();


	// Geometry Step3
	UINT boxIndexOffset = 0;
	UINT box2IndexOffset = (UINT)box.Indices32.size();
	UINT cylinderIndexOffset = box2IndexOffset + (UINT)box2.Indices32.size();
	UINT cylinder2IndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();
	UINT coneIndexOffset = cylinder2IndexOffset + (UINT)cylinder2.Indices32.size();
	UINT wedgeIndexOffset = coneIndexOffset + (UINT)cone.Indices32.size();
	UINT diamondIndexOffset = wedgeIndexOffset + (UINT)wedge.Indices32.size();


	// Geometry Step4
	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry box2Submesh;
	box2Submesh.IndexCount = (UINT)box2.Indices32.size();
	box2Submesh.StartIndexLocation = box2IndexOffset;
	box2Submesh.BaseVertexLocation = box2VertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry cylinder2Submesh;
	cylinder2Submesh.IndexCount = (UINT)cylinder2.Indices32.size();
	cylinder2Submesh.StartIndexLocation = cylinder2IndexOffset;
	cylinder2Submesh.BaseVertexLocation = cylinder2VertexOffset;

	SubmeshGeometry coneSubmesh;
	coneSubmesh.IndexCount = (UINT)cone.Indices32.size();
	coneSubmesh.StartIndexLocation = coneIndexOffset;
	coneSubmesh.BaseVertexLocation = coneVertexOffset;

	SubmeshGeometry wedgeSubmesh;
	wedgeSubmesh.IndexCount = (UINT)cone.Indices32.size();
	wedgeSubmesh.StartIndexLocation = wedgeIndexOffset;
	wedgeSubmesh.BaseVertexLocation = wedgeVertexOffset;

	SubmeshGeometry diamondSubmesh;
	diamondSubmesh.IndexCount = (UINT)cone.Indices32.size();
	diamondSubmesh.StartIndexLocation = diamondIndexOffset;
	diamondSubmesh.BaseVertexLocation = diamondVertexOffset;


	// Geometry Step5
	auto totalVertexCount = box.Vertices.size() + box2.Vertices.size() + cylinder.Vertices.size() + cylinder2.Vertices.size() + cone.Vertices.size() + wedge.Vertices.size() + diamond.Vertices.size();

	// For Bounding  Box
	XMFLOAT3 vMinf3(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);
	XMVECTOR vMin = DirectX::XMLoadFloat3(&vMinf3);
	XMVECTOR vMax = DirectX::XMLoadFloat3(&vMaxf3);

	// Geometry Step6
	std::vector<Vertex> vertices(totalVertexCount);
	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		// Texture Step14
		vertices[k].TexC = box.Vertices[i].TexC;

		XMVECTOR P = DirectX::XMLoadFloat3(&vertices[i].Pos);
		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
	}
	BoundingBox bounds;
	DirectX::XMStoreFloat3(&bounds.Center, 0.5f * (vMin + vMax));
	DirectX::XMStoreFloat3(&bounds.Extents, 0.5f * (vMax - vMin));
	boxSubmesh.Bounds = bounds;

	XMFLOAT3 vMinf3Box2(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3Box2(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);
	XMVECTOR vMinBox2 = DirectX::XMLoadFloat3(&vMinf3Box2);
	XMVECTOR vMaxBox2 = DirectX::XMLoadFloat3(&vMaxf3Box2);
	for (size_t i = 0; i < box2.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box2.Vertices[i].Position;
		vertices[k].Normal = box2.Vertices[i].Normal;
		vertices[k].TexC = box2.Vertices[i].TexC;

		XMVECTOR P = DirectX::XMLoadFloat3(&vertices[i].Pos);
		vMinBox2 = XMVectorMin(vMinBox2, P);
		vMaxBox2 = XMVectorMax(vMaxBox2, P);
	}
	BoundingBox boundsBox2;
	DirectX::XMStoreFloat3(&boundsBox2.Center, 0.5f * (vMinBox2 + vMaxBox2));
	DirectX::XMStoreFloat3(&boundsBox2.Extents, 0.5f * (vMaxBox2 - vMinBox2));
	box2Submesh.Bounds = boundsBox2;

	XMFLOAT3 vMinf3Cylinder(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3Cylinder(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);
	XMVECTOR vMinCylinder = DirectX::XMLoadFloat3(&vMinf3Cylinder);
	XMVECTOR vMaxCylinder = DirectX::XMLoadFloat3(&vMaxf3Cylinder);
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;

		XMVECTOR P = DirectX::XMLoadFloat3(&vertices[i].Pos);
		vMinCylinder = XMVectorMin(vMinCylinder, P);
		vMaxCylinder = XMVectorMax(vMaxCylinder, P);
	}
	BoundingBox boundsCylinder;
	DirectX::XMStoreFloat3(&boundsCylinder.Center, 0.5f * (vMinCylinder + vMaxCylinder));
	DirectX::XMStoreFloat3(&boundsCylinder.Extents, 0.5f * (vMaxCylinder - vMinCylinder));
	cylinderSubmesh.Bounds = boundsCylinder;

	XMFLOAT3 vMinf3Cylinder2(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3Cylinder2(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);
	XMVECTOR vMinCylinder2 = DirectX::XMLoadFloat3(&vMinf3Cylinder2);
	XMVECTOR vMaxCylinder2 = DirectX::XMLoadFloat3(&vMaxf3Cylinder2);
	for (size_t i = 0; i < cylinder2.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder2.Vertices[i].Position;
		vertices[k].Normal = cylinder2.Vertices[i].Normal;
		vertices[k].TexC = cylinder2.Vertices[i].TexC;

		XMVECTOR P = DirectX::XMLoadFloat3(&vertices[i].Pos);
		vMinCylinder2 = XMVectorMin(vMinCylinder2, P);
		vMaxCylinder2 = XMVectorMax(vMaxCylinder2, P);
	}
	BoundingBox boundsCylinder2;
	DirectX::XMStoreFloat3(&boundsCylinder2.Center, 0.5f * (vMinCylinder2 + vMaxCylinder2));
	DirectX::XMStoreFloat3(&boundsCylinder2.Extents, 0.5f * (vMaxCylinder2 - vMinCylinder2));
	cylinder2Submesh.Bounds = boundsCylinder2;

	XMFLOAT3 vMinf3Cone(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3Cone(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);
	XMVECTOR vMinCone = DirectX::XMLoadFloat3(&vMinf3Cone);
	XMVECTOR vMaxCone = DirectX::XMLoadFloat3(&vMaxf3Cone);
	for (size_t i = 0; i < cone.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cone.Vertices[i].Position;
		vertices[k].Normal = cone.Vertices[i].Normal;
		vertices[k].TexC = cone.Vertices[i].TexC;

		XMVECTOR P = DirectX::XMLoadFloat3(&vertices[i].Pos);
		vMinCone = XMVectorMin(vMinCone, P);
		vMaxCone = XMVectorMax(vMaxCone, P);
	}
	BoundingBox boundsCone;
	DirectX::XMStoreFloat3(&boundsCone.Center, 0.5f * (vMinCone + vMaxCone));
	DirectX::XMStoreFloat3(&boundsCone.Extents, 0.5f * (vMaxCone - vMinCone));
	coneSubmesh.Bounds = boundsCone;

	XMFLOAT3 vMinf3Wedge(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3Wedge(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);
	XMVECTOR vMinWedge = DirectX::XMLoadFloat3(&vMinf3Wedge);
	XMVECTOR vMaxWedge = DirectX::XMLoadFloat3(&vMaxf3Wedge);
	for (size_t i = 0; i < wedge.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = wedge.Vertices[i].Position;
		vertices[k].Normal = wedge.Vertices[i].Normal;
		vertices[k].TexC = wedge.Vertices[i].TexC;

		XMVECTOR P = DirectX::XMLoadFloat3(&vertices[i].Pos);
		vMinWedge = XMVectorMin(vMinWedge, P);
		vMaxWedge = XMVectorMax(vMaxWedge, P);
	}
	BoundingBox boundsWedge;
	DirectX::XMStoreFloat3(&boundsWedge.Center, 0.5f * (vMinWedge + vMaxWedge));
	DirectX::XMStoreFloat3(&boundsWedge.Extents, 0.5f * (vMaxWedge - vMinWedge));
	wedgeSubmesh.Bounds = boundsWedge;

	XMFLOAT3 vMinf3Diamond(+MathHelper::Infinity, +MathHelper::Infinity, +MathHelper::Infinity);
	XMFLOAT3 vMaxf3Diamond(-MathHelper::Infinity, -MathHelper::Infinity, -MathHelper::Infinity);
	XMVECTOR vMinDiamond = DirectX::XMLoadFloat3(&vMinf3Diamond);
	XMVECTOR vMaxDiamond = DirectX::XMLoadFloat3(&vMaxf3Diamond);
	for (size_t i = 0; i < diamond.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = diamond.Vertices[i].Position;
		vertices[k].Normal = diamond.Vertices[i].Normal;
		vertices[k].TexC = diamond.Vertices[i].TexC;

		XMVECTOR P = DirectX::XMLoadFloat3(&vertices[i].Pos);
		vMinDiamond = XMVectorMin(vMinDiamond, P);
		vMaxDiamond = XMVectorMax(vMaxDiamond, P);
	}
	BoundingBox boundsDiamond;
	DirectX::XMStoreFloat3(&boundsDiamond.Center, 0.5f * (vMinDiamond + vMaxDiamond));
	DirectX::XMStoreFloat3(&boundsDiamond.Extents, 0.5f * (vMaxDiamond - vMinDiamond));
	diamondSubmesh.Bounds = boundsDiamond;

	// Geometry Step7
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(box2.GetIndices16()), std::end(box2.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder2.GetIndices16()), std::end(cylinder2.GetIndices16()));
	indices.insert(indices.end(), std::begin(cone.GetIndices16()), std::end(cone.GetIndices16()));
	indices.insert(indices.end(), std::begin(wedge.GetIndices16()), std::end(wedge.GetIndices16()));
	indices.insert(indices.end(), std::begin(diamond.GetIndices16()), std::end(diamond.GetIndices16()));


	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;


	// Geometry Step8
	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["box2"] = box2Submesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["cylinder2"] = cylinder2Submesh;
	geo->DrawArgs["cone"] = coneSubmesh;
	geo->DrawArgs["wedge"] = wedgeSubmesh;
	geo->DrawArgs["diamond"] = diamondSubmesh;



	mGeometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildSkullGeometry()
{
	std::ifstream fin("Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
		fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

		//step15: find uv mapping for skull model
		XMVECTOR P = DirectX::XMLoadFloat3(&vertices[i].Pos);

		// Project point onto unit sphere and generate spherical texture coordinates.
		XMFLOAT3 spherePos;
		DirectX::XMStoreFloat3(&spherePos, XMVector3Normalize(P));

		float theta = atan2f(spherePos.z, spherePos.x);

		// Put in [0, 2pi].
		if (theta < 0.0f)
			theta += XM_2PI;

		float phi = acosf(spherePos.y);

		float u = theta / (2.0f * XM_PI);
		float v = phi / XM_PI;

		vertices[i].TexC = { u, v };
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	//
	// Pack the indices of all the meshes into one index buffer.
	//

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);

	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "skullGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R32_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["skull"] = submesh;

	mGeometries[geo->Name] = std::move(geo);
}

// Tree Step3
void ShapesApp::BuildTreeSpritesGeometry()
{
	//step4  (step5 and 6 are in TreeSprite.hlsl)
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	static const int treeCount = 45;
	std::array<TreeSpriteVertex, 45> vertices;
	for (UINT i = 0; i < treeCount; ++i)
	{
		float x = MathHelper::RandF(-20.0f, 5.0f);
		float y = 4.1f;
		float z = MathHelper::RandF(-95.0f, -17.0f);


		// Move flags slightly above.
		//y += 8.0f;

		vertices[i].Pos = XMFLOAT3(x, y, z);
		vertices[i].Size = XMFLOAT2(10.0f, 10.0f);
	}


	std::array<std::uint16_t, 45> indices =
	{
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
		21, 22, 23, 24, 25, 26, 27, 28, 29, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeSpriteVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeSpritesGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(TreeSpriteVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["points"] = submesh;

	mGeometries["treeSpritesGeo"] = std::move(geo);
}

void ShapesApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()),
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	//opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	//ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mOpaquePSO)));
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));
	//
	// PSO for transparent objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	

	//
	// PSO for opaque wireframe objects.
	//

	/*D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	opaqueWireframePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));*/

	/*opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));*/

	// Tree Step8
		// PSO for tree sprites
		//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = opaquePsoDesc;
	treeSpritePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteVS"]->GetBufferPointer()),
		mShaders["treeSpriteVS"]->GetBufferSize()
	};
	treeSpritePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteGS"]->GetBufferPointer()),
		mShaders["treeSpriteGS"]->GetBufferSize()
	};
	treeSpritePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpritePS"]->GetBufferPointer()),
		mShaders["treeSpritePS"]->GetBufferSize()
	};

	treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	treeSpritePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };
	treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&mPSOs["treeSprites"])));
	// PSO for highlight objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC highlightPsoDesc = opaquePsoDesc;

	// Change the depth test from < to <= so that if we draw the same triangle twice, it will
	// still pass the depth test.  This is needed because we redraw the picked triangle with a
	// different material to highlight it.  If we do not use <=, the triangle will fail the 
	// depth test the 2nd time we try and draw it.
	highlightPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	highlightPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&highlightPsoDesc, IID_PPV_ARGS(&mPSOs["highlight"])));
}

void ShapesApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size(), (UINT)mMaterials.size(), mWaves->VertexCount()));
	}
}

void ShapesApp::BuildMaterials()
{
	auto one = std::make_unique<Material>();
	one->Name = "one";
	one->MatCBIndex = 0;
	one->DiffuseSrvHeapIndex = 0;
	one->DiffuseAlbedo = XMFLOAT4(Colors::ForestGreen);
	one->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	one->Roughness = 0.1f;

	auto two = std::make_unique<Material>();
	two->Name = "two";
	two->MatCBIndex = 1;
	two->DiffuseSrvHeapIndex = 1;
	two->DiffuseAlbedo = XMFLOAT4(Colors::Black);
	two->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	two->Roughness = 0.3f;

	auto three = std::make_unique<Material>();
	three->Name = "three";
	three->MatCBIndex = 2;
	three->DiffuseSrvHeapIndex = 2;
	three->DiffuseAlbedo = XMFLOAT4(Colors::LightGray);
	three->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	three->Roughness = 0.2f;

	auto four = std::make_unique<Material>();
	four->Name = "four";
	four->MatCBIndex = 3;
	four->DiffuseSrvHeapIndex = 3;
	four->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	four->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	four->Roughness = 0.3f;

	// Tree step30
	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 4;
	treeSprites->DiffuseSrvHeapIndex = 4;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	//Add texture Step6
	auto six = std::make_unique<Material>();
	six->Name = "six";
	six->MatCBIndex = 5;
	six->DiffuseSrvHeapIndex = 5;
	six->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	six->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	six->Roughness = 0.125f;

	auto seven = std::make_unique<Material>();
	seven->Name = "seven";
	seven->MatCBIndex = 6;
	seven->DiffuseSrvHeapIndex = 6;
	seven->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	seven->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	seven->Roughness = 0.125f;

	auto eight = std::make_unique<Material>();
	eight->Name = "eight";
	eight->MatCBIndex = 7;
	eight->DiffuseSrvHeapIndex = 7;
	eight->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	eight->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	eight->Roughness = 0.125f;

	auto nine = std::make_unique<Material>();
	nine->Name = "nine";
	nine->MatCBIndex = 8;
	nine->DiffuseSrvHeapIndex = 8;
	nine->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	nine->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	nine->Roughness = 0.125f;

	auto ten = std::make_unique<Material>();
	ten->Name = "ten";
	ten->MatCBIndex = 9;
	ten->DiffuseSrvHeapIndex = 9;
	ten->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	ten->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	ten->Roughness = 0.125f;


	//Add texture Step7
	mMaterials["one"] = std::move(one);
	mMaterials["two"] = std::move(two);
	mMaterials["three"] = std::move(three);
	mMaterials["four"] = std::move(four);
	mMaterials["treeSprites"] = std::move(treeSprites);
	mMaterials["six"] = std::move(six);
	mMaterials["seven"] = std::move(seven);
	mMaterials["eight"] = std::move(eight);
	mMaterials["nine"] = std::move(nine);
	mMaterials["ten"] = std::move(ten);
}

// Geometry Step9
void ShapesApp::BuildRenderItems()
{
	// Base 1
	auto boxRitem = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem->World, XMMatrixScaling(210.0f, 0.4f, 210.0f) * XMMatrixTranslation(35.0f, 0.4f, -40.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->ObjCBIndex = 0;
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->Mat = mMaterials["eight"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->Bounds = boxRitem->Geo->DrawArgs["box"].Bounds;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem.get());
	mAllRitems.push_back(std::move(boxRitem));


	// Base 2
	auto boxRitem2 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem2->World, XMMatrixScaling(8.5f, 0.4f, 17.5f) * XMMatrixTranslation(-10.0f, 0.0f, 5.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem2->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem2->ObjCBIndex = 1;
	boxRitem2->Geo = mGeometries["shapeGeo"].get();
	boxRitem2->Mat = mMaterials["two"].get();
	boxRitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem2->Bounds = boxRitem2->Geo->DrawArgs["box2"].Bounds;
	boxRitem2->IndexCount = boxRitem2->Geo->DrawArgs["box2"].IndexCount;
	boxRitem2->StartIndexLocation = boxRitem2->Geo->DrawArgs["box2"].StartIndexLocation;
	boxRitem2->BaseVertexLocation = boxRitem2->Geo->DrawArgs["box2"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem2.get());
	mAllRitems.push_back(std::move(boxRitem2));


	// Base 3
	auto boxRitem3 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem3->World, XMMatrixScaling(7.8f, 0.4f, 16.8f) * XMMatrixTranslation(-10.0f, 0.6f, 5.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem3->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem3->ObjCBIndex = 2;
	boxRitem3->Geo = mGeometries["shapeGeo"].get();
	boxRitem3->Mat = mMaterials["three"].get();
	boxRitem3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem3->Bounds = boxRitem3->Geo->DrawArgs["box"].Bounds;
	boxRitem3->IndexCount = boxRitem3->Geo->DrawArgs["box"].IndexCount;
	boxRitem3->StartIndexLocation = boxRitem3->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem3->BaseVertexLocation = boxRitem3->Geo->DrawArgs["box"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem3.get());
	mAllRitems.push_back(std::move(boxRitem3));


	// [8][17] Cylinders
	std::unique_ptr<RenderItem> rItem[8][17];
	UINT objCBIndex = 3;

	XMMATRIX brickTexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);

	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 17; j++)
		{
			rItem[i][j] = std::make_unique<RenderItem>();
			DirectX::XMStoreFloat4x4(&rItem[i][j]->World, XMMatrixScaling(0.5f, 1.0, 0.5f) * XMMatrixTranslation(-13.5f + i, 2.1f, -3.0f + j));
			// Texture
			DirectX::XMStoreFloat4x4(&rItem[i][j]->TexTransform, brickTexTransform);
			rItem[i][j]->ObjCBIndex = objCBIndex++;
			rItem[i][j]->Geo = mGeometries["shapeGeo"].get();
			rItem[i][j]->Mat = mMaterials["three"].get();
			rItem[i][j]->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			rItem[i][j]->Bounds = rItem[i][j]->Geo->DrawArgs["cylinder"].Bounds;
			rItem[i][j]->IndexCount = rItem[i][j]->Geo->DrawArgs["cylinder"].IndexCount;
			rItem[i][j]->StartIndexLocation = rItem[i][j]->Geo->DrawArgs["cylinder"].StartIndexLocation;
			rItem[i][j]->BaseVertexLocation = rItem[i][j]->Geo->DrawArgs["cylinder"].BaseVertexLocation;
			mRitemLayer[(int)RenderLayer::Opaque].push_back(rItem[i][j].get());
			mAllRitems.push_back(std::move(rItem[i][j]));
		}
	}


	// Top Base 1
	auto boxRitem4 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem4->World, XMMatrixScaling(8.0f, 0.4f, 16.8f) * XMMatrixTranslation(-10.0f, 3.8f, 5.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem4->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem4->ObjCBIndex = 139;
	boxRitem4->Geo = mGeometries["shapeGeo"].get();
	boxRitem4->Mat = mMaterials["nine"].get();
	boxRitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem4->Bounds = boxRitem4->Geo->DrawArgs["box"].Bounds;
	boxRitem4->IndexCount = boxRitem4->Geo->DrawArgs["box"].IndexCount;
	boxRitem4->StartIndexLocation = boxRitem4->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem4->BaseVertexLocation = boxRitem4->Geo->DrawArgs["box"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem4.get());
	mAllRitems.push_back(std::move(boxRitem4));


	// Top Base 2 
	auto boxRitem5 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem5->World, XMMatrixScaling(8.0f, 0.8f, 16.8f) * XMMatrixTranslation(-10.0f, 4.4f, 5.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem5->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem5->ObjCBIndex = 140;
	boxRitem5->Geo = mGeometries["shapeGeo"].get();
	boxRitem5->Mat = mMaterials["seven"].get();
	boxRitem5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem5->Bounds = boxRitem5->Geo->DrawArgs["box2"].Bounds;
	boxRitem5->IndexCount = boxRitem5->Geo->DrawArgs["box2"].IndexCount;
	boxRitem5->StartIndexLocation = boxRitem5->Geo->DrawArgs["box2"].StartIndexLocation;
	boxRitem5->BaseVertexLocation = boxRitem5->Geo->DrawArgs["box2"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem5.get());
	mAllRitems.push_back(std::move(boxRitem5));


	// Top Base 3
	auto boxRitem6 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem6->World, XMMatrixScaling(10.0f, 0.2f, 16.8f) * XMMatrixTranslation(-10.0f, 4.9f, 5.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem6->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem6->ObjCBIndex = 141;
	boxRitem6->Geo = mGeometries["shapeGeo"].get();
	boxRitem6->Mat = mMaterials["seven"].get();
	boxRitem6->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem6->Bounds = boxRitem6->Geo->DrawArgs["box"].Bounds;
	boxRitem6->IndexCount = boxRitem6->Geo->DrawArgs["box"].IndexCount;
	boxRitem6->StartIndexLocation = boxRitem6->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem6->BaseVertexLocation = boxRitem6->Geo->DrawArgs["box"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem6.get());
	mAllRitems.push_back(std::move(boxRitem6));


	// Top 45 deg rec 1
	auto boxRitem7 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem7->World, XMMatrixScaling(4.2f, 1.3f, 16.5f) * XMMatrixRotationZ(XMConvertToRadians(20.0f)) * XMMatrixTranslation(-11.8f, 5.0f, 5.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem7->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem7->ObjCBIndex = 142;
	boxRitem7->Geo = mGeometries["shapeGeo"].get();
	boxRitem7->Mat = mMaterials["two"].get();
	boxRitem7->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem7->Bounds = boxRitem7->Geo->DrawArgs["box2"].Bounds;
	boxRitem7->IndexCount = boxRitem7->Geo->DrawArgs["box2"].IndexCount;
	boxRitem7->StartIndexLocation = boxRitem7->Geo->DrawArgs["box2"].StartIndexLocation;
	boxRitem7->BaseVertexLocation = boxRitem7->Geo->DrawArgs["box2"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem7.get());
	mAllRitems.push_back(std::move(boxRitem7));


	// Top 5 deg rec 2
	auto boxRitem8 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem8->World, XMMatrixScaling(4.2f, 1.3f, 16.5f) * XMMatrixRotationZ(XMConvertToRadians(-20.0f)) * XMMatrixTranslation(-8.5f, 5.0f, 5.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem8->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem8->ObjCBIndex = 143;
	boxRitem8->Geo = mGeometries["shapeGeo"].get();
	boxRitem8->Mat = mMaterials["two"].get();
	boxRitem8->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem8->Bounds = boxRitem8->Geo->DrawArgs["box2"].Bounds;
	boxRitem8->IndexCount = boxRitem8->Geo->DrawArgs["box2"].IndexCount;
	boxRitem8->StartIndexLocation = boxRitem8->Geo->DrawArgs["box2"].StartIndexLocation;
	boxRitem8->BaseVertexLocation = boxRitem8->Geo->DrawArgs["box2"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem8.get());
	mAllRitems.push_back(std::move(boxRitem8));


	// Top 3
	auto boxRitem9 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem9->World, XMMatrixScaling(4.3f, 0.2f, 16.8f) * XMMatrixRotationZ(XMConvertToRadians(-20.0f)) * XMMatrixTranslation(-8.0f, 5.6f, 5.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem9->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem9->ObjCBIndex = 144;
	boxRitem9->Geo = mGeometries["shapeGeo"].get();
	boxRitem9->Mat = mMaterials["four"].get();
	boxRitem9->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem9->Bounds = boxRitem9->Geo->DrawArgs["box"].Bounds;
	boxRitem9->IndexCount = boxRitem9->Geo->DrawArgs["box"].IndexCount;
	boxRitem9->StartIndexLocation = boxRitem9->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem9->BaseVertexLocation = boxRitem9->Geo->DrawArgs["box"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem9.get());
	mAllRitems.push_back(std::move(boxRitem9));

	// Top 4
	auto boxRitem10 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem10->World, XMMatrixScaling(4.3f, 0.2f, 16.8f) * XMMatrixRotationZ(XMConvertToRadians(20.0f)) * XMMatrixTranslation(-12.0f, 5.6f, 5.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem10->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem10->ObjCBIndex = 145;
	boxRitem10->Geo = mGeometries["shapeGeo"].get();
	boxRitem10->Mat = mMaterials["six"].get();
	boxRitem10->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem10->Bounds = boxRitem10->Geo->DrawArgs["box"].Bounds;
	boxRitem10->IndexCount = boxRitem10->Geo->DrawArgs["box"].IndexCount;
	boxRitem10->StartIndexLocation = boxRitem10->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem10->BaseVertexLocation = boxRitem10->Geo->DrawArgs["box"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem10.get());
	mAllRitems.push_back(std::move(boxRitem10));


	// Top left dots
	std::unique_ptr<RenderItem> rItem2[4][17];
	UINT objCBIndex2 = 146;

	XMMATRIX brickTex3Transform = XMMatrixScaling(1.0f, 1.0f, 1.0f);

	// Define rotation angle and axis
	float rotationAngle = 20.0f; // Example rotation angle in degrees
	XMVECTOR rotationAxis = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // Rotate around Y-axis
	// Create rotation matrix
	XMMATRIX rotationMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(rotationAngle));

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 17; j++)
		{
			rItem2[i][j] = std::make_unique<RenderItem>();
			DirectX::XMStoreFloat4x4(&rItem2[i][j]->World, XMMatrixScaling(0.5f, 0.1f, 0.5f) * XMMatrixRotationZ(XMConvertToRadians(2.0f)) * XMMatrixTranslation(-11.0f + i, 9.5f, -3.0f + j) * rotationMatrix);
			// Texture
			DirectX::XMStoreFloat4x4(&rItem2[i][j]->TexTransform, brickTex3Transform);
			rItem2[i][j]->ObjCBIndex = objCBIndex2++;
			rItem2[i][j]->Geo = mGeometries["shapeGeo"].get();
			rItem2[i][j]->Mat = mMaterials["one"].get();
			rItem2[i][j]->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			rItem2[i][j]->Bounds = rItem2[i][j]->Geo->DrawArgs["cylinder2"].Bounds;
			rItem2[i][j]->IndexCount = rItem2[i][j]->Geo->DrawArgs["cylinder2"].IndexCount;
			rItem2[i][j]->StartIndexLocation = rItem2[i][j]->Geo->DrawArgs["cylinder2"].StartIndexLocation;
			rItem2[i][j]->BaseVertexLocation = rItem2[i][j]->Geo->DrawArgs["cylinder2"].BaseVertexLocation;
			mRitemLayer[(int)RenderLayer::Opaque].push_back(rItem2[i][j].get());
			mAllRitems.push_back(std::move(rItem2[i][j]));
		}
	}


	std::unique_ptr<RenderItem> rItem3[4][17];
	UINT objCBIndex3 = 214;

	XMMATRIX brickTex4Transform = XMMatrixScaling(1.0f, 1.0f, 1.0f);

	// Define rotation angle and axis
	float rotationAngle2 = -20.0f; // Example rotation angle in degrees
	XMVECTOR rotationAxis2 = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // Rotate around Y-axis
	// Create rotation matrix
	XMMATRIX rotationMatrix2 = XMMatrixRotationAxis(rotationAxis2, XMConvertToRadians(rotationAngle2));

	for (int i = 0; i < 4; i++)
	{
		for (int j = 0; j < 17; j++)
		{
			rItem3[i][j] = std::make_unique<RenderItem>();
			DirectX::XMStoreFloat4x4(&rItem3[i][j]->World, XMMatrixScaling(0.5f, 0.1f, 0.5f) * XMMatrixRotationZ(XMConvertToRadians(-2.0f)) * XMMatrixTranslation(-11.0f + i, 2.7f, -3.0f + j) * rotationMatrix2);
			// Texture
			DirectX::XMStoreFloat4x4(&rItem3[i][j]->TexTransform, brickTex4Transform);
			rItem3[i][j]->ObjCBIndex = objCBIndex3++;
			rItem3[i][j]->Geo = mGeometries["shapeGeo"].get();
			rItem3[i][j]->Mat = mMaterials["two"].get();
			rItem3[i][j]->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			rItem3[i][j]->Bounds = rItem3[i][j]->Geo->DrawArgs["cylinder2"].Bounds;
			rItem3[i][j]->IndexCount = rItem3[i][j]->Geo->DrawArgs["cylinder2"].IndexCount;
			rItem3[i][j]->StartIndexLocation = rItem3[i][j]->Geo->DrawArgs["cylinder2"].StartIndexLocation;
			rItem3[i][j]->BaseVertexLocation = rItem3[i][j]->Geo->DrawArgs["cylinder2"].BaseVertexLocation;
			mRitemLayer[(int)RenderLayer::Opaque].push_back(rItem3[i][j].get());
			mAllRitems.push_back(std::move(rItem3[i][j]));
		}
	}


	// Cylinder Rod
	std::unique_ptr<RenderItem> rItem4;
	UINT objCBIndex4 = 282;
	// Define rotation angle and axis
	float rotationAngle3 = 270.0f; // Example rotation angle in degrees
	XMVECTOR rotationAxis3 = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // Rotate around Y-axis
	// Create rotation matrix
	XMMATRIX rotationMatrix3 = XMMatrixRotationAxis(rotationAxis3, XMConvertToRadians(rotationAngle3));

	rItem4 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&rItem4->World, XMMatrixScaling(0.0f, 0.0f, 0.0f) * XMMatrixRotationX(XMConvertToRadians(90.0f)) * XMMatrixTranslation(-12.5f, 0.0f, 6.0f) * rotationMatrix3);
	// Texture
	DirectX::XMStoreFloat4x4(&rItem4->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	rItem4->ObjCBIndex = objCBIndex4;
	rItem4->Geo = mGeometries["shapeGeo"].get();
	rItem4->Mat = mMaterials["one"].get();
	rItem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rItem4->Bounds = rItem4->Geo->DrawArgs["cylinder"].Bounds;
	rItem4->IndexCount = rItem4->Geo->DrawArgs["cylinder"].IndexCount;
	rItem4->StartIndexLocation = rItem4->Geo->DrawArgs["cylinder"].StartIndexLocation;
	rItem4->BaseVertexLocation = rItem4->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(rItem4.get());
	mAllRitems.push_back(std::move(rItem4));


	// Tree step12
	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	treeSpritesRitem->ObjCBIndex = 283;
	treeSpritesRitem->Mat = mMaterials["ten"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
	treeSpritesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());
	mAllRitems.push_back(std::move(treeSpritesRitem));

	//Trees base
	auto boxRitem12 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&boxRitem12->World, XMMatrixScaling(0.0f, 0.0f, 0.0f) * XMMatrixTranslation(-10.0f, 0.0f, -10.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&boxRitem12->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem12->ObjCBIndex = 284;
	boxRitem12->Geo = mGeometries["shapeGeo"].get();
	boxRitem12->Mat = mMaterials["three"].get();
	boxRitem12->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem12->Bounds = boxRitem12->Geo->DrawArgs["box"].Bounds;
	boxRitem12->IndexCount = boxRitem12->Geo->DrawArgs["box"].IndexCount;
	boxRitem12->StartIndexLocation = boxRitem12->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem12->BaseVertexLocation = boxRitem12->Geo->DrawArgs["box"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(boxRitem12.get());
	mAllRitems.push_back(std::move(boxRitem12));

	// L forward Pillar 
	std::unique_ptr<RenderItem> rItem5;
	rItem5 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&rItem5->World, XMMatrixScaling(2.5f, 4.0f, 2.5f) * XMMatrixTranslation(-21.0f, 5.5f, -10.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&rItem5->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	rItem5->ObjCBIndex = 285;
	rItem5->Geo = mGeometries["shapeGeo"].get();
	rItem5->Mat = mMaterials["nine"].get();
	rItem5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rItem5->Bounds = rItem5->Geo->DrawArgs["cylinder"].Bounds;
	rItem5->IndexCount = rItem5->Geo->DrawArgs["cylinder"].IndexCount;
	rItem5->StartIndexLocation = rItem5->Geo->DrawArgs["cylinder"].StartIndexLocation;
	rItem5->BaseVertexLocation = rItem5->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(rItem5.get());
	mAllRitems.push_back(std::move(rItem5));

	// R forward Pillar
	std::unique_ptr<RenderItem> rItem6;
	rItem6 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&rItem6->World, XMMatrixScaling(2.5f, 4.0f, 2.5f) * XMMatrixTranslation(1.0f, 5.5f, -10.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&rItem6->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	rItem6->ObjCBIndex = 286;
	rItem6->Geo = mGeometries["shapeGeo"].get();
	rItem6->Mat = mMaterials["nine"].get();
	rItem6->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rItem6->Bounds = rItem6->Geo->DrawArgs["cylinder"].Bounds;
	rItem6->IndexCount = rItem6->Geo->DrawArgs["cylinder"].IndexCount;
	rItem6->StartIndexLocation = rItem6->Geo->DrawArgs["cylinder"].StartIndexLocation;
	rItem6->BaseVertexLocation = rItem6->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(rItem6.get());
	mAllRitems.push_back(std::move(rItem6));

	// L Back Pillar
	std::unique_ptr<RenderItem> rItem7;
	rItem7 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&rItem7->World, XMMatrixScaling(2.5f, 4.0f, 2.5f) * XMMatrixTranslation(-21.0f, 5.5f, 18.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&rItem7->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	rItem7->ObjCBIndex = 287;
	rItem7->Geo = mGeometries["shapeGeo"].get();
	rItem7->Mat = mMaterials["nine"].get();
	rItem7->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rItem7->Bounds = rItem7->Geo->DrawArgs["cylinder"].Bounds;
	rItem7->IndexCount = rItem7->Geo->DrawArgs["cylinder"].IndexCount;
	rItem7->StartIndexLocation = rItem7->Geo->DrawArgs["cylinder"].StartIndexLocation;
	rItem7->BaseVertexLocation = rItem7->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(rItem7.get());
	mAllRitems.push_back(std::move(rItem7));

	// R Back Pillar
	std::unique_ptr<RenderItem> rItem8;
	rItem8 = std::make_unique<RenderItem>();
	DirectX::XMStoreFloat4x4(&rItem8->World, XMMatrixScaling(2.5f, 4.0f, 2.5f) * XMMatrixTranslation(1.0f, 5.5f, 18.0f));
	// Texture
	DirectX::XMStoreFloat4x4(&rItem8->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	rItem8->ObjCBIndex = 288;
	rItem8->Geo = mGeometries["shapeGeo"].get();
	rItem8->Mat = mMaterials["nine"].get();
	rItem8->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	rItem8->Bounds = rItem8->Geo->DrawArgs["cylinder"].Bounds;
	rItem8->IndexCount = rItem8->Geo->DrawArgs["cylinder"].IndexCount;
	rItem8->StartIndexLocation = rItem8->Geo->DrawArgs["cylinder"].StartIndexLocation;
	rItem8->BaseVertexLocation = rItem8->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(rItem8.get());
	mAllRitems.push_back(std::move(rItem8));



	// Water nw
	auto wavesRitem = std::make_unique<RenderItem>();
	wavesRitem->World = MathHelper::Identity4x4();
	DirectX::XMStoreFloat4x4(&wavesRitem->TexTransform, XMMatrixScaling(5.0f, 7.0f, 1.0f));
	wavesRitem->ObjCBIndex = 289;
	wavesRitem->Mat = mMaterials["eight"].get();
	wavesRitem->Geo = mGeometries["waterGeo"].get();
	wavesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wavesRitem->IndexCount = wavesRitem->Geo->DrawArgs["grid"].IndexCount;
	wavesRitem->StartIndexLocation = wavesRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	wavesRitem->BaseVertexLocation = wavesRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mWavesRitem = wavesRitem.get();
	mRitemLayer[(int)RenderLayer::Transparent].push_back(wavesRitem.get());
	mAllRitems.push_back(std::move(wavesRitem));

	//// land
	//auto gridRitem = std::make_unique<RenderItem>();
	//gridRitem->World = MathHelper::Identity4x4();
	//XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(5.0f, 7.0f, 7.0f));
	//gridRitem->ObjCBIndex = 290;
	//gridRitem->Mat = mMaterials["three"].get();
	//gridRitem->Geo = mGeometries["landGeo"].get();
	//gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	//gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	//gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	//gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	//mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	//mAllRitems.push_back(std::move(gridRitem));

	objCBIndex = 290; //1
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(85.0f, 4.25f, -95.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //1
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(75.0f, 4.25f, -95.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //2
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(85.0f, 4.25f, -85.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //3
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(80.0f, 4.25f, -80.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //4
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(70.0f, 4.25f, -90.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //5
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(65.0f, 4.25f, -85.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //6
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(65.0f, 4.25f, -95.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //7
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(55.0f, 4.25f, -95.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //8
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(45.0f, 4.25f, -95.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //9
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(35.0f, 4.25f, -95.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //10
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(25.0f, 4.25f, -95.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //11
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(55.0f, 4.25f, -75.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //12
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(45.0f, 4.25f, -85.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //13
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(35.0f, 4.25f, -85.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //14
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(25.0f, 4.25f, -85.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //15
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(45.0f, 4.25f, -75.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //16
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(50.0f, 4.25f, -80.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //17
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(15.0f, 4.25f, -95.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //18
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(20.0f, 4.25f, -90.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //19
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(10.0f, 4.25f, -90.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //20
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(10.0f, 4.25f, -80.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //21
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(90.0f, 4.25f, -80.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //22
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(90.0f, 4.25f, -70.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //23
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(90.0f, 4.25f, -60.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //24
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(90.0f, 4.25f, -50.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //25
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(90.0f, 4.25f, -40.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //26
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(90.0f, 4.25f, -30.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //27
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(90.0f, 4.25f, -20.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //28
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(70.0f, 4.25f, -70.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //29
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(30.0f, 4.25f, -70.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //30
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(10.0f, 4.25f, -70.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //31
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(10.0f, 4.25f, -60.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //32
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(10.0f, 4.25f, -50.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //33
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(10.0f, 4.25f, -40.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //34
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(10.0f, 4.25f, -30.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //35
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(10.0f, 4.25f, -20.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //36
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(75.0f, 4.25f, -65.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //37
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(65.0f, 4.25f, -65.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //38
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(55.0f, 4.25f, -65.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //39
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(35.0f, 4.25f, -65.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //40
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(60.0f, 4.25f, -60.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //41
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(40.0f, 4.25f, -60.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //42
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(20.0f, 4.25f, -60.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //43
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(20.0f, 4.25f, -50.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //44
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(20.0f, 4.25f, -40.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //45
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(20.0f, 4.25f, -30.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //46
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(30.0f, 4.25f, -50.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //47
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(30.0f, 4.25f, -40.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //48
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(85.0f, 4.25f, -55.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //49
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(75.0f, 4.25f, -55.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //50
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(65.0f, 4.25f, -55.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //51
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(45.0f, 4.25f, -55.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //52
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(25.0f, 4.25f, -55.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //53
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(50.0f, 4.25f, -50.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //54
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(40.0f, 4.25f, -40.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //55
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(40.0f, 4.25f, -30.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //56
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(80.0f, 4.25f, -40.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //57
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(80.0f, 4.25f, -30.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //58
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(60.0f, 4.25f, -30.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //59
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(70.0f, 4.25f, -20.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //60
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(50.0f, 4.25f, -20.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //61
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(75.0f, 4.25f, -45.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //62
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(55.0f, 4.25f, -45.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //63
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(65.0f, 4.25f, -35.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //64
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(55.0f, 4.25f, -35.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //65
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(45.0f, 4.25f, -35.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //66
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(75.0f, 4.25f, -25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //67
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(65.0f, 4.25f, -25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //68
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(35.0f, 4.25f, -25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //69
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(15.0f, 4.25f, -25.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //70
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(85.0f, 4.25f, -15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //71
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(75.0f, 4.25f, -15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //72
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(65.0f, 4.25f, -15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //73
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(55.0f, 4.25f, -15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //74
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(45.0f, 4.25f, -15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //75
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(35.0f, 4.25f, -15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //76
	//CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(25.0f, 4.25f, -15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	//objCBIndex++; //77
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(15.0f, 4.25f, -15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //78
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(25.0f, 4.25f, -75.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //79
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(15.0f, 4.25f, -75.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //80
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(90.0f, 4.25f, -10.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //81
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(90.0f, 4.25f, 0.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //82
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(90.0f, 4.25f, 10.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //83
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(80.0f, 4.25f, 0.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //84
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(70.0f, 4.25f, 0.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //85
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(70.0f, 4.25f, -10.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //85
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(40.0f, 4.25f, 0.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //86
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(10.0f, 4.25f, 0.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //87
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(10.0f, 4.25f, -10.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //88
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(20.0f, 4.25f, -10.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //89
	CreateItem("box", XMMatrixScaling(1.0f, 8.0f, 10.0f), XMMatrixTranslation(50.0f, 4.25f, 10.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //90
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(85.0f, 4.25f, -5.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //91
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(65.0f, 4.25f, -5.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //92
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(45.0f, 4.25f, -5.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //93
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(25.0f, 4.25f, -5.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //94
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(65.0f, 4.25f, 5.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //95
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(35.0f, 4.25f, 5.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //96
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(25.0f, 4.25f, 5.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //97
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(15.0f, 4.25f, 5.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //98
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(85.0f, 4.25f, 15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //99
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(75.0f, 4.25f, 15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //100
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(65.0f, 4.25f, 15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //101
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(55.0f, 4.25f, 15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //102
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(45.0f, 4.25f, 15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //103
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(35.0f, 4.25f, 15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //104
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(25.0f, 4.25f, 15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	objCBIndex++; //105
	CreateItem("box", XMMatrixScaling(10.0f, 8.0f, 1.0f), XMMatrixTranslation(15.0f, 4.25f, 15.0f), XMMatrixRotationRollPitchYaw(0.f, 0.f, 0.f), objCBIndex, "four");//front left wall
	//objCBIndex++; //106

	auto pickedRitem = std::make_unique<RenderItem>();
	pickedRitem->World = MathHelper::Identity4x4();
	pickedRitem->TexTransform = MathHelper::Identity4x4();
	pickedRitem->ObjCBIndex = objCBIndex++;
	pickedRitem->Mat = mMaterials["one"].get();
	pickedRitem->Geo = mGeometries["shapeGeo"].get();
	pickedRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// Picked triangle is not visible until one is picked.
	pickedRitem->Visible = false;

	// DrawCall parameters are filled out when a triangle is picked.
	pickedRitem->IndexCount = 0;
	pickedRitem->StartIndexLocation = 0;
	pickedRitem->BaseVertexLocation = 0;
	mPickedRitem = pickedRitem.get();
	mRitemLayer[(int)RenderLayer::Highlight].push_back(pickedRitem.get());
	mAllRitems.push_back(std::move(pickedRitem));

	// All the render items are opaque.
	// Tree Step28
	/*for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());*/
}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Textures Step18
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		// Textures Step19
		cmdList->SetGraphicsRootDescriptorTable(0, tex);
		cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}

// Texture Step20
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> ShapesApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

float ShapesApp::GetHillsHeight(float x, float z)const
{
	return 0.1f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 ShapesApp::GetHillsNormal(float x, float z)const
{
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize(DirectX::XMLoadFloat3(&n));
	DirectX::XMStoreFloat3(&n, unitNormal);

	return n;
}

void ShapesApp::MazeCollision(int sx, int sy)
{
	XMFLOAT4X4 P = mCamera.GetProj4x4f();   ////XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);


	// Compute picking ray in view space.
	//we can shoot our picking ray through the point (v'x, v'y, 1) instead. Note that this yields the same picking ray as the one shot through the point (xv, yv, d) on the projection window.

	float vx = (+2.0f * sx / mClientWidth - 1.0f) / P(0, 0);
	float vy = (-2.0f * sy / mClientHeight + 1.0f) / P(1, 1);

	// Ray definition in view space.
	XMVECTOR rayOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	XMVECTOR rayDir = XMVectorSet(vx, vy, 1.0f, 0.0f);

	XMMATRIX V = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(V), V);

	//Generally, each object in the scene has its own local space. Therefore, the ray must be transformed to the local space of each scene object to do the intersection test.
	//The following code shows how the picking ray is transformed from view space to the local space of an object :


	// Assume nothing is picked to start, so the picked render-item is invisible.
	mPickedRitem->Visible = false;

	// Check if we picked an opaque render item.  A real app might keep a separate "picking list"
	// of objects that can be selected.   
	for (auto ri : mRitemLayer[(int)RenderLayer::Opaque])
	{
		auto geo = ri->Geo;

		// Skip invisible render-items.
		if (ri->Visible == false)
			continue;

		XMMATRIX W = DirectX::XMLoadFloat4x4(&ri->World);

		//If W is the world matrix of an object, the matrix W?1 transforms geometry from world space to the local space of the object.
		XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(W), W);

		//Because the view matrix transforms geometry from world space to view space, the inverse of the view matrix transforms geometry from view space to world space. 
		XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);

		//Generally, each object in the scene has its own local space. Therefore, the ray must be transformed to the local space of each scene object to do the intersection test.
		rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
		rayDir = XMVector3TransformNormal(rayDir, toLocal);

		// Make the ray direction unit length for the intersection tests.
		rayDir = XMVector3Normalize(rayDir);

		// If we hit the bounding box of the Mesh, then we might have picked a Mesh triangle,
		// so do the ray/triangle tests.
		//
		// If we did not hit the bounding box, then it is impossible that we hit 
		// the Mesh, so do not waste effort doing ray/triangle tests.
		float tmin = 0.0f;
		if (ri->Bounds.Intersects(rayOrigin, rayDir, tmin))
		{
			// NOTE: For the demo, we know what to cast the vertex/index data to.  If we were mixing
			// formats, some metadata would be needed to figure out what to cast it to.
			auto vertices = static_cast<Vertex*>(geo->VertexBufferCPU->GetBufferPointer());
			auto indices = static_cast<std::uint32_t*>(geo->IndexBufferCPU->GetBufferPointer());
			UINT triCount = ri->IndexCount / 3;

			// Find the nearest ray/triangle intersection.
			tmin = MathHelper::Infinity;
			for (UINT i = 0; i < triCount; ++i)
			{
				// Indices for this triangle.
				UINT i0 = indices[i * 3 + 0];
				UINT i1 = indices[i * 3 + 1];
				UINT i2 = indices[i * 3 + 2];


				//TODO: figure out why it breaks here
				// Vertices for this triangle.
				XMVECTOR v0 = DirectX::XMLoadFloat3(&vertices[i0].Pos);
				XMVECTOR v1 = DirectX::XMLoadFloat3(&vertices[i1].Pos);
				XMVECTOR v2 = DirectX::XMLoadFloat3(&vertices[i2].Pos);

				// We have to iterate over all the triangles in order to find the nearest intersection.
				float t = 0.0f;
				if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, t))
				{
					if (t < tmin)
					{
						// This is the new nearest picked triangle.
						tmin = t;
						UINT pickedTriangle = i;

						mPickedRitem->Visible = true;
						mPickedRitem->IndexCount = 3;
						mPickedRitem->BaseVertexLocation = 0;

						// Picked render item needs same world matrix as object picked.
						mPickedRitem->World = ri->World;
						mPickedRitem->NumFramesDirty = gNumFrameResources;

						// Offset to the picked triangle in the mesh index buffer.
						mPickedRitem->StartIndexLocation = 3 * pickedTriangle;
					}
				}
			}
		}
		if (tmin <= 2.0f)
		{
			bStopForwardMovement = true;
		}
		else if (tmin >= 2.0f)
		{
			bStopForwardMovement = false;
		}
	}
}

