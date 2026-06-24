#include "MyApp.h"
#include "SDL_GLDebugMessageCallback.h"
#include "GLUtils.hpp"
#include "ObjParser.h"
#include "ProgramBuilder.h"

#include <imgui.h>
#include <algorithm>
#include <iostream>
#include <random>
#include <string>

CMyApp::CMyApp()
{
}

CMyApp::~CMyApp()
{
}

void CMyApp::SetupDebugCallback()
{
	// Enable and set the debug callback function if we are in debug context
	GLint context_flags;
	glGetIntegerv(GL_CONTEXT_FLAGS, &context_flags);
	if (context_flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
		glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR, GL_DONT_CARE, 0, nullptr, GL_FALSE);
		glDebugMessageCallback(SDL_GLDebugMessageCallback, nullptr);
	}
}

void CMyApp::InitShaders()
{
	m_geom_pass_programID = glCreateProgram();
	ProgramBuilder{ m_geom_pass_programID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/gbuffer.vert")
		.ShaderStage(GL_TESS_CONTROL_SHADER, "Shaders/gbuffer.tesc")
		.ShaderStage(GL_TESS_EVALUATION_SHADER, "Shaders/gbuffer.tese")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/gbuffer.frag")
		.Link();

	m_deferred_pass_programID = glCreateProgram();
	ProgramBuilder{ m_deferred_pass_programID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/fullscreen.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/deferred.frag")
		.Link();
	
	m_postprocess_programID = glCreateProgram();
	ProgramBuilder{ m_postprocess_programID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/fullscreen.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/postprocess.frag")
		.Link();

	m_ssao_programID = glCreateProgram();
	ProgramBuilder{ m_ssao_programID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/fullscreen.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/ssao.frag")
		.Link();

	m_ssao_blur_programID = glCreateProgram();
	ProgramBuilder{ m_ssao_blur_programID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/fullscreen.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/ssao_blur.frag")
		.Link();

	m_ssr_programID = glCreateProgram();
	ProgramBuilder{ m_ssr_programID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/fullscreen.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/ssr.frag")
		.Link();


	m_shadow_dir_programID = glCreateProgram();
	ProgramBuilder{ m_shadow_dir_programID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/shadow.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/shadow.frag")
		.Link();

	m_shadow_omni_programID = glCreateProgram();
	ProgramBuilder{ m_shadow_omni_programID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/shadow_omni.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/shadow_omni.frag")
		.Link();
	InitAxesShader();
}

void CMyApp::CleanShaders()
{
	glDeleteProgram(m_geom_pass_programID);
	glDeleteProgram(m_deferred_pass_programID);
	glDeleteProgram(m_postprocess_programID);
	glDeleteProgram(m_ssao_programID);
	glDeleteProgram(m_ssao_blur_programID);
	glDeleteProgram(m_shadow_dir_programID);
	CleanAxesShader();
}

void CMyApp::InitAxesShader()
{
	m_programAxesID = glCreateProgram();
	ProgramBuilder{ m_programAxesID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/Vert_axes.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/Frag_PosCol.frag")
		.Link();
}

void CMyApp::CleanAxesShader()
{
	glDeleteProgram(m_programAxesID);
}

// SSAO Helper methods
float lerp(float a, float b, float f)
{
	return a + f * (b - a);
}

void GenSSAOKernel(std::vector<glm::vec3>& ssaoKernel, std::vector<glm::vec3>& ssaoNoise, const unsigned int samples, const unsigned int rotations)
{
	if (std::sqrt(rotations) * std::sqrt(rotations) != rotations) SDL_LogError(SDL_LOG_PRIORITY_ERROR, "[Init] Invalid Kernel Rotation size provided!");


	ssaoKernel.clear();

	std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
	std::default_random_engine generator;
	for (unsigned int i = 0; i < samples; ++i)
	{
		glm::vec3 sample(
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator)
		);
		sample = glm::normalize(sample);
		sample *= randomFloats(generator);


		float scale = (float)i / 64.0f;
		scale = lerp(0.1f, 1.0f, scale * scale);
		sample *= scale;
		ssaoKernel.push_back(sample);
	}

	ssaoNoise.clear();

	for (unsigned int i = 0; i < rotations; ++i)
	{
		glm::vec3 noise(
			randomFloats(generator) * 2.0 - 1.0,
			randomFloats(generator) * 2.0 - 1.0,
			0.0
		);
		ssaoNoise.push_back(noise);

	}

}

void CMyApp::InitSSAO_Noise(const int kernelSize, const int rotations)
{
	// generate ssao kernel
	int sizeTex = (int)std::sqrt(rotations);
	GenSSAOKernel(m_ssaoKernel, m_ssaoNoise, kernelSize, rotations);

	// bind to texture
	glGenTextures(1, &m_ssao_noise_TextureID);
	glBindTexture(GL_TEXTURE_2D, m_ssao_noise_TextureID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, sizeTex, sizeTex, 0, GL_RGB, GL_FLOAT, &m_ssaoNoise[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void CMyApp::InitGeometry()
{
	const std::initializer_list<VertexAttributeDescriptor> vertexAttribList =
	{
		{ 0, offsetof(Vertex, position), 3, GL_FLOAT },
		{ 1, offsetof(Vertex, normal),	 3, GL_FLOAT },
		{ 2, offsetof(Vertex, texcoord), 2, GL_FLOAT },
	};

	glCreateVertexArrays(1, &m_emptyVAO);

	// Suzanne
	m_sceneObjects.push_back(CreateObject(
		"Assets/Suzanne.obj",
		vertexAttribList,
		"Assets/metal.png",
		m_defaultMat,
		glm::translate(glm::vec3(9, -4, -7.75f)) * glm::scale(glm::vec3(2)),
		0.0f
	));

	// Mirror
	m_sceneObjects.push_back(CreateObject(
		"Assets/Mirror.obj",
		vertexAttribList,
		"Assets/mirror.png",
		m_defaultMat,
		glm::translate(glm::vec3(0, -3.5, 0)) * glm::scale(glm::vec3(0.135, 0.3, 0.085)),
		1.0f
	));

	// Bird
	m_sceneObjects.push_back(CreateObject(
		"Assets/Bird_v1.obj", 
		vertexAttribList,
		"Assets/bird_v1.jpg",
		m_defaultMat,
		m_birdWorldTransform,
		0.0f
	));

	// Wall, load tex manually
	const std::string wallTex = "Assets/wall.jpg";
	ImageRGBA image = ImageFromFile(wallTex);

	if (image.width > 0 && image.height > 0)
	{
		glCreateTextures(GL_TEXTURE_2D, 1, &m_wallTexID);
		glTextureStorage2D(m_wallTexID, NumberOfMIPLevels(image), GL_RGBA8, image.width, image.height);
		glTextureSubImage2D(m_wallTexID, 0, 0, 0, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, image.data());

		glGenerateTextureMipmap(m_wallTexID);
	}
	else
	{
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failure to load texture: %s", wallTex);
	}

	for (int i = 0; i < 4; ++i) {
		m_sceneObjects.push_back(CreateObject(
			"Assets/Wall.obj",
			vertexAttribList, 
			m_wallTexID,
			m_defaultMat,
			glm::translate(glm::vec3(i * 9.75, -5, -10)) * glm::scale(glm::vec3(1)),
			0.35f
		));
	}

	/*// table
	m_sceneObjects.push_back(CreateObject(
		"Assets/table.obj",
		vertexAttribList,
		"Assets/table.jpg",
		m_defaultMat,
		glm::translate(glm::vec3(10, -15, 0)) * glm::scale(glm::vec3(0.1, 0.1, 0.1)) * glm::rotate<float>(glm::radians(-90.0), glm::vec3(1, 0, 0)),
		0.0f
	));*/
}

void CMyApp::CleanGeometry()
{
	for (auto& obj : m_sceneObjects)
	{
		CleanOGLObject(obj.m_mesh);
	}

	m_sceneObjects.clear();
	glDeleteVertexArrays(1, &m_emptyVAO);
}

void CMyApp::InitLightFBO(Light& light)
{
	if (light.state != FBO_NOT_BOUND) return;

	// Directional light
	if (light.m_lightPosition.w == 0.0f) 
	{
		glCreateFramebuffers(1, &light.m_shadowFBO);
		// Simple depth buffer texture
		glCreateTextures(GL_TEXTURE_2D, 1, &light.m_shadowTexID);
		glTextureStorage2D(light.m_shadowTexID, 1, GL_DEPTH_COMPONENT24, light.m_shadowMapSize, light.m_shadowMapSize);

		// https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
		// @ Oversampling, fragments outside the light's frustum will be sampled with incorrect depth values, when we read the depth buffer of light
		glTextureParameteri(light.m_shadowTexID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
		glTextureParameteri(light.m_shadowTexID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
		const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		glTextureParameterfv(light.m_shadowTexID, GL_TEXTURE_BORDER_COLOR, borderColor);
		glNamedFramebufferTexture(light.m_shadowFBO, GL_DEPTH_ATTACHMENT, light.m_shadowTexID, 0);

		// no color attachments
		glNamedFramebufferDrawBuffer(light.m_shadowFBO, GL_NONE);
		glNamedFramebufferReadBuffer(light.m_shadowFBO, GL_NONE);

		// Completeness check
		GLenum status = glCheckNamedFramebufferStatus(light.m_shadowFBO, GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			switch (status) {
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
				SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT!");
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
				SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT!");
				break;
			case GL_FRAMEBUFFER_UNSUPPORTED:
				SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_UNSUPPORTED!");
				break;
			}
		}
		else light.state = INITIALIZED;
	}
	// Point light
	else
	{
		glCreateFramebuffers(1, &light.m_shadowFBO);
		// Simple depth buffer texture
		glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &light.m_shadowTexID);
		glTextureStorage2D(light.m_shadowTexID, 1, GL_DEPTH_COMPONENT24, light.m_shadowMapSize, light.m_shadowMapSize);
		glTextureParameteri(light.m_shadowTexID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTextureParameteri(light.m_shadowTexID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTextureParameteri(light.m_shadowTexID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTextureParameteri(light.m_shadowTexID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTextureParameteri(light.m_shadowTexID, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		
		// At initialization we bind to cubemap's 
		glNamedFramebufferTextureLayer(light.m_shadowFBO, GL_DEPTH_ATTACHMENT, light.m_shadowTexID, 0, 0);

		// no color attachments
		glNamedFramebufferDrawBuffer(light.m_shadowFBO, GL_NONE);
		glNamedFramebufferReadBuffer(light.m_shadowFBO, GL_NONE);

		// Completeness check
		GLenum status = glCheckNamedFramebufferStatus(light.m_shadowFBO, GL_FRAMEBUFFER);

		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			switch (status) {
			case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
				SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT!");
				break;
			case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
				SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT!");
				break;
			case GL_FRAMEBUFFER_UNSUPPORTED:
				SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_UNSUPPORTED!");
				break;
			}
		}

		else light.state = INITIALIZED;
	}

}

void CMyApp::CleanLightFBO(Light& light)
{
	glDeleteFramebuffers(1, &light.m_shadowFBO);
	glDeleteTextures(1, &light.m_shadowTexID);
	light.state = NOT_INITIALIZED;
}


void CMyApp::InitLightSources()
{
	// Lights in a hexagonal shape
	for (int i = 0; i < 6; ++i)
	{
		float angle = glm::radians(i * 60.0f);
		float x = 20.0f * cos(angle);
		float z = 20.0f * sin(angle);
		m_lightSources.push_back(
			CreateLight(glm::vec3(x, 0.0f, z), 
						true,
						glm::vec3(0.0f),
						glm::vec3(abs(cos(angle)), abs(sin(angle)), abs(cos(angle + 1.0f))),
						glm::vec3(0.0f),
						true, 1024, 18
		));
	}

	// Directional sunlight
	m_lightSources.push_back(
		CreateLight(glm::vec3(0.0f, 1.0f, 1.0f),
			false,
			glm::vec3(0.1f),
			glm::vec3(0.55f),
			glm::vec3(0.55f),
			true,
			2048
		));

	for (auto& light : m_lightSources)
	{
		InitLightFBO(light);
	}
}


void CMyApp::InitMaterials()
{
	//m_materials.push_back({ glm::vec3(1.0f), glm::vec3(1.0f), glm::vec3(1.0f), 20.0f });
}

void CMyApp::InitTextures()
{
	glCreateSamplers( 1, &m_SamplerID );
	glSamplerParameteri( m_SamplerID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glSamplerParameteri( m_SamplerID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glSamplerParameteri( m_SamplerID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glSamplerParameteri( m_SamplerID, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
}

void CMyApp::CleanTextures()
{
	for (auto& obj : m_sceneObjects)
	{
		glDeleteTextures(1, &obj.m_textureID);
	}

	glDeleteSamplers( 1, &m_SamplerID );
}

void CMyApp::InitFrameBufferObjects()
{
	// FBO létrehozása
	glCreateFramebuffers(1, &m_geometry_fboID);
	glCreateFramebuffers(1, &m_accum_fboID);
	glCreateFramebuffers(1, &m_deferred_light_fboID);
	glCreateFramebuffers(1, &m_ssao_fboID);
	glCreateFramebuffers(1, &m_ssao_blur_fboID);
	glCreateFramebuffers(1, &m_ssr_fboID);
}

void CMyApp::CleanFrameBufferObjects()
{
	glDeleteFramebuffers(1, &m_geometry_fboID );
	glDeleteFramebuffers(1, &m_accum_fboID);
	glDeleteFramebuffers(1, &m_deferred_light_fboID );
	glDeleteFramebuffers(1, &m_ssao_fboID);
	glDeleteFramebuffers(1, &m_ssao_blur_fboID);
	glDeleteFramebuffers(1, &m_ssr_fboID);

}

void CMyApp::InitGeometryFBO(int width, int height)
{
	// Setup the texture
	// We use texture because we will sample it later in the shader

	// Diffuse
	glCreateTextures(GL_TEXTURE_2D, 1, &m_diffuseBufferID);
	glTextureStorage2D(m_diffuseBufferID, 1, GL_RGBA8, width, height);
	glTextureParameteri(m_diffuseBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_diffuseBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_geometry_fboID, GL_COLOR_ATTACHMENT0, m_diffuseBufferID, 0);


	// Normal
	glCreateTextures(GL_TEXTURE_2D, 1, &m_normalBufferID);
	glTextureStorage2D(m_normalBufferID, 1, GL_RGBA16_SNORM, width, height);
	glTextureParameteri(m_normalBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_normalBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_geometry_fboID, GL_COLOR_ATTACHMENT1, m_normalBufferID, 0);


	// Depth
	glCreateTextures(GL_TEXTURE_2D, 1, &m_depthBufferID);
	glTextureStorage2D(m_depthBufferID, 1, GL_DEPTH_COMPONENT24, width, height);
	glTextureParameteri(m_depthBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_depthBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_geometry_fboID, GL_DEPTH_ATTACHMENT, m_depthBufferID, 0);

	const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };

	glNamedFramebufferDrawBuffers(m_geometry_fboID, 2, drawBuffers);

	// Completeness check
	GLenum status = glCheckNamedFramebufferStatus(m_geometry_fboID, GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		switch (status) {
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_UNSUPPORTED!");
			break;
		}
	}
}

void CMyApp::CleanGeometryFBO()
{
	glDeleteTextures(1, &m_diffuseBufferID);
	glDeleteTextures(1, &m_normalBufferID);
	glDeleteTextures(1, &m_depthBufferID);
}

void CMyApp::InitAccumFBO(int width, int height)
{
	// Setup one high resolution color channel
	glCreateTextures(GL_TEXTURE_2D, 1, &m_accum_colorBufferID);
	glTextureStorage2D(m_accum_colorBufferID, 1, GL_RGBA32F, width, height);
	glTextureParameteri(m_accum_colorBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_accum_colorBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_accum_fboID, GL_COLOR_ATTACHMENT0, m_accum_colorBufferID, 0);


	const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0};

	glNamedFramebufferDrawBuffers(m_accum_fboID, 1, drawBuffers);
	// Completeness check
	GLenum status = glCheckNamedFramebufferStatus(m_accum_fboID, GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		switch (status) {
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_UNSUPPORTED!");
			break;
		}
	}
}

void CMyApp::CleanAccumFBO()
{
	glDeleteTextures(1, &m_accum_colorBufferID);
}

void CMyApp::InitSSAO_FBO(int width, int height)
{

	// SSAO depth buffer
	glCreateTextures(GL_TEXTURE_2D, 1, &m_ssao_colorBufferID);
	glTextureStorage2D(m_ssao_colorBufferID, 1, GL_R16, width, height);
	glTextureParameteri(m_ssao_colorBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_ssao_colorBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	glNamedFramebufferTexture(m_ssao_fboID, GL_COLOR_ATTACHMENT0, m_ssao_colorBufferID, 0);
	const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };
	glNamedFramebufferDrawBuffers(m_ssao_fboID, 1, drawBuffers);
	// Completeness check
	GLenum status = glCheckNamedFramebufferStatus(m_ssao_fboID, GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		switch (status) {
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_UNSUPPORTED!");
			break;
		}
	}

	// SSAO blur buffer
	glCreateTextures(GL_TEXTURE_2D, 1, &m_ssao_blur_colorBufferID);
	glTextureStorage2D(m_ssao_blur_colorBufferID, 1, GL_R16, width, height);
	glTextureParameteri(m_ssao_blur_colorBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_ssao_blur_colorBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_ssao_blur_fboID, GL_COLOR_ATTACHMENT0, m_ssao_blur_colorBufferID, 0);
	glNamedFramebufferDrawBuffers(m_ssao_blur_fboID, 1, drawBuffers);
	// Completeness check
	status = glCheckNamedFramebufferStatus(m_ssao_blur_fboID, GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		switch (status) {
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_UNSUPPORTED!");
			break;
		}
	}
}

void CMyApp::CleanSSAO_FBO()
{
	glDeleteTextures(1, &m_ssao_colorBufferID);
	glDeleteTextures(1, &m_ssao_blur_colorBufferID);
}



void CMyApp::InitLightPassFBO(int width, int height)
{
	// Setup one high resolution color channel
	glCreateTextures(GL_TEXTURE_2D, 1, &m_deferred_light_colorBufferID);
	glTextureStorage2D(m_deferred_light_colorBufferID, 1, GL_RGBA32F, width, height);
	glTextureParameteri(m_deferred_light_colorBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_deferred_light_colorBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_deferred_light_fboID, GL_COLOR_ATTACHMENT0, m_deferred_light_colorBufferID, 0);

	const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };

	glNamedFramebufferDrawBuffers(m_deferred_light_fboID, 1, drawBuffers);

	// Completeness check
	GLenum status = glCheckNamedFramebufferStatus(m_deferred_light_fboID, GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		switch (status) {
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_UNSUPPORTED!");
			break;
		}
	}
}	

void CMyApp::CleanLightPassFBO()
{
	glDeleteTextures(1, &m_deferred_light_colorBufferID);
}

void CMyApp::InitSSR_FBO(int width, int height)
{
	glCreateTextures(GL_TEXTURE_2D, 1, &m_ssr_colorBufferID);
	glTextureStorage2D(m_ssr_colorBufferID, 1, GL_RGBA16F, width, height);
	glTextureParameteri(m_ssr_colorBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_ssr_colorBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_ssr_fboID, GL_COLOR_ATTACHMENT0, m_ssr_colorBufferID, 0);

	const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };

	glNamedFramebufferDrawBuffers(m_ssr_fboID, 1, drawBuffers);

	// Completeness check
	GLenum status = glCheckNamedFramebufferStatus(m_ssr_fboID, GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE)
	{
		switch (status) {
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT!");
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED:
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "[InitFramebuffer] Incomplete framebuffer GL_FRAMEBUFFER_UNSUPPORTED!");
			break;
		}
	}
}

void CMyApp::CleanSSR_FBO()
{
	glDeleteTextures(1, &m_ssr_fboID);
}

void CMyApp::InitFBOResources(int width, int height)
{
	InitGeometryFBO(width, height);
	InitAccumFBO(width, height);
	InitSSAO_FBO(width, height);
	InitLightPassFBO(width, height);
	InitSSR_FBO(width, height);
}

void CMyApp::CleanFBOResources()
{
	CleanGeometryFBO();
	CleanAccumFBO();
	CleanSSAO_FBO();
	CleanLightPassFBO();
	CleanSSR_FBO();
}

bool CMyApp::Init()
{
	SetupDebugCallback();

	// Set a bluish clear color
	// glClear() will use this for clearing the color buffer.
	glClearColor(0.125f, 0.25f, 0.5f, 1.0f);


	// Init SSAO
	InitSSAO_Noise(16, 16);

	InitShaders();
	InitGeometry();
	InitLightSources();
	InitMaterials();
	InitTextures();
	InitFrameBufferObjects();

	//
	// Other
	//

	glEnable(GL_CULL_FACE);	 // Enable discarding the back-facing faces.
	glCullFace(GL_BACK);     // GL_BACK: facets facing away from camera, GL_FRONT: facets facing towards the camera
	glEnable(GL_DEPTH_TEST); // Enable depth testing. (for overlapping geometry)

	// Camera
	m_camera.SetView(
		glm::vec3(0, 5, 25),// From where we look at the scene - eye
		glm::vec3(0, 0, 0),	// Which point of the scene we are looking at - at
		glm::vec3(0, 1, 0)	// Upwards direction - up
	);
	m_cameraManipulator.SetCamera(&m_camera);

	return true;
}

void CMyApp::Clean()
{
	CleanShaders();
	CleanGeometry();
	CleanTextures();
	CleanFBOResources();
	CleanFrameBufferObjects();
}

void CMyApp::Update(const SUpdateInfo& updateInfo)
{
	m_cameraManipulator.Update(updateInfo.DeltaTimeInSec);

	// Objects stay frozen when the time is frozen
	if (m_TimeFrozen) return;

    m_ElapsedTimeInSec = updateInfo.ElapsedTimeInSec;
	m_DeltaTimeInSec = updateInfo.DeltaTimeInSec;

	// Spin point lights
	/*int pLightIdx = 0;
	for (size_t i = 0; i < m_lightSources.size(); ++i)
	{
		if (m_lightSources[i].m_lightPosition.w == 1.0f)
		{
			float angle = glm::radians(m_ElapsedTimeInSec * 30.0f + pLightIdx * 60.0f);
			float x = 6.0f * cos(angle);
			float z = 6.0f * sin(angle);

			m_lightSources[i].m_lightPosition.x = x;
			m_lightSources[i].m_lightPosition.z = z;


			pLightIdx++;
		}
	}*/
	m_defaultMat = { glm::vec3(m_ambient), glm::vec3(m_diffuse), glm::vec3(m_specular), 16.0 };
}

void CMyApp::RenderGeometry(GLenum primitiveType)
{
	glBindSampler(0, m_SamplerID);

	if (primitiveType == GL_PATCHES) {
		glPatchParameteri(GL_PATCH_VERTICES, 3);
	}

	for (const RenderObject& obj : m_sceneObjects)
	{
		SetUniform("receiveShadow", obj.m_receiveShadow ? 1 : 0);

		DrawObject(obj, primitiveType);
	}
}

void CMyApp::DrawAxes()
{
	glDisable(GL_DEPTH_TEST);
	glUseProgram(m_programAxesID);

	glUniformMatrix4fv( ul("VP"), 1, GL_FALSE, glm::value_ptr(m_camera.GetViewProj()));
	glUniformMatrix4fv( ul("world"), 1, GL_FALSE, glm::value_ptr(glm::translate(m_camera.GetAt())));

	glDrawArrays(GL_LINES, 0, 6);
	glEnable(GL_DEPTH_TEST);
	glUseProgram(0);
}

glm::mat4 CMyApp::GetRandOffsetProj(const glm::mat4& projection)
{
	// Random offset between [0, 1) for x and y
	float randX = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	float randY = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	
	glm::mat4 jitter = glm::translate(glm::vec3(randX / m_render_w, randY / m_render_h, 0.0f));
	return jitter * projection;
}

void CMyApp::SetRenderPass(const RenderPassConfig& config)
{
	// Bind FBO and viewport
	glBindFramebuffer(GL_FRAMEBUFFER, config.fboID);
	glViewport(0, 0, config.viewportWidth, config.viewportHeight);

	// Bind shader program
	if (config.programID != 0) {
		glUseProgram(config.programID);
	}

	// Enable depth test if needed
	if (config.depthTest) glEnable(GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);
	glDepthMask(config.depthWrite ? GL_TRUE : GL_FALSE);

	// Turn on blend if needed
	if (config.blend) {
		glEnable(GL_BLEND);
		glBlendEquation(config.blendEquation);
		glBlendFunc(config.blendSrc, config.blendDst);
	}
	else {
		glDisable(GL_BLEND);
	}

	// Clear buffer if needed
	GLbitfield clearMask = 0;
	if (config.clearColor) {
		glClearColor(config.clearColorValue.r, config.clearColorValue.g, config.clearColorValue.b, config.clearColorValue.a);
		clearMask |= GL_COLOR_BUFFER_BIT;
	}
	if (config.clearDepth) {
		clearMask |= GL_DEPTH_BUFFER_BIT;
	}

	if (clearMask != 0) {
		glClear(clearMask);
	}

	// bind empty vao intially (for passes that don't use their own geometry such as post)
	glBindVertexArray(m_emptyVAO); 
}

void CMyApp::Render()
{

	//
	// 0. Setup 
	//

	glm::mat4 proj = m_camera.GetProj();
	glm::mat4 view = m_camera.GetViewMatrix();

	// Check if camera has changed and reset accumulation if necessary
	bool cameraChanged = (m_lastTickView != view);
	m_lastTickView = view;

	if (!m_TimeFrozen || cameraChanged) {
		m_AccumulationFrameCounter = 0;
	}

	// If time is frozen and camera hasn't changed, we can accumulate frames
	if (m_TimeFrozen && !cameraChanged) {
		m_AccumulationFrameCounter++;

		proj = GetRandOffsetProj(proj);
	}
	else {
		m_AccumulationFrameCounter = 1;
	}

	glm::mat4 VP = proj * view;
	glm::mat4 invVP = glm::inverse(VP);

	//
	// 1. Calculate shadow maps
	//

	for (auto& light : m_lightSources)
	{
		light.m_currentTick++;
		if (light.state == FBO_NOT_BOUND)
		{
			InitLightFBO(light);
		}

		// Directional light
		if (light.m_lightPosition.w == 0.0f && light.m_castShadow && light.state == INITIALIZED)
		{
			glm::mat4 lightP = glm::ortho(-20.0f, 20.0f, -20.0f, 20.0f, 1.0f, 50.0f);
			glm::mat4 lightV = glm::lookAt(glm::vec3(light.m_lightPosition), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
			light.m_lightVP = lightP * lightV;

			// config render pass
			RenderPassConfig shadowPass = {};
			shadowPass.fboID = light.m_shadowFBO;
			shadowPass.viewportWidth = light.m_shadowMapSize;
			shadowPass.viewportHeight = light.m_shadowMapSize;
			shadowPass.depthTest = true;
			shadowPass.depthWrite = true;
			shadowPass.clearDepth = true;
			shadowPass.clearColor = false;

			SetRenderPass(shadowPass);

			glUseProgram(m_shadow_dir_programID);
			SetUniforms("lightVP", light.m_lightVP);

			// Draw objects in the scene from light's view
			for (const auto& obj : m_sceneObjects)
			{
				if (!obj.m_castShadow) continue;

				glUniformMatrix4fv(ul("world"), 1, GL_FALSE, glm::value_ptr(obj.m_worldTransform));
				glBindVertexArray(obj.m_mesh.vaoID);
				glDrawElements(GL_TRIANGLES, obj.m_mesh.count, GL_UNSIGNED_INT, 0);
			}
		}
		// Point light
		else if (light.m_lightPosition.w == 1.0f && light.m_castShadow && light.state == INITIALIZED)
		{
			float z_far = 50.0f;

			int face = (light.m_currentTick / light.m_updateInterval) % 6;

			// bind the current indexed face of cubemap to fbo
			glNamedFramebufferTextureLayer(light.m_shadowFBO, GL_DEPTH_ATTACHMENT, light.m_shadowTexID, 0, face);

			// Cubemap's i-th face orientation (with flipped y axis)
			glm::vec3 cubemap_dir[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
			glm::vec3 cubemap_up[6] = { {0,-1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}, {0,-1,0}, {0,-1,0} };

			glm::mat4 lightP = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, z_far);
			glm::vec3 lightPos = glm::vec3(light.m_lightPosition);
			glm::mat4 lightV = glm::lookAt(lightPos, lightPos + cubemap_dir[face], cubemap_up[face]);

			light.m_lightVP = lightP * lightV;

			RenderPassConfig shadowPass = {};
			shadowPass.fboID = light.m_shadowFBO;
			shadowPass.viewportWidth = light.m_shadowMapSize;
			shadowPass.viewportHeight = light.m_shadowMapSize;
			shadowPass.depthTest = true;
			shadowPass.depthWrite = true;
			shadowPass.clearDepth = true;

			SetRenderPass(shadowPass);

			glUseProgram(m_shadow_omni_programID);
			SetUniforms("lightVP", light.m_lightVP, 
						"lightPos", lightPos, 
						"z_far", z_far);

			for (const auto& obj : m_sceneObjects)
			{
				if (!obj.m_castShadow) continue;

				glUniformMatrix4fv(ul("world"), 1, GL_FALSE, glm::value_ptr(obj.m_worldTransform));
				glBindVertexArray(obj.m_mesh.vaoID);
				glDrawElements(GL_TRIANGLES, obj.m_mesh.count, GL_UNSIGNED_INT, 0);
			}
		}
	}

	//
	// 2. Render geometry into G-buffer
	//

	RenderPassConfig geomPass = {};
	geomPass.fboID = m_geometry_fboID;
	geomPass.viewportWidth = m_render_w;
	geomPass.viewportHeight = m_render_h;
	geomPass.depthTest = true;
	geomPass.depthWrite = true;
	geomPass.clearColor = true;
	geomPass.clearDepth = true;
	geomPass.clearColorValue = glm::vec4(0.125f, 0.25f, 0.5f, 1.0f);

	SetRenderPass(geomPass);

	if (m_wireframe_enable) glDisable(GL_CULL_FACE);
	glPolygonMode(GL_FRONT_AND_BACK, m_wireframe_enable ? GL_LINE : GL_FILL);

	glUseProgram(m_geom_pass_programID);
	SetUniforms(
		"textureImage", 0,
		"m_max_tess_level", m_max_tess_level,
		"m_min_tess_dist", m_min_tess_dist,
		"m_max_tess_dist", m_max_tess_dist,
		"VP", VP,
		"invVP", invVP,
		"m_cameraPos", m_camera.GetEye()
	);
	RenderGeometry(GL_PATCHES);

	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glEnable(GL_CULL_FACE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//
	// 3. Evaluate SSAO based on G-buffer depth values
	//

	RenderPassConfig ssaoPass = {};
	ssaoPass.fboID = m_ssao_fboID;
	ssaoPass.viewportWidth = m_render_w;
	ssaoPass.viewportHeight = m_render_h;
	ssaoPass.clearColor = true;
	ssaoPass.clearColorValue = glm::vec4(1.0f);

	SetRenderPass(ssaoPass);

	glBindTextureUnit(0, m_depthBufferID);
	glBindTextureUnit(1, m_normalBufferID);
	glBindTextureUnit(2, m_ssao_noise_TextureID);

	glUseProgram(m_ssao_programID);
	SetUniforms(
		"gDepth", 0, "gNormal", 1, "texNoise", 2,
		"view", view, "proj", proj, "invProj", glm::inverse(proj),
		"invVP", invVP, "resolution", glm::vec2((float)m_render_w, (float)m_render_h)
	);
	glUniform3fv(ul("samples"), m_ssaoKernel.size(), (const GLfloat*)m_ssaoKernel.data());
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// SSAO Blur

	RenderPassConfig ssaoBlurPass = {};
	ssaoBlurPass.fboID = m_ssao_blur_fboID;
	ssaoBlurPass.viewportWidth = m_render_w;
	ssaoBlurPass.viewportHeight = m_render_h;
	ssaoBlurPass.clearColor = true;
	ssaoBlurPass.clearColorValue = glm::vec4(1.0f);

	SetRenderPass(ssaoBlurPass);

	glBindTextureUnit(0, m_ssao_colorBufferID);
	glUseProgram(m_ssao_blur_programID);
	SetUniforms("ssaoTex", 0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	//
	// 4. Lighting pass
	//

	RenderPassConfig lightPass = {};
	lightPass.fboID = m_deferred_light_fboID;
	lightPass.viewportWidth = m_render_w;
	lightPass.viewportHeight = m_render_h;
	lightPass.clearColor = true;
	lightPass.clearColorValue = glm::vec4(0.125f, 0.25f, 0.5f, 1.0f);
	lightPass.blend = true;
	lightPass.blendSrc = GL_ONE; // Additive blending
	lightPass.blendDst = GL_ONE;

	SetRenderPass(lightPass);

	glBindTextureUnit(0, m_diffuseBufferID);
	glBindTextureUnit(1, m_normalBufferID);
	glBindTextureUnit(2, m_depthBufferID);
	glBindTextureUnit(3, m_ssao_blur_colorBufferID);
	glBindSampler(0, 0);

	glUseProgram(m_deferred_pass_programID);
	SetUniforms("VP", VP, "invVP", invVP, "m_cameraPos", m_camera.GetEye(), "ssaoTex", 3);

	// Accumulate lights
	bool first = true;
	for (auto& light : m_lightSources)
	{
		if (first) {
			glBlendFunc(GL_ONE, GL_ZERO);
			first = false;
		}
		else {
			glBlendFunc(GL_ONE, GL_ONE);
		}

		BindLightSource(light);
		BindMaterial(m_defaultMat);

		if (light.m_castShadow)
		{
			SetUniforms("hasShadow", 1);

			if (light.m_lightPosition.w == 0.0f) // Directional light
			{
				glBindTextureUnit(4, light.m_shadowTexID);
				SetUniforms("lightVP", light.m_lightVP, 
							"shadowTex", 4, 
							"isPointLight", 0);
			}
			else // Point light
			{
				glBindTextureUnit(5, light.m_shadowTexID);
				SetUniforms("shadowCubeTex", 5, 
							"z_far", 50.0f, 
							"isPointLight", 1);
			}
		}
		else
		{
			SetUniforms("hasShadow", 0); // no shadow
		}

		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	//
	// 5. SSR Pass
	//

	RenderPassConfig ssrPass = {};
	ssrPass.fboID = m_ssr_fboID;
	ssrPass.viewportWidth = m_render_w;
	ssrPass.viewportHeight = m_render_h;
	ssrPass.clearColor = true;
	ssrPass.clearColorValue = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	SetRenderPass(ssrPass);

	glBindTextureUnit(0, m_diffuseBufferID);
	glBindTextureUnit(1, m_normalBufferID);
	glBindTextureUnit(2, m_depthBufferID);
	glBindTextureUnit(3, m_deferred_light_colorBufferID);

	glUseProgram(m_ssr_programID);
	SetUniforms(
		"gDiffuse", 0, "gNormal", 1, "gDepth", 2, "gLight", 3,
		"proj", proj, "view", view, "invProj", glm::inverse(proj),
		"invVP", invVP, "resolution", glm::vec2((float)m_render_w, (float)m_render_h)
	);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	//
	// 6. Accumulation pass
	//

	// Copy the light pass result to the accumulation buffer
	// important: we don't clear the previous frame, because we want to accumulate the frames
	RenderPassConfig accumPass = {};
	accumPass.fboID = m_accum_fboID;
	accumPass.viewportWidth = m_render_w;
	accumPass.viewportHeight = m_render_h;
	accumPass.clearColor = false;
	accumPass.blend = true;
	accumPass.blendSrc = GL_CONSTANT_COLOR;
	accumPass.blendDst = GL_ONE_MINUS_CONSTANT_COLOR;

	SetRenderPass(accumPass);

	// Draw this frame's light pass result to the accumulation buffer
	float w = 1.0f / static_cast<float>(m_AccumulationFrameCounter);
	glBlendColor(w, w, w, 1.0f);

	glBindTextureUnit(0, m_ssr_colorBufferID);
	glUseProgram(m_postprocess_programID);
	SetUniforms("channel_c0", 0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	//
	// 7. Postprocess pass
	//

	// Draw the final image to the default framebuffer (screen)
	// We draw from the accumulation buffer onto backbuffer

	RenderPassConfig screenPass = {};
	screenPass.fboID = 0;
	screenPass.viewportWidth = m_w;
	screenPass.viewportHeight = m_h;
	screenPass.clearColor = true;
	screenPass.clearColorValue = glm::vec4(0.125f, 0.25f, 0.5f, 1.0f);

	SetRenderPass(screenPass);

	glBindTextureUnit(0, m_accum_colorBufferID);
	glBindTextureUnit(1, 0);
	glBindTextureUnit(2, 0);

	glUseProgram(m_postprocess_programID);
	SetUniforms("channel_c0", 0);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	// Clean up buffers
	glBindSampler( 0, 0 );
	glUseProgram(0);
	glBindVertexArray(0);

	// unused
	//DrawAxes();
}

void CMyApp::RenderGUI()
{
	// ImGui DemoWindow
	//ImGui::ShowDemoWindow();

	ImGui::SetNextWindowSize(ImVec2(455, 60), ImGuiCond_FirstUseEver);
	if (ImGui::Begin("ImGui example"))
	{
		if (ImGui::CollapsingHeader("Options"))
		{
			static float refresh_time = 0.1f;
			static float timer = 0;
			static int   frameCount = 0;
			static float fps = 0;

			timer += static_cast<float>(m_DeltaTimeInSec);
			++frameCount;
			if (timer > refresh_time) {
				fps = frameCount / timer;
				timer = 0;
				frameCount = 0;
			}
			ImGui::Text("FPS: %d", static_cast<int>(fps));

			ImGui::SliderFloat("Refresh time", &refresh_time, 0.01f, 1.0f);
			ImGui::SliderFloat("Rendering resolution", &m_renderResolution, 0.1f, 1.0f);
		}


		if (ImGui::CollapsingHeader("Tessellation"))
		{
			ImGui::Checkbox("Enable wireframe mode?", &m_wireframe_enable);
			ImGui::Separator();
			ImGui::SliderFloat("Max Tessellation level", &m_max_tess_level, 0.0f, 16.0f);
			ImGui::SliderFloat("Min Tessellation distance", &m_min_tess_dist, 0.1f, m_max_tess_dist);
			ImGui::SliderFloat("Max Tessellation distance", &m_max_tess_dist, 1.0f, 100.0f);
		}

		if (ImGui::CollapsingHeader("Time"))
		{
			ImGui::Checkbox("Freeze time?", &m_TimeFrozen);
			ImGui::Text("Elapsed time: %.2f sec", m_ElapsedTimeInSec);
			ImGui::Text("Delta time: %.5f sec", m_DeltaTimeInSec);
			ImGui::Text("Accumulated frames: %d", m_AccumulationFrameCounter);
		}

		if (ImGui::CollapsingHeader("Lighting"))
		{
			ImGui::Text("Global");
			ImGui::SliderFloat("Ambient Light", &m_ambient, 0.0f, 1.0f);
			ImGui::SliderFloat("Diffuse Light", &m_diffuse, 0.0f, 1.0f);
			ImGui::SliderFloat("Specular Light", &m_specular, 0.0f, 1.0f);
			ImGui::Separator();

			if (ImGui::Button("Add Point Light")) {
				m_lightSources.push_back(CreateLight(glm::vec3(0, 5, 0), true, glm::vec3(0), glm::vec3(1), glm::vec3(1), true, 512, 6));
			}
			ImGui::SameLine();
			if (ImGui::Button("Add Directional Light")) {
				m_lightSources.push_back(CreateLight(glm::vec3(1, 1, 1), false, glm::vec3(0.1), glm::vec3(0.8), glm::vec3(0.8), true, 1024, 1));
			}

			ImGui::Separator();

			for (size_t i = 0; i < m_lightSources.size(); )
			{
				auto& light = m_lightSources[i];
				ImGui::PushID(static_cast<int>(i));

				std::string lightName = (light.m_lightPosition.w == 1.0f) ? "Point Light " + std::to_string(i) : "Directional Light " + std::to_string(i);

				bool deleteLight = false;

				if (ImGui::TreeNode(lightName.c_str()))
				{
					// Pos / Dir
					if (light.m_lightPosition.w == 1.0f) {
						ImGui::DragFloat3("Position", glm::value_ptr(light.m_lightPosition), 0.1f);
					}
					else {
						ImGui::DragFloat3("Direction", glm::value_ptr(light.m_lightPosition), 0.05f);
					}

					// Colors
					ImGui::ColorEdit3("Ambient (La)", glm::value_ptr(light.m_La));
					ImGui::ColorEdit3("Diffuse (Ld)", glm::value_ptr(light.m_Ld));
					ImGui::ColorEdit3("Specular (Ls)", glm::value_ptr(light.m_Ls));

					// Shadow
					ImGui::Checkbox("Cast Shadow", &light.m_castShadow);

					if (light.m_castShadow)
					{
						ImGui::SliderInt("Update Interval (Frames)", &light.m_updateInterval, 1, 60);
					}

					// Delete light
					if (ImGui::Button("Delete Light", ImVec2(-1, 0))) {
						deleteLight = true;
					}

					ImGui::TreePop();
				}
				ImGui::PopID();

				// cleanup on deletion
				if (deleteLight) {
					CleanLightFBO(light);
					m_lightSources.erase(m_lightSources.begin() + i);
				}
				else {
					++i;
				}
			}

			if (ImGui::CollapsingHeader("Scene Objects (Shadows)"))
			{
				for (size_t i = 0; i < m_sceneObjects.size(); ++i)
				{
					auto& obj = m_sceneObjects[i];
					ImGui::PushID(static_cast<int>(i));
					if (ImGui::TreeNode((std::string("Object ") + std::to_string(i)).c_str()))
					{
						ImGui::Checkbox("Cast Shadow", &obj.m_castShadow);
						ImGui::Checkbox("Receive Shadow", &obj.m_receiveShadow);
						ImGui::TreePop();
					}
					ImGui::PopID();
				}
			}


		}
	} //window
	ImGui::End();
}

// https://wiki.libsdl.org/SDL3/SDL_KeyboardEvent
// https://wiki.libsdl.org/SDL3/SDL_Keysym
// https://wiki.libsdl.org/SDL3/SDL_Keycode
// https://wiki.libsdl.org/SDL3/SDL_Keymod

void CMyApp::KeyboardDown(const SDL_KeyboardEvent& key)
{
	if (!key.repeat) // Triggers only once when held
	{
		if (key.key == SDLK_F5 && key.mod & SDL_KMOD_CTRL) // CTRL + F5
		{
			CleanShaders();
			InitShaders();
		}
		if (key.key == SDLK_F1) // F1
		{
			GLint polygonModeFrontAndBack[2] = {};
			// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGet.xhtml
			glGetIntegerv(GL_POLYGON_MODE, polygonModeFrontAndBack); // Query the current polygon mode. It gives the front and back modes separately.
			GLenum polygonMode = (polygonModeFrontAndBack[0] != GL_FILL ? GL_FILL : GL_LINE); // Switch between FILL and LINE
			// https://registry.khronos.org/OpenGL-Refpages/gl4/html/glPolygonMode.xhtml
			glPolygonMode(GL_FRONT_AND_BACK, polygonMode); // Set the new polygon mode
		}
	}
	m_cameraManipulator.KeyboardDown(key);
}

void CMyApp::KeyboardUp(const SDL_KeyboardEvent& key)
{
	m_cameraManipulator.KeyboardUp(key);
}

// https://wiki.libsdl.org/SDL3/SDL_MouseMotionEvent

void CMyApp::MouseMove(const SDL_MouseMotionEvent& mouse)
{
	m_cameraManipulator.MouseMove(mouse);
}

// https://wiki.libsdl.org/SDL3/SDL_MouseButtonEvent

void CMyApp::MouseDown(const SDL_MouseButtonEvent& mouse)
{
}

void CMyApp::MouseUp(const SDL_MouseButtonEvent& mouse)
{
}

// https://wiki.libsdl.org/SDL3/SDL_MouseWheelEvent

void CMyApp::MouseWheel(const SDL_MouseWheelEvent& wheel)
{
	m_cameraManipulator.MouseWheel(wheel);
}

// New window size
void CMyApp::Resize(int _w, int _h)
{
	m_w = _w;
	m_h = _h;

	m_render_w = std::max(1, (int)(_w * m_renderResolution));
	m_render_h = std::max(1, (int)(_h * m_renderResolution));

	m_camera.SetAspect(static_cast<float>(_w) / _h);

	CleanFBOResources();
	InitFBOResources(m_render_w, m_render_h);
}

// Other SDL events
// https://wiki.libsdl.org/SDL3/SDL_Event

void CMyApp::OtherEvent(const SDL_Event& ev)
{
}