// gles_ffp.hpp — fixed-function OpenGL on top of GLES 3 for d3d8_compat.cpp.
//
// Design (from the TH08 port, N0zoM1z0/th08, MIT): do NOT declare functions
// named glBegin/glEnd etc. — that would collide with the real ES functions
// or recurse. Instead include GLES3/gl3.h and redirect glBegin -> ffp::Begin
// with MACROS. The implementation in gles_ffp.cpp calls the real functions
// as ::glEnable — there the macros are #undef'ed again.
//
// Include INSTEAD OF <GL/gl.h> + <GL/glext.h> (in d3d8_compat.cpp under
// __SWITCH__).
#pragma once

#include "gles_ffp_tokens.hpp"

#include <GLES3/gl3.h>

// gl3.h has no GLdouble, but desktop GL signatures use it.
#ifndef GLdouble
typedef double GLdouble;
#endif
#ifndef GLclampf
typedef float GLclampf;
#endif

namespace ffp
{

// ---- immediate mode ----
void Begin(GLenum mode);
void End();
void Vertex2f(GLfloat x, GLfloat y);
void Vertex3f(GLfloat x, GLfloat y, GLfloat z);
void Color4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void TexCoord2f(GLfloat s, GLfloat t);
void FogCoordf(GLfloat coordinate);

// ---- matrices ----
void MatrixMode(GLenum mode);
void LoadIdentity();
void Ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
           GLdouble nearVal, GLdouble farVal);
void PushMatrix();
void PopMatrix();

// ---- attrib stack ----
void PushAttrib(GLbitfield mask);
void PopAttrib();

// ---- texture environment ----
void TexEnvi(GLenum target, GLenum pname, GLint param);
void TexEnvfv(GLenum target, GLenum pname, const GLfloat *params);

// ---- alpha test ----
void AlphaFunc(GLenum func, GLclampf ref);

// ---- fog ----
void Fogi(GLenum pname, GLint param);
void Fogf(GLenum pname, GLfloat param);
void Fogfv(GLenum pname, const GLfloat *params);

// ---- desktop-only bits missing from ES ----
void ClearDepth(GLdouble depth);
void DrawBuffer(GLenum mode);
void DepthMask(GLboolean flag);

// ---- enable/disable (TEXTURE_2D/ALPHA_TEST/FOG tracked, LIGHTING no-op) ----
void Enable(GLenum cap);
void Disable(GLenum cap);

// ---- GL_CLAMP -> GL_CLAMP_TO_EDGE ----
void TexParameteri(GLenum target, GLenum pname, GLint param);

// ---- 60 Hz pacer (called from Present() after SwapWindow) ----
void SwitchPace();

} // namespace ffp

// ---- redirect desktop names to the shim ----
#define glBegin ffp::Begin
#define glEnd ffp::End
#define glVertex2f ffp::Vertex2f
#define glVertex3f ffp::Vertex3f
#define glColor4ub ffp::Color4ub
#define glTexCoord2f ffp::TexCoord2f
#define glFogCoordf ffp::FogCoordf

#define glMatrixMode ffp::MatrixMode
#define glLoadIdentity ffp::LoadIdentity
#define glOrtho ffp::Ortho
#define glPushMatrix ffp::PushMatrix
#define glPopMatrix ffp::PopMatrix

#define glPushAttrib ffp::PushAttrib
#define glPopAttrib ffp::PopAttrib

#define glTexEnvi ffp::TexEnvi
#define glTexEnvfv ffp::TexEnvfv

#define glAlphaFunc ffp::AlphaFunc

#define glFogi ffp::Fogi
#define glFogf ffp::Fogf
#define glFogfv ffp::Fogfv

#define glClearDepth ffp::ClearDepth
#define glDrawBuffer ffp::DrawBuffer
#define glDepthMask ffp::DepthMask

#define glEnable ffp::Enable
#define glDisable ffp::Disable

#define glTexParameteri ffp::TexParameteri
