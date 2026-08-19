"""Read the machine paths the build needs out of build.env.

The file sits beside the project, is not tracked, and holds one KEY=VALUE per
line. build.env.example lists every key with a description. tools/build_cpp.bat
reads the same file, and a real environment variable wins over it.
"""

import os

from project import PROJECT_ROOT

ENV_FILE = os.path.join(PROJECT_ROOT, "build.env")
EXAMPLE_FILE = os.path.join(PROJECT_ROOT, "build.env.example")

_values = None


def _load():
    """Return the file's contents as a dict, reading it once.

    @return Mapping of key to value; empty when the file is absent.
    """
    # One file for both halves of the build, so a machine path is written once
    # whichever half wants it. The environment winning is enough for a one-off
    # override without editing anything.
    global _values
    if _values is not None:
        return _values

    _values = {}
    if os.path.isfile(ENV_FILE):
        with open(ENV_FILE, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                key, value = line.split("=", 1)
                # Quotes are what a shell would strip, and a path with spaces
                # invites them.
                _values[key.strip()] = value.strip().strip('"')
    return _values


def get(key):
    """Return the value for `key`, or None.

    @param key Name as it appears in build.env.
    @return The environment's value, else the file's, else None.
    """
    return os.environ.get(key) or _load().get(key)


def require(key, what):
    """Return the value for `key`, or explain what to do about its absence.

    @param key Name as it appears in build.env.
    @param what Human description of the path, used in the message.
    @return The value.
    """
    value = get(key)
    if not value:
        raise RuntimeError(
            "%s is not set: it holds %s.\n"
            "Copy %s to build.env and fill it in, or set the variable in the "
            "environment." % (key, what, os.path.basename(EXAMPLE_FILE)))
    return value
