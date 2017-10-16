#!/bin/sh -e

pip install --no-cache-dir -v -e .

pylint src
