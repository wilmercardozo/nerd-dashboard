Import("env")
import os, shutil, hashlib

# ---------------------------------------------------------------------------
# Copia include/lv_conf.h al directorio de la libreria LVGL en cada build.
# Con LV_CONF_INCLUDE_SIMPLE + -I include/, LVGL encuentra lv_conf.h; la copia
# garantiza que los includes relativos internos de LVGL tambien lo resuelvan.
#
# NOTA: en esta version de PlatformIO/espressif el LDF (deep+) compila las
# fuentes .c de LVGL automaticamente, asi que NO forzamos su compilacion aqui
# (hacerlo duplicaria simbolos en el link).
# ---------------------------------------------------------------------------

def _file_hash(path):
    with open(path, 'rb') as f:
        return hashlib.md5(f.read()).hexdigest()

def copy_lv_conf(source, target, env):
    src = os.path.join(env["PROJECT_INCLUDE_DIR"], "lv_conf.h")
    dst = os.path.join(env["PROJECT_LIBDEPS_DIR"], env["PIOENV"], "lvgl", "lv_conf.h")
    if not os.path.exists(src):
        return
    if not os.path.exists(dst) or _file_hash(src) != _file_hash(dst):
        shutil.copy2(src, dst)
        print("[lv_conf] Copied lv_conf.h -> {}".format(dst))
    else:
        print("[lv_conf] lv_conf.h up to date, skipping copy")

env.AddPreAction("buildprog", copy_lv_conf)
