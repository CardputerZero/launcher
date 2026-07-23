from pathlib import Path


GENERATED_CONFIG_FILES = (
    "global_config.mk",
    "global_config.h",
    "lvgl_config.h",
)


def config_default_identity(selected_config):
    selected_path = Path(selected_config).resolve()
    return str(selected_path) + "\n" + selected_path.read_text(encoding="utf-8")


def sync_config_default(config_dir, selected_config):
    config_dir = Path(config_dir)
    stamp_path = config_dir / "config_default_file"
    selected_identity = config_default_identity(selected_config)
    previous_identity = stamp_path.read_text(encoding="utf-8") if stamp_path.exists() else None
    if previous_identity == selected_identity:
        return ()

    removed = []
    for filename in GENERATED_CONFIG_FILES:
        generated_path = config_dir / filename
        if generated_path.exists():
            generated_path.unlink()
            removed.append(filename)

    config_dir.mkdir(parents=True, exist_ok=True)
    stamp_path.write_text(selected_identity, encoding="utf-8")
    return tuple(removed)


def sync_config_tmp(config_dir, content):
    config_tmp_path = Path(config_dir) / "config_tmp.mk"
    if content is None:
        if config_tmp_path.exists():
            config_tmp_path.unlink()
            return True
        return False

    if config_tmp_path.exists() and config_tmp_path.read_text(encoding="utf-8") == content:
        return False
    config_tmp_path.parent.mkdir(parents=True, exist_ok=True)
    config_tmp_path.write_text(content, encoding="utf-8")
    return True
