#pragma once
#include "scene.h"
#include "prefab.h"
#include "camera.h"

#include "light.h"

//forward declarations
class Camera;
class Skeleton;
namespace GFX {
	class Shader;
	class Mesh;
	class FBO;
}

namespace SCN {

	class Prefab;
	class Material;

	// This class is in charge of rendering anything in our system.
	// Separating the render from anything else makes the code cleaner
	class Renderer
	{
	public:
		bool render_wireframe;
		bool render_boundaries;

		GFX::Texture* shadow_map = nullptr;
		GFX::FBO* shadow_fbo = nullptr;
		GFX::FBO* lighting_fbo = nullptr;

		GFX::FBO* gbuffer_fbo = nullptr;
		bool use_deferred = false;

		// variables for assignment 6 part 2
		GFX::FBO* ssao_fbo = nullptr;
		GFX::Shader* ssao_shader = nullptr;
		std::vector<vec3> ao_sample_points;
		int ssao_sample_count = 32;
		float ssao_radius = 0.05f;
		bool ssao_compute_enabled = false;
		bool ssao_apply_to_lighting = false;
		GFX::Texture* ssao_noise_texture = nullptr;

		std::vector<GFX::FBO*> shadow_fbos;

		// variables for 6 part 3
		GFX::FBO* hdr_fbo = nullptr;
		float hdr_exposure = 0.2f;
		bool apply_gamma = true;

		GFX::Texture* skybox_cubemap;

		SCN::Scene* scene;

		bool use_multipass;

		float shadow_bias;

		bool front_face_culling;


		//updated every frame
		Renderer(const char* shaders_atlas_filename );

		//just to be sure we have everything ready for the rendering
		void setupScene();

		//add here your functions
		//...

		void setupLight(SCN::LightEntity* light); // 3.2.1 ASSIGNMENT 3
		void renderShadowMap(SCN::Scene* scene); // 3.2.2 ASSIGNMENT 3

		void parseSceneEntities(SCN::Scene* scene, Camera* camera);

		//renders several elements of the scene
		void renderScene(SCN::Scene* scene, Camera* camera);

		//render the skybox
		void renderSkybox(GFX::Texture* cubemap);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);
		void renderToGBuffer();
		void renderDeferredSinglePass();

		void renderLightVolumes(Camera* camera);

		void renderSSAO(Camera* camera);
		
		void renderToTonemap();

		void showUI();
	};

};