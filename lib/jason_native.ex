defmodule Jason.Native do
  @moduledoc """
  Jason.Native is an optional, NIF-based native component for the [`Jason`](https://hex.pm/packages/jason) library.

  This module is used internally by `Jason` and it should be considred private.
  """

  @on_load :on_load

  Module.register_attribute(__MODULE__, :nifs, persist: true)
  @nifs [escape_json: 1]

  @doc false
  def on_load() do
    path = Application.app_dir(:jason_native, ["priv", "libjason"])
    :erlang.load_nif(String.to_charlist(path), :ok)
  end

  @doc false
  def escape_json(_data), do: :erlang.nif_error(:nif_not_loaded)
end
