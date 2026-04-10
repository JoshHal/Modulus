#include "FDGLayout.h"
#include <cmath>
#include <algorithm>

// ─── helpers ─────────────────────────────────────────────────────────────────

static double safe_dist(double dx, double dy) {
    double d = std::sqrt(dx*dx + dy*dy);
    return (d < 1e-6) ? 1e-6 : d;
}

// ─── public interface ─────────────────────────────────────────────────────────

void FDGLayout::run(
    std::vector<Machine>& machines,
    const std::vector<Pipe>& pipes,
    double workspaceW,
    double workspaceH,
    int    iterations)
{
    if (machines.empty()) return;

    initialGridPlacement(machines, workspaceW, workspaceH);

    const int    N = static_cast<int>(machines.size());
    const double area = workspaceW * workspaceH;
    const double k    = std::sqrt(area / static_cast<double>(N));   // ideal spring length

    // Velocity vectors (x, y only)
    std::vector<double> vx(N, 0.0), vy(N, 0.0);

    // Simulated-annealing temperature schedule: starts at workspaceW/10,
    // reduces linearly to 0 over all iterations.
    double temp = workspaceW / 10.0;
    const double tempStep = temp / static_cast<double>(iterations);

    for (int iter = 0; iter < iterations; ++iter) {

        // ── repulsion: all pairs ────────────────────────────────────────────
        std::vector<double> dispX(N, 0.0), dispY(N, 0.0);

        for (int i = 0; i < N; ++i) {
            for (int j = i + 1; j < N; ++j) {
                double dx = machines[i].position.x - machines[j].position.x;
                double dy = machines[i].position.y - machines[j].position.y;
                double d  = safe_dist(dx, dy);
                double fr = (k * k) / d;   // Fruchterman-Reingold repulsion
                dispX[i] += (dx / d) * fr;
                dispY[i] += (dy / d) * fr;
                dispX[j] -= (dx / d) * fr;
                dispY[j] -= (dy / d) * fr;
            }
        }

        // ── attraction: pipe edges ──────────────────────────────────────────
        for (const auto& p : pipes) {
            int a = -1, b = -1;
            for (int i = 0; i < N; ++i) {
                if (machines[i].id == p.fromMachineId) a = i;
                if (machines[i].id == p.toMachineId)   b = i;
            }
            if (a < 0 || b < 0) continue;

            double dx = machines[a].position.x - machines[b].position.x;
            double dy = machines[a].position.y - machines[b].position.y;
            double d  = safe_dist(dx, dy);
            double fa = (d * d) / k;   // Fruchterman-Reingold attraction
            dispX[a] -= (dx / d) * fa;
            dispY[a] -= (dy / d) * fa;
            dispX[b] += (dx / d) * fa;
            dispY[b] += (dy / d) * fa;
        }

        // ── apply displacement (capped by temperature) ──────────────────────
        for (int i = 0; i < N; ++i) {
            double len = safe_dist(dispX[i], dispY[i]);
            double scale = std::min(len, temp) / len;
            machines[i].position.x += dispX[i] * scale;
            machines[i].position.y += dispY[i] * scale;
            clamp(machines[i], workspaceW, workspaceH);
        }

        temp -= tempStep;
    }
}

// ─── private ──────────────────────────────────────────────────────────────────

void FDGLayout::initialGridPlacement(
    std::vector<Machine>& machines,
    double workspaceW, double workspaceH)
{
    const int N = static_cast<int>(machines.size());
    // Square-ish grid
    int cols = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(N))));
    int rows = (N + cols - 1) / cols;

    double cellW = workspaceW / static_cast<double>(cols + 1);
    double cellH = workspaceH / static_cast<double>(rows + 1);

    for (int i = 0; i < N; ++i) {
        int col = i % cols;
        int row = i / cols;
        machines[i].position.x = cellW * (col + 1);
        machines[i].position.y = cellH * (row + 1);
        machines[i].position.z = 0.0;
    }
}

void FDGLayout::clamp(Machine& m, double workspaceW, double workspaceH) {
    double halfW = m.size.x / 2.0;
    double halfH = m.size.y / 2.0;
    m.position.x = std::max(halfW, std::min(workspaceW - halfW, m.position.x));
    m.position.y = std::max(halfH, std::min(workspaceH - halfH, m.position.y));
}