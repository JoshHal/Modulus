#pragma once

#include <vector>
#include <cmath>
#include <algorithm>
#include <queue>
#include <unordered_map>
#include <string>

namespace ModulusLite {

// ============================================================================
// ENUMERATIONS
// ============================================================================

enum class MaterialType { Liquid, Gas, Solid, Sludge };
enum class ServiceType { Water, Air, Slurry, Chemical };

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct Point3D {
    double x, y, z;
    
    Point3D(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    
    Point3D operator+(const Point3D& p) const {
        return Point3D(x + p.x, y + p.y, z + p.z);
    }
    
    Point3D operator-(const Point3D& p) const {
        return Point3D(x - p.x, y - p.y, z - p.z);
    }
    
    Point3D operator*(double s) const {
        return Point3D(x * s, y * s, z * s);
    }
    
    double dot(const Point3D& p) const {
        return x * p.x + y * p.y + z * p.z;
    }
    
    Point3D cross(const Point3D& p) const {
        return Point3D(y * p.z - z * p.y, z * p.x - x * p.z, x * p.y - y * p.x);
    }
    
    double distance(const Point3D& p) const {
        double dx = x - p.x, dy = y - p.y, dz = z - p.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    
    double magnitude() const {
        return std::sqrt(x*x + y*y + z*z);
    }
    
    Point3D normalize() const {
        double len = magnitude();
        if (len < 1e-6) return Point3D(0, 0, 0);
        return *this * (1.0 / len);
    }
};

struct Machine {
    int id;
    std::string name;
    Point3D position;
    Point3D size;
    std::vector<Point3D> ports;
    std::string modelPath;
    
    // Temporary: velocity for FDG
    Point3D velocity;
};

struct Pipe {
    int id;
    int fromMachineId;
    int toMachineId;
    int fromPortIndex;
    int toPortIndex;
    
    std::vector<Point3D> path;
    
    double diameter;
    MaterialType material;
    ServiceType service;
    
    double length;
    double cost;
    
    int lineNumber;
    std::string specCode;
    std::string tag;
};

// ============================================================================
// FORCE-DIRECTED GRAPH LAYOUT
// ============================================================================

class FDGLayout {
public:
    static constexpr double K = 5.0;           // Spring constant
    static constexpr double REPULSION = 50.0;  // Repulsion strength
    static constexpr int ITERATIONS = 100;
    static constexpr double DAMPING = 0.9;
    static constexpr double WORKSPACE_X = 20.0;
    static constexpr double WORKSPACE_Y = 15.0;
    static constexpr double WORKSPACE_Z = 5.0;
    
    static void layout(std::vector<Machine>& machines) {
        // Initialize velocities
        for (auto& m : machines) {
            m.velocity = Point3D(0, 0, 0);
        }
        
        // Iterative layout
        for (int iter = 0; iter < ITERATIONS; ++iter) {
            // Reset forces
            std::vector<Point3D> forces(machines.size(), Point3D(0, 0, 0));
            
            // Repulsive forces (machines pushing each other away)
            for (size_t i = 0; i < machines.size(); ++i) {
                for (size_t j = i + 1; j < machines.size(); ++j) {
                    Point3D delta = machines[i].position - machines[j].position;
                    double dist = delta.magnitude();
                    if (dist < 0.01) dist = 0.01;
                    
                    Point3D dir = delta.normalize();
                    double repulsion = (REPULSION * K * K) / (dist * dist);
                    
                    forces[i] = forces[i] + dir * repulsion;
                    forces[j] = forces[j] + dir * (-repulsion);
                }
            }
            
            // Attractive forces (pull toward center and spacing)
            for (size_t i = 0; i < machines.size(); ++i) {
                // Center attraction (weak)
                Point3D center(WORKSPACE_X / 2, WORKSPACE_Y / 2, WORKSPACE_Z / 2);
                Point3D toCenter = center - machines[i].position;
                forces[i] = forces[i] + toCenter * 0.01;
            }
            
            // Update positions
            for (size_t i = 0; i < machines.size(); ++i) {
                machines[i].velocity = (machines[i].velocity + forces[i]) * DAMPING;
                machines[i].position = machines[i].position + machines[i].velocity;
                
                // Clamp to workspace
                machines[i].position.x = std::clamp(machines[i].position.x, 
                    machines[i].size.x / 2, WORKSPACE_X - machines[i].size.x / 2);
                machines[i].position.y = std::clamp(machines[i].position.y,
                    machines[i].size.y / 2, WORKSPACE_Y - machines[i].size.y / 2);
                machines[i].position.z = std::clamp(machines[i].position.z,
                    machines[i].size.z / 2, WORKSPACE_Z - machines[i].size.z / 2);
            }
        }
    }
};

// ============================================================================
// A* MANHATTAN ROUTING
// ============================================================================

struct GridNode {
    int x, y, z;
    
    GridNode(int x = 0, int y = 0, int z = 0) : x(x), y(y), z(z) {}
    
    bool operator==(const GridNode& other) const {
        return x == other.x && y == other.y && z == other.z;
    }
    
    int hash() const {
        return x * 10007 ^ y * 10009 ^ z * 10037;
    }
};

class ManhattanRouter {
public:
    static constexpr double GRID_RESOLUTION = 0.5;  // meters
    static constexpr double MACHINE_BUFFER = 1.0;    // buffer around machines
    static constexpr double PIPE_PENALTY = 5.0;      // cost for crossing pipes
    
    struct RouteResult {
        bool success;
        std::vector<Point3D> path;
    };
    
    static RouteResult route(
        const Point3D& start,
        const Point3D& goal,
        const std::vector<Machine>& machines,
        const std::vector<Pipe>& existingPipes
    ) {
        GridNode startNode = toGrid(start);
        GridNode goalNode = toGrid(goal);
        
        std::unordered_map<int, GridNode> cameFrom;
        std::unordered_map<int, double> gScore, fScore;
        
        auto openSet = std::priority_queue<std::pair<double, GridNode>,
            std::vector<std::pair<double, GridNode>>,
            std::greater<std::pair<double, GridNode>>>();
        
        openSet.push({heuristic(startNode, goalNode), startNode});
        gScore[startNode.hash()] = 0;
        fScore[startNode.hash()] = heuristic(startNode, goalNode);
        
        int maxIter = 10000;
        while (!openSet.empty() && --maxIter > 0) {
            auto [_, current] = openSet.top();
            openSet.pop();
            
            if (current == goalNode) {
                return {true, reconstructPath(cameFrom, current)};
            }
            
            // Explore 6 neighbors (±X, ±Y, ±Z)
            GridNode neighbors[] = {
                GridNode(current.x + 1, current.y, current.z),
                GridNode(current.x - 1, current.y, current.z),
                GridNode(current.x, current.y + 1, current.z),
                GridNode(current.x, current.y - 1, current.z),
                GridNode(current.x, current.y, current.z + 1),
                GridNode(current.x, current.y, current.z - 1),
            };
            
            for (const auto& neighbor : neighbors) {
                double tentativeG = gScore[current.hash()] + 
                    nodeCost(neighbor, machines, existingPipes);
                
                int nHash = neighbor.hash();
                if (gScore.find(nHash) == gScore.end() || tentativeG < gScore[nHash]) {
                    cameFrom[nHash] = current;
                    gScore[nHash] = tentativeG;
                    double newF = tentativeG + heuristic(neighbor, goalNode);
                    fScore[nHash] = newF;
                    openSet.push({newF, neighbor});
                }
            }
        }
        
        return {false, std::vector<Point3D>()};
    }
    
private:
    static GridNode toGrid(const Point3D& p) {
        return GridNode(
            static_cast<int>(p.x / GRID_RESOLUTION),
            static_cast<int>(p.y / GRID_RESOLUTION),
            static_cast<int>(p.z / GRID_RESOLUTION)
        );
    }
    
    static Point3D fromGrid(const GridNode& n) {
        return Point3D(n.x * GRID_RESOLUTION, n.y * GRID_RESOLUTION, n.z * GRID_RESOLUTION);
    }
    
    static double heuristic(const GridNode& a, const GridNode& b) {
        return (std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z)) 
            * GRID_RESOLUTION;
    }
    
    static double nodeCost(
        const GridNode& node,
        const std::vector<Machine>& machines,
        const std::vector<Pipe>& pipes
    ) {
        Point3D pos = fromGrid(node);
        
        // Hard blocking: machines
        for (const auto& m : machines) {
            Point3D delta = pos - m.position;
            if (std::abs(delta.x) < m.size.x / 2 + MACHINE_BUFFER &&
                std::abs(delta.y) < m.size.y / 2 + MACHINE_BUFFER &&
                std::abs(delta.z) < m.size.z / 2 + MACHINE_BUFFER) {
                return 1e6;  // Impassable
            }
        }
        
        // Soft penalty: existing pipes
        double pipePenalty = 0.0;
        for (const auto& pipe : pipes) {
            for (size_t i = 0; i + 1 < pipe.path.size(); ++i) {
                if (distanceToSegment(pos, pipe.path[i], pipe.path[i+1]) < 0.5) {
                    pipePenalty += PIPE_PENALTY;
                }
            }
        }
        
        return 1.0 + pipePenalty;
    }
    
    static double distanceToSegment(const Point3D& p, const Point3D& a, const Point3D& b) {
        Point3D ab = b - a;
        Point3D ap = p - a;
        double t = ap.dot(ab) / ab.dot(ab);
        t = std::clamp(t, 0.0, 1.0);
        Point3D closest = a + ab * t;
        return p.distance(closest);
    }
    
    static std::vector<Point3D> reconstructPath(
        const std::unordered_map<int, GridNode>& cameFrom,
        GridNode current
    ) {
        std::vector<GridNode> path;
        path.push_back(current);
        
        while (cameFrom.find(current.hash()) != cameFrom.end()) {
            current = cameFrom.at(current.hash());
            path.push_back(current);
        }
        
        std::reverse(path.begin(), path.end());
        
        std::vector<Point3D> result;
        for (const auto& node : path) {
            result.push_back(fromGrid(node));
        }
        return result;
    }
};

// ============================================================================
// PIPE PROPERTIES & TAGGING
// ============================================================================

class PipeProperties {
public:
    static double getDiameter(MaterialType material) {
        switch (material) {
            case MaterialType::Gas:   return 0.15;
            case MaterialType::Liquid: return 0.10;
            case MaterialType::Solid:  return 0.25;
            case MaterialType::Sludge: return 0.30;
            default: return 0.10;
        }
    }
    
    static double getMaterialCost(MaterialType material) {
        switch (material) {
            case MaterialType::Gas:    return 120.0;
            case MaterialType::Liquid: return 100.0;
            case MaterialType::Solid:  return 180.0;
            case MaterialType::Sludge: return 220.0;
            default: return 100.0;
        }
    }
    
    static std::string getMaterialCode(MaterialType material) {
        switch (material) {
            case MaterialType::Liquid: return "L";
            case MaterialType::Gas:    return "G";
            case MaterialType::Solid:  return "S";
            case MaterialType::Sludge: return "SL";
            default: return "L";
        }
    }
    
    static std::string getServiceCode(ServiceType service) {
        switch (service) {
            case ServiceType::Water:   return "W";
            case ServiceType::Air:     return "AIR";
            case ServiceType::Slurry:  return "SLURRY";
            case ServiceType::Chemical: return "CHEM";
            default: return "W";
        }
    }
    
    static void computeLength(Pipe& pipe) {
        pipe.length = 0.0;
        for (size_t i = 0; i + 1 < pipe.path.size(); ++i) {
            pipe.length += pipe.path[i].distance(pipe.path[i+1]);
        }
    }
    
    static void computeCost(Pipe& pipe) {
        double materialCost = getMaterialCost(pipe.material);
        pipe.cost = pipe.length * materialCost * pipe.diameter;
    }
    
    static void generateTag(Pipe& pipe) {
        int sizeInMm = static_cast<int>(pipe.diameter * 1000.0);
        std::string materialCode = getMaterialCode(pipe.material);
        std::string serviceCode = getServiceCode(pipe.service);
        
        pipe.tag = std::to_string(pipe.lineNumber) + "-" +
                  std::to_string(sizeInMm) + "-" +
                  materialCode + "-" +
                  serviceCode + "-" +
                  pipe.specCode;
    }
};

}  // namespace ModulusLite
