// Private capture-only GLFW adapter: select an EGL device without changing the renderer.
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <cstring>

namespace {
EGLDisplay display=EGL_NO_DISPLAY;
EGLContext context=EGL_NO_CONTEXT;
EGLSurface surface=EGL_NO_SURFACE;
int width=0,height=0,major=4,minor=3,samples=4;
char token;
GLFWwindow* window=reinterpret_cast<GLFWwindow*>(&token);
void* user=nullptr;
const auto start=std::chrono::steady_clock::now();
void release() {
    if(display!=EGL_NO_DISPLAY) {
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
        if(context!=EGL_NO_CONTEXT) eglDestroyContext(display,context);
        if(surface!=EGL_NO_SURFACE) eglDestroySurface(display,surface);
        eglTerminate(display);
    }
    display=EGL_NO_DISPLAY;context=EGL_NO_CONTEXT;surface=EGL_NO_SURFACE;
}
bool create(int w,int h) {
    const char* selected=std::getenv("PLANET_EGL_DEVICE");
    if(!selected) return false;
    char* end=nullptr;long index=std::strtol(selected,&end,10);
    if(!end || *end || index<0 || index>=16) return false;
    const auto devices=reinterpret_cast<PFNEGLQUERYDEVICESEXTPROC>(eglGetProcAddress("eglQueryDevicesEXT"));
    const auto platform=reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(eglGetProcAddress("eglGetPlatformDisplayEXT"));
    EGLDeviceEXT entries[16];EGLint count=0;
    if(!devices || !platform || !devices(16,entries,&count) || index>=count) return false;
    display=platform(EGL_PLATFORM_DEVICE_EXT,entries[index],nullptr);
    EGLint a,b;
    if(display==EGL_NO_DISPLAY || !eglInitialize(display,&a,&b)) return false;
    const EGLint attrs[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_BIT,
        EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,
        EGL_DEPTH_SIZE,24,EGL_STENCIL_SIZE,8,EGL_SAMPLE_BUFFERS,samples>0?1:0,EGL_SAMPLES,samples,EGL_NONE};
    EGLConfig config;EGLint configs=0;
    if(!eglChooseConfig(display,attrs,&config,1,&configs) || !configs || !eglBindAPI(EGL_OPENGL_API)) return false;
    const EGLint size[]={EGL_WIDTH,w,EGL_HEIGHT,h,EGL_NONE};
    surface=eglCreatePbufferSurface(display,config,size);
    const EGLint version[]={EGL_CONTEXT_MAJOR_VERSION_KHR,major,EGL_CONTEXT_MINOR_VERSION_KHR,minor,
        EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR,EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR,EGL_NONE};
    context=eglCreateContext(display,config,EGL_NO_CONTEXT,version);
    if(surface==EGL_NO_SURFACE || context==EGL_NO_CONTEXT || !eglMakeCurrent(display,surface,surface,context)) return false;
    width=w;height=h;
    const auto binary=reinterpret_cast<PFNEGLQUERYDEVICEBINARYEXTPROC>(eglGetProcAddress("eglQueryDeviceBinaryEXT"));
    unsigned char uuid[16];EGLint bytes=0;
    if(!binary || !binary(entries[index],EGL_DEVICE_UUID_EXT,sizeof(uuid),uuid,&bytes) || bytes!=16) return false;
    std::fprintf(stderr,"EGL device UUID: GPU-");
    for(int i=0;i<16;++i) {
        if(i==4 || i==6 || i==8 || i==10) std::fprintf(stderr,"-");
        std::fprintf(stderr,"%02x",uuid[i]);
    }
    std::fprintf(stderr,"\n");
    EGLint red=0,green=0,blue=0,alpha=0,depth=0,stencil=0,actualSamples=0;
    eglGetConfigAttrib(display,config,EGL_RED_SIZE,&red);eglGetConfigAttrib(display,config,EGL_GREEN_SIZE,&green);
    eglGetConfigAttrib(display,config,EGL_BLUE_SIZE,&blue);eglGetConfigAttrib(display,config,EGL_ALPHA_SIZE,&alpha);
    eglGetConfigAttrib(display,config,EGL_DEPTH_SIZE,&depth);eglGetConfigAttrib(display,config,EGL_STENCIL_SIZE,&stencil);
    eglGetConfigAttrib(display,config,EGL_SAMPLES,&actualSamples);
    std::fprintf(stderr,"EGL config: RGBA %d/%d/%d/%d; depth %d; stencil %d; samples %d\n",
        red,green,blue,alpha,depth,stencil,actualSamples);
    if(red!=8 || green!=8 || blue!=8 || alpha!=8 || depth!=24 || stencil!=8 || actualSamples!=samples) return false;
    std::fprintf(stderr,"EGL capture device %ld: %s; %s; viewport %dx%d; requested samples %d\n",
        index,glGetString(GL_RENDERER),glGetString(GL_VERSION),w,h,samples);
    return true;
}
}
extern "C" {
int glfwInit() {return GLFW_TRUE;}
void glfwTerminate() {release();}
void glfwWindowHint(int hint,int value) {
    if(hint==GLFW_CONTEXT_VERSION_MAJOR) major=value;
    if(hint==GLFW_CONTEXT_VERSION_MINOR) minor=value;
    if(hint==GLFW_SAMPLES) samples=value;
}
GLFWwindow* glfwCreateWindow(int w,int h,const char* title,GLFWmonitor*,GLFWwindow*) {
    if(std::strcmp(title,"PlanetSimulation Render Test")) {
        std::fprintf(stderr,"EGL adapter supports capture mode only\n");return nullptr;
    }
    if(create(w,h)) return window;
    std::fprintf(stderr,"EGL capture creation failed: 0x%x\n",eglGetError());release();return nullptr;
}
void glfwDestroyWindow(GLFWwindow*) {release();}
void glfwMakeContextCurrent(GLFWwindow* w) {
    const auto ok=w ? eglMakeCurrent(display,surface,surface,context) :
        eglMakeCurrent(display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
    if(!ok) {std::fprintf(stderr,"EGL make-current failed\n");std::abort();}
}
GLFWwindow* glfwGetCurrentContext() {return eglGetCurrentContext()!=EGL_NO_CONTEXT?window:nullptr;}
void glfwSwapInterval(int interval) {eglSwapInterval(display,interval);}
void glfwSwapBuffers(GLFWwindow*) {
    if(!eglSwapBuffers(display,surface)) {std::fprintf(stderr,"EGL swap failed\n");std::abort();}
}
void glfwPollEvents() {}
double glfwGetTime() {return std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();}
void glfwGetFramebufferSize(GLFWwindow*,int* w,int* h) {*w=width;*h=height;}
void glfwSetWindowAttrib(GLFWwindow*,int,int) {}
void glfwSetInputMode(GLFWwindow*,int,int) {}
void glfwSetWindowUserPointer(GLFWwindow*,void* value) {user=value;}
void* glfwGetWindowUserPointer(GLFWwindow*) {return user;}
GLFWmousebuttonfun glfwSetMouseButtonCallback(GLFWwindow*,GLFWmousebuttonfun) {return nullptr;}
GLFWcursorposfun glfwSetCursorPosCallback(GLFWwindow*,GLFWcursorposfun) {return nullptr;}
GLFWscrollfun glfwSetScrollCallback(GLFWwindow*,GLFWscrollfun) {return nullptr;}
GLFWkeyfun glfwSetKeyCallback(GLFWwindow*,GLFWkeyfun) {return nullptr;}
GLenum glewInit() {
    const auto initialize=reinterpret_cast<decltype(&glewInit)>(dlsym(RTLD_NEXT,"glewInit"));
    if(!initialize) return GLEW_ERROR_NO_GL_VERSION;
    const auto result=initialize();
    // Debian's GLX GLEW initializes GL entry points before rejecting EGL's lack
    // of a GLX display. Only accept that specific result with real GL 4.3 loaded.
    if(result==GLEW_ERROR_NO_GLX_DISPLAY && eglGetCurrentContext()!=EGL_NO_CONTEXT &&
       GLEW_VERSION_4_3 && __glewDispatchCompute && __glewQueryCounter) {
        std::fprintf(stderr,"GLEW GL initialized; GLX-display-only check bypassed for EGL\n");
        return GLEW_OK;
    }
    return result;
}
}
