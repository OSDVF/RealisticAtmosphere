﻿/*
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
#include <numeric>
#include <tinystl/vector.h>
#include <sstream>
#include <algorithm>
#include "Tonemapping.h"
#include "LUTs/PhaseFunctions.h"
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
#define SHADER_LOCAL_GROUP_COUNT 16

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
		// Number of frame at which a screenshot will be taken
		uint32_t _screenshotFrame = SCREENSHOT_NEVER;
		float _performanceFrequency = 0;
		float _renderTime = 0;
		uint64_t _renderStartedAt = 0;
		uint32_t _frame = 0;
		int _slicesCount = 3;
		int _currentChunk = 0;
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
		int _tonemappingType = 0;
		bool _showFlare = true;
		float _flareVisibility = 1.0f;
		int _flareOcclusionSamples = 40;
		entry::MouseState _mouseState;

		float _sunAngle = 1.5;//86 deg
		float _moonAngle = 0;
		float _secondSunAngle = -1.5;
		float _secondMoonAngle = 1;
		// Droplet size distribution
		bool _cloudsDSDUniformNotDisperse = false;
		vec3 _cloudsWind = vec3(-50, 0, 0);
		bool _moveClouds = false;

		ScreenSpaceQuad _screenSpaceQuad;/**< Output of raytracer (both the compute-shader variant and fragment-shader variant) */

		bool _debugNormals = false;
		bool _debugAtmoOff = false;
		bool _debugRm = false;
		bool _showGUI = true;
		bool _pathTracingMode = true;
		// TO "unlock" camera movement we must lock the mouse
		bool _mouseLock = false;
		float _tanFovY = 0;
		float _tanFovX = 0;
		float _fovY = 45;
		FirstPersonController _person;
		vec4 _settingsBackup[10];

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
		bgfx::UniformHandle _displaySettingsHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _multisamplingSettingsHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _qualitySettingsHandle = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _raytracerColorOutput = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle _raytracerNormalsOutput = BGFX_INVALID_HANDLE;
		//Depth is in R channel and albedo is packed in the G channel
		bgfx::TextureHandle _raytracerDepthAlbedoBuffer = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _colorOutputSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _depthBufferSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _normalBufferSampler = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _hqSettingsHandle = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle _lightSettings = BGFX_INVALID_HANDLE;
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
			// Initial realtime raytracing settings values.
			_settingsBackup[0] = QualitySettings;
			_settingsBackup[1] = MultisamplingSettings;
			_settingsBackup[2] = RaymarchingSteps;
			_settingsBackup[3] = LightSettings[0];
			_settingsBackup[4] = LightSettings[1];
			_settingsBackup[5] = LightSettings[2];
			_settingsBackup[6] = HQSettings;
			_settingsBackup[7] = CloudsSettings[0];
			_settingsBackup[8] = CloudsSettings[1];
			_settingsBackup[9] = CloudsSettings[2];

			//
			// Pathtracing must have higher quality
			//

			// Display earth shadows and compute atmosphere exacly
			*(uint32_t*)&HQSettings_flags |= HQFlags_EARTH_SHADOWS | HQFlags_ATMO_COMPUTE;

			// 300 planet raymarching steps
			*(int*)&RaymarchingSteps.x = 300;
			// 200 cloud raymarching steps
			Clouds_iter = 200;
			Clouds_terrainSteps = 80;
			Clouds_lightSteps = 8;
			Clouds_lightFarPlane = 40000;
			LightSettings_farPlane = 10000;
			*(int*)&LightSettings_shadowSteps = 70;

			applyPreset(0);//Set default player and sun positions

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
			_displaySettingsHandle = bgfx::createUniform("DisplaySettings", bgfx::UniformType::Vec4);
			_cameraHandle = bgfx::createUniform("Camera", bgfx::UniformType::Vec4, 4);//It is an array of 4 vec4
			_sunRadToLumHandle = bgfx::createUniform("SunRadianceToLuminance", bgfx::UniformType::Vec4);
			_skyRadToLumHandle = bgfx::createUniform("SkyRadianceToLuminance", bgfx::UniformType::Vec4);
			_planetMaterialHandle = bgfx::createUniform("PlanetMaterial", bgfx::UniformType::Vec4);
			_raymarchingStepsHandle = bgfx::createUniform("RaymarchingSteps", bgfx::UniformType::Vec4);
			_colorOutputSampler = bgfx::createUniform("colorOutput", bgfx::UniformType::Sampler);
			_normalBufferSampler = bgfx::createUniform("normalsBuffer", bgfx::UniformType::Sampler);
			_depthBufferSampler = bgfx::createUniform("depthBuffer", bgfx::UniformType::Sampler);
			_heightmapSampler = bgfx::createUniform("heightmapTexture", bgfx::UniformType::Sampler);
			_cloudsPhaseSampler = bgfx::createUniform("cloudsMieLUT", bgfx::UniformType::Sampler);
			_opticalDepthSampler = bgfx::createUniform("opticalDepthTable", bgfx::UniformType::Sampler);
			_irradianceSampler = bgfx::createUniform("irradianceTable", bgfx::UniformType::Sampler);
			_transmittanceSampler = bgfx::createUniform("transmittanceTable", bgfx::UniformType::Sampler);
			_singleScatteringSampler = bgfx::createUniform("singleScatteringTable", bgfx::UniformType::Sampler);
			_terrainTexSampler = bgfx::createUniform("terrainTextures", bgfx::UniformType::Sampler);
#if _DEBUG
			_debugAttributesHandle = bgfx::createUniform("debugAttributes", bgfx::UniformType::Vec4);
#endif
			_timeHandle = bgfx::createUniform("time", bgfx::UniformType::Vec4);
			_multisamplingSettingsHandle = bgfx::createUniform("MultisamplingSettings", bgfx::UniformType::Vec4);
			_qualitySettingsHandle = bgfx::createUniform("QualitySettings", bgfx::UniformType::Vec4);
			_hqSettingsHandle = bgfx::createUniform("HQSettings", bgfx::UniformType::Vec4);
			_lightSettings = bgfx::createUniform("LightSettings", bgfx::UniformType::Vec4, sizeof(LightSettings) / sizeof(vec4));
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
			DefaultScene::materialBuffer[0].emission = /*sun emission calcualation from its angular radius*/
				bx::div(
					DefaultScene::directionalLightBuffer[0].irradiance,
					M_PI * DefaultScene::sunAngularRadius * DefaultScene::sunAngularRadius
				);

			// Render optical depth, transmittance and direct irradiance textures
			precompute();

			// Create Immediate GUI graphics context
			imguiCreate();
			_lastTicks = SDL_GetPerformanceCounter();
			_performanceFrequency = (float)SDL_GetPerformanceFrequency();

			// Prepare scene for the first time
			updateScene();
		}

		void heightMap()
		{
			_heightmapTextureHandle = bgfx::createTexture2D(8192, 8192, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_COMPUTE_WRITE);
			bgfx::setImage(0, _heightmapTextureHandle, 0, bgfx::Access::Write);
			bgfx::dispatch(0, _heightmapShaderProgram, bx::ceil(8192 / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(8192 / float(SHADER_LOCAL_GROUP_COUNT)));
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
				_opticalDepthTable = bgfx::createTexture2D(128, 64, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_UVW_CLAMP);
			}
			//steps are locked to 300
			updateBuffers();
			bgfx::setImage(0, _opticalDepthTable, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA32F);
			bgfx::setBuffer(1, _atmosphereBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(2, _directionalLightBufferHandle, bgfx::Access::Read);
			bgfx::dispatch(0, opticalProgram, bx::ceil(128 / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(64 / float(SHADER_LOCAL_GROUP_COUNT)));

			bgfx::ShaderHandle precomputeTransmittance = loadShader("Transmittance.comp");
			bgfx::ProgramHandle transmittanceProgram = bgfx::createProgram(precomputeTransmittance);
			if (!bgfx::isValid(_transmittanceTable))
			{
				_transmittanceTable = bgfx::createTexture2D(256, 64, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_UVW_CLAMP);
			}
			bgfx::setImage(0, _transmittanceTable, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA32F);
			bgfx::setBuffer(1, _atmosphereBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(2, _directionalLightBufferHandle, bgfx::Access::Read);
			bgfx::dispatch(0, transmittanceProgram, bx::ceil(256 / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(64 / float(SHADER_LOCAL_GROUP_COUNT)));
Tw
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
			setBuffersAndUniforms();
			bgfx::setTexture(7, _transmittanceSampler, _transmittanceTable);
			bgfx::dispatch(0, scatteringProgram, bx::ceil(SCATTERING_TEXTURE_WIDTH / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(SCATTERING_TEXTURE_HEIGHT / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(SCATTERING_TEXTURE_DEPTH / 4.0f));

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
			bgfx::dispatch(0, irradianceProgram, bx::ceil(64 / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(16 / float(SHADER_LOCAL_GROUP_COUNT)), DefaultScene::directionalLightBuffer.size());

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
			updateVirtualCameraPerspective();
			// Delete previous buffers
			if (bgfx::isValid(_raytracerColorOutput))
			{
				bgfx::destroy(_raytracerColorOutput);
				bgfx::destroy(_raytracerNormalsOutput);
				bgfx::destroy(_raytracerDepthAlbedoBuffer);
			}
			// Create new
			_raytracerColorOutput = bgfx::createTexture2D(newWidth, newHeight, false, 1, bgfx::TextureFormat::RGBA16F,
				BGFX_TEXTURE_COMPUTE_WRITE);

			_raytracerNormalsOutput = bgfx::createTexture2D(newWidth, newHeight, false, 1,
				bgfx::TextureFormat::RGBA16F,
				BGFX_TEXTURE_COMPUTE_WRITE);
			_raytracerDepthAlbedoBuffer = bgfx::createTexture2D(newWidth, newHeight, false, 1,
				bgfx::TextureFormat::RG32F,
				BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_UVW_CLAMP);

			currentSample = 0;//Reset sample counter - otherwise path tracing would continue with previous samples
		}

		void updateVirtualCameraPerspective()
		{
			// "Real" camera which does rasterization is orthogonal.
			// "Virtual" camera is used in raytracer
			_person.Camera.SetProjectionMatrixPerspective(_fovY, (float)_renderImageSize.width / (float)_renderImageSize.height, 1.f, DefaultScene::sunObjectDistance);
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
			bgfx::destroy(_qualitySettingsHandle);
			bgfx::destroy(_heightmapTextureHandle);
			bgfx::destroy(_raytracerDepthAlbedoBuffer);
			bgfx::destroy(_raytracerColorOutput);
			bgfx::destroy(_raytracerNormalsOutput);
			bgfx::destroy(_colorOutputSampler);
			bgfx::destroy(_depthBufferSampler);
			bgfx::destroy(_normalBufferSampler);
			bgfx::destroy(_terrainTexSampler);
			bgfx::destroy(_computeShaderHandle);
			bgfx::destroy(_computeShaderProgram);
			bgfx::destroy(_displayingShaderProgram);
			bgfx::destroy(_materialBufferHandle);
			bgfx::destroy(_directionalLightBufferHandle);
			bgfx::destroy(_atmosphereBufferHandle);
			bgfx::destroy(_objectBufferHandle);
			bgfx::destroy(_cameraHandle);
#if _DEBUG
			bgfx::destroy(_debugAttributesHandle);
#endif
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
				//Check for user actions
				uint8_t modifiers = inputGetModifiersState();
				switch (asciiKey)
				{
				case 'l':
					_mouseLock = false;
					SDL_CaptureMouse(SDL_FALSE);
					SDL_SetRelativeMouseMode(SDL_FALSE);
					break;
				case 'k':
					_mouseLock = true;
					SDL_CaptureMouse(SDL_TRUE);
					SDL_SetRelativeMouseMode(SDL_TRUE);
					break;
				case 'g':
					_showGUI = true;
					break;
				case 'f':
					_showGUI = false;
					break;
				case 'r':
					DefaultScene::objectBuffer[4].position = vec3(Camera[0].x, Camera[0].y, Camera[0].z);
					break;
				default:
					if (modifiers & entry::Modifier::LeftCtrl)
					{
						if (inputGetKeyState(entry::Key::Key1))
						{
							applyPreset(0);
						}
						else if (inputGetKeyState(entry::Key::Key2))
						{
							applyPreset(1);
						}
						else if (inputGetKeyState(entry::Key::Key3))
						{
							applyPreset(2);
						}
						else if (inputGetKeyState(entry::Key::Key4))
						{
							applyPreset(3);
						}
						else if (inputGetKeyState(entry::Key::Key5))
						{
							applyPreset(4);
						}
						else if (inputGetKeyState(entry::Key::Key6))
						{
							applyPreset(5);
						}
					}
					break;
				}
				//
				// Displaying actions
				//
				viewportActions();
				bgfx::setTexture(0, _colorOutputSampler, _raytracerColorOutput);
				bgfx::setTexture(1, _normalBufferSampler, _raytracerNormalsOutput);
				bgfx::setTexture(2, _depthBufferSampler, _raytracerDepthAlbedoBuffer);
				_screenSpaceQuad.draw();//Draw screen space quad with our shader program
				bgfx::setState(BGFX_STATE_WRITE_RGB);

				setDisplaySettings();
				bgfx::submit(0, _displayingShaderProgram);

				int maxSamples = DIRECT_SAMPLES_COUNT * *(int*)&Multisampling_indirect;

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
					ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
					showDebugDialog(this);

					drawSettingsDialogUI();

					imguiEndFrame();
				}


				// Advance to next frame
				_frame = bgfx::frame();
				//
				// Rendering actions
				// 

				bool tracingComplete;
				auto stats = bgfx::getStats();
				if (currentSample == 0 || !_pathTracingMode && _currentChunk == 0)
				{
					_renderStartedAt = stats->gpuTimeBegin;
				}
				if (tracingComplete = (currentSample >= maxSamples))
				{
					if (!_pathTracingMode)
					{
						currentSample = 0;
						renderScene();
						_renderTime = (stats->gpuTimeEnd - _renderStartedAt) * 1000 / (float)stats->gpuTimerFreq;
						updateScene();
					}
					else if (currentSample == maxSamples)/*add last frame latency*/
					{
						// Rendering complete
						_renderTime = (stats->gpuTimeEnd - _renderStartedAt) * 1000 / (float)stats->gpuTimerFreq;
						currentSample = INT_MAX;
					}
				}
				else
				{
					renderScene();
				}
				doNextScreenshotStep(tracingComplete);
				return true;
			}
			// update() should return false when we want the application to exit
			return false;
		}

		void setDisplaySettings()
		{
			auto sunPos = DefaultScene::objectBuffer[0].position;
			glm::vec3 screenSpaceSun;
			_person.Camera.ProjectWorldToScreen(glm::vec3(sunPos.x, sunPos.y, sunPos.z), glm::vec4(0, 0, _renderImageSize.width, _renderImageSize.height),/*out*/ screenSpaceSun);
			auto aspectRatio = (float(_renderImageSize.width) / (float)_renderImageSize.height);
			screenSpaceSun.x = (-screenSpaceSun.x / (screenSpaceSun.z * _renderImageSize.width) + 0.5) * aspectRatio;
			screenSpaceSun.y = screenSpaceSun.y / (screenSpaceSun.z * _renderImageSize.height) - 0.5;

			float flareBrightness = 0;
			if (screenSpaceSun.z > 0 && _showFlare)
			{
				// If the sun is not behind the camera
				flareBrightness = std::fmax(
					bx::dot(Camera[1].toVec3(), DefaultScene::directionalLightBuffer[0].direction) * _flareVisibility,
					0);
			}

			uint32_t packedTonemappingAndOcclusion =
				(_tonemappingType & 0xFFFF //lower 16 bits*
					| _flareOcclusionSamples << 16); //higher 16 bits
			vec4 displaySettValue(
				*(float*)&packedTonemappingAndOcclusion, flareBrightness,
				screenSpaceSun.x, screenSpaceSun.y
			);
			bgfx::setUniform(_displaySettingsHandle, &displaySettValue);
			bgfx::setUniform(_hqSettingsHandle, &HQSettings);
		}

		void updateScene()
		{
			if (_mouseLock)
			{
				auto nowTicks = SDL_GetPerformanceCounter();
				auto deltaTime = (nowTicks - _lastTicks) * 1000 / _performanceFrequency;
				_person.Update(_mouseLock, deltaTime, _mouseState);

				_lastTicks = nowTicks;
			}

			// Update scene objects
			updateClouds();
			updateLights();
			// And save their values into buffers
			updateBuffers();
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
			if (!_pathTracingMode && _moveClouds)
			{
				DefaultScene::planetBuffer[0].clouds.position = bx::add(DefaultScene::planetBuffer[0].clouds.position, _cloudsWind);
			}
		}

		float firstMultipleOfXGreaterThanY(float x, float y)
		{
			return (bx::floor(y / x) + 1) * x;
		}

		void renderScene()
		{
			int chunkSizeX = bx::ceil(_renderImageSize.width / (float)_slicesCount);
			chunkSizeX = firstMultipleOfXGreaterThanY(SHADER_LOCAL_GROUP_COUNT, chunkSizeX);
			int chunkSizeY = bx::ceil(_renderImageSize.height / (float)_slicesCount);
			chunkSizeY = firstMultipleOfXGreaterThanY(SHADER_LOCAL_GROUP_COUNT, chunkSizeY);
			int chunkXstart = chunkSizeX * (_currentChunk % _slicesCount);
			int chunkYstart = chunkSizeY * (_currentChunk / _slicesCount);
			SunRadianceToLuminance.w = *(float*)&chunkXstart;
			SkyRadianceToLuminance.w = *(float*)&chunkYstart;

			setBuffersAndSamplers();

#if _DEBUG
			updateDebugUniforms();
#endif

			computeShaderRaytracer(chunkSizeX / SHADER_LOCAL_GROUP_COUNT, chunkSizeY / SHADER_LOCAL_GROUP_COUNT);
			_currentChunk++;
			if (_currentChunk >= _slicesCount * _slicesCount)
			{
				_currentChunk = 0;
				currentSample++;
			}
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
			glm::vec3 pos(0, bx::length(bx::sub(lightObject.position, planet.center)), 0);
			pos = rotQua * pos;
			lightObject.position = bx::add(vec3(pos.x, pos.y, pos.z), planet.center);

			light.direction = bx::normalize(lightObject.position);
		}
#if _DEBUG
		void updateDebugUniforms()
		{
			_debugAttributesResult = vec4(_debugNormals ? 1 : 0, _debugRm ? 1 : 0, _debugAtmoOff ? 1 : 0, 0);
			bgfx::setUniform(_debugAttributesHandle, &_debugAttributesResult);
		}
#endif
		void computeShaderRaytracer(uint32_t numX, uint32_t numY)
		{
			bgfx::setImage(0, _raytracerColorOutput, 0, bgfx::Access::ReadWrite);
			bgfx::setImage(1, _raytracerNormalsOutput, 0, bgfx::Access::ReadWrite);
			bgfx::setImage(2, _raytracerDepthAlbedoBuffer, 0, bgfx::Access::ReadWrite);
			bgfx::dispatch(0, _computeShaderProgram, numX, numY);
		}

		void updateBuffers()
		{
			bgfx::update(_objectBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::objectBuffer, sizeof(DefaultScene::objectBuffer)));
			bgfx::update(_atmosphereBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::planetBuffer.data(), sizeof(DefaultScene::planetBuffer)));
			bgfx::update(_materialBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::materialBuffer, sizeof(DefaultScene::materialBuffer)));
			bgfx::update(_directionalLightBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::directionalLightBuffer.data(), sizeof(DefaultScene::directionalLightBuffer)));
		}

		void setBuffersAndUniforms()
		{
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
			bgfx::setUniform(_lightSettings, LightSettings, sizeof(LightSettings) / sizeof(vec4));
			bgfx::setUniform(_sunRadToLumHandle, &SunRadianceToLuminance);
			bgfx::setUniform(_skyRadToLumHandle, &SkyRadianceToLuminance);
			bgfx::setUniform(_cloudsSettings, CloudsSettings, sizeof(CloudsSettings) / sizeof(vec4));
		}

		void setBuffersAndSamplers()
		{
			setBuffersAndUniforms();
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
			swap(_settingsBackup[3], LightSettings[0]);
			swap(_settingsBackup[4], LightSettings[1]);
			swap(_settingsBackup[5], LightSettings[2]);
			swap(_settingsBackup[6], HQSettings);
			swap(_settingsBackup[7], CloudsSettings[0]);
			swap(_settingsBackup[8], CloudsSettings[1]);
			swap(_settingsBackup[9], CloudsSettings[2]);
		}

		void drawPerformanceGUI()
		{
			if (ImGui::TreeNodeEx("Performance", ImGuiTreeNodeFlags_DefaultOpen))
			{
				flags = *(uint32_t*)&HQSettings_flags;
				bool usePrecomputed = (flags & HQFlags_ATMO_COMPUTE) == 0;
				bool earthShadows = (flags & HQFlags_EARTH_SHADOWS) != 0;
				bool lightShafts = (flags & HQFlags_LIGHT_SHAFTS) != 0;
				bool indirectApprox = (flags & HQFlags_INDIRECT_APPROX) != 0;
				bool reduceBanding = (flags & HQFlags_REDUCE_BANDING) != 0;
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
				ImGui::Checkbox("Light shafts", &lightShafts);
				ImGui::Checkbox("Approximate skylight", &indirectApprox);
				ImGui::Checkbox("Reduce banding", &reduceBanding);
				if (reduceBanding)
				{
					ImGui::PushItemWidth(100);
					ImGui::InputFloat("Primary factor", &LightSettings_deBanding);
					ImGui::InputFloat("Light factor", &Clouds_deBanding);
					ImGui::InputFloat("Cone width", &Clouds_cone);
					ImGui::PopItemWidth();
					flags |= HQFlags_REDUCE_BANDING;
				}
				else
				{
					flags &= ~HQFlags_REDUCE_BANDING;
				}

				if (usePrecomputed)
				{
					flags &= ~HQFlags_ATMO_COMPUTE;
				}
				else
				{
					flags |= HQFlags_ATMO_COMPUTE;
				}

				ImGui::Checkbox("Terrain shadows", &earthShadows);
				if (earthShadows)
				{
					ImGui::InputFloat("Cascade distance", &LightSettings_shadowCascade);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("First cascade of shadows is more precise, second has lower quality (quite blocky)");
					}
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

				if (indirectApprox)
				{
					flags |= HQFlags_INDIRECT_APPROX;
				}
				else
				{
					flags &= ~HQFlags_INDIRECT_APPROX;
				}
				HQSettings_flags = *(float*)&flags;
				ImGui::TreePop();
			}
			if (ImGui::TreeNode("Debug"))
			{
				ImGui::Checkbox("Debug Normals", &_debugNormals);
				ImGui::Checkbox("Debug RayMarch", &_debugRm);
				ImGui::Checkbox("Hide atmosphere", &_debugAtmoOff);

				ImGui::PushItemWidth(90);
				ImGui::InputInt("Realtime multisampling", (int*)&HQSettings_directSamples);
				ImGui::PopItemWidth();
				ImGui::TreePop();
			}
		}

		void drawSettingsDialogUI()
		{
			ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(_windowWidth / 4, _windowHeight), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowBgAlpha(.7f);
			if (_pathTracingMode)
			{
				drawPathTracerGUI();
				return;
			}
			ImGui::Begin("Realtime Preview");
			if (ImGui::Button("Go Path Tracing"))
			{
				_pathTracingMode = true;
				_currentChunk = 0;
				swapSettingsBackup(); // This may set DIRECT_SAMPLES_COUNT to something different than 1
				cleanRenderedBuffers(_windowWidth, _windowHeight);
			}
			ImGui::Text("Latency %f ms", _renderTime);
			ImGui::SetNextItemWidth(90);
			inputSlicesCount();
			if (ImGui::TreeNodeEx("Controls", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Text(
					"Camera: K-Unlock, L-Lock\n"
					"Move: WASD\n"
					"Sprint: Shift\n"
					"GUI: F-Hide, G-Show\n"
					"Presets: Ctrl + 1-5\n"
					"Place sphere: R"
				);
				ImGui::TreePop();
			}
			drawPlanetGUI();
			drawTerrainGUI();
			drawLightGUI();
			drawCloudsGUI();
			drawPerformanceGUI();
			ImGui::End();
		}

		void drawPlanetGUI()
		{
			if (ImGui::TreeNodeEx("Camera", _pathTracingMode ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::SetNextItemWidth(100);
				if (ImGui::InputFloat("FOV", &_fovY))
				{
					updateVirtualCameraPerspective();
				}
				ImGui::BeginGroup();
				ImGui::PushItemWidth(90);
				ImGui::InputFloat("Speed", &_person.WalkSpeed, 1);
				ImGui::InputFloat("RunSpeed", &_person.RunSpeed, 10);
				ImGui::PopItemWidth();
				ImGui::SetNextItemWidth(120);
				ImGui::SliderFloat("Sensitivity", &_person.Camera.Sensitivity, 0, 5.0f);
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
				ImGui::EndGroup();
				if (ImGui::IsItemHovered() && _pathTracingMode)
				{
					ImGui::SetTooltip("Changing these values makes sense only in Realtime Raytracing mode");
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNode("Planet"))
			{
				Planet& singlePlanet = DefaultScene::planetBuffer[0];
				ImGui::SetNextItemWidth(150);
				ImGui::InputFloat3("Center", (float*)&singlePlanet.center);
				ImGui::InputFloat("Sun Angle", &_sunAngle);
				ImGui::SetNextItemWidth(150);
				ImGui::SliderAngle("Y", &_sunAngle, 0, 180);
				ImGui::InputFloat("Second Angle", &_secondSunAngle);
				ImGui::SetNextItemWidth(150);
				ImGui::SliderAngle("X", &_secondSunAngle, 0, 359);

				ImGui::PushItemWidth(120);
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
				ImGui::PushItemWidth(100);
				ImGui::InputFloat("Radius", &singlePlanet.surfaceRadius);
				ImGui::InputFloat("Amosphere Radius", &singlePlanet.atmosphereRadius);
				ImGui::InputFloat("Mountain Radius", &singlePlanet.mountainsRadius);
				static vec3 previousOzoneCoefs(0, 0, 0);
				bool ozone = singlePlanet.absorptionCoefficients.x != 0;
				ImGui::BeginGroup();
				if (ImGui::TreeNode("Scattering"))
				{
					ImGui::PushItemWidth(90);
					ImGui::InputFloat("Rayleigh ScatR", &singlePlanet.rayleighCoefficients.x, 0, 0, "%e");
					ImGui::InputFloat("Rayleigh ScatG", &singlePlanet.rayleighCoefficients.y, 0, 0, "%e");
					ImGui::InputFloat("Rayleigh ScatB", &singlePlanet.rayleighCoefficients.z, 0, 0, "%e");
					ImGui::InputFloat("Mie Scat", &singlePlanet.mieCoefficient, 0, 0, "%e");
					ImGui::InputFloat("M.Asssymetry Factor", &singlePlanet.mieAsymmetryFactor);
					ImGui::InputFloat("M.Scale Height", &singlePlanet.mieScaleHeight);
					ImGui::InputFloat("R.Scale Height", &singlePlanet.rayleighScaleHeight);
					if (ImGui::TreeNode("Ozone"))
					{
						if (ImGui::Checkbox("O. Enable", &ozone))
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
						ImGui::PushItemWidth(150);
						ImGui::InputFloat3("Extinct.", &singlePlanet.absorptionCoefficients.x, "%e");
						ImGui::PopItemWidth();
						ImGui::InputFloat("O.Peak Height", &singlePlanet.ozonePeakHeight);
						ImGui::InputFloat("O.Trop Coef", &singlePlanet.ozoneTroposphereCoef, 0, 0, "%e");
						ImGui::InputFloat("O.Trop Const", &singlePlanet.ozoneTroposphereConst);
						ImGui::InputFloat("O.Strat Coef", &singlePlanet.ozoneStratosphereCoef, 0, 0, "%e");
						ImGui::InputFloat("O.Strat Const", &singlePlanet.ozoneStratosphereConst);
						ImGui::TreePop();
					}
					ImGui::InputFloat("Sun Intensity", &DefaultScene::directionalLightBuffer[0].intensity);
					ImGui::PopItemWidth();
					ImGui::PopItemWidth();
					ImGui::InputFloat3("SunRadToLum", &SunRadianceToLuminance.x, "%.2f");
					ImGui::InputFloat3("SkyRadToLum", &SkyRadianceToLuminance.x, "%.2f");
					ImGui::TreePop();
				}
				ImGui::EndGroup();
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("After editing values, hit Recompute button in Performance section");
				}
				ImGui::TreePop();

				singlePlanet.atmosphereThickness = singlePlanet.atmosphereRadius - singlePlanet.surfaceRadius;
			}
		}

		void drawLightGUI()
		{
			if (ImGui::TreeNode("Light"))
			{
				ImGui::PushItemWidth(100);
				const char* const tmTypes[] = { "Exposure", "Reinhard", "Custom Reinhard", "ACES", "Uneral", "Uchimura", "Lottes", "No Tonemapping" };
				ImGui::Combo("Tonemapping", &_tonemappingType, tmTypes, sizeof(tmTypes) / sizeof(const char*));
				switch (_tonemappingType)
				{
				case 0:	/*fallthrough*/
				case 5:
					ImGui::InputFloat("Exposure", &HQSettings_exposure);
					break;
				case 2: /*fallthrough*/
				case 6:
					ImGui::InputFloat("White point", &HQSettings_exposure);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Defines luminance value at which the image will be completely white");
					}
					break;
				}
				ImGui::InputFloat("Shadow near plane", &LightSettings_shadowNearPlane);
				ImGui::Checkbox("Lens Flare", &_showFlare);
				if (_showFlare)
				{
					ImGui::SliderFloat("Visibility", &_flareVisibility, 0, 2);
					ImGui::PushItemWidth(90);
					ImGui::InputInt("Occlusion samples", &_flareOcclusionSamples);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Creates screen-space light shafts."
							"\n1 = only test for sun visibility."
							"\n0 = always show flare"
							"\nMultisampling is not applied to them because they are rendered after the whole image");
					ImGui::PopItemWidth();
					if (_flareOcclusionSamples < 0) _flareOcclusionSamples = 0;
				}

				bool lum = SkyRadianceToLuminance.x != 10;
				static vec4 skyRLbackup;
				static vec4 sunRLbackup;
				if (ImGui::Checkbox("Photometric Units", &lum))
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
				if (ImGui::TreeNode("Shafts Raymarching"))
				{
					ImGui::InputFloat("Precision", &LightSettings_precision, 0, 0, "%e");
					ImGui::InputFloat("Far Plane", &LightSettings_farPlane);
					ImGui::SetNextItemWidth(90);
					ImGui::InputInt("Shadow steps", (int*)&LightSettings_shadowSteps);
					ImGui::InputFloat("Hit Optimism", &LightSettings_terrainOptimMult);
					static float prevShadowedThreshold = 0;
					static float prevNoRayThres = 0.4;
					static float prevViewThres = -1;
					static float prevCutoffDist = 0;
					bool empiricOptimizations = RaymarchingSteps.y != 0;
					if (ImGui::Checkbox("Empiric optimizations", &empiricOptimizations))
					{
						swap(prevShadowedThreshold, RaymarchingSteps.y);
						swap(prevNoRayThres, LightSettings_noRayThres);
						swap(prevViewThres, LightSettings_viewThres);
						swap(prevCutoffDist, LightSettings_cutoffDist);
					}
					if (empiricOptimizations)
					{
						ImGui::InputFloat("ShadowedThres", &RaymarchingSteps.y);
						if (RaymarchingSteps.y < 1.0f)
							RaymarchingSteps.y = 1.0f;
						ImGui::InputFloat("NoRayThres", &LightSettings_noRayThres);
						ImGui::InputFloat("ViewThres", &LightSettings_viewThres);
						ImGui::InputFloat("Gradient", &LightSettings_gradient);
						ImGui::InputFloat("CutoffDist", &LightSettings_cutoffDist);
					}
					ImGui::TreePop();
				}
				ImGui::PopItemWidth();
				ImGui::TreePop();
			}
		}

		void drawTerrainGUI()
		{
			if (ImGui::TreeNode("Terrain"))
			{
				bool enabled = RaymarchingSteps.x != 0;
				ImGui::PushItemWidth(120);
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
				if (ImGui::TreeNode("Materials"))
				{
					ImGui::InputFloat("1", &PlanetMaterial.x);
					ImGui::InputFloat("2", &PlanetMaterial.y);
					ImGui::InputFloat("Gradient", &PlanetMaterial.w);
					ImGui::TreePop();
				}
				ImGui::PopItemWidth();
				ImGui::TreePop();
			}
		}

		void drawCloudsGUI()
		{
			if (ImGui::TreeNode("Clouds"))
			{
				CloudLayer& cloudsLayer = DefaultScene::planetBuffer[0].clouds;
				ImGui::InputFloat3("Size", &cloudsLayer.sizeMultiplier.x, "%.1e");
				ImGui::InputFloat3("Pos", &cloudsLayer.position.x);
				ImGui::PushItemWidth(100);
				if (ImGui::TreeNode("Settings"))
				{
					ImGui::Checkbox("Wind", &_moveClouds);
					if (_moveClouds)
					{
						ImGui::InputFloat3("Speed", &_cloudsWind.x, "%e");
					}
					ImGui::InputFloat("Steps", &Clouds_iter, 1, 1, "%.0f");
					ImGui::InputFloat("TerrainSteps", &Clouds_terrainSteps, 1, 1, "%.0f");
					ImGui::InputFloat("Render Distance", &Clouds_farPlane);
					if (ImGui::TreeNode("Cl. Lighting"))
					{
						ImGui::InputFloat("LightSteps", &Clouds_lightSteps, 1, 1, "%.0f");
						ImGui::InputFloat("LightFarPlane", &Clouds_lightFarPlane);
						if (flags & HQFlags_REDUCE_BANDING)
						{
							ImGui::Text("de-Banding enabled");
							ImGui::PushItemWidth(100);
							ImGui::InputFloat("Primary factor", &LightSettings_deBanding);
							ImGui::InputFloat("Light factor", &Clouds_deBanding);
							ImGui::InputFloat("Cone", &Clouds_cone);
							ImGui::PopItemWidth();
						}
						else
						{
							ImGui::Text("de-Banding can be enabled\nin the Performance section.");
						}
						ImGui::TreePop();
					}

					ImGui::InputFloat("Coverage", &cloudsLayer.coverage);

					if (ImGui::TreeNode("Downsampling"))
					{
						ImGui::InputFloat("Amount", &Clouds_cheapDownsample);
						ImGui::InputFloat("Threshold", &Clouds_cheapThreshold);
						ImGui::TreePop();
					}

					ImGui::InputFloat("Sample thres", &Clouds_sampleThres, 0, 0, "%e");
					ImGui::InputFloat("Layer start", &cloudsLayer.startRadius);
					ImGui::InputFloat("Layer end", &cloudsLayer.endRadius);
					cloudsLayer.layerThickness = cloudsLayer.endRadius - cloudsLayer.startRadius;
					ImGui::TreePop();
				}
				if (ImGui::TreeNode("Scattering"))
				{
					int currentDSDItem = _cloudsDSDUniformNotDisperse ? 0 : 1;
					const char* const items[] = { "Uniform", "Disperse" };
					if (ImGui::Combo("DSD", &currentDSDItem, (const char* const*)items, 2))
					{
						_cloudsDSDUniformNotDisperse = !_cloudsDSDUniformNotDisperse;
						if (_cloudsDSDUniformNotDisperse)
						{
							Clouds_aerosols *= 5;//Because uniform is too much uniform and we must add some aerosols to preserver realism
						}
						else
						{
							Clouds_aerosols *= (1.0 / 5.0);
						}
						cloudsMiePhaseFunction();
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Water Droplet Size Distribution.\nUniform creates Brocken spectre and corona halos\nDisperse creates fogbow and glory");
					}
					ImGui::PushItemWidth(120);
					ImGui::InputFloat("Density", &cloudsLayer.density, 0, 0, "%e");

					bool useMultScattApprox = Clouds_beerAmbient != 0;
					static float prevBeerAmbient = 0.f;
					static float prevPowderDensity = 0.f;
					static float prevPowderAmbient = 1.f;
					if (ImGui::Checkbox("Multiple scat. approx", &useMultScattApprox))
					{
						swap(prevBeerAmbient, Clouds_beerAmbient);
						swap(prevPowderDensity, Clouds_powderDensity);
						swap(prevPowderAmbient, Clouds_powderAmbient);
					}
					if(useMultScattApprox)
					{
						ImGui::TreePush();
						ImGui::InputFloat("Ambient", &Clouds_beerAmbient);
						ImGui::PushItemWidth(90);
						ImGui::InputFloat("Powder density", &Clouds_powderDensity, 0, 0, "%e");
						if (ImGui::IsItemHovered())
						{
							ImGui::SetTooltip("Simulates multiple scattering in the form of \"powder\" effect.");
						}
						ImGui::InputFloat("Powder ambient", &Clouds_powderAmbient);
						ImGui::PopItemWidth();
						ImGui::TreePop();
					}
					ImGui::InputFloat("Sharpness", &cloudsLayer.sharpness);
					ImGui::InputFloat("Lower gradient", &cloudsLayer.lowerGradient);
					ImGui::InputFloat("Upper gradient", &cloudsLayer.upperGradient);
					ImGui::InputFloat("Gradient power", &Clouds_fadePower);
					ImGui::InputFloat("Scattering", &cloudsLayer.scatteringCoef, 0, 0, "%e");
					ImGui::InputFloat("Extinction", &cloudsLayer.extinctionCoef, 0, 0, "%e");
					ImGui::InputFloat("Aerosols", &Clouds_aerosols, 0.05, .1);
					if (Clouds_aerosols < 0)
					{
						Clouds_aerosols = 0;
					}
					else if (Clouds_aerosols > 1)
					{
						Clouds_aerosols = 1;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Fractional amount of aerosols which differ from overall DSD. Increasing it makes the atomospheric phenomena appear less noticeable."
							"\nNote: When switching between Uniform and Disperse DSD, this number gets divided by 5 automatically to reflect the way how aerosols are actually present in clouds");
					}
					ImGui::PopItemWidth();
					ImGui::TreePop();
				}
				ImGui::PopItemWidth();
				ImGui::TreePop();
			}
		}

		void drawPathTracerGUI()
		{
			ImGui::Begin("Path Tracer");
			if (ImGui::Button("Go RT Raytracing"))
			{
				swapSettingsBackup();
				_renderImageSize = { (uint16_t)_windowWidth, (uint16_t)_windowHeight };
				currentSample = 0;
				_customScreenshotSize = false;
				_pathTracingMode = false;
			}
			ImGui::SameLine();
			if (ImGui::Button("Re-render"))
			{
				_renderImageSize = { (uint16_t)_windowWidth, (uint16_t)_windowHeight };
				currentSample = 0;
				updateScene();
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
				ImGui::Text("Rendered in %f ms", _renderTime);
			}
			else
			{
				ImGui::Text("%d sampled", currentSample);
			}
			ImGui::PushItemWidth(90);

			ImGui::InputInt("Primary rays", (int*)&HQSettings_directSamples);
			*(int*)&HQSettings_directSamples = bx::max(*(int*)&HQSettings_directSamples, 1);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Number of rays cast from one pixel");

			int secondary = *(int*)&Multisampling_indirect - 1;
			ImGui::InputInt("Secondary rays", &secondary);
			secondary = bx::max(secondary, 0);
			*(int*)&Multisampling_indirect = secondary + 1;
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Number of rays cast per one surface hit point\nTotal rays per pixel = primary * (1 + secondary)");

			ImGui::InputInt("Bounces", (int*)&Multisampling_maxBounces);
			*(int*)&Multisampling_maxBounces = bx::max(*(int*)&Multisampling_maxBounces, 0);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("How many times can a secondary ray bounce from surface");

			inputSlicesCount();
			ImGui::PopItemWidth();
			drawPresetButton("1", 0);
			ImGui::SameLine();
			drawPresetButton("2", 1);
			ImGui::SameLine();
			drawPresetButton("3", 2);
			ImGui::SameLine();
			drawPresetButton("4", 3);
			ImGui::SameLine();
			drawPresetButton("5", 4);
			ImGui::SameLine();
			ImGui::Text("Predefined views");
			drawPlanetGUI();

			drawTerrainGUI();
			drawLightGUI();
			drawCloudsGUI();
			drawPerformanceGUI();
			ImGui::End();
		}

		void drawPresetButton(const char* text, int presetNum)
		{
			if (ImGui::SmallButton(text))
			{
				applyPreset(presetNum);
				currentSample = 0;
				_currentChunk = -1;
			}
		}

		void applyPreset(uint8_t preset)
		{
			auto presetObj = DefaultScene::presets[preset];
			_person.Camera.SetPosition(presetObj.camera);
			_person.Camera.SetRotation(presetObj.rotation);
			_sunAngle = presetObj.sun.y;
			_secondSunAngle = presetObj.sun.x;
			Clouds_farPlane = presetObj.cloudsFarPlane;
		}

		void inputSlicesCount()
		{
			if (ImGui::InputInt("Render Slices", &_slicesCount, 1, 0))
			{
				_currentChunk = 0; // When changing slices count, we must start rendering again from the first slice
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Slices image rendering into X chunks on both directions.\nUse when application is crashing because of too long frame rendering time\nIntroduces tearing in realtime mode");
			if (_slicesCount < 1)//Minimum is one chunk
				_slicesCount = 1;
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
					glm::vec3 tonemapped;
					//Convert RGB16F to 32bit float
					glm::vec3 rgb(
						bx::halfToFloat(_readedTexture[x]) / dirSamp,
						bx::halfToFloat(_readedTexture[x + 1]) / dirSamp,
						bx::halfToFloat(_readedTexture[x + 2]) / dirSamp);
					tonemapped = Tonemapping::tmFunc(rgb, _tonemappingType);

					_readedTexture[x] = bx::halfFromFloat(tonemapped.r);
					_readedTexture[x + 1] = bx::halfFromFloat(tonemapped.g);
					_readedTexture[x + 2] = bx::halfFromFloat(tonemapped.b);
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
			bgfx::blit(0, _stagingBuffer, 0, 0, _raytracerColorOutput, 0, 0, _renderImageSize.width, _renderImageSize.height);
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

