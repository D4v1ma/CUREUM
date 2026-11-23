#include "Formula.h"
#include <cmath>

namespace cureum
{

namespace
{
    const char* const kFunctions[] = { "sin", "cos", "tan", "asin", "acos", "atan", "sinh", "cosh", "tanh", "abs", "sqrt", "exp", "log",
                                       "log10", "floor", "ceil", "round", "fract", "sign", "saw", "square", "tri", "rand", "noise",
                                       "pow", "min", "max", "step", "atan2", "clamp", "mix", "lerp" };
    constexpr int kNumFunctions = (int) (sizeof (kFunctions) / sizeof (kFunctions[0]));
    int arity (int fn) { return fn < 24 ? 1 : (fn < 29 ? 2 : 3); }

    double hash (double v)
    {
        auto bits = (juce::uint64) (juce::int64) std::floor (v * 1000.0 + 0.5) * 6364136223846793005ULL + 1442695040888963407ULL;
        bits ^= bits >> 33; bits *= 0xff51afd7ed558ccdULL; bits ^= bits >> 33;
        return (double) (bits >> 11) / 9007199254740992.0;
    }
    double fract (double v) { return v - std::floor (v); }
}

void Formula::skipSpace()
{
    while (pos < src.length() && juce::CharacterFunctions::isWhitespace (src[pos])) ++pos;
}

bool Formula::match (const char* token)
{
    skipSpace();
    const juce::String t (token);
    if (src.substring (pos, pos + t.length()) == t) { pos += t.length(); return true; }
    return false;
}

bool Formula::compile (const juce::String& text, juce::String& error)
{
    nodes.clear();
    src = text;
    pos = 0;
    err.clear();
    root = parseExpr();
    skipSpace();
    if (err.isEmpty() && pos < src.length()) err = "Unexpected '" + src.substring (pos, pos + 1) + "' at position " + juce::String (pos + 1);
    error = err;
    if (err.isNotEmpty()) { root = -1; return false; }
    return true;
}

int Formula::parseExpr() { return parseCompare(); }

int Formula::parseCompare()
{
    int left = parseAdd();
    while (err.isEmpty())
    {
        Op op;
        if (match ("<=")) op = Op::le;
        else if (match (">=")) op = Op::ge;
        else if (match ("==")) op = Op::eq;
        else if (match ("!=")) op = Op::ne;
        else if (match ("<")) op = Op::lt;
        else if (match (">")) op = Op::gt;
        else break;
        const int right = parseAdd();
        left = add ({ op, 0.0, 0, 0, left, right });
    }
    return left;
}

int Formula::parseAdd()
{
    int left = parseMul();
    while (err.isEmpty())
    {
        if (match ("+")) left = add ({ Op::add, 0.0, 0, 0, left, parseMul() });
        else if (match ("-")) left = add ({ Op::sub, 0.0, 0, 0, left, parseMul() });
        else break;
    }
    return left;
}

int Formula::parseMul()
{
    int left = parsePow();
    while (err.isEmpty())
    {
        if (match ("*")) left = add ({ Op::mul, 0.0, 0, 0, left, parsePow() });
        else if (match ("/")) left = add ({ Op::div, 0.0, 0, 0, left, parsePow() });
        else if (match ("%")) left = add ({ Op::mod, 0.0, 0, 0, left, parsePow() });
        else break;
    }
    return left;
}

int Formula::parsePow()
{
    const int base = parseUnary();
    if (match ("^"))
        return add ({ Op::pow, 0.0, 0, 0, base, parsePow() });
    return base;
}

int Formula::parseUnary()
{
    if (match ("-")) return add ({ Op::neg, 0.0, 0, 0, parseUnary() });
    if (match ("+")) return parseUnary();
    return parsePrimary();
}

int Formula::parsePrimary()
{
    skipSpace();
    if (pos >= src.length()) { err = "Unexpected end of formula"; return add ({ Op::num }); }
    if (match ("("))
    {
        const int e = parseExpr();
        if (! match (")")) err = "Missing ')'";
        return e;
    }
    const auto c = src[pos];
    if (juce::CharacterFunctions::isDigit (c) || c == '.')
    {
        int end = pos;
        while (end < src.length() && (juce::CharacterFunctions::isDigit (src[end]) || src[end] == '.')) ++end;
        if (end < src.length() && src[end] == 'e' && end + 1 < src.length() && (juce::CharacterFunctions::isDigit (src[end + 1]) || src[end + 1] == '-'))
        {
            end += 2;
            while (end < src.length() && juce::CharacterFunctions::isDigit (src[end])) ++end;
        }
        const double v = src.substring (pos, end).getDoubleValue();
        pos = end;
        return add ({ Op::num, v });
    }
    if (juce::CharacterFunctions::isLetter (c))
    {
        int end = pos;
        while (end < src.length() && (juce::CharacterFunctions::isLetterOrDigit (src[end]) || src[end] == '_')) ++end;
        const auto rawName = src.substring (pos, end);
        pos = end;
        if (rawName == "N") return add ({ Op::var, 0.0, 4 });   // number of frames (case-sensitive)
        const auto name = rawName.toLowerCase();
        if (name == "pi") return add ({ Op::num, juce::MathConstants<double>::pi });
        if (name == "e")  return add ({ Op::num, juce::MathConstants<double>::euler });
        if (name == "x") return add ({ Op::var, 0.0, 0 });
        if (name == "w") return add ({ Op::var, 0.0, 1 });
        if (name == "y") return add ({ Op::var, 0.0, 2 });
        if (name == "n") return add ({ Op::var, 0.0, 3 });
        if (name == "frames") return add ({ Op::var, 0.0, 4 });
        for (int f = 0; f < kNumFunctions; ++f)
        {
            if (name != kFunctions[f]) continue;
            if (! match ("(")) { err = "Expected '(' after " + name; return add ({ Op::num }); }
            Node n { Op::call };
            n.fn = f;
            n.a = parseExpr();
            if (arity (f) >= 2) { if (! match (",")) err = name + " needs " + juce::String (arity (f)) + " arguments"; n.b = parseExpr(); }
            if (arity (f) >= 3) { if (! match (",")) err = name + " needs 3 arguments"; n.c = parseExpr(); }
            if (! match (")")) err = "Missing ')' after " + name;
            return add (n);
        }
        err = "Unknown name '" + name + "'";
        return add ({ Op::num });
    }
    err = "Unexpected '" + juce::String::charToString (c) + "'";
    return add ({ Op::num });
}

double Formula::eval (int i, const double* v) const noexcept
{
    const auto& n = nodes[(size_t) i];
    switch (n.op)
    {
        case Op::num: return n.value;
        case Op::var: return v[n.var];
        case Op::neg: return -eval (n.a, v);
        case Op::add: return eval (n.a, v) + eval (n.b, v);
        case Op::sub: return eval (n.a, v) - eval (n.b, v);
        case Op::mul: return eval (n.a, v) * eval (n.b, v);
        case Op::div: { const double d = eval (n.b, v); return std::abs (d) < 1e-12 ? 0.0 : eval (n.a, v) / d; }
        case Op::mod: { const double d = eval (n.b, v); return std::abs (d) < 1e-12 ? 0.0 : std::fmod (eval (n.a, v), d); }
        case Op::pow: return std::pow (eval (n.a, v), eval (n.b, v));
        case Op::lt:  return eval (n.a, v) <  eval (n.b, v) ? 1.0 : 0.0;
        case Op::gt:  return eval (n.a, v) >  eval (n.b, v) ? 1.0 : 0.0;
        case Op::le:  return eval (n.a, v) <= eval (n.b, v) ? 1.0 : 0.0;
        case Op::ge:  return eval (n.a, v) >= eval (n.b, v) ? 1.0 : 0.0;
        case Op::eq:  return std::abs (eval (n.a, v) - eval (n.b, v)) < 1e-9 ? 1.0 : 0.0;
        case Op::ne:  return std::abs (eval (n.a, v) - eval (n.b, v)) >= 1e-9 ? 1.0 : 0.0;
        case Op::call:
        {
            const double a = eval (n.a, v);
            switch (n.fn)
            {
                case 0: return std::sin (a);   case 1: return std::cos (a);   case 2: return std::tan (a);
                case 3: return std::asin (juce::jlimit (-1.0, 1.0, a)); case 4: return std::acos (juce::jlimit (-1.0, 1.0, a));
                case 5: return std::atan (a);  case 6: return std::sinh (a);  case 7: return std::cosh (a); case 8: return std::tanh (a);
                case 9: return std::abs (a);   case 10: return std::sqrt (std::max (0.0, a)); case 11: return std::exp (a);
                case 12: return a > 0 ? std::log (a) : 0.0; case 13: return a > 0 ? std::log10 (a) : 0.0;
                case 14: return std::floor (a); case 15: return std::ceil (a); case 16: return std::round (a);
                case 17: return fract (a);      case 18: return a > 0 ? 1.0 : (a < 0 ? -1.0 : 0.0);
                case 19: return 2.0 * fract (a + 0.5) - 1.0;                          // saw
                case 20: return fract (a) < 0.5 ? 1.0 : -1.0;                          // square
                case 21: return 1.0 - 4.0 * std::abs (fract (a + 0.25) - 0.5);         // triangle
                case 22: return hash (a);                                              // rand
                case 23: { const double i0 = std::floor (a), t = a - i0; const double s = t * t * (3 - 2 * t); return (hash (i0) + (hash (i0 + 1) - hash (i0)) * s) * 2.0 - 1.0; }
                case 24: return std::pow (a, eval (n.b, v));
                case 25: return std::min (a, eval (n.b, v));
                case 26: return std::max (a, eval (n.b, v));
                case 27: return a >= eval (n.b, v) ? 1.0 : 0.0;                         // step(x, edge)
                case 28: return std::atan2 (a, eval (n.b, v));
                case 29: return juce::jlimit (eval (n.b, v), eval (n.c, v), a);         // clamp(x, lo, hi)
                case 30:
                case 31: { const double b = eval (n.b, v), t = eval (n.c, v); return a + (b - a) * t; }
                default: return 0.0;
            }
        }
    }
    return 0.0;
}

double Formula::evaluate (double x, double w, double y, double n, double N) const noexcept
{
    if (root < 0) return 0.0;
    const double vars[] = { x, w, y, n, N };
    const double r = eval (root, vars);
    return std::isfinite (r) ? r : 0.0;
}

std::vector<float> Formula::render (int numFrames) const
{
    numFrames = juce::jlimit (1, 256, numFrames);
    std::vector<float> out ((size_t) numFrames * 2048);
    for (int f = 0; f < numFrames; ++f)
    {
        const double y = numFrames > 1 ? (double) f / (double) (numFrames - 1) : 0.0;
        for (int i = 0; i < 2048; ++i)
        {
            const double w = (double) i / 2048.0;
            out[(size_t) f * 2048 + (size_t) i] = (float) evaluate (w * 2.0 - 1.0, w, y, (double) f, (double) numFrames);
        }
    }
    return out;
}

} // namespace cureum
