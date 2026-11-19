/**************************************************************************/
/*  nav_polygon_bvh_3d.h                                                  */
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

#pragma once

#include "../nav_utils_3d.h"

#include "core/math/face3.h"
#include "core/templates/local_vector.h"

// Bounding volume hierarchy over the polygons of one region iteration, used to
// find the polygons near a point without testing every polygon.
//
// Each polygon's box holds every point a closest point query can report for it:
// its vertices, and the vertices projected onto the plane of its first triangle
// (where the polygon closest point query projects points that fall inside it).
// Boxes are grown by a small tolerance against float error. Polygons whose
// plane is so skewed against an edge that the inside test is unreliable are
// kept out of the tree and visited by every query.
class NavPolygonBVH3D {
public:
	struct Bounds {
		Vector3 min;
		Vector3 max;

		_FORCE_INLINE_ real_t distance_squared_to(const Vector3 &p_point) const {
			real_t d = 0.0;
			for (int i = 0; i < 3; i++) {
				real_t v = p_point[i];
				if (v < min[i]) {
					real_t e = min[i] - v;
					d += e * e;
				} else if (v > max[i]) {
					real_t e = v - max[i];
					d += e * e;
				}
			}
			return d;
		}
	};

private:
	struct Node {
		Bounds bounds;
		// Leaf: first entry in `items`. Inner node: index of the second child (the first child follows the node).
		uint32_t start = 0;
		// Leaf: number of items. Inner node: 0.
		uint32_t count = 0;
	};

	LocalVector<Node> nodes;
	LocalVector<uint32_t> items;
	LocalVector<Bounds> item_bounds;
	LocalVector<uint32_t> unindexed;

public:
	// Distances found within this tolerance of the best one are still visited.
	static constexpr real_t RELATIVE_SLACK = 1e-3;

	static real_t query_tolerance(const Vector3 &p_point) {
		real_t m = MAX(Math::abs(p_point.x), MAX(Math::abs(p_point.y), Math::abs(p_point.z)));
		return real_t(1e-4) + m * real_t(1e-5);
	}

	// Squared distance a box must be within to be visited, for a best distance found so far.
	static real_t visit_threshold_squared(real_t p_best_distance, real_t p_tolerance) {
		if (!(p_best_distance < real_t(FLT_MAX))) {
			return real_t(Math::INF);
		}
		real_t d = p_best_distance * (real_t(1.0) + RELATIVE_SLACK) + p_tolerance;
		return d * d;
	}

	void clear() {
		nodes.clear();
		items.clear();
		item_bounds.clear();
		unindexed.clear();
	}

	void build(const LocalVector<Nav3D::Polygon> &p_polygons);

	// Calls `r_visitor.visit(polygon_index)` for every polygon whose box is within
	// `sqrt(r_visitor.threshold_squared)` of the point, nearest boxes first.
	// The visitor may lower `threshold_squared` as it finds closer polygons.
	template <typename Visitor>
	void query(const Vector3 &p_point, Visitor &r_visitor) const {
		for (uint32_t polygon_index : unindexed) {
			r_visitor.visit(polygon_index);
		}

		if (nodes.is_empty()) {
			return;
		}

		struct StackEntry {
			uint32_t node;
			real_t distance_squared;
		};
		StackEntry stack[64];
		uint32_t stack_size = 0;

		uint32_t node_index = 0;
		if (nodes[0].bounds.distance_squared_to(p_point) > r_visitor.threshold_squared) {
			return;
		}

		while (true) {
			const Node &node = nodes[node_index];
			if (node.count > 0) {
				const uint32_t end = node.start + node.count;
				for (uint32_t i = node.start; i < end; i++) {
					if (item_bounds[i].distance_squared_to(p_point) <= r_visitor.threshold_squared) {
						r_visitor.visit(items[i]);
					}
				}
			} else {
				uint32_t near_child = node_index + 1;
				uint32_t far_child = node.start;
				real_t near_d = nodes[near_child].bounds.distance_squared_to(p_point);
				real_t far_d = nodes[far_child].bounds.distance_squared_to(p_point);
				if (far_d < near_d) {
					SWAP(near_child, far_child);
					SWAP(near_d, far_d);
				}
				if (near_d <= r_visitor.threshold_squared) {
					if (far_d <= r_visitor.threshold_squared && stack_size < 64) {
						stack[stack_size++] = { far_child, far_d };
					}
					node_index = near_child;
					continue;
				}
			}

			bool next_found = false;
			while (stack_size > 0) {
				const StackEntry &entry = stack[--stack_size];
				if (entry.distance_squared <= r_visitor.threshold_squared) {
					node_index = entry.node;
					next_found = true;
					break;
				}
			}
			if (!next_found) {
				break;
			}
		}
	}

	uint32_t get_node_count() const { return nodes.size(); }
	uint32_t get_unindexed_count() const { return unindexed.size(); }
};

// Closest point on the triangle fans of polygons, matching a linear scan over
// regions, polygons and triangles that keeps the first strictly closer one: ties
// go to the lowest (region, polygon, triangle). SQUARED compares squared distances.
template <bool SQUARED>
struct NavPolygonClosestTriangleSearch3D {
	const LocalVector<Nav3D::Polygon> *polygons = nullptr;
	uint32_t region = 0;

	Vector3 point;
	real_t tolerance = 0.0;
	real_t threshold_squared = real_t(Math::INF);

	bool found = false;
	real_t best = real_t(FLT_MAX);
	uint32_t best_region = 0;
	uint32_t best_polygon = 0;
	uint32_t best_triangle = 0;
	Vector3 best_point;

	NavPolygonClosestTriangleSearch3D(const Vector3 &p_point, real_t p_max = real_t(FLT_MAX)) :
			point(p_point), best(p_max) {
		tolerance = NavPolygonBVH3D::query_tolerance(p_point);
		_update_threshold();
	}

	void search(const LocalVector<Nav3D::Polygon> &p_polygons, const NavPolygonBVH3D &p_bvh, uint32_t p_region) {
		polygons = &p_polygons;
		region = p_region;
		p_bvh.query(point, *this);
	}

	_FORCE_INLINE_ void visit(uint32_t p_polygon) {
		const Nav3D::Polygon &polygon = (*polygons)[p_polygon];
		bool improved = false;
		for (uint32_t point_id = 2; point_id < polygon.vertices.size(); point_id++) {
			const Face3 face(polygon.vertices[0], polygon.vertices[point_id - 1], polygon.vertices[point_id]);
			const Vector3 closest = face.get_closest_point_to(point);
			const real_t d = SQUARED ? closest.distance_squared_to(point) : closest.distance_to(point);
			if (d < best || (found && d == best && _is_before(p_polygon, point_id))) {
				found = true;
				best = d;
				best_region = region;
				best_polygon = p_polygon;
				best_triangle = point_id;
				best_point = closest;
				improved = true;
			}
		}
		if (improved) {
			_update_threshold();
		}
	}

private:
	_FORCE_INLINE_ bool _is_before(uint32_t p_polygon, uint32_t p_triangle) const {
		if (region != best_region) {
			return region < best_region;
		}
		if (p_polygon != best_polygon) {
			return p_polygon < best_polygon;
		}
		return p_triangle < best_triangle;
	}

	void _update_threshold() {
		if (!(best < real_t(FLT_MAX))) {
			threshold_squared = real_t(Math::INF);
			return;
		}
		threshold_squared = NavPolygonBVH3D::visit_threshold_squared(SQUARED ? Math::sqrt(best) : best, tolerance);
	}
};
