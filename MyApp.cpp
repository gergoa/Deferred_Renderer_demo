#include "MyApp.h"
#include "SDL_GLDebugMessageCallback.h"
#include "GLUtils.hpp"
#include "ObjParser.h"
#include "ProgramBuilder.h"

#include <imgui.h>
#include <algorithm>
#include <iostream>
#include <random>

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
	InitAxesShader();
}

void CMyApp::CleanShaders()
{
	glDeleteProgram(m_geom_pass_programID);
	glDeleteProgram(m_deferred_pass_programID);
	glDeleteProgram(m_postprocess_programID);
	glDeleteProgram(m_ssao_programID);
	glDeleteProgram(m_ssao_blur_programID);
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

void GenSSAOKernel(std::vector<glm::vec3>& ssaoKernel, std::vector<glm::vec3>& ssaoNoise, const int samples, const int rotations)
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


		float scale = (float)i / 64.0;
		scale = lerp(0.1f, 1.0f, scale * scale);
		sample *= scale;
		ssaoKernel.push_back(sample);
	}

	ssaoNoise.clear();

	for (int i = 0; i < rotations; ++i)
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
	MeshObject<Vertex> suzanneMeshCPU = ObjParser::parse("Assets/Suzanne.obj");
	m_Suzanne = CreateGLObjectFromMesh(suzanneMeshCPU, vertexAttribList);

	// Bird
	MeshObject<Vertex> birdMeshCPU = ObjParser::parse("Assets/Bird_v1.obj");
	m_Bird = CreateGLObjectFromMesh(birdMeshCPU, vertexAttribList);

	// Wall
	MeshObject<Vertex> wallMeshCPU = ObjParser::parse("Assets/Wall.obj");
	m_Wall = CreateGLObjectFromMesh(wallMeshCPU, vertexAttribList);

	// Mirror
	MeshObject<Vertex> mirrorMeshCPU = ObjParser::parse("Assets/Mirror.obj");
	m_Mirror = CreateGLObjectFromMesh(mirrorMeshCPU, vertexAttribList);
}

void CMyApp::CleanGeometry()
{
	CleanOGLObject(m_Suzanne);
	CleanOGLObject(m_Bird);
	CleanOGLObject(m_Wall);
	CleanOGLObject(m_Mirror);
	glDeleteVertexArrays(1, &m_emptyVAO);
}

void CMyApp::InitLightSources()
{
	// Lights in a hexagonal shape
	for (int i = 0; i < 6; ++i)
	{
		float angle = glm::radians(i * 60.0f);
		float x = 6.0f * cos(angle);
		float z = 6.0f * sin(angle);
		m_lightSources.push_back({	glm::vec4(x, -2.0f, z, 1.0f), 
									glm::vec3(0.0f), 
									glm::vec3(abs(cos(angle)), abs(sin(angle)), abs(cos(angle + 1.0f))),
									glm::vec3(0.0f) });
	}

	// Directional sunlight
	m_lightSources.push_back({glm::vec4(0.0f, 1.0f, 1.0f, 0.0f),
								glm::vec3(0.25f),
								glm::vec3(0.0f),
								glm::vec3(0.0f) });
}


void CMyApp::InitMaterials()
{
	m_materials.push_back({ glm::vec3(1.0f), glm::vec3(1.0f), glm::vec3(1.0f), 20.0f });
}

void CMyApp::InitTextures()
{
	glCreateSamplers( 1, &m_SamplerID );
	glSamplerParameteri( m_SamplerID, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glSamplerParameteri( m_SamplerID, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glSamplerParameteri( m_SamplerID, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	glSamplerParameteri( m_SamplerID, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

	ImageRGBA metalImage = ImageFromFile( "Assets/metal.png" );

	glCreateTextures( GL_TEXTURE_2D, 1, &m_metalTextureID );
	glTextureStorage2D( m_metalTextureID, NumberOfMIPLevels( metalImage ), GL_RGBA8, metalImage.width, metalImage.height );
	glTextureSubImage2D( m_metalTextureID, 0, 0, 0, metalImage.width, metalImage.height, GL_RGBA, GL_UNSIGNED_BYTE, metalImage.data() );

	glGenerateTextureMipmap( m_metalTextureID );


	ImageRGBA birdImage = ImageFromFile("Assets/Bird_v1.jpg");

	glCreateTextures(GL_TEXTURE_2D, 1, &m_birdTextureID);
	glTextureStorage2D(m_birdTextureID, NumberOfMIPLevels(birdImage), GL_RGBA8, birdImage.width, birdImage.height);
	glTextureSubImage2D(m_birdTextureID, 0, 0, 0, birdImage.width, birdImage.height, GL_RGBA, GL_UNSIGNED_BYTE, birdImage.data());

	glGenerateTextureMipmap(m_birdTextureID);


	ImageRGBA wallImage = ImageFromFile("Assets/wall.jpg");
	glCreateTextures(GL_TEXTURE_2D, 1, &m_wallTextureID);
	glTextureStorage2D(m_wallTextureID, NumberOfMIPLevels(wallImage), GL_RGBA8, wallImage.width, wallImage.height);
	glTextureSubImage2D(m_wallTextureID, 0, 0, 0, wallImage.width, wallImage.height, GL_RGBA, GL_UNSIGNED_BYTE, wallImage.data());
	glGenerateTextureMipmap(m_wallTextureID);
}

void CMyApp::CleanTextures()
{
	glDeleteTextures(1, &m_metalTextureID);
	glDeleteTextures(1, &m_birdTextureID);
	glDeleteTextures(1, &m_wallTextureID);

	glDeleteSamplers( 1, &m_SamplerID );
}

void CMyApp::InitFrameBufferObjects()
{
	// FBO létrehozása
	glCreateFramebuffers(1, &m_geometry_fboID);
	glCreateFramebuffers(1, &m_accum_fboID);
	glCreateFramebuffers(1, &m_final_light_fboID);
	glCreateFramebuffers(1, &m_ssao_fboID);
	glCreateFramebuffers(1, &m_ssao_blur_fboID);
}

void CMyApp::CleanFrameBufferObjects()
{
	glDeleteFramebuffers( 1, &m_geometry_fboID );
	glDeleteFramebuffers(1, &m_accum_fboID);
	glDeleteFramebuffers( 1, &m_final_light_fboID );
	glDeleteFramebuffers(1, &m_ssao_fboID);
	glDeleteFramebuffers(1, &m_ssao_blur_fboID);

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
	glCreateTextures(GL_TEXTURE_2D, 1, &m_final_light_colorBufferID);
	glTextureStorage2D(m_final_light_colorBufferID, 1, GL_RGBA32F, width, height);
	glTextureParameteri(m_final_light_colorBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_final_light_colorBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_final_light_fboID, GL_COLOR_ATTACHMENT0, m_final_light_colorBufferID, 0);

	const GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0 };

	glNamedFramebufferDrawBuffers(m_final_light_fboID, 1, drawBuffers);

	// Completeness check
	GLenum status = glCheckNamedFramebufferStatus(m_final_light_fboID, GL_FRAMEBUFFER);
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
	glDeleteTextures(1, &m_final_light_colorBufferID);
}

void CMyApp::InitFBOResources(int width, int height)
{
	InitGeometryFBO(width, height);
	InitAccumFBO(width, height);
	InitSSAO_FBO(width, height);
	InitLightPassFBO(width, height);
}

void CMyApp::CleanFBOResources()
{
	CleanGeometryFBO();
	CleanAccumFBO();
	CleanSSAO_FBO();
	CleanLightPassFBO();
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
	for (size_t i = 0; i < m_lightSources.size(); ++i)
	{
		if (i < 6)
		{
			float angle = glm::radians(m_ElapsedTimeInSec * 30.0f + i * 60.0f);
			float x = 6.0f * cos(angle);
			float z = 6.0f * sin(angle);
			m_lightSources[i].m_lightPosition = glm::vec4(x, -2.0f, z, 1.0f);
			m_lightSources[i].m_Ld = { abs(cos(angle)), abs(sin(angle)), abs(cos(angle + 1.0f)) };
		}
	}
	m_birdWorldTransform *= glm::rotate<float>(m_DeltaTimeInSec, glm::vec3(0,0,1));

	m_materials[0] = {
		glm::vec3(m_ambient), glm::vec3(m_diffuse), glm::vec3(m_specular), 20.0f
	};
}

void CMyApp::RenderGeometry(GLenum primitiveType)
{
	glBindSampler(0, m_SamplerID);
	glBindVertexArray(m_Suzanne.vaoID);

	if (primitiveType == GL_PATCHES)
	{
		glPatchParameteri(GL_PATCH_VERTICES, 3);
	}

	// Suzanne
	glBindTextureUnit(0, m_metalTextureID);

	const glm::mat4& suzanneWorld = m_suzanneWorldTransform;
    glUniformMatrix4fv( ul( "world" ), 1, GL_FALSE, glm::value_ptr( suzanneWorld ) );
    glUniformMatrix4fv( ul( "worldIT" ), 1, GL_FALSE, glm::value_ptr( glm::transpose( glm::inverse( suzanneWorld ) ) ) );
    glDrawElements( primitiveType, m_Suzanne.count, GL_UNSIGNED_INT, 0 );

	// Bird
	glBindVertexArray(m_Bird.vaoID);
	glBindTextureUnit(0, m_birdTextureID);

	const glm::mat4 birdWorld = m_birdWorldTransform;
	glUniformMatrix4fv(ul("world"), 1, GL_FALSE, glm::value_ptr(birdWorld));
	glUniformMatrix4fv(ul("worldIT"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(birdWorld))));
	glDrawElements(primitiveType, m_Bird.count, GL_UNSIGNED_INT, 0);

	// Wall
	glBindVertexArray(m_Wall.vaoID);
	glBindTextureUnit(0, m_wallTextureID);

	for (int i = 0; i < 3; ++i)
	{
		const glm::mat4 wallWorld = glm::translate(glm::vec3(i*7, -5, -10)) * glm::scale(glm::vec3(1, 1, 1));
		glUniformMatrix4fv(ul("world"), 1, GL_FALSE, glm::value_ptr(wallWorld));
		glUniformMatrix4fv(ul("worldIT"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(wallWorld))));
		glDrawElements(primitiveType, m_Wall.count, GL_UNSIGNED_INT, 0);
	}

	// Mirror
	glBindVertexArray(m_Mirror.vaoID);
	glBindTextureUnit(0, 0); 

	const glm::mat4 mirrorWorld = glm::translate(glm::vec3(0, -5, 0)) * glm::scale(glm::vec3(0.125, 0.3, 0.065));
	glUniformMatrix4fv(ul("world"), 1, GL_FALSE, glm::value_ptr(mirrorWorld));
	glUniformMatrix4fv(ul("worldIT"), 1, GL_FALSE, glm::value_ptr(glm::transpose(glm::inverse(mirrorWorld))));
	glDrawElements(primitiveType, m_Mirror.count, GL_UNSIGNED_INT, 0);

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

void CMyApp::Render()
{

	//
	// 0. Setup 
	//

	// reset state machine for good measure yay opengl
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

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
	// 1. Render geometry into G-buffer
	//

	glViewport(0, 0, m_render_w, m_render_h);

	glBindFramebuffer(GL_FRAMEBUFFER, m_geometry_fboID);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Wireframe mode
	if (m_wireframe_enable) glDisable(GL_CULL_FACE);

	glPolygonMode(GL_FRONT, m_wireframe_enable ? GL_LINE : GL_FILL);
	glPolygonMode(GL_BACK, m_wireframe_enable ? GL_LINE : GL_FILL);

	glUseProgram(m_geom_pass_programID);

	// Set uniforms for the geometry pass
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

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// TODO: SSAO Pass here

	glBindFramebuffer(GL_FRAMEBUFFER, m_ssao_fboID);
	glClear(GL_COLOR_BUFFER_BIT);

	glBindVertexArray(m_emptyVAO);

	// Input channels from G-buffer + noise
	glBindTextureUnit(0, m_depthBufferID);
	glBindTextureUnit(1, m_normalBufferID);
	glBindTextureUnit(2, m_ssao_noise_TextureID);

	glUseProgram(m_ssao_programID);

	SetUniforms(
		"gDepth", 0,
		"gNormal", 1,
		"texNoise", 2,
		"view", view,
		"proj", proj,
		"invProj", glm::inverse(proj),
		"invVP", invVP,
		"resolution", glm::vec2((float)m_render_w, (float)m_render_h)
	);
	glUniform3fv(ul("samples"), m_ssaoKernel.size(), (const GLfloat*)m_ssaoKernel.data());

	glBindVertexArray(m_emptyVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// SSAO Blur

	glBindFramebuffer(GL_FRAMEBUFFER, m_ssao_blur_fboID);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	glBindVertexArray(m_emptyVAO);

	// Input channel is ssao
	glBindTextureUnit(0, m_ssao_colorBufferID);

	glUseProgram(m_ssao_blur_programID);

	SetUniforms(
		"ssaoTex", 0
	);

	glBindVertexArray(m_emptyVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//
	// 2. Lighting pass
	//

	glBindFramebuffer(GL_FRAMEBUFFER, m_final_light_fboID);

	glClearColor(0.125f, 0.25f, 0.5f, 1.0f);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glUseProgram(m_deferred_pass_programID);
	SetUniforms(
		"VP", VP,
		"invVP", invVP,
		"m_cameraPos", m_camera.GetEye(),
		"ssaoTex", 3
	);

	glBindVertexArray(m_emptyVAO);

	// Input channels from G-buffer
	glBindTextureUnit(0, m_diffuseBufferID);
	glBindTextureUnit(1, m_normalBufferID);
	glBindTextureUnit(2, m_depthBufferID);
	glBindTextureUnit(3, m_ssao_blur_colorBufferID);
	
	glBindSampler(0, 0);

	// Accumulate light sources in backbuffer
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glDepthMask(GL_FALSE);

	bool first = true;
	for (auto& light : m_lightSources)
	{
		// First light overwrites the backbuffer, the others are added to it 
		if (first)
		{
			glBlendFunc(GL_ONE, GL_ZERO);
			first = false;
		}
		else
		{
			glBlendFunc(GL_ONE, GL_ONE);
		}

		CMyApp::BindLightSource(light);
		CMyApp::BindMaterial(m_materials[0]);

		// Draw a full-screen quad
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}
	
	// turn off blending
	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	//
	// 3. Accumulation pass
	//

	// Copy the light pass result to the accumulation buffer
	// important: we don't clear the previous frame, because we want to accumulate the frames
	glBindFramebuffer(GL_FRAMEBUFFER, m_accum_fboID);

	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);

	float w = 1.0f / static_cast<float>(m_AccumulationFrameCounter);
	glBlendColor(w, w, w, 1.0f);

	glBlendFunc(GL_CONSTANT_COLOR, GL_ONE_MINUS_CONSTANT_COLOR);

	// Draw this frame's light pass result to the accumulation buffer

	glUseProgram(m_postprocess_programID);
	glBindTextureUnit(0, m_final_light_colorBufferID);
	SetUniforms("channel_c0", 0);
	glBindVertexArray(m_emptyVAO);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	

	// turn off blending
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);


	//
	// 4. Postprocess pass
	//

	// Draw the final image to the default framebuffer (screen)
	// We draw from the accumulation buffer onto backbuffer

	glViewport(0, 0, m_w, m_h);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	glUseProgram(m_postprocess_programID);

	// Input channel is the light pass color buffer, bind others to 0
	glBindTextureUnit(0, m_accum_colorBufferID);
	glBindTextureUnit(1, 0);
	glBindTextureUnit(2, 0);

	SetUniforms("channel_c0", 0);

	glBindVertexArray(m_emptyVAO);

	// Draw full-screen quad
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
			ImGui::SliderFloat("Ambient Light", &m_ambient, 0.0f, 1.0f);
			ImGui::SliderFloat("Diffuse Light", &m_diffuse, 0.0f, 1.0f);
			ImGui::SliderFloat("Specular Light", &m_specular, 0.0f, 1.0f);

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