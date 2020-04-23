#!/usr/bin/env python3
import sys
import os
import os.path

used = set()

INCLUDE_PATHS = ['/Users/alexeyd/ClionProjects/Olymp/spcppl']


def include_target(file, path):
    for include_path in [os.path.dirname(file), *INCLUDE_PATHS]:
        candidate = os.path.join(include_path, path)
        if os.path.exists(candidate):
            return candidate
    return None


def dfs(file):
    if file in used:
        return
    used.add(file)
    with open(file, "r") as f:
        for line in f:
            if line.startswith("#include "):
                path = include_target(file, line[8:].strip("\n\r \"<>"))
                if path is not None:
                    dfs(path)
                else:
                    print(line, end='')
            else:
                print(line, end='')


def main():
    dfs(sys.argv[1])


if __name__ == '__main__':
    main()
