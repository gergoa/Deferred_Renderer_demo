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

struct Light
{
	glm::vec4 m_lightPosition;
	glm::vec3 m_La;
	glm::vec3 m_Ld;
	glm::vec3 m_Ls;
};

struct Material
{
	glm::vec3 m_Ka;
	glm::vec3 m_Kd;
	glm::vec3 m_Ks;
	float m_shininess;
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
	float m_ElapsedTimeInSec = 0.0f;

	glm::mat4 m_suzanneWorldTransform = glm::translate<float>(glm::vec3(0,0,0));

	// Camera
	Camera m_camera;
	CameraManipulator m_cameraManipulator;

	//
	// OpenGL
	//
	GLuint m_emptyVAO = 0;

	void DrawAxes();

	// Shader variables
	GLuint m_geom_pass_programID = 0;			// Shader of the objects
	GLuint m_programAxesID = 0;		// Program showing X,Y,Z directions
	GLuint m_deferred_pass_programID = 0; // Postprocess program


	// Light sources
	std::vector<Light> m_lightSources;

	// Material properties
	std::vector<Material> m_materials;

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
	OGLObject m_Suzanne = {};

	// Geometry initialization and termination
	void InitGeometry();
	void CleanGeometry();

	// Texture variables
	GLuint m_SamplerID = 0;

	GLuint m_metalTextureID = 0;


	// Texture initialization and termination
	void InitTextures();
	void CleanTextures();

	// Tessellation
	bool m_wireframe_enable = false;
	float m_tess_level = 1.0f;

	// Framebuffer variables
	GLuint m_frameBufferID = 0;
	GLuint m_diffuseBufferID = 0;
	GLuint m_normalBufferID = 0;
	GLuint m_depthBufferID = 0;

	// Framebuffer initialization and termination
	void InitFrameBufferObject();
	void CleanFrameBufferObject();
	void InitFBOResources(int, int);
	void CleanFBOResources();
};