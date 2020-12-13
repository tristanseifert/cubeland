#include "Model.h"
#include "Mesh.h"

#include "gfx/gl/texture/Texture2D.h"

#include <Logging.h>
#include "io/Format.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <tiny_obj_loader.h>
#include <cmrc/cmrc.hpp>

#include <stdexcept>
#include <filesystem>
#include <vector>

using namespace gfx;

CMRC_DECLARE(models);

/**
 * Loads a model (obj file) from the given path in the models resource bundle.
 */
Model::Model(const std::string &path) {
    // get base directory (for texture loading later)
    std::filesystem::path p(path);
    this->modelBase = p.root_directory().string();
    Logging::debug("Base for model '{}' = '{}'", path, this->modelBase);

    // try to open the model file
    auto fs = cmrc::models::get_filesystem();
    auto model = fs.open(path);
    auto modelStr = std::string(model.begin(), model.end());

    // read the material as well if it exists
    const auto materialPath = p.replace_extension(".mtl").string();
    std::string materialStr = "";

    if(fs.exists(materialPath)) {
        auto mtlFile = fs.open(materialPath);
        materialStr = std::string(mtlFile.begin(), mtlFile.end());
    }

    Logging::debug("Loaded {} bytes model data, {} bytes material", modelStr.size(),
                   materialStr.size());

    // begin loading
    this->parseModel(modelStr, materialStr);
}

/**
 * Parses model data from the given model (obj) and material (mtl) strings.
 */
void Model::parseModel(const std::string &objStr, const std::string &mtlStr) {
    // loader config
    tinyobj::ObjReaderConfig conf;

    // load the file
    tinyobj::ObjReader reader;

    if(!reader.ParseFromString(objStr, mtlStr, conf)) {
        throw std::runtime_error(f("Failed to load model: {}", reader.Error()));
    }
    if(!reader.Warning().empty()) {
        Logging::warn("Model load warning: {}", reader.Warning());
    }

    // iterate over each shape (mesh)
    auto& attrib = reader.GetAttrib();

    Logging::debug("Attributes: have {} vertices, {} normals, {} tex coords", attrib.vertices.size(), attrib.normals.size(), attrib.texcoords.size());

    for(auto &shape : reader.GetShapes()) {
        std::vector<VertexStruct> vertices;
        std::vector<gl::GLuint> indices;
        std::vector<TextureStruct> textures;

        // loop over faces (polygons)
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            int fv = shape.mesh.num_face_vertices[f];

            // loop over vertices in the face
            for (size_t v = 0; v < fv; v++) {
                glm::vec3 pos, normal;
                glm::vec2 texCoord(0, 0);

                // get index into the vertex
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

                // these are the vertex pos, normal, and texture pos
                pos.x = attrib.vertices[3*idx.vertex_index+0];
                pos.y = attrib.vertices[3*idx.vertex_index+1];
                pos.z = attrib.vertices[3*idx.vertex_index+2];

                normal.x = attrib.normals[3*idx.normal_index+0];
                normal.y = attrib.normals[3*idx.normal_index+1];
                normal.z = attrib.normals[3*idx.normal_index+2];

                if(idx.texcoord_index != -1) {
                    texCoord.x = attrib.texcoords[2*idx.texcoord_index+0];
                    texCoord.y = attrib.texcoords[2*idx.texcoord_index+1];
                }

                vertices.push_back(VertexStruct({
                    .Position = pos,
                    .Normal = normal,
                    .TexCoords = texCoord
                }));
                indices.push_back(index_offset + v);
            }
            index_offset += fv;

            // TODO: handle textures/materials
        }

        // create a mesh
        Logging::debug("Got {} vertices, {} indices", vertices.size(), indices.size());
        this->meshes.emplace_back(vertices, indices, textures);
    }
}

/**
 * Draw all the meshes this model is composed of in turn.
 */
void Model::draw(std::shared_ptr<RenderProgram> program) {
    for(auto &mesh : this->meshes) {
        mesh.draw(program);
    }
}
