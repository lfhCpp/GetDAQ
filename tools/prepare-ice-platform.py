#!/usr/bin/env python3
"""Adapt legacy Ice CPU detection in a build copy, preserving curated originals."""
import argparse
from pathlib import Path


def patch_config(source, destination):
    text=source.read_text()
    endian='#if defined(__i386)'
    replacement='''// GetDAQ compatibility: compiler byte order covers Apple Silicon.
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#   define ICE_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#   define ICE_BIG_ENDIAN
#elif defined(__i386)'''
    if text.count(endian)!=1:raise ValueError('Unrecognized Ice endianness declaration')
    text=text.replace(endian,replacement,1)
    width='#if defined(__sun) && (defined(__sparcv9) || defined(__x86_64))'
    if text.count(width)!=1:raise ValueError('Unrecognized Ice pointer-width declaration')
    text=text.replace(width,'#if defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 8) || \\\n      defined(__sun) && (defined(__sparcv9) || defined(__x86_64))',1)
    destination.parent.mkdir(parents=True,exist_ok=True)
    if not destination.exists() or destination.read_text()!=text:destination.write_text(text)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',type=Path,required=True);parser.add_argument('--destination',type=Path,required=True)
    args=parser.parse_args();patch_config(args.source,args.destination)
