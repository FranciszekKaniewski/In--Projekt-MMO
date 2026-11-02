%%%-------------------------------------------------------------------
%% @doc server top level supervisor.
%% @end
%%%-------------------------------------------------------------------

-module(server_sup).

-behaviour(supervisor).

-export([start_link/0]).

-export([init/1]).

-define(SERVER, ?MODULE).

start_link() ->
    supervisor:start_link({local, ?SERVER}, ?MODULE, []).

init([]) ->
    SupFlags = #{
        strategy => rest_for_one,
        intensity => 5,
        period => 10
    },
    %% The listener depends on the connection supervisor. Restart it too if
    %% that supervisor fails; a listener restart can keep existing clients.
    ChildSpecs = [
        #{id => server_connection_sup,
          start => {server_connection_sup, start_link, []},
          type => supervisor},
        #{id => server_listener,
          start => {server_listener, start_link, []}}
    ],
    {ok, {SupFlags, ChildSpecs}}.

%% internal functions
