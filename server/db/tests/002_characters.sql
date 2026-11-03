-- All accounts and characters created here are rolled back.
-- Identity sequences may have gaps after these checks.
BEGIN;

DO $test$
DECLARE
    suffix text := substr(md5(random()::text), 1, 12);
    owner_id bigint;
    other_owner_id bigint;
    character_id bigint;
    second_character_id bigint;
    character_name text := 'Hero_' || suffix;
    snapshot public.characters%ROWTYPE;
    matches bigint;
    bad_value text;
    direction integer;
    saved_creation_time timestamptz;
BEGIN
    INSERT INTO public.users (username, password_hash)
    VALUES ('owner_' || suffix, 'schema-test-only-not-a-real-password-hash')
    RETURNING id INTO owner_id;
    INSERT INTO public.users (username, password_hash)
    VALUES ('other_' || suffix, 'schema-test-only-not-a-real-password-hash')
    RETURNING id INTO other_owner_id;

    INSERT INTO public.characters (user_id, name)
    VALUES (owner_id, character_name)
    RETURNING * INTO snapshot;
    character_id := snapshot.id;
    saved_creation_time := snapshot.created_at;

    IF snapshot.id <= 0 OR snapshot.user_id <> owner_id
       OR snapshot.map_key <> 'map_expanded'
       OR snapshot.position_x <> 1350.0 OR snapshot.position_y <> 1320.0
       OR snapshot.facing <> 4 OR snapshot.level <> 1 OR snapshot.experience <> 0
       OR snapshot.created_at IS NULL OR snapshot.updated_at IS NULL
       OR snapshot.last_logout_at IS NOT NULL THEN
        RAISE EXCEPTION 'Character defaults do not match SDL Scene2 and MovementComponent';
    END IF;

    INSERT INTO public.characters (user_id, name)
    VALUES (owner_id, 'Alt_' || suffix)
    RETURNING id INTO second_character_id;
    SELECT count(*) INTO matches FROM public.characters WHERE user_id = owner_id;
    IF matches <> 2 OR second_character_id = character_id THEN
        RAISE EXCEPTION 'One user must be able to own multiple distinct characters';
    END IF;

    -- A global character name cannot be claimed from another account.
    BEGIN
        INSERT INTO public.characters (user_id, name) VALUES (other_owner_id, upper(character_name));
        RAISE EXCEPTION 'A duplicate character name was accepted';
    EXCEPTION WHEN unique_violation THEN
        NULL;
    END;

    BEGIN
        INSERT INTO public.characters (user_id, name) VALUES (NULL, 'Null_' || suffix);
        RAISE EXCEPTION 'A character without an owner was accepted';
    EXCEPTION WHEN not_null_violation THEN
        NULL;
    END;

    -- A generated, unused negative ID avoids relying on the account sequence.
    BEGIN
        INSERT INTO public.characters (user_id, name)
        VALUES (-(owner_id + other_owner_id), 'Orphan_' || suffix);
        RAISE EXCEPTION 'A character referring to a missing account was accepted';
    EXCEPTION WHEN foreign_key_violation THEN
        NULL;
    END;

    BEGIN
        DELETE FROM public.users WHERE id = owner_id;
        RAISE EXCEPTION 'An account owning characters was deleted';
    EXCEPTION WHEN restrict_violation THEN
        NULL;
    END;

    FOREACH bad_value IN ARRAY ARRAY['', 'ab', repeat('x', 33), ' Hero', 'Hero ', E'Bad\nName', E'Bad\tName']
    LOOP
        BEGIN
            INSERT INTO public.characters (user_id, name) VALUES (other_owner_id, bad_value);
            RAISE EXCEPTION 'An invalid character name was accepted';
        EXCEPTION WHEN check_violation THEN
            NULL;
        END;
    END LOOP;

    -- Polish names and names containing ordinary internal spaces match chat.
    INSERT INTO public.characters (user_id, name)
    VALUES (other_owner_id, 'Żółw ' || suffix);

    FOREACH bad_value IN ARRAY ARRAY['', 'Map_expanded', 'bad map', '../map', repeat('a', 65)]
    LOOP
        BEGIN
            UPDATE public.characters SET map_key = bad_value WHERE id = character_id;
            RAISE EXCEPTION 'An invalid map key was accepted';
        EXCEPTION WHEN check_violation THEN
            NULL;
        END;
    END LOOP;

    FOREACH bad_value IN ARRAY ARRAY['NaN', 'Infinity', '-Infinity']
    LOOP
        BEGIN
            UPDATE public.characters SET position_x = bad_value::real WHERE id = character_id;
            RAISE EXCEPTION 'A non-finite X coordinate was accepted';
        EXCEPTION WHEN check_violation THEN
            NULL;
        END;
        BEGIN
            UPDATE public.characters SET position_y = bad_value::real WHERE id = character_id;
            RAISE EXCEPTION 'A non-finite Y coordinate was accepted';
        EXCEPTION WHEN check_violation THEN
            NULL;
        END;
    END LOOP;

    FOR direction IN 0..7 LOOP
        UPDATE public.characters SET facing = direction WHERE id = character_id;
    END LOOP;
    FOREACH direction IN ARRAY ARRAY[-1, 8] LOOP
        BEGIN
            UPDATE public.characters SET facing = direction WHERE id = character_id;
            RAISE EXCEPTION 'An invalid facing direction was accepted';
        EXCEPTION WHEN check_violation THEN
            NULL;
        END;
    END LOOP;

    BEGIN
        UPDATE public.characters SET level = 0 WHERE id = character_id;
        RAISE EXCEPTION 'A non-positive level was accepted';
    EXCEPTION WHEN check_violation THEN
        NULL;
    END;
    BEGIN
        UPDATE public.characters SET experience = -1 WHERE id = character_id;
        RAISE EXCEPTION 'Negative experience was accepted';
    EXCEPTION WHEN check_violation THEN
        NULL;
    END;

    UPDATE public.characters
    SET map_key = 'another_map', position_x = -10.25, position_y = 123.5,
        facing = 2, level = 2, experience = 100,
        last_logout_at = CURRENT_TIMESTAMP, updated_at = '2000-01-01 UTC'
    WHERE id = character_id
    RETURNING * INTO snapshot;
    IF snapshot.map_key <> 'another_map'
       OR snapshot.position_x <> -10.25 OR snapshot.position_y <> 123.5
       OR snapshot.facing <> 2 OR snapshot.level <> 2 OR snapshot.experience <> 100
       OR snapshot.last_logout_at IS NULL OR snapshot.updated_at <= '2000-01-01 UTC'
       OR snapshot.created_at <> saved_creation_time THEN
        RAISE EXCEPTION 'Character checkpoint or automatic update timestamp was not saved correctly';
    END IF;

    DELETE FROM public.characters WHERE user_id = owner_id;
    DELETE FROM public.users WHERE id = owner_id;
    IF NOT EXISTS (SELECT 1 FROM public.users WHERE id = other_owner_id) THEN
        RAISE EXCEPTION 'Deleting one account affected another account';
    END IF;

    RAISE NOTICE 'Character schema checks passed; all test accounts and characters will be rolled back';
END;
$test$;

ROLLBACK;
