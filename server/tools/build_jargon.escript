#!/usr/bin/env escript
%% Build the pinned reference Argon2 implementation without shell dependencies.
main(_) ->
    Windows = element(1, os:type()) =:= win32,
    Output = case Windows of true -> "priv/jargon.dll"; false -> "priv/jargon.so" end,
    ok = filelib:ensure_dir(Output),
    {ok, Source} = file:read_file("c_src/jargon.c"),
    %% jargon 1.1.0 accidentally uses decimal 13 instead of Argon2 version 0x13.
    %% Correct the wrapper only; the reference Argon2 sources stay untouched.
    Patched = binary:replace(Source, <<"#define ARGON2_VERSION 13">>,
                            <<"#define ARGON2_VERSION 0x13">>),
    ok = file:write_file("c_src/jargon_build.c", Patched),
    Sources = ["c_src/jargon_build.c", "argon2/src/argon2.c", "argon2/src/core.c",
               "argon2/src/blake2/blake2b.c", "argon2/src/thread.c",
               "argon2/src/encoding.c", "argon2/src/ref.c"],
    Include = filename:join([code:root_dir(),
                            "erts-" ++ erlang:system_info(version), "include"]),
    Platform = case os:type() of
        {win32, _} -> ["-static-libgcc"];
        {unix, darwin} -> ["-fPIC", "-undefined", "dynamic_lookup"];
        _ -> ["-fPIC"]
    end,
    Args = ["-O2", "-std=c99", "-shared", "-DARGON2_NO_THREADS",
            "-I", Include, "-I", "argon2/include"] ++ Platform ++ Sources ++
           ["-o", Output],
    Compiler = compiler(Windows),
    Port = open_port({spawn_executable, Compiler},
                     [{args, Args}, binary, exit_status, use_stdio, stderr_to_stdout]),
    wait(Port).

compiler(Windows) ->
    Candidates = case os:getenv("CC") of
        false when Windows -> ["C:/msys64/mingw64/bin/gcc.exe", "gcc"];
        false -> ["cc", "gcc"];
        Value -> [Value]
    end,
    case [Path || Name <- Candidates,
                  Path <- [os:find_executable(Name)], Path =/= false] of
        [First | _] -> First;
        [] -> io:format(standard_error, "Install a C compiler and set CC to its path.~n", []),
              halt(1)
    end.

wait(Port) ->
    receive
        {Port, {data, Data}} -> io:put_chars(Data), wait(Port);
        {Port, {exit_status, Status}} -> halt(Status)
    after 60000 -> port_close(Port), halt(1)
    end.
