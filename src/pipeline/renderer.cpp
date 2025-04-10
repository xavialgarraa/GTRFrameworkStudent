#include "renderer.h"

#include <algorithm> //sort

#include "camera.h"
#include "../gfx/gfx.h"
#include "../gfx/shader.h"
#include "../gfx/mesh.h"
#include "../gfx/texture.h"
#include "../gfx/fbo.h"
#include "../pipeline/prefab.h"
#include "../pipeline/light.h"

#include "../pipeline/material.h"
#include "../pipeline/animation.h"
#include "../utils/utils.h"
#include "../extra/hdre.h"
#include "../core/ui.h"

#include "scene.h"

struct sDrawCommand {
	GFX::Mesh* mesh;
	SCN::Material* material;
	Matrix44 model;
	float distance_to_camera;
};

std::vector<sDrawCommand> draw_command_list;
std::vector<SCN::LightEntity*> light_list;

using namespace SCN;

//some globals
GFX::Mesh sphere;

Renderer::Renderer(const char* shader_atlas_filename)
{
	render_wireframe = false;
	render_boundaries = false;
	scene = nullptr;
	skybox_cubemap = nullptr;

	if (!GFX::Shader::LoadAtlas(shader_atlas_filename))
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f);
	sphere.uploadToVRAM();
}

void Renderer::setupScene()
{
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;
}

// Updated parseNodes function to include frustum culling
void parseNodes(SCN::Node* node, Camera* cam) {
	if (!node || !cam) {
		return;
	}

	if (node->mesh) {
		// Get global matrix and bounding box
		Matrix44 model = node->getGlobalMatrix();
		BoundingBox world_bounding = transformBoundingBox(model, node->mesh->box);

		// Frustum culling check
		if (cam->testBoxInFrustum(world_bounding.center, world_bounding.halfsize)) {
			sDrawCommand draw_com;
			draw_com.mesh = node->mesh;
			draw_com.material = node->material;
			draw_com.model = model;

			// Calculate distance to camera for sorting
			vec3 position = model.getTranslation();
			draw_com.distance_to_camera = cam->eye.distance(position);

			draw_command_list.push_back(draw_com);
		}
	}

	for (SCN::Node* child : node->children) {
		parseNodes(child, cam);
	}
}


void Renderer::parseSceneEntities(SCN::Scene* scene, Camera* cam) {
	// HERE =====================
	// TODO: GENERATE RENDERABLES
	// ==========================

	std::vector<PrefabEntity*> renderable_ent;
	std::vector<LightEntity*> lights;

	for (int i = 0; i < scene->entities.size(); i++) {
		BaseEntity* entity = scene->entities[i];

		if (!entity->visible) {
			continue;
		}

		if (entity->getType() == eEntityType::PREFAB) {
			parseNodes(&((PrefabEntity*)entity)->root, cam);
		}
		else if (entity->getType() == eEntityType::LIGHT) {
			light_list.push_back((LightEntity*)entity);
		}

		if (entity->getType() == eEntityType::LIGHT) {
			LightEntity* light_entt = (LightEntity*)entity;
			lights.push_back(light_entt);
		}
	}
}

void Renderer::renderScene(SCN::Scene* scene, Camera* camera)
{
	this->scene = scene;
	setupScene();

	// Clear previous frame data
	draw_command_list.clear();
	light_list.clear();

	parseSceneEntities(scene, camera);

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	//render skybox
	if (skybox_cubemap)
		renderSkybox(skybox_cubemap);

	// Separate opaque and transparent objects
	std::vector<sDrawCommand> opaque_commands;
	std::vector<sDrawCommand> transparent_commands;

	for (const sDrawCommand& command : draw_command_list) {
		if (command.material && command.material->alpha_mode == SCN::eAlphaMode::BLEND) {
			transparent_commands.push_back(command);
		}
		else {
			opaque_commands.push_back(command);
		}
	}

	// Render opaque objects first
	for (const sDrawCommand& command : opaque_commands) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}

	// Enable blending for transparent objects
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Render transparent objects last
	for (const sDrawCommand& command : transparent_commands) {
		renderMeshWithMaterial(command.model, command.mesh, command.material);
	}

	// Disable blending for next frame
	glDisable(GL_BLEND);
}

void Renderer::renderSkybox(GFX::Texture* cubemap)
{
	Camera* camera = Camera::current;

	// Apply skybox necesarry config:
	// No blending, no dpeth test, we are always rendering the skybox
	// Set the culling aproppiately, since we just want the back faces
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	GFX::Shader* shader = GFX::Shader::Get("skybox");
	if (!shader)
		return;
	shader->enable();

	// Center the skybox at the camera, with a big sphere
	Matrix44 m;
	m.setTranslation(camera->eye.x, camera->eye.y, camera->eye.z);
	m.scale(10, 10, 10);
	shader->setUniform("u_model", m);

	// Upload camera uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);

	shader->setUniform("u_texture", cubemap, 0);

	sphere.render(GL_TRIANGLES);

	shader->disable();

	// Return opengl state to default
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_DEPTH_TEST);
}

// Renders a mesh given its transform and material
void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	//in case there is nothing to do
	if (!mesh || !mesh->getNumVertices() || !material)
		return;
	assert(glGetError() == GL_NO_ERROR);

	//define locals to simplify coding
	GFX::Shader* shader = NULL;
	Camera* camera = Camera::current;

	glEnable(GL_DEPTH_TEST);

	//chose a shader based on material properties
	shader = GFX::Shader::Get("phong");

	assert(glGetError() == GL_NO_ERROR);

	//no shader? then nothing to render
	if (!shader)
		return;
	shader->enable();

	material->bind(shader);
	shader->setUniform("u_shininess", 1.0f - material->roughness_factor); // Convert roughness to shininess

	//send lights
	vec3* light_pos = new vec3[light_list.size()];
	vec3* light_color = new vec3[light_list.size()];
	float* light_int = new float[light_list.size()];
	vec3* light_dir = new vec3[light_list.size()];
	int* light_type = new int[light_list.size()];
	vec2* cone_info = new vec2[light_list.size()];

	int i = 0;
	for (LightEntity* light : light_list) {
		light_pos[i] = light->root.getGlobalMatrix().getTranslation();
		light_int[i] = light->intensity;
		light_color[i] = light->color;
		light_dir[i] = light->root.model.frontVector();
		light_type[i] = light->light_type;
		cone_info[i] = light->cone_info;
		i++;
	}

	shader->setUniform("u_light_count", (int)min(light_list.size(), 10));
	shader->setUniform3Array("u_light_pos", (float*)light_pos, min(light_list.size(), 10));
	shader->setUniform3Array("u_light_color", (float*)light_color, min(light_list.size(), 10));
	shader->setUniform1Array("u_light_intensity", light_int, min(light_list.size(), 10));
	shader->setUniform1Array("u_light_type", (int*)light_type, min(light_list.size(), 10));
	shader->setUniform3Array("u_light_dir", (float*)light_dir, min(light_list.size(), 10));
	shader->setUniform2Array("u_light_cone", (float*)cone_info, min(light_list.size(), 10));

	delete[] light_pos;
	delete[] light_color;
	delete[] light_int;
	delete[] light_dir;
	delete[] cone_info;
	delete[] light_type;


	//upload uniforms
	shader->setUniform("u_model", model);
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);
	
	// Upload time, for cool shader effects
	float t = getTime();
	shader->setUniform("u_time", t);

	// Render just the verticies as a wireframe
	if (render_wireframe)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	//do the draw call that renders the mesh into the screen
	mesh->render(GL_TRIANGLES);

	//disable shader
	shader->disable();

	//set the render state as it was before to avoid problems with future renders
	glDisable(GL_BLEND);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{

	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);

	//add here your stuff
	//...
}

#else
void Renderer::showUI() {}
#endif