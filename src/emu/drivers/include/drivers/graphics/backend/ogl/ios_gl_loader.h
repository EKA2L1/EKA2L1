// iOS GLES3 shim that stands in for <glad/glad.h>.
//
// glad is not built on iOS. The ogl backend was originally written
// against desktop GL / GLES via glad's function-pointer table. On iOS we link
// directly against OpenGLES.framework, which exposes GLES3 symbols at link
// time, and emulate the small set of glad APIs and desktop-only enums the
// backend touches. Keep this header iOS-only: desktop / Android continue to
// use glad untouched.

#pragma once

#include <common/platform.h>

#if !EKA2L1_PLATFORM(IOS)
#error "ios_gl_loader.h is iOS-only; desktop and Android still use <glad/glad.h>."
#endif

// OpenGLES.framework is deprecated since iOS 12 but still ships; silence the
// avalanche of deprecation warnings. A Metal backend is out of scope here.
#ifndef GLES_SILENCE_DEPRECATION
#define GLES_SILENCE_DEPRECATION 1
#endif

#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>

// --- glad function-table fall-throughs --------------------------------------
// glad emits both bare names (`glLineWidth`) and prefixed pointers
// (`glad_glLineWidth`). On iOS the prefixed forms are not declared anywhere,
// so map them straight onto the real GLES3 entry points.
#define glad_glGetError glGetError
#define glad_glLineWidth glLineWidth

// glad's runtime loader hooks. No-op on iOS — symbols are resolved at link
// time by the OpenGLES framework, and we never install an error callback.
typedef void (*GLADloadproc)(const char *);
inline int gladLoadGL(void) { return 1; }
inline int gladLoadGLES2Loader(GLADloadproc) { return 1; }
inline void glad_set_post_callback(void (*)(const char *, void *, int, ...)) {}

// --- desktop-only enums that GLES3 lacks ------------------------------------
// These are referenced by enum-translation tables in common_ogl / graphics_ogl.
// The backend itself gates the matching feature paths, so providing harmless
// stand-in values keeps compilation green without changing runtime behaviour.
#ifndef GL_BGRA
#define GL_BGRA GL_BGRA_EXT
#endif

#ifndef GL_BGR
// GLES has no BGR fixed-format upload path; alias to RGB so format-table
// entries compile. Upload paths that actually need BGR ordering are guarded
// by feature checks at runtime.
#define GL_BGR GL_RGB
#endif

#ifndef GL_LINE_SMOOTH
#define GL_LINE_SMOOTH 0x0B20
#endif

#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

#ifndef GL_SAMPLE_ALPHA_TO_ONE
// Not in GLES3. The backend looks it up via glIsEnabled fall-through; the
// stub value keeps the enum table compiling but the feature stays disabled.
#define GL_SAMPLE_ALPHA_TO_ONE 0x809F
#endif

#ifndef GL_TEXTURE_1D
#define GL_TEXTURE_1D 0x0DE0
#endif

#ifndef GL_TEXTURE_BINDING_1D
#define GL_TEXTURE_BINDING_1D 0x8068
#endif

#ifndef GL_SAMPLER_1D
#define GL_SAMPLER_1D 0x8B5D
#endif

#ifndef GL_GEOMETRY_SHADER
// GLES3 has no geometry shaders. Stub the enum so shader-type tables compile;
// runtime paths that try to create one will fail glCreateShader cleanly.
#define GL_GEOMETRY_SHADER 0x8DD9
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

// --- desktop-only entry points: stubbed inline so call-sites still build ----
// 1D textures, glDrawBuffer (single), glClearDepth (double), glPolygonMode
// have no GLES3 equivalent. Stubs let the code link; the calling paths are
// either guarded at runtime, never reached in the Symbian frontend, or
// degrade gracefully (e.g. depth clear via glClearDepthf).

inline void glClearDepth(double depth) { glClearDepthf(static_cast<GLfloat>(depth)); }
inline void glDepthRange(double n, double f) { glDepthRangef(static_cast<GLfloat>(n), static_cast<GLfloat>(f)); }
inline void glDrawElementsBaseVertex(GLenum mode, GLsizei count, GLenum type, const void *indices, GLint /*basevertex*/) {
    // GLES3.0 has no base-vertex variant. Symbian content reaches this path
    // rarely; on iOS we drop the offset and call the plain variant so meshes
    // render (potentially with wrong vertex selection in the offset case).
    glDrawElements(mode, count, type, indices);
}
inline void glDrawBuffer(GLenum buf) {
    const GLenum buffers[1] = { buf };
    glDrawBuffers(1, buffers);
}
inline void glPolygonMode(GLenum, GLenum) { /* GLES3 has no polygon-mode line/fill toggle */ }

inline void glTexImage1D(GLenum, GLint, GLint, GLsizei, GLint, GLenum, GLenum, const void *) {}
inline void glTexSubImage1D(GLenum, GLint, GLint, GLsizei, GLenum, GLenum, const void *) {}
inline void glCompressedTexImage1D(GLenum, GLint, GLenum, GLsizei, GLint, GLsizei, const void *) {}
inline void glCompressedTexSubImage1D(GLenum, GLint, GLint, GLsizei, GLenum, GLsizei, const void *) {}
