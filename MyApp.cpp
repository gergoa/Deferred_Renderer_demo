#include "MyApp.h"
#include "SDL_GLDebugMessageCallback.h"
#include "GLUtils.hpp"
#include "ObjParser.h"
#include "ProgramBuilder.h"

#include <imgui.h>
#include <iostream>
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
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/gbuffer.frag")
		.Link();

	m_deferred_pass_programID = glCreateProgram();
	ProgramBuilder{ m_deferred_pass_programID }
		.ShaderStage(GL_VERTEX_SHADER, "Shaders/fullscreen.vert")
		.ShaderStage(GL_FRAGMENT_SHADER, "Shaders/deferred.frag")
		.Link();

	InitAxesShader();
}

void CMyApp::CleanShaders()
{
	glDeleteProgram(m_geom_pass_programID);
	glDeleteProgram(m_deferred_pass_programID);
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
}

void CMyApp::CleanGeometry()
{
	CleanOGLObject(m_Suzanne);
	glDeleteVertexArrays(1, &m_emptyVAO);
}

void CMyApp::InitLightSources()
{
	// Lights in a hexagonal shape
	for (int i = 0; i < 6; ++i)
	{
		float angle = glm::radians(i * 60.0f);
		float x = 2.0f * cos(angle);
		float z = 2.0f * sin(angle);
		m_lightSources.push_back({	glm::vec4(x, 3.0f, z, 1.0f), 
									glm::vec3(0.0f), 
									glm::vec3(abs(cos(angle)), abs(sin(angle)), abs(cos(angle + 1.0f))),
									glm::vec3(0.0f) });
	}

	// Directional sunlight
	m_lightSources.push_back({ glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
								glm::vec3(0.35f),
								glm::vec3(1.0f),
								glm::vec3(0.5f) });
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
}

void CMyApp::CleanTextures()
{
	glDeleteTextures(1, &m_metalTextureID);

	glDeleteSamplers( 1, &m_SamplerID );
}

void CMyApp::InitFrameBufferObject()
{
	// FBO létrehozása
	glCreateFramebuffers(1, &m_frameBufferID);
}

void CMyApp::CleanFrameBufferObject()
{
	glDeleteFramebuffers( 1, &m_frameBufferID );
}

void CMyApp::InitFBOResources( int width, int height )
{
	// Setup the texture
	// We use texture because we will sample it later in the shader

	// Diffuse
	glCreateTextures( GL_TEXTURE_2D, 1, &m_diffuseBufferID );
	glTextureStorage2D( m_diffuseBufferID, 1, GL_RGBA8, width, height );
	glTextureParameteri(m_diffuseBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_diffuseBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_frameBufferID, GL_COLOR_ATTACHMENT0, m_diffuseBufferID, 0);


	// Normal
	glCreateTextures(GL_TEXTURE_2D, 1, &m_normalBufferID);
	glTextureStorage2D(m_normalBufferID, 1, GL_RGBA16_SNORM, width, height);
	glTextureParameteri(m_normalBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_normalBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_frameBufferID, GL_COLOR_ATTACHMENT1, m_normalBufferID, 0);


	// Depth
	glCreateTextures(GL_TEXTURE_2D, 1, &m_depthBufferID);
	glTextureStorage2D(m_depthBufferID, 1, GL_DEPTH_COMPONENT24, width, height);
	glTextureParameteri(m_depthBufferID, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(m_depthBufferID, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glNamedFramebufferTexture(m_frameBufferID, GL_DEPTH_ATTACHMENT, m_depthBufferID, 0);

	const GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

	glNamedFramebufferDrawBuffers(m_frameBufferID, 2, drawBuffers);

	// Completeness check
	GLenum status = glCheckNamedFramebufferStatus(m_frameBufferID, GL_FRAMEBUFFER);
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

void CMyApp::CleanFBOResources()
{
	glDeleteTextures(1, &m_depthBufferID);
	glDeleteTextures(1, &m_diffuseBufferID);
	glDeleteTextures(1, &m_normalBufferID);
}

bool CMyApp::Init()
{
	SetupDebugCallback();

	// Set a bluish clear color
	// glClear() will use this for clearing the color buffer.
	glClearColor(0.125f, 0.25f, 0.5f, 1.0f);

	InitShaders();
	InitGeometry();
	InitLightSources();
	InitMaterials();
	InitTextures();
	InitFrameBufferObject();

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
	CleanFrameBufferObject();
}

void CMyApp::Update(const SUpdateInfo& updateInfo)
{
	m_cameraManipulator.Update(updateInfo.DeltaTimeInSec);
    m_ElapsedTimeInSec = updateInfo.ElapsedTimeInSec;

	// Spin point lights
	for (size_t i = 0; i < m_lightSources.size(); ++i)
	{
		if (i < 6)
		{
			float angle = glm::radians(m_ElapsedTimeInSec * 30.0f + i * 60.0f);
			float x = 2.0f * cos(angle);
			float z = 2.0f * sin(angle);
			m_lightSources[i].m_lightPosition = glm::vec4(x, 3.0f, z, 1.0f);
			m_lightSources[i].m_Ld = { abs(cos(angle)), abs(sin(angle)), abs(cos(angle + 1.0f)) };
		}
	}

}

void CMyApp::RenderGeometry()
{
	glUseProgram(m_geom_pass_programID);
	glUniform1i( ul("textureImage"),0);
	glBindTextureUnit( 0, m_metalTextureID );
	glBindSampler( 0, m_SamplerID );

	glUniformMatrix4fv(ul("VP"), 1, GL_FALSE, glm::value_ptr(m_camera.GetViewProj()));

	glBindVertexArray(m_Suzanne.vaoID);

	// Suzanne
	const glm::mat4& suzanneWorld = m_suzanneWorldTransform;
    glUniformMatrix4fv( ul( "world" ), 1, GL_FALSE, glm::value_ptr( suzanneWorld ) );
    glUniformMatrix4fv( ul( "worldIT" ), 1, GL_FALSE, glm::value_ptr( glm::transpose( glm::inverse( suzanneWorld ) ) ) );
    glDrawElements( GL_TRIANGLES, m_Suzanne.count, GL_UNSIGNED_INT, 0 );
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

void CMyApp::Render()
{
	//
	// Render geometry into framebuffer
	//

	glBindFramebuffer(GL_FRAMEBUFFER, m_frameBufferID);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glUseProgram(m_geom_pass_programID);

	RenderGeometry();

	//
	// Draw a full-screen quad and stretch the image rendered before as texture
	//

	// Step back to default FBO- (=backbuffer)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);

	glUseProgram(m_deferred_pass_programID);

	glBindVertexArray( m_emptyVAO );

	glBindTextureUnit( 0, m_diffuseBufferID);
	glBindTextureUnit( 1, m_normalBufferID);
	glBindTextureUnit( 2, m_depthBufferID);
	glBindSampler( 0, m_SamplerID );


	glUniformMatrix4fv(ul("invVP"), 1, GL_FALSE, glm::value_ptr(glm::inverse(m_camera.GetViewProj())));
	glUniform3fv(ul("m_cameraPos"), 1, glm::value_ptr(m_camera.GetEye()));

	// Accumulate light sources in backbuffer
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
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

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);

	// Clean up buffers
	glBindSampler( 0, 0 );
	glUseProgram(0);
	glBindVertexArray(0);


	DrawAxes();
}

void CMyApp::RenderGUI()
{
	// ImGui DemoWindow
	ImGui::ShowDemoWindow();

	ImGui::SetNextWindowSize(ImVec2(455, 60), ImGuiCond_FirstUseEver);
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
	glViewport(0, 0, _w, _h);
	m_camera.SetAspect(static_cast<float>(_w) / _h);

	// When we resize we need to remake the framebuffer with the new size,
	// because now we want the two framebuffer (default,ours) to be the same resolution
    CleanFBOResources();
	InitFBOResources(_w, _h);
}

// Other SDL events
// https://wiki.libsdl.org/SDL3/SDL_Event

void CMyApp::OtherEvent(const SDL_Event& ev)
{
}