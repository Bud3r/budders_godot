/**************************************************************************/
/*  nav_polygon_bvh_3d.cpp                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "nav_polygon_bvh_3d.h"

#include "core/templates/sort_array.h"

namespace {

constexpr uint32_t LEAF_SIZE = 4;
// Below this sine between the plane normal and an edge, the inside test can
// misplace a point far from the polygon, so the polygon is always tested.
constexpr real_t MIN_EDGE_NORMAL_SINE = 0.05;

struct CenterComparator {
	const Vector3 *centers = nullptr;
	int axis = 0;

	_FORCE_INLINE_ bool operator()(uint32_t p_a, uint32_t p_b) const {
		return centers[p_a][axis] < centers[p_b][axis];
	}
};

struct BuildRange {
	uint32_t start;
	uint32_t end;
	uint32_t parent;
};

} // namespace

void NavPolygonBVH3D::build(const LocalVector<Nav3D::Polygon> &p_polygons) {
	clear();

	real_t max_abs = 0.0;
	for (const Nav3D::Polygon &polygon : p_polygons) {
		for (const Vector3 &v : polygon.vertices) {
			max_abs = MAX(max_abs, MAX(Math::abs(v.x), MAX(Math::abs(v.y), Math::abs(v.z))));
		}
	}
	const real_t base_pad = real_t(1e-5) + max_abs * real_t(1e-5);

	LocalVector<Bounds> polygon_bounds;
	LocalVector<Vector3> centers;
	polygon_bounds.resize(p_polygons.size());
	centers.resize(p_polygons.size());
	items.reserve(p_polygons.size());

	for (uint32_t i = 0; i < p_polygons.size(); i++) {
		const LocalVector<Vector3> &vertices = p_polygons[i].vertices;
		const uint32_t vertex_count = vertices.size();
		if (vertex_count < 3) {
			continue;
		}

		const Vector3 normal = (vertices[1] - vertices[0]).cross(vertices[2] - vertices[0]).normalized();
		bool skewed = false;

		Bounds b;
		b.min = vertices[0];
		b.max = vertices[0];
		Vector3 previous = vertices[vertex_count - 1];
		for (const Vector3 &v : vertices) {
			const Vector3 projected = v - normal * normal.dot(v - vertices[0]);
			b.min = b.min.min(v).min(projected);
			b.max = b.max.max(v).max(projected);

			const Vector3 edge = v - previous;
			const real_t edge_length = edge.length();
			if (edge_length > 0.0 && !normal.is_zero_approx() && edge.cross(normal).length() < MIN_EDGE_NORMAL_SINE * edge_length) {
				skewed = true;
			}
			previous = v;
		}

		if (skewed) {
			unindexed.push_back(i);
			continue;
		}

		const real_t pad = base_pad + (b.max - b.min).length() * real_t(2e-3);
		b.min -= Vector3(pad, pad, pad);
		b.max += Vector3(pad, pad, pad);
		polygon_bounds[i] = b;
		centers[i] = (b.min + b.max) * 0.5;
		items.push_back(i);
	}

	const uint32_t item_count = items.size();
	if (item_count == 0) {
		return;
	}

	nodes.reserve(2 * (item_count / LEAF_SIZE + 1));

	SortArray<uint32_t, CenterComparator, false> sorter;
	sorter.compare.centers = centers.ptr();

	LocalVector<BuildRange> stack;
	stack.push_back({ 0, item_count, UINT32_MAX });

	while (!stack.is_empty()) {
		const BuildRange range = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);

		const uint32_t node_index = nodes.size();
		nodes.push_back(Node());
		if (range.parent != UINT32_MAX) {
			nodes[range.parent].start = node_index;
		}

		Bounds bounds = polygon_bounds[items[range.start]];
		Vector3 center_min = centers[items[range.start]];
		Vector3 center_max = center_min;
		for (uint32_t i = range.start + 1; i < range.end; i++) {
			const Bounds &b = polygon_bounds[items[i]];
			bounds.min = bounds.min.min(b.min);
			bounds.max = bounds.max.max(b.max);
			center_min = center_min.min(centers[items[i]]);
			center_max = center_max.max(centers[items[i]]);
		}
		nodes[node_index].bounds = bounds;

		const uint32_t count = range.end - range.start;
		if (count <= LEAF_SIZE) {
			nodes[node_index].start = range.start;
			nodes[node_index].count = count;
			continue;
		}

		sorter.compare.axis = (center_max - center_min).max_axis_index();
		const uint32_t mid = range.start + count / 2;
		sorter.nth_element(range.start, range.end, mid, items.ptr());

		// The second child is pushed first so the first child is built next, right after this node.
		stack.push_back({ mid, range.end, node_index });
		stack.push_back({ range.start, mid, UINT32_MAX });
	}

	item_bounds.resize(item_count);
	for (uint32_t i = 0; i < item_count; i++) {
		item_bounds[i] = polygon_bounds[items[i]];
	}
}
