#pragma once

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>

// GLEW
#include <GL/glew.h>

// SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

// Utils
#include "GLUtils.hpp"
#include "Camera.h"
#include "CameraManipulator.h"

struct SUpdateInfo
{
	float ElapsedTimeInSec = 0.0f;	// Elapsed time since start of the program
	float DeltaTimeInSec = 0.0f;	// Elapsed time since last update
};

struct RenderPassConfig
{
	GLuint fboID = 0;              // FBO to render in (default is 0, screen)
	GLuint programID = 0;          // used shader program

	bool depthTest = false;
	bool depthWrite = false;	   // for glDepthMask, whether we will be writing into the depth buffer

	bool blend = false;
	GLenum blendSrc = GL_ONE;
	GLenum blendDst = GL_ZERO;
	GLenum blendEquation = GL_FUNC_ADD;

	bool clearColor = true;		  // Clear parameters at the beginning of the pass
	bool clearDepth = false;
	glm::vec4 clearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	int viewportWidth = 0;
	int viewportHeight = 0;
};

class CMyApp
{
public:
	CMyApp();
	~CMyApp();

	bool Init();
	void Clean();

	void Update(const SUpdateInfo&);
	void Render();
	void RenderGUI();

	void KeyboardDown(const SDL_KeyboardEvent&);
	void KeyboardUp(const SDL_KeyboardEvent&);
	void MouseMove(const SDL_MouseMotionEvent&);
	void MouseDown(const SDL_MouseButtonEvent&);
	void MouseUp(const SDL_MouseButtonEvent&);
	void MouseWheel(const SDL_MouseWheelEvent&);
	void Resize(int, int);

	void OtherEvent(const SDL_Event&);
protected:
	void SetupDebugCallback();
	void RenderGeometry(GLenum primitiveType);

	//
	// Variables
	//
	int m_w, m_h; // Window size
	int m_render_w;
	int m_render_h;
	float m_renderResolution = 1.0f;

	float m_ElapsedTimeInSec = 0.0f;
	float m_DeltaTimeInSec = 0.0f;
	bool m_TimeFrozen = false;

	// Frame accumulation
	int m_AccumulationFrameCounter = 0;
	glm::mat4 m_lastTickView = glm::mat4(1.0f);
	glm::mat4 GetRandOffsetProj(const glm::mat4& projection);
	glm::mat4 GetPortalView(const glm::mat4& camView, const glm::mat4& srcPortal, const glm::mat4& dstPortal);


	glm::mat4 m_suzanneWorldTransform = glm::translate<float>(glm::vec3(9,-4,-7.75)) * glm::scale(glm::vec3(2));
	glm::mat4 m_birdWorldTransform = glm::translate(glm::vec3(0,-4.22,0)) * glm::rotate<float>(glm::radians(-90.0f), glm::vec3(1, 0, 0)) * glm::scale(glm::vec3(0.07f));

	// Camera
	Camera m_camera;
	CameraManipulator m_cameraManipulator;

	//
	// OpenGL
	//
	GLuint m_emptyVAO = 0;

	void SetRenderPass(const RenderPassConfig&);
	void DrawAxes();

	// Shader variables
	GLuint m_geom_pass_programID = 0;			// Shader of the objects
	GLuint m_programAxesID = 0;		// Program showing X,Y,Z directions
	GLuint m_deferred_pass_programID = 0; // Program for deferred shading pass
	GLuint m_postprocess_programID = 0; // Postprocess program
	GLuint m_ssao_programID = 0;
	GLuint m_ssao_blur_programID = 0;
	GLuint m_ssr_programID = 0;
	GLuint m_shadow_dir_programID = 0;
	GLuint m_shadow_omni_programID = 0;
	GLuint m_portal_programID = 0;

	// Light sources
	std::vector<Light> m_lightSources;

	float m_ambient = 1.0;
	float m_diffuse = 1.0;
	float m_specular = 1.0;

	// Material properties
	Material m_defaultMat = { glm::vec3(1.0), glm::vec3(1.0), glm::vec3(1.0), 16.0 };

	void InitLightFBO(Light& light);
	void CleanLightFBO(Light& light);


	void InitLightSources();
	void InitMaterials();

	void BindLightSource(const Light& light)
	{
		glUniform4fv(ul("lightPosition"), 1, glm::value_ptr(light.m_lightPosition));
		glUniform3fv(ul("La"), 1, glm::value_ptr(light.m_La));
		glUniform3fv(ul("Ld"), 1, glm::value_ptr(light.m_Ld));
		glUniform3fv(ul("Ls"), 1, glm::value_ptr(light.m_Ls));
	}

	void BindMaterial(const Material& material)
	{
		glUniform3fv(ul("Ka"), 1, glm::value_ptr(material.m_Ka));
		glUniform3fv(ul("Kd"), 1, glm::value_ptr(material.m_Kd));
		glUniform3fv(ul("Ks"), 1, glm::value_ptr(material.m_Ks));
		glUniform1f(ul("shininess"), material.m_shininess);
	}

	// Shader initialization and termination
	void InitShaders();
	void CleanShaders();
	void InitAxesShader();
	void CleanAxesShader();

	// Geometry variables

	std::vector<RenderObject> m_sceneObjects;
	GLuint m_wallTexID = 0;

	// Geometry initialization and termination
	void InitGeometry();
	void CleanGeometry();

	void CMyApp::DrawObject(const RenderObject& obj, GLenum primitiveType)
	{
		// Bind VAO and texture
		glBindVertexArray(obj.m_mesh.vaoID);
		glBindTextureUnit(0, obj.m_textureID);

		// Set uniforms
		glUniformMatrix4fv(ul("world"), 1, GL_FALSE, glm::value_ptr(obj.m_worldTransform));
		glUniformMatrix4fv(ul("worldIT"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(obj.m_worldTransform))));
		glUniform1f(ul("m_reflectivity"), obj.m_reflectivity);

		glDrawElements(primitiveType, obj.m_mesh.count, GL_UNSIGNED_INT, 0);
	}

	void CMyApp::RenderPortal(
		const glm::mat4& camView, const glm::mat4& camProj,
		const RenderObject& srcPortal, const RenderObject& dstPortal,
		GLuint targetFBO, int width, int height);

	// Texture variables
	GLuint m_SamplerID = 0;

	// Texture initialization and termination
	void InitTextures();
	void CleanTextures();

	// Tessellation
	bool m_wireframe_enable = false;
	float m_min_tess_dist = 1.5f;
	float m_max_tess_dist = 7.0f;
	float m_max_tess_level = 8.0f;

	// SSAO
	std::vector<glm::vec3> m_ssaoKernel;
	std::vector<glm::vec3> m_ssaoNoise;
	

	// Deferred Framebuffer variables
	GLuint m_geometry_fboID = 0;
	GLuint m_diffuseBufferID = 0;
	GLuint m_normalBufferID = 0;
	GLuint m_depthBufferID = 0;
	void InitGeometryFBO(int, int);
	void CleanGeometryFBO();

	// Accumulation Framebuffer variables
	GLuint m_accum_fboID = 0;
	GLuint m_accum_colorBufferID = 0;
	void InitAccumFBO(int, int);
	void CleanAccumFBO();

	// SSAO framebuffer variables
	GLuint m_ssao_fboID = 0;
	GLuint m_ssao_colorBufferID = 0;

	GLuint m_ssao_blur_fboID = 0;
	GLuint m_ssao_blur_colorBufferID = 0;

	GLuint m_ssao_noise_TextureID = 0;

	void InitSSAO_Noise(const int, const int);

	void InitSSAO_FBO(const int,const int);
	void CleanSSAO_FBO();


	// Lighting pass framebuffer
	GLuint m_deferred_light_fboID = 0;
	GLuint m_deferred_light_colorBufferID = 0;
	void InitLightPassFBO(int, int);
	void CleanLightPassFBO();

	GLuint m_ssr_fboID = 0;
	GLuint m_ssr_colorBufferID = 0;
	void InitSSR_FBO(const int, const int);
	void CleanSSR_FBO();

	GLuint m_bluePortalFBO = 0;
	GLuint m_orangePortalFBO = 0;
	GLuint m_bluePortalTexID = 0;
	GLuint m_orangePortalTexID = 0;
	GLuint m_bluePortalDepthID = 0;
	GLuint m_orangePortalDepthID = 0;

	void InitPortalFBO(const int, const int);
	void CleanPortalFBO();

	// Framebuffer initialization and termination
	void InitFrameBufferObjects();
	void CleanFrameBufferObjects();
	void InitFBOResources(int, int);
	void CleanFBOResources();
};