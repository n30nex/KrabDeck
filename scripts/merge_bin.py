#!/usr/bin/env python3
# Placeholder — ESP32 merge-bin post-build script
import os
Import("env")

def after_build(source, target, env):
    print("SlopOS T-Deck: build complete")

env.AddPostAction("buildprog", after_build)
