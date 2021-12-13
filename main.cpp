﻿/*
 * Copyright 2021 Ondřej Sabela
 */
#include <bx/uint32_t.h>
#define ENTRY_CONFIG_USE_SDL
 // Library for controlling aplication flow (init() and update() methods) and event system (mouse & keyboard)
#include "entry/entry.h"
#include "imgui/imgui.h"
#include "gl_utils.h"
#include <glm/gtc/type_ptr.hpp>

 // Utils for working with resources (shaders, meshes..)
#include "ScreenSpaceQuad.h"
#include "SceneObjects.h"
#include "FirstPersonController.h"
#include <entry/input.h>
#include <SDL2/SDL.h>

#define HANDLE_OF_DEFALUT_WINDOW entry::WindowHandle{ 0 }

const Material _materialBuffer[] = {
	{
		{1,1,1,1},// White
		{0,0,0},// No Specular part
		{0},// No Roughness
		{10,10,10}, // Max emission
		0
	},
	{
		{1, 0.5, 1 / 39, 1},// Orange albedo
		{0.5,0.5,0.5},// Half specular
		{0.5},// Half Roughness
		{0,0,0}, // No emission
		0
	},
	{
		{0, 0.1, 1, 1},// Blue albedo
		{0.5,0.5,0.5},// Half specular
		{0.5},// Half Roughness
		{0,0,0}, // No emission
		0
	}
};

const float earthRadius = 6360000; // cit. E. Bruneton page 3
const Sphere _objectBuffer[] = {
	{//Sun
		{earthRadius * 0.2 , earthRadius * 10, earthRadius * 60}, //Position
		{earthRadius}, //Radius
		0 //Material index
	},
	{
		{1, earthRadius + 8000, 30200}, //Position
		{5}, //Radius
		1, //Material index
	}
};

const DirectionalLight _directionalLightBuffer[] = {
	{
		vec4(0, 2, -1, 0).normalize(),// direction
		{1, 1, 1, 1}// color
	}
};

//When rendering participating media, all the coeficients start with β
/* Scattering coefficient for Rayleigh scattering βˢᵣ */
const vec3 precomputedRayleighScatteringCoefficients = vec3(0.0000058, 0.0000135, 0.0000331);/*for wavelengths (680,550,440)nm (roughly R,G,B)*/
/*Scattering coefficient for Mie scattering βˢₘ */
const float precomputedMieScaterringCoefficient = 21e-6f;
const float mieAssymetryFactor = 0.76;

Planet _planetBuffer[] = {
	{
		{0, 0, 0},//center
		earthRadius,//start radius
		6420000,//end radius
		precomputedMieScaterringCoefficient,
		mieAssymetryFactor,
		1200,//Mie scale height
		//These values are based on Nishita's measurements
		//This means that atmospheric thickness of 60 Km covers troposphere, stratosphere and a bit of mezosphere
		//I am usnure of the "completeness" of this model, but Nishita and Bruneton used this
		precomputedRayleighScatteringCoefficients,
		7994,//Rayleigh scale heigh
		10, //Sun intensity
		0, // Sun object index
		earthRadius + 40000, // Mountains radius
		0 //padding 
	}
};

namespace RealisticAtmosphere
{
	class RealisticAtmosphere : public entry::AppI // Entry point for our application
	{
	public:
		RealisticAtmosphere(const char* name, const char* description, const char* projectUrl)
			: entry::AppI(name, description, projectUrl) {}

		Uint64 _freq;
		uint32_t _frame = 0;
		Uint64 _lastTicks;
		uint32_t _windowWidth = 1024;
		uint32_t _windowHeight = 600;
		uint32_t _debugFlags = 0;
		uint32_t _resetFlags = 0;
		entry::MouseState _mouseState;

		bgfx::UniformHandle _timeHandle;
		bgfx::UniformHandle _multisamplingSettingsHandle;
		bgfx::UniformHandle _qualitySettingsHandle;
		bgfx::TextureHandle _raytracerOutputTexture; /**< Used only when using compute-shader variant */
		bgfx::UniformHandle _raytracerOutputSampler;

		bgfx::ProgramHandle _computeShaderProgram;
		bgfx::ProgramHandle _displayingShaderProgram; /**< This program displays output from compute-shader raytracer */

		bgfx::ProgramHandle _raytracerShaderProgram; /**< This program includes full raytracer as fragment shader */

		ScreenSpaceQuad _screenSpaceQuad;/**< Output of raytracer (both the compute-shader variant and fragment-shader variant) */

		bool _useComputeShader = true;
		bool _debugNormals = false;
		bool _debugAtmoOff = false;
		bool _debugRm = false;
		bool _showGUI = true;

		bgfx::DynamicIndexBufferHandle _objectBufferHandle;
		bgfx::DynamicIndexBufferHandle _atmosphereBufferHandle;
		bgfx::DynamicIndexBufferHandle _materialBufferHandle;
		bgfx::DynamicIndexBufferHandle _directionalLightBufferHandle;
		bgfx::ShaderHandle _computeShaderHandle;
		bgfx::ShaderHandle _heightmapShaderHandle;
		bgfx::ProgramHandle _heightmapShaderProgram;
		bgfx::TextureHandle _heightmapTextureHandle;
		bgfx::UniformHandle _heightmapSampler;
		bgfx::TextureHandle _texture1Handle;
		bgfx::TextureHandle _texture2Handle;
		bgfx::TextureHandle _texture3Handle;
		bgfx::UniformHandle _texSampler1;
		bgfx::UniformHandle _texSampler2;
		bgfx::UniformHandle _texSampler3;

		bgfx::UniformHandle _cameraHandle;
#if _DEBUG
		bgfx::UniformHandle _debugAttributesHandle;
		vec4 _debugAttributesResult = vec4(0, 0, 0, 0);
#endif
		float _tanFovY;
		float _tanFovX;
		float _fovY = 45;
		bool _mouseLock = false;

		FirstPersonController _person;

		// The Entry library will call this method after setting up window manager
		void init(int32_t argc, const char* const* argv, uint32_t width, uint32_t height) override
		{
			_person.Camera.SetPosition(glm::vec3(0, earthRadius + 8000, 30000));

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

			_cameraHandle = bgfx::createUniform("Camera", bgfx::UniformType::Vec4, 4);//It is an array of 4 vec4
			_raytracerOutputSampler = bgfx::createUniform("computeShaderOutput", bgfx::UniformType::Sampler);
			_heightmapSampler = bgfx::createUniform("heightmapTexture", bgfx::UniformType::Sampler);
			_texSampler1 = bgfx::createUniform("texSampler1", bgfx::UniformType::Sampler);
			_texSampler2 = bgfx::createUniform("texSampler2", bgfx::UniformType::Sampler);
			_texSampler3 = bgfx::createUniform("texSampler3", bgfx::UniformType::Sampler);
			_debugAttributesHandle = bgfx::createUniform("debugAttributes", bgfx::UniformType::Vec4);
			_timeHandle = bgfx::createUniform("time", bgfx::UniformType::Vec4);
			_multisamplingSettingsHandle = bgfx::createUniform("MultisamplingSettings", bgfx::UniformType::Vec4);
			_qualitySettingsHandle = bgfx::createUniform("QualitySettings", bgfx::UniformType::Vec4);

			_displayingShaderProgram = loadProgram("rt_display.vert", "rt_display.frag");
			_raytracerShaderProgram = loadProgram("normal_render.vert", "normal_render.frag");
			_computeShaderHandle = loadShader("compute_render.comp");
			_computeShaderProgram = bgfx::createProgram(_computeShaderHandle);
			_heightmapShaderHandle = loadShader("Heightmap.comp");
			_heightmapShaderProgram = bgfx::createProgram(_heightmapShaderHandle);
			_texture1Handle = loadTexture("textures/grass.ktx");
			_texture2Handle = loadTexture("textures/dirt.ktx");
			_texture3Handle = loadTexture("textures/rock.ktx");
			_heightmapTextureHandle = bgfx::createTexture2D(8192, 2048, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_COMPUTE_WRITE);

			/*auto data = imageLoad("textures/grass.ktx", bgfx::TextureFormat::RGB8);
			bgfx::updateTexture2D(_texturesHandle, 0, 0, 0, 0, 2048, 2048, bgfx::makeRef(data->m_data, data->m_size));
			data = imageLoad("textures/dirt.ktx", bgfx::TextureFormat::RGB8);
			bgfx::updateTexture2D(_texturesHandle, 1, 0, 0, 0, 2048, 2048, bgfx::makeRef(data->m_data, data->m_size));
			data = imageLoad("textures/rock.ktx", bgfx::TextureFormat::RGB8);
			bgfx::updateTexture2D(_texturesHandle, 2, 0, 0, 0, 2048, 2048, bgfx::makeRef(data->m_data, data->m_size));*/

			_objectBufferHandle = bgfx_utils::createDynamicComputeReadBuffer(sizeof(_objectBuffer));
			_atmosphereBufferHandle = bgfx_utils::createDynamicComputeReadBuffer(sizeof(_planetBuffer));
			_materialBufferHandle = bgfx_utils::createDynamicComputeReadBuffer(sizeof(_materialBuffer));
			_directionalLightBufferHandle = bgfx_utils::createDynamicComputeReadBuffer(sizeof(_directionalLightBuffer));

			resetBufferSize();

			// Render heighmap
			bgfx::setImage(0, _heightmapTextureHandle, 0, bgfx::Access::Write);
			bgfx::dispatch(0, _heightmapShaderProgram, bx::ceil(8192 / 16.0f), bx::ceil(2048 / 16.0f));

			// Create Immediate GUI graphics context
			imguiCreate();
			_lastTicks = SDL_GetPerformanceCounter();
			_freq = SDL_GetPerformanceFrequency();
		}

		void resetBufferSize()
		{
			_screenSpaceQuad = ScreenSpaceQuad((float)_windowWidth, (float)_windowHeight);//Init internal vertex layout
			_raytracerOutputTexture = bgfx::createTexture2D(
				uint16_t(_windowWidth)
				, uint16_t(_windowHeight)
				, false
				, 1
				, bgfx::TextureFormat::RGBA8
				, BGFX_TEXTURE_COMPUTE_WRITE);
		}

		virtual int shutdown() override
		{
			// Destroy Immediate GUI graphics context
			imguiDestroy();

			//Destroy resources
			bgfx::destroy(_timeHandle);
			bgfx::destroy(_multisamplingSettingsHandle);
			bgfx::destroy(_qualitySettingsHandle);
			bgfx::destroy(_heightmapTextureHandle);
			bgfx::destroy(_raytracerOutputTexture);
			bgfx::destroy(_raytracerOutputSampler);
			bgfx::destroy(_texSampler1);
			bgfx::destroy(_texSampler2);
			bgfx::destroy(_texSampler3);
			bgfx::destroy(_computeShaderHandle);
			bgfx::destroy(_computeShaderProgram);
			bgfx::destroy(_displayingShaderProgram);
			bgfx::destroy(_raytracerShaderProgram);
			bgfx::destroy(_materialBufferHandle);
			bgfx::destroy(_directionalLightBufferHandle);
			bgfx::destroy(_atmosphereBufferHandle);
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
			auto previousWidth = _windowWidth, previousHeight = _windowHeight;
			if (!entry::processEvents(_windowWidth, _windowHeight, _debugFlags, _resetFlags, &_mouseState))
			{
				// Maybe reset window buffer size
				if (previousWidth != _windowWidth || previousHeight != _windowHeight)
				{
					_screenSpaceQuad.destroy();
					resetBufferSize();
				}

				//
				// GUI Actions
				// 
				const uint8_t* utf8 = inputGetChar();
				char asciiKey = (nullptr != utf8) ? utf8[0] : 0;
				uint8_t modifiers = inputGetModifiersState();
				switch (asciiKey)
				{
				case 'l':
					_mouseLock = !_mouseLock;
					if (_mouseLock)
					{
						SDL_SetRelativeMouseMode(SDL_TRUE);
					}
					else
					{
						SDL_SetRelativeMouseMode(SDL_FALSE);
					}
					break;
				case 'g':
					_showGUI = !_showGUI;
					break;
				}
				if (_mouseLock)
				{
					auto nowTicks = SDL_GetPerformanceCounter();
					auto deltaTime = (float)((nowTicks - _lastTicks) * 1000 / (float)_freq);
					_person.Update(_mouseLock, deltaTime);

					_lastTicks = nowTicks;
				}

				// Supply mouse events to GUI library
				if (_showGUI)
				{
					imguiBeginFrame(_mouseState.m_mx,
						_mouseState.m_my,
						(_mouseState.m_buttons[entry::MouseButton::Left] ? IMGUI_MBUT_LEFT : 0)
						| (_mouseState.m_buttons[entry::MouseButton::Right] ? IMGUI_MBUT_RIGHT : 0)
						| (_mouseState.m_buttons[entry::MouseButton::Middle] ? IMGUI_MBUT_MIDDLE : 0),
						_mouseState.m_mz,
						uint16_t(_windowWidth),
						uint16_t(_windowHeight),
						asciiKey
					);
					// Displays/Updates an innner dialog with debug and profiler information
					showDebugDialog(this);

					drawSettingsDialogUI();

					imguiEndFrame();
				}

				//
				// Graphics actions
				// 

				viewportActions();

				updateBuffersAndUniforms();

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
				_frame++;
				return true;
			}
			// update() should return false when we want the application to exit
			return false;
		}

		void updateDebugUniforms()
		{
			_debugAttributesResult = vec4(_debugNormals ? 1 : 0, _debugRm ? 1 : 0, _debugAtmoOff ? 1 : 0 , 0);
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

		void updateBuffersAndUniforms()
		{
			bgfx::update(_objectBufferHandle, 0, bgfx::makeRef((void*)_objectBuffer, sizeof(_objectBuffer)));
			bgfx::update(_atmosphereBufferHandle, 0, bgfx::makeRef((void*)_planetBuffer, sizeof(_planetBuffer)));
			bgfx::update(_materialBufferHandle, 0, bgfx::makeRef((void*)_materialBuffer, sizeof(_materialBuffer)));
			bgfx::update(_directionalLightBufferHandle, 0, bgfx::makeRef((void*)_directionalLightBuffer, sizeof(_directionalLightBuffer)));

			bgfx::setBuffer(1, _objectBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(2, _atmosphereBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(3, _materialBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(4, _directionalLightBufferHandle, bgfx::Access::Read);
			vec4 timeWrapper = vec4(_frame, 0, 0, 0);
			bgfx::setUniform(_timeHandle, &timeWrapper);
			bgfx::setUniform(_multisamplingSettingsHandle, &MultisamplingSettings);
			bgfx::setUniform(_qualitySettingsHandle, &QualitySettings);
			bgfx::setTexture(5, _texSampler1, _texture1Handle, BGFX_SAMPLER_UVW_MIRROR | BGFX_SAMPLER_MIN_ANISOTROPIC);
			bgfx::setTexture(6, _texSampler2, _texture2Handle, BGFX_SAMPLER_UVW_MIRROR | BGFX_SAMPLER_MIN_ANISOTROPIC);
			bgfx::setTexture(7, _texSampler3, _texture3Handle, BGFX_SAMPLER_UVW_MIRROR | BGFX_SAMPLER_MIN_ANISOTROPIC);
			bgfx::setTexture(8, _heightmapSampler, _heightmapTextureHandle);
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

			glm::vec3 camPos = _person.Camera.GetPosition();
			glm::vec3 camRot = _person.Camera.GetForward();
			glm::vec3 camUp = _person.Camera.GetUp();
			glm::vec3 camRight = _person.Camera.GetRight();
			Camera[0] = vec4(camPos.x, camPos.y, camPos.z, 1);
			Camera[1] = vec4(camRot.x, camRot.y, camRot.z, 0);
			Camera[2] = vec4(camUp.x, camUp.y, camUp.z, _tanFovY);
			Camera[3] = vec4(camRight.x, camRight.y, camRight.z, _tanFovX);
			bgfx::setUniform(_cameraHandle, Camera, 4);
		}

		void drawSettingsDialogUI()
		{
			Planet& singlePlanet = _planetBuffer[0];
			ImGui::SetNextWindowPos(
				ImVec2(0, 145)
				, ImGuiCond_FirstUseEver
			);
			ImGui::Begin("Planet");
			ImGui::InputFloat3("Center", (float*)&singlePlanet.center);
			ImGui::PushItemWidth(90);
			ImGui::InputFloat("Rayleigh CoefR", &singlePlanet.rayleighCoefficients.x, 0, 0, "%e");
			ImGui::InputFloat("Rayleigh CoefG", &singlePlanet.rayleighCoefficients.y, 0, 0, "%e");
			ImGui::InputFloat("Rayleigh CoefB", &singlePlanet.rayleighCoefficients.z, 0, 0, "%e");
			ImGui::InputFloat("Radius", &singlePlanet.surfaceRadius);
			ImGui::InputFloat("Amosphere Radius", &singlePlanet.atmosphereRadius);
			ImGui::InputFloat("Mie Coef", &singlePlanet.mieCoefficient, 0, 0, "%e");
			ImGui::InputFloat("M.Asssymetry Factor", &singlePlanet.mieAsymmetryFactor);
			ImGui::InputFloat("M.Scale Height", &singlePlanet.mieScaleHeight);
			ImGui::InputFloat("R.Scale Height", &singlePlanet.rayleighScaleHeight);
			ImGui::InputFloat("Sun Intensity", &singlePlanet.sunIntensity);
			int sunObjectIndex = singlePlanet.sunObjectIndex;
			ImGui::InputInt("Sun Object", &sunObjectIndex);
			singlePlanet.sunObjectIndex = sunObjectIndex;
			ImGui::PopItemWidth();
			ImGui::End();

			ImGui::SetNextWindowPos(
				ImVec2(0, 0)
				, ImGuiCond_FirstUseEver
			);
			ImGui::Begin("Camera");
			ImGui::SliderFloat("Sensitivity", &_person.Camera.Sensitivity, 0, 5.0f);
			ImGui::InputFloat("FOV", &_fovY);
			ImGui::PushItemWidth(180);
			auto glmRot = _person.Camera.GetRotation();
			auto glmPos = _person.Camera.GetPosition();
			float pos[3] = { glmPos.x,glmPos.y,glmPos.z };
			float rot[3] = { glmRot.x,glmRot.y,glmRot.z };
			ImGui::InputFloat3("Pos", pos);
			_person.Camera.SetPosition({ pos[0],pos[1],pos[2] });
			ImGui::InputFloat3("Rot", rot);
			_person.Camera.SetRotation({ rot[0],rot[1],rot[2] });
			ImGui::PopItemWidth();
			ImGui::End();

			ImGui::SetNextWindowPos(
				ImVec2(220, 0)
				, ImGuiCond_FirstUseEver
			);
			ImGui::SetNextWindowSize(
				ImVec2(250, 180)
				, ImGuiCond_FirstUseEver
			);

			ImGui::Begin("Settings");

			ImGui::Checkbox("Use Compute Shader", &_useComputeShader);
			ImGui::Checkbox("Debug Normals", &_debugNormals);
			ImGui::PushItemWidth(90);

			//We need to cast the values to int and back to float because BGFX does not recognize ivec4 uniforms
			int perPixel = Multisampling_perPixel;
			ImGui::InputInt("Pixel supersampling", &perPixel);
			Multisampling_perPixel = perPixel;
			int perAtmo = Multisampling_perAtmospherePixel;
			ImGui::InputInt("Atmosphere supersampling", &perAtmo);
			Multisampling_perAtmospherePixel = perAtmo;
			int perLight = Multisampling_perLightRay;
			ImGui::InputInt("Light ray supersampling", &perLight);
			Multisampling_perLightRay = perLight;
			ImGui::PopItemWidth();

			ImGui::Checkbox("Hide atmosphere", &_debugAtmoOff);
			ImGui::Checkbox("Debug RayMarch", &_debugRm);
			ImGui::PushItemWidth(90);
			ImGui::InputFloat("Optimism", &QualitySettings_optimism, 0, 1);
			int steps = QualitySettings_steps;
			ImGui::InputInt("Steps", &steps);
			QualitySettings_steps = steps;
			int prec = QualitySettings_precision;
			ImGui::InputInt("Inaccuracy", &prec);
			QualitySettings_precision = prec;
			int far = QualitySettings_farPlane;
			ImGui::InputInt("Far Plane", &far);
			QualitySettings_farPlane = far;
			ImGui::PopItemWidth();
			ImGui::End();
		}
	};

} // namespace

using MainClass = RealisticAtmosphere::RealisticAtmosphere;
ENTRY_IMPLEMENT_MAIN(
	MainClass
	, "Realistic Atmosphere"
	, ""
	, "https://github.com/OSDVF/RealisticAtmosphere"
);
// Declares main() function

