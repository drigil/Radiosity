////////////////////////////////////////////////////////////////////////
//
// rendering.h: Put the calculated polys on the screen
//
// Copyright (c) Simon Frankau 2018
//

#ifndef RADIOSITY_RENDERING_H
#define RADIOSITY_RENDERING_H

// Render the scene in flat-shaded quads
void renderFlat(std::vector<Quad> f, std::vector<Vertex> v);

// Render the scene with Gouraud shading
void renderGouraud(std::vector<Quad> *faces, std::vector<Vertex> *vertices, std::vector<SubdivInfo> *subdivs);

// Normalise the brightness of non-emitting components
void normaliseBrightness(std::vector<Quad> &qs, std::vector<Vertex> const &vs);

void MouseCallback(int x, int y);

const std::string currentDateTime();

std::vector<float> getCameraPos();

void generateQuads(std::vector<Quad> *f, std::vector<Vertex> *v, std::vector<SubdivInfo> *subdivs);

#endif // RADIOSITY_RENDERING_H
