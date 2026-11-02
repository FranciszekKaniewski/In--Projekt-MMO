-- Run once against sdl_game. Later schema changes get separate migrations.
BEGIN;

CREATE TABLE public.users (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    username text NOT NULL,
    password_hash text NOT NULL,
    created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,

    CONSTRAINT users_username_format CHECK (
        char_length(username) BETWEEN 3 AND 32
        AND username !~ '[[:space:][:cntrl:]]'
    ),
    CONSTRAINT users_password_hash_not_empty CHECK (
        btrim(password_hash) <> ''
    )
);

CREATE UNIQUE INDEX users_username_ci_key ON public.users (lower(username));

COMMENT ON TABLE public.users IS 'Game accounts; character and session state belongs in separate tables.';
COMMENT ON COLUMN public.users.password_hash IS 'Encoded password hash including algorithm, parameters and salt, e.g. Argon2id PHC. Computed and verified by the application.';
COMMENT ON COLUMN public.users.created_at IS 'Account creation instant; PostgreSQL stores timestamptz independently of the display time zone.';

COMMIT;
