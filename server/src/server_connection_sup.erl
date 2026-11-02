-module(server_connection_sup).
-behaviour(supervisor).

-export([start_link/0, start_connection/1, init/1]).

start_link() ->
    supervisor:start_link({local, ?MODULE}, ?MODULE, []).

start_connection(Socket) ->
    %% A closed connection must not be restarted with an obsolete socket.
    Child = #{id => make_ref(),
              start => {server_connection, start_link, [Socket]},
              restart => temporary},
    supervisor:start_child(?MODULE, Child).

init([]) ->
    {ok, {#{strategy => one_for_one, intensity => 5, period => 10}, []}}.
