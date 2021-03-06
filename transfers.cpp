////////////////////////////////////////////////////////////////////////
//
// transfers.cpp: Calculate the weights used for working out how the
// light is transferred between elements.
//
// For the rendering version, we render the scene from the view of all
// elements to see where the light comes from.
//
// Copyright (c) Simon Frankau 2018
//

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <cmath>
#include <iostream>
#include <vector>

#include "geom.h"
#include "glut_wrap.h"
#include "transfers.h"
#include "weighting.h"

////////////////////////////////////////////////////////////////////////
// Functions to face in cube map directions
//

// For the sides, the forward-facing part is mapped to the bottom of
// the framebuffer, which is the start of the data when read with
// glReadPixels, so that we only need to do the weighted sum on the
// first half of the read data.

static void viewFront(void)
{
    // Don't need to do anything to look forward
}

static void viewBack(void)
{
    glRotated(180.0, 0.0, 1.0, 0.0);
}

static void viewRight(void)
{
    glRotated(+90.0, 0.0, 0.0, 1.0);
    glRotated(+90.0, 0.0, 1.0, 0.0);
}

static void viewLeft(void)
{
    glRotated(-90.0, 0.0, 0.0, 1.0);
    glRotated(-90.0, 0.0, 1.0, 0.0);
}

static void viewUp(void)
{
    glRotated(-90.0, 1.0, 0.0, 0.0);
}

static void viewDown(void)
{
    glRotated(180.0, 0.0, 0.0, 1.0);
    glRotated(+90.0, 1.0, 0.0, 0.0);
}

////////////////////////////////////////////////////////////////////////
// Use scene rendering to calculate the transfer functions.
//

RenderTransferCalculator::RenderTransferCalculator(
    std::vector<Vertex> const &vertices,
    std::vector<Quad> const &faces,
    int resolution)
    : m_vertices(vertices),
      m_faces(faces),
      m_resolution(resolution),
      m_win(gwTransferSetup(resolution))
{
}

RenderTransferCalculator::~RenderTransferCalculator()
{
    glutDestroyWindow(m_win);
}

// Extremely simple rendering of the scene.
void RenderTransferCalculator::render(void)
{
    for (int i = 0, n = m_faces.size(); i < n; ++i) {
        m_faces[i].renderIndex(i + 1, m_vertices);
    }
}

static const int NUM_CHANS = 4;

// Sum up value of the pixels, with the given weights.
void RenderTransferCalculator::sumWeights(std::vector<double> const &weights)
{
    std::vector<GLubyte> pixels(NUM_CHANS * weights.size());
    glReadPixels(0, 0, m_resolution, weights.size() / m_resolution,
                 GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);

    for (int i = 0, n = pixels.size(); i < n; i += NUM_CHANS) {
        // We're not using that many polys, so skip the low bits.
        int index = (pixels[i] + (pixels[i+1] << 6) + (pixels[i+2] << 12)) >> 2;
        if (index > 0) {
            m_sums[index - 1] += weights[i/4];
        }
    }
}

// Work out contributions from the given face.
void RenderTransferCalculator::calcFace(
    Camera const &cam,
    viewFn_t view,
    std::vector<double> const &weights)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    view();
    cam.applyViewTransform();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    render();
    sumWeights(weights);
    // glutSwapBuffers is unnecessary for offscreen calculation.
}

// Calculate the area subtended by the faces, using a cube map.
std::vector<double> RenderTransferCalculator::calcSubtended(Camera const &cam)
{
    m_sums.clear();
    m_sums.resize(m_faces.size());

    std::vector<double> const &ws = getSubtendWeights();

    calcFace(cam, viewFront, ws);
    calcFace(cam, viewBack,  ws);
    calcFace(cam, viewRight, ws);
    calcFace(cam, viewLeft,  ws);
    calcFace(cam, viewUp,    ws);
    calcFace(cam, viewDown,  ws);

    return m_sums;
}

// Calculate the light received, using half a cube map.
std::vector<double> RenderTransferCalculator::calcLight(Camera const &cam)
{
    m_sums.clear();
    m_sums.resize(m_faces.size());

    std::vector<double> const &fws = getForwardLightWeights();
    std::vector<double> const &sws = getSideLightWeights();

    calcFace(cam, viewFront, fws);
    // Avoid rendering things we don't need to. Doesn't seem to
    // actually make rendering go faster! I also tried calling
    // glutReshapeWindow, similarly didn't affect performance. I only
    // care about rendering half the scene, but can't make it go much
    // faster by trying to convince the renderer of this...
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, m_resolution, m_resolution / 2);
    calcFace(cam, viewRight, sws);
    calcFace(cam, viewLeft,  sws);
    calcFace(cam, viewUp,    sws);
    calcFace(cam, viewDown,  sws);
    glDisable(GL_SCISSOR_TEST);

    return m_sums;
}

std::vector<double> const &RenderTransferCalculator::getSubtendWeights()
{
    if (m_subtendWeights.empty()) {
        calcSubtendWeights(m_resolution, m_subtendWeights);
    }
    return m_subtendWeights;
}

std::vector<double> const &RenderTransferCalculator::getForwardLightWeights()
{
    if (m_forwardLightWeights.empty()) {
        calcForwardLightWeights(m_resolution, m_forwardLightWeights);
    }
    return m_forwardLightWeights;
}

std::vector<double> const &RenderTransferCalculator::getSideLightWeights()
{
    if (m_sideLightWeights.empty()) {
        calcSideLightWeights(m_resolution, m_sideLightWeights);
    }
    return m_sideLightWeights;
}

void RenderTransferCalculator::calcAllLights(std::vector<double> &weights)
{
    int const n = m_faces.size();
    weights.clear();
    weights.reserve(n * n);

    // Iterate over targets
    for (int i = 0; i < n; ++i) {
        Quad currQuad = m_faces[i];
        Vertex eye(paraCentre(currQuad, m_vertices)); // Gives centre of the quad
        Vertex dir(paraCross(currQuad, m_vertices)); // Gives normal to the quad
        Vertex lookAt(eye - dir);
        Vertex up(dir.perp());
        Camera cam(eye, lookAt, up);

        std::vector<double> faceWeights = calcLight(cam);
        weights.insert(weights.end(), faceWeights.begin(), faceWeights.end());
        // Somewhat slow, so print progress.
        std::cerr << ".";
    }
    std::cerr << std::endl;
}

////////////////////////////////////////////////////////////////////////
// Calculate analytic approximations of the transfer functions.
//

AnalyticTransferCalculator::AnalyticTransferCalculator(
    std::vector<Vertex> const &vertices,
    std::vector<Quad> const &faces)
    : m_vertices(vertices),
      m_faces(faces)
{
}

std::vector<double> AnalyticTransferCalculator::calcSubtended(
    Camera const &cam)
{
    std::vector<double> weights;
    for (int i = 0, n = m_faces.size(); i < n; ++i) {
        weights.push_back(calcSingleQuadSubtended(cam, m_faces[i]));
    }
    return weights;
}

double AnalyticTransferCalculator::calcSingleQuadSubtended(
    Camera const &cam,
    Quad const &quad) const
{
    Vertex centre = paraCentre(quad, m_vertices);
    Vertex dir = centre - cam.getEyePos();

    // Inverse square component.
    double l = dir.len();
    double r2 = 1.0 / (l * l);

    // Area, scaled by angle to camera.
    dir = dir.norm();
    Vertex norm = paraCross(quad, m_vertices);
    double area = fmax(0, dot(norm, dir));

    // Normalise to surface area of 6.
    return 1.5 * r2 * area / M_PI;
}

void AnalyticTransferCalculator::calcAllLights(std::vector<double> &weights)
{
    int const n = m_faces.size();
    weights.clear();
    weights.reserve(n * n);
    Vertex up(0.0, 0.0, 0.0);

    // Iterate over targets
    for (int i = 0; i < n; ++i) {
        Quad currQuad = m_faces[i];
        Vertex eye(paraCentre(currQuad, m_vertices));
        Vertex lookAt(eye - paraCross(currQuad, m_vertices));
        Camera cam(eye, lookAt, up);

        // Iterate over sources
        for (int j = 0; j < n; ++j) {
            weights.push_back(calcSingleQuadLight(cam, m_faces[j]));
        }
    }
}

std::vector<double> AnalyticTransferCalculator::calcLight(
    Camera const &cam)
{
    std::vector<double> weights;
    for (int i = 0, n = m_faces.size(); i < n; ++i) {
        weights.push_back(calcSingleQuadLight(cam, m_faces[i]));
    }
    return weights;
}

double AnalyticTransferCalculator::calcSingleQuadLight(
    Camera const &cam,
    Quad const &quad) const
{
    Vertex eyePos = cam.getEyePos();
    Vertex centre = paraCentre(quad, m_vertices);
    Vertex dir = centre - eyePos;

    // Inverse square component.
    double l = dir.len();
    double r2 = 1.0 / (l * l);

    // Area, scaled by angle to camera.
    dir = dir.norm();
    Vertex norm = paraCross(quad, m_vertices);
    double area = fmax(0, dot(norm, dir));

    // And angle to surface.
    Vertex lookVec = (cam.getLookAt() - eyePos).norm();
    double cosCamAngle = fmax(0.0, dot(lookVec, dir));

    // Normalise to surface area of 6.
    return cosCamAngle * r2 * area / M_PI;
}
