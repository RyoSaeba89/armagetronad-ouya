/*
 * Minimal GLU implementation for the Armagetron Advanced OUYA port.
 *
 * gl4es ships <GL/glu.h> (prototypes) but NOT a GLU implementation. Armagetron
 * uses only a handful of GLU entry points; this shim provides real
 * implementations for those, plus link-satisfying stubs for the GLU
 * tessellator that FTGL's FTVectoriser references (only exercised by
 * polygon/outline fonts, which the OUYA build never selects because
 * FONT_TYPE is forced to sr_fontTexture).
 *
 * All matrix/geometry work is emitted through gl4es' immediate-mode API,
 * which gl4es translates to GLES2.
 */

#include <GL/gl.h>
#include <GL/glu.h>
#include <math.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- helpers -------------------------------------------------------------- */

static void normalize3(double v[3]) {
    double len = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len > 0.0) { v[0] /= len; v[1] /= len; v[2] /= len; }
}
static void cross3(const double a[3], const double b[3], double out[3]) {
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

/* ---- gluPerspective ------------------------------------------------------- */

void GLAPIENTRY gluPerspective(GLdouble fovy, GLdouble aspect,
                               GLdouble zNear, GLdouble zFar) {
    double f = 1.0 / tan((fovy * (M_PI / 180.0)) / 2.0);
    GLfloat m[16];
    int i;
    for (i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0]  = (GLfloat)(f / aspect);
    m[5]  = (GLfloat)f;
    m[10] = (GLfloat)((zFar + zNear) / (zNear - zFar));
    m[11] = -1.0f;
    m[14] = (GLfloat)((2.0 * zFar * zNear) / (zNear - zFar));
    glMultMatrixf(m);
}

/* ---- gluLookAt ------------------------------------------------------------ */

void GLAPIENTRY gluLookAt(GLdouble eyeX, GLdouble eyeY, GLdouble eyeZ,
                          GLdouble centerX, GLdouble centerY, GLdouble centerZ,
                          GLdouble upX, GLdouble upY, GLdouble upZ) {
    double fwd[3] = { centerX - eyeX, centerY - eyeY, centerZ - eyeZ };
    double up[3]  = { upX, upY, upZ };
    double side[3], u[3];
    GLfloat m[16];

    normalize3(fwd);
    cross3(fwd, up, side);
    normalize3(side);
    cross3(side, fwd, u);

    m[0] = (GLfloat)side[0]; m[4] = (GLfloat)side[1]; m[8]  = (GLfloat)side[2]; m[12] = 0.0f;
    m[1] = (GLfloat)u[0];    m[5] = (GLfloat)u[1];    m[9]  = (GLfloat)u[2];    m[13] = 0.0f;
    m[2] = -(GLfloat)fwd[0]; m[6] = -(GLfloat)fwd[1]; m[10] = -(GLfloat)fwd[2]; m[14] = 0.0f;
    m[3] = 0.0f;             m[7] = 0.0f;             m[11] = 0.0f;            m[15] = 1.0f;

    glMultMatrixf(m);
    glTranslatef((GLfloat)-eyeX, (GLfloat)-eyeY, (GLfloat)-eyeZ);
}

/* ---- gluBuild2DMipmaps ---------------------------------------------------- */

GLint GLAPIENTRY gluBuild2DMipmaps(GLenum target, GLint internalFormat,
                                   GLsizei width, GLsizei height,
                                   GLenum format, GLenum type,
                                   const void *data) {
    /* Armagetron textures are power-of-two; let gl4es build the mip chain.
       GL_GENERATE_MIPMAP is honoured by gl4es' fixed-function path. */
    glTexParameteri(target, GL_GENERATE_MIPMAP, GL_TRUE);
    glTexImage2D(target, 0, internalFormat, width, height, 0, format, type, data);
    return 0;
}

/* ---- gluErrorString ------------------------------------------------------- */

const GLubyte * GLAPIENTRY gluErrorString(GLenum error) {
    switch (error) {
    case GL_NO_ERROR:          return (const GLubyte *)"no error";
    case GL_INVALID_ENUM:      return (const GLubyte *)"invalid enumerant";
    case GL_INVALID_VALUE:     return (const GLubyte *)"invalid value";
    case GL_INVALID_OPERATION: return (const GLubyte *)"invalid operation";
    case GL_OUT_OF_MEMORY:     return (const GLubyte *)"out of memory";
    default:                   return (const GLubyte *)"unknown error";
    }
}

/* ---- quadric / gluSphere -------------------------------------------------- */

struct GLUquadric { int normals; int texture; int orientation; int drawStyle; };

GLUquadric * GLAPIENTRY gluNewQuadric(void) {
    GLUquadric *q = (GLUquadric *)malloc(sizeof(GLUquadric));
    if (q) { q->normals = 1; q->texture = 0; q->orientation = 0; q->drawStyle = 0; }
    return q;
}
void GLAPIENTRY gluDeleteQuadric(GLUquadric *quad) { free(quad); }

/* no-op quadric setters (declared by glu.h; harmless if Armagetron calls them) */
void GLAPIENTRY gluQuadricNormals(GLUquadric *q, GLenum n)    { if (q) q->normals = (n != GL_NONE); }
void GLAPIENTRY gluQuadricTexture(GLUquadric *q, GLboolean t) { if (q) q->texture = t; }
void GLAPIENTRY gluQuadricOrientation(GLUquadric *q, GLenum o){ if (q) q->orientation = o; }
void GLAPIENTRY gluQuadricDrawStyle(GLUquadric *q, GLenum d)  { if (q) q->drawStyle = d; }

void GLAPIENTRY gluSphere(GLUquadric *quad, GLdouble radius,
                          GLint slices, GLint stacks) {
    int i, j;
    if (slices < 2) slices = 2;
    if (stacks < 2) stacks = 2;
    (void)quad;
    for (j = 0; j < stacks; ++j) {
        double lat0 = M_PI * (-0.5 + (double)j       / stacks);
        double lat1 = M_PI * (-0.5 + (double)(j + 1) / stacks);
        double z0 = sin(lat0), zr0 = cos(lat0);
        double z1 = sin(lat1), zr1 = cos(lat1);
        glBegin(GL_TRIANGLE_STRIP);
        for (i = 0; i <= slices; ++i) {
            double lng = 2.0 * M_PI * (double)i / slices;
            double x = cos(lng), y = sin(lng);
            glNormal3f((GLfloat)(x*zr0), (GLfloat)(y*zr0), (GLfloat)z0);
            glVertex3f((GLfloat)(x*zr0*radius), (GLfloat)(y*zr0*radius), (GLfloat)(z0*radius));
            glNormal3f((GLfloat)(x*zr1), (GLfloat)(y*zr1), (GLfloat)z1);
            glVertex3f((GLfloat)(x*zr1*radius), (GLfloat)(y*zr1*radius), (GLfloat)(z1*radius));
        }
        glEnd();
    }
}

/* ---- GLU tessellator: link-satisfying stubs ------------------------------- *
 * Referenced by FTGL's FTVectoriser (polygon/outline/extruded fonts). The OUYA
 * build forces FONT_TYPE=sr_fontTexture, so these are never invoked at runtime;
 * they exist only so the static FTGL objects link. */

GLUtesselator * GLAPIENTRY gluNewTess(void) { return NULL; }
void GLAPIENTRY gluDeleteTess(GLUtesselator *t) { (void)t; }
void GLAPIENTRY gluTessBeginPolygon(GLUtesselator *t, GLvoid *d) { (void)t; (void)d; }
void GLAPIENTRY gluTessBeginContour(GLUtesselator *t) { (void)t; }
void GLAPIENTRY gluTessVertex(GLUtesselator *t, GLdouble c[3], GLvoid *d) { (void)t; (void)c; (void)d; }
void GLAPIENTRY gluTessEndContour(GLUtesselator *t) { (void)t; }
void GLAPIENTRY gluTessEndPolygon(GLUtesselator *t) { (void)t; }
void GLAPIENTRY gluTessCallback(GLUtesselator *t, GLenum w, _GLUfuncptr f) { (void)t; (void)w; (void)f; }
void GLAPIENTRY gluTessProperty(GLUtesselator *t, GLenum w, GLdouble v) { (void)t; (void)w; (void)v; }
void GLAPIENTRY gluTessNormal(GLUtesselator *t, GLdouble x, GLdouble y, GLdouble z) { (void)t; (void)x; (void)y; (void)z; }
