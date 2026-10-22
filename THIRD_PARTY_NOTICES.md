# Third-party notices

## CMU Flite 2.2 and the cmu_us_slt voice

Used by the speech work in issue #6. Flite source and voice data are **not committed**.
`tools/Get-Toolchain.ps1 -IncludeFlite` fetches the pinned release tarball
(`https://github.com/festvox/flite/archive/refs/tags/v2.2.tar.gz`, SHA-256
`AB1555FE5ADC3F99F1D4A1A0EB1596D329FD6D74F1464A0097C81F53C0CF9E5C`). Host tools
build from it. Any JOSHUA assembly derived from Flite's algorithms or data is a
modified work and is marked as such in its source.

The `cmu_us_slt` voice (JOSHUA's target; `rms` is a host-only alternative)
was built by CMU from the CMU ARCTIC speech database (speaker "slt"). Voice and lexicon data files carry no header of their own and are
distributed under Flite's top-level licence, reproduced below.

```
                  Language Technologies Institute
                     Carnegie Mellon University
                      Copyright (c) 1999-2017
                        All Rights Reserved.

  Permission is hereby granted, free of charge, to use and distribute
  this software and its documentation without restriction, including
  without limitation the rights to use, copy, modify, merge, publish,
  distribute, sublicense, and/or sell copies of this work, and to
  permit persons to whom this work is furnished to do so, subject to
  the following conditions:
   1. The code must retain the above copyright notice, this list of
      conditions and the following disclaimer.
   2. Any modifications must be clearly marked as such.
   3. Original authors' names are not deleted.
   4. The authors' names are not used to endorse or promote products
      derived from this software without specific prior written
      permission.

  CARNEGIE MELLON UNIVERSITY AND THE CONTRIBUTORS TO THIS WORK
  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
  SHALL CARNEGIE MELLON UNIVERSITY NOR THE CONTRIBUTORS BE LIABLE
  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
  THIS SOFTWARE.
```

Original authors of Flite and its voices include Alan W Black and Kevin A. Lenzo
(Carnegie Mellon University). No endorsement by CMU or the authors is implied.

## Zig 0.13.0 (host build tool only)

Used only on the development host to compile the reference Flite. It is not
committed or shipped. Zig is MIT licensed; see https://ziglang.org.
