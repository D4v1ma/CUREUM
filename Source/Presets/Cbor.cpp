#include "Cbor.h"

namespace cureum
{

juce::var CborDecoder::decode (const void* data, size_t size, juce::String& error)
{
    CborDecoder d (static_cast<const juce::uint8*> (data), size);
    auto v = d.item (0);
    error = d.err;
    return d.err.isEmpty() ? v : juce::var();
}

bool CborDecoder::readUint (int info, juce::uint64& out)
{
    if (info < 24) { out = (juce::uint64) info; return true; }
    int bytes = 0;
    switch (info)
    {
        case 24: bytes = 1; break;
        case 25: bytes = 2; break;
        case 26: bytes = 4; break;
        case 27: bytes = 8; break;
        default: err = "invalid CBOR length encoding"; return false;
    }
    if (! need ((size_t) bytes)) { err = "truncated CBOR data"; return false; }
    out = 0;
    for (int i = 0; i < bytes; ++i)
        out = (out << 8) | *p++;
    return true;
}

static double halfToDouble (juce::uint16 h)
{
    const int e = (h >> 10) & 0x1f;
    const int m = h & 0x3ff;
    double v;
    if (e == 0)       v = std::ldexp ((double) m, -24);
    else if (e != 31) v = std::ldexp ((double) (m + 1024), e - 25);
    else              v = m == 0 ? std::numeric_limits<double>::infinity() : std::numeric_limits<double>::quiet_NaN();
    return (h & 0x8000) ? -v : v;
}

juce::var CborDecoder::item (int depth)
{
    if (depth > kMaxDepth) { err = "CBOR nesting too deep"; return {}; }
    if (! need (1)) { err = "truncated CBOR data"; return {}; }

    const juce::uint8 initial = *p++;
    const int major = initial >> 5;
    const int info = initial & 0x1f;

    switch (major)
    {
        case 0:
        {
            juce::uint64 v;
            if (! readUint (info, v)) return {};
            return v <= (juce::uint64) std::numeric_limits<juce::int64>::max() ? juce::var ((juce::int64) v) : juce::var ((double) v);
        }
        case 1:
        {
            juce::uint64 v;
            if (! readUint (info, v)) return {};
            return juce::var (-1 - (juce::int64) std::min<juce::uint64> (v, (juce::uint64) std::numeric_limits<juce::int64>::max()));
        }
        case 2:
        case 3:
        {
            juce::MemoryBlock mb;
            auto appendChunk = [&] (int chunkInfo) -> bool
            {
                juce::uint64 len;
                if (! readUint (chunkInfo, len)) return false;
                if (len > (juce::uint64) (end - p)) { err = "truncated CBOR string"; return false; }
                mb.append (p, (size_t) len);
                p += len;
                return true;
            };
            if (info == 31)
            {
                while (true)
                {
                    if (! need (1)) { err = "unterminated CBOR string"; return {}; }
                    if (*p == 0xff) { ++p; break; }
                    const juce::uint8 c = *p++;
                    if ((c >> 5) != major) { err = "bad CBOR string chunk"; return {}; }
                    if (! appendChunk (c & 0x1f)) return {};
                }
            }
            else if (! appendChunk (info))
                return {};
            if (major == 3)
                return juce::String::fromUTF8 (static_cast<const char*> (mb.getData()), (int) mb.getSize());
            return juce::var (mb);
        }
        case 4:
        {
            juce::Array<juce::var> arr;
            if (info == 31)
            {
                while (true)
                {
                    if (! need (1)) { err = "unterminated CBOR array"; return {}; }
                    if (*p == 0xff) { ++p; break; }
                    arr.add (item (depth + 1));
                    if (err.isNotEmpty()) return {};
                }
            }
            else
            {
                juce::uint64 n;
                if (! readUint (info, n)) return {};
                if (n > (juce::uint64) (end - p)) { err = "CBOR array too long"; return {}; }
                for (juce::uint64 i = 0; i < n; ++i)
                {
                    arr.add (item (depth + 1));
                    if (err.isNotEmpty()) return {};
                }
            }
            return arr;
        }
        case 5:
        {
            auto* obj = new juce::DynamicObject();
            juce::var result (obj);
            auto readPair = [&]() -> bool
            {
                auto key = item (depth + 1);
                if (err.isNotEmpty()) return false;
                auto value = item (depth + 1);
                if (err.isNotEmpty()) return false;
                obj->setProperty (juce::Identifier (key.toString().isEmpty() ? juce::String ("_") : key.toString()), value);
                return true;
            };
            if (info == 31)
            {
                while (true)
                {
                    if (! need (1)) { err = "unterminated CBOR map"; return {}; }
                    if (*p == 0xff) { ++p; break; }
                    if (! readPair()) return {};
                }
            }
            else
            {
                juce::uint64 n;
                if (! readUint (info, n)) return {};
                if (n > (juce::uint64) (end - p)) { err = "CBOR map too long"; return {}; }
                for (juce::uint64 i = 0; i < n; ++i)
                    if (! readPair()) return {};
            }
            return result;
        }
        case 6:
        {
            juce::uint64 tag;
            if (! readUint (info, tag)) return {};
            return item (depth + 1);   // tags are ignored; the tagged item is returned as-is
        }
        case 7:
        {
            switch (info)
            {
                case 20: return false;
                case 21: return true;
                case 22:
                case 23: return {};
                case 25:
                {
                    if (! need (2)) { err = "truncated CBOR float"; return {}; }
                    const auto h = (juce::uint16) ((p[0] << 8) | p[1]);
                    p += 2;
                    return halfToDouble (h);
                }
                case 26:
                {
                    if (! need (4)) { err = "truncated CBOR float"; return {}; }
                    const juce::uint32 b = ((juce::uint32) p[0] << 24) | ((juce::uint32) p[1] << 16) | ((juce::uint32) p[2] << 8) | p[3];
                    p += 4;
                    float f;
                    std::memcpy (&f, &b, 4);
                    return (double) f;
                }
                case 27:
                {
                    if (! need (8)) { err = "truncated CBOR double"; return {}; }
                    juce::uint64 b = 0;
                    for (int i = 0; i < 8; ++i) b = (b << 8) | p[i];
                    p += 8;
                    double d;
                    std::memcpy (&d, &b, 8);
                    return d;
                }
                default:
                    if (info < 24) return (int) info;   // unassigned simple value
                    if (info == 24 && need (1)) { ++p; return {}; }
                    err = "unsupported CBOR simple value";
                    return {};
            }
        }
        default: break;
    }
    err = "invalid CBOR";
    return {};
}

} // namespace cureum
