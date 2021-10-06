/*
 * Copyright 2021 Ondøej Sabela
 */

#include <bx/uint32_t.h>
 // Library for controlling aplication flow (init() and update() methods)
#include "entry/entry.h"
#include "imgui/imgui.h"
#include "gl_utils.h"

 // Utils for working with resources (shaders, meshes..)
#include "ScreenSpaceQuad.h"
#include "SceneObjects.h"

#define HANDLE_OF_DEFALUT_WINDOW entry::WindowHandle{ 0 }

const Material _materialBuffer[] = {
	{// Only one material
		{1, 0.5, 1 / 39, 1},// Orange albedo
		{0.5},// Half Roughness
		{0.5},// Half metalness
		{0,0,0} // No emission
	}
};

const Sphere _objectBuffer[] = {
	{
		{0, 0, -15}, //Position
		{5} //Radius
	}
};

const DirectionalLight _directionalLightBuffer[] = {
	{
		{0.5, 0.5, 0.5},// direction
		{1,1,1}// color
	}
};

namespace RealisticAtmosphere
{
	class RealisticAtmosphere : public entry::AppI // Entry point for our application
	{
	public:
		RealisticAtmosphere(const char* name, const char* description, const char* projectUrl)
			: entry::AppI(name, description, projectUrl) {}

		uint32_t _windowWidth = 1024;
		uint32_t _windowHeight = 600;
		uint32_t _debugFlags;
		uint32_t _resetFlags;
		entry::MouseState _mouseState;

		bgfx::TextureHandle _raytracerOutputTexture; /**< Used only when using compute-shader variant */
		bgfx::UniformHandle _raytracerOutputSampler;

		bgfx::ProgramHandle _computeShaderProgram;
		bgfx::ProgramHandle _displayingShaderProgram; /**< This program displays output from compute-shader raytracer */

		bgfx::ProgramHandle _raytracerShaderProgram; /**< This program includes full raytracer as fragment shader */

		ScreenSpaceQuad _screenSpaceQuad;/**< Output of raytracer (both the compute-shader variant and fragment-shader variant) */

		bool _useComputeShader = true;
		bool _debugNormals = false;

		bgfx::DynamicIndexBufferHandle _objectBufferHandle;
		bgfx::DynamicIndexBufferHandle _materialBufferHandle;
		bgfx::DynamicIndexBufferHandle _directionalLightBufferHandle;
		bgfx::ShaderHandle _computeShaderHandle;

		bgfx::UniformHandle _cameraHandle;
#if _DEBUG
		bgfx::UniformHandle _debugAttributesHandle;
		vec4 _debugAttributesResult = vec4(0,0,0,0);
#endif
		float _tanFovY;
		float _tanFovX;
		float _fovY = 45;

		// The Entry library will call this method after setting up window manager
		void init(int32_t argc, const char* const* argv, uint32_t width, uint32_t height) override
		{
			entry::setWindowFlags(HANDLE_OF_DEFALUT_WINDOW, ENTRY_WINDOW_FLAG_ASPECT_RATIO, false);
			entry::setWindowSize(HANDLE_OF_DEFALUT_WINDOW, 1024, 600);

			// Supply program arguments for setting graphics backend to BGFX.
			Args args(argc, argv);

			_debugFlags = BGFX_DEBUG_TEXT;
			_resetFlags = BGFX_RESET_VSYNC;

			//Check for OpenGL backend
			if (args.m_type != bgfx::RendererType::Count && args.m_type != bgfx::RendererType::OpenGL)
			{
				bx::debugPrintf("Only OpenGL version is implemented yet");
			}

			// Initialize BFGX with supplied arguments
			bgfx::Init init;
			init.type = bgfx::RendererType::OpenGL;
			init.vendorId = args.m_pciId;
			init.resolution.width = _windowWidth;
			init.resolution.height = _windowHeight;
			init.resolution.reset = _resetFlags;
			bgfx::init(init);

			// Enable debug text.
			bgfx::setDebug(_debugFlags);
			_useComputeShader = !!(bgfx::getCaps()->supported & BGFX_CAPS_COMPUTE);

			// Set view 0 clear state.
			bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH);

			//
			// Setup Resources
			//

			_screenSpaceQuad = ScreenSpaceQuad((float)_windowWidth, (float)_windowHeight);//Init internal vertex layout
			_cameraHandle = bgfx::createUniform("Camera", bgfx::UniformType::Vec4, 4);//It is an array of 4 vec4
			_raytracerOutputSampler = bgfx::createUniform("computeShaderOutput", bgfx::UniformType::Sampler);
			_debugAttributesHandle = bgfx::createUniform("debugAttributes", bgfx::UniformType::Vec4);

			_displayingShaderProgram = loadProgram("rt_display.vert", "rt_display.frag");
			_raytracerShaderProgram = loadProgram("normal_render.vert", "normal_render.frag");
			_computeShaderHandle = loadShader("compute_render.comp");
			_computeShaderProgram = bgfx::createProgram(_computeShaderHandle);

			// "general" buffers are called "dynamic index" buffers by BGFX. C'est la vie.
			_materialBufferHandle = bgfx::createDynamicIndexBuffer(sizeof(_materialBuffer), BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_ALLOW_RESIZE);
			_directionalLightBufferHandle = bgfx::createDynamicIndexBuffer(sizeof(_directionalLightBuffer), BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_ALLOW_RESIZE);
			_objectBufferHandle = bgfx::createDynamicIndexBuffer(sizeof(_objectBuffer), BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_ALLOW_RESIZE);

			_raytracerOutputTexture = bgfx::createTexture2D(
				uint16_t(_windowWidth)
				, uint16_t(_windowHeight)
				, false
				, 1
				, bgfx::TextureFormat::RGBA8
				, BGFX_TEXTURE_COMPUTE_WRITE);


			// Create Immediate GUI graphics context
			imguiCreate();
		}

		virtual int shutdown() override
		{
			// Destroy Immediate GUI graphics context
			imguiDestroy();

			//Destroy resources
			bgfx::destroy(_raytracerOutputTexture);
			bgfx::destroy(_raytracerOutputSampler);
			bgfx::destroy(_computeShaderHandle);
			bgfx::destroy(_computeShaderProgram);
			bgfx::destroy(_displayingShaderProgram);
			bgfx::destroy(_raytracerShaderProgram);
			bgfx::destroy(_materialBufferHandle);
			bgfx::destroy(_directionalLightBufferHandle);
			bgfx::destroy(_objectBufferHandle);
			bgfx::destroy(_cameraHandle);
			bgfx::destroy(_debugAttributesHandle);

			_screenSpaceQuad.destroy();

			// Shutdown bgfx.
			bgfx::shutdown();

			return 0;
		}

		// Called every frame
		bool update() override
		{
			// Polling about mouse, keyboard and window events
			// Returns true when the window is up to close itself
			if (!entry::processEvents(_windowWidth, _windowHeight, _debugFlags, _resetFlags, &_mouseState))
			{
				//
				// GUI Actions
				// 

				// Supply mouse events to GUI library
				imguiBeginFrame(_mouseState.m_mx
					, _mouseState.m_my
					, (_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
					| (_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
					| (_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0)
					, _mouseState.m_mz
					, uint16_t(_windowWidth)
					, uint16_t(_windowHeight)
				);

				// Displays/Updates an innner dialog with debug and profiler information
				showDebugDialog(this);
				drawSettingsDialog();

				imguiEndFrame();

				//
				// Graphics actions
				// 

				drawDebugInfo();

				viewportActions();

				updateBuffers();

#if _DEBUG
				updateDebugUniforms();
#endif

				if (_useComputeShader)
				{
					computeShaderRaytracer();
				}
				else
				{
					fragmentShaderRaytracer();
				}

				// Advance to next frame. Rendering thread will be kicked to
				// process submitted rendering primitives.
				bgfx::frame();

				return true;
			}
			// update() should return false when we want the application to exit
			return false;
		}
		void drawDebugInfo()
		{
			bgfx::dbgTextPrintf(0, 1, 0x0f, "Color can be changed with ANSI \x1b[9;me\x1b[10;ms\x1b[11;mc\x1b[12;ma\x1b[13;mp\x1b[14;me\x1b[0m code too.");

			bgfx::dbgTextPrintf(80, 1, 0x0f, "\x1b[;0m    \x1b[;1m    \x1b[; 2m    \x1b[; 3m    \x1b[; 4m    \x1b[; 5m    \x1b[; 6m    \x1b[; 7m    \x1b[0m");
			bgfx::dbgTextPrintf(80, 2, 0x0f, "\x1b[;8m    \x1b[;9m    \x1b[;10m    \x1b[;11m    \x1b[;12m    \x1b[;13m    \x1b[;14m    \x1b[;15m    \x1b[0m");

			const bgfx::Stats* stats = bgfx::getStats();
			bgfx::dbgTextPrintf(0, 2, 0x0f, "Backbuffer %dW x %dH in pixels, debug text %dW x %dH in characters."
				, stats->width
				, stats->height
				, stats->textWidth
				, stats->textHeight
			);
		}

		void updateDebugUniforms()
		{
			_debugAttributesResult = vec4(_debugNormals ? 1 : 0, 0, 0, 0);
			bgfx::setUniform(_debugAttributesHandle, &_debugAttributesResult);
		}

		void fragmentShaderRaytracer()
		{
			_screenSpaceQuad.draw();//Draw screen space quad with our shader program
			bgfx::setState(BGFX_STATE_DEFAULT);
			bgfx::submit(0, _raytracerShaderProgram);
		}
		void computeShaderRaytracer()
		{
			bgfx::setImage(0, _raytracerOutputTexture, 0, bgfx::Access::Write);
			bgfx::dispatch(0, _computeShaderProgram, bx::ceil(_windowWidth / 16.0f), bx::ceil(_windowHeight / 16.0f));

			/** We cannot do a blit into backbuffer - not implemented in BGFX
			  * e.g. bgfx::blit(0, BGFX_INVALID_HANDLE, 0, 0, _raytracerOutputTexture, 0, 0, _windowWidth, _windowHeight);
			  */

			bgfx::setTexture(0, _raytracerOutputSampler, _raytracerOutputTexture);
			_screenSpaceQuad.draw();//Draw screen space quad with our shader program

			bgfx::setState(BGFX_STATE_DEFAULT);
			bgfx::submit(0, _displayingShaderProgram);
			bgfx::touch(0);
		}

		void updateBuffers()
		{
			bgfx::update(_objectBufferHandle, 0, bgfx::makeRef((void*)_objectBuffer, sizeof(_objectBuffer)));
			bgfx::update(_materialBufferHandle, 0, bgfx::makeRef((void*)_materialBuffer, sizeof(_materialBuffer)));
			bgfx::update(_directionalLightBufferHandle, 0, bgfx::makeRef((void*)_directionalLightBuffer, sizeof(_directionalLightBuffer)));

			bgfx::setBuffer(1, _objectBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(2, _materialBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(3, _directionalLightBufferHandle, bgfx::Access::Read);
		}

		void viewportActions()
		{
			float proj[16];
			bx::mtxOrtho(proj, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);

			// Set view 0 default viewport.
			bgfx::setViewTransform(0, NULL, proj);
			bgfx::setViewRect(0, 0, 0, uint16_t(_windowWidth), uint16_t(_windowHeight));

			_tanFovY = bx::tan(_fovY * bx::acos(-1) / 180.f / 2.0f);
			_tanFovX = (static_cast<float>(_windowWidth) * _tanFovY) / static_cast<float>(_windowHeight);
			Camera_fovX = _tanFovX;
			Camera_fovY = _tanFovY;
			bgfx::setUniform(_cameraHandle, Camera, 4);
		}

		void drawSettingsDialog()
		{
			ImGui::SetNextWindowPos(
				ImVec2(_windowWidth - _windowHeight / 5.0f - 10.0f, 10.0f)
				, ImGuiCond_FirstUseEver
			);
			ImGui::SetNextWindowSize(
				ImVec2(_windowWidth / 6.0f, _windowHeight / 3.5f)
				, ImGuiCond_FirstUseEver
			);
			ImGui::Begin("Settings", NULL, 0);

			ImGui::Checkbox("Use Compute Shader", &_useComputeShader);
			ImGui::Checkbox("Debug Normals", &_debugNormals);
			ImGui::End();
		}
	};

} // namespace

using MainClass = RealisticAtmosphere::RealisticAtmosphere;
ENTRY_IMPLEMENT_MAIN(
	MainClass
	, "Realistic Atmosphere"
	, ""
	, "https://github.com/OSDVF/RealisticAtmoshpere"
);
// Declares main() function
