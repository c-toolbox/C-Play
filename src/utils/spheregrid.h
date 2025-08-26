/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef __SPHEREGRID__H__
#define __SPHEREGRID__H__

class SphereGrid {
public:
    SphereGrid(float radius, unsigned int segments);
    ~SphereGrid();

    void draw();

private:
    unsigned int _nFaces = 0;

    unsigned int _vao = 0;
    unsigned int _vbo = 0;
    unsigned int _ibo = 0;
};

#endif // __SPHEREGRID__H__
