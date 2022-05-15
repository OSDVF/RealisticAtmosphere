/**
 * @author Ondřej Sabela
 * @brief Realistic Atmosphere - Thesis implementation.
 * @date 2021-2022
 * Copyright 2022 Ondřej Sabela. All rights reserved.
 * Uses ray tracing, path tracing and ray marching to create visually plausible outdoor scenes with atmosphere, terrain, clouds and analytical objects.
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
#include <bx/commandline.h>

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
		std::string lastScreenshotFilename;
		float _performanceFrequency = 0;
		float _renderTime = 0;
		uint64_t _renderStartedAt = 0;
		uint32_t _frame = 0;
		float _currentSample = 0;
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
		bool _whiteBalance = true;
		vec4 _whitePoint;
		int _cameraType = 0;
		bool _showFlare = true;
		bool _preserveSunPreset = false;
		float _flareVisibility = 1.0f;
		int _flareOcclusionSamples = 40;
		entry::MouseState _mouseState;

		float _sunAngle = 1.5;//86 deg
		float _moonAngle = 0;
		float _secondSunAngle = -1.5;
		float _secondMoonAngle = 1;
		// Droplet size distribution
		bool _cloudsDSDUniformNotDisperse = false;
		//true => uniform, false => disperse

		vec3 _cloudsWind = vec3(-50, 0, 0);
		bool _moveClouds = false;

		ScreenSpaceQuad _screenSpaceQuad;/**< Output of raytracer (both the compute-shader variant and fragment-shader variant) */

		int _debugOutput = 0;
		bool _debugAtmoOff = false;
		bool _showGUI = true;
		bool _pathTracingMode = true;
		// To "unlock" camera movement we must lock the mouse
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
		bgfx::UniformHandle _whiteBalanceHandle = BGFX_INVALID_HANDLE;
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
		bgfx::ProgramHandle _postShaderProgram = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle _displayingShaderProgram = BGFX_INVALID_HANDLE; /**< This program displays output from compute-shader raytracer */

		bgfx::ShaderHandle _computeShaderHandle = BGFX_INVALID_HANDLE;
		bgfx::ShaderHandle _postShaderHandle = BGFX_INVALID_HANDLE;
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

		// The Entry library will call this method once after setting up window manager
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

			//Parse command line args
			bx::CommandLine cmdLine(argc, argv);
			if (!cmdLine.hasArg("cheap"))
			{
				//
				// Pathtracing must have higher quality by default ;)
				//

				// Display earth shadows and compute atmosphere exacly
				*(uint32_t*)&HQSettings_flags |= HQFlags_EARTH_SHADOWS | HQFlags_ATMO_COMPUTE | HQFlags_LIGHT_SHAFTS;

				// 300 planet raymarching steps
				*(int*)&RaymarchingSteps_terrain = 300;
				// 200 cloud raymarching steps
				Clouds_iter = 200;
				Clouds_terrainSteps = 80;
				Clouds_lightSteps = 8;
				Clouds_lightFarPlane = 40000;
				LightSettings_farPlane = 10000;
				*(int*)&LightSettings_shadowSteps = 70;
			}

			applyPreset(0);//Set default player and sun positions

			entry::setWindowFlags(HANDLE_OF_DEFALUT_WINDOW, ENTRY_WINDOW_FLAG_ASPECT_RATIO, false);
			entry::setWindowSize(HANDLE_OF_DEFALUT_WINDOW, 1024, 600);

			// Supply program arguments for setting graphics backend to BGFX.
			uint16_t pciId = 0;
			if (cmdLine.hasArg("amd"))
			{
				pciId = BGFX_PCI_ID_AMD;
			}
			else if (cmdLine.hasArg("nvidia"))
			{
				pciId = BGFX_PCI_ID_NVIDIA;
			}
			else if (cmdLine.hasArg("intel"))
			{
				pciId = BGFX_PCI_ID_INTEL;
			}
			else if (cmdLine.hasArg("sw"))
			{
				pciId = BGFX_PCI_ID_SOFTWARE_RASTERIZER;
			}

			// Initialize BFGX with supplied arguments
			bgfx::Init init;
			init.type = bgfx::RendererType::OpenGL;
			init.vendorId = pciId;
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
			_whiteBalanceHandle = bgfx::createUniform("WhiteBalance", bgfx::UniformType::Vec4);
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

			_postShaderHandle = loadShader("post_process.comp");
			_postShaderProgram = bgfx::createProgram(_postShaderHandle);

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
			ColorMapping::FillSpectrum(SkyRadianceToLuminance, SunRadianceToLuminance, DefaultScene::planetBuffer[0], DefaultScene::directionalLightBuffer[0], _whitePoint);
			DefaultScene::materialBuffer[0].emission = /*sun emission calcualation from its angular radius*/
				vec4::fromVec3(bx::div(
					DefaultScene::directionalLightBuffer[0].irradiance,
					M_PI * DefaultScene::sunAngularRadius * DefaultScene::sunAngularRadius
				));

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
			// Precompute small-scale repeating terrain heightmap
			_heightmapTextureHandle = bgfx::createTexture2D(8192, 8192, false, 1, bgfx::TextureFormat::RGBA32F, BGFX_TEXTURE_COMPUTE_WRITE);
			bgfx::setImage(0, _heightmapTextureHandle, 0, bgfx::Access::Write);
			bgfx::dispatch(0, _heightmapShaderProgram, bx::ceil(8192 / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(8192 / float(SHADER_LOCAL_GROUP_COUNT)));
		}
		void cloudsMiePhaseFunction()
		{
			// Copy lookup table of effective Mie phase function of clouds into a GPU texture
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
				// Fit the separate color channels into one texture.
				// In the texture the channel values are nexto to each other. One pixel is therefore 4 'floats' wide
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
			// Dispatch all the precomputation procedures

			bgfx::UniformHandle precomputeSettingsHandle = bgfx::createUniform("PrecomputeSettings", bgfx::UniformType::Vec4);
			uint32_t PrecomputeSettings[] = { 0,0,0,0 };
			// Send settings to the GPU
			bgfx::setUniform(precomputeSettingsHandle, PrecomputeSettings);

			// Sun optical depth table for atmosphere raymarching
			bgfx::ShaderHandle precomputeOptical = loadShader("OpticalDepth.comp");
			bgfx::ProgramHandle opticalProgram = bgfx::createProgram(precomputeOptical);
			if (!bgfx::isValid(_opticalDepthTable))
			{
				_opticalDepthTable = bgfx::createTexture2D(128, 64, false, 1, bgfx::TextureFormat::RGBA32F,
					BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_UVW_CLAMP
				);
			}

			updateBuffers();// Planet buffer will be needed in the shaders below
			bgfx::setImage(0, _opticalDepthTable, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA32F);
			bgfx::setBuffer(1, _atmosphereBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(2, _directionalLightBufferHandle, bgfx::Access::Read);
			bgfx::dispatch(0, opticalProgram, bx::ceil(128 / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(64 / float(SHADER_LOCAL_GROUP_COUNT)));

			bgfx::ShaderHandle precomputeTransmittance = loadShader("Transmittance.comp");
			bgfx::ProgramHandle transmittanceProgram = bgfx::createProgram(precomputeTransmittance);
			if (!bgfx::isValid(_transmittanceTable))
			{
				_transmittanceTable = bgfx::createTexture2D(256, 64, false, 1, bgfx::TextureFormat::RGBA32F,
					BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_UVW_CLAMP
				);
			}
			bgfx::setImage(0, _transmittanceTable, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA32F);
			bgfx::setBuffer(1, _atmosphereBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(2, _directionalLightBufferHandle, bgfx::Access::Read);
			bgfx::dispatch(0, transmittanceProgram, bx::ceil(256 / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(64 / float(SHADER_LOCAL_GROUP_COUNT)));

			bgfx::ShaderHandle precomputeSingleScattering = loadShader("SingleScattering.comp");
			bgfx::ProgramHandle scatteringProgram = bgfx::createProgram(precomputeSingleScattering);
			constexpr int SCATTERING_TEXTURE_WIDTH =
				SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_MU_S_SIZE;
			constexpr int SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_R_SIZE;
			if (!bgfx::isValid(_singleScatteringTable))
			{
				_singleScatteringTable = bgfx::createTexture3D(SCATTERING_TEXTURE_WIDTH, SCATTERING_TEXTURE_HEIGHT, SCATTERING_TEXTURE_DEPTH, false, bgfx::TextureFormat::RGBA16F,
					BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_UVW_CLAMP
				);
			}
			bgfx::setImage(0, _singleScatteringTable, 0, bgfx::Access::Write, bgfx::TextureFormat::RGBA16F);
			setBuffersAndUniforms();
			bgfx::setTexture(7, _transmittanceSampler, _transmittanceTable);
			bgfx::dispatch(0, scatteringProgram, bx::ceil(SCATTERING_TEXTURE_WIDTH / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(SCATTERING_TEXTURE_HEIGHT / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(SCATTERING_TEXTURE_DEPTH / 4.0f));

			bgfx::ShaderHandle precomputeIrradiance = loadShader("IndirectIrradiance.comp");
			bgfx::ProgramHandle irradianceProgram = bgfx::createProgram(precomputeIrradiance);
			if (!bgfx::isValid(_irradianceTable))
			{
				_irradianceTable = bgfx::createTexture2D(64, 16, false, SCATTERING_LIGHT_COUNT, bgfx::TextureFormat::RGBA16F,
					BGFX_TEXTURE_COMPUTE_WRITE | BGFX_SAMPLER_UVW_CLAMP
				);
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

		// Resize and clean render buffers
		void cleanRenderedBuffers(uint16_t newWidth, uint16_t newHeight)
		{
			// Also resizes compute shader output buffer
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

			_currentSample = 0;//Reset sample counter - otherwise path tracing would continue with previous samples
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
			bgfx::destroy(_postShaderProgram);
			bgfx::destroy(_postShaderHandle);
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
					DefaultScene::objectBuffer[3].position = vec3(Camera[0].x, Camera[0].y, Camera[0].z);
					break;
				case 't':
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
						else if (inputGetKeyState(entry::Key::Key7))
						{
							applyPreset(6);
						}
						else if (inputGetKeyState(entry::Key::Key8))
						{
							applyPreset(7);
						}
						else if (inputGetKeyState(entry::Key::Key9))
						{
							applyPreset(8);
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

				if (lastScreenshotFilename.length() > 0)
				{
					// Post processing is already done
					const vec4 allNull{ 1, 1, 1, 0 };
					bgfx::setUniform(_hqSettingsHandle, &allNull);
				}
				else
				{
					setDisplaySettings();
				}
				bgfx::submit(0, _displayingShaderProgram);

				int maxSamples = HQSettings_directSamples * Multisampling_indirect;

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
					if (_pathTracingMode && _currentSample < maxSamples)
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
				if (_currentSample == 0 || !_pathTracingMode && _currentChunk == 0)
				{
					_renderStartedAt = stats->gpuTimeBegin;
				}
				if (tracingComplete = (_currentSample >= maxSamples))
				{
					if (!_pathTracingMode)
					{
						_currentSample = 0;
						renderScene();
						_renderTime = (stats->gpuTimeEnd - _renderStartedAt) * 1000 / (float)stats->gpuTimerFreq;
						updateScene();
					}
					else if (_currentSample == maxSamples)/*add last frame latency*/
					{
						// Rendering complete
						_renderTime = (stats->gpuTimeEnd - _renderStartedAt) * 1000 / (float)stats->gpuTimerFreq;
						_currentSample = INFINITY;//Do not render further in any case! :)
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
			updateLensFlare();
			if (_whiteBalance)
			{
				bgfx::setUniform(_whiteBalanceHandle, &_whitePoint);
			}
			else
			{
				const vec4 white{ 1, 1, 1, 0 };
				bgfx::setUniform(_whiteBalanceHandle, &white);
			}
			bgfx::setUniform(_hqSettingsHandle, &HQSettings);
		}

		void updateLensFlare()
		{
			vec4 displaySettValue;
			if (_cameraType == 0 && _showFlare)
			{
				auto sunPos = DefaultScene::objectBuffer[0].position;
				glm::vec3 screenSpaceSun;
				_person.Camera.ProjectWorldToScreen(glm::vec3(sunPos.x, sunPos.y, sunPos.z), glm::vec4(0, 0, _renderImageSize.width, _renderImageSize.height),/*out*/ screenSpaceSun);
				auto aspectRatio = (float(_renderImageSize.width) / (float)_renderImageSize.height);
				screenSpaceSun.x = (-screenSpaceSun.x / (screenSpaceSun.z * _renderImageSize.width) + 0.5) * aspectRatio;
				screenSpaceSun.y = screenSpaceSun.y / (screenSpaceSun.z * _renderImageSize.height) - 0.5;

				float flareBrightness = 0;
				if (screenSpaceSun.z > 0)
				{
					// If the sun is not behind the camera
					flareBrightness = std::fmax(
						bx::dot(Camera[1].toVec3(), DefaultScene::directionalLightBuffer[0].direction) * _flareVisibility,
						0);
				}

				uint32_t packedTonemappingAndOcclusion =
					(_tonemappingType & 0xFFFF //lower 16 bits*
						| _flareOcclusionSamples << 16); //higher 16 bits
				displaySettValue = vec4(
					*(float*)&packedTonemappingAndOcclusion, flareBrightness,
					screenSpaceSun.x, screenSpaceSun.y
				);
			}
			else
			{
				displaySettValue = vec4(*(float*)&_tonemappingType, 0, 0, 0);
			}

			bgfx::setUniform(_displaySettingsHandle, &displaySettValue);
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
				// Although this is the first branch, it is actually the last step of screenshot process.
				// Save the taken screenshot
				savePng();
				if (_renderImageSize.width != _windowHeight && _renderImageSize.height != _windowHeight)
				{
					// Reset back to window dimensions
					cleanRenderedBuffers(_windowWidth, _windowHeight);
					_currentSample = INFINITY;//Do not render again
					bgfx::reset(_windowWidth, _windowHeight);
					bgfx::blit(0, _raytracerColorOutput, 0, 0, _stagingBuffer);
				}
				bgfx::destroy(_stagingBuffer);
				// Return to normal rendering
				_screenshotFrame = SCREENSHOT_NEVER;//Do not take any screenshot at next frame
			}
			// Delayed screenshot request
			else if (_screenshotFrame == SCREENSHOT_AFTER_RENDER)
			{
				if (tracingComplete)
				{
					//Do post-processing
					bgfx::setImage(0, _raytracerColorOutput, 0, bgfx::Access::ReadWrite);
					bgfx::setTexture(1, _depthBufferSampler, _raytracerDepthAlbedoBuffer);
					setDisplaySettings();
					bgfx::dispatch(0, _postShaderProgram, bx::ceil(_renderImageSize.width / float(SHADER_LOCAL_GROUP_COUNT)), bx::ceil(_renderImageSize.height / float(SHADER_LOCAL_GROUP_COUNT)));

					_screenshotFrame = SCREENSHOT_AFTER_RENDER_PENDING;
				}
			}
			else if (tracingComplete && _screenshotFrame == SCREENSHOT_AFTER_RENDER_PENDING)
			{
				// Everything is prepared to do a screenshot
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
			// Get corresponding chunk coords
			int chunkSizeX = bx::ceil(_renderImageSize.width / (float)_slicesCount);
			chunkSizeX = firstMultipleOfXGreaterThanY(SHADER_LOCAL_GROUP_COUNT, chunkSizeX);
			int chunkSizeY = bx::ceil(_renderImageSize.height / (float)_slicesCount);
			chunkSizeY = firstMultipleOfXGreaterThanY(SHADER_LOCAL_GROUP_COUNT, chunkSizeY);
			int chunkXstart = chunkSizeX * (_currentChunk % _slicesCount);
			int chunkYstart = chunkSizeY * (_currentChunk / _slicesCount);

			// Save them into some GPU uniform variable
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
				_currentSample++;
			}
		}

		void updateLights()
		{
			// Send the sun position to the GPU
			auto& planet = DefaultScene::planetBuffer[0];
			updateLight(DefaultScene::objectBuffer[0], planet, DefaultScene::directionalLightBuffer[0], _sunAngle, _secondSunAngle);
		}
		void updateLight(AnalyticalObject& lightObject, Planet& planet, DirectionalLight& light, float angle, float secondAngle)
		{
			// Update ghost sphere object position according to specified angles
			glm::quat rotQua(glm::vec3(angle, secondAngle, 0));
			glm::vec3 pos(0, bx::length(bx::sub(lightObject.position, planet.center)), 0);
			pos = rotQua * pos;
			lightObject.position = bx::add(vec3(pos.x, pos.y, pos.z), planet.center);

			light.direction = bx::normalize(lightObject.position);
		}
#if _DEBUG
		void updateDebugUniforms()
		{
			_debugAttributesResult = vec4(_debugOutput == 1 ? 1 : 0, _debugOutput == 2 ? 1 : 0, _debugAtmoOff ? 1 : 0, 0);
			bgfx::setUniform(_debugAttributesHandle, &_debugAttributesResult);
		}
#endif
		void computeShaderRaytracer(uint32_t numX, uint32_t numY)
		{
			// Bind buffers and dispatch the render program
			bgfx::setImage(0, _raytracerColorOutput, 0, bgfx::Access::ReadWrite);
			bgfx::setImage(1, _raytracerNormalsOutput, 0, bgfx::Access::ReadWrite);
			bgfx::setImage(2, _raytracerDepthAlbedoBuffer, 0, bgfx::Access::ReadWrite);
			bgfx::dispatch(0, _computeShaderProgram, numX, numY);
		}

		void updateBuffers()
		{
			// Send CPU buffers to GPU
			bgfx::update(_objectBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::objectBuffer, sizeof(DefaultScene::objectBuffer)));
			bgfx::update(_atmosphereBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::planetBuffer.data(), sizeof(DefaultScene::planetBuffer)));
			bgfx::update(_materialBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::materialBuffer, sizeof(DefaultScene::materialBuffer)));
			bgfx::update(_directionalLightBufferHandle, 0, bgfx::makeRef((void*)DefaultScene::directionalLightBuffer.data(), sizeof(DefaultScene::directionalLightBuffer)));
		}

		void setBuffersAndUniforms()
		{
			// Assign buffers to uniform handles
			bgfx::setBuffer(3, _objectBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(4, _atmosphereBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(5, _materialBufferHandle, bgfx::Access::Read);
			bgfx::setBuffer(6, _directionalLightBufferHandle, bgfx::Access::Read);
			vec4 timeWrapper = vec4(_frame, 0, 0, 0);/*It is more frame number thant real time*/
			HQSettings_sampleNum = _currentSample;
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
			// 
			// "Real" scene camera (displaying only the full-screen quad)
			//
			float proj[16];
			bx::mtxOrtho(proj, 0.0f, 1.0f, 0.0f, 1.0f, -1.0f, 100.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);

			// Set view 0 default viewport.
			bgfx::setViewTransform(0, NULL, proj);
			bgfx::setViewRect(0, 0, 0, _renderImageSize.width, _renderImageSize.height);

			// 
			// "Virtual" PT/RT camera
			//
			glm::vec3 prevRotation;
			switch (_cameraType)
			{
			case 0:
				// Perspective camera
				_tanFovY = bx::tan(_fovY * bx::acos(-1) / 180.f / 2.0f);
				_tanFovX = (static_cast<float>(_renderImageSize.width) * _tanFovY) / static_cast<float>(_renderImageSize.height);
				break;
			case 1:
				// 360 Panoramatic camera
				_tanFovY = 0;// Custom indicator combination of "panoramatic" cam
				_tanFovX = 1;
				break;
			case 2:
				// Fisheye cam
				_tanFovY = 0;// Custom indicator combination of "fisheye" cam
				_tanFovX = 0;

				// Fisheye camera needs to be rotatex around X axis by 90 degrees
				prevRotation = _person.Camera.GetRotation();
				_person.Camera.SetRotation(glm::vec3(prevRotation.x + 90, prevRotation.y, prevRotation.z));
				break;
			}

			glm::vec3 camPos = _person.Camera.GetPosition();
			glm::vec3 camRot = _person.Camera.GetForward();
			glm::vec3 camUp = _person.Camera.GetUp();
			glm::vec3 camRight = _person.Camera.GetRight();
			if (_cameraType == 2)
			{
				_person.Camera.SetRotation(prevRotation);
			}
			Camera[0] = vec4(camPos.x, camPos.y, camPos.z, 1);
			Camera[1] = vec4(camRot.x, camRot.y, camRot.z, 0);
			Camera[2] = vec4(camUp.x, camUp.y, camUp.z, _tanFovY);
			Camera[3] = vec4(camRight.x, camRight.y, camRight.z, _tanFovX);
			bgfx::setUniform(_cameraHandle, Camera, 4);
		}

		void swapSettingsBackup()
		{
			// Swap between settings sets (RT and PT mode has each its own set of settings)
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
				ImGui::Checkbox("Precompute atmo. scat.", &usePrecomputed);
				if (usePrecomputed)
				{
					if (ImGui::SmallButton("Recompute"))
					{
						precompute();
					}
				}
				ImGui::Checkbox("Light shafts", &lightShafts);
				ImGui::PushItemWidth(95);
				if (!usePrecomputed || lightShafts)
				{
					ImGui::InputInt("Samples", (int*)&Multisampling_perAtmospherePixel);
				}
				ImGui::Checkbox("Approximate skylight", &indirectApprox);
				if (indirectApprox)
				{
					flags |= HQFlags_INDIRECT_APPROX;
				}
				else
				{
					flags &= ~(HQFlags_INDIRECT_APPROX);
				}
				ImGui::Checkbox("Reduce banding", &reduceBanding);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Reduce vertical lines visible in clouds. Actually makes the clouds noisy.");
				}
				if (reduceBanding)
				{
					ImGui::InputFloat("Primary factor", &LightSettings_deBanding);
					ImGui::InputFloat("Light factor", &Clouds_deBanding);
					ImGui::InputFloat("Cone width", &Clouds_cone);
					flags |= HQFlags_REDUCE_BANDING;
				}
				else
				{
					flags &= ~HQFlags_REDUCE_BANDING;
				}
				ImGui::PopItemWidth();

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
					ImGui::TreePush();
					ImGui::PushItemWidth(95);
					ImGui::InputFloat("Cascade distance", &LightSettings_shadowCascade);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("First cascade of shadows is more precise, second has lower quality (quite blocky)");
					}
					ImGui::InputFloat("Sharpness", &LightSettings_shadowHardness);
					ImGui::InputFloat("de-Banding coef", &LightSettings_shadowDeBanding);
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Reduces blocky shadows. Makes them noisy instead ;)");
					}
					ImGui::PopItemWidth();
					ImGui::TreePop();
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
				ImGui::TreePop();
			}
#if _DEBUG
			if (ImGui::TreeNode("Debug"))
			{
				const char* const debugOutputTypes[] = { "Nothing", "Normals", "Albedo" };

				ImGui::Combo("Debug Output", &_debugOutput, debugOutputTypes, sizeof(debugOutputTypes) / sizeof(const char*));
				ImGui::Checkbox("Hide atmosphere", &_debugAtmoOff);
				ImGui::TreePop();
			}
#endif
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
					"Presets: Ctrl + 1-9\n"
					"Place object: R-Sphere, T-Cube"
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
				static float previousFovY = 0;
				const char* const cameraTypes[] = { "Perspective", "Panorama", "Fisheye" };
				ImGui::Combo("Type", &_cameraType, cameraTypes, sizeof(cameraTypes) / sizeof(const char*));
				if (_cameraType == 0 && ImGui::InputFloat("FOV", &_fovY))
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
					ImGui::InputFloat("M.Asymmetry Factor", &singlePlanet.mieAsymmetryFactor);
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
				singlePlanet.mountainsHeight = singlePlanet.mountainsRadius - singlePlanet.surfaceRadius;
			}
		}

		void drawLightGUI()
		{
			if (ImGui::TreeNode("Light"))
			{
				ImGui::PushItemWidth(100);
				ImGui::Checkbox("White balance", &_whiteBalance);
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
				ImGui::InputFloat("Secondary ray offset", &LightSettings_secondaryOffset);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Offsets secondary rays a bit to prevent self-shadowing");
				}
				if (_cameraType == 0)
				{
					ImGui::Checkbox("Lens Flare", &_showFlare);
					if (_showFlare)
					{
						ImGui::SliderFloat("Visibility", &_flareVisibility, 0, 2);
						ImGui::PushItemWidth(90);
						ImGui::InputInt("Occlusion samples", &_flareOcclusionSamples);
						if (_flareOcclusionSamples > 255)
							_flareOcclusionSamples = 255;
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Creates screen-space light shafts."
								"\n1 = only test for sun visibility."
								"\n0 = always show flare"
								"\nMultisampling is not applied to them because they are rendered after the whole image");
						ImGui::PopItemWidth();
						if (_flareOcclusionSamples < 0) _flareOcclusionSamples = 0;
					}
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
					bool empiricOptimizations = LightSettings_rayAlsoShadowedThres != 0;
					if (ImGui::Checkbox("Empiric optimizations", &empiricOptimizations))
					{
						swap(prevShadowedThreshold, LightSettings_rayAlsoShadowedThres);
						swap(prevNoRayThres, LightSettings_noRayThres);
						swap(prevViewThres, LightSettings_viewThres);
						swap(prevCutoffDist, LightSettings_cutoffDist);
					}
					if (empiricOptimizations)
					{
						ImGui::InputFloat("ShadowedThres", &LightSettings_rayAlsoShadowedThres);
						if (LightSettings_rayAlsoShadowedThres < 1.0f)
							LightSettings_rayAlsoShadowedThres = 1.0f;//Prevent user to get to value below 1. This would break our "prevShadowedThreshold" swap trick
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
				bool enabled = RaymarchingSteps_terrain != 0;
				ImGui::PushItemWidth(120);
				if (ImGui::Checkbox("Enabled", &enabled))
				{
					if (enabled)
					{
						RaymarchingSteps_terrain = prevTerrainSteps;
						LightSettings_shadowSteps = prevLightSteps;
					}
					else
					{
						prevLightSteps = LightSettings_shadowSteps;
						prevTerrainSteps = RaymarchingSteps_terrain;
						RaymarchingSteps_terrain = 0;
						LightSettings_shadowSteps = 0;
					}
				}
				if (enabled)
				{
					ImGui::InputFloat("Optimism", &QualitySettings_optimism, 0, 1);
					ImGui::InputFloat("Far Plane", &QualitySettings_farPlane);
					ImGui::InputInt("Planet Steps", (int*)&RaymarchingSteps_terrain);
					if (*(int*)&RaymarchingSteps_terrain < 1.0f)
						*(int*)&RaymarchingSteps_terrain = 1.0f;//Prevent user to go below 1. This would break our "prevTerrainSteps" swap trick

					ImGui::InputFloat("Precision", &RaymarchingSteps_precision, 0, 0, "%e");
					ImGui::InputFloat("LOD A", &RaymarchingSteps_lodA);
					ImGui::InputFloat("LOD B", &RaymarchingSteps_lodA);
					if (ImGui::TreeNode("Materials"))
					{
						ImGui::InputFloat("1", &PlanetMaterial.x);
						ImGui::InputFloat("2", &PlanetMaterial.y);
						ImGui::InputFloat("Gradient A", &PlanetMaterial.z);
						ImGui::InputFloat("Gradient B", &PlanetMaterial.w);
						ImGui::TreePop();
					}
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
				static float prevCloudsIter[2] = { 0.0f , 0.0f };
				static float prevCloudsTerrIter[2] = { 0.0f , 0.0f };
				bool enableClouds = Clouds_iter != 0.0f;
				if (ImGui::Checkbox("Enable Cl.", &enableClouds))
				{
					swap(prevCloudsIter[_pathTracingMode ? 0 : 1], Clouds_iter);
					swap(prevCloudsTerrIter[_pathTracingMode ? 0 : 1], Clouds_terrainSteps);
				}
				if (enableClouds)
				{
					ImGui::PushItemWidth(200);
					ImGui::InputFloat3("Size", &cloudsLayer.sizeMultiplier.x, "%.1e");
					ImGui::InputFloat3("Pos", &cloudsLayer.position.x);
					ImGui::PopItemWidth();
					ImGui::PushItemWidth(100);
					ImGui::Checkbox("Wind", &_moveClouds);
					if (_moveClouds)
					{
						ImGui::InputFloat3("Speed", &_cloudsWind.x, "%e");
					}
					if (ImGui::TreeNode("Settings"))
					{
						ImGui::InputFloat("Steps", &Clouds_iter, 1, 1, "%.0f");
						if (Clouds_iter < 1.0f)
						{
							Clouds_iter = 1.0f;
							//Prevent user to manually set something lower than one.
							//That would break the "enabled when greater than 1" mechanism
						}
						ImGui::InputFloat("TerrainSteps", &Clouds_terrainSteps, 1, 1, "%.0f");
						ImGui::InputFloat("Render Distance", &Clouds_farPlane);
						if (ImGui::TreeNode("Cl. Lighting"))
						{
							ImGui::InputFloat("LimitLight", &Clouds_maximumLuminance);
							ImGui::InputFloat("SmoothLimit", &Clouds_luminanceSmoothness);
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
						ImGui::InputFloat("Distortion", &Clouds_distortion);
						ImGui::InputFloat("Dist Size", &Clouds_distortionSize, 0, 0, "%e");

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

						bool useMultScattApprox = Clouds_beerAmbient != 0;
						static float prevBeerAmbient[2] = { 0.0f , 0.0f };
						static float prevPowderDensity[2] = { 0.0f , 0.0f };
						static float prevPowderAmbient[2] = { 1.0f , 1.0f };
						if (ImGui::Checkbox("Multiple scat. approx", &useMultScattApprox))
						{
							swap(prevBeerAmbient[_pathTracingMode ? 0 : 1], Clouds_beerAmbient);
							swap(prevPowderDensity[_pathTracingMode ? 0 : 1], Clouds_powderDensity);
							swap(prevPowderAmbient[_pathTracingMode ? 0 : 1], Clouds_powderAmbient);
						}
						if (useMultScattApprox)
						{
							ImGui::TreePush();
							ImGui::InputFloat("Ambient", &Clouds_beerAmbient);
							if (Clouds_beerAmbient == 0.0f)
							{
								Clouds_beerAmbient = FLT_EPSILON;
							}
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
						ImGui::PopItemWidth();
						ImGui::TreePop();
					}
					ImGui::PopItemWidth();
				}
				ImGui::TreePop();
			}
		}

		void drawPathTracerGUI()
		{
			ImGui::Begin("Path Tracer");
			if (ImGui::Button("Go RT Raytracing"))
			{
				lastScreenshotFilename.clear();
				swapSettingsBackup();
				_renderImageSize = { (uint16_t)_windowWidth, (uint16_t)_windowHeight };
				_currentSample = 0;
				_customScreenshotSize = false;
				_pathTracingMode = false;
			}
			ImGui::SameLine();
			auto maxSamples = HQSettings_directSamples * Multisampling_indirect;
			if (ImGui::Button("Re-render"))
			{
				lastScreenshotFilename.clear();
				_renderImageSize = { (uint16_t)_windowWidth, (uint16_t)_windowHeight };
				_currentSample = 0;
				updateScene();
			}
			if (_currentSample < maxSamples)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("Stop"))
				{
					_currentSample = INFINITY;
					_currentChunk = 0;
				}
			}
			if (ImGui::Button("Save Image"))
			{
				lastScreenshotFilename.clear();
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

			if (_currentSample >= maxSamples)
			{
				if (lastScreenshotFilename.length() > 0)
				{
					ImGui::Text("Saved as %s", lastScreenshotFilename.c_str());
				}
				ImGui::Text("Rendered in %f ms", _renderTime);
			}
			else
			{
				ImGui::Text("%.0f sampled", _currentSample);
			}
			ImGui::PushItemWidth(90);

			ImGui::InputFloat("Primary rays", &HQSettings_directSamples, 1, 1, "%.0f");
			HQSettings_directSamples = bx::max(HQSettings_directSamples, 1.0f);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Number of rays cast from one pixel");

			ImGui::InputInt("Bounces", (int*)&Multisampling_maxBounces);
			*(int*)&Multisampling_maxBounces = bx::max(*(int*)&Multisampling_maxBounces, 0);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("How many times can a secondary ray bounce from surface");

			if (*(int*)&Multisampling_maxBounces > 0)
			{
				ImGui::InputFloat("Secondary rays", &Multisampling_indirect, 1, 1, "%.0f");
				Multisampling_indirect = bx::max(Multisampling_indirect, 0.0f);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Number of rays cast per one surface hit point\nTotal rays per pixel = primary * (1 + secondary) * (1 + bounces)");
			}

			inputSlicesCount();
			ImGui::PopItemWidth();
			auto presetCount = sizeof(DefaultScene::presets) / sizeof(DefaultScene::Preset);
			for (int i = 0; i < presetCount; i++)
			{
				drawPresetButton(i);
				if (i < presetCount - 1)
				{
					ImGui::SameLine();
				}
			}

			ImGui::Checkbox("Do not change sun angle", &_preserveSunPreset);
			drawPlanetGUI();

			drawTerrainGUI();
			drawLightGUI();
			drawCloudsGUI();
			drawPerformanceGUI();
			ImGui::End();
		}

		void drawPresetButton(int presetNum)
		{
			char text[] = "1";
			text[0] += presetNum;
			if (ImGui::SmallButton(text))
			{
				applyPreset(presetNum);
				_currentSample = 0;
				_currentChunk = -1;
				updateScene();
			}
		}

		void applyPreset(uint8_t preset)
		{
			auto presetObj = DefaultScene::presets[preset];
			_person.Camera.SetPosition(presetObj.camera);
			_person.Camera.SetRotation(presetObj.rotation);
			if (!_preserveSunPreset)
			{
				_sunAngle = presetObj.sun.y;
				_secondSunAngle = presetObj.sun.x;
			}
			Clouds_farPlane = presetObj.cloudsFarPlane;
			QualitySettings_farPlane = presetObj.terrainFarPlane;
		}

		void inputSlicesCount()
		{
			if (ImGui::InputInt("Render Slices", &_slicesCount, 1, 0))
			{
				_currentChunk = 0; // When changing slices count, we must start rendering again from the first slice
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Slices the screen into X chunks on both directions.\nIncrease when application is crashing because of too long frame rendering time\nIntroduces tearing in realtime mode");
			if (_slicesCount < 1)//Minimum is one chunk
				_slicesCount = 1;
		}

		// Saves the captured screenshot. Called when the picture is completely in CPU memory
		void savePng()
		{
			bx::FileWriter writer;
			bx::Error err;
			std::ostringstream fileName;
			fileName << _frame;
			fileName << ".png";
			lastScreenshotFilename = fileName.str();

			if (bx::open(&writer, lastScreenshotFilename.c_str(), false, &err))
			{
				float* converted = new float[_renderImageSize.width * _renderImageSize.height];
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
			}
			else
			{
				bx::debugOutput("Screenshot failed.");
			}
		}

		// Capture rendered image into CPU-readable texture
		void takeScreenshot()
		{
			_readedTexture = new uint16_t[_renderImageSize.width * _renderImageSize.height * 4/*RGBA channels*/];
			//Create readable buffer for reading by the CPU
			_stagingBuffer = bgfx::createTexture2D(_renderImageSize.width, _renderImageSize.height, false, 1, bgfx::TextureFormat::Enum::RGBA16F, BGFX_TEXTURE_READ_BACK);
			//Copy unreadable raytracer output into the readable buffer
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
