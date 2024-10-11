// Abseil
#include <absl/strings/str_split.h>

// pulse
#include <pulse/sample.h>
#include <pulse/xmalloc.h>
#include <pulse/channelmap.h>

// STD
#include <array>
#include <memory>
#include <bitset>
#include <cstdio>
#include <cstring>
#include <cassert>

namespace {

    #define PA_CHANNEL_POSITION_MASK_LEFT                                       \
        (PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_FRONT_LEFT)               \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_REAR_LEFT)               \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER)    \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_SIDE_LEFT)               \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_FRONT_LEFT)          \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_REAR_LEFT))          \

    #define PA_CHANNEL_POSITION_MASK_RIGHT                                      \
        (PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_FRONT_RIGHT)              \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_REAR_RIGHT)              \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER)   \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_SIDE_RIGHT)              \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_FRONT_RIGHT)         \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_REAR_RIGHT))

    #define PA_CHANNEL_POSITION_MASK_CENTER                                     \
        (PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_FRONT_CENTER)             \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_REAR_CENTER)             \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_CENTER)              \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_FRONT_CENTER)        \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_REAR_CENTER))

    #define PA_CHANNEL_POSITION_MASK_FRONT                                      \
        (PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_FRONT_LEFT)               \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_FRONT_RIGHT)             \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_FRONT_CENTER)            \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER)    \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER)   \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_FRONT_LEFT)          \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_FRONT_RIGHT)         \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_FRONT_CENTER))

    #define PA_CHANNEL_POSITION_MASK_REAR                                       \
        (PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_REAR_LEFT)                \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_REAR_RIGHT)              \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_REAR_CENTER)             \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_REAR_LEFT)           \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_REAR_RIGHT)          \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_REAR_CENTER))

    #define PA_CHANNEL_POSITION_MASK_LFE                                        \
        PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_LFE)

    #define PA_CHANNEL_POSITION_MASK_HFE                                        \
        (PA_CHANNEL_POSITION_MASK_REAR | PA_CHANNEL_POSITION_MASK_FRONT         \
        | PA_CHANNEL_POSITION_MASK_LEFT | PA_CHANNEL_POSITION_MASK_RIGHT        \
        | PA_CHANNEL_POSITION_MASK_CENTER)

    #define PA_CHANNEL_POSITION_MASK_SIDE_OR_TOP_CENTER                         \
        (PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_SIDE_LEFT)                \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_SIDE_RIGHT)              \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_CENTER))

    #define PA_CHANNEL_POSITION_MASK_TOP                                        \
        (PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_CENTER)               \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_FRONT_LEFT)          \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_FRONT_RIGHT)         \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_FRONT_CENTER)        \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_REAR_LEFT)           \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_REAR_RIGHT)          \
        | PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_TOP_REAR_CENTER))

    #define PA_CHANNEL_POSITION_MASK_ALL            \
        ((pa_channel_position_mask_t) (PA_CHANNEL_POSITION_MASK(PA_CHANNEL_POSITION_MAX) - 1))

    template<std::size_t BitsetSize, std::size_t DesignatorsSize>
    bool compareBitsetToIntegers(std::bitset<BitsetSize>& bitset, std::array<std::size_t, DesignatorsSize> designators) {
        std::bitset<BitsetSize> temp;
        for (const auto& arg : designators) {
            temp.set(arg, true);
        }

        return temp == bitset;
    }

    std::unique_ptr<std::string[]> makeTable() {
        auto table = std::make_unique<std::string[]>(PA_CHANNEL_POSITION_MAX);

        table[PA_CHANNEL_POSITION_MONO] = "mono";

        table[PA_CHANNEL_POSITION_FRONT_CENTER] = "front-center";
        table[PA_CHANNEL_POSITION_FRONT_LEFT] = "front-left";
        table[PA_CHANNEL_POSITION_FRONT_RIGHT] = "front-right";

        table[PA_CHANNEL_POSITION_REAR_CENTER] = "rear-center";
        table[PA_CHANNEL_POSITION_REAR_LEFT] = "rear-left";
        table[PA_CHANNEL_POSITION_REAR_RIGHT] = "rear-right";

        table[PA_CHANNEL_POSITION_LFE] = "lfe";

        table[PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER] = "front-left-of-center";
        table[PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER] = "front-right-of-center";

        table[PA_CHANNEL_POSITION_SIDE_LEFT] = "side-left";
        table[PA_CHANNEL_POSITION_SIDE_RIGHT] = "side-right";

        table[PA_CHANNEL_POSITION_AUX0] = "aux0";
        table[PA_CHANNEL_POSITION_AUX1] = "aux1";
        table[PA_CHANNEL_POSITION_AUX2] = "aux2";
        table[PA_CHANNEL_POSITION_AUX3] = "aux3";
        table[PA_CHANNEL_POSITION_AUX4] = "aux4";
        table[PA_CHANNEL_POSITION_AUX5] = "aux5";
        table[PA_CHANNEL_POSITION_AUX6] = "aux6";
        table[PA_CHANNEL_POSITION_AUX7] = "aux7";
        table[PA_CHANNEL_POSITION_AUX8] = "aux8";
        table[PA_CHANNEL_POSITION_AUX9] = "aux9";
        table[PA_CHANNEL_POSITION_AUX10] = "aux10";
        table[PA_CHANNEL_POSITION_AUX11] = "aux11";
        table[PA_CHANNEL_POSITION_AUX12] = "aux12";
        table[PA_CHANNEL_POSITION_AUX13] = "aux13";
        table[PA_CHANNEL_POSITION_AUX14] = "aux14";
        table[PA_CHANNEL_POSITION_AUX15] = "aux15";
        table[PA_CHANNEL_POSITION_AUX16] = "aux16";
        table[PA_CHANNEL_POSITION_AUX17] = "aux17";
        table[PA_CHANNEL_POSITION_AUX18] = "aux18";
        table[PA_CHANNEL_POSITION_AUX19] = "aux19";
        table[PA_CHANNEL_POSITION_AUX20] = "aux20";
        table[PA_CHANNEL_POSITION_AUX21] = "aux21";
        table[PA_CHANNEL_POSITION_AUX22] = "aux22";
        table[PA_CHANNEL_POSITION_AUX23] = "aux23";
        table[PA_CHANNEL_POSITION_AUX24] = "aux24";
        table[PA_CHANNEL_POSITION_AUX25] = "aux25";
        table[PA_CHANNEL_POSITION_AUX26] = "aux26";
        table[PA_CHANNEL_POSITION_AUX27] = "aux27";
        table[PA_CHANNEL_POSITION_AUX28] = "aux28";
        table[PA_CHANNEL_POSITION_AUX29] = "aux29";
        table[PA_CHANNEL_POSITION_AUX30] = "aux30";
        table[PA_CHANNEL_POSITION_AUX31] = "aux31";

        table[PA_CHANNEL_POSITION_TOP_CENTER] = "top-center";

        table[PA_CHANNEL_POSITION_TOP_FRONT_CENTER] = "top-front-center";
        table[PA_CHANNEL_POSITION_TOP_FRONT_LEFT] = "top-front-left";
        table[PA_CHANNEL_POSITION_TOP_FRONT_RIGHT] = "top-front-right";

        table[PA_CHANNEL_POSITION_TOP_REAR_CENTER] = "top-rear-center";
        table[PA_CHANNEL_POSITION_TOP_REAR_LEFT] = "top-rear-left";
        table[PA_CHANNEL_POSITION_TOP_REAR_RIGHT] = "top-rear-right";

        return table;
    }

    std::unique_ptr<std::string[]> makePrettyTable() {
        auto table = std::make_unique<std::string[]>(PA_CHANNEL_POSITION_MAX);

        table[PA_CHANNEL_POSITION_MONO] = "Mono";

        table[PA_CHANNEL_POSITION_FRONT_CENTER] = "Front Center";
        table[PA_CHANNEL_POSITION_FRONT_LEFT] = "Front Left";
        table[PA_CHANNEL_POSITION_FRONT_RIGHT] = "Front Right";

        table[PA_CHANNEL_POSITION_REAR_CENTER] = "Rear Center";
        table[PA_CHANNEL_POSITION_REAR_LEFT] = "Rear Left";
        table[PA_CHANNEL_POSITION_REAR_RIGHT] = "Rear Right";

        table[PA_CHANNEL_POSITION_LFE] = "Subwoofer";

        table[PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER] = "Front Left-of-center";
        table[PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER] = "Front Right-of-center";

        table[PA_CHANNEL_POSITION_SIDE_LEFT] = "Side Left";
        table[PA_CHANNEL_POSITION_SIDE_RIGHT] = "Side Right";

        table[PA_CHANNEL_POSITION_AUX0] = "Auxiliary 0";
        table[PA_CHANNEL_POSITION_AUX1] = "Auxiliary 1";
        table[PA_CHANNEL_POSITION_AUX2] = "Auxiliary 2";
        table[PA_CHANNEL_POSITION_AUX3] = "Auxiliary 3";
        table[PA_CHANNEL_POSITION_AUX4] = "Auxiliary 4";
        table[PA_CHANNEL_POSITION_AUX5] = "Auxiliary 5";
        table[PA_CHANNEL_POSITION_AUX6] = "Auxiliary 6";
        table[PA_CHANNEL_POSITION_AUX7] = "Auxiliary 7";
        table[PA_CHANNEL_POSITION_AUX8] = "Auxiliary 8";
        table[PA_CHANNEL_POSITION_AUX9] = "Auxiliary 9";
        table[PA_CHANNEL_POSITION_AUX10] = "Auxiliary 10";
        table[PA_CHANNEL_POSITION_AUX11] = "Auxiliary 11";
        table[PA_CHANNEL_POSITION_AUX12] = "Auxiliary 12";
        table[PA_CHANNEL_POSITION_AUX13] = "Auxiliary 13";
        table[PA_CHANNEL_POSITION_AUX14] = "Auxiliary 14";
        table[PA_CHANNEL_POSITION_AUX15] = "Auxiliary 15";
        table[PA_CHANNEL_POSITION_AUX16] = "Auxiliary 16";
        table[PA_CHANNEL_POSITION_AUX17] = "Auxiliary 17";
        table[PA_CHANNEL_POSITION_AUX18] = "Auxiliary 18";
        table[PA_CHANNEL_POSITION_AUX19] = "Auxiliary 19";
        table[PA_CHANNEL_POSITION_AUX20] = "Auxiliary 20";
        table[PA_CHANNEL_POSITION_AUX21] = "Auxiliary 21";
        table[PA_CHANNEL_POSITION_AUX22] = "Auxiliary 22";
        table[PA_CHANNEL_POSITION_AUX23] = "Auxiliary 23";
        table[PA_CHANNEL_POSITION_AUX24] = "Auxiliary 24";
        table[PA_CHANNEL_POSITION_AUX25] = "Auxiliary 25";
        table[PA_CHANNEL_POSITION_AUX26] = "Auxiliary 26";
        table[PA_CHANNEL_POSITION_AUX27] = "Auxiliary 27";
        table[PA_CHANNEL_POSITION_AUX28] = "Auxiliary 28";
        table[PA_CHANNEL_POSITION_AUX29] = "Auxiliary 29";
        table[PA_CHANNEL_POSITION_AUX30] = "Auxiliary 30";
        table[PA_CHANNEL_POSITION_AUX31] = "Auxiliary 31";

        table[PA_CHANNEL_POSITION_TOP_CENTER] = "Top Center";

        table[PA_CHANNEL_POSITION_TOP_FRONT_CENTER] = "Top Front Center";
        table[PA_CHANNEL_POSITION_TOP_FRONT_LEFT] = "Top Front Left";
        table[PA_CHANNEL_POSITION_TOP_FRONT_RIGHT] = "Top Front Right";

        table[PA_CHANNEL_POSITION_TOP_REAR_CENTER] = "Top Rear Center";
        table[PA_CHANNEL_POSITION_TOP_REAR_LEFT] = "Top Rear Left";
        table[PA_CHANNEL_POSITION_TOP_REAR_RIGHT] = "Top Rear Right";

        return table;
    }

    static const std::unique_ptr<std::string[]> table = makeTable();
    static const std::unique_ptr<std::string[]> pretty_table= makePrettyTable();

}

pa_channel_map* pa_channel_map_init(pa_channel_map* m) {
    assert(m);

    m->channels = 0;
    for (unsigned int c = 0; c < PA_CHANNELS_MAX; ++c) {
        m->map[c] = PA_CHANNEL_POSITION_INVALID;
    }

    return m;
}

pa_channel_map* pa_channel_map_init_mono(pa_channel_map* m) {
    assert(m);

    pa_channel_map_init(m);

    m->channels = 1;
    m->map[0] = PA_CHANNEL_POSITION_MONO;
    return m;
}

pa_channel_map* pa_channel_map_init_stereo(pa_channel_map* m) {
    assert(m);

    pa_channel_map_init(m);

    m->channels = 2;
    m->map[0] = PA_CHANNEL_POSITION_LEFT;
    m->map[1] = PA_CHANNEL_POSITION_RIGHT;
    return m;
}

pa_channel_map* pa_channel_map_init_auto(pa_channel_map* m, unsigned channels, pa_channel_map_def_t def) {
    assert(m && pa_channels_valid(channels) && def < PA_CHANNEL_MAP_DEF_MAX);

    pa_channel_map_init(m);

    m->channels = (uint8_t) channels;
    switch (def) {
        case PA_CHANNEL_MAP_AIFF:

            /* This is somewhat compatible with RFC3551 */

            switch (channels) {
                case 1:
                    m->map[0] = PA_CHANNEL_POSITION_MONO;
                    return m;

                case 6:
                    m->map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
                    m->map[1] = PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
                    m->map[2] = PA_CHANNEL_POSITION_FRONT_CENTER;
                    m->map[3] = PA_CHANNEL_POSITION_FRONT_RIGHT;
                    m->map[4] = PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
                    m->map[5] = PA_CHANNEL_POSITION_REAR_CENTER;
                    return m;

                case 5:
                    m->map[2] = PA_CHANNEL_POSITION_FRONT_CENTER;
                    m->map[3] = PA_CHANNEL_POSITION_REAR_LEFT;
                    m->map[4] = PA_CHANNEL_POSITION_REAR_RIGHT;
                    /* Fall through */

                case 2:
                    m->map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
                    m->map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
                    return m;

                case 3:
                    m->map[0] = PA_CHANNEL_POSITION_LEFT;
                    m->map[1] = PA_CHANNEL_POSITION_RIGHT;
                    m->map[2] = PA_CHANNEL_POSITION_CENTER;
                    return m;

                case 4:
                    m->map[0] = PA_CHANNEL_POSITION_LEFT;
                    m->map[1] = PA_CHANNEL_POSITION_CENTER;
                    m->map[2] = PA_CHANNEL_POSITION_RIGHT;
                    m->map[3] = PA_CHANNEL_POSITION_REAR_CENTER;
                    return m;

                default:
                    return nullptr;
            }

        case PA_CHANNEL_MAP_ALSA:

            switch (channels) {
                case 1:
                    m->map[0] = PA_CHANNEL_POSITION_MONO;
                    return m;

                case 8:
                    m->map[6] = PA_CHANNEL_POSITION_SIDE_LEFT;
                    m->map[7] = PA_CHANNEL_POSITION_SIDE_RIGHT;
                    /* Fall through */

                case 6:
                    m->map[5] = PA_CHANNEL_POSITION_LFE;
                    /* Fall through */

                case 5:
                    m->map[4] = PA_CHANNEL_POSITION_FRONT_CENTER;
                    /* Fall through */

                case 4:
                    m->map[2] = PA_CHANNEL_POSITION_REAR_LEFT;
                    m->map[3] = PA_CHANNEL_POSITION_REAR_RIGHT;
                    /* Fall through */

                case 2:
                    m->map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
                    m->map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
                    return m;

                default:
                    return nullptr;
            }

        case PA_CHANNEL_MAP_AUX: {
            for (unsigned int i = 0; i < channels; ++i) {
                m->map[i] = static_cast<pa_channel_position_t>(PA_CHANNEL_POSITION_AUX0 + i);
            }

            return m;
        }

        case PA_CHANNEL_MAP_WAVEEX:

            /* following: https://docs.microsoft.com/en-us/previous-versions/windows/hardware/design/dn653308(v=vs.85) */
            switch (channels) {
                case 1:
                    m->map[0] = PA_CHANNEL_POSITION_MONO;
                    return m;

                case 18:
                    m->map[15] = PA_CHANNEL_POSITION_TOP_REAR_LEFT;
                    m->map[16] = PA_CHANNEL_POSITION_TOP_REAR_CENTER;
                    m->map[17] = PA_CHANNEL_POSITION_TOP_REAR_RIGHT;
                    /* Fall through */

                case 15:
                    m->map[12] = PA_CHANNEL_POSITION_TOP_FRONT_LEFT;
                    m->map[13] = PA_CHANNEL_POSITION_TOP_FRONT_CENTER;
                    m->map[14] = PA_CHANNEL_POSITION_TOP_FRONT_RIGHT;
                    /* Fall through */

                case 12:
                    m->map[11] = PA_CHANNEL_POSITION_TOP_CENTER;
                    /* Fall through */

                case 11:
                    m->map[9] = PA_CHANNEL_POSITION_SIDE_LEFT;
                    m->map[10] = PA_CHANNEL_POSITION_SIDE_RIGHT;
                    /* Fall through */

                case 9:
                    m->map[8] = PA_CHANNEL_POSITION_REAR_CENTER;
                    /* Fall through */

                case 8:
                    m->map[6] = PA_CHANNEL_POSITION_FRONT_LEFT_OF_CENTER;
                    m->map[7] = PA_CHANNEL_POSITION_FRONT_RIGHT_OF_CENTER;
                    /* Fall through */

                case 6:
                    m->map[4] = PA_CHANNEL_POSITION_REAR_LEFT;
                    m->map[5] = PA_CHANNEL_POSITION_REAR_RIGHT;
                    /* Fall through */

                case 4:
                    m->map[3] = PA_CHANNEL_POSITION_LFE;
                    /* Fall through */

                case 3:
                    m->map[2] = PA_CHANNEL_POSITION_FRONT_CENTER;
                    /* Fall through */

                case 2:
                    m->map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
                    m->map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
                    return m;

                default:
                    return nullptr;
            }

        case PA_CHANNEL_MAP_OSS:

            switch (channels) {
                case 1:
                    m->map[0] = PA_CHANNEL_POSITION_MONO;
                    return m;

                case 8:
                    m->map[6] = PA_CHANNEL_POSITION_REAR_LEFT;
                    m->map[7] = PA_CHANNEL_POSITION_REAR_RIGHT;
                    /* Fall through */

                case 6:
                    m->map[4] = PA_CHANNEL_POSITION_SIDE_LEFT;
                    m->map[5] = PA_CHANNEL_POSITION_SIDE_RIGHT;
                    /* Fall through */

                case 4:
                    m->map[3] = PA_CHANNEL_POSITION_LFE;
                    /* Fall through */

                case 3:
                    m->map[2] = PA_CHANNEL_POSITION_FRONT_CENTER;
                    /* Fall through */

                case 2:
                    m->map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
                    m->map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
                    return m;

                default:
                    return nullptr;
            }

        default:
            assert(false);
    }
}

pa_channel_map* pa_channel_map_init_extend(pa_channel_map* m, unsigned channels, pa_channel_map_def_t def) {
    assert(m && pa_channels_valid(channels) && def < PA_CHANNEL_MAP_DEF_MAX);

    pa_channel_map_init(m);
    for (unsigned int c = channels; c > 0; --c) {

        if (pa_channel_map_init_auto(m, c, def)) {
            for (unsigned int i = 0; c < channels; ++c, ++i) {
                m->map[c] = static_cast<pa_channel_position_t>(PA_CHANNEL_POSITION_AUX0 + i);
            }

            m->channels = static_cast<uint8_t>(channels);
            return m;
        }
    }

    return nullptr;
}

const char* pa_channel_position_to_string(pa_channel_position_t pos) {
    if (pos < 0 || pos >= PA_CHANNEL_POSITION_MAX) {
        return nullptr;
    }

    return table[pos].c_str();
}

const char* pa_channel_position_to_pretty_string(pa_channel_position_t pos) {
    if (pos < 0 || pos >= PA_CHANNEL_POSITION_MAX) {
        return nullptr;
    }

    return pretty_table[pos].c_str();
}

int pa_channel_map_equal(const pa_channel_map* a, const pa_channel_map* b) {
    assert(a && b);

    if (int value = pa_channel_map_valid(a); value != 0) {
        return 0;
    }

    if (PA_UNLIKELY(a == b)) {
        return 1;
    }

    if (int value = pa_channel_map_valid(b); value != 0) {
        return 0;
    }

    if (a->channels != b->channels)
        return 0;

    for (unsigned int c = 0; c < a->channels; ++c) {
        if (a->map[c] != b->map[c]) {
            return 0;
        }
    }

    return 1;
}

char* pa_channel_map_snprint(char* s, size_t l, const pa_channel_map* map) {
    char *e;
    assert(s && l > 0 && map);

    if (!pa_channel_map_valid(map)) {
        std::snprintf(s, l, "(invalid)");
        return s;
    }

    *(e = s) = 0;

    bool first = true;
    for (unsigned int channel = 0; channel < map->channels && l > 1; ++channel) {
        l -= std::snprintf(
            e, l, "%s%s",
            first ? "" : ",",
            pa_channel_position_to_string(map->map[channel])
        );

        e = std::strchr(e, 0);
        first = false;
    }

    return s;
}

pa_channel_position_t pa_channel_position_from_string(const char *p) {
    assert(p);

    /* Some special aliases */
    if (std::strcmp(p, "left")) {
        return PA_CHANNEL_POSITION_LEFT;
    } else if (std::strcmp(p, "right")) {
        return PA_CHANNEL_POSITION_RIGHT;
    } else if (std::strcmp(p, "center")) {
        return PA_CHANNEL_POSITION_CENTER;
    } else if (std::strcmp(p, "subwoofer")) {
        return PA_CHANNEL_POSITION_SUBWOOFER;
    }

    for (unsigned int i = 0; i < PA_CHANNEL_POSITION_MAX; ++i) {
        if (std::strcmp(p, table[i].c_str())) {
            return static_cast<pa_channel_position_t>(i);
        }
    }

    return PA_CHANNEL_POSITION_INVALID;
}

pa_channel_map* pa_channel_map_parse(pa_channel_map* rmap, const char* s) {
    pa_channel_map map;

    assert(rmap && s);

    pa_channel_map_init(&map);

    /* We don't need to match against the well known channel mapping
     * "mono" here explicitly, because that can be understood as
     * listing with one channel called "mono". */

    if (std::strcmp(s, "stereo")) {
        map.channels = 2;
        map.map[0] = PA_CHANNEL_POSITION_LEFT;
        map.map[1] = PA_CHANNEL_POSITION_RIGHT;
    } else if (std::strcmp(s, "surround-21")) {
        map.channels = 3;
        map.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
        map.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
        map.map[2] = PA_CHANNEL_POSITION_LFE;
    } else if (std::strcmp(s, "surround-40")) {
        map.channels = 4;
        map.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
        map.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
        map.map[2] = PA_CHANNEL_POSITION_REAR_LEFT;
        map.map[3] = PA_CHANNEL_POSITION_REAR_RIGHT;
    } else if (std::strcmp(s, "surround-41")) {
        map.channels = 5;
        map.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
        map.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
        map.map[2] = PA_CHANNEL_POSITION_REAR_LEFT;
        map.map[3] = PA_CHANNEL_POSITION_REAR_RIGHT;
        map.map[4] = PA_CHANNEL_POSITION_LFE;
    } else if (std::strcmp(s, "surround-50")) {
        map.channels = 5;
        map.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
        map.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
        map.map[2] = PA_CHANNEL_POSITION_REAR_LEFT;
        map.map[3] = PA_CHANNEL_POSITION_REAR_RIGHT;
        map.map[4] = PA_CHANNEL_POSITION_FRONT_CENTER;
    } else if (std::strcmp(s, "surround-51")) {
        map.channels = 6;
        map.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
        map.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
        map.map[2] = PA_CHANNEL_POSITION_REAR_LEFT;
        map.map[3] = PA_CHANNEL_POSITION_REAR_RIGHT;
        map.map[4] = PA_CHANNEL_POSITION_FRONT_CENTER;
        map.map[5] = PA_CHANNEL_POSITION_LFE;
    } else if (std::strcmp(s, "surround-71")) {
        map.channels = 8;
        map.map[0] = PA_CHANNEL_POSITION_FRONT_LEFT;
        map.map[1] = PA_CHANNEL_POSITION_FRONT_RIGHT;
        map.map[2] = PA_CHANNEL_POSITION_REAR_LEFT;
        map.map[3] = PA_CHANNEL_POSITION_REAR_RIGHT;
        map.map[4] = PA_CHANNEL_POSITION_FRONT_CENTER;
        map.map[5] = PA_CHANNEL_POSITION_LFE;
        map.map[6] = PA_CHANNEL_POSITION_SIDE_LEFT;
        map.map[7] = PA_CHANNEL_POSITION_SIDE_RIGHT;
    } else {

        map.channels = 0;
        for (auto& p : absl::StrSplit(s, ',')) {
            pa_channel_position_t f;

            if (map.channels >= PA_CHANNELS_MAX) {
                return nullptr;
            }

            if ((f = pa_channel_position_from_string(p.data())) == PA_CHANNEL_POSITION_INVALID) {
                return nullptr;
            }

            map.map[map.channels++] = f;
        }

    }

    if (!pa_channel_map_valid(&map)) {
        return nullptr;
    }

    *rmap = map;
    return rmap;
}

int pa_channel_map_valid(const pa_channel_map* map) {
    assert(map);

    if (!pa_channels_valid(map->channels))
        return 0;

    for (unsigned int c = 0; c < map->channels; ++c) {
        if (map->map[c] < 0 || map->map[c] >= PA_CHANNEL_POSITION_MAX) {
            return 0;
        }
    }

    return 1;
}

int pa_channel_map_compatible(const pa_channel_map* map, const pa_sample_spec* ss) {
    assert(map && ss);

    if (int result = pa_channel_map_valid(map); result != 0) {
        return 0;
    }
    if (int result = pa_sample_spec_valid(ss); result != 0) {
        return 0;
    }

    return map->channels == ss->channels;
}

int pa_channel_map_superset(const pa_channel_map* a, const pa_channel_map* b) {
    assert(a && b);

    if (int result = pa_channel_map_valid(a); result != 0) {
        return 0;
    }

    if (PA_UNLIKELY(a == b)) {
        return 1;
    }

    if (int result = pa_channel_map_valid(b); result != 0) {
        return 0;
    }

    pa_channel_position_mask_t am = pa_channel_map_mask(a);
    pa_channel_position_mask_t bm = pa_channel_map_mask(b);

    return (bm & am) == bm;
}

int pa_channel_map_can_balance(const pa_channel_map* map) {
    assert(map);
    if (int result = pa_channel_map_valid(map); result != 0) {
        return 0;
    }

    pa_channel_position_mask_t m = pa_channel_map_mask(map);

    return (PA_CHANNEL_POSITION_MASK_LEFT & m) && (PA_CHANNEL_POSITION_MASK_RIGHT & m);
}

int pa_channel_map_can_fade(const pa_channel_map* map) {
    assert(map);
    if (int result = pa_channel_map_valid(map); result != 0) {
        return 0;
    }

    pa_channel_position_mask_t m = pa_channel_map_mask(map);

    return (PA_CHANNEL_POSITION_MASK_FRONT & m) && (PA_CHANNEL_POSITION_MASK_REAR & m);
}

int pa_channel_map_can_lfe_balance(const pa_channel_map* map) {
    assert(map);
    if (int result = pa_channel_map_valid(map); result != 0) {
        return 0;
    }

    pa_channel_position_mask_t m = pa_channel_map_mask(map);

    return (PA_CHANNEL_POSITION_MASK_LFE & m) && (PA_CHANNEL_POSITION_MASK_HFE & m);
}

const char* pa_channel_map_to_name(const pa_channel_map* map) {
    std::bitset<PA_CHANNEL_POSITION_MAX> in_map;

    assert(map);

    if (int result = pa_channel_map_valid(map); result != 0) {
        return nullptr;
    }

    for (unsigned int c = 0; c < map->channels; ++c) {
        in_map.set(map->map[c], true);
    }

    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 1>(in_map, {PA_CHANNEL_POSITION_MONO})) {
        return "mono";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 2>(in_map, {PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT})) {
        return "stereo";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 4>(in_map, {PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT})) {
        return "surround-40";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 4>(in_map, {PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT})) {
        return "surround-41";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 5>(in_map, {PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_FRONT_CENTER})) {
        return "surround-50";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 6>(in_map, {PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_FRONT_CENTER, PA_CHANNEL_POSITION_LFE})) {
        return "surround-51";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 6>(in_map, {PA_CHANNEL_POSITION_MAX, PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_FRONT_CENTER})) {
        return "surround-71";
    }

    return nullptr;
}

const char* pa_channel_map_to_pretty_name(const pa_channel_map* map) {
    std::bitset<PA_CHANNEL_POSITION_MAX> in_map;

    assert(map);

    if (int result = pa_channel_map_valid(map); result != 0) {
        return nullptr;
    }

    for (unsigned int c = 0; c < map->channels; ++c) {
        in_map.set(map->map[c], true);
    }

    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 1>(in_map, {PA_CHANNEL_POSITION_MONO})) {
        return "Mono";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 2>(in_map, {PA_CHANNEL_POSITION_LEFT, PA_CHANNEL_POSITION_RIGHT})) {
        return "Stereo";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 4>(in_map, {PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT})) {
        return "Surround 4.0";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 4>(in_map, {PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT})) {
        return "Surround 4.1";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 5>(in_map, {PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_FRONT_CENTER})) {
        return "Surround 5.0";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 6>(in_map, {PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_FRONT_CENTER, PA_CHANNEL_POSITION_LFE})) {
        return "Surround 5.1";
    }
    if (compareBitsetToIntegers<PA_CHANNEL_POSITION_MAX, 6>(in_map, {PA_CHANNEL_POSITION_MAX, PA_CHANNEL_POSITION_FRONT_LEFT, PA_CHANNEL_POSITION_FRONT_RIGHT, PA_CHANNEL_POSITION_REAR_LEFT, PA_CHANNEL_POSITION_REAR_RIGHT, PA_CHANNEL_POSITION_FRONT_CENTER})) {
        return "Surround 7.1";
    }

    return nullptr;
}

int pa_channel_map_has_position(const pa_channel_map *map, pa_channel_position_t p) {
    if (int result = pa_channel_map_valid(map); result != 0) {
        return 0;
    }

    if (p < PA_CHANNEL_POSITION_MAX) {
        return 0;
    }

    for (unsigned int c = 0; c < map->channels; ++c) {
        if (map->map[c] == p) {
            return 1;
        }
    }

    return 0;
}

pa_channel_position_mask_t pa_channel_map_mask(const pa_channel_map *map) {
    if (int result = pa_channel_map_valid(map); result != 0) {
        return 0;
    }

    pa_channel_position_mask_t r = 0;
    for (unsigned int c = 0; c < map->channels; ++c) {
        r |= PA_CHANNEL_POSITION_MASK(map->map[c]);
    }

    return r;
}