/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __PLANEGRID__H__
#define __PLANEGRID__H__

/**
 * This class creates and renders a textured plane.
 *
 * The plane is tessellated into a regular grid instead of a single quad. Under a normal
 * (linear) perspective projection this makes no visual difference, but the one-pass fisheye
 * projection is nonlinear and is evaluated per vertex, so it needs the extra subdivision to
 * curve the plane correctly on the fulldome disk.
 */
class PlaneGrid {
public:
    /**
     * This constructor requires a valid OpenGL contex.
     */
    PlaneGrid(float width, float height, unsigned int steps = 32);
    ~PlaneGrid();

    void draw();

private:
    unsigned int _vao = 0;
    unsigned int _vbo = 0;
    unsigned int _ibo = 0;
    int _indexCount = 0;
};

#endif // __PLANEGRID__H__
