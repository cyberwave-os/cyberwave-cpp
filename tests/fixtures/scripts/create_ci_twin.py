#!/usr/bin/env python3
"""
Fixture for C++ SDK CI: create a project, environment, asset, and twin via the API.

Used by the C++ workflow so examples have a twin to use (TWIN_UUID). The twin
has no URDF; quickstart tolerates that by skipping the joint section on error.

Prints one line: TWIN_UUID=<uuid>
Read CYBERWAVE_BASE_URL and CYBERWAVE_API_KEY from the environment.
"""

import os
import sys

# Prefer repo sibling so CI can pip install once and run this script
try:
    from cyberwave import Cyberwave
except ImportError as e:
    print(f"Import error: {e}", file=sys.stderr)
    sys.exit("Install the Python SDK first, e.g.: pip install -e cyberwave-sdks/cyberwave-python")

BASE_URL = os.environ.get("CYBERWAVE_BASE_URL", "http://localhost:8000")
API_KEY = os.environ.get("CYBERWAVE_API_KEY", "")
if not API_KEY:
    print("CYBERWAVE_API_KEY is required", file=sys.stderr)
    sys.exit(1)

NAME_PROJECT = "CI Example Project"
NAME_ENVIRONMENT = "CI Example Environment"
NAME_ASSET = "CI Example Asset"
NAME_TWIN = "CI Example Twin"


def main():
    cw = Cyberwave(base_url=BASE_URL, api_key=API_KEY)

    workspaces = cw.workspaces.list()
    if not workspaces:
        print("No workspaces found; run backend seed_data first", file=sys.stderr)
        sys.exit(1)
    workspace_id = str(workspaces[0].uuid)

    project = cw.projects.create(
        name=NAME_PROJECT,
        workspace_id=workspace_id,
        description="Project for C++ SDK CI examples",
    )
    project_id = str(project.uuid)

    environment = cw.environments.create(
        name=NAME_ENVIRONMENT,
        project_id=project_id,
        description="Environment for C++ SDK CI twin",
    )
    environment_id = str(environment.uuid)

    assets = cw.assets.list()
    if not assets:
        asset = cw.assets.create(
            name=NAME_ASSET,
            description="Asset for C++ SDK CI twin",
        )
    else:
        asset = assets[0]
    asset_id = str(asset.uuid)

    twin = cw.twins.create(
        asset_id=asset_id,
        environment_id=environment_id,
        name=NAME_TWIN,
    )
    print(f"TWIN_UUID={twin.uuid}")
    sys.stdout.flush()


if __name__ == "__main__":
    try:
        main()
    except Exception as e:
        print(f"create_ci_twin failed: {e}", file=sys.stderr)
        raise
