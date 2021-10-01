/*
 * Copyright 2021 Ondøej Sabela
 */

#include <bx/uint32_t.h>
 // Library for controlling aplication flow (init() and update() methods)
#include "entry/entry.h"
#include "imgui/imgui.h"

 // Utils for working with resources (shaders, meshes..)
#include "bgfx_utils.h"
#include "ScreenSpaceQuad.h"

#define HANDLE_OF_DEFALUT_WINDOW entry::WindowHandle{ 0 }

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

		bgfx::TextureHandle _raytracerRenderTexture; /**< Used only when using compute-shader variant */
		bgfx::UniformHandle _raytracerOutputSampler;

		bgfx::ProgramHandle _computShaderProgram;
		bgfx::ProgramHandle _displayingShaderProgram; /**< This program displays output from compute-shader raytracer */

		bgfx::ProgramHandle _raytracerShaderProgram; /**< This program includes full raytracer as fragment shader */

		ScreenSpaceQuad _screenSpaceQuad;/**< Output of raytracer (both the compute-shader variant and fragment-shader variant) */

		bool _useComputeShader = true;

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

			// Set view 0 clear state.
			bgfx::setViewClear(0, BGFX_CLEAR_NONE);//We don't need to clear anything, because whole screen covers screenSpaceQuad

			//
			// Setup Resources
			//

			_screenSpaceQuad = ScreenSpaceQuad(_windowWidth, _windowHeight);

			_displayingShaderProgram = loadProgram("rt_display.vert", "rt_display.frag");
			_raytracerShaderProgram = loadProgram("normal_render.vert", "normal_render.frag");
			_computShaderProgram = bgfx::createProgram(loadShader("compute_render.comp"));

			_raytracerRenderTexture = bgfx::createTexture2D(
				uint16_t(_windowWidth)
				, uint16_t(_windowHeight)
				, false
				, 1
				, bgfx::TextureFormat::RGBA8
				, BGFX_TEXTURE_RT_WRITE_ONLY);

			_raytracerOutputSampler = bgfx::createUniform("computeShaderOutput", bgfx::UniformType::Sampler);

			// Create Immediate GUI graphics context
			imguiCreate();
		}

		virtual int shutdown() override
		{
			// Destroy Immediate GUI graphics context
			imguiDestroy();

			// Shutdown bgfx.
			_screenSpaceQuad.destroy();
			bgfx::destroy(_computShaderProgram);
			bgfx::destroy(_displayingShaderProgram);
			bgfx::destroy(_raytracerShaderProgram);

			bgfx::destroy(_raytracerRenderTexture);
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
				imguiEndFrame();

				//
				// Graphics actions
				// 

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

				bgfx::setState(BGFX_STATE_WRITE_RGB);

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
		void fragmentShaderRaytracer()
		{
			_screenSpaceQuad.draw();//Draw screen space quad with our shader program
			bgfx::submit(0, _raytracerShaderProgram);
		}
		void computeShaderRaytracer()
		{
			bgfx::setImage(0, _raytracerRenderTexture, 0, bgfx::Access::Write);
			bgfx::dispatch(0, _computShaderProgram);

			bgfx::setTexture(0, _raytracerOutputSampler, _raytracerRenderTexture);
			_screenSpaceQuad.draw();//Draw screen space quad with our shader program
			bgfx::submit(0, _displayingShaderProgram);
		}

		/**
		 * This function is not used because it will set viewport which is already the default viewport
		 */
		void viewportActions()
		{
			float proj[16];
			bx::mtxOrtho(proj, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);

			// Set view 0 default viewport.
			bgfx::setViewTransform(0, NULL, proj);
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
