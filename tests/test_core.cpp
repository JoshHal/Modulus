#include <cassert>
#include <cmath>
#include <iostream>
#include "../core.h"

using namespace ModulusLite;

// ============================================================================
// TEST UTILITIES
// ============================================================================

void assert_equal(double a, double b, double epsilon = 1e-6) {
    assert(std::abs(a - b) < epsilon);
}

void assert_point_equal(const Point3D& a, const Point3D& b, double epsilon = 1e-6) {
    assert_equal(a.x, b.x, epsilon);
    assert_equal(a.y, b.y, epsilon);
    assert_equal(a.z, b.z, epsilon);
}

// ============================================================================
// TESTS FOR POINT3D
// ============================================================================

void test_point3d_distance() {
    Point3D p1(0, 0, 0);
    Point3D p2(3, 4, 0);
    assert_equal(p1.distance(p2), 5.0);
    std::cout << "✓ Point3D distance test passed\n";
}

void test_point3d_magnitude() {
    Point3D p(3, 4, 0);
    assert_equal(p.magnitude(), 5.0);
    std::cout << "✓ Point3D magnitude test passed\n";
}

void test_point3d_normalize() {
    Point3D p(3, 4, 0);
    Point3D n = p.normalize();
    assert_equal(n.magnitude(), 1.0);
    assert_equal(n.x, 0.6);
    assert_equal(n.y, 0.8);
    std::cout << "✓ Point3D normalize test passed\n";
}

void test_point3d_dot_product() {
    Point3D a(1, 2, 3);
    Point3D b(4, 5, 6);
    double dot = a.dot(b);
    assert_equal(dot, 32.0);  // 1*4 + 2*5 + 3*6
    std::cout << "✓ Point3D dot product test passed\n";
}

void test_point3d_cross_product() {
    Point3D a(1, 0, 0);
    Point3D b(0, 1, 0);
    Point3D c = a.cross(b);
    assert_point_equal(c, Point3D(0, 0, 1));
    std::cout << "✓ Point3D cross product test passed\n";
}

// ============================================================================
// TESTS FOR FDG LAYOUT
// ============================================================================

void test_fdg_no_overlap() {
    std::vector<Machine> machines(3);
    
    // Initialize machines
    for (int i = 0; i < 3; ++i) {
        machines[i].id = i;
        machines[i].name = "M" + std::to_string(i);
        machines[i].position = Point3D(i * 10, 0, 0);
        machines[i].size = Point3D(1, 1, 1);
        machines[i].velocity = Point3D(0, 0, 0);
    }
    
    // Run layout
    FDGLayout::layout(machines);
    
    // Check no overlap
    for (int i = 0; i < 3; ++i) {
        for (int j = i + 1; j < 3; ++j) {
            double minDist = (machines[i].size.x + machines[j].size.x) / 2.0;
            double actualDist = machines[i].position.distance(machines[j].position);
            assert(actualDist >= minDist * 0.9);  // Allow 10% tolerance
        }
    }
    
    std::cout << "✓ FDG no-overlap test passed\n";
}

void test_fdg_workspace_bounds() {
    std::vector<Machine> machines(2);
    
    machines[0].id = 0;
    machines[0].position = Point3D(0, 0, 0);
    machines[0].size = Point3D(1, 1, 1);
    machines[0].velocity = Point3D(0, 0, 0);
    
    machines[1].id = 1;
    machines[1].position = Point3D(100, 100, 100);  // Out of bounds
    machines[1].size = Point3D(1, 1, 1);
    machines[1].velocity = Point3D(0, 0, 0);
    
    FDGLayout::layout(machines);
    
    // Check bounds
    for (const auto& m : machines) {
        assert(m.position.x >= 0.5 && m.position.x <= FDGLayout::WORKSPACE_X - 0.5);
        assert(m.position.y >= 0.5 && m.position.y <= FDGLayout::WORKSPACE_Y - 0.5);
        assert(m.position.z >= 0.5 && m.position.z <= FDGLayout::WORKSPACE_Z - 0.5);
    }
    
    std::cout << "✓ FDG workspace bounds test passed\n";
}

// ============================================================================
// TESTS FOR A* ROUTING
// ============================================================================

void test_astar_simple_path() {
    std::vector<Machine> machines;
    std::vector<Pipe> pipes;
    
    Point3D start(0, 0, 0);
    Point3D goal(10, 10, 0);
    
    auto result = ManhattanRouter::route(start, goal, machines, pipes);
    
    assert(result.success);
    assert(result.path.size() > 0);
    assert_point_equal(result.path.front(), start, 1.0);  // First point near start
    assert_point_equal(result.path.back(), goal, 1.0);    // Last point near goal
    
    std::cout << "✓ A* simple path test passed\n";
}

void test_astar_avoids_machine() {
    std::vector<Machine> machines(1);
    std::vector<Pipe> pipes;
    
    // Create a machine obstacle
    machines[0].id = 0;
    machines[0].position = Point3D(5, 5, 0);
    machines[0].size = Point3D(2, 2, 1);
    
    Point3D start(0, 5, 0);
    Point3D goal(10, 5, 0);
    
    auto result = ManhattanRouter::route(start, goal, machines, pipes);
    
    if (result.success) {
        // Check that path doesn't collide with machine
        for (const auto& pt : result.path) {
            Point3D delta = pt - machines[0].position;
            bool inMachine = std::abs(delta.x) < machines[0].size.x / 2 + 1.0 &&
                            std::abs(delta.y) < machines[0].size.y / 2 + 1.0 &&
                            std::abs(delta.z) < machines[0].size.z / 2 + 1.0;
            assert(!inMachine);
        }
    }
    
    std::cout << "✓ A* obstacle avoidance test passed\n";
}

// ============================================================================
// TESTS FOR PIPE PROPERTIES
// ============================================================================

void test_pipe_diameter() {
    assert_equal(PipeProperties::getDiameter(MaterialType::Gas), 0.15);
    assert_equal(PipeProperties::getDiameter(MaterialType::Liquid), 0.10);
    assert_equal(PipeProperties::getDiameter(MaterialType::Solid), 0.25);
    assert_equal(PipeProperties::getDiameter(MaterialType::Sludge), 0.30);
    std::cout << "✓ Pipe diameter test passed\n";
}

void test_pipe_cost() {
    Pipe pipe;
    pipe.length = 10.0;
    pipe.material = MaterialType::Liquid;
    pipe.diameter = PipeProperties::getDiameter(MaterialType::Liquid);
    
    PipeProperties::computeCost(pipe);
    
    // Cost = 10.0 * 100 * 0.10 = 100.0
    assert_equal(pipe.cost, 100.0);
    std::cout << "✓ Pipe cost test passed\n";
}

void test_pipe_tag_format() {
    Pipe pipe;
    pipe.id = 0;
    pipe.lineNumber = 100;
    pipe.material = MaterialType::Liquid;
    pipe.service = ServiceType::Water;
    pipe.diameter = 0.10;
    pipe.specCode = "CS150";
    
    PipeProperties::generateTag(pipe);
    
    // Expected tag: "100-100-L-W-CS150"
    assert(pipe.tag == "100-100-L-W-CS150");
    std::cout << "✓ Pipe tag format test passed\n";
}

void test_pipe_length_computation() {
    Pipe pipe;
    pipe.path.push_back(Point3D(0, 0, 0));
    pipe.path.push_back(Point3D(3, 0, 0));
    pipe.path.push_back(Point3D(3, 4, 0));
    
    PipeProperties::computeLength(pipe);
    
    // Length should be 3 + 4 = 7
    assert_equal(pipe.length, 7.0);
    std::cout << "✓ Pipe length computation test passed\n";
}

// ============================================================================
// MAIN TEST RUNNER
// ============================================================================

int main() {
    std::cout << "\n=== Modulus Lite Core Tests ===\n\n";
    
    // Point3D tests
    test_point3d_distance();
    test_point3d_magnitude();
    test_point3d_normalize();
    test_point3d_dot_product();
    test_point3d_cross_product();
    
    // FDG tests
    test_fdg_no_overlap();
    test_fdg_workspace_bounds();
    
    // A* tests
    test_astar_simple_path();
    test_astar_avoids_machine();
    
    // Pipe property tests
    test_pipe_diameter();
    test_pipe_cost();
    test_pipe_tag_format();
    test_pipe_length_computation();
    
    std::cout << "\n=== All tests passed! ===\n\n";
    return 0;
}
