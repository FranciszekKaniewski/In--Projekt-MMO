-- Constraint checks only. The synthetic hash is not a usable account password.
-- All test rows are rolled back; identity sequence values may have gaps.
BEGIN;

DO $test$
DECLARE
    suffix text := substr(md5(random()::text), 1, 12);
    login text := 'test_' || suffix;
    test_hash text := 'schema-test-only-not-a-real-password-hash';
    new_id bigint;
    creation_time timestamptz;
    matches bigint;
    invalid_login text;
BEGIN
    INSERT INTO public.users (username, password_hash)
    VALUES (login, test_hash)
    RETURNING id, created_at INTO new_id, creation_time;

    IF new_id IS NULL OR new_id <= 0 OR creation_time IS NULL THEN
        RAISE EXCEPTION 'Missing generated ID or creation timestamp';
    END IF;

    SELECT count(*) INTO matches FROM public.users
    WHERE lower(username) = lower(upper(login));
    IF matches <> 1 THEN
        RAISE EXCEPTION 'Case-insensitive account lookup failed';
    END IF;

    BEGIN
        INSERT INTO public.users (username, password_hash) VALUES (upper(login), test_hash);
        RAISE EXCEPTION 'A duplicate login with different letter case was accepted';
    EXCEPTION WHEN unique_violation THEN
        NULL;
    END;

    FOREACH invalid_login IN ARRAY ARRAY['', 'ab', repeat('a', 33), 'bad login', ' bad', E'bad\tlogin']
    LOOP
        BEGIN
            INSERT INTO public.users (username, password_hash) VALUES (invalid_login, test_hash);
            RAISE EXCEPTION 'An invalid login was accepted: %', invalid_login;
        EXCEPTION WHEN check_violation THEN
            NULL;
        END;
    END LOOP;

    BEGIN
        INSERT INTO public.users (username, password_hash) VALUES (NULL, test_hash);
        RAISE EXCEPTION 'A NULL login was accepted';
    EXCEPTION WHEN not_null_violation THEN
        NULL;
    END;

    BEGIN
        INSERT INTO public.users (username, password_hash) VALUES ('null_' || suffix, NULL);
        RAISE EXCEPTION 'A NULL password hash was accepted';
    EXCEPTION WHEN not_null_violation THEN
        NULL;
    END;

    BEGIN
        INSERT INTO public.users (username, password_hash) VALUES ('empty_' || suffix, '   ');
        RAISE EXCEPTION 'An empty password hash was accepted';
    EXCEPTION WHEN check_violation THEN
        NULL;
    END;

    RAISE NOTICE 'Users schema checks passed; test rows will be rolled back';
END;
$test$;

ROLLBACK;
