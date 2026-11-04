"""Generate a TLS clock seed at build time; network time replaces it."""
from pathlib import Path
import time
Import("env")
header = Path(env.subst("$PROJECT_DIR")) / "include/build_time.h"
header.write_text("#pragma once\n#include <time.h>\nconstexpr time_t BUILD_EPOCH = %d;\n" % int(time.time()))
