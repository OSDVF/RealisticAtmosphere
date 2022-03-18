#pragma once
/*
 * Copyright 2011-2021 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#ifndef BGFX_UTILS_H_HEADER_GUARD
#define BGFX_UTILS_H_HEADER_GUARD

#include <bx/pixelformat.h>
#include <bx/string.h>
#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <bimg/decode.h>

#include <tinystl/allocator.h>
#include <tinystl/vector.h>
#include <string>
namespace stl = tinystl;


///
void* load(const char* _filePath, uint32_t* _size = NULL);

///
void unload(void* _ptr);

///
bgfx::ShaderHandle loadShader(const char* _name);

///
bgfx::ProgramHandle loadProgram(const char* _vsName, const char* _fsName);

///
bgfx::TextureHandle loadTexture(const char* _name, uint64_t _flags = BGFX_TEXTURE_NONE | BGFX_SAMPLER_NONE, uint8_t _skip = 0, bgfx::TextureInfo* _info = NULL, bimg::Orientation::Enum* _orientation = NULL);

///
bimg::ImageContainer* imageLoad(const char* _filePath, bgfx::TextureFormat::Enum _dstFormat);

///
void calcTangents(void* _vertices, uint16_t _numVertices, bgfx::VertexLayout _layout, const uint16_t* _indices, uint32_t _numIndices);

/// Returns true if both internal transient index and vertex buffer have
/// enough space.
///
/// @param[in] _numVertices Number of vertices.
/// @param[in] _layout Vertex layout.
/// @param[in] _numIndices Number of indices.
///
inline bool checkAvailTransientBuffers(uint32_t _numVertices, const bgfx::VertexLayout& _layout, uint32_t _numIndices)
{
	return _numVertices == bgfx::getAvailTransientVertexBuffer(_numVertices, _layout)
		&& (0 == _numIndices || _numIndices == bgfx::getAvailTransientIndexBuffer(_numIndices))
		;
}

///
inline uint32_t encodeNormalRgba8(float _x, float _y = 0.0f, float _z = 0.0f, float _w = 0.0f)
{
	const float src[] =
	{
		_x * 0.5f + 0.5f,
		_y * 0.5f + 0.5f,
		_z * 0.5f + 0.5f,
		_w * 0.5f + 0.5f,
	};
	uint32_t dst;
	bx::packRgba8(&dst, src);
	return dst;
}

///
/*
struct MeshState
{
	struct Texture
	{
		uint32_t            m_flags;
		bgfx::UniformHandle m_sampler;
		bgfx::TextureHandle m_texture;
		uint8_t             m_stage;
	};

	Texture             m_textures[4];
	uint64_t            m_state;
	bgfx::ProgramHandle m_program;
	uint8_t             m_numTextures;
	bgfx::ViewId        m_viewId;
};

struct Primitive
{
	uint32_t m_startIndex;
	uint32_t m_numIndices;
	uint32_t m_startVertex;
	uint32_t m_numVertices;

	Sphere m_sphere;
	Aabb m_aabb;
	Obb m_obb;
};

typedef stl::vector<Primitive> PrimitiveArray;

struct Group
{
	Group();
	void reset();

	bgfx::VertexBufferHandle m_vbh;
	bgfx::IndexBufferHandle m_ibh;
	uint16_t m_numVertices;
	uint8_t* m_vertices;
	uint32_t m_numIndices;
	uint16_t* m_indices;
	Sphere m_sphere;
	Aabb m_aabb;
	Obb m_obb;
	PrimitiveArray m_prims;
};
typedef stl::vector<Group> GroupArray;

struct Mesh
{
	void load(bx::ReaderSeekerI* _reader, bool _ramcopy);
	void unload();
	void submit(bgfx::ViewId _id, bgfx::ProgramHandle _program, const float* _mtx, uint64_t _state) const;
	void submit(const MeshState* const* _state, uint8_t _numPasses, const float* _mtx, uint16_t _numMatrices) const;

	bgfx::VertexLayout m_layout;
	GroupArray m_groups;
};

///
Mesh* meshLoad(const char* _filePath, bool _ramcopy = false);

///
void meshUnload(Mesh* _mesh);

///
MeshState* meshStateCreate();

///
void meshStateDestroy(MeshState* _meshState);

///
void meshSubmit(const Mesh* _mesh, bgfx::ViewId _id, bgfx::ProgramHandle _program, const float* _mtx, uint64_t _state = BGFX_STATE_MASK);

///
void meshSubmit(const Mesh* _mesh, const MeshState* const* _state, uint8_t _numPasses, const float* _mtx, uint16_t _numMatrices = 1);
*/
/// bgfx::RendererType::Enum to name.
bx::StringView getName(bgfx::RendererType::Enum _type);

/// Name to bgfx::RendererType::Enum.
bgfx::RendererType::Enum getType(const bx::StringView& _name);

///
struct Args
{
	Args(int _argc, const char* const* _argv);

	bgfx::RendererType::Enum m_type;
	uint16_t m_pciId;
};

namespace bgfx_utils
{
	bgfx::DynamicIndexBufferHandle createDynamicComputeReadBuffer(
		uint32_t _num
		, uint16_t _flags = BGFX_BUFFER_COMPUTE_READ | BGFX_BUFFER_ALLOW_RESIZE
	)
	{
		return bgfx::createDynamicIndexBuffer(_num / 2 /* because BGFX expects 2-byte indices */, _flags);
	}

	static void imageReleaseCb(void* _ptr, void* _userData)
	{
		BX_UNUSED(_ptr);
		bimg::ImageContainer* imageContainer = (bimg::ImageContainer*)_userData;
		bimg::imageFree(imageContainer);
	}

	static void textureArrayRelease(void* _ptr, void* _userData)
	{
		BX_UNUSED(_ptr);
		delete[] _userData;
	}


	static bgfx::TextureHandle createTextureArray(stl::vector<std::string> filePaths, uint64_t _flags)
	{
		assert(filePaths.size() > 0);

		bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
		char* arrayData = nullptr;
		bgfx::TextureFormat::Enum format;
		uint16_t width;
		uint16_t height;
		uint16_t numLayers = 0;
		uint32_t totalMemorySize;
		bool hasMips;

		int i = 0;
		do
		{
			auto filePath = filePaths[i];
			uint32_t size;
			void* currentImageData = load(filePath.c_str(), &size);

			if (NULL != currentImageData)
			{
				bimg::ImageContainer* imageContainer = bimg::imageParse(entry::getAllocator(), currentImageData, size);

				if (NULL != imageContainer)
				{
					unload(currentImageData);
					auto thisFormat = bgfx::TextureFormat::Enum(imageContainer->m_format);
					if (bgfx::isTextureValid(0, false, imageContainer->m_numLayers, thisFormat, _flags))
					{
						if (i == 0)//First texture sets the dimensions
						{
							width = imageContainer->m_width;
							height = imageContainer->m_height;
							format = thisFormat;
							hasMips = 1 < imageContainer->m_numMips;

							totalMemorySize = filePaths.size() * imageContainer->m_size;
							arrayData = new char[totalMemorySize];

						}
						else
						{
							BX_ASSERT(width == imageContainer->m_width, "All images must have the same size. The first had width %d and the %dth has %d.", width, i+1, imageContainer->m_width);
							BX_ASSERT(height == imageContainer->m_height, "All images must have the same size. The first had height %d and the %dth has %d.", height, i + 1, imageContainer->m_height);
							BX_ASSERT(format == thisFormat, "All images must have the same format");
							BX_ASSERT(hasMips == 1 < imageContainer->m_numMips, "All array images must have same mip format");
						}
						//Copy image to array
						bx::memCopy(arrayData + i * imageContainer->m_size, imageContainer->m_data, imageContainer->m_size);
						numLayers++;
					}
					bimg::imageFree(imageContainer);
				}
			}
		} while (filePaths.size() > ++i);

		const bgfx::Memory* mem = bgfx::makeRef(
			arrayData, totalMemorySize, textureArrayRelease, arrayData
		);

		handle = bgfx::createTexture2D(width, height, hasMips, numLayers, format, _flags, mem);

		if (bgfx::isValid(handle))
		{
			bgfx::setName(handle, filePaths[0].c_str());
		}

		return handle;
	}
}
#endif // BGFX_UTILS_H_HEADER_GUARD
