-module(server_protocol).

-export([reply/1]).

reply(Data) ->
    Response = case decode(Data) of
        {ok, #{<<"type">> := <<"ping">>}} ->
            erlang:display("Gets ping from client"),
            #{<<"type">> => <<"pong">>};
        {ok, #{<<"type">> := Type}} when is_binary(Type) ->
            error_message(<<"unknown_message_type">>);
        {ok, _InvalidMessage} ->
            error_message(<<"invalid_message">>);
        {error, invalid_json} ->
            error_message(<<"invalid_json">>)
    end,
    json:encode(Response).

decode(Data) ->
    try json:decode(Data) of
        Message -> {ok, Message}
    catch
        error:_Reason -> {error, invalid_json}
    end.

error_message(Code) ->
    #{<<"type">> => <<"error">>, <<"code">> => Code}.
