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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef GLTF_DEBUG
	#define LOG_DEBUG
#endif
#define LOG_TAG "gltf"
#include "../libcc/cc_log.h"
#include "../libcc/cc_memory.h"
#include "../libcc/jsmn/cc_jsmnWrapper.h"
#include "gltf.h"

typedef struct
{
	uint32_t magic;
	uint32_t version;
	uint32_t length;
} gltf_header_t;

typedef enum
{
	GLTF_CHUNK_TYPE_JSON = 0x4E4F534A,
	GLTF_CHUNK_TYPE_BIN  = 0x004E4942,
} gltf_chunkType_e;

typedef struct
{
	uint32_t chunkLength;
	uint32_t chunkType;
} gltf_chunk_t;

/***********************************************************
* private - objects                                        *
***********************************************************/

static uint32_t gltf_val_uint32(cc_jsmnVal_t* val)
{
	ASSERT(val);

	uint32_t x = 0;
	if((val->type == CC_JSMN_TYPE_STRING) ||
	   (val->type == CC_JSMN_TYPE_PRIMITIVE))
	{
		x = (uint32_t) strtol(val->data, NULL, 0);
	}
	else
	{
		LOGE("invalid type=%u", val->type);
	}

	return x;
}

static float gltf_val_float(cc_jsmnVal_t* val)
{
	ASSERT(val);

	float x = 0;
	if((val->type == CC_JSMN_TYPE_STRING) ||
	   (val->type == CC_JSMN_TYPE_PRIMITIVE))
	{
		x = (float) strtod(val->data, NULL);
	}
	else
	{
		LOGE("invalid type=%u", val->type);
	}

	return x;
}

static void
gltf_val_string(cc_jsmnVal_t* val, char* str)
{
	ASSERT(val);
	ASSERT(str);

	if(val->type == CC_JSMN_TYPE_STRING)
	{
		snprintf(str, 256, "%s", val->data);
	}
	else
	{
		LOGE("invalid type=%u", val->type);
		snprintf(str, 256, "%s", "");
	}
}

static int
gltf_val_floats(cc_jsmnVal_t* val, uint32_t count,
                float* x)
{
	ASSERT(val);
	ASSERT(count > 0);
	ASSERT(x);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%u", val->type);
		return 0;
	}

	cc_jsmnArray_t* array = val->array;
	if(cc_list_size(array->list) != count)
	{
		LOGE("invalid size=%i", cc_list_size(array->list));
		return 0;
	}

	int idx = 0;
	cc_listIter_t* iter = cc_list_head(array->list);
	while(iter)
	{
		cc_jsmnVal_t* item;
		item   = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		x[idx] = gltf_val_float(item);
		++idx;

		iter = cc_list_next(iter);
	}

	return 1;
}

static int
gltf_node_parseChildren(gltf_node_t* self,
                        cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*  item;
	uint32_t*      nd;
	cc_listIter_t* iter = cc_list_head(val->array->list);
	while(iter)
	{
		item = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		nd   = (uint32_t*) CALLOC(1, sizeof(uint32_t));
		if(nd == NULL)
		{
			return 0;
		}
		*nd = gltf_val_uint32(item);

		if(cc_list_append(self->children, NULL,
		                  (const void*) nd) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		FREE(nd);
	return 0;
}

static gltf_node_t* gltf_node_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_node_t* self;
	self = (gltf_node_t*) CALLOC(1, sizeof(gltf_node_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	self->children = cc_list_new();
	if(self->children == NULL)
	{
		goto fail_list;
	}

	cc_mat4f_t translate;
	cc_mat4f_t rotate;
	cc_mat4f_t scale;
	cc_mat4f_t matrix;
	cc_mat4f_identity(&translate);
	cc_mat4f_identity(&rotate);
	cc_mat4f_identity(&scale);
	cc_mat4f_identity(&matrix);

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "mesh") == 0)
		{
			self->mesh     = gltf_val_uint32(kv->val);
			self->has_mesh = 1;
		}
		else if(strcmp(kv->key, "name") == 0)
		{
			gltf_val_string(kv->val, self->name);
		}
		else if(strcmp(kv->key, "camera") == 0)
		{
			self->camera     = gltf_val_uint32(kv->val);
			self->has_camera = 1;
		}
		else if(strcmp(kv->key, "matrix") == 0)
		{
			gltf_val_floats(kv->val, 16, (float*) &matrix);
		}
		else if(strcmp(kv->key, "translation") == 0)
		{
			float t[3] = { 0.0f, 0.0f, 0.0f };
			gltf_val_floats(kv->val, 3, t);
			cc_mat4f_translate(&translate, 1, t[0], t[1], t[2]);
		}
		else if(strcmp(kv->key, "rotation") == 0)
		{
			float r[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			gltf_val_floats(kv->val, 4, r);
			cc_mat4f_rotate(&rotate, 1, r[0], r[1], r[2], r[3]);
		}
		else if(strcmp(kv->key, "scale") == 0)
		{
			float s[3] = { 1.0f, 1.0f, 1.0f };
			gltf_val_floats(kv->val, 3, s);
			cc_mat4f_scale(&scale, 1, s[0], s[1], s[2]);
		}
		else if(strcmp(kv->key, "children") == 0)
		{
			if(gltf_node_parseChildren(self, kv->val) == 0)
			{
				goto fail_children;
			}
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	cc_mat4f_copy(&self->matrix, &matrix);
	cc_mat4f_mulm(&self->matrix, &translate);
	cc_mat4f_mulm(&self->matrix, &rotate);
	cc_mat4f_mulm(&self->matrix, &scale);

	// success
	return self;

	// failure
	fail_children:
	{
		cc_listIter_t* iter = cc_list_head(self->children);
		while(iter)
		{
			uint32_t* nd;
			nd = (uint32_t*)
			     cc_list_remove(self->children, &iter);
			FREE(nd);
		}
		cc_list_delete(&self->children);
	}
	fail_list:
		FREE(self);
	return NULL;
}

static void gltf_node_delete(gltf_node_t** _self)
{
	ASSERT(_self);

	gltf_node_t* self = *_self;
	if(self)
	{
		cc_listIter_t* iter = cc_list_head(self->children);
		while(iter)
		{
			uint32_t* nd;
			nd = (uint32_t*)
			     cc_list_remove(self->children, &iter);
			FREE(nd);
		}

		cc_list_delete(&self->children);
		FREE(self);
		*_self = NULL;
	}
}

static void
gltf_camera_parseType(gltf_camera_t* self,
                      cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_STRING)
	{
		LOGE("invalid type=%u", val->type);
		return;
	}

	if(strcmp(val->data, "perspective") == 0)
	{
		self->type = GLTF_CAMERA_TYPE_PERSPECTIVE;
	}
	else if(strcmp(val->data, "orthographic") == 0)
	{
		self->type = GLTF_CAMERA_TYPE_ORTHOGRAPHIC;
	}
	else
	{
		LOGE("invalid data=%s", val->data);
	}
}

static void
gltf_camera_parsePerspective(gltf_camera_t* self,
                             cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return;
	}

	gltf_cameraPerspective_t* cp = &self->cameraPerspective;

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "aspectRatio") == 0)
		{
			cp->aspectRatio = gltf_val_float(kv->val);
		}
		else if(strcmp(kv->key, "yfov") == 0)
		{
			cp->yfov = gltf_val_float(kv->val);
		}
		else if(strcmp(kv->key, "zfar") == 0)
		{
			cp->zfar = gltf_val_float(kv->val);
		}
		else if(strcmp(kv->key, "znear") == 0)
		{
			cp->znear = gltf_val_float(kv->val);
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}
}

static void
gltf_camera_parseOrthographic(gltf_camera_t* self,
                              cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return;
	}

	gltf_cameraOrthographic_t* co = &self->cameraOrthographic;

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "xmag") == 0)
		{
			co->xmag = gltf_val_float(kv->val);
		}
		else if(strcmp(kv->key, "ymag") == 0)
		{
			co->ymag = gltf_val_float(kv->val);
		}
		else if(strcmp(kv->key, "zfar") == 0)
		{
			co->zfar = gltf_val_float(kv->val);
		}
		else if(strcmp(kv->key, "znear") == 0)
		{
			co->znear = gltf_val_float(kv->val);
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}
}

static gltf_camera_t*
gltf_camera_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_camera_t* self;
	self = (gltf_camera_t*) CALLOC(1, sizeof(gltf_camera_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	// required members
	int has_perspective  = 0;
	int has_orthographic = 0;

	cc_jsmnObject_t* obj = val->obj;
	cc_listIter_t* iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);
		if(strcmp(kv->key, "type") == 0)
		{
			gltf_camera_parseType(self, kv->val);
		}
		else if(strcmp(kv->key, "perspective") == 0)
		{
			gltf_camera_parsePerspective(self, kv->val);
			has_perspective = 1;
		}
		else if(strcmp(kv->key, "orthographic") == 0)
		{
			gltf_camera_parseOrthographic(self, kv->val);
			has_orthographic = 1;
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	// check for required members
	if((self->type == GLTF_CAMERA_TYPE_PERSPECTIVE) &&
	   (has_perspective  && (has_orthographic == 0)))
	{
		return self;
	}
	else if((self->type == GLTF_CAMERA_TYPE_ORTHOGRAPHIC) &&
	        (has_orthographic && (has_perspective == 0)))
	{
		return self;
	}

	LOGE("invalid type=%u, has_perspective=%i, has_orthographic=%i",
	     self->type, has_perspective, has_orthographic);
	FREE(self);
	return NULL;
}

static void gltf_camera_delete(gltf_camera_t** _self)
{
	ASSERT(_self);

	gltf_camera_t* self = *_self;
	if(self)
	{
		FREE(self);
		*_self = NULL;
	}
}

static gltf_attribute_t*
gltf_attribute_new(cc_jsmnKeyval_t* kv)
{
	ASSERT(kv);

	gltf_attribute_t* self;
	self = (gltf_attribute_t*)
	       CALLOC(1, sizeof(gltf_attribute_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	snprintf(self->name, 256, "%s", kv->key);
	self->accessor = gltf_val_uint32(kv->val);

	return self;
}

static void
gltf_attribute_delete(gltf_attribute_t** _self)
{
	ASSERT(_self);

	gltf_attribute_t* self = *_self;
	if(self)
	{
		FREE(self);
		*_self = NULL;
	}
}

static int
gltf_primitive_parseAttributes(gltf_primitive_t* self,
                               cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return 0;
	}

	cc_jsmnObject_t*  obj  = val->obj;
	cc_listIter_t*    iter = cc_list_head(obj->list);
	gltf_attribute_t* attr;
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		attr = gltf_attribute_new(kv);
		if(attr == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->attributes, NULL, attr) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_attribute_delete(&attr);
	return 0;
}

static gltf_primitive_t*
gltf_primitive_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_primitive_t* self;
	self = (gltf_primitive_t*)
	       CALLOC(1, sizeof(gltf_primitive_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	// defaults
	self->mode = GLTF_PRIMITIVE_MODE_TRIANGLES;

	self->attributes = cc_list_new();
	if(self->attributes == NULL)
	{
		goto fail_list;
	}

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);
		if(strcmp(kv->key, "mode") == 0)
		{
			self->mode = (gltf_primitiveMode_e)
			             gltf_val_uint32(kv->val);
		}
		else if(strcmp(kv->key, "indices") == 0)
		{
			self->indices     = gltf_val_uint32(kv->val);
			self->has_indices = 1;
		}
		else if(strcmp(kv->key, "material") == 0)
		{
			self->material     = gltf_val_uint32(kv->val);
			self->has_material = 1;
		}
		else if(strcmp(kv->key, "attributes") == 0)
		{
			if(gltf_primitive_parseAttributes(self, kv->val) == 0)
			{
				goto fail_attributes;
			}
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	// success
	return self;

	// failure
	fail_attributes:
	{
		iter = cc_list_head(self->attributes);
		while(iter)
		{
			gltf_attribute_t* attr;
			attr = (gltf_attribute_t*)
			       cc_list_remove(self->attributes, &iter);
			gltf_attribute_delete(&attr);
		}
		cc_list_delete(&self->attributes);
	}
	fail_list:
		FREE(self);
	return NULL;
}

static void
gltf_primitive_delete(gltf_primitive_t** _self)
{
	ASSERT(_self);

	gltf_primitive_t* self = *_self;
	if(self)
	{
		cc_listIter_t* iter = cc_list_head(self->attributes);
		while(iter)
		{
			gltf_attribute_t* attr;
			attr = (gltf_attribute_t*)
			       cc_list_remove(self->attributes, &iter);
			gltf_attribute_delete(&attr);
		}

		cc_list_delete(&self->attributes);
		FREE(self);
		*_self = NULL;
	}
}

static int
gltf_mesh_parsePrimitives(gltf_mesh_t* self,
                          cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*     item;
	gltf_primitive_t* prim;
	cc_listIter_t*    iter = cc_list_head(val->array->list);
	while(iter)
	{
		item = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		prim = gltf_primitive_new(item);
		if(prim == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->primitives, NULL,
		                  (const void*) prim) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_primitive_delete(&prim);
	return 0;
}

static gltf_mesh_t* gltf_mesh_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_mesh_t* self;
	self = (gltf_mesh_t*) CALLOC(1, sizeof(gltf_mesh_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	self->primitives = cc_list_new();
	if(self->primitives == NULL)
	{
		goto fail_list;
	}

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t* iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);
		if(strcmp(kv->key, "primitives") == 0)
		{
			if(gltf_mesh_parsePrimitives(self, kv->val) == 0)
			{
				goto fail_primitives;
			}
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	// success
	return self;

	// failure
	fail_primitives:
	{
		cc_listIter_t* iter = cc_list_head(self->primitives);
		while(iter)
		{
			gltf_primitive_t* prim;
			prim = (gltf_primitive_t*)
			       cc_list_remove(self->primitives, &iter);
			gltf_primitive_delete(&prim);
		}
		cc_list_delete(&self->primitives);
	}
	fail_list:
		FREE(self);
	return NULL;
}

static void gltf_mesh_delete(gltf_mesh_t** _self)
{
	ASSERT(_self);

	gltf_mesh_t* self = *_self;
	if(self)
	{
		cc_listIter_t* iter = cc_list_head(self->primitives);
		while(iter)
		{
			gltf_primitive_t* prim;
			prim = (gltf_primitive_t*)
			       cc_list_remove(self->primitives, &iter);
			gltf_primitive_delete(&prim);
		}

		cc_list_delete(&self->primitives);
		FREE(self);
		*_self = NULL;
	}
}

static int
gltf_materialTexture_parse(gltf_materialTexture_t* self,
                           cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return 0;
	}

	// required members
	int has_index    = 0;
	int has_texCoord = 0;

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "index") == 0)
		{
			self->index = gltf_val_uint32(kv->val);
			has_index   = 1;
		}
		else if(strcmp(kv->key, "texCoord") == 0)
		{
			self->texCoord = gltf_val_uint32(kv->val);
			has_texCoord   = 1;
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	// check for required members
	if(has_index == 0)
	{
		LOGE("invalid has_index=%i, has_texCoord=%i",
		     has_index, has_texCoord);
		return 0;
	}

	return 1;
}

static int
gltf_material_parsePbrMetallicRoughness(gltf_material_t* self,
                                        cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return 0;
	}

	gltf_materialPbrMetallicRoughness_t* pbr;
	pbr = &self->pbrMetallicRoughness;

	int              ret  = 1;
	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "baseColorTexture") == 0)
		{
			ret &= gltf_materialTexture_parse(&pbr->baseColorTexture,
			                                  kv->val);
			pbr->has_baseColorTexture = 1;
		}
		else if(strcmp(kv->key, "baseColorFactor") == 0)
		{
			gltf_val_floats(kv->val, 4,
			                (float*) &pbr->baseColorFactor);
		}
		else if(strcmp(kv->key, "metalicRoughnessTexture") == 0)
		{
			ret &= gltf_materialTexture_parse(&pbr->metalicRoughnessTexture,
			                                  kv->val);
			pbr->has_metalicRoughnessTexture = 1;
		}
		else if(strcmp(kv->key, "metallicFactor") == 0)
		{
			pbr->metallicFactor = gltf_val_float(kv->val);
		}
		else if(strcmp(kv->key, "roughnessFactor") == 0)
		{
			pbr->roughnessFactor = gltf_val_float(kv->val);
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	return ret;
}

static int
gltf_material_parseNormalTexture(gltf_material_t* self,
                                 cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return 0;
	}

	gltf_materialNormalTexture_t* nt = &self->normalTexture;
	if(gltf_materialTexture_parse(&nt->base, val) == 0)
	{
		return 0;
	}

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "scale") == 0)
		{
			nt->scale = gltf_val_float(kv->val);
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	return 1;
}

static int
gltf_material_parseOcclusionTexture(gltf_material_t* self,
                                    cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return 0;
	}

	gltf_materialOcclusionTexture_t* ot;
	ot = &self->occlusionTexture;
	if(gltf_materialTexture_parse(&ot->base, val) == 0)
	{
		return 0;
	}

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "strength") == 0)
		{
			ot->strength = gltf_val_float(kv->val);
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	return 1;
}

static void
gltf_material_parseDoubleSided(gltf_material_t* self,
                               cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if((val->type == CC_JSMN_TYPE_STRING) ||
	   (val->type == CC_JSMN_TYPE_PRIMITIVE))
	{
		if(strcmp(val->data, "true") == 0)
		{
			self->doubleSided = 1;
		}
		else
		{
			self->doubleSided = 0;
		}
	}
	else
	{
		LOGE("invalid type=%u", val->type);
	}
}

static void
gltf_material_parseAlphaMode(gltf_material_t* self,
                             cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_STRING)
	{
		LOGE("invalid type=%u", val->type);
		return;
	}

	if(strcmp(val->data, "OPAQUE") == 0)
	{
		self->alphaMode = GLTF_MATERIAL_ALPHAMODE_OPAQUE;
	}
	else if(strcmp(val->data, "BLEND") == 0)
	{
		self->alphaMode = GLTF_MATERIAL_ALPHAMODE_BLEND;
	}
	else
	{
		LOGD("unsupported %s", val->data);
		self->alphaMode = GLTF_MATERIAL_ALPHAMODE_BLEND;
	}
}

static gltf_material_t*
gltf_material_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_material_t* self;
	self = (gltf_material_t*)
	       CALLOC(1, sizeof(gltf_material_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	// defaults
	cc_vec4f_load(&self->pbrMetallicRoughness.baseColorFactor,
	              1.0f, 1.0f, 1.0f, 1.0f);
	self->pbrMetallicRoughness.metallicFactor  = 1.0f;
	self->pbrMetallicRoughness.roughnessFactor = 1.0f;
	self->normalTexture.scale                  = 1.0f;
	self->occlusionTexture.strength            = 1.0f;

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);
		if(strcmp(kv->key, "pbrMetallicRoughness") == 0)
		{
			if(gltf_material_parsePbrMetallicRoughness(self,
			                                           kv->val) == 0)
			{
				goto fail_parse;
			}
		}
		else if(strcmp(kv->key, "normalTexture") == 0)
		{
			if(gltf_material_parseNormalTexture(self, kv->val) == 0)
			{
				goto fail_parse;
			}
			self->has_normalTexture = 1;
		}
		else if(strcmp(kv->key, "occlusionTexture") == 0)
		{
			if(gltf_material_parseOcclusionTexture(self,
			                                       kv->val) == 0)
			{
				goto fail_parse;
			}
			self->has_occlusionTexture = 1;
		}
		else if(strcmp(kv->key, "emissiveTexture") == 0)
		{
			if(gltf_materialTexture_parse(&self->emissiveTexture,
			                              kv->val) == 0)
			{
				goto fail_parse;
			}
			self->has_emissiveTexture = 1;
		}
		else if(strcmp(kv->key, "emissiveFactor") == 0)
		{
			gltf_val_floats(kv->val, 3,
			                (float*) &self->emissiveFactor);
		}
		else if(strcmp(kv->key, "doubleSided") == 0)
		{
			gltf_material_parseDoubleSided(self, kv->val);
		}
		else if(strcmp(kv->key, "alphaMode") == 0)
		{
			gltf_material_parseAlphaMode(self, kv->val);
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	// success
	return self;

	// failure
	fail_parse:
		FREE(self);
	return NULL;
}

static void gltf_material_delete(gltf_material_t** _self)
{
	ASSERT(_self);

	gltf_material_t* self = *_self;
	if(self)
	{
		FREE(self);
		*_self = NULL;
	}
}

static int
gltf_accessor_parseType(gltf_accessor_t* self,
                        cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_STRING)
	{
		LOGE("invalid type=%u", val->type);
		return 0;
	}

	if(strcmp(val->data, "SCALAR") == 0)
	{
		self->type = GLTF_ACCESSOR_TYPE_SCALAR;
	}
	else if(strcmp(val->data, "VEC2") == 0)
	{
		self->type = GLTF_ACCESSOR_TYPE_VEC2;
	}
	else if(strcmp(val->data, "VEC3") == 0)
	{
		self->type = GLTF_ACCESSOR_TYPE_VEC3;
	}
	else if(strcmp(val->data, "VEC4") == 0)
	{
		self->type = GLTF_ACCESSOR_TYPE_VEC4;
	}
	else if(strcmp(val->data, "MAT2") == 0)
	{
		self->type = GLTF_ACCESSOR_TYPE_MAT2;
	}
	else if(strcmp(val->data, "MAT3") == 0)
	{
		self->type = GLTF_ACCESSOR_TYPE_MAT3;
	}
	else if(strcmp(val->data, "MAT4") == 0)
	{
		self->type = GLTF_ACCESSOR_TYPE_MAT4;
	}
	else
	{
		LOGE("invalid type=%s", val->data);
		return 0;
	}

	return 1;
}

static gltf_accessor_t*
gltf_accessor_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_accessor_t* self;
	self = (gltf_accessor_t*)
	       CALLOC(1, sizeof(gltf_accessor_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	int has_componentType = 0;
	int has_count         = 0;
	int has_min           = 0;
	int has_max           = 0;

	uint32_t         elem  = 0;
	cc_jsmnObject_t* obj   = val->obj;
	cc_listIter_t*   iter  = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "bufferView") == 0)
		{
			self->bufferView     = gltf_val_uint32(kv->val);
			self->has_bufferView = 1;
		}
		else if(strcmp(kv->key, "byteOffset") == 0)
		{
			self->byteOffset = gltf_val_uint32(kv->val);
		}
		else if(strcmp(kv->key, "type") == 0)
		{
			if(gltf_accessor_parseType(self, kv->val) == 0)
			{
				goto fail_type;
			}

			if(self->type == GLTF_ACCESSOR_TYPE_SCALAR)
			{
				elem = 1;
			}
			else if(self->type == GLTF_ACCESSOR_TYPE_VEC2)
			{
				elem = 2;
			}
			else if(self->type == GLTF_ACCESSOR_TYPE_VEC3)
			{
				elem = 3;
			}
			else if(self->type == GLTF_ACCESSOR_TYPE_VEC4)
			{
				elem = 4;
			}
		}
		else if(strcmp(kv->key, "componentType") == 0)
		{
			self->componentType = (gltf_componentType_e)
			                      gltf_val_uint32(kv->val);
			has_componentType   = 1;
		}
		else if(strcmp(kv->key, "count") == 0)
		{
			self->count = gltf_val_uint32(kv->val);
			has_count   = 1;
		}
		else if(strcmp(kv->key, "min") == 0)
		{
			if(elem &&
			   (self->componentType == GLTF_COMPONENT_TYPE_FLOAT) &&
			   gltf_val_floats(kv->val, elem, (float*) &self->min))
			{
				has_min = 1;
			}
		}
		else if(strcmp(kv->key, "max") == 0)
		{
			if(elem &&
			   (self->componentType == GLTF_COMPONENT_TYPE_FLOAT) &&
			   gltf_val_floats(kv->val, elem, (float*) &self->max))
			{
				has_max = 1;
			}
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	// combine min/max flag
	if(has_min && has_max)
	{
		self->has_minMax = 1;
	}

	// check for required members
	if((self->type == GLTF_ACCESSOR_TYPE_UNKNOWN) ||
	   (has_componentType == 0) || (has_count == 0))
	{
		LOGE("invalid type=%u, has_componentType=%i, has_count=%i",
		     (uint32_t) self->type, has_componentType, has_count);
		goto fail_member;
	}

	// success
	return self;

	// failure
	fail_member:
	fail_type:
		FREE(self);
	return NULL;
}

static void gltf_accessor_delete(gltf_accessor_t** _self)
{
	ASSERT(_self);

	gltf_accessor_t* self = *_self;
	if(self)
	{
		FREE(self);
		*_self = NULL;
	}
}

static gltf_texture_t*
gltf_texture_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_texture_t* self;
	self = (gltf_texture_t*)
	       CALLOC(1, sizeof(gltf_texture_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	cc_jsmnObject_t* obj = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);
		if(strcmp(kv->key, "source") == 0)
		{
			self->source     = gltf_val_uint32(kv->val);
			self->has_source = 1;
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	return self;
}

static void gltf_texture_delete(gltf_texture_t** _self)
{
	ASSERT(_self);

	gltf_texture_t* self = *_self;
	if(self)
	{
		FREE(self);
		*_self = NULL;
	}
}

static gltf_bufferView_t*
gltf_bufferView_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_bufferView_t* self;
	self = (gltf_bufferView_t*)
	       CALLOC(1, sizeof(gltf_bufferView_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	// required members
	int has_buffer     = 0;
	int has_byteLength = 0;

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);
		if(strcmp(kv->key, "buffer") == 0)
		{
			self->buffer = gltf_val_uint32(kv->val);
			has_buffer   = 1;
		}
		else if(strcmp(kv->key, "byteOffset") == 0)
		{
			self->byteOffset = gltf_val_uint32(kv->val);
		}
		else if(strcmp(kv->key, "byteLength") == 0)
		{
			self->byteLength = gltf_val_uint32(kv->val);
			has_byteLength   = 1;
		}
		else if(strcmp(kv->key, "byteStride") == 0)
		{
			self->byteStride     = gltf_val_uint32(kv->val);
			self->has_byteStride = 1;
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	// check for required members
	if((has_buffer == 0) || (has_byteLength == 0))
	{
		LOGE("invalid has_buffer=%i, has_byteLength=%i",
		     has_buffer, has_byteLength);
		FREE(self);
		return NULL;
	}

	return self;
}

static void gltf_bufferView_delete(gltf_bufferView_t** _self)
{
	ASSERT(_self);

	gltf_bufferView_t* self = *_self;
	if(self)
	{
		FREE(self);
		*_self = NULL;
	}
}

static int gltf_image_parseMimeType(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_STRING)
	{
		LOGE("invalid type=%u", val->type);
		return GLTF_IMAGE_TYPE_UNKNOWN;
	}

	if(strcmp(val->data, "image/png") == 0)
	{
		return GLTF_IMAGE_TYPE_PNG;
	}
	else if(strcmp(val->data, "image/jpeg") == 0)
	{
		return GLTF_IMAGE_TYPE_JPG;
	}

	return GLTF_IMAGE_TYPE_UNKNOWN;
}

static gltf_image_t* gltf_image_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_image_t* self;
	self = (gltf_image_t*)
	       CALLOC(1, sizeof(gltf_image_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "bufferView") == 0)
		{
			self->bufferView     = gltf_val_uint32(kv->val);
			self->has_bufferView = 1;
		}
		else if(strcmp(kv->key, "mimeType") == 0)
		{
			if(gltf_image_parseMimeType(kv->val) == 0)
			{
				goto fail_type;
			}
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	// success
	return self;

	// failure
	fail_type:
		FREE(self);
	return NULL;
}

static void gltf_image_delete(gltf_image_t** _self)
{
	ASSERT(_self);

	gltf_image_t* self = *_self;
	if(self)
	{
		FREE(self);
		*_self = NULL;
	}
}

static gltf_buffer_t* gltf_buffer_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_buffer_t* self;
	self = (gltf_buffer_t*)
	       CALLOC(1, sizeof(gltf_buffer_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	// required members
	int has_byteLength = 0;

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t* iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "byteLength") == 0)
		{
			self->byteLength = gltf_val_uint32(kv->val);
			has_byteLength   = 1;
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	// check for required members
	if(has_byteLength == 0)
	{
		LOGE("invalid has_byteLength=%i", has_byteLength);
		FREE(self);
		return NULL;
	}

	return self;
}

static void gltf_buffer_delete(gltf_buffer_t** _self)
{
	ASSERT(_self);

	gltf_buffer_t* self = *_self;
	if(self)
	{
		FREE(self);
		*_self = NULL;
	}
}

static int
gltf_scene_parseNodes(gltf_scene_t* self, cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*  item;
	uint32_t*      nd;
	cc_listIter_t* iter = cc_list_head(val->array->list);
	while(iter)
	{
		item = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		nd   = (uint32_t*) CALLOC(1, sizeof(uint32_t));
		if(nd == NULL)
		{
			return 0;
		}
		*nd = gltf_val_uint32(item);

		if(cc_list_append(self->nodes, NULL,
		                  (const void*) nd) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		FREE(nd);
	return 0;
}

static gltf_scene_t* gltf_scene_new(cc_jsmnVal_t* val)
{
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%u", val->type);
		return NULL;
	}

	gltf_scene_t* self;
	self = (gltf_scene_t*) CALLOC(1, sizeof(gltf_scene_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	self->nodes = cc_list_new();
	if(self->nodes == NULL)
	{
		goto fail_list;
	}

	cc_jsmnObject_t* obj  = val->obj;
	cc_listIter_t* iter = cc_list_head(obj->list);
	while(iter)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);

		if(strcmp(kv->key, "name") == 0)
		{
			gltf_val_string(kv->val, self->name);
		}
		else if(strcmp(kv->key, "nodes") == 0)
		{
			if(gltf_scene_parseNodes(self, kv->val) == 0)
			{
				goto fail_nodes;
			}
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	// success
	return self;

	// failure
	fail_nodes:
	{
		cc_listIter_t* iter = cc_list_head(self->nodes);
		while(iter)
		{
			uint32_t* nd;
			nd = (uint32_t*) cc_list_remove(self->nodes, &iter);
			FREE(nd);
		}

		cc_list_delete(&self->nodes);
	}
	fail_list:
		FREE(self);
	return NULL;
}

static void gltf_scene_delete(gltf_scene_t** _self)
{
	ASSERT(_self);

	gltf_scene_t* self = *_self;
	if(self)
	{
		cc_listIter_t* iter = cc_list_head(self->nodes);
		while(iter)
		{
			uint32_t* nd;
			nd = (uint32_t*) cc_list_remove(self->nodes, &iter);
			FREE(nd);
		}

		cc_list_delete(&self->nodes);
		FREE(self);
		*_self = NULL;
	}
}

/***********************************************************
* private - file                                           *
***********************************************************/

static int
gltf_file_parseHeader(gltf_file_t* self)
{
	ASSERT(self);

	gltf_header_t* header = (gltf_header_t*) self->data;
	if((header->magic   != 0x46546C67) ||
	   (header->version != 2)          ||
	   (header->length  != self->length))
	{
		LOGE("magic=0x%X, version=%u, length=%u",
		      header->magic, header->version, header->length);
		return 0;
	}

	return 1;
}

static int
gltf_file_parseDefaultScene(gltf_file_t* self,
                            cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if((val->type == CC_JSMN_TYPE_STRING) ||
	   (val->type == CC_JSMN_TYPE_PRIMITIVE))
	{
		self->scene = (uint32_t) strtol(val->data, NULL, 0);
		return 1;
	}

	LOGE("invalid type=%i", val->type);
	return 0;
}

static int
gltf_file_parseScenes(gltf_file_t* self, cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*  item;
	gltf_scene_t*  scene;
	cc_listIter_t* iter = cc_list_head(val->array->list);
	while(iter)
	{
		item  = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		scene = gltf_scene_new(item);
		if(scene == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->scenes, NULL,
		                  (const void*) scene) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_scene_delete(&scene);
	return 0;
}

static int
gltf_file_parseNodes(gltf_file_t* self, cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*  item;
	gltf_node_t*   node;
	cc_listIter_t* iter = cc_list_head(val->array->list);
	while(iter)
	{
		item  = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		node = gltf_node_new(item);
		if(node == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->nodes, NULL,
		                  (const void*) node) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_node_delete(&node);
	return 0;
}

static int
gltf_file_parseCameras(gltf_file_t* self, cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*  item;
	gltf_camera_t* camera;
	cc_listIter_t* iter = cc_list_head(val->array->list);
	while(iter)
	{
		item   = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		camera = gltf_camera_new(item);
		if(camera == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->cameras, NULL,
		                  (const void*) camera) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_camera_delete(&camera);
	return 0;
}

static int
gltf_file_parseMeshes(gltf_file_t* self, cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*  item;
	gltf_mesh_t*   mesh;
	cc_listIter_t* iter = cc_list_head(val->array->list);
	while(iter)
	{
		item = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		mesh = gltf_mesh_new(item);
		if(mesh == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->meshes, NULL,
		                  (const void*) mesh) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_mesh_delete(&mesh);
	return 0;
}

static int
gltf_file_parseMaterials(gltf_file_t* self,
                         cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*    item;
	gltf_material_t* material;
	cc_listIter_t*   iter = cc_list_head(val->array->list);
	while(iter)
	{
		item     = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		material = gltf_material_new(item);
		if(material == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->materials, NULL,
		                  (const void*) material) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_material_delete(&material);
	return 0;
}

static int
gltf_file_parseAccessors(gltf_file_t* self,
                         cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*    item;
	gltf_accessor_t* accessor;
	cc_listIter_t*   iter = cc_list_head(val->array->list);
	while(iter)
	{
		item     = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		accessor = gltf_accessor_new(item);
		if(accessor == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->accessors, NULL,
		                  (const void*) accessor) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_accessor_delete(&accessor);
	return 0;
}

static int
gltf_file_parseTextures(gltf_file_t* self,
                        cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*   item;
	gltf_texture_t* texture;
	cc_listIter_t*  iter = cc_list_head(val->array->list);
	while(iter)
	{
		item     = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		texture = gltf_texture_new(item);
		if(texture == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->textures, NULL,
		                  (const void*) texture) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_texture_delete(&texture);
	return 0;
}

static int
gltf_file_parseBufferViews(gltf_file_t* self,
                           cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*      item;
	gltf_bufferView_t* bufferView;
	cc_listIter_t*     iter = cc_list_head(val->array->list);
	while(iter)
	{
		item       = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		bufferView = gltf_bufferView_new(item);
		if(bufferView == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->bufferViews, NULL,
		                  (const void*) bufferView) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_bufferView_delete(&bufferView);
	return 0;
}

static int
gltf_file_parseImages(gltf_file_t* self,
                      cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*  item;
	gltf_image_t*  image;
	cc_listIter_t* iter = cc_list_head(val->array->list);
	while(iter)
	{
		item  = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		image = gltf_image_new(item);
		if(image == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->images, NULL,
		                  (const void*) image) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_image_delete(&image);
	return 0;
}

static int
gltf_file_parseBuffers(gltf_file_t* self,
                       cc_jsmnVal_t* val)
{
	ASSERT(self);
	ASSERT(val);

	if(val->type != CC_JSMN_TYPE_ARRAY)
	{
		LOGE("invalid type=%i", val->type);
		return 0;
	}

	cc_jsmnVal_t*  item;
	gltf_buffer_t* buffer;
	cc_listIter_t* iter = cc_list_head(val->array->list);
	while(iter)
	{
		item   = (cc_jsmnVal_t*) cc_list_peekIter(iter);
		buffer = gltf_buffer_new(item);
		if(buffer == NULL)
		{
			return 0;
		}

		if(cc_list_append(self->buffers, NULL,
		                  (const void*) buffer) == NULL)
		{
			goto fail_append;
		}

		iter = cc_list_next(iter);
	}

	// success
	return 1;

	// failure
	fail_append:
		gltf_buffer_delete(&buffer);
	return 0;
}

static int
gltf_file_parseJson(gltf_file_t* self, gltf_chunk_t* chunk,
                    size_t offset)
{
	ASSERT(self);
	ASSERT(chunk);

	char* data = &self->data[offset + sizeof(gltf_chunk_t)];

	cc_jsmnVal_t* root;
	root = cc_jsmnVal_new(data, chunk->chunkLength);
	if(root == NULL)
	{
		return 0;
	}

	#ifdef GLTF_DEBUG
	cc_jsmnVal_print(root);
	#endif

	int result = 1;
	if(root->type != CC_JSMN_TYPE_OBJECT)
	{
		LOGE("invalid type=%i", root->type);
		result = 0;
	}

	cc_jsmnObject_t* obj  = root->obj;
	cc_listIter_t*   iter = cc_list_head(obj->list);
	while(iter && result)
	{
		cc_jsmnKeyval_t* kv;
		kv = (cc_jsmnKeyval_t*) cc_list_peekIter(iter);
		if(strcmp(kv->key, "scene") == 0)
		{
			result &= gltf_file_parseDefaultScene(self, kv->val);
		}
		else if(strcmp(kv->key, "scenes") == 0)
		{
			result &= gltf_file_parseScenes(self, kv->val);
		}
		else if(strcmp(kv->key, "nodes") == 0)
		{
			result &= gltf_file_parseNodes(self, kv->val);
		}
		else if(strcmp(kv->key, "cameras") == 0)
		{
			result &= gltf_file_parseCameras(self, kv->val);
		}
		else if(strcmp(kv->key, "meshes") == 0)
		{
			result &= gltf_file_parseMeshes(self, kv->val);
		}
		else if(strcmp(kv->key, "materials") == 0)
		{
			result &= gltf_file_parseMaterials(self, kv->val);
		}
		else if(strcmp(kv->key, "accessors") == 0)
		{
			result &= gltf_file_parseAccessors(self, kv->val);
		}
		else if(strcmp(kv->key, "textures") == 0)
		{
			result &= gltf_file_parseTextures(self, kv->val);
		}
		else if(strcmp(kv->key, "bufferViews") == 0)
		{
			result &= gltf_file_parseBufferViews(self, kv->val);
		}
		else if(strcmp(kv->key, "images") == 0)
		{
			result &= gltf_file_parseImages(self, kv->val);
		}
		else if(strcmp(kv->key, "buffers") == 0)
		{
			result &= gltf_file_parseBuffers(self, kv->val);
		}
		else
		{
			LOGD("unsupported key=%s", kv->key);
		}

		iter = cc_list_next(iter);
	}

	cc_jsmnVal_delete(&root);

	return result;
}

static int
gltf_file_parseChunk(gltf_file_t* self, size_t* _offset,
                     gltf_chunkType_e chunkType)
{
	ASSERT(self);
	ASSERT(_offset);

	size_t offset = *_offset;

	gltf_chunk_t* chunk;
	chunk = (gltf_chunk_t*) &self->data[offset];

	// check for buffer overruns
	offset += sizeof(gltf_chunk_t) + chunk->chunkLength;
	if(offset > self->length)
	{
		LOGE("offset=%" PRIu64 ", chunkLength=%" PRIu64,
		     (uint64_t) offset, (uint64_t) self->length);
		return 0;
	}

	#ifdef GLTF_DEBUG
	printf("CHUNK: offset=%u, length=%u, type=0x%X\n",
	     (uint32_t) *_offset, (uint32_t) chunk->chunkLength,
	     (uint32_t) chunk->chunkType);
	#endif

	// check expected chunkType
	if(chunk->chunkType != chunkType)
	{
		LOGE("invalid chunkType=%u", chunk->chunkType);
		return 0;
	}

	// validate and parse chunk
	if(chunk->chunkType == GLTF_CHUNK_TYPE_JSON)
	{
		if(gltf_file_parseJson(self, chunk, *_offset) == 0)
		{
			return 0;
		}
	}
	else if(chunk->chunkType == GLTF_CHUNK_TYPE_BIN)
	{
		// accept
	}
	else
	{
		LOGE("invalid chunkType=%u", chunk->chunkType);
		return 0;
	}

	// update offset
	*_offset = offset;

	return 1;
}

static void gltf_file_discard(gltf_file_t* self)
{
	ASSERT(self);

	cc_listIter_t* iter = cc_list_head(self->scenes);
	while(iter)
	{
		gltf_scene_t* scene;
		scene = (gltf_scene_t*)
		        cc_list_remove(self->scenes, &iter);
		gltf_scene_delete(&scene);
	}

	iter = cc_list_head(self->nodes);
	while(iter)
	{
		gltf_node_t* node;
		node = (gltf_node_t*)
		       cc_list_remove(self->nodes, &iter);
		gltf_node_delete(&node);
	}

	iter = cc_list_head(self->cameras);
	while(iter)
	{
		gltf_camera_t* camera;
		camera = (gltf_camera_t*)
		         cc_list_remove(self->cameras, &iter);
		gltf_camera_delete(&camera);
	}

	iter = cc_list_head(self->meshes);
	while(iter)
	{
		gltf_mesh_t* mesh;
		mesh = (gltf_mesh_t*)
		        cc_list_remove(self->meshes, &iter);
		gltf_mesh_delete(&mesh);
	}

	iter = cc_list_head(self->materials);
	while(iter)
	{
		gltf_material_t* material;
		material = (gltf_material_t*)
		           cc_list_remove(self->materials, &iter);
		gltf_material_delete(&material);
	}

	iter = cc_list_head(self->accessors);
	while(iter)
	{
		gltf_accessor_t* accessor;
		accessor = (gltf_accessor_t*)
		           cc_list_remove(self->accessors, &iter);
		gltf_accessor_delete(&accessor);
	}

	iter = cc_list_head(self->textures);
	while(iter)
	{
		gltf_texture_t* texture;
		texture = (gltf_texture_t*)
		          cc_list_remove(self->textures, &iter);
		gltf_texture_delete(&texture);
	}

	iter = cc_list_head(self->bufferViews);
	while(iter)
	{
		gltf_bufferView_t* bufferView;
		bufferView = (gltf_bufferView_t*)
		             cc_list_remove(self->bufferViews, &iter);
		gltf_bufferView_delete(&bufferView);
	}

	iter = cc_list_head(self->images);
	while(iter)
	{
		gltf_image_t* image;
		image = (gltf_image_t*)
		        cc_list_remove(self->images, &iter);
		gltf_image_delete(&image);
	}

	iter = cc_list_head(self->buffers);
	while(iter)
	{
		gltf_buffer_t* buffer;
		buffer = (gltf_buffer_t*)
		         cc_list_remove(self->buffers, &iter);
		gltf_buffer_delete(&buffer);
	}
}

/***********************************************************
* public                                                   *
***********************************************************/

gltf_file_t* gltf_file_open(const char* fname)
{
	ASSERT(fname);

	FILE* f = fopen(fname, "r");
	if(f == NULL)
	{
		LOGE("fopen %s failed", fname);
		return NULL;
	}

	// get file lenth
	if(fseek(f, (long) 0, SEEK_END) == -1)
	{
		LOGE("fseek_end fname=%s", fname);
		goto fail_fseek_end;
	}
	size_t length = ftell(f);

	// rewind to start
	if(fseek(f, 0, SEEK_SET) == -1)
	{
		LOGE("fseek_set fname=%s", fname);
		goto fail_fseek_set;
	}

	gltf_file_t* self = gltf_file_openf(f, length);
	if(self == NULL)
	{
		goto fail_openf;
	}

	fclose(f);

	// success
	return self;

	// failure
	fail_openf:
	fail_fseek_set:
	fail_fseek_end:
		fclose(f);
	return NULL;
}

gltf_file_t* gltf_file_openf(FILE* f, size_t length)
{
	ASSERT(f);

	// allocate data
	char* data = (char*) CALLOC(length, sizeof(char));
	if(data == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	// read data
	if(fread((void*) data, length, 1, f) != 1)
	{
		LOGE("fread failed");
		goto fail_read_data;
	}

	gltf_file_t* self;
	self = gltf_file_openb(data, length, GLTF_FILEMODE_OWNED);
	if(self == NULL)
	{
		goto fail_openb;
	}

	// success
	return self;

	// failure
	fail_openb:
	fail_read_data:
		FREE(data);
	return NULL;
}

gltf_file_t*
gltf_file_openb(char* data, size_t size,
                gltf_fileMode_e mode)
{
	ASSERT(data);

	// check minimum file size
	if(size < sizeof(gltf_header_t))
	{
		LOGE("invalid size=%" PRIu64, (uint64_t) size);
		return NULL;
	}

	gltf_file_t* self;
	self = (gltf_file_t*) CALLOC(1, sizeof(gltf_file_t));
	if(self == NULL)
	{
		LOGE("CALLOC failed");
		return NULL;
	}

	self->mode   = mode;
	self->length = size;

	if(mode == GLTF_FILEMODE_COPY)
	{
		self->data = (char*) CALLOC(1, size);
		if(self->data == NULL)
		{
			goto fail_data;
		}
		memcpy(self->data, data, size);
	}
	else
	{
		self->data = data;
	}

	self->scenes = cc_list_new();
	if(self->scenes == NULL)
	{
		goto fail_scenes;
	}

	self->nodes = cc_list_new();
	if(self->nodes == NULL)
	{
		goto fail_nodes;
	}

	self->cameras = cc_list_new();
	if(self->cameras == NULL)
	{
		goto fail_cameras;
	}

	self->meshes = cc_list_new();
	if(self->meshes == NULL)
	{
		goto fail_meshes;
	}

	self->materials = cc_list_new();
	if(self->materials == NULL)
	{
		goto fail_materials;
	}

	self->accessors = cc_list_new();
	if(self->accessors == NULL)
	{
		goto fail_accessors;
	}

	self->textures = cc_list_new();
	if(self->textures == NULL)
	{
		goto fail_textures;
	}

	self->bufferViews = cc_list_new();
	if(self->bufferViews == NULL)
	{
		goto fail_bufferViews;
	}

	self->images = cc_list_new();
	if(self->images == NULL)
	{
		goto fail_images;
	}

	self->buffers = cc_list_new();
	if(self->buffers == NULL)
	{
		goto fail_buffers;
	}

	// parse header
	if(gltf_file_parseHeader(self) == 0)
	{
		goto fail_header;
	}

	// parse chunks
	uint32_t chunk  = 0;
	size_t   offset = sizeof(gltf_header_t);
	while(offset < self->length)
	{
		if(chunk == 0)
		{
			if(gltf_file_parseChunk(self, &offset,
			                        GLTF_CHUNK_TYPE_JSON) == 0)
			{
				goto fail_chunk;
			}
		}
		else if(chunk == 1)
		{
			if(gltf_file_parseChunk(self, &offset,
			                        GLTF_CHUNK_TYPE_BIN) == 0)
			{
				goto fail_chunk;
			}
		}
		else
		{
			LOGE("invalid chunk=%u", chunk);
			goto fail_chunk;
		}

		++chunk;
	}

	// ensure json + bin chunks exist
	if(chunk != 2)
	{
		LOGE("invalid chunk=%u", chunk);
		goto fail_chunk;
	}

	// success
	return self;

	// failure
	fail_chunk:
		gltf_file_discard(self);
	fail_header:
		cc_list_delete(&self->buffers);
	fail_buffers:
		cc_list_delete(&self->images);
	fail_images:
		cc_list_delete(&self->bufferViews);
	fail_bufferViews:
		cc_list_delete(&self->textures);
	fail_textures:
		cc_list_delete(&self->accessors);
	fail_accessors:
		cc_list_delete(&self->materials);
	fail_materials:
		cc_list_delete(&self->meshes);
	fail_meshes:
		cc_list_delete(&self->cameras);
	fail_cameras:
		cc_list_delete(&self->nodes);
	fail_nodes:
		cc_list_delete(&self->scenes);
	fail_scenes:
	{
		if(self->mode == GLTF_FILEMODE_COPY)
		{
			FREE(self->data);
		}
	}
	fail_data:
		FREE(self);
	return NULL;
}

void gltf_file_close(gltf_file_t** _self)
{
	ASSERT(_self);

	gltf_file_t* self = *_self;
	if(self)
	{
		gltf_file_discard(self);
		cc_list_delete(&self->buffers);
		cc_list_delete(&self->images);
		cc_list_delete(&self->bufferViews);
		cc_list_delete(&self->textures);
		cc_list_delete(&self->accessors);
		cc_list_delete(&self->materials);
		cc_list_delete(&self->meshes);
		cc_list_delete(&self->cameras);
		cc_list_delete(&self->nodes);
		cc_list_delete(&self->scenes);
		if((self->mode == GLTF_FILEMODE_COPY) ||
		   (self->mode == GLTF_FILEMODE_OWNED))
		{
			FREE(self->data);
		}
		FREE(self);
		*_self = NULL;
	}
}

gltf_scene_t*
gltf_file_getScene(gltf_file_t* self,
                   uint32_t idx)
{
	ASSERT(self);

	cc_listIter_t* iter;
	iter = cc_list_get(self->scenes, (int) idx);
	if(iter == NULL)
	{
		LOGE("invalid idx=%u", idx);
		return NULL;
	}

	return (gltf_scene_t*) cc_list_peekIter(iter);
}

gltf_node_t*
gltf_file_getNode(gltf_file_t* self,
                  uint32_t idx)
{
	ASSERT(self);

	cc_listIter_t* iter;
	iter = cc_list_get(self->nodes, (int) idx);
	if(iter == NULL)
	{
		LOGE("invalid idx=%u", idx);
		return NULL;
	}

	return (gltf_node_t*) cc_list_peekIter(iter);
}

gltf_camera_t*
gltf_file_getCamera(gltf_file_t* self,
                    uint32_t idx)
{
	ASSERT(self);

	cc_listIter_t* iter;
	iter = cc_list_get(self->cameras, (int) idx);
	if(iter == NULL)
	{
		LOGE("invalid idx=%u", idx);
		return NULL;
	}

	return (gltf_camera_t*) cc_list_peekIter(iter);
}

gltf_mesh_t*
gltf_file_getMesh(gltf_file_t* self,
                  uint32_t idx)
{
	ASSERT(self);

	cc_listIter_t* iter;
	iter = cc_list_get(self->meshes, (int) idx);
	if(iter == NULL)
	{
		LOGE("invalid idx=%u", idx);
		return NULL;
	}

	return (gltf_mesh_t*) cc_list_peekIter(iter);
}

gltf_material_t*
gltf_file_getMaterial(gltf_file_t* self,
                      uint32_t idx)
{
	ASSERT(self);

	cc_listIter_t* iter;
	iter = cc_list_get(self->materials, (int) idx);
	if(iter == NULL)
	{
		LOGE("invalid idx=%u", idx);
		return NULL;
	}

	return (gltf_material_t*) cc_list_peekIter(iter);
}

gltf_accessor_t*
gltf_file_getAccessor(gltf_file_t* self,
                      uint32_t idx)
{
	ASSERT(self);

	cc_listIter_t* iter;
	iter = cc_list_get(self->accessors, (int) idx);
	if(iter == NULL)
	{
		LOGE("invalid idx=%u", idx);
		return NULL;
	}

	return (gltf_accessor_t*) cc_list_peekIter(iter);
}

gltf_texture_t*
gltf_file_getTexture(gltf_file_t* self,
                     uint32_t idx)
{
	ASSERT(self);

	cc_listIter_t* iter;
	iter = cc_list_get(self->textures, (int) idx);
	if(iter == NULL)
	{
		LOGE("invalid idx=%u", idx);
		return NULL;
	}

	return (gltf_texture_t*) cc_list_peekIter(iter);
}

gltf_bufferView_t*
gltf_file_getBufferView(gltf_file_t* self,
                        uint32_t idx)
{
	ASSERT(self);

	cc_listIter_t* iter;
	iter = cc_list_get(self->bufferViews, (int) idx);
	if(iter == NULL)
	{
		LOGE("invalid idx=%u", idx);
		return NULL;
	}

	return (gltf_bufferView_t*) cc_list_peekIter(iter);
}

gltf_image_t*
gltf_file_getImage(gltf_file_t* self,
                   uint32_t idx)
{
	ASSERT(self);

	cc_listIter_t* iter;
	iter = cc_list_get(self->images, (int) idx);
	if(iter == NULL)
	{
		LOGE("invalid idx=%u", idx);
		return NULL;
	}

	return (gltf_image_t*) cc_list_peekIter(iter);
}

const char*
gltf_file_getBuffer(gltf_file_t* self,
                    gltf_bufferView_t* bufferView)
{
	ASSERT(self);
	ASSERT(bufferView);

	if(bufferView->buffer != 0)
	{
		LOGE("unsupported buffer=%u", bufferView->buffer);
		return NULL;
	}

	// compute the offset to buffer
	gltf_chunk_t* chunk;
	size_t        offset = sizeof(gltf_header_t);
	chunk   = (gltf_chunk_t*) &self->data[offset];
	offset += sizeof(gltf_chunk_t) + chunk->chunkLength;
	chunk   = (gltf_chunk_t*) &self->data[offset];
	offset += sizeof(gltf_chunk_t);
	offset += bufferView->byteOffset;

	return &self->data[offset];
}
