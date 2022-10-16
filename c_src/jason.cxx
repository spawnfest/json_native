#include <vector>
#include <string_view>
#include <limits>
#include <cstring>

#include "erl_nif.h"
#include "unicode.hh"

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

template <class T>
struct enif_allocator
{
    typedef T value_type;

    enif_allocator() = default;
    template <class U> constexpr enif_allocator(const enif_allocator <U>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
            return nullptr;
        }

        if (auto p = static_cast<T*>(enif_alloc(n * sizeof(T)))) {
            return p;
        }

        return nullptr;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        (void)n;
        enif_free(p);
    }
};

const char8_t BB = 'b';  // \x08
const char8_t TT = 't';  // \x09
const char8_t NN = 'n';  // \x0A
const char8_t FF = 'f';  // \x0C
const char8_t RR = 'r';  // \x0D
const char8_t QU = '"';  // \x22
const char8_t BS = '\\'; // \x5C
const char8_t UU = 'u';  // \x00...\x1F except the ones above
const char8_t VV = 'v';  // validate \x80..\xFF
const char8_t __ = 0;

// Lookup table of escape sequences. A value of 'x' at index i means that byte
// i is escaped as "\x" in JSON. A value of __ means that byte i is not escaped.
const char8_t ESCAPE[256] = {
    //   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
    UU, UU, UU, UU, UU, UU, UU, UU, BB, TT, NN, UU, FF, RR, UU, UU, // 0
    UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, UU, // 1
    __, __, QU, __, __, __, __, __, __, __, __, __, __, __, __, __, // 2
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 3
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 4
    __, __, __, __, __, __, __, __, __, __, __, __, BS, __, __, __, // 5
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 6
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 7
    VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, // 8
    VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, // 9
    VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, // A
    VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, // B
    VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, // C
    VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, // D
    VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, // E
    VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, VV, // F
};


// How much to overallocate the binary in anticipation of escapes
// compared to the original data
#define ESTIMATE_GROWTH (110/100)
const size_t MIN_CHUNK = 64;

class IoListBuffer
{
public:
    IoListBuffer(ErlNifEnv* env) : _env(env), _current({}), _current_written(0) {}

    ~IoListBuffer() {
        if (_current.data != nullptr) {
            enif_release_binary(&_current);
        }
    }

    void restore(ERL_NIF_TERM term) {
        _acc.push_back(term);
    }

    bool write(std::u8string_view span, size_t remaining_hint) {
        size_t to_write = std::min(_current.size - _current_written, span.size());
        this->write_current(span.data(), to_write);

        size_t left = span.size() - to_write;
        if (left > 0) {
            if (_current_written > 0) {
                ERL_NIF_TERM bin = this->make_binary();
                _acc.push_back(bin);
            }
            size_t needed = std::max((left + remaining_hint) * ESTIMATE_GROWTH, MIN_CHUNK);
            if (!enif_alloc_binary(needed, &_current)) {
                return false;
            }
            this->write_current(&span[to_write], left);
        }
        return true;
    }

    ERL_NIF_TERM finish() {
        if (_current_written != 0) {
            if (!enif_realloc_binary(&_current, _current_written)) {
                return enif_make_badarg(_env);
            }
            ERL_NIF_TERM bin = this->make_binary();
            _acc.push_back(bin);
        }
        if (_acc.size() == 1) {
            return _acc[0];
        } else {
            return enif_make_list_from_array(_env, _acc.data(), _acc.size());
        }
    }

private:
    ERL_NIF_TERM make_binary() {
        ERL_NIF_TERM bin = enif_make_binary(_env, &_current);
        _current = {};
        _current_written = 0;
        return bin;
    }

    void write_current(const char8_t* src, size_t len) {
        std::memcpy(&_current.data[_current_written], src, len);
        _current_written += len;
    }

    ErlNifEnv* _env;
    ErlNifBinary _current;
    size_t _current_written;
    std::vector<ERL_NIF_TERM, enif_allocator<ERL_NIF_TERM>> _acc;
};

// How many times per schedule to call consume_percentage
const int CHECKIN_FREQUENCY = 4;
const int CHECKIN_PROGRESS = 100 / CHECKIN_FREQUENCY;
#ifdef MIX_TEST
// Force more freuqent yielding when testing
const int REDS = 16;
#else
// Mirrors Erlang's default reductions per schedule (4000) as power of two
const int REDS = 4096;
#endif
const int DEFAULT_REDS = REDS / CHECKIN_FREQUENCY;
// Bytes per single reduction - power of 2 for fast division
const int SKIP_REDS_FACTOR = 64;
const int LOOP_REDS_FACTOR = 32;

ERL_NIF_TERM escape_json(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    if (argc < 1) {
        return enif_make_badarg(env);
    }

    ERL_NIF_TERM original = argv[0];
    ErlNifBinary raw_data;
    if (!enif_inspect_binary(env, original, &raw_data)) {
        return enif_make_badarg(env);
    }
    std::u8string_view data(reinterpret_cast<char8_t*>(raw_data.data), raw_data.size);

    int reds = DEFAULT_REDS;

    reds *= SKIP_REDS_FACTOR;

    size_t skipped = 0;
    if (argc == 1) {
        // Nothing, plain call
    } else if (argc == 2) {
        // Restore, if we yielded during skipping
        if (!enif_get_ulong(env, argv[1], &skipped)) {
            return enif_make_badarg(env);
        }
    } else if (argc == 4) {
        // Restore if we yielded during main loop
        goto main_loop;
    } else {
        return enif_make_badarg(env);
    }

    for (; skipped < data.size(); skipped++) {
        if (unlikely(reds-- == 0)) {
            if (enif_consume_timeslice(env, CHECKIN_PROGRESS)) {
                ERL_NIF_TERM skipped_term = enif_make_ulong(env, skipped);

                const ERL_NIF_TERM new_argv[2] = { original, skipped_term };
                return enif_schedule_nif(env, "escape_json", 0, escape_json, 2, new_argv);
            } else {
                reds = DEFAULT_REDS * SKIP_REDS_FACTOR;
            }
        }

        char8_t byte = data[skipped];
        if (ESCAPE[byte] != __) {
            break;
        }
    }

    reds /= SKIP_REDS_FACTOR;

    // No escaping is necessary
    if (skipped == data.size()) {
        enif_consume_timeslice(env, (DEFAULT_REDS - reds) * CHECKIN_PROGRESS / DEFAULT_REDS);
        return original;
    }

main_loop:
    IoListBuffer buffer = IoListBuffer(env);
    size_t start = 0;
    size_t i = skipped;

    if (argc == 4) {
        // Restore, if we yielded during main loop
        if (!enif_get_ulong(env, argv[1], &i)) {
            return enif_make_badarg(env);
        }
        if (!enif_get_ulong(env, argv[2], &start)) {
            return enif_make_badarg(env);
        }
        buffer.restore(argv[3]);
    }

    reds *= LOOP_REDS_FACTOR;
    for (; i < data.size(); i++) {
        if (unlikely(reds-- <= 0)) {
            if (enif_consume_timeslice(env, CHECKIN_PROGRESS)) {
                ERL_NIF_TERM i_term = enif_make_ulong(env, i);
                ERL_NIF_TERM start_term = enif_make_ulong(env, start);
                ERL_NIF_TERM buffer_term = buffer.finish();
                if (enif_is_exception(env, buffer_term)) {
                    return buffer_term;
                }

                const ERL_NIF_TERM new_argv[4] = { original, i_term, start_term, buffer_term };
                return enif_schedule_nif(env, "escape_json", 0, escape_json, 4, new_argv);
            } else {
                reds = DEFAULT_REDS * LOOP_REDS_FACTOR;
            }
        }

        char8_t byte = data[i];
        char8_t escape = ESCAPE[byte];
        if (escape == __) {
            continue;
        } else if (escape == VV) {
            uint32_t codepoint;
            uint32_t state = 0;

            for (; i < data.size(); i++) {
                char8_t byte = data[i];
                // We delay yielding for couple bytes for simplicity,
                // this isn't a big issue since a UTF8 char has at most 4 bytes
                reds--;
                switch (utf8_decode(&state, &codepoint, byte)) {
                case UTF8_ACCEPT:
                    goto cont;
                case UTF8_REJECT:
                    goto reject;
                default:
                    break;
                }
            }
        reject:
            {
                ERL_NIF_TERM tag = enif_make_atom(env, "invalid_byte");
                ERL_NIF_TERM byte_term = enif_make_uint(env, byte);
                ERL_NIF_TERM reason = enif_make_tuple3(env, tag, byte_term, original);
                return enif_raise_exception(env, reason);
            }
        cont:
            continue;
        }

        if (start < i) {
            // overallocate for entire escape sequence
            if (!buffer.write(data.substr(start, i - start), data.size() - i + 6)) {
                return enif_make_badarg(env);
            }
        }

        if (escape == UU) {
            const char8_t HEX_DIGITS[17] = u8"0123456789ABCDEF";

            char8_t escaped[] = { '\\', 'u', '0', '0', HEX_DIGITS[byte >> 4], HEX_DIGITS[byte & 0xF] };
            std::u8string_view span(escaped, 6);
            buffer.write(span, data.size() - i);
        } else {
            char8_t escaped[] = { '\\', escape };
            std::u8string_view span(escaped, 2);
            buffer.write(span, data.size() - i);
        }

        start = i + 1;
    }
    reds /= LOOP_REDS_FACTOR;

    if (start != data.size()) {
        if (!buffer.write(data.substr(start), 0)) {
            return enif_make_badarg(env);
        }
    }

    enif_consume_timeslice(env, (DEFAULT_REDS - reds) * CHECKIN_PROGRESS / DEFAULT_REDS);
    return buffer.finish();
}

static ErlNifFunc jason_funcs[] = {
    {"escape_json", 1, escape_json, 0},
};

ERL_NIF_INIT(Elixir.Jason.Native, jason_funcs, NULL, NULL, NULL, NULL);
