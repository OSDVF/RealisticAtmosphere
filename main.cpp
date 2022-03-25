/*
 * Copyright 2021 Ondřej Sabela
 */
 //Comment out to not render the "Rendering in progress" text
#define DRAW_RENDERING_PROGRESS

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
#include <tinystl/vector.h>
#include <sstream>
#include "Tonemapping.h"
#include "PhaseFunctions.h"
#include "ColorMapping.h"
#include "DefaultScene.h"

#define swap(x,y) do \
   { unsigned char swap_temp[sizeof(x) == sizeof(y) ? (signed)sizeof(x) : -1]; \
	   memcpy(swap_temp, &y, sizeof(x)); \
	   memcpy(&y, &x, sizeof(x)); \
	   memcpy(&x, swap_temp, sizeof(x)); \
	} while (0)

#define HANDLE_OF_DEFALUT_WINDOW entry::WindowHandle{ 0 }
#define SCREENSHOT_NEVER UINT32_MAX
#define SCREENSHOT_AFTER_RENDER SCREENSHOT_NEVER - 1
#define SCREENSHOT_AFTER_RENDER_PENDING SCREENSHOT_AFTER_RENDER - 1

#define DIRECT_SAMPLES_COUNT (*(int*)&HQSettings_directSamples)
// We swap buffers only when the rendering is realtime - because one buffer is currently rendering and the other is displaying.
// In non-realtime mode we just wait for the compute shader to complete
#define DO_DOUBLE_BUFFERING (DIRECT_SAMPLES_COUNT <= 1)

namespace RealisticAtmosphere
{
	class RealisticAtmosphere : public entry::AppI // Entry point for our application
	{
	public:
		RealisticAtmosphere(const char* name, const char* description, const char* projectUrl)
			: entry::AppI(name, description, projectUrl) {}

#pragma region Application_State
		uint16_t* _readedTexture = nullptr;//CPU buffer for converted texture
		bgfx::TextureHandle _stagingBuffer = BGFX_INVALID_HANDLE;//Texture handle in which a screenshot will be copied
		void* _syncObj = nullptr;
		// Number of frame at which a screenshot will be taken
		uint32_t _screenshotFrame = SCREENSHOT_NEVER;
		Uint64 _performanceFrequency = 0;
		uint32_t _frame = 0;
		Uint64 _lastTicks = 0;
		uint32_t _windowWidth = 1024;
		uint32_t _windowHeight = 600;
		bool _customScreenshotSize = false;
		struct {
			uint16_t width = 1024;
			uint16_t height = 600;
		} _renderImageSize;
		uint32_t _debugFlags = 0;
		uint32_t _resetFlags = 0;
		entry::MouseState _mouseState;
		float _sunAngle = 1.518;//87 deg
		float _moonAngle = 0;
		float _secondSunAngle = -1.55;
		float _secondMoonAngle = 1;
		bool _cloudsDSDUniformNotDisperse = true;
		vec3 _cloudsWind = vec3(-50, 0, 0);

		ScreenSpaceQuad _screenSpaceQuad;/**< Output of raytracer (both the compute-shader variant and fragment-shader variant) */

		bool _debugNormals = false;
		bool _debugAtmoOff = false;
		bool _debugRm = false;
		bool _showGUI = true;
		bool _pathTracingMode = true;
		bool _mouseLock = false;
		float _tanFovY = 0;
		float _tanFovX = 0;
		float _fovY = 45;
		FirstPersonController _person;
		vec4 _settingsBackup[7];
		int _outBufferIndex = 0;

		// Save these to be returned in place when the terrain rendering is re-enabled
		float prevTerrainSteps = 0;
		float prevLightSteps = 0;
#pragma endregion

#if _DEBUG
		bgfx::UniformHandle _debugAttributesHandle = BGFX_INVALID_HANDLE;
		vec4 _debugAttributesResult = vec4(0, 0, 0, 0);
#endif

#pragma region Settings_Uniforms
		bgfx::UniformHandle _timeHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _multisamplingSettingsHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _qualitySettingsHandle = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _raytracerColorOutput[2] = { BGFX_INVALID_HANDLE , BGFX_INVALID_HANDLE };//For double buffering
		bgfx::TextureHandle _raytracerNormalsOutput = BGFX_INVALID_HANDLE;
		//Depth is in R channel and albedo is packed in the G channel
		bgfx::TextureHandle _raytracerDepthAlbedoBuffer = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _colorOutputSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _hqSettingsHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _lightSettings = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _lightSettings2 = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _cloudsSettings = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _sunRadToLumHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _skyRadToLumHandle = BGFX_INVALID_HANDLE;

		bgfx::UniformHandle _cameraHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _planetMaterialHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _raymarchingStepsHandle = BGFX_INVALID_HANDLE;
#pragma endregion Settings_Uniforms
#pragma region Shaders_And_Buffers
		bgfx::ProgramHandle _computeShaderProgram = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle _displayingShaderProgram = BGFX_INVALID_HANDLE; /**< This program displays output from compute-shader raytracer */

		bgfx::ShaderHandle _computeShaderHandle = BGFX_INVALID_HANDLE;
		bgfx::ShaderHandle _heightmapShaderHandle = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle _heightmapShaderProgram = BGFX_INVALID_HANDLE;

		bgfx::DynamicIndexBufferHandle _objectBufferHandle = BGFX_INVALID_HANDLE;
		bgfx::DynamicIndexBufferHandle _atmosphereBufferHandle = BGFX_INVALID_HANDLE;
		bgfx::DynamicIndexBufferHandle _materialBufferHandle = BGFX_INVALID_HANDLE;
		bgfx::DynamicIndexBufferHandle _directionalLightBufferHandle = BGFX_INVALID_HANDLE;
#pragma endregion Shaders_And_Buffers
#pragma region Textures_And_Samplers
		bgfx::TextureHandle _heightmapTextureHandle = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _cloudsPhaseTextureHandle = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _textureArrayHandle = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _texture2Handle = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _texture3Handle = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _texture4Handle = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _opticalDepthTable = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _irradianceTable = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _transmittanceTable = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _singleScatteringTable = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _terrainTexSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _opticalDepthSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _singleScatteringSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _irradianceSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _transmittanceSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _heightmapSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _cloudsPhaseSampler = BGFX_INVALID_HANDLE;
#pragma endregion Textures_And_Samplers

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
			/*_settingsBackup[6] = CloudsSettings[0];
			_settingsBackup[7] = CloudsSettings[1];
			_settingsBackup[8] = CloudsSettings[2];*/

			_person.Camera.SetPosition(glm::vec3(-15654, 1661, 15875));
			_person.Camera.SetRotation(glm::vec3(-4, -273, 0));

			entry::setWindowFlags(HANDLE_OF_DEFALUT_WINDOW, ENTRY_WINDOW_FLAG_ASPECT_RATIO, false);
			entry::setWindowSize(HANDLE_OF_DEFALUT_WINDOW, 1024, 600);

			// Supply program arguments for setting graphics backend to BGFX.
			Args args(argc, argv);

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
			init.resolution.reset = BGFX_RESET_NONE;
			bgfx::init(init);

			// Set view 0 clear state.
			bgfx::setViewClear(0, BGFX_CLEAR_NONE);

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
			_cloudsPhaseSampler = bgfx::createUniform("cloudsMieLUT", bgfx::UniformType::Sampler);
			_opticalDepthSampler = bgfx::createUniform("opticalDepthTable", bgfx::UniformType::Sampler);
			_irradianceSampler = bgfx::createUniform("irradianceTable", bgfx::UniformType::Sampler);
			_transmittanceSampler = bgfx::createUniform("transmittanceTable", bgfx::UniformType::Sampler);
			_singleScatteringSampler = bgfx::createUniform("singleScatteringTable", bgfx::UniformType::Sampler);
			_terrainTexSampler = bgfx::createUniform("terrainTextures", bgfx::UniformType::Sampler);
			_debugAttributesHandle = bgfx::createUniform("debugAttributes", bgfx::UniformType::Vec4);
			_timeHandle = bgfx::createUniform("time", bgfx::UniformType::Vec4);
			_multisamplingSettingsHandle = bgfx::createUniform("MultisamplingSettings", bgfx::UniformType::Vec4);
			_qualitySettingsHandle = bgfx::createUniform("QualitySettings", bgfx::UniformType::Vec4);
			_hqSettingsHandle = bgfx::createUniform("HQSettings", bgfx::UniformType::Vec4);
			_lightSettings = bgfx::createUniform("LightSettings", bgfx::UniformType::Vec4);
			_lightSettings2 = bgfx::createUniform("LightSettings2", bgfx::UniformType::Vec4);
			_cloudsSettings = bgfx::createUniform("CloudsSettings", bgfx::UniformType::Vec4, sizeof(CloudsSettings) / sizeof(vec4));

			_displayingShaderProgram = loadProgram("rt_display.vert", "rt_display.frag");
			_computeShaderHandle = loadShader("compute_render.comp");
			_computeShaderProgram = bgfx::createProgram(_computeShaderHandle);
			_heightmapShaderHandle = loadShader("Heightmap.comp");
			_heightmapShaderProgram = bgfx::createProgram(_heightmapShaderHandle);
			tinystl::vector<std::string> textureFileNames(4);
			textureFileNames[0] = ("textures/rocks.dds");
			textureFileNames[1] = ("textures/dirt.dds");
			textureFileNames[2] = ("textures/rock.dds");
			textureFileNames[3] = ("textures/grass.dds");

			_textureArrayHandle = bgfx_utils::createTextureArray(
				textureFileNames, BGFX_TEXTURE_SRGB
			);

			_objectBufferHandle = bgfx_utils::createDynamicComputeReadBuffer(sizeof(DefaultScene::objectBuffer));
			_atmosphereBufferHandle = bgfx_utils::createDynamicComputeReadBuffer(sizeof(DefaultScene::planetBuffer));
			_materialBufferHandle = bgfx_utils::createDynamicComputeReadBuffer(sizeof(DefaultScene::materialBuffer));
			_directionalLightBufferHandle = bgfx_utils::createDynamicComputeReadBuffer(sizeof(DefaultScene::directionalLightBuffer));

			cleanRenderedBuffers(_windowWidth, _windowHeight);

			// Render terrain heighmap
			heightMap();

			// Render cloud particles Mie phase functions for all wavelengths
			cloudsMiePhaseFunction();

			// Compute spectrum mapping functions
			ColorMapping::FillSpectrum(SkyRadianceToLuminance, SunRadianceToLuminance, DefaultScene::planetBuffer[0], DefaultScene::directionalLightBuffer[0]);

			// Render optical depth, transmittance and direct irradiance textures
			precompute();

			// Create Immediate GUI graphics context
			imguiCreate();
			_lastTicks = SDL_GetPerformanceCounter();
			_performanceFrequency = SDL_GetPerformanceFrequency();

			// Prepare scene for the first time
			updateScene('\0');
		}

		void heightMap()
		{
			_heightmapTextureHandle = bgfx::createTexture2D(8192, 8192, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_COMPUTE_WRITE);
			bgfx::setImage(0, _heightmapTextureHandle, 0, bgfx::Access::Write);
			bgfx::dispatch(0, _heightmapShaderProgram, bx::ceil(8192 / 16.0f), bx::ceil(8192 / 16.0f));
		}
		void cloudsMiePhaseFunction()
		{
			auto cloudsMieData = new std::array<float, 1801 * 4>();
			const float* red;
			const float* green;
			const float* blue;
			if (_cloudsDSDUniformNotDisperse)
			{
				red = PhaseFunctions::CloudsRedUniform;
				green = PhaseFunctions::CloudsGreenUniform;
				blue = PhaseFunctions::CloudsBlueUniform;
			}
			else
			{
				red = PhaseFunctions::CloudsRedDisperse;
				green = PhaseFunctions::CloudsGreenDisperse;
				blue = PhaseFunctions::CloudsBlueDisperse;
			}
			for (uint64_t i = 0; i < 1801 * 4; i += 4)
			{
				auto singlePhaseFuncIndex = i / 4;

				(*cloudsMieData)[i] = red[singlePhaseFuncIndex];
				(*cloudsMieData)[i + 1] = green[singlePhaseFuncIndex];
				(*cloudsMieData)[i + 2] = blue[singlePhaseFuncIndex];
			}
			auto memData = bgfx::makeRef(cloudsMieData->data(), cloudsMieData->size() * sizeof(float), [](void* ptr, void* userData) {delete ptr; });
			if (bgfx::isValid(_cloudsPhaseTextureHandle))
			{
				bgfx::destroy(_cloudsPhaseTextureHandle);
			}

			_cloudsPhaseTextureHandle = bgfx::createTexture2D(1801, 1, false, 1, bgfx::TextureFormat::RGBA32F, 0, memData);

		}

		void precompute()
		{
			bgfx::UniformHandle precomputeSettingsHandle = bgfx::createUniform("PrecomputeSettings", bgfx::UniformType::Vec4);
			uint32_t PrecomputeSettings[] = { 0,0,0,0 };
			bgfx::setUniform(precomputeSettingsHandle, PrecomputeSettings);
			bgfx::ShaderHandle precomputeOptical = loadShader("OpticalDepth.comp");
			bgfx::ProgramHandle opticalProgram = bgfx::createProgram(precomputeOptical);
			if (!bgfx::isValid(_opticalDepthTable))
			{
				_opticalDepthTable = bgfx::createTexture2D(512, 256, false, 1, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_UVW_CLAMP);
			}
			//steps are locked to 300
			updateBuffers();
			bgfx::setImage(0, _opticalDepthTable, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA16F);
			bgfx::setBuffer(1, _atmosphereBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(2, _directionalLightBufferHandle, bgfx::Access::Read);
			bgfx::dispatch(0, opticalProgram, bx::ceil(512 / 16.0f), bx::ceil(256 / 16.0f));

			bgfx::ShaderHandle precomputeTransmittance = loadShader("Transmittance.comp");
			bgfx::ProgramHandle transmittanceProgram = bgfx::createProgram(precomputeTransmittance);
			if (!bgfx::isValid(_transmittanceTable))
			{
				_transmittanceTable = bgfx::createTexture2D(256, 64, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_UVW_CLAMP);
			}
			bgfx::setImage(0, _transmittanceTable, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA32F);
			bgfx::setBuffer(1, _atmosphereBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(2, _directionalLightBufferHandle, bgfx::Access::Read);
			bgfx::dispatch(0, transmittanceProgram, bx::ceil(256 / 16.0f), bx::ceil(64 / 16.0f));

			bgfx::ShaderHandle precomputeSingleScattering = loadShader("SingleScattering.comp");
			bgfx::ProgramHandle scatteringProgram = bgfx::createProgram(precomputeSingleScattering);
			constexpr int SCATTERING_TEXTURE_WIDTH =
				SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE;
			constexpr int SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_R_SIZE;
			if (!bgfx::isValid(_singleScatteringTable))
			{
				_singleScatteringTable = bgfx::createTexture3D(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, false, bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_UVW_CLAMP);
			}
			bgfx::setImage(0, _singleScatteringTable, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA16F);
			setBuffers();
			bgfx::setTexture(7, _transmittanceSampler, _transmittanceTable);
			bgfx::dispatch(0, scatteringProgram, bx::ceil(SCATTERING_TEXTURE_WIDTH / 16.0f), bx::ceil(SCATTERING_TEXTURE_HEIGHT / 16.0f), bx::ceil(SCATTERING_TEXTURE_DEPTH / 4.0f));

			bgfx::ShaderHandle precomputeIrradiance = loadShader("IndirectIrradiance.comp");
			bgfx::ProgramHandle irradianceProgram = bgfx::createProgram(precomputeIrradiance);
			if (!bgfx::isValid(_irradianceTable))
			{
				_irradianceTable = bgfx::createTexture2D(64, 16, false, DefaultScene::directionalLightBuffer.size(), bgfx::TextureFormat::RGBA16F, BGFX_TEXTURE_COMPUTE_WRITE);
			}
			bgfx::setImage(0, _irradianceTable, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA16F);
			bgfx::setBuffer(1, _atmosphereBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(2, _directionalLightBufferHandle, bgfx::Access::Read);
			bgfx::setTexture(3, _singleScatteringSampler, _singleScatteringTable);
			bgfx::dispatch(0, irradianceProgram, bx::ceil(64 / 16.0f), bx::ceil(16 / 16.0f), DefaultScene::directionalLightBuffer.size());

			bgfx::touch(0);
			bgfx::frame(); // Actually execute the compute shaders

			bgfx::destroy(opticalProgram);
			bgfx::destroy(irradianceProgram);
			bgfx::destroy(precomputeIrradiance);
			bgfx::destroy(precomputeOptical);
			bgfx::destroy(transmittanceProgram);
			bgfx::destroy(precomputeTransmittance);
			bgfx::destroy(scatteringProgram);
			bgfx::destroy(precomputeSingleScattering);
			bgfx::destroy(precomputeSettingsHandle);
		}

		void cleanRenderedBuffers(uint16_t newWidth, uint16_t newHeight)
		{
			_screenSpaceQuad = ScreenSpaceQuad((float)newWidth, (float)newHeight);//Init internal vertex layout
			_renderImageSize = { newWidth, newHeight };
			// Delete previous buffers
			if (bgfx::isValid(_raytracerColorOutput[0]))
			{
				bgfx::destroy(_raytracerColorOutput[0]);
				if (bgfx::isValid(_raytracerColorOutput[1]))
				{
					bgfx::destroy(_raytracerColorOutput[1]);
				}
				bgfx::destroy(_raytracerNormalsOutput);
				bgfx::destroy(_raytracerDepthAlbedoBuffer);
			}
			// Create new
			_raytracerColorOutput[0] = bgfx::createTexture2D(newWidth, newHeight, false, 1, bgfx::TextureFormat::RGBA16F,
				BGFX_TEXTURE_COMPUTE_WRITE);
			if (DO_DOUBLE_BUFFERING)
			{
				// The double buffering is necessary only when we are rendering in realtime (which we are only if DIRECT_SAMPLES_COUNT == 1)
				_raytracerColorOutput[1] = bgfx::createTexture2D(newWidth, newHeight, false, 1, bgfx::TextureFormat::RGBA16F,
					BGFX_TEXTURE_COMPUTE_WRITE);
			}
			else
			{
				_raytracerColorOutput[1] = BGFX_INVALID_HANDLE;
			}

			_raytracerNormalsOutput = bgfx::createTexture2D(newWidth, newHeight, false, 1,
				bgfx::TextureFormat::RGBA8,
				BGFX_TEXTURE_COMPUTE_WRITE);
			_raytracerDepthAlbedoBuffer = bgfx::createTexture2D(newWidth, newHeight, false, 1,
				bgfx::TextureFormat::RG32F,
				BGFX_TEXTURE_COMPUTE_WRITE);

			_outBufferIndex = 0;
			currentSample = 0;//Reset sample counter - otherwise path tracing would continue with previous samples
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
			bgfx::destroy(_raytracerDepthAlbedoBuffer);
			bgfx::destroy(_raytracerColorOutput[0]);
			if (bgfx::isValid(_raytracerColorOutput[1]))
			{
				bgfx::destroy(_raytracerColorOutput[1]);
			}
			bgfx::destroy(_raytracerNormalsOutput);
			bgfx::destroy(_colorOutputSampler);
			bgfx::destroy(_terrainTexSampler);
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
			bgfx::destroy(_opticalDepthSampler);
			bgfx::destroy(_irradianceSampler);
			bgfx::destroy(_transmittanceSampler);
			bgfx::destroy(_opticalDepthTable);
			bgfx::destroy(_singleScatteringSampler);
			bgfx::destroy(_singleScatteringTable);
			bgfx::destroy(_sunRadToLumHandle);
			bgfx::destroy(_skyRadToLumHandle);
			bgfx::destroy(_cloudsPhaseSampler);
			bgfx::destroy(_heightmapSampler);
			bgfx::destroy(_cloudsSettings);
			bgfx::destroy(_textureArrayHandle);

			_screenSpaceQuad.destroy();

			// Shutdown bgfx.
			bgfx::shutdown();

			return 0;
		}

		// Called every frame
		bool update() override
		{
			// Polling about mouse, keyboard and window events from the "Input Thread"
			// Returns true when the window is up to close itself
			auto previousWidth = _windowWidth, previousHeight = _windowHeight;
			_mouseState.xrel = _mouseState.yrel = 0;
			if (!entry::processEvents(_windowWidth, _windowHeight, _debugFlags, _resetFlags, &_mouseState))
			{
				// Maybe reset window buffer size
				if (previousWidth != _windowWidth || previousHeight != _windowHeight)
				{
					// When the window resizes, we must update our bufers.
					// No need to call bgfx::reset() because entry::processEvents() does this automatically
					_screenSpaceQuad.destroy();
					cleanRenderedBuffers(_windowWidth, _windowHeight);
				}

				const uint8_t* utf8 = inputGetChar();
				char asciiKey = (nullptr != utf8) ? utf8[0] : 0;

				//
				// Graphics actions
				// 

				viewportActions();
				int maxSamples = DIRECT_SAMPLES_COUNT * *(int*)&Multisampling_indirect;
				bool tracingComplete;
				if (tracingComplete = (currentSample >= maxSamples))
				{
					if (!_pathTracingMode)
					{
						currentSample = 0;
						updateScene(asciiKey);
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
				auto bufferToDisplay =
					// If compute shader already completed its work, we can display its Output buffer. Otherwise we display the previous Output buffer.
					(_syncObj != nullptr && bgfx::syncComplete(_syncObj)) ? _outBufferIndex : 1 - _outBufferIndex;

				if (bufferToDisplay != 1 || DO_DOUBLE_BUFFERING)//If we are not double buffering, display only buffer #0
				{
					if (_debugNormals)
						bgfx::setTexture(0, _colorOutputSampler, _raytracerNormalsOutput);
					else
						bgfx::setTexture(0, _colorOutputSampler, _raytracerColorOutput[bufferToDisplay]);
					_screenSpaceQuad.draw();//Draw screen space quad with our shader program
					bgfx::setState(BGFX_STATE_WRITE_RGB);
					bgfx::submit(0, _displayingShaderProgram);
				}

				//
				// GUI Actions
				// 
				if (_showGUI)
				{
					// Supply mouse events to GUI library
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
#ifdef DRAW_RENDERING_PROGRESS
					if (_pathTracingMode && currentSample < maxSamples)
					{
						ImDrawList* list = ImGui::GetBackgroundDrawList();
						ImVec2 center = { (float)_windowWidth / 2, (float)_windowHeight / 2 };
						list->AddRectFilled(center, { center.x + 135, center.y + 20 }, 0xFF000000, 0);
						list->AddText(center, 0xFFFFFFFF, "Rendering in progress");
					}
#endif
					// Displays/Updates an innner dialog with debug and profiler information
					showDebugDialog(this);

					drawSettingsDialogUI();


					imguiEndFrame();
				}

				// Advance to next frame
				_frame = bgfx::frame();
				doNextScreenshotStep(tracingComplete);
				return true;
			}
			// update() should return false when we want the application to exit
			return false;
		}

		void updateScene(char asciiKey)
		{
			//Check for user actions
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
				DefaultScene::objectBuffer[1].position = vec3(Camera[0].x, Camera[0].y, Camera[0].z);
				break;
			case 't':
				_person.Camera.SetPosition(glm::vec3(-18253, 1709, 16070));
				_person.Camera.SetRotation(glm::vec3(-5, 113, 0));
				break;
			}
			if (_mouseLock)
			{
				auto nowTicks = SDL_GetPerformanceCounter();
				auto deltaTime = (float)((nowTicks - _lastTicks) * 1000 / (float)_performanceFrequency);
				_person.Update(_mouseLock, deltaTime, _mouseState);

				_lastTicks = nowTicks;
			}

			// Update scene objects
			updateClouds();
			updateLights();
		}

		void doNextScreenshotStep(bool tracingComplete)
		{
			if (_frame >= _screenshotFrame)
			{
				// Save the taken screenshot
				savePng();
				// Reset back to window dimensions
				cleanRenderedBuffers(_windowWidth, _windowHeight);
				bgfx::reset(_windowWidth, _windowHeight);
				// Return to normal rendering
				_screenshotFrame = SCREENSHOT_NEVER;//Do not take any screenshot at next frame
			}
			// Delayed screenshot request
			else if (_screenshotFrame == SCREENSHOT_AFTER_RENDER)
			{
				_screenshotFrame = SCREENSHOT_AFTER_RENDER_PENDING;
			}
			else if (tracingComplete && _screenshotFrame == SCREENSHOT_AFTER_RENDER_PENDING)
			{
				takeScreenshot();
			}
		}

		void updateClouds()
		{
			if (!_pathTracingMode)
			{
				DefaultScene::planetBuffer[0].clouds.position = bx::add(DefaultScene::planetBuffer[0].clouds.position, _cloudsWind);
			}
		}

		bool renderScene()
		{
			if (_syncObj == nullptr || bgfx::syncComplete(_syncObj))
			{
				updateBuffersAndSamplers();

#if _DEBUG
				updateDebugUniforms();
#endif

				computeShaderRaytracer();
				currentSample++;
				return true;
			}
			return false;
		}

		void updateLights()
		{
			auto& planet = DefaultScene::planetBuffer[0];
			updateLight(DefaultScene::objectBuffer[0], planet, DefaultScene::directionalLightBuffer[0], _sunAngle, _secondSunAngle);
			updateLight(DefaultScene::objectBuffer[1], planet, DefaultScene::directionalLightBuffer[1], _moonAngle, _secondMoonAngle);
		}
		void updateLight(Sphere& lightObject, Planet& planet, DirectionalLight& light, float angle, float secondAngle)
		{
			glm::quat rotQua(glm::vec3(angle, secondAngle, 0));
			glm::vec3 pos(0, bx::length(lightObject.position), 0);
			pos = rotQua * pos;
			lightObject.position = vec3(pos.x, pos.y, pos.z);

			light.direction = bx::normalize(bx::sub(lightObject.position, planet.center /* light direction is reverse */));
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
			if (DO_DOUBLE_BUFFERING)
				_outBufferIndex = _outBufferIndex == 0 ? 1 : 0;

			bgfx::setImage(0, _raytracerColorOutput[_outBufferIndex], 0, bgfx::Access::ReadWrite);
			bgfx::setImage(1, _raytracerNormalsOutput, 0, bgfx::Access::ReadWrite);
			bgfx::setImage(2, _raytracerDepthAlbedoBuffer, 0, bgfx::Access::ReadWrite);
			bgfx::dispatch(0, _computeShaderProgram, bx::ceil(_renderImageSize.width / 16.0f), bx::ceil(_renderImageSize.height / 16.0f));
			// Create synchronization fence which we will ask if the compute shader has finished
			_syncObj = bgfx::fenceSync();
		}

		void updateBuffers()
		{
			bgfx::update(_objectBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::objectBuffer, sizeof(DefaultScene::objectBuffer)));
			bgfx::update(_atmosphereBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::planetBuffer.data(), sizeof(DefaultScene::planetBuffer)));
			bgfx::update(_materialBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::materialBuffer, sizeof(DefaultScene::materialBuffer)));
			bgfx::update(_directionalLightBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::directionalLightBuffer.data(), sizeof(DefaultScene::directionalLightBuffer)));
		}

		void setBuffers()
		{
			updateBuffers();

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
			bgfx::setUniform(_cloudsSettings, CloudsSettings, sizeof(CloudsSettings) / sizeof(vec4));
		}

		void updateBuffersAndSamplers()
		{
			setBuffers();
			bgfx::setTexture(7, _terrainTexSampler, _textureArrayHandle);
			bgfx::setTexture(8, _heightmapSampler, _heightmapTextureHandle);
			bgfx::setTexture(9, _opticalDepthSampler, _opticalDepthTable, BGFX_SAMPLER_UVW_CLAMP);
			bgfx::setTexture(10, _cloudsPhaseSampler, _cloudsPhaseTextureHandle, BGFX_SAMPLER_UVW_MIRROR);
			bgfx::setTexture(11, _transmittanceSampler, _transmittanceTable, BGFX_SAMPLER_UVW_CLAMP);
			bgfx::setTexture(12, _irradianceSampler, _irradianceTable);
			bgfx::setTexture(13, _singleScatteringSampler, _singleScatteringTable);
		}

		void viewportActions()
		{
			float proj[16];
			bx::mtxOrtho(proj, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);

			// Set view 0 default viewport.
			bgfx::setViewTransform(0, NULL, proj);
			_tanFovY = bx::tan(_fovY * bx::acos(-1) / 180.f / 2.0f);

			bgfx::setViewRect(0, 0, 0, _renderImageSize.width, _renderImageSize.height);
			_tanFovX = (static_cast<float>(_renderImageSize.width) * _tanFovY) / static_cast<float>(_renderImageSize.height);

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
			//swap(_settingsBackup[6], CloudsSettings);
		}

		void drawFlagsGUI()
		{
			ImGui::SetNextWindowPos(
				ImVec2(0, 440)
				, ImGuiCond_FirstUseEver
			);
			ImGui::Begin("Flags");
			flags = *(int*)&HQSettings_flags;
			bool usePrecomputed = (flags & HQFlags_ATMO_COMPUTE) == 0;
			bool earthShadows = (flags & HQFlags_EARTH_SHADOWS) != 0;
			bool lightShafts = (flags & HQFlags_LIGHT_SHAFTS) != 0;
			ImGui::Checkbox("Precompute atmo", &usePrecomputed);
			if (usePrecomputed)
			{
				if (ImGui::SmallButton("Recompute"))
				{
					precompute();
				}
			}
			else
			{
				ImGui::InputInt("Samples", (int*)&Multisampling_perAtmospherePixel);
			}
			ImGui::Checkbox("Terrain shadows", &earthShadows);
			ImGui::Checkbox("Light shafts", &lightShafts);

			if (usePrecomputed)
			{
				flags &= ~HQFlags_ATMO_COMPUTE;
			}
			else
			{
				flags |= HQFlags_ATMO_COMPUTE;
			}

			if (earthShadows)
			{
				flags |= HQFlags_EARTH_SHADOWS;
			}
			else
			{
				flags &= ~HQFlags_EARTH_SHADOWS;
			}

			if (lightShafts)
			{
				flags |= HQFlags_LIGHT_SHAFTS;
			}
			else
			{
				flags &= ~HQFlags_LIGHT_SHAFTS;
			}
			HQSettings_flags = *(float*)&flags;
			ImGui::End();
		}

		void drawSettingsDialogUI()
		{
			Planet& singlePlanet = DefaultScene::planetBuffer[0];
			ImGui::SetNextWindowPos(
				ImVec2(0, 200)
				, ImGuiCond_FirstUseEver
			);

			ImGui::Begin("Planet");
			ImGui::PushItemWidth(120);
			ImGui::InputFloat3("Center", (float*)&singlePlanet.center);
			ImGui::InputFloat("Sun Angle", &_sunAngle);
			ImGui::SliderAngle("", &_sunAngle, 0, 180);
			ImGui::InputFloat("Second Angle", &_secondSunAngle);
			bool showMoon = singlePlanet.lastLight == 1;
			ImGui::Checkbox("Moon illuminance", &showMoon);
			if (showMoon)
			{
				ImGui::SliderAngle("Moon Angle", &_moonAngle, 0, 180);
				ImGui::SliderAngle("Sec M. Angle", &_secondMoonAngle, 0, 359);
				singlePlanet.lastLight = 1;
			}
			else
			{
				singlePlanet.lastLight = 0;
			}
			ImGui::PopItemWidth();
			static vec3 previousOzoneCoefs(0, 0, 0);
			bool ozone = singlePlanet.absorptionCoefficients.x != 0;
			if (ImGui::Checkbox("Ozone", &ozone))
			{
				if (ozone)
				{
					singlePlanet.absorptionCoefficients = previousOzoneCoefs;
				}
				else
				{
					previousOzoneCoefs = singlePlanet.absorptionCoefficients;
					singlePlanet.absorptionCoefficients = { 0,0,0 };
				}
			}

			ImGui::End();

			ImGui::SetNextWindowPos(
				ImVec2(0, 400),
				ImGuiCond_FirstUseEver
			);
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			ImGui::Begin("Coefficients");
			ImGui::PushItemWidth(90);
			ImGui::InputFloat("Rayleigh CoefR", &singlePlanet.rayleighCoefficients.x, 0, 0, "%e");
			ImGui::InputFloat("Rayleigh CoefG", &singlePlanet.rayleighCoefficients.y, 0, 0, "%e");
			ImGui::InputFloat("Rayleigh CoefB", &singlePlanet.rayleighCoefficients.z, 0, 0, "%e");
			ImGui::InputFloat3("Absorption", &singlePlanet.absorptionCoefficients.x, "%e");
			ImGui::InputFloat("Radius", &singlePlanet.surfaceRadius);
			ImGui::InputFloat("Amosphere Radius", &singlePlanet.atmosphereRadius);
			ImGui::InputFloat("Mountain Radius", &singlePlanet.mountainsRadius);
			ImGui::InputFloat("Mie Coef", &singlePlanet.mieCoefficient, 0, 0, "%e");
			ImGui::InputFloat("M.Asssymetry Factor", &singlePlanet.mieAsymmetryFactor);
			ImGui::InputFloat("M.Scale Height", &singlePlanet.mieScaleHeight);
			ImGui::InputFloat("R.Scale Height", &singlePlanet.rayleighScaleHeight);
			ImGui::InputFloat("O.Peak Height", &singlePlanet.ozonePeakHeight);
			ImGui::InputFloat("O.Trop Coef", &singlePlanet.ozoneTroposphereCoef, 0, 0, "%e");
			ImGui::InputFloat("O.Trop Const", &singlePlanet.ozoneTroposphereConst);
			ImGui::InputFloat("O.Strat Coef", &singlePlanet.ozoneStratosphereCoef, 0, 0, "%e");
			ImGui::InputFloat("O.Strat Const", &singlePlanet.ozoneStratosphereConst);
			ImGui::InputFloat("Sun Intensity", &DefaultScene::directionalLightBuffer[0].intensity);
			ImGui::InputFloat3("SunRadToLum", &SunRadianceToLuminance.x);
			ImGui::InputFloat3("SkyRadToLum", &SkyRadianceToLuminance.x);
			ImGui::PopItemWidth();
			ImGui::End();
			singlePlanet.atmosphereThickness = singlePlanet.atmosphereRadius - singlePlanet.surfaceRadius;

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
				swapSettingsBackup(); // This could set DIRECT_SAMPLES_COUNT to something different than 1
				cleanRenderedBuffers(_windowWidth, _windowHeight);
			}
			if (ImGui::Button("CheapQ"))
			{
				//Disable terrain
				prevLightSteps = LightSettings_shadowSteps;
				prevTerrainSteps = RaymarchingSteps.x;
				RaymarchingSteps.x = 0;
				LightSettings_shadowSteps = 0;
				//Disable atmosphere
				_debugAtmoOff = true;
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

			ImGui::SetNextWindowPos(ImVec2(230, 20), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			ImGui::Begin("Materials");
			ImGui::InputFloat("1", &PlanetMaterial.x);
			ImGui::InputFloat("2", &PlanetMaterial.y);
			ImGui::InputFloat("Gradient", &PlanetMaterial.w);
			ImGui::InputFloat("Detail", &RaymarchingSteps.y);
			ImGui::End();

			drawLightGUI();
			drawCloudsGUI();
			drawFlagsGUI();
		}

		void drawLightGUI()
		{
			ImGui::SetNextWindowPos(ImVec2(230, 40), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			ImGui::Begin("Light");
			ImGui::InputFloat("Exposure", &HQSettings_exposure);
			bool lum = SkyRadianceToLuminance.x != 10;
			static vec4 skyRLbackup;
			static vec4 sunRLbackup;
			if (ImGui::Checkbox("Render Luminance", &lum))
			{
				if (lum)
				{
					SkyRadianceToLuminance = skyRLbackup;
					SunRadianceToLuminance = sunRLbackup;
				}
				else
				{
					skyRLbackup = SkyRadianceToLuminance;
					sunRLbackup = SunRadianceToLuminance;
					SkyRadianceToLuminance = vec4(10, 10, 10);
					SunRadianceToLuminance = vec4(10, 10, 10);
				}
			}
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
			ImGui::SetNextWindowPos(ImVec2(230, 60), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			ImGui::Begin("Terrain");
			bool enabled = RaymarchingSteps.x != 0;
			if (ImGui::Checkbox("Enabled", &enabled))
			{
				if (enabled)
				{
					RaymarchingSteps.x = prevTerrainSteps;
					LightSettings_shadowSteps = prevLightSteps;
				}
				else
				{
					prevLightSteps = LightSettings_shadowSteps;
					prevTerrainSteps = RaymarchingSteps.x;
					RaymarchingSteps.x = 0;
					LightSettings_shadowSteps = 0;
				}
			}
			ImGui::InputFloat("Optimism", &QualitySettings_optimism, 0, 1);
			ImGui::InputFloat("Far Plane", &QualitySettings_farPlane);
			ImGui::InputFloat("MinStepSize", &QualitySettings_minStepSize);
			ImGui::InputInt("Planet Steps", (int*)&RaymarchingSteps.x);
			ImGui::InputFloat("Precision", &RaymarchingSteps.z, 0, 0, "%e");
			ImGui::InputFloat("LOD Div", &QualitySettings_lodPow);
			ImGui::InputFloat("LOD Bias", &RaymarchingSteps.w);
			ImGui::InputFloat("Normals", &PlanetMaterial.z);
			ImGui::End();
		}

		void drawCloudsGUI()
		{
			auto& cloudsLayer = DefaultScene::planetBuffer[0].clouds;
			ImGui::SetNextWindowPos(ImVec2(230, 80), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			ImGui::Begin("Clouds");
			ImGui::InputFloat("Steps", &Clouds_iter, 1, 1);
			ImGui::InputFloat("LightSteps", &Clouds_lightSteps, 1, 1);
			ImGui::InputFloat("TerrainSteps", &Clouds_terrainSteps, 1, 1);
			ImGui::InputFloat("LightFarPlane", &Clouds_lightFarPlane);

			ImGui::InputFloat("Coverage", &cloudsLayer.coverage);
			ImGui::InputFloat("SizeX", &cloudsLayer.sizeMultiplier.x, 0, 0, "%e");
			ImGui::InputFloat("SizeY", &cloudsLayer.sizeMultiplier.y, 0, 0, "%e");
			ImGui::InputFloat("SizeZ", &cloudsLayer.sizeMultiplier.z, 0, 0, "%e");
			ImGui::InputFloat("Render Distance", &Clouds_farPlane);
			ImGui::InputFloat("Downsampling", &Clouds_cheapDownsample);
			ImGui::InputFloat("Sample thres", &Clouds_sampleThres, 0, 0, "%e");
			ImGui::InputFloat("Threshold", &Clouds_cheapThreshold);
			ImGui::InputFloat3("Wind", &_cloudsWind.x, "%e");
			ImGui::InputFloat3("Pos", &cloudsLayer.position.x);
			ImGui::InputFloat("From height", &cloudsLayer.startRadius);
			ImGui::InputFloat("To height", &cloudsLayer.endRadius);
			cloudsLayer.layerThickness = cloudsLayer.endRadius - cloudsLayer.startRadius;
			ImGui::End();

			ImGui::SetNextWindowPos(ImVec2(230, 100), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
			ImGui::Begin("Cloud scattering");
			int currentDSDItem = _cloudsDSDUniformNotDisperse ? 0 : 1;
			const char* const items[] = { "Uniform", "Disperse" };
			if (ImGui::Combo("DSD", &currentDSDItem, (const char* const*)items, 2))
			{
				_cloudsDSDUniformNotDisperse = !_cloudsDSDUniformNotDisperse;
				if (_cloudsDSDUniformNotDisperse)
				{
					Clouds_aerosols *= 10;//Because uniform is too much uniform and we must add some aerosols to preserver realism
				}
				else
				{
					Clouds_aerosols *= 0.1;
				}
				cloudsMiePhaseFunction();
			}
			ImGui::PushItemWidth(120);
			ImGui::InputFloat("Density", &cloudsLayer.density, 0, 0, "%e");
			ImGui::InputFloat("Powder density", &Clouds_powderDensity, 0, 0, "%e");
			ImGui::InputFloat("Sharpness", &cloudsLayer.sharpness);
			ImGui::InputFloat("Lower gradient", &cloudsLayer.lowerGradient);
			ImGui::InputFloat("Upper Cutoff", &cloudsLayer.upperGradient);
			ImGui::InputFloat("Fade power", &Clouds_fadePower);
			ImGui::InputFloat("Scattering", &cloudsLayer.scatteringCoef, 0, 0, "%e");
			ImGui::InputFloat("Extinction", &cloudsLayer.extinctionCoef, 0, 0, "%e");
			ImGui::InputFloat("Aerosols", &Clouds_aerosols, 0.05, .1);
			ImGui::PopItemWidth();
			ImGui::End();
		}

		void drawPathTracerGUI()
		{
			ImGui::Begin("Path Tracer");
			if (ImGui::Button("Go RT Raytracing"))
			{
				swapSettingsBackup();
				cleanRenderedBuffers(_windowWidth, _windowHeight);
				_customScreenshotSize = false;
				_pathTracingMode = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("Re-render"))
			{
				cleanRenderedBuffers(_windowWidth, _windowHeight);
			}
			if (ImGui::Button("Save Image"))
			{
				requestRenderAndScreenshot();
			}
			ImGui::SameLine();
			ImGui::Checkbox("Custom size", &_customScreenshotSize);
			if (_customScreenshotSize)
			{
				ImGui::InputScalarN("W,H", ImGuiDataType_U16, &_renderImageSize.width, 2);
			}
			else
			{
				_renderImageSize = { (uint16_t)_windowWidth, (uint16_t)_windowHeight };
			}

			if (currentSample >= DIRECT_SAMPLES_COUNT * *(int*)&Multisampling_indirect)
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
			ImGui::PopItemWidth();
			ImGui::End();

			drawLightGUI();
			drawTerrainGUI();
			drawCloudsGUI();
			drawFlagsGUI();
		}

		// Called when the picture is completely in CPU memory
		void savePng()
		{
			bx::FileWriter writer;
			bx::Error err;
			std::ostringstream fileName;
			fileName << _frame;
			fileName << ".png";
			if (bx::open(&writer, fileName.str().c_str(), false, &err))
			{
				float* converted = new float[_renderImageSize.width * _renderImageSize.height];
				for (int x = 0; x < _renderImageSize.width * _renderImageSize.height * sizeof(float); x += sizeof(float))
				{
					auto dirSamp = DIRECT_SAMPLES_COUNT;
					//Tonemapping
					float r, g, b;
					//Convert RGB16F to 32bit float
					r = tmFunc(bx::halfToFloat(_readedTexture[x]) / dirSamp);
					g = tmFunc(bx::halfToFloat(_readedTexture[x + 1]) / dirSamp);
					b = tmFunc(bx::halfToFloat(_readedTexture[x + 2]) / dirSamp);

					_readedTexture[x] = bx::halfFromFloat(r);
					_readedTexture[x + 1] = bx::halfFromFloat(g);
					_readedTexture[x + 2] = bx::halfFromFloat(b);
					_readedTexture[x + 3] = bx::halfFromFloat(1.0);
				}
				//Convert RGBA16F to RGBA8
				bimg::imageConvert(entry::getAllocator(), converted, bimg::TextureFormat::RGBA8, _readedTexture, bimg::TextureFormat::RGBA16F, _renderImageSize.width, _renderImageSize.height, 1);
				//Write to PNG file
				bimg::imageWritePng(&writer, _renderImageSize.width, _renderImageSize.height, _renderImageSize.width * sizeof(float), converted, bimg::TextureFormat::RGBA8, false, &err);
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
				bgfx::destroy(_stagingBuffer);
			}
			else
			{
				bx::debugOutput("Screenshot failed.");
			}
		}

		void takeScreenshot()
		{
			_readedTexture = new uint16_t[_renderImageSize.width * _renderImageSize.height * 4/*RGBA channels*/];
			//Create buffer for reading by the CPU
			_stagingBuffer = bgfx::createTexture2D(_renderImageSize.width, _renderImageSize.height, false, 1, bgfx::TextureFormat::Enum::RGBA16F, BGFX_TEXTURE_READ_BACK);
			//Copy raytracer output into the buffer
			bgfx::blit(0, _stagingBuffer, 0, 0, _raytracerColorOutput[_outBufferIndex], 0, 0, _renderImageSize.width, _renderImageSize.height);
			//Read from GPU to CPU memory
			_screenshotFrame = bgfx::readTexture(_stagingBuffer, _readedTexture);
		}

		void requestRenderAndScreenshot()
		{
			if (_customScreenshotSize)
			{
				bgfx::reset(_renderImageSize.width, _renderImageSize.height);
				cleanRenderedBuffers(_renderImageSize.width, _renderImageSize.height);
			}
			_screenshotFrame = SCREENSHOT_AFTER_RENDER;
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

