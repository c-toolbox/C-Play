/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __DOMEGRID__H__
#define __DOMEGRID__H__

/**
 * Helper class to render a dome grid.
 */
class DomeGrid {
public:
    /**
     * This constructor requires a valid OpenGL context.
     */
    DomeGrid(float r, float FOV, unsigned int azimuthSteps, unsigned int elevationSteps);

    /**
     * The destructor requires a valid OpenGL context.
     */
    ~DomeGrid();

    void draw();

private:
    const int _elevationSteps;
    const int _azimuthSteps;

    unsigned int _vao = 0;
    unsigned int _vbo = 0;
    unsigned int _ibo = 0;
};

#endif // __DOMEGRID__H__
