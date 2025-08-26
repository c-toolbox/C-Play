/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <utils/spheregrid.h>

#include <sgct/opengl.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <vector>

SphereGrid::SphereGrid(float radius, unsigned int segments) {
    const size_t vsegs = std::max<size_t>(segments, 2);
    const size_t hsegs = vsegs * 2;
    const size_t nVertices = 1 + (vsegs - 1) * (hsegs + 1) + 1; // top + middle + bottom
    _nFaces = static_cast<unsigned int>(
        hsegs + (vsegs - 2) * hsegs * 2 + hsegs
    ); // top + middle + bottom

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

    std::vector<VertexData> verts(nVertices);

    // First vertex: top pole (+y is "up" in object local coords)
    verts[0] = { 0.5f, 1.f, 0.f, 1.f, 0.f, 0.f, radius, 0.f };

    // Last vertex: bottom pole
    verts[nVertices - 1] = { 0.5f, 0.f, 0.f, -1.f, 0.f, 0.f, -radius, 0.f };

    // All other vertices:
    // vsegs-1 latitude rings of hsegs+1 vertices each
    // (duplicates at texture seam s=0 / s=1)
    for (size_t j = 0; j < vsegs - 1; j++) {
        // vsegs-1 latitude rings of vertices
        const double theta = (static_cast<double>(j + 1) / vsegs) * glm::pi<double>();
        const float y = static_cast<float>(cos(theta));
        const float r = static_cast<float>(sin(theta));

        for (size_t i = 0; i <= static_cast<size_t>(hsegs); i++) {
            // hsegs+1 vertices in each ring (duplicate for texcoords)
            const double phi = (static_cast<double>(i) / hsegs) * glm::two_pi<double>();
            const float x = r * static_cast<float>(cos(phi));
            const float z = r * static_cast<float>(sin(phi));

            verts[1 + j * (hsegs + 1) + i] = {
                static_cast<float>(i) / static_cast<float>(hsegs), // s
                1.f - static_cast<float>(j + 1) / static_cast<float>(vsegs), // t
                x, y, z, // normals
                radius * x, radius * y, radius * z
            };
        }
    }

    std::vector<unsigned int> indices(static_cast<size_t>(_nFaces) * 3, 0);
    // The index array: triplets of integers, one for each triangle
    // Top cap
    for (size_t i = 0; i < hsegs; i++) {
        indices[3 * i] = static_cast<unsigned int>(0);
        indices[3 * i + 2] = static_cast<unsigned int>(1 + i);
        indices[3 * i + 1] = static_cast<unsigned int>(2 + i);
    }
    // Middle part (possibly empty if vsegs=2)
    for (size_t j = 0; j < vsegs - 2; j++) {
        for (size_t i = 0; i < hsegs; i++) {
            const size_t base = 3 * (hsegs + 2 * (j * hsegs + i));
            const size_t i0 = 1 + j * (hsegs + 1) + i;
            indices[base] = static_cast<unsigned int>(i0);
            indices[base + 1] = static_cast<unsigned int>(i0 + 1);
            indices[base + 2] = static_cast<unsigned int>(i0 + hsegs + 1);
            indices[base + 3] = static_cast<unsigned int>(i0 + hsegs + 1);
            indices[base + 4] = static_cast<unsigned int>(i0 + 1);
            indices[base + 5] = static_cast<unsigned int>(i0 + hsegs + 2);
        }
    }
    // Bottom cap
    for (size_t i = 0; i < static_cast<size_t>(hsegs); i++) {
        const size_t base = 3 * (hsegs + 2 * (vsegs - 2) * hsegs);
        indices[base + 3 * i] = static_cast<unsigned int>(nVertices - 1);
        indices[base + 3 * i + 2] = static_cast<unsigned int>(nVertices - 2 - i);
        indices[base + 3 * i + 1] = static_cast<unsigned int>(nVertices - 3 - i);
    }

    constexpr GLsizei size = sizeof(VertexData);

    glGenVertexArrays(1, &_vao);
    glBindVertexArray(_vao);

    glGenBuffers(1, &_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, _vbo);
    glBufferData(GL_ARRAY_BUFFER, nVertices * size, verts.data(), GL_STATIC_DRAW);

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
        static_cast<GLsizeiptr>(_nFaces) * 3 * sizeof(unsigned int),
        indices.data(),
        GL_STATIC_DRAW
    );

    glBindVertexArray(0);
}

SphereGrid::~SphereGrid() {
    glDeleteVertexArrays(1, &_vao);
    glDeleteBuffers(1, &_vbo);
    glDeleteBuffers(1, &_ibo);
}

void SphereGrid::draw() {
    glBindVertexArray(_vao);
    glDrawElements(GL_TRIANGLES, _nFaces * 3, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
