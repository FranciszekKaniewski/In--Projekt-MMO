-- Requires 001_create_users.sql. Run once against sdl_game.
BEGIN;

CREATE TABLE public.characters (
    id bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id bigint NOT NULL,
    name text NOT NULL,

    -- Logical map key; currently resolved to Assets/Maps/Map_expanded.json.
    map_key text NOT NULL DEFAULT 'map_expanded',
    position_x real NOT NULL DEFAULT 1350.0,
    position_y real NOT NULL DEFAULT 1320.0,
    -- FacingDirection: N=0, NE=1, E=2, SE=3, S=4, SW=5, W=6, NW=7.
    facing smallint NOT NULL DEFAULT 4,

    -- Progression is planned; no level/XP formula is implemented in SDL yet.
    level integer NOT NULL DEFAULT 1,
    experience bigint NOT NULL DEFAULT 0,

    created_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at timestamptz NOT NULL DEFAULT CURRENT_TIMESTAMP,
    last_logout_at timestamptz,

    CONSTRAINT characters_user_fk FOREIGN KEY (user_id)
        REFERENCES public.users (id) ON DELETE RESTRICT,
    CONSTRAINT characters_name_format CHECK (
        char_length(name) BETWEEN 3 AND 32
        AND name !~ '^[[:space:]]|[[:space:]]$|[[:cntrl:]]'
    ),
    CONSTRAINT characters_map_key_format CHECK (
        map_key ~ '^[a-z][a-z0-9_]{0,63}$'
    ),
    -- Also reject NaN, which PostgreSQL orders above every finite value.
    CONSTRAINT characters_position_finite CHECK (
        position_x > '-Infinity'::real AND position_x < 'Infinity'::real
        AND position_y > '-Infinity'::real AND position_y < 'Infinity'::real
    ),
    CONSTRAINT characters_facing_valid CHECK (facing BETWEEN 0 AND 7),
    CONSTRAINT characters_level_positive CHECK (level >= 1),
    CONSTRAINT characters_experience_nonnegative CHECK (experience >= 0)
);

-- Names identify players in shared chat, regardless of letter case.
CREATE UNIQUE INDEX characters_name_ci_key ON public.characters (lower(name));
CREATE INDEX characters_user_id_idx ON public.characters (user_id);

CREATE FUNCTION public.characters_set_updated_at()
RETURNS trigger
LANGUAGE plpgsql
AS $function$
BEGIN
    NEW.updated_at := clock_timestamp();
    RETURN NEW;
END;
$function$;

CREATE TRIGGER characters_updated_at
BEFORE UPDATE ON public.characters
FOR EACH ROW EXECUTE FUNCTION public.characters_set_updated_at();

COMMENT ON TABLE public.characters IS 'Persistent player characters. Each account may own multiple characters; client ECS IDs are not database IDs.';
COMMENT ON COLUMN public.characters.user_id IS 'Owning game account. Accounts with characters cannot be deleted implicitly.';
COMMENT ON COLUMN public.characters.name IS 'Character name mapped to ChatSpeakerComponent.name; globally unique ignoring letter case.';
COMMENT ON COLUMN public.characters.map_key IS 'Stable map identifier, not a local filesystem path. Map existence and movement bounds are validated by the server.';
COMMENT ON COLUMN public.characters.position_x IS 'Last saved TransformComponent.position.x in world units, independent of camera and viewport.';
COMMENT ON COLUMN public.characters.position_y IS 'Last saved TransformComponent.position.y in world units, independent of camera and viewport.';
COMMENT ON COLUMN public.characters.facing IS 'MovementComponent.facing enum value, not the sprite sheet animation row.';
COMMENT ON COLUMN public.characters.level IS 'Progression level; application must update consistently with experience once progression rules exist.';
COMMENT ON COLUMN public.characters.updated_at IS 'Automatically refreshed whenever the character row is updated.';
COMMENT ON COLUMN public.characters.last_logout_at IS 'Last completed logout, set by the server together with its position checkpoint. NULL before first logout.';

COMMIT;
