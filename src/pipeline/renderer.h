#pragma once
#include "scene.h"
#include "prefab.h"
#include "camera.h"
#define M_PI 3.14159265358979323846
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
		bool light_volume = false;

		GFX::Texture* shadow_map = nullptr;
		GFX::FBO* shadow_fbo = nullptr;
		GFX::FBO* lighting_fbo = nullptr;

		GFX::FBO* gbuffer_fbo = nullptr;
		bool use_deferred = false;

		GFX::FBO* hdr_fbo = nullptr;

		std::vector<GFX::FBO*> shadow_fbos;

		GFX::Texture* skybox_cubemap;

		SCN::Scene* scene;

		//Blur 
		SCN::Node* car1 = nullptr;
		SCN::Node* car2 = nullptr;

		struct MotionBlurData {
			Matrix44 prev_model;
			Matrix44 current_model;
		};

		std::map<SCN::Node*, MotionBlurData> motion_data;


		bool scene_blur_object;

		int last_ssao_kernel_size = -1;
		float last_ssao_radius = -1.0f;
		bool last_ssao_plus = false;

		float shadow_bias = 0.003f;
		float amplitud;
		float frecuencia;
		bool use_motion_blur;
		bool use_multipass = false;

		bool front_face_culling;
		float motion_blur_strength;
		bool use_object_motion_blur;
		int motion_blur_samples;
		bool has_prev_view_projection = false;


		// In Renderer.h
		GFX::FBO* ssao_fbo;
		GFX::FBO* ssao_blur_fbo;
		std::vector<vec3> ssao_samples;
		//GLuint ssao_noise_texture;
		float ssao_radius = 0.5f;
		int ssao_kernel_size = 32;
		bool use_ssao = false;
		bool use_hdr = false;
		float exposure = 1.f;
		bool use_ssao_plus = false;

		bool apply_gamma = true;
		bool use_aces = false;
		enum ToneMappingOperator {
			TONE_LINEAR = 0,
			TONE_REINHARD = 1,
			TONE_ACES = 2
		};

		int tone_operator = TONE_REINHARD;

		bool ssao_plus_deferred = false;

		float ambient_intensity = 0.3f;


		//updated every frame
		Renderer(const char* shaders_atlas_filename );

		//just to be sure we have everything ready for the rendering
		void setupScene();

		//add here your functions
		//...

		void setupLight(SCN::LightEntity* light); // 3.2.1 ASSIGNMENT 3
		void renderShadowMap(SCN::Scene* scene); // 3.2.2 ASSIGNMENT 3
		void parseNodes(SCN::Node* node, Camera* cam, BaseEntity* entity);
		void parseSceneEntities(SCN::Scene* scene, Camera* camera);

		//renders several elements of the scene
		void renderScene(SCN::Scene* scene, Camera* camera);

		//render the skybox
		void renderSkybox(GFX::Texture* cubemap);

		//to render one mesh given its material and transformation matrix
		void renderMeshWithMaterial(const Matrix44 model, GFX::Mesh* mesh, SCN::Material* material);
		void renderToGBuffer();
		void renderDeferredSinglePass();
		void renderDirectionalLights();

		void copyDepthBuffer(GFX::FBO* source, GFX::FBO* dest);


		void restoreDefaultRenderState();

		void setLightVolumeRenderState();

		void renderDeferredAmbientPass();

		void generateSpherePoints(int num, float radius, bool hemi);

		void renderSSAO(Camera* camera);

		void renderToTonemap();

		void renderLightVolumes(Camera* camera);
		
		void renderMotionVectors();

		void renderFBOToScreen(GFX::FBO* fbo, GFX::Shader* shader);


		void applyMotionBlur();



		void showUI();

		void update(float dt);
	};

};