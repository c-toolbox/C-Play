/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __PLANEGRID__H__
#define __PLANEGRID__H__

/**
 * This class creates and renders a textured box.
 */
class PlaneGrid {
public:
    /**
     * This constructor requires a valid OpenGL contex.
     */
    PlaneGrid(float width, float height);
    ~PlaneGrid();

    void draw();

private:
    unsigned int _vao = 0;
    unsigned int _vbo = 0;
};

#endif // __PLANEGRID__H__
