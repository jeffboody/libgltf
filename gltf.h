/*
 * Copyright (c) 2022 Jeff Boody
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#ifndef gltf_H
#define gltf_H

#include <inttypes.h>
#include <stdio.h>

#include "../libcc/math/cc_mat4f.h"
#include "../libcc/math/cc_vec3f.h"
#include "../libcc/math/cc_vec4f.h"
#include "../libcc/cc_list.h"

typedef struct gltf_scene_s
{
	char name[256];

	// uint nd ptrs
	cc_list_t* nodes;
} gltf_scene_t;

typedef struct gltf_node_s
{
	struct
	{
		unsigned int has_mesh   : 1;
		unsigned int has_camera : 1;
		unsigned int has_pad    : 29;
	};

	char name[256];

	// uint nd ptrs
	cc_list_t* children;
	cc_mat4f_t matrix; // M=T*R*S
	uint32_t   mesh;
	uint32_t   camera;
} gltf_node_t;

typedef enum
{
	GLTF_CAMERA_TYPE_UNKNOWN,
	GLTF_CAMERA_TYPE_PERSPECTIVE,
	GLTF_CAMERA_TYPE_ORTHOGRAPHIC,
} gltf_cameraType_e;

typedef struct gltf_cameraPerspective_s
{
	float aspectRatio;
	float yfov;
	float zfar;
	float znear;
} gltf_cameraPerspective_t;

typedef struct gltf_cameraOrthographic_s
{
	float xmag;
	float ymag;
	float zfar; // optional
	float znear;
} gltf_cameraOrthographic_t;

typedef struct gltf_camera_s
{
	gltf_cameraType_e type;
	union
	{
		gltf_cameraPerspective_t  cameraPerspective;
		gltf_cameraOrthographic_t cameraOrthographic;
	};
} gltf_camera_t;

typedef struct gltf_attribute_s
{
	char     name[256];
	uint32_t accessor;
} gltf_attribute_t;

typedef enum
{
	GLTF_PRIMITIVE_MODE_POINTS,
	GLTF_PRIMITIVE_MODE_LINES,
	GLTF_PRIMITIVE_MODE_LINE_LOOP,
	GLTF_PRIMITIVE_MODE_LINE_STRIP,
	GLTF_PRIMITIVE_MODE_TRIANGLES,
	GLTF_PRIMITIVE_MODE_TRIANGLE_STRIP,
	GLTF_PRIMITIVE_MODE_TRIANGLE_FAN,
} gltf_primitiveMode_e;

typedef struct gltf_primitive_s
{
	struct
	{
		unsigned int has_indices  : 1;
		unsigned int has_material : 1;
		unsigned int has_pad      : 30;
	};

	gltf_primitiveMode_e mode;
	uint32_t             indices;
	uint32_t             material;
	cc_list_t*           attributes;
	// TODO - targets
} gltf_primitive_t;

typedef struct gltf_mesh_s
{
	cc_list_t* primitives;
	// TODO - weights
} gltf_mesh_t;

typedef struct gltf_materialTexture_s
{
	uint32_t index;
	uint32_t texCoord;
} gltf_materialTexture_t;

typedef struct gltf_materialPbrMetallicRoughness_s
{
	struct
	{
		unsigned int has_baseColorTexture        : 1;
		unsigned int has_metalicRoughnessTexture : 1;
		unsigned int has_pad                     : 30;
	};

	gltf_materialTexture_t baseColorTexture;
	cc_vec4f_t             baseColorFactor;
	gltf_materialTexture_t metalicRoughnessTexture;
	float                  metallicFactor;
	float                  roughnessFactor;
} gltf_materialPbrMetallicRoughness_t;

typedef struct gltf_materialNormalTexture_s
{
	gltf_materialTexture_t base;
	float                  scale;
} gltf_materialNormalTexture_t;

typedef struct gltf_materialOcclusionTexture_s
{
	gltf_materialTexture_t base;
	float                  strength;
} gltf_materialOcclusionTexture_t;

typedef enum
{
	// TODO - MASK and alpha cutoff (default 0.5)
	GLTF_MATERIAL_ALPHAMODE_OPAQUE,
	GLTF_MATERIAL_ALPHAMODE_BLEND,
} gltf_materialAlphaMode_e;

typedef struct gltf_material_s
{
	struct
	{
		unsigned int has_normalTexture    : 1;
		unsigned int has_occlusionTexture : 1;
		unsigned int has_emissiveTexture  : 1;
		unsigned int has_pad              : 29;
	};

	gltf_materialPbrMetallicRoughness_t pbrMetallicRoughness;
	gltf_materialNormalTexture_t        normalTexture;
	gltf_materialOcclusionTexture_t     occlusionTexture;
	gltf_materialTexture_t              emissiveTexture;
	cc_vec3f_t                          emissiveFactor;
	gltf_materialAlphaMode_e            alphaMode;
	int                                 doubleSided;
} gltf_material_t;

typedef enum
{
	GLTF_ACCESSOR_TYPE_UNKNOWN,
	GLTF_ACCESSOR_TYPE_SCALAR,
	GLTF_ACCESSOR_TYPE_VEC2,
	GLTF_ACCESSOR_TYPE_VEC3,
	GLTF_ACCESSOR_TYPE_VEC4,
	GLTF_ACCESSOR_TYPE_MAT2,
	GLTF_ACCESSOR_TYPE_MAT3,
	GLTF_ACCESSOR_TYPE_MAT4,
} gltf_accessorType_e;

// maps to OpenGL type (e.g. GL_FLOAT)
typedef enum
{
	GLTF_COMPONENT_TYPE_BYTE           = 0x1400,
	GLTF_COMPONENT_TYPE_UNSIGNED_BYTE  = 0x1401,
	GLTF_COMPONENT_TYPE_SHORT          = 0x1402,
	GLTF_COMPONENT_TYPE_UNSIGNED_SHORT = 0x1403,
	GLTF_COMPONENT_TYPE_UNSIGNED_INT   = 0x1405,
	GLTF_COMPONENT_TYPE_FLOAT          = 0x1406,
} gltf_componentType_e;

typedef struct gltf_accessor_s
{
	struct
	{
		unsigned int has_bufferView : 1;
		unsigned int has_minMax     : 1;
		unsigned int has_pad        : 30;
	};

	uint32_t bufferView;
	uint32_t byteOffset;

	gltf_accessorType_e  type;
	gltf_componentType_e componentType;

	uint32_t count;

	// size based on accessorType
	float min[4];
	float max[4];

	// TODO - sparse and indices blocks
} gltf_accessor_t;

typedef struct gltf_texture_s
{
	struct
	{
		unsigned int has_source : 1;
		unsigned int has_pad    : 31;
	};

	uint32_t source;
	// TODO - sampler
} gltf_texture_t;

typedef struct gltf_bufferView_s
{
	struct
	{
		unsigned int has_byteStride : 1;
		unsigned int has_pad        : 31;
	};

	uint32_t buffer;
	uint32_t byteOffset;
	uint32_t byteLength;
	uint32_t byteStride;
	// TODO - optional target
} gltf_bufferView_t;

typedef enum
{
	GLTF_IMAGE_TYPE_UNKNOWN,
	GLTF_IMAGE_TYPE_PNG,
	GLTF_IMAGE_TYPE_JPG,
} gltf_imageType_e;

typedef struct gltf_image_s
{
	struct
	{
		unsigned int has_bufferView : 1;
		unsigned int has_pad        : 31;
	};

	uint32_t bufferView;

	gltf_imageType_e type;
} gltf_image_t;

typedef struct gltf_buffer_s
{
	uint32_t byteLength;
} gltf_buffer_t;

typedef struct gltf_file_s
{
	uint32_t   scene;
	cc_list_t* scenes;
	cc_list_t* nodes;
	cc_list_t* cameras;
	cc_list_t* meshes;
	cc_list_t* materials;
	cc_list_t* accessors;
	cc_list_t* textures;
	cc_list_t* bufferViews;
	cc_list_t* images;
	cc_list_t* buffers;
	// TODO - samplers, skins and animations

	// file data
	size_t length;
	char*  data;
} gltf_file_t;

gltf_file_t*       gltf_file_open(const char* fname);
gltf_file_t*       gltf_file_openf(FILE* f, size_t size);
void               gltf_file_close(gltf_file_t** _self);
gltf_scene_t*      gltf_file_getScene(gltf_file_t* self,
                                      uint32_t idx);
gltf_node_t*       gltf_file_getNode(gltf_file_t* self,
                                     uint32_t idx);
gltf_camera_t*     gltf_file_getCamera(gltf_file_t* self,
                                       uint32_t idx);
gltf_mesh_t*       gltf_file_getMesh(gltf_file_t* self,
                                     uint32_t idx);
gltf_material_t*   gltf_file_getMaterial(gltf_file_t* self,
                                         uint32_t idx);
gltf_accessor_t*   gltf_file_getAccessor(gltf_file_t* self,
                                         uint32_t idx);
gltf_texture_t*    gltf_file_getTexture(gltf_file_t* self,
                                        uint32_t idx);
gltf_bufferView_t* gltf_file_getBufferView(gltf_file_t* self,
                                           uint32_t idx);
gltf_image_t*      gltf_file_getImage(gltf_file_t* self,
                                      uint32_t idx);
const char*        gltf_file_getBuffer(gltf_file_t* self,
                                       gltf_bufferView_t* bufferView);

#endif
