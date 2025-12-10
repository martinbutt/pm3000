// RNC decompression (embedded from external/swos_extract).
#include "rnc_decompress.hpp"

#include <cstring>
#include <optional>
#include <vector>

namespace {

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;

struct Huftable {
    u32 l1;
    u16 l2;
    u32 l3;
    u16 bit_depth;
};

struct State {
    const u8 *input = nullptr;
    size_t file_size = 0;
    size_t input_offset = 0;
    size_t output_offset = 0;

    u32 method = 0;
    u32 input_size = 0;
    u32 packed_size = 0;
    u16 unpacked_crc = 0;
    u16 packed_crc = 0;
    u16 unpacked_crc_real = 0;
    u16 enc_key = 0;

    u32 processed_size = 0;
    u16 bit_count = 0;
    u32 bit_buffer = 0;

    u32 dict_size = 0x8000; // defaults to method 1 window
    u16 match_count = 0;
    u16 match_offset = 0;

    std::vector<u8> output;
    std::vector<u8> mem1;
    std::vector<u8> decoded;
    u8 *pack_block_start = nullptr;
    u8 *window = nullptr;

    Huftable raw_table[16]{};
    Huftable pos_table[16]{};
    Huftable len_table[16]{};

    bool packed_crc_ok = true;
    bool unpacked_crc_ok = true;
};

constexpr u32 kRncSign = 0x524E43;      // "RNC"
constexpr size_t kRncHeaderSize = 0x12; // 18 bytes
constexpr size_t kBufferSize = 0x10000; // 65536 bytes

static const u16 crc_table[] = {
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
        0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
        0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
        0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
        0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
        0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
        0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
        0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
        0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
        0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
        0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
        0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
        0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
        0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
        0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
        0xBE01, 0x7EC1, 0x7F81, 0x7D40, 0xBF01, 0x7FC0, 0x7E80, 0xBE41,
        0xB601, 0x76C1, 0x7781, 0x7740, 0xB401, 0x74C1, 0x7581, 0x7540,
        0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C1, 0x7081, 0x7040,
        0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
        0x9601, 0x56C1, 0x5781, 0x5740, 0x9501, 0x55C1, 0x5481, 0x5440,
        0x9C01, 0x5CC1, 0x5D81, 0x5D40, 0x9D01, 0x5FC1, 0x5E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C1, 0x5881, 0x5840,
        0x8801, 0x48C1, 0x4981, 0x4940, 0x8B01, 0x4BC1, 0x4A81, 0x4A40,
        0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC1, 0x4C81, 0x4C40,
        0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C1, 0x4681, 0x4640,
        0x8201, 0x42C1, 0x4381, 0x4340, 0x8101, 0x41C1, 0x4081, 0x4040};

u8 read_byte(const u8 *buf, size_t &offset) { return buf[offset++]; }

u16 read_word_be(const u8 *buf, size_t &offset) {
    u8 b1 = read_byte(buf, offset);
    u8 b2 = read_byte(buf, offset);
    return static_cast<u16>((b1 << 8) | b2);
}

u32 read_dword_be(const u8 *buf, size_t &offset) {
    u16 w1 = read_word_be(buf, offset);
    u16 w2 = read_word_be(buf, offset);
    return (static_cast<u32>(w1) << 16) | w2;
}

u16 crc_block(const u8 *buf, size_t offset, size_t size) {
    u16 crc = 0;
    while (size--) {
        crc ^= buf[offset++];
        crc = static_cast<u16>((crc >> 8) ^ crc_table[crc & 0xFF]);
    }
    return crc;
}

u32 inverse_bits(u32 value, int count) {
    u32 out = 0;
    while (count--) {
        out = (out << 1) | (value & 1);
        value >>= 1;
    }
    return out;
}

void proc_20(Huftable *data, int count) {
    int val = 0;
    u32 div = 0x80000000;
    int bits_count = 1;

    while (bits_count <= 16) {
        int i = 0;
        while (true) {
            if (i >= count) {
                bits_count++;
                div >>= 1;
                break;
            }
            if (data[i].bit_depth == bits_count) {
                data[i].l3 = inverse_bits(static_cast<u32>(val / div), bits_count);
                val += static_cast<int>(div);
            }
            i++;
        }
    }
}

u8 read_source_byte(State &v) {
    if (v.pack_block_start == &v.mem1[0xFFFD]) {
        size_t left_size = v.file_size - v.input_offset;
        size_t size_to_read = (left_size <= 0xFFFD) ? left_size : 0xFFFD;

        v.pack_block_start = v.mem1.data();
        if (size_to_read) {
            std::memcpy(v.pack_block_start, v.input + v.input_offset, size_to_read);
            v.input_offset += size_to_read;
        }

        size_t tail = 0;
        if (left_size > size_to_read) {
            tail = (left_size - size_to_read > 2) ? 2 : (left_size - size_to_read);
            std::memcpy(&v.mem1[size_to_read], v.input + v.input_offset, tail);
            v.input_offset += tail;
        }

        v.input_offset -= tail;
    }

    return *v.pack_block_start++;
}

u32 input_bits_m2(State &v, short count) {
    u32 bits = 0;
    while (count--) {
        if (!v.bit_count) {
            v.bit_buffer = read_source_byte(v);
            v.bit_count = 8;
        }

        bits <<= 1;
        if (v.bit_buffer & 0x80) {
            bits |= 1;
        }
        v.bit_buffer <<= 1;
        v.bit_count--;
    }

    return bits;
}

int build_dec_table(State &v, Huftable *table) {
    int i = 0;
    while (true) {
        u32 bits = input_bits_m2(v, 1);
        if (bits) {
            u32 depth = input_bits_m2(v, 4);
            if (depth == 15) {
                break;
            }
            table[i].bit_depth = static_cast<u16>(depth + 1);
        }
        i++;
    }
    return i;
}

int read_tree(State &v, u16 *table, int cnt) {
    int i = 0;
    while (true) {
        u32 bits = input_bits_m2(v, 1);
        if (bits) {
            table[i] = static_cast<u16>(input_bits_m2(v, 4));
            if (table[i] == 15) {
                break;
            }
        }
        if (++i >= cnt) {
            break;
        }
    }
    return i;
}

void fill_table(State &v, Huftable *dtab, int cnt) {
    for (int i = 0; i < cnt; ++i) {
        dtab[i].l3 = 0;
    }

    proc_20(dtab, cnt);

    u16 tab2[74];
    int cnt2 = read_tree(v, tab2, 74);

    int v0x18 = 0;
    int max_i = cnt2 - 1;
    int index = 0;
    int copy_idx = 0;
    while (index <= max_i && v0x18 < cnt) {
        int shift = 0;
        int counter = tab2[index++];
        if (counter == 15) {
            break;
        }
        if (counter) {
            shift = input_bits_m2(v, counter);
        }
        int copy_bits = 16 - counter;
        int count = 1 << copy_bits;
        for (int i = 0; i < count; ++i) {
            dtab[copy_idx].l3 = static_cast<u32>((copy_idx >> copy_bits) + shift);
            dtab[copy_idx].l2 = static_cast<u16>(copy_bits);
            copy_idx++;
        }
        v0x18 += count;
    }
}

u16 get_code(State &v, Huftable *dtab) {
    int bitcount = 0;
    int lastb = 0;
    u16 val = 0;

    for (int i = 0; i < 16; i++) {
        bitcount++;
        val = static_cast<u16>(input_bits_m2(v, 1) | (val << 1));
        for (int j = lastb; j >= 0; j--) {
            if (dtab[j].bit_depth == bitcount && dtab[j].l3 == val) {
                v.bit_count += static_cast<u16>(16 - (val >> 27));
                if (v.bit_count > 16) {
                    v.bit_count = 16;
                }
                return static_cast<u16>(lastb - j);
            }
        }
        lastb += dtab[lastb + 1].l2;
    }
    return 0;
}

bool decode_block(State &v) {
    v.match_offset = 1;
    bool continue_flag = false;

    while (true) {
        if (!v.bit_count) {
            v.bit_buffer = read_source_byte(v);
            v.bit_count = 8;
        }

        int bits = static_cast<int>(v.bit_buffer >> 15);
        v.bit_buffer <<= 1;
        v.bit_count--;

        if (bits) {
            u16 count = get_code(v, v.len_table);
            u16 length;

            if (count == 2) {
                continue_flag = true;
                break;
            }
            length = static_cast<u16>((count != 1) ? count : input_bits_m2(v, 8));
            count = get_code(v, v.pos_table);
            if (count == 2) {
                v.match_offset = static_cast<u16>(input_bits_m2(v, 14));
            } else if (count == 3) {
                v.match_offset = static_cast<u16>(input_bits_m2(v, 15));
            } else if (count == 4) {
                v.match_offset = static_cast<u16>(input_bits_m2(v, 16));
            } else {
                v.match_offset = static_cast<u16>((v.match_offset & 0xFF00) | get_code(v, v.raw_table));
                if (v.match_offset == 0) {
                    return false;
                }
            }
            length += 2;
            v.match_count = length;

            int pos = static_cast<int>(v.output_offset - v.match_offset);
            int c = v.match_count + pos - 2;
            if (pos < 0) {
                pos += static_cast<int>(v.dict_size);
            }
            for (int i = pos; i <= c; ++i) {
                u8 b = v.window[i & (v.dict_size - 1)];
                v.output.push_back(b);
                v.window[v.output_offset++ & (v.dict_size - 1)] = b;
            }
        } else {
            u16 count = get_code(v, v.raw_table);
            if (!count) {
                return false;
            }
            if (count == 2) {
                break;
            }
            for (int i = 0; i < count; ++i) {
                u8 b = read_source_byte(v);
                v.output.push_back(b);
                v.window[v.output_offset++ & (v.dict_size - 1)] = b;
            }
        }
    }
    return continue_flag;
}

bool proc_pack(State &v) {
    v.output_offset = 0;
    v.bit_count = 0;
    v.bit_buffer = 0;

    int cnt = build_dec_table(v, v.raw_table);
    fill_table(v, v.raw_table, cnt);
    cnt = build_dec_table(v, v.pos_table);
    fill_table(v, v.pos_table, cnt);
    cnt = build_dec_table(v, v.len_table);
    fill_table(v, v.len_table, cnt);

    v.output.reserve(v.input_size);

    bool result = true;
    do {
        result = decode_block(v);
    } while (result);

    return v.output.size() == v.input_size;
}

bool proc_2(State &v) {
    v.window = v.mem1.data();
    v.pack_block_start = &v.mem1[0xFFFD];
    return proc_pack(v);
}

bool proc_1(State &v) {
    if (v.input_size < v.dict_size) {
        v.dict_size = v.input_size;
    }
    v.window = v.output.data();
    v.output.resize(v.dict_size);
    v.output_offset = 0;
    v.bit_buffer = 0;
    v.bit_count = 0;

    int cnt = build_dec_table(v, v.raw_table);
    fill_table(v, v.raw_table, cnt);
    cnt = build_dec_table(v, v.pos_table);
    fill_table(v, v.pos_table, cnt);
    cnt = build_dec_table(v, v.len_table);
    fill_table(v, v.len_table, cnt);

    bool result = true;
    do {
        result = decode_block(v);
    } while (result);

    v.output.resize(v.input_size);
    return v.output.size() == v.input_size;
}

bool parse_header(State &v) {
    if (v.file_size < kRncHeaderSize) {
        return false;
    }
    size_t offset = 0;
    if (read_dword_be(v.input, offset) != kRncSign) {
        return false;
    }
    v.method = read_byte(v.input, offset);
    v.input_size = read_dword_be(v.input, offset);
    v.packed_size = read_dword_be(v.input, offset);
    v.unpacked_crc = read_word_be(v.input, offset);
    v.packed_crc = read_word_be(v.input, offset);
    v.unpacked_crc_real = read_word_be(v.input, offset);
    v.enc_key = read_word_be(v.input, offset);
    return true;
}

} // namespace

namespace rnc {

bool decompress(const std::vector<uint8_t> &input, std::vector<uint8_t> &output, std::string &error) {
    State v{};
    v.input = input.data();
    v.file_size = input.size();

    if (!parse_header(v)) {
        error = "Invalid RNC header";
        return false;
    }

    v.mem1.resize(kBufferSize + 2 + kRncHeaderSize);
    v.mem1[kBufferSize] = static_cast<u8>((v.enc_key >> 8) & 0xFF);
    v.mem1[kBufferSize + 1] = static_cast<u8>(v.enc_key & 0xFF);
    v.pack_block_start = &v.mem1[0xFFFD];

    v.output.reserve(v.input_size);
    v.decoded.resize(v.packed_size + kRncHeaderSize);

    size_t to_copy = std::min<size_t>(v.packed_size, input.size() - kRncHeaderSize);
    std::memcpy(v.decoded.data(), input.data() + kRncHeaderSize, to_copy);

    v.input = v.decoded.data();
    v.input_offset = 0;
    v.file_size = v.decoded.size();

    v.dict_size = 0x8000;
    v.window = v.mem1.data();
    v.pack_block_start = &v.mem1[0xFFFD];

    bool ok = false;
    if (v.method == 1) {
        ok = proc_1(v);
    } else if (v.method == 2) {
        ok = proc_2(v);
    } else {
        error = "Unsupported RNC method";
        return false;
    }

    if (!ok) {
        error = "RNC decode failed";
        return false;
    }

    v.unpacked_crc_ok = crc_block(v.output.data(), 0, v.output.size()) == v.unpacked_crc;
    if (!v.unpacked_crc_ok) {
        error = "CRC mismatch after decode";
        return false;
    }

    output.swap(v.output);
    return true;
}

} // namespace rnc
