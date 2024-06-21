//cs335 Sample
//This program demonstrates billiard ball collisions
//Originally designed to be used with air hockey collisions
//
//program: bump.cpp
//author:  Cecilio Navarro
//date:    2014, 2017, 2024
//
//-------------------------------------------
//Press arrow keys to move a ball
//Press S to slow the ball movement
//Grab and move a ball with mouse left button
//-------------------------------------------
//
//
//Depending on your Linux distribution,
//may have to install these packages:
// libx11-dev
// libglew1.6
// libglew1.6-dev
//
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <unistd.h>
#include <time.h>
#include <math.h>
#include <X11/Xlib.h>
//#include <X11/Xutil.h>
//#include <GL/gl.h>
//#include <GL/glu.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include "fonts.h"

#include <unistd.h>
#include <X11/Xatom.h>
#include <fstream>
#include <sstream>

using namespace std;

typedef float Flt;
typedef Flt Vec[3];
//macros to manipulate vectors
#define MakeVector(x,y,z,v) (v)[0]=(x),(v)[1]=(y),(v)[2]=(z)
#define VecNegate(a) (a)[0]=(-(a)[0]); (a)[1]=(-(a)[1]); (a)[2]=(-(a)[2]);
#define VecDot(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define VecCopy(a,b) (b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];
#define VecCopy2d(a,b) (b)[0]=(a)[0];(b)[1]=(a)[1];
#define VecNegate2d(a) (a)[0]=(-(a)[0]); (a)[1]=(-(a)[1]);
#define VecDot2d(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1])
static int xres=1280, yres=720;
void setup_screen_res(const int w, const int h);

//image
class Image {
    public:
        int width, height;
        unsigned char *data;
        ~Image() { delete [] data; }
        Image(const char *fname) {
            if (fname[0] == '\0')
                return;
            //printf("fname **%s**\n", fname);
            int ppmFlag = 0;
            char name[40];
            strcpy(name, fname);
            int slen = strlen(name);
            char ppmname[80];
            if (strncmp(name+(slen-4), ".ppm", 4) == 0)
                ppmFlag = 1;
            if (ppmFlag) {
                strcpy(ppmname, name);
            } else {
                name[slen-4] = '\0';
                //printf("name **%s**\n", name);
                sprintf(ppmname,"%s.ppm", name);
                //printf("ppmname **%s**\n", ppmname);
                char ts[100];
                //system("convert img.jpg img.ppm");
                sprintf(ts, "convert %s %s", fname, ppmname);
                system(ts);
            }
            //sprintf(ts, "%s", name);
            FILE *fpi = fopen(ppmname, "r");
            if (fpi) {
                char line[200];
                fgets(line, 200, fpi);
                fgets(line, 200, fpi);
                //skip comments and blank lines
                while (line[0] == '#' || strlen(line) < 2)
                    fgets(line, 200, fpi);
                sscanf(line, "%i %i", &width, &height);
                fgets(line, 200, fpi);
                //get pixel data
                int n = width * height * 3;
                data = new unsigned char[n];
                for (int i=0; i<n; i++)
                    data[i] = fgetc(fpi);
                fclose(fpi);
            } else {
                printf("ERROR opening image: %s\n",ppmname);
                exit(0);
            }
            if (!ppmFlag)
                unlink(ppmname);
        }
};

Image img[2] = {"intro.jpg", "player.jpg"};

class Texture {
    public:
    Image *introImage = NULL;
    Image *playerImage = NULL;
    GLuint introTexture;
    GLuint playerTexture;
    float xc[2];
    float yc[2];
};

class Global {
public:
    int xres, yres;
    Texture tex;
    int fps;
    int vsync;
    float highScore = 0;
    float globalScore = 0;
    Global() {
        xres = 1280, yres=720;
        vsync = 1;
    }
} g;

class X11_wrapper {
    //X Windows variables
    Window win;
    GLXContext glc;
    public:
    Display *dpy;
    X11_wrapper() {
        Window root;
        GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
        //GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, None };
        XVisualInfo *vi;
        Colormap cmap;
        XSetWindowAttributes swa;
        setup_screen_res(1280, 720);
        dpy = XOpenDisplay(NULL);
        if (dpy == NULL) {
            printf("\n\tcannot connect to X server\n\n");
            exit(EXIT_FAILURE);
        }
        root = DefaultRootWindow(dpy);
        vi = glXChooseVisual(dpy, 0, att);
        if (vi == NULL) {
            printf("\n\tno appropriate visual found\n\n");
            exit(EXIT_FAILURE);
        } 
        //else {
        //	// %p creates hexadecimal output like in glxinfo
        //	printf("\n\tvisual %p selected\n", (void *)vi->visualid);
        //}
        cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
        swa.colormap = cmap;
        swa.event_mask =
            ExposureMask |
            KeyPressMask |
            KeyReleaseMask |
            PointerMotionMask |
            ButtonPressMask |
            ButtonReleaseMask |
            StructureNotifyMask |
            SubstructureNotifyMask;
        win = XCreateWindow(dpy, root, 0, 0, xres, yres, 0,
                vi->depth, InputOutput, vi->visual,
                CWColormap | CWEventMask, &swa);
        set_title();
        glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
        glXMakeCurrent(dpy, win, glc);
    }
    ~X11_wrapper() {
        XDestroyWindow(dpy, win);
        XCloseDisplay(dpy);
    }
    void set_title(void) {
        //Set the window title bar.
        XMapWindow(dpy, win);
        XStoreName(dpy, win, "MICROWARS");
    }
    bool getPending() {
        return XPending(dpy);
    }
    void getNextEvent(XEvent *e) {
        XNextEvent(dpy, e);
    }
    void swapBuffers() {
        glXSwapBuffers(dpy, win);
    }
} x11;

//#define USE_SOUND
#ifdef USE_SOUND
#include </usr/include/AL/alut.h>
class Openal {
    ALuint alSource[2];
    ALuint alBuffer[2];
    public:
    Openal() {
        //Get started right here.
        alutInit(0, NULL);
        if (alGetError() != AL_NO_ERROR) {
            printf("ERROR: starting sound.\n");
        }
        //Clear error state
        alGetError();
        //Setup the listener.
        //Forward and up vectors are used.
        float vec[6] = {0.0f,0.0f,1.0f, 0.0f,1.0f,0.0f};
        alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
        alListenerfv(AL_ORIENTATION, vec);
        alListenerf(AL_GAIN, 1.0f);
        //
        //Buffer holds the sound information.
        alBuffer[0] = alutCreateBufferFromFile("./billiard.wav");
        alBuffer[1] = alutCreateBufferFromFile("./wall.wav");
        //Source refers to the sound stored in buffer.
        for (int i=0; i<2; i++) {
            alGenSources(1, &alSource[i]);
            alSourcei(alSource[i], AL_BUFFER, alBuffer[i]);
            alSourcef(alSource[i], AL_GAIN, 1.0f);
            if (i==0) alSourcef(alSource[i], AL_GAIN, 0.4f);
            alSourcef(alSource[i], AL_PITCH,1.0f);
            alSourcei(alSource[i], AL_LOOPING,  AL_FALSE);
            if (alGetError() != AL_NO_ERROR) {
                printf("ERROR\n");
            }
        }
    }
    void playSound(int i)
    {
        alSourcePlay(alSource[i]);
    }
    ~Openal() {
        //First delete the source.
        alDeleteSources(1, &alSource[0]);
        alDeleteSources(1, &alSource[1]);
        //Delete the buffer
        alDeleteBuffers(1, &alBuffer[0]);
        alDeleteBuffers(1, &alBuffer[1]);
        //Close out OpenAL itself.
        //unsigned int alSampleSet;
        ALCcontext *Context;
        ALCdevice *Device;
        //Get active context
        Context=alcGetCurrentContext();
        //Get device for active context
        Device=alcGetContextsDevice(Context);
        //Disable context
        alcMakeContextCurrent(NULL);
        //Release context(s)
        alcDestroyContext(Context);
        //Close device
        alcCloseDevice(Device);
    }
} oal;
#endif

void playSound(int s) {
#ifdef USE_SOUND
    oal.playSound(s);
#else
    if (s) {}
#endif
}

int lbump=0;
int lbumphigh=0;

void init_opengl(void);
void init_balls(void);
void init_food(void);
void scenario1(void);
void check_resize(XEvent *e);
void check_mouse(XEvent *e);
void check_keys(XEvent *e);
void physics(void);
void absorb(int absorber, int absorbed);
void checkFoodCollision();
void showIntro(int xres, int yres, GLuint capitalIntro);
void showScore(int xres, int yres, GLuint capitalIntro);
void showInfo(int xres, int yres, GLuint capitalIntro);
void showSettings(int xres, int yres, GLuint capitalIntro);
void render(void);
std::string getCurrResolution();
void change_resolution(const char* resolution);
std::string originalRes;
void change_fullscreen();
bool intro = true;
bool info = false;
bool score = false;
bool settings = false;
bool fullscreen  = false;

int done=0;
int leftButtonDown=0;
Vec leftButtonPos;
class Ball {
    public:
        Vec pos;
        Vec vel;
        Vec force;
        float radius;
        float mass;
        double lastChangeTime;
        double changeInterval;
} ball[16];
const int n=16;

// Ball food;
const int nfood = 20;
Ball foods[nfood]; // Number of food

//-----------------------------------------------------------------------------
//Setup timers
const double physicsRate = 1.0 / 60.0;
const double oobillion = 1.0 / 1e9;
struct timespec timeStart, timeCurrent;
struct timespec timePause;
double physicsCountdown=0.0;
double timeSpan=0.0;
double timeDiff(struct timespec *start, struct timespec *end) {
    return (double)(end->tv_sec - start->tv_sec ) +
        (double)(end->tv_nsec - start->tv_nsec) * oobillion;
}
void timeCopy(struct timespec *dest, struct timespec *source) {
    memcpy(dest, source, sizeof(struct timespec));
}
//-----------------------------------------------------------------------------


int main(void)
{
    init_opengl();
    srand(time(NULL));
    scenario1();
    init_food();
    clock_gettime(CLOCK_REALTIME, &timePause);
    clock_gettime(CLOCK_REALTIME, &timeStart);
    struct timespec fpsStart;
    struct timespec fpsCurr;
    clock_gettime(CLOCK_REALTIME, &fpsStart);
    int fps = 0;
    originalRes = getCurrResolution();
    while (!done) {
        while (x11.getPending()) {
            XEvent e;
            x11.getNextEvent(&e);
            check_resize(&e);
            check_mouse(&e);
            check_keys(&e);
        }
        clock_gettime(CLOCK_REALTIME, &timeCurrent);
        timeSpan = timeDiff(&timeStart, &timeCurrent);
        timeCopy(&timeStart, &timeCurrent);
        physicsCountdown += timeSpan;
        while (physicsCountdown >= physicsRate) {
            physics();
            physicsCountdown -= physicsRate;
        }
        ++fps;
        timeCopy(&fpsCurr, &timeCurrent);
        double diff = timeDiff(&fpsStart, &fpsCurr);
        if (diff >= 1.0) {
             g.fps = fps;
             fps = 0;
             timeCopy(&fpsStart, &fpsCurr);
         }
        render();
        x11.swapBuffers();
    }
    cleanup_fonts();
    return 0;
}

void setup_screen_res(const int w, const int h)
{
    xres = w;
    yres = h;
}

void reshape_window(int width, int height)
{
    //window has been resized.
    setup_screen_res(width, height);
    glViewport(0, 0, (GLint)width, (GLint)height);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    glOrtho(0, xres, 0, yres, -1, 1);
    x11.set_title();
}
unsigned char *buildAlphaData(Image *img)
{
    int i;
    int a,b,c;
    unsigned char *newdata, *ptr;
    unsigned char *data = (unsigned char *)img->data;
    newdata = (unsigned char *)malloc(img->width * img->height * 4);
    ptr = newdata;
    for (i=0; i<img->width * img->height * 3; i+=3) {
        a = *(data+0);
        b = *(data+1);
        c = *(data+2);
        *(ptr+0) = a;
        *(ptr+1) = b;
        *(ptr+2) = c;
        *(ptr+3) = (a|b|c);
        ptr += 4;
        data += 3;
    }
    return newdata;
}

void init_opengl(void)
{
   //OpenGL initialization
    glViewport(0, 0, xres, yres);
    //Initialize matrices
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
    //This sets 2D mode (no perspective)
    glOrtho(0, xres, 0, yres, -1, 1);
    //Clear the screen
    glClearColor(0.0, 0.0, 0.0, 1.0);
    ///Do this to allow fonts
    glEnable(GL_TEXTURE_2D);

    g.tex.introImage = &img[0];
    glGenTextures(1, &g.tex.introTexture);
    int w = g.tex.introImage->width;
    int h = g.tex.introImage->height;

 
    glBindTexture(GL_TEXTURE_2D, g.tex.introTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, w, h, 0,
            GL_RGB, GL_UNSIGNED_BYTE, g.tex.introImage->data);
    g.tex.xc[0] = 0.0;
    g.tex.xc[1] = 0.25;
    g.tex.xc[0] = 0.0;
    g.tex.yc[1] = 1.0;

    //player image
    g.tex.playerImage = &img[1];
    glGenTextures(1, &g.tex.playerTexture);
 
    glBindTexture(GL_TEXTURE_2D, g.tex.playerTexture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, 10, 10, 0,
            GL_RGB, GL_UNSIGNED_BYTE, g.tex.introImage->data);
    g.tex.xc[0] = 0.0;
    g.tex.xc[1] = 0.25;
    g.tex.xc[0] = 0.0;
    g.tex.yc[1] = 1.0;

    initialize_fonts();
}

#define sphereVolume(r) (r)*(r)*(r)*3.14159265358979*4.0/3.0;

void init_balls(void)
{
    ball[0].pos[0] = 100;
    ball[0].pos[1] = yres-150;
    ball[0].vel[0] = 2.6;
    ball[0].vel[1] = 0.0;
    ball[0].force[0] =
        ball[0].force[1] = 0.0;
    ball[0].radius = 90.0;
    ball[0].mass = sphereVolume(ball[0].radius);
    //
    ball[1].pos[0] = 400;
    ball[1].pos[1] = yres-150;
    ball[1].vel[0] = -1.6;
    ball[1].vel[1] = 0.0;
    ball[1].force[0] =
        ball[1].force[1] = 0.0;
    ball[1].radius = 90.0;
    ball[1].mass = sphereVolume(ball[1].radius);
}

void init_food() {
    srand(time(NULL));
    for (int i = 0; i < nfood; i++) {
        foods[i].pos[0] = rand() % xres;
        foods[i].pos[1] = rand() % yres;
        foods[i].vel[0] = 0; // Static food does not move
        foods[i].vel[1] = 0;
        foods[i].radius = 10; // Set the radius size
        foods[i].mass = sphereVolume(foods[i].radius);
    }
}

#define CONSTANT_SPEED 1.0
void scenario1(void)
{
    //get current time for balls
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    double currentTime = now.tv_sec + now.tv_nsec * oobillion;

    ball[0].pos[0] = 100;
    ball[0].pos[1] = yres-150;
    ball[0].vel[0] = 0.0;
    ball[0].vel[1] = 0.0;
    ball[0].force[0] =
        ball[0].force[1] = 0.0;
    ball[0].radius = 20.0;
    ball[0].mass = sphereVolume(ball[0].radius);

    //random directions
    srand(time(NULL));
    float angle;
    for (int i = 1; i < n; i++) {
        ball[i].lastChangeTime = currentTime;
        ball[i].changeInterval = (rand() % 6 + 5);
        angle = (float)(rand() % 360) * (M_PI / 180.0);
        ball[i].pos[0] = rand() % (xres - 40) + 20;
        ball[i].pos[1] = rand() % (yres - 40) + 20; 
        ball[i].vel[0] = cos(angle) * CONSTANT_SPEED;
        ball[i].vel[1] = sin(angle) * CONSTANT_SPEED;
        ball[i].radius = (rand() % (40 - 10 + 1)) + 10;
        ball[i].mass = sphereVolume(ball[i].radius);
    }

}

void check_resize(XEvent *e)
{
    //The ConfigureNotify is sent by the
    //server if the window is resized.
    if (e->type != ConfigureNotify)
        return;
    XConfigureEvent xce = e->xconfigure;
    if (xce.width != xres || xce.height != yres) {
        //Window size did change.
        reshape_window(xce.width, xce.height);
    }
}

void check_mouse(XEvent *e)
{
    //Did the mouse move?
    //Was a mouse button clicked?
    static int savex = 0;
    static int savey = 0;
    //
    if (e->type == ButtonRelease) {
        leftButtonDown=0;
        return;
    }
    if (e->type == ButtonPress) {
        if (e->xbutton.button==1) {
            //Left button is down
            leftButtonDown = 1;
            leftButtonPos[0] = (Flt)e->xbutton.x;
            leftButtonPos[1] = (Flt)(yres - e->xbutton.y);
        }
        if (e->xbutton.button==3) {
            //Right button is down
        }
    }
    //  static int lastx = 0, lasty = 0;

    if (savex != e->xbutton.x || savey != e->xbutton.y) {
        //Mouse moved
        int savex = e->xmotion.x;
        int savey = yres - e->xmotion.y; // update
        float desired_vel_x = (float)(savex - ball[0].pos[0]); //diff between mouse
        float desired_vel_y = (float)(savey - ball[0].pos[1]); // and ball pos

        float speed = sqrt(desired_vel_x * desired_vel_x + desired_vel_y * desired_vel_y);
        const float MAX_SPEED = 1.0; // same max speed as other balls

        if (speed > MAX_SPEED) {
            desired_vel_x = (desired_vel_x / speed) * MAX_SPEED;
            desired_vel_y = (desired_vel_y / speed) * MAX_SPEED;
        }
        ball[0].vel[0] = desired_vel_x;
        ball[0].vel[1] = desired_vel_y;
        ball[0].pos[0] += ball[0].vel[0];
        ball[0].pos[1] += ball[0].vel[1];
    }
}

void check_keys(XEvent *e)
{
    if (e->type == KeyPress) {
        int key = XLookupKeysym(&e->xkey, 0);
        switch(key) {
            case XK_Escape:
                change_resolution(originalRes.c_str());
                done=1;
                break;
            case XK_space:
                intro = 0;
                break;
            case XK_i:
                info ^= 1;
                settings = 0;
                score = 0;
                break;
            case XK_h:
                score ^= 1;
                info = 0;
                settings = 0;
                break;
            case XK_r:
                //scenario
                scenario1();
                break;
            case XK_s:
                settings = !settings;
                info = false;
                score = false;
                break;
            case XK_F1:
                if (settings == true) {
                    change_resolution("640x480");
                }
                break;
            case XK_F2:
                if (settings == true) {
                    change_resolution("800x600");
                }
                break;
            case XK_F3:
                if (settings == true) {
                    change_resolution("1024x768");
                }
                break;
            case XK_F4:
                if (settings == true) {
                    change_resolution("1280x720");
                }
                break;
            case XK_F5:
                if (settings == true) {
                    change_resolution("1440x900");
                }
            case XK_F6:
                if (settings == true) {
                    change_resolution("1280x1024");
                }
                break;
            case XK_F7:
                if (settings == true) {
                    change_resolution("1680x1050");
                }
                break;
            case XK_F8:
                if (settings == true) {
                    change_resolution("1920x1080");
                }
                break;
            case XK_F9:
                // Toggle fullscreen mode
                change_fullscreen();
                break;
            case XK_v: {
                g.vsync ^= 1;
                //vertical synchronization
                //https://github.com/godotengine/godot/blob/master/platform/
                //x11/context_gl_x11.cpp
                static PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT = NULL;
                glXSwapIntervalEXT =
                    (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
                    (const GLubyte *)"glXSwapIntervalEXT");
                GLXDrawable drawable = glXGetCurrentDrawable();
                if (g.vsync) {
                    glXSwapIntervalEXT(x11.dpy, drawable, 1);
                } else {
                    glXSwapIntervalEXT(x11.dpy, drawable, 0);
                }
                break;
            }
            case XK_1:
                ball[0].radius -= 1.0;
                ball[0].mass = sphereVolume(ball[0].radius);
                break;
            case XK_2:
                ball[0].radius += 1.0;
                ball[0].mass = sphereVolume(ball[0].radius);
                break;
            case XK_3:
                ball[1].radius -= 1.0;
                ball[1].mass = sphereVolume(ball[1].radius);
                break;
            case XK_4:
                ball[1].radius += 1.0;
                ball[1].mass = sphereVolume(ball[1].radius);
                break;
        }
    }
}

std::string getCurrResolution() {
    //system("xrandr -q | grep ' connected ' > out.txt");
    system("xrandr -q | grep '*' | awk '{print $1}' > out.txt");
    std::ifstream infile("out.txt");
    std::stringstream buffer;
    buffer << infile.rdbuf();
    return buffer.str();
}

void change_resolution(const char* resolution) {
    //std::string cmd = "xrandr --mode ";
    std::string cmd = "xrandr --output $(xrandr | grep ' connected' | cut -d ' ' -f1) --mode ";
    cmd += resolution;
    system(cmd.c_str());
}

void change_fullscreen() {
    if (fullscreen) {
        // Switch back to the original resolution
        change_resolution(originalRes.c_str());
        fullscreen = false;
    } else {
        // Change to full screen mode (find the best matching resolution)
        std::string cmd = "xrandr --output $(xrandr | grep ' connected' | cut -d ' ' -f1) --mode 1920x1080 --rate 60";  // Adjust according to your highest available resolution
        system(cmd.c_str());
        fullscreen = true;
    }
}

void VecNormalize(Vec v)
{
    Flt dist = v[0]*v[0]+v[1]*v[1]+v[2]*v[2];
    if (dist==0.0)
        return;
    Flt len = 1.0 / sqrt(dist);
    v[0] *= len;
    v[1] *= len;
    v[2] *= len;
}

void VecNormalize2d(Vec v)
{
    Flt dist = v[0]*v[0]+v[1]*v[1];
    if (dist==0.0)
        return;
    Flt len = 1.0 / sqrt(dist);
    v[0] *= len;
    v[1] *= len;
    //v[2] *= len;
}


void physics(void)
{
    //change direction randomly
    //get time
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    double currentTime = now.tv_sec + now.tv_nsec * oobillion;

    for (int i = 1; i < n; i++) {
        if (currentTime > ball[i].lastChangeTime + ball[i].changeInterval) {
            // Time to change direction
            float angle = (float)(rand() % 360) * (M_PI / 180.0);
            ball[i].vel[0] = cos(angle) * CONSTANT_SPEED;
            ball[i].vel[1] = sin(angle) * CONSTANT_SPEED;

            // Reset timer to go again
            ball[i].lastChangeTime = currentTime;
            ball[i].changeInterval = (rand() % 6 + 5); // every 5 to 10 sec
        }
        // update movement
        ball[i].pos[0] += ball[i].vel[0];
        ball[i].pos[1] += ball[i].vel[1];
    }

    const float MAX_SPEED = 1.0;
    Vec dir;
    float repelStrength = 0.00001;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            if (i != j) {
                dir[0] = ball[i].pos[0] - ball[j].pos[0];
                dir[1] = ball[i].pos[1] - ball[j].pos[1];
                //find distance
                float distance = sqrt(dir[0]*dir[0] + dir[1]*dir[1]);

                // if distance is less than or equal to 90
                if (distance > 0 && distance <= 125) {
                    //if mass difference is great more repel
                    //if similar less repel
                    float force = repelStrength * (ball[j].mass - ball[i].mass) / distance;
                    VecNormalize2d(dir);
                    //update vel
                    ball[i].vel[0] += dir[0] * force;
                    ball[i].vel[1] += dir[1] * force;
                }
            }
        }
    }
    //move background in intro
    g.tex.xc[0] += 0.001;
    g.tex.xc[1] += 0.001;

    //new physics for absorbtion
    for (int i = 0; i < n - 1; i++) {
        for (int j = i + 1; j < n; j++) {
            Vec vcontact;
            vcontact[0] = ball[i].pos[0] - ball[j].pos[0];
            vcontact[1] = ball[i].pos[1] - ball[j].pos[1];
            double distance = sqrt(vcontact[0]*vcontact[0] + vcontact[1]*vcontact[1]);

            // Absorption condition: mass at least 25% greater and overlap center
            if ((ball[i].mass >= 1.25 * ball[j].mass && distance < ball[i].radius) ||
                (ball[j].mass >= 1.25 * ball[i].mass && distance < ball[j].radius)) {
                if (ball[i].mass > ball[j].mass) {
                    absorb(i, j);
                } else {
                    absorb(j, i);
                }
            }
        }
    }

    //Different physics applied here...
    //100% elastic collisions.
    for (int i=0; i<n; i++) {
        ball[i].pos[0] += ball[i].vel[0];
        ball[i].pos[1] += ball[i].vel[1];
        // ball[i].vel[0] *= 0.995;
        // ball[i].vel[1] *= 0.995; 

    }
    //check for collision here
    Flt distance;
    Flt movi[2], movj[2];
    Vec vcontact[2];
    Vec vmove[2];
    // Flt dot0, dot1;
    for (int i=0; i<n-1; i++) {
        for (int j=i+1; j<n; j++) {
            //vx = ball[i].pos[0] - ball[j].pos[0];
            //vy = ball[i].pos[1] - ball[j].pos[1];
            vcontact[0][0] = ball[i].pos[0] - ball[j].pos[0];
            vcontact[0][1] = ball[i].pos[1] - ball[j].pos[1];
            //vcontact[0][2] = 0.0;
            distance = sqrt(vcontact[0][0]*vcontact[0][0] +
                    vcontact[0][1]*vcontact[0][1]);
            if (distance < (ball[i].radius + ball[j].radius)) {
                //We have a collision!
                playSound(0);
                //vector from center to center.
                VecNormalize2d(vcontact[0]);
                VecCopy2d(vcontact[0], vcontact[1]);
                VecNegate2d(vcontact[1]);
                movi[0] = ball[i].vel[0];
                movi[1] = ball[i].vel[1];
                movj[0] = ball[j].vel[0];
                movj[1] = ball[j].vel[1];
                vmove[0][0] = movi[0];
                vmove[0][1] = movi[1];
                VecNormalize2d(vmove[0]);
                vmove[1][0] = movj[0];
                vmove[1][1] = movj[1];
                VecNormalize2d(vmove[1]);
                //Determine how direct the hit was...
                // dot0 = VecDot2d(vcontact[0], vmove[0]);
                // dot1 = VecDot2d(vcontact[1], vmove[1]);
                //Find the closing (relative) speed of the objects...
                // speed =
                //     sqrtf(movi[0]*movi[0] + movi[1]*movi[1]) * dot0 +
                //     sqrtf(movj[0]*movj[0] + movj[1]*movj[1]) * dot1;
            }
        }
    }
    //Check for collision with window edges
    for (int i=0; i<n; i++) {
        if (ball[i].pos[0] < ball[i].radius && ball[i].vel[0] <= 0.0) {
            ball[i].vel[0] = -ball[i].vel[0];
            playSound(1);
        }
        if (ball[i].pos[0] >= (Flt)xres-ball[i].radius &&
                ball[i].vel[0] >= 0.0) {
            ball[i].vel[0] = -ball[i].vel[0];
            playSound(1);
        }
        if (ball[i].pos[1] < ball[i].radius && ball[i].vel[1] <= 0.0) {
            ball[i].vel[1] = -ball[i].vel[1];
            playSound(1);
        }
        if (ball[i].pos[1] >= (Flt)yres-ball[i].radius &&
                ball[i].vel[1] >= 0.0) {
            ball[i].vel[1] = -ball[i].vel[1];
            playSound(1);
        }
    }
    for (int i = 0; i < n; i++) {
        float speed = sqrt(ball[i].vel[0] * ball[i].vel[0] + ball[i].vel[1] * ball[i].vel[1]);
        if (speed > MAX_SPEED) {
            ball[i].vel[0] = (ball[i].vel[0] / speed) * MAX_SPEED;
            ball[i].vel[1] = (ball[i].vel[1] / speed) * MAX_SPEED;
        }
    }
    checkFoodCollision();
}

void absorb(int absorber, int absorbed) {
    // Increase mass and new radius
    ball[absorber].mass += ball[absorbed].mass;
    ball[absorber].radius = cbrt((3.0 / (4.0 * M_PI)) * ball[absorber].mass);  // Assuming volume directly related to mass

    // init the ball again in a random spot and mass
    ball[absorbed].pos[0] = rand() % (xres - 40) + 20;
    ball[absorbed].pos[1] = rand() % (yres - 40) + 20;
    ball[absorbed].vel[0] = 0;
    ball[absorbed].vel[1] = 0;
    // ball[absorbed].mass = 0;
    float radius = ball[absorbed].radius = (rand() % (30 - 10 + 1)) + 10;
    ball[absorbed].mass = sphereVolume(radius);
}

void checkFoodCollision() {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < nfood; j++) {
            float dx = ball[i].pos[0] - foods[j].pos[0];
            float dy = ball[i].pos[1] - foods[j].pos[1];
            float distance = sqrt(dx * dx + dy * dy);
            if (distance < (ball[i].radius + foods[j].radius)) {
                ball[i].mass += foods[j].mass;
                ball[i].radius = cbrt((3.0 * ball[i].mass) / (4.0 * M_PI));
                
                // Respawn food at a new position
                foods[j].pos[0] = rand() % xres;
                foods[j].pos[1] = rand() % yres;
            }
        }
    }
}

void drawBall(Flt rad)
{
    int i;
    static int firsttime=1;
    static Flt verts[32][2];
    static int n=32;
    if (firsttime) {
        Flt ang=0.0;
        Flt inc = 3.14159 * 2.0 / (Flt)n;
        for (i=0; i<n; i++) {
            verts[i][0] = sin(ang);
            verts[i][1] = cos(ang);
            ang += inc;
        }
        firsttime=0;
    }
    glBegin(GL_TRIANGLE_FAN);
    for (i=0; i<n; i++) {
        glVertex2f(verts[i][0]*rad, verts[i][1]*rad);
    }
    glEnd();
}

void showIntro(int xres, int yres, GLuint introTexture)
{

    Rect r;
    int xcent = xres / 2;
    int ycent = yres / 2;
    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
    glVertex2f(0, 0);
    glVertex2f(0, yres);
    glVertex2f(xres, yres);
    glVertex2f(xres, 0);
    glEnd();

    r.left = xcent;
    r.bot  = ycent + 80;
    r.center = 50;
    ggprint16(&r, 50, 0xffffffff, "Hello");
    ggprint16(&r, 50, 0xffffffff, "Press: 'I' to continue");

    //Display img
    //    int imgx = xcent;
    //    int imgy = ycent + 150 + 16;

    glBindTexture(GL_TEXTURE_2D, introTexture);
    glBegin(GL_QUADS);      
    glTexCoord2f(g.tex.xc[0], g.tex.yc[1]); glVertex2i(0, 0);
    glTexCoord2f(g.tex.xc[0], g.tex.yc[0]); glVertex2i(0, g.yres);
    glTexCoord2f(g.tex.xc[1], g.tex.yc[0]); glVertex2i(g.xres, g.yres);
    glTexCoord2f(g.tex.xc[1], g.tex.yc[1]); glVertex2i(g.xres, 0);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    r.left = xcent;
    //    r.bot  = 70;
    r.center = 50;
    ggprint17(&r, 50, 0xffffffff, "MICROWARS");
    ggprint16(&r, 50, 0xffffffff, "Press 'SPACE' to begin");
}

void showScore(int xres, int yres, GLuint capitalIntro)
{
    //from battleship game project Software Engineering
    Rect r;
    int xcent = xres / 2;
    int ycent = yres / 2;
    int w = 380;
    int h = 300;
    int space = 30;

    //dim background
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glColor4f(0, 0, 0, 0.6f);
    glBegin(GL_QUADS);
        glVertex2f(0, yres);
        glVertex2f(xres, yres);
        glVertex2f(xres, 0);
        glVertex2f(0, 0);
    glEnd();
    glDisable(GL_BLEND);

    //credits rectangle
    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
        glVertex2f(xcent - w/2, ycent - h/2);
        glVertex2f(xcent - w/2, ycent + h/2);
        glVertex2f(xcent + w/2, ycent + h/2);
        glVertex2f(xcent + w/2, ycent - h/2);
    glEnd();

    // Text centering calculations
    r.left = xcent; // Horizontal center
    r.center = 1;  // center on 'left'.

    r.bot = ycent + 100;
    ggprint16(&r, 0, 0xffffffff, "CREDITS AND SCORE");

    r.bot -= space * 2;  // Move next line
    ggprint16(&r, 0, 0xffffffff, "A game by:");

    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "Cecilio Navarro");

    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "Thanks for playing!");

    r.bot -= space * 2;
    ggprint16(&r, 0, 0xffffffff, "YOUR HIGH SCORE: %.0f", g.highScore / 4188.79);

    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "GLOBAL HIGH SCORE: %.0f", g.globalScore / 4188.79);
}

void showInfo(int xres, int yres, GLuint playerTexture)
{
    //from battleship game project Software Engineering
    //from showCredits in thooser.cpp
    Rect r;
    int xcent = xres / 2;
    int ycent = yres / 2;
    int w = 380;
    int h = 300;
    int space = 30;
    int imgdim = 1;
    int imgx;
    int imgy;

    //dim background
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glColor4f(0, 0, 0, 0.6f);
    glBegin(GL_QUADS);
        glVertex2f(0, yres);
        glVertex2f(xres, yres);
        glVertex2f(xres, 0);
        glVertex2f(0, 0);
    glEnd();
    glDisable(GL_BLEND);

    //credits rectangle
    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
        glVertex2f(xcent - w/2, ycent - h/2);
        glVertex2f(xcent - w/2, ycent + h/2);
        glVertex2f(xcent + w/2, ycent + h/2);
        glVertex2f(xcent + w/2, ycent - h/2);
    glEnd();

    // Text centering calculations
    r.left = xcent; // Horizontal center
    r.center = 1;  // center on 'left'.

    r.bot = ycent + 100;
    ggprint16(&r, 0, 0xffffffff, "INFORMATION");

    r.bot -= space * 2;  // Move next line
    ggprint16(&r, 0, 0xffffffff, "You are a little germ in a big world.");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "Try and eat other germs to survive.");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "You can eat others if you are");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "25 percent bigger");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "Purple dots are food for germs");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "Don't get eaten!");

    imgx = xcent - 100;
	imgy = ycent + 130 + 16 - 4*80;
	
	glBindTexture(GL_TEXTURE_2D, playerTexture);
	glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f);
		glVertex2f(imgx-imgdim, imgy-imgdim);
		glTexCoord2f(0.0f, 0.0f);
		glVertex2f(imgx-imgdim, imgy+imgdim);
		glTexCoord2f(1.0f, 0.0f);
		glVertex2f(imgx+imgdim, imgy+imgdim);
		glTexCoord2f(1.0f, 1.0f);
		glVertex2f(imgx+imgdim, imgy-imgdim);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
    
}

void showSettings(int xres, int yres, GLuint capitalIntro)
{
    //from battleship game project Software Engineering
    Rect r;
    int xcent = xres / 2;
    int ycent = yres / 2;
    int w = 380;
    int h = 500;
    int space = 30;

    //dim background
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
     glEnable(GL_BLEND);
     glColor4f(0, 0, 0, 0.6f);
     glBegin(GL_QUADS);
         glVertex2f(0, yres);
         glVertex2f(xres, yres);
         glVertex2f(xres, 0);
         glVertex2f(0, 0);
         glEnd();
     glDisable(GL_BLEND);

    //credits rectangle
    glColor3f(0, 0, 0);
    glBegin(GL_QUADS);
        glVertex2f(xcent - w/2, ycent - h/2);
        glVertex2f(xcent - w/2, ycent + h/2);
        glVertex2f(xcent + w/2, ycent + h/2);
        glVertex2f(xcent + w/2, ycent - h/2);
    glEnd();

    // Text centering calculations
    r.left = xcent;
    r.center = 1;

    r.bot = ycent + 100;
    ggprint16(&r, 0, 0xffffffff, "SETTINGS");

    r.bot -= space;  // Move next line
    ggprint16(&r, 0, 0xffffffff, "F1 - 640x480");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "F2 - 800x600");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "F3 - 1024x768");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "F4 - 1280x720");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "F5 - 1440x900");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "F6 - 1280x1024");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "F7 - 1680x1050");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "F8 - 1920x1080");
    r.bot -= space;
    ggprint16(&r, 0, 0xffffffff, "F9 - Original Resolution");
}

void render(void)
{
    glClear(GL_COLOR_BUFFER_BIT);

    if (settings) {
        showSettings(xres, yres, 0);
    }
    
    Rect r;

    glClear(GL_COLOR_BUFFER_BIT);

    // Draw all food items
    glColor3ub(150, 0, 255); // Food color
    for (int i = 0; i < nfood; i++) {
        glPushMatrix();
        glTranslatef(foods[i].pos[0], foods[i].pos[1], 0);
        drawBall(foods[i].radius);
        glPopMatrix();
    }

    //draw balls
    glColor3ub(245,238,226);
    glPushMatrix();
    glTranslatef(ball[0].pos[0], ball[0].pos[1], ball[0].pos[2]);
    drawBall(ball[0].radius);
    glPopMatrix();

    for (int i=1; i<n; i++) {
        if (i==1 || i==9 ) {
            glColor3ub(225,153,1);
        } else if (i==2 || i==10 ) {
            glColor3ub(50,98,215);
        } else if (i==3 || i==11 ) {
            glColor3ub(222,40,22);
        } else if (i==4 || i==12 ) {
            glColor3ub(86,69,112);
        } else if (i==5 || i==13 ) {
            glColor3ub(225,104,44);
        } else if (i==6 || i==14 ) {
            glColor3ub(1,105,29);
        } else if (i==7 || i==15 ) {
            glColor3ub(161,53,55);
        } else if (i==8) {
            glColor3ub(27,27,27);
        }
        glPushMatrix();
        glTranslatef(ball[i].pos[0], ball[i].pos[1], ball[i].pos[2]);
        drawBall(ball[i].radius);
        glPopMatrix();
    }
    r.bot = yres - 20;
    r.left = 10;
    r.center = 0;
    ggprint8b(&r, 16, 0xFFFFFF, "Use Mouse to Move");
    ggprint8b(&r, 16, 0xFFFFFF, "ESC - Quit");
    ggprint8b(&r, 16, 0xFFFFFF, "I - Information");
    ggprint8b(&r, 16, 0xFFFFFF, "H - High Score");
    ggprint8b(&r, 16, 0xFFFFFF, "R - Reset Game");
    ggprint8b(&r, 16, 0xFFFFFF, "S - Settings");
    ggprint8b(&r, 16, 0xFFFFFF, "vsync: %s", ((g.vsync)?"ON":"OFF"));
    ggprint8b(&r, 16, 0xFFFFFF, "fps: %i", g.fps);
    char ts[16];
    sprintf(ts, "%i", lbumphigh);
    ggprint8b(&r, 16, 0x00ff000, ts);
    //
    r.center = 1;
    for(int i=0; i<n; i++) {
        r.left = ball[i].pos[0];
        r.bot  = ball[i].pos[1]-4;

        if (ball[0].mass > g.highScore) {
            g.highScore = ball[0].mass;
        }

        if (ball[i].mass > g.globalScore) {
            g.globalScore = ball[i].mass;
        }

        char massText[32];
        sprintf(massText, "%.0f", ball[i].mass/4188.79);
        if (i == 0) {
            ggprint8b(&r, 16, 0x00000000, massText);
        } else {
            ggprint8b(&r, 16, 0xffffff, massText);
        }
    }

    if (intro) {
        showIntro(xres, yres, g.tex.introTexture);
    }
    if (score) {
        showScore(xres, yres, g.tex.introTexture);
    }
    if (info) {
        showInfo(xres, yres, g.tex.playerTexture);
    }
    if (settings) {
        showSettings(xres, yres, g.tex.introTexture);
    }

}
