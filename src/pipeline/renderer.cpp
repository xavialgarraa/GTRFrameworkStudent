﻿#include "renderer.h"

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
#include "../core/core.h"

#include "scene.h"

struct sDrawCommand {
	GFX::Mesh* mesh;
	SCN::Material* material;
	Matrix44 model;
    std::string name;
	float distance_to_camera;
	SCN::BaseEntity* entity = nullptr;
	SCN::Node* node = nullptr;

};

std::vector<sDrawCommand> draw_command_list;
std::vector<SCN::LightEntity*> light_list;
std::vector<GFX::FBO*> shadow_fbos;

GFX::FBO* motion_blur_fbo;
GFX::FBO* velocity_fbo;
GFX::FBO* tonemap_fbo;
Matrix44 prev_view_projection;
Matrix44 current_view_projection;


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

	scene_blur_object = false;
	frecuencia = 0.5f;
	amplitud = -0.5f;

	use_object_motion_blur = false;
	motion_blur_strength = 2.f;
	motion_blur_samples = 4;
	use_motion_blur = false;

	if (!GFX::Shader::LoadAtlas(shader_atlas_filename))
		exit(1);
	GFX::checkGLErrors();

	sphere.createSphere(1.0f);
	sphere.uploadToVRAM();

	// 3.1 ASSIGNMENT 3: Fuerzo la creacion de 4 shadow_fbo
	for (int i = 0; i < 4; i++)
	{
		shadow_fbo = new GFX::FBO();
		shadow_fbo->setDepthOnly(1028, 1028);
		shadow_fbo->depth_texture->filename = "Shadow map Light - " + std::to_string(i);
		shadow_fbos.push_back(shadow_fbo);
	}

	//Assigment 2.1 Generate G-Buffer
	gbuffer_fbo = new GFX::FBO();

	Vector2ui size = CORE::getWindowSize();
	int width = size.x;
	int height = size.y;

	gbuffer_fbo->create(width, height, 3, GL_RGBA, GL_UNSIGNED_BYTE, true);

	gbuffer_fbo->color_textures[0]->filename = "G-Buffer Albedo";
	gbuffer_fbo->color_textures[1]->filename = "G-Buffer Normals";
	gbuffer_fbo->color_textures[2]->filename = "G-Buffer Screen Positions";

	gbuffer_fbo->depth_texture->filename = "G-Buffer Depth";

	lighting_fbo = new GFX::FBO();
	lighting_fbo->create(width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, true);
	lighting_fbo->color_textures[0]->filename = "Lighting Result";
	lighting_fbo->depth_texture->filename = "Lighting Depth";

	// 2.1 assignment 5
	ssao_fbo = new GFX::FBO();
	ssao_fbo->create(width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, false);
	ssao_fbo->color_textures[0]->filename = "SSAO Texture";

	// 3.1 assignment 6
	hdr_fbo = new GFX::FBO();
	hdr_fbo->create(width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, false);
	hdr_fbo->color_textures[0]->filename = "HDR Result";

	tonemap_fbo = new GFX::FBO();
	tonemap_fbo->create(width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, false);
	tonemap_fbo->color_textures[0]->filename = "Tonemap Result";

	//Lab3 - Motion Blur
	motion_blur_fbo = new GFX::FBO();
	motion_blur_fbo->create(width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, false);
	motion_blur_fbo->color_textures[0]->filename = "Motion Blur Result";

	velocity_fbo = new GFX::FBO();
	velocity_fbo->create(width, height, 1, GL_RG, GL_FLOAT, true); // RG para X,Y velocity
	velocity_fbo->color_textures[0]->filename = "Velocity Buffer";
}

void Renderer::setupScene()
{
	if (scene->skybox_filename.size())
		skybox_cubemap = GFX::Texture::Get(std::string(scene->base_folder + "/" + scene->skybox_filename).c_str());
	else
		skybox_cubemap = nullptr;
}

void Renderer::parseNodes(SCN::Node* node, Camera* cam, BaseEntity* entity) {
	if (!node || !cam) {
		return;
	}

	if (node->mesh) {
		// Get global matrix and bounding box
		Matrix44 model = node->getGlobalMatrix();
		BoundingBox world_bounding = transformBoundingBox(model, node->mesh->box);
		
		// Inicializa motion_data para todos los nodos con mesh (es lo que se renderiza)
		if (node->mesh) {
			if (motion_data.count(node) == 0) {
				motion_data[node].prev_model = node->getGlobalMatrix();
			}
		}

		// Frustum culling check
		if (cam->testBoxInFrustum(world_bounding.center, world_bounding.halfsize)) {
			sDrawCommand draw_com;
			draw_com.mesh = node->mesh;
			draw_com.material = node->material;
			draw_com.model = model;
			draw_com.name = node->name;
			draw_com.node = node;
			draw_com.entity = entity;  // Debes pasarlo desde parseSceneEntities()

			// Calculate distance to camera for sorting
			vec3 position = model.getTranslation();
			draw_com.distance_to_camera = cam->eye.distance(position);

			draw_command_list.push_back(draw_com);
		}
	}

	for (SCN::Node* child : node->children) {
		parseNodes(child, cam, entity);
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
			parseNodes(&((PrefabEntity*)entity)->root, cam, entity);
		}
		else if (entity->getType() == eEntityType::LIGHT) {
			light_list.push_back((LightEntity*)entity);
		}
		
		if (entity->name == "car1")
		{
			car1 = ((Node*)&entity->root);
			
		}

		if (entity->name == "car2")
		{
			car2 = ((Node*)&entity->root);
			car2->model._41 = -2.3f;
			motion_data[car2];

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

	GFX::Shader* quad_texture = GFX::Shader::Get("quad_texture");

	if (!has_prev_view_projection)
	{
		prev_view_projection = Camera::current->viewprojection_matrix;
		has_prev_view_projection = true;
	}

	current_view_projection = Camera::current->viewprojection_matrix;

	for (auto& pair : motion_data)
	{
		SCN::Node* node = pair.first;
		MotionBlurData& data = pair.second;

		data.prev_model = data.current_model;
		data.current_model = node->getGlobalMatrix();
	}

	if (scene_blur_object)
	{
		float dt = CORE::getTime() / 1000.f;
		update(dt);
	}

	glClearColor(scene->background_color.x, scene->background_color.y, scene->background_color.z, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GFX::checkGLErrors();

	renderToGBuffer();

	// Mostrar G-buffer si no hay más pasos (debug)
	// gbuffer_fbo->color_textures[0]->toViewport();
	renderFBOToScreen(gbuffer_fbo, quad_texture);

	renderMotionVectors();
	renderFBOToScreen(velocity_fbo, quad_texture);

	// Render skybox
	if (skybox_cubemap)
		renderSkybox(skybox_cubemap);

	if (use_deferred)
	{
		if (light_volume)
		{
			copyDepthBuffer(gbuffer_fbo, lighting_fbo);

			lighting_fbo->bind();
			glClear(GL_COLOR_BUFFER_BIT);
			glDisable(GL_BLEND);
			glDepthMask(GL_FALSE);
			glDepthFunc(GL_LEQUAL);

			if (skybox_cubemap)
				renderSkybox(skybox_cubemap);

			renderDeferredAmbientPass();
			renderDirectionalLights();
			renderLightVolumes(camera);
			lighting_fbo->unbind();

			renderFBOToScreen(lighting_fbo, quad_texture);

			if (use_motion_blur)
			{
				motion_blur_fbo->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDisable(GL_BLEND);
				glDepthMask(GL_FALSE);
				glDepthFunc(GL_LEQUAL);
				lighting_fbo->color_textures[0]->toViewport();

				applyMotionBlur();

				motion_blur_fbo->unbind();
				renderFBOToScreen(motion_blur_fbo, quad_texture);
			}
		}
		else if (use_ssao)
		{
			copyDepthBuffer(gbuffer_fbo, ssao_fbo);

			if (ssao_kernel_size != last_ssao_kernel_size ||
				ssao_radius != last_ssao_radius ||
				use_ssao_plus != last_ssao_plus)
			{
				generateSpherePoints(ssao_kernel_size, ssao_radius, use_ssao_plus);
				last_ssao_kernel_size = ssao_kernel_size;
				last_ssao_radius = ssao_radius;
				last_ssao_plus = use_ssao_plus;
			}

			ssao_fbo->bind();
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDisable(GL_BLEND);
			glDepthMask(GL_FALSE);
			glDepthFunc(GL_LEQUAL);

			if (skybox_cubemap)
				renderSkybox(skybox_cubemap);

			renderSSAO(Camera::current);
			ssao_fbo->unbind();

			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LESS);

			renderFBOToScreen(ssao_fbo, quad_texture);

			if (ssao_plus_deferred)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				glDepthMask(GL_TRUE);
				glDepthFunc(GL_LESS);
				glDisable(GL_BLEND);

				if (skybox_cubemap)
					renderSkybox(skybox_cubemap);

				renderDeferredSinglePass();
			}
		}
		else if (use_hdr)
		{
			copyDepthBuffer(gbuffer_fbo, hdr_fbo);

			hdr_fbo->bind();
			glClearColor(0, 0, 0, 1);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LESS);
			glDisable(GL_BLEND);

			renderDeferredSinglePass();
			hdr_fbo->unbind();

			copyDepthBuffer(hdr_fbo, tonemap_fbo);

			tonemap_fbo->bind();
			glClear(GL_COLOR_BUFFER_BIT);
			glDisable(GL_BLEND);
			glDepthMask(GL_FALSE);
			glDepthFunc(GL_LEQUAL);

			hdr_fbo->color_textures[0]->toViewport();

			renderToTonemap();
			tonemap_fbo->unbind();

			renderFBOToScreen(tonemap_fbo, quad_texture);

			if (use_motion_blur)
			{
				motion_blur_fbo->bind();
				glClear(GL_COLOR_BUFFER_BIT);
				glDisable(GL_BLEND);
				glDepthMask(GL_FALSE);
				glDepthFunc(GL_LEQUAL);
				tonemap_fbo->color_textures[0]->toViewport();

				applyMotionBlur();

				motion_blur_fbo->unbind();
				renderFBOToScreen(motion_blur_fbo, quad_texture);
			}
		}
		else
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, gbuffer_fbo->width, gbuffer_fbo->height);  

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LESS);

			glDisable(GL_BLEND);
			glEnable(GL_CULL_FACE);
			glCullFace(GL_BACK);

			if (skybox_cubemap)
				renderSkybox(skybox_cubemap);

			renderDeferredSinglePass();


		}

		// Blending ON para objetos transparentes
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		std::vector<sDrawCommand> transparent_commands;
		std::sort(transparent_commands.begin(), transparent_commands.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
			return a.distance_to_camera > b.distance_to_camera;
			});

		for (const sDrawCommand& command : transparent_commands)
			renderMeshWithMaterial(command.model, command.mesh, command.material);

		glDisable(GL_BLEND);
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);
		glDisable(GL_BLEND);

		std::vector<sDrawCommand> opaque_commands;
		std::vector<sDrawCommand> transparent_commands;

		for (const sDrawCommand& command : draw_command_list)
		{
			if (command.material && command.material->alpha_mode == SCN::eAlphaMode::BLEND)
				transparent_commands.push_back(command);
			else
				opaque_commands.push_back(command);
		}

		std::sort(opaque_commands.begin(), opaque_commands.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
			return a.distance_to_camera < b.distance_to_camera;
			});
		std::sort(transparent_commands.begin(), transparent_commands.end(), [](const sDrawCommand& a, const sDrawCommand& b) {
			return a.distance_to_camera > b.distance_to_camera;
			});

		for (const sDrawCommand& command : opaque_commands)
			renderMeshWithMaterial(command.model, command.mesh, command.material);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		for (const sDrawCommand& command : transparent_commands)
			renderMeshWithMaterial(command.model, command.mesh, command.material);

		glDisable(GL_BLEND);
	}

	prev_view_projection = current_view_projection;
}


void Renderer::renderFBOToScreen(GFX::FBO* fbo, GFX::Shader* shader)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
	glDisable(GL_BLEND);

	// Render quad con el shader
	GFX::Mesh* quad = GFX::Mesh::getQuad();

	shader->enable();
	shader->setTexture("u_texture", fbo->color_textures[0], 0);
	quad->render(GL_TRIANGLES);
	shader->disable();

	// Restaurar estados para render normal
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LESS);
}


void Renderer::update(float dt) {
	//Car1
	Matrix44 model1;
	model1 = car1->model;
	/*float z1 = model1._43 * sinf(2.0f * M_PI * 0.5f * dt);
	model1._43 = z1;
	car1->model = model1;*/

	Matrix44 model2;
	model2 = car2->model;
	float z1 = amplitud + 2.5f * sinf(2.0f * M_PI * frecuencia * dt);
	car2->model._43 = z1;

}

void Renderer::renderSkybox(GFX::Texture* cubemap)
{
	Camera* camera = Camera::current;

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
		// 1. Ambient Pass
		GFX::Shader* ambient_shader = GFX::Shader::Get("phong_multipass_ambient");
		if (ambient_shader)
		{
			ambient_shader->enable();

			material->bind(ambient_shader);
			ambient_shader->setUniform("u_model", model);
			ambient_shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
			ambient_shader->setUniform("u_camera_position", camera->eye);
			ambient_shader->setUniform("u_ambient_light", scene->ambient_light);
			ambient_shader->setUniform("u_alpha_cutoff", material->alpha_cutoff);

			if (material->alpha_mode == SCN::eAlphaMode::BLEND) {
				glDepthMask(GL_FALSE);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}
			else {
				glDepthMask(GL_TRUE);
				glDisable(GL_BLEND);
			}

			mesh->render(GL_TRIANGLES);
			ambient_shader->disable();
		}

		// 2. Light Pass
		if (material->alpha_mode != SCN::eAlphaMode::BLEND) {
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

				for (LightEntity* light : light_list)
				{
					light_shader->setUniform("u_light_pos", light->root.getGlobalMatrix().getTranslation());
					light_shader->setUniform("u_light_color", light->color);
					light_shader->setUniform("u_light_intensity", light->intensity);
					light_shader->setUniform("u_light_type", int(light->light_type));
					light_shader->setUniform("u_light_dir", light->root.model.frontVector());
					light_shader->setUniform("u_light_cone", light->cone_info);

					mesh->render(GL_TRIANGLES);
				}

				light_shader->disable();

				glDepthFunc(GL_LESS);
				glDepthMask(GL_TRUE);
				glDisable(GL_BLEND);
			}
		}

		glPolygonMode(GL_FRONT_AND_BACK, render_wireframe ? GL_LINE : GL_FILL);
	}
	else {

		// Single Pass:
	//chose a shader based on material properties
		GFX::Shader* shader = NULL;
		shader = GFX::Shader::Get("phong");

		assert(glGetError() == GL_NO_ERROR);

		//no shader? then nothing to render
		if (!shader)
			return;
		shader->enable();



		material->bind(shader);
		//shader->setUniform("u_shininess", 1.0f - material->roughness_factor); // Convert roughness to shininess

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

void Renderer::renderShadowMap(SCN::Scene* scene)
{
	for (int i = 0; i < min(light_list.size(), shadow_fbos.size()); ++i)
	{
		SCN::LightEntity* light = light_list[i];
		if (!light->cast_shadows) continue;

		// Configura la camara de la luz
		setupLight(light);
		light->view_projection = light_camera.viewprojection_matrix;

		// Prepara el FBO para solo profundidad
		shadow_fbos[i]->bind();
		glViewport(0, 0, 1024, 1024);
		glClear(GL_DEPTH_BUFFER_BIT);

		// Desactiva color writes
		glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		// Configura profundidad y culling
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(GL_TRUE);

		if (front_face_culling)
		{
			glEnable(GL_CULL_FACE);
			glFrontFace(GL_CW); // culling reverso para evitar shadow acne
		}
		else
		{
			glDisable(GL_CULL_FACE);
		}

		// Dibujar cada comando sin blending
		for (const sDrawCommand& command : draw_command_list)
		{
			if (command.material->alpha_mode == eAlphaMode::BLEND)
				continue; // no sombras para objetos transparentes

			GFX::Shader* plain_shader = GFX::Shader::Get("plain");
			plain_shader->enable();
			plain_shader->setUniform("u_model", command.model);
			plain_shader->setUniform("u_viewprojection", light->view_projection);

			// Soporte para alpha masking
			bool useMask = (command.material->alpha_mode == SCN::MASK &&
				command.material->textures[SCN::OPACITY].texture);

			plain_shader->setUniform("u_mask", (int)useMask);
			plain_shader->setUniform("u_alpha_cutoff", command.material->alpha_cutoff);

			if (useMask)
				plain_shader->setUniform("u_op_map", command.material->textures[SCN::OPACITY].texture, 0);

			command.mesh->render(GL_TRIANGLES);
			plain_shader->disable();
		}

		// Restaurar estado de OpenGL
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

void Renderer::renderMotionVectors() {
	velocity_fbo->bind();
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	GFX::Shader* velocity_shader = GFX::Shader::Get("velocity");
	if (!velocity_shader) return;

	velocity_shader->enable();

	// Pasar matrices de cámara
	velocity_shader->setUniform("u_view_projection", current_view_projection);
	velocity_shader->setUniform("u_prev_view_projection", prev_view_projection);

	// Renderizar objetos con vectores de velocidad
	for (const sDrawCommand& command : draw_command_list) {
		if (command.material && command.material->alpha_mode == SCN::eAlphaMode::BLEND)
			continue; // Skip transparent objects

		Matrix44 model = command.model;
		Matrix44 current_mvp = current_view_projection * model;

		SCN::Node* node = command.node;
		Matrix44 prev_model = model; // default fallback

		if (use_object_motion_blur && node && motion_data.count(node)) {
			prev_model = motion_data[node].prev_model;
		}

		Matrix44 prev_mvp = prev_view_projection * prev_model;

		velocity_shader->setUniform("u_model", model);
		velocity_shader->setUniform("u_prev_model", prev_model);
		velocity_shader->setUniform("u_current_mvp", current_mvp);
		velocity_shader->setUniform("u_prev_mvp", prev_mvp);

		command.material->bind(velocity_shader);
		command.mesh->render(GL_TRIANGLES);
	}

	velocity_shader->disable();
	velocity_fbo->unbind();
}

void Renderer::applyMotionBlur() {
	if (!use_motion_blur) return;

	//motion_blur_fbo->bind();
	glClear(GL_COLOR_BUFFER_BIT);

	GFX::Shader* motion_blur_shader = GFX::Shader::Get("motion_blur");
	if (!motion_blur_shader) return;

	motion_blur_shader->enable();

	// Bind textures
	motion_blur_shader->setTexture("u_color_texture",
		use_hdr ? tonemap_fbo->color_textures[0] : lighting_fbo->color_textures[0], 0);
	motion_blur_shader->setTexture("u_velocity_texture", velocity_fbo->color_textures[0], 1);
	motion_blur_shader->setTexture("u_depth_texture", gbuffer_fbo->depth_texture, 2);

	// Uniforms
	motion_blur_shader->setUniform("u_motion_blur_strength", motion_blur_strength);
	motion_blur_shader->setUniform("u_motion_blur_samples", motion_blur_samples);
	motion_blur_shader->setUniform("u_use_object_motion_blur", use_object_motion_blur);

	Vector2ui size = CORE::getWindowSize();
	motion_blur_shader->setUniform("u_texel_size",
		Vector2f(1.0f / size.x, 1.0f / size.y));

	// Render fullscreen quad
	GFX::Mesh::getQuad()->render(GL_TRIANGLES);

	motion_blur_shader->disable();
	//motion_blur_fbo->unbind();
}

void Renderer::renderDeferredSinglePass()
{
	Camera* camera = Camera::current;
	int texture_slots = 0;

	GFX::Mesh* quad = GFX::Mesh::getQuad();

	GFX::Shader* shader = NULL;
	shader = GFX::Shader::Get("phong_deferred");

	assert(glGetError() == GL_NO_ERROR);

	if (!shader)
		return;

	shader->enable();

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
	shader->setUniform("u_shadow_map_0", (shadow_fbos[0]->depth_texture), texture_slots++); //SPOT
	//shader->setUniform("u_shadow_map_1", (shadow_fbos[1]->depth_texture), 3);
	//shader->setUniform("u_shadow_map_2", (shadow_fbos[2]->depth_texture), 4);
	shader->setUniform("u_shadow_map_3", (shadow_fbos[3]->depth_texture), texture_slots++); //DIRECTIONAL

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

	// Bind SSAO texture if enabled
	if (ssao_plus_deferred)
	{
		shader->setTexture("u_ssao_map", ssao_fbo->color_textures[0], texture_slots++);
	}
	else
	{
		// Bind a white 1x1 texture if SSAO is disabled, to avoid black screen
		static GFX::Texture* white_texture = GFX::Texture::getWhiteTexture();
		shader->setTexture("u_ssao_map", white_texture, texture_slots++);
	}
	//upload uniforms
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);



	// Upload time, for cool shader effects
	float t = getTime();
	shader->setUniform("u_time", t);

	// Bind the GBuffers
	shader->setTexture("u_gbuffer_color", gbuffer_fbo->color_textures[0], texture_slots++);
	shader->setTexture("u_gbuffer_normal", gbuffer_fbo->color_textures[1], texture_slots++);
	shader->setTexture("u_gbuffer_depth", gbuffer_fbo->depth_texture, texture_slots++);

	Matrix44 inv_vp = Camera::current->viewprojection_matrix;
	inv_vp.inverse();
	shader->setUniform("u_inverse_viewprojection", inv_vp);
	shader->setUniform("u_res_inv", Vector2f(1.0f / gbuffer_fbo->width, 1.0f / gbuffer_fbo->height));

	if (render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	quad->render(GL_TRIANGLES);

	shader->disable();
}

void Renderer::renderDirectionalLights()
{
	Camera* camera = Camera::current;
	int texture_slots = 0;


	GFX::Mesh* quad = GFX::Mesh::getQuad();

	GFX::Shader* shader = NULL;
	shader = GFX::Shader::Get("phong_deferred");

	assert(glGetError() == GL_NO_ERROR);

	if (!shader)
		return;

	shader->enable();

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
		if (light->light_type == 3)
		{
			light_pos[i] = light->root.getGlobalMatrix().getTranslation();
			light_int[i] = light->intensity;
			light_color[i] = light->color;
			light_dir[i] = light->root.model.frontVector();
			light_type[i] = light->light_type;
			cone_info[i] = light->cone_info;
			shadow_mat[i] = light->view_projection;
		}

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
	shader->setUniform("u_shadow_map_0", (shadow_fbos[0]->depth_texture), texture_slots++); //SPOT
	//shader->setUniform("u_shadow_map_1", (shadow_fbos[1]->depth_texture), 3);
	//shader->setUniform("u_shadow_map_2", (shadow_fbos[2]->depth_texture), 4);
	shader->setUniform("u_shadow_map_3", (shadow_fbos[3]->depth_texture), texture_slots++); //DIRECTIONAL

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
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_position", camera->eye);



	// Upload time, for cool shader effects
	float t = getTime();
	shader->setUniform("u_time", t);

	// Bind the GBuffers
	shader->setTexture("u_gbuffer_color", gbuffer_fbo->color_textures[0], texture_slots++);
	shader->setTexture("u_gbuffer_normal", gbuffer_fbo->color_textures[1], texture_slots++);
	shader->setTexture("u_gbuffer_depth", gbuffer_fbo->depth_texture, texture_slots++);

	Matrix44 inv_vp = Camera::current->viewprojection_matrix;
	inv_vp.inverse();
	shader->setUniform("u_inverse_viewprojection", inv_vp);
	shader->setUniform("u_res_inv", Vector2f(1.0f / gbuffer_fbo->width, 1.0f / gbuffer_fbo->height));

	if (render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	quad->render(GL_TRIANGLES);

	shader->disable();
}

void Renderer::renderLightVolumes(Camera* camera)
{
	if (!light_list.empty())
	{
		GFX::Shader* light_volume_shader = GFX::Shader::Get("light_volume");
		if (!light_volume_shader)
			return;

		light_volume_shader->enable();

		// Bind GBuffer textures
		light_volume_shader->setTexture("u_gbuffer_color", gbuffer_fbo->color_textures[0], 0);
		light_volume_shader->setTexture("u_gbuffer_normal", gbuffer_fbo->color_textures[1], 1);
		light_volume_shader->setTexture("u_gbuffer_depth", gbuffer_fbo->depth_texture, 2);

		// Camera and inverse matrices
		light_volume_shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
		light_volume_shader->setUniform("u_camera_position", camera->eye);
		Matrix44 inv_view_projection_matrix = camera->inverse_viewprojection_matrix;
		light_volume_shader->setUniform("u_inverse_viewprojection", inv_view_projection_matrix);
		light_volume_shader->setUniform("u_res_inv", vec2(1.0f / gbuffer_fbo->width, 1.0f / gbuffer_fbo->height));

		setLightVolumeRenderState();


		// Render each light volume
		for (LightEntity* light : light_list)
		{
			if (light->light_type == eLightType::DIRECTIONAL)
				continue;

			Matrix44 model;
			Vector3f translation = light->root.getGlobalMatrix().getTranslation();
			model.setTranslation(translation.x, translation.y, translation.z);
			model.scale(light->max_distance, light->max_distance, light->max_distance);

			light_volume_shader->setUniform("u_model", model);
			light_volume_shader->setUniform("u_light_pos", translation); // Usar posici�n directa
			light_volume_shader->setUniform("u_light_color", light->color);
			light_volume_shader->setUniform("u_light_intensity", light->intensity);
			light_volume_shader->setUniform("u_light_type", (int)light->light_type);

			if (light->light_type == eLightType::SPOT)
			{
				light_volume_shader->setUniform("u_light_dir", light->root.model.frontVector());
				light_volume_shader->setUniform("u_light_cone", vec2(cos(light->cone_info.x), cos(light->cone_info.y)));
			}

			sphere.render(GL_TRIANGLES);
		}

		// Restore state
		restoreDefaultRenderState();

		light_volume_shader->disable();
	}
}

void Renderer::renderDeferredAmbientPass() {
	GFX::Mesh* quad2 = GFX::Mesh::getQuad();

	GFX::Shader* ambient_shader = GFX::Shader::Get("deferred_ambient");
	if (!ambient_shader) return;

	ambient_shader->enable();

	// Bind GBuffer textures
	ambient_shader->setTexture("u_gbuffer_color", gbuffer_fbo->color_textures[0], 0);
	ambient_shader->setTexture("u_gbuffer_normal", gbuffer_fbo->color_textures[1], 1);
	ambient_shader->setTexture("u_gbuffer_depth", gbuffer_fbo->depth_texture, 2);

	// Set uniforms
	ambient_shader->setUniform("u_ambient_light", scene->ambient_light);
	ambient_shader->setUniform("u_viewprojection", Camera::current->viewprojection_matrix);
	ambient_shader->setUniform("u_camera_position", Camera::current->eye);

	Matrix44 inv_vp = Camera::current->viewprojection_matrix;
	inv_vp.inverse();
	ambient_shader->setUniform("u_inverse_viewprojection", inv_vp);
	ambient_shader->setUniform("u_res_inv", vec2(1.0f / gbuffer_fbo->width, 1.0f / gbuffer_fbo->height));


	// Render fullscreen quad
	quad2->render(GL_TRIANGLES);

	ambient_shader->disable();

}

void Renderer::generateSpherePoints(int num, float radius, bool hemi) {
	std::vector<vec3> points;
	points.resize(num);

	for (int i = 0; i < num; i++) {
		float u = random();
		float v = random();

		float theta = u * 2.0f * PI;
		float phi = acos(2.0f * v - 1.0f);
		float r = cbrt(random() * 0.9f + 0.1f) * radius;

		vec3 p;
		p.x = r * sin(phi) * cos(theta);
		p.y = r * sin(phi) * sin(theta);
		p.z = r * cos(phi);

		if (hemi && p.z < 0.0f) p.z *= -1.0f;

		points[i] = p;
	}

	ssao_samples = points;
}

void Renderer::renderSSAO(Camera* camera)
{
	GFX::Shader* ssao_shader = GFX::Shader::Get("ssao");
	GFX::Mesh* quad = GFX::Mesh::getQuad();


	if (!use_ssao || !ssao_shader) return;

	glViewport(0, 0, ssao_fbo->width, ssao_fbo->height);
	glClearColor(1.0, 1.0, 1.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	ssao_shader->enable();

	ssao_shader->setUniform("u_res_inv", Vector2f(1.0f / ssao_fbo->width, 1.0f / ssao_fbo->height));
	ssao_shader->setUniform("u_sample_count", ssao_kernel_size);
	ssao_shader->setUniform("u_sample_radius", ssao_radius);

	vec3* ssao_pos = new vec3[ssao_samples.size()];

	for (int i = 0; i < ssao_samples.size(); i++)
	{
		ssao_pos[i] = ssao_samples[i];

	}
	ssao_shader->setUniform3Array("u_sample_pos", (float*)ssao_pos, min(ssao_samples.size(), 64));
	ssao_shader->setUniform("u_use_ssao_plus", use_ssao_plus ? 1 : 0);

	ssao_shader->setTexture("u_gbuffer_normal", gbuffer_fbo->color_textures[1], 1);

	// Bind depth texture
	ssao_shader->setTexture("u_gbuffer_depth", gbuffer_fbo->depth_texture, 0);

	// Send projection and inverse
	Matrix44 proj = camera->projection_matrix;
	Matrix44 inv_proj = proj;
	inv_proj.inverse();

	ssao_shader->setUniform("u_p_mat", proj);
	ssao_shader->setUniform("u_inv_p_mat", inv_proj);

	ssao_shader->setUniform("u_view_mat", camera->view_matrix); 

	ssao_shader->setUniform("u_near", camera->near_plane);
	ssao_shader->setUniform("u_far", camera->far_plane);


	glDisable(GL_DEPTH_TEST);
	// Draw quad
	quad->render(GL_TRIANGLES);

	glEnable(GL_DEPTH_TEST);

	ssao_shader->disable();

	delete[] ssao_pos;

}

void Renderer::renderToTonemap()
{

	GFX::Shader* shader = GFX::Shader::Get("tonemap");
	if (!shader) return;

	shader->enable();
	shader->setUniform("u_exposure", exposure);
	shader->setTexture("u_hdr_texture", hdr_fbo->color_textures[0], 0);
	shader->setUniform("u_apply_gamma", apply_gamma);
	shader->setUniform("u_tone_operator", tone_operator);

	GFX::Mesh::getQuad()->render(GL_TRIANGLES);
	shader->disable();

	

}

void Renderer::copyDepthBuffer(GFX::FBO* source, GFX::FBO* dest) {
	glBindFramebuffer(GL_READ_FRAMEBUFFER, source->fbo_id);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest->fbo_id);
	glBlitFramebuffer(0, 0, source->width, source->height,
		0, 0, dest->width, dest->height,
		GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::setLightVolumeRenderState() {
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE); // Blending aditivo
	glDepthFunc(GL_GREATER); // Solo renderizar detr�s de la geometr�a
	glDepthMask(GL_FALSE); // No escribir en depth buffer
	glCullFace(GL_FRONT); // Renderizar solo back faces (GL_CW)
}

void Renderer::restoreDefaultRenderState() {
	glDisable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ZERO);

	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);

	glCullFace(GL_BACK);
	glFrontFace(GL_CCW);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}

#ifndef SKIP_IMGUI

void Renderer::showUI()
{
	// Configuración básica de renderizado
	ImGui::Checkbox("Wireframe", &render_wireframe);
	ImGui::Checkbox("Boundaries", &render_boundaries);
	ImGui::SliderFloat("Shadow Bias", &shadow_bias, 0.0f, 0.01f);
	ImGui::Checkbox("Front Face Culling", &front_face_culling);

	// Modos de renderizado: Deferred o Multipass (mutuamente excluyentes)
	bool deferred_selected = use_deferred;
	bool multipass_selected = use_multipass;

	if (ImGui::Checkbox("Deferred Rendering", &deferred_selected)) {
		use_deferred = deferred_selected;
		if (use_deferred) use_multipass = false;
	}

	if (ImGui::Checkbox("Multipass Rendering", &multipass_selected)) {
		use_multipass = multipass_selected;
		if (use_multipass) use_deferred = false;
	}

	if (use_deferred)
	{
		ImGui::Indent();

		// Opciones específicas de deferred rendering
		const char* deferred_modes[] = { "Single Pass", "Light Volumes", "SSAO", "HDR" };
		static int current_deferred_mode = 0;
		ImGui::Combo("Deferred Mode", &current_deferred_mode, deferred_modes, IM_ARRAYSIZE(deferred_modes));

		switch (current_deferred_mode)
		{
		case 0: // Single Pass
			light_volume = false;
			use_ssao = false;
			use_hdr = false;
			break;

		case 1: // Light Volumes
			light_volume = true;
			use_ssao = false;
			use_hdr = false;

			// Motion Blur 
			ImGui::Checkbox("Motion Blur", &use_motion_blur);
			if (use_motion_blur)
			{
				ImGui::Indent();
				ImGui::SliderFloat("Motion Blur Strength", &motion_blur_strength, 0.0f, 5.0f);
				ImGui::SliderInt("Motion Blur Samples", &motion_blur_samples, 4, 32);
				ImGui::Checkbox("Per-Object Motion Blur", &use_object_motion_blur);
				ImGui::Unindent();
			}

			break;

		case 2: // SSAO
			light_volume = false;
			use_ssao = true;
			use_hdr = false;

			ImGui::Checkbox("SSAO+", &use_ssao_plus);
			ImGui::SliderFloat("SSAO Radius", &ssao_radius, 0.01f, 2.0f);
			ImGui::SliderInt("SSAO Samples", &ssao_kernel_size, 1, 64);

			if (use_ssao_plus)
			{
				ImGui::Checkbox("SSAO + Deferred", &ssao_plus_deferred);
				if (ssao_plus_deferred)
				{
					ImGui::SliderFloat("Ambient Intensity", &ambient_intensity, 0.1f, 1.0f);
				}
			}
			break;

		case 3: // HDR
			light_volume = false;
			use_ssao = false;
			use_hdr = true;

			const char* tone_ops[] = { "Linear", "Reinhard", "ACES" };
			ImGui::Combo("Tone Mapping", &tone_operator, tone_ops, IM_ARRAYSIZE(tone_ops));
			ImGui::SliderFloat("HDR Exposure", &exposure, 0.1f, 5.0f);
			ImGui::Checkbox("Apply Gamma Correction", &apply_gamma);

			// Motion Blur 
			ImGui::Checkbox("Motion Blur", &use_motion_blur);
			if (use_motion_blur)
			{
				ImGui::Indent();
				ImGui::SliderFloat("Motion Blur Strength", &motion_blur_strength, 0.0f, 10.0f);
				ImGui::SliderInt("Motion Blur Samples", &motion_blur_samples, 4, 32);
				ImGui::Checkbox("Per-Object Motion Blur", &use_object_motion_blur);
				ImGui::Unindent();
			}

			break;
		}

		ImGui::Unindent();
	}

	

	// Scene Blur (opción independiente)
	ImGui::Checkbox("Scene - Blur per Object", &scene_blur_object);
	if (scene_blur_object)
	{
		ImGui::Indent();
		ImGui::SliderFloat("Car - Frequency", &frecuencia, 0.05f, 1.0f);
		ImGui::Unindent();
	}
}

#else
void Renderer::showUI() {}
#endif