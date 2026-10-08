#!/usr/bin/env python3
"""Validate optional local historical data using the frozen original checker."""
from pathlib import Path
import runpy
import sys

repo = next(p for p in Path(__file__).resolve().parents if (p/'scripts/benchmarks/archives').is_dir())
sys.path.insert(0, str(repo/'scripts/benchmarks'))
from archives.local import prepare_archive

if __name__ == '__main__':
    view = prepare_archive(__file__)
    runpy.run_path(str(view/'validate.py'), run_name='__main__')
