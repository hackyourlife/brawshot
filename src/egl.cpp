#include <cstdio>
#include <cstdlib>
#include <GL/gl.h>
#include <EGL/egl.h>

#include "egl.h"

static const EGLint configAttribs[] = {
	EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
	EGL_BLUE_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_RED_SIZE, 8,
	EGL_DEPTH_SIZE, 8,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
	EGL_NONE
};

EGL::EGL()
{
	if(!init()) {
		exit(1);
	}
}

EGL::~EGL()
{
	// Terminate EGL when finished
	eglTerminate(display);
}

bool EGL::init()
{
	// Initialize EGL
	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	if(error()) {
		return false;
	}

	eglInitialize(display, &major, &minor);

	if(error()) {
		return false;
	}

	// Select an appropriate configuration
	EGLint numConfigs;
	EGLConfig eglCfg;

	eglChooseConfig(display, configAttribs, &eglCfg, 1, &numConfigs);

	if(error()) {
		return false;
	}

	// Bind the API
	eglBindAPI(EGL_OPENGL_API);

	if(error()) {
		return false;
	}

	// Create a context and make it current
	context = eglCreateContext(display, eglCfg, EGL_NO_CONTEXT, NULL);

	if(error()) {
		return false;
	}

	return true;
}

bool EGL::make_current()
{
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, context);

	return !error();
}

void EGL::unbind()
{
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	error();
}

const char* EGL::error_msg(EGLint err)
{
	switch(err) {
		case EGL_SUCCESS:
			return "The last function succeeded without error.";

		case EGL_NOT_INITIALIZED:
			return "EGL is not initialized, or could not be initialized, for the specified EGL display connection.";

		case EGL_BAD_ACCESS:
			return "EGL cannot access a requested resource (for example a context is bound in another thread).";

		case EGL_BAD_ALLOC:
			return "EGL failed to allocate resources for the requested operation.";

		case EGL_BAD_ATTRIBUTE:
			return "An unrecognized attribute or attribute value was passed in the attribute list.";

		case EGL_BAD_CONTEXT:
			return "An EGLContext argument does not name a valid EGL rendering context.";

		case EGL_BAD_CONFIG:
			return "An EGLConfig argument does not name a valid EGL frame buffer configuration.";

		case EGL_BAD_CURRENT_SURFACE:
			return "The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid.";

		case EGL_BAD_DISPLAY:
			return "An EGLDisplay argument does not name a valid EGL display connection.";

		case EGL_BAD_SURFACE:
			return "An EGLSurface argument does not name a valid surface (window, pixel buffer or pixmap) configured for GL rendering.";

		case EGL_BAD_MATCH:
			return "Arguments are inconsistent (for example, a valid context requires buffers not supplied by a valid surface).";

		case EGL_BAD_PARAMETER:
			return "One or more argument values are invalid.";

		case EGL_BAD_NATIVE_PIXMAP:
			return "A NativePixmapType argument does not refer to a valid native pixmap.";

		case EGL_BAD_NATIVE_WINDOW:
			return "A NativeWindowType argument does not refer to a valid native window.";

		case EGL_CONTEXT_LOST:
			return "A power management event has occurred. The application must destroy all contexts and reinitialise OpenGL ES state and objects to continue rendering.";

		default:
			return nullptr;
	}
}

bool EGL::error()
{
	EGLint err = eglGetError();
	if(err == EGL_SUCCESS) {
		return false;
	} else {
		const char* msg = error_msg(err);
		if(msg) {
			printf("EGL error: %s\n", msg);
			return true;
		} else {
			printf("EGL error: 0x%X\n", err);
			return true;
		}
	}
}

void EGL::info()
{
	const unsigned char* gl_vendor = glGetString(GL_VENDOR);
	const unsigned char* gl_renderer = glGetString(GL_RENDERER);
	const unsigned char* gl_version = glGetString(GL_VERSION);
	const unsigned char* gl_glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);

	printf("EGL Version:  %d.%d\n", major, minor);
	printf("GL Vendor:    %s\n", gl_vendor);
	printf("GL Renderer:  %s\n", gl_renderer);
	printf("GL Version:   %s\n", gl_version);
	printf("GLSL Version: %s\n", gl_glsl_version);
}
