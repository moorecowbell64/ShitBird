#!/usr/bin/env python3
"""
ShitBird Firmware - Build Version Script
Generates version header with build timestamp and git info
"""

import subprocess
import datetime
import os

def get_git_revision():
    """Get current git revision hash"""
    try:
        return subprocess.check_output(
            ['git', 'rev-parse', '--short', 'HEAD'],
            stderr=subprocess.DEVNULL
        ).decode('utf-8').strip()
    except:
        return "unknown"

def get_git_branch():
    """Get current git branch"""
    try:
        return subprocess.check_output(
            ['git', 'rev-parse', '--abbrev-ref', 'HEAD'],
            stderr=subprocess.DEVNULL
        ).decode('utf-8').strip()
    except:
        return "unknown"

def get_git_dirty():
    """Check if working directory has uncommitted changes"""
    try:
        status = subprocess.check_output(
            ['git', 'status', '--porcelain'],
            stderr=subprocess.DEVNULL
        ).decode('utf-8').strip()
        return len(status) > 0
    except:
        return False

def main(env):
    """PlatformIO build script entry point"""

    # Get version info
    version_major = 1
    version_minor = 0
    version_patch = 0

    git_rev = get_git_revision()
    git_branch = get_git_branch()
    git_dirty = get_git_dirty()

    build_time = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    build_date = datetime.datetime.now().strftime("%Y%m%d")

    # Build version string
    version_string = f"{version_major}.{version_minor}.{version_patch}"
    if git_rev != "unknown":
        version_string += f"-{git_rev}"
    if git_dirty:
        version_string += "-dirty"

    # Add build flags
    env.Append(CPPDEFINES=[
        ("SHITBIRD_VERSION_MAJOR", version_major),
        ("SHITBIRD_VERSION_MINOR", version_minor),
        ("SHITBIRD_VERSION_PATCH", version_patch),
        ("SHITBIRD_VERSION_STRING", f'\\"{version_string}\\"'),
        ("SHITBIRD_BUILD_TIME", f'\\"{build_time}\\"'),
        ("SHITBIRD_BUILD_DATE", f'\\"{build_date}\\"'),
        ("SHITBIRD_GIT_REV", f'\\"{git_rev}\\"'),
        ("SHITBIRD_GIT_BRANCH", f'\\"{git_branch}\\"'),
    ])

    print(f"[ShitBird] Building version {version_string}")
    print(f"[ShitBird] Git: {git_branch}@{git_rev} {'(dirty)' if git_dirty else ''}")
    print(f"[ShitBird] Build time: {build_time}")

# For standalone execution (testing)
if __name__ == "__main__":
    print(f"Version: 1.0.0-{get_git_revision()}")
    print(f"Branch: {get_git_branch()}")
    print(f"Dirty: {get_git_dirty()}")
