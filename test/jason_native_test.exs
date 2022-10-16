defmodule Jason.NativeTest do
  use ExUnit.Case, async: true
  use ExUnitProperties

  doctest Jason.Native

  import Jason.Native

  test "escape_json/1" do
    assert escape_json("hello world") == ~S(hello world)
    assert escape_json("hello\nworld") == ~S(hello\nworld)
    assert escape_json("\nhello\nworld\n") == ~S(\nhello\nworld\n)

    assert escape_json("\"") == ~S(\")
    assert escape_json("\0") == ~S(\u0000)
    assert escape_json(<<31>>) == ~S(\u001F)

    assert escape_json("\0\0\0\0\0\0") == ~S(\u0000\u0000\u0000\u0000\u0000\u0000)

    long_text = String.duplicate("a", 64 * 2)

    assert escape_json(long_text <> "\0\0\0\0\0\0") == [
             long_text <> ~S(\u0000\u0000),
             ~S(\u0000\u0000\u0000\u0000)
           ]

    # Force to go into main loop early
    assert escape_json("\0" <> long_text) == ~S(\u0000) <> long_text
  end

  ## Adapted from https://github.com/davisp/jiffy/blob/9ea1b35b6e60ba21dfd4adbd18e7916a831fd7d4/test/jiffy_04_string_tests.erl#L136
  @bad_unicode [
    <<0xC2, 0x81, 0x80>>,
    <<"foo", 0x80, "bar">>,

    # Not enough extension bytes
    <<0xC0>>,
    <<0xE0>>,
    <<0xE0, 0x80>>,
    <<0xF0>>,
    <<0xF0, 0x80>>,
    <<0xF0, 0x80, 0x80>>,
    <<0xF8>>,
    <<0xF8, 0x80>>,
    <<0xF8, 0x80, 0x80>>,
    <<0xF8, 0x80, 0x80, 0x80>>,
    <<0xFC>>,
    <<0xFC, 0x80>>,
    <<0xFC, 0x80, 0x80>>,
    <<0xFC, 0x80, 0x80, 0x80>>,
    <<0xFC, 0x80, 0x80, 0x80, 0x80>>,

    # No data in high bits.
    <<0xC0, 0x80>>,
    <<0xC1, 0x80>>,
    <<0xE0, 0x80, 0x80>>,
    <<0xE0, 0x90, 0x80>>,
    <<0xF0, 0x80, 0x80, 0x80>>,
    <<0xF0, 0x88, 0x80, 0x80>>,

    # UTF-8-like sequenecs of greater than 4 bytes aren't valid
    <<0xF8, 0x80, 0x80, 0x80, 0x80>>,
    <<0xF8, 0x84, 0x80, 0x80, 0x80>>,
    <<0xFC, 0x80, 0x80, 0x80, 0x80, 0x80>>,
    <<0xFC, 0x82, 0x80, 0x80, 0x80, 0x80>>
  ]

  test "bad unicode" do
    assert_raise ErlangError, "Erlang error: {:invalid_byte, 128, <<128>>}", fn ->
      escape_json(<<0x80>>)
    end

    assert_raise ErlangError, "Erlang error: {:invalid_byte, 208, <<97, 208>>}", fn ->
      escape_json(<<?a, 208>>)
    end

    for data <- @bad_unicode do
      assert_raise ErlangError, ~r/:invalid_byte/, fn ->
        escape_json(data)
      end
    end
  end

  property "escape_json/1" do
    check all(string <- string(:printable)) do
      escaped = IO.iodata_to_binary(escape_json(string))

      for <<(<<byte>> <- escaped)>> do
        assert byte < 0xF1 or byte not in [?", ?\\]
      end

      assert Jason.decode!([?", escaped, ?"]) == string
    end
  end
end
