#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace cureum
{

// Small expression language for generating wavetables, e.g. "sin(pi*x*(1+y*7))".
// Variables: x (-1..1 in the cycle), w (0..1 in the cycle), y (0..1 frame position), n, N, pi, e.
class Formula
{
public:
    bool compile (const juce::String& text, juce::String& error);
    double evaluate (double x, double w, double y, double n, double N) const noexcept;

    // Renders numFrames * 2048 samples.
    std::vector<float> render (int numFrames) const;

private:
    enum class Op { num, var, neg, add, sub, mul, div, mod, pow, lt, gt, le, ge, eq, ne, call };
    struct Node { Op op; double value = 0.0; int var = 0, fn = 0, a = -1, b = -1, c = -1; };
    int parseExpr();
    int parseCompare();
    int parseAdd();
    int parseMul();
    int parsePow();
    int parseUnary();
    int parsePrimary();
    void skipSpace();
    bool match (const char* token);
    int add (Node n) { nodes.push_back (n); return (int) nodes.size() - 1; }
    double eval (int index, const double* vars) const noexcept;

    std::vector<Node> nodes;
    int root = -1;
    juce::String src;
    int pos = 0;
    juce::String err;
};

} // namespace cureum
