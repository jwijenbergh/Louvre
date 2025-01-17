#ifndef LOPENGL_H
#define LOPENGL_H

#include <LNamespaces.h>

/**
 * @brief OpenGL utility functions.
 *
 * Set of OpenGL ES 2.0 utility functions.
 */
class Louvre::LOpenGL
{
public:
    /**
     * @brief Open a GLSL shader file.
     *
     * @note The returned string must be manually freed when no longer used.
     *
     * @param fileName Path to the shader file.
     * @returns A string with the contents of the shader or `nullptr` in case of error.
     */
    static char *openShader(const char *fileName);

    /**
     * @brief Get a string representation of an OpenGL error code.
     *
     * This function converts an OpenGL error code obtained from glGetError() into a human-readable string.
     *
     * @param error The OpenGL error code returned by glGetError().
     * @return A const char pointer containing the error message.
     */
    static const char* glErrorString(GLenum error);

    /**
     * @brief Maximum number of texture units.
     * @returns An integer with the maximum number of texture units supported by OpenGL.
     */
    static GLuint maxTextureUnits();

    /**
     * @brief Compile a shader.
     *
     * @param type Type of shader (GL_VERTEX_SHADER or GL_FRAGMENT_SHADER).
     * @param shaderString String with the shader source code.
     * @returns The shader ID or 0 if an error occurs.
     */
    static GLuint compileShader(GLenum type, const char *shaderString);

    /**
     * @brief Create a texture from an image file.
     *
     * Loads and creates a texture from an image file.
     *
     * @note The image is always converted to the `DRM_FORMAT_ARGB8888` format.
     *
     * @param file Path to the image file. Must be an image format supported by [FreeImage](https://freeimage.sourceforge.io/features.html) (JPEG, PNG, BMP, etc).
     * @returns A new texture or `nullptr` in case of error.
     */
    static LTexture *loadTexture(const char *file);

    /**
     * @brief Check if a specific OpenGL extension is available.
     *
     * This function queries the list of supported OpenGL extensions and determines
     * whether the specified extension is among them.
     *
     * @param extension The name of the OpenGL extension to check.
     * @return `true` if the extension is available, `false` otherwise.
     */
    static bool hasExtension(const char *extension);
};

#endif // LOPENGL_H
