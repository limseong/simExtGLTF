#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include "simPlusPlus/Plugin.h"
#include "simPlusPlus/Handle.h"
#include "plugin.h"
#include "stubs.h"
#include "config.h"

#include <boost/format.hpp>
#include <boost/foreach.hpp>
#include <boost/algorithm/string/predicate.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
// #define TINYGLTF_NOEXCEPTION // optional. disable exception handling.
#include "external/tinygltf/tiny_gltf.h"

tinygltf::TinyGLTF gltf;

using sim::Handle;

template <typename T>
std::ostream& operator<<(std::ostream& out, const std::vector<T>& v)
{
    if(!v.empty())
    {
        out << '[';
        std::copy(v.begin(), v.end(), std::ostream_iterator<T>(out, ", "));
        out << "\b\b]";
    }
    return out;
}

template<> std::string Handle<tinygltf::Model>::tag()
{
    return PLUGIN_NAME ".model";
}

void create(SScriptCallBack *p, const char *cmd, create_in *in, create_out *out)
{
    tinygltf::Model *model = new tinygltf::Model;

    model->asset.version = "2.0";
    model->asset.generator = "CoppeliaSim glTF plugin";

    model->samplers.push_back({});
    model->samplers[0].magFilter = TINYGLTF_TEXTURE_FILTER_LINEAR;
    model->samplers[0].minFilter = TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;
    model->samplers[0].wrapS = TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT;
    model->samplers[0].wrapT = TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT;

    model->nodes.push_back({});
    model->nodes[0].name = "Root node";

    model->scenes.push_back({});
    model->scenes[0].name = "Default Scene";
    model->scenes[0].nodes = {0};

    model->defaultScene = 0;

    out->handle = Handle<tinygltf::Model>::str(model);
}

void destroy(SScriptCallBack *p, const char *cmd, destroy_in *in, destroy_out *out)
{
    tinygltf::Model *model = Handle<tinygltf::Model>::obj(in->handle);
    if(!model) return;
    delete model;
}

void loadASCII(SScriptCallBack *p, const char *cmd, loadASCII_in *in, loadASCII_out *out)
{
    tinygltf::Model *model = new tinygltf::Model;
    if(gltf.LoadASCIIFromFile(model, &out->errors, &out->warnings, in->filepath))
        out->handle = Handle<tinygltf::Model>::str(model);
    else
        delete model;
}

void loadBinary(SScriptCallBack *p, const char *cmd, loadBinary_in *in, loadBinary_out *out)
{
    tinygltf::Model *model = new tinygltf::Model;
    if(gltf.LoadBinaryFromFile(model, &out->errors, &out->warnings, in->filepath))
        out->handle = Handle<tinygltf::Model>::str(model);
    else
        delete model;
}

void saveASCII(SScriptCallBack *p, const char *cmd, saveASCII_in *in, saveASCII_out *out)
{
    tinygltf::Model *model = Handle<tinygltf::Model>::obj(in->handle);
    if(!model) return;
    out->success = gltf.WriteGltfSceneToFile(model, in->filepath, true, true, true, false);
}

void saveBinary(SScriptCallBack *p, const char *cmd, saveBinary_in *in, saveBinary_out *out)
{
    tinygltf::Model *model = Handle<tinygltf::Model>::obj(in->handle);
    if(!model) return;
    out->success = gltf.WriteGltfSceneToFile(model, in->filepath, true, true, true, true);
}

void serialize(SScriptCallBack *p, const char *cmd, serialize_in *in, serialize_out *out)
{
    tinygltf::Model *model = Handle<tinygltf::Model>::obj(in->handle);
    if(!model) return;
    std::stringstream ss;
    gltf.WriteGltfSceneToStream(model, ss, true, false);
    out->json = ss.str();
}

bool getGLTFMatrix(int handle, int relTo, std::vector<double> &m)
{
    simFloat x[12];
    if(simGetObjectMatrix(handle, relTo, &x[0]) == -1)
        return false;
    m = {
        x[0], x[4], x[8], 0,
        x[1], x[5], x[9], 0,
        x[2], x[6], x[10], 0,
        x[3], x[7], x[11], 1
    };
    return true;
}

template<typename T>
void minMax(const T *v, simInt size, simInt offset, simInt step, double &min, double &max)
{
    for(int i = offset; i < size; i += step)
    {
        if(i == offset || v[i] < min) min = v[i];
        if(i == offset || v[i] > max) max = v[i];
    }
}

template<typename T>
void releaseBuffer(const T *b)
{
    simReleaseBuffer(reinterpret_cast<const char *>(b));
}

int addBuffer(tinygltf::Model *model, void *buffer, int size, const std::string &name)
{
    int i = model->buffers.size();
    model->buffers.push_back({});
    tinygltf::Buffer &b = model->buffers[i];
    b.data.resize(size);
    b.name = name + " buffer";
    std::memcpy(b.data.data(), buffer, size);
    return i;
}

int addBufferView(tinygltf::Model *model, int buffer, int byteLength, int byteOffset, const std::string &name)
{
    int i = model->bufferViews.size();
    model->bufferViews.push_back({});
    tinygltf::BufferView &v = model->bufferViews[i];
    v.buffer = buffer;
    v.byteLength = byteLength;
    v.byteOffset = byteOffset;
    v.name = name + " buffer view";
    return i;
}

int addAccessor(tinygltf::Model *model, int bufferView, int byteOffset, int componentType, int type, int count, const std::vector<double> &minValues, const std::vector<double> &maxValues, const std::string &name)
{
    int i = model->accessors.size();
    model->accessors.push_back({});
    tinygltf::Accessor &a = model->accessors[i];
    a.bufferView = bufferView;
    a.byteOffset = byteOffset;
    a.componentType = componentType;
    a.type = type;
    a.count = count;
    a.minValues = minValues;
    a.maxValues = maxValues;
    a.name = name + " accessor";
    return i;
}

int addMesh(tinygltf::Model *model, int handle, const std::string &name)
{
    simFloat *vertices;
    simInt verticesSize;
    simInt *indices;
    simInt indicesSize;
    simFloat *normals;
    if(simGetShapeMesh(handle, &vertices, &verticesSize, &indices, &indicesSize, &normals) == -1)
        return -1;

    int bv = addBuffer(model, vertices, sizeof(simFloat) * verticesSize, name + " vertex");
    int bi = addBuffer(model, indices, sizeof(simInt) * indicesSize, name + " index");
    //int bn = addBuffer(model, normals, sizeof(simFloat) * indicesSize * 3, name + " normal");

    int vv = addBufferView(model, bv, sizeof(simFloat) * verticesSize, 0, name + " vertex");
    int vi = addBufferView(model, bi, sizeof(simInt) * indicesSize, 0, name + " index");
    //int vn = addBufferView(model, bv, sizeof(simFloat) * indicesSize * 3, 0, name + " normal");

    double vxmin, vxmax, vymin, vymax, vzmin, vzmax;
    minMax<simFloat>(vertices, verticesSize, 0, 3, vxmin, vxmax);
    minMax<simFloat>(vertices, verticesSize, 1, 3, vymin, vymax);
    minMax<simFloat>(vertices, verticesSize, 2, 3, vzmin, vzmax);
    int av = addAccessor(model, vv, 0, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, verticesSize / 3, {vxmin, vymin, vzmin}, {vxmax, vymax, vzmax}, name + " vertex");
    double imin, imax;
    minMax<simInt>(indices, indicesSize, 0, 1, imin, imax);
    int ai = addAccessor(model, vi, 0, TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, TINYGLTF_TYPE_SCALAR, indicesSize, {double(imin)}, {double(imax)}, name + " index");

    int i = model->meshes.size();
    model->meshes.push_back({});
    tinygltf::Mesh &mesh = model->meshes[i];
    mesh.name = name + " mesh";
    mesh.primitives.push_back({});
    mesh.primitives[0].attributes["POSITION"] = av;
    mesh.primitives[0].indices = ai;
    mesh.primitives[0].mode = TINYGLTF_MODE_TRIANGLES;
    return i;
}

int addBasicMaterial(tinygltf::Model *model, const std::vector<simFloat> &col1, const std::vector<simFloat> &col2, const std::string &name)
{
    model->materials.resize(model->materials.size() + 1);
    tinygltf::Material &material = model->materials[model->materials.size() - 1];
    material.name = name + " material";
    material.pbrMetallicRoughness.baseColorFactor = {col1[0], col1[1], col1[2], 1.0};
    material.pbrMetallicRoughness.metallicFactor = 0.1;
    material.pbrMetallicRoughness.roughnessFactor = 0.5;
    return model->materials.size() - 1;
}

int addTexture(tinygltf::Model *model, void *imgdata)
{
    //local i=#model.images
    //table.insert(model.images,{uri='data:application/octet-stream;base64,'..b64enc(imgdata)})
    //return i
    return 0;
}

void getObjectSelection(std::vector<simInt> &v)
{
    int selectionSize = simGetObjectSelectionSize();
    v.resize(selectionSize);
    simGetObjectSelection(v.data());
}

void getAllObjects(std::vector<simInt> &v)
{
    simInt allObjectsCount;
    simInt *allObjectsBuf = simGetObjectsInTree(sim_handle_scene, sim_handle_all, 0, &allObjectsCount);
    if(allObjectsBuf)
    {
        for(int i = 0; i < allObjectsCount; i++)
            v.push_back(allObjectsBuf[i]);
        releaseBuffer(allObjectsBuf);
    }
}

simInt getVisibleLayers()
{
    simInt v = 0;
    if(simGetInt32Parameter(sim_intparam_visible_layers, &v) == -1)
        return 0;
    else
        return v;
}

std::string getObjectName(simInt handle)
{
    simChar *name = simGetObjectName(handle);
    std::string ret;
    if(name)
    {
        ret = name;
        releaseBuffer(name);
    }
    return ret;
}

simInt getObjectLayers(simInt handle)
{
    simInt v = 0;
    if(simGetObjectInt32Parameter(handle, sim_objintparam_visibility_layer, &v) == 1)
        return v;
    else
        return 0;
}

bool isCompound(simInt handle)
{
    simInt v = 0;
    if(simGetObjectInt32Parameter(handle, sim_shapeintparam_compound, &v) == 1)
        return v != 0;
    else
        return false;
}

std::vector<simInt> ungroupShape(simInt handle)
{
    std::vector<simInt> ret;
    simInt count;
    simInt *shapes = simUngroupShape(handle, &count);
    if(shapes)
    {
        ret.resize(count);
        for(int i = 0; i < count; i++)
            ret[i] = shapes[i];
        releaseBuffer(shapes);
    }
    return ret;
}

std::vector<simInt> ungroupShapeCopy(simInt handle)
{
    simInt handles[1] = {handle};
    simCopyPasteObjects(handles, 1, 0);
    return ungroupShape(handles[0]);
}

std::vector<simFloat> getShapeColor(simInt handle, simInt colorComponent)
{
    std::vector<simFloat> ret;
    ret.resize(3);
    simGetShapeColor(handle, 0, colorComponent, ret.data());
    return ret;
}

void exportShape(SScriptCallBack *p, const char *cmd, exportShape_in *in, exportShape_out *out)
{
    tinygltf::Model *model = Handle<tinygltf::Model>::obj(in->handle);
    if(!model) return;

    simInt obj = in->shapeHandle;
    out->nodeIndex = model->nodes.size();
    model->nodes.push_back({});
    model->nodes[in->parentNodeIndex].children.push_back(out->nodeIndex);
    model->nodes[out->nodeIndex].name = getObjectName(obj);
    getGLTFMatrix(obj, in->parentHandle, model->nodes[out->nodeIndex].matrix);

    if(isCompound(obj))
    {
        for(simInt subObj : ungroupShapeCopy(obj))
        {
            exportShape_in args;
            args.handle = in->handle;
            args.shapeHandle = subObj;
            args.parentHandle = obj;
            args.parentNodeIndex = out->nodeIndex;
            exportShape_out ret;
            exportShape(p, &args, &ret);
            simRemoveObject(subObj);
        }
        return;
    }

    std::vector<simFloat> col1 = getShapeColor(obj, sim_colorcomponent_ambient_diffuse);
    std::vector<simFloat> col2 = getShapeColor(obj, sim_colorcomponent_specular);
    model->nodes[out->nodeIndex].mesh = addMesh(model, obj, model->nodes[out->nodeIndex].name);
    model->meshes[model->nodes[out->nodeIndex].mesh].primitives[0].material = addBasicMaterial(model, col1, col2, model->nodes[out->nodeIndex].name);
}

void exportObject(SScriptCallBack *p, const char *cmd, exportObject_in *in, exportObject_out *out)
{
    tinygltf::Model *model = Handle<tinygltf::Model>::obj(in->handle);
    if(!model) return;

	simInt visibleLayers = getVisibleLayers();
    simInt obj = in->objectHandle;
    simInt layers = getObjectLayers(obj);
    if(!(visibleLayers & layers)) return;

    simInt objType = simGetObjectType(obj);
    if(objType == sim_object_shape_type)
    {
        exportShape_in args;
        args.handle = in->handle;
        args.shapeHandle = obj;
        exportShape_out ret;
        exportShape(p, &args, &ret);
        out->nodeIndex = ret.nodeIndex;
    }
}

void exportAllObjects(SScriptCallBack *p, const char *cmd, exportAllObjects_in *in, exportAllObjects_out *out)
{
    tinygltf::Model *model = Handle<tinygltf::Model>::obj(in->handle);
    if(!model) return;

    exportObjects_in args;
    args.handle = in->handle;
    getAllObjects(args.objectHandles);
    if(args.objectHandles.empty()) return;
    exportObjects_out ret;
    exportObjects(p, &args, &ret);
}

void exportSelectedObjects(SScriptCallBack *p, const char *cmd, exportSelectedObjects_in *in, exportSelectedObjects_out *out)
{
    tinygltf::Model *model = Handle<tinygltf::Model>::obj(in->handle);
    if(!model) return;

    exportObjects_in args;
    args.handle = in->handle;
    getObjectSelection(args.objectHandles);
    if(args.objectHandles.empty()) return;
    exportObjects_out ret;
    exportObjects(p, &args, &ret);
}

void exportObjects(SScriptCallBack *p, const char *cmd, exportObjects_in *in, exportObjects_out *out)
{
    tinygltf::Model *model = Handle<tinygltf::Model>::obj(in->handle);
    if(!model) return;

    exportObject_in args;
    exportObject_out ret;
    for(simInt obj : in->objectHandles)
    {
        args.handle = in->handle;
        args.objectHandle = obj;
        exportObject(p, &args, &ret);
    }
}

class Plugin : public sim::Plugin
{
public:
    void onStart()
    {
        if(!registerScriptStuff())
            throw std::runtime_error("failed to register script stuff");

        simSetModuleInfo(PLUGIN_NAME, 0, "glTF support", 0);
        simSetModuleInfo(PLUGIN_NAME, 1, BUILD_DATE, 0);
    }
};

SIM_PLUGIN(PLUGIN_NAME, PLUGIN_VERSION, Plugin)
