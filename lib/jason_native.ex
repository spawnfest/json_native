defmodule Jason.Native do
  @moduledoc """
  Documentation for `JasonNative`.
  """

  @on_load :on_load

  Module.register_attribute(__MODULE__, :nifs, persist: true)
  @nifs [escape_json: 1]

  def on_load() do
    path = Application.app_dir(:jason_native, ["priv", "libjason"])
    :erlang.load_nif(String.to_charlist(path), :ok)
  end

  def escape_json(_data), do: :erlang.nif_error(:nif_not_loaded)
end
