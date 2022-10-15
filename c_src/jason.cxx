#include "erl_nif.h"

ERL_NIF_TERM escape_json(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[]) {
    if (argc != 1) {
        return enif_make_badarg(env);
    }

    ErlNifBinary data;
    if (!enif_inspect_binary(env, argv[0], &data)) {
        return enif_make_badarg(env);
    }

    return argv[0];
}

static ErlNifFunc jason_funcs[] = {
    {"escape_json", 1, escape_json, 0},
};

ERL_NIF_INIT(Elixir.Jason.Native, jason_funcs, NULL, NULL, NULL, NULL);
