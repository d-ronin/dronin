#!/usr/bin/env python

import sys

def main(args):
    if (len(args) != 2):
        print("usage: %s fontfile.mcm")
        print("outputs to stdout")
        return

    with open(args[1]) as f:
        font_data = f.readlines()[1:]

    print("/* autogenerated from %s */"%(args[1]))

    # Only carry the useful bits, to not waste memory.
    for i in range(0, len(font_data), 64):
        for j in range(0, 54):
            line = font_data[i+j]
            print('\t0b' + line.strip() + ',')

if __name__ == '__main__':
    main(sys.argv)
