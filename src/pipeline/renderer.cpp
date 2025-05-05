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
std::vector<GFX::FBO*> shadow_fbos;

Camera light_camera;

using namespace SCN;

//some globals
GFX::Mesh sphere;

Renderer::Renderer(const char* shader_atlas_filename)
{
	render_wireframe = false;
	render_boundaries = false;
	scene = nullptr;
	skybox_cubemap = nullptr;

	use_multipass = true;


	if (!GFX::Shader::LoadAtlas(shader_atlas_filename))
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f);
	sphere.uploadToVRAM();
	
	// 3.1 ASSIGNMENT 3: Fuerzo la creacion de 4 shadow_fbo
	for (int i = 0; i < 4; i++)
	{
		shadow_fbo = new GFX::FBO();
		shadow_fbo->setDepthOnly(1024, 1024);
		shadow_fbo->depth_texture->filename = "Shadow map Light - " + std::to_string(i);
		shadow_fbos.push_back(shadow_fbo);
	}

	//Assigment 2.1 Generate G-Buffer
	gbuffer_fbo = new GFX::FBO();

	// Hardcode to test - luego reemplazar
	int width = 1280;
	int height = 720;

	gbuffer_fbo->create(width, height, 2, GL_RGBA, GL_UNSIGNED_BYTE, true);

	// Name the textures for debugging
	gbuffer_fbo->color_textures[0]->filename = "G-Buffer Albedo";
	gbuffer_fbo->color_textures[1]->filename = "G-Buffer Normals";
	gbuffer_fbo->depth_texture->filename = "G-Buffer Depth";

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

	renderShadowMap(scene); // 3.2.2 ASSIGNMENT 3

	//set the clear color (the background color)
	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0);

	// Clear the color and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	renderToGBuffer();
	gbuffer_fbo->color_textures[0]->toViewport();

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

	// Sort opaque commands front-to-back
	std::sort(opaque_commands.begin(), opaque_commands.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
		return a.distance_to_camera < b.distance_to_camera;
		});

	// Sort transparent commands back-to-front
	std::sort(transparent_commands.begin(), transparent_commands.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
		return a.distance_to_camera > b.distance_to_camera;
		});

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

void Renderer::renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material)
{
	if (!mesh || !mesh->getNumVertices() || !material)
		return;

	assert(glGetError() == GL_NO_ERROR);

	Camera* camera = Camera::current;
	glEnable(GL_DEPTH_TEST);

	if (use_multipass)
	{
		// 3.5 ASSIGNMENT 2
		// ambient light
		GFX::Shader* ambient_shader = GFX::Shader::Get("phong_multipass_ambient");
		if (ambient_shader)
		{
			ambient_shader->enable();

			material->bind(ambient_shader);
			ambient_shader->setUniform("u_model", model);
			ambient_shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
			ambient_shader->setUniform("u_camera_position", camera->eye);


			// Set ambient light
			ambient_shader->setUniform("u_ambient_light", scene->ambient_light);
			ambient_shader->setUniform("u_alpha_cutoff", material->alpha_cutoff);

			// Disable blending for the ambient pass
			glDisable(GL_BLEND);
			glDepthMask(GL_TRUE); // Write to depth buffer
			mesh->render(GL_TRIANGLES);

			ambient_shader->disable();
		}

		// per-light
		GFX::Shader* light_shader = GFX::Shader::Get("phong_multipass_light");
		if (light_shader && !light_list.empty())
		{
			light_shader->enable();

			material->bind(light_shader);
			light_shader->setUniform("u_model", model);
			light_shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
			light_shader->setUniform("u_camera_position", camera->eye);
			light_shader->setUniform("u_shininess", 1.0f - material->roughness_factor);
			light_shader->setUniform("u_alpha_cutoff", material->alpha_cutoff);

			// Additive blending
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);

			glDepthFunc(GL_EQUAL);
			glDepthMask(GL_FALSE);

			int index = 0;
			for (LightEntity* light : light_list)
			{
				light_shader->setUniform("u_light_pos", light->root.getGlobalMatrix().getTranslation());
				light_shader->setUniform("u_light_color", light->color);
				light_shader->setUniform("u_light_intensity", light->intensity);
				light_shader->setUniform("u_light_type", int(light->light_type));
				light_shader->setUniform("u_light_dir", light->root.model.frontVector());
				light_shader->setUniform("u_light_cone", light->cone_info);

				if (light->cast_shadows && index < shadow_fbos.size()) {
					light_shader->setUniform("u_shadow_map", shadow_fbos[index]->depth_texture, 2);
					light_shader->setUniform("u_shadow_matrix", light->view_projection);
				}
				else {
					light_shader->setUniform("u_shadow_map", GFX::Texture::getWhiteTexture(), 2);
					Matrix44 identity;
					identity.setIdentity();
					light_shader->setUniform("u_shadow_matrix", identity);
				}
				mesh->render(GL_TRIANGLES);
				index++;
			}

			// Upload time, for cool shader effects
			float t = getTime();
			light_shader->setUniform("u_time", t);

			light_shader->disable();

			// Restaurar estado
			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);
		}

		glDisable(GL_BLEND);

		if (render_wireframe)
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		else
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
	else {
		// single pass
		// chose a shader based on material properties
		GFX::Shader* shader = NULL;
		shader = GFX::Shader::Get("phong");

		assert(glGetError() == GL_NO_ERROR);

		if (!shader)
			return;
		shader->enable();

		material->bind(shader);
		shader->setUniform("u_shininess", material->shininess); // Convert roughness to shininess

		//send lights
		vec3* light_pos = new vec3[light_list.size()];
		vec3* light_color = new vec3[light_list.size()];
		float* light_int = new float[light_list.size()];
		vec3* light_dir = new vec3[light_list.size()];
		int* light_type = new int[light_list.size()];
		vec2* cone_info = new vec2[light_list.size()];
		Matrix44* shadow_mat = new Matrix44[light_list.size()];

		int i = 0;
		for (LightEntity* light : light_list) {
			light_pos[i] = light->root.getGlobalMatrix().getTranslation();
			light_int[i] = light->intensity;
			light_color[i] = light->color;
			light_dir[i] = light->root.model.frontVector();
			light_type[i] = light->light_type;
			cone_info[i] = light->cone_info;
			shadow_mat[i] = light->view_projection;
			i++;
		}

		shader->setUniform("u_numShadows", (int)min(light_list.size(), 10));
		shader->setUniform("u_bias", shadow_bias);
		shader->setUniform("u_light_count", (int)min(light_list.size(), 10));
		shader->setUniform3Array("u_light_pos", (float*)light_pos, min(light_list.size(), 10));
		shader->setUniform3Array("u_light_color", (float*)light_color, min(light_list.size(), 10));
		shader->setUniform1Array("u_light_intensity", light_int, min(light_list.size(), 10));
		shader->setUniform1Array("u_light_type", (int*)light_type, min(light_list.size(), 10));
		shader->setUniform3Array("u_light_dir", (float*)light_dir, min(light_list.size(), 10));
		shader->setUniform2Array("u_light_cone", (float*)cone_info, min(light_list.size(), 10));
		shader->setUniform("u_ambient_light", scene->ambient_light);

		// We uploaded all the shadow maps manual
		shader->setUniform("u_shadow_map_0", (shadow_fbos[0]->depth_texture), 2); //SPOT
		//shader->setUniform("u_shadow_map_1", (shadow_fbos[1]->depth_texture), 3);
		//shader->setUniform("u_shadow_map_2", (shadow_fbos[2]->depth_texture), 4);
		shader->setUniform("u_shadow_map_3", (shadow_fbos[3]->depth_texture), 5); //DIRECTIONAL

		shader->setUniform("u_shadow_matrix_0", shadow_mat[0]); //SPOT
		//shader->setUniform("u_shadow_matrix_1", shadow_mat[1]);
		//shader->setUniform("u_shadow_matrix_2", shadow_mat[2]);
		shader->setUniform("u_shadow_matrix_3", shadow_mat[3]); //DIRECTIONAL

		delete[] light_pos;
		delete[] light_color;
		delete[] light_int;
		delete[] light_dir;
		delete[] cone_info;
		delete[] light_type;
		delete[] shadow_mat;

		//upload uniforms
		shader->setUniform("u_model", model);
		shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
		shader->setUniform("u_camera_position", camera->eye);

		Matrix44 bias_m;
		bias_m.setIdentity();
		bias_m.scale(0.5, 0.5, 0.5);
		bias_m.translate(1.0, 1.0, 1.0);

		shader->setUniform("u_shadow_maps", shadow_fbo->depth_texture, 2);

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
}


// 3.2.1 ASSIGNMENT 3
void Renderer::setupLight(SCN::LightEntity* light)
{
	mat4 light_model = light->root.getGlobalMatrix();
	vec3 light_pos = light_model.getTranslation();

	if (light->light_type == eLightType::SPOT) {
		float fov = light->cone_info.y * 2.f;
		light_camera.setPerspective(fov, 1.0f, light->near_distance, light->max_distance);
	}
	else if (light->light_type == eLightType::DIRECTIONAL) {
		float size = light->area / 2.f;
		light_camera.setOrthographic(-size, size, -size, size, light->near_distance, light->max_distance);
	}
	
	light_camera.lookAt(light_pos, light_model * vec3(0.f, 0.f, -1.f), vec3(0.0f, 1.0f, 0.0f));
}


// 3.2.2 ASSIGNMENT 3
void Renderer::renderShadowMap(SCN::Scene* scene)
{
	
	for (int i = 0; i < light_list.size(); i++)
	{
		SCN::LightEntity* light = light_list[i];
		if (!light->cast_shadows) continue;

		// Setup light camera
		setupLight(light);
		light->view_projection = light_camera.viewprojection_matrix;
	}
	
	for (int i = 0; i < shadow_fbos.size(); i++)
	{
		if (!light_list[i]->cast_shadows)
		{
			continue;
		}
		// Bind and render
		shadow_fbos[i]->bind();
		glViewport(0, 0, 1024, 1024);
		glClear(GL_DEPTH_BUFFER_BIT);
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);

		if (front_face_culling) {
			glEnable(GL_CULL_FACE);
			glFrontFace(GL_CW);
		}
		else {
			glDisable(GL_CULL_FACE);
		}

		for (const sDrawCommand& command : draw_command_list)
		{
			if (command.material->alpha_mode == eAlphaMode::BLEND)
				continue;

			GFX::Shader* plain_shader = GFX::Shader::Get("plain");
			plain_shader->enable();
			plain_shader->setUniform("u_model", command.model);
			plain_shader->setUniform("u_viewprojection", light_list[i]->view_projection);

			int useMask = (command.material->alpha_mode == SCN::MASK &&
				command.material->textures[SCN::OPACITY].texture) ? 1 : 0;

			plain_shader->setUniform("u_mask", useMask);
			plain_shader->setUniform("u_alpha_cutoff", command.material->alpha_cutoff);
			if (useMask)
				plain_shader->setUniform("u_op_map", command.material->textures[SCN::OPACITY].texture, 0);

			command.mesh->render(GL_TRIANGLES);
			plain_shader->disable();
		}

		glFrontFace(GL_CCW);
		glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
		shadow_fbos[i]->unbind();
	}
	
}

void Renderer::renderToGBuffer()
{
	// Bind G-Buffer FBO
	
	gbuffer_fbo->bind();

	// Clear all buffers
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Get GBuffer fill shader
	GFX::Shader* shader = GFX::Shader::Get("gbuffer_fill");
	
	shader->enable();

	// Render all opaque objects
	for (const sDrawCommand& command : draw_command_list)
	{
		if (command.material && command.material->alpha_mode == SCN::eAlphaMode::BLEND)
			continue; // Skip transparent objects

		// Set model matrix
		shader->setUniform("u_model", command.model);
		shader->setUniform("u_viewprojection", Camera::current->viewprojection_matrix);

		// Bind material properties
		command.material->bind(shader);

		// Render mesh
		command.mesh->render(GL_TRIANGLES);
	}

	shader->disable();
	gbuffer_fbo->unbind();
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);
	ImGui::Checkbox("Multipass Rendering", &use_multipass);
	ImGui::SliderFloat("Shadow Bias", &shadow_bias, 0.0f, 0.01f);
	ImGui::Checkbox("Front Face Culling", &front_face_culling);

	ImGui::Checkbox("Deferred Rendering", &use_deferred);

	// Solo usamos use_deferred
	if (use_deferred && use_multipass)
	{
		use_multipass = false;
	}
}

#else
void Renderer::showUI() {}
#endif