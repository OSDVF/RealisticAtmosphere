/*
 * Copyright 2021 Ondøej Sabela
 */

#include <bx/uint32_t.h>
 // Library for controlling aplication flow (init() and update() methods)
#include "entry/entry.h"
#include "imgui/imgui.h"

 // Utils for working with resources (shaders, meshes..)
#include "bgfx_utils.h"

#include "TestCube.h"

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

		bgfx::VertexBufferHandle _vertexBuffer;
		bgfx::IndexBufferHandle _indexBuffer;
		bgfx::ProgramHandle _shaderProgram;

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
			bgfx::setViewClear(0
				, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH
				, 0x303030ff
				, 1.0f
				, 0
			);

			//
			// Setup Resources
			//

			// Create vertex stream declaration.
			TestCube::PosColorVertex::init();

			// Create static vertex buffer.
			_vertexBuffer = bgfx::createVertexBuffer(
				// Static data can be passed with bgfx::makeRef
				bgfx::makeRef(TestCube::s_cubeVertices, sizeof(TestCube::s_cubeVertices))
				, TestCube::PosColorVertex::ms_layout
			);

			// Create static index buffer for triangle list rendering.
			_indexBuffer = bgfx::createIndexBuffer(
				// Static data can be passed with bgfx::makeRef
				bgfx::makeRef(TestCube::s_cubeTriList, sizeof(TestCube::s_cubeTriList))
			);

			_shaderProgram = loadProgram("raytracing.vert","raytracing.frag");

			// Create Immediate GUI graphics context
			imguiCreate();
		}

		virtual int shutdown() override
		{
			// Destroy Immediate GUI graphics context
			imguiDestroy();

			// Shutdown bgfx.
			bgfx::destroy(_indexBuffer);
			bgfx::destroy(_vertexBuffer);
			bgfx::destroy(_shaderProgram);
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

				// Set view 0 default viewport.
				bgfx::setViewRect(0, 0, 0, uint16_t(_windowWidth), uint16_t(_windowHeight));

				// This dummy draw call is here to make sure that view 0 is cleared
				// if no other draw calls are submitted to view 0.
				bgfx::touch(0);

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

				const bx::Vec3 at = { 0.0f, 0.0f,   0.0f };
				const bx::Vec3 eye = { 0.0f, 0.0f, -35.0f };

				// Set view and projection matrix for view 0.
				{
					float view[16];
					bx::mtxLookAt(view, eye, at);

					float proj[16];
					bx::mtxProj(proj, 60.0f, float(_windowWidth) / float(_windowHeight), 0.1f, 100.0f, bgfx::getCaps()->homogeneousDepth);
					bgfx::setViewTransform(0, view, proj);

					// Set view 0 default viewport.
					bgfx::setViewRect(0, 0, 0, uint16_t(_windowWidth), uint16_t(_windowHeight));
				}

				bgfx::setVertexBuffer(0, _vertexBuffer);
				bgfx::setIndexBuffer(_indexBuffer);

				bgfx::submit(0, _shaderProgram);

				// Advance to next frame. Rendering thread will be kicked to
				// process submitted rendering primitives.
				bgfx::frame();

				return true;
			}
			// update() should return false when we want the application to exit
			return false;
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
