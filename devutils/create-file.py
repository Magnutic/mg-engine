#!/usr/bin/env python3

import sys
import os
from pathlib import Path
from datetime import datetime


def create(filepath: str, content: str | None):
    script_path = os.path.dirname(os.path.realpath(__file__))
    project_path = Path(script_path).parent
    suffix = Path(filepath).suffix.lower()

    template_path = Path(script_path) / ("template" + suffix)

    if template_path.is_file():
        print("Using template:", str(template_path))
        with (open(template_path) as f):
            contents_out = f.read()
        header_path = os.path.relpath(filepath.replace('src/', '')
                                              .replace('.cpp', '.h'),
                                      project_path)
        contents_out = contents_out.replace("@@HEADER_PATH@@", header_path)
        contents_out = contents_out.replace("@@YEAR@@", str(datetime.now().year))
        contents_out = contents_out.replace("@@FILENAME@@", os.path.basename(filepath))
        contents_out = contents_out.replace("@@CONTENT@@", content if content else '')
    else:
        contents_out = content

    if Path(filepath).is_file():
        raise RuntimeError(f"{filepath} already exists")

    with (open(filepath, 'w') as f):
        f.write(contents_out)


if __name__ == '__main__':
    content = None
    if not sys.stdin.isatty():
        content = sys.stdin.read()

    if len(sys.argv) != 2:
        raise RuntimeError("Expected args: destination-path")

    create(sys.argv[1], content)
