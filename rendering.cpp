////////////////////////////////////////////////////////////////////////
//
// rendering.cpp: Put the calculated polys on the screen
//
// Copyright (c) Simon Frankau 2018
//

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <vector>
#include <png.h>
#include "defines.h"
#include "geom.h"
#include <chrono>
#include "camera.h"
#include "rendering.h"

static const Vertex EYE_POS = Vertex(0.0, 0.0, -3.0);

static std::vector<Quad> flatFaces;
static std::vector<GouraudQuad> gouraudFaces;
static std::vector<Vertex> vertices;

png_structp png_ptr;
png_infop info_ptr;

SceneCamera sceneCamera(glm::vec3(EYE_POS.x(), EYE_POS.y(), EYE_POS.z()));

static bool firstRender = true;

static void screenshotPNG(const char *filename)
{
    const size_t format_nchannels = 4;
    size_t nvals = format_nchannels * WIDTH * HEIGHT;

    // Assuming a GLubyte is a png_byte...
    std::vector<png_byte> pixels(nvals);
    glReadPixels(0, 0, WIDTH, HEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);

    std::vector<png_byte *> png_rows(HEIGHT);
    for (size_t i = 0; i < HEIGHT; i++) {
        png_rows[HEIGHT - i - 1] = &pixels[i * WIDTH * format_nchannels];
    }

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                              NULL, NULL, NULL);
    
    if (png_ptr == NULL) {
        throw std::runtime_error("png_create_write_struct failed");
    }

    info_ptr = png_create_info_struct(png_ptr);
    if ((info_ptr == NULL) || (!info_ptr)) {
        throw std::runtime_error("png_create_info_struct failed");
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        throw std::runtime_error("Something failed in libpng");
    }
    
    FILE *f = fopen(filename, "wb");
    if(f==NULL){
        printf("%s\n", "File couldnt be created");
    }
    png_init_io(png_ptr, f);
    
    png_set_IHDR(png_ptr, info_ptr, WIDTH, HEIGHT, 8, PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    
    png_write_info(png_ptr, info_ptr);
    
    png_write_image(png_ptr, &png_rows[0]);
    
    png_write_end(png_ptr, NULL);
    
    png_destroy_write_struct(&png_ptr, &info_ptr);
    
    fclose(f);
}

static void drawScene(void)
{
    for (std::vector<Quad>::const_iterator iter = flatFaces.begin(),
             end = flatFaces.end(); iter != end; ++iter) {
        iter->render(vertices);
    }
    for (std::vector<GouraudQuad>::const_iterator iter = gouraudFaces.begin(),
             end = gouraudFaces.end(); iter != end; ++iter) {
        iter->render(vertices);
    }
}

static void display(void)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    drawScene();
    if (firstRender) {
        screenshotPNG("../png/scene.png");
        firstRender = false;
    }
    glutSwapBuffers();
}

static void initGL(void)
{
    // Flat shading.
    glEnable(GL_COLOR_MATERIAL);
    // Use depth buffering for hidden surface elimination.
    glEnable(GL_DEPTH_TEST);
    // Back-face culling.
    glEnable(GL_CULL_FACE);

    // Setup the view of the cube. Will become a view from inside the
    // cube.
    glMatrixMode(GL_PROJECTION);
    gluPerspective(45.0,  // Field of view in degrees
                   1.0,   // Aspect ratio
                   1.0,   // Z near
                   10.0); // Z far
    glMatrixMode(GL_MODELVIEW);

    gluLookAt(sceneCamera.Position.x, sceneCamera.Position.y, sceneCamera.Position.z,
              sceneCamera.Position.x + sceneCamera.Front.x, sceneCamera.Position.y + sceneCamera.Front.y, sceneCamera.Position.z + sceneCamera.Front.z,  // Looking at origin
              sceneCamera.Up.x, sceneCamera.Up.y, sceneCamera.Up.z); // Up is in positive Y direction
}

static bool facesUs(Quad const &q, std::vector<Vertex> const &vs)
{
    return dot(paraCentre(q, vs) - Vertex(sceneCamera.Position.x, sceneCamera.Position.y, sceneCamera.Position.z),
               paraCross(q, vs)) > 0;
}

// Normalise the brightness of non-emitting components
void normaliseBrightness(std::vector<Quad> &qs, std::vector<Vertex> const &vs)
{
    const double TARGET = 1.0;

    double max = 0.0;
    for (std::vector<Quad>::iterator iter = qs.begin(), end = qs.end();
         iter != end; ++iter) {
        // Only include non-emitters, facing us.
        if (!iter->isEmitter && facesUs(*iter, vs)) {
            max = std::max(max, iter->screenColour.r);
            max = std::max(max, iter->screenColour.g);
            max = std::max(max, iter->screenColour.b);
        }
    }

    double scale = max < TARGET ? TARGET / max : 1.0;
    for (std::vector<Quad>::iterator iter = qs.begin(), end = qs.end();
         iter != end; ++iter) {
        if (!iter->isEmitter) {
            iter->screenColour = iter->screenColour * scale;
        }
    }
}

void MouseCallback(int x, int y){
    sceneCamera.ProcessMouseMovement(x, y);
    glLoadIdentity();
    gluLookAt(sceneCamera.Position.x, sceneCamera.Position.y, sceneCamera.Position.z,
              sceneCamera.Position.x + sceneCamera.Front.x, sceneCamera.Position.y + sceneCamera.Front.y, sceneCamera.Position.z + sceneCamera.Front.z,  // Looking at origin
              sceneCamera.Up.x, sceneCamera.Up.y, sceneCamera.Up.z); // Up is in positive Y direction
    glutPostRedisplay();
}

void KeyboardCallback(unsigned char key, int x, int y){
    // Keyboard Inputs for movement
    bool isIntersection;
    if(key == 'w'){
        isIntersection = sceneCamera.ProcessKeyboard(FORWARD, vertices);
        if(isIntersection == true){
            return;
        }

        glLoadIdentity();
        gluLookAt(sceneCamera.Position.x, sceneCamera.Position.y, sceneCamera.Position.z,
                  sceneCamera.Position.x + sceneCamera.Front.x, sceneCamera.Position.y + sceneCamera.Front.y, sceneCamera.Position.z + sceneCamera.Front.z,  // Looking at origin
                  sceneCamera.Up.x, sceneCamera.Up.y, sceneCamera.Up.z); // Up is in positive Y direction
        glutPostRedisplay();
    }
    else if(key == 's'){
        isIntersection = sceneCamera.ProcessKeyboard(BACKWARD, vertices);
        if(isIntersection == true){
            return;
        }
        
        glLoadIdentity();
        gluLookAt(sceneCamera.Position.x, sceneCamera.Position.y, sceneCamera.Position.z,
                  sceneCamera.Position.x + sceneCamera.Front.x, sceneCamera.Position.y + sceneCamera.Front.y, sceneCamera.Position.z + sceneCamera.Front.z,  // Looking at origin
                  sceneCamera.Up.x, sceneCamera.Up.y, sceneCamera.Up.z); // Up is in positive Y direction
        glutPostRedisplay();
    }
    else if(key == 'a'){
        isIntersection = sceneCamera.ProcessKeyboard(LEFT, vertices);
        if(isIntersection == true){
            return;
        }
        
        glLoadIdentity();
        gluLookAt(sceneCamera.Position.x, sceneCamera.Position.y, sceneCamera.Position.z,
                  sceneCamera.Position.x + sceneCamera.Front.x, sceneCamera.Position.y + sceneCamera.Front.y, sceneCamera.Position.z + sceneCamera.Front.z,  // Looking at origin
                  sceneCamera.Up.x, sceneCamera.Up.y, sceneCamera.Up.z); // Up is in positive Y direction
        glutPostRedisplay();
    }
    else if(key == 'd'){
        isIntersection = sceneCamera.ProcessKeyboard(RIGHT, vertices);
        if(isIntersection == true){
            return;
        }
        
        glLoadIdentity();
        gluLookAt(sceneCamera.Position.x, sceneCamera.Position.y, sceneCamera.Position.z,
                  sceneCamera.Position.x + sceneCamera.Front.x, sceneCamera.Position.y + sceneCamera.Front.y, sceneCamera.Position.z + sceneCamera.Front.z,  // Looking at origin
                  sceneCamera.Up.x, sceneCamera.Up.y, sceneCamera.Up.z); // Up is in positive Y direction
        glutPostRedisplay();
    }

    // Keyboard input for saving current screenshot
    else if(key == 'c'){
        std::string path = "../png/" + currentDateTime() + ".png";
        screenshotPNG(path.c_str());
    }
}

static void render()
{   
    printf("%s\n", "Rendering Started");
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("Radiosity demo");
    printf("%s\n", "Window Created");
    glutDisplayFunc(display);
    glutPassiveMotionFunc(MouseCallback);
    glutKeyboardFunc(KeyboardCallback);
    initGL();
    printf("%s\n", "Initialized Opengl");
    // Render one-off first, so we can get it saved to disk without
    // getting it limited to screen size...
    display();
    printf("%s\n", "display function called");
    glutMainLoop();
    printf("%s\n", "glutMainLoop");
}

void renderFlat(std::vector<Quad> f, std::vector<Vertex> v)
{
    flatFaces = f;
    vertices = v;
    render();
}

void renderGouraud(std::vector<GouraudQuad> f, std::vector<Vertex> v)
{
    gouraudFaces = f;
    vertices = v;
    render();
}

// Get current date/time, format is YYYY-MM-DD.HH:mm:ss
const std::string currentDateTime() {
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d.%X", &tstruct);

    return buf;
}

std::vector<float> getCameraPos(){
    std::vector<float> pos(3, 0);
    pos[0] = sceneCamera.Position.x;
    pos[1] = sceneCamera.Position.y;
    pos[2] = sceneCamera.Position.z;

    return pos;
}

