#include <vector>
#include <string_view>
#include <limits>

#include "erl_nif.h"

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
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 8
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // 9
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // A
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // B
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // C
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // D
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // E
    __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __, // F
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

ERL_NIF_TERM escape_json(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    if (argc != 1) {
        return enif_make_badarg(env);
    }

    ERL_NIF_TERM original = argv[0];
    ErlNifBinary raw_data;
    if (!enif_inspect_binary(env, original, &raw_data)) {
        return enif_make_badarg(env);
    }
    std::u8string_view data(reinterpret_cast<char8_t*>(raw_data.data), raw_data.size);

    size_t skipped = 0;
    for (; skipped < data.size(); skipped++) {
        char8_t byte = data[skipped];
        if (ESCAPE[byte] != __) {
            break;
        }
    }

    // No escaping is necessary
    if (skipped == data.size()) {
        return original;
    }

    IoListBuffer buffer(env);
    size_t start = 0;

    for (size_t i = skipped; i < data.size(); i++) {
        char8_t byte = data[i];
        char8_t escape = ESCAPE[byte];

        if (escape == __) {
            continue;
        }

        if (start < i) {
            // overallocate for entire escape sequence
            if (!buffer.write(data.substr(start, i - start), data.size() - i + 6)) {
                return enif_make_badarg(env);
            }
        }

        char8_t escaped[] = { '\\', escape };
        std::u8string_view span(escaped, 2);
        // overallocate for the hex escape sequence
        buffer.write(span, data.size() - i + 4);

        if (escape == 'u') {
            const char8_t HEX_DIGITS[17] = u8"0123456789ABCDEF";

            char8_t escaped[] = { '0', '0', HEX_DIGITS[byte >> 4], HEX_DIGITS[byte & 0xF] };
            std::u8string_view span(escaped, 4);
            buffer.write(span, data.size() - i);
        }

        start = i + 1;
    }

    if (start != data.size()) {
        if (!buffer.write(data.substr(start), 0)) {
            return enif_make_badarg(env);
        }
    }

    return buffer.finish();
}

static ErlNifFunc jason_funcs[] = {
    {"escape_json", 1, escape_json, 0},
};

ERL_NIF_INIT(Elixir.Jason.Native, jason_funcs, NULL, NULL, NULL, NULL);
