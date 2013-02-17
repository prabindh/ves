/*========================================================================
  VES --- VTK OpenGL ES Rendering Toolkit

      http://www.kitware.com/ves

  Copyright 2011 Kitware, Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
 ========================================================================*/
/// \class vesKiwiX11TestDriver
/// \ingroup ves

#include "vesKiwiTestDriver.h"

#include <GLES/gl.h>
#include <GLES/egl.h>

class vesKiwiX11TestDriver : public vesKiwiTestDriver
{
public:
  vesKiwiX11TestDriver(vesKiwiBaseApp* app) : vesKiwiTestDriver(app),
    width(800),
    height(600)
  {
    dpyName = NULL;
    printInfo = GL_FALSE;

    haveLastMotion = false;
    lastMotionX = 0;
    lastMotionY = 0;
    currentX = 0;
    currentY = 0;
  }

  void make_x_window(Display *x_dpy, EGLDisplay egl_dpy,
                     const char *name,
                     int x, int y, int width, int height,
                     Window *winRet,
                     EGLContext *ctxRet,
                     EGLSurface *surfRet)
  {
     static const EGLint attribs[] = {
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_DEPTH_SIZE, 1,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_NONE
     };
     static const EGLint ctx_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 1,
        EGL_NONE
     };
     int scrnum;
     XSetWindowAttributes attr;
     unsigned long mask;
     Window root;
     Window win;
     XVisualInfo *visInfo, visTemplate;
     int num_visuals;
     EGLContext ctx;
     EGLConfig config;
     EGLint num_configs;
     EGLint vid;

     scrnum = DefaultScreen( x_dpy );
     root = RootWindow( x_dpy, scrnum );

     if (!eglChooseConfig( egl_dpy, attribs, &config, 1, &num_configs)) {
        printf("Error: couldn't get an EGL visual config\n");
        exit(1);
     }

     assert(config);
     assert(num_configs > 0);

     if (!eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid)) {
        printf("Error: eglGetConfigAttrib() failed\n");
        exit(1);
     }

     /* The X window visual must match the EGL config */
     visTemplate.visualid = vid;
     visInfo = XGetVisualInfo(x_dpy, VisualIDMask, &visTemplate, &num_visuals);
     if (!visInfo) {
        printf("Error: couldn't get X visual\n");
        exit(1);
     }

     /* window attributes */
     attr.background_pixel = 0;
     attr.border_pixel = 0;
     attr.colormap = XCreateColormap( x_dpy, root, visInfo->visual, AllocNone);
     attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | ButtonMotionMask;
     mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

     win = XCreateWindow( x_dpy, root, 0, 0, width, height,
                          0, visInfo->depth, InputOutput,
                          visInfo->visual, mask, &attr );

     /* set hints and properties */
     {
        XSizeHints sizehints;
        sizehints.x = x;
        sizehints.y = y;
        sizehints.width  = width;
        sizehints.height = height;
        sizehints.flags = USSize | USPosition;
        XSetNormalHints(x_dpy, win, &sizehints);
        XSetStandardProperties(x_dpy, win, name, name,
                                None, (char **)NULL, 0, &sizehints);
     }

  #if USE_FULL_GL /* XXX fix this when eglBindAPI() works */
     eglBindAPI(EGL_OPENGL_API);
  #else
     eglBindAPI(EGL_OPENGL_ES_API);
  #endif

     ctx = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, ctx_attribs );
     if (!ctx) {
        printf("Error: eglCreateContext failed\n");
        exit(1);
     }

     /* test eglQueryContext() */
     {
        EGLint val;
        eglQueryContext(egl_dpy, ctx, EGL_CONTEXT_CLIENT_VERSION, &val);
        assert(val == 1);
     }

     *surfRet = eglCreateWindowSurface(egl_dpy, config, win, NULL);
     if (!*surfRet) {
        printf("Error: eglCreateWindowSurface failed\n");
        exit(1);
     }

     /* sanity checks */
     {
        EGLint val;
        eglQuerySurface(egl_dpy, *surfRet, EGL_WIDTH, &val);
        assert(val == width);
        eglQuerySurface(egl_dpy, *surfRet, EGL_HEIGHT, &val);
        assert(val == height);
        assert(eglGetConfigAttrib(egl_dpy, config, EGL_SURFACE_TYPE, &val));
        assert(val & EGL_WINDOW_BIT);
     }

     XFree(visInfo);

     *winRet = win;
     *ctxRet = ctx;
  }


  virtual int init() {
    x_dpy = XOpenDisplay(dpyName);
    if (!x_dpy) {
      printf("Error: couldn't open display %s\n",
             dpyName ? dpyName : getenv("DISPLAY"));
      return -1;
    }

    egl_dpy = eglGetDisplay(x_dpy);
    if (!egl_dpy) {
      printf("Error: eglGetDisplay() failed\n");
      return -1;
    }

    if (!eglInitialize(egl_dpy, &egl_major, &egl_minor)) {
      printf("Error: eglInitialize() failed\n");
      return -1;
    }

    s = eglQueryString(egl_dpy, EGL_VERSION);
    printf("EGL_VERSION = %s\n", s);

    s = eglQueryString(egl_dpy, EGL_VENDOR);
    printf("EGL_VENDOR = %s\n", s);

    s = eglQueryString(egl_dpy, EGL_EXTENSIONS);
    printf("EGL_EXTENSIONS = %s\n", s);

    s = eglQueryString(egl_dpy, EGL_CLIENT_APIS);
    printf("EGL_CLIENT_APIS = %s\n", s);

    make_x_window(x_dpy, egl_dpy,
                 "OpenGL ES 1.x tri", 0, 0, width, height,
                 &win, &egl_ctx, &egl_surf);

    XMapWindow(x_dpy, win);
    if (!eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx)) {
      printf("Error: eglMakeCurrent() failed\n");
      return -1;
    }

    if (printInfo) {
      printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
      printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
      printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
      printf("GL_EXTENSIONS = %s\n", (char *) glGetString(GL_EXTENSIONS));
    }
  }

  void render() {
    this->m_test->render();
  }

  void start() {
    this->event_loop(x_dpy, win, egl_dpy, egl_surf);
  }

  void finalize() {
    eglDestroyContext(egl_dpy, egl_ctx);
    eglDestroySurface(egl_dpy, egl_surf);
    eglTerminate(egl_dpy);

    XDestroyWindow(x_dpy, win);
    XCloseDisplay(x_dpy);
  }

  void event_loop(Display *dpy, Window win,
    EGLDisplay egl_dpy, EGLSurface egl_surf)
  {
    vesNotUsed(win);

     while (1) {
        int redraw = 0;
        XEvent event;

        XNextEvent(dpy, &event);

        switch (event.type) {
        case Expose:
           redraw = 1;
           break;
        case ConfigureNotify:
           m_test->resizeView(event.xconfigure.width, event.xconfigure.height);
           break;

        case ButtonPress:
          m_test->handleSingleTouchDown(event.xbutton.x, event.xbutton.y);

          haveLastMotion = true;
          lastMotionX = event.xbutton.x;
          lastMotionY = event.xbutton.y;
          break;

        case ButtonRelease:
          m_test->handleSingleTouchUp();
          haveLastMotion = false;
          break;

        case MotionNotify:

          if (haveLastMotion) {
            currentX = event.xmotion.x;
            currentY = event.xmotion.y;
            m_test->handleSingleTouchPanGesture(currentX - lastMotionX, currentY - lastMotionY);
            lastMotionX = currentX;
            lastMotionY = currentY;
            redraw = 1;
          }
          break;

        case KeyPress:
           {
              int panDelta = 100;
              char buffer[10];
              int r, code;
              code = XLookupKeysym(&event.xkey, 0);
              if (code == XK_Left) {
                m_test->handleSingleTouchPanGesture(-panDelta, 0);
              }
              else if (code == XK_Right) {
                m_test->handleSingleTouchPanGesture(panDelta, 0);
              }
              else if (code == XK_Up) {
                m_test->handleSingleTouchPanGesture(0, -panDelta);
              }
              else if (code == XK_Down) {
                m_test->handleSingleTouchPanGesture(0, panDelta);
              }
              else {
                 r = XLookupString(&event.xkey, buffer, sizeof(buffer),
                                   NULL, NULL);
                 if (buffer[0] == 27) {
                    /* escape */
                    return;
                 }
                 else if (buffer[0] == 'r') {
                   m_test->resetView();
                 }
              }
           }
           redraw = 1;
           break;
        default:
           ; /*no-op*/
        }

        if (redraw) {
           m_test->render();
           eglSwapBuffers(egl_dpy, egl_surf);
        }
     }
  }

public:
  Display *x_dpy;
  Window win;
  EGLSurface egl_surf;
  EGLContext egl_ctx;
  EGLDisplay egl_dpy;
  char *dpyName;
  GLboolean printInfo;
  EGLint egl_major, egl_minor;
  const char *s;
  const int width;
  const int height;

  bool haveLastMotion;
  int lastMotionX;
  int lastMotionY;
  int currentX;
  int currentY;
};