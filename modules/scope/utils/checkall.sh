#!/bin/bash
grep -l "pre.*id.*grammar" ../documentation/*.html | xargs ./parser.py
