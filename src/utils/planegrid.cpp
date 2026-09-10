/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <utils/planegrid.h>

#include <sgct/opengl.h>
#include <algorithm>
#include <vector>

PlaneGrid::PlaneGrid(float width, float height, unsigned int steps) {
    struct VertexData {
        float s = 0.f;
        float t = 0.f;  // Texcoord0 -> size=8
        float nx = 0.f;
        float ny = 0.f;
        float nz = 0.f; // size=12
        float x = 0.f;
        float y = 0.f;
        float z = 0.f;  // size=12 ; total size=32 = power of two
    };

    const unsigned int cells = std::max(1u, steps);
    const unsigned int perSide = cells + 1;

    std::vector<VertexData> verts;
    verts.reserve(static_cast<size_t>(perSide) * perSide);

    for (unsigned int row = 0; row < perSide; row++) {
        const float t = static_cast<float>(row) / static_cast<float>(cells);
        for (unsigned int col = 0; col < perSide; col++) {
            const float s = static_cast<float>(col) / static_cast<float>(cells);
            verts.push_back({
                s, t,
                0.f, 0.f, 1.f,
                (s - 0.5f) * width, (t - 0.5f) * height, 0.f });
        }
    }

    std::vector<unsigned int> indices;
    indices.reserve(static_cast<size_t>(cells) * cells * 6);

    for (unsigned int row = 0; row < cells; row++) {
        for (unsigned int col = 0; col < cells; col++) {
            const unsigned int bottomLeft = row * perSide + col;
            const unsigned int bottomRight = bottomLeft + 1;
            const unsigned int topLeft = bottomLeft + perSide;
            const unsigned int topRight = topLeft + 1;

            indices.push_back(bottomLeft);
            indices.push_back(bottomRight);
            indices.push_back(topLeft);

            indices.push_back(topLeft);
            indices.push_back(bottomRight);
            indices.push_back(topRight);
        }
    }

    _indexCount = static_cast<int>(indices.size());

    glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);

    glGenBuffers(1, &_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    const GLsizei size = sizeof(VertexData);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * size, verts.data(), GL_STATIC_DRAW);

    // texcoords
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, size, nullptr);

    // normals
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, size, reinterpret_cast<void*>(8));

    // vert positions
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, size, reinterpret_cast<void*>(20));

    glGenBuffers(1, &_ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ibo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW);

    glBindVertexArray(0);
}

PlaneGrid::~PlaneGrid() {
    glDeleteBuffers(1, &_ibo);
    glDeleteBuffers(1, &_vbo);
    glDeleteVertexArrays(1, &_vao);
}

void PlaneGrid::draw() {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, _indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
