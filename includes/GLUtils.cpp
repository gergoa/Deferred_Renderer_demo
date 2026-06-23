#include "GLUtils.hpp"

#include <stdio.h>
#include <string>
#include <iostream>
#include <fstream>

#include <SDL3_image/SDL_image.h>

#include "ObjParser.h"

/* 

 Az http://www.opengl-tutorial.org/ oldal alapján.
*/

static void loadShaderCode( std::string& shaderCode, const std::filesystem::path& _fileName )
{
	// shaderkód betöltése _fileName fájlból 
	shaderCode = "";

	// _fileName megnyitása 
	std::ifstream shaderStream( _fileName );
	if ( !shaderStream.is_open() )
	{
		SDL_LogMessage( SDL_LOG_CATEGORY_ERROR,
						SDL_LOG_PRIORITY_ERROR,
						"Error while opening shader code file %s!", _fileName.string().c_str());
		return;
	}

	// file tartalmának betöltése a shaderCode string-be 
	std::string line = "";
	while ( std::getline( shaderStream, line ) )
	{
		shaderCode += line + "\n";
	}

	shaderStream.close();
}

GLuint AttachShader( const GLuint programID, GLenum shaderType, const std::filesystem::path& _fileName )
{
	// shaderkód betoltese _fileName fájlból 
    std::string shaderCode;
    loadShaderCode( shaderCode, _fileName );

    return AttachShaderCode( programID, shaderType, shaderCode );
}

GLuint AttachShaderCode( const GLuint programID, GLenum shaderType, std::string_view shaderCode )
{
	if (programID == 0)
	{
		SDL_LogMessage(SDL_LOG_CATEGORY_ERROR,
						SDL_LOG_PRIORITY_ERROR,
						"Program needs to be inited before loading!");
		return 0;
	}

	// shader létrehozása 
	GLuint shaderID = glCreateShader( shaderType );

	// kód hozzárendelése a shader-hez 
	const char* sourcePointer = shaderCode.data();
	GLint sourceLength = static_cast<GLint>( shaderCode.length() );

	glShaderSource( shaderID, 1, &sourcePointer, &sourceLength );

	// shader lefordítása 
	glCompileShader( shaderID );

	// ellenőrizzük, hogy minden rendben van-e 
	GLint result = GL_FALSE;
	int infoLogLength;

	// fordítas státuszának lekérdezése 
	glGetShaderiv( shaderID, GL_COMPILE_STATUS, &result );
	glGetShaderiv( shaderID, GL_INFO_LOG_LENGTH, &infoLogLength );

	if ( GL_FALSE == result || infoLogLength != 0 )
	{
		// hibaüzenet elkérése es kiírasa 
		std::string ErrorMessage( infoLogLength, '\0' );
		glGetShaderInfoLog( shaderID, infoLogLength, NULL, ErrorMessage.data() );

		SDL_LogMessage( SDL_LOG_CATEGORY_ERROR,
						( result ) ? SDL_LOG_PRIORITY_WARN : SDL_LOG_PRIORITY_ERROR,
						"[glCompileShader]: %s", ErrorMessage.data() );
	}

	// shader hozzárendelése a programhoz 
	glAttachShader( programID, shaderID );

	return shaderID;

}

void LinkProgram( const GLuint programID, bool OwnShaders )
{
	// illesszük össze a shadereket (kimenő-bemenő változók összerendelése stb.)
	glLinkProgram( programID );

	// linkelés ellenőrzése 
	GLint infoLogLength = 0, result = 0;

	glGetProgramiv( programID, GL_LINK_STATUS, &result );
	glGetProgramiv( programID, GL_INFO_LOG_LENGTH, &infoLogLength );
	if ( GL_FALSE == result || infoLogLength != 0 )
	{
		std::string ErrorMessage( infoLogLength, '\0' );
		glGetProgramInfoLog( programID, infoLogLength, nullptr, ErrorMessage.data() );
		SDL_LogMessage( SDL_LOG_CATEGORY_ERROR,
						( result ) ? SDL_LOG_PRIORITY_WARN : SDL_LOG_PRIORITY_ERROR,
						"[glLinkProgram]: %s", ErrorMessage.data() );
	}

	// Ebben az esetben a program objektumhoz tartozik a shader objektum.
	// Vagyis a shader objektumokat ki tudjuk "törölni".
    // Szabvány szerint (https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDeleteShader.xhtml)
    // a shader objektumok csak akkor törlődnek, ha nincsennek hozzárendelve egyetlen program objektumhoz sem.
	// Vagyis mikor a program objektumot töröljük, akkor törlődnek a shader objektumok is.
	if ( OwnShaders )
	{
		// kerjük le a program objektumhoz tartozó shader objektumokat, ... 
        GLint attachedShaders = 0;
        glGetProgramiv( programID, GL_ATTACHED_SHADERS, &attachedShaders );
        std::vector<GLuint> shaders( attachedShaders );

        glGetAttachedShaders( programID, attachedShaders, nullptr, shaders.data() );

        // ...  és "töröljük" őket 
        for ( GLuint shader : shaders )
        {
            glDeleteShader( shader );
        }

	}
}

static inline ImageRGBA::TexelRGBA* get_image_row( ImageRGBA& image, int rowIndex )
{
	return &image.texelData[  rowIndex * image.width ];
}

static void invert_image_RGBA(ImageRGBA& image)
{
	int height_div_2 = image.height / 2;


	for ( int index = 0; index < height_div_2; index++ )
	{
		std::uint32_t* lower_data  =reinterpret_cast<std::uint32_t*>(get_image_row( image, index) );
		std::uint32_t* higher_data =reinterpret_cast<std::uint32_t*>(get_image_row( image, image.height - 1 - index ) );

		for ( unsigned int rowIndex = 0; rowIndex < image.width; rowIndex++ )
		{
			lower_data[ rowIndex ] ^= higher_data[ rowIndex ];
			higher_data[ rowIndex ] ^= lower_data[ rowIndex ];
			lower_data[ rowIndex ] ^= higher_data[ rowIndex ];
		}
	}
}

GLsizei NumberOfMIPLevels( const ImageRGBA& image )
{
	GLsizei targetlevel = 1;
	unsigned int index = std::max( image.width, image.height );

	while (index >>= 1) ++targetlevel;

	return targetlevel;
}

[[nodiscard]] ImageRGBA ImageFromFile( const std::filesystem::path& fileName, bool needsFlip )
{
	ImageRGBA img;

	// Kép betöltése 
	std::unique_ptr<SDL_Surface, decltype( &SDL_DestroySurface )> loaded_img( IMG_Load( fileName.string().c_str() ), SDL_DestroySurface );
	if ( !loaded_img )
	{
		SDL_LogMessage( SDL_LOG_CATEGORY_ERROR, 
						SDL_LOG_PRIORITY_ERROR,
						"[ImageFromFile] Error while loading image file: %s", fileName.string().c_str());
		return img;
	}

	// Uint32-ben tárolja az SDL a színeket, ezért számít a bájtsorrend 
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	SDL_PixelFormat format = SDL_PIXELFORMAT_ABGR8888;
#else
	SDL_PixelFormat format = SDL_PIXELFORMAT_RGBA8888;
#endif

	// Átalakítás 32bit RGBA formátumra, ha nem abban volt 
	std::unique_ptr<SDL_Surface, decltype( &SDL_DestroySurface )> formattedSurf( SDL_ConvertSurface( loaded_img.get(), format ), SDL_DestroySurface );

	if (!formattedSurf)
	{
		SDL_LogMessage( SDL_LOG_CATEGORY_ERROR, 
						SDL_LOG_PRIORITY_ERROR,
						"[ImageFromFile] Error while processing texture");
		return img;
	}

	// Rakjuk át az SDL Surface-t az ImageRGBA-ba 
	img.Assign( reinterpret_cast<const std::uint32_t*>(formattedSurf->pixels), formattedSurf->w, formattedSurf->h );

	// Áttérés SDL koordinátarendszerről ( (0,0) balfent ) OpenGL textúra-koordinátarendszerre ( (0,0) ballent )

	if ( needsFlip ) invert_image_RGBA( img );

	return img;
}

void CleanOGLObject( OGLObject& ObjectGPU )
{
	glDeleteBuffers(1,      &ObjectGPU.vboID);
	ObjectGPU.vboID = 0;
	glDeleteBuffers(1,      &ObjectGPU.iboID);
	ObjectGPU.iboID = 0;
	glDeleteVertexArrays(1, &ObjectGPU.vaoID);
	ObjectGPU.vaoID = 0;
}

// Uniform értékek beállítása az aktív programban
void SetUniform(const char* name, const glm::mat4& matrix) {
	GLint location = ul(name);
	if (location != -1) {
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}
}
void SetUniform(const char* name, float value) {
	GLint location = ul(name);
	if (location != -1) {
		glUniform1f(location, value);
	}
}
void SetUniform(const char* name, double value) {
	GLint location = ul(name);
	if (location != -1) {
		glUniform1d(location, value);
	}
}
void SetUniform(const char* name, int value) {
	GLint location = ul(name);
	if (location != -1) {
		glUniform1i(location, value);
	}
}
void SetUniform(const char* name, const glm::vec3& vector) {
	GLint location = ul(name);
	if (location != -1) {
		glUniform3fv(location, 1, glm::value_ptr(vector));
	}
}
void SetUniform(const char* name, const glm::vec4& vector) {
	GLint location = ul(name);
	if (location != -1) {
		glUniform4fv(location, 1, glm::value_ptr(vector));
	}
}

void SetUniform(const char* name, const glm::vec2& vector) {
	GLint location = ul(name);
	if (location != -1) {
		glUniform2fv(location, 1, glm::value_ptr(vector));
	}
}



RenderObject CreateObject(
	const std::string& fileName,
	const std::initializer_list<VertexAttributeDescriptor>& descriptor,
	GLuint textureID,
	const Material& material,
	glm::mat4 worldTransform,
	float reflectivity)
{
	RenderObject obj;

	MeshObject<Vertex> meshCPU = ObjParser::parse(fileName.c_str());
	obj.m_mesh = CreateGLObjectFromMesh(meshCPU, descriptor);
	obj.m_textureID = textureID;
	obj.m_material = material;
	obj.m_worldTransform = worldTransform;
	obj.m_reflectivity = reflectivity;

	return obj;
};

RenderObject CreateObject(
	const std::string& fileName,
	const std::initializer_list<VertexAttributeDescriptor>& descriptor,
	const std::string& texName,
	const Material& material,
	glm::mat4 worldTransform,
	float reflectivity)
{
	GLuint texID = 0;
	if (!texName.empty())
	{
		ImageRGBA image = ImageFromFile(texName.c_str());

		if (image.width > 0 && image.height > 0)
		{
			glCreateTextures(GL_TEXTURE_2D, 1, &texID);
			glTextureStorage2D(texID, NumberOfMIPLevels(image), GL_RGBA8, image.width, image.height);
			glTextureSubImage2D(texID, 0, 0, 0, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, image.data());

			glGenerateTextureMipmap(texID);
		}
		else
		{
			SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failure to load texture: %s", texName.c_str());
		}
	}

	return CreateObject(fileName, descriptor, texID, material, worldTransform, reflectivity);
}