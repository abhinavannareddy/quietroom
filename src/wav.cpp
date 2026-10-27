#include "quietroom/wav.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>

namespace quietroom {

namespace {

std::uint16_t le16(const unsigned char* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
std::uint32_t le32(const unsigned char* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
void put16(std::vector<unsigned char>& b, std::uint16_t v) {
    b.push_back(static_cast<unsigned char>(v & 0xFF));
    b.push_back(static_cast<unsigned char>(v >> 8));
}
void put32(std::vector<unsigned char>& b, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<unsigned char>((v >> (8 * i)) & 0xFF));
}
void put_tag(std::vector<unsigned char>& b, const char* tag) {
    b.insert(b.end(), tag, tag + 4);
}

constexpr std::uint16_t kFormatPcm = 1;
constexpr std::uint16_t kFormatFloat = 3;
constexpr std::uint16_t kFormatExtensible = 0xFFFE;

}  // namespace

bool read_wav(const std::string& path, WavData& out, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot open " + path;
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff file_size = file.tellg();
    if (file_size < 0 || static_cast<std::uint64_t>(file_size) > kMaxWavBytes) {
        error = path + " is not a regular file or is larger than " +
                std::to_string(kMaxWavBytes >> 20) + " MiB";
        return false;
    }
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buf(static_cast<std::size_t>(file_size));
    file.read(reinterpret_cast<char*>(buf.data()), file_size);
    if (!file) {
        error = "could not read all of " + path;
        return false;
    }
    if (buf.size() < 12 || std::memcmp(buf.data(), "RIFF", 4) != 0 ||
        std::memcmp(buf.data() + 8, "WAVE", 4) != 0) {
        error = path + " is not a RIFF/WAVE file";
        return false;
    }

    std::uint16_t format = 0, channels = 0, bits = 0;
    std::uint32_t rate = 0;
    bool have_fmt = false;
    std::size_t data_pos = 0, data_len = 0;
    bool have_data = false;

    std::size_t pos = 12;
    while (pos + 8 <= buf.size()) {
        const unsigned char* chunk = buf.data() + pos;
        const std::size_t size = le32(chunk + 4);
        const std::size_t body = pos + 8;
        const std::size_t available = buf.size() - body;
        if (std::memcmp(chunk, "fmt ", 4) == 0) {
            if (size < 16 || available < 16) {
                error = "truncated fmt chunk";
                return false;
            }
            format = le16(buf.data() + body);
            channels = le16(buf.data() + body + 2);
            rate = le32(buf.data() + body + 4);
            bits = le16(buf.data() + body + 14);
            if (format == kFormatExtensible) {
                // The real format is the first two bytes of the sub-format GUID (offset 24).
                if (size < 26 || available < 26) {
                    error = "truncated WAVE_FORMAT_EXTENSIBLE fmt chunk";
                    return false;
                }
                format = le16(buf.data() + body + 24);
            }
            have_fmt = true;
        } else if (std::memcmp(chunk, "data", 4) == 0) {
            data_pos = body;
            data_len = std::min(size, available);  // tolerate a size that overstates the file
            have_data = true;
            break;
        }
        if (size > available) break;  // chunk claims more than the file holds: stop walking
        pos = body + size + (size & 1);
    }

    if (!have_fmt || !have_data) {
        error = "missing fmt or data chunk";
        return false;
    }
    if (channels != 1) {
        error = "only mono input is supported (got " + std::to_string(channels) + " channels)";
        return false;
    }
    if (rate != kSampleRate) {
        error = "sample rate must be 16000 Hz (got " + std::to_string(rate) + ")";
        return false;
    }
    const bool pcm16 = format == kFormatPcm && bits == 16;
    const bool float32 = format == kFormatFloat && bits == 32;
    if (!pcm16 && !float32) {
        error = "unsupported format: need 16-bit PCM or 32-bit float";
        return false;
    }

    out.sample_rate = rate;
    out.samples.clear();
    const unsigned char* data = buf.data() + data_pos;
    if (pcm16) {
        const std::size_t n = data_len / 2;
        out.samples.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            out.samples[i] = static_cast<float>(static_cast<std::int16_t>(le16(data + 2 * i))) / 32768.0f;
        }
    } else {
        const std::size_t n = data_len / 4;
        out.samples.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            const std::uint32_t raw = le32(data + 4 * i);
            float value;
            std::memcpy(&value, &raw, sizeof(float));
            // NaN/Inf would poison the recursive noise and energy estimates for the whole file.
            out.samples[i] = std::isfinite(value) ? std::min(1.0f, std::max(-1.0f, value)) : 0.0f;
        }
    }
    return true;
}

bool write_wav(const std::string& path, const std::vector<float>& samples, std::uint32_t sample_rate,
               std::string& error) {
    const std::uint32_t data_bytes = static_cast<std::uint32_t>(samples.size() * 2);
    std::vector<unsigned char> b;
    b.reserve(44 + data_bytes);
    put_tag(b, "RIFF");
    put32(b, 36 + data_bytes);
    put_tag(b, "WAVE");
    put_tag(b, "fmt ");
    put32(b, 16);
    put16(b, kFormatPcm);
    put16(b, 1);
    put32(b, sample_rate);
    put32(b, sample_rate * 2);
    put16(b, 2);
    put16(b, 16);
    put_tag(b, "data");
    put32(b, data_bytes);
    for (float s : samples) {
        // Same 32768 scale the reader divides by; clamp so +1.0 maps to 32767, not a wrap.
        const long scaled = std::lround(std::max(-1.0f, std::min(1.0f, s)) * 32768.0f);
        const long v = std::max(-32768L, std::min(32767L, scaled));
        put16(b, static_cast<std::uint16_t>(static_cast<std::int16_t>(v)));
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        error = "cannot create " + path;
        return false;
    }
    file.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
    if (!file) {
        error = "write failed for " + path;
        return false;
    }
    return true;
}

}  // namespace quietroom
