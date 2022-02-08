/*
 * Copyright 2021 Ondřej Sabela
 */
#include <bx/uint32_t.h>
#include <bx/file.h>
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
#include <array>
#include <sstream>
#include "Tonemapping.h"
#include "ColorMapping.h"

#define swap(x,y) do \
   { unsigned char swap_temp[sizeof(x) == sizeof(y) ? (signed)sizeof(x) : -1]; \
	   memcpy(swap_temp, &y, sizeof(x)); \
	   memcpy(&y, &x, sizeof(x)); \
	   memcpy(&x, swap_temp, sizeof(x)); \
	} while (0)

#define HANDLE_OF_DEFALUT_WINDOW entry::WindowHandle{ 0 }

const Material _materialBuffer[] = {
	{
		{1,1,1,1},// White
		{0,0,0},// No Specular part
		{0},// No Roughness
		{20,20,20}, // Max emission
		0
	},
	{
		{1, 1, 1, 1},// White albedo
		{0.5,0.5,0.5},// Half specular
		{0.5},// Half Roughness
		{0,0,0}, // No emission
		0
	},
	{
		{0, 0, 1, 1},// Blue albedo
		{0.5,0.5,0.5},// Half specular
		{0.5},// Half Roughness
		{0,0,0}, // No emission
		0
	}
};

const float earthRadius = 6360000; // cit. E. Bruneton page 3
const float atmosphereRadius = 6420000;
Sphere _objectBuffer[] = {
	{//Sun
		{0, 0, earthRadius * 60}, //Position
		{earthRadius}, //Radius
		0 //Material index
	},
	{
		{-14401, 1998, 7000}, //Position
		{1}, //Radius
		1, //Material index
	},
	{
		{-14401, 1998, 7001.7}, //Position
		{1}, //Radius
		2, //Material index
	}
};
vec4 _sunColor = { 1, 1, 1, 1 };// color

DirectionalLight _directionalLightBuffer[] = {
	{
		{0,0,0,0},//Direction will be assigned automatically
		_sunColor
	}
};

//When rendering participating media, all the coeficients start with β
/* Scattering coefficient for Rayleigh scattering βˢᵣ */
const vec3 precomputedRayleighScatteringCoefficients = vec3(0.0000058, 0.0000135, 0.0000331);/*for wavelengths (680,550,440)nm (roughly R,G,B)*/
/*Scattering coefficient for Mie scattering βˢₘ */
const float precomputedMieScaterringCoefficient = 21e-6f;
const float mieAssymetryFactor = 0.76;
const float mieScaleHeight = 1200;
const float rayleighScaleHeight = 7994;

std::array<Planet, 1> _planetBuffer = {
	Planet{
		vec3(0, -earthRadius, 0),//center
		earthRadius,//start radius
		atmosphereRadius,//end radius
		precomputedMieScaterringCoefficient,
		mieAssymetryFactor,
		mieScaleHeight,
		//These values are based on Nishita's measurements
		//This means that atmospheric thickness of 60 Km covers troposphere, stratosphere and a bit of mezosphere
		//I am usnure of the "completeness" of this model, but Nishita and Bruneton used this
		precomputedRayleighScatteringCoefficients,
		rayleighScaleHeight,
		1, //Sun intensity
		0, // Sun object index
		6365000, // Mountains radius
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

		float* _readedTexture;
		uint32_t _screenshotFrame = -1;
		Uint64 _freq;
		uint32_t _frame = 0;
		Uint64 _lastTicks;
		uint32_t _windowWidth = 1024;
		uint32_t _windowHeight = 600;
		uint32_t _debugFlags = 0;
		uint32_t _resetFlags = 0;
		entry::MouseState _mouseState;
		float _sunAngle = 1.46607657;//84 deg

		bgfx::UniformHandle _timeHandle;
		bgfx::UniformHandle _multisamplingSettingsHandle;
		bgfx::UniformHandle _qualitySettingsHandle;
		bgfx::TextureHandle _raytracerColorOutput;
		bgfx::TextureHandle _raytracerNormalsOutput;
		bgfx::TextureHandle _raytracerDepthBuffer;
		bgfx::UniformHandle _colorOutputSampler;
		bgfx::UniformHandle _hqSettingsHandle;
		bgfx::UniformHandle _lightSettings;
		bgfx::UniformHandle _lightSettings2;

		bgfx::ProgramHandle _computeShaderProgram;
		bgfx::ProgramHandle _displayingShaderProgram; /**< This program displays output from compute-shader raytracer */

		ScreenSpaceQuad _screenSpaceQuad;/**< Output of raytracer (both the compute-shader variant and fragment-shader variant) */

		bool _debugNormals = false;
		bool _debugAtmoOff = false;
		bool _debugRm = false;
		bool _showGUI = true;
		bool _pathTracingMode = false;

		vec4 _settingsBackup[6];

		bgfx::DynamicIndexBufferHandle _objectBufferHandle;
		bgfx::DynamicIndexBufferHandle _atmosphereBufferHandle;
		bgfx::DynamicIndexBufferHandle _materialBufferHandle;
		bgfx::DynamicIndexBufferHandle _directionalLightBufferHandle;
		bgfx::ShaderHandle _computeShaderHandle;
		bgfx::ShaderHandle _heightmapShaderHandle;
		bgfx::ShaderHandle _precomputeShaderHandle;
		bgfx::ProgramHandle _precomputeProgram;
		bgfx::ProgramHandle _heightmapShaderProgram;
		bgfx::TextureHandle _heightmapTextureHandle;
		bgfx::UniformHandle _heightmapSampler;
		bgfx::TextureHandle _texture1Handle;
		bgfx::TextureHandle _texture2Handle;
		bgfx::TextureHandle _texture3Handle;
		bgfx::TextureHandle _opticalDepthTable;
		bgfx::UniformHandle _texSampler1;
		bgfx::UniformHandle _texSampler2;
		bgfx::UniformHandle _texSampler3;
		bgfx::UniformHandle _opticalDepthSampler;
		bgfx::UniformHandle _atmoParameters;
		bgfx::UniformHandle _sunRadToLumHandle;
		bgfx::UniformHandle _skyRadToLumHandle;

		bgfx::UniformHandle _cameraHandle;
		bgfx::UniformHandle _planetMaterialHandle;
		bgfx::UniformHandle _raymarchingStepsHandle;
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
			// Initial pathtracing settings values
			_settingsBackup[0] = QualitySettings;
			_settingsBackup[1] = MultisamplingSettings;
			_settingsBackup[2] = RaymarchingSteps;
			_settingsBackup[3] = LightSettings;
			_settingsBackup[4] = LightSettings2;
			_settingsBackup[5] = HQSettings;

			_person.Camera.SetPosition(glm::vec3(-14401, 2289, 7052));
			_person.Camera.SetRotation(glm::vec3(3, -272, 0));

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
			bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH);

			//
			// Setup Resources
			//
			_cameraHandle = bgfx::createUniform("Camera", bgfx::UniformType::Vec4, 4);//It is an array of 4 vec4
			_sunRadToLumHandle = bgfx::createUniform("SunRadianceToLuminance", bgfx::UniformType::Vec4);
			_skyRadToLumHandle = bgfx::createUniform("SkyRadianceToLuminance", bgfx::UniformType::Vec4);
			_planetMaterialHandle = bgfx::createUniform("PlanetMaterial", bgfx::UniformType::Vec4);
			_raymarchingStepsHandle = bgfx::createUniform("RaymarchingSteps", bgfx::UniformType::Vec4);
			_colorOutputSampler = bgfx::createUniform("colorOutput", bgfx::UniformType::Sampler);
			_heightmapSampler = bgfx::createUniform("heightmapTexture", bgfx::UniformType::Sampler);
			_opticalDepthSampler = bgfx::createUniform("opticalDepthTable", bgfx::UniformType::Sampler);
			_atmoParameters = bgfx::createUniform("AtmoParameters", bgfx::UniformType::Vec4);
			_texSampler1 = bgfx::createUniform("texSampler1", bgfx::UniformType::Sampler);
			_texSampler2 = bgfx::createUniform("texSampler2", bgfx::UniformType::Sampler);
			_texSampler3 = bgfx::createUniform("texSampler3", bgfx::UniformType::Sampler);
			_debugAttributesHandle = bgfx::createUniform("debugAttributes", bgfx::UniformType::Vec4);
			_timeHandle = bgfx::createUniform("time", bgfx::UniformType::Vec4);
			_multisamplingSettingsHandle = bgfx::createUniform("MultisamplingSettings", bgfx::UniformType::Vec4);
			_qualitySettingsHandle = bgfx::createUniform("QualitySettings", bgfx::UniformType::Vec4);
			_hqSettingsHandle = bgfx::createUniform("HQSettings", bgfx::UniformType::Vec4);
			_lightSettings = bgfx::createUniform("LightSettings", bgfx::UniformType::Vec4);
			_lightSettings2 = bgfx::createUniform("LightSettings2", bgfx::UniformType::Vec4);

			_displayingShaderProgram = loadProgram("rt_display.vert", "rt_display.frag");
			_computeShaderHandle = loadShader("compute_render.comp");
			_computeShaderProgram = bgfx::createProgram(_computeShaderHandle);
			_heightmapShaderHandle = loadShader("Heightmap.comp");
			_heightmapShaderProgram = bgfx::createProgram(_heightmapShaderHandle);
			_precomputeShaderHandle = loadShader("Precompute.comp");
			_precomputeProgram = bgfx::createProgram(_precomputeShaderHandle);
			_texture1Handle = loadTexture("textures/grass.dds", BGFX_TEXTURE_SRGB);
			_texture2Handle = loadTexture("textures/dirt.dds", BGFX_TEXTURE_SRGB);
			_texture3Handle = loadTexture("textures/rock.dds", BGFX_TEXTURE_SRGB);

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
			heightMap();

			// Render optical depth
			precompute();

			// Compute spectrum mapping functions
			ColorMapping::FillSpectrum(SkyRadianceToLuminance, SunRadianceToLuminance);

			// Create Immediate GUI graphics context
			imguiCreate();
			_lastTicks = SDL_GetPerformanceCounter();
			_freq = SDL_GetPerformanceFrequency();
		}

		void heightMap()
		{
			_heightmapTextureHandle = bgfx::createTexture2D(4096, 4096, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_COMPUTE_WRITE);
			bgfx::setImage(0, _heightmapTextureHandle, 0, bgfx::Access::Write);
			bgfx::dispatch(0, _heightmapShaderProgram, bx::ceil(4096 / 16.0f), bx::ceil(4096 / 16.0f));
		}

		void precompute()
		{
			_opticalDepthTable = bgfx::createTexture2D(2048, 1024, false, 1, bgfx::TextureFormat::RG32F, BGFX_TEXTURE_COMPUTE_WRITE);
			//steps are locked to 300
			vec4 _atmoParametersValues = { rayleighScaleHeight,earthRadius, atmosphereRadius, mieScaleHeight };
			bgfx::setUniform(_atmoParameters, &_atmoParametersValues);
			bgfx::setImage(0, _opticalDepthTable, 0, bgfx::Access::Write, bgfx::TextureFormat::RG32F);
			bgfx::dispatch(0, _precomputeProgram, bx::ceil(2048 / 16.0f), bx::ceil(1024 / 16.0f));
		}

		void resetBufferSize()
		{
			_screenSpaceQuad = ScreenSpaceQuad((float)_windowWidth, (float)_windowHeight);//Init internal vertex layout
			_raytracerColorOutput = bgfx::createTexture2D(
				uint16_t(_windowWidth)
				, uint16_t(_windowHeight)
				, false
				, 1
				,
				bgfx::TextureFormat::RGBA32F
				, BGFX_TEXTURE_COMPUTE_WRITE | BGFX_TEXTURE_READ_BACK);
			_raytracerNormalsOutput = bgfx::createTexture2D(
				uint16_t(_windowWidth)
				, uint16_t(_windowHeight)
				, false
				, 1
				,
				bgfx::TextureFormat::RGBA8
				, BGFX_TEXTURE_COMPUTE_WRITE);
			_raytracerDepthBuffer = bgfx::createTexture2D(
				uint16_t(_windowWidth)
				, uint16_t(_windowHeight)
				, false
				, 1
				,
				bgfx::TextureFormat::R32F
				, BGFX_TEXTURE_COMPUTE_WRITE);
			currentSample = 0;//Reset sample counter - otherwise the sampling would continue with wrong number of samples
		}

		virtual int shutdown() override
		{
			// Destroy Immediate GUI graphics context
			imguiDestroy();

			//Destroy resources
			bgfx::destroy(_timeHandle);
			bgfx::destroy(_multisamplingSettingsHandle);
			bgfx::destroy(_hqSettingsHandle);
			bgfx::destroy(_lightSettings);
			bgfx::destroy(_lightSettings2);
			bgfx::destroy(_qualitySettingsHandle);
			bgfx::destroy(_heightmapTextureHandle);
			bgfx::destroy(_raytracerDepthBuffer);
			bgfx::destroy(_raytracerColorOutput);
			bgfx::destroy(_raytracerNormalsOutput);
			bgfx::destroy(_colorOutputSampler);
			bgfx::destroy(_texSampler1);
			bgfx::destroy(_texSampler2);
			bgfx::destroy(_texSampler3);
			bgfx::destroy(_computeShaderHandle);
			bgfx::destroy(_computeShaderProgram);
			bgfx::destroy(_displayingShaderProgram);
			bgfx::destroy(_materialBufferHandle);
			bgfx::destroy(_directionalLightBufferHandle);
			bgfx::destroy(_atmosphereBufferHandle);
			bgfx::destroy(_objectBufferHandle);
			bgfx::destroy(_cameraHandle);
			bgfx::destroy(_debugAttributesHandle);
			bgfx::destroy(_planetMaterialHandle);
			bgfx::destroy(_raymarchingStepsHandle);
			bgfx::destroy(_heightmapShaderHandle);
			bgfx::destroy(_heightmapShaderProgram);
			bgfx::destroy(_precomputeShaderHandle);
			bgfx::destroy(_precomputeProgram);
			bgfx::destroy(_opticalDepthSampler);
			bgfx::destroy(_opticalDepthTable);
			bgfx::destroy(_atmoParameters);
			bgfx::destroy(_sunRadToLumHandle);
			bgfx::destroy(_skyRadToLumHandle);

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
				updateSun();
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
						SDL_CaptureMouse(SDL_TRUE);
						SDL_SetRelativeMouseMode(SDL_TRUE);
					}
					else
					{
						SDL_CaptureMouse(SDL_FALSE);
						SDL_SetRelativeMouseMode(SDL_FALSE);
					}
					break;
				case 'g':
					_showGUI = !_showGUI;
					break;
				case 'r':
					_objectBuffer[1].position = vec3(Camera[0].x, Camera[0].y, Camera[0].z);
					break;
				case 't':
					_person.Camera.SetPosition(glm::vec3(-14409.8f, 1997.61f, 6999.89));
					_person.Camera.SetRotation(glm::vec3(3, -85, 0));
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
				int maxSamples = *(int*)&HQSettings_directSamples * *(int*)&Multisampling_indirect;
				if (currentSample >= maxSamples)
				{
					if (!_pathTracingMode)
					{
						currentSample = 0;
						renderScene();
					}
					else
					{
						currentSample = INT_MAX;
					}
				}
				else
				{
					renderScene();
				}

				if (_debugNormals)
					bgfx::setTexture(0, _colorOutputSampler, _raytracerNormalsOutput);
				else
					bgfx::setTexture(0, _colorOutputSampler, _raytracerColorOutput);
				_screenSpaceQuad.draw();//Draw screen space quad with our shader program

				bgfx::setState(BGFX_STATE_DEFAULT);
				bgfx::submit(0, _displayingShaderProgram);
				bgfx::touch(0);

				// Advance to next frame. Rendering thread will be kicked to
				// process submitted rendering primitives.
				if (bgfx::frame() == _screenshotFrame)
				{
					savePng();
				}
				_frame++;
				return true;
			}
			// update() should return false when we want the application to exit
			return false;
		}

		void renderScene()
		{
			updateBuffersAndUniforms();

#if _DEBUG
			updateDebugUniforms();
#endif

			computeShaderRaytracer();
			currentSample++;
		}

		void updateSun()
		{
			for (int i = 0; i < _planetBuffer.size(); i++)
			{
				auto& planet = _planetBuffer[i];
				auto& sun = _objectBuffer[0];
				glm::quat rotQua(glm::vec3(_sunAngle, 30, 0));
				glm::vec3 pos(0, bx::length(sun.position), 0);
				pos = rotQua * pos;
				sun.position = vec3(pos.x, pos.y, pos.z);

				auto& sunLight = _directionalLightBuffer[planet.sunDrectionalLightIndex];
				sunLight.color = _sunColor;
				/*auto intensity = _planetBuffer[0].sunIntensity / 10;
				sunLight.color.x *= intensity;
				sunLight.color.y *= intensity;
				sunLight.color.z *= intensity;*/
				sunLight.direction = vec4::fromVec3(bx::sub(sun.position, planet.center /* light direction is reverse */)).normalize();
			}
		}
#if _DEBUG
		void updateDebugUniforms()
		{
			_debugAttributesResult = vec4(_debugNormals ? 1 : 0, _debugRm ? 1 : 0, _debugAtmoOff ? 1 : 0, 0);
			bgfx::setUniform(_debugAttributesHandle, &_debugAttributesResult);
		}
#endif
		void computeShaderRaytracer()
		{
			bgfx::setImage(0, _raytracerColorOutput, 0, bgfx::Access::ReadWrite);
			bgfx::setImage(1, _raytracerNormalsOutput, 0, bgfx::Access::ReadWrite);
			bgfx::setImage(2, _raytracerDepthBuffer, 0, bgfx::Access::ReadWrite);
			bgfx::dispatch(0, _computeShaderProgram, bx::ceil(_windowWidth / 16.0f), bx::ceil(_windowHeight / 16.0f));
		}

		void updateBuffersAndUniforms()
		{
			bgfx::update(_objectBufferHandle, 0, bgfx::makeRef((void*)_objectBuffer, sizeof(_objectBuffer)));
			bgfx::update(_atmosphereBufferHandle, 0, bgfx::makeRef((void*)_planetBuffer.data(), sizeof(_planetBuffer)));
			bgfx::update(_materialBufferHandle, 0, bgfx::makeRef((void*)_materialBuffer, sizeof(_materialBuffer)));
			bgfx::update(_directionalLightBufferHandle, 0, bgfx::makeRef((void*)_directionalLightBuffer, sizeof(_directionalLightBuffer)));

			bgfx::setBuffer(3, _objectBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(4, _atmosphereBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(5, _materialBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(6, _directionalLightBufferHandle, bgfx::Access::Read);
			vec4 timeWrapper = vec4(_frame, 0, 0, 0);
			HQSettings_sampleNum = *(float*)&currentSample;
			bgfx::setUniform(_timeHandle, &timeWrapper);
			bgfx::setUniform(_multisamplingSettingsHandle, &MultisamplingSettings);
			bgfx::setUniform(_qualitySettingsHandle, &QualitySettings);
			bgfx::setUniform(_planetMaterialHandle, &PlanetMaterial);
			bgfx::setUniform(_raymarchingStepsHandle, &RaymarchingSteps);
			bgfx::setUniform(_hqSettingsHandle, &HQSettings);
			bgfx::setUniform(_lightSettings, &LightSettings);
			bgfx::setUniform(_lightSettings2, &LightSettings2);
			bgfx::setUniform(_sunRadToLumHandle, &SunRadianceToLuminance);
			bgfx::setUniform(_skyRadToLumHandle, &SkyRadianceToLuminance);
			bgfx::setTexture(7, _texSampler1, _texture1Handle);
			bgfx::setTexture(8, _texSampler2, _texture2Handle);
			bgfx::setTexture(9, _texSampler3, _texture3Handle);
			bgfx::setTexture(10, _heightmapSampler, _heightmapTextureHandle);
			bgfx::setTexture(11, _opticalDepthSampler, _opticalDepthTable, BGFX_SAMPLER_UVW_CLAMP);
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

		void swapSettingsBackup()
		{
			swap(_settingsBackup[0], QualitySettings);
			swap(_settingsBackup[1], MultisamplingSettings);
			swap(_settingsBackup[2], RaymarchingSteps);
			swap(_settingsBackup[3], LightSettings);
			swap(_settingsBackup[4], LightSettings2);
			swap(_settingsBackup[5], HQSettings);
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
			ImGui::InputFloat("Mountain Radius", &singlePlanet.mountainsRadius);
			ImGui::InputFloat("Mie Coef", &singlePlanet.mieCoefficient, 0, 0, "%e");
			ImGui::InputFloat("M.Asssymetry Factor", &singlePlanet.mieAsymmetryFactor);
			ImGui::InputFloat("M.Scale Height", &singlePlanet.mieScaleHeight);
			ImGui::InputFloat("R.Scale Height", &singlePlanet.rayleighScaleHeight);
			ImGui::InputFloat("Sun Intensity", &singlePlanet.sunIntensity);
			ImGui::PopItemWidth();
			ImGui::InputFloat("Sun Angle", &_sunAngle);
			ImGui::SliderAngle("", &_sunAngle, 0, 180);
			ImGui::End();

			ImGui::SetNextWindowPos(
				ImVec2(0, 0)
				, ImGuiCond_FirstUseEver
			);
			if (_pathTracingMode)
			{
				drawPathTracerGUI();
				return;
			}
			ImGui::Begin("Realtime Preview");
			if (ImGui::Button("Go Path Tracing"))
			{
				_pathTracingMode = true;
				swapSettingsBackup();
			}
			if (ImGui::Button("Save Image"))
			{
				requestSavePng();
			}
			ImGui::InputFloat("Speed", &_person.WalkSpeed);
			ImGui::InputFloat("RunSpeed", &_person.RunSpeed);
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
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);

			ImGui::Begin("Settings");
			ImGui::Checkbox("Debug Normals", &_debugNormals);
			ImGui::Checkbox("Debug RayMarch", &_debugRm);
			ImGui::Checkbox("Hide atmosphere", &_debugAtmoOff);

			ImGui::PushItemWidth(90);
			ImGui::InputInt("Samples", (int*)&HQSettings_directSamples);
			ImGui::InputInt("Atmosphere supersampling", (int*)&Multisampling_perAtmospherePixel);
			ImGui::PopItemWidth();
			ImGui::End();

			drawTerrainGUI();

			ImGui::SetNextWindowPos(ImVec2(250, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			ImGui::Begin("Materials");
			ImGui::InputFloat("1", &PlanetMaterial.x);
			ImGui::InputFloat("2", &PlanetMaterial.y);
			ImGui::InputFloat("Gradient", &PlanetMaterial.w);
			ImGui::End();

			drawLightGUI();

		}

		void drawLightGUI()
		{
			ImGui::SetNextWindowPos(ImVec2(250, 40), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			ImGui::Begin("Light");
			ImGui::InputFloat("Exposure", &HQSettings_exposure);
			ImGui::InputFloat("Precision", &LightSettings_precision, 0, 0, "%e");
			ImGui::InputFloat("Far Plane", &LightSettings_farPlane);
			ImGui::InputFloat("NoRayThres", &LightSettings_noRayThres);
			ImGui::InputFloat("ViewThres", &LightSettings_viewThres);
			ImGui::InputFloat("Gradient", &LightSettings_gradient);
			ImGui::InputFloat("CutoffDist", &LightSettings_cutoffDist);
			ImGui::InputInt("Shdw dtct stps", (int*)&LightSettings_shadowSteps);
			ImGui::InputFloat("TerrainOptim", &LightSettings_terrainOptimMult);
			ImGui::End();
		}

		void drawTerrainGUI()
		{
			ImGui::SetNextWindowPos(ImVec2(250, 60), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			ImGui::Begin("Terrain");
			ImGui::InputFloat("Optimism", &QualitySettings_optimism, 0, 1);
			ImGui::InputFloat("Far Plane", &QualitySettings_farPlane);
			ImGui::InputFloat("MinStepSize", &QualitySettings_minStepSize);
			ImGui::InputInt("Planet Steps", (int*)&RaymarchingSteps.x);
			ImGui::InputFloat("Precision", &RaymarchingSteps.z, 0, 0, "%e");
			ImGui::InputFloat("LOD Div", &RaymarchingSteps.y);
			ImGui::InputFloat("LOD Bias", &RaymarchingSteps.w);
			ImGui::InputFloat("Normals", &PlanetMaterial.z);
			ImGui::End();
		}

		void drawPathTracerGUI()
		{
			ImGui::Begin("Path Tracer");
			if (ImGui::Button("Back To Realtime"))
			{
				_pathTracingMode = false;
				swapSettingsBackup();
			}
			if (ImGui::Button("Re-render"))
			{
				currentSample = 0;
			}
			if (ImGui::Button("Save Image"))
			{
				requestSavePng();
			}
			if (currentSample >= *(int*)&HQSettings_directSamples * *(int*)&Multisampling_indirect)
			{
				ImGui::Text("Completed", currentSample);
			}
			else
			{
				ImGui::Text("%d sampled", currentSample);
			}
			ImGui::PushItemWidth(90);
			ImGui::InputInt("Direct rays", (int*)&HQSettings_directSamples);
			ImGui::InputInt("Secondary rays", (int*)&Multisampling_indirect);

			ImGui::InputInt("Bounces", (int*)&Multisampling_maxBounces);
			ImGui::InputInt("Atmosphere samples", (int*)&Multisampling_perAtmospherePixel);
			ImGui::PopItemWidth();
			ImGui::End();

			drawLightGUI();
			drawTerrainGUI();
		}

		void savePng()
		{
			bx::FileWriter writer;
			bx::Error err;
			std::ostringstream fileName;
			fileName << _frame;
			fileName << ".png";
			if (bx::open(&writer, fileName.str().c_str(), false, &err))
			{
				char* converted = new char[_windowHeight * _windowWidth * 4];
				for (int x = 0; x < _windowWidth * _windowHeight * 4; x += 4)
				{
					auto dirSamp = *(int*)&HQSettings_directSamples;
					_readedTexture[x] = tmFunc(_readedTexture[x] / dirSamp);
					_readedTexture[x + 1] = tmFunc(_readedTexture[x + 1] / dirSamp);
					_readedTexture[x + 2] = tmFunc(_readedTexture[x + 2] / dirSamp);
					_readedTexture[x + 3] = 1.0f;
				}
				bimg::imageConvert(entry::getAllocator(), converted, bimg::TextureFormat::RGBA8, _readedTexture, bimg::TextureFormat::RGBA32F, _windowWidth, _windowHeight, 1);
				bimg::imageWritePng(&writer, _windowWidth, _windowHeight, _windowWidth * 4, converted, bimg::TextureFormat::RGBA8, false, &err);
				bx::close(&writer);
				if (err.isOk())
				{
					bx::debugOutput("Screenshot successfull");
				}
				else
				{
					bx::debugOutput(err.getMessage());
				}
				delete[] converted;
				delete[] _readedTexture;
			}
			else
			{
				bx::debugOutput("Screenshot failed.");
			}
		}

		void requestSavePng()
		{
			_readedTexture = new float[_windowHeight * _windowWidth * 4];
			_screenshotFrame = bgfx::readTexture(_raytracerColorOutput, _readedTexture);
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

