-module(server_auth).
-export([register/3, login/3]).

register(IP, Username, Password) ->
    perform(register, IP, Username, Password, fun(_Dummy) ->
        case server_password:hash(Password) of
            {ok, Hash} -> server_accounts:create(Username, Hash);
            Error -> Error
        end
    end).

login(IP, Username, Password) ->
    perform(login, IP, Username, Password, fun(Dummy) ->
        case server_accounts:find(Username) of
            {ok, User, Hash} ->
                %% Invalid legacy/test hashes still pay the same hashing cost.
                case Hash of
                    <<"$argon2id$v=19$m=19456,t=2,p=1$", _/binary>> ->
                        case server_password:verify(Password, Hash) of
                            true -> {ok, User};
                            false -> {error, invalid_credentials}
                        end;
                    _ -> server_password:verify(Password, Dummy),
                         {error, invalid_credentials}
                end;
            not_found -> server_password:verify(Password, Dummy),
                         {error, invalid_credentials};
            Error -> Error
        end
    end).

perform(Action, IP, Username, Password, Fun) ->
    PasswordValid = case Action of
        register -> server_password:valid_password(Password);
        login -> server_password:valid_login_password(Password)
    end,
    case server_password:valid_username(Username) andalso
         PasswordValid of
        false when Action =:= login -> {error, invalid_credentials};
        false -> {error, invalid_registration};
        true ->
            case server_auth_guard:acquire(Action, IP, Username) of
                {ok, Lease, Dummy} ->
                    try Fun(Dummy)
                    catch _:_ -> {error, service_unavailable}
                    after server_auth_guard:release(Lease)
                    end;
                Error -> Error
            end
    end.
