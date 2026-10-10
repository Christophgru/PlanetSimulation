// Private timing adapter: real GLFW/X11 events and input, UUID-selected NVIDIA
// EGL pbuffer rendering. This measures the production interactive loop without
// a desktop/display-refresh claim. Capture adapter supplies audited EGL setup.
#define glfwInit capture_glfwInit
#define glfwTerminate capture_glfwTerminate
#define glfwWindowHint capture_glfwWindowHint
#define glfwCreateWindow capture_glfwCreateWindow
#define glfwDestroyWindow capture_glfwDestroyWindow
#define glfwMakeContextCurrent capture_glfwMakeContextCurrent
#define glfwGetCurrentContext capture_glfwGetCurrentContext
#define glfwSwapInterval capture_glfwSwapInterval
#define glfwSwapBuffers capture_glfwSwapBuffers
#define glfwPollEvents capture_glfwPollEvents
#define glfwGetTime capture_glfwGetTime
#define glfwGetFramebufferSize capture_glfwGetFramebufferSize
#define glfwSetWindowAttrib capture_glfwSetWindowAttrib
#define glfwSetInputMode capture_glfwSetInputMode
#define glfwSetWindowUserPointer capture_glfwSetWindowUserPointer
#define glfwGetWindowUserPointer capture_glfwGetWindowUserPointer
#define glfwSetMouseButtonCallback capture_glfwSetMouseButtonCallback
#define glfwSetCursorPosCallback capture_glfwSetCursorPosCallback
#define glfwSetScrollCallback capture_glfwSetScrollCallback
#define glfwSetKeyCallback capture_glfwSetKeyCallback
#include "../gpu_compare/EglCapture.cpp"
#undef glfwInit
#undef glfwTerminate
#undef glfwWindowHint
#undef glfwCreateWindow
#undef glfwDestroyWindow
#undef glfwMakeContextCurrent
#undef glfwGetCurrentContext
#undef glfwSwapInterval
#undef glfwSwapBuffers
#undef glfwPollEvents
#undef glfwGetTime
#undef glfwGetFramebufferSize
#undef glfwSetWindowAttrib
#undef glfwSetInputMode
#undef glfwSetWindowUserPointer
#undef glfwGetWindowUserPointer
#undef glfwSetMouseButtonCallback
#undef glfwSetCursorPosCallback
#undef glfwSetScrollCallback
#undef glfwSetKeyCallback

namespace {
template<class F> F next(const char* name) {
    auto function=reinterpret_cast<F>(dlsym(RTLD_NEXT,name));
    if(!function) std::abort();return function;
}
}
extern "C" {
int glfwInit() {return next<decltype(&capture_glfwInit)>("glfwInit")();}
void glfwTerminate() {release();next<decltype(&capture_glfwTerminate)>("glfwTerminate")();}
void glfwWindowHint(int hint,int value) {
    capture_glfwWindowHint(hint,value);
    next<decltype(&capture_glfwWindowHint)>("glfwWindowHint")(hint,value);
}
GLFWwindow* glfwCreateWindow(int w,int h,const char* title,GLFWmonitor* monitor,GLFWwindow* shared) {
    next<decltype(&capture_glfwWindowHint)>("glfwWindowHint")(GLFW_CLIENT_API,GLFW_NO_API);
    auto native=next<decltype(&capture_glfwCreateWindow)>("glfwCreateWindow")(w,h,title,monitor,shared);
    if(!native) return nullptr;
    if(!create(w,h)) {
        next<decltype(&capture_glfwDestroyWindow)>("glfwDestroyWindow")(native);release();return nullptr;
    }
    window=native;return native;
}
void glfwDestroyWindow(GLFWwindow* w) {release();next<decltype(&capture_glfwDestroyWindow)>("glfwDestroyWindow")(w);}
void glfwMakeContextCurrent(GLFWwindow* w) {capture_glfwMakeContextCurrent(w);}
GLFWwindow* glfwGetCurrentContext() {return capture_glfwGetCurrentContext();}
void glfwSwapInterval(int interval) {capture_glfwSwapInterval(interval);}
void glfwSwapBuffers(GLFWwindow* w) {capture_glfwSwapBuffers(w);}
}
