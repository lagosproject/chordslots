// ChordSlots core - exact sampling of a loop of discrete variables.
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace cs
{
namespace detail
{

// Sum-product elimination on the ring. Tied AA' slots share a variable, turning
// intervals between tied slots into parallel paths. Eliminating degree-two
// vertices combines each path into one edge and leaves a ring or a path.
class RingInference
{
public:
    using Weights = std::vector<double>;

    explicit RingInference (std::vector<Weights> unaryWeights)
        : unary (std::move (unaryWeights)), edges (unary.size(), std::vector<Weights> (unary.size()))
    {
    }

    size_t size (int node) const { return unary[(size_t) node].size(); }

    // Matrices arrive row-major, from first to second. Store each edge once.
    void addEdge (int first, int second, const Weights& weights)
    {
        if (first == second)
        {
            for (size_t s = 0; s < size (first); ++s)
                unary[(size_t) first][s] *= weights[s * size (first) + s];
            return;
        }

        auto& edge = edges[(size_t) std::min (first, second)][(size_t) std::max (first, second)];
        if (edge.empty())
            edge.resize (weights.size(), 1.0);
        for (size_t a = 0; a < size (first); ++a)
            for (size_t b = 0; b < size (second); ++b)
                edge[first < second ? a * size (second) + b : b * size (first) + a]
                    *= weights[a * size (second) + b];
        normalize (edge);
    }

    template <typename Draw>
    std::vector<int> sample (Draw&& draw) const
    {
        auto work = *this;
        std::vector<Eliminated> trace;
        std::vector<int> active (unary.size());
        for (size_t i = 0; i < active.size(); ++i)
            active[i] = (int) i;

        while (! active.empty())
        {
            size_t smallestWork = static_cast<size_t> (-1);
            int pivot = -1;
            std::vector<int> adjacent;
            for (int node : active)
            {
                auto neighbours = work.neighboursOf (node);
                if (neighbours.size() > 2 && size (node) != 1)
                    continue;
                size_t effort = size (node);
                for (int other : neighbours)
                    effort *= size (other);
                if (effort < smallestWork)
                {
                    pivot = node;
                    adjacent = std::move (neighbours);
                    smallestWork = effort;
                }
            }
            if (pivot < 0)
                return {}; // Only the ring and contracted AA' ring are supported.

            auto& step = trace.emplace_back();
            step.node = pivot;
            step.unary = work.unary[(size_t) pivot];
            for (int other : adjacent)
            {
                step.factors.push_back ({ other, work.orientedEdge (pivot, other) });
                work.edges[(size_t) std::min (pivot, other)][(size_t) std::max (pivot, other)].clear();
            }

            if (size (pivot) == 1 || adjacent.size() == 1)
            {
                for (auto& factor : step.factors)
                {
                    auto& target = work.unary[(size_t) factor.other];
                    for (size_t b = 0; b < target.size(); ++b)
                    {
                        double sum = 0;
                        for (size_t a = 0; a < step.unary.size(); ++a)
                            sum += step.unary[a] * factor.weights[a * target.size() + b];
                        target[b] *= sum;
                    }
                    normalize (target);
                }
            }
            else if (adjacent.size() == 2)
            {
                const auto& left = step.factors[0];
                const auto& right = step.factors[1];
                const size_t nl = size (left.other), nr = size (right.other);
                Weights message (nl * nr, 0.0);
                for (size_t a = 0; a < step.unary.size(); ++a)
                {
                    const double* rightRow = right.weights.data() + a * nr;
                    for (size_t l = 0; l < nl; ++l)
                    {
                        const double w = step.unary[a] * left.weights[a * nl + l];
                        double* row = message.data() + l * nr;
                        for (size_t r = 0; r < nr; ++r)
                            row[r] += w * rightRow[r];
                    }
                }
                normalize (message);
                work.addEdge (left.other, right.other, message);
            }
            active.erase (std::find (active.begin(), active.end(), pivot));
        }

        std::vector<int> states (unary.size(), 0);
        for (auto it = trace.rbegin(); it != trace.rend(); ++it)
        {
            if (it->unary.size() == 1)
                continue;
            for (const auto& factor : it->factors)
                applyConditional (it->unary, factor, states);
            states[(size_t) it->node] = pick (it->unary, draw());
        }
        return states;
    }

    Weights conditional (int node, const std::vector<int>& states) const
    {
        auto probabilities = unary[(size_t) node];
        for (int other : neighboursOf (node))
            applyConditional (probabilities, { other, orientedEdge (node, other) }, states);
        return probabilities;
    }

    static int pick (const Weights& weights, double u)
    {
        double total = 0.0;
        for (double weight : weights)
            total += weight;
        double threshold = u * total;
        int last = 0;
        for (size_t i = 0; i < weights.size(); ++i)
        {
            if (weights[i] <= 0)
                continue;
            last = (int) i;
            threshold -= weights[i];
            if (threshold < 0)
                return last;
        }
        return last;
    }

private:
    struct Factor
    {
        int other;
        Weights weights;
    };
    struct Eliminated
    {
        int node;
        Weights unary;
        std::vector<Factor> factors;
    };

    std::vector<Weights> unary;
    std::vector<std::vector<Weights>> edges;

    // Dropping a common log offset after each message keeps the sum-product
    // multiplication in a numerically useful range. Offsets cancel in sampling.
    static void normalize (Weights& weights)
    {
        const double peak = *std::max_element (weights.begin(), weights.end());
        if (peak > 0)
            for (double& weight : weights)
                weight /= peak;
    }

    std::vector<int> neighboursOf (int node) const
    {
        std::vector<int> result;
        for (int other = 0; other < (int) unary.size(); ++other)
            if (node != other && ! edgeBetween (node, other).empty())
                result.push_back (other);
        return result;
    }

    const Weights& edgeBetween (int a, int b) const
    {
        return edges[(size_t) std::min (a, b)][(size_t) std::max (a, b)];
    }

    Weights orientedEdge (int from, int to) const
    {
        const auto& edge = edgeBetween (from, to);
        if (from < to)
            return edge;
        Weights transposed (edge.size());
        for (size_t i = 0; i < size (from); ++i)
            for (size_t j = 0; j < size (to); ++j)
                transposed[i * size (to) + j] = edge[j * size (from) + i];
        return transposed;
    }

    void applyConditional (Weights& weights, const Factor& factor, const std::vector<int>& states) const
    {
        const size_t otherState = (size_t) states[(size_t) factor.other];
        for (size_t s = 0; s < weights.size(); ++s)
            weights[s] *= factor.weights[s * size (factor.other) + otherState];
        normalize (weights);
    }
};

inline RingInference::Weights boltzmannWeights (const std::vector<float>& costs, float temperature)
{
    const double minimum = *std::min_element (costs.begin(), costs.end());
    RingInference::Weights weights;
    weights.reserve (costs.size());
    for (float cost : costs)
        weights.push_back (std::exp ((minimum - cost) / temperature));
    return weights;
}

} // namespace detail
} // namespace cs
