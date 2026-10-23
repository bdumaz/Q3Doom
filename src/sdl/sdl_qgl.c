/*
===========================================================================
Q3Doom - OpenGL entry points resolved through SDL

Generated from renderer/qgl.h (every "extern ... qglXxx" declaration).
Licensed under the GNU General Public License version 2 or later.
===========================================================================
*/
#include "../renderer/tr_local.h"
#include "sdl_local.h"

void ( APIENTRY * qglMultiTexCoord2fARB )( GLenum texture, GLfloat s, GLfloat t );
void ( APIENTRY * qglActiveTextureARB )( GLenum texture );
void ( APIENTRY * qglClientActiveTextureARB )( GLenum texture );
void ( APIENTRY * qglLockArraysEXT) (GLint, GLint);
void ( APIENTRY * qglUnlockArraysEXT) (void);
void ( APIENTRY * qglAccum )(GLenum op, GLfloat value);
void ( APIENTRY * qglAlphaFunc )(GLenum func, GLclampf ref);
GLboolean ( APIENTRY * qglAreTexturesResident )(GLsizei n, const GLuint *textures, GLboolean *residences);
void ( APIENTRY * qglArrayElement )(GLint i);
void ( APIENTRY * qglBegin )(GLenum mode);
void ( APIENTRY * qglBindTexture )(GLenum target, GLuint texture);
void ( APIENTRY * qglBitmap )(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte *bitmap);
void ( APIENTRY * qglBlendFunc )(GLenum sfactor, GLenum dfactor);
void ( APIENTRY * qglCallList )(GLuint list);
void ( APIENTRY * qglCallLists )(GLsizei n, GLenum type, const GLvoid *lists);
void ( APIENTRY * qglClear )(GLbitfield mask);
void ( APIENTRY * qglClearAccum )(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void ( APIENTRY * qglClearColor )(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
void ( APIENTRY * qglClearDepth )(GLclampd depth);
void ( APIENTRY * qglClearIndex )(GLfloat c);
void ( APIENTRY * qglClearStencil )(GLint s);
void ( APIENTRY * qglClipPlane )(GLenum plane, const GLdouble *equation);
void ( APIENTRY * qglColor3b )(GLbyte red, GLbyte green, GLbyte blue);
void ( APIENTRY * qglColor3bv )(const GLbyte *v);
void ( APIENTRY * qglColor3d )(GLdouble red, GLdouble green, GLdouble blue);
void ( APIENTRY * qglColor3dv )(const GLdouble *v);
void ( APIENTRY * qglColor3f )(GLfloat red, GLfloat green, GLfloat blue);
void ( APIENTRY * qglColor3fv )(const GLfloat *v);
void ( APIENTRY * qglColor3i )(GLint red, GLint green, GLint blue);
void ( APIENTRY * qglColor3iv )(const GLint *v);
void ( APIENTRY * qglColor3s )(GLshort red, GLshort green, GLshort blue);
void ( APIENTRY * qglColor3sv )(const GLshort *v);
void ( APIENTRY * qglColor3ub )(GLubyte red, GLubyte green, GLubyte blue);
void ( APIENTRY * qglColor3ubv )(const GLubyte *v);
void ( APIENTRY * qglColor3ui )(GLuint red, GLuint green, GLuint blue);
void ( APIENTRY * qglColor3uiv )(const GLuint *v);
void ( APIENTRY * qglColor3us )(GLushort red, GLushort green, GLushort blue);
void ( APIENTRY * qglColor3usv )(const GLushort *v);
void ( APIENTRY * qglColor4b )(GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
void ( APIENTRY * qglColor4bv )(const GLbyte *v);
void ( APIENTRY * qglColor4d )(GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
void ( APIENTRY * qglColor4dv )(const GLdouble *v);
void ( APIENTRY * qglColor4f )(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
void ( APIENTRY * qglColor4fv )(const GLfloat *v);
void ( APIENTRY * qglColor4i )(GLint red, GLint green, GLint blue, GLint alpha);
void ( APIENTRY * qglColor4iv )(const GLint *v);
void ( APIENTRY * qglColor4s )(GLshort red, GLshort green, GLshort blue, GLshort alpha);
void ( APIENTRY * qglColor4sv )(const GLshort *v);
void ( APIENTRY * qglColor4ub )(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha);
void ( APIENTRY * qglColor4ubv )(const GLubyte *v);
void ( APIENTRY * qglColor4ui )(GLuint red, GLuint green, GLuint blue, GLuint alpha);
void ( APIENTRY * qglColor4uiv )(const GLuint *v);
void ( APIENTRY * qglColor4us )(GLushort red, GLushort green, GLushort blue, GLushort alpha);
void ( APIENTRY * qglColor4usv )(const GLushort *v);
void ( APIENTRY * qglColorMask )(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void ( APIENTRY * qglColorMaterial )(GLenum face, GLenum mode);
void ( APIENTRY * qglColorPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglCopyPixels )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
void ( APIENTRY * qglCopyTexImage1D )(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLint border);
void ( APIENTRY * qglCopyTexImage2D )(GLenum target, GLint level, GLenum internalFormat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void ( APIENTRY * qglCopyTexSubImage1D )(GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
void ( APIENTRY * qglCopyTexSubImage2D )(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);
void ( APIENTRY * qglCullFace )(GLenum mode);
void ( APIENTRY * qglDeleteLists )(GLuint list, GLsizei range);
void ( APIENTRY * qglDeleteTextures )(GLsizei n, const GLuint *textures);
void ( APIENTRY * qglDepthFunc )(GLenum func);
void ( APIENTRY * qglDepthMask )(GLboolean flag);
void ( APIENTRY * qglDepthRange )(GLclampd zNear, GLclampd zFar);
void ( APIENTRY * qglDisable )(GLenum cap);
void ( APIENTRY * qglDisableClientState )(GLenum array);
void ( APIENTRY * qglDrawArrays )(GLenum mode, GLint first, GLsizei count);
void ( APIENTRY * qglDrawBuffer )(GLenum mode);
void ( APIENTRY * qglDrawElements )(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices);
void ( APIENTRY * qglDrawPixels )(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglEdgeFlag )(GLboolean flag);
void ( APIENTRY * qglEdgeFlagPointer )(GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglEdgeFlagv )(const GLboolean *flag);
void ( APIENTRY * qglEnable )(GLenum cap);
void ( APIENTRY * qglEnableClientState )(GLenum array);
void ( APIENTRY * qglEnd )(void);
void ( APIENTRY * qglEndList )(void);
void ( APIENTRY * qglEvalCoord1d )(GLdouble u);
void ( APIENTRY * qglEvalCoord1dv )(const GLdouble *u);
void ( APIENTRY * qglEvalCoord1f )(GLfloat u);
void ( APIENTRY * qglEvalCoord1fv )(const GLfloat *u);
void ( APIENTRY * qglEvalCoord2d )(GLdouble u, GLdouble v);
void ( APIENTRY * qglEvalCoord2dv )(const GLdouble *u);
void ( APIENTRY * qglEvalCoord2f )(GLfloat u, GLfloat v);
void ( APIENTRY * qglEvalCoord2fv )(const GLfloat *u);
void ( APIENTRY * qglEvalMesh1 )(GLenum mode, GLint i1, GLint i2);
void ( APIENTRY * qglEvalMesh2 )(GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
void ( APIENTRY * qglEvalPoint1 )(GLint i);
void ( APIENTRY * qglEvalPoint2 )(GLint i, GLint j);
void ( APIENTRY * qglFeedbackBuffer )(GLsizei size, GLenum type, GLfloat *buffer);
void ( APIENTRY * qglFinish )(void);
void ( APIENTRY * qglFlush )(void);
void ( APIENTRY * qglFogf )(GLenum pname, GLfloat param);
void ( APIENTRY * qglFogfv )(GLenum pname, const GLfloat *params);
void ( APIENTRY * qglFogi )(GLenum pname, GLint param);
void ( APIENTRY * qglFogiv )(GLenum pname, const GLint *params);
void ( APIENTRY * qglFrontFace )(GLenum mode);
void ( APIENTRY * qglFrustum )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
GLuint ( APIENTRY * qglGenLists )(GLsizei range);
void ( APIENTRY * qglGenTextures )(GLsizei n, GLuint *textures);
void ( APIENTRY * qglGetBooleanv )(GLenum pname, GLboolean *params);
void ( APIENTRY * qglGetClipPlane )(GLenum plane, GLdouble *equation);
void ( APIENTRY * qglGetDoublev )(GLenum pname, GLdouble *params);
GLenum ( APIENTRY * qglGetError )(void);
void ( APIENTRY * qglGetFloatv )(GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetIntegerv )(GLenum pname, GLint *params);
void ( APIENTRY * qglGetLightfv )(GLenum light, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetLightiv )(GLenum light, GLenum pname, GLint *params);
void ( APIENTRY * qglGetMapdv )(GLenum target, GLenum query, GLdouble *v);
void ( APIENTRY * qglGetMapfv )(GLenum target, GLenum query, GLfloat *v);
void ( APIENTRY * qglGetMapiv )(GLenum target, GLenum query, GLint *v);
void ( APIENTRY * qglGetMaterialfv )(GLenum face, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetMaterialiv )(GLenum face, GLenum pname, GLint *params);
void ( APIENTRY * qglGetPixelMapfv )(GLenum map, GLfloat *values);
void ( APIENTRY * qglGetPixelMapuiv )(GLenum map, GLuint *values);
void ( APIENTRY * qglGetPixelMapusv )(GLenum map, GLushort *values);
void ( APIENTRY * qglGetPointerv )(GLenum pname, GLvoid* *params);
void ( APIENTRY * qglGetPolygonStipple )(GLubyte *mask);
const GLubyte * ( APIENTRY * qglGetString )(GLenum name);
void ( APIENTRY * qglGetTexEnvfv )(GLenum target, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetTexEnviv )(GLenum target, GLenum pname, GLint *params);
void ( APIENTRY * qglGetTexGendv )(GLenum coord, GLenum pname, GLdouble *params);
void ( APIENTRY * qglGetTexGenfv )(GLenum coord, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetTexGeniv )(GLenum coord, GLenum pname, GLint *params);
void ( APIENTRY * qglGetTexImage )(GLenum target, GLint level, GLenum format, GLenum type, GLvoid *pixels);
void ( APIENTRY * qglGetTexLevelParameterfv )(GLenum target, GLint level, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetTexLevelParameteriv )(GLenum target, GLint level, GLenum pname, GLint *params);
void ( APIENTRY * qglGetTexParameterfv )(GLenum target, GLenum pname, GLfloat *params);
void ( APIENTRY * qglGetTexParameteriv )(GLenum target, GLenum pname, GLint *params);
void ( APIENTRY * qglHint )(GLenum target, GLenum mode);
void ( APIENTRY * qglIndexMask )(GLuint mask);
void ( APIENTRY * qglIndexPointer )(GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglIndexd )(GLdouble c);
void ( APIENTRY * qglIndexdv )(const GLdouble *c);
void ( APIENTRY * qglIndexf )(GLfloat c);
void ( APIENTRY * qglIndexfv )(const GLfloat *c);
void ( APIENTRY * qglIndexi )(GLint c);
void ( APIENTRY * qglIndexiv )(const GLint *c);
void ( APIENTRY * qglIndexs )(GLshort c);
void ( APIENTRY * qglIndexsv )(const GLshort *c);
void ( APIENTRY * qglIndexub )(GLubyte c);
void ( APIENTRY * qglIndexubv )(const GLubyte *c);
void ( APIENTRY * qglInitNames )(void);
void ( APIENTRY * qglInterleavedArrays )(GLenum format, GLsizei stride, const GLvoid *pointer);
GLboolean ( APIENTRY * qglIsEnabled )(GLenum cap);
GLboolean ( APIENTRY * qglIsList )(GLuint list);
GLboolean ( APIENTRY * qglIsTexture )(GLuint texture);
void ( APIENTRY * qglLightModelf )(GLenum pname, GLfloat param);
void ( APIENTRY * qglLightModelfv )(GLenum pname, const GLfloat *params);
void ( APIENTRY * qglLightModeli )(GLenum pname, GLint param);
void ( APIENTRY * qglLightModeliv )(GLenum pname, const GLint *params);
void ( APIENTRY * qglLightf )(GLenum light, GLenum pname, GLfloat param);
void ( APIENTRY * qglLightfv )(GLenum light, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglLighti )(GLenum light, GLenum pname, GLint param);
void ( APIENTRY * qglLightiv )(GLenum light, GLenum pname, const GLint *params);
void ( APIENTRY * qglLineStipple )(GLint factor, GLushort pattern);
void ( APIENTRY * qglLineWidth )(GLfloat width);
void ( APIENTRY * qglListBase )(GLuint base);
void ( APIENTRY * qglLoadIdentity )(void);
void ( APIENTRY * qglLoadMatrixd )(const GLdouble *m);
void ( APIENTRY * qglLoadMatrixf )(const GLfloat *m);
void ( APIENTRY * qglLoadName )(GLuint name);
void ( APIENTRY * qglLogicOp )(GLenum opcode);
void ( APIENTRY * qglMap1d )(GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble *points);
void ( APIENTRY * qglMap1f )(GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat *points);
void ( APIENTRY * qglMap2d )(GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble *points);
void ( APIENTRY * qglMap2f )(GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat *points);
void ( APIENTRY * qglMapGrid1d )(GLint un, GLdouble u1, GLdouble u2);
void ( APIENTRY * qglMapGrid1f )(GLint un, GLfloat u1, GLfloat u2);
void ( APIENTRY * qglMapGrid2d )(GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
void ( APIENTRY * qglMapGrid2f )(GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
void ( APIENTRY * qglMaterialf )(GLenum face, GLenum pname, GLfloat param);
void ( APIENTRY * qglMaterialfv )(GLenum face, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglMateriali )(GLenum face, GLenum pname, GLint param);
void ( APIENTRY * qglMaterialiv )(GLenum face, GLenum pname, const GLint *params);
void ( APIENTRY * qglMatrixMode )(GLenum mode);
void ( APIENTRY * qglMultMatrixd )(const GLdouble *m);
void ( APIENTRY * qglMultMatrixf )(const GLfloat *m);
void ( APIENTRY * qglNewList )(GLuint list, GLenum mode);
void ( APIENTRY * qglNormal3b )(GLbyte nx, GLbyte ny, GLbyte nz);
void ( APIENTRY * qglNormal3bv )(const GLbyte *v);
void ( APIENTRY * qglNormal3d )(GLdouble nx, GLdouble ny, GLdouble nz);
void ( APIENTRY * qglNormal3dv )(const GLdouble *v);
void ( APIENTRY * qglNormal3f )(GLfloat nx, GLfloat ny, GLfloat nz);
void ( APIENTRY * qglNormal3fv )(const GLfloat *v);
void ( APIENTRY * qglNormal3i )(GLint nx, GLint ny, GLint nz);
void ( APIENTRY * qglNormal3iv )(const GLint *v);
void ( APIENTRY * qglNormal3s )(GLshort nx, GLshort ny, GLshort nz);
void ( APIENTRY * qglNormal3sv )(const GLshort *v);
void ( APIENTRY * qglNormalPointer )(GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglOrtho )(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble zNear, GLdouble zFar);
void ( APIENTRY * qglPassThrough )(GLfloat token);
void ( APIENTRY * qglPixelMapfv )(GLenum map, GLsizei mapsize, const GLfloat *values);
void ( APIENTRY * qglPixelMapuiv )(GLenum map, GLsizei mapsize, const GLuint *values);
void ( APIENTRY * qglPixelMapusv )(GLenum map, GLsizei mapsize, const GLushort *values);
void ( APIENTRY * qglPixelStoref )(GLenum pname, GLfloat param);
void ( APIENTRY * qglPixelStorei )(GLenum pname, GLint param);
void ( APIENTRY * qglPixelTransferf )(GLenum pname, GLfloat param);
void ( APIENTRY * qglPixelTransferi )(GLenum pname, GLint param);
void ( APIENTRY * qglPixelZoom )(GLfloat xfactor, GLfloat yfactor);
void ( APIENTRY * qglPointSize )(GLfloat size);
void ( APIENTRY * qglPolygonMode )(GLenum face, GLenum mode);
void ( APIENTRY * qglPolygonOffset )(GLfloat factor, GLfloat units);
void ( APIENTRY * qglPolygonStipple )(const GLubyte *mask);
void ( APIENTRY * qglPopAttrib )(void);
void ( APIENTRY * qglPopClientAttrib )(void);
void ( APIENTRY * qglPopMatrix )(void);
void ( APIENTRY * qglPopName )(void);
void ( APIENTRY * qglPrioritizeTextures )(GLsizei n, const GLuint *textures, const GLclampf *priorities);
void ( APIENTRY * qglPushAttrib )(GLbitfield mask);
void ( APIENTRY * qglPushClientAttrib )(GLbitfield mask);
void ( APIENTRY * qglPushMatrix )(void);
void ( APIENTRY * qglPushName )(GLuint name);
void ( APIENTRY * qglRasterPos2d )(GLdouble x, GLdouble y);
void ( APIENTRY * qglRasterPos2dv )(const GLdouble *v);
void ( APIENTRY * qglRasterPos2f )(GLfloat x, GLfloat y);
void ( APIENTRY * qglRasterPos2fv )(const GLfloat *v);
void ( APIENTRY * qglRasterPos2i )(GLint x, GLint y);
void ( APIENTRY * qglRasterPos2iv )(const GLint *v);
void ( APIENTRY * qglRasterPos2s )(GLshort x, GLshort y);
void ( APIENTRY * qglRasterPos2sv )(const GLshort *v);
void ( APIENTRY * qglRasterPos3d )(GLdouble x, GLdouble y, GLdouble z);
void ( APIENTRY * qglRasterPos3dv )(const GLdouble *v);
void ( APIENTRY * qglRasterPos3f )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglRasterPos3fv )(const GLfloat *v);
void ( APIENTRY * qglRasterPos3i )(GLint x, GLint y, GLint z);
void ( APIENTRY * qglRasterPos3iv )(const GLint *v);
void ( APIENTRY * qglRasterPos3s )(GLshort x, GLshort y, GLshort z);
void ( APIENTRY * qglRasterPos3sv )(const GLshort *v);
void ( APIENTRY * qglRasterPos4d )(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void ( APIENTRY * qglRasterPos4dv )(const GLdouble *v);
void ( APIENTRY * qglRasterPos4f )(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void ( APIENTRY * qglRasterPos4fv )(const GLfloat *v);
void ( APIENTRY * qglRasterPos4i )(GLint x, GLint y, GLint z, GLint w);
void ( APIENTRY * qglRasterPos4iv )(const GLint *v);
void ( APIENTRY * qglRasterPos4s )(GLshort x, GLshort y, GLshort z, GLshort w);
void ( APIENTRY * qglRasterPos4sv )(const GLshort *v);
void ( APIENTRY * qglReadBuffer )(GLenum mode);
void ( APIENTRY * qglReadPixels )(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels);
void ( APIENTRY * qglRectd )(GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
void ( APIENTRY * qglRectdv )(const GLdouble *v1, const GLdouble *v2);
void ( APIENTRY * qglRectf )(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
void ( APIENTRY * qglRectfv )(const GLfloat *v1, const GLfloat *v2);
void ( APIENTRY * qglRecti )(GLint x1, GLint y1, GLint x2, GLint y2);
void ( APIENTRY * qglRectiv )(const GLint *v1, const GLint *v2);
void ( APIENTRY * qglRects )(GLshort x1, GLshort y1, GLshort x2, GLshort y2);
void ( APIENTRY * qglRectsv )(const GLshort *v1, const GLshort *v2);
GLint ( APIENTRY * qglRenderMode )(GLenum mode);
void ( APIENTRY * qglRotated )(GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
void ( APIENTRY * qglRotatef )(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglScaled )(GLdouble x, GLdouble y, GLdouble z);
void ( APIENTRY * qglScalef )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglScissor )(GLint x, GLint y, GLsizei width, GLsizei height);
void ( APIENTRY * qglSelectBuffer )(GLsizei size, GLuint *buffer);
void ( APIENTRY * qglShadeModel )(GLenum mode);
void ( APIENTRY * qglStencilFunc )(GLenum func, GLint ref, GLuint mask);
void ( APIENTRY * qglStencilMask )(GLuint mask);
void ( APIENTRY * qglStencilOp )(GLenum fail, GLenum zfail, GLenum zpass);
void ( APIENTRY * qglTexCoord1d )(GLdouble s);
void ( APIENTRY * qglTexCoord1dv )(const GLdouble *v);
void ( APIENTRY * qglTexCoord1f )(GLfloat s);
void ( APIENTRY * qglTexCoord1fv )(const GLfloat *v);
void ( APIENTRY * qglTexCoord1i )(GLint s);
void ( APIENTRY * qglTexCoord1iv )(const GLint *v);
void ( APIENTRY * qglTexCoord1s )(GLshort s);
void ( APIENTRY * qglTexCoord1sv )(const GLshort *v);
void ( APIENTRY * qglTexCoord2d )(GLdouble s, GLdouble t);
void ( APIENTRY * qglTexCoord2dv )(const GLdouble *v);
void ( APIENTRY * qglTexCoord2f )(GLfloat s, GLfloat t);
void ( APIENTRY * qglTexCoord2fv )(const GLfloat *v);
void ( APIENTRY * qglTexCoord2i )(GLint s, GLint t);
void ( APIENTRY * qglTexCoord2iv )(const GLint *v);
void ( APIENTRY * qglTexCoord2s )(GLshort s, GLshort t);
void ( APIENTRY * qglTexCoord2sv )(const GLshort *v);
void ( APIENTRY * qglTexCoord3d )(GLdouble s, GLdouble t, GLdouble r);
void ( APIENTRY * qglTexCoord3dv )(const GLdouble *v);
void ( APIENTRY * qglTexCoord3f )(GLfloat s, GLfloat t, GLfloat r);
void ( APIENTRY * qglTexCoord3fv )(const GLfloat *v);
void ( APIENTRY * qglTexCoord3i )(GLint s, GLint t, GLint r);
void ( APIENTRY * qglTexCoord3iv )(const GLint *v);
void ( APIENTRY * qglTexCoord3s )(GLshort s, GLshort t, GLshort r);
void ( APIENTRY * qglTexCoord3sv )(const GLshort *v);
void ( APIENTRY * qglTexCoord4d )(GLdouble s, GLdouble t, GLdouble r, GLdouble q);
void ( APIENTRY * qglTexCoord4dv )(const GLdouble *v);
void ( APIENTRY * qglTexCoord4f )(GLfloat s, GLfloat t, GLfloat r, GLfloat q);
void ( APIENTRY * qglTexCoord4fv )(const GLfloat *v);
void ( APIENTRY * qglTexCoord4i )(GLint s, GLint t, GLint r, GLint q);
void ( APIENTRY * qglTexCoord4iv )(const GLint *v);
void ( APIENTRY * qglTexCoord4s )(GLshort s, GLshort t, GLshort r, GLshort q);
void ( APIENTRY * qglTexCoord4sv )(const GLshort *v);
void ( APIENTRY * qglTexCoordPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglTexEnvf )(GLenum target, GLenum pname, GLfloat param);
void ( APIENTRY * qglTexEnvfv )(GLenum target, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglTexEnvi )(GLenum target, GLenum pname, GLint param);
void ( APIENTRY * qglTexEnviv )(GLenum target, GLenum pname, const GLint *params);
void ( APIENTRY * qglTexGend )(GLenum coord, GLenum pname, GLdouble param);
void ( APIENTRY * qglTexGendv )(GLenum coord, GLenum pname, const GLdouble *params);
void ( APIENTRY * qglTexGenf )(GLenum coord, GLenum pname, GLfloat param);
void ( APIENTRY * qglTexGenfv )(GLenum coord, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglTexGeni )(GLenum coord, GLenum pname, GLint param);
void ( APIENTRY * qglTexGeniv )(GLenum coord, GLenum pname, const GLint *params);
void ( APIENTRY * qglTexImage1D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglTexImage2D )(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglTexParameterf )(GLenum target, GLenum pname, GLfloat param);
void ( APIENTRY * qglTexParameterfv )(GLenum target, GLenum pname, const GLfloat *params);
void ( APIENTRY * qglTexParameteri )(GLenum target, GLenum pname, GLint param);
void ( APIENTRY * qglTexParameteriv )(GLenum target, GLenum pname, const GLint *params);
void ( APIENTRY * qglTexSubImage1D )(GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglTexSubImage2D )(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels);
void ( APIENTRY * qglTranslated )(GLdouble x, GLdouble y, GLdouble z);
void ( APIENTRY * qglTranslatef )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglVertex2d )(GLdouble x, GLdouble y);
void ( APIENTRY * qglVertex2dv )(const GLdouble *v);
void ( APIENTRY * qglVertex2f )(GLfloat x, GLfloat y);
void ( APIENTRY * qglVertex2fv )(const GLfloat *v);
void ( APIENTRY * qglVertex2i )(GLint x, GLint y);
void ( APIENTRY * qglVertex2iv )(const GLint *v);
void ( APIENTRY * qglVertex2s )(GLshort x, GLshort y);
void ( APIENTRY * qglVertex2sv )(const GLshort *v);
void ( APIENTRY * qglVertex3d )(GLdouble x, GLdouble y, GLdouble z);
void ( APIENTRY * qglVertex3dv )(const GLdouble *v);
void ( APIENTRY * qglVertex3f )(GLfloat x, GLfloat y, GLfloat z);
void ( APIENTRY * qglVertex3fv )(const GLfloat *v);
void ( APIENTRY * qglVertex3i )(GLint x, GLint y, GLint z);
void ( APIENTRY * qglVertex3iv )(const GLint *v);
void ( APIENTRY * qglVertex3s )(GLshort x, GLshort y, GLshort z);
void ( APIENTRY * qglVertex3sv )(const GLshort *v);
void ( APIENTRY * qglVertex4d )(GLdouble x, GLdouble y, GLdouble z, GLdouble w);
void ( APIENTRY * qglVertex4dv )(const GLdouble *v);
void ( APIENTRY * qglVertex4f )(GLfloat x, GLfloat y, GLfloat z, GLfloat w);
void ( APIENTRY * qglVertex4fv )(const GLfloat *v);
void ( APIENTRY * qglVertex4i )(GLint x, GLint y, GLint z, GLint w);
void ( APIENTRY * qglVertex4iv )(const GLint *v);
void ( APIENTRY * qglVertex4s )(GLshort x, GLshort y, GLshort z, GLshort w);
void ( APIENTRY * qglVertex4sv )(const GLshort *v);
void ( APIENTRY * qglVertexPointer )(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer);
void ( APIENTRY * qglViewport )(GLint x, GLint y, GLsizei width, GLsizei height);

typedef struct {
	void		**ptr;
	const char	*name;
} qglProc_t;

// core OpenGL 1.1 entry points, all mandatory
static const qglProc_t qglProcs[] = {
	{ (void **)&qglAccum, "glAccum" },
	{ (void **)&qglAlphaFunc, "glAlphaFunc" },
	{ (void **)&qglAreTexturesResident, "glAreTexturesResident" },
	{ (void **)&qglArrayElement, "glArrayElement" },
	{ (void **)&qglBegin, "glBegin" },
	{ (void **)&qglBindTexture, "glBindTexture" },
	{ (void **)&qglBitmap, "glBitmap" },
	{ (void **)&qglBlendFunc, "glBlendFunc" },
	{ (void **)&qglCallList, "glCallList" },
	{ (void **)&qglCallLists, "glCallLists" },
	{ (void **)&qglClear, "glClear" },
	{ (void **)&qglClearAccum, "glClearAccum" },
	{ (void **)&qglClearColor, "glClearColor" },
	{ (void **)&qglClearDepth, "glClearDepth" },
	{ (void **)&qglClearIndex, "glClearIndex" },
	{ (void **)&qglClearStencil, "glClearStencil" },
	{ (void **)&qglClipPlane, "glClipPlane" },
	{ (void **)&qglColor3b, "glColor3b" },
	{ (void **)&qglColor3bv, "glColor3bv" },
	{ (void **)&qglColor3d, "glColor3d" },
	{ (void **)&qglColor3dv, "glColor3dv" },
	{ (void **)&qglColor3f, "glColor3f" },
	{ (void **)&qglColor3fv, "glColor3fv" },
	{ (void **)&qglColor3i, "glColor3i" },
	{ (void **)&qglColor3iv, "glColor3iv" },
	{ (void **)&qglColor3s, "glColor3s" },
	{ (void **)&qglColor3sv, "glColor3sv" },
	{ (void **)&qglColor3ub, "glColor3ub" },
	{ (void **)&qglColor3ubv, "glColor3ubv" },
	{ (void **)&qglColor3ui, "glColor3ui" },
	{ (void **)&qglColor3uiv, "glColor3uiv" },
	{ (void **)&qglColor3us, "glColor3us" },
	{ (void **)&qglColor3usv, "glColor3usv" },
	{ (void **)&qglColor4b, "glColor4b" },
	{ (void **)&qglColor4bv, "glColor4bv" },
	{ (void **)&qglColor4d, "glColor4d" },
	{ (void **)&qglColor4dv, "glColor4dv" },
	{ (void **)&qglColor4f, "glColor4f" },
	{ (void **)&qglColor4fv, "glColor4fv" },
	{ (void **)&qglColor4i, "glColor4i" },
	{ (void **)&qglColor4iv, "glColor4iv" },
	{ (void **)&qglColor4s, "glColor4s" },
	{ (void **)&qglColor4sv, "glColor4sv" },
	{ (void **)&qglColor4ub, "glColor4ub" },
	{ (void **)&qglColor4ubv, "glColor4ubv" },
	{ (void **)&qglColor4ui, "glColor4ui" },
	{ (void **)&qglColor4uiv, "glColor4uiv" },
	{ (void **)&qglColor4us, "glColor4us" },
	{ (void **)&qglColor4usv, "glColor4usv" },
	{ (void **)&qglColorMask, "glColorMask" },
	{ (void **)&qglColorMaterial, "glColorMaterial" },
	{ (void **)&qglColorPointer, "glColorPointer" },
	{ (void **)&qglCopyPixels, "glCopyPixels" },
	{ (void **)&qglCopyTexImage1D, "glCopyTexImage1D" },
	{ (void **)&qglCopyTexImage2D, "glCopyTexImage2D" },
	{ (void **)&qglCopyTexSubImage1D, "glCopyTexSubImage1D" },
	{ (void **)&qglCopyTexSubImage2D, "glCopyTexSubImage2D" },
	{ (void **)&qglCullFace, "glCullFace" },
	{ (void **)&qglDeleteLists, "glDeleteLists" },
	{ (void **)&qglDeleteTextures, "glDeleteTextures" },
	{ (void **)&qglDepthFunc, "glDepthFunc" },
	{ (void **)&qglDepthMask, "glDepthMask" },
	{ (void **)&qglDepthRange, "glDepthRange" },
	{ (void **)&qglDisable, "glDisable" },
	{ (void **)&qglDisableClientState, "glDisableClientState" },
	{ (void **)&qglDrawArrays, "glDrawArrays" },
	{ (void **)&qglDrawBuffer, "glDrawBuffer" },
	{ (void **)&qglDrawElements, "glDrawElements" },
	{ (void **)&qglDrawPixels, "glDrawPixels" },
	{ (void **)&qglEdgeFlag, "glEdgeFlag" },
	{ (void **)&qglEdgeFlagPointer, "glEdgeFlagPointer" },
	{ (void **)&qglEdgeFlagv, "glEdgeFlagv" },
	{ (void **)&qglEnable, "glEnable" },
	{ (void **)&qglEnableClientState, "glEnableClientState" },
	{ (void **)&qglEnd, "glEnd" },
	{ (void **)&qglEndList, "glEndList" },
	{ (void **)&qglEvalCoord1d, "glEvalCoord1d" },
	{ (void **)&qglEvalCoord1dv, "glEvalCoord1dv" },
	{ (void **)&qglEvalCoord1f, "glEvalCoord1f" },
	{ (void **)&qglEvalCoord1fv, "glEvalCoord1fv" },
	{ (void **)&qglEvalCoord2d, "glEvalCoord2d" },
	{ (void **)&qglEvalCoord2dv, "glEvalCoord2dv" },
	{ (void **)&qglEvalCoord2f, "glEvalCoord2f" },
	{ (void **)&qglEvalCoord2fv, "glEvalCoord2fv" },
	{ (void **)&qglEvalMesh1, "glEvalMesh1" },
	{ (void **)&qglEvalMesh2, "glEvalMesh2" },
	{ (void **)&qglEvalPoint1, "glEvalPoint1" },
	{ (void **)&qglEvalPoint2, "glEvalPoint2" },
	{ (void **)&qglFeedbackBuffer, "glFeedbackBuffer" },
	{ (void **)&qglFinish, "glFinish" },
	{ (void **)&qglFlush, "glFlush" },
	{ (void **)&qglFogf, "glFogf" },
	{ (void **)&qglFogfv, "glFogfv" },
	{ (void **)&qglFogi, "glFogi" },
	{ (void **)&qglFogiv, "glFogiv" },
	{ (void **)&qglFrontFace, "glFrontFace" },
	{ (void **)&qglFrustum, "glFrustum" },
	{ (void **)&qglGenLists, "glGenLists" },
	{ (void **)&qglGenTextures, "glGenTextures" },
	{ (void **)&qglGetBooleanv, "glGetBooleanv" },
	{ (void **)&qglGetClipPlane, "glGetClipPlane" },
	{ (void **)&qglGetDoublev, "glGetDoublev" },
	{ (void **)&qglGetError, "glGetError" },
	{ (void **)&qglGetFloatv, "glGetFloatv" },
	{ (void **)&qglGetIntegerv, "glGetIntegerv" },
	{ (void **)&qglGetLightfv, "glGetLightfv" },
	{ (void **)&qglGetLightiv, "glGetLightiv" },
	{ (void **)&qglGetMapdv, "glGetMapdv" },
	{ (void **)&qglGetMapfv, "glGetMapfv" },
	{ (void **)&qglGetMapiv, "glGetMapiv" },
	{ (void **)&qglGetMaterialfv, "glGetMaterialfv" },
	{ (void **)&qglGetMaterialiv, "glGetMaterialiv" },
	{ (void **)&qglGetPixelMapfv, "glGetPixelMapfv" },
	{ (void **)&qglGetPixelMapuiv, "glGetPixelMapuiv" },
	{ (void **)&qglGetPixelMapusv, "glGetPixelMapusv" },
	{ (void **)&qglGetPointerv, "glGetPointerv" },
	{ (void **)&qglGetPolygonStipple, "glGetPolygonStipple" },
	{ (void **)&qglGetString, "glGetString" },
	{ (void **)&qglGetTexEnvfv, "glGetTexEnvfv" },
	{ (void **)&qglGetTexEnviv, "glGetTexEnviv" },
	{ (void **)&qglGetTexGendv, "glGetTexGendv" },
	{ (void **)&qglGetTexGenfv, "glGetTexGenfv" },
	{ (void **)&qglGetTexGeniv, "glGetTexGeniv" },
	{ (void **)&qglGetTexImage, "glGetTexImage" },
	{ (void **)&qglGetTexLevelParameterfv, "glGetTexLevelParameterfv" },
	{ (void **)&qglGetTexLevelParameteriv, "glGetTexLevelParameteriv" },
	{ (void **)&qglGetTexParameterfv, "glGetTexParameterfv" },
	{ (void **)&qglGetTexParameteriv, "glGetTexParameteriv" },
	{ (void **)&qglHint, "glHint" },
	{ (void **)&qglIndexMask, "glIndexMask" },
	{ (void **)&qglIndexPointer, "glIndexPointer" },
	{ (void **)&qglIndexd, "glIndexd" },
	{ (void **)&qglIndexdv, "glIndexdv" },
	{ (void **)&qglIndexf, "glIndexf" },
	{ (void **)&qglIndexfv, "glIndexfv" },
	{ (void **)&qglIndexi, "glIndexi" },
	{ (void **)&qglIndexiv, "glIndexiv" },
	{ (void **)&qglIndexs, "glIndexs" },
	{ (void **)&qglIndexsv, "glIndexsv" },
	{ (void **)&qglIndexub, "glIndexub" },
	{ (void **)&qglIndexubv, "glIndexubv" },
	{ (void **)&qglInitNames, "glInitNames" },
	{ (void **)&qglInterleavedArrays, "glInterleavedArrays" },
	{ (void **)&qglIsEnabled, "glIsEnabled" },
	{ (void **)&qglIsList, "glIsList" },
	{ (void **)&qglIsTexture, "glIsTexture" },
	{ (void **)&qglLightModelf, "glLightModelf" },
	{ (void **)&qglLightModelfv, "glLightModelfv" },
	{ (void **)&qglLightModeli, "glLightModeli" },
	{ (void **)&qglLightModeliv, "glLightModeliv" },
	{ (void **)&qglLightf, "glLightf" },
	{ (void **)&qglLightfv, "glLightfv" },
	{ (void **)&qglLighti, "glLighti" },
	{ (void **)&qglLightiv, "glLightiv" },
	{ (void **)&qglLineStipple, "glLineStipple" },
	{ (void **)&qglLineWidth, "glLineWidth" },
	{ (void **)&qglListBase, "glListBase" },
	{ (void **)&qglLoadIdentity, "glLoadIdentity" },
	{ (void **)&qglLoadMatrixd, "glLoadMatrixd" },
	{ (void **)&qglLoadMatrixf, "glLoadMatrixf" },
	{ (void **)&qglLoadName, "glLoadName" },
	{ (void **)&qglLogicOp, "glLogicOp" },
	{ (void **)&qglMap1d, "glMap1d" },
	{ (void **)&qglMap1f, "glMap1f" },
	{ (void **)&qglMap2d, "glMap2d" },
	{ (void **)&qglMap2f, "glMap2f" },
	{ (void **)&qglMapGrid1d, "glMapGrid1d" },
	{ (void **)&qglMapGrid1f, "glMapGrid1f" },
	{ (void **)&qglMapGrid2d, "glMapGrid2d" },
	{ (void **)&qglMapGrid2f, "glMapGrid2f" },
	{ (void **)&qglMaterialf, "glMaterialf" },
	{ (void **)&qglMaterialfv, "glMaterialfv" },
	{ (void **)&qglMateriali, "glMateriali" },
	{ (void **)&qglMaterialiv, "glMaterialiv" },
	{ (void **)&qglMatrixMode, "glMatrixMode" },
	{ (void **)&qglMultMatrixd, "glMultMatrixd" },
	{ (void **)&qglMultMatrixf, "glMultMatrixf" },
	{ (void **)&qglNewList, "glNewList" },
	{ (void **)&qglNormal3b, "glNormal3b" },
	{ (void **)&qglNormal3bv, "glNormal3bv" },
	{ (void **)&qglNormal3d, "glNormal3d" },
	{ (void **)&qglNormal3dv, "glNormal3dv" },
	{ (void **)&qglNormal3f, "glNormal3f" },
	{ (void **)&qglNormal3fv, "glNormal3fv" },
	{ (void **)&qglNormal3i, "glNormal3i" },
	{ (void **)&qglNormal3iv, "glNormal3iv" },
	{ (void **)&qglNormal3s, "glNormal3s" },
	{ (void **)&qglNormal3sv, "glNormal3sv" },
	{ (void **)&qglNormalPointer, "glNormalPointer" },
	{ (void **)&qglOrtho, "glOrtho" },
	{ (void **)&qglPassThrough, "glPassThrough" },
	{ (void **)&qglPixelMapfv, "glPixelMapfv" },
	{ (void **)&qglPixelMapuiv, "glPixelMapuiv" },
	{ (void **)&qglPixelMapusv, "glPixelMapusv" },
	{ (void **)&qglPixelStoref, "glPixelStoref" },
	{ (void **)&qglPixelStorei, "glPixelStorei" },
	{ (void **)&qglPixelTransferf, "glPixelTransferf" },
	{ (void **)&qglPixelTransferi, "glPixelTransferi" },
	{ (void **)&qglPixelZoom, "glPixelZoom" },
	{ (void **)&qglPointSize, "glPointSize" },
	{ (void **)&qglPolygonMode, "glPolygonMode" },
	{ (void **)&qglPolygonOffset, "glPolygonOffset" },
	{ (void **)&qglPolygonStipple, "glPolygonStipple" },
	{ (void **)&qglPopAttrib, "glPopAttrib" },
	{ (void **)&qglPopClientAttrib, "glPopClientAttrib" },
	{ (void **)&qglPopMatrix, "glPopMatrix" },
	{ (void **)&qglPopName, "glPopName" },
	{ (void **)&qglPrioritizeTextures, "glPrioritizeTextures" },
	{ (void **)&qglPushAttrib, "glPushAttrib" },
	{ (void **)&qglPushClientAttrib, "glPushClientAttrib" },
	{ (void **)&qglPushMatrix, "glPushMatrix" },
	{ (void **)&qglPushName, "glPushName" },
	{ (void **)&qglRasterPos2d, "glRasterPos2d" },
	{ (void **)&qglRasterPos2dv, "glRasterPos2dv" },
	{ (void **)&qglRasterPos2f, "glRasterPos2f" },
	{ (void **)&qglRasterPos2fv, "glRasterPos2fv" },
	{ (void **)&qglRasterPos2i, "glRasterPos2i" },
	{ (void **)&qglRasterPos2iv, "glRasterPos2iv" },
	{ (void **)&qglRasterPos2s, "glRasterPos2s" },
	{ (void **)&qglRasterPos2sv, "glRasterPos2sv" },
	{ (void **)&qglRasterPos3d, "glRasterPos3d" },
	{ (void **)&qglRasterPos3dv, "glRasterPos3dv" },
	{ (void **)&qglRasterPos3f, "glRasterPos3f" },
	{ (void **)&qglRasterPos3fv, "glRasterPos3fv" },
	{ (void **)&qglRasterPos3i, "glRasterPos3i" },
	{ (void **)&qglRasterPos3iv, "glRasterPos3iv" },
	{ (void **)&qglRasterPos3s, "glRasterPos3s" },
	{ (void **)&qglRasterPos3sv, "glRasterPos3sv" },
	{ (void **)&qglRasterPos4d, "glRasterPos4d" },
	{ (void **)&qglRasterPos4dv, "glRasterPos4dv" },
	{ (void **)&qglRasterPos4f, "glRasterPos4f" },
	{ (void **)&qglRasterPos4fv, "glRasterPos4fv" },
	{ (void **)&qglRasterPos4i, "glRasterPos4i" },
	{ (void **)&qglRasterPos4iv, "glRasterPos4iv" },
	{ (void **)&qglRasterPos4s, "glRasterPos4s" },
	{ (void **)&qglRasterPos4sv, "glRasterPos4sv" },
	{ (void **)&qglReadBuffer, "glReadBuffer" },
	{ (void **)&qglReadPixels, "glReadPixels" },
	{ (void **)&qglRectd, "glRectd" },
	{ (void **)&qglRectdv, "glRectdv" },
	{ (void **)&qglRectf, "glRectf" },
	{ (void **)&qglRectfv, "glRectfv" },
	{ (void **)&qglRecti, "glRecti" },
	{ (void **)&qglRectiv, "glRectiv" },
	{ (void **)&qglRects, "glRects" },
	{ (void **)&qglRectsv, "glRectsv" },
	{ (void **)&qglRenderMode, "glRenderMode" },
	{ (void **)&qglRotated, "glRotated" },
	{ (void **)&qglRotatef, "glRotatef" },
	{ (void **)&qglScaled, "glScaled" },
	{ (void **)&qglScalef, "glScalef" },
	{ (void **)&qglScissor, "glScissor" },
	{ (void **)&qglSelectBuffer, "glSelectBuffer" },
	{ (void **)&qglShadeModel, "glShadeModel" },
	{ (void **)&qglStencilFunc, "glStencilFunc" },
	{ (void **)&qglStencilMask, "glStencilMask" },
	{ (void **)&qglStencilOp, "glStencilOp" },
	{ (void **)&qglTexCoord1d, "glTexCoord1d" },
	{ (void **)&qglTexCoord1dv, "glTexCoord1dv" },
	{ (void **)&qglTexCoord1f, "glTexCoord1f" },
	{ (void **)&qglTexCoord1fv, "glTexCoord1fv" },
	{ (void **)&qglTexCoord1i, "glTexCoord1i" },
	{ (void **)&qglTexCoord1iv, "glTexCoord1iv" },
	{ (void **)&qglTexCoord1s, "glTexCoord1s" },
	{ (void **)&qglTexCoord1sv, "glTexCoord1sv" },
	{ (void **)&qglTexCoord2d, "glTexCoord2d" },
	{ (void **)&qglTexCoord2dv, "glTexCoord2dv" },
	{ (void **)&qglTexCoord2f, "glTexCoord2f" },
	{ (void **)&qglTexCoord2fv, "glTexCoord2fv" },
	{ (void **)&qglTexCoord2i, "glTexCoord2i" },
	{ (void **)&qglTexCoord2iv, "glTexCoord2iv" },
	{ (void **)&qglTexCoord2s, "glTexCoord2s" },
	{ (void **)&qglTexCoord2sv, "glTexCoord2sv" },
	{ (void **)&qglTexCoord3d, "glTexCoord3d" },
	{ (void **)&qglTexCoord3dv, "glTexCoord3dv" },
	{ (void **)&qglTexCoord3f, "glTexCoord3f" },
	{ (void **)&qglTexCoord3fv, "glTexCoord3fv" },
	{ (void **)&qglTexCoord3i, "glTexCoord3i" },
	{ (void **)&qglTexCoord3iv, "glTexCoord3iv" },
	{ (void **)&qglTexCoord3s, "glTexCoord3s" },
	{ (void **)&qglTexCoord3sv, "glTexCoord3sv" },
	{ (void **)&qglTexCoord4d, "glTexCoord4d" },
	{ (void **)&qglTexCoord4dv, "glTexCoord4dv" },
	{ (void **)&qglTexCoord4f, "glTexCoord4f" },
	{ (void **)&qglTexCoord4fv, "glTexCoord4fv" },
	{ (void **)&qglTexCoord4i, "glTexCoord4i" },
	{ (void **)&qglTexCoord4iv, "glTexCoord4iv" },
	{ (void **)&qglTexCoord4s, "glTexCoord4s" },
	{ (void **)&qglTexCoord4sv, "glTexCoord4sv" },
	{ (void **)&qglTexCoordPointer, "glTexCoordPointer" },
	{ (void **)&qglTexEnvf, "glTexEnvf" },
	{ (void **)&qglTexEnvfv, "glTexEnvfv" },
	{ (void **)&qglTexEnvi, "glTexEnvi" },
	{ (void **)&qglTexEnviv, "glTexEnviv" },
	{ (void **)&qglTexGend, "glTexGend" },
	{ (void **)&qglTexGendv, "glTexGendv" },
	{ (void **)&qglTexGenf, "glTexGenf" },
	{ (void **)&qglTexGenfv, "glTexGenfv" },
	{ (void **)&qglTexGeni, "glTexGeni" },
	{ (void **)&qglTexGeniv, "glTexGeniv" },
	{ (void **)&qglTexImage1D, "glTexImage1D" },
	{ (void **)&qglTexImage2D, "glTexImage2D" },
	{ (void **)&qglTexParameterf, "glTexParameterf" },
	{ (void **)&qglTexParameterfv, "glTexParameterfv" },
	{ (void **)&qglTexParameteri, "glTexParameteri" },
	{ (void **)&qglTexParameteriv, "glTexParameteriv" },
	{ (void **)&qglTexSubImage1D, "glTexSubImage1D" },
	{ (void **)&qglTexSubImage2D, "glTexSubImage2D" },
	{ (void **)&qglTranslated, "glTranslated" },
	{ (void **)&qglTranslatef, "glTranslatef" },
	{ (void **)&qglVertex2d, "glVertex2d" },
	{ (void **)&qglVertex2dv, "glVertex2dv" },
	{ (void **)&qglVertex2f, "glVertex2f" },
	{ (void **)&qglVertex2fv, "glVertex2fv" },
	{ (void **)&qglVertex2i, "glVertex2i" },
	{ (void **)&qglVertex2iv, "glVertex2iv" },
	{ (void **)&qglVertex2s, "glVertex2s" },
	{ (void **)&qglVertex2sv, "glVertex2sv" },
	{ (void **)&qglVertex3d, "glVertex3d" },
	{ (void **)&qglVertex3dv, "glVertex3dv" },
	{ (void **)&qglVertex3f, "glVertex3f" },
	{ (void **)&qglVertex3fv, "glVertex3fv" },
	{ (void **)&qglVertex3i, "glVertex3i" },
	{ (void **)&qglVertex3iv, "glVertex3iv" },
	{ (void **)&qglVertex3s, "glVertex3s" },
	{ (void **)&qglVertex3sv, "glVertex3sv" },
	{ (void **)&qglVertex4d, "glVertex4d" },
	{ (void **)&qglVertex4dv, "glVertex4dv" },
	{ (void **)&qglVertex4f, "glVertex4f" },
	{ (void **)&qglVertex4fv, "glVertex4fv" },
	{ (void **)&qglVertex4i, "glVertex4i" },
	{ (void **)&qglVertex4iv, "glVertex4iv" },
	{ (void **)&qglVertex4s, "glVertex4s" },
	{ (void **)&qglVertex4sv, "glVertex4sv" },
	{ (void **)&qglVertexPointer, "glVertexPointer" },
	{ (void **)&qglViewport, "glViewport" },
};

/*
** QGL_Init
**
** Resolves every core GL function; needs a current GL context.
*/
qboolean QGL_Init( void )
{
	int		i;

	for ( i = 0 ; i < sizeof( qglProcs ) / sizeof( qglProcs[0] ) ; i++ ) {
		*qglProcs[i].ptr = SDL_GL_GetProcAddress( qglProcs[i].name );
		if ( !*qglProcs[i].ptr ) {
			ri.Printf( PRINT_ALL, "QGL_Init: missing %s\n", qglProcs[i].name );
			return qfalse;
		}
	}

	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	qglMultiTexCoord2fARB = NULL;
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;

	return qtrue;
}

/*
** QGL_Shutdown
*/
void QGL_Shutdown( void )
{
	int		i;

	for ( i = 0 ; i < sizeof( qglProcs ) / sizeof( qglProcs[0] ) ; i++ ) {
		*qglProcs[i].ptr = NULL;
	}

	qglActiveTextureARB = NULL;
	qglClientActiveTextureARB = NULL;
	qglMultiTexCoord2fARB = NULL;
	qglLockArraysEXT = NULL;
	qglUnlockArraysEXT = NULL;
}
